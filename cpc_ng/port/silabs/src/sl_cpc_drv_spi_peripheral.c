/***************************************************************************/ /**
 * @file sl_cpc_drv_spi_peripheral.c
 * @brief CPC SPI SECONDARY implementation.
 *******************************************************************************
 * # License
 * <b>Copyright 2019 Silicon Laboratories Inc. www.silabs.com</b>
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

#include "sl_cpc_drv_spi_peripheral.h"
#include "sl_cpc_drv_spi.h"
#include "sl_cpc_frame.h"
#include "sli_cpc_drv_spi_peripheral.h"

#include "sl_common.h"
#include "sl_component_catalog.h"
#include "sl_device_peripheral.h"
#include "sl_dma_manager.h"
#include "sl_hal_gpio.h"
#include "sl_hal_ldma.h"
#include "sl_hal_prs.h"
#include "sl_slist.h"
#include "sl_status.h"
#if defined(SL_CATALOG_CPC_DRIVER_HW_CRC_PRESENT)
#include "sl_hal_gpcrc.h"
#endif

#include "../../../src/sli_cpc_assert.h"
#include "../../../src/sli_cpc_atomic.h"
#include "../../../src/sli_cpc_bus.h"
#include "../../../src/sli_cpc_crc.h"
#include "../../../src/sli_cpc_drv.h"
#include "../../../src/sli_cpc_frame_list.h"
#include "../../../src/sli_cpc_hdr.h"
#include "../../../src/sli_cpc_log.h"
#include "../../../src/sli_cpc_memory.h"

#if defined(SL_CATALOG_POWER_MANAGER_PRESENT)
#include "sl_power_manager.h"
#endif

#if defined(SL_CATALOG_CPC_DRIVER_HW_CRC_PRESENT)
#include "sl_cpc_drv_secondary_spi_hw_crc_config.h"
#endif

/*******************************************************************************
 *********************************   DEFINES   *********************************
 ******************************************************************************/

#define CPC_SPI_CHKSM_SIZE 2
#define CPC_SPI_MAX_WRITE_SLOTS 5 ///< Maximum number of frames that can be queued in the driver

#if !defined(_SILICON_LABS_32B_SERIES_2) && !defined(_SILICON_LABS_32B_SERIES_3)
#error "This driver is only compatible with Series 2 & 3"
#endif

static_assert(SL_CPC_EP_MAX_PAYLOAD_SIZE
                <= SL_CPC_DRV_SPI_PERIPHERAL_LDMA_DESCRIPTORS * SL_HAL_LDMA_DESCRIPTOR_MAX_XFER_SIZE,
              "Invalid payload size");

#if defined(SL_CATALOG_CPC_DRIVER_HW_CRC_PRESENT)
#define GPCRC_SYNC_BIT_MASK (1 << SL_CPC_DRV_SPI_GPCRC_SYNC_BIT_CH)
#endif

#define LOG_HEADER(header)                                                                               \
  SLI_CPC_LOG_DEBUG("[SPI] Received header: [SoF: %02x | Size: %04x | Addr: %04x | ", header->sof,       \
                    sli_cpc_u16_from_le(header->hdr.size_le), sli_cpc_u16_from_le(header->hdr.addr_le)); \
  SLI_CPC_LOG_DEBUG("Ctrl: %02x | RX Wnd: %02x | Seq: %02x | ", header->hdr.ctrl, header->hdr.rx_wnd,    \
                    header->hdr.seq);                                                                    \
  SLI_CPC_LOG_DEBUG("Ack: %02x | CRC: %04x]", header->hdr.ack, sli_cpc_u16_from_le(header->csum_le));

/*******************************************************************************
 ***************************  LOCAL VARIABLES   ********************************
 ******************************************************************************/

// Shared sink for RX FIFO bytes discarded after RXBLOCKEN (not instance state).
static uint8_t rx_dummy_byte;

/*******************************************************************************
 **************************   LOCAL FUNCTIONS   ********************************
 ******************************************************************************/

static inline sl_cpc_drv_spi_peripheral_t *to_drv(sl_cpc_bus_t *bus)
{
  return container_of(bus, sl_cpc_drv_spi_peripheral_t, bus);
}

/***************************************************************************/ /**
 * Notifies the core when a frame has been sent on the wire.
 ******************************************************************************/
static inline void flush_rx(sl_cpc_drv_spi_peripheral_t *drv)
{
  if (drv->rx.current_frame == NULL) {
    // Noting to flush to the core
    return;
  }

  sli_cpc_frame_list_push_back(&drv->rx.pending_frame_list, drv->rx.current_frame);

  drv->rx.current_frame = NULL;

  sli_cpc_bus_notify_rx_data_from_drv(&drv->bus);
}

/***************************************************************************/ /**
 * Notifies the core when a frame has been sent on the wire.
 ******************************************************************************/
static inline void flush_tx(sl_cpc_drv_spi_peripheral_t *drv)
{
  if (drv->tx.current_frame == NULL) {
    // Nothing to notify the core about if there is no current TX entry
    return;
  }

  // Reset that variable which is set to 1 by the TX DMA chain
  drv->tx.frame_complete = 0;

  sli_cpc_frame_list_push_back(&drv->tx.completed_frame_list, drv->tx.current_frame);

  // Notify the core that this entry has been sent on the wire. It will detach its buffer
  sli_cpc_bus_notify_tx_data_by_drv(&drv->bus, &drv->tx.completed_frame_list);

  // Important to set this back to NULL
  drv->tx.current_frame = NULL;
}

static inline void spi_ldma_start_transfer(const sl_cpc_drv_spi_peripheral_t *drv, uint8_t channel,
                                           const sl_hal_ldma_transfer_config_t *transfer,
                                           const sl_hal_ldma_descriptor_t *descriptor)
{
  LDMA_TypeDef *ldma = sl_device_peripheral_ldma_get_base_addr((sl_peripheral_t)drv->dma_handle->dma_peripheral);
  sl_hal_ldma_init_transfer(ldma, channel, transfer, descriptor);
  sl_hal_ldma_start_transfer(ldma, channel);
}

SLI_CPC_STATIC_ASSERT_PACKED_SIZE(struct sli_cpc_drv_spi_peripheral_caps, 4);

static void get_local_capabilities(sl_cpc_bus_t *bus, const void **caps_p, uint16_t *caps_size_p)
{
  const sl_cpc_drv_spi_peripheral_t *drv = to_drv(bus);

  *caps_p = &drv->local_caps;
  *caps_size_p = sizeof(drv->local_caps);
}

/***************************************************************************/ /**
 * Start reception
 ******************************************************************************/
static sl_status_t spi_drv_start_rx(sl_cpc_bus_t *bus)
{
  const sl_cpc_drv_spi_peripheral_t *drv = to_drv(bus);
  const uint32_t tx_ready_window_trig_bit_mask = 1UL << drv->tx_availability_synctrig_ch;
  LDMA_TypeDef *ldma = sl_device_peripheral_ldma_get_base_addr((sl_peripheral_t)drv->dma_handle->dma_peripheral);

  // Due to the nature of the DMA chain, the start descriptor for the initial
  // DMA priming is in the middle of the chain, as opposed to when the DMA
  // is armed in the header interrupt
  const sl_hal_ldma_descriptor_t *start_descriptor = &drv->rx.desc.wait_cs_low_before_header;

  // The initial reception priming is special because we prime the RX DMA channel from
  // a descriptor in the middle of the chain.

  ldma->SYNCSWSET = tx_ready_window_trig_bit_mask;

#if defined(SL_CATALOG_CPC_DRIVER_HW_CRC_PRESENT)
  ldma->SYNCSWSET = GPCRC_SYNC_BIT_MASK;
#endif

  spi_ldma_start_transfer(drv, drv->rx.dma_channel, &drv->rx.dma_config, start_descriptor);

  return SL_STATUS_OK;
}

/***************************************************************************/ /**
 * Read bytes from SPI.
 ******************************************************************************/
static sl_status_t spi_drv_read_data(sl_cpc_bus_t *bus, sli_cpc_frame_list_t *frames)
{
  sl_cpc_drv_spi_peripheral_t *drv = to_drv(bus);
  sli_cpc_frame_list_t rx_pending_frames;
  sli_cpc_frame_list_t free_frames;
  uint16_t computed_crc;
  uint16_t received_crc;
  sl_cpc_frame_t *frame;

  MCU_DECLARE_IRQ_STATE;

  sli_cpc_frame_list_init(&rx_pending_frames);
  sli_cpc_frame_list_init(&free_frames);

  MCU_ENTER_ATOMIC();
  sli_cpc_frame_list_extend(&rx_pending_frames, &drv->rx.pending_frame_list);
  MCU_EXIT_ATOMIC();

  if (sli_cpc_frame_list_empty(&rx_pending_frames)) {
    return SL_STATUS_EMPTY;
  }

  SLI_CPC_FRAME_LIST_FOR_EACH(&rx_pending_frames, frame)
  {
    computed_crc = sli_cpc_crc_buf(frame->payload);
    received_crc = sli_cpc_u16_from_le(frame->payload_csum_le);
    frame->payload_csum_is_valid = computed_crc == received_crc;
  }

  for (uint32_t i = 0; i < sli_cpc_frame_list_get_len(&rx_pending_frames); i++) {
    frame = sli_cpc_frame_new(&drv->bus, true);
    if (frame == NULL) {
      break;
    }

    sli_cpc_frame_list_push_back(&free_frames, frame);
  }

  MCU_ATOMIC_SECTION(sli_cpc_frame_list_extend(&drv->rx.free_frame_list, &free_frames);)

  sli_cpc_frame_list_extend(frames, &rx_pending_frames);

  return SL_STATUS_OK;
}

/***************************************************************************/ /**
 * Reconfigure only what needs to be reconfigured in the RX DMA descriptor chain
 * and start the RX DMA channel.
 * Returns if the DMA was successfully started for reception.
 ******************************************************************************/
