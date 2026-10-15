/***************************************************************************/ /**
 * @file
 * @brief CPC SPI SECONDARY driver configuration file.
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

#ifndef SL_CPC_DRV_SPI_PERIPHERAL_USART_INSTANCE_CONFIG_H
#define SL_CPC_DRV_SPI_PERIPHERAL_USART_INSTANCE_CONFIG_H

#include "sl_cpc_wake.h"
#include "sl_gpio.h"

// <<< Use Configuration Wizard in Context Menu >>>

// <h> CPC-Secondary SPI Driver Configuration

// <h> Queues size configuration

// <o SL_CPC_DRV_SPI_PERIPHERAL_USART_INSTANCE_RX_QUEUE_SIZE> Number of frame that can be queued in the driver receive queue
// <i> A greater number decreases the chances of retransmission due to dropped frames at the cost of memory footprint
// <i> Default : 10
// <d> 10
#define SL_CPC_DRV_SPI_PERIPHERAL_USART_INSTANCE_RX_QUEUE_SIZE 10

// <o SL_CPC_DRV_SPI_PERIPHERAL_USART_INSTANCE_TX_QUEUE_SIZE> Number of frame that can be queued in the driver transmit queue
// <i> A greater number increases the transmission responsiveness at the cost of memory footprint
// <i> Default : 10
// <d> 10
#define SL_CPC_DRV_SPI_PERIPHERAL_USART_INSTANCE_TX_QUEUE_SIZE 10
// </h>

// <h> Frame pool configuration

// <o SL_CPC_DRV_SPI_PERIPHERAL_USART_INSTANCE_RX_FRAME_POOL_COUNT> RX frame pool size
// <i> Default: 20
// <i> Number of CPC RX frames that can be allocated for this SPI instance
#define SL_CPC_DRV_SPI_PERIPHERAL_USART_INSTANCE_RX_FRAME_POOL_COUNT 20

// <o SL_CPC_DRV_SPI_PERIPHERAL_USART_INSTANCE_TX_FRAME_POOL_COUNT> TX frame pool size
// <i> Default: 20
// <i> Number of CPC TX frames that can be allocated for this SPI instance
#define SL_CPC_DRV_SPI_PERIPHERAL_USART_INSTANCE_TX_FRAME_POOL_COUNT 20
// </h>

// <h> Wake / power configuration

// <o SL_CPC_DRV_SPI_PERIPHERAL_EUSART_INSTANCE_POWER_MODE> Local power mode
// <SL_CPC_POWER_MODE_ALWAYS_ON=> Always on
// <SL_CPC_POWER_MODE_WAKE_FROM_GPIO=> Wake from GPIO
// <i> Default: Always on
#define SL_CPC_DRV_SPI_PERIPHERAL_EUSART_INSTANCE_POWER_MODE SL_CPC_POWER_MODE_ALWAYS_ON

// <o SL_CPC_DRV_SPI_PERIPHERAL_EUSART_INSTANCE_WAKE_IN_RESPONSE> Wake-in response
// <SL_CPC_WAKE_IN_RESPONSE_NONE=> None
// <SL_CPC_WAKE_IN_RESPONSE_AWAKE=> Send AWAKE
// <i> Default: None
#define SL_CPC_DRV_SPI_PERIPHERAL_EUSART_INSTANCE_WAKE_IN_RESPONSE SL_CPC_WAKE_IN_RESPONSE_NONE

// <o SL_CPC_DRV_SPI_PERIPHERAL_EUSART_INSTANCE_WAKE_OUT_MODE> Wake-out ready mode
// <SL_CPC_WAKE_OUT_DISABLED=> Disabled
// <SL_CPC_WAKE_OUT_MODE_CONFIRM=> Wait for peer AWAKE then transmit
// <i> Default: Disabled
#define SL_CPC_DRV_SPI_PERIPHERAL_EUSART_INSTANCE_WAKE_OUT_MODE SL_CPC_WAKE_OUT_DISABLED

// <<< sl:start pin_tool >>>
// <gpio optional> SL_CPC_DRV_SPI_PERIPHERAL_EUSART_INSTANCE_WAKE_IN
// $[GPIO_SL_CPC_DRV_SPI_PERIPHERAL_EUSART_INSTANCE_WAKE_IN]
// #define SL_CPC_DRV_SPI_PERIPHERAL_EUSART_INSTANCE_WAKE_IN_PORT  SL_GPIO_PORT_A
// #define SL_CPC_DRV_SPI_PERIPHERAL_EUSART_INSTANCE_WAKE_IN_PIN   0
// [GPIO_SL_CPC_DRV_SPI_PERIPHERAL_EUSART_INSTANCE_WAKE_IN]$

// <gpio optional> SL_CPC_DRV_SPI_PERIPHERAL_EUSART_INSTANCE_WAKE_OUT
// $[GPIO_SL_CPC_DRV_SPI_PERIPHERAL_EUSART_INSTANCE_WAKE_OUT]
// #define SL_CPC_DRV_SPI_PERIPHERAL_EUSART_INSTANCE_WAKE_OUT_PORT  SL_GPIO_PORT_A
// #define SL_CPC_DRV_SPI_PERIPHERAL_EUSART_INSTANCE_WAKE_OUT_PIN   0
// [GPIO_SL_CPC_DRV_SPI_PERIPHERAL_EUSART_INSTANCE_WAKE_OUT]$
// <<< sl:end pin_tool >>>
// </h>

// <h> LDMA SYNCTRIG bit configuration
// <o SL_CPC_DRV_SPI_PERIPHERAL_USART_INSTANCE_TX_AVAILABILITY_SYNCTRIG_CH> The LDMA SYNCTRIG bit number for the TX availability [bit] <0-7>
// <i> Specify which SYNCTRIG bit is used. Modify this value to avoid collisions if specific LDMA SYNCTRIG bits need to be used elsewhere in the project.
// <i> Default : 7
// <d> 7
#define SL_CPC_DRV_SPI_PERIPHERAL_USART_INSTANCE_TX_AVAILABILITY_SYNCTRIG_CH 6

// <o SL_CPC_DRV_SPI_PERIPHERAL_USART_INSTANCE_CS_SYNCTRIG_PRS_CH> The LDMA SYNCTRIG bit number for the CS PRS channel [bit] <0-7>
// <i> Specify which SYNCTRIG bit is used. Modify this value to avoid collisions if specific LDMA SYNCTRIG bits need to be used elsewhere in the project.
// <i> This bit number is tied to the equivalent PRS channel number. Keep that in mind if using PRS channels elsewhere in the project.
// <i> Default : 5
// <d> 5
#define SL_CPC_DRV_SPI_PERIPHERAL_USART_INSTANCE_CS_SYNCTRIG_PRS_CH 5

// <o SL_CPC_DRV_SPI_PERIPHERAL_USART_INSTANCE_TXC_SYNCTRIG_PRS_CH> The LDMA SYNCTRIG bit number for the TXC PRS channel [bit] <0-7>
// <i> Specify which SYNCTRIG bit is used. Modify this value to avoid collisions if specific LDMA SYNCTRIG bits need to be used elsewhere in the project.
// <i> This bit number is tied to the equivalent PRS channel number. Keep that in mind if using PRS channels elsewhere in the project.
// <i> Default : 6
// <d> 6
#define SL_CPC_DRV_SPI_PERIPHERAL_USART_INSTANCE_TXC_SYNCTRIG_PRS_CH 4
// </h>

// </h>
// <<< end of configuration section >>>

// <<< sl:start pin_tool >>>
// <gpio> SL_CPC_DRV_SPI_PERIPHERAL_USART_INSTANCE_IRQ
// $[GPIO_SL_CPC_DRV_SPI_PERIPHERAL_USART_INSTANCE_IRQ]
#warning "CPC SPI IRQ not configured"
// #define SL_CPC_DRV_SPI_PERIPHERAL_USART_INSTANCE_IRQ_PORT                   SL_GPIO_PORT_B
// #define SL_CPC_DRV_SPI_PERIPHERAL_USART_INSTANCE_IRQ_PIN                    14
// [GPIO_SL_CPC_DRV_SPI_PERIPHERAL_USART_INSTANCE_IRQ]$

// <usart signal=COPI,CIPO,CLK,(CS)> SL_CPC_DRV_SPI_PERIPHERAL_USART_INSTANCE
// $[USART_SL_CPC_DRV_SPI_PERIPHERAL_USART_INSTANCE]
#warning "CPC USART peripheral not configured"
// #define SL_CPC_DRV_SPI_PERIPHERAL_USART_INSTANCE_PERIPHERAL                 USART0
// #define SL_CPC_DRV_SPI_PERIPHERAL_USART_INSTANCE_PERIPHERAL_NO              0
//
// #define SL_CPC_DRV_SPI_PERIPHERAL_USART_INSTANCE_COPI_PORT                  SL_GPIO_PORT_D
// #define SL_CPC_DRV_SPI_PERIPHERAL_USART_INSTANCE_COPI_PIN                   13
//
// #define SL_CPC_DRV_SPI_PERIPHERAL_USART_INSTANCE_CIPO_PORT                  SL_GPIO_PORT_D
// #define SL_CPC_DRV_SPI_PERIPHERAL_USART_INSTANCE_CIPO_PIN                   14
//
// #define SL_CPC_DRV_SPI_PERIPHERAL_USART_INSTANCE_CLK_PORT                   SL_GPIO_PORT_D
// #define SL_CPC_DRV_SPI_PERIPHERAL_USART_INSTANCE_CLK_PIN                    15
//
// #define SL_CPC_DRV_SPI_PERIPHERAL_USART_INSTANCE_CS_PORT                    SL_GPIO_PORT_B
// #define SL_CPC_DRV_SPI_PERIPHERAL_USART_INSTANCE_CS_PIN                     11
// [USART_SL_CPC_DRV_SPI_PERIPHERAL_USART_INSTANCE]$
// <<< sl:end pin_tool >>>

#endif /* SL_CPC_DRV_SPI_PERIPHERAL_USART_INSTANCE_CONFIG_H */
