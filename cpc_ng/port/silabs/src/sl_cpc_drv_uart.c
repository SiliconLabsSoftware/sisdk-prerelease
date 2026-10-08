/***************************************************************************/ /**
 * @file
 * @brief CPC UART Driver implementation.
 *******************************************************************************
 * # License
 * <b>Copyright 2026 Silicon Laboratories Inc. www.silabs.com</b>
 *******************************************************************************
 *
 * SPDX-License-Identifier: Zlib
 *
 * The licensor of this software is Silicon Laboratories Inc.
 *
 * This software is provided 'as-is', without any express or implied
 * warranty. In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 *
 ******************************************************************************/

#include "sl_cpc_drv_uart.h"

#include "sl_common.h"
#include "sl_cpc_drv_instances.h"
#include "sl_iostream.h"
#include "sl_iostream_uart.h"
#include "sl_slist.h"
#include "sl_status.h"

#include "../../../src/sli_cpc_assert.h"
#include "../../../src/sli_cpc_atomic.h"
#include "../../../src/sli_cpc_bus.h"
#include "../../../src/sli_cpc_crc.h"
#include "../../../src/sli_cpc_dispatcher.h"
#include "../../../src/sli_cpc_drv.h"
#include "../../../src/sli_cpc_endianness.h"
#include "../../../src/sli_cpc_frame_list.h"
#include "../../../src/sli_cpc_hdr.h"
#include "../../../src/sli_cpc_memory.h"
#include "sli_iostream.h"
#include "sli_iostream_uart.h"

#if defined(_SILICON_LABS_32B_SERIES_3)
#include "sl_hal_ldma.h"
#define CPC_DRV_UART_LDMA_DESCRIPTOR_MAX_XFER_SIZE SL_HAL_LDMA_DESCRIPTOR_MAX_XFER_SIZE
#elif defined(_SILICON_LABS_32B_SERIES_2)
#include "em_ldma.h"
#define CPC_DRV_UART_LDMA_DESCRIPTOR_MAX_XFER_SIZE LDMA_DESCRIPTOR_MAX_XFER_SIZE
#else
#error "UART driver is only compatible with Series 2 & 3"
#endif

/*******************************************************************************
 *********************************   DEFINES   *********************************
 ******************************************************************************/

#define SLI_CPC_DRV_UART_ASYNC_TX_OP_POOL_SIZE 10
#define SLI_CPC_DRV_UART_ASYNC_TX_HEADER_POOL_SIZE 5
#define SLI_CPC_DRV_UART_PREAMBLE 0xEB
#define SLI_CPC_DRV_UART_CRC_SIZE sizeof(uint16_t)
#define SLI_CPC_DRV_UART_HEADER_BLOCK_SIZE (1U + SLI_CPC_HEADER_SIZE + SLI_CPC_DRV_UART_CRC_SIZE)

static_assert(SL_CPC_EP_MAX_PAYLOAD_SIZE <= (2U * CPC_DRV_UART_LDMA_DESCRIPTOR_MAX_XFER_SIZE),
              "Payload size exceeds UART max transfer capacity");

/*******************************************************************************
 ***************************  LOCAL VARIABLES   ********************************
 ******************************************************************************/
typedef struct cpc_drv_uart_tx_op {
  sl_slist_node_t node;
  sli_iostream_write_async_op_t op;
  uint8_t crc_le[SLI_CPC_DRV_UART_CRC_SIZE];
  bool write_completed;
} cpc_drv_uart_tx_op_t;

typedef struct cpc_drv_uart_tx_header {
  sl_slist_node_t node;
  uint8_t preamble;
  uint8_t hdr[SLI_CPC_HEADER_SIZE];
  uint8_t hdr_crc[SLI_CPC_DRV_UART_CRC_SIZE];
} cpc_drv_uart_tx_header_t;

typedef enum cpc_drv_uart_rx_state {
  RECEIVING_HEADER_BLOCK,
  RECEIVING_PAYLOAD,
  RECEIVING_PAYLOAD_CRC,
} cpc_drv_uart_rx_state_t;

typedef enum {
  CPC_DRV_UART_HDR_SYNC_WAIT,
  CPC_DRV_UART_HDR_SYNC_RETRY,
  CPC_DRV_UART_HDR_SYNC_FOUND,
} cpc_drv_uart_hdr_sync_result_t;

typedef struct {
  cpc_drv_uart_rx_state_t state;
  sl_cpc_frame_t *frame;
  uint8_t hdr_block[SLI_CPC_DRV_UART_HEADER_BLOCK_SIZE];
  uint16_t hdr_block_count;
  uint32_t flush_bytes_remaining;
  uint8_t crc_buf[SLI_CPC_DRV_UART_CRC_SIZE];
  uint16_t index;
} cpc_drv_uart_rx_ctx_t;

/*
 * Singleton runtime state. Ops take `sl_cpc_bus_t *` per sli_cpc_drv.h
 * but mostly ignore `bus` and use these file-scope variables. For multi-bus
 * support, move this state into `sl_cpc_drv_uart` and access it via container_of.
 */
static sl_cpc_bus_t *s_bus;

static sli_cpc_dispatcher_handle_t rx_dispatcher;
static sli_cpc_frame_list_t rx_pending_frame_list;

static sli_cpc_dispatcher_handle_t tx_dispatcher;
static sli_cpc_frame_list_t tx_pending_frame_list;
static sli_cpc_frame_list_t tx_inflight_frame_list;

static sl_slist_node_t *tx_op_free_list;
static cpc_drv_uart_tx_op_t tx_async_op_pool[SLI_CPC_DRV_UART_ASYNC_TX_OP_POOL_SIZE];
static sl_slist_node_t *tx_header_free_list;
static cpc_drv_uart_tx_header_t tx_header_pool[SLI_CPC_DRV_UART_ASYNC_TX_HEADER_POOL_SIZE];

/*******************************************************************************
 **************************   LOCAL FUNCTIONS   ********************************
 ******************************************************************************/

static sl_status_t cpc_drv_uart_start_rx(sl_cpc_bus_t *bus);
static sl_status_t cpc_drv_uart_init(sl_cpc_bus_t *bus);
static sl_status_t cpc_drv_uart_read_data(sl_cpc_bus_t *bus, sli_cpc_frame_list_t *frames);

static uint32_t cpc_drv_uart_get_available_write_frame_slots(sl_cpc_bus_t *bus);

