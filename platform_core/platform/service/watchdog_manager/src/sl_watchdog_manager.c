/***************************************************************************//**
 * @file
 * @brief Watchdog Manager Service Implementation
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
#include "sl_watchdog_manager_config.h"
#include "sli_watchdog_manager_hal.h"
#include "sl_assert.h"
#include "sl_common.h"
#include "sl_core.h"
#include "sl_hal_emu.h"
#include "sl_component_catalog.h"

#include <string.h>

/***************************************************************************//**
 * @addtogroup watchdog_manager
 * @{
 ******************************************************************************/

/*******************************************************************************
 *******************************   TYPEDEFS   **********************************
 ******************************************************************************/

/// Watchdog manager internal state.
typedef struct {
  uint32_t watchdog_uids[SL_WATCHDOG_MANAGER_MAX_SW_WATCHDOGS]; ///< UID per slot (for debugging).
  uint32_t fed_mask;      ///< Bitmask of watchdogs that have been fed this period.
  uint32_t enabled_mask;  ///< Bitmask of enabled watchdogs.
  uint32_t allocated_mask; ///< Bitmask of allocated watchdogs.
  bool initialized;       ///< Whether the manager has been initialized.
  bool started;           ///< Whether the manager has been started.
} watchdog_manager_state_t;

/// No-init state for preserving data across resets.
typedef struct {
  uint32_t faulty_handle; ///< Handle of watchdog that caused reset.
  uint32_t magic;         ///< Magic number to validate data.
  bool valid;             ///< Whether the data is valid.
} watchdog_manager_noinit_state_t;

/*******************************************************************************
 *****************************   LOCAL DATA   **********************************
 ******************************************************************************/

/// Magic number to validate no-init data.
#define WATCHDOG_MANAGER_MAGIC  0x574D4752u  // "WMGR".

/// Watchdog manager state.
static watchdog_manager_state_t manager_state;

/// Optional application starve callback (invoked from WDOG warning IRQ).
static sl_watchdog_manager_starve_callback_t starve_callback = NULL;

/// No-init state for reset cause tracking (preserved across resets).
#if defined(__ICCARM__)
__no_init static watchdog_manager_noinit_state_t noinit_state @ ".noinit";
#else
static watchdog_manager_noinit_state_t noinit_state SL_ATTRIBUTE_SECTION(".noinit");
#endif

#if SL_WATCHDOG_MANAGER_LOCK == 0
/// Pending timeout period for apply_hardware_configuration_safely().
static uint8_t pending_timeout_period;

/// Pending clock source for apply_hardware_configuration_safely().
static sli_watchdog_manager_hal_clock_source_t pending_clock_source;

/// Previous timeout period used for rollback.
static uint8_t previous_timeout_period;

/// Previous clock source used for rollback.
static sli_watchdog_manager_hal_clock_source_t previous_clock_source;
#endif // SL_WATCHDOG_MANAGER_LOCK == 0

/*******************************************************************************
 **************************   LOCAL FUNCTIONS   ********************************
 ******************************************************************************/

/***************************************************************************//**
 * @brief Find first available handle slot.
 *
 * @return Handle for the new watchdog (0-31), or UINT32_MAX if none available.
 ******************************************************************************/
static sl_watchdog_handle_t find_available_handle(void)
{
  for (uint32_t i = 0; i < SL_WATCHDOG_MANAGER_MAX_SW_WATCHDOGS; i++) {
    if ((manager_state.allocated_mask & (1u << i)) == 0) {
      return i;
    }
  }
  return UINT32_MAX;
}

/***************************************************************************//**
 * @brief Validate a watchdog handle.
 *
 * @param[in] handle Handle to validate (bit position 0-31).
 *
 * @return true if valid, false otherwise.
 ******************************************************************************/
static bool is_valid_handle(sl_watchdog_handle_t handle)
{
  // Check if handle is within valid range (0-31).
  if (handle >= SL_WATCHDOG_MANAGER_MAX_SW_WATCHDOGS) {
    return false;
  }

  // Check if handle is allocated.
  return (manager_state.allocated_mask & (1u << handle)) != 0;
}