static bool prime_dma_for_reception(sl_cpc_drv_spi_peripheral_t *drv, size_t payload_size, bool received_valid_header)
{
  sl_status_t status;
#if defined(SL_CATALOG_CPC_DRIVER_HW_CRC_PRESENT) // With HW CRC
  const uint16_t effective_payload_size = payload_size - SLI_CPC_HDLC_FCS_SIZE;
  const uint16_t remaining_bytes = effective_payload_size % 4;
  bool skip_gpcrc = false;
#endif

  if (received_valid_header == false) {
    // We received a header full of 0s or a corrupted header, skip the payload(s) descriptors
    drv->rx.desc.set_irq_high.wri.link_addr = SL_HAL_LDMA_DESCRIPTOR_LINKABS_ADDR_TO_LINKADDR(&drv->rx.desc.rxblocken);

#if defined(SL_CATALOG_CPC_DRIVER_HW_CRC_PRESENT)
    skip_gpcrc = true;
#endif

    goto start_transfer;
  }

  drv->rx.current_frame = sli_cpc_frame_list_pop(&drv->rx.free_frame_list);

  if (SL_BRANCH_UNLIKELY(drv->rx.current_frame == NULL)) {
    // Ran out of RX frames. Hold the IRQ line in order to block communication
    // on the primary. Communication will be resumed on next call to
    // spi_drv_on_rx_frame_free.
    SLI_CPC_LOG_DEBUG("[SPI] Out of RX frames, hold IRQ");
    drv->rx.need_frame = true;
    return false;
  }

  payload_size = 0;

  // Copy the received header from DMA buffer into the frame's header
  sli_cpc_hdr_t *header = &drv->rx.current_frame->hdr;
  memcpy(header, &drv->rx.current_header.hdr, sizeof(*header));

  if (!sli_cpc_header_is_size_valid(header)) {
    // Invalid header size - discard the frame
    SLI_CPC_LOG_ERROR("[SPI] Invalid header size, discarding frame");
    goto check_payload_length;
  }

  payload_size = sli_cpc_header_get_payload_size(header);

  status = sli_cpc_alloc_rx_payload(&drv->bus, drv->rx.current_frame);
  if (status != SL_STATUS_OK) {
    SLI_CPC_LOG_ERROR("[SPI] Failed to allocate RX payload, discarding. Length: %d", payload_size);
    payload_size = 0;
  }

check_payload_length:

  if (payload_size == 0) {
    // Either received a frame with no payload, or a frame with an oversized payload.
    // Either way, skip over payload reception.

    drv->rx.desc.set_irq_high.wri.link_addr = SL_HAL_LDMA_DESCRIPTOR_LINKABS_ADDR_TO_LINKADDR(&drv->rx.desc.rxblocken);

#if defined(SL_CATALOG_CPC_DRIVER_HW_CRC_PRESENT)
    skip_gpcrc = true;
#endif

    goto start_transfer;
  }

  // Now that we expect to receive a payload, configure the reception descriptor chain to include the payload reception descriptor
  drv->rx.desc.set_irq_high.wri.link_addr
    = SL_HAL_LDMA_DESCRIPTOR_LINKABS_ADDR_TO_LINKADDR(&drv->rx.desc.recv_payload[0]);

  uint8_t *payload
    = (drv->rx.current_frame && drv->rx.current_frame->payload) ? drv->rx.current_frame->payload->ptr : NULL;

  // Set the RX buffer address in the payload descriptor using CPC frame payload
  drv->rx.desc.recv_payload[0].xfer.dst_addr = (uint32_t)payload;

  // Set the CRC address in payload's crc descriptor
  drv->rx.desc.recv_payload_crc.xfer.dst_addr = (uint32_t)drv->rx.current_frame->payload_csum_le;

  {
#if defined(SL_CATALOG_CPC_DRIVER_HW_CRC_PRESENT) // With HW CRC
    {
      if (SL_BRANCH_LIKELY(payload_size <= SL_HAL_LDMA_DESCRIPTOR_MAX_XFER_SIZE)) { // Using only one payload descriptor
        drv->rx.desc.recv_payload[0].xfer.xfer_count = payload_size - 1;

        // Skip over the large buffer descriptor
        drv->rx.desc.recv_payload[0].xfer.link_addr
          = SL_HAL_LDMA_DESCRIPTOR_LINKABS_ADDR_TO_LINKADDR(&drv->rx.desc.rxblocken);

        if (SL_BRANCH_UNLIKELY(effective_payload_size
                               <= 3)) { // In the real world, it will be unlikely that payloads will be that small
          // Since the payload is not even one word long, skip the GPCRC full-word transfer descriptor and branch to the one that transfers single bytes
          drv->rx.gpcrc_desc.clear_gpcrc_sync_bit.sync.link_addr = SL_HAL_LDMA_DESCRIPTOR_LINKABS_ADDR_TO_LINKADDR(
            &drv->rx.gpcrc_desc.write_rx_remaining_payload_bytes_in_gpcrc);

          // The "drv->rx.desc.recv_payload" took the payload from the E/USART and wrote it to memory
          // Now take what has been written to memory and write it to the GPCRC;
          drv->rx.gpcrc_desc.write_rx_remaining_payload_bytes_in_gpcrc.xfer.src_addr = (uint32_t)payload;

          drv->rx.gpcrc_desc.write_rx_remaining_payload_bytes_in_gpcrc.xfer.xfer_count = effective_payload_size - 1;

          goto start_transfer;
        } else { // payload length of one full word or more
          // Branch the payload descriptor to the descriptor that transfers full words to the GPCRC
          drv->rx.gpcrc_desc.clear_gpcrc_sync_bit.sync.link_addr
            = SL_HAL_LDMA_DESCRIPTOR_LINKABS_ADDR_TO_LINKADDR(&drv->rx.gpcrc_desc.write_rx_payload_words_in_gpcrc);

          // The "drv->rx.desc.recv_payload" took the payload from the E/USART and wrote it to memory
          // Now take what has been written to memory and write it to the GPCRC;
          drv->rx.gpcrc_desc.write_rx_payload_words_in_gpcrc.xfer.src_addr = (uint32_t)payload;

          // Unlike the "drv->rx.desc.recv_payload" descriptor who's length was in bytes, this descriptor moves full words around
          // in order to be as efficient and fast as possible.
          drv->rx.gpcrc_desc.write_rx_payload_words_in_gpcrc.xfer.xfer_count = (effective_payload_size / 4) - 1;
        }
      } else { // Using second payload descriptor
        // We have large buffer compiled, and the payload spans the two payload descriptors
        // load the first descriptor to the max, and link to the one right under it
        drv->rx.desc.recv_payload[0].xfer.xfer_count = SL_HAL_LDMA_DESCRIPTOR_MAX_XFER_SIZE - 1;
        drv->rx.desc.recv_payload[0].xfer.link_addr
          = SL_HAL_LDMA_DESCRIPTOR_LINKABS_ADDR_TO_LINKADDR(&drv->rx.desc.recv_payload[1]);

        // Fill the second payload descriptor with the remaining data
        drv->rx.desc.recv_payload[1].xfer.dst_addr = (uint32_t)&payload[SL_HAL_LDMA_DESCRIPTOR_MAX_XFER_SIZE];
        drv->rx.desc.recv_payload[1].xfer.xfer_count = (payload_size - SL_HAL_LDMA_DESCRIPTOR_MAX_XFER_SIZE) - 1;

        // Branch the payload descriptor to the descriptor that transfers full words to the GPCRC
        drv->rx.gpcrc_desc.clear_gpcrc_sync_bit.sync.link_addr
          = SL_HAL_LDMA_DESCRIPTOR_LINKABS_ADDR_TO_LINKADDR(&drv->rx.gpcrc_desc.write_rx_payload_words_in_gpcrc);

        // The "drv->rx.desc.recv_payload" and "rx_desc_recv_payload_large_buf" took the payload from the E/USART and
        // wrote it to memory. Now take what has been written to memory and write it to the GPCRC;
        drv->rx.gpcrc_desc.write_rx_payload_words_in_gpcrc.xfer.src_addr = (uint32_t)payload;

        // Unlike the "drv->rx.desc.recv_payload" descriptor who's length was in bytes, this descriptor moves full words around
        // in order to be as efficient and fast as possible.
        drv->rx.gpcrc_desc.write_rx_payload_words_in_gpcrc.xfer.xfer_count = (effective_payload_size / 4) - 1;
      }

      // Take care of the remaining %4 bytes
      if (SL_BRANCH_UNLIKELY(remaining_bytes
                             == 0)) { // With random payload size distribution, having a payload size % 4 == 0 is 1/4
        // The effective payload length was an integer multiple of word-size, skip the descriptor that transfer the remainder
        drv->rx.gpcrc_desc.write_rx_payload_words_in_gpcrc.xfer.link_addr
          = SL_HAL_LDMA_DESCRIPTOR_LINKABS_ADDR_TO_LINKADDR(&drv->rx.gpcrc_desc.save_rx_gpcrc_computed_crc);
      } else {
        // If 3,2 or 1 bytes of payload remain after full word transfers, link to the byte-transfer descriptor
        drv->rx.gpcrc_desc.write_rx_payload_words_in_gpcrc.xfer.link_addr
          = SL_HAL_LDMA_DESCRIPTOR_LINKABS_ADDR_TO_LINKADDR(
            &drv->rx.gpcrc_desc.write_rx_remaining_payload_bytes_in_gpcrc);

        drv->rx.gpcrc_desc.write_rx_remaining_payload_bytes_in_gpcrc.xfer.src_addr
          = (uint32_t)((uintptr_t)payload + effective_payload_size - remaining_bytes);

        drv->rx.gpcrc_desc.write_rx_remaining_payload_bytes_in_gpcrc.xfer.xfer_count = remaining_bytes - 1;
      }
    }
#else // Without HW CRC
    {
      if (SL_BRANCH_LIKELY(payload_size <= SL_HAL_LDMA_DESCRIPTOR_MAX_XFER_SIZE)) {
        // We have large buffer compiled, but this payload doesn't span 2 descriptors.
        // load the first descriptor, and jump over the large-buffer reception descriptor.
        drv->rx.desc.recv_payload[0].xfer.xfer_count = payload_size - 1;
        drv->rx.desc.recv_payload[0].xfer.link_addr
          = SL_HAL_LDMA_DESCRIPTOR_LINKABS_ADDR_TO_LINKADDR(&drv->rx.desc.recv_payload_crc);
      } else {
        // We have large buffer compiled, and the payload spans the two payload descriptors
        // load the first descriptor to the max, and link to the one right under it
        drv->rx.desc.recv_payload[0].xfer.xfer_count = SL_HAL_LDMA_DESCRIPTOR_MAX_XFER_SIZE - 1;
        drv->rx.desc.recv_payload[0].xfer.link_addr
          = SL_HAL_LDMA_DESCRIPTOR_LINKABS_ADDR_TO_LINKADDR(&drv->rx.desc.recv_payload[1]);

        // We have large buffer compiled, and the payload spans the two payload descriptors
        // load the first descriptor to the max, and link to the one right under it
        drv->rx.desc.recv_payload[1].xfer.dst_addr
          = payload ? (uint32_t)&payload[SL_HAL_LDMA_DESCRIPTOR_MAX_XFER_SIZE] : 0;
        drv->rx.desc.recv_payload[1].xfer.xfer_count = (payload_size - SL_HAL_LDMA_DESCRIPTOR_MAX_XFER_SIZE) - 1;
      }
    }
#endif
  }

start_transfer:
#if defined(SL_CATALOG_CPC_DRIVER_HW_CRC_PRESENT)
{
  // Store the GPCRC's computed CRC in the buffer when it is done computing
  drv->rx.gpcrc_desc.save_rx_gpcrc_computed_crc.xfer.dst_addr = (uint32_t)&drv->rx.current_header.csum_le;
}
#endif

  spi_ldma_start_transfer(drv, drv->rx.dma_channel, &drv->rx.dma_config, &drv->rx.desc.wait_cs_high_after_header);

#if defined(SL_CATALOG_CPC_DRIVER_HW_CRC_PRESENT)
  {
    if (skip_gpcrc == false) {
      LDMA_TypeDef *ldma = sl_device_peripheral_ldma_get_base_addr((sl_peripheral_t)drv->dma_handle->dma_peripheral);

      // By clearing the IEN bit, the RX DMA chain will still set its interrupt flag then the payload is done
      // receiving, but it ensures the interrupt is not triggered until the GPCRC chain sets back the RX chain IEN bit.
      // This is to ensure the RX DMA chain interrupt bit set is not lost to the header interrupt.
      ldma->IEN_CLR = (1 << drv->rx.dma_channel);

      // Ensures that the launch SYNC bit that was set at the end of the last transaction is clear so that the GPCRC chain
      // starts by waiting against the RX chain
      ldma->SYNCSWCLR = GPCRC_SYNC_BIT_MASK;

      spi_ldma_start_transfer(drv, drv->rx.gpcrc_dma_channel, &drv->rx.gpcrc_dma_config,
                              &drv->rx.gpcrc_desc.wait_for_gpcrc_sync_bit);
    }
  }
#endif

  return true;
}

