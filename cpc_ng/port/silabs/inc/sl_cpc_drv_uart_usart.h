/* Copyright 2026 Silicon Laboratories Inc. */

#ifndef SL_CPC_DRV_UART_USART_H
#define SL_CPC_DRV_UART_USART_H

#include "sl_cpc_bus.h"
#include "sl_cpc_drv_uart.h"

typedef struct sl_cpc_drv_uart_config sl_cpc_drv_uart_usart_config_t;
typedef struct sl_cpc_drv_uart sl_cpc_drv_uart_usart_t;

/***************************************************************************/ /**
 * Initialize an USART UART CPC driver and its embedded bus.
 *
 * @param[in] drv  UART driver handle. Must not be NULL.
 * @param[in] cfg  UART driver configuration. Must not be NULL.
 * @param[in] bus_cfg  Bus configuration. Must not be NULL.
 *
 * @retval SL_STATUS_OK Driver and bus initialized successfully.
 * @retval Other        An error occurred.
 ******************************************************************************/
sl_status_t sl_cpc_drv_uart_usart_init(sl_cpc_drv_uart_usart_t *drv, const sl_cpc_drv_uart_usart_config_t *cfg,
                                       const sl_cpc_bus_config_t *bus_cfg);

static inline sl_cpc_bus_t *sl_cpc_drv_uart_usart_get_bus(sl_cpc_drv_uart_usart_t *drv)
{
  return &drv->bus;
}

extern sl_cpc_drv_uart_usart_t sl_cpc_drv_uart_usart_instances[];

#endif /* SL_CPC_DRV_UART_USART_H */