/***************************************************************************//**
 * @brief Update hardware watchdog state based on enabled watchdogs.
 *
 * @details
 * Enables the hardware watchdog if any software watchdog is enabled.
 * Disables the hardware watchdog if no software watchdogs are enabled.
 ******************************************************************************/
static void update_hw_watchdog_state(void)
{
  // If any watchdog is enabled, hardware watchdog should be enabled.
  if (manager_state.enabled_mask != 0) {
    // Hardware watchdog needs to be enabled.
    sl_watchdog_manager_start();
  } else {
    // No watchdogs enabled, disable hardware watchdog to save power.
    sl_status_t status = sli_watchdog_manager_hal_disable();
    manager_state.started = false;
    (void)status; // Suppress unused variable warning.
  }
}

/***************************************************************************//**
 * @brief Check if all enabled watchdogs have been fed and feed HW watchdog.
 *
 * @note Caller must hold the atomic section (CORE_ENTER_ATOMIC).
 ******************************************************************************/
static void check_and_feed_hw_watchdog(void)
{
  // Check if any watchdogs are enabled and all of them have been fed.
  if (manager_state.enabled_mask != 0
      && (manager_state.fed_mask & manager_state.enabled_mask)
      == manager_state.enabled_mask) {
    // All enabled watchdogs fed, feed hardware watchdog.
    sli_watchdog_manager_hal_feed();

    // Reset fed mask for next period.
    manager_state.fed_mask = 0;
  } else if (manager_state.enabled_mask != 0) {
    // Record faulty watchdog for post-reset retrieval.
    sli_watchdog_manager_record_state();
  }
}

/***************************************************************************//**
 * @brief Record faulty watchdog for post-reset retrieval.
 ******************************************************************************/
static void record_faulty_watchdog(void)
{
  // Find first unfed enabled watchdog.
  uint32_t unfed_mask = manager_state.enabled_mask & ~manager_state.fed_mask;

  if (unfed_mask != 0) {
    // Find first unfed watchdog (bit position).
    for (uint32_t i = 0; i < SL_WATCHDOG_MANAGER_MAX_SW_WATCHDOGS; i++) {
      if (unfed_mask & (1u << i)) {
        noinit_state.faulty_handle = i;  // Store bit position, not bitmask.
        noinit_state.magic = WATCHDOG_MANAGER_MAGIC;
        noinit_state.valid = true;
        break;
      }
    }
  }
}

/***************************************************************************//**
 * @brief Enable or disable starve interrupt based on callback registration.
 ******************************************************************************/
static sl_status_t sync_starve_interrupt(void)
{
#if defined(_WDOG_CFG_WARNSEL_MASK)
#if defined(SL_CATALOG_CRASH_MANAGER_COMPONENT_PRESENT)
  return sli_watchdog_manager_hal_enable_starve_interrupt();
#else
  if (starve_callback != NULL
      && SL_WATCHDOG_MANAGER_WARNING_TIME != SL_WATCHDOG_MANAGER_WARNING_DISABLE) {
    return sli_watchdog_manager_hal_enable_starve_interrupt();
  }
  return sli_watchdog_manager_hal_disable_starve_interrupt();
#endif
#else
  return SL_STATUS_OK;
#endif
}

/***************************************************************************//**
 * @brief Convert a HAL clock source to the public representation.
 ******************************************************************************/
static sl_status_t clock_source_from_hal(
  sli_watchdog_manager_hal_clock_source_t hal_clock_source,
  sl_watchdog_manager_clock_source_t *clock_source)
{
  switch (hal_clock_source) {
    case SLI_WATCHDOG_MANAGER_HAL_CLK_HCLKDIV1024:
      *clock_source = SL_WATCHDOG_MANAGER_CLOCK_SOURCE_HCLKDIV1024;
      break;

    case SLI_WATCHDOG_MANAGER_HAL_CLK_LFRCO:
      *clock_source = SL_WATCHDOG_MANAGER_CLOCK_SOURCE_LFRCO;
      break;

    case SLI_WATCHDOG_MANAGER_HAL_CLK_LFXO:
      *clock_source = SL_WATCHDOG_MANAGER_CLOCK_SOURCE_LFXO;
      break;

    case SLI_WATCHDOG_MANAGER_HAL_CLK_ULFRCO:
      *clock_source = SL_WATCHDOG_MANAGER_CLOCK_SOURCE_ULFRCO;
      break;

    case SLI_WATCHDOG_MANAGER_HAL_CLK_INVALID:
    default:
      return SL_STATUS_INVALID_STATE;
  }

  return SL_STATUS_OK;
}