/***************************************************************************/ /**
  * Reconfigure only what needs to be reconfigured in the TX DMA descriptor chain
  * and start the TX DMA channel.
  *
  * We assume the TX DMA channel is not running and we are in interrupt context
  ******************************************************************************/
static void prime_dma_for_transmission(sl_cpc_drv_spi_peripheral_t *drv)
{
  const sli_cpc_hdr_t *header;
  bool tx_pending;

  if (drv->tx.current_frame) {
    // There is already a frame programmed to be sent. Do nothing now, when the
    // frame currently programmed in the TX DMA chain gets tx_flushed(), the
    // frame for which this function was called will be cocked in.
    return;
  }

  MCU_ATOMIC_SECTION(tx_pending = !sli_cpc_frame_list_empty(&drv->tx.submitted_frame_list);)

  if (!tx_pending) {
    // No TX frame pending, nothing to do.
    return;
  }

  // Pick the next frame to send
  drv->tx.current_frame = sli_cpc_frame_list_pop(&drv->tx.submitted_frame_list);
  sl_cpc_frame_t *frame = drv->tx.current_frame;

  header = &frame->hdr;
  memcpy(&drv->tx.current_header.hdr, header, sizeof(drv->tx.current_header.hdr));

  sli_cpc_u16_to_le(sli_cpc_crc((uint8_t *)&drv->tx.current_header.hdr, sizeof(drv->tx.current_header.hdr)),
                    drv->tx.current_header.csum_le);

  if (drv->tx.current_frame == NULL || drv->tx.current_frame->payload == NULL) {
    // Turn off branching to payload descriptor
    drv->tx.desc.set_tx_frame_complete_variable_after_header.wri.link = 0;

    goto start_transfer;
  }

  // Turn on branching to payload descriptor
  drv->tx.desc.set_tx_frame_complete_variable_after_header.wri.link = 1;

  uint8_t *payload = drv->tx.current_frame->payload->ptr;
  size_t payload_size = drv->tx.current_frame->payload->len;

  // Set the payload source address
  drv->tx.desc.xfer_payload[0].xfer.src_addr = (uint32_t)payload;

  {
    if (SL_BRANCH_LIKELY(payload_size <= SL_HAL_LDMA_DESCRIPTOR_MAX_XFER_SIZE)) {
      // We have large buffer compiled, but this payload doesn't span 2 descriptors.
      // load the first descriptor, and jump over the one that follows.
      drv->tx.desc.xfer_payload[0].xfer.xfer_count = payload_size - 1;

#if defined(SL_CATALOG_CPC_SECURITY_PRESENT)
      {
        if (frame->security_tag) {
          drv->tx.desc.xfer_payload[0].xfer.link_addr
            = SL_HAL_LDMA_DESCRIPTOR_LINKABS_ADDR_TO_LINKADDR(&drv->tx.desc.xfer_tag);
          drv->tx.desc.xfer_tag.xfer.src_addr = (uint32_t)frame->security_tag;
        } else {
          drv->tx.desc.xfer_payload[0].xfer.link_addr
            = SL_HAL_LDMA_DESCRIPTOR_LINKABS_ADDR_TO_LINKADDR(&drv->tx.desc.xfer_checksum);
        }
      }
#else
      {
        drv->tx.desc.xfer_payload[0].xfer.link_addr
          = SL_HAL_LDMA_DESCRIPTOR_LINKABS_ADDR_TO_LINKADDR(&drv->tx.desc.xfer_checksum);
      }
#endif
    } else {
      // We have large buffer compiled, and the payload spans the two payload descriptors
      // load the first payload descriptor to the max, and link it to the large buffer payload descriptor
      drv->tx.desc.xfer_payload[0].xfer.xfer_count = SL_HAL_LDMA_DESCRIPTOR_MAX_XFER_SIZE - 1;
      drv->tx.desc.xfer_payload[0].xfer.link_addr
        = SL_HAL_LDMA_DESCRIPTOR_LINKABS_ADDR_TO_LINKADDR(&drv->tx.desc.xfer_payload[1]);

      // Load the large buffer payload descriptor with the remaining data
      drv->tx.desc.xfer_payload[1].xfer.src_addr = (uint32_t)&payload[SL_HAL_LDMA_DESCRIPTOR_MAX_XFER_SIZE];
      drv->tx.desc.xfer_payload[1].xfer.xfer_count = (payload_size - SL_HAL_LDMA_DESCRIPTOR_MAX_XFER_SIZE) - 1;

#if defined(SL_CATALOG_CPC_SECURITY_PRESENT)
      {
        if (frame->security_tag) {
          drv->tx.desc.xfer_payload[1].xfer.link_addr
            = SL_HAL_LDMA_DESCRIPTOR_LINKABS_ADDR_TO_LINKADDR(&drv->tx.desc.xfer_tag);
          drv->tx.desc.xfer_tag.xfer.src_addr = (uint32_t)frame->security_tag;
        } else {
          drv->tx.desc.xfer_payload[1].xfer.link_addr
            = SL_HAL_LDMA_DESCRIPTOR_LINKABS_ADDR_TO_LINKADDR(&drv->tx.desc.xfer_checksum);
        }
      }
#endif
    }
  }

  // Finally load the checksum descriptor
  drv->tx.desc.xfer_checksum.xfer.src_addr = (uint32_t)frame->payload_csum_le;

start_transfer:

  spi_ldma_start_transfer(drv, drv->tx.dma_channel, &drv->tx.dma_config, &drv->tx.desc.wait_availability_sync_bit);
}

/**************************************************************************/ /**
 * Called by the core to send frames over the bus.
 ******************************************************************************/
static uint32_t spi_drv_write(sl_cpc_bus_t *bus, sli_cpc_frame_list_t *frames)
{
  sl_cpc_drv_spi_peripheral_t *drv = to_drv(bus);
  uint32_t num_frames = sli_cpc_frame_list_get_len(frames);
  sl_cpc_frame_t *frame;

  SLI_CPC_FRAME_LIST_FOR_EACH(frames, frame)
  {
    if (!frame->payload_csum_is_valid) {
      sli_cpc_u16_to_le(sli_cpc_crc_buf(frame->payload), frame->payload_csum_le);
      frame->payload_csum_is_valid = true;
    }
  }

  MCU_ATOMIC_SECTION(sli_cpc_frame_list_extend(&drv->tx.submitted_frame_list, frames);)

  prime_dma_for_transmission(drv);

  return num_frames;
}

/**************************************************************************/ /**
 * Return the number of frames that can be queued in the driver.
 ******************************************************************************/
static uint32_t spi_drv_get_available_write_frame_slots(sl_cpc_bus_t *bus)
{
  const sl_cpc_drv_spi_peripheral_t *drv = to_drv(bus);
  uint32_t num_available_slots;

  MCU_ATOMIC_SECTION(num_available_slots
                     = (CPC_SPI_MAX_WRITE_SLOTS - sli_cpc_frame_list_get_len(&drv->tx.submitted_frame_list));)

  // Sanity check for underflow
  if (num_available_slots > CPC_SPI_MAX_WRITE_SLOTS) {
    SLI_CPC_ASSERT(false);
    return 0;
  }

  return num_available_slots;
}

/***************************************************************************/ /**
 * Check if buffer passed in parameter is only made of zeroes or not.
 ******************************************************************************/
static bool header_is_zeroes(const sli_cpc_drv_spi_header_t *buffer)
{
  for (size_t i = 0; i < sizeof(*buffer); i++) {
    if (((const uint8_t *)buffer)[i] != 0) {
      return false;
    }
  }

  return true;
}

