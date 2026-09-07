/***************************************************************************//**
 * @file
 * @brief HFXO Manager configuration file.
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

// <<< Use Configuration Wizard in Context Menu >>>

#ifndef SL_HFXO_MANAGER_CONFIG_H
#define SL_HFXO_MANAGER_CONFIG_H

// <h>HFXO Manager Configuration

// <q SL_HFXO_MANAGER_CUSTOM_HFXO_IRQ_HANDLER> Enable custom IRQ handler for crystal HF oscillator.
// <i> Enable if HFXO0_IRQHandler is needed from your application.
// <i> The HFXO IRQ priority must not be changed as the HFXO Manager module needs it to be high priority
// <i> and to stay enabled through atomic sections.
// <i> The function sl_hfxo_manager_irq_handler() will have to be called from you custom handler if this is enabled.
// <i> Default: 0
#define SL_HFXO_MANAGER_CUSTOM_HFXO_IRQ_HANDLER  0

// <q SL_HFXO_MANAGER_SLEEPY_CRYSTAL_SUPPORT> Enable support for Sleepy Crystals.
// <i> If Enabled and if HFXO fails to startup due to a sleepy crystal, HFXO Manager will retry the startup with more aggressive settings
// <i> before falling back to the configured settings.
// <i> Default: 0
#define SL_HFXO_MANAGER_SLEEPY_CRYSTAL_SUPPORT  0

// </h>

// <o SL_HFXO_MANAGER_LOG_LEVEL_COMPILE_TIME> HFXO Manager Log Level
// <SL_LOG_CONFIG_LEVEL_NONE  => NONE  (all HFXO Manager logs compiled out)
// <SL_LOG_CONFIG_LEVEL_ERROR => ERROR
// <SL_LOG_CONFIG_LEVEL_WARN  => WARN
// <SL_LOG_CONFIG_LEVEL_INFO  => INFO
// <SL_LOG_CONFIG_LEVEL_DEBUG => DEBUG (most verbose)
// <i> Requires the Debug Logger. The global compile-time level may restrict
// <i> this setting further; the global runtime level also filters emission.
// <i> Default: SL_LOG_CONFIG_LEVEL_NONE
#ifndef SL_HFXO_MANAGER_LOG_LEVEL_COMPILE_TIME
#define SL_HFXO_MANAGER_LOG_LEVEL_COMPILE_TIME  SL_LOG_CONFIG_LEVEL_NONE
#endif

#endif /* SL_HFXO_MANAGER_CONFIG_H */

// <<< end of configuration section >>>
