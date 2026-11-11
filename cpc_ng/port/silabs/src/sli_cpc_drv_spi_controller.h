/* Copyright 2026 Silicon Laboratories Inc. */

#ifndef SLI_CPC_DRV_SPI_CONTROLLER_H
#define SLI_CPC_DRV_SPI_CONTROLLER_H

#include "sl_cpc_bus.h"
#include "sl_cpc_drv_spi_controller.h"
#include "sl_status.h"

/***************************************************************************/ /**
 * Initialize the SPI controller CPC bus.
 *
 * Peripheral-specific init (USART/EUSART) must configure clocks, GPIOs, and
 * the SPI peripheral before calling this function. The driver handle must
 * already be zeroed and initialized by the caller.
 *
 * @param[in] drv      SPI controller driver handle. Must not be NULL.
 * @param[in] spi_cfg  SPI controller configuration. Must not be NULL.
 * @param[in] bus_cfg  Bus configuration. Must not be NULL.
 *
 * @retval SL_STATUS_OK Bus initialized successfully.
 * @retval Other        An error occurred.
 ******************************************************************************/
sl_status_t sli_cpc_drv_spi_controller_init(struct sl_cpc_drv_spi_controller *drv,
                                            const struct sl_cpc_drv_spi_controller_config *spi_cfg,
                                            const sl_cpc_bus_config_t *bus_cfg);

#endif /* SLI_CPC_DRV_SPI_CONTROLLER_H */
