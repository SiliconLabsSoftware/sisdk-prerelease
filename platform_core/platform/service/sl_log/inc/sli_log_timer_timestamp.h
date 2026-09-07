/***************************************************************************//**
 * @file
 * @brief Shared general-purpose TIMERn timestamp source for the logging core.
 *
 *   The logger's timestamp source is a general-purpose TIMERn on Series 2
 *   (always) and on Series 3 when RAIL is absent. Both cases share the exact
 *   same free-running-counter + software-epoch implementation; that code lives
 *   in sli_log_timer_timestamp.c and is exposed here so the per-series
 *   sl_log_platform_specific_s{2,3}.c files can forward to it instead of each
 *   carrying their own copy.
 *
 *   On Series 3 with RAIL the radio PROTIMER is the timestamp source instead,
 *   so this module compiles to nothing there (@ref SLI_LOG_USE_PROTIMER == 1).
 *******************************************************************************
 * # License
 * <b>Copyright 2026 Silicon Laboratories Inc. www.silabs.com</b>
 *******************************************************************************
 *
 * SPDX-License-Identifier: Zlib
 *
 ******************************************************************************/

#ifndef SLI_LOG_TIMER_TIMESTAMP_H
#define SLI_LOG_TIMER_TIMESTAMP_H

#include "sl_component_catalog.h"
#include "sl_status.h"
#include "em_device.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Whether the logger uses the radio PROTIMER as its timestamp source.
 *
 *   Single source of truth for the timestamp-source selector, shared by the
 *   per-series platform files (sl_log_platform_specific_s{2,3}.c), the inlined
 *   hot-path accessors (inc/s{2,3}/sl_log_hal_inline.h) and the power-manager
 *   glue (sl_log_power_manager.c).
 *
 *   The PROTIMER is a shared radio peripheral that only powers up as part of
 *   the radio subsystem. It is reachable only on Series 3 with the RAIL library
 *   in the image (RAIL contributes @c SL_CATALOG_RAIL_LIB_PRESENT and keeps the
 *   PROTIMER RTC-synced across deep sleep). In every other configuration - all
 *   of Series 2, and Series 3 without RAIL - the radio domain is not brought up
 *   and a general-purpose TIMERn is used instead.
 */
#if defined(_SILICON_LABS_32B_SERIES_3) && defined(SL_CATALOG_RAIL_LIB_PRESENT)
#define SLI_LOG_USE_PROTIMER  1
#else
#define SLI_LOG_USE_PROTIMER  0
#endif

#if SLI_LOG_USE_PROTIMER

/**
 * @brief Prime the PROTIMER timebase once RAIL has brought the peripheral up.
 *
 * Registered as a stack_init handler ordered after RAIL's own init, which is the
 * earliest point the PROTIMER is both enabled and reachable: the logger's
 * sl_log_init_stage2() runs back at service_init. Deriving the tick rate is what
 * makes every timestamp reader go live - the inlined hot-path accessors in
 * sl_log_hal_inline.h return zeros until then - so this must run even in builds
 * that never call sl_log_get_timestamp_count().
 */
void sli_log_protimer_prime_timebase(void);

#else /* SLI_LOG_USE_PROTIMER */

/**
 * @brief Software epoch: high 32 bits of the 64-bit time.
 *
 * Incremented by the TIMER overflow IRQ and by the wake-side sleep-offset push
 * when crediting the deep-sleep window advances CNT past its 32-bit wrap.
 * Declared here (and defined in sli_log_timer_timestamp.c) so the logger core's
 * inline hot-path accessor can read it directly.
 */
extern volatile uint32_t sli_log_timer_sw_epoch;

/**
 * @brief Initialize and start the general-purpose TIMERn timestamp counter.
 * @return SL_STATUS_OK on success or an sl_status_t error code.
 */
sl_status_t sli_log_timer_init(void);

/**
 * @brief Stop the TIMERn timestamp counter.
 * @return SL_STATUS_OK on success or an sl_status_t error code.
 */
sl_status_t sli_log_timer_deinit(void);

/**
 * @brief Read the low 32 bits of the 64-bit time (raw TIMERn ticks).
 * @return Free-running counter value, or 0 if not yet initialized.
 */
uint32_t sli_log_timer_get_count(void);

/**
 * @brief Read the epoch (high 32 bits of the 64-bit time).
 * @return Current software epoch value.
 */
uint32_t sli_log_timer_get_epoch(void);

/**
 * @brief Get the effective counter frequency in Hz.
 * @return Counter frequency (timer branch clock / prescaler), or 0 if the
 *         timer is not yet initialized.
 */
uint32_t sli_log_timer_get_frequency(void);

/**
 * @brief Snapshot the current raw TIMER ticks at sleep entry.
 *
 * Sampled by the pre-sleep handler so the wake-side offset push
 * (@ref sli_log_platform_add_sleep_offset_us) can tell the EM2/EM3-gated
 * portion of the sleep window apart from any portion where the HF TIMER kept
 * running (e.g. EM1 while waiting for HFXO accuracy restore).
 */
void sli_log_timer_pre_sleep_snapshot(void);

#endif /* SLI_LOG_USE_PROTIMER */

#ifdef __cplusplus
}
#endif

#endif // SLI_LOG_TIMER_TIMESTAMP_H
