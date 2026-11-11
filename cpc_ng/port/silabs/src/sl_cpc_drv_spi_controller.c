/***************************************************************************/ /**
 * @file
 * @brief Common CPC SPI controller driver.
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

#include "sl_common.h"
#include "sl_component_catalog.h"
#include "sl_gpio.h"
#include "sl_hal_gpio.h"
#include "sl_slist.h"
#include "spidrv.h"

#include "cpc_atomic.h"
#include "sl_cpc_drv_spi_controller.h"

#include "../../../src/sli_cpc_bus.h"
#include "../../../src/sli_cpc_drv.h"
#include "../../../src/sli_cpc_frame_list.h"
#include "../../../src/sli_cpc_log.h"
#include "sli_cpc_drv_spi_controller.h"

#define CPC_SPI_MAX_WRITE_SLOTS 5 ///< Maximum number of frames that can be queued in the driver

/*******************************************************************************
 ***************************  LOCAL FUNCTIONS   ********************************
 ******************************************************************************/

static void spi_drv_tfer_payload(struct sl_cpc_drv_spi_controller *drv);
static void spi_drv_tfer_hdr(struct sl_cpc_drv_spi_controller *drv);

static inline struct sl_cpc_drv_spi_controller *bus_to_drv(const sl_cpc_bus_t *bus)
{
  return container_of(bus, struct sl_cpc_drv_spi_controller, bus);
}

static inline struct sl_cpc_drv_spi_controller *spidrv_to_drv(const struct SPIDRV_HandleData *spidrv)
{
  return container_of(spidrv, struct sl_cpc_drv_spi_controller, spidrv);
}

/**************************************************************************/ /**
 * Allocate RX frames until the free list reaches the maximum number of frames.
 ******************************************************************************/
static inline void alloc_rx_frames(struct sl_cpc_drv_spi_controller *drv)
{
  sl_cpc_frame_t *frame;
  while ((frame = sli_cpc_frame_new(&drv->bus, true))) {
    sli_cpc_frame_list_push_back(&drv->rx.free_frame_list, frame);
  }
}

/**************************************************************************/ /**
 * Return the next buffer in a payload chain, or NULL.
 ******************************************************************************/
static inline sl_cpc_buf_t *buf_chain_next(const sl_cpc_buf_t *buf)
{
  if (buf == NULL || buf->node.node == NULL) {
    return NULL;
  }

  return SL_SLIST_ENTRY(buf->node.node, sl_cpc_buf_t, node);
}

/**************************************************************************/ /**
 * Payload wire length for a frame: payload bytes + CRC, or 0 if no payload.
 ******************************************************************************/
static inline size_t frame_payload_wire_len(const sl_cpc_frame_t *frame)
{
  if (frame == NULL || frame->payload == NULL) {
    // When the frame has no payload, ensure that we sent at least 1 byte, otherwise the secondary
    // may not have the time to de-assert the IRQ pin after the payload transfer starts.
    return 1;
  }

  return (size_t)frame->payload->tot_len + sizeof(frame->payload_csum_le);
}

/**************************************************************************/ /**
 * Resolve the contiguous span still available from @p offset in the active
 * payload buffer, or in the trailing CRC once @p buf has been fully processed.
 *
 * @p ptr_out / @p remaining_out describe that span. When @p buf is NULL,
 * @p offset is an index into @p frame->payload_csum_le.
 ******************************************************************************/
static void payload_chunk_at(sl_cpc_frame_t *frame, sl_cpc_buf_t *buf, size_t offset, void **ptr_out,
                             size_t *remaining_out)
{
  *ptr_out = NULL;
  *remaining_out = 0;

  if (buf != NULL) {
    SLI_CPC_ASSERT(offset < buf->len);
    *ptr_out = (uint8_t *)buf->ptr + offset;
    *remaining_out = buf->len - offset;
    return;
  }

  if (frame == NULL || frame->payload == NULL || offset >= sizeof(frame->payload_csum_le)) {
    return;
  }

  SLI_CPC_ASSERT(offset < sizeof(frame->payload_csum_le));
  *ptr_out = &frame->payload_csum_le[offset];
  *remaining_out = sizeof(frame->payload_csum_le) - offset;
}

/**************************************************************************/ /**
 * Advance the active buffer/CRC slice by @p count bytes after they have been
 * queued. Moves to the next buffer when the current one is fully queued, and
 * resets @p offset to 0 when entering the next buffer or the CRC.
 ******************************************************************************/
