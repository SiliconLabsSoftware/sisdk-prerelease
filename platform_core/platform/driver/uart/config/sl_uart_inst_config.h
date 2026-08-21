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

// <<< Use Configuration Wizard in Context Menu >>>

// <h>UART settings

// <o SL_UART_INSTANCE_BAUDRATE> Baud rate
// <i> Default: 115200
// <i> May be set to SL_UART_BAUDRATE_AUTO for auto detection upon receiving 0x55 from the remote.
// <i> The detected baud rate may be read using sl_uart_get_line_config().
#define SL_UART_INSTANCE_BAUDRATE             115200

// <o SL_UART_INSTANCE_PARITY> Parity mode to use
// <SL_UART_PARITY_NONE=> No parity bit
// <SL_UART_PARITY_EVEN=> Even parity bit
// <SL_UART_PARITY_ODD=> Odd parity bit
// <i> Default: SL_UART_PARITY_NONE
#define SL_UART_INSTANCE_PARITY               SL_UART_PARITY_NONE

// <o SL_UART_INSTANCE_STOP_BITS> Number of stop bits to use
// <SL_UART_STOP_BITS_0_5=> 0.5
// <SL_UART_STOP_BITS_1=> 1
// <SL_UART_STOP_BITS_1_5=> 1.5
// <SL_UART_STOP_BITS_2=> 2
// <i> Default: SL_UART_STOP_BITS_1
#define SL_UART_INSTANCE_STOP_BITS            SL_UART_STOP_BITS_1

// <o SL_UART_INSTANCE_DATA_BITS> Number of data bits to use
// <SL_UART_DATA_BITS_4=> 4
// <SL_UART_DATA_BITS_5=> 5
// <SL_UART_DATA_BITS_6=> 6
// <SL_UART_DATA_BITS_7=> 7
// <SL_UART_DATA_BITS_8=> 8
// <i> Default: SL_UART_DATA_BITS_8
// <i> Note: EUSART peripherals only support 7 & 8 data bit frames.
#define SL_UART_INSTANCE_DATA_BITS              SL_UART_DATA_BITS_8

// <o SL_UART_INSTANCE_FLOW_CONTROL> Flow control method
// <SL_UART_FLOW_CONTROL_NONE=> None
// <SL_UART_FLOW_CONTROL_CTS_RTS=> CTS/RTS hardware handshake
// <i> Default: SL_UART_FLOW_CONTROL_NONE
#define SL_UART_INSTANCE_FLOW_CONTROL           SL_UART_FLOW_CONTROL_NONE

// <o SL_UART_INSTANCE_OVERSAMPLING> Oversampling selection
// <SL_UART_OVERSAMPLING_0=> 0x oversampling
// <SL_UART_OVERSAMPLING_4=> 4x oversampling
// <SL_UART_OVERSAMPLING_6=> 6x oversampling
// <SL_UART_OVERSAMPLING_8=> 8x oversampling
// <SL_UART_OVERSAMPLING_16=> 16x oversampling
// <i> Default: SL_UART_OVERSAMPLING_16
// <i> Note: USART peripheral does not support 0x oversampling.
#define SL_UART_INSTANCE_OVERSAMPLING           SL_UART_OVERSAMPLING_16

// </h> end UART config

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
