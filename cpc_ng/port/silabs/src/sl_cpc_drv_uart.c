/***************************************************************************/ /**
 * @file sl_cpc_drv_uart.c
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
#include <string.h>

#include "sli_cpc_drv_uart.h"

#include "sl_cpc_drv_instances.h"
#include "sl_status.h"
#include "sl_uart.h"

#include "../../../src/sli_cpc_assert.h"
#include "../../../src/sli_cpc_atomic.h"
#include "../../../src/sli_cpc_bus.h"
#include "../../../src/sli_cpc_crc.h"
#include "../../../src/sli_cpc_drv.h"
#include "../../../src/sli_cpc_endianness.h"
#include "../../../src/sli_cpc_frame_list.h"
#include "../../../src/sli_cpc_hdr.h"
#include "../../../src/sli_cpc_log.h"
#include "../../../src/sli_cpc_memory.h"

/*******************************************************************************
 *********************************   DEFINES   *********************************
 ******************************************************************************/

#define SLI_CPC_DRV_UART_TX_QUEUE_SIZE 5U
#define SLI_CPC_DRV_UART_SOF 0xEB
#define SLI_CPC_DRV_UART_CRC_SIZE sizeof(uint16_t)
#define SLI_CPC_DRV_UART_HEADER_BLOCK_SIZE (1U + SLI_CPC_HEADER_SIZE + SLI_CPC_DRV_UART_CRC_SIZE)
#define SLI_CPC_UART_RX_PAYLOAD_TRASH_SIZE 64U

/*******************************************************************************
 ***************************  LOCAL VARIABLES   ********************************
 ******************************************************************************/

SLI_CPC_STATIC_ASSERT_PACKED_SIZE(struct sli_cpc_drv_uart_header, SLI_CPC_DRV_UART_HEADER_BLOCK_SIZE);

static inline struct sl_cpc_drv_uart *to_drv(sl_cpc_bus_t *bus)
{
  return container_of(bus, struct sl_cpc_drv_uart, bus);
}

/***************************************************************************/ /**
 * Acquire a free RX frame for storing the next validated header.
 * Returns a free RX frame, or NULL if no frame is currently available.
 *         In the latter case, RX is marked as waiting for a frame and will
 *         be resumed by on_rx_frame_free().
 ******************************************************************************/
static sl_cpc_frame_t *acquire_rx_frame(struct sli_cpc_drv_uart_rx *rx)
{
  sl_cpc_frame_t *frame;
  MCU_DECLARE_IRQ_STATE;

  MCU_ENTER_ATOMIC();
  frame = sli_cpc_frame_list_pop(&rx->free_frame_list);
  rx->waiting_for_frame = frame == NULL;
  MCU_EXIT_ATOMIC();

  return frame;
}

/***************************************************************************/ /**
 * Preserve the first possible next-frame SOF found in a bad header block
 * and arrange for the next read to complete that partial header.
 *
 * If no later SOF exists, discard the block and restart with an empty
 * header buffer.
 ******************************************************************************/
static void resync_header(struct sli_cpc_drv_uart_rx *rx)
{
  // Find first SOF after the initial SOF at byte 0
  uint8_t *header_bytes = (uint8_t *)&rx->header;
  const uint8_t *sof = memchr(&header_bytes[1], SLI_CPC_DRV_UART_SOF, sizeof(rx->header) - 1);

  if (sof == NULL) {
    rx->hdr_bytes_received = 0;

    return;
  }

  size_t sof_index = (size_t)(sof - header_bytes);

  // SOF found, move following bytes to start of header block
  rx->hdr_bytes_received = sizeof(rx->header) - sof_index;
  memmove(header_bytes, sof, rx->hdr_bytes_received);
}

/***************************************************************************/ /**
 * Return the TX frame to the CPC core after the UART transfer.
 ******************************************************************************/
static void notify_tx_frame_completed(sl_cpc_bus_t *bus, sl_cpc_frame_t *frame)
{
  sli_cpc_frame_list_t completed_list;

  sli_cpc_frame_list_init(&completed_list);
  sli_cpc_frame_list_push_back(&completed_list, frame);
  sli_cpc_bus_notify_tx_data_by_drv(bus, &completed_list);
}

