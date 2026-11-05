/***************************************************************************/ /**
 * @file sl_cpc_drv_sdio_device_instance_wake_host_config.h
 * @brief CPC SDIO device driver instance host wake configuration.
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

#ifndef SL_CPC_DRV_SDIO_DEVICE_INSTANCE_WAKE_HOST_CONFIG_H
#define SL_CPC_DRV_SDIO_DEVICE_INSTANCE_WAKE_HOST_CONFIG_H

#include "sl_cpc_wake.h"
#include "sl_device_gpio.h"

// <<< Use Configuration Wizard in Context Menu >>>

// <h> CPC-Secondary SDIO Driver Host Wake Configuration

// <o SL_CPC_DRV_SDIO_DEVICE_INSTANCE_WAKE_OUT_MODE> Wake-out ready mode
// <SL_CPC_WAKE_OUT_DISABLED=> Disabled
// <SL_CPC_WAKE_OUT_MODE_CONFIRM=> Wait for peer AWAKE then transmit
// <i> Default: Disabled
// <i> When not disabled, set WAKE_OUT PORT/PIN below.
#define SL_CPC_DRV_SDIO_DEVICE_INSTANCE_WAKE_OUT_MODE SL_CPC_WAKE_OUT_DISABLED

// <<< sl:start pin_tool >>>
// <gpio optional> SL_CPC_DRV_SDIO_DEVICE_INSTANCE_WAKE_OUT
// $[GPIO_SL_CPC_DRV_SDIO_DEVICE_INSTANCE_WAKE_OUT]
// #define SL_CPC_DRV_SDIO_DEVICE_INSTANCE_WAKE_OUT_PORT SL_GPIO_PORT_A
// #define SL_CPC_DRV_SDIO_DEVICE_INSTANCE_WAKE_OUT_PIN 0
// [GPIO_SL_CPC_DRV_SDIO_DEVICE_INSTANCE_WAKE_OUT]$
// <<< sl:end pin_tool >>>
// </h>
// <<< end of configuration section >>>

#endif /* SL_CPC_DRV_SDIO_DEVICE_INSTANCE_WAKE_HOST_CONFIG_H */
