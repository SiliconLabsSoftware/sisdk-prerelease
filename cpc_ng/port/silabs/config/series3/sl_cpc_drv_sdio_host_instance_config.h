/***************************************************************************/ /**
 * @file
 * @brief CPC SDIO PRIMARY driver configuration file.
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

#ifndef SL_CPC_DRV_SDIO_HOST_INSTANCE_CONFIG_H
#define SL_CPC_DRV_SDIO_HOST_INSTANCE_CONFIG_H
#include "sl_sdhc_host_types.h"

// <<< Use Configuration Wizard in Context Menu >>>

// <h> CPC-Primary SDIO Driver Configuration

// <h> Frame pool configuration

// <o SL_CPC_DRV_SDIO_HOST_INSTANCE_RX_FRAME_POOL_COUNT> RX frame pool size
// <i> Default: 20
// <i> Number of CPC RX frames that can be allocated for this SDIO instance
#define SL_CPC_DRV_SDIO_HOST_INSTANCE_RX_FRAME_POOL_COUNT 20

// <o SL_CPC_DRV_SDIO_HOST_INSTANCE_TX_FRAME_POOL_COUNT> TX frame pool size
// <i> Default: 20
// <i> Number of CPC TX frames that can be allocated for this SDIO instance
#define SL_CPC_DRV_SDIO_HOST_INSTANCE_TX_FRAME_POOL_COUNT 20
// </h>

// <h> SDIO Host Configuration

// <o SL_CPC_DRV_SDIO_INSTANCE_FUNCTION_NUM> SDIO Function number used for CPC.
// <i> The SDIO function number must match with the function number used on the card. When in doubt, leave default value.
// <i> Default : 1
// <d> 1
#define SL_CPC_DRV_SDIO_INSTANCE_FUNCTION_NUM 1

// <o SL_CPC_DRV_SDIO_INSTANCE_BLOCK_SIZE> Block size used over the SDIO bus.
// <512=> 512 Bytes block size
// <i> Larger block size may increase throughput when transferring large buffers over CPC.
// <i> Must match the block size configured on the secondary.
// <i> Default : 512
// <d> 512
#define SL_CPC_DRV_SDIO_INSTANCE_BLOCK_SIZE 512

// <o SL_CPC_DRV_SDIO_INSTANCE_MAX_SD_FREQ> Maximum SD bus clock (negotiation limit)
// <SL_SDHC_CLOCK_SDMMC_400KHZ=> 400 kHz
// <SL_SDHC_CLOCK_SD_25MHZ=> 25 MHz
// <SL_SDHC_CLOCK_SD_50MHZ=> 50 MHz
// <SL_SDHC_CLOCK_SD_100MHZ=> 100 MHz
// <SL_SDHC_CLOCK_SD_208MHZ=> 208 MHz
// <i> Upper bound passed to sl_sdhc_sdio_init(); actual rate is the best supported by host, card, and this limit.
// <i> Default : 50 MHz
#define SL_CPC_DRV_SDIO_INSTANCE_MAX_SD_FREQ SL_SDHC_CLOCK_SD_50MHZ

// <o SL_CPC_DRV_SDIO_INSTANCE_MAX_BUS_WIDTH> Maximum bus width (negotiation limit)
// <SL_SDHC_BUS_WIDTH_1BIT_MODE=> 1-bit
// <SL_SDHC_BUS_WIDTH_4BIT_MODE=> 4-bit
// <SL_SDHC_BUS_WIDTH_8BIT_MODE=> 8-bit
// <i> Upper bound passed to sl_sdhc_sdio_init().
// <i> Default : 4-bit
#define SL_CPC_DRV_SDIO_INSTANCE_MAX_BUS_WIDTH SL_SDHC_BUS_WIDTH_4BIT_MODE

// <o SL_CPC_DRV_SDIO_INSTANCE_MAX_SPEED_MODE> Maximum speed mode (negotiation limit)
// <SL_SDHC_SPEED_MODE_DEFAULT_SPEED=> Default Speed
// <SL_SDHC_SPEED_MODE_HIGH_SPEED=> High Speed
// <SL_SDHC_SPEED_MODE_SDR12=> SDR12
// <SL_SDHC_SPEED_MODE_SDR25=> SDR25
// <SL_SDHC_SPEED_MODE_SDR50=> SDR50
// <SL_SDHC_SPEED_MODE_DDR50=> DDR50
// <SL_SDHC_SPEED_MODE_SDR104=> SDR104
// <i> Upper bound passed to sl_sdhc_sdio_init().
// <i> Default : High Speed
#define SL_CPC_DRV_SDIO_INSTANCE_MAX_SPEED_MODE SL_SDHC_SPEED_MODE_HIGH_SPEED

// </h>
// </h>
// <<< end of configuration section >>>

#endif /* SL_CPC_DRV_SDIO_HOST_INSTANCE_CONFIG_H */
