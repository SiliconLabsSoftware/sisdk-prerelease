/***************************************************************************//**
 * @file
 * @brief Platform specific helpers for the logging core (Series 3).
 *
 *   The timestamp source on Series 3 depends on whether the RAIL library is
 *   part of the image. This file exposes one set of sl_log_hal_* APIs; each
 *   selects its behaviour internally on @ref SLI_LOG_USE_PROTIMER:
 *
 *     - RAIL present (SLI_LOG_USE_PROTIMER == 1): RAIL owns the shared PROTIMER -
 *       it powers the radio subsystem, enables the PROTIMER clock and IP,
 *       programs and starts the PRECNT -> BASECNT -> WRAPCNT cascade, and keeps
 *       it RTC-synced across deep sleep. The logger does NOT configure or start
 *       the PROTIMER; it only reads the latched counters via register access
 *       (the registers it reads are declared in sl_log_protimer.h at their
 *       fixed offsets that are common across Series 3, since the public device
 *       package omits radio peripheral headers): a read of
 *       LPRECNT latches LBASECNT (the raw 32-bit timestamp) and LWRAPCNT (the
 *       hardware epoch) for a coherent snapshot, and no software sleep
 *       compensation is applied.
 *
 *     - RAIL absent (SLI_LOG_USE_PROTIMER == 0): the PROTIMER lives in the radio
 *       power domain, which is not brought up without RAIL's radio clock
 *       sequence; touching it would fault. The logger instead drives a
 *       general-purpose TIMERn - the same model as Series 2 - via the shared
 *       sli_log_timer_timestamp.c helpers, providing a free-running raw counter
 *       timestamp plus a software epoch (u32) that together form a 64-bit
 *       monotonic time. Deep-sleep (EM2/EM3) time is folded straight into CNT
 *       on wake, so the epoch advances on TIMER overflow and when that
 *       wake-side CNT write wraps the counter; the per-event read stays a bare
 *       register load with no software offset.
 *
 *   Timestamps are reported in raw counter ticks; the host converts to
 *   wall-clock time using sl_log_hal_get_timestamp_timer_frequency().
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
#include "em_device.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/*******************************************************************************
**************************   LOCAL VARIABLES   ********************************
*******************************************************************************/

#if SLI_LOG_USE_PROTIMER

#include "sl_log_protimer.h"

/* Rate (Hz) of the 32-bit timestamp counter and ready flag for the inline hot
 * path: zero until RAIL has the PROTIMER enabled and configured. Declared
 * extern in sl_log_hal_inline.h. */
volatile uint32_t sli_log_protimer_raw_freq_hz;

/* Epoch (high u32 of the 64-bit time) latched during the most recent
 * sl_log_hal_get_timestamp_count() call so the paired
 * sl_log_hal_get_timestamp_epoch() returns a value consistent with the
 * timestamp just read. */
static volatile uint32_t cached_epoch;

/* Epoch from the most recent inline hot-path PROTIMER latch
 * (sli_log_hal_stamp_time / sli_log_hal_get_timestamp in sl_log_hal_inline.h). */
volatile uint32_t sli_log_protimer_latched_epoch;

/* Set once RAIL has run its own init and ungated the PROTIMER, so the registers
 * are reachable. Before that any PROTIMER access stalls the bus, so nothing may
 * go looking for the timebase until this is true. Enforced centrally by
 * log_platform_prime_timebase(). */
static volatile bool protimer_reachable;

/*******************************************************************************
**************************   LOCAL FUNCTIONS   ********************************
*******************************************************************************/

typedef struct {
  uint32_t timestamp;
  uint32_t epoch;
} sl_log_protimer_time_t;

/**
 * @brief Read the raw PROTIMER timestamp/epoch pair (RAIL-present source).
 *
 *   Reads LBASECNT (32-bit timestamp) and LWRAPCNT (hardware epoch) as an
 *   atomic pair: a read of LPRECNT latches both counters for a coherent
 *   snapshot. Returns { 0, 0 } if init has not run yet (sli_log_protimer_raw_freq_hz == 0).
 *   Performs only plain register loads (no SYNCBUSY / wait-sync).
 */
static sl_log_protimer_time_t log_platform_read_hw_time(void)
{
  sl_log_protimer_time_t time = { 0U, 0U };

  if (sli_log_protimer_raw_freq_hz == 0U) {
    return time;
  }

  /* Reading LPRECNT latches BASECNT/WRAPCNT for a coherent snapshot. */
  (void)PROTIMER->LPRECNT;
  time.timestamp = PROTIMER->LBASECNT;
  time.epoch = PROTIMER->LWRAPCNT;
  return time;
}

