/***************************************************************************//**
 * @file
 * @brief Inlined timestamp accessors for the logging core (Series 2).
 *
 *   The logger core (sl_log.c) stamps every event with a timestamp and an
 *   epoch on the hot path. Routing those reads through the platform
 *   api_core function pointers adds an indirect call and a struct
 *   dereference per event. To keep the hot path as cheap as a couple of
 *   register loads while keeping sl_log.c free of any series-specific code,
 *   the core instead calls the fixed-name static-inline accessors declared
 *   here, and the build selects the matching per-series implementation of
 *   this header via the include path (see log_platform_specific.slcc).
 *
 *   Series 2: the timestamp is the raw free-running TIMERn CNT and the epoch
 *   is a software overflow counter. Deep-sleep compensation is folded into
 *   CNT on wake (see sli_log_platform_add_sleep_offset_us in
 *   sl_log_platform_specific_s2.c), so the read is a bare register load with
 *   no software offset.
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

#include "sl_log_platform_core_config.h"
#include "sli_log_timer_timestamp.h"
#include "em_device.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Series 2 always uses the general-purpose TIMERn timestamp source (never the
 * radio PROTIMER), including when RAIL is present in the project - so
 * SLI_LOG_USE_PROTIMER (from sli_log_timer_timestamp.h) is always 0 here. */

/* Resolve the configured TIMER instance (e.g. TIMER0) at compile time. */
#define SLI_LOG_TIMER_INSTANCE   TIMER(SL_LOG_CONFIG_TIMER_INSTANCE)

/* Software epoch defined in sli_log_timer_timestamp.c. Incremented on hardware
 * CNT overflow and when the wake-side sleep-offset push wraps CNT. */

/***************************************************************************//**
 * @brief Read timestamp and epoch for one log event.
 *
 * On Series 2 both fields come from TIMERn CNT and the software epoch. An
 * overflow IRQ between the two loads can skew epoch by one at a wrap boundary;
 * that is accepted on this path (no hardware latch).
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
 * path. An overflow IRQ landing between the two reads can skew a single
 * event's epoch by one at a wrap boundary; the stored 32-bit count never goes
 * backwards, so this is accepted rather than guarded with a heavier latch.
 ******************************************************************************/
static inline uint32_t sli_log_hal_get_epoch(void)
{
  return sli_log_timer_sw_epoch;
}

#ifdef __cplusplus
}
#endif

#endif // SL_LOG_HAL_INLINE_H
