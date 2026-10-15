/* Copyright 2026 Silicon Laboratories Inc. */

#ifndef SL_CPC_DRV_SPI_PERIPHERAL_H
#define SL_CPC_DRV_SPI_PERIPHERAL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "sl_component_catalog.h"
#include "sl_cpc_bus.h"
#include "sl_device_gpio.h"
#include "sl_device_peripheral.h"
#include "sl_dma_manager.h"
#include "sl_enum.h"
#include "sl_hal_ldma.h"
#include "sl_hal_prs_enums.h"

#include "sl_cpc_drv_spi.h"
#include "sli_cpc_drv.h"

#define SL_CPC_DRV_SPI_PERIPHERAL_LDMA_DESCRIPTORS 2

SL_ENUM(sl_cpc_drv_spi_peripheral_irq_state_t){
  SL_CPC_DRV_SPI_PERIPHERAL_IRQ_STATE_HEADER = 0,
  SL_CPC_DRV_SPI_PERIPHERAL_IRQ_STATE_PAYLOAD,
};

struct sli_cpc_drv_spi_peripheral_caps {
  uint8_t max_speed_le[4];
};

struct sl_cpc_drv_spi_peripheral_config {
  sl_peripheral_t peripheral;
  sl_gpio_t copi;
  sl_gpio_t cipo;
  sl_gpio_t clk;
  sl_gpio_t cs;
  sl_gpio_t irq;
  uint8_t cs_exti_no;
  uint8_t cs_synctrig_prs_ch;
  uint8_t txc_synctrig_prs_ch;
  uint8_t tx_availability_synctrig_ch;
};

struct sl_cpc_drv_spi_peripheral_prs_producer {
  uint8_t channel;                           ///< LDMA SYNCTRIG / PRS channel number
  sl_hal_prs_async_producer_signal_t signal; ///< PRS async producer signal
};

struct sl_cpc_drv_spi_peripheral_prs_producers {
  struct sl_cpc_drv_spi_peripheral_prs_producer txc;
  struct sl_cpc_drv_spi_peripheral_prs_producer cs;
};

struct sl_cpc_drv_spi_peripheral_registers {
  const volatile uint32_t *rxdata;
  volatile uint32_t *txdata;
  volatile uint32_t *cmd;
};

struct sl_cpc_drv_spi_peripheral_rx_ldma_desc {
  sl_hal_ldma_descriptor_t wait_cs_high_after_header;
  sl_hal_ldma_descriptor_t wait_cs_low_before_payload;
  sl_hal_ldma_descriptor_t set_irq_high;
  sl_hal_ldma_descriptor_t recv_payload[SL_CPC_DRV_SPI_PERIPHERAL_LDMA_DESCRIPTORS];
  sl_hal_ldma_descriptor_t recv_payload_crc;
  sl_hal_ldma_descriptor_t rxblocken;
  sl_hal_ldma_descriptor_t wait_cs_high_after_payload; // Generates the end-of-payload interrupt
  struct {
    // throw_away_extra_bytes is a loop descriptor. This kind of descriptor
    // jumps to the next descriptor in memory when loopcnt == 0. In order to
    // guarantee that the order between these two is correct, put them in a struct.
    sl_hal_ldma_descriptor_t throw_away_extra_bytes;
    sl_hal_ldma_descriptor_t set_availability_sync_bit; // When HWCRC enabled, also sets the GPCRC launch bit
  } group;
  sl_hal_ldma_descriptor_t rxblockdis;
  sl_hal_ldma_descriptor_t wait_cs_low_before_header;
  sl_hal_ldma_descriptor_t clear_availability_sync_bit;
  sl_hal_ldma_descriptor_t set_irq_high_after_header_cs_low;
  sl_hal_ldma_descriptor_t recv_header;
#if defined(SL_CATALOG_CPC_DRIVER_HW_CRC_PRESENT)
  sl_hal_ldma_descriptor_t wait_gpcrc_sync_bit;
  sl_hal_ldma_descriptor_t throw_header_in_gpcrc;
  sl_hal_ldma_descriptor_t recv_header_crc;
#endif
};

