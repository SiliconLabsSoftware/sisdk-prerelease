/***************************************************************************//**
 * @file
 * @brief Power Manager EM4 configuration file.
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

// <<< Use Configuration Wizard in Context Menu >>>

#ifndef SL_POWER_MANAGER_EM4_CONFIG_H
#define SL_POWER_MANAGER_EM4_CONFIG_H

// <h>Power Manager EM4 Configuration

// <o SL_POWER_MANAGER_INIT_EMU_EM4_PIN_RETENTION_MODE> Pin retention mode
// <i>
// <EMU_EM4CTRL_EM4IORETMODE_DISABLE=> No retention
// <EMU_EM4CTRL_EM4IORETMODE_EM4EXIT=> Retention through EM4
// <EMU_EM4CTRL_EM4IORETMODE_SWUNLATCH=> Retention through EM4 and wakeup
// <d> power_manager_pin_retention_disable
#ifndef SL_POWER_MANAGER_INIT_EMU_EM4_PIN_RETENTION_MODE
#define SL_POWER_MANAGER_INIT_EMU_EM4_PIN_RETENTION_MODE EMU_EM4CTRL_EM4IORETMODE_DISABLE
#endif

// </h>

#endif /* SL_POWER_MANAGER_EM4_CONFIG_H */
