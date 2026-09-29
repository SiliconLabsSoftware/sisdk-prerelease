/***************************************************************************/ /**
 * @file
 * @brief CPC Host SDIO Driver implementation.
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

#include "sl_cpc_drv_sdio_host.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "../../../src/sli_cpc_bus.h"
#include "../../../src/sli_cpc_dispatcher.h"
#include "../../../src/sli_cpc_drv.h"
#include "../../../src/sli_cpc_frame_list.h"
#include "../../../src/sli_cpc_log.h"
#include "sl_clock_manager.h"
#include "sl_cpc_drv_instances.h"
#include "sl_device_gpio.h"
#include "sl_device_peripheral.h"
#include "sl_hal_gpio.h"
#include "sl_hal_sdhc.h"
#include "sl_sdhc_sdio.h"
#include "sl_sdhc_spec.h"
#include "sl_slist.h"
#include "sl_status.h"
#include "sli_sdhc.h"

/*******************************************************************************
 ******************************  DEFINES ***************************************
 ******************************************************************************/
// Kept local: no HAL/spec macros for these yet.
#define SDIO_CARD_INT_ID_REG_ADDR 0x08U   ///< FN1 interrupt identification.
#define SDIO_CARD_INT_EN_REG_ADDR 0x09U   ///< FN1 interrupt enable.
#define SDIO_CARD_DATA_READY_FLAG 0x01U   ///< INT_ID/EN: read-data-ready (RDDATRDY).
#define SDIO_XFER_COUNT_REG_ADDR 0x0CU    ///< FN1 advertised CMD53 transfer length.
#define SDIO_CCCR_CARD_CAP_E4MI 0x20U     ///< CCCR Card Cap bit5 (E4MI).
#define SDIO_CLOCK_WAKE_REG_ADDR 0x18000U ///< FN0 auto clock-wake (DAT1 card IRQ).
#define SL_CPC_DRV_SDIO_FUNCTION_0 0U     ///< FN0 function number.

#define CPC_DRV_SDIO_DESC_COUNT SL_CPC_DRV_SDIO_HOST_RX_FRAME_POOL_COUNT
#define CPC_SDIO_DEFAULT_AGGREGATION SL_CPC_DRV_SDIO_HOST_RX_FRAME_POOL_COUNT
static_assert(CPC_DRV_SDIO_DESC_COUNT >= 3, "Need room for unaligned payload + trailing flush descriptors");
static_assert(CPC_DRV_SDIO_DESC_COUNT == SL_CPC_DRV_SDIO_HOST_RX_FRAME_POOL_COUNT, "RX frame pool count mismatch");
static_assert(CPC_SDIO_DEFAULT_AGGREGATION < 256, "Aggregation too large");
static_assert(SL_CPC_DRV_SDIO_HOST_RX_FRAME_POOL_COUNT >= CPC_SDIO_DEFAULT_AGGREGATION,
              "RX frame pool count is too small");

/// Round a payload length up to the ADMA 4-byte alignment.
#define ALIGNED_PAYLOAD_SIZE(x) (SL_DIV_ROUND_UP((x), SL_CPC_BUF_MIN_ALIGNMENT) * SL_CPC_BUF_MIN_ALIGNMENT)

/*******************************************************************************
 ******************************  TYPES *****************************************
 ******************************************************************************/

SLI_CPC_STATIC_ASSERT_PACKED_SIZE(struct sli_cpc_drv_caps, 1);

/*******************************************************************************
 ****************************** PRIVATE FUNCTIONS ******************************
 ******************************************************************************/

/***************************************************************************/ /**
 * Initialize SDHC host pins: clear shared SDIO device route, enable SDHC route,
 * and set pin modes/drive (CLK push-pull; CMD/DAT open-drain + pull-up).
 ******************************************************************************/
static void primary_sdio_init_pins(const sl_sdhc_sdio_handle_t *sdio_handle)
{
  const sl_sdhc_gpio_config_t *gpio = &sdio_handle->sdhc_controller.gpio_config;
  sl_gpio_t pins[] = {
    gpio->clk, gpio->cmd, gpio->dat0, gpio->dat1, gpio->dat2, gpio->dat3,
  };
  uint8_t max_drive;

#if defined(GPIO_SDIO0PRI2ROUTEPEN)
  // SDIO device routing shares the same PE0-PE5 pins; host must own them alone.
  GPIO->SDIO0PRI2ROUTEPEN = 0U;
#endif

#if defined(GPIO_SDHC0PRI1ROUTEPEN_SDHC0PRI1DATA0ROUTEPEN)
  GPIO->SDHC0PRI1ROUTEPEN
    = (GPIO_SDHC0PRI1ROUTEPEN_SDHC0PRI1DATA0ROUTEPEN | GPIO_SDHC0PRI1ROUTEPEN_SDHC0PRI1DATA1ROUTEPEN
       | GPIO_SDHC0PRI1ROUTEPEN_SDHC0PRI1DATA2ROUTEPEN | GPIO_SDHC0PRI1ROUTEPEN_SDHC0PRI1DATA3ROUTEPEN
       | GPIO_SDHC0PRI1ROUTEPEN_SDHC0PRI1CMDROUTEPEN | GPIO_SDHC0PRI1ROUTEPEN_SDHC0PRI1CLKROUTEPEN);
#endif

  max_drive = (uint8_t)(_HSIO_P_DRVSTRENGTH_DRVSTRENGTH0_MASK >> _HSIO_P_DRVSTRENGTH_DRVSTRENGTH0_SHIFT);
  for (size_t i = 0; i < SL_ARRAY_SIZE(pins); i++) {
    sl_hal_gpio_set_pin_mode(&pins[i], (i == 0U) ? SL_GPIO_MODE_PUSH_PULL : SL_GPIO_MODE_WIRED_AND_PULLUP, true);
    sl_hal_gpio_set_drive_strength(&pins[i], max_drive);
  }
}

