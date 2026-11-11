/***************************************************************************/ /**
 * @file sli_crash_capture.h
 * @brief Crash Capture internal API (SL_DIAG CPU Crash provider)
 * @version 2.0.0
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

#ifndef SLI_CRASH_CAPTURE_H
#define SLI_CRASH_CAPTURE_H

#include <stdint.h>

#include "sl_enum.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @addtogroup crash_capture Crash Capture
 * @brief Fault / assert / watchdog entry that produces SL_DIAG retained records
 *
 * Crash Capture captures Tier-0 CPU state and seals it with sli_diag_fault_seal().
 * SL_DIAG writes the Mini Emergency Tail before the retained record. Retained
 * storage, read, and clear are owned by SL_DIAG (`.sl_diag_retained.noinit`).
 * Stack snapshot size is configured in `sl_diag_config.h`
 * (`SL_DIAG_STACK_SNAPSHOT_BYTES`).
 *
 * @{
 */

/**
 * @defgroup sli_crash_capture_types Types
 * @{
 */

/**
 * @brief Crash category used for fault-vector dispatch and reason mapping.
 *
 * @note The IAR fault handlers load these values as immediates; keep the
 *       HARDFAULT..USAGEFAULT values stable.
 */
SL_ENUM(sli_crash_type_t) {
  SLI_CRASH_TYPE_NONE,            /**< Unused */
  SLI_CRASH_TYPE_HARDFAULT,       /**< HardFault */
  SLI_CRASH_TYPE_MEMFAULT,        /**< MemManage */
  SLI_CRASH_TYPE_BUSFAULT,        /**< BusFault */
  SLI_CRASH_TYPE_USAGEFAULT,      /**< UsageFault */
  SLI_CRASH_TYPE_WATCHDOG,        /**< WDOG warning */
  SLI_CRASH_TYPE_ASSERT,          /**< Assert */
  SLI_CRASH_TYPE_STACKOVERFLOW,   /**< Refined Mem/Bus fault */
  SLI_CRASH_TYPE_MPUFAULT,        /**< Refined MemManage */
  SLI_CRASH_TYPE_MAX              /**< Exclusive upper bound */
};

/** @} (end addtogroup sli_crash_capture_types) */

/**
 * @defgroup sli_crash_capture_api API Functions
 * @{
 */

/**
 * @brief Initialize Crash Capture and register the CPU Crash Provider.
 */
void sli_crash_capture_init(void);

/**
 * @brief Capture and seal a watchdog-warning incident.
 *
 * @param[in] unfed_mask  sl_hal_wdog_get_pending_interrupts() value
 *                        (e.g. WDOG_IF_WARN). Stored as the record detail.
 *
 * @note Called from the WDOG warning interrupt. Returns so the caller can
 *       clear the interrupt; the watchdog reset follows.
 */
void sli_crash_capture_on_watchdog_warning(uint32_t unfed_mask);

/** @} (end addtogroup sli_crash_capture_api) */

/** @} (end addtogroup crash_capture) */

#ifdef __cplusplus
}
#endif

#endif /* SLI_CRASH_CAPTURE_H */
