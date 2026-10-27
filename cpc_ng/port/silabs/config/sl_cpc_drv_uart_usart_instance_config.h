/***************************************************************************/ /**
 * @file sl_cpc_drv_uart_usart_instance_config.h
 * @brief CPC UART USART driver instance configuration.
 *******************************************************************************
 * # License
 * <b>Copyright 2026 Silicon Laboratories Inc. www.silabs.com</b>
 *******************************************************************************
 *
 * SPDX-License-Identifier: Zlib
 *
 * The licensor of this software is Silicon Laboratories Inc.
 *
 * This software is provided 'as-is', without any express or implied
 * warranty. In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 *
 ******************************************************************************/

#ifndef SL_CPC_DRV_UART_USART_INSTANCE_CONFIG_H
#define SL_CPC_DRV_UART_USART_INSTANCE_CONFIG_H

#include "sl_cpc_wake.h"
#include "sl_gpio.h"

// <<< Use Configuration Wizard in Context Menu >>>

// <h> CPC UART Driver Configuration

// <h> Frame pool configuration

// <o SL_CPC_DRV_UART_USART_INSTANCE_RX_FRAME_POOL_COUNT> RX frame pool size
// <i> Default: 20
// <i> Number of CPC RX frames that can be allocated for this UART instance
#define SL_CPC_DRV_UART_USART_INSTANCE_RX_FRAME_POOL_COUNT 20

// <o SL_CPC_DRV_UART_USART_INSTANCE_TX_FRAME_POOL_COUNT> TX frame pool size
// <i> Default: 20
// <i> Number of CPC TX frames that can be allocated for this UART instance
#define SL_CPC_DRV_UART_USART_INSTANCE_TX_FRAME_POOL_COUNT 20

// </h>

// <h> UART settings

// <o SL_CPC_DRV_UART_USART_INSTANCE_BAUDRATE> Baud rate
// <i> Default: 115200
#define SL_CPC_DRV_UART_USART_INSTANCE_BAUDRATE 115200

// <o SL_CPC_DRV_UART_USART_INSTANCE_FLOW_CONTROL> Flow control method
// <SL_UART_FLOW_CONTROL_NONE=> None
// <SL_UART_FLOW_CONTROL_CTS_RTS=> CTS/RTS hardware handshake
// <i> Default: SL_UART_FLOW_CONTROL_CTS_RTS
// <i> The CPC UART driver requires hardware flow control.
#define SL_CPC_DRV_UART_USART_INSTANCE_FLOW_CONTROL SL_UART_FLOW_CONTROL_CTS_RTS

// <o SL_CPC_DRV_UART_USART_INSTANCE_OVERSAMPLING> Oversampling
// <SL_UART_OVERSAMPLING_0=> 0
// <SL_UART_OVERSAMPLING_4=> 4
// <SL_UART_OVERSAMPLING_6=> 6
// <SL_UART_OVERSAMPLING_8=> 8
// <SL_UART_OVERSAMPLING_16=> 16
// <i> Default: SL_UART_OVERSAMPLING_16
#define SL_CPC_DRV_UART_USART_INSTANCE_OVERSAMPLING SL_UART_OVERSAMPLING_16

// </h>

// <h> Wake / power configuration

// <o SL_CPC_DRV_UART_USART_INSTANCE_POWER_MODE> Local power mode
// <SL_CPC_POWER_MODE_ALWAYS_ON=> Always on
// <SL_CPC_POWER_MODE_WAKE_FROM_GPIO=> Wake from GPIO
// <i> Default: Always on
#define SL_CPC_DRV_UART_USART_INSTANCE_POWER_MODE SL_CPC_POWER_MODE_ALWAYS_ON

// <o SL_CPC_DRV_UART_USART_INSTANCE_WAKE_IN_RESPONSE> Wake-in response
// <SL_CPC_WAKE_IN_RESPONSE_NONE=> None
// <SL_CPC_WAKE_IN_RESPONSE_AWAKE=> Send AWAKE
// <i> Default: None
#define SL_CPC_DRV_UART_USART_INSTANCE_WAKE_IN_RESPONSE SL_CPC_WAKE_IN_RESPONSE_NONE

// <o SL_CPC_DRV_UART_USART_INSTANCE_WAKE_OUT_MODE> Wake-out ready mode
// <SL_CPC_WAKE_OUT_DISABLED=> Disabled
// <SL_CPC_WAKE_OUT_MODE_CONFIRM=> Wait for peer AWAKE then transmit
// <i> Default: Disabled
#define SL_CPC_DRV_UART_USART_INSTANCE_WAKE_OUT_MODE SL_CPC_WAKE_OUT_DISABLED

// <<< sl:start pin_tool >>>
// <gpio optional> SL_CPC_DRV_UART_USART_INSTANCE_WAKE_IN
// $[GPIO_SL_CPC_DRV_UART_USART_INSTANCE_WAKE_IN]
#define SL_CPC_DRV_UART_USART_INSTANCE_WAKE_IN_PORT SL_GPIO_PORT_A
#define SL_CPC_DRV_UART_USART_INSTANCE_WAKE_IN_PIN 0
// [GPIO_SL_CPC_DRV_UART_USART_INSTANCE_WAKE_IN]$

// <gpio optional> SL_CPC_DRV_UART_USART_INSTANCE_WAKE_OUT
// $[GPIO_SL_CPC_DRV_UART_USART_INSTANCE_WAKE_OUT]
#define SL_CPC_DRV_UART_USART_INSTANCE_WAKE_OUT_PORT SL_GPIO_PORT_A
#define SL_CPC_DRV_UART_USART_INSTANCE_WAKE_OUT_PIN 0
// [GPIO_SL_CPC_DRV_UART_USART_INSTANCE_WAKE_OUT]$
// <<< sl:end pin_tool >>>
// </h>
// </h>
// <<< end of configuration section >>>

// <<< sl:start pin_tool >>>
// <usart signal=TX,RX,CTS,RTS> SL_CPC_DRV_UART_USART_INSTANCE
// $[USART_SL_CPC_DRV_UART_USART_INSTANCE]
#warning "CPC UART USART peripheral not configured"
// #define SL_CPC_DRV_UART_USART_INSTANCE_PERIPHERAL                 USART0
// #define SL_CPC_DRV_UART_USART_INSTANCE_PERIPHERAL_NO              0
//
// #define SL_CPC_DRV_UART_USART_INSTANCE_TX_PORT                    SL_GPIO_PORT_A
// #define SL_CPC_DRV_UART_USART_INSTANCE_TX_PIN                     8
//
// #define SL_CPC_DRV_UART_USART_INSTANCE_RX_PORT                    SL_GPIO_PORT_A
// #define SL_CPC_DRV_UART_USART_INSTANCE_RX_PIN                     9
//
// #define SL_CPC_DRV_UART_USART_INSTANCE_CTS_PORT                   SL_GPIO_PORT_B
// #define SL_CPC_DRV_UART_USART_INSTANCE_CTS_PIN                    5
//
// #define SL_CPC_DRV_UART_USART_INSTANCE_RTS_PORT                   SL_GPIO_PORT_A
// #define SL_CPC_DRV_UART_USART_INSTANCE_RTS_PIN                    0
// [USART_SL_CPC_DRV_UART_USART_INSTANCE]$
// <<< sl:end pin_tool >>>

#endif /* SL_CPC_DRV_UART_USART_INSTANCE_CONFIG_H */
