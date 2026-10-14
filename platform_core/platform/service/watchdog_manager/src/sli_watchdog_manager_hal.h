/***************************************************************************//**
 * @file
 * @brief Watchdog Manager HAL Interface
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

#ifndef SLI_WATCHDOG_MANAGER_HAL_H
#define SLI_WATCHDOG_MANAGER_HAL_H

#include "sl_status.h"
#include <stdint.h>
#include <stdbool.h>
#include "sl_component_catalog.h"
#include "em_device.h"

/***************************************************************************//**
 * Whether the hardware watchdog supports EMxRUN (counter runs in EM1/2/3).
 * EM1RUN: when 1, power manager disable/enable on EM transition is not needed
 * for EM1-only sleep (see SLI_WATCHDOG_MANAGER_HAS_EM1RUN uses).
 * EM2RUN / EM3RUN: reflect WDOG EM2/EM3 run capability for init and config.
 * Based on device WDOG capability (_WDOG_CFG_EMxRUN_MASK).
 ******************************************************************************/
#if defined(_WDOG_CFG_EM1RUN_MASK)
#define SLI_WATCHDOG_MANAGER_HAS_EM1RUN  1
#else
#define SLI_WATCHDOG_MANAGER_HAS_EM1RUN  0
#endif

#if defined(_WDOG_CFG_EM2RUN_MASK)
#define SLI_WATCHDOG_MANAGER_HAS_EM2RUN  1
#else
#define SLI_WATCHDOG_MANAGER_HAS_EM2RUN  0
#endif

#if defined(_WDOG_CFG_EM3RUN_MASK)
#define SLI_WATCHDOG_MANAGER_HAS_EM3RUN  1
#else
#define SLI_WATCHDOG_MANAGER_HAS_EM3RUN  0
#endif

/** 1 if power manager should call sli_watchdog_manager_hal_on_em_transition: any EM1/2/3
 *  run capability missing and power manager present. */
#if (!SLI_WATCHDOG_MANAGER_HAS_EM1RUN \
     || !SLI_WATCHDOG_MANAGER_HAS_EM2RUN \
     || !SLI_WATCHDOG_MANAGER_HAS_EM3RUN) \
    && defined(SL_CATALOG_POWER_MANAGER_PRESENT)
#define SLI_WATCHDOG_MANAGER_USE_EM_TRANSITION_HOOK  1
#else
#define SLI_WATCHDOG_MANAGER_USE_EM_TRANSITION_HOOK  0
#endif

