/***************************************************************************/ /**
 * @file
 * @brief CPC configuration file.
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

#ifndef SL_CPC_CONFIG_H
#define SL_CPC_CONFIG_H

// <h>CPC Configuration

// <o SL_CPC_LOG_LEVEL> CPC log level
// <SL_LOG_CONFIG_LEVEL_NONE  => NONE
// <SL_LOG_CONFIG_LEVEL_ERROR => ERROR
// <SL_LOG_CONFIG_LEVEL_WARN  => WARN
// <SL_LOG_CONFIG_LEVEL_INFO  => INFO
// <SL_LOG_CONFIG_LEVEL_DEBUG => DEBUG
// <i> Default: SL_LOG_CONFIG_LEVEL_NONE
// <i> Minimum log level for CPC messages. NONE disables all logging.
// <i> When the logger component is present, filtering is done by sl_log.
// <i> Otherwise, this threshold is applied at runtime using printf.
#define SL_CPC_LOG_LEVEL SL_LOG_CONFIG_LEVEL_NONE

// <q SL_CPC_DEBUG_SYSTEM_VIEW_LOG_CORE_EVENT> Enable debug core tracing with system view
// <i> Default: 0
#define SL_CPC_DEBUG_SYSTEM_VIEW_LOG_CORE_EVENT 0

// <q SL_CPC_DEBUG_SYSTEM_VIEW_LOG_EP_EVENT> Enable debug endpoint tracing with system view
// <i> Default: 0
#define SL_CPC_DEBUG_SYSTEM_VIEW_LOG_EP_EVENT 0

// <q SL_CPC_DEBUG_CORE_EVENT_COUNTERS> Enable debug counters for core events
// <i> Default: 0
#define SL_CPC_DEBUG_CORE_EVENT_COUNTERS 0

// <q SL_CPC_DEBUG_EP_EVENT_COUNTERS> Enable debug counters for endpoint events
// <i> Default: 0
#define SL_CPC_DEBUG_EP_EVENT_COUNTERS 0

// </h>

// <<< end of configuration section >>>

#endif /* SL_CPC_CONFIG_H */
