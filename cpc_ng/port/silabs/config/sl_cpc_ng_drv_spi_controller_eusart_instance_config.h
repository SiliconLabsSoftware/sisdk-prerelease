/***************************************************************************/ /**
 * @file
 * @brief CPC SPI Controller EUSART driver configuration file.
 *******************************************************************************
 * # License
 * <b>Copyright 2020 Silicon Laboratories Inc. www.silabs.com</b>
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

#ifndef SL_CPC_NG_DRV_SPI_CONTROLLER_EUSART_INSTANCE_CONFIG_H
#define SL_CPC_NG_DRV_SPI_CONTROLLER_EUSART_INSTANCE_CONFIG_H

#include "sl_cpc_wake.h"
#include "sl_device_gpio.h"

// <<< Use Configuration Wizard in Context Menu >>>

// <h> CPC-Primary SPI Driver Configuration

// <h> Queues size configuration

// <o SL_CPC_DRV_SPI_CONTROLLER_EUSART_INSTANCE_RX_QUEUE_SIZE> Number of frame that can be queued in the driver receive queue
// <i> A greater number decreases the chances of retransmission due to dropped frames at the cost of memory footprint
// <i> Default : 10
// <d> 10
#define SL_CPC_DRV_SPI_CONTROLLER_EUSART_INSTANCE_RX_QUEUE_SIZE 10

// <o SL_CPC_DRV_SPI_CONTROLLER_EUSART_INSTANCE_TX_QUEUE_SIZE> Number of frame that can be queued in the driver transmit queue
// <i> A greater number increases the transmission responsiveness at the cost of memory footprint
// <i> Default : 10
// <d> 10
#define SL_CPC_DRV_SPI_CONTROLLER_EUSART_INSTANCE_TX_QUEUE_SIZE 10
// </h>

// <h> Frame pool configuration

// <o SL_CPC_DRV_SPI_CONTROLLER_EUSART_INSTANCE_RX_FRAME_POOL_COUNT> RX frame pool size
// <i> Default: 20
// <i> Number of CPC RX frames that can be allocated for this SPI instance
#define SL_CPC_DRV_SPI_CONTROLLER_EUSART_INSTANCE_RX_FRAME_POOL_COUNT 20

// <o SL_CPC_DRV_SPI_CONTROLLER_EUSART_INSTANCE_TX_FRAME_POOL_COUNT> TX frame pool size
// <i> Default: 20
// <i> Number of CPC TX frames that can be allocated for this SPI instance
#define SL_CPC_DRV_SPI_CONTROLLER_EUSART_INSTANCE_TX_FRAME_POOL_COUNT 20
// </h>

// <h> SPI bus configuration

// <o SL_CPC_DRV_SPI_CONTROLLER_EUSART_INSTANCE_BITRATE> SPI bitrate
// <i> Default: 1000000
#define SL_CPC_DRV_SPI_CONTROLLER_EUSART_INSTANCE_BITRATE 1000000
// </h>

// <<< end of configuration section >>>

// <<< sl:start pin_tool >>>
// <gpio> SL_CPC_DRV_SPI_CONTROLLER_EUSART_INSTANCE_IRQ
// $[GPIO_SL_CPC_DRV_SPI_CONTROLLER_EUSART_INSTANCE_IRQ]
#warning "CPC SPI IRQ not configured"
//#define SL_CPC_DRV_SPI_CONTROLLER_EUSART_INSTANCE_IRQ_PORT SL_GPIO_PORT_D
//#define SL_CPC_DRV_SPI_CONTROLLER_EUSART_INSTANCE_IRQ_PIN 2
// [GPIO_SL_CPC_DRV_SPI_CONTROLLER_EUSART_INSTANCE_IRQ]$

// <eusart signal=COPI,CIPO,SCLK,(CS)> SL_CPC_DRV_SPI_CONTROLLER_EUSART_EXP
// $[EUSART_SL_CPC_DRV_SPI_CONTROLLER_EUSART_EXP]
#warning "CPC SPI peripheral not configured"
// #define SL_CPC_DRV_SPI_CONTROLLER_EUSART_INSTANCE_PERIPHERAL EUSART1
// #define SL_CPC_DRV_SPI_CONTROLLER_EUSART_INSTANCE_PERIPHERAL_NO 1

// #define SL_CPC_DRV_SPI_CONTROLLER_EUSART_INSTANCE_COPI_PORT SL_GPIO_PORT_C
// #define SL_CPC_DRV_SPI_CONTROLLER_EUSART_INSTANCE_COPI_PIN 1

// #define SL_CPC_DRV_SPI_CONTROLLER_EUSART_INSTANCE_CIPO_PORT SL_GPIO_PORT_C
// #define SL_CPC_DRV_SPI_CONTROLLER_EUSART_INSTANCE_CIPO_PIN 2

// #define SL_CPC_DRV_SPI_CONTROLLER_EUSART_INSTANCE_SCLK_PORT SL_GPIO_PORT_C
// #define SL_CPC_DRV_SPI_CONTROLLER_EUSART_INSTANCE_SCLK_PIN 3

// #define SL_CPC_DRV_SPI_CONTROLLER_EUSART_INSTANCE_CS_PORT SL_GPIO_PORT_C
// #define SL_CPC_DRV_SPI_CONTROLLER_EUSART_INSTANCE_CS_PIN 0
// [USART_SL_CPC_DRV_SPI_CONTROLLER_EUSART_EXP]$
// <<< sl:end pin_tool >>>

#endif /* SL_CPC_NG_DRV_SPI_CONTROLLER_EUSART_INSTANCE_CONFIG_H */