#if SL_WATCHDOG_MANAGER_LOCK == 0
/***************************************************************************//**
 * @brief Convert a public clock source to the HAL representation.
 ******************************************************************************/
static sl_status_t clock_source_to_hal(
  sl_watchdog_manager_clock_source_t clock_source,
  sli_watchdog_manager_hal_clock_source_t *hal_clock_source)
{
  switch (clock_source) {
    case SL_WATCHDOG_MANAGER_CLOCK_SOURCE_HCLKDIV1024:
      *hal_clock_source = SLI_WATCHDOG_MANAGER_HAL_CLK_HCLKDIV1024;
      break;

    case SL_WATCHDOG_MANAGER_CLOCK_SOURCE_LFRCO:
      *hal_clock_source = SLI_WATCHDOG_MANAGER_HAL_CLK_LFRCO;
      break;

    case SL_WATCHDOG_MANAGER_CLOCK_SOURCE_LFXO:
      *hal_clock_source = SLI_WATCHDOG_MANAGER_HAL_CLK_LFXO;
      break;

    case SL_WATCHDOG_MANAGER_CLOCK_SOURCE_ULFRCO:
      *hal_clock_source = SLI_WATCHDOG_MANAGER_HAL_CLK_ULFRCO;
      break;

    default:
      return SL_STATUS_INVALID_PARAMETER;
  }

  return SL_STATUS_OK;
}

/***************************************************************************//**
 * @brief Restore previous hardware configuration and running state.
 ******************************************************************************/
static void restore_hardware_configuration(
  sl_status_t (*apply_previous)(void),
  bool was_started)
{
  (void)apply_previous();
  (void)sli_watchdog_manager_hal_feed();

  if (was_started) {
    (void)sli_watchdog_manager_hal_enable();
    (void)sync_starve_interrupt();
  }
}

/***************************************************************************//**
 * @brief Disable WDOG, apply a configuration change, feed, and restore state.
 ******************************************************************************/
static sl_status_t apply_hardware_configuration_safely(
  sl_status_t (*apply_configuration)(void),
  sl_status_t (*apply_previous)(void))
{
  sl_status_t status;
  bool was_started = manager_state.started;

  status = sli_watchdog_manager_hal_disable();
  if (status != SL_STATUS_OK && status != SL_STATUS_NOT_SUPPORTED) {
    return status;
  }

  status = apply_configuration();
  if (status != SL_STATUS_OK) {
    restore_hardware_configuration(apply_previous, was_started);
    return status;
  }

  status = sli_watchdog_manager_hal_feed();
  if (status != SL_STATUS_OK) {
    restore_hardware_configuration(apply_previous, was_started);
    return status;
  }

  if (was_started) {
    status = sli_watchdog_manager_hal_enable();
    if (status != SL_STATUS_OK && status != SL_STATUS_NOT_SUPPORTED) {
      restore_hardware_configuration(apply_previous, was_started);
      return status;
    }

    status = sync_starve_interrupt();
  }

  return status;
}

static sl_status_t apply_pending_timeout_period(void)
{
  return sli_watchdog_manager_hal_set_timeout_period(pending_timeout_period);
}

static sl_status_t apply_previous_timeout_period(void)
{
  return sli_watchdog_manager_hal_set_timeout_period(previous_timeout_period);
}

static sl_status_t apply_pending_clock_source(void)
{
  return sli_watchdog_manager_hal_set_clock_source(pending_clock_source);
}

static sl_status_t apply_previous_clock_source(void)
{
  return sli_watchdog_manager_hal_set_clock_source(previous_clock_source);
}
#endif // SL_WATCHDOG_MANAGER_LOCK == 0

/*******************************************************************************
 **************************   GLOBAL FUNCTIONS   *******************************
 ******************************************************************************/

/***************************************************************************//**
 * Initialize watchdog manager.
 ******************************************************************************/