static void cpc_drv_uart_process_tx(void *data);
static void cpc_drv_uart_rx_process(void *data);
static void cpc_drv_uart_on_new_rx_data(void *data);
static void cpc_drv_uart_on_rx_frame_free(sl_cpc_bus_t *bus);
static uint32_t cpc_drv_uart_write(sl_cpc_bus_t *bus, sli_cpc_frame_list_t *frames);
static void cpc_drv_uart_on_tx_complete(sli_iostream_write_async_op_t *op, sl_status_t status, void *arg);
static void cpc_drv_uart_set_rx_state(cpc_drv_uart_rx_ctx_t *rx_ctx, cpc_drv_uart_rx_state_t state);

typedef struct {
  cpc_drv_uart_tx_op_t *header_op;
  cpc_drv_uart_tx_header_t *header;
  cpc_drv_uart_tx_op_t *payload_op_first;
  cpc_drv_uart_tx_op_t *payload_op_second;
  cpc_drv_uart_tx_op_t *payload_crc_op;
} cpc_drv_uart_tx_frame_ops_t;

typedef struct {
  bool header_op;
  bool payload_first;
  bool payload_second;
} cpc_drv_uart_tx_frame_submit_state_t;

static sl_status_t cpc_drv_uart_init_payload_tx_ops(sl_cpc_frame_t *frame, cpc_drv_uart_tx_frame_ops_t *ops);
static sl_status_t cpc_drv_uart_submit_tx_frame(const cpc_drv_uart_tx_frame_ops_t *ops,
                                                cpc_drv_uart_tx_frame_submit_state_t *submit_state,
                                                sl_cpc_frame_t *frame);
static void cpc_drv_uart_release_unsubmitted_tx_frame(const cpc_drv_uart_tx_frame_ops_t *ops,
                                                      const cpc_drv_uart_tx_frame_submit_state_t *submit_state);
static bool cpc_drv_uart_complete_partial_tx_frame(const cpc_drv_uart_tx_frame_ops_t *ops,
                                                   const cpc_drv_uart_tx_frame_submit_state_t *submit_state);
static void cpc_drv_uart_notify_tx_frame_completed(sl_cpc_frame_t *frame);

sl_status_t sl_cpc_drv_uart_init(sl_cpc_drv_uart_t *drv, const sl_cpc_drv_uart_config_t *cfg,
                                 const sl_cpc_bus_config_t *bus_cfg)
{
  static const sli_cpc_drv_ops_t ops = {
    .init = &cpc_drv_uart_init,
    .start_rx = &cpc_drv_uart_start_rx,
    .read = &cpc_drv_uart_read_data,
    .write = &cpc_drv_uart_write,
    .get_available_write_frame_slots = &cpc_drv_uart_get_available_write_frame_slots,
    .on_rx_frame_free = &cpc_drv_uart_on_rx_frame_free,
  };

  if (cfg == NULL || bus_cfg == NULL || (cfg->iostream == NULL || cfg->iostream_uart == NULL)) {
    return SL_STATUS_NULL_POINTER;
  }

  memset(drv, 0, sizeof(*drv));
  drv->iostream = cfg->iostream;
  drv->iostream_uart = cfg->iostream_uart;

  return sli_cpc_bus_init(&drv->bus, bus_cfg, &ops);
}

static inline sl_cpc_drv_uart_t *to_drv(sl_cpc_bus_t *bus)
{
  return container_of(bus, sl_cpc_drv_uart_t, bus);
}

static bool cpc_drv_uart_rx_enabled = false;
static bool cpc_drv_uart_rx_waiting_for_frame = false;

/***************************************************************************/ /**
 * Initialize software structures required by the driver.
 ******************************************************************************/
static sl_status_t cpc_drv_uart_init(sl_cpc_bus_t *bus)
{
  sl_cpc_drv_uart_t *drv = to_drv(bus);

  s_bus = bus;

  sli_cpc_dispatcher_init_handle(&tx_dispatcher, bus);
  sli_cpc_dispatcher_init_handle(&rx_dispatcher, bus);
  sli_cpc_frame_list_init(&rx_pending_frame_list);
  sli_cpc_frame_list_init(&tx_pending_frame_list);
  sli_cpc_frame_list_init(&tx_inflight_frame_list);

  sl_slist_init(&tx_op_free_list);
  for (uint8_t i = 0; i < SLI_CPC_DRV_UART_ASYNC_TX_OP_POOL_SIZE; i++) {
    sl_slist_push(&tx_op_free_list, &tx_async_op_pool[i].node);
  }

  sl_slist_init(&tx_header_free_list);
  for (uint8_t i = 0; i < SLI_CPC_DRV_UART_ASYNC_TX_HEADER_POOL_SIZE; i++) {
    sl_slist_push(&tx_header_free_list, &tx_header_pool[i].node);
  }

#if defined(SL_CATALOG_KERNEL_PRESENT)
  // Make sure IOStream is not called blocking, as it could deadlock the CPC Core, since reads are
  // done in the CPC dispatcher.
  sl_iostream_uart_set_read_block(drv->iostream_uart, false);
#endif

  sli_iostream_uart_subscribe_to_new_data(drv->iostream_uart, cpc_drv_uart_on_new_rx_data, NULL);

  // Async TX mode is set via private IOStream context until the driver no longer uses IOStream.
  // Shall be fixed as soon as UART driver is used instead of IOStream.
  sl_iostream_uart_context_t *ctx = (sl_iostream_uart_context_t *)drv->iostream_uart->stream.context;
  ctx->async_tx_mode = true;

  return SL_STATUS_OK;
}

/***************************************************************************/ /**
 * Read data received
 ******************************************************************************/
static sl_status_t cpc_drv_uart_read_data(sl_cpc_bus_t *bus, sli_cpc_frame_list_t *frames)
{
  (void)bus;

  if (SL_BRANCH_UNLIKELY(sli_cpc_frame_list_empty(&rx_pending_frame_list))) {
    return SL_STATUS_EMPTY;
  }

  sli_cpc_frame_list_extend(frames, &rx_pending_frame_list);

  return SL_STATUS_OK;
}

/***************************************************************************/ /**
 * Allocate a TX header
 ******************************************************************************/
static cpc_drv_uart_tx_header_t *cpc_drv_uart_tx_header_alloc(void)
{
  sl_slist_node_t *node;

  MCU_ATOMIC_SECTION(node = sl_slist_pop(&tx_header_free_list););
  if (node == NULL) {
    return NULL;
  }

  cpc_drv_uart_tx_header_t *header = SL_SLIST_ENTRY(node, cpc_drv_uart_tx_header_t, node);

  return header;
}