static void payload_chunk_advance(sl_cpc_buf_t **buf, size_t *offset, size_t count)
{
  if (count == 0) {
    return;
  }

  *offset += count;

  if (*buf == NULL) {
    // Advancing through the CRC; nothing else to do.
    return;
  }

  if (*offset < (*buf)->len) {
    return;
  }

  // Current buffer fully queued — move to the next chunk (or CRC when NULL).
  SLI_CPC_ASSERT(*offset == (*buf)->len);
  *buf = buf_chain_next(*buf);
  *offset = 0;
}

/**************************************************************************/ /**
 * Push the active RX frame to the pending list and notify the core.
 ******************************************************************************/
static inline void flush_rx(struct sl_cpc_drv_spi_controller *drv)
{
  if (drv->rx.active.frame == NULL) {
    return;
  }

  sli_cpc_frame_list_push_back(&drv->rx.pending_frame_list, drv->rx.active.frame);
  memset(&drv->rx.active, 0, sizeof(drv->rx.active));
  sli_cpc_bus_notify_rx_data_from_drv(&drv->bus);
}

/**************************************************************************/ /**
 * Push the active TX frame to the completed list and notify the core.
 ******************************************************************************/
static inline void flush_tx(struct sl_cpc_drv_spi_controller *drv)
{
  if (drv->tx.active.frame == NULL) {
    return;
  }

  sli_cpc_frame_list_push_back(&drv->tx.completed_frame_list, drv->tx.active.frame);
  memset(&drv->tx.active, 0, sizeof(drv->tx.active));
  sli_cpc_bus_notify_tx_data_by_drv(&drv->bus, &drv->tx.completed_frame_list);
}

/**************************************************************************/ /**
 * Check if the header is all zeros.
 ******************************************************************************/
static inline bool is_hdr_all_zeros(sli_cpc_drv_spi_header_t *header)
{
  for (size_t i = 0; i < sizeof(*header); i++) {
    if (((uint8_t *)header)[i] != 0) {
      return false;
    }
  }

  return true;
}

/**************************************************************************/ /**
 * Check if the header is valid.
 ******************************************************************************/
static inline bool is_hdr_valid(const sli_cpc_drv_spi_header_t *header)
{
  if (header->sof != SLI_CPC_DRV_SPI_SOF) {
    return false;
  }

  uint16_t csum = sli_cpc_crc(header, sizeof(*header) - sizeof(header->csum_le));

  return csum == sli_cpc_u16_from_le(header->csum_le);
}

/**************************************************************************/ /**
 * Payload chunk transfer complete callback.
 ******************************************************************************/
static void spi_drv_tfer_payload_chunk_complete_cb(struct SPIDRV_HandleData *handle, Ecode_t transferStatus,
                                                   int itemsTransferred)
{
  struct sl_cpc_drv_spi_controller *drv = spidrv_to_drv(handle);

  if (SL_BRANCH_UNLIKELY(transferStatus != ECODE_OK)) {
    if (transferStatus != ECODE_EMDRV_SPIDRV_ABORTED) {
      SLI_CPC_LOG_ERROR("Unexpected payload chunk transfer failure: %lu. Stopping bus...", transferStatus);
    }
    return;
  }

  // Finished processing a payload chunk, proceed to the next one.
  drv->tfer_count += (size_t)itemsTransferred;
  spi_drv_tfer_payload(drv);
}

/**************************************************************************/ /**
 * SPI IRQ pin callback.
 ******************************************************************************/
static void spi_drv_irq_callback(uint8_t int_no, void *context)
{
  struct sl_cpc_drv_spi_controller *drv = context;
  SLI_CPC_ASSERT(int_no == drv->irq.int_no);
  MCU_DECLARE_IRQ_STATE;

  MCU_ENTER_ATOMIC();

  // Interrupt is re-enabled when the driver waits for the remote to signal,
  // i.e.: after processing the header, or after the payload transfer has completed.
  sl_status_t status = sl_gpio_disable_interrupts(1 << drv->irq.int_no);
  SLI_CPC_ASSERT(status == SL_STATUS_OK);

  switch (drv->state) {
    case SL_CPC_DRV_SPI_CONTROLLER_STATE_IDLE:
      // Secondary has something to send, start a frame transfer.
      spi_drv_tfer_hdr(drv);
      break;

    case SL_CPC_DRV_SPI_CONTROLLER_STATE_TFER_HDR:
      // Secondary has finished processing the header, proceed to payload.
      spi_drv_tfer_payload(drv);
      break;

    case SL_CPC_DRV_SPI_CONTROLLER_STATE_TFER_PAYLOAD:
    default:
      // IRQ interrupt should only be enabled when idle or pending header processing.
      SLI_CPC_ASSERT(false);
      break;
  }

  MCU_EXIT_ATOMIC();
}