static inline void log_header(const sli_cpc_drv_spi_header_t *header)
{
  (void)header; // Suppress unused parameter warning
  SLI_CPC_LOG_DEBUG("[SPI] Received header: [SoF: %02x | Size: %04x | Addr: %04x | ", header->sof,
                    sli_cpc_u16_from_le(header->hdr.size_le), sli_cpc_u16_from_le(header->hdr.addr_le));
  SLI_CPC_LOG_DEBUG("Ctrl: %02x | RX Wnd: %02x | Seq: %02x | ", header->hdr.ctrl, header->hdr.rx_wnd, header->hdr.seq);
  SLI_CPC_LOG_DEBUG("Ack: %02x | CRC: %04x]", header->hdr.ack, sli_cpc_u16_from_le(header->csum_le));
}

/**************************************************************************/ /**
 * Allocate RX frames until the free list reaches the maximum number of frames.
 ******************************************************************************/
static void alloc_rx_frames(sl_cpc_drv_spi_peripheral_t *drv)
{
  sl_cpc_frame_t *frame;
  while ((frame = sli_cpc_frame_new(&drv->bus, true))) {
    sli_cpc_frame_list_push_back(&drv->rx.free_frame_list, frame);
  }
}

static void end_of_header_xfer(sl_cpc_drv_spi_peripheral_t *drv)
{
  bool received_valid_header = false;
  size_t rx_payload_length = 0;
  uint16_t computed_crc;
  uint16_t received_crc;
  bool rx_dma_primed;

  if (!header_is_zeroes(&drv->rx.current_header)) {
#if defined(SL_CATALOG_CPC_DRIVER_HW_CRC_PRESENT)
    // GPCRC's autoInit is enabled, having read the GPCRC->DATA register restarted the GPCRC
    computed_crc = (uint16_t)GPCRC->DATAREV;
#else
    computed_crc = sli_cpc_crc((uint8_t *)&drv->rx.current_header, sizeof(drv->rx.current_header) - SLI_CPC_CRC_LEN);
#endif

    received_crc = sli_cpc_u16_from_le(drv->rx.current_header.csum_le);

    received_valid_header = computed_crc == received_crc;

    if (SL_BRANCH_UNLIKELY(!received_valid_header)) {
      SLI_CPC_LOG_DEBUG("[SPI] Invalid header checksum, discarding payload.");
      log_header(&drv->rx.current_header);
    }
  }

  rx_dma_primed = prime_dma_for_reception(drv, rx_payload_length, received_valid_header);

  if (SL_BRANCH_LIKELY(rx_dma_primed)) {
    // Ready for payload reception.
    // Clear the IRQ line to send the signal to the primary that we are done with
    // this interrupt critical section. The primary is now free to start clocking
    // us data.
    sl_hal_gpio_clear_pin(&drv->irq);
  } else {
    // Out of RX frames. IRQ will be held until
    // spi_drv_on_rx_frame_free is called.
    LDMA_TypeDef *ldma = sl_device_peripheral_ldma_get_base_addr((sl_peripheral_t)drv->dma_handle->dma_peripheral);
    sl_hal_ldma_clear_interrupts(ldma, (1 << drv->rx.dma_channel));
  }
}

/***************************************************************************/ /**
 * Notifies the core when a frame has been sent on the wire.
 *
 * @return true  : "end_of_header_xfer" needs to be tail chained to this routine
 *         false : The DMA callback can return after this function
 ******************************************************************************/
static bool end_of_payload_xfer(sl_cpc_drv_spi_peripheral_t *drv)
{
  bool tfer_done;
  // Used to keep track of whether the buffer held in "drv->tx.current_frame" did go out on the wire
  // after the last transaction completion or not.
  bool pending_late_header = false;
  LDMA_TypeDef *ldma = sl_device_peripheral_ldma_get_base_addr((sl_peripheral_t)drv->dma_handle->dma_peripheral);

  // Check if we had a late header transmission situation
  if (drv->tx.current_frame) {
    // At this point, a header have been exchanged and we realize a TX entry is registered for transmission
    // We need to know if this "drv->tx.current_frame" had the chance to have its header clocked in the header
    // exchange that just happened
    tfer_done = sl_hal_ldma_transfer_is_done(ldma, drv->tx.dma_channel);

    if (SL_BRANCH_UNLIKELY(drv->tx.frame_complete == 0 || !tfer_done)) {
      // This "drv->tx.current_frame" has arrived lated. Yes it is at this point the current active TX frame, but
      // at the moment the primary started clocking the header, its header did't go through. Mark this current
      // TX entry transmission as a "pending_late_header" so that we don't try to flush this "drv->tx.current_frame"
      // as having been sent on the wire.
      SLI_CPC_LOG_DEBUG("[SPI] Submitted late TX frame, will be sent on next transfer");
      pending_late_header = true;
    }
  }

  flush_rx(drv);

  if (SL_BRANCH_LIKELY(pending_late_header == false)) {
    flush_tx(drv);

    // Check to see if more data is available to be sent.
    prime_dma_for_transmission(drv);
  }

  tfer_done = sl_hal_ldma_transfer_is_done(ldma, drv->rx.dma_channel);
  if (SL_BRANCH_UNLIKELY(tfer_done)) {
    // If the RX DMA channel is already done here, it means as we just finished dealing with this frame,
    // the header of the next frame was received. This happens when this interrupt suffered a high interrupt
    // latency and its execution was delayed so much that the header interrupt of the next header is also pending.
    // Since there is one interrupt flag for this channel and its been set twice (once by this payload interrupt
    // descriptor, and once by the next header descriptor) the two event blended into one. If we were to just return
    // from the interrupt, the next header interrupt would be lost. Return true so that the higher callback can
    // call directly the header interrupt routine.
    SLI_CPC_LOG_DEBUG("[SPI] Received header before previous RX frame was processed");

    // Make sure to clear the DMA IRQ flag here. If the header arrived during the
    // execution of the handler, the IRQ flag will be set from the RX header descriptor
    // completing. If this happens, the handler will execute once more as though
    // a payload was received, when in fact it was the header that caused the flag
    // to be set. In other words, in order to avoid IRQ handler re-entry and an
    // eventual de-sync, make sure we clear the IRQ flag, because the header
    // reception has already been handled.
    sl_hal_ldma_clear_interrupts(ldma, (1 << (drv->rx.dma_channel)));
    return true;
  } else {
    return false;
  }
}

/***************************************************************************/ /**
 * This is the callback called by the LDMA IRQ handler when the RX DMA channel
 * fires an interrupt. Note that thanks to the synchronization between TX and RX
 * channels, only the RX channel triggers interrupts, not the TX channel; it
 * silently finishes its descriptor chain
 ******************************************************************************/
static void rx_dma_callback(void)
{
  uint8_t ch;
  void *user_data;
  sl_status_t status = sl_dma_manager_retrieve_current_channel_user_data(&ch, &user_data);
  SLI_CPC_ASSERT(status == SL_STATUS_OK);

  sl_cpc_drv_spi_peripheral_t *drv = user_data;
  SLI_CPC_ASSERT(drv != NULL);

  if (drv->rx.irq_state == SL_CPC_DRV_SPI_PERIPHERAL_IRQ_STATE_HEADER) {
    end_of_header_xfer(drv);

    // Received header chunk, expect payload chunk next.
    drv->rx.irq_state = SL_CPC_DRV_SPI_PERIPHERAL_IRQ_STATE_PAYLOAD;
  } else { // drv->rx.irq_state == SL_CPC_DRV_SPI_PERIPHERAL_IRQ_STATE_PAYLOAD
    bool piggy_back = end_of_payload_xfer(drv);

    // Received payload chunk, expect header chunk next.
    drv->rx.irq_state = SL_CPC_DRV_SPI_PERIPHERAL_IRQ_STATE_HEADER;

    if (SL_BRANCH_UNLIKELY(piggy_back)) {
      // There must have been a lot of interrupt latency and "end_of_payload_xfer" was delayed so long
      // that it was detected at the end of the servicing of the payload that the header of a next frame
      // was already fully received and ready to be serviced. This resulted in the LDMA interrupt bit for
      // this channel to be set while it was already set and not yet serviced, effectively "loosing" the
      // second interrupt bit set event. If we do not piggy-back the servicing of the header, that event
      // would be lost. Luckily we were able to logically detect that in the "end_of_payload_xfer" routine
      end_of_header_xfer(drv);

      // Interrupt latency caused an additional header interrupt to be received. Expect payload chunk next.
      drv->rx.irq_state = SL_CPC_DRV_SPI_PERIPHERAL_IRQ_STATE_PAYLOAD;
    }
  }
}

/**************************************************************************/ /**
 * Allocate and configure LDMA channels for RX, TX, and optional GPCRC.
 ******************************************************************************/
