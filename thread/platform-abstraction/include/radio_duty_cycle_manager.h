/*******************************************************************************
 * @file
 * @brief Sub-GHz radio duty-cycle policy and consumer API.
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

#ifndef SL_OT_RADIO_DUTY_CYCLE_H
#define SL_OT_RADIO_DUTY_CYCLE_H

#include <stdbool.h>
#include <stdint.h>

/// Rolling window length in seconds. Fixed at one hour and not a configuration option.
#ifdef SL_OT_RADIO_TX_TIME_WINDOW_SEC_DEFAULT
#undef SL_OT_RADIO_TX_TIME_WINDOW_SEC_DEFAULT
#endif
#define SL_OT_RADIO_TX_TIME_WINDOW_SEC_DEFAULT (3600u)

#include "sl_openthread_subghz_config.h"
#include "sl_status.h"

#ifdef __cplusplus
/// Hour cap in permille, derived from @ref SL_OT_DUTY_CYCLE_MAX_PERCENT_DEFAULT.
/// One percent is 10 permille. The cast rounds down, so the cap stays at or
/// under the configured percent. The default 2.8% is 28.
constexpr uint16_t max_duty_cycle_permille = static_cast<uint16_t>(SL_OT_DUTY_CYCLE_MAX_PERCENT_DEFAULT * 10.0);

static_assert(max_duty_cycle_permille >= 1 && max_duty_cycle_permille <= 1000,
              "SL_OT_DUTY_CYCLE_MAX_PERCENT_DEFAULT must be from 0.1 to 100");

extern "C" {
#endif

/**
 * @addtogroup sl_ot_radio_duty_cycle
 * @{
 *
 * @brief
 *   Sub-GHz TX duty-cycle policy (default 2.8% per rolling hour).
 *
 * Radio PAL uses only this public API: init/deinit, an alert callback supplied
 * through @ref sl_ot_duty_cycle_config_t (once, at init), @ref sl_ot_duty_cycle_check_tx
 * before transmit, and @ref sl_ot_duty_cycle_record_tx_complete after transmit.
 * The hour cap is the configured percent. Alert bands are compile-time
 * percentages of that budget and cannot be changed at runtime.
 *
 * @c check_tx denies a frame only when its estimated on-air time would exceed
 * remaining budget. MEDIUM, WARNING, and HIGH still allow transmission. The
 * alert callback fires once when the band increases; @c status->level is the
 * landing band, so a jump is reported once and skipped bands are not. Drops
 * do not fire.
 */

/// Default medium: 50% of the TX budget (not 50% of the hour).
#define SL_OT_DUTY_CYCLE_MEDIUM_PERCENT_DEFAULT (50u)

/// Default warning: 75% of the TX budget (not 75% of the hour).
#define SL_OT_DUTY_CYCLE_WARNING_PERCENT_DEFAULT (75u)

/// Default high: 90% of the TX budget (not 90% of the hour). TX is still allowed until 100%.
#define SL_OT_DUTY_CYCLE_HIGH_PERCENT_DEFAULT (90u)

/// Alert level derived from TX usage versus configured budget.
typedef enum
{
    SL_OT_DUTY_CYCLE_ALERT_LEVEL_NONE = 0,  ///< Module not initialized.
    SL_OT_DUTY_CYCLE_ALERT_LEVEL_LOW,       ///< Below 50% of the TX budget.
    SL_OT_DUTY_CYCLE_ALERT_LEVEL_MEDIUM,    ///< At or above 50% of budget, below 75%.
    SL_OT_DUTY_CYCLE_ALERT_LEVEL_WARNING,   ///< At or above 75% of budget, below 90%.
    SL_OT_DUTY_CYCLE_ALERT_LEVEL_HIGH,      ///< At or above 90% of the TX budget, below 100%.
    SL_OT_DUTY_CYCLE_ALERT_LEVEL_EXHAUSTED, ///< At or above 100% of the TX budget.
} sl_ot_duty_cycle_alert_level_t;

/// Consumer-visible duty-cycle status.
typedef struct sl_ot_duty_cycle_status
{
    uint32_t total_tx_us;                 ///< TX time in the rolling window (µs).
    uint32_t current_period_tx_us;        ///< TX time in the current bucket (µs).
    uint32_t max_budget_us;               ///< Allowed TX time in the window (µs).
    uint32_t remaining_budget_us;         ///< max_budget_us minus total_tx_us, clamped at 0.
    uint16_t duty_permille;               ///< Current usage in parts per thousand of the window.
    uint16_t max_duty_permille;           ///< Configured hour cap in permille (@ref max_duty_cycle_permille).
    sl_ot_duty_cycle_alert_level_t level; ///< Current alert level.
} sl_ot_duty_cycle_status_t;

/**
 * Alert callback invoked when usage moves into a higher band.
 *
 * Runs in the context that crossed the threshold, which may be the sleeptimer
 * interrupt that rotated the bucket. Keep the callback short and do not call
 * back into this module from it. Inspect @p status->level for MEDIUM, WARNING,
 * HIGH, or EXHAUSTED.
 *
 * @param[in] status   Status at the time of the alert.
 * @param[in] context  Matching context field from @ref sl_ot_duty_cycle_config_t.
 */
typedef void (*sl_ot_duty_cycle_alert_callback_t)(const sl_ot_duty_cycle_status_t *status, void *context);

/// Alert callback registered once at init. Thresholds are compile-time constants.
typedef struct sl_ot_duty_cycle_config
{
    sl_ot_duty_cycle_alert_callback_t on_alert;         ///< Fired when the alert band increases. May be NULL.
    void                             *on_alert_context; ///< Context for @c on_alert.
} sl_ot_duty_cycle_config_t;

