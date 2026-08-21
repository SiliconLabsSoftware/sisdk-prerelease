/***************************************************************************//**
 * @file
 * @brief Event System Configuration
 *******************************************************************************
 * # License
 * <b>Copyright 2025 Silicon Laboratories Inc. www.silabs.com</b>
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

#ifndef SL_EVENT_CONFIG_H
#define SL_EVENT_CONFIG_H

// <<< Use Configuration Wizard in Context Menu >>>

// <h> Event System Configuration

// <o SL_EVENT_SYSTEM_LOG_LEVEL_COMPILE_TIME> Event System Log Level
// <SL_LOG_CONFIG_LEVEL_NONE  => NONE  (all Event System logs compiled out)
// <SL_LOG_CONFIG_LEVEL_ERROR => ERROR
// <SL_LOG_CONFIG_LEVEL_WARN  => WARN
// <SL_LOG_CONFIG_LEVEL_INFO  => INFO
// <SL_LOG_CONFIG_LEVEL_DEBUG => DEBUG (most verbose)
// <i> Requires the Debug Logger. The global compile-time level may restrict
// <i> this setting further; the global runtime level also filters emission.
// <i> The runtime level starts here and can be narrowed through
// <i> sl_event_system_log_level from a debugger.
// <i> Default: SL_LOG_CONFIG_LEVEL_NONE
#ifndef SL_EVENT_SYSTEM_LOG_LEVEL_COMPILE_TIME
#define SL_EVENT_SYSTEM_LOG_LEVEL_COMPILE_TIME  SL_LOG_CONFIG_LEVEL_NONE
#endif

// <q SL_EVENT_SUPERVISOR_QUEUE_COUNT> Supervisor mode queue maximum events count.
// <i> Default: 255
#define SL_EVENT_SUPERVISOR_QUEUE_COUNT    255
// </h>

// <<< end of configuration section >>>
#endif // SL_EVENT_CONFIG_H