/***************************************************************************/ /**
 * Serialize a frame into the UART driver write chain and submit its header, payload,
 * and payload CRC in order.
 *
 * A header-only frame is a single write whereas a frame with payload is a chain:
 *   [SOF + hdr + hdr_crc] -> [payload] -> [payload_crc]
 ******************************************************************************/
static void send_frame(sl_uart_handle_t *uart_handle, struct sli_cpc_drv_uart_tx *tx)
{
  const sl_cpc_frame_t *frame = tx->frame;
  sl_status_t status;

  // process_tx() ensures send_frame is protected until tx->frame is cleared again.
  SLI_CPC_ASSERT(frame != NULL);
  tx->header.sof = SLI_CPC_DRV_UART_SOF;
  memcpy(&tx->header.hdr, &frame->hdr, sizeof(tx->header.hdr));
  sli_cpc_u16_to_le(sli_cpc_crc(&tx->header.hdr, sizeof(tx->header.hdr)), tx->header.hdr_crc);

  status = sl_uart_async_write(uart_handle, &tx->header, sizeof(tx->header));
  SLI_CPC_ASSERT(status == SL_STATUS_OK);

  if (frame->payload != NULL) {
    status = sl_uart_async_write(uart_handle, frame->payload->ptr, frame->payload->len);
    SLI_CPC_ASSERT(status == SL_STATUS_OK);
    status = sl_uart_async_write(uart_handle, frame->payload_csum_le, sizeof(frame->payload_csum_le));
    SLI_CPC_ASSERT(status == SL_STATUS_OK);
  }
}

/***************************************************************************/ /**
 * Map the current RX state to the next UART read.
 *
 * Normally a header read starts at the beginning of header. After a header
 * error, resync_header() may preserve a suffix beginning with
 * a new SOF. In that case, start after the preserved bytes and read
 * only the remaining bytes needed to complete the header block.
 ******************************************************************************/
static void arm_rx(sl_uart_handle_t *uart_handle, struct sli_cpc_drv_uart_rx *rx)
{
  static uint8_t sli_cpc_uart_rx_payload_trash[SLI_CPC_UART_RX_PAYLOAD_TRASH_SIZE];
  void *buf = NULL;
  size_t len = 0;

#if defined(SL_CATALOG_CPC_NG_PM_RUNTIME_PRESENT)
  if (rx->suspended) {
    return;
  }
#endif

  SLI_CPC_ASSERT(!rx->waiting_for_frame);

  switch (rx->state) {
    case SLI_CPC_DRV_UART_RX_STATE_RECEIVING_HEADER_BLOCK:
      // On resynced header block, the rx context is partly filled with received header bytes.
      // Read only the remaining bytes to complete the header resync block.
      buf = (uint8_t *)&rx->header + rx->hdr_bytes_received;
      len = sizeof(rx->header) - rx->hdr_bytes_received;
      break;

    case SLI_CPC_DRV_UART_RX_STATE_RECEIVING_PAYLOAD:
      SLI_CPC_ASSERT(rx->frame != NULL && rx->frame->payload != NULL);
      buf = rx->frame->payload->ptr;
      len = rx->frame->payload->len;
      break;

    case SLI_CPC_DRV_UART_RX_STATE_DISCARDING_PAYLOAD:
      // CPC-3531: Temporarily dump the payload in chunks until sli_uart_async_rx_discard is supported.
      buf = sli_cpc_uart_rx_payload_trash;
      len = rx->payload_bytes_remaining < sizeof(sli_cpc_uart_rx_payload_trash) ? rx->payload_bytes_remaining
                                                                                : sizeof(sli_cpc_uart_rx_payload_trash);
      break;

    case SLI_CPC_DRV_UART_RX_STATE_RECEIVING_PAYLOAD_CRC:
      buf = rx->frame->payload_csum_le;
      len = sizeof(rx->frame->payload_csum_le);
      break;

    default:
      return;
  }

  sl_status_t status = sl_uart_async_read(uart_handle, buf, len);
  SLI_CPC_ASSERT(status == SL_STATUS_OK);
}

/***************************************************************************/ /**
 * Recover from an invalid header by resyncing to the next SOF if available.
 ******************************************************************************/
static void on_header_error(sl_uart_handle_t *uart_handle, struct sli_cpc_drv_uart_rx *rx)
{
  // Discard the invalid header while preserving any possible next frame.
  resync_header(rx);
  rx->state = SLI_CPC_DRV_UART_RX_STATE_RECEIVING_HEADER_BLOCK;
  arm_rx(uart_handle, rx);
}

