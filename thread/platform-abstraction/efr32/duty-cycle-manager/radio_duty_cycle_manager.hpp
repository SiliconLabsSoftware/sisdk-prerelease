/*******************************************************************************
 * @file
 * @brief Internal duty-cycle policy state.
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

#ifndef RADIO_DUTY_CYCLE_MANAGER_HPP
#define RADIO_DUTY_CYCLE_MANAGER_HPP

#include <stdbool.h>
#include <stdint.h>

#include "radio_duty_cycle_manager.h"
#include "sl_sleeptimer.h"
#include "sl_status.h"

/**
 * @addtogroup sli_ot_radio_duty_cycle
 * @{
 *
 * @brief
 *   File-scope duty-cycle policy state.
 *
 * This class owns the rolling TX-time buckets, the current alert band, and
 * the periodic sleeptimer that rotates those buckets. Thresholds are
 * compile-time constants.
 *
 * The array holds @ref SL_OT_RADIO_TX_TIME_BUCKET_COUNT_DEFAULT slots: the
 * buckets that make up the rolling window plus one spare. @ref switch_bucket
 * rotates into the spare and clears it. @ref get_window_us walks
 * @ref SL_OT_RADIO_TX_TIME_BUCKETS_IN_WINDOW_DEFAULT slots back from the
 * current bucket and therefore never includes stale time. Recording happens
 * on the TX path while rotation happens in the sleeptimer interrupt, so every
 * method that touches the array runs its accesses inside a critical section.
 *
 * Outbound ACK TX state is a separate PAL block at the bottom of this class.
 * Remove that block when duty-cycle accounting moves to the stack.
 */

class RadioDutyCycleManager
{
    // SuccessOrExit() treats any non-zero status as failure.
    static_assert(SL_STATUS_OK == 0, "SuccessOrExit requires SL_STATUS_OK to be 0");

public:
    /**
     * Store @p config callbacks, start the accountant, and start the bucket timer.
     *
     * @retval SL_STATUS_OK                  Success.
     * @retval SL_STATUS_INVALID_PARAMETER   @p config is NULL.
     * @retval SL_STATUS_ALREADY_INITIALIZED Already initialized.
     */
    sl_status_t init(const sl_ot_duty_cycle_config_t *config);

    /**
     * Stop the bucket timer, release the accountant, and clear all state.
     *
     * @retval SL_STATUS_OK               Success.
     * @retval SL_STATUS_NOT_INITIALIZED  Module was not initialized.
     */
    sl_status_t deinit(void);

    /**
     * Decide whether a frame of @p psdu_len bytes fits the remaining budget.
     *
     * @retval SL_STATUS_OK               Frame may be transmitted.
     * @retval SL_STATUS_FAIL             Estimate exceeds the remaining budget.
     * @retval SL_STATUS_NOT_INITIALIZED  Module was not initialized.
     */
    sl_status_t check_tx(uint8_t psdu_len);

    /**
     * Charge a completed transmission of @p psdu_len bytes and re-evaluate.
     *
     * @retval SL_STATUS_OK               Success.
     * @retval SL_STATUS_NOT_INITIALIZED  Module was not initialized.
     */
    sl_status_t record_tx_complete(uint8_t psdu_len);

    /**
     * Fill @p status with usage, budgets, and the current alert level.
     *
     * @retval SL_STATUS_OK                Success.
     * @retval SL_STATUS_INVALID_PARAMETER @p status is NULL.
     * @retval SL_STATUS_NOT_INITIALIZED   Module was not initialized.
     */
    sl_status_t get_status(sl_ot_duty_cycle_status_t *status) const;

    /// Rotate the accounting bucket and re-evaluate. Runs in timer context.
    void handle_bucket_timer(void);

    /**
     * Copy every bucket total and the current index into @p buckets.
     *
     * @retval SL_STATUS_OK                  Success.
     * @retval SL_STATUS_INVALID_PARAMETER   @p buckets is NULL.
     * @retval SL_STATUS_NOT_INITIALIZED     Module was not initialized.
     */
    sl_status_t get_buckets(sl_ot_duty_cycle_buckets_t *buckets) const;

    // PAL outbound ACK TX accounting. Remove with the matching .cpp block
    // when duty-cycle accounting moves to the stack.