static sl_status_t ldma_init(sl_cpc_drv_spi_peripheral_t *drv)
{
  sl_status_t ret = sl_dma_manager_get_default_handle(&drv->dma_handle);
  if (ret != SL_STATUS_OK) {
    return ret;
  }

  ret = sl_dma_manager_allocate_channel(drv->dma_handle, &drv->rx.dma_channel);
  if (ret != SL_STATUS_OK) {
    return ret;
  }

  LDMA_TypeDef *ldma = sl_device_peripheral_ldma_get_base_addr((sl_peripheral_t)drv->dma_handle->dma_peripheral);

  if (drv->rx.dma_channel < 16) {
    sl_hal_ldma_enable_interrupts(ldma, (1 << drv->rx.dma_channel));
  }
#if defined(_LDMA_IFH_MASK)
  else {
    sl_hal_ldma_enable_high_interrupts(ldma, (1 << (drv->rx.dma_channel - 16)));
  }
#endif

  ret = sl_dma_manager_allocate_channel(drv->dma_handle, &drv->tx.dma_channel);
  if (ret != SL_STATUS_OK) {
    return ret;
  }

#if defined(SL_CATALOG_CPC_DRIVER_HW_CRC_PRESENT)
  ret = sl_dma_manager_allocate_channel(drv->dma_handle, &drv->rx.gpcrc_dma_channel);
  if (ret != SL_STATUS_OK) {
    return ret;
  }
#endif

  // The DMA channel that serves receive buffer should have higher priority than
  // the DMA channel that serves transmit buffer.
  if (drv->rx.dma_channel > drv->tx.dma_channel) {
    // A lower number DMA channel is higher priority. If the allocated DMA channels
    // gave us an inverted priority, switch those channels. They have been
    // allocated, its safe to just switch them.
    uint8_t tmp = drv->rx.dma_channel;
    drv->rx.dma_channel = drv->tx.dma_channel;
    drv->tx.dma_channel = tmp;
  }

  ret = sl_dma_manager_register_channel_irq_callback(drv->dma_handle, drv->rx.dma_channel, rx_dma_callback);
  if (ret != SL_STATUS_OK) {
    SLI_CPC_ASSERT(0);
    return SL_STATUS_INVALID_PARAMETER;
  }

  ret = sl_dma_manager_register_channel_user_data(drv->dma_handle, drv->rx.dma_channel, drv);
  if (ret != SL_STATUS_OK) {
    SLI_CPC_ASSERT(0);
    return ret;
  }

  // Create DMA configs for TX/RX from the DMA-REQ signals of the E/USART
  drv->rx.dma_config = (sl_hal_ldma_transfer_config_t)SL_HAL_LDMA_TRANSFER_CFG_PERIPHERAL(
    *sl_device_peripheral_get_serial_dma_signal_rx_trigger(drv->peripheral));
  drv->tx.dma_config = (sl_hal_ldma_transfer_config_t)SL_HAL_LDMA_TRANSFER_CFG_PERIPHERAL(
    *sl_device_peripheral_get_serial_dma_signal_tx_trigger(drv->peripheral));
#if defined(SL_CATALOG_CPC_DRIVER_HW_CRC_PRESENT)
  drv->rx.gpcrc_dma_config = (sl_hal_ldma_transfer_config_t)SL_HAL_LDMA_TRANSFER_CFG_MEMORY();
#endif

  drv->rx.dma_config.debug_halt_en = true;
  drv->tx.dma_config.debug_halt_en = true;
#if defined(SL_CATALOG_CPC_DRIVER_HW_CRC_PRESENT)
  drv->rx.gpcrc_dma_config.debug_halt_en = true;
#endif

  return SL_STATUS_OK;
}

/**************************************************************************/ /**
 * Configure the LDMA SYNCTRIG mechanism used to synchronize RX and TX DMA chains.
 *
 * One sync bit follows chip select: set on rising edge, cleared on falling edge.
 * There are two parts to that setup:
 *  - Set the Chip Select of (E)USART as signal producer for a PRS channel
 *  - Set the LDMA SYNCHW config to have a syncbit set and cleared based on that PRS channel
 *
 * The behaviour is not clearly defined when the PRS channel value is high and then
 * the syncbit is configured to be set on a rising edge. For some devices, that will
 * set the syncbit and on others it will not.
 *
 * To get deterministic behaviour, configure the LDMA first and only then connect the
 * chip select to the PRS channel. If chip select was high, this will make the LDMA
 * see a rising edge for all devices, and consequently the sync bit will be set.
 ******************************************************************************/
static void ldma_setup_synctrig(const sl_cpc_drv_spi_peripheral_t *drv)
{
  LDMA_TypeDef *ldma = sl_device_peripheral_ldma_get_base_addr((sl_peripheral_t)drv->dma_handle->dma_peripheral);

  // Enable the "usart_txc_prs_channel" PRS channel (a.k.a the USART TXC signal) to
  // be able to SET its corresponding SYNCTRIG[] bit
  ldma->SYNCHWEN_SET = (1UL << drv->prs_producers.txc.channel) << _LDMA_SYNCHWEN_SYNCSETEN_SHIFT;
  // The "usart_txc_prs_channel" SYNCTRIG bit don't need SYNCHWSEL modification (rising-edge to SET by default)

  // Enable the "usart_cs_prs_channel" PRS channel (a.k.a the USART CS signal) to
  // be able to SET and CLEAR its corresponding SYNCTRIG[] bit
  ldma->SYNCHWEN_SET
    = ((1UL << drv->prs_producers.cs.channel) << _LDMA_SYNCHWEN_SYNCSETEN_SHIFT) // CS will SET its SYNC bit
      | ((1UL << drv->prs_producers.cs.channel)
         << _LDMA_SYNCHWEN_SYNCCLREN_SHIFT); // CS will [also] CLEAR its SYNC bit.

  // Configure how USART CS will set/clear its SYNC bit
  // With this configuration, the SYNCTRIG[usart_txc_prs_channel] bit will reflect the state of the USART CS line :
  // == 1 when CS is high, and == 0 when CS is low
  ldma->SYNCHWSEL_SET = ((_LDMA_SYNCHWSEL_SYNCSETEDGE_RISE << drv->prs_producers.cs.channel)
                         << _LDMA_SYNCHWSEL_SYNCSETEDGE_SHIFT) // CS will SET its SYNC bit on rising edge
                        | ((_LDMA_SYNCHWSEL_SYNCSETEDGE_FALL << drv->prs_producers.cs.channel)
                           << _LDMA_SYNCHWSEL_SYNCCLREDGE_SHIFT); // CS will CLR its SYNC bit on falling edge
}

/**************************************************************************/ /**
 * Connect E/USART TXC and CS producers to their configured PRS channels.
 ******************************************************************************/
static void ldma_init_prs(const sl_cpc_drv_spi_peripheral_t *drv)
{
  sl_hal_prs_async_producer_signal_t signal;

  // Setup a PRS channel to connect the E/USART TXC signal
  // Make sure the SYNCTRIG/PRS combination is within bound
  SLI_CPC_ASSERT(drv->prs_producers.txc.channel <= 7);

  // Retrieve the connected signal
#if defined(_PRS_ASYNC_CH_CTRL_SOURCESEL_MASK)
  signal = (PRS->ASYNC_CH[drv->prs_producers.txc.channel].CTRL
            & (_PRS_ASYNC_CH_CTRL_SOURCESEL_MASK | _PRS_ASYNC_CH_CTRL_SIGSEL_MASK));
#else
  signal = (PRS->CH[drv->prs_producers.txc.channel].CTRL & (_PRS_CH_CTRL_SOURCESEL_MASK | _PRS_CH_CTRL_SIGSEL_MASK));
#endif

  // The configured TXC SYNCTRIG bit works with its corresponding PRS Channel number. Make sure that PRS Channel is not used.
  SLI_CPC_ASSERT(signal == SL_HAL_PRS_ASYNC_NONE);

  // The PRS Channel was free, now configure it with the TXC producer
  // provided by the USART/EUSART-specific init.
  sl_hal_prs_async_connect_channel_producer(drv->prs_producers.txc.channel, drv->prs_producers.txc.signal);

  // This driver needs to route the incoming Chip Select signal to a PRS channel.
  // Setup a PRS channel with the CS producer provided by the USART/EUSART-specific init.
  // Make sure the SYNCTRIG/PRS combination is within bound
  SLI_CPC_ASSERT(drv->prs_producers.cs.channel <= 7);

#if defined(_PRS_ASYNC_CH_CTRL_SOURCESEL_MASK)
  signal = (PRS->ASYNC_CH[drv->prs_producers.cs.channel].CTRL
            & (_PRS_ASYNC_CH_CTRL_SOURCESEL_MASK | _PRS_ASYNC_CH_CTRL_SIGSEL_MASK));
#else
  signal = (PRS->CH[drv->prs_producers.cs.channel].CTRL & (_PRS_CH_CTRL_SOURCESEL_MASK | _PRS_CH_CTRL_SIGSEL_MASK));
#endif

  // The configured CS SYNCTRIG bit works with its corresponding PRS Channel number. Make sure that PRS Channel is not used.
  SLI_CPC_ASSERT(signal == SL_HAL_PRS_ASYNC_NONE);

  sl_hal_prs_async_connect_channel_producer(drv->prs_producers.cs.channel, drv->prs_producers.cs.signal);
}

/**************************************************************************/ /**
 * Prepare the reception DMA descriptor chain (and optional GPCRC chain).
 ******************************************************************************/