#if defined(SL_CATALOG_CPC_DRIVER_HW_CRC_PRESENT)
struct sl_cpc_drv_spi_peripheral_gpcrc_ldma_desc {
  sl_hal_ldma_descriptor_t wait_for_gpcrc_sync_bit;
  sl_hal_ldma_descriptor_t clear_gpcrc_sync_bit;
  sl_hal_ldma_descriptor_t write_rx_payload_words_in_gpcrc;
  sl_hal_ldma_descriptor_t write_rx_remaining_payload_bytes_in_gpcrc;
  sl_hal_ldma_descriptor_t save_rx_gpcrc_computed_crc;
  sl_hal_ldma_descriptor_t set_rxchain_ien_bit;
  sl_hal_ldma_descriptor_t set_gpcrc_sync_bit;
};
#endif

struct sl_cpc_drv_spi_peripheral_tx_ldma {
  sl_hal_ldma_descriptor_t wait_availability_sync_bit;
  sl_hal_ldma_descriptor_t set_irq_low;
  sl_hal_ldma_descriptor_t xfer_header;
  sl_hal_ldma_descriptor_t wait_header_tranfered;
  sl_hal_ldma_descriptor_t set_tx_frame_complete_variable_after_header;
  sl_hal_ldma_descriptor_t xfer_payload[SL_CPC_DRV_SPI_PERIPHERAL_LDMA_DESCRIPTORS];
#if defined(SL_CATALOG_CPC_SECURITY_PRESENT)
  sl_hal_ldma_descriptor_t xfer_tag;
#endif
  sl_hal_ldma_descriptor_t xfer_checksum;
};

struct sl_cpc_drv_spi_peripheral_rx {
  uint8_t dma_channel;
  sl_hal_ldma_transfer_config_t dma_config;

  // Pre-allocated frames available for receiving.
  sli_cpc_frame_list_t free_frame_list;
  // Frames that have been received and are waiting for the core.
  sli_cpc_frame_list_t pending_frame_list;

  // Frame currently used for reception (DMA destination), or NULL.
  sl_cpc_frame_t *current_frame;
  // DMA receive buffer for the header (avoids taking a frame until late).
  sli_cpc_drv_spi_header_t current_header;

  bool need_frame;
  sl_cpc_drv_spi_peripheral_irq_state_t irq_state;

  struct sl_cpc_drv_spi_peripheral_rx_ldma_desc desc;
#if defined(SL_CATALOG_CPC_DRIVER_HW_CRC_PRESENT)
  uint8_t gpcrc_dma_channel;
  sl_hal_ldma_transfer_config_t gpcrc_dma_config;
  struct sl_cpc_drv_spi_peripheral_gpcrc_ldma_desc gpcrc_desc;
#endif
};

/***************************************************************************/ /**
 * Per-instance TX runtime context.
 ******************************************************************************/
struct sl_cpc_drv_spi_peripheral_tx {
  uint8_t dma_channel;
  sl_hal_ldma_transfer_config_t dma_config;

  // Frames ready for transmission waiting to be sent on the wire.
  sli_cpc_frame_list_t submitted_frame_list;
  // Frames that have been completely sent over the bus.
  sli_cpc_frame_list_t completed_frame_list;

  // Frame currently being transmitted / configured in the TX DMA chain, or NULL.
  sl_cpc_frame_t *current_frame;
  // Raw header of current_frame with its checksum.
  sli_cpc_drv_spi_header_t current_header;

  // Set to 1 by the TX DMA; cleared by software.
  volatile uint32_t frame_complete;

  struct sl_cpc_drv_spi_peripheral_tx_ldma desc;
};

typedef struct sl_cpc_drv_spi_peripheral {
  sl_peripheral_t peripheral;
  sl_dma_handle_t *dma_handle;
  sl_cpc_bus_t bus;
  struct sli_cpc_drv_spi_peripheral_caps local_caps;
  struct sl_cpc_drv_spi_peripheral_prs_producers prs_producers;
  uint32_t rx_blocken_cmd;
  uint32_t rx_blockdis_cmd;
  struct sl_cpc_drv_spi_peripheral_registers reg;
  sl_gpio_t irq;
  uint8_t tx_availability_synctrig_ch;

  struct sl_cpc_drv_spi_peripheral_rx rx;
  struct sl_cpc_drv_spi_peripheral_tx tx;
} sl_cpc_drv_spi_peripheral_t;

#endif /* SL_CPC_DRV_SPI_PERIPHERAL_H */
