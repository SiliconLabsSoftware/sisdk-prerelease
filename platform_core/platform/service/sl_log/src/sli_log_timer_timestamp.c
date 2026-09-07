/***************************************************************************//**
 * @file
 * @brief Shared general-purpose TIMERn timestamp source for the logging core.
 *
 *   Drives a general-purpose TIMERn through the timer HAL, providing a
 *   free-running raw counter timestamp plus a software epoch (u32) that
 *   together form a 64-bit monotonic time. Timestamps are reported in raw
 *   counter ticks; the host converts to wall-clock time using the counter
 *   frequency from sli_log_timer_get_frequency(). Deep-sleep (EM2/EM3) time is
 *   folded straight into CNT on wake, so the epoch advances on TIMER overflow
 *   and when that wake-side CNT write wraps the counter - the per-event read
 *   stays a bare register load with no software offset.
 *
 *   This is the timestamp source on Series 2 (always) and on Series 3 when RAIL
 *   is absent. The two per-series platform files forward to the functions here
 *   so the implementation lives in exactly one place. On Series 3 with RAIL the
 *   radio PROTIMER is used instead, so this whole module compiles out
 *   (SLI_LOG_USE_PROTIMER == 1).
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
#include "sli_log_timer_timestamp.h"

#if !SLI_LOG_USE_PROTIMER

#include "sl_clock_manager.h"
#include "sl_log_internal.h"
#include "sl_log_platform_core_config.h"
#include "sl_interrupt_manager.h"
#include "sl_hal_timer.h"
#include "sl_device_peripheral.h"
#include "em_device.h"
#include <stdbool.h>

/*******************************************************************************
*******************************   DEFINES   ***********************************
*******************************************************************************/

/* Counter prescaler divisor (must match init_config.prescaler below).
 * The free-running counter ticks at (timer branch clock / TIMER_PRESCALER_DIV). */
#define TIMER_PRESCALER_DIV  8U

#define TIMER_TOP_VALUE      0xFFFFFFFF

/* Microseconds per second; used to convert an LF-measured sleep delta
 * (microseconds) into raw counter ticks for the offset accumulator. */
#define SL_LOG_US_PER_SEC    1000000ULL

#define _CONCAT_TWO_TOKENS(token_1, token_2)                     token_1 ## token_2
#define _CONCAT_THREE_TOKENS(token_1, token_2, token_3)          token_1 ## token_2 ## token_3
#define CONCAT_TWO_TOKENS(token_1, token_2)                      _CONCAT_TWO_TOKENS(token_1, token_2)
#define CONCAT_THREE_TOKENS(token_1, token_2, token_3)           _CONCAT_THREE_TOKENS(token_1, token_2, token_3)

#define TIMER_INSTANCE      TIMER(SL_LOG_CONFIG_TIMER_INSTANCE)
#define TIMER_BUS_CLOCK     CONCAT_TWO_TOKENS(SL_BUS_CLOCK_TIMER, SL_LOG_CONFIG_TIMER_INSTANCE)
#define LOGGER_TIMER_IRQ         CONCAT_THREE_TOKENS(TIMER, SL_LOG_CONFIG_TIMER_INSTANCE, _IRQn)
#define LOGGER_TIMER_IRQHandler  CONCAT_THREE_TOKENS(TIMER, SL_LOG_CONFIG_TIMER_INSTANCE, _IRQHandler)
#define LOGGER_TIMER_PERIPHERAL  CONCAT_TWO_TOKENS(SL_PERIPHERAL_TIMER, SL_LOG_CONFIG_TIMER_INSTANCE)

/*******************************************************************************
**************************   LOCAL VARIABLES   ********************************
*******************************************************************************/

/* Flag set when the timestamp timer has been initialized. */
static volatile bool timer_initialized;

/* Effective counter frequency in Hz (timer branch clock / TIMER_PRESCALER_DIV),
 * computed in init. This is the value reported by sli_log_timer_get_frequency()
 * so a host-side post-processing script can convert raw counter ticks to
 * wall-clock time. */
static uint32_t counter_freq_hz;

/* HF-TIMER raw tick reading captured at the most recent pre-sleep entry. Used
 * by the wake-side offset push to figure out how much of the LF-measured
 * sleep window was actually spent with the HF TIMER gated off (EM2) versus
 * running (EM1, e.g. while waiting for HFXO accuracy restore on the way out of
 * EM2). Only the EM2 portion needs to be back-filled into the counter; the EM1
 * portion was already counted natively by the HF TIMER's CNT advance. */