/***************************************************************************/ /**
 * Start the transfer.
 *
 * ADMA descriptors should be prepared before calling this function.
 * The ADMA cannot be refilled mid-transfer.
 ******************************************************************************/
static void transfer_start(sl_cpc_drv_sdio_host_t *drv, adma_state_t next_state, size_t xfer_len, bool is_write)
{
  sl_sdhc_sdio_adma_config_t adma_config;
  sl_status_t status;
  uint32_t block_count;
  MCU_DECLARE_IRQ_STATE;

  // Block-mode CMD53: xfer.size is block count (payload remains a contiguous byte stream on DAT).
  SLI_CPC_ASSERT((xfer_len % drv->block_size) == 0U);
  block_count = (uint32_t)(xfer_len / drv->block_size);
  SLI_CPC_ASSERT(block_count > 0U);

  MCU_ATOMIC_STORE(drv->adma_state, next_state);

  // xfer.buffer is required by the SDHC API even for ADMA (null-check); descriptors carry the SG list.
  memset(&adma_config, 0, sizeof(adma_config));
  adma_config.xfer.func_num = drv->function_num;
  adma_config.xfer.reg_addr = 0U;
  adma_config.xfer.xfer_mode = SL_SDHC_SDIO_BLOCK_MODE_FIXED_ADDR;
  adma_config.xfer.buffer = (uint8_t *)&drv->sdio_hdr_block;
  adma_config.xfer.data_timeout_ms = 1000U;
  adma_config.descriptor_table = drv->frame_descriptors;
  adma_config.xfer.size = block_count;
  adma_config.enable_dma_interrupt = true;

  if (is_write) {
    status = sl_sdhc_sdio_write_extended_adma(drv->sdio_handle, &adma_config);
  } else {
    status = sl_sdhc_sdio_read_extended_adma(drv->sdio_handle, &adma_config);
  }

  if (status != SL_STATUS_OK) {
    SLI_CPC_LOG_ERROR("Failed to start ADMA %s, state: %u status: 0x%lx", is_write ? "write" : "read", next_state,
                      (unsigned long)status);

    // Nothing reached the bus, so undo what was staged for this CMD53.
    MCU_ENTER_ATOMIC();
    if (next_state == ADMA_TRANSMIT) {
      // Frames are queued for completion notification; requeue them instead so
      // the next start_pending_tx() retries and they are never reported as sent.
      sli_cpc_frame_list_extend(&drv->tx_header_pending_frames, &drv->tx_pending_xfer_complete_frames);
    } else {
      // on_card_interrupt() already acked FN1 INT_ID; re-latch so it retries.
      drv->card_int_pending = true;
    }
    drv->adma_state = ADMA_IDLE;
    MCU_EXIT_ATOMIC();
  }
}

/***************************************************************************/ /**
 * Prepare the payload descriptors for the TX transfer.
 ******************************************************************************/
static void adma_tx_prepare_payload_descriptors(sl_cpc_drv_sdio_host_t *drv, size_t idx)
{
  sl_cpc_frame_t *frame = NULL;
  size_t unaligned_len = 0;
  size_t aligned_len = 0;
  size_t payload_len = 0;
  uint32_t dword_idx = 0;
  uint32_t list_len = 0;
  size_t xfer_len = 0;
  size_t flush_len = 0;

  // Prepare the aggregate payload block.
  list_len = sli_cpc_frame_list_get_len(&drv->tx_payload_pending_frames);
  if (!list_len || (drv->sdio_hdr_block.frame_count == 0U)) {
    SLI_CPC_LOG_ERROR("TX - No frames to send");
    SLI_CPC_ASSERT(false);
    return;
  }

  for (uint32_t i = 0; i < drv->sdio_hdr_block.frame_count; i++) {
    // Reserve room for an unaligned payload descriptor and the trailing flush
    // descriptor (both may be appended after the aligned part).
    if (idx >= (CPC_DRV_SDIO_DESC_COUNT - 2U)) {
      // No more descriptors; send what is already prepared.
      break;
    }

    // Pop the frame from the header pending list and push it to the payload pending list.
    frame = sli_cpc_frame_list_pop(&drv->tx_payload_pending_frames);
    if (frame == NULL) {
      SLI_CPC_LOG_WARN("TX - Frame count exceeds list length");
      // This should never happen, as we should pop less or equal to the frame count.
      SLI_CPC_ASSERT(false);
      return;
    }

    payload_len = sli_cpc_header_get_payload_size(sli_cpc_frame_get_header(frame));
    if (payload_len > 0U) {
      uint8_t *payload;

      SLI_CPC_ASSERT(frame->payload != NULL);
      payload = frame->payload->ptr;

      unaligned_len = payload_len % SL_CPC_BUF_MIN_ALIGNMENT;
      aligned_len = payload_len - unaligned_len;

      xfer_len += ALIGNED_PAYLOAD_SIZE(payload_len);

      if (aligned_len > 0U) {
        drv->frame_descriptors[idx++] = SL_HAL_SDHC_DMA_DESCRIPTOR_XFER_DMA_INTERRUPT(payload, aligned_len);
      }

      if (unaligned_len > 0U) {
        SLI_CPC_ASSERT(dword_idx < CPC_DRV_SDIO_DESC_COUNT);
        drv->unaligned_payload_pool[dword_idx] = 0;
        memcpy(&drv->unaligned_payload_pool[dword_idx], &payload[aligned_len], unaligned_len);
        drv->frame_descriptors[idx++] = SL_HAL_SDHC_DMA_DESCRIPTOR_XFER_DMA_INTERRUPT(
          &drv->unaligned_payload_pool[dword_idx], sizeof(drv->unaligned_payload_pool[dword_idx]));
        dword_idx++;
      }
    }
    sli_cpc_frame_list_push_back(&drv->tx_pending_xfer_complete_frames, frame);
  }

  // Frames that did not fit go back to the header queue for a later CMD53.
  if (!sli_cpc_frame_list_empty(&drv->tx_payload_pending_frames)) {
    uint32_t prepared = sli_cpc_frame_list_get_len(&drv->tx_pending_xfer_complete_frames);

    sli_cpc_frame_list_extend(&drv->tx_header_pending_frames, &drv->tx_payload_pending_frames);
    if (prepared == 0U) {
      MCU_ATOMIC_STORE(drv->adma_state, ADMA_IDLE);
      return;
    }
    drv->sdio_hdr_block.frame_count = (uint8_t)prepared;
  }

  xfer_len += sizeof(struct sdio_hdr_block) + sizeof(sli_cpc_hdr_t);

  // Pad to the next block size. When already aligned, do not append a full extra block.
  flush_len = xfer_len % drv->block_size;
  if (flush_len != 0U) {
    flush_len = drv->block_size - flush_len;
    if (idx >= CPC_DRV_SDIO_DESC_COUNT) {
      SLI_CPC_LOG_ERROR("TX - No descriptor slot for flush");
      return;
    }
    drv->frame_descriptors[idx++] = SL_HAL_SDHC_DMA_DESCRIPTOR_XFER_DMA_END_INTERRUPT(drv->flush_buff, flush_len);
    xfer_len += flush_len;
  } else {
    // Last payload/header descriptor must End so the host ADMA engine completes.
    SLI_CPC_ASSERT(idx > 0U);
    drv->frame_descriptors[idx - 1U].xfer.end_ifs = 1U;
  }

  transfer_start(drv, ADMA_TRANSMIT, xfer_len, true);
}