/***************************************************************************/ /**
 * Allocate an asynchronous TX operation
 ******************************************************************************/
static cpc_drv_uart_tx_op_t *cpc_drv_uart_tx_async_op_alloc(void)
{
  sl_slist_node_t *node;

  MCU_ATOMIC_SECTION(node = sl_slist_pop(&tx_op_free_list););
  if (node == NULL) {
    return NULL;
  }

  cpc_drv_uart_tx_op_t *op = SL_SLIST_ENTRY(node, cpc_drv_uart_tx_op_t, node);
  op->write_completed = false;

  return op;
}

/***************************************************************************/ /**
 * Free a TX header
 ******************************************************************************/
static void cpc_drv_uart_tx_header_free(cpc_drv_uart_tx_header_t *header)
{
  MCU_ATOMIC_SECTION(sl_slist_push(&tx_header_free_list, &header->node););
}

/***************************************************************************/ /**
 * Free an asynchronous TX operation
 ******************************************************************************/
static void cpc_drv_uart_tx_async_op_free(sli_iostream_write_async_op_t *op, sl_status_t status, void *arg)
{
  SLI_CPC_ASSERT(status == SL_STATUS_OK);

  cpc_drv_uart_tx_header_t *header = (cpc_drv_uart_tx_header_t *)arg;

  if (header != NULL) {
    cpc_drv_uart_tx_header_free(header);
  }

  cpc_drv_uart_tx_op_t *tx_op = container_of(op, cpc_drv_uart_tx_op_t, op);
  tx_op->write_completed = true;

  MCU_ATOMIC_SECTION(sl_slist_push(&tx_op_free_list, &tx_op->node););
}

static void cpc_drv_uart_notify_tx_frame_completed(sl_cpc_frame_t *frame)
{
  sli_cpc_frame_list_t completed_list;

  sli_cpc_frame_list_init(&completed_list);
  sli_cpc_frame_list_push_back(&completed_list, frame);
  sli_cpc_bus_notify_tx_data_by_drv(s_bus, &completed_list);
  sli_cpc_dispatcher_push(&tx_dispatcher, cpc_drv_uart_process_tx, NULL);
}

/***************************************************************************/ /**
 * TX callback executed when a write operation is completed
 ******************************************************************************/
static void cpc_drv_uart_on_tx_complete(sli_iostream_write_async_op_t *op, sl_status_t status, void *arg)
{
  sl_cpc_frame_t *frame;

  SLI_CPC_ASSERT(status == SL_STATUS_OK);
  cpc_drv_uart_tx_async_op_free(op, status, arg);

  MCU_ATOMIC_SECTION(frame = sli_cpc_frame_list_pop(&tx_inflight_frame_list););
  SLI_CPC_ASSERT(frame != NULL);

  cpc_drv_uart_notify_tx_frame_completed(frame);
}

/***************************************************************************/ /**
 * Prepare one or two async payload transfers.
 *
 * IOStream UART TX is backed by LDMA, which limits a single descriptor transfer
 * to `CPC_DRV_UART_LDMA_DESCRIPTOR_MAX_XFER_SIZE` bytes. Payloads larger than that
 * are sent as two back-to-back writes from the same endpoint buffer:
 *
 *   [first: bytes 0 .. MAX-1][second: bytes MAX .. end]
 *
 * Each transfer gets its own `cpc_drv_uart_tx_op_t` from the op pool. When
 * the payload fits in one transfer, `payload_op_second` is left NULL.
 *
 * `on_final_payload_xfer_complete` runs when the transfer that sends the last
 * payload byte finishes. Earlier transfers call `cpc_drv_uart_tx_async_op_free`
 * so the op is returned to the pool while the frame is still on the wire.
 ******************************************************************************/
static sl_status_t cpc_drv_uart_prepare_payload_ops(const sl_cpc_frame_t *frame,
                                                    cpc_drv_uart_tx_op_t **payload_op_first,
                                                    cpc_drv_uart_tx_op_t **payload_op_second,
                                                    sli_iostream_on_write_completed_t *on_final_payload_xfer_complete)
{
  cpc_drv_uart_tx_op_t *second = NULL;
  cpc_drv_uart_tx_op_t *first = NULL;
  size_t second_payload_chunk;
  size_t first_payload_chunk;
  uint8_t *payload_data;
  size_t payload_length;
  sl_status_t status;

  if (frame->payload == NULL) {
    return SL_STATUS_NULL_POINTER;
  }

  payload_data = frame->payload->ptr;
  payload_length = frame->payload->len;

  *payload_op_first = NULL;
  *payload_op_second = NULL;

  if (payload_length == 0) {
    return SL_STATUS_OK;
  }
  if (payload_data == NULL) {
    return SL_STATUS_NULL_POINTER;
  }
  if (payload_length > SL_CPC_EP_MAX_PAYLOAD_SIZE) {
    return SL_STATUS_INVALID_RANGE;
  }

  first_payload_chunk = payload_length > CPC_DRV_UART_LDMA_DESCRIPTOR_MAX_XFER_SIZE
                          ? CPC_DRV_UART_LDMA_DESCRIPTOR_MAX_XFER_SIZE
                          : payload_length;
  second_payload_chunk = payload_length - first_payload_chunk;

  first = cpc_drv_uart_tx_async_op_alloc();
  if (first == NULL) {
    return SL_STATUS_NO_MORE_RESOURCE;
  }

  if (second_payload_chunk > 0) {
    second = cpc_drv_uart_tx_async_op_alloc();
    if (second == NULL) {
      cpc_drv_uart_tx_async_op_free(&first->op, SL_STATUS_OK, NULL);
      return SL_STATUS_NO_MORE_RESOURCE;
    }
  }

  status = sli_iostream_init_async_write_op(
    &first->op, payload_data, first_payload_chunk,
    (second != NULL) ? &cpc_drv_uart_tx_async_op_free : on_final_payload_xfer_complete, NULL);
  if (status != SL_STATUS_OK) {
    SLI_CPC_LOG_ERROR("[UART] Failed to init first payload chunk: 0x%lx", (unsigned long)status);
    cpc_drv_uart_tx_async_op_free(&first->op, SL_STATUS_OK, NULL);
    if (second != NULL) {
      cpc_drv_uart_tx_async_op_free(&second->op, SL_STATUS_OK, NULL);
    }
    return status;
  }

  if (second != NULL) {
    payload_data += first_payload_chunk;
    status = sli_iostream_init_async_write_op(&second->op, payload_data, second_payload_chunk,
                                              on_final_payload_xfer_complete, NULL);
    if (status != SL_STATUS_OK) {
      SLI_CPC_LOG_ERROR("[UART] Failed to init second payload chunk: 0x%lx", (unsigned long)status);
      cpc_drv_uart_tx_async_op_free(&first->op, SL_STATUS_OK, NULL);
      cpc_drv_uart_tx_async_op_free(&second->op, SL_STATUS_OK, NULL);
      return status;
    }
  }

  *payload_op_first = first;
  *payload_op_second = second;

  return SL_STATUS_OK;
}

