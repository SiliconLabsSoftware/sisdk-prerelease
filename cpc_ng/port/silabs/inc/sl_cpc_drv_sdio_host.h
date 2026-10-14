/* Copyright 2026 Silicon Laboratories Inc. */

#ifndef SL_CPC_DRV_SDIO_HOST_H
#define SL_CPC_DRV_SDIO_HOST_H

#include <stdalign.h>
#include <stddef.h>
#include <stdint.h>

#include <sl_sdhc_sdio.h> /* sl_sdhc_sdio_handle_t */

#include "sl_cpc_buf.h"                  /* SL_CPC_BUF_MIN_ALIGNMENT */
#include "sl_cpc_bus.h"                  /* sl_cpc_bus_t */
#include "sl_cpc_drv_sdio_host_config.h" /* SL_CPC_DRV_SDIO_HOST_RX_FRAME_POOL_COUNT */
#include "sl_cpc_ep.h"                   /* SL_CPC_EP_MAX_PAYLOAD_SIZE */
#include "sli_cpc_types.h"               /* sli_cpc_dispatcher_handle_t, sli_cpc_frame_list_t */

/** Maximum SDIO block size supported by the host driver flush buffer. */
#define SL_CPC_DRV_SDIO_HOST_MAX_BLOCK_SIZE 512

typedef enum {
  ADMA_IDLE,
  ADMA_RECEIVE,
  ADMA_TRANSMIT,
} adma_state_t;

struct sli_cpc_drv_caps {
  uint8_t max_aggregation;
};

/** @brief SDIO host CPC driver configuration. */
typedef struct sl_cpc_drv_sdio_host_config {
  sl_sdhc_sdio_handle_t *sdio_handle;
  uint8_t function_num;
  uint16_t block_size;
  sl_sdhc_clock_frequency_t max_sd_freq;
  sl_sdhc_bus_width_t max_bus_width;
  sl_sdhc_speed_mode_t max_speed_mode;
} sl_cpc_drv_sdio_host_config_t;

// Header block structure (frame_count + reserved; no flexible array so it embeds in drv).
struct sdio_hdr_block {
  uint8_t frame_count;
  uint8_t reserved[3];
};

typedef struct sl_cpc_drv_sdio_host {
  sl_cpc_bus_t bus;

  sl_sdhc_sdio_handle_t *sdio_handle;
  uint8_t function_num;
  uint16_t block_size;
  struct sli_cpc_drv_caps local_caps;
  struct sli_cpc_drv_caps remote_caps;

  // TX: frames waiting for header / payload / completion notify.
  sli_cpc_frame_list_t tx_header_pending_frames;

  // TX: frames waiting for payload.
  sli_cpc_frame_list_t tx_payload_pending_frames;

  // TX: frames waiting for transfer completion.
  sli_cpc_frame_list_t tx_pending_xfer_complete_frames;

  // RX: completed frames for core, and free frames for the next CMD53.
  sli_cpc_frame_list_t rx_completed_frames;

  // RX: free frames.
  sli_cpc_frame_list_t rx_free_frames;

  // RX: ADMA state.
  adma_state_t adma_state;

  // Card interrupt pending, used to defer CARDINT servicing out of ISR.
  bool card_int_pending;

  // RX: transfer length, used to store the advertised transfer length for the current CMD53 transfer.
  size_t rx_xfer_len;

  // Defers CARDINT servicing out of ISR; wakes CPC via SYSTEM.
  sli_cpc_dispatcher_handle_t card_irq_dispatcher;

  alignas(SL_CPC_BUF_MIN_ALIGNMENT) struct sdio_hdr_block sdio_hdr_block;
  alignas(8) sl_sdhc_adma_descriptor_t frame_descriptors[SL_CPC_DRV_SDIO_HOST_RX_FRAME_POOL_COUNT];
  uint32_t unaligned_payload_pool[SL_CPC_DRV_SDIO_HOST_RX_FRAME_POOL_COUNT];
  // CPC-1998: avoid having to waste this block, just for a flush.
  alignas(SL_CPC_BUF_MIN_ALIGNMENT) uint8_t flush_buff[SL_CPC_DRV_SDIO_HOST_MAX_BLOCK_SIZE];
  alignas(SL_CPC_BUF_MIN_ALIGNMENT) uint8_t rx_buffer[SL_CPC_EP_MAX_PAYLOAD_SIZE];
} sl_cpc_drv_sdio_host_t;

/***************************************************************************/ /**
 * Initialize an SDIO host CPC driver and its embedded bus.
 *
 * @param[in] drv  SDIO host driver handle. Must not be NULL.
 * @param[in] cfg  SDIO host driver configuration. Must not be NULL.
 * @param[in] bus_cfg  Bus configuration. Must not be NULL.
 *
 * @retval SL_STATUS_OK Driver and bus initialized successfully.
 * @retval Other        An error occurred.
 ******************************************************************************/
sl_status_t sl_cpc_drv_sdio_host_init(sl_cpc_drv_sdio_host_t *drv, const sl_cpc_drv_sdio_host_config_t *cfg,
                                      const sl_cpc_bus_config_t *bus_cfg);

/***************************************************************************/ /**
 * Get the CPC bus embedded in an SDIO host driver instance.
 *
 * @param[in] drv  SDIO host driver handle. Must not be NULL.
 * @return Pointer to the driver's bus.
 ******************************************************************************/
static inline sl_cpc_bus_t *sl_cpc_drv_sdio_host_get_bus(sl_cpc_drv_sdio_host_t *drv)
{
  return &drv->bus;
}

extern sl_cpc_drv_sdio_host_t sl_cpc_drv_sdio_host_instances[];

#endif /* SL_CPC_DRV_SDIO_HOST_H */
