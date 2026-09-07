/***************************************************************************//**
 * @file
 * @brief Inlined timestamp accessors for the logging core (Series 3).
 *
 *   The logger core (sl_log.c) stamps every event with a timestamp and an
 *   epoch on the hot path. Routing those reads through the platform
 *   api_core function pointers adds an indirect call and a struct
 *   dereference per event. To keep the hot path cheap while keeping sl_log.c
 *   free of any series-specific code, the core instead calls the fixed-name
 *   static-inline accessors declared here, and the build selects the matching
 *   per-series implementation of this header via the include path (see
 *   log_platform_specific.slcc).
 *
 *   Series 3 has two timestamp sources, selected the same way as in
 *   sl_log_platform_specific_s3.c:
 *
 *     - RAIL present: the PROTIMER (a radio peripheral RAIL powers and keeps
 *       RTC-synced across deep sleep). LBASECNT is the raw 32-bit timestamp and
 *       LWRAPCNT is the hardware epoch; reading LPRECNT latches both for a
 *       coherent snapshot. The latched counters are used directly with no
 *       software offset. Register definitions come from sl_log_protimer.h.
 *
 *     - RAIL absent: a general-purpose TIMERn (the radio domain that hosts the
 *       PROTIMER is not powered without RAIL). The timestamp is the raw
 *       free-running CNT and the epoch is a software overflow counter, exactly
 *       as on Series 2; deep-sleep compensation is folded into CNT on wake, so
 *       the read is a bare register load with no software offset.
 *******************************************************************************
 * # License
 * <b>Copyright 2026 Silicon Laboratories Inc. www.silabs.com</b>
 *******************************************************************************
 *
 * SPDX-License-Identifier: Zlib
 *
 ******************************************************************************/

#ifndef SL_LOG_HAL_INLINE_H
#define SL_LOG_HAL_INLINE_H

#include "sl_component_catalog.h"
#include "sli_log_timer_timestamp.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* SLI_LOG_USE_PROTIMER (from sli_log_timer_timestamp.h) selects the timestamp
 * source: when RAIL is present it powers the radio PROTIMER and owns its RTC
 * sync across deep sleep, so that peripheral is the source and the logger
 * applies no software offset. Otherwise a general-purpose TIMERn is used (like
 * Series 2). */

#if SLI_LOG_USE_PROTIMER

#include "sl_log_protimer.h"

/* PROTIMER timestamp-counter rate (Hz), and the readiness gate for the readers
 * below: non-zero only once sli_log_protimer_prime_timebase() has derived the
 * rate, which happens at stack_init after RAIL has enabled the peripheral. Until
 * then the PROTIMER is unreachable - an access would stall the bus - so the gate
 * must be honoured before touching any register here. */
extern volatile uint32_t sli_log_protimer_raw_freq_hz;

/* Epoch latched with the most recent PROTIMER snapshot on this path. */
extern volatile uint32_t sli_log_protimer_latched_epoch;

/***************************************************************************//**
 * @brief Read timestamp and epoch for one log event.
 *
 * Returns zeros while the PROTIMER timebase is not primed
 * (sli_log_protimer_raw_freq_hz == 0), which covers events logged before RAIL
 * starts the peripheral. Otherwise reads LPRECNT once and returns LBASECNT and
 * LWRAPCNT from the same hardware snapshot.
 ******************************************************************************/
static inline void sli_log_hal_stamp_time(uint32_t *timestamp, uint32_t *epoch)
{
  if (sli_log_protimer_raw_freq_hz == 0U) {
    *timestamp = 0U;
    *epoch = 0U;
    sli_log_protimer_latched_epoch = 0U;
    return;
  }

  (void)PROTIMER->LPRECNT;
  *timestamp = PROTIMER->LBASECNT;
  *epoch = PROTIMER->LWRAPCNT;
  sli_log_protimer_latched_epoch = *epoch;
}

/***************************************************************************//**
 * @brief Read the low 32 bits of the 64-bit time (raw PROTIMER ticks).
 *
 * Returns 0 while the PROTIMER timebase is not primed. Otherwise reading
 * LPRECNT latches BASECNT/WRAPCNT for a coherent snapshot.
 ******************************************************************************/
static inline uint32_t sli_log_hal_get_timestamp(void)
{
  if (sli_log_protimer_raw_freq_hz == 0U) {
    sli_log_protimer_latched_epoch = 0U;
    return 0U;
  }

  (void)PROTIMER->LPRECNT;
  sli_log_protimer_latched_epoch = PROTIMER->LWRAPCNT;
  return PROTIMER->LBASECNT;
}

/***************************************************************************//**
 * @brief Read the epoch (high 32 bits of the 64-bit time).
 *
 * Returns the WRAPCNT value latched by the matching
 * sli_log_hal_get_timestamp() or sli_log_hal_stamp_time() call. Does not
 * re-read LPRECNT.
 ******************************************************************************/
static inline uint32_t sli_log_hal_get_epoch(void)
{
  return sli_log_protimer_latched_epoch;
}

#else /* SLI_LOG_USE_PROTIMER */

#include "sl_log_platform_core_config.h"
#include "em_device.h"

/* Resolve the configured TIMER instance (e.g. TIMER0) at compile time. */
#define SLI_LOG_TIMER_INSTANCE   TIMER(SL_LOG_CONFIG_TIMER_INSTANCE)

/* Software epoch defined in sli_log_timer_timestamp.c. Incremented on hardware
 * CNT overflow and when the wake-side sleep-offset push wraps CNT. */

/***************************************************************************//**
 * @brief Read timestamp and epoch for one log event.
 *
 * On the TIMERn path both fields come from CNT and the software epoch. An
 * overflow IRQ between the two loads can skew epoch by one at a wrap
 * boundary; that is accepted (no hardware latch).
 ******************************************************************************/
static inline void sli_log_hal_stamp_time(uint32_t *timestamp, uint32_t *epoch)
{
  *timestamp = SLI_LOG_TIMER_INSTANCE->CNT;
  *epoch = sli_log_timer_sw_epoch;
}

/***************************************************************************//**
 * @brief Read the low 32 bits of the 64-bit time (raw TIMERn ticks).
 *
 * Bare CNT register load. The host converts ticks to wall-clock time using
 * sl_log_hal_get_timestamp_timer_frequency().
 ******************************************************************************/
static inline uint32_t sli_log_hal_get_timestamp(void)
{
  return SLI_LOG_TIMER_INSTANCE->CNT;
}

/***************************************************************************//**
 * @brief Read the epoch (high 32 bits of the 64-bit time).
 *
 * Read immediately after sli_log_hal_get_timestamp() on the event-stamping
 * path. An overflow IRQ landing between the two reads can skew a single event's
 * epoch by one at a wrap boundary; the stored 32-bit count never goes
 * backwards, so this is accepted rather than guarded with a heavier latch.
 ******************************************************************************/
static inline uint32_t sli_log_hal_get_epoch(void)
{
  return sli_log_timer_sw_epoch;
}

#endif /* SLI_LOG_USE_PROTIMER */

#ifdef __cplusplus
}
#endif

#endif // SL_LOG_HAL_INLINE_H