/***************************************************************************/ /**
 * Prepare the header descriptors for the TX transfer.
 ******************************************************************************/
static void adma_tx_prepare_header_descriptors(sl_cpc_drv_sdio_host_t *drv, size_t idx)
{
  sl_cpc_frame_t *frame = NULL;
  uint32_t list_len = 0;
  uint32_t max_aggregation = 0;

  // Prepare the aggregate header block.
  list_len = sli_cpc_frame_list_get_len(&drv->tx_header_pending_frames);
  if (!list_len || (drv->sdio_hdr_block.frame_count == 0U)) {
    SLI_CPC_LOG_ERROR("TX - No frames to send");
    return;
  }

  // Negotiated aggregation ceiling (unused until CPC-3336): computed so the
  // future frame_count assignment has a clear source of truth. Loop bound is
  // still drv->sdio_hdr_block.frame_count (== 1) by design.
  max_aggregation = SL_MIN(drv->local_caps.max_aggregation, drv->remote_caps.max_aggregation);
  max_aggregation = SL_MIN(max_aggregation, list_len);
  (void)max_aggregation;

  // Make sure the frame count is not greater than the list length.
  SLI_CPC_ASSERT(drv->sdio_hdr_block.frame_count <= list_len);

  for (uint32_t i = 0; i < drv->sdio_hdr_block.frame_count; i++) {
    // Pop the frame from the header pending list and push it to the payload pending list.
    frame = sli_cpc_frame_list_pop(&drv->tx_header_pending_frames);
    if (frame == NULL || idx >= CPC_DRV_SDIO_DESC_COUNT) {
      SLI_CPC_LOG_WARN("TX - Frame count exceeds list length or descriptor count");
      // This should never happen, as we should pop less or equal to the frame count.
      break;
    }

    // Add header descriptor (ADMA requires 4-byte-aligned buffer addresses).
    SLI_CPC_ASSERT(((uintptr_t)sli_cpc_frame_get_header(frame) % SL_CPC_BUF_MIN_ALIGNMENT) == 0U);
    drv->frame_descriptors[idx++]
      = SL_HAL_SDHC_DMA_DESCRIPTOR_XFER_DMA_INTERRUPT(sli_cpc_frame_get_header(frame), SLI_CPC_HEADER_SIZE);

    // Push the frame to the payload pending list.
    sli_cpc_frame_list_push_back(&drv->tx_payload_pending_frames, frame);
  }

  drv->sdio_hdr_block.frame_count = (uint8_t)sli_cpc_frame_list_get_len(&drv->tx_payload_pending_frames);
  if (drv->sdio_hdr_block.frame_count == 0U) {
    MCU_ATOMIC_STORE(drv->adma_state, ADMA_IDLE);
    return;
  }

  adma_tx_prepare_payload_descriptors(drv, idx);
}

/***************************************************************************/ /**
 * Prepare the frame count descriptor for the TX transfer.
 ******************************************************************************/
static void adma_tx_prepare_frame_count(sl_cpc_drv_sdio_host_t *drv)
{
  adma_state_t state;
  size_t idx = 0;

  MCU_ATOMIC_LOAD(state, drv->adma_state);
  SLI_CPC_ASSERT(state == ADMA_TRANSMIT);

  // Initialize the header block.
  memset(&drv->sdio_hdr_block, 0, sizeof(struct sdio_hdr_block));

  // Aggregation is negotiated via local/remote caps for forward-compatibility
  // (CPC-3336), but TX still sends one frame per CMD53 until that lands.
  // Keep frame_count at 1; do not wire max_aggregation into it yet.
  drv->sdio_hdr_block.frame_count = 1U;
  drv->frame_descriptors[idx++] = SL_HAL_SDHC_DMA_DESCRIPTOR_XFER(&drv->sdio_hdr_block, sizeof(struct sdio_hdr_block));

  adma_tx_prepare_header_descriptors(drv, idx);
}

/***************************************************************************/ /**
 * Try to start a pending TX transfer if the ADMA is idle
 * and there are no card interrupts pending.
 ******************************************************************************/