/**
 * @brief Derive the PROTIMER tick rate, making the timebase live.
 *
 *   sli_log_protimer_raw_freq_hz doubles as the readiness gate for every
 *   timestamp reader, including the inlined hot-path accessors in
 *   sl_log_hal_inline.h, which return zeros while it is 0. Deriving the rate is
 *   therefore what switches timestamping on, and it must happen off the logging
 *   hot path because the inlined readers deliberately never touch PROTIMER
 *   configuration registers.
 *
 *   Safe to call on a partially initialized system and can be retried until it
 *   takes: it is a no-op until RAIL has ungated the radio power domain, and
 *   again until RAIL has the PROTIMER enabled. Every path that wants to derive
 *   the rate goes through here so the reachability rule lives in one place.
 */
static void log_platform_prime_timebase(void)
{
  if (sli_log_protimer_raw_freq_hz != 0U) {
    return;
  }

  /* Until RAIL's own init has ungated the radio power domain the PROTIMER is
   * unreachable and even the EN probe below would stall the bus, so this has to
   * be checked before touching any register. */
  if (!protimer_reachable) {
    return;
  }

  if (PROTIMER->EN == 0U) {
    return;
  }

  /* The timestamp counter advances once per PRECNT overflow, i.e. every
   * (PRECNTTOP + 1) input-clock ticks. Read PRECNTTOP back from the register
   * RAIL programmed so the rate matches the live PROTIMER configuration. */
  uint32_t precnt_top = (PROTIMER->PRECNTTOP & _PROTIMER_PRECNTTOP_PRECNTTOP_MASK)
                        >> _PROTIMER_PRECNTTOP_PRECNTTOP_SHIFT;
  sli_log_protimer_raw_freq_hz = SystemHFXOClockGet() / (precnt_top + 1U);
}

#endif /* SLI_LOG_USE_PROTIMER */

/*******************************************************************************
*************************   GLOBAL FUNCTIONS   ********************************
*******************************************************************************/

#if SLI_LOG_USE_PROTIMER
/**
 * @brief Prime the PROTIMER timebase once RAIL has started it.
 *
 *   Registered as a stack_init handler that runs after RAIL's own init (see
 *   log_platform_specific.slcc). The logger's sl_log_init_stage2() runs back at
 *   service_init, where the PROTIMER is neither enabled nor even reachable, so
 *   this is the earliest point the tick rate can be derived - and deriving it is
 *   what makes the inlined hot-path accessors start emitting real timestamps
 *   instead of zeros. Events logged before this runs carry a zero timestamp
 *   because no radio timebase exists yet.
 */
void sli_log_protimer_prime_timebase(void)
{
  protimer_reachable = true;
  log_platform_prime_timebase();
}
#endif

/**
 * @brief Start (or attach to) the platform timestamp counter used by the
 *        logging core.
 *
 *   RAIL present: RAIL already owns and runs the PROTIMER, so this only
 *   validates the radio input clock is available. At boot the PROTIMER is still
 *   down, and the tick rate is instead derived by
 *   sli_log_protimer_prime_timebase() once RAIL has brought it up. On a re-init
 *   after that point the timebase is re-derived here.
 *
 *   RAIL absent: initializes and starts the general-purpose TIMERn via the
 *   shared helper.
 *
 * @return SL_STATUS_OK on success or an sl_status_t error code.
 */
sl_status_t sl_log_hal_platform_core_init(void)
{
#if SLI_LOG_USE_PROTIMER
  uint32_t input_clock_hz = SystemHFXOClockGet();
  if (input_clock_hz == 0U) {
    return SL_STATUS_INVALID_CONFIGURATION;
  }

  /* No-op before RAIL has brought the PROTIMER up; re-derives the rate on a
   * re-init after that point. */
  log_platform_prime_timebase();

  return SL_STATUS_OK;
#else
  return sli_log_timer_init();
#endif
}

/**
 * @brief Stop using the platform timestamp counter.
 *
 *   RAIL present: RAIL owns the PROTIMER lifecycle, so the logger must not stop
 *   it; just clears the cached tick rate so later timestamp reads return 0
 *   until a subsequent init re-reads it.
 *
 *   RAIL absent: stops the TIMERn peripheral via the shared helper.
 *
 * @return SL_STATUS_OK on success or an sl_status_t error code.
 */
