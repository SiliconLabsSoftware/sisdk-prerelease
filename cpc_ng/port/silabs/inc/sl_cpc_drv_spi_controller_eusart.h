/* Copyright 2026 Silicon Laboratories Inc. */

#ifndef SL_CPC_DRV_SPI_CONTROLLER_EUSART_H
#define SL_CPC_DRV_SPI_CONTROLLER_EUSART_H

#include <stddef.h>
#include <stdint.h>

#include "sl_cpc_bus.h"
#include "sl_cpc_drv_spi_controller.h"
#include "sl_status.h"
#include "sli_cpc_drv.h"

/** @brief SPI controller CPC driver configuration (EUSART). */
typedef struct sl_cpc_drv_spi_controller_config sl_cpc_drv_spi_controller_eusart_config_t;

typedef struct sl_cpc_drv_spi_controller sl_cpc_drv_spi_controller_eusart_t;

/***************************************************************************/ /**
 * Initialize an EUSART-backed SPI controller CPC driver and its embedded bus.
 *
 * @param[in] drv  SPI controller EUSART driver handle. Must not be NULL.
 * @param[in] cfg  SPI controller EUSART driver configuration. Must not be NULL.
 * @param[in] bus_cfg  Bus configuration. Must not be NULL.
 *
 * @retval SL_STATUS_OK Driver and bus initialized successfully.
 * @retval Other        An error occurred.
 ******************************************************************************/
sl_status_t sl_cpc_drv_spi_controller_eusart_init(sl_cpc_drv_spi_controller_eusart_t *drv,
                                                  const sl_cpc_drv_spi_controller_eusart_config_t *cfg,
                                                  const sl_cpc_bus_config_t *bus_cfg);

/***************************************************************************/ /**
 * Get the CPC bus embedded in an SPI controller driver instance.
 *
 * @param[in] drv  SPI controller driver handle. Must not be NULL.
 * @return Pointer to the driver's bus.
 ******************************************************************************/
static inline sl_cpc_bus_t *sl_cpc_drv_spi_controller_eusart_get_bus(sl_cpc_drv_spi_controller_eusart_t *drv)
{
  return &drv->bus;
}

extern sl_cpc_drv_spi_controller_eusart_t sl_cpc_drv_spi_controller_eusart_instances[];

#endif /* SL_CPC_DRV_SPI_CONTROLLER_EUSART_H */