/***************************************************************************/ /**
 * Notify the core of the frame with an incomplete/aborted payload transfer by
 * setting the frame status to SL_STATUS_RECEIVE.
 ******************************************************************************/
static void on_payload_error(sl_uart_handle_t *uart_handle, struct sl_cpc_drv_uart *drv)
{
  struct sli_cpc_drv_uart_rx *rx = &drv->rx;
  // Return the frame so the core can account for the incomplete reception.
  if (rx->frame != NULL) {
    rx->frame->status = SL_STATUS_RECEIVE;
    sli_cpc_frame_list_push_back(&rx->pending_frame_list, rx->frame);
    rx->frame = NULL;
    sli_cpc_bus_notify_rx_data_from_drv(&drv->bus);
  }

  // Reset the receiver state and rearm the receiver
  rx->state = SLI_CPC_DRV_UART_RX_STATE_RECEIVING_HEADER_BLOCK;
  rx->hdr_bytes_received = 0;
  rx->payload_bytes_remaining = 0;
  arm_rx(uart_handle, rx);
}

/***************************************************************************/ /**
 * Process a validated header once an RX frame is available.
 *
 * If no frame is available, the complete header remains in header and
 * hdr_bytes_received remains unchanged so it can be resumed later.
 ******************************************************************************/
static bool process_header(struct sl_cpc_drv_uart *drv)
{
  struct sli_cpc_drv_uart_rx *rx = &drv->rx;
  sl_cpc_frame_t *frame;
  uint16_t payload_size;

  frame = acquire_rx_frame(rx);
  if (frame == NULL) {
    return false;
  }

  rx->hdr_bytes_received = 0;
  rx->frame = frame;

  memcpy(&rx->frame->hdr, &rx->header.hdr, sizeof(rx->frame->hdr));
  payload_size = sli_cpc_header_get_payload_size(&rx->frame->hdr);

  if (payload_size == 0) {
    // No payload, push the frame to the pending list
    sli_cpc_frame_list_push_back(&rx->pending_frame_list, rx->frame);
    rx->frame = NULL;
    rx->state = SLI_CPC_DRV_UART_RX_STATE_RECEIVING_HEADER_BLOCK;
    sli_cpc_bus_notify_rx_data_from_drv(&drv->bus);

    return true;
  }

  if (sli_cpc_alloc_rx_payload(&drv->bus, rx->frame) != SL_STATUS_OK) {
    // Failed to allocate payload, drop the coming payload bytes.
    rx->frame->status = SL_STATUS_RECEIVE;
    rx->payload_bytes_remaining = payload_size;
    rx->state = SLI_CPC_DRV_UART_RX_STATE_DISCARDING_PAYLOAD;

    return true;
  }

  // Header valid, receive payload
  rx->state = SLI_CPC_DRV_UART_RX_STATE_RECEIVING_PAYLOAD;

  return true;
}

/***************************************************************************/ /**
 * Send a frame as chunks through the UART driver.
 ******************************************************************************/
static void process_tx(sl_uart_handle_t *uart_handle, struct sl_cpc_drv_uart *drv)
{
  sl_cpc_frame_t *frame;
  struct sli_cpc_drv_uart_tx *tx = &drv->tx;
  MCU_DECLARE_IRQ_STATE;

  // Atomically set tx->frame if it is not already NULL (active transfer)
  // or the pending frame list is empty.
  MCU_ENTER_ATOMIC();
  frame = NULL;
  if (tx->frame == NULL) {
    frame = sli_cpc_frame_list_pop(&tx->pending_frame_list);
    if (frame != NULL) {
      tx->frame = frame;
    }
  }
  MCU_EXIT_ATOMIC();

  // send_frame is only called if the tx->frame validations above are successful.
  if (frame != NULL) {
    send_frame(uart_handle, tx);
  }
}

/***************************************************************************/ /**
 * Move completed RX frames to the CPC core after a sli_cpc_bus_notify_rx_data_from_drv.
 * Payload CRC is validated here so it's outside of the IRQ context during RX processing.
 ******************************************************************************/