static void start_pending_tx(sl_cpc_drv_sdio_host_t *drv)
{
  bool start_tx = false;
  MCU_DECLARE_IRQ_STATE;

  MCU_ENTER_ATOMIC();
  if (drv->adma_state == ADMA_IDLE && !drv->card_int_pending
      && !sli_cpc_frame_list_empty(&drv->tx_header_pending_frames)) {
    // Claim ADMA before leaving the critical section so another context cannot
    // start a second transfer from the same pending queue.
    drv->adma_state = ADMA_TRANSMIT;
    start_tx = true;
  }
  MCU_EXIT_ATOMIC();

  if (start_tx) {
    adma_tx_prepare_frame_count(drv);
  }
}

/***************************************************************************/ /**
 * Parse drv->rx_buffer after a completed CMD53: [frame_count][header...][payload...][pad].
 * Aggregation limited to 1 for now.
 ******************************************************************************/
static void adma_rx_parse_bounce(sl_cpc_drv_sdio_host_t *drv)
{
  const struct sdio_hdr_block *hdr_block = (const struct sdio_hdr_block *)drv->rx_buffer;
  const sli_cpc_hdr_t *hdr;
  sl_cpc_frame_t *frame;
  size_t payload_len;
  size_t offset;
  sl_status_t status;

  if (hdr_block->frame_count != drv->local_caps.max_aggregation) {
    SLI_CPC_LOG_ERROR("RX - unsupported frame_count %u", hdr_block->frame_count);
    SLI_CPC_ASSERT(false);
    return;
  }

  MCU_ATOMIC_SECTION(frame = sli_cpc_frame_list_pop(&drv->rx_free_frames);)
  if (frame == NULL) {
    // Should be unreachable when adma_rx_start() defers on an empty free list;
    SLI_CPC_LOG_WARN("RX - no free frame; dropping frame");
    return;
  }

  offset = sizeof(struct sdio_hdr_block);
  memcpy(sli_cpc_frame_get_header(frame), &drv->rx_buffer[offset], SLI_CPC_HEADER_SIZE);
  offset += SLI_CPC_HEADER_SIZE;
  hdr = sli_cpc_frame_get_header(frame);

  if (!sli_cpc_header_is_size_valid(hdr)) {
    SLI_CPC_LOG_ERROR("RX - invalid header size");
    MCU_ATOMIC_SECTION(sli_cpc_frame_list_push_back(&drv->rx_free_frames, frame);)
    SLI_CPC_ASSERT(false);
    return;
  }

  // SDIO block CRC covers the wire; mark payload csum valid for the datalink layer.
  frame->payload_csum_is_valid = true;

  payload_len = sli_cpc_header_get_payload_size(sli_cpc_frame_get_header(frame));
  if (payload_len > 0U) {
    size_t aligned_len = ALIGNED_PAYLOAD_SIZE(payload_len);

    // Bound against the advertised transfer length,
    // so a short transfer length cannot expose stale leftovers as payload.
    if ((offset + aligned_len) > drv->rx_xfer_len) {
      SLI_CPC_LOG_ERROR("RX - payload exceeds transferred length (%lu > %lu)", (unsigned long)(offset + aligned_len),
                        (unsigned long)drv->rx_xfer_len);
      MCU_ATOMIC_SECTION(sli_cpc_frame_list_push_back(&drv->rx_free_frames, frame);)
      return;
    }

    status = sli_cpc_alloc_rx_payload(&drv->bus, frame);
    if (status != SL_STATUS_OK) {
      SLI_CPC_LOG_ERROR("RX - payload alloc failed: 0x%lx", (unsigned long)status);
      MCU_ATOMIC_SECTION(sli_cpc_frame_list_push_back(&drv->rx_free_frames, frame);)
      return;
    }

    memcpy(frame->payload->ptr, &drv->rx_buffer[offset], payload_len);
  } else {
    // Header-only frame (ACK/RESET/etc.): still bind the endpoint.
    status = sli_cpc_alloc_rx_payload(&drv->bus, frame);
    if (status != SL_STATUS_OK) {
      SLI_CPC_LOG_ERROR("RX - endpoint bind failed: 0x%lx", (unsigned long)status);
      MCU_ATOMIC_SECTION(sli_cpc_frame_list_push_back(&drv->rx_free_frames, frame);)
      return;
    }
  }

  MCU_ATOMIC_SECTION(sli_cpc_frame_list_push_back(&drv->rx_completed_frames, frame);)
  sli_cpc_bus_notify_rx_data_from_drv(&drv->bus);
}

