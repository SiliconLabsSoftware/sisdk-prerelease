/***************************************************************************//**
 * @file
 * @brief UART instance configuration
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

#ifndef SL_UART_INSTANCE_CONFIG_H
#define SL_UART_INSTANCE_CONFIG_H

#include "sl_device_gpio.h"

// <<< end of configuration section >>>

// <<< sl:start pin_tool >>>
// <eusart signal=TX,RX,(CTS),(RTS)> SL_UART_INSTANCE
// $[EUSART_SL_UART_INSTANCE]
#warning "UART peripheral not configured"
// #define SL_UART_INSTANCE_PERIPHERAL      EUSART0
// #define SL_UART_INSTANCE_PERIPHERAL_NO   0

// #define SL_UART_INSTANCE_TX_PORT         SL_GPIO_PORT_A
// #define SL_UART_INSTANCE_TX_PIN          5

// #define SL_UART_INSTANCE_RX_PORT         SL_GPIO_PORT_A
// #define SL_UART_INSTANCE_RX_PIN          6

// #define SL_UART_INSTANCE_CTS_PORT        SL_GPIO_PORT_A
// #define SL_UART_INSTANCE_CTS_PIN         12

// #define SL_UART_INSTANCE_RTS_PORT        SL_GPIO_PORT_C
// #define SL_UART_INSTANCE_RTS_PIN         8
// [EUSART_SL_UART_INSTANCE]$
// <<< sl:end pin_tool >>>

#endif // SL_UART_INSTANCE_CONFIG_H