static sl_status_t read_data(sl_cpc_bus_t *bus, sli_cpc_frame_list_t *frames)
{
  struct sl_cpc_drv_uart *drv = to_drv(bus);
  sl_cpc_frame_t *frame;

  MCU_ATOMIC_SECTION(sli_cpc_frame_list_extend(frames, &drv->rx.pending_frame_list);)

  if (SL_BRANCH_UNLIKELY(sli_cpc_frame_list_empty(frames))) {
    return SL_STATUS_EMPTY;
  }

  SLI_CPC_FRAME_LIST_FOR_EACH(frames, frame)
  {
    if (frame->payload != NULL) {
      if (frame->status == SL_STATUS_RECEIVE) {
        // An invalid payload is marked with SL_STATUS_RECEIVE.
        // Set CRC as invalid so the core handles the failure.
        frame->payload_csum_is_valid = false;
      } else {
        // CRC validation is done outside of the IRQ
        frame->payload_csum_is_valid = sli_cpc_crc_buf(frame->payload) == sli_cpc_u16_from_le(frame->payload_csum_le);
      }
    }
  }

  return SL_STATUS_OK;
}

/***************************************************************************/ /**
 * Arm the first header-block read. Subsequent reads are driven
 * by the RX state machine and UART driver callbacks.
 ******************************************************************************/
static sl_status_t start_rx(sl_cpc_bus_t *bus)
{
  struct sl_cpc_drv_uart *drv = to_drv(bus);

  drv->rx.rx_started = true;
  arm_rx(&drv->uart_handle, &drv->rx);

  return SL_STATUS_OK;
}

/***************************************************************************/ /**
 * Get the number of available slots for write in the TX queue.
 ******************************************************************************/
static uint32_t get_available_write_frame_slots(sl_cpc_bus_t *bus)
{
  const struct sl_cpc_drv_uart *drv = to_drv(bus);
  uint32_t num_available_slots;
  uint32_t active_count;
  const struct sli_cpc_drv_uart_tx *tx = &drv->tx;

  MCU_ATOMIC_SECTION(active_count = tx->frame == NULL ? 0 : 1;
                     num_available_slots = (SLI_CPC_DRV_UART_TX_QUEUE_SIZE
                                            - sli_cpc_frame_list_get_len(&tx->pending_frame_list) - active_count);)

  return num_available_slots;
}

/***************************************************************************/ /**
 * Called when the core queues new frames for write.
 ******************************************************************************/
static uint32_t write(sl_cpc_bus_t *bus, sli_cpc_frame_list_t *frames)
{
  struct sl_cpc_drv_uart *drv = to_drv(bus);
  uint32_t num_frames = sli_cpc_frame_list_get_len(frames);
  sl_cpc_frame_t *frame;

  if (get_available_write_frame_slots(bus) < num_frames) {
    // Core attempted to send more frames than the driver can take. In emulation, we don't expect
    // this to ever happen.
    SLI_CPC_ASSERT(false);
    return 0;
  }

  SLI_CPC_FRAME_LIST_FOR_EACH(frames, frame)
  {
    if (frame->payload != NULL) {
      sli_cpc_u16_to_le(sli_cpc_crc_buf(frame->payload), frame->payload_csum_le);
    }
  }

  MCU_ATOMIC_SECTION(sli_cpc_frame_list_extend(&drv->tx.pending_frame_list, frames);)

  process_tx(&drv->uart_handle, drv);

  return num_frames;
}

/***************************************************************************/ /**
 * Resume RX when the CPC core releases a frame that was held because no RX
 * frame buffer was available.
 ******************************************************************************/
static void on_rx_frame_free(sl_cpc_bus_t *bus)
{
  struct sl_cpc_drv_uart *drv = to_drv(bus);
  sl_cpc_frame_t *frame;
  bool rx_was_waiting_for_frame;
  MCU_DECLARE_IRQ_STATE;

  frame = sli_cpc_frame_new(&drv->bus, true);
  if (frame == NULL) {
    return;
  }

  MCU_ENTER_ATOMIC();
  sli_cpc_frame_list_push_back(&drv->rx.free_frame_list, frame);
  rx_was_waiting_for_frame = drv->rx.waiting_for_frame;
  if (drv->rx.waiting_for_frame) {
    drv->rx.waiting_for_frame = false;
  }
  MCU_EXIT_ATOMIC();

  if (rx_was_waiting_for_frame && process_header(drv)) {
    // RX waited on the RX frame, resume now that the frame is available.
    arm_rx(&drv->uart_handle, &drv->rx);
  }
}