static sl_status_t cpc_drv_uart_init_payload_tx_ops(sl_cpc_frame_t *frame, cpc_drv_uart_tx_frame_ops_t *ops)
{
  sl_status_t status;

  if (frame->payload == NULL || frame->payload->len == 0) {
    return SL_STATUS_OK;
  }

  if (!frame->payload_csum_is_valid) {
    frame->payload_csum = sli_cpc_crc_buf(frame->payload);
    frame->payload_csum_is_valid = true;
  }

  ops->payload_crc_op = cpc_drv_uart_tx_async_op_alloc();
  if (ops->payload_crc_op == NULL) {
    return SL_STATUS_NO_MORE_RESOURCE;
  }

  sli_cpc_u16_to_le(frame->payload_csum, ops->payload_crc_op->crc_le);
  status = sli_iostream_init_async_write_op(&ops->payload_crc_op->op, ops->payload_crc_op->crc_le,
                                            SLI_CPC_DRV_UART_CRC_SIZE, &cpc_drv_uart_on_tx_complete, NULL);
  if (status != SL_STATUS_OK) {
    SLI_CPC_LOG_ERROR("[UART] Failed to init payload CRC write: 0x%lx", (unsigned long)status);
    return status;
  }

  status = cpc_drv_uart_prepare_payload_ops(frame, &ops->payload_op_first, &ops->payload_op_second,
                                            &cpc_drv_uart_tx_async_op_free);
  if (status != SL_STATUS_OK) {
    SLI_CPC_LOG_ERROR("[UART] Failed to prepare payload transfers: 0x%lx", (unsigned long)status);
  }

  return status;
}

static sl_status_t cpc_drv_uart_submit_tx_op(sli_iostream_write_async_op_t *op, bool *submitted)
{
  sl_cpc_drv_uart_t *drv = to_drv(s_bus);
  sl_status_t status = sli_iostream_async_write(drv->iostream, op);

  if (status != SL_STATUS_OK) {
    return status;
  }

  if (submitted != NULL) {
    *submitted = true;
  }

  return SL_STATUS_OK;
}

static sl_status_t cpc_drv_uart_submit_tx_frame(const cpc_drv_uart_tx_frame_ops_t *ops,
                                                cpc_drv_uart_tx_frame_submit_state_t *submit_state,
                                                sl_cpc_frame_t *frame)
{
  sl_cpc_frame_t *notify_frame = NULL;
  sl_status_t status;

  MCU_DECLARE_IRQ_STATE;

  MCU_ENTER_ATOMIC();

  status = cpc_drv_uart_submit_tx_op(&ops->header_op->op, &submit_state->header_op);
  if (status != SL_STATUS_OK) {
    SLI_CPC_LOG_ERROR("[UART] Failed to submit header block transfer: 0x%lx", (unsigned long)status);
    goto submit_failed;
  }

  if (ops->payload_op_first != NULL) {
    status = cpc_drv_uart_submit_tx_op(&ops->payload_op_first->op, &submit_state->payload_first);
    if (status != SL_STATUS_OK) {
      SLI_CPC_LOG_ERROR("[UART] Failed to submit payload chunk: 0x%lx", (unsigned long)status);
      goto submit_failed;
    }
  }

  if (ops->payload_op_second != NULL) {
    status = cpc_drv_uart_submit_tx_op(&ops->payload_op_second->op, &submit_state->payload_second);
    if (status != SL_STATUS_OK) {
      SLI_CPC_LOG_ERROR("[UART] Failed to submit payload chunk: 0x%lx", (unsigned long)status);
      goto submit_failed;
    }
  }

  if (ops->payload_crc_op != NULL) {
    status = cpc_drv_uart_submit_tx_op(&ops->payload_crc_op->op, NULL);
    if (status != SL_STATUS_OK) {
      SLI_CPC_LOG_ERROR("[UART] Failed to submit payload CRC: 0x%lx", (unsigned long)status);
      goto submit_failed;
    }
  }

  sli_cpc_frame_list_push_back(&tx_inflight_frame_list, frame);
  MCU_EXIT_ATOMIC();

  return SL_STATUS_OK;

submit_failed:
  if (submit_state->header_op) {
    if (!cpc_drv_uart_complete_partial_tx_frame(ops, submit_state)) {
      notify_frame = frame;
    } else {
      sli_cpc_frame_list_push_back(&tx_inflight_frame_list, frame);
    }
    status = SL_STATUS_IN_PROGRESS;
  }
  MCU_EXIT_ATOMIC();
  if (notify_frame != NULL) {
    cpc_drv_uart_notify_tx_frame_completed(notify_frame);
  }
  return status;
}

static bool cpc_drv_uart_complete_partial_tx_frame(const cpc_drv_uart_tx_frame_ops_t *ops,
                                                   const cpc_drv_uart_tx_frame_submit_state_t *submit_state)
{
  sli_iostream_write_async_op_t *last_op;
  cpc_drv_uart_tx_op_t *last_tx_op;
  void *last_arg;

  if (submit_state->payload_second && ops->payload_op_second != NULL) {
    last_tx_op = ops->payload_op_second;
    last_op = &last_tx_op->op;
    last_arg = last_op->on_write_completed_arg;
  } else if (submit_state->payload_first && ops->payload_op_first != NULL) {
    last_tx_op = ops->payload_op_first;
    last_op = &last_tx_op->op;
    last_arg = last_op->on_write_completed_arg;
  } else if (submit_state->header_op) {
    last_tx_op = ops->header_op;
    last_op = &last_tx_op->op;
    last_arg = ops->header;
  } else {
    return false;
  }

  if (last_tx_op->write_completed) {
    return false;
  }

  last_op->on_write_completed = &cpc_drv_uart_on_tx_complete;
  last_op->on_write_completed_arg = last_arg;

  return true;
}

