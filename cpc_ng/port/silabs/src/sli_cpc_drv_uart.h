/* Copyright 2026 Silicon Laboratories Inc. */

#ifndef SLI_CPC_DRV_UART_H
#define SLI_CPC_DRV_UART_H

#include "sl_cpc_bus.h"
#include "sl_cpc_drv_uart.h"
#include "sl_status.h"

/***************************************************************************/ /**
 * Initialize a UART CPC driver, its UART peripheral, and its embedded bus.
 *
 * @param[in] drv  UART driver handle. Must not be NULL.
 * @param[in] cfg  UART driver configuration. Must not be NULL.
 * @param[in] bus_cfg  Bus configuration. Must not be NULL.
 *
 * @retval SL_STATUS_OK Driver and bus initialized successfully.
 * @retval Other        An error occurred.
 ******************************************************************************/
sl_status_t sli_cpc_drv_uart_init(struct sl_cpc_drv_uart *drv, const struct sl_cpc_drv_uart_config *cfg,
                                  const sl_cpc_bus_config_t *bus_cfg);

#endif /* SLI_CPC_DRV_UART_H */
