/***************************************************************************/ /**
 * @file
 * @brief Test-only control API for the mock CPC timer.
 *
 * The mock CPC timer implementation does not advance time on its own.
 * Tests use the APIs declared here to step the virtual clock, fire pending
 * timers, and inspect the active timer queue.
 ******************************************************************************/

#ifndef MOCK_CPC_TIMER_H
#define MOCK_CPC_TIMER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Reset the mock to a pristine state.
 *
 * Clears all queued timers (without firing them), zeroes the virtual clock
 * and restores the default frequency. Typically called from `TEST_SETUP`.
 */
void mock_cpc_timer_reset(void);

/**
 * @brief Override the tick frequency reported by the mock.
 *
 * Defaults to 32768 Hz. Affects `sli_cpc_timer_get_timer_frequency()` and the
 * millisecond <-> tick conversions.
 *
 * @param freq_hz Frequency in hertz. Must be non-zero.
 */
void mock_cpc_timer_set_frequency(uint32_t freq_hz);

/**
 * @brief Return the current virtual tick value.
 */
uint64_t mock_cpc_timer_get_tick_count(void);

/**
 * @brief Return how many timers are currently scheduled.
 */
size_t mock_cpc_timer_count_running(void);

/**
 * @brief True if at least one timer is currently scheduled.
 */
bool mock_cpc_timer_has_pending(void);

/**
 * @brief Return the absolute expiration tick of the next timer to fire.
 *
 * @param[out] tick Receives the expiration tick. May be NULL.
 * @return true if a timer is scheduled and `tick` was populated, false
 *         otherwise.
 */
bool mock_cpc_timer_next_expiration_tick(uint64_t *tick);

/**
 * @brief Advance the virtual clock by `delta_ticks` ticks.
 *
 * Any timer whose expiration falls within the new clock value fires, in
 * order of expiration. Callbacks may schedule new timers; those new timers
 * are honored only if their expiration is still within the new clock window.
 *
 * @return Number of timers that fired during the advance.
 */
size_t mock_cpc_timer_advance_ticks(uint64_t delta_ticks);

/**
 * @brief Advance the virtual clock by `delta_ms` milliseconds.
 *
 * Convenience wrapper around `mock_cpc_timer_advance_ticks()` using the
 * mock's current frequency.
 *
 * @return Number of timers that fired during the advance.
 */
size_t mock_cpc_timer_advance_ms(uint32_t delta_ms);

/**
 * @brief Advance the clock just enough to fire the next pending timer.
 *
 * If no timer is scheduled, returns false and the clock is untouched.
 *
 * @return true if a timer was fired, false otherwise.
 */
bool mock_cpc_timer_fire_next(void);

/**
 * @brief Fire every currently scheduled timer.
 *
 * Walks the queue in expiration order, advancing the virtual clock to each
 * expiration tick and invoking the corresponding callback. Newly scheduled
 * timers added by callbacks are NOT fired by this call unless their
 * expiration falls before the latest already-processed expiration.
 *
 * @return Number of timers fired.
 */
size_t mock_cpc_timer_fire_all_pending(void);

#ifdef __cplusplus
}
#endif

#endif // MOCK_CPC_TIMER_H