void sl_watchdog_manager_init(void)
{
  // Clear manager state.
  memset(&manager_state, 0, sizeof(manager_state));

  // Initialize HAL.
  sl_status_t status = sli_watchdog_manager_hal_init(SL_WATCHDOG_MANAGER_TIMEOUT_PERIOD);
  EFM_ASSERT(status == SL_STATUS_OK);
  (void)status; // Suppress unused variable warning in release builds.

  manager_state.initialized = true;
}

/***************************************************************************//**
 * Start watchdog manager.
 ******************************************************************************/
void sl_watchdog_manager_start(void)
{
  EFM_ASSERT(manager_state.initialized);

  if (!manager_state.started) {
    sl_status_t status = sli_watchdog_manager_hal_start();
    EFM_ASSERT(status == SL_STATUS_OK);
    (void)status; // Suppress unused variable warning in release builds.

#if SLI_WATCHDOG_MANAGER_USE_EM_TRANSITION_HOOK
    sli_watchdog_manager_power_subscribe_em_transition();
#endif

#if defined(SL_CATALOG_MICRIUMOS_KERNEL_PRESENT)
    sli_watchdog_manager_micrium_install_task_sw_hook();
#endif

    manager_state.started = true;

    (void)sync_starve_interrupt();
  }
}

/***************************************************************************//**
 * Create a software watchdog.
 ******************************************************************************/