sl_status_t sl_log_hal_core_deinit(void)
{
#if SLI_LOG_USE_PROTIMER
  sli_log_protimer_raw_freq_hz = 0U;
  return SL_STATUS_OK;
#else
  return sli_log_timer_deinit();
#endif
}

/**
 * @brief Get the current timestamp count for the specified core.
 *
 *   RAIL present: reads the latched PROTIMER counters; RAIL keeps them
 *   RTC-synced across deep sleep, so they are returned directly with no
 *   software offset, and the paired epoch is cached for the matching
 *   sl_log_hal_get_timestamp_epoch() call. Also retries priming the timebase,
 *   which covers a re-init cycle that cleared the rate and the case where RAIL
 *   had not enabled the PROTIMER yet when the stack_init handler ran. Callers
 *   before RAIL's init read zeros: the peripheral is unreachable until then, so
 *   the retry deliberately does not probe it.
 *
 *   RAIL absent: returns the raw free-running TIMER CNT via the shared helper.
 *
 * @param[in] core_id Core identifier (0 = host, currently unused)
 * @return Low 32 bits of the 64-bit time, in raw counter ticks (convert to us
 *         with @ref sl_log_hal_get_timestamp_timer_frequency).
 */
uint32_t sl_log_hal_get_timestamp_count(uint8_t core_id)
{
  (void)core_id;

#if SLI_LOG_USE_PROTIMER
  log_platform_prime_timebase();
  sl_log_protimer_time_t time = log_platform_read_hw_time();
  cached_epoch = time.epoch;
  return time.timestamp;
#else
  return sli_log_timer_get_count();
#endif
}

/**
 * @brief Get the timestamp epoch (high 32 bits of the 64-bit time).
 *
 *   RAIL present: the epoch latched during the most recent
 *   sl_log_hal_get_timestamp_count() (PROTIMER LWRAPCNT).
 *
 *   RAIL absent: the software epoch maintained by the shared TIMER helper.
 *
 * @param[in] core_id Core identifier (unused)
 * @return Epoch (high u32 of the 64-bit time).
 */
uint32_t sl_log_hal_get_timestamp_epoch(uint8_t core_id)
{
  (void)core_id;

#if SLI_LOG_USE_PROTIMER
  return cached_epoch;
#else
  return sli_log_timer_get_epoch();
#endif
}

/**
 * @brief Get the timestamp timer frequency (Hz) for a core.
 *
 * @param[in] core_id Core identifier (unused)
 * @return Rate of the 32-bit timestamp counter in Hz, or 0 if not yet
 *         initialized.
 */
uint32_t sl_log_hal_get_timestamp_timer_frequency(uint8_t core_id)
{
  (void)core_id;

#if SLI_LOG_USE_PROTIMER
  return sli_log_protimer_raw_freq_hz;
#else
  return sli_log_timer_get_frequency();
#endif
}

/**
 * @brief Prepare logging subsystem before entering sleep.
 *
 * Marks the logger as suspended.
 *
 *   RAIL present: the PROTIMER is left running and RAIL keeps it RTC-synced
 *   across EM2/EM3, so no snapshot or software offset is needed.
 *
 *   RAIL absent: also snapshots the HF TIMER reading (via the shared helper) so
 *   the wake-side offset push can tell the EM2 portion of the sleep window
 *   apart from any EM1 portion.
 *
 * @param[in] args Unused.
 * @return SL_STATUS_OK on success.
 */
sl_status_t sl_log_hal_pre_sleep_process(const void *args)
{
  (void)args;

#if !SLI_LOG_USE_PROTIMER
  sli_log_timer_pre_sleep_snapshot();
#endif

  sli_log_set_suspended(true);

  return SL_STATUS_OK;
}

/**
 * @brief Restore logging subsystem after wake-up from sleep.
 *
 * Clears the suspension flag and gives the active backend a chance to refresh
 * transport state via its optional @c on_wake hook. When the HF-TIMER path is
 * in use the integration layer (sl_log_power_manager.c) has already folded the
 * elapsed-sleep window into the hardware CNT before this runs.
 *
 * Runs inside the PM critical section with interrupts masked, so it must NOT
 * touch any timestamp-source register that requires a SYNCBUSY wait.
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
 * @return Pointer to the populated sl_log_api_core_t structure.
 */
sl_log_api_core_t *sl_log_get_api_core(void)
{
  return &sl_log_api_core;
}