/**************************************************************************/ /**
 * Enable the IRQ pin callback. Since our MCUs can only detect edge transitions,
 * check if the pin was already asserted by the remote before the callback was armed.
 * If it was, call the callback immediately.
 ******************************************************************************/
static void spi_drv_enable_irq(const struct sl_cpc_drv_spi_controller *drv)
{
  MCU_DECLARE_IRQ_STATE;
  sl_status_t status;
  bool irq;

  MCU_ENTER_ATOMIC();

  // Clear any latent IRQ transitions that may have occurred since the callback was last armed.
  sl_hal_gpio_clear_interrupts(1 << drv->irq.int_no);
  status = sl_gpio_enable_interrupts(1 << drv->irq.int_no);
  SLI_CPC_ASSERT(status == SL_STATUS_OK);

  status = sl_gpio_get_pin_input(&drv->irq.gpio, &irq);
  SLI_CPC_ASSERT(status == SL_STATUS_OK);

  // Check the interrupt after enabling it to avoid time-of-check time-of-use race condition, as
  // the remote could have asserted its pin right after we cleared the interrupt.
  if (!irq) {
    sl_hal_gpio_set_interrupts(1 << drv->irq.int_no);
  }

  MCU_EXIT_ATOMIC();
}

/**************************************************************************/ /**
 * Queue the next payload SPI transfer for the active TX and/or RX frames.
 *
 * Each frame's payload may be a scatter-gather chain of buffers, followed by a
 * 2-byte CRC that is not contiguous with the payload. TX and RX may also differ
 * in length. Each side keeps an active buffer slice (buf + offset); when the
 * current buffer is fully queued the slice advances to the next buffer, then
 * to the CRC once the chain is exhausted.
 *
 * This function finds the overlap between the two current chunks and queues a
 * duplex transfer of MIN(tx_remaining, rx_remaining). If one side is already
 * done, it uses a unidirectional transmit or receive instead. The completion
 * callback advances tfer_count and re-enters here until all wire bytes have 
 * been clocked.
 *
 * Example:
 *
 *   TX: [ Chunk 0 ] [...] [Chunk n] [CRC]
 *   RX: [Chunk 0] [CRC]
 ******************************************************************************/