sl_status_t sl_watchdog_manager_create(sl_watchdog_handle_t *handle,
                                       uint32_t watchdog_uid)
{
  if (handle == NULL) {
    return SL_STATUS_NULL_POINTER;
  }

  if (!manager_state.initialized) {
    return SL_STATUS_NOT_INITIALIZED;
  }

  CORE_DECLARE_IRQ_STATE;
  CORE_ENTER_ATOMIC();

  // Find available handle.
  sl_watchdog_handle_t new_handle = find_available_handle();
  if (new_handle == UINT32_MAX) {
    CORE_EXIT_ATOMIC();
    return SL_STATUS_NO_MORE_RESOURCE;
  }

  EFM_ASSERT(new_handle < SL_WATCHDOG_MANAGER_MAX_SW_WATCHDOGS);

  // Store UID for this slot (for debugging / reset cause).
  manager_state.watchdog_uids[new_handle] = watchdog_uid;

  // Update masks (convert handle to bitmask).
  uint32_t bit = (1u << new_handle);
  manager_state.allocated_mask |= bit;
  manager_state.enabled_mask |= bit;

  // Check if this matches a faulty watchdog from previous reset.
  if (noinit_state.valid
      && noinit_state.magic == WATCHDOG_MANAGER_MAGIC
      && noinit_state.faulty_handle == new_handle) {
    // Log that this watchdog was previously faulty.
    // In a real implementation, this would use the logging system.
    // For now, we just clear the noinit state.
    noinit_state.valid = false;
  }

  *handle = new_handle;

  CORE_EXIT_ATOMIC();

  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Delete a software watchdog.
 ******************************************************************************/
sl_status_t sl_watchdog_manager_delete(sl_watchdog_handle_t *handle)
{
  if (handle == NULL) {
    return SL_STATUS_NULL_POINTER;
  }

  if (!manager_state.initialized) {
    return SL_STATUS_NOT_INITIALIZED;
  }

  if (!is_valid_handle(*handle)) {
    return SL_STATUS_INVALID_HANDLE;
  }

  CORE_DECLARE_IRQ_STATE;
  CORE_ENTER_ATOMIC();

  EFM_ASSERT(is_valid_handle(*handle));

  // Clear UID for this slot.
  manager_state.watchdog_uids[*handle] = 0;

  // Update masks (convert handle to bitmask).
  uint32_t bit = (1u << *handle);
  manager_state.allocated_mask &= ~bit;
  manager_state.enabled_mask &= ~bit;
  manager_state.fed_mask &= ~bit;

  // Update hardware watchdog state based on remaining enabled watchdogs.
  update_hw_watchdog_state();

  // Clear handle.
  *handle = UINT32_MAX;

  // Check if we can now feed HW watchdog.
  check_and_feed_hw_watchdog();

  CORE_EXIT_ATOMIC();

  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Feed a software watchdog.
 ******************************************************************************/
sl_status_t sl_watchdog_manager_feed(sl_watchdog_handle_t *handle)
{
  if (handle == NULL) {
    return SL_STATUS_NULL_POINTER;
  }

  if (!manager_state.initialized) {
    return SL_STATUS_NOT_INITIALIZED;
  }

  if (!is_valid_handle(*handle)) {
    return SL_STATUS_INVALID_HANDLE;
  }

  CORE_DECLARE_IRQ_STATE;
  CORE_ENTER_ATOMIC();

  // Mark this watchdog as fed (convert handle to bitmask).
  manager_state.fed_mask |= (1u << *handle);

  // Check if all watchdogs are fed and feed HW watchdog if so.
  check_and_feed_hw_watchdog();

  CORE_EXIT_ATOMIC();

  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Disable a software watchdog.
 ******************************************************************************/
sl_status_t sl_watchdog_manager_disable(sl_watchdog_handle_t *handle)
{
  if (handle == NULL) {
    return SL_STATUS_NULL_POINTER;
  }

  if (!manager_state.initialized) {
    return SL_STATUS_NOT_INITIALIZED;
  }

  if (!is_valid_handle(*handle)) {
    return SL_STATUS_INVALID_HANDLE;
  }

  CORE_DECLARE_IRQ_STATE;
  CORE_ENTER_ATOMIC();

  EFM_ASSERT(is_valid_handle(*handle));
  manager_state.enabled_mask &= ~(1u << *handle);

  // Update hardware watchdog state based on remaining enabled watchdogs.
  update_hw_watchdog_state();

  // Check if we can now feed HW watchdog.
  check_and_feed_hw_watchdog();

  CORE_EXIT_ATOMIC();

  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Check if a software watchdog is enabled.
 ******************************************************************************/
sl_status_t sl_watchdog_manager_is_enabled(sl_watchdog_handle_t *handle,
                                           bool *is_enabled)
{
  if (handle == NULL || is_enabled == NULL) {
    return SL_STATUS_NULL_POINTER;
  }

  if (!manager_state.initialized) {
    return SL_STATUS_NOT_INITIALIZED;
  }

  if (!is_valid_handle(*handle)) {
    return SL_STATUS_INVALID_HANDLE;
  }

  EFM_ASSERT(is_valid_handle(*handle));
  *is_enabled = (manager_state.enabled_mask & (1u << *handle)) != 0;

  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Enable a software watchdog.
 ******************************************************************************/
sl_status_t sl_watchdog_manager_enable(sl_watchdog_handle_t *handle)
{
  if (handle == NULL) {
    return SL_STATUS_NULL_POINTER;
  }

  if (!manager_state.initialized) {
    return SL_STATUS_NOT_INITIALIZED;
  }

  if (!is_valid_handle(*handle)) {
    return SL_STATUS_INVALID_HANDLE;
  }

  CORE_DECLARE_IRQ_STATE;
  CORE_ENTER_ATOMIC();

  EFM_ASSERT(is_valid_handle(*handle));
  manager_state.enabled_mask |= (1u << *handle);

  // Update hardware watchdog state based on enabled watchdogs.
  update_hw_watchdog_state();

  CORE_EXIT_ATOMIC();

  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Force feed hardware watchdog.
 ******************************************************************************/
sl_status_t sl_watchdog_manager_force_feed(void)
{
  if (!manager_state.initialized) {
    return SL_STATUS_NOT_INITIALIZED;
  }

  CORE_DECLARE_IRQ_STATE;
  CORE_ENTER_ATOMIC();

  sl_status_t status = sli_watchdog_manager_hal_feed();
  // Reset fed mask since we just fed HW watchdog.
  manager_state.fed_mask = 0;

  CORE_EXIT_ATOMIC();

  return status;
}

/***************************************************************************//**
 * Get the hardware watchdog timeout period.
 ******************************************************************************/
sl_status_t sl_watchdog_manager_get_timeout_period(uint8_t *period)
{
  return sli_watchdog_manager_hal_get_timeout_period(period);
}

/***************************************************************************//**
 * Set the hardware watchdog timeout period.
 ******************************************************************************/
sl_status_t sl_watchdog_manager_set_timeout_period(uint8_t period)
{
  sl_status_t status;

  if (!manager_state.initialized) {
    return SL_STATUS_NOT_INITIALIZED;
  }

#if SL_WATCHDOG_MANAGER_LOCK != 0
  (void)period;
  return SL_STATUS_PERMISSION;
#else
  if (period > 15) {
    return SL_STATUS_INVALID_PARAMETER;
  }

  status = sli_watchdog_manager_hal_get_timeout_period(&previous_timeout_period);
  if (status != SL_STATUS_OK) {
    return status;
  }

  if (previous_timeout_period == period) {
    return SL_STATUS_OK;
  }

  pending_timeout_period = period;
  return apply_hardware_configuration_safely(apply_pending_timeout_period,
                                             apply_previous_timeout_period);
#endif
}

/***************************************************************************//**
 * Get the hardware watchdog clock source.
 ******************************************************************************/
sl_status_t sl_watchdog_manager_get_clock_source(
  sl_watchdog_manager_clock_source_t *clock_source)
{
  sl_status_t status;
  sli_watchdog_manager_hal_clock_source_t hal_clock_source;

  if (clock_source == NULL) {
    return SL_STATUS_NULL_POINTER;
  }

  status = sli_watchdog_manager_hal_get_clock_source(&hal_clock_source);
  if (status != SL_STATUS_OK) {
    return status;
  }

  return clock_source_from_hal(hal_clock_source, clock_source);
}

/***************************************************************************//**
 * Set the hardware watchdog clock source.
 ******************************************************************************/
sl_status_t sl_watchdog_manager_set_clock_source(
  sl_watchdog_manager_clock_source_t clock_source)
{
  sl_status_t status;
  sli_watchdog_manager_hal_clock_source_t hal_clock_source;

  if (!manager_state.initialized) {
    return SL_STATUS_NOT_INITIALIZED;
  }

#if SL_WATCHDOG_MANAGER_LOCK != 0
  (void)clock_source;
  return SL_STATUS_PERMISSION;
#else
  status = clock_source_to_hal(clock_source, &hal_clock_source);
  if (status != SL_STATUS_OK) {
    return status;
  }

  status = sli_watchdog_manager_hal_get_clock_source(&previous_clock_source);
  if (status != SL_STATUS_OK) {
    return status;
  }

  if (previous_clock_source == hal_clock_source) {
    return SL_STATUS_OK;
  }

  pending_clock_source = hal_clock_source;
  return apply_hardware_configuration_safely(apply_pending_clock_source,
                                             apply_previous_clock_source);
#endif
}

/***************************************************************************//**
 * Retrieve faulty watchdog from previous reset.
 ******************************************************************************/
sl_status_t sl_watchdog_manager_retrieve_faulty(sl_watchdog_handle_t *handle)
{
  if (handle == NULL) {
    return SL_STATUS_NULL_POINTER;
  }

  // This must be called before init.
  if (manager_state.initialized) {
    return SL_STATUS_INVALID_STATE;
  }

  // Check if we have valid noinit data.
  if (!noinit_state.valid || noinit_state.magic != WATCHDOG_MANAGER_MAGIC) {
    return SL_STATUS_NOT_AVAILABLE;
  }

  // Check if reset was caused by watchdog.
  #if defined(_EMU_SOFTRST_CAUSE_MASK)
  uint32_t reset_cause = sl_hal_emu_get_soft_reset_cause();
  #else
  uint32_t reset_cause = sl_hal_emu_get_reset_cause();
  #endif

  // Check for watchdog reset (bit positions may vary by device).
  // This is a simplified check - actual implementation would need
  // device-specific handling.
  bool is_watchdog_reset = false;
  #if defined(EMU_SOFTRST_CAUSE_WDOG3)
  is_watchdog_reset = (reset_cause & (EMU_SOFTRST_CAUSE_WDOG0 | EMU_SOFTRST_CAUSE_WDOG1 | EMU_SOFTRST_CAUSE_WDOG2 | EMU_SOFTRST_CAUSE_WDOG3)) != 0;
  #elif defined(EMU_RSTCAUSE_WDOG1)
  is_watchdog_reset = (reset_cause & (EMU_RSTCAUSE_WDOG0 | EMU_RSTCAUSE_WDOG1)) != 0;
  #elif defined(EMU_RSTCAUSE_WDOG0)
  is_watchdog_reset = (reset_cause & (EMU_RSTCAUSE_WDOG0)) != 0;
  #endif
  if (!is_watchdog_reset) {
    noinit_state.valid = false;
    return SL_STATUS_NOT_AVAILABLE;
  }

  *handle = noinit_state.faulty_handle;

  // Keep noinit_state valid so we can log when the watchdog is recreated.

  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Internal function called before potential watchdog reset to record state.
 * This is called from check_and_feed_hw_watchdog() function.
 * This function is called repeatedly so only the last faulty watchdog handle is stored.
 ******************************************************************************/
void sli_watchdog_manager_record_state(void)
{
  if (manager_state.initialized && manager_state.started) {
    record_faulty_watchdog();
  }
}

/***************************************************************************//**
 * Register starve callback.
 ******************************************************************************/
sl_status_t sl_watchdog_manager_set_starve_callback(
  sl_watchdog_manager_starve_callback_t callback)
{
#if !defined(_WDOG_CFG_WARNSEL_MASK)
  (void)callback;
  return SL_STATUS_NOT_SUPPORTED;
#else
  if (SL_WATCHDOG_MANAGER_WARNING_TIME == SL_WATCHDOG_MANAGER_WARNING_DISABLE
      && callback != NULL) {
    return SL_STATUS_NOT_SUPPORTED;
  }

  CORE_DECLARE_IRQ_STATE;
  CORE_ENTER_ATOMIC();
  starve_callback = callback;
  bool started = manager_state.started;
  CORE_EXIT_ATOMIC();

  if (started) {
    return sync_starve_interrupt();
  }

  return SL_STATUS_OK;
#endif
}

/***************************************************************************//**
 * Invoke starve callback from WDOG warning IRQ.
 ******************************************************************************/
void sli_watchdog_manager_on_starve(void)
{
  sl_watchdog_manager_starve_context_t context;
  sl_watchdog_manager_starve_callback_t callback;

  context.faulty_handle = UINT32_MAX;
  context.watchdog_uid = 0;

  CORE_DECLARE_IRQ_STATE;
  CORE_ENTER_ATOMIC();

  uint32_t unfed_mask = manager_state.enabled_mask & ~manager_state.fed_mask;
  if (unfed_mask != 0) {
    for (uint32_t i = 0; i < SL_WATCHDOG_MANAGER_MAX_SW_WATCHDOGS; i++) {
      if (unfed_mask & (1u << i)) {
        context.faulty_handle = i;
        context.watchdog_uid = manager_state.watchdog_uids[i];
        break;
      }
    }
  }

  record_faulty_watchdog();
  callback = starve_callback;

  CORE_EXIT_ATOMIC();

  if (callback != NULL) {
    callback(&context);
  }
}

/***************************************************************************//**
 * Enable reset on timeout.
 ******************************************************************************/
sl_status_t sl_watchdog_manager_enable_reset(void)
{
  sl_status_t status;
  bool was_running = manager_state.started;

  status = sli_watchdog_manager_hal_disable();
  if (status != SL_STATUS_OK) {
    return status;
  }

  status = sli_watchdog_manager_hal_enable_reset();
  if (status != SL_STATUS_OK) {
    return status;
  }

  if (was_running) {
    status = sli_watchdog_manager_hal_enable();
    if (status != SL_STATUS_OK) {
      return status;
    }
  }

  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Disable reset on timeout.
 ******************************************************************************/
sl_status_t sl_watchdog_manager_disable_reset(void)
{
  sl_status_t status;
  bool was_running = manager_state.started;

  status = sli_watchdog_manager_hal_disable();
  if (status != SL_STATUS_OK) {
    return status;
  }

  status = sli_watchdog_manager_hal_disable_reset();
  if (status != SL_STATUS_OK) {
    return status;
  }

  if (was_running) {
    status = sli_watchdog_manager_hal_enable();
    if (status != SL_STATUS_OK) {
      return status;
    }
  }

  return SL_STATUS_OK;
}

/** @} (end addtogroup watchdog_manager) */
