/***************************************************************************//**
 * @file
 * @brief Configuration for sl_rail_util_scheduler (kernel worker).
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

#ifndef SL_RAIL_UTIL_SCHEDULER_CONFIG_H
#define SL_RAIL_UTIL_SCHEDULER_CONFIG_H

// <<< Use Configuration Wizard in Context Menu >>>

// <h> RAIL Utility Scheduler

#if defined(SL_CATALOG_KERNEL_PRESENT)
#include "cmsis_os2.h"

// <o SL_RAIL_UTIL_SCHEDULER_TASK_PRIORITY> Worker task priority
// <i> Only used when a kernel is present.
#ifndef SL_RAIL_UTIL_SCHEDULER_TASK_PRIORITY
#define SL_RAIL_UTIL_SCHEDULER_TASK_PRIORITY  osPriorityAboveNormal
#endif
#endif // SL_CATALOG_KERNEL_PRESENT

// <o SL_RAIL_UTIL_SCHEDULER_TASK_STACK_SIZE> Worker task stack size (words)
// <i> Only used when a kernel is present.
#ifndef SL_RAIL_UTIL_SCHEDULER_TASK_STACK_SIZE
#define SL_RAIL_UTIL_SCHEDULER_TASK_STACK_SIZE  512
#endif

// </h>

// <<< end of configuration section >>>

#endif // SL_RAIL_UTIL_SCHEDULER_CONFIG_H