static void spi_drv_tfer_payload(struct sl_cpc_drv_spi_controller *drv)
{
  sl_cpc_frame_t *tx_frame = drv->tx.active.frame;
  sl_cpc_frame_t *rx_frame = drv->rx.active.frame;
  size_t tx_remaining = 0;
  size_t rx_remaining = 0;
  void *tx_ptr = NULL;
  void *rx_ptr = NULL;
  sl_status_t status;
  size_t tx_wire_len;
  size_t rx_wire_len;
  size_t count;
  Ecode_t err;

  tx_wire_len = frame_payload_wire_len(tx_frame);
  rx_wire_len = frame_payload_wire_len(rx_frame);

  // Both directions are exhausted once we have clocked the longer payload and CRC.
  if (drv->tfer_count >= SL_MAX(tx_wire_len, rx_wire_len)) {
    drv->state = SL_CPC_DRV_SPI_CONTROLLER_STATE_IDLE;
    drv->tfer_count = 0;

    // Finished the transfer, de-assert CS.
    status = sl_gpio_set_pin(&drv->cs);
    SLI_CPC_ASSERT(status == SL_STATUS_OK);

    if (rx_frame) {
      flush_rx(drv);
    }

    if (tx_frame) {
      flush_tx(drv);
    }

    if (sli_cpc_frame_list_empty(&drv->tx.submitted_frame_list)) {
      // No more frames to send, wait for the remote to signal IRQ pin.
      spi_drv_enable_irq(drv);
    } else {
      // Frames pending, start a header transfer.
      spi_drv_tfer_hdr(drv);
    }

    return;
  }

  payload_chunk_at(tx_frame, drv->tx.active.buf, drv->tx.active.offset, &tx_ptr, &tx_remaining);
  payload_chunk_at(rx_frame, drv->rx.active.buf, drv->rx.active.offset, &rx_ptr, &rx_remaining);

  // Assert CS until the payload transfer is complete.
  status = sl_gpio_clear_pin(&drv->cs);
  SLI_CPC_ASSERT(status == SL_STATUS_OK);

  if (tx_ptr != NULL && rx_ptr != NULL) {
    count = SL_MIN(tx_remaining, rx_remaining);
    err = SPIDRV_MTransfer(&drv->spidrv, tx_ptr, rx_ptr, (int)count, spi_drv_tfer_payload_chunk_complete_cb);
  } else if (tx_ptr != NULL) {
    count = tx_remaining;
    err = SPIDRV_MTransmit(&drv->spidrv, tx_ptr, (int)count, spi_drv_tfer_payload_chunk_complete_cb);
  } else if (rx_ptr != NULL) {
    count = rx_remaining;
    err = SPIDRV_MReceive(&drv->spidrv, rx_ptr, (int)count, spi_drv_tfer_payload_chunk_complete_cb);
  } else {
    // No payload to send, send a dummy byte.
    static const uint8_t dummy_byte = 0;
    count = sizeof(dummy_byte);
    err = SPIDRV_MTransmit(&drv->spidrv, &dummy_byte, (int)count, spi_drv_tfer_payload_chunk_complete_cb);
  }

  SLI_CPC_ASSERT(err == ECODE_OK);

  // Advance slices for the bytes just queued; fully consumed buffers move on.
  payload_chunk_advance(&drv->tx.active.buf, &drv->tx.active.offset, (tx_ptr != NULL) ? count : 0);
  payload_chunk_advance(&drv->rx.active.buf, &drv->rx.active.offset, (rx_ptr != NULL) ? count : 0);
}

/**************************************************************************/ /**
 * Header transfer complete callback.
 ******************************************************************************/
static void spi_drv_tfer_hdr_complete_cb(struct SPIDRV_HandleData *handle, Ecode_t transferStatus, int itemsTransferred)
{
  struct sl_cpc_drv_spi_controller *drv = spidrv_to_drv(handle);
  sl_status_t status;

  (void)itemsTransferred;

  if (SL_BRANCH_UNLIKELY(transferStatus != ECODE_OK)) {
    if (transferStatus != ECODE_EMDRV_SPIDRV_ABORTED) {
      SLI_CPC_LOG_ERROR("Unexpected header transfer failure: %lu. Stopping bus...", transferStatus);
    }
    return;
  }

  // De-assert CS once the header transfer is complete.
  status = sl_gpio_set_pin(&drv->cs);
  SLI_CPC_ASSERT(status == SL_STATUS_OK);

  // Finished exchanging headers. Check if the remote has sent us something...
  if (!is_hdr_all_zeros(&drv->rx.active.hdr)) {
    if (SL_BRANCH_UNLIKELY(!is_hdr_valid(&drv->rx.active.hdr))) {
      // Received an invalid header checksum. Secondary is responsible for detecting early CS
      // de-assertion, so we can safely ignore the header and proceed to payload.
      // CPC-3611: This is not currently supported on the secondary side.
      goto start_payload_tfer;
    }

    // Remote has sent us a header! Let's get a frame to receive it.
    sl_cpc_frame_t *frame = sli_cpc_frame_list_pop(&drv->rx.free_frame_list);
    if (frame == NULL) {
      // We're out of RX frames. Pause the bus until on_rx_frame_free is called.
      drv->rx.need_frame = true;
      return;
    }

    // Copy the CPC header to the frame.
    memcpy(&frame->hdr, &drv->rx.active.hdr.hdr, sizeof(drv->rx.active.hdr.hdr));

    status = sli_cpc_alloc_rx_payload(&drv->bus, frame);
    if (SL_BRANCH_UNLIKELY(status != SL_STATUS_OK)) {
      // Remote attempted to send a frame to an endpoint with no more memory. This can happend during
      // retramsmissions; discard the payload and serve the frame as-is to the core.
      // CPC-3611: This will cause the wire length to be undersized when sending the payload. The secondary
      // needs to handle short payloads gracefully.
    }

    // Prepare the active RX frame context.
    drv->rx.active.frame = frame;
    drv->rx.active.buf = frame->payload;
  }

start_payload_tfer:
  // Wait for the secondary to signal its IRQ pin, indicating it has finished processing the header.
  spi_drv_enable_irq(drv);
}