static void prepare_rx_descriptors(sl_cpc_drv_spi_peripheral_t *drv)
{
  const uint32_t irq_pin_mask = 1UL << drv->irq.pin;
  volatile uint32_t *const irq_gpio_set_reg = &GPIO->P_SET[drv->irq.port].DOUT;
  const uint32_t cs_synctrig_bit = 1UL << drv->prs_producers.cs.channel;
  const uint32_t tx_ready_window_trig_bit_mask = 1UL << drv->tx_availability_synctrig_ch;
  const uint32_t enable_sync_on_cs_prs_channel = cs_synctrig_bit;
  const uint32_t sync_on_cs_prs_channel_equal_one = cs_synctrig_bit;
  const uint32_t sync_on_cs_prs_channel_equal_zero = 0;

  // This loop count is used by rx.desc.group.throw_away_extra_bytes to loop
  // over itself. Setting the loop count to 1 means it's going to do two
  // iterations. As this structure is passed as argument when starting
  // transfers, that means the loop count will be reinitialized to one every
  // time a new transfer is started, which is the expect behavior.
  drv->rx.dma_config.loop_count = 1;

  // Sync descriptor to wait for the CS high between the header and the payload
  drv->rx.desc.wait_cs_high_after_header = (sl_hal_ldma_descriptor_t)SL_HAL_LDMA_DESCRIPTOR_LINKABS_SYNC(
    0x0, 0x0, sync_on_cs_prs_channel_equal_one, enable_sync_on_cs_prs_channel);

  // Fixed branching skipping the 3 following descriptors used for the SERIES_2_CONFIG_5 hack
  drv->rx.desc.wait_cs_high_after_header.sync.link_addr
    = SL_HAL_LDMA_DESCRIPTOR_LINKABS_ADDR_TO_LINKADDR(&drv->rx.desc.wait_cs_low_before_payload);

  // Sync descriptor to wait for the CS low before payload clocking
  drv->rx.desc.wait_cs_low_before_payload = (sl_hal_ldma_descriptor_t)SL_HAL_LDMA_DESCRIPTOR_LINKABS_SYNC(
    0x0, 0x0, sync_on_cs_prs_channel_equal_zero, enable_sync_on_cs_prs_channel);

  drv->rx.desc.wait_cs_low_before_payload.sync.link_addr
    = SL_HAL_LDMA_DESCRIPTOR_LINKABS_ADDR_TO_LINKADDR(&drv->rx.desc.set_irq_high);

  // WRITE descriptor to set the IRQ pin high after the falling edge of CS of payload
  drv->rx.desc.set_irq_high
    = (sl_hal_ldma_descriptor_t)SL_HAL_LDMA_DESCRIPTOR_LINKABS_WRITE(irq_pin_mask, irq_gpio_set_reg);

  // This descriptor's branching address is computed before each time this chain is armed
  // If there is a payload to receive, it branches to the payload descriptor
  // If there is NO payload to receive, it branches to the sync descriptor that sets RXBLOCKEN

  // Transfer descriptor to receive the payload
  drv->rx.desc.recv_payload[0]
    = (sl_hal_ldma_descriptor_t)SL_HAL_LDMA_DESCRIPTOR_LINKABS_P2M(SL_HAL_LDMA_CTRL_SIZE_BYTE, drv->reg.rxdata,
                                                                   0,  // place holder for data pointer
                                                                   1); // place holder for length

  // When NOT large buffer, this descriptor's branching remains fixed
  drv->rx.desc.recv_payload[0].xfer.link_addr
    = SL_HAL_LDMA_DESCRIPTOR_LINKABS_ADDR_TO_LINKADDR(&drv->rx.desc.recv_payload_crc);

  // When large buffer, the branching is computed before each arming

  // Transfer descriptor to receive the large buffer payload
  drv->rx.desc.recv_payload[1]
    = (sl_hal_ldma_descriptor_t)SL_HAL_LDMA_DESCRIPTOR_LINKABS_P2M(SL_HAL_LDMA_CTRL_SIZE_BYTE, drv->reg.rxdata,
                                                                   0,  // place holder for payload pointer
                                                                   1); // place holder for length

  drv->rx.desc.recv_payload[1].xfer.link_addr
    = SL_HAL_LDMA_DESCRIPTOR_LINKABS_ADDR_TO_LINKADDR(&drv->rx.desc.recv_payload_crc);

  // Transfer descriptor to receive payload's crc
  drv->rx.desc.recv_payload_crc
    = (sl_hal_ldma_descriptor_t)SL_HAL_LDMA_DESCRIPTOR_LINKABS_P2M(SL_HAL_LDMA_CTRL_SIZE_BYTE, drv->reg.rxdata,
                                                                   0,                   // place holder for data pointer
                                                                   CPC_SPI_CHKSM_SIZE); // checksum's length

  drv->rx.desc.recv_payload_crc.xfer.link_addr
    = SL_HAL_LDMA_DESCRIPTOR_LINKABS_ADDR_TO_LINKADDR(&drv->rx.desc.rxblocken);

  // Write descriptor to enable the RXBLOCK of the E/USART to block incoming bytes
  drv->rx.desc.rxblocken
    = (sl_hal_ldma_descriptor_t)SL_HAL_LDMA_DESCRIPTOR_LINKABS_WRITE(drv->rx_blocken_cmd, drv->reg.cmd);

  // Fixed branching
  drv->rx.desc.rxblocken.wri.link_addr
    = SL_HAL_LDMA_DESCRIPTOR_LINKABS_ADDR_TO_LINKADDR(&drv->rx.desc.wait_cs_high_after_payload);

  // Sync descriptor to wait for the rising edge of the UART CS following the payload.
  drv->rx.desc.wait_cs_high_after_payload = (sl_hal_ldma_descriptor_t)SL_HAL_LDMA_DESCRIPTOR_LINKABS_SYNC(
    0x0, 0x0, sync_on_cs_prs_channel_equal_one, enable_sync_on_cs_prs_channel);

  // The payload interrupt is generated when this descriptor unblocks.
  // With HWCRC, since the IEN bit of this channel is voluntarily cleared, the IF bit might be pending
  // for a bit until the GPCRC DMA chain sets the IEN bit
  drv->rx.desc.wait_cs_high_after_payload.sync.done_ifs = 1;

  // Fixed branching to the descriptor following
  drv->rx.desc.wait_cs_high_after_payload.sync.link_addr
    = SL_HAL_LDMA_DESCRIPTOR_LINKABS_ADDR_TO_LINKADDR(&drv->rx.desc.group.throw_away_extra_bytes);

  // Transfer descriptor to throw away two bytes of data away. This is needed
  // when operating at high speed because the RXBLOCKEN command is not applied
  // immediately and up to two bytes might slip through in the RX FIFO.
  drv->rx.desc.group.throw_away_extra_bytes = (sl_hal_ldma_descriptor_t)SL_HAL_LDMA_DESCRIPTOR_LINKABS_M2M(
    SL_HAL_LDMA_CTRL_SIZE_BYTE, drv->reg.rxdata, &rx_dummy_byte, 1);

  // Fixed branching
  drv->rx.desc.group.throw_away_extra_bytes.xfer.dec_loop_count = 1;
  drv->rx.desc.group.throw_away_extra_bytes.xfer.link_addr
    = SL_HAL_LDMA_DESCRIPTOR_LINKABS_ADDR_TO_LINKADDR(&drv->rx.desc.group.throw_away_extra_bytes);

  // Sync descriptor to set the TX availability bit
#if defined(SL_CATALOG_CPC_DRIVER_HW_CRC_PRESENT)
  // When HC CRC is enabled, take advantage of this descriptor to set another sync trig bit that
  // will enable the 3rd DMA chain to compute the RX'ed data's CRC
  const uint32_t sync_trig_bits_to_set = tx_ready_window_trig_bit_mask | GPCRC_SYNC_BIT_MASK;
#else
  const uint32_t sync_trig_bits_to_set = tx_ready_window_trig_bit_mask;
#endif

  drv->rx.desc.group.set_availability_sync_bit
    = (sl_hal_ldma_descriptor_t)SL_HAL_LDMA_DESCRIPTOR_LINKABS_SYNC(sync_trig_bits_to_set, 0, 0, 0);

  // Fixed branching
  drv->rx.desc.group.set_availability_sync_bit.sync.link_addr
    = SL_HAL_LDMA_DESCRIPTOR_LINKABS_ADDR_TO_LINKADDR(&drv->rx.desc.rxblockdis);

  // Write descriptor to disable the RXBLOCK of the E/USART to accept incoming bytes
  drv->rx.desc.rxblockdis
    = (sl_hal_ldma_descriptor_t)SL_HAL_LDMA_DESCRIPTOR_LINKABS_WRITE(drv->rx_blockdis_cmd, drv->reg.cmd);

  drv->rx.desc.rxblockdis.wri.link_addr
    = SL_HAL_LDMA_DESCRIPTOR_LINKABS_ADDR_TO_LINKADDR(&drv->rx.desc.wait_cs_low_before_header);

  // Sync descriptor to wait for the falling edge of the UART CS of the next header.
  drv->rx.desc.wait_cs_low_before_header = (sl_hal_ldma_descriptor_t)SL_HAL_LDMA_DESCRIPTOR_LINKABS_SYNC(
    0, 0x0, sync_on_cs_prs_channel_equal_zero, enable_sync_on_cs_prs_channel);

  drv->rx.desc.wait_cs_low_before_header.sync.link_addr
    = SL_HAL_LDMA_DESCRIPTOR_LINKABS_ADDR_TO_LINKADDR(&drv->rx.desc.clear_availability_sync_bit);

  // Sync descriptor to clear the SYNCTRIG[7] bit
  drv->rx.desc.clear_availability_sync_bit = (sl_hal_ldma_descriptor_t)SL_HAL_LDMA_DESCRIPTOR_LINKABS_SYNC(
    0x0,
    tx_ready_window_trig_bit_mask, // Clears TX READY WINDOW trig bit when loaded
    0x0, 0x0);

  // Fixed branching to the descriptor following
  drv->rx.desc.clear_availability_sync_bit.sync.link_addr
    = SL_HAL_LDMA_DESCRIPTOR_LINKABS_ADDR_TO_LINKADDR(&drv->rx.desc.set_irq_high_after_header_cs_low);

  // Immediate write to set the IRQ pin high.
  drv->rx.desc.set_irq_high_after_header_cs_low
    = (sl_hal_ldma_descriptor_t)SL_HAL_LDMA_DESCRIPTOR_LINKABS_WRITE(irq_pin_mask, irq_gpio_set_reg);

  // Fixed branching to the descriptor following
  drv->rx.desc.set_irq_high_after_header_cs_low.wri.link_addr
    = SL_HAL_LDMA_DESCRIPTOR_LINKABS_ADDR_TO_LINKADDR(&drv->rx.desc.recv_header);

  // Transfer the received header. Because its a _SINGLE_ descriptor, the
  // .done_ifs bit is set and an interrupt will fire after it gets executed
  drv->rx.desc.recv_header = (sl_hal_ldma_descriptor_t)SL_HAL_LDMA_DESCRIPTOR_SINGLE_P2M(
    SL_HAL_LDMA_CTRL_SIZE_BYTE, drv->reg.rxdata, &drv->rx.current_header,
#if defined(SL_CATALOG_CPC_DRIVER_HW_CRC_PRESENT)
    // In case the CRC HW acceleration is enabled, only transfer to memory the first 8 bytes of the header
    // which are the effective header data. The remaining 2, the CRC, will be transfered after and if anything
    // they will accumulate in the E/USART's FIFO which is 2 + 1 (for the shift register) = 3 in the worst case (USART)
    sizeof(drv->rx.current_header) - sizeof(drv->rx.current_header.csum_le));
#else
    // In case the CRC HW acceleration is not enabled, transfer the full header like normal
    sizeof(drv->rx.current_header));
#endif

  // For Series 2 without CRC HW acceleration, the chain ends here (_SINGLE_ => .link = 0)