static void adma_rx_start(sl_cpc_drv_sdio_host_t *drv)
{
  sl_status_t status;
  uint32_t xfer_len = 0;
  bool no_free;
  MCU_DECLARE_IRQ_STATE;

  // Claim ADMA for RX under IRQ lock, matching start_pending_tx(), so TX cannot
  // race into the window between the idle check and ADMA_RECEIVE.
  MCU_ENTER_ATOMIC();
  if (drv->adma_state != ADMA_IDLE) {
    // Defer until ADMA is idle; card_int_pending re-enters on_card_interrupt.
    drv->card_int_pending = true;
    MCU_EXIT_ATOMIC();
    return;
  }
  drv->adma_state = ADMA_RECEIVE;
  MCU_EXIT_ATOMIC();

  // Defer CMD53 until a free RX frame exists so we never DMA a frame we
  // cannot keep (secondary pattern: backpressure before arming).
  MCU_ATOMIC_SECTION(no_free = sli_cpc_frame_list_empty(&drv->rx_free_frames);)
  if (no_free) {
    MCU_ENTER_ATOMIC();
    drv->adma_state = ADMA_IDLE;
    drv->card_int_pending = true;
    MCU_EXIT_ATOMIC();
    SLI_CPC_LOG_WARN("RX - no free frame; deferring CMD53");
    return;
  }

  sl_sdhc_sdio_xfer_params_t xfer = {
    .func_num = drv->function_num,
    .reg_addr = SDIO_XFER_COUNT_REG_ADDR, // 0x0C
    .xfer_mode = SL_SDHC_SDIO_BYTE_MODE_INCR_ADDR,
    .buffer = (uint8_t *)&xfer_len,
    .size = sizeof(xfer_len),
    .data_timeout_ms = 1000U,
  };

  // Read transfer length
  status = sl_sdhc_sdio_read_extended_blocking(drv->sdio_handle, &xfer);
  if (status != SL_STATUS_OK) {
    SLI_CPC_LOG_ERROR("Failed to read transfer length: 0x%lx", (unsigned long)status);
    MCU_ENTER_ATOMIC();
    drv->adma_state = ADMA_IDLE;
    drv->card_int_pending = true;
    MCU_EXIT_ATOMIC();
    return;
  }

  // If the transfer length is 0, the CMD53 transfer is not valid, don't start the transfer, try later.
  if (xfer_len == 0U) {
    SLI_CPC_LOG_ERROR("Transfer length is 0");
    // Transfer length is 0, don't start the transfer, try later.
    MCU_ENTER_ATOMIC();
    drv->adma_state = ADMA_IDLE;
    drv->card_int_pending = true;
    MCU_EXIT_ATOMIC();
    return;
  }

  // Transfer length must cover the SDIO header block + CPC header don't start the transfer, try later.
  if (xfer_len < (sizeof(struct sdio_hdr_block) + SLI_CPC_HEADER_SIZE)) {
    SLI_CPC_LOG_ERROR("RX - xfer_len %lu below minimum frame size %lu", (unsigned long)xfer_len,
                      (unsigned long)(sizeof(struct sdio_hdr_block) + SLI_CPC_HEADER_SIZE));
    MCU_ENTER_ATOMIC();
    drv->adma_state = ADMA_IDLE;
    drv->card_int_pending = true;
    MCU_EXIT_ATOMIC();
    return;
  }

  // Incoming transfer length exceeds the bounce buffer size don't start the transfer, try later.
  if (xfer_len > sizeof(drv->rx_buffer)) {
    SLI_CPC_LOG_ERROR("RX - xfer_len %lu exceeds bounce buffer %lu", (unsigned long)xfer_len,
                      (unsigned long)sizeof(drv->rx_buffer));
    MCU_ENTER_ATOMIC();
    drv->adma_state = ADMA_IDLE;
    drv->card_int_pending = true;
    MCU_EXIT_ATOMIC();
    return;
  }

  xfer_len = ALIGNED_PAYLOAD_SIZE(xfer_len);
  // Store the advertised transfer length for the current CMD53 transfer.
  drv->rx_xfer_len = xfer_len;

  drv->frame_descriptors[0] = SL_HAL_SDHC_DMA_DESCRIPTOR_XFER_DMA_END_INTERRUPT(drv->rx_buffer, xfer_len);
  transfer_start(drv, ADMA_RECEIVE, (size_t)xfer_len, false);
}

/***************************************************************************/ /**
 * Dispatched CARDINT handler for the RX transfer.
 ******************************************************************************/
static void on_card_interrupt(void *arg)
{
  sl_cpc_drv_sdio_host_t *drv = arg;
  bool claimed = false;
  uint8_t int_id = 0;
  sl_status_t status;
  bool pending;

  MCU_DECLARE_IRQ_STATE;

  // Atomically claim a pending RX while ADMA is idle so an ISR latch
  // between the check and clear cannot be dropped.
  MCU_ENTER_ATOMIC();
  if (drv->card_int_pending && drv->adma_state == ADMA_IDLE) {
    drv->card_int_pending = false;
    claimed = true;
  }
  MCU_EXIT_ATOMIC();

  if (!claimed) {
    // Still latched; ADMA-idle path will re-push when the bus is free.
    return;
  }

  // Ack FN1 INT_ID when present. After a deferred resume INT_ID may already
  // be clear; xfer_len is then the source of truth in adma_rx_start().
  status = sl_sdhc_sdio_read_byte(drv->sdio_handle, drv->function_num, SDIO_CARD_INT_ID_REG_ADDR, &int_id);
  if (status != SL_STATUS_OK) {
    SLI_CPC_LOG_ERROR("Card INT_ID read failed: 0x%lx", (unsigned long)status);
    MCU_ATOMIC_SECTION(drv->card_int_pending = true;)
  } else {
    if (int_id != 0U) {
      (void)sl_sdhc_sdio_write_byte(drv->sdio_handle, drv->function_num, SDIO_CARD_INT_ID_REG_ADDR, int_id);
    }
    adma_rx_start(drv);
  }

  // Always re-arm (clearing CARDINTSTS clears STSENA).
  (void)sl_sdhc_sdio_enable_card_interrupt(drv->sdio_handle);

  // adma_rx_start may re-latch on transient failure; retry from thread context.

  MCU_ATOMIC_LOAD(pending, drv->card_int_pending);
  if (pending) {
    sli_cpc_dispatcher_push(&drv->card_irq_dispatcher, on_card_interrupt, drv);
  }
}

/***************************************************************************/ /**
 * Allocate a single RX frame and push it onto drv->rx_free_frames.
 ******************************************************************************/
static sl_status_t alloc_rx_frame_to_free_list(sl_cpc_drv_sdio_host_t *drv)
{
  sl_cpc_frame_t *frame;

  frame = sli_cpc_frame_new(&drv->bus, true);
  if (frame == NULL) {
    return SL_STATUS_ALLOCATION_FAILED;
  }

  MCU_ATOMIC_SECTION(sli_cpc_frame_list_push_back(&drv->rx_free_frames, frame);)

  return SL_STATUS_OK;
}

/***************************************************************************/ /**
 * ADMA interrupt handler: handles the completion of a data transfer.
 ******************************************************************************/