/**************************************************************************/ /**
 * Start a header transfer.
 ******************************************************************************/
static void spi_drv_tfer_hdr(struct sl_cpc_drv_spi_controller *drv)
{
  SLI_CPC_ASSERT(drv->state == SL_CPC_DRV_SPI_CONTROLLER_STATE_IDLE);
  SLI_CPC_ASSERT(drv->tx.active.frame == NULL);
  SLI_CPC_ASSERT(drv->rx.active.frame == NULL);

  // Check if we have something to send.
  if (!sli_cpc_frame_list_empty(&drv->tx.submitted_frame_list)) {
    // Driver has a frame pending transmission, queue it.
    drv->tx.active.frame = sli_cpc_frame_list_pop(&drv->tx.submitted_frame_list);
    drv->tx.active.buf = drv->tx.active.frame->payload;
    drv->tx.active.offset = 0;

    // Prepare header and CRC.
    drv->tx.active.hdr.sof = SLI_CPC_DRV_SPI_SOF;

    memcpy(&drv->tx.active.hdr.hdr, &drv->tx.active.frame->hdr, sizeof(drv->tx.active.frame->hdr));

    // Compute header CRC, including SOF.
    uint16_t hdr_csum
      = sli_cpc_crc(&drv->tx.active.hdr, sizeof(drv->tx.active.hdr) - sizeof(drv->tx.active.hdr.csum_le));
    sli_cpc_u16_to_le(hdr_csum, drv->tx.active.hdr.csum_le);
  }

  drv->state = SL_CPC_DRV_SPI_CONTROLLER_STATE_TFER_HDR;

  // Assert CS until the header transfer is complete.
  sl_status_t status = sl_gpio_clear_pin(&drv->cs);
  SLI_CPC_ASSERT(status == SL_STATUS_OK);

  // Queue the header exchange.
  Ecode_t err = SPIDRV_MTransfer(&drv->spidrv, &drv->tx.active.hdr, &drv->rx.active.hdr, sizeof(drv->tx.active.hdr),
                                 spi_drv_tfer_hdr_complete_cb);
  SLI_CPC_ASSERT(err == ECODE_OK);
}

/**************************************************************************/ /**
 * Initialize the SPI controller driver.
 ******************************************************************************/
static sl_status_t spi_drv_init(sl_cpc_bus_t *bus)
{
  struct sl_cpc_drv_spi_controller *drv = bus_to_drv(bus);

  // Fetch all the available RX frames.
  alloc_rx_frames(drv);

  return SL_STATUS_OK;
}

/**************************************************************************/ /**
 * Start SPI Controller reception.
 ******************************************************************************/
static sl_status_t spi_drv_start_rx(sl_cpc_bus_t *bus)
{
  const struct sl_cpc_drv_spi_controller *drv = bus_to_drv(bus);

  // Enable the IRQ pin so the remote can signal a new frame.
  spi_drv_enable_irq(drv);

  return SL_STATUS_OK;
}

/**************************************************************************/ /**
 * Return a received frame to the core.
 ******************************************************************************/
static sl_status_t spi_drv_read(sl_cpc_bus_t *bus, sli_cpc_frame_list_t *frames)
{
  struct sl_cpc_drv_spi_controller *drv = bus_to_drv(bus);
  sl_status_t status = SL_STATUS_OK;
  sl_cpc_frame_t *frame;
  uint16_t csum;
  MCU_DECLARE_IRQ_STATE;

  MCU_ENTER_ATOMIC();
  if (sli_cpc_frame_list_empty(&drv->rx.pending_frame_list)) {
    status = SL_STATUS_EMPTY;
  }

  sli_cpc_frame_list_extend(frames, &drv->rx.pending_frame_list);
  MCU_EXIT_ATOMIC();

  SLI_CPC_FRAME_LIST_FOR_EACH(frames, frame)
  {
    if (!frame->payload) {
      frame->payload_csum_is_valid = true;
      continue;
    }

    csum = sli_cpc_crc_buf(frame->payload);
    frame->payload_csum_is_valid = csum == sli_cpc_u16_from_le(frame->payload_csum_le);
  }

  return status;
}

