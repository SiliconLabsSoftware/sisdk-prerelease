/***************************************************************************/ /**
 * @file sl_cpc_drv_sdio_device.h
 * @brief Silicon Labs SDIO device driver interface for CPC.
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

#ifndef SL_CPC_DRV_SDIO_DEVICE_H
#define SL_CPC_DRV_SDIO_DEVICE_H

#include <stdalign.h>
#include <stddef.h>
#include <stdint.h>

#include "sl_hal_sdio_device.h" /* sl_hal_sdio_dev_dma_descriptor_t */

#include "sl_cpc_buf.h"                    /* SL_CPC_BUF_MIN_ALIGNMENT */
#include "sl_cpc_bus.h"                    /* sl_cpc_bus_t */
#include "sl_cpc_drv_sdio_device_config.h" /* SL_CPC_DRV_SDIO_DEVICE_RX_FRAME_POOL_COUNT, SL_CPC_DRV_SDIO_BLOCK_SIZE */
#include "sli_cpc_types.h"                 /* sli_cpc_frame_list_t */

typedef enum sli_cpc_drv_sdio_device_adma_state {
  SLI_CPC_DRV_SDIO_DEVICE_ADMA_STATE_IDLE,
  SLI_CPC_DRV_SDIO_DEVICE_ADMA_STATE_RECEIVE_HEADER,
  SLI_CPC_DRV_SDIO_DEVICE_ADMA_STATE_RECEIVE_PAYLOAD,
  SLI_CPC_DRV_SDIO_DEVICE_ADMA_STATE_TRANSMIT_HEADER,
  SLI_CPC_DRV_SDIO_DEVICE_ADMA_STATE_TRANSMIT_PAYLOAD,
  SLI_CPC_DRV_SDIO_DEVICE_ADMA_STATE_FLUSH_SDIO,
} sli_cpc_drv_sdio_device_adma_state_t;

struct sli_cpc_drv_sdio_device_caps {
  uint8_t max_aggregation;
};

/** @brief SDIO device CPC driver configuration. */
typedef struct sl_cpc_drv_sdio_device_config {
} sl_cpc_drv_sdio_device_config_t;

// Header block structure (frame_count + reserved; no flexible array so it embeds in drv).
struct sli_cpc_drv_sdio_device_hdr_block {
  uint8_t frame_count;
  uint8_t reserved[3];
};

typedef struct sl_cpc_drv_sdio_device {
  sl_cpc_bus_t bus;

  struct sli_cpc_drv_sdio_device_caps local_caps;
  struct sli_cpc_drv_sdio_device_caps remote_caps;

  // RX: frames with valid headers waiting for payload DMA.
  sli_cpc_frame_list_t rx_payload_pending_frames;

  // RX: frames that finished ADMA and wait for CMD53 write complete.
  sli_cpc_frame_list_t rx_complete_pending_frames;

  // RX: completed frames ready for the core.
  sli_cpc_frame_list_t rx_completed_frames;

  // RX: free frames ready for the next CMD53.
  sli_cpc_frame_list_t rx_free_frame_list;

  // RX: true when header RX stalled waiting for free frames.
  bool rx_alloc_stalled;

  // TX: true while a CMD53 read (device→host) is in progress.
  bool tx_busy;

  // TX: frames submitted by the core waiting to be armed.
  sli_cpc_frame_list_t tx_pending_frames;

  // TX: frames waiting for header DMA.
  sli_cpc_frame_list_t tx_header_pending_frames;

  // TX: frames waiting for payload DMA.
  sli_cpc_frame_list_t tx_payload_pending_frames;

  // TX: frames waiting for transfer completion notify.
  sli_cpc_frame_list_t tx_pending_xfer_complete_frames;

  // ADMA state machine.
  sli_cpc_drv_sdio_device_adma_state_t adma_state;

  alignas(SL_CPC_BUF_MIN_ALIGNMENT) struct sli_cpc_drv_sdio_device_hdr_block sdio_hdr_block;
  sl_hal_sdio_dev_dma_descriptor_t frame_descriptors[SL_CPC_DRV_SDIO_DEVICE_RX_FRAME_POOL_COUNT];
  uint32_t unaligned_payload_pool[SL_CPC_DRV_SDIO_DEVICE_RX_FRAME_POOL_COUNT];
  // CPC-1998: avoid having to waste this block, just for a flush.
  alignas(SL_CPC_BUF_MIN_ALIGNMENT) uint8_t flush_buff[SL_CPC_DRV_SDIO_BLOCK_SIZE];
  sl_hal_sdio_dev_dma_descriptor_t flush_desc;
} sl_cpc_drv_sdio_device_t;

/***************************************************************************/ /**
 * Initialize an SDIO device CPC driver and its embedded bus.
 *
 * @param[in] drv  SDIO device driver handle. Must not be NULL.
 * @param[in] cfg  SDIO device driver configuration. Must not be NULL.
 * @param[in] bus_cfg  Bus configuration. Must not be NULL.
 *
 * @retval SL_STATUS_OK Driver and bus initialized successfully.
 * @retval Other        An error occurred.
 ******************************************************************************/
sl_status_t sl_cpc_drv_sdio_device_init(sl_cpc_drv_sdio_device_t *drv, const sl_cpc_drv_sdio_device_config_t *cfg,
                                        const sl_cpc_bus_config_t *bus_cfg);

/***************************************************************************/ /**
 * SDIO peripheral interrupt handler for a driver instance.
 *
 * Called from the auto-generated per-instance IRQ vector.
 *
 * @param[in] drv  SDIO device driver handle. Must not be NULL.
 ******************************************************************************/
void sli_cpc_drv_sdio_device_irq_handler(sl_cpc_drv_sdio_device_t *drv);

/***************************************************************************/ /**
 * Get the CPC bus embedded in an SDIO device driver instance.
 *
 * @param[in] drv  SDIO device driver handle. Must not be NULL.
 * @return Pointer to the driver's bus.
 ******************************************************************************/
static inline sl_cpc_bus_t *sl_cpc_drv_sdio_device_get_bus(sl_cpc_drv_sdio_device_t *drv)
{
  return &drv->bus;
}

extern sl_cpc_drv_sdio_device_t sl_cpc_drv_sdio_device_instances[];

#endif /* SL_CPC_DRV_SDIO_DEVICE_H */
