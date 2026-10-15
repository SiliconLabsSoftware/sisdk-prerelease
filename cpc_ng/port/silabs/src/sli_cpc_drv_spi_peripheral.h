/* Copyright 2026 Silicon Laboratories Inc. */

#ifndef SLI_CPC_DRV_SPI_PERIPHERAL_H
#define SLI_CPC_DRV_SPI_PERIPHERAL_H

#include "sl_cpc_bus.h"
#include "sl_cpc_drv_spi_peripheral.h"
#include "sl_status.h"

/***************************************************************************/ /**
 * Initialize the SPI peripheral CPC bus and bind the shared driver ops.
 *
 * Peripheral-specific init (USART/EUSART) must configure clocks, GPIOs, and
 * the SPI peripheral before calling this function. The driver handle must
 * already be zeroed and initialized by the caller.
 *
 * @param[in] drv      SPI peripheral driver handle. Must not be NULL.
 * @param[in] bus_cfg  Bus configuration. Must not be NULL.
 *
 * @retval SL_STATUS_OK Bus initialized successfully.
 * @retval Other        An error occurred.
 ******************************************************************************/
sl_status_t sli_cpc_drv_spi_peripheral_init(sl_cpc_drv_spi_peripheral_t *drv, const sl_cpc_bus_config_t *bus_cfg);

#endif /* SLI_CPC_DRV_SPI_PERIPHERAL_H */