static void adma_irq_handler(sl_cpc_drv_sdio_host_t *drv)
{
  adma_state_t state;
  bool pending;

  MCU_ATOMIC_LOAD(state, drv->adma_state);
  switch (state) {
    case ADMA_IDLE:
      break;

    case ADMA_RECEIVE:
      MCU_ATOMIC_STORE(drv->adma_state, ADMA_IDLE);
      adma_rx_parse_bounce(drv);
      // Re-arm the level-triggered source. A still-asserted DAT1 generates
      // another CARDINT event; do not synthesize a poll here.
      sl_sdhc_sdio_enable_card_interrupt(drv->sdio_handle);
      MCU_ATOMIC_LOAD(pending, drv->card_int_pending);
      if (pending) {
        sli_cpc_dispatcher_push(&drv->card_irq_dispatcher, on_card_interrupt, drv);
      }

      // Try to start a pending TX transfer if the ADMA is idle
      start_pending_tx(drv);
      break;

    case ADMA_TRANSMIT:
      // Publish the idle state before waking CPC. A newly submitted frame
      // must be able to start immediately rather than becoming stranded.
      MCU_ATOMIC_STORE(drv->adma_state, ADMA_IDLE);
      sli_cpc_bus_notify_tx_data_by_drv(&drv->bus, &drv->tx_pending_xfer_complete_frames);

      // Re-arm only. If DAT1 is asserted, hardware raises CARDINT and the
      // event callback latches it asynchronously.
      sl_sdhc_sdio_enable_card_interrupt(drv->sdio_handle);
      MCU_ATOMIC_LOAD(pending, drv->card_int_pending);
      if (pending) {
        sli_cpc_dispatcher_push(&drv->card_irq_dispatcher, on_card_interrupt, drv);
      }

      // Try to start a pending TX transfer if the ADMA is idle
      start_pending_tx(drv);
      break;

    default:
      SLI_CPC_ASSERT(false);
      break;
  }
}

/***************************************************************************/ /**
 * Transfer complete callback: handles the completion of a data transfer.
 ******************************************************************************/
static sl_status_t cpc_drv_sdio_xfer_complete_cb(sl_sdhc_sdio_handle_t *sdio_handle, sl_sdhc_xfer_direction_t direction,
                                                 uint32_t r5_response, void *user_data)
{
  sl_cpc_drv_sdio_host_t *drv = user_data;

  (void)sdio_handle;
  (void)r5_response;

  if (direction == SL_SDHC_XFER_DIRECTION_WRITE) {
    SLI_CPC_LOG_DEBUG("Write transfer complete: 0x%lx", (unsigned long)r5_response);
  } else {
    SLI_CPC_LOG_DEBUG("Read transfer complete: 0x%lx", (unsigned long)r5_response);
  }

  adma_irq_handler(drv);

  return SL_STATUS_OK;
}

/***************************************************************************/ /**
 * Event callback: latch CARDINT and defer CMD52/RX start to thread context.
 * Clearing CARDINTSTS disables NORMALINTSTSENA; re-arm after servicing.
 ******************************************************************************/
static void cpc_drv_sdio_event_cb(sl_sdhc_sdio_handle_t *sdio_handle, sl_sdhc_event_t event_bits, void *user_data)
{
  sl_cpc_drv_sdio_host_t *drv = user_data;

  (void)sdio_handle;
  (void)user_data;

  if ((event_bits & SL_SDHC_EVENT_CARD_INTERRUPT) != 0U) {
    MCU_ATOMIC_STORE(drv->card_int_pending, true);
    // SYSTEM wake only; notify_rx is reserved for parsed frames.
    sli_cpc_dispatcher_push(&drv->card_irq_dispatcher, on_card_interrupt, drv);
  }
}

/***************************************************************************/ /**
 * Error callback: driver reports errors here if needed.
 ******************************************************************************/
static void cpc_drv_sdio_error_cb(sl_sdhc_sdio_handle_t *sdio_handle, sl_sdhc_error_bit_t error_bits,
                                  uint32_t r5_response, void *user_data)
{
  sl_cpc_drv_sdio_host_t *drv = user_data;
  (void)error_bits;
  (void)r5_response;

  drv->sdio_handle = sdio_handle;

  SLI_CPC_LOG_ERROR("Error callback: 0x%lx", (unsigned long)error_bits);
}