/**************************************************************************/ /**
 * Write data to the bus. When bus is busy, the frame is queued and will be 
 * sent when the current transfer is complete.
 ******************************************************************************/
static uint32_t spi_drv_write(sl_cpc_bus_t *bus, sli_cpc_frame_list_t *frames)
{
  struct sl_cpc_drv_spi_controller *drv = bus_to_drv(bus);
  uint32_t queued_frames = sli_cpc_frame_list_get_len(frames);
  MCU_DECLARE_IRQ_STATE;

  // Compute payload CRC now to avoid doing it in an interrupt context.
  sl_cpc_frame_t *frame;
  SLI_CPC_FRAME_LIST_FOR_EACH(frames, frame)
  {
    if (!frame->payload) {
      continue;
    }

    sli_cpc_u16_to_le(sli_cpc_crc_buf(frame->payload), frame->payload_csum_le);
  }

  MCU_ENTER_ATOMIC();
  // Queue the frames for transmission.
  sli_cpc_frame_list_extend(&drv->tx.submitted_frame_list, frames);

  if (drv->state == SL_CPC_DRV_SPI_CONTROLLER_STATE_IDLE) {
    // Driver is idle, kick it!
    spi_drv_tfer_hdr(drv);

    // Wait for the header to be transferred before enabling the IRQ pin, signaling the remote
    // has finished processing the header.
    sl_status_t status = sl_gpio_disable_interrupts(1 << drv->irq.int_no);
    SLI_CPC_ASSERT(status == SL_STATUS_OK);
  }
  MCU_EXIT_ATOMIC();

  return queued_frames - sli_cpc_frame_list_get_len(frames);
}

/**************************************************************************/ /**
 * Get the local capabilities of the driver.
 ******************************************************************************/
static void spi_drv_get_local_capabilities(sl_cpc_bus_t *bus, const void **caps_p, uint16_t *caps_size_p)
{
  const struct sl_cpc_drv_spi_controller *drv = bus_to_drv(bus);
  (void)drv;
  (void)caps_p;
  (void)caps_size_p;
}

/**************************************************************************/ /**
 * Get the number of available write frame slots.
 ******************************************************************************/
static uint32_t spi_drv_get_available_write_frame_slots(sl_cpc_bus_t *bus)
{
  const struct sl_cpc_drv_spi_controller *drv = bus_to_drv(bus);
  uint32_t num_available_slots;

  MCU_ATOMIC_SECTION(num_available_slots
                     = (CPC_SPI_MAX_WRITE_SLOTS - sli_cpc_frame_list_get_len(&drv->tx.submitted_frame_list));)

  return num_available_slots;
}

/**************************************************************************/ /**
 * Notify the driver that a RX frame has been freed.
 ******************************************************************************/
static void spi_drv_on_rx_frame_free(sl_cpc_bus_t *bus)
{
  struct sl_cpc_drv_spi_controller *drv = bus_to_drv(bus);
  MCU_DECLARE_IRQ_STATE;

  MCU_ENTER_ATOMIC();
  // Grab all available RX frames.
  alloc_rx_frames(drv);

  if (drv->rx.need_frame) {
    drv->rx.need_frame = false;

    // Just unlocked the bus, invoke the header transfer callback as if the transfer had just
    // completed, resuming the transfer.
    spi_drv_tfer_hdr_complete_cb(&drv->spidrv, ECODE_OK, sizeof(drv->rx.active.hdr));
  }
  MCU_EXIT_ATOMIC();
}

/**************************************************************************/ /**
 * Deinitialize the SPI controller driver.
 ******************************************************************************/
static void spi_drv_deinit(sl_cpc_bus_t *bus)
{
  struct sl_cpc_drv_spi_controller *drv = bus_to_drv(bus);
  sl_status_t status;

  // Make sure the remote can't start a transfer as we are tearing down.
  status = sl_gpio_disable_interrupts(1 << drv->irq.int_no);
  SLI_CPC_ASSERT(status == SL_STATUS_OK);

  Ecode_t err = SPIDRV_DeInit(&drv->spidrv);
  SLI_CPC_ASSERT(err == ECODE_OK);

  status = sl_gpio_deconfigure_external_interrupt(drv->irq.int_no);
  SLI_CPC_ASSERT(status == SL_STATUS_OK);
  status = sl_gpio_set_pin_mode(&drv->irq.gpio, SL_GPIO_MODE_DISABLED, 0);
  SLI_CPC_ASSERT(status == SL_STATUS_OK);
  status = sl_gpio_set_pin_mode(&drv->cs, SL_GPIO_MODE_DISABLED, 0);
  SLI_CPC_ASSERT(status == SL_STATUS_OK);
}

