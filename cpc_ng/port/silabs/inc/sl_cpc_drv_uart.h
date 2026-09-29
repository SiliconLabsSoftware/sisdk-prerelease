/* Copyright 2026 Silicon Laboratories Inc. */

#ifndef SL_CPC_DRV_UART_H
#define SL_CPC_DRV_UART_H

#include <stddef.h>

#include "sl_cpc_bus.h"
#include "sl_iostream.h"
#include "sl_iostream_uart.h"
#include "sli_cpc_drv.h"

/** @brief UART CPC driver configuration. */
typedef struct sl_cpc_drv_uart_config {
  sl_iostream_t *iostream;
  sl_iostream_uart_t *iostream_uart;
} sl_cpc_drv_uart_config_t;

typedef struct sl_cpc_drv_uart {
  sl_cpc_bus_t bus;
  sl_iostream_t *iostream;
  sl_iostream_uart_t *iostream_uart;
} sl_cpc_drv_uart_t;

/***************************************************************************/ /**
 * Initialize a UART CPC driver and its embedded bus.
 *
 * @param[in] drv  UART driver handle. Must not be NULL.
 * @param[in] cfg  UART driver configuration. Must not be NULL.
 * @param[in] bus_cfg  Bus configuration. Must not be NULL.
 *
 * @retval SL_STATUS_OK Driver and bus initialized successfully.
 * @retval Other        An error occurred.
 ******************************************************************************/
sl_status_t sl_cpc_drv_uart_init(sl_cpc_drv_uart_t *drv, const sl_cpc_drv_uart_config_t *cfg,
                                 const sl_cpc_bus_config_t *bus_cfg);

/***************************************************************************/ /**
 * Get the CPC bus embedded in a UART driver instance.
 *
 * @param[in] drv  UART driver handle. Must not be NULL.
 * @return Pointer to the driver's bus.
 ******************************************************************************/
static inline sl_cpc_bus_t *sl_cpc_drv_uart_get_bus(sl_cpc_drv_uart_t *drv)
{
  return &drv->bus;
}

extern sl_cpc_drv_uart_t sl_cpc_drv_uart_instances[];

#endif /* SL_CPC_DRV_UART_H */
