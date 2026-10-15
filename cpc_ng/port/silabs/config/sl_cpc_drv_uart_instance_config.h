#ifndef SL_CPC_DRV_UART_INSTANCE_CONFIG_H
#define SL_CPC_DRV_UART_INSTANCE_CONFIG_H

#include "sl_cpc_wake.h"
#include "sl_gpio.h"

// <<< Use Configuration Wizard in Context Menu >>>

// <h> CPC UART Driver Configuration

// <h> Frame pool configuration

// <o SL_CPC_DRV_UART_INSTANCE_RX_FRAME_POOL_COUNT> RX frame pool size
// <i> Default: 20
// <i> Number of CPC RX frames that can be allocated for this UART instance
#define SL_CPC_DRV_UART_INSTANCE_RX_FRAME_POOL_COUNT 20

// <o SL_CPC_DRV_UART_INSTANCE_TX_FRAME_POOL_COUNT> TX frame pool size
// <i> Default: 20
// <i> Number of CPC TX frames that can be allocated for this UART instance
#define SL_CPC_DRV_UART_INSTANCE_TX_FRAME_POOL_COUNT 20

// </h>

// <h> Wake / power configuration

// <o SL_CPC_DRV_UART_INSTANCE_POWER_MODE> Local power mode
// <SL_CPC_POWER_MODE_ALWAYS_ON=> Always on
// <SL_CPC_POWER_MODE_WAKE_FROM_GPIO=> Wake from GPIO
// <i> Default: Always on
#define SL_CPC_DRV_UART_INSTANCE_POWER_MODE SL_CPC_POWER_MODE_ALWAYS_ON

// <o SL_CPC_DRV_UART_INSTANCE_WAKE_IN_RESPONSE> Wake-in response
// <SL_CPC_WAKE_IN_RESPONSE_NONE=> None
// <SL_CPC_WAKE_IN_RESPONSE_AWAKE=> Send AWAKE
// <i> Default: None
#define SL_CPC_DRV_UART_INSTANCE_WAKE_IN_RESPONSE SL_CPC_WAKE_IN_RESPONSE_NONE

// <o SL_CPC_DRV_UART_INSTANCE_WAKE_OUT_MODE> Wake-out ready mode
// <SL_CPC_WAKE_OUT_DISABLED=> Disabled
// <SL_CPC_WAKE_OUT_MODE_CONFIRM=> Wait for peer AWAKE then transmit
// <i> Default: Disabled
#define SL_CPC_DRV_UART_INSTANCE_WAKE_OUT_MODE SL_CPC_WAKE_OUT_DISABLED

// <<< sl:start pin_tool >>>
// <gpio optional> SL_CPC_DRV_UART_INSTANCE_WAKE_IN
// $[GPIO_SL_CPC_DRV_UART_INSTANCE_WAKE_IN]
#define SL_CPC_DRV_UART_INSTANCE_WAKE_IN_PORT SL_GPIO_PORT_A
#define SL_CPC_DRV_UART_INSTANCE_WAKE_IN_PIN 0
// [GPIO_SL_CPC_DRV_UART_INSTANCE_WAKE_IN]$

// <gpio optional> SL_CPC_DRV_UART_INSTANCE_WAKE_OUT
// $[GPIO_SL_CPC_DRV_UART_INSTANCE_WAKE_OUT]
#define SL_CPC_DRV_UART_INSTANCE_WAKE_OUT_PORT SL_GPIO_PORT_A
#define SL_CPC_DRV_UART_INSTANCE_WAKE_OUT_PIN 0
// [GPIO_SL_CPC_DRV_UART_INSTANCE_WAKE_OUT]$
// <<< sl:end pin_tool >>>
// </h>
// </h>
// <<< end of configuration section >>>

#endif /* SL_CPC_DRV_UART_INSTANCE_CONFIG_H */