static const sli_cpc_drv_ops_t ops = {
  .init = &spi_drv_init,
  .start_rx = &spi_drv_start_rx,
  .read = &spi_drv_read,
  .write = &spi_drv_write,
  .get_local_capabilities = &spi_drv_get_local_capabilities,
  .get_available_write_frame_slots = &spi_drv_get_available_write_frame_slots,
  .on_rx_frame_free = &spi_drv_on_rx_frame_free,
  .deinit = &spi_drv_deinit,
};

/***************************************************************************/ /**
 * Attach SPI driver to CPC bus.
 ******************************************************************************/
sl_status_t sli_cpc_drv_spi_controller_init(struct sl_cpc_drv_spi_controller *drv,
                                            const struct sl_cpc_drv_spi_controller_config *spi_cfg,
                                            const sl_cpc_bus_config_t *bus_cfg)
{
  sl_status_t status;

  memset(drv, 0, sizeof(*drv));
  drv->peripheral = spi_cfg->peripheral;

  // CS needs to stay asserted between payload chunks, so handle it in this driver.
  drv->cs = spi_cfg->cs;

  // Use PUSH_PULL, as WIRED_AND_PULLUP results in a VERY slow slewrate.
  status = sl_gpio_set_pin_mode(&drv->cs, SL_GPIO_MODE_PUSH_PULL, 1);
  if (status != SL_STATUS_OK) {
    return status;
  }

  // IRQ has to be pulled up, noise may be confused with remote signaling.
  drv->irq.gpio = spi_cfg->irq;
  status = sl_gpio_set_pin_mode(&drv->irq.gpio, SL_GPIO_MODE_INPUT_PULL, 1);
  if (status != SL_STATUS_OK) {
    goto disable_cs;
  }

  // Initialize the IRQ pin's interrupt handler, signaled when the remote has something to send.
  status = sl_gpio_configure_external_interrupt(&drv->irq.gpio, &drv->irq.int_no, SL_GPIO_INTERRUPT_FALLING_EDGE,
                                                spi_drv_irq_callback, drv);
  if (status != SL_STATUS_OK) {
    goto disable_irq;
  }

  // Initialize the SPI peripheral and its pins.
  SPIDRV_Init_t init = {
    .port = (void *)spi_cfg->peripheral->base,
    .portClk = spi_cfg->clk.port,
    .pinClk = spi_cfg->clk.pin,
    .portCs = spi_cfg->cs.port,
    .pinCs = spi_cfg->cs.pin,
    .portRx = spi_cfg->cipo.port,
    .pinRx = spi_cfg->cipo.pin,
    .portTx = spi_cfg->copi.port,
    .pinTx = spi_cfg->copi.pin,
    .bitRate = spi_cfg->bitrate,
    .frameLength = 8,
    .dummyTxValue = 0,                       // Send 0 when RX only
    .csControl = spidrvCsControlApplication, // CS must be maintained between payload chunks
    .type = spidrvMaster,
    .bitOrder = spidrvBitOrderMsbFirst,
    .clockMode = spidrvClockMode3,
    .slaveStartMode = spidrvSlaveStartImmediate,
  };

  Ecode_t err = SPIDRV_Init(&drv->spidrv, &init);
  if (err != ECODE_EMDRV_SPIDRV_OK) {
    goto disable_irq_ext_int;
  }

  status = sli_cpc_bus_init(&drv->bus, bus_cfg, &ops);
  if (status != SL_STATUS_OK) {
    goto deinit_spidrv;
  }

  return SL_STATUS_OK;

deinit_spidrv:
  SPIDRV_DeInit(&drv->spidrv);
disable_irq_ext_int:
  sl_gpio_deconfigure_external_interrupt(drv->irq.int_no);
disable_irq:
  sl_gpio_set_pin_mode(&drv->irq.gpio, SL_GPIO_MODE_DISABLED, 0);
disable_cs:
  sl_gpio_set_pin_mode(&drv->cs, SL_GPIO_MODE_DISABLED, 0);

  return status;
}
