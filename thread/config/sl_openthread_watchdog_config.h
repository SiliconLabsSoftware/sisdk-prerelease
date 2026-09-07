/*******************************************************************************
 * @file
 * @brief OpenThread watchdog configuration file.
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

#ifndef _SL_OPENTHREAD_WATCHDOG_CONFIG
#define _SL_OPENTHREAD_WATCHDOG_CONFIG

// <<< Use Configuration Wizard in Context Menu >>>

// <h> OpenThread Watchdog configuration

// <q SL_OPENTHREAD_WATCHDOG_CRASH_CATCH_ENABLE> Enable watchdog warn crash catch
// <i> Registers sl_watchdog_manager_set_starve_callback() so platform WM HAL
// <i> (WDOG WARN IRQ) invokes ot_crash_handler for RESET_WATCHDOG_CAUGHT (LWM).
// <i> Set SL_WATCHDOG_MANAGER_WARNING_TIME in sl_watchdog_manager_config.h.
// <d> 1
#define SL_OPENTHREAD_WATCHDOG_CRASH_CATCH_ENABLE 1

// </h>

#endif /* _SL_OPENTHREAD_WATCHDOG_CONFIG */

// <<< end of configuration section >>>