#if defined(SL_CATALOG_CPC_DRIVER_HW_CRC_PRESENT)
  LDMA_TypeDef *ldma = sl_device_peripheral_ldma_get_base_addr((sl_peripheral_t)drv->dma_handle->dma_peripheral);

  // In case of Series 1 and/or CRC HW acceleration, this descriptor is not the end of the chain and needs to
  // branch to the following one. Undo the _SINGLE_
  drv->rx.desc.recv_header.xfer.link = 1;

  // Force this descriptor to use absolute linking
  drv->rx.desc.recv_header.xfer.link_mode = SL_HAL_LDMA_LINK_MODE_ABS;

  // Override .done_ifs : Don't trigger an interrupt at the end of this descriptor
  drv->rx.desc.recv_header.xfer.done_ifs = 0;

  // Fixed branching to the descriptor following
  drv->rx.desc.recv_header.xfer.link_addr
    = SL_HAL_LDMA_DESCRIPTOR_LINKABS_ADDR_TO_LINKADDR(&drv->rx.desc.wait_gpcrc_sync_bit);

  // Sync descriptor to wait until the payload GPCRC calculation is done
  drv->rx.desc.wait_gpcrc_sync_bit = (sl_hal_ldma_descriptor_t)SL_HAL_LDMA_DESCRIPTOR_LINKABS_SYNC(
    0, 0,
    GPCRC_SYNC_BIT_MASK,  // Wait for the GPCRC launch bit to be 1
    GPCRC_SYNC_BIT_MASK); // Wait for the GPCRC launch bit

  drv->rx.desc.wait_gpcrc_sync_bit.sync.link_addr
    = SL_HAL_LDMA_DESCRIPTOR_LINKABS_ADDR_TO_LINKADDR(&drv->rx.desc.throw_header_in_gpcrc);

  // Memory-to-Memory descriptor to throw the 5 header byte into the GPCRC
  // Doing M2M single byte transfers with a DMA when the data in on hand is not optimal, but
  // here since its only 5 bytes its okay. The alternative would've been 1 1-word descriptor
  // chaining to 1 1-byte descriptor. The act of fetching a second 4-word descriptor for the
  // remaining byte should be about the same extra bus usage in the end.
  drv->rx.desc.throw_header_in_gpcrc = (sl_hal_ldma_descriptor_t)SL_HAL_LDMA_DESCRIPTOR_LINKABS_M2M(
    SL_HAL_LDMA_CTRL_SIZE_BYTE, &drv->rx.current_header.hdr, &GPCRC->INPUTDATABYTE, SLI_CPC_HDLC_HEADER_SIZE);

  // Because this is a M2M descriptor, the source increments (which is what we want) but the
  // destination as well. Here we send the header bytes to the same register, so override the
  // source increment to stay the same.
  drv->rx.desc.throw_header_in_gpcrc.xfer.dst_inc = SL_HAL_LDMA_CTRL_DST_INC_NONE;

  drv->rx.desc.throw_header_in_gpcrc.xfer.link_addr
    = SL_HAL_LDMA_DESCRIPTOR_LINKABS_ADDR_TO_LINKADDR(&drv->rx.desc.recv_header_crc);

  // P2M descriptor to receive the remaining 2 CRC bytes of the header from the E/USART
  drv->rx.desc.recv_header_crc = (sl_hal_ldma_descriptor_t)SL_HAL_LDMA_DESCRIPTOR_SINGLE_P2M(
    SL_HAL_LDMA_CTRL_SIZE_BYTE, drv->reg.rxdata,
    &drv->rx.current_header.csum_le, // Append to the already received 5 bytes of header
    SLI_CPC_HDLC_FCS_SIZE);

  // Prepare the GPCRC descriptor chain

  // Sync descriptor to wait until the payload was received to start computing the CRC, which is set by the RX DMA chain
  drv->rx.gpcrc_desc.wait_for_gpcrc_sync_bit = (sl_hal_ldma_descriptor_t)SL_HAL_LDMA_DESCRIPTOR_LINKABS_SYNC(
    0, 0,
    GPCRC_SYNC_BIT_MASK,  // Wait for the GPCRC sync bit to be 1
    GPCRC_SYNC_BIT_MASK); // Wait for the GPCRC sync bit

  // Fixed branching
  drv->rx.gpcrc_desc.wait_for_gpcrc_sync_bit.sync.link_addr
    = SL_HAL_LDMA_DESCRIPTOR_LINKABS_ADDR_TO_LINKADDR(&drv->rx.gpcrc_desc.clear_gpcrc_sync_bit);

  // Sync descriptor to clear the launch sync bit
  drv->rx.gpcrc_desc.clear_gpcrc_sync_bit = (sl_hal_ldma_descriptor_t)SL_HAL_LDMA_DESCRIPTOR_LINKABS_SYNC(
    0,
    GPCRC_SYNC_BIT_MASK, // Immediately clear the GPCRC sync bit
    0, 0);

  // The branching is computed each time the header interrupt occurs

  // Memory-to-Memory descriptor to write payload into the GPCRC
  // This descriptor takes care of the entire 4-byte words
  // To decrease the time it takes for the DMA to write the data in the GPCRC and decreases the number
  // of transaction on the memory bus, we do as many 4-byte transfers as possible.
  // If the payload length is not divisible by 4, the remaining 1,2 or 3 bytes will be handled by
  // the descriptors that follow
  drv->rx.gpcrc_desc.write_rx_payload_words_in_gpcrc = (sl_hal_ldma_descriptor_t)SL_HAL_LDMA_DESCRIPTOR_LINKABS_M2M(
    SL_HAL_LDMA_CTRL_SIZE_WORD, NULL, // Placeholder for the payload pointer
    &GPCRC->INPUTDATA,                // The WORD input register
    0);                               // Placeholder for the length

  // Because this is a M2M descriptor, the source increments (which is what we want) but the
  // destination as well. Here we send the payload bytes to the same register, so override the
  // source increment to stay the same.
  drv->rx.gpcrc_desc.write_rx_payload_words_in_gpcrc.xfer.dst_inc = SL_HAL_LDMA_CTRL_DST_INC_NONE;

  // The branching will be computed each time the header interrupt happens in function of the payload_len % 4 result

  // M2M descriptor to potentially transfer remaining 1-to-3 bytes of payload into the GPCRC engine (modulo 4 of payload length)
  drv->rx.gpcrc_desc.write_rx_remaining_payload_bytes_in_gpcrc
    = (sl_hal_ldma_descriptor_t)SL_HAL_LDMA_DESCRIPTOR_LINKABS_M2M(SL_HAL_LDMA_CTRL_SIZE_BYTE,
                                                                   NULL, // Placeholder for the payload pointer
                                                                   &GPCRC->INPUTDATABYTE, // The BYTE input register
                                                                   0);                    // Placeholder, will be 1-to-3

  // Because this is a M2M descriptor, the source increments (which is what we want) but the
  // destination as well. Here we send the payload bytes to the same register, so override the
  // source increment to stay the same.
  drv->rx.gpcrc_desc.write_rx_remaining_payload_bytes_in_gpcrc.xfer.dst_inc = SL_HAL_LDMA_CTRL_DST_INC_NONE;

  // Fixed to the descriptor below
  drv->rx.gpcrc_desc.write_rx_remaining_payload_bytes_in_gpcrc.xfer.link_addr
    = SL_HAL_LDMA_DESCRIPTOR_LINKABS_ADDR_TO_LINKADDR(&drv->rx.gpcrc_desc.save_rx_gpcrc_computed_crc);

  // M2M descriptor to transfer the single half-word GPCRC result in the buffer's .fcs field
  drv->rx.gpcrc_desc.save_rx_gpcrc_computed_crc = (sl_hal_ldma_descriptor_t)SL_HAL_LDMA_DESCRIPTOR_LINKABS_M2M(
    SL_HAL_LDMA_CTRL_SIZE_HALF, &GPCRC->DATAREV, // Read the CRC from the engine
    NULL,                                        // Placeholder for the buffer's .fcs pointer
    1);                                          // Only one half-word

  drv->rx.gpcrc_desc.save_rx_gpcrc_computed_crc.xfer.link_addr
    = SL_HAL_LDMA_DESCRIPTOR_LINKABS_ADDR_TO_LINKADDR(&drv->rx.gpcrc_desc.set_rxchain_ien_bit);

  // Write descriptor to set RX DMA chain IEN bit to trigger an interrupt
  drv->rx.gpcrc_desc.set_rxchain_ien_bit
    = (sl_hal_ldma_descriptor_t)SL_HAL_LDMA_DESCRIPTOR_LINKABS_WRITE((1 << drv->rx.dma_channel), &ldma->IEN_SET);

  drv->rx.gpcrc_desc.set_rxchain_ien_bit.xfer.link_addr
    = SL_HAL_LDMA_DESCRIPTOR_LINKABS_ADDR_TO_LINKADDR(&drv->rx.gpcrc_desc.set_gpcrc_sync_bit);

  // Sync descriptor to set the launch sync bit
  drv->rx.gpcrc_desc.set_gpcrc_sync_bit
    = (sl_hal_ldma_descriptor_t)SL_HAL_LDMA_DESCRIPTOR_SINGLE_SYNC(GPCRC_SYNC_BIT_MASK, // Immediately set the GPCRC bit
                                                                   0, 0, 0);

  // Override .done_ifs, do not generate interrupt
  drv->rx.gpcrc_desc.set_gpcrc_sync_bit.sync.done_ifs = 0;
#endif
}

/**************************************************************************/ /**
 * Prepare the transmission DMA descriptor chain.
 ******************************************************************************/