    /// Clear the prepared ACK and any ACK time waiting for the main loop.
    void reset_ack_tx(void);

    /// Remember @p ack_psdu_len. A sent ACK not yet recorded is kept.
    void prepare_ack_tx(uint8_t ack_psdu_len);

    /// Mark the prepared ACK as sent.
    void notify_ack_tx_transmitted(void);

    /// Drop the prepared ACK. Already-sent time is kept.
    void discard_ack_tx(void);

    /// Record sent ACK airtime into the shared buckets.
    void process_pending_ack_tx(void);

    /// Return whether sent ACK airtime is waiting for the main loop.
    bool is_ack_tx_pending(void) const;

#ifdef TESTING
    /// Charge raw microseconds without re-evaluating alert bands.
    sl_status_t record_us_for_test(uint32_t duration_us);
#endif

private:
    /// TX time in the current rolling window.
    struct TxWindowSnapshot
    {
        uint32_t total_tx_us;
        uint32_t window_duration_us;
        uint32_t current_period_tx_us;
    };

    /// Zero the bucket array. Does not change @c initialized.
    void clear_buckets(void);

    /// Add @p duration_us to the current bucket, saturating at @c UINT32_MAX.
    sl_status_t record_bucket_us(uint32_t duration_us);

    /// Advance to the next slot and clear it.
    void switch_bucket(uint64_t now_us);

    /// Summed TX time over the buckets in the window, or 0 if not initialized.
    uint32_t get_window_us(void) const;

    /// TX time in the current bucket, or 0 if not initialized.
    uint32_t get_current_period_us(void) const;

    /// Fill @p snapshot from the bucket array.
    sl_status_t get_tx_snapshot(TxWindowSnapshot *snapshot) const;

    /// Compact debug line for window usage.
    void log_tx_window(uint32_t charged_us, uint8_t bucket_index, uint32_t bucket_us, uint32_t window_tx_us) const;

    /// Copy every bucket total and the current index.
    sl_status_t copy_buckets(uint32_t *bucket_tx_us, uint8_t capacity, uint8_t *out_count, uint8_t *current) const;

    /**
     * Derive budgets and usage from the accountant snapshot.
     *
     * @param[out] status  Caller-provided status (must be non-NULL).
     */
    sl_status_t fill_status(sl_ot_duty_cycle_status_t *status) const;

    /**
     * Recompute the alert band and fire the callback if the band increased.
     *
     * @param[out] status  Optional; receives the status used for the decision,
     *                     so callers can reuse it instead of re-snapshotting.
     */
    void evaluate_thresholds(sl_ot_duty_cycle_status_t *status = nullptr);

    /// Charge completed on-air time and re-evaluate the alert level.
    sl_status_t record_tx_us(uint32_t duration_us);

    sl_sleeptimer_timer_handle_t   tx_time_timer;
    sl_ot_duty_cycle_config_t      config;
    sl_ot_duty_cycle_alert_level_t level;
    bool                           initialized                                            = false;
    uint32_t                       bucket_tx_us[SL_OT_RADIO_TX_TIME_BUCKET_COUNT_DEFAULT] = {};
    uint8_t                        current_bucket                                         = 0;
    uint64_t                       last_bucket_boundary_us                                = 0;

    // PAL outbound ACK TX accounting. One ACK is prepared at a time.
    // ack_tx_accumulated_us holds a sent ACK whose prepare was replaced
    // before the main loop recorded it.
    uint8_t  ack_tx_psdu_len       = 0;
    bool     ack_tx_sent           = false;
    uint32_t ack_tx_accumulated_us = 0;
};

/** @} (end addtogroup sli_ot_radio_duty_cycle) */

#ifdef TESTING
namespace Testing {
namespace Radio {
namespace DutyCycleTest {
/// Fire one bucket-period expiry (advances test time by one bucket, then rotates).
void AdvanceBucketTimer(void);
/// Fire @p count successive bucket-period expiries.
void AdvanceBucketTimers(unsigned count);
/// Charge raw microseconds without re-evaluating alert bands.
sl_status_t RecordTxUs(uint32_t duration_us);
} // namespace DutyCycleTest
} // namespace Radio
} // namespace Testing
#endif

#endif // RADIO_DUTY_CYCLE_MANAGER_HPP