#ifdef __cplusplus
extern "C" {
#endif

/***************************************************************************//**
 * @addtogroup watchdog_manager_hal Watchdog Manager HAL
 * @{
 *
 * @brief Hardware Abstraction Layer for Watchdog Manager
 *
 * @details
 * This interface defines the HAL functions that must be implemented to support
 * the Watchdog Manager on different hardware platforms. The HAL provides
 * abstraction for:
 * - Hardware watchdog initialization and configuration
 * - Starting and stopping the watchdog
 * - Feeding the watchdog
 * - Power mode handling
 *
 * Different implementations can be provided for:
 * - Devices with WDOG peripheral (Series 2/3)
 * - Devices missing EM1/2/3 run in any sleep mode (EM transition hook)
 * - Stub implementation for customers who want to manage their own watchdog
 *
 ******************************************************************************/
/// WDOG CMU clock source selection (maps to CMU WDOGxCLKCTRL.CLKSEL).
typedef enum {
  SLI_WATCHDOG_MANAGER_HAL_CLK_HCLKDIV1024 = 0, ///< HCLK divided by 1024
  SLI_WATCHDOG_MANAGER_HAL_CLK_LFRCO,           ///< Low-frequency RC oscillator
  SLI_WATCHDOG_MANAGER_HAL_CLK_LFXO,            ///< Low-frequency crystal oscillator
  SLI_WATCHDOG_MANAGER_HAL_CLK_ULFRCO,          ///< Ultra-low-frequency RC oscillator
  SLI_WATCHDOG_MANAGER_HAL_CLK_INVALID,         ///< Sentinel: not a programmable clock source
} sli_watchdog_manager_hal_clock_source_t;

/***************************************************************************//**
 * @brief Initialize the hardware watchdog.
 *
 * @details
 * This function initializes the hardware watchdog peripheral and configures
 * it according to the settings in sl_watchdog_manager_config.h. The watchdog
 * is not started by this function.
 *
 * Configuration includes:
 * - Timeout period (WDOG.CFG.PERSEL)
 * - CMU WDOG clock source on re-init (first init loads Clock Manager selection)
 * - EM1RUN setting (if available)
 * - Disabling interrupts (if available)
 *
 * @param[in] timeout_period Timeout period configuration value (0-15)
 *                           corresponding to the period settings.
 *
 * @return SL_STATUS_OK if successful.
 * @return Error code on failure.
 ******************************************************************************/
sl_status_t sli_watchdog_manager_hal_init(uint8_t timeout_period);

/***************************************************************************//**
 * @brief Get the hardware watchdog timeout period index.
 *
 * @param[out] timeout_period Current PERSEL index (0-15).
 *
 * @return SL_STATUS_OK on success.
 * @return SL_STATUS_NULL_POINTER if @p timeout_period is NULL.
 * @return SL_STATUS_NOT_INITIALIZED if the HAL has not been initialized.
 ******************************************************************************/
sl_status_t sli_watchdog_manager_hal_get_timeout_period(uint8_t *timeout_period);

/***************************************************************************//**
 * @brief Set the hardware watchdog timeout period index.
 *
 * @details Re-initializes the WDOG with the new PERSEL and the current clock
 *          source. The hardware watchdog must be disabled before calling this
 *          function.
 *
 * @param[in] timeout_period PERSEL index to apply (0-15).
 *
 * @return SL_STATUS_OK on success.
 * @return SL_STATUS_INVALID_PARAMETER if @p timeout_period is greater than 15.
 * @return SL_STATUS_NOT_INITIALIZED if the HAL has not been initialized.
 ******************************************************************************/
sl_status_t sli_watchdog_manager_hal_set_timeout_period(uint8_t timeout_period);

/***************************************************************************//**
 * @brief Get the hardware watchdog CMU clock source.
 *
 * @param[out] clock_source Current clock source selection.
 *
 * @return SL_STATUS_OK on success.
 * @return SL_STATUS_NULL_POINTER if @p clock_source is NULL.
 * @return SL_STATUS_NOT_INITIALIZED if the HAL has not been initialized.
 * @return SL_STATUS_NOT_SUPPORTED if the device has no WDOG CLKSEL register.
 ******************************************************************************/
sl_status_t sli_watchdog_manager_hal_get_clock_source(
  sli_watchdog_manager_hal_clock_source_t *clock_source);

/***************************************************************************//**
 * @brief Set the hardware watchdog CMU clock source.
 *
 * @details Re-initializes the WDOG with the new clock and the current PERSEL.
 *          The hardware watchdog must be disabled before calling this function.
 *
 * @param[in] clock_source Clock source to select.
 *
 * @return SL_STATUS_OK on success.
 * @return SL_STATUS_INVALID_PARAMETER if @p clock_source is invalid.
 * @return SL_STATUS_NOT_INITIALIZED if the HAL has not been initialized.
 * @return SL_STATUS_NOT_SUPPORTED if the device has no CLKSEL register or the
 *         clock source is not supported (for example LFXO when disabled).
 ******************************************************************************/
sl_status_t sli_watchdog_manager_hal_set_clock_source(
  sli_watchdog_manager_hal_clock_source_t clock_source);

/***************************************************************************//**
 * @brief Start the hardware watchdog.
 *
 * @details
 * Enables and starts the hardware watchdog timer. After this function is
 * called, the watchdog must be fed regularly to prevent a reset.
 *
 * @return SL_STATUS_OK if successful.
 * @return Error code on failure.
 ******************************************************************************/
sl_status_t sli_watchdog_manager_hal_start(void);

/***************************************************************************//**
 * @brief Feed the hardware watchdog.
 *
 * @details
 * Resets the hardware watchdog counter, preventing it from expiring and
 * resetting the system. This function should be called regularly before the
 * watchdog timeout period expires.
 *
 * @return SL_STATUS_OK if successful.
 * @return Error code on failure.
 ******************************************************************************/
sl_status_t sli_watchdog_manager_hal_feed(void);

/***************************************************************************//**
 * @brief Disable the hardware watchdog.
 *
 * @details
 * Disables the hardware watchdog timer. This function is used on devices
 * without EM1RUN capability to stop the watchdog before entering EM1.
 *
 * @note On devices that support lock registers, this function may fail if
 *       the watchdog has been locked.
 *
 * @return SL_STATUS_OK if successful.
 * @return SL_STATUS_NOT_SUPPORTED if disabling is not supported.
 * @return Error code on failure.
 ******************************************************************************/
sl_status_t sli_watchdog_manager_hal_disable(void);

/***************************************************************************//**
 * @brief Enable the hardware watchdog.
 *
 * @details
 * Re-enables the hardware watchdog timer after it has been disabled. This
 * function is used on devices without EM1RUN capability to restart the
 * watchdog after exiting EM1.
 *
 * @return SL_STATUS_OK if successful.
 * @return Error code on failure.
 ******************************************************************************/
sl_status_t sli_watchdog_manager_hal_enable(void);

/***************************************************************************//**
 * @brief Check if hardware watchdog has EM1RUN capability.
 *
 * @details
 * Returns whether the hardware watchdog supports the EM1RUN feature, which
 * allows the watchdog to be configured to stop counting in EM1.
 *
 * @return true if EM1RUN is supported and configured, false otherwise.
 ******************************************************************************/
bool sli_watchdog_manager_hal_has_em1run(void);

#if SLI_WATCHDOG_MANAGER_USE_EM_TRANSITION_HOOK
/***************************************************************************//**
 * @brief Notify HAL of an energy mode transition.
 *
 * @details
 * When the WDOG lacks EM1/2/3 run support for any deeper sleep mode, disables
 * the hardware watchdog when leaving EM0 (per config) and re-enables when
 * entering EM0.
 *
 * @param from Energy mode we are leaving.
 * @param to   Energy mode we are entering.
 ******************************************************************************/
void sli_watchdog_manager_hal_on_em_transition(uint8_t from, uint8_t to);
#endif

#if defined(_WDOG_CFG_WARNSEL_MASK)
/***************************************************************************//**
 * @brief Enable the WDOG warning interrupt and NVIC routing.
 *
 * @details Installs the HAL starve IRQ handler via the interrupt manager.
 *          Requires @ref SL_WATCHDOG_MANAGER_WARNING_TIME to be enabled in config.
 *
 * @return SL_STATUS_OK on success.
 * @return SL_STATUS_NOT_SUPPORTED if warning interrupts are unavailable.
 ******************************************************************************/
sl_status_t sli_watchdog_manager_hal_enable_starve_interrupt(void);

/***************************************************************************//**
 * @brief Disable the WDOG warning interrupt and NVIC routing.
 *
 * @details Clears pending WARN flags, disables peripheral and NVIC interrupts,
 *          and resets the HAL enabled state. Safe to call when not enabled.
 *
 * @return SL_STATUS_OK on success.
 ******************************************************************************/
sl_status_t sli_watchdog_manager_hal_disable_starve_interrupt(void);
#endif
/***************************************************************************//**
 * @brief Enable reset on timeout.
 *
 * @details
 * Enables the reset on timeout feature of the hardware watchdog.
 *
 * @return SL_STATUS_OK if successful.
 * @return SL_STATUS_NOT_PERMISSION if the HAL has not been disabled.
 ******************************************************************************/
sl_status_t sli_watchdog_manager_hal_enable_reset(void);

/***************************************************************************//**
 * @brief Disable reset on timeout.
 *
 * @details
 * Disables the reset on timeout feature of the hardware watchdog.
 *
 * @return SL_STATUS_OK if successful.
 * @return SL_STATUS_NOT_PERMISSION if the HAL has not been disabled.
 ******************************************************************************/
sl_status_t sli_watchdog_manager_hal_disable_reset(void);
/** @} (end addtogroup watchdog_manager_hal) */

#ifdef __cplusplus
}
#endif

#endif /* SLI_WATCHDOG_MANAGER_HAL_H */