static void cpc_drv_uart_release_unsubmitted_tx_frame(const cpc_drv_uart_tx_frame_ops_t *ops,
                                                      const cpc_drv_uart_tx_frame_submit_state_t *submit_state)
{
  if (ops->payload_crc_op != NULL) {
    cpc_drv_uart_tx_async_op_free(&ops->payload_crc_op->op, SL_STATUS_OK, NULL);
  }
  if (ops->payload_op_second != NULL && !submit_state->payload_second) {
    cpc_drv_uart_tx_async_op_free(&ops->payload_op_second->op, SL_STATUS_OK, NULL);
  }
  if (ops->payload_op_first != NULL && !submit_state->payload_first) {
    cpc_drv_uart_tx_async_op_free(&ops->payload_op_first->op, SL_STATUS_OK, NULL);
  }
  if (!submit_state->header_op) {
    if (ops->header != NULL) {
      cpc_drv_uart_tx_header_free(ops->header);
    }
    if (ops->header_op != NULL) {
      cpc_drv_uart_tx_async_op_free(&ops->header_op->op, SL_STATUS_OK, NULL);
    }
  }
}

/***************************************************************************/ /**
 * Send a frame.
 *
 * Frames are submitted to IOStream as a queue of async writes. A header-only
 * frame is a single write; a frame with payload is a chain:
 *
 *   [preamble + hdr + hdr_crc] -> [payload_op_first -> payload_op_second] -> payload_crc
 ******************************************************************************/
static sl_status_t cpc_drv_uart_send_frame(sl_cpc_frame_t *frame)
{
  sli_iostream_on_write_completed_t *on_header_block_complete;
  cpc_drv_uart_tx_frame_submit_state_t submit_state = {0};
  cpc_drv_uart_tx_frame_ops_t ops = {0};
  sl_status_t status = SL_STATUS_OK;

  ops.header_op = cpc_drv_uart_tx_async_op_alloc();
  if (ops.header_op == NULL) {
    status = SL_STATUS_NO_MORE_RESOURCE;
    goto exit;
  }

  ops.header = cpc_drv_uart_tx_header_alloc();
  if (ops.header == NULL) {
    status = SL_STATUS_NO_MORE_RESOURCE;
    goto cleanup;
  }

  ops.header->preamble = SLI_CPC_DRV_UART_PREAMBLE;

  // Copy the raw header
  memcpy(ops.header->hdr, sli_cpc_frame_get_header(frame), SLI_CPC_HEADER_SIZE);

  sli_cpc_u16_to_le(sli_cpc_crc(ops.header->hdr, SLI_CPC_HEADER_SIZE), ops.header->hdr_crc);

  status = cpc_drv_uart_init_payload_tx_ops(frame, &ops);
  if (status != SL_STATUS_OK) {
    goto cleanup;
  }

  on_header_block_complete = (ops.payload_op_first != NULL || ops.payload_crc_op != NULL)
                               ? &cpc_drv_uart_tx_async_op_free
                               : &cpc_drv_uart_on_tx_complete;

  status = sli_iostream_init_async_write_op(&ops.header_op->op, &ops.header->preamble,
                                            sizeof(ops.header->preamble) + sizeof(ops.header->hdr)
                                              + sizeof(ops.header->hdr_crc),
                                            on_header_block_complete, ops.header);
  if (status != SL_STATUS_OK) {
    SLI_CPC_LOG_ERROR("[UART] Failed to init header block write: 0x%lx", (unsigned long)status);
    goto cleanup;
  }

  status = cpc_drv_uart_submit_tx_frame(&ops, &submit_state, frame);
  if (status != SL_STATUS_OK) {
    goto cleanup;
  }

  goto exit;

cleanup:
  cpc_drv_uart_release_unsubmitted_tx_frame(&ops, &submit_state);

exit:
  return status;
}

/***************************************************************************/ /**
 * Called when the core queues new frames for write.
 ******************************************************************************/
static uint32_t cpc_drv_uart_write(sl_cpc_bus_t *bus, sli_cpc_frame_list_t *frames)
{
  uint32_t num_frames = sli_cpc_frame_list_get_len(frames);
  (void)bus;

  if (cpc_drv_uart_get_available_write_frame_slots(bus) < num_frames) {
    // Core attempted to send more frames than the driver can take. In emulation, we don't expect
    // this to ever happen.
    SLI_CPC_ASSERT(false);
    return 0;
  }

  MCU_ATOMIC_SECTION(sli_cpc_frame_list_extend(&tx_pending_frame_list, frames);)

  cpc_drv_uart_process_tx(NULL);

  return num_frames;
}

/***************************************************************************/ /**
 * Get the number of available slots for write
 ******************************************************************************/
static uint32_t cpc_drv_uart_get_available_write_frame_slots(sl_cpc_bus_t *bus)
{
  uint32_t num_available_slots;

  (void)bus;

  MCU_ATOMIC_SECTION(num_available_slots = (SLI_CPC_DRV_UART_ASYNC_TX_HEADER_POOL_SIZE
                                            - sli_cpc_frame_list_get_len(&tx_pending_frame_list));)

  // Sanity check for underflow
  if (num_available_slots > SLI_CPC_DRV_UART_ASYNC_TX_HEADER_POOL_SIZE) {
    SLI_CPC_ASSERT(false);
    return 0;
  }

  return num_available_slots;
}

/***************************************************************************/ /**
 * Process pending TX frames
 ******************************************************************************/
static void cpc_drv_uart_process_tx(void *data)
{
  sl_cpc_frame_t *frame;
  sl_status_t status;
  (void)data;

  while ((frame = sli_cpc_frame_list_pop(&tx_pending_frame_list)) != NULL) {
    status = cpc_drv_uart_send_frame(frame);
    if (status != SL_STATUS_OK && status != SL_STATUS_IN_PROGRESS) {
      // Try again later
      sli_cpc_frame_list_push_front(&tx_pending_frame_list, frame);
      MCU_ATOMIC_SECTION(sli_cpc_dispatcher_push(&tx_dispatcher, cpc_drv_uart_process_tx, NULL););
      break;
    }
  }
}

/***************************************************************************/ /**
 * Start reception
 ******************************************************************************/
static sl_status_t cpc_drv_uart_start_rx(sl_cpc_bus_t *bus)
{
  MCU_DECLARE_IRQ_STATE;

  (void)bus;
  SLI_CPC_ASSERT(!cpc_drv_uart_rx_enabled);

  MCU_ENTER_ATOMIC();
  cpc_drv_uart_rx_waiting_for_frame = false;
  MCU_EXIT_ATOMIC();

  cpc_drv_uart_rx_enabled = true;
  (void)sli_cpc_dispatcher_push(&rx_dispatcher, cpc_drv_uart_rx_process, NULL);

  return SL_STATUS_OK;
}

