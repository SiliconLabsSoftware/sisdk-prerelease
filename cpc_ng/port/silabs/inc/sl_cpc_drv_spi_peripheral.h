/* Copyright 2026 Silicon Laboratories Inc. */

#ifndef SL_CPC_DRV_SPI_PERIPHERAL_H
#define SL_CPC_DRV_SPI_PERIPHERAL_H

#include <stddef.h>
#include <stdint.h>

#include "sl_cpc_bus.h"
#include "sli_cpc_drv.h"

struct sli_cpc_drv_caps {
  uint8_t max_speed_le[4];
};

/** @brief SPI peripheral CPC driver configuration. */
typedef struct sl_cpc_drv_spi_peripheral_config {
} sl_cpc_drv_spi_peripheral_config_t;

typedef struct sl_cpc_drv_spi_peripheral {
  sl_cpc_bus_t bus;
  struct sli_cpc_drv_caps local_caps;
} sl_cpc_drv_spi_peripheral_t;

/***************************************************************************/ /**
 * Initialize an SPI peripheral CPC driver and its embedded bus.
 *
 * @param[in] drv  SPI peripheral driver handle. Must not be NULL.
 * @param[in] cfg  SPI peripheral driver configuration. Must not be NULL.
 * @param[in] bus_cfg  Bus configuration. Must not be NULL.
 *
 * @retval SL_STATUS_OK Driver and bus initialized successfully.
 * @retval Other        An error occurred.
 ******************************************************************************/
sl_status_t sl_cpc_drv_spi_peripheral_init(sl_cpc_drv_spi_peripheral_t *drv,
                                           const sl_cpc_drv_spi_peripheral_config_t *cfg,
                                           const sl_cpc_bus_config_t *bus_cfg);

/***************************************************************************/ /**
 * Get the CPC bus embedded in an SPI peripheral driver instance.
 *
 * @param[in] drv  SPI peripheral driver handle. Must not be NULL.
 * @return Pointer to the driver's bus.
 ******************************************************************************/
static inline sl_cpc_bus_t *sl_cpc_drv_spi_peripheral_get_bus(sl_cpc_drv_spi_peripheral_t *drv)
{
  return &drv->bus;
}

extern sl_cpc_drv_spi_peripheral_t sl_cpc_drv_spi_peripheral_instances[];

#endif /* SL_CPC_DRV_SPI_PERIPHERAL_H */