/***************************************************************************/ /**
 * Consume completion callbacks of the UART driver writes for a frame.
 *
 * A payload frame consists of multiple UART writes, so intermediate write
 * completions are ignored. The final write completion notifies the frame to
 * the core.
   ******************************************************************************/
static void on_async_tx_complete(sl_uart_handle_t *uart_handle, const void *data, size_t size, void *user_data,
                                 bool aborted)
{
  struct sl_cpc_drv_uart *drv = user_data;
  struct sli_cpc_drv_uart_tx *tx = &drv->tx;
  sl_cpc_frame_t *frame;
  (void)size;

  SLI_CPC_ASSERT(tx->frame != NULL);
  frame = tx->frame;

  if (aborted) {
    frame->status = SL_STATUS_TRANSMIT_INCOMPLETE;
  } else if (frame->payload != NULL && data != frame->payload_csum_le) {
    // If the frame has a payload, ignore intermediate write completions until the CRC callback.
    return;
  }

  tx->frame = NULL;

  notify_tx_frame_completed(&drv->bus, frame);
  process_tx(uart_handle, drv);
}

/***************************************************************************/ /**
 * Callback when the armed RX is completed and the buffer is released.
 * Handles the data processing and RX state machine.
 ******************************************************************************/
static void on_async_rx_event(sl_uart_handle_t *uart_handle, void *data, size_t size, void *user_data,
                              sl_uart_async_rx_event_t rx_event)
{
  struct sl_cpc_drv_uart *drv = user_data;
  struct sli_cpc_drv_uart_rx *rx = &drv->rx;

  (void)data;

  if ((rx_event & SL_UART_ASYNC_RX_EVENT_BUF_RELEASED) == 0) {
    return;
  }

  if (rx_event & SL_UART_ASYNC_RX_EVENT_ABORTED) {
    if (rx->state == SLI_CPC_DRV_UART_RX_STATE_RECEIVING_HEADER_BLOCK) {
      on_header_error(uart_handle, rx);
    } else {
      on_payload_error(uart_handle, drv);
    }
    return;
  }

  switch (rx->state) {
    case SLI_CPC_DRV_UART_RX_STATE_RECEIVING_HEADER_BLOCK:
      if (rx->header.sof != SLI_CPC_DRV_UART_SOF) {
        on_header_error(uart_handle, rx);
        return;
      }
      // Validate header CRC
      uint16_t header_crc = sli_cpc_u16_from_le(rx->header.hdr_crc);
      uint16_t computed_header_crc = sli_cpc_crc(&rx->header.hdr, sizeof(rx->header.hdr));
      if (header_crc != computed_header_crc) {
        on_header_error(uart_handle, rx);
        return;
      }
      rx->hdr_bytes_received = sizeof(rx->header);

      // Claim a RX frame for the validated header. If none is available,
      // keep the completed header and resume from on_rx_frame_free().
      if (!process_header(drv)) {
        return;
      }
      break;

    case SLI_CPC_DRV_UART_RX_STATE_RECEIVING_PAYLOAD:
      rx->state = SLI_CPC_DRV_UART_RX_STATE_RECEIVING_PAYLOAD_CRC;
      break;

    case SLI_CPC_DRV_UART_RX_STATE_DISCARDING_PAYLOAD:
      SLI_CPC_ASSERT(rx->payload_bytes_remaining >= size);
      rx->payload_bytes_remaining -= (uint16_t)size;
      if (rx->payload_bytes_remaining == 0) {
        rx->state = SLI_CPC_DRV_UART_RX_STATE_RECEIVING_PAYLOAD_CRC;
      }
      break;

    case SLI_CPC_DRV_UART_RX_STATE_RECEIVING_PAYLOAD_CRC:
      sli_cpc_frame_list_push_back(&rx->pending_frame_list, rx->frame);
      rx->frame = NULL;
      rx->state = SLI_CPC_DRV_UART_RX_STATE_RECEIVING_HEADER_BLOCK;
      rx->payload_bytes_remaining = 0;
      sli_cpc_bus_notify_rx_data_from_drv(&drv->bus);
      break;

    default:
      SLI_CPC_ASSERT(false);
      rx->state = SLI_CPC_DRV_UART_RX_STATE_RECEIVING_HEADER_BLOCK;
      break;
  }

  arm_rx(uart_handle, rx);
}