static sl_status_t init_hw(sl_cpc_drv_sdio_host_t *drv, const sl_cpc_drv_sdio_host_config_t *cfg)
{
  sl_sdhc_sdio_init_params_t sdio_init_params;
  sl_status_t status;
  uint8_t func_mask;
  uint8_t card_cap;
  uint8_t int_id;

  sl_sdhc_sdio_callbacks_t callbacks = {
    .transfer_complete = cpc_drv_sdio_xfer_complete_cb,
    .event = cpc_drv_sdio_event_cb,
    .error = cpc_drv_sdio_error_cb,
    .user_data = drv,
  };

  SLI_CPC_LOG_INFO("HW Init");

  if (drv->sdio_handle == NULL) {
    return SL_STATUS_NOT_INITIALIZED;
  }

  primary_sdio_init_pins(drv->sdio_handle);

  // Enable the SD bus power (requires SDHC clock already running; EN is set in
  sl_hal_sdhc_enable_bus_power(sli_sdhc_get_base_addr(drv->sdio_handle->sdhc_controller.sdhc_peripheral));

  sdio_init_params.max_sd_freq = cfg->max_sd_freq;
  sdio_init_params.max_bus_width = cfg->max_bus_width;
  sdio_init_params.max_speed_mode = cfg->max_speed_mode;

  status = sl_sdhc_sdio_init(drv->sdio_handle, &sdio_init_params);
  if (status != SL_STATUS_OK) {
    SLI_CPC_LOG_ERROR("Card init failed: 0x%lx", (unsigned long)status);
    return status;
  }

  func_mask = SL_SDHC_CCCR_IO_ENABLE_IOEx(drv->function_num);

  // sl_sdhc_sdio_init already arms host CARDINT. Mask it while enabling CCCR
  // IENM/FN1 INT_EN — otherwise DAT1 can assert mid-CMD52 and time out (0x7).
  SDHCCORE_TypeDef *sdhc_base = sli_sdhc_get_base_addr(drv->sdio_handle->sdhc_controller.sdhc_peripheral);

  sl_hal_sdhc_disable_normal_interrupt_status(sdhc_base, SDHCCORE_NORMALINTSTS_CARDINTSTS);
  sl_hal_sdhc_disable_normal_interrupt_signal(sdhc_base, SDHCCORE_NORMALINTSTS_CARDINTSTS);

  status = sl_sdhc_sdio_register_callbacks(drv->sdio_handle, &callbacks);
  if (status != SL_STATUS_OK) {
    SLI_CPC_LOG_ERROR("Register callbacks failed: 0x%lx", (unsigned long)status);
    return status;
  }

  status = sl_sdhc_sdio_enable_functions(drv->sdio_handle, func_mask, true);
  if (status != SL_STATUS_OK) {
    SLI_CPC_LOG_ERROR("Function enable failed: 0x%lx", (unsigned long)status);
    return status;
  }

  status = sl_sdhc_sdio_enable_function_interrupts(drv->sdio_handle, func_mask, true);
  if (status != SL_STATUS_OK) {
    SLI_CPC_LOG_ERROR("Function interrupts enable failed: 0x%lx", (unsigned long)status);
    return status;
  }

  status = sl_sdhc_sdio_write_byte(drv->sdio_handle, drv->function_num, SDIO_CARD_INT_EN_REG_ADDR,
                                   SDIO_CARD_DATA_READY_FLAG);
  if (status != SL_STATUS_OK) {
    SLI_CPC_LOG_ERROR("FN1 INT_EN write failed: 0x%lx", (unsigned long)status);
    return status;
  }

  status = sl_sdhc_sdio_set_block_size(drv->sdio_handle, drv->function_num, drv->block_size);
  if (status != SL_STATUS_OK) {
    SLI_CPC_LOG_ERROR("Block size set failed: 0x%lx", (unsigned long)status);
    return status;
  }

  status = sl_sdhc_sdio_read_byte(drv->sdio_handle, drv->function_num, SL_SDHC_CCCR_CARD_CAP, &card_cap);
  if (status != SL_STATUS_OK) {
    SLI_CPC_LOG_ERROR("Card cap read failed: 0x%lx", (unsigned long)status);
    return status;
  }

  status = sl_sdhc_sdio_write_byte(drv->sdio_handle, drv->function_num, SL_SDHC_CCCR_CARD_CAP,
                                   (uint8_t)(card_cap | SDIO_CCCR_CARD_CAP_E4MI));
  if (status != SL_STATUS_OK) {
    SLI_CPC_LOG_ERROR("E4MI enable failed: 0x%lx", (unsigned long)status);
    return status;
  }

  status = sl_sdhc_sdio_write_byte(drv->sdio_handle, SL_CPC_DRV_SDIO_FUNCTION_0, SDIO_CLOCK_WAKE_REG_ADDR, 0x01U);
  if (status != SL_STATUS_OK) {
    SLI_CPC_LOG_ERROR("Clock wake enable failed: 0x%lx", (unsigned long)status);
    return status;
  }

  status = sl_sdhc_sdio_enable_card_interrupt(drv->sdio_handle);
  if (status != SL_STATUS_OK) {
    SLI_CPC_LOG_ERROR("Card interrupt enable failed: 0x%lx", (unsigned long)status);
    return status;
  }

  status = sl_sdhc_sdio_read_byte(drv->sdio_handle, drv->function_num, SDIO_CARD_INT_ID_REG_ADDR, &int_id);
  if (status != SL_STATUS_OK) {
    SLI_CPC_LOG_ERROR("Post-init FN1 INT_ID read failed: 0x%lx", (unsigned long)status);
  }

  SLI_CPC_LOG_INFO("Card ready (fn=%u, blk=%u)", (unsigned)drv->function_num, (unsigned)drv->block_size);
  return SL_STATUS_OK;
}

static inline sl_cpc_drv_sdio_host_t *to_drv(sl_cpc_bus_t *bus)
{
  return container_of(bus, sl_cpc_drv_sdio_host_t, bus);
}

static sl_status_t cpc_drv_sdio_init(sl_cpc_bus_t *bus)
{
  sl_cpc_drv_sdio_host_t *drv = to_drv(bus);
  SDHCCORE_TypeDef *sdhc_base = sli_sdhc_get_base_addr(drv->sdio_handle->sdhc_controller.sdhc_peripheral);

  MCU_ATOMIC_STORE(drv->adma_state, ADMA_IDLE);
  MCU_ATOMIC_STORE(drv->card_int_pending, false);
  drv->rx_xfer_len = 0U;

  // Initialize driver frame lists.
  sli_cpc_frame_list_init(&drv->tx_header_pending_frames);
  sli_cpc_frame_list_init(&drv->tx_payload_pending_frames);
  sli_cpc_frame_list_init(&drv->tx_pending_xfer_complete_frames);
  sli_cpc_frame_list_init(&drv->rx_completed_frames);
  sli_cpc_frame_list_init(&drv->rx_free_frames);
  // Service CARDINT before TX in the same process_action iteration.
  sli_cpc_dispatcher_init_handle(&drv->card_irq_dispatcher, &drv->bus);
  sli_cpc_dispatcher_set_pre(&drv->card_irq_dispatcher);

  for (uint32_t i = 0; i < CPC_SDIO_DEFAULT_AGGREGATION; i++) {
    sl_status_t status = alloc_rx_frame_to_free_list(drv);
    if (status != SL_STATUS_OK) {
      SLI_CPC_ASSERT(false);
      return status;
    }
  }

  // Re-arm host CARDINT now that the driver is initialized and ready to receive data.
  sl_hal_sdhc_enable_normal_interrupt_status(sdhc_base, SDHCCORE_NORMALINTSTS_CARDINTSTS);
  sl_hal_sdhc_enable_normal_interrupt_signal(sdhc_base, SDHCCORE_NORMALINTSTS_CARDINTSTS);

  // CPC-3336 Add support for SDIO frame aggregation in the host driver.
  drv->remote_caps.max_aggregation = 1;
  // Initialize local capabilities
  drv->local_caps.max_aggregation = 1;
  return SL_STATUS_OK;
}

