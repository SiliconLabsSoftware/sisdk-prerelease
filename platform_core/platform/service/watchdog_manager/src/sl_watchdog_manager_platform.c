/***************************************************************************//**
 * @file
 * @brief Watchdog Manager default platform integration (Baremetal)
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

#include "sl_watchdog_manager.h"
#include "sli_watchdog_manager.h"
#include "sl_status.h"

/***************************************************************************//**
 * @addtogroup watchdog_manager
 * @{
 ******************************************************************************/

/*******************************************************************************
 *****************************   LOCAL DATA   **********************************
 ******************************************************************************/

/// Platform default watchdog handle.
static sl_watchdog_handle_t platform_watchdog_handle = 0;

/*******************************************************************************
 **************************   GLOBAL FUNCTIONS   *******************************
 ******************************************************************************/

/***************************************************************************//**
 * Initialize platform default watchdog.
 *
 * @note This function is called automatically during system initialization.
 ******************************************************************************/
void sli_watchdog_manager_platform_init(void)
{
  sl_status_t status;

  // Create platform default watchdog with reserved UID.
  status = sl_watchdog_manager_create(&platform_watchdog_handle,
                                      SL_WATCHDOG_MANAGER_PLATFORM_DEFAULT_UID);
  (void)status; // Suppress unused variable warning in release builds.
}

/***************************************************************************//**
 * Feed platform default watchdog.
 *
 * @note This function should be called from the main loop in baremetal
 *       applications, or from the idle task in RTOS applications.
 ******************************************************************************/
void sli_watchdog_manager_platform_feed(void)
{
  sl_watchdog_manager_feed(&platform_watchdog_handle);
}

/** @} (end addtogroup watchdog_manager) */
