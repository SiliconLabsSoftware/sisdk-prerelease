/***************************************************************************//**
 * @file
 * @brief Platform specific helpers for the logging core (Series 2).
 *
 *   Series 2 always uses a general-purpose TIMERn as its timestamp source. The
 *   actual TIMER implementation (free-running counter, software epoch, TIMER
 *   overflow IRQ and deep-sleep offset fold-in) is shared with the Series 3
 *   no-RAIL path and lives in sli_log_timer_timestamp.c. This file only maps
 *   the generic sl_log_hal_* core API onto those shared helpers.
 *******************************************************************************
 * # License
 * <b>Copyright 2025 Silicon Laboratories Inc. www.silabs.com</b>
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
#include "sl_log_platform_specific.h"
#include "sl_log_internal.h"
#include "sli_log_timer_timestamp.h"
#include "sl_status.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/*******************************************************************************
*************************   GLOBAL FUNCTIONS   ********************************
*******************************************************************************/

/**
 * @brief Start the platform timestamp counter used by the logging core.
 *
 * @return SL_STATUS_OK on success or an sl_status_t error code.
 */
sl_status_t sl_log_hal_platform_core_init(void)
{
  return sli_log_timer_init();
}

/**
 * @brief Stop the platform timestamp counter.
 *
 * @return SL_STATUS_OK on success or an sl_status_t error code.
 */
sl_status_t sl_log_hal_core_deinit(void)
{
  return sli_log_timer_deinit();
}

/**
 * @brief Get the current timestamp count for the specified core.
 *
 * Returns the raw free-running counter value (in timer ticks). A host-side
 * post-processing script converts the returned ticks to wall-clock time using
 * sl_log_hal_get_timestamp_timer_frequency(). The high 32 bits of the 64-bit
 * time are obtained from the paired accessor sl_log_hal_get_timestamp_epoch().
 *
 * @param[in] core_id Core identifier (0 = host, currently unused)
 * @return Low 32 bits of the 64-bit time, in raw timer ticks
 */
uint32_t sl_log_hal_get_timestamp_count(uint8_t core_id)
{
  (void)core_id;

  return sli_log_timer_get_count();
}

/**
 * @brief Get the timestamp epoch (high 32 bits of the 64-bit time).
 *
 * @param[in] core_id Core identifier (unused)
 * @return Epoch (high u32 of the 64-bit time).
 */
uint32_t sl_log_hal_get_timestamp_epoch(uint8_t core_id)
{
  (void)core_id;

  return sli_log_timer_get_epoch();
}

/**
 * @brief Get the timestamp timer frequency (Hz) for a core.
 *
 * @param[in] core_id Core identifier (unused)
 * @return Counter frequency in Hz, or 0 if the timer is not yet initialized.
 */
uint32_t sl_log_hal_get_timestamp_timer_frequency(uint8_t core_id)
{
  (void)core_id;

  return sli_log_timer_get_frequency();
}

/**
 * @brief Prepare logging subsystem before entering sleep.
 *
 * Snapshots the HF TIMER reading (so the wake-side offset push can separate the
 * EM2-gated portion of the sleep window from any EM1 portion) and marks the
 * logger as suspended. Pending events already in the ring buffer keep their
 * pre-sleep timestamps and resume transmission after wake.
 *
 * Runs inside the power-manager critical section with interrupts masked; the
 * snapshot uses only a bare TIMER register read (no SYNCBUSY busy-wait).
 *
 * @param[in] args Unused.
 * @return SL_STATUS_OK on success.
 */
sl_status_t sl_log_hal_pre_sleep_process(const void *args)
{
  (void)args;

  sli_log_timer_pre_sleep_snapshot();

  sli_log_set_suspended(true);

  return SL_STATUS_OK;
}

/**
 * @brief Restore logging subsystem after wake-up from sleep.
 *
 * Clears the suspension flag and gives the active backend a chance to refresh
 * transport state via its optional @c on_wake hook. The integration layer
 * (sl_log_power_manager.c) has already folded the elapsed-sleep window into the
 * hardware CNT before this runs, so the next timestamp read advances
 * continuously across the sleep window.
 *
 * Runs inside the PM critical section with interrupts masked, so it must not
 * touch the HF TIMER hardware.
 *
 * @param[in] args Unused.
 * @return SL_STATUS_OK, or the backend's @c on_wake error code.
 */
sl_status_t sl_log_hal_post_sleep_process(const void *args)
{
  (void)args;

  sl_status_t status = SL_STATUS_OK;

  sl_log_api_backend_t *backend = sl_log_get_api_backend();
  if (backend != NULL && backend->on_wake != NULL) {
    status = backend->on_wake();
  }

  sli_log_set_suspended(false);
  return status;
}

/**
 * @brief Set the platform-specific logging configuration.
 *
 * @param[in] args Pointer to platform config structure
 * @param[in] core_id Core identifier
 * @return SL_STATUS_OK currently always returned
 */
sl_status_t sl_log_hal_set_configuration(const void *args, uint8_t core_id)
{
  (void)args;
  (void)core_id;

  return SL_STATUS_OK;
}

/**
 * @brief Get the platform-specific logging configuration.
 *
 * @param[out] args Pointer to a platform config structure to fill
 * @param[in]  core_id Core identifier
 * @return SL_STATUS_OK currently always returned
 */
sl_status_t sl_log_hal_get_configuration(void *args, uint8_t core_id)
{
  (void)args;
  (void)core_id;

  return SL_STATUS_OK;
}

/**
 * @brief   Core API structure.
 *
 */
sl_log_api_core_t sl_log_api_core = { .platform_core_init       = sl_log_hal_platform_core_init,
                                      .platform_core_deinit        = sl_log_hal_core_deinit,
                                      .get_timestamp                 = sl_log_hal_get_timestamp_count,
                                      .get_timestamp_epoch           = sl_log_hal_get_timestamp_epoch,
                                      .get_timestamp_timer_frequency = sl_log_hal_get_timestamp_timer_frequency,
                                      .post_sleep_process            = sl_log_hal_post_sleep_process,
                                      .pre_sleep_process             = sl_log_hal_pre_sleep_process,
                                      .set_configuration             = sl_log_hal_set_configuration,
                                      .get_configuration             = sl_log_hal_get_configuration };

/**
 * @brief Return pointer to the core API structure.
 *
 * Provides the generic logging core APIs
 *
 * @return Pointer to the populated sl_log_api_backend_t structure.
 */
sl_log_api_core_t *sl_log_get_api_core(void)
{
  return &sl_log_api_core;
}
