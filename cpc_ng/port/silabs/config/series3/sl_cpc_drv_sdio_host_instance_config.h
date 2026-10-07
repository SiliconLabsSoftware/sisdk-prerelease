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

// <h> SDHC host configuration

// <o SL_CPC_DRV_SDIO_HOST_INSTANCE_BASE_CLOCK_FREQ_HZ> Base clock frequency (Hz)
// <i> Must match SDHC0CLK. SDCLK is this clock divided by a power of two.
#define SL_CPC_DRV_SDIO_HOST_INSTANCE_BASE_CLOCK_FREQ_HZ 100000000UL

// <o SL_CPC_DRV_SDIO_HOST_INSTANCE_BUS_VOLTAGE> SD bus voltage
// <SL_SDHC_BUS_VOLTAGE_3V3=> 3.3 V
// <SL_SDHC_BUS_VOLTAGE_3V0=> 3.0 V
// <SL_SDHC_BUS_VOLTAGE_1V8=> 1.8 V
// <SL_SDHC_BUS_VOLTAGE_1V2=> 1.2 V
// <i> Must match the voltage physically supplied by the board.
#define SL_CPC_DRV_SDIO_HOST_INSTANCE_BUS_VOLTAGE SL_SDHC_BUS_VOLTAGE_3V3

// <o SL_CPC_DRV_SDIO_HOST_INSTANCE_SLOT_TYPE> Slot type
// <SL_SDHC_SLOT_TYPE_REMOVABLE=> Removable
// <SL_SDHC_SLOT_TYPE_EMBEDDED=> Embedded
// <i> Default: Embedded (fixed CPC SDIO link)
#define SL_CPC_DRV_SDIO_HOST_INSTANCE_SLOT_TYPE SL_SDHC_SLOT_TYPE_EMBEDDED

// <o SL_CPC_DRV_SDIO_HOST_INSTANCE_CARD_DETECT_SOURCE> Card detect source
// <SL_SDHC_CD_SOURCE_SDCD_PIN=> SDCD# pin (hardware, with IP debounce)
// <SL_SDHC_CD_SOURCE_EXTERNAL_GPIO=> External GPIO (driver updates CDTL)
#define SL_CPC_DRV_SDIO_HOST_INSTANCE_CARD_DETECT_SOURCE SL_SDHC_CD_SOURCE_SDCD_PIN

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

// <<< sl:start pin_tool >>>
// <sdhc signal=CLK,CMD,DAT0,DAT1,DAT2,DAT3> SL_CPC_DRV_SDIO_HOST_INSTANCE
// <i> CLK, CMD, and DAT0-DAT3 are required.
// $[SDHC_SL_CPC_DRV_SDIO_HOST_INSTANCE]
#warning "CPC SDIO host peripheral and pins not configured"
// #define SL_CPC_DRV_SDIO_HOST_INSTANCE_PERIPHERAL            SDHCCORE
// #define SL_CPC_DRV_SDIO_HOST_INSTANCE_PERIPHERAL_NO         0

// #define SL_CPC_DRV_SDIO_HOST_INSTANCE_CLK_PORT              SL_GPIO_PORT_E
// #define SL_CPC_DRV_SDIO_HOST_INSTANCE_CLK_PIN               0

// #define SL_CPC_DRV_SDIO_HOST_INSTANCE_CMD_PORT              SL_GPIO_PORT_E
// #define SL_CPC_DRV_SDIO_HOST_INSTANCE_CMD_PIN               1

// #define SL_CPC_DRV_SDIO_HOST_INSTANCE_DAT0_PORT             SL_GPIO_PORT_E
// #define SL_CPC_DRV_SDIO_HOST_INSTANCE_DAT0_PIN              2

// #define SL_CPC_DRV_SDIO_HOST_INSTANCE_DAT1_PORT             SL_GPIO_PORT_E
// #define SL_CPC_DRV_SDIO_HOST_INSTANCE_DAT1_PIN              3

// #define SL_CPC_DRV_SDIO_HOST_INSTANCE_DAT2_PORT             SL_GPIO_PORT_E
// #define SL_CPC_DRV_SDIO_HOST_INSTANCE_DAT2_PIN              4

// #define SL_CPC_DRV_SDIO_HOST_INSTANCE_DAT3_PORT             SL_GPIO_PORT_E
// #define SL_CPC_DRV_SDIO_HOST_INSTANCE_DAT3_PIN              5

// [SDHC_SL_CPC_DRV_SDIO_HOST_INSTANCE]$
// <<< sl:end pin_tool >>>

#endif /* SL_CPC_DRV_SDIO_HOST_INSTANCE_CONFIG_H */