/**
 * Initialize the duty-cycle module and TX-time accountant, and start the
 * bucket-period timer.
 *
 * @param[in] config  Configuration (must be non-NULL). @c on_alert may be NULL.
 *
 * @retval SL_STATUS_OK                  Success.
 * @retval SL_STATUS_INVALID_PARAMETER   @p config is NULL.
 * @retval SL_STATUS_ALREADY_INITIALIZED Already initialized.
 */
sl_status_t sl_ot_duty_cycle_init(const sl_ot_duty_cycle_config_t *config);

/**
 * Stop the bucket timer and release the TX-time accountant.
 *
 * @retval SL_STATUS_OK               Success.
 * @retval SL_STATUS_NOT_INITIALIZED  Module was not initialized.
 */
sl_status_t sl_ot_duty_cycle_deinit(void);

/**
 * Check whether a frame of @p psdu_len bytes fits in the remaining TX budget.
 *
 * Uses the same on-air time estimate as @ref sl_ot_duty_cycle_record_tx_complete.
 * MEDIUM, WARNING, and HIGH still allow the frame; it is denied only when the
 * estimate does not fit in remaining budget. This does not record TX time.
 *
 * @param[in] psdu_len  MAC PSDU length in bytes.
 *
 * @retval SL_STATUS_OK               Frame may be transmitted.
 * @retval SL_STATUS_FAIL             Estimated duration exceeds remaining budget.
 * @retval SL_STATUS_NOT_INITIALIZED  Module was not initialized.
 */
sl_status_t sl_ot_duty_cycle_check_tx(uint8_t psdu_len);

/**
 * Record a completed transmission of @p psdu_len bytes.
 *
 * Converts length to microseconds with the same estimator as
 * @ref sl_ot_duty_cycle_check_tx, then updates accounting and alert level.
 *
 * @param[in] psdu_len  MAC PSDU length in bytes.
 *
 * @retval SL_STATUS_OK               Success.
 * @retval SL_STATUS_NOT_INITIALIZED  Module was not initialized.
 */
sl_status_t sl_ot_duty_cycle_record_tx_complete(uint8_t psdu_len);

/**
 * Fill @p status with current window usage, budgets, and alert level.
 *
 * @param[out] status  Caller-provided status (must be non-NULL).
 *
 * @retval SL_STATUS_OK                  Success.
 * @retval SL_STATUS_INVALID_PARAMETER   @p status is NULL.
 * @retval SL_STATUS_NOT_INITIALIZED     Module was not initialized.
 */
sl_status_t sl_ot_duty_cycle_get_status(sl_ot_duty_cycle_status_t *status);

/// Snapshot of every TX-time bucket, including the spare rotation slot.
typedef struct sl_ot_duty_cycle_buckets
{
    uint8_t  current_bucket; ///< Index of the bucket currently accumulating TX time.
    uint8_t  bucket_count;   ///< Number of valid entries in @c bucket_tx_us.
    uint32_t bucket_tx_us[SL_OT_RADIO_TX_TIME_BUCKET_COUNT_DEFAULT]; ///< Per-bucket TX time (µs).
} sl_ot_duty_cycle_buckets_t;

/**
 * Fill @p buckets with the current index and every slot's accumulated TX time.
 *
 * @param[out] buckets  Caller-provided snapshot (must be non-NULL).
 *
 * @retval SL_STATUS_OK                  Success.
 * @retval SL_STATUS_INVALID_PARAMETER   @p buckets is NULL.
 * @retval SL_STATUS_NOT_INITIALIZED     Module was not initialized.
 */
sl_status_t sl_ot_duty_cycle_get_buckets(sl_ot_duty_cycle_buckets_t *buckets);

/**
 * Convert an alert level to a string.
 *
 * @param[in] level  Alert level.
 *
 * @return String representation of the alert level.
 */
const char *sl_ot_duty_cycle_alert_level_to_string(sl_ot_duty_cycle_alert_level_t level);

/** @} (end addtogroup sl_ot_radio_duty_cycle) */

// PAL outbound ACK TX accounting.
// Remove this block when duty-cycle accounting moves to the stack.
// It does not gate transmissions. It only records an ACK that TXACK_PACKET_SENT
// confirms, using one saved length and a sent flag.

/// Immediate ACK PSDU length in bytes.
#define SLI_OT_DUTY_CYCLE_IMMEDIATE_ACK_PSDU_LEN (5u)

/// Clear a prepared ACK and any ACK time waiting for the main loop.
void sli_ot_duty_cycle_ack_tx_reset(void);

/// Remember the ACK about to be sent. A still-unrecorded sent ACK is kept.
void sli_ot_duty_cycle_ack_tx_prepare(uint8_t ack_psdu_len);

/// Mark the prepared ACK as sent. The main loop records the airtime.
void sli_ot_duty_cycle_ack_tx_notify_transmitted(void);

/// Drop the prepared ACK without recording airtime. Already-sent time is kept.
void sli_ot_duty_cycle_ack_tx_discard(void);

/// Record a sent ACK into the duty-cycle buckets. Call from the main loop.
void sli_ot_duty_cycle_ack_tx_process_pending(void);

/// Return whether a sent ACK is waiting for main-loop accounting.
bool sli_ot_duty_cycle_ack_tx_is_pending(void);

#ifdef __cplusplus
}
#endif

#endif // SL_OT_RADIO_DUTY_CYCLE_H