static void cpc_drv_uart_on_new_rx_data(void *data)
{
  (void)data;

  if (cpc_drv_uart_rx_enabled) {
    (void)sli_cpc_dispatcher_push(&rx_dispatcher, cpc_drv_uart_rx_process, NULL);
  }
}

static void cpc_drv_uart_on_rx_frame_free(sl_cpc_bus_t *bus)
{
  bool kick_rx = false;
  MCU_DECLARE_IRQ_STATE;

  (void)bus;

  if (!cpc_drv_uart_rx_enabled) {
    return;
  }

  MCU_ENTER_ATOMIC();
  if (cpc_drv_uart_rx_waiting_for_frame) {
    cpc_drv_uart_rx_waiting_for_frame = false;
    kick_rx = true;
  }
  MCU_EXIT_ATOMIC();

  if (kick_rx) {
    (void)sli_cpc_dispatcher_push(&rx_dispatcher, cpc_drv_uart_rx_process, NULL);
  }
}

/***************************************************************************/ /**
 * Ensure an RX frame is available in the RX context.
 ******************************************************************************/
static bool cpc_drv_uart_acquire_rx_frame(cpc_drv_uart_rx_ctx_t *rx_ctx, sl_cpc_bus_t *bus)
{
  sl_cpc_frame_t *frame = rx_ctx->frame;

  if (frame != NULL) {
    return true;
  }

  frame = sli_cpc_frame_new(bus, true);
  if (frame == NULL) {
    MCU_ATOMIC_SECTION(cpc_drv_uart_rx_waiting_for_frame = true;);
    return false;
  }

  MCU_ATOMIC_SECTION(cpc_drv_uart_rx_waiting_for_frame = false;);

  rx_ctx->frame = frame;

  return true;
}

/***************************************************************************/ /**
 * Set the RX state machine state and reset the byte index for the new phase.
 ******************************************************************************/
static void cpc_drv_uart_set_rx_state(cpc_drv_uart_rx_ctx_t *rx_ctx, cpc_drv_uart_rx_state_t state)
{
  rx_ctx->state = state;
  rx_ctx->index = 0;
}

/***************************************************************************/ /**
 * Flush UART bytes left over from an abandoned frame; returns false while paused.
 ******************************************************************************/
static bool cpc_drv_uart_flush_bytes(cpc_drv_uart_rx_ctx_t *rx_ctx)
{
  size_t bytes_read = 0;
  sl_status_t status;
  uint8_t byte;

  while (rx_ctx->flush_bytes_remaining > 0U) {
    status = sl_iostream_read(to_drv(s_bus)->iostream, &byte, sizeof(byte), &bytes_read);
    switch (status) {
      case SL_STATUS_OK:
        if (bytes_read == 0U) {
          return false;
        }
        rx_ctx->flush_bytes_remaining--;
        break;
      case SL_STATUS_EMPTY:
        return false;
      default:
        SLI_CPC_LOG_ERROR("[UART] Flush read failed: 0x%lx, abandoning flush", (unsigned long)status);
        rx_ctx->flush_bytes_remaining = 0;
        return true;
    }
  }

  return true;
}

/***************************************************************************/ /**
 * Count payload/CRC bytes still unread on UART for an abandoned frame.
 ******************************************************************************/
static size_t cpc_drv_uart_count_flush_bytes(const cpc_drv_uart_rx_ctx_t *rx_ctx, sl_cpc_frame_t *frame)
{
  const sli_cpc_hdr_t *hdr;
  size_t payload_remaining;
  uint16_t payload_size;

  // No frame in progress — nothing to skip on the UART stream.
  if (frame == NULL) {
    return 0;
  }

  hdr = sli_cpc_frame_get_header(frame);
  payload_size = sli_cpc_header_get_payload_size(hdr);

  // Header CRC passed but the frame is being abandoned before payload RX.
  if (rx_ctx->state == RECEIVING_HEADER_BLOCK && payload_size > 0U) {
    return payload_size + SLI_CPC_DRV_UART_CRC_SIZE;
  }

  // Payload RX was in progress — flush whatever is left plus the payload CRC.
  if (rx_ctx->state == RECEIVING_PAYLOAD) {
    payload_remaining = 0;

    if (payload_size > rx_ctx->index) {
      payload_remaining = payload_size - rx_ctx->index;
    }

    return payload_remaining + SLI_CPC_DRV_UART_CRC_SIZE;
  }

  // Payload CRC RX was in progress — flush the unread CRC byte(s).
  if (rx_ctx->state == RECEIVING_PAYLOAD_CRC && rx_ctx->index < SLI_CPC_DRV_UART_CRC_SIZE) {
    return SLI_CPC_DRV_UART_CRC_SIZE - rx_ctx->index;
  }

  // Header-only abandon or no unread tail bytes remain.
  return 0;
}

/***************************************************************************/ /**
 * Discard current RX frame and reset the RX state.
 ******************************************************************************/
static void cpc_drv_uart_discard_rx_frame(cpc_drv_uart_rx_ctx_t *rx_ctx)
{
  sl_cpc_frame_t *frame = rx_ctx->frame;
  size_t flush_bytes = 0;

  if (frame != NULL) {
    flush_bytes = cpc_drv_uart_count_flush_bytes(rx_ctx, frame);

    // Return an allocated payload buffer to the endpoint pool, if any.
    if (frame->payload != NULL) {
      sl_cpc_ep_push_recv_buf(frame->ep, frame->payload);
      frame->payload = NULL;
    }

    sli_cpc_frame_put_ref(&rx_ctx->frame);
  }

  // Queue the tail for flushing, then reset header reception state.
  rx_ctx->flush_bytes_remaining += (uint32_t)flush_bytes;
  rx_ctx->hdr_block_count = 0;
  cpc_drv_uart_set_rx_state(rx_ctx, RECEIVING_HEADER_BLOCK);
}

/***************************************************************************/ /**
 * Drop @p count bytes from the front of the header block buffer.
 ******************************************************************************/
static void cpc_drv_uart_hdr_block_drop(cpc_drv_uart_rx_ctx_t *rx_ctx, size_t count)
{
  if (count >= rx_ctx->hdr_block_count) {
    rx_ctx->hdr_block_count = 0;
    return;
  }

  rx_ctx->hdr_block_count -= (uint16_t)count;
  memmove(rx_ctx->hdr_block, rx_ctx->hdr_block + count, rx_ctx->hdr_block_count);
}