#if defined(SL_CATALOG_CPC_NG_PM_RUNTIME_PRESENT)
sl_status_t pm_runtime_resume(sl_cpc_bus_t *bus)
{
  struct sl_cpc_drv_uart *drv = to_drv(bus);

  sl_uart_resume(&drv->uart_handle);

  drv->rx.suspended = false;

  if (drv->rx.rx_started) {
    arm_rx(&drv->uart_handle, &drv->rx);
  }

  return SL_STATUS_OK;
}

void pm_runtime_suspend(sl_cpc_bus_t *bus)
{
  struct sl_cpc_drv_uart *drv = to_drv(bus);

  drv->rx.suspended = true;

  sl_uart_async_disable_rx(&drv->uart_handle);
  sl_uart_suspend(&drv->uart_handle);
}
#endif

/***************************************************************************/ /**
 * Initialize the UART peripheral owned by this driver.
 ******************************************************************************/
static sl_status_t init_uart(struct sl_cpc_drv_uart *drv, const struct sl_cpc_drv_uart_config *cfg)
{
  static const sl_uart_async_config_t async_config = SL_UART_ASYNC_CONFIG_DEFAULT;
  sl_status_t status;
  const sl_uart_config_t line = {
    .baudrate = cfg->baudrate,
    .parity = SL_UART_PARITY_NONE,
    .stop_bits = SL_UART_STOP_BITS_1,
    .data_bits = SL_UART_DATA_BITS_8,
    .flow_control = cfg->flow_control,
    .oversampling = cfg->oversampling,
  };

  status = sl_uart_init(&drv->uart_handle, cfg->peripheral, &cfg->pins, &async_config);
  if (status != SL_STATUS_OK) {
    return status;
  }

  status = sl_uart_configure_line(&drv->uart_handle, &line);
  if (status != SL_STATUS_OK) {
    (void)sl_uart_deinit(&drv->uart_handle);
    return status;
  }

  sl_uart_async_set_tx_complete_callback(&drv->uart_handle, &on_async_tx_complete, drv);
  sl_uart_async_set_rx_event_callback(&drv->uart_handle, &on_async_rx_event, drv);

  return SL_STATUS_OK;
}

/***************************************************************************/ /**
 * Initialize the UART peripheral and bind the CPC bus to this driver.
 ******************************************************************************/
sl_status_t sli_cpc_drv_uart_init(struct sl_cpc_drv_uart *drv, const struct sl_cpc_drv_uart_config *cfg,
                                  const sl_cpc_bus_config_t *bus_cfg)
{
  static const sli_cpc_drv_ops_t ops = {
    .start_rx = &start_rx,
    .read = &read_data,
    .write = &write,
    .get_available_write_frame_slots = &get_available_write_frame_slots,
    .on_rx_frame_free = &on_rx_frame_free,
#if defined(SL_CATALOG_CPC_NG_PM_RUNTIME_PRESENT)
    .pm_runtime_resume = &pm_runtime_resume,
    .pm_runtime_suspend = &pm_runtime_suspend,
#endif
  };

  sl_cpc_frame_t *frame;
  sl_status_t status;

  SLI_CPC_ASSERT(drv != NULL);

  if (cfg == NULL || bus_cfg == NULL || cfg->peripheral == NULL) {
    return SL_STATUS_NULL_POINTER;
  }

  memset(drv, 0, sizeof(*drv));

  status = init_uart(drv, cfg);
  if (status != SL_STATUS_OK) {
    return status;
  }

  sli_cpc_frame_list_init(&drv->rx.free_frame_list);
  sli_cpc_frame_list_init(&drv->rx.pending_frame_list);
  sli_cpc_frame_list_init(&drv->tx.pending_frame_list);

#if defined(SL_CATALOG_CPC_NG_PM_RUNTIME_PRESENT)
  sl_uart_suspend(&drv->uart_handle);
  drv->rx.suspended = true;
#endif

  status = sli_cpc_bus_init(&drv->bus, bus_cfg, &ops);
  if (status != SL_STATUS_OK) {
    sl_uart_deinit(&drv->uart_handle);
    return status;
  }

  while ((frame = sli_cpc_frame_new(&drv->bus, true)) != NULL) {
    sli_cpc_frame_list_push_back(&drv->rx.free_frame_list, frame);
  }

  return SL_STATUS_OK;
}