static volatile uint32_t pre_sleep_hw_ticks = 0;

/* Software epoch: high 32 bits of the 64-bit time. See the doxygen in
 * sli_log_timer_timestamp.h. Non-static and read directly by the logger core's
 * inline timestamp accessor (sl_log_send_* hot path). */
volatile uint32_t sli_log_timer_sw_epoch;

/*******************************************************************************
**************************   LOCAL FUNCTIONS   ********************************
*******************************************************************************/

/**
 * @brief Read the HF TIMER's current raw counter value (no offset).
 *
 * Bare register read of the free-running counter, used by the sleep
 * entry/exit path so the wake-side offset push can subtract the ticks the
 * HF TIMER advanced during the LF-measured sleep window (e.g. EM1 wait for
 * HFXO accuracy restore) instead of double-counting them. Returns 0 if the
 * timer hasn't been initialized yet.
 */
static uint32_t log_timer_read_hw_ticks(void)
{
  if (!timer_initialized) {
    return 0U;
  }
  return sl_hal_timer_get_counter(TIMER_INSTANCE);
}

/*******************************************************************************
*************************   GLOBAL FUNCTIONS   ********************************
*******************************************************************************/

sl_status_t sli_log_timer_init(void)
{
  sl_clock_branch_t clock_branch = sl_device_peripheral_get_clock_branch(LOGGER_TIMER_PERIPHERAL);
  sl_hal_timer_config_t init_config = SL_HAL_TIMER_CONFIG_DEFAULT;
  uint32_t log_timer_freq_hz = 0;
  sl_status_t status;

  sl_clock_manager_enable_bus_clock(TIMER_BUS_CLOCK);

  /* With prescaler DIV1 the free-running counter advanced at the full timer branch rate
   * so the 32-bit hardware counter wrapped in a short interval producing
   * confusing early timestamp resets in logs.
   * So we use DIV8 to slow the counter by 8x so the wrap occurs later.
   */
  init_config.prescaler = SL_HAL_TIMER_PRESCALER_DIV8;

  sl_hal_timer_init(TIMER_INSTANCE, &init_config);
  sl_hal_timer_enable(TIMER_INSTANCE);
  sl_hal_timer_set_top(TIMER_INSTANCE, TIMER_TOP_VALUE);
  sl_hal_timer_start(TIMER_INSTANCE);
  sl_hal_timer_clear_interrupts(TIMER_INSTANCE, TIMER_IEN_OF);
  sl_hal_timer_enable_interrupts(TIMER_INSTANCE, TIMER_IEN_OF);

  status = sl_clock_manager_get_clock_branch_frequency(clock_branch, &log_timer_freq_hz);
  if (status) {
      return status;
  }

  /* log_timer_freq_hz is the timer branch clock (before prescaler). The
   * free-running counter ticks at freq / TIMER_PRESCALER_DIV; that effective
   * rate is what sli_log_timer_get_count() returns ticks at, and what the host
   * post-processing script must use to convert ticks to wall-clock time.
   */
  counter_freq_hz = log_timer_freq_hz / TIMER_PRESCALER_DIV;

  sl_interrupt_manager_clear_irq_pending(LOGGER_TIMER_IRQ);
  sl_interrupt_manager_enable_irq(LOGGER_TIMER_IRQ);

  timer_initialized = true;
  return SL_STATUS_OK;
}

sl_status_t sli_log_timer_deinit(void)
{
  sl_hal_timer_stop(TIMER_INSTANCE);
  return SL_STATUS_OK;
}

uint32_t sli_log_timer_get_count(void)
{
  if (!timer_initialized) {
    return 0U;
  }

  return TIMER_INSTANCE->CNT;
}

uint32_t sli_log_timer_get_epoch(void)
{
  return sli_log_timer_sw_epoch;
}

uint32_t sli_log_timer_get_frequency(void)
{
  return counter_freq_hz;
}

void sli_log_timer_pre_sleep_snapshot(void)
{
  /* Snapshot the HF TIMER reading so the wake-side offset push can tell
   * apart the EM2 portion of the sleep window (HF gated) from any EM1
   * portion (HF still ticking, e.g. while the power_manager waits for
   * HFXO accuracy on the way out). See sli_log_platform_add_sleep_offset_us.
   *
   * Reading the timer here is safe even though the caller runs in the
   * power_manager critical section: sl_hal_timer_get_counter is a bare
   * register load with no SYNCBUSY busy-wait. */
  pre_sleep_hw_ticks = log_timer_read_hw_ticks();
}