/***************************************************************************/ /**
 * Read UART data into the header block buffer until it is full or the stream is empty.
 *
 * @return true when at least one byte was read, false on empty stream or error.
 ******************************************************************************/
static bool cpc_drv_uart_fill_header_block(cpc_drv_uart_rx_ctx_t *rx_ctx)
{
  size_t bytes_read = 0;
  sl_status_t status;

  if (rx_ctx->hdr_block_count >= SLI_CPC_DRV_UART_HEADER_BLOCK_SIZE) {
    return true;
  }

  status = sl_iostream_read(to_drv(s_bus)->iostream, rx_ctx->hdr_block + rx_ctx->hdr_block_count,
                            SLI_CPC_DRV_UART_HEADER_BLOCK_SIZE - rx_ctx->hdr_block_count, &bytes_read);
  switch (status) {
    case SL_STATUS_OK:
      break;
    case SL_STATUS_EMPTY:
      return false;
    default:
      rx_ctx->hdr_block_count = 0;
      return false;
  }

  rx_ctx->hdr_block_count += (uint16_t)bytes_read;
  return bytes_read > 0U;
}

/***************************************************************************/ /**
 * Scan hdr_block for the first sync (0xEB) byte.
 ******************************************************************************/
static size_t cpc_drv_uart_hdr_block_scan_preamble(const cpc_drv_uart_rx_ctx_t *rx_ctx)
{
  size_t preamble_idx;

  for (preamble_idx = 0; preamble_idx < rx_ctx->hdr_block_count; preamble_idx++) {
    if (rx_ctx->hdr_block[preamble_idx] == SLI_CPC_DRV_UART_PREAMBLE) {
      break;
    }
  }

  return preamble_idx;
}

/***************************************************************************/ /**
 * Align on sync and validate the header CRC for the current hdr_block contents.
 ******************************************************************************/
static cpc_drv_uart_hdr_sync_result_t cpc_drv_uart_hdr_block_sync_step(cpc_drv_uart_rx_ctx_t *rx_ctx, bool got_data)
{
  uint16_t computed_crc;
  uint16_t received_crc;
  size_t preamble_idx;
  const uint8_t *hdr;

  // Locate the first candidate sync byte in the current buffer.
  preamble_idx = cpc_drv_uart_hdr_block_scan_preamble(rx_ctx);

  // No sync byte found in the bytes currently held in hdr_block.
  if (preamble_idx >= rx_ctx->hdr_block_count) {
    // No SoF exists in [0, hdr_block_count) — discard these bytes.
    if (rx_ctx->hdr_block_count > 0U) {
      cpc_drv_uart_hdr_block_drop(rx_ctx, rx_ctx->hdr_block_count);
      return CPC_DRV_UART_HDR_SYNC_RETRY;
    }

    return got_data ? CPC_DRV_UART_HDR_SYNC_RETRY : CPC_DRV_UART_HDR_SYNC_WAIT;
  }

  // Sync found after leading garbage — realign so it sits at index 0.
  if (preamble_idx > 0U) {
    cpc_drv_uart_hdr_block_drop(rx_ctx, preamble_idx);
  }

  // Sync is at index 0; wait until the full header block is assembled.
  if (rx_ctx->hdr_block_count < SLI_CPC_DRV_UART_HEADER_BLOCK_SIZE) {
    return got_data ? CPC_DRV_UART_HDR_SYNC_RETRY : CPC_DRV_UART_HDR_SYNC_WAIT;
  }

  // Full block aligned on sync — validate the header CRC.
  hdr = &rx_ctx->hdr_block[1];
  computed_crc = sli_cpc_crc(hdr, SLI_CPC_HEADER_SIZE);
  received_crc = sli_cpc_u16_from_le(&rx_ctx->hdr_block[1U + SLI_CPC_HEADER_SIZE]);
  if (computed_crc != received_crc) {
    SLI_CPC_LOG_DEBUG("[UART] Invalid header CRC, resyncing, computed=0x%04x received=0x%04x", computed_crc,
                      received_crc);
    // False sync — drop it and search again from the next byte.
    cpc_drv_uart_hdr_block_drop(rx_ctx, 1U);
    return CPC_DRV_UART_HDR_SYNC_RETRY;
  }

  // Header CRC is valid; hdr_block holds a complete header block.
  return CPC_DRV_UART_HDR_SYNC_FOUND;
}

/***************************************************************************/ /**
 * Fill hdr_block and sync on a valid header block.
 *
 * @return true when a header block with a valid CRC is aligned in hdr_block.
 ******************************************************************************/
static bool cpc_drv_uart_hdr_block_sync(cpc_drv_uart_rx_ctx_t *rx_ctx)
{
  cpc_drv_uart_hdr_sync_result_t result;
  bool got_data;

  for (;;) {
    got_data = cpc_drv_uart_fill_header_block(rx_ctx);
    if (!got_data && rx_ctx->hdr_block_count == 0U) {
      return false;
    }

    result = cpc_drv_uart_hdr_block_sync_step(rx_ctx, got_data);
    if (result == CPC_DRV_UART_HDR_SYNC_WAIT) {
      return false;
    }

    if (result == CPC_DRV_UART_HDR_SYNC_FOUND) {
      return true;
    }
  }
}

/***************************************************************************/ /**
 * Allocate an RX frame and deliver a validated header block from hdr_block.
 ******************************************************************************/
