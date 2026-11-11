/***************************************************************************/ /**
 * @file sl_cpc_ng_drv_sdio_host_instance_pm_runtime_config.h
 * @brief CPC SDIO host driver instance runtime power management configuration.
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

#ifndef SL_CPC_NG_DRV_SDIO_HOST_INSTANCE_PM_RUNTIME_CONFIG_H
#define SL_CPC_NG_DRV_SDIO_HOST_INSTANCE_PM_RUNTIME_CONFIG_H

#include "sl_cpc_wake.h"
#include "sl_device_gpio.h"

// <<< Use Configuration Wizard in Context Menu >>>

// <h> CPC-Primary SDIO Driver Runtime PM Configuration

// <o SL_CPC_DRV_SDIO_HOST_INSTANCE_POWER_MODE> Local power mode
// <SL_CPC_POWER_MODE_ALWAYS_ON=> Always on
// <SL_CPC_POWER_MODE_WAKE_FROM_GPIO=> Wake from GPIO
// <i> Default: Always on
// <i> When waking from GPIO, set WAKE_IN PORT/PIN below.
#define SL_CPC_DRV_SDIO_HOST_INSTANCE_POWER_MODE SL_CPC_POWER_MODE_ALWAYS_ON

// <o SL_CPC_DRV_SDIO_HOST_INSTANCE_WAKE_IN_RESPONSE> Wake-in response
// <SL_CPC_WAKE_IN_RESPONSE_NONE=> None
// <SL_CPC_WAKE_IN_RESPONSE_AWAKE=> Send AWAKE
// <i> Default: None
// <i> Pair with peer wake-out confirm when sending AWAKE.
#define SL_CPC_DRV_SDIO_HOST_INSTANCE_WAKE_IN_RESPONSE SL_CPC_WAKE_IN_RESPONSE_NONE

// <<< sl:start pin_tool >>>
// <gpio optional> SL_CPC_DRV_SDIO_HOST_INSTANCE_WAKE_IN
// $[GPIO_SL_CPC_DRV_SDIO_HOST_INSTANCE_WAKE_IN]
// #define SL_CPC_DRV_SDIO_HOST_INSTANCE_WAKE_IN_PORT SL_GPIO_PORT_A
// #define SL_CPC_DRV_SDIO_HOST_INSTANCE_WAKE_IN_PIN 0
// [GPIO_SL_CPC_DRV_SDIO_HOST_INSTANCE_WAKE_IN]$
// <<< sl:end pin_tool >>>
// </h>
// <<< end of configuration section >>>

#endif /* SL_CPC_NG_DRV_SDIO_HOST_INSTANCE_PM_RUNTIME_CONFIG_H */