static sl_status_t cpc_drv_sdio_start_rx(sl_cpc_bus_t *bus)
{
  (void)bus;
  return SL_STATUS_OK;
}

static sl_status_t cpc_drv_sdio_read(sl_cpc_bus_t *bus, sli_cpc_frame_list_t *frames)
{
  sl_cpc_drv_sdio_host_t *drv = to_drv(bus);
  MCU_DECLARE_IRQ_STATE;

  MCU_ENTER_ATOMIC();
  if (sli_cpc_frame_list_empty(&drv->rx_completed_frames)) {
    MCU_EXIT_ATOMIC();
    return SL_STATUS_EMPTY;
  }

  sli_cpc_frame_list_extend(frames, &drv->rx_completed_frames);
  MCU_EXIT_ATOMIC();

  return SL_STATUS_OK;
}

static uint32_t cpc_drv_sdio_write(sl_cpc_bus_t *bus, sli_cpc_frame_list_t *frames)
{
  sl_cpc_drv_sdio_host_t *drv = to_drv(bus);
  uint32_t num_frames = sli_cpc_frame_list_get_len(frames);

  if (num_frames == 0) {
    goto exit;
  }

  // tx_header_pending_frames also accessed from IRQ to queue another CMD53 Read operation.
  MCU_ATOMIC_SECTION(sli_cpc_frame_list_extend(&drv->tx_header_pending_frames, frames);)

  start_pending_tx(drv);

exit:
  return num_frames;
}

static uint32_t cpc_drv_sdio_get_available_write_frame_slots(sl_cpc_bus_t *bus)
{
  const sl_cpc_drv_sdio_host_t *drv = to_drv(bus);
  uint32_t list_len;
  uint32_t num_available_slots;

  MCU_ATOMIC_SECTION(list_len = sli_cpc_frame_list_get_len(&drv->tx_header_pending_frames);)
  num_available_slots = drv->remote_caps.max_aggregation - list_len;

  // Sanity check for underflow
  if (num_available_slots > drv->remote_caps.max_aggregation) {
    SLI_CPC_LOG_ERROR("Number of available write frame slots is greater than the maximum aggregation");
    SLI_CPC_ASSERT(false);
    return 0;
  }

  return num_available_slots;
}

static void cpc_drv_sdio_on_rx_frame_free(sl_cpc_bus_t *bus)
{
  sl_cpc_drv_sdio_host_t *drv = to_drv(bus);
  bool pending;

  alloc_rx_frame_to_free_list(drv);

  MCU_ATOMIC_LOAD(pending, drv->card_int_pending);
  if (pending) {
    sli_cpc_dispatcher_push(&drv->card_irq_dispatcher, on_card_interrupt, drv);
  }
}

static void cpc_drv_sdio_get_local_capabilities(sl_cpc_bus_t *bus, const void **caps_p, uint16_t *caps_size_p)
{
  const sl_cpc_drv_sdio_host_t *drv = to_drv(bus);

  *caps_p = &drv->local_caps;
  *caps_size_p = sizeof(drv->local_caps);
}

static sl_status_t cpc_drv_sdio_set_remote_capabilities(sl_cpc_bus_t *bus, const void *caps, uint16_t caps_size)
{
  sl_cpc_drv_sdio_host_t *drv = to_drv(bus);

  if (caps_size != sizeof(drv->remote_caps)) {
    SLI_CPC_LOG_ERROR("Set remote capabilities: invalid parameter");
    return SL_STATUS_INVALID_COUNT;
  }
  memcpy(&drv->remote_caps, caps, caps_size);
  return SL_STATUS_OK;
}

sl_status_t sl_cpc_drv_sdio_host_init(sl_cpc_drv_sdio_host_t *drv, const sl_cpc_drv_sdio_host_config_t *cfg,
                                      const sl_cpc_bus_config_t *bus_cfg)
{
  static const sli_cpc_drv_ops_t ops = {
    .init = &cpc_drv_sdio_init,
    .start_rx = &cpc_drv_sdio_start_rx,
    .read = &cpc_drv_sdio_read,
    .write = &cpc_drv_sdio_write,
    .get_available_write_frame_slots = &cpc_drv_sdio_get_available_write_frame_slots,
    .on_rx_frame_free = &cpc_drv_sdio_on_rx_frame_free,
    .get_local_capabilities = &cpc_drv_sdio_get_local_capabilities,
    .set_remote_capabilities = &cpc_drv_sdio_set_remote_capabilities,
  };

  sl_status_t status;

  if (cfg == NULL || bus_cfg == NULL || cfg->sdio_handle == NULL) {
    return SL_STATUS_NULL_POINTER;
  }

  if (cfg->block_size == 0U || cfg->block_size > SL_CPC_DRV_SDIO_HOST_MAX_BLOCK_SIZE) {
    return SL_STATUS_INVALID_PARAMETER;
  }

  memset(drv, 0, sizeof(*drv));
  drv->sdio_handle = cfg->sdio_handle;
  drv->function_num = cfg->function_num;
  drv->block_size = cfg->block_size;

  status = init_hw(drv, cfg);
  if (status != SL_STATUS_OK) {
    return status;
  }

  return sli_cpc_bus_init(&drv->bus, bus_cfg, &ops);
}

sl_cpc_drv_sdio_host_t sl_cpc_drv_sdio_host_instances[SL_CPC_DRV_SDIO_HOST_INSTANCES_COUNT];
