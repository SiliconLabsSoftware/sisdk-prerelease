/***************************************************************************//**
 * @file
 * @brief Interrupt Manager configuration.
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

#ifndef SL_INTERRUPT_MANAGER_CONFIG_H
#define SL_INTERRUPT_MANAGER_CONFIG_H

// <h> Interrupt Manager Configuration

// <o SL_INTERRUPT_MANAGER_LOG_LEVEL_COMPILE_TIME> Interrupt Manager Log Level
// <SL_LOG_CONFIG_LEVEL_NONE  => NONE  (all Interrupt Manager logs compiled out)
// <SL_LOG_CONFIG_LEVEL_ERROR => ERROR
// <SL_LOG_CONFIG_LEVEL_WARN  => WARN
// <SL_LOG_CONFIG_LEVEL_INFO  => INFO
// <SL_LOG_CONFIG_LEVEL_DEBUG => DEBUG (most verbose)
// <i> Requires the Debug Logger. The global compile-time level may restrict
// <i> this setting further; the global runtime level also filters emission.
// <i> The runtime level starts here and can be narrowed through
// <i> sl_interrupt_manager_log_level from a debugger.
// <i> Default: SL_LOG_CONFIG_LEVEL_NONE
#ifndef SL_INTERRUPT_MANAGER_LOG_LEVEL_COMPILE_TIME
#define SL_INTERRUPT_MANAGER_LOG_LEVEL_COMPILE_TIME  SL_LOG_CONFIG_LEVEL_NONE
#endif

// </h>

// <<< end of configuration section >>>

#endif // SL_INTERRUPT_MANAGER_CONFIG_H