static bool cpc_drv_uart_deliver_header_block(cpc_drv_uart_rx_ctx_t *rx_ctx)
{
  sl_cpc_frame_t *frame;
  uint16_t payload_size;
  sl_status_t status;
  const uint8_t *hdr;

  // Allocate (or reuse) an RX frame object for this frame.
  if (!cpc_drv_uart_acquire_rx_frame(rx_ctx, s_bus)) {
    return false;
  }

  frame = rx_ctx->frame;
  hdr = &rx_ctx->hdr_block[1];
  // Copy the decoded CPC header out of the wire buffer.
  memcpy(sli_cpc_frame_get_header(frame), hdr, SLI_CPC_HEADER_SIZE);
  payload_size = sli_cpc_header_get_payload_size(sli_cpc_frame_get_header(frame));

  status = sli_cpc_alloc_rx_payload(s_bus, frame);

  if (status != SL_STATUS_OK) {
    if (payload_size > 0U) {
      rx_ctx->flush_bytes_remaining += (uint32_t)payload_size + SLI_CPC_DRV_UART_CRC_SIZE;
    }

    rx_ctx->hdr_block_count = 0;
    rx_ctx->frame = NULL;
    sli_cpc_frame_list_push_back(&rx_pending_frame_list, frame);
    sli_cpc_bus_notify_rx_data_from_drv(s_bus);
    cpc_drv_uart_set_rx_state(rx_ctx, RECEIVING_HEADER_BLOCK);
    return true;
  }

  if (payload_size > 0) {
    // Header block consumed; hdr_block is free for the next frame.
    rx_ctx->hdr_block_count = 0;
    cpc_drv_uart_set_rx_state(rx_ctx, RECEIVING_PAYLOAD);
    return true;
  }

  // Header-only frame.
  rx_ctx->hdr_block_count = 0;
  rx_ctx->frame = NULL;
  sli_cpc_frame_list_push_back(&rx_pending_frame_list, frame);
  sli_cpc_bus_notify_rx_data_from_drv(s_bus);
  cpc_drv_uart_set_rx_state(rx_ctx, RECEIVING_HEADER_BLOCK);

  return true;
}

/***************************************************************************/ /**
 * Receive and validate a UART header block.
 *
 * Each frame on the wire is:
 *
 *   [0xEB sync][8-byte header][2-byte header CRC]
 *
 * The receiver keeps one contiguous buffer large enough for the full block.
 * It fills the buffer, scans for the sync byte, shifts the buffer left to
 * align on that byte, fills again, and validates the header CRC. On CRC
 * failure it shifts left by one byte to drop the false sync and retries.
 *
 * @return true when another IOStream read may be useful, false when idle.
 ******************************************************************************/
static bool extract_header_block(cpc_drv_uart_rx_ctx_t *rx_ctx)
{
  if (!cpc_drv_uart_hdr_block_sync(rx_ctx)) {
    return false;
  }

  return cpc_drv_uart_deliver_header_block(rx_ctx);
}

/***************************************************************************/ /**
 * Extract payload data, returns true when another IOStream read is required
 ******************************************************************************/
static bool extract_payload(cpc_drv_uart_rx_ctx_t *rx_ctx)
{
  sl_cpc_frame_t *frame = rx_ctx->frame;
  uint8_t *payload = frame->payload->ptr;
  size_t payload_length = 0;
  size_t bytes_read = 0;
  sl_status_t status;

  payload_length = sli_cpc_header_get_payload_size(sli_cpc_frame_get_header(frame));

  status
    = sl_iostream_read(to_drv(s_bus)->iostream, &payload[rx_ctx->index], payload_length - rx_ctx->index, &bytes_read);
  switch (status) {
    case SL_STATUS_OK:
      // Continue with payload assembly.
      break;
    case SL_STATUS_EMPTY:
      // No data currently available. Keep rx_ctx state and retry later.
      return false;
    default:
      // Unexpected UART read error: drop in-progress frame and fail this pass.
      cpc_drv_uart_discard_rx_frame(rx_ctx);
      return false;
  }

  rx_ctx->index += bytes_read;

  SLI_CPC_ASSERT(rx_ctx->index <= payload_length);
  if (rx_ctx->index == payload_length) {
    cpc_drv_uart_set_rx_state(rx_ctx, RECEIVING_PAYLOAD_CRC);
    return true;
  }

  return false;
}

/***************************************************************************/ /**
 * Extract and validate the payload CRC, returns true when another IOStream read is required
 ******************************************************************************/
static bool extract_payload_crc(cpc_drv_uart_rx_ctx_t *rx_ctx)
{
  sl_cpc_frame_t *frame = rx_ctx->frame;
  size_t bytes_read = 0;
  uint16_t computed_crc;
  uint16_t received_crc;
  sl_status_t status;

  status = sl_iostream_read(to_drv(s_bus)->iostream, rx_ctx->crc_buf + rx_ctx->index,
                            SLI_CPC_DRV_UART_CRC_SIZE - rx_ctx->index, &bytes_read);
  switch (status) {
    case SL_STATUS_OK:
      break;
    case SL_STATUS_EMPTY:
      return false;
    default:
      cpc_drv_uart_discard_rx_frame(rx_ctx);
      return false;
  }

  rx_ctx->index += bytes_read;
  if (rx_ctx->index != SLI_CPC_DRV_UART_CRC_SIZE) {
    return false;
  }

  received_crc = sli_cpc_u16_from_le(rx_ctx->crc_buf);
  computed_crc = sli_cpc_crc_buf(frame->payload);
  frame->payload_csum = received_crc;
  frame->payload_csum_is_valid = computed_crc == received_crc;
  if (!frame->payload_csum_is_valid) {
    SLI_CPC_LOG_DEBUG("[UART] Invalid payload CRC, computed=0x%04x received=0x%04x", computed_crc, received_crc);
  }

  sli_cpc_frame_list_push_back(&rx_pending_frame_list, frame);
  sli_cpc_bus_notify_rx_data_from_drv(s_bus);

  rx_ctx->frame = NULL;
  rx_ctx->hdr_block_count = 0;
  cpc_drv_uart_set_rx_state(rx_ctx, RECEIVING_HEADER_BLOCK);

  return true;
}

/***************************************************************************/ /**
 * Extract either header or payload based on the current reception state.
 * Returns true when another frame could be extracted.
 ******************************************************************************/
static bool extract_frame(cpc_drv_uart_rx_ctx_t *rx_ctx)
{
  if (rx_ctx->flush_bytes_remaining > 0U && !cpc_drv_uart_flush_bytes(rx_ctx)) {
    return false;
  }

  if (rx_ctx->state == RECEIVING_HEADER_BLOCK) {
    return extract_header_block(rx_ctx);
  }

  if (rx_ctx->state == RECEIVING_PAYLOAD) {
    return extract_payload(rx_ctx);
  }

  SLI_CPC_ASSERT(rx_ctx->state == RECEIVING_PAYLOAD_CRC);
  return extract_payload_crc(rx_ctx);
}

/***************************************************************************/ /**
 * Process incoming data to assemble a complete frame.
 ******************************************************************************/
static void cpc_drv_uart_rx_process(void *data)
{
  (void)data;

  static cpc_drv_uart_rx_ctx_t rx_ctx = {.state = RECEIVING_HEADER_BLOCK, .frame = NULL, .index = 0};

  while (extract_frame(&rx_ctx))
    ;
}

sl_cpc_drv_uart_t sl_cpc_drv_uart_instances[SL_CPC_DRV_UART_INSTANCES_COUNT];