/**
 * @brief Push the elapsed time of an LF-measured sleep window into the
 *        platform timestamp counter, compensating for any HF-TIMER advance
 *        that already occurred during the same window.
 *
 * @param[in] delta_us Microseconds elapsed during the most recent sleep,
 *                     measured by the integration layer against a
 *                     low-power-domain time source (e.g. sleeptimer).
 *                     64-bit so a sleep longer than 2^32 us still
 *                     advances the software epoch.
 *
 * The HF TIMER is gated while the device is in EM2/EM3 but keeps ticking
 * in EM0/EM1 - including the EM1-while-waiting-for-HFXO-accuracy stretch
 * the power_manager runs after waking from EM2. The offset push therefore
 * credits only the *gated* portion of the LF window. Working entirely in raw
 * counter ticks:
 *
 *     delta_ticks   = delta_us * counter_freq_hz / 1e6
 *     em_gated_ticks = delta_ticks - (post_hw_ticks - pre_hw_ticks)
 *
 * where @c pre_hw_ticks was sampled by sli_log_timer_pre_sleep_snapshot() at
 * the matching ENTERING_EM2 callback. A clamp at zero protects against the
 * (rare) case where measurement noise makes the HF advance look slightly
 * larger than the LF window.
 *
 * The @c em_gated_ticks advance is folded directly into the hardware CNT
 * (rather than kept as a software offset added on every read), so the
 * sli_log_timer_get_count() hot path stays a bare register load. When the new
 * time wraps past the 32-bit CNT boundary the write produces the same effect a
 * hardware overflow would, so @c sli_log_timer_sw_epoch takes one increment per
 * boundary crossed - a sleep spanning several counter periods advances it by
 * several epochs - to keep the 64-bit time monotonic.
 *
 * Caller runs in the power-manager critical section, but on the *wake* side:
 * the timer clock branch has been restored, so the SYNCBUSY busy-wait inside
 * sl_hal_timer_set_counter() clears normally and cannot deadlock. The overflow
 * IRQ is masked by the PM critical section, so the read-modify-write of CNT and
 * the epoch bump are atomic with respect to LOGGER_TIMER_IRQHandler.
 */
void sli_log_platform_add_sleep_offset_us(uint64_t delta_us)
{
  /* Convert the LF-measured microsecond window into raw counter ticks so it
   * accumulates in the same units the timestamp counter reports. A sleep can
   * outlast one 32-bit counter period, so the tick count stays 64-bit. */
  uint64_t delta_ticks = ((uint64_t)delta_us
                          * (uint64_t)counter_freq_hz)
                         / SL_LOG_US_PER_SEC;

  uint32_t cur_cnt        = log_timer_read_hw_ticks();
  uint32_t hw_advance     = cur_cnt - pre_sleep_hw_ticks;   /* wrap-safe */
  uint64_t em_gated_ticks = (delta_ticks > (uint64_t)hw_advance)
                            ? (delta_ticks - (uint64_t)hw_advance) : 0ULL;
  uint64_t new_time       = (uint64_t)cur_cnt + em_gated_ticks;

  /* Folding the sleep advance into CNT can push it past its 32-bit wrap, once
   * per counter period spent asleep; those crossings are real epoch
   * increments the overflow IRQ never saw. */
  sli_log_timer_sw_epoch += (uint32_t)(new_time >> 32);

  sl_hal_timer_set_counter(TIMER_INSTANCE, (uint32_t)new_time);
}

/*******************************************************************************
 * TIMER interrupt handler.
 ******************************************************************************/
void LOGGER_TIMER_IRQHandler(void)
{
  uint32_t irq_flag = sl_hal_timer_get_pending_interrupts(TIMER_INSTANCE);

  if (irq_flag & TIMER_IEN_OF) {
    sl_hal_timer_clear_interrupts(TIMER_INSTANCE, irq_flag & TIMER_IEN_OF);
    sli_log_timer_sw_epoch++;
  }
}

#else /* SLI_LOG_USE_PROTIMER */

/* On the PROTIMER timestamp path (Series 3 with RAIL) the general-purpose
 * TIMER source is unused and this module compiles to nothing. Provide a single
 * typedef so the translation unit is never empty. */
typedef int sli_log_timer_timestamp_unused_t;

#endif /* !SLI_LOG_USE_PROTIMER */
