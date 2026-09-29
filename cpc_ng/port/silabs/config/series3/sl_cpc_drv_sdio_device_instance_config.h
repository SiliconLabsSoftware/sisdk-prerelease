/***************************************************************************/ /**
 * @file
 * @brief CPC SDIO SECONDARY driver configuration file.
 *******************************************************************************
 * # License
 * <b>Copyright 2024 Silicon Laboratories Inc. www.silabs.com</b>
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

#ifndef SL_CPC_DRV_SDIO_DEVICE_INSTANCE_CONFIG_H
#define SL_CPC_DRV_SDIO_DEVICE_INSTANCE_CONFIG_H

// <<< Use Configuration Wizard in Context Menu >>>

// <h> CPC-Secondary SDIO Driver Configuration

// <h> Queues size configuration

// <o SL_CPC_DRV_SDIO_INSTANCE_RX_QUEUE_SIZE> Number of frame that can be queued in the driver receive queue
// <i> A greater number decreases the chances of retransmission due to dropped frames at the cost of memory footprint
// <i> Default : 10
// <d> 10
#define SL_CPC_DRV_SDIO_INSTANCE_RX_QUEUE_SIZE 10

// <o SL_CPC_DRV_SDIO_INSTANCE_TX_QUEUE_SIZE> Number of frame that can be queued in the driver transmit queue
// <i> A greater number increases the transmission responsiveness at the cost of memory footprint
// <i> Default : 10
// <d> 10
#define SL_CPC_DRV_SDIO_INSTANCE_TX_QUEUE_SIZE 10
// </h>

// <h> Frame pool configuration

// <o SL_CPC_DRV_SDIO_DEVICE_INSTANCE_RX_FRAME_POOL_COUNT> RX frame pool size
// <i> Default: 20
// <i> Number of CPC RX frames that can be allocated for this SDIO instance
#define SL_CPC_DRV_SDIO_DEVICE_INSTANCE_RX_FRAME_POOL_COUNT 20

// <o SL_CPC_DRV_SDIO_DEVICE_INSTANCE_TX_FRAME_POOL_COUNT> TX frame pool size
// <i> Default: 20
// <i> Number of CPC TX frames that can be allocated for this SDIO instance
#define SL_CPC_DRV_SDIO_DEVICE_INSTANCE_TX_FRAME_POOL_COUNT 20
// </h>

// <h> SDIO Device Configuration

// <o SL_CPC_DRV_SDIO_INSTANCE_FUNCTION_NUM> SDIO Function number used for CPC.
// <i> The SDIO function number must match with the function number used on the host. When in doubt, leave default value.
// <i> Default : 1
// <d> 1
#define SL_CPC_DRV_SDIO_INSTANCE_FUNCTION_NUM 1

// <o SL_CPC_DRV_SDIO_INSTANCE_BLOCK_SIZE> Block size used over the SDIO bus.
// <512=> 512 Bytes block size
// <i> Larger block size may increase throughput when transfering large buffers over CPC.
// <i> Default : 512
// <d> 1
#define SL_CPC_DRV_SDIO_INSTANCE_BLOCK_SIZE 512

// <o SL_CPC_DRV_SDIO_INSTANCE_HOST_1V8_ONLY> Host only supports 1.8V signaling.
// <true=> Host supports only 1.8V.
// <false=> Host supports 3.3V and 1.8V signaling.
// <i> Only used when SPEED_MODE is UHS-I. With hosts only supporting 1.8V,
// <i> the SDIO peripheral will skip voltage negotiation and use 1.8V directly.
// <i> Default : false
// <d> 0
#define SL_CPC_DRV_SDIO_INSTANCE_HOST_1V8_ONLY false

// <o SL_CPC_DRV_SDIO_INSTANCE_SPEED_MODE> SDIO device speed mode advertised to the host.
// <SL_HAL_SDIO_DEV_SPEED_LOW=> Low Speed
// <SL_HAL_SDIO_DEV_SPEED_FULL=> Full Speed
// <SL_HAL_SDIO_DEV_SPEED_HIGH=> High Speed
// <SL_HAL_SDIO_DEV_SPEED_UHS_I=> UHS-I
// <i> Default : Full Speed
// <d> SL_HAL_SDIO_DEV_SPEED_FULL
#define SL_CPC_DRV_SDIO_INSTANCE_SPEED_MODE SL_HAL_SDIO_DEV_SPEED_FULL

// </h>
// </h>
// <<< end of configuration section >>>

// <<< sl:start pin_tool >>>

// <sdio signal=SCLK,CMD,DAT0,DAT1,DAT2,DAT3> SL_CPC_DRV_SDIO
// $[SL_CPC_DRV_SDIO]
#define SL_CPC_DRV_SDIO_INSTANCE_PERIPHERAL SDIO0
#define SL_CPC_DRV_SDIO_INSTANCE_PERIPHERAL_NO 0

#define SL_CPC_DRV_SDIO_INSTANCE_SCLK_PORT SL_GPIO_PORT_D
#define SL_CPC_DRV_SDIO_INSTANCE_SCLK_PIN 0

#define SL_CPC_DRV_SDIO_INSTANCE_CMD_PORT SL_GPIO_PORT_D
#define SL_CPC_DRV_SDIO_INSTANCE_CMD_PIN 1

#define SL_CPC_DRV_SDIO_INSTANCE_DAT0_PORT SL_GPIO_PORT_D
#define SL_CPC_DRV_SDIO_INSTANCE_DAT0_PIN 2

#define SL_CPC_DRV_SDIO_INSTANCE_DAT1_PORT SL_GPIO_PORT_D
#define SL_CPC_DRV_SDIO_INSTANCE_DAT1_PIN 3

#define SL_CPC_DRV_SDIO_INSTANCE_DAT2_PORT SL_GPIO_PORT_D
#define SL_CPC_DRV_SDIO_INSTANCE_DAT2_PIN 4

#define SL_CPC_DRV_SDIO_INSTANCE_DAT3_PORT SL_GPIO_PORT_D
#define SL_CPC_DRV_SDIO_INSTANCE_DAT3_PIN 5

// Optional: define CD GPIO for GPIO card detect; omit to use the SDIO CD pin.
// #define SL_CPC_DRV_SDIO_INSTANCE_CD_PORT SL_GPIO_PORT_G
// #define SL_CPC_DRV_SDIO_INSTANCE_CD_PIN 4
// [SL_CPC_DRV_SDIO]$
// <<< sl:end pin_tool >>>

#endif /* SL_CPC_DRV_SDIO_DEVICE_INSTANCE_CONFIG_H */
