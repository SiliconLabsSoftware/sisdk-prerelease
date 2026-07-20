/***************************************************************************//**
 * @file
 * @brief Watchdog Manager HAL clock configuration dependencies
 *
 * @details
 * Bridges the watchdog HAL to Clock Manager oscillator project settings without
 * requiring sl_clock_manager_oscillator_config.h on every include path.
 *
 * When the project config header is available (Simplicity Studio builds), it is
 * included and SL_CLOCK_MANAGER_LFXO_EN reflects the Wizard LFXO enable setting.
 * When it is absent (for example platform static analysis), LFXO defaults to
 * disabled so the HAL and CLI reject LFXO at compile time.
 *
 * Use SLI_WATCHDOG_MANAGER_HAL_LFXO_CLOCK_AVAILABLE to gate LFXO in HAL encode
 * and CLI parsing.
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

#ifndef SLI_WATCHDOG_MANAGER_HAL_CLOCK_CONFIG_H
#define SLI_WATCHDOG_MANAGER_HAL_CLOCK_CONFIG_H

// Use Clock Manager oscillator settings when the project config header is on the
// include path. Builds without project config (e.g. platform static analysis)
// default LFXO to disabled so LFXO is rejected at compile time.
#if defined(__has_include)
  #if __has_include("sl_clock_manager_oscillator_config.h")
    #include "sl_clock_manager_oscillator_config.h"
  #endif
#endif

#ifndef SL_CLOCK_MANAGER_LFXO_EN
  #define SL_CLOCK_MANAGER_LFXO_EN  0
#endif

/// True when the project enables LFXO and watchdog HAL may select it.
#define SLI_WATCHDOG_MANAGER_HAL_LFXO_CLOCK_AVAILABLE  \
  (SL_CLOCK_MANAGER_LFXO_EN != 0)

#endif /* SLI_WATCHDOG_MANAGER_HAL_CLOCK_CONFIG_H */
