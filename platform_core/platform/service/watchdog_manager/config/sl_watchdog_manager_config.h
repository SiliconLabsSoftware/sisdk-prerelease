/***************************************************************************//**
 * @file
 * @brief Watchdog Manager Configuration
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

#ifndef SL_WATCHDOG_MANAGER_CONFIG_H
#define SL_WATCHDOG_MANAGER_CONFIG_H

// <<< Use Configuration Wizard in Context Menu >>>

// Timeout period enum values (WDOG clock periods).
#define SL_WATCHDOG_MANAGER_TIMEOUT_PERIOD_9       0
#define SL_WATCHDOG_MANAGER_TIMEOUT_PERIOD_17      1
#define SL_WATCHDOG_MANAGER_TIMEOUT_PERIOD_33      2
#define SL_WATCHDOG_MANAGER_TIMEOUT_PERIOD_65      3
#define SL_WATCHDOG_MANAGER_TIMEOUT_PERIOD_129     4
#define SL_WATCHDOG_MANAGER_TIMEOUT_PERIOD_257     5
#define SL_WATCHDOG_MANAGER_TIMEOUT_PERIOD_513     6
#define SL_WATCHDOG_MANAGER_TIMEOUT_PERIOD_1K      7
#define SL_WATCHDOG_MANAGER_TIMEOUT_PERIOD_2K      8
#define SL_WATCHDOG_MANAGER_TIMEOUT_PERIOD_4K      9
#define SL_WATCHDOG_MANAGER_TIMEOUT_PERIOD_8K      10
#define SL_WATCHDOG_MANAGER_TIMEOUT_PERIOD_16K     11
#define SL_WATCHDOG_MANAGER_TIMEOUT_PERIOD_32K     12
#define SL_WATCHDOG_MANAGER_TIMEOUT_PERIOD_64K     13
#define SL_WATCHDOG_MANAGER_TIMEOUT_PERIOD_128K    14
#define SL_WATCHDOG_MANAGER_TIMEOUT_PERIOD_256K    15

// <h> Watchdog Manager Configuration

// Default WDOG clock source values (must match sl_watchdog_manager_clock_source_t).
#define SL_WATCHDOG_MANAGER_DEFAULT_CLOCK_SOURCE_HCLKDIV1024  0
#define SL_WATCHDOG_MANAGER_DEFAULT_CLOCK_SOURCE_LFRCO        1
#define SL_WATCHDOG_MANAGER_DEFAULT_CLOCK_SOURCE_LFXO         2
#define SL_WATCHDOG_MANAGER_DEFAULT_CLOCK_SOURCE_ULFRCO       3

// <o SL_WATCHDOG_MANAGER_DEFAULT_CLOCK_SOURCE> Default WDOG clock source
// <i> Applied on first watchdog manager HAL init when the device has a WDOG
// <i> CLKSEL register. Programs CMU WDOGxCLKCTRL rather than keeping the Clock
// <i> Manager / reset default (often HCLK/1024).
// <i> Default LFRCO so a given SL_WATCHDOG_MANAGER_TIMEOUT_PERIOD has a similar
// <i> wall-clock duration across Series 2 and Series 3, independent of SYSCLK.
// <i> Ignored on devices without WDOG CLKSEL.
// <SL_WATCHDOG_MANAGER_DEFAULT_CLOCK_SOURCE_HCLKDIV1024=> HCLK / 1024
// <SL_WATCHDOG_MANAGER_DEFAULT_CLOCK_SOURCE_LFRCO=> LFRCO
// <SL_WATCHDOG_MANAGER_DEFAULT_CLOCK_SOURCE_LFXO=> LFXO
// <SL_WATCHDOG_MANAGER_DEFAULT_CLOCK_SOURCE_ULFRCO=> ULFRCO
// <d> SL_WATCHDOG_MANAGER_DEFAULT_CLOCK_SOURCE_LFRCO
#ifndef SL_WATCHDOG_MANAGER_DEFAULT_CLOCK_SOURCE
#define SL_WATCHDOG_MANAGER_DEFAULT_CLOCK_SOURCE \
  SL_WATCHDOG_MANAGER_DEFAULT_CLOCK_SOURCE_LFRCO
#endif

// <o SL_WATCHDOG_MANAGER_TIMEOUT_PERIOD> Timeout period
// <i> Number of periods of the selected WDOG clock before timeout.
// <i> The wall-clock timeout depends on SL_WATCHDOG_MANAGER_DEFAULT_CLOCK_SOURCE
// <i> (or a runtime clock override) and that clock's frequency:
// <i> Timeout = Timeout period / WDOG clock frequency.
// <i> Times below assume the nominal 32.768 kHz LFRCO frequency.
// <i> The actual LFRCO frequency and timeout vary with device and conditions.
// <i> The hardware watchdog resets the system if all enabled software
// <i> watchdogs are not fed within this period.
// <i> Default: SL_WATCHDOG_MANAGER_TIMEOUT_PERIOD_256K (longest timeout)
// <SL_WATCHDOG_MANAGER_TIMEOUT_PERIOD_9=> 9 periods (~0.275 ms)
// <SL_WATCHDOG_MANAGER_TIMEOUT_PERIOD_17=> 17 periods (~0.519 ms)
// <SL_WATCHDOG_MANAGER_TIMEOUT_PERIOD_33=> 33 periods (~1.01 ms)
// <SL_WATCHDOG_MANAGER_TIMEOUT_PERIOD_65=> 65 periods (~1.98 ms)
// <SL_WATCHDOG_MANAGER_TIMEOUT_PERIOD_129=> 129 periods (~3.94 ms)
// <SL_WATCHDOG_MANAGER_TIMEOUT_PERIOD_257=> 257 periods (~7.84 ms)
// <SL_WATCHDOG_MANAGER_TIMEOUT_PERIOD_513=> 513 periods (~15.7 ms)
// <SL_WATCHDOG_MANAGER_TIMEOUT_PERIOD_1K=> 1025 periods (~31.3 ms)
// <SL_WATCHDOG_MANAGER_TIMEOUT_PERIOD_2K=> 2049 periods (~62.5 ms)
// <SL_WATCHDOG_MANAGER_TIMEOUT_PERIOD_4K=> 4097 periods (~125 ms)
// <SL_WATCHDOG_MANAGER_TIMEOUT_PERIOD_8K=> 8193 periods (~250 ms)
// <SL_WATCHDOG_MANAGER_TIMEOUT_PERIOD_16K=> 16385 periods (~500 ms)
// <SL_WATCHDOG_MANAGER_TIMEOUT_PERIOD_32K=> 32769 periods (~1000 ms)
// <SL_WATCHDOG_MANAGER_TIMEOUT_PERIOD_64K=> 65537 periods (~2000 ms)
// <SL_WATCHDOG_MANAGER_TIMEOUT_PERIOD_128K=> 131073 periods (~4000 ms)
// <SL_WATCHDOG_MANAGER_TIMEOUT_PERIOD_256K=> 262145 periods (~8000 ms)
#define SL_WATCHDOG_MANAGER_TIMEOUT_PERIOD \
  SL_WATCHDOG_MANAGER_TIMEOUT_PERIOD_256K

// <o SL_WATCHDOG_MANAGER_MAX_SW_WATCHDOGS> Maximum number of software watchdogs
// <i> Defines the maximum number of concurrent software watchdog instances.
// <i> Each watchdog consumes memory for the entry structure.
// <i> Reduce to save memory if fewer watchdogs are needed.
// <i> Default: 32 (supports all 32 bit positions)
// <1-32>
// <d> 32
#define SL_WATCHDOG_MANAGER_MAX_SW_WATCHDOGS 32u

// <q SL_WATCHDOG_MANAGER_DEBUG_RUN> WDOG runs during debug halt
// <i> When enabled (1), the hardware watchdog counter keeps running when the CPU
// <i> is halted in a debugger. When disabled (0, default), the counter stops
// <i> during debug halt so the device does not reset while stepping code.
// <i> Enable for testing watchdog behavior in debug; disable for normal debugging.
// <d> 0
#define SL_WATCHDOG_MANAGER_DEBUG_RUN 0

// <q SL_WATCHDOG_MANAGER_EM4_BLOCK> Block entry to EM4 while WDOG is enabled
// <i> When enabled (1), the system cannot enter EM4 while the watchdog
// <i> is running, avoiding unrecoverable sleep with an active watchdog.
// <i> When disabled (0, default), EM4 can be entered (use only if WDOG is disabled in deep sleep).
// <d> 0
#define SL_WATCHDOG_MANAGER_EM4_BLOCK 0

// <q SL_WATCHDOG_MANAGER_LOCK> Lock WDOG configuration after init
// <i> When enabled (1), software cannot change WDOG settings after init; a reset
// <i> is required to reconfigure. Use for production hardening.
// <i> When disabled (0, default), configuration remains modifiable.
// <d> 0
#define SL_WATCHDOG_MANAGER_LOCK 0

// <q SL_WATCHDOG_MANAGER_RESET_DISABLE> Disable WDOG reset output
// <i> When enabled (1), the watchdog counts but does not assert reset (useful
// <i> for debug or when using warning/interrupt only). When disabled (0, default),
// <i> timeout triggers a system reset.
// <d> 0
#define SL_WATCHDOG_MANAGER_RESET_DISABLE 0

// Warning period enum values (WDOG WARNSEL).
#define SL_WATCHDOG_MANAGER_WARNING_DISABLE  0
#define SL_WATCHDOG_MANAGER_WARNING_TIME25   1
#define SL_WATCHDOG_MANAGER_WARNING_TIME50   2
#define SL_WATCHDOG_MANAGER_WARNING_TIME75   3

// <o SL_WATCHDOG_MANAGER_WARNING_TIME> Hardware watchdog warning period
// <i> Select when the starve callback is invoked relative to the hardware
// <i> watchdog timeout. Requires a registered starve callback
// <i> (sl_watchdog_manager_set_starve_callback). Use SL_WATCHDOG_MANAGER_WARNING_DISABLE
// <i> to disable the warning interrupt. Combine with RESET_DISABLE to handle
// <i> starvation without a hard reset.
// <SL_WATCHDOG_MANAGER_WARNING_DISABLE=> Disabled
// <SL_WATCHDOG_MANAGER_WARNING_TIME25=> 25% of timeout
// <SL_WATCHDOG_MANAGER_WARNING_TIME50=> 50% of timeout
// <SL_WATCHDOG_MANAGER_WARNING_TIME75=> 75% of timeout
// <d> SL_WATCHDOG_MANAGER_WARNING_TIME75
#define SL_WATCHDOG_MANAGER_WARNING_TIME SL_WATCHDOG_MANAGER_WARNING_TIME75

// <q SL_WATCHDOG_MANAGER_EM1_RUN> WDOG runs in EM1
// <i> When enabled (1), the watchdog counter keeps running in EM1 sleep.
// <i> When disabled (0, default), the counter stops in EM1. On parts without
// <i> EM1RUN support, the manager disables WDOG on sleep and re-enables on wake.
// <d> 0
#define SL_WATCHDOG_MANAGER_EM1_RUN 0

// <q SL_WATCHDOG_MANAGER_EM2_RUN> WDOG runs in EM2
// <i> When enabled (1), the watchdog counter keeps running in EM2 sleep.
// <i> When disabled (0, default), the counter stops in EM2. On parts without
// <i> EM2RUN support, the manager disables WDOG on sleep and re-enables on wake.
// <d> 0
#define SL_WATCHDOG_MANAGER_EM2_RUN 0

// <q SL_WATCHDOG_MANAGER_EM3_RUN> WDOG runs in EM3
// <i> When enabled (1), the watchdog counter keeps running in EM3 sleep.
// <i> When disabled (0, default), the counter stops in EM3. On parts without
// <i> EM3RUN support, the manager disables WDOG on sleep and re-enables on wake.
// <d> 0
#define SL_WATCHDOG_MANAGER_EM3_RUN 0

// </h>

#endif /* SL_WATCHDOG_MANAGER_CONFIG_H */

// <<< end of configuration section >>>
