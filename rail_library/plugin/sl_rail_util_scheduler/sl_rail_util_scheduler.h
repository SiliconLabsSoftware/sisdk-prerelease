/***************************************************************************//**
 * @file
 * @brief RAIL utility to drain HWSCHED deferred work from process_action / RTOS.
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

#ifndef SL_RAIL_UTIL_SCHEDULER_H
#define SL_RAIL_UTIL_SCHEDULER_H

#include <stdbool.h>

#ifdef SL_COMPONENT_CATALOG_PRESENT
#include "sl_component_catalog.h"
#endif

#if defined(SL_CATALOG_POWER_MANAGER_PRESENT)
#include "sl_power_manager.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @addtogroup Scheduler_Utility Scheduler Utility
 * @ingroup Multiprotocol
 * @brief Drain hardware-scheduler deferred work for multiprotocol apps.
 * @{
 */

/**
 * Initialize the scheduler utility.
 *
 * On kernel builds, starts a wakeable worker that drains HWSCHED work.
 * On baremetal builds this is a no-op; draining runs from
 * service_process_action.
 */
void sl_rail_util_scheduler_init(void);

/**
 * Drain deferred HWSCHED mailbox / resume work (and optional hooks).
 *
 * Registered as an SLC service_process_action handler on baremetal.
 * Also called from the RTOS worker when a kernel is present.
 */
void sl_rail_util_scheduler_process_action(void);

/**
 * Return true when deferred HWSCHED work is waiting to be drained.
 */
bool sl_rail_util_scheduler_has_pending_work(void);

/**
 * Register a notifier invoked when HWSCHED work is queued (may be from IRQ).
 *
 * @param[in] notify_fn  Callback; may be NULL to clear
 */
void sl_rail_util_scheduler_set_pending_work_notifier(void (*notify_fn)(void));

#if defined(SL_CATALOG_POWER_MANAGER_PRESENT) \
  && !defined(SL_CATALOG_KERNEL_PRESENT)
/**
 * Power Manager is_ok_to_sleep hook: block sleep while work is pending.
 */
bool sl_rail_util_scheduler_is_ok_to_sleep(void);

/**
 * Power Manager sleep_on_isr_exit hook: wake when work was queued in an ISR.
 */
sl_power_manager_on_isr_exit_t sl_rail_util_scheduler_sleep_on_isr_exit(void);
#endif

/** @} */

#ifdef __cplusplus
}
#endif

#endif // SL_RAIL_UTIL_SCHEDULER_H
