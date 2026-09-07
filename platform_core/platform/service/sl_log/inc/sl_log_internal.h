/***************************************************************************//**
 * @file
 * @brief Internal logger APIs shared between the core, the platform-specific
 *        layer, and optional integration components (e.g. power manager).
 *******************************************************************************
 * # License
 * <b>Copyright 2026 Silicon Laboratories Inc. www.silabs.com</b>
 *******************************************************************************
 *
 * SPDX-License-Identifier: Zlib
 *
 ******************************************************************************/

#ifndef SL_LOG_INTERNAL_H
#define SL_LOG_INTERNAL_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Mark the logger as suspended or resumed.
 *
 * While suspended, every @ref sl_log_send_* call returns without producing an
 * event. This is intended to be used by integration code that knows the
 * timestamp source and/or the backend transport are temporarily unavailable
 * (typically across EM2/EM3 sleep).
 *
 * The suspension is silent: no synthetic "logs dropped" event is generated on
 * resume. The user-configured runtime log level is preserved across the
 * suspend/resume cycle.
 *
 * @param[in] suspended  true to drop subsequent log events, false to resume.
 *
 * @note Safe to call from a critical section / power-manager EM transition
 *       callback. The flag is a single volatile bool, so writes are atomic on
 *       Cortex-M.
 */
void sli_log_set_suspended(bool suspended);

/**
 * @brief Add a software offset (in microseconds) to every subsequent
 *        timestamp returned by the platform timer.
 *
 * Called by integration code on wake-up after measuring the elapsed sleep
 * time from a low-power-domain time source (e.g. via sleeptimer). The
 * offset is accumulated, so multiple sleep cycles compose naturally.
 *
 * @param[in] delta_us  Microseconds elapsed during the most recent sleep,
 *                      as measured against a low-power-domain time source.
 *                      Full 64-bit span so multi-hour EM2/EM3 still advances
 *                      the software epoch; do not truncate to 32 bits.
 *
 * @note On Series 2, and on Series 3 without RAIL, the timestamp source is a
 *       general-purpose TIMERn whose CNT register is retained across sleep but
 *       pauses while its clock branch is gated. The implementation therefore
 *       credits only the EM2/EM3-gated portion of @p delta_us into CNT; any
 *       TIMER advance during the same window (for example while returning
 *       through EM1) was already counted natively. Sleeps that cross one or
 *       more 32-bit CNT periods increment the software epoch accordingly. On
 *       Series 3 with RAIL, RAIL keeps the shared PROTIMER synchronized across
 *       sleep, so this offset path is not used.
 */
void sli_log_platform_add_sleep_offset_us(uint64_t delta_us);

/**
 * @brief Convert a pre/post-sleep pair of LF tick counter readings to
 *        a microsecond delta in a wrap-safe way.
 *
 * Internal helper extracted from the power-manager glue so the wrap
 * arithmetic can be unit-tested directly. Two overflow layers are
 * handled:
 *
 *   1. @p now_ticks - @p pre_ticks is computed in @c uint32_t modular
 *      arithmetic - correct for any sleep window shorter than the LF
 *      counter's full wrap period (~36 h at 32768 Hz LFXO/LFRCO,
 *      ~49 days at 1 kHz ULFRCO).
 *   2. The tick * 1e6 product and the returned span stay @c uint64_t so
 *      a sleep longer than 2^32 microseconds (~71.6 minutes) still
 *      composes with @ref sli_log_platform_add_sleep_offset_us and
 *      advances the 64-bit time (CNT + software epoch).
 *
 * @param[in] pre_ticks  Tick counter sampled at sleep entry.
 * @param[in] now_ticks  Tick counter sampled at sleep exit.
 * @param[in] freq_hz    LF tick frequency in Hz. If zero, the function
 *                       returns 0 (no time-base available).
 *
 * @return Elapsed time in microseconds (64-bit, not truncated).
 */
uint64_t sli_log_pm_lf_ticks_to_us(uint32_t pre_ticks,
                                   uint32_t now_ticks,
                                   uint32_t freq_hz);

#ifdef __cplusplus
}
#endif

#endif // SL_LOG_INTERNAL_H