static void prepare_tx_descriptors(sl_cpc_drv_spi_peripheral_t *drv)
{
  const uint32_t irq_pin_mask = 1UL << drv->irq.pin;
  volatile uint32_t *const irq_gpio_clr_reg = &GPIO->P_CLR[drv->irq.port].DOUT;
  const uint32_t txc_synctrig_bit = 1UL << drv->prs_producers.txc.channel;
  const uint32_t tx_ready_window_trig_bit_mask = 1UL << drv->tx_availability_synctrig_ch;

  // Sync descriptor to wait to be in the TX_AVAILABLE region
  drv->tx.desc.wait_availability_sync_bit = (sl_hal_ldma_descriptor_t)SL_HAL_LDMA_DESCRIPTOR_LINKABS_SYNC(
    0x0, 0x0, tx_ready_window_trig_bit_mask, tx_ready_window_trig_bit_mask);

  // Fixed branching
  drv->tx.desc.wait_availability_sync_bit.sync.link_addr
    = SL_HAL_LDMA_DESCRIPTOR_LINKABS_ADDR_TO_LINKADDR(&drv->tx.desc.set_irq_low);

  // Set the IRQ pin LOW
  drv->tx.desc.set_irq_low
    = (sl_hal_ldma_descriptor_t)SL_HAL_LDMA_DESCRIPTOR_LINKABS_WRITE(irq_pin_mask, irq_gpio_clr_reg);

  drv->tx.desc.set_irq_low.wri.link_addr = SL_HAL_LDMA_DESCRIPTOR_LINKABS_ADDR_TO_LINKADDR(&drv->tx.desc.xfer_header);

  // Send header
  drv->tx.desc.xfer_header = (sl_hal_ldma_descriptor_t)SL_HAL_LDMA_DESCRIPTOR_LINKREL_M2P(
    SL_HAL_LDMA_CTRL_SIZE_BYTE,
    &drv->tx.current_header, //Place holder for header buffer address
    drv->reg.txdata, sizeof(drv->tx.current_header),
    1); // Overridden below

  // The macro SL_HAL_LDMA_DESCRIPTOR_LINKABS_M2P does not exist as a convenience BYTE form for whatever reason.
  // Force this descriptor to use absolute linking
  drv->tx.desc.xfer_header.xfer.link_mode = SL_HAL_LDMA_LINK_MODE_ABS;

  // Override .done_ifs : Don't trigger an interrupt at the end of this descriptor
  drv->tx.desc.xfer_header.xfer.done_ifs = 0;

  drv->tx.desc.xfer_header.xfer.link_addr
    = SL_HAL_LDMA_DESCRIPTOR_LINKABS_ADDR_TO_LINKADDR(&drv->tx.desc.wait_header_tranfered);

  // Wait until the header has been sent on the wire
  drv->tx.desc.wait_header_tranfered = (sl_hal_ldma_descriptor_t)SL_HAL_LDMA_DESCRIPTOR_LINKABS_SYNC(
    0x0, txc_synctrig_bit, txc_synctrig_bit, txc_synctrig_bit);

  // Fixed branching
  drv->tx.desc.wait_header_tranfered.sync.link_addr
    = SL_HAL_LDMA_DESCRIPTOR_LINKABS_ADDR_TO_LINKADDR(&drv->tx.desc.set_tx_frame_complete_variable_after_header);

  // Write descriptor to set the "drv->tx.frame_complete" variable to 1.
  drv->tx.desc.set_tx_frame_complete_variable_after_header
    = (sl_hal_ldma_descriptor_t)SL_HAL_LDMA_DESCRIPTOR_LINKABS_WRITE(1, &drv->tx.frame_complete);

  // The link bit will be set or cleared depending on whether there is a payload to send

  drv->tx.desc.set_tx_frame_complete_variable_after_header.wri.link_addr
    = SL_HAL_LDMA_DESCRIPTOR_LINKABS_ADDR_TO_LINKADDR(&drv->tx.desc.xfer_payload[0]);

  // Branch if there is a payload

  // Transfer descriptor for the payload
  drv->tx.desc.xfer_payload[0] = (sl_hal_ldma_descriptor_t)SL_HAL_LDMA_DESCRIPTOR_LINKREL_M2P(
    SL_HAL_LDMA_CTRL_SIZE_BYTE, NULL, // Place holder for the payload address
    drv->reg.txdata,
    1,  // Place holder for the payload length
    1); // Overridden below

  // The macro SL_HAL_LDMA_DESCRIPTOR_LINKABS_M2P does not exist as a convenience BYTE form for whatever reason.
  // Force this descriptor to use absolute linking
  drv->tx.desc.xfer_payload[0].xfer.link_mode = SL_HAL_LDMA_LINK_MODE_ABS;

  // Override .done_ifs : Don't trigger an interrupt at the end of this descriptor
  drv->tx.desc.xfer_payload[0].xfer.done_ifs = 0;

  // In the case of NO large buffer and NO security, this descriptor branches  to the
  // send checksum descriptor no matter what, else its link address is computed each
  // time the chain is arms. In the case it never changes, set it right now
  drv->tx.desc.xfer_payload[0].xfer.link_addr
    = SL_HAL_LDMA_DESCRIPTOR_LINKABS_ADDR_TO_LINKADDR(&drv->tx.desc.xfer_checksum);

  // Transfer descriptor for the large buffer payload
  drv->tx.desc.xfer_payload[1] = (sl_hal_ldma_descriptor_t)SL_HAL_LDMA_DESCRIPTOR_LINKABS_P2M(
    SL_HAL_LDMA_CTRL_SIZE_BYTE, NULL, // Place holder for the payload address
    drv->reg.txdata,
    1); // Place holder for the payload length

  // In the case of large buffer and NO security, this descriptor branches  to the
  // send checksum descriptor no matter what, else its link address is computed each
  // time the chain is arms. In the case it never changes, set it right now
  drv->tx.desc.xfer_payload[1].xfer.link_addr
    = SL_HAL_LDMA_DESCRIPTOR_LINKABS_ADDR_TO_LINKADDR(&drv->tx.desc.xfer_checksum);

#if defined(SL_CATALOG_CPC_SECURITY_PRESENT)
  // If the security is enabled and there is a security tag to send, send it before the payload checksum.
  drv->tx.desc.xfer_tag = (sl_hal_ldma_descriptor_t)SL_HAL_LDMA_DESCRIPTOR_LINKABS_P2M(
    SL_HAL_LDMA_CTRL_SIZE_BYTE, NULL, /* Place holder for the tag address */
    drv->reg.txdata, SLI_SECURITY_TAG_LENGTH_BYTES);

  // Fixed branching
  drv->tx.desc.xfer_tag.xfer.link_addr = SL_HAL_LDMA_DESCRIPTOR_LINKABS_ADDR_TO_LINKADDR(&drv->tx.desc.xfer_checksum);
#endif

  // Send the checksum
  drv->tx.desc.xfer_checksum = (sl_hal_ldma_descriptor_t)SL_HAL_LDMA_DESCRIPTOR_SINGLE_M2P(
    SL_HAL_LDMA_CTRL_SIZE_BYTE, NULL, // Placeholder for the checksum
    drv->reg.txdata, CPC_SPI_CHKSM_SIZE);

  // Override .done_ifs : TX DMA chain doesn't trigger interrupts
  drv->tx.desc.xfer_checksum.xfer.done_ifs = 0;
}

#if defined(SL_CATALOG_CPC_DRIVER_HW_CRC_PRESENT)
/**************************************************************************/ /**
 * Initialize and start the GPCRC hardware for RX CRC acceleration.
 ******************************************************************************/
static void gpcrc_setup(void)
{
  sl_hal_gpcrc_init_t init = {
    .reverse_byte_order = false, // Important: The bytes in the full-word writes are already in the right order
    .reverse_bits = true,        // Important: Our software CRC has bit order reversed; match the host
    .enable_byte_mode = false,   // Important: We use the full word-size INPUT capability
    .auto_init = true,           // Important: Re-start GPCRC automatically when DMA reads the DATA register
    .crc_poly = 0x1021,          // CRC-16 XMODEM 16 bits polynomial
    .init_value = 0x0000,        // CRC-16 XMODEM 16 bits init value
  };

  sl_hal_gpcrc_init(GPCRC, &init);
  sl_hal_gpcrc_enable(GPCRC);

  // Manually load the INIT value in the DATA register to initialize the first CRC computation
  // After that, it will be done automatically when reading the DATA register (thanks to auto_init)
  sl_hal_gpcrc_start(GPCRC);
}
#endif

/**************************************************************************/ /**
 * Notification when RX buffer handle becomes free.
 ******************************************************************************/
static void spi_drv_on_rx_frame_free(sl_cpc_bus_t *bus)
{
  sl_cpc_drv_spi_peripheral_t *drv = to_drv(bus);
  MCU_DECLARE_IRQ_STATE;

  MCU_ENTER_ATOMIC();
  alloc_rx_frames(drv);

  if (drv->rx.need_frame && !sli_cpc_frame_list_empty(&drv->rx.free_frame_list)) {
    // Driver was out of RX frames. Resume the reception.
    drv->rx.need_frame = false;
    SLI_CPC_LOG_DEBUG("Got RX frame, resume RX");
    end_of_header_xfer(drv);
  }
  MCU_EXIT_ATOMIC();
}

/***************************************************************************/ /**
 * Initialize the rest of the driver after the SPI peripheral has been
 * initialized by the USART/EUSART-specific public init.
 ******************************************************************************/
static sl_status_t spi_drv_init(sl_cpc_bus_t *bus)
{
  sl_cpc_drv_spi_peripheral_t *drv = to_drv(bus);
  sl_status_t ret;

  // Initialize the lists
  sli_cpc_frame_list_init(&drv->rx.free_frame_list);
  sli_cpc_frame_list_init(&drv->rx.pending_frame_list);
  sli_cpc_frame_list_init(&drv->tx.submitted_frame_list);
  sli_cpc_frame_list_init(&drv->tx.completed_frame_list);

  drv->tx.current_header.sof = SLI_CPC_DRV_SPI_SOF;
  drv->rx.irq_state = SL_CPC_DRV_SPI_PERIPHERAL_IRQ_STATE_HEADER;

  alloc_rx_frames(drv);

  ret = ldma_init(drv);
  if (ret != SL_STATUS_OK) {
    SLI_CPC_ASSERT(0);
    return ret;
  }

  ldma_setup_synctrig(drv);
  ldma_init_prs(drv);
  prepare_rx_descriptors(drv);
  prepare_tx_descriptors(drv);

#if defined(SL_CATALOG_POWER_MANAGER_PRESENT)
  sl_power_manager_add_em_requirement(SL_POWER_MANAGER_EM1);
#endif

#if defined(SL_CATALOG_CPC_DRIVER_HW_CRC_PRESENT)
  gpcrc_setup();
#endif

  return SL_STATUS_OK;
}

/***************************************************************************/ /**
 * Bind shared driver ops and initialize the embedded CPC bus.
 *
 * Clocks, GPIOs, and the (E)USART peripheral must already be configured by the
 * USART/EUSART-specific public init before this is called.
 ******************************************************************************/
sl_status_t sli_cpc_drv_spi_peripheral_init(sl_cpc_drv_spi_peripheral_t *drv, const sl_cpc_bus_config_t *bus_cfg)
{
  static const sli_cpc_drv_ops_t ops = {
    .init = &spi_drv_init,
    .start_rx = &spi_drv_start_rx,
    .read = &spi_drv_read_data,
    .write = &spi_drv_write,
    .get_available_write_frame_slots = &spi_drv_get_available_write_frame_slots,
    .on_rx_frame_free = &spi_drv_on_rx_frame_free,
    .get_local_capabilities = &get_local_capabilities,
  };

  return sli_cpc_bus_init(&drv->bus, bus_cfg, &ops);
}
