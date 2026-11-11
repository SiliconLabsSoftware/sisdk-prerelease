/*******************************************************************************
 * @file
 * @brief Sub-GHz radio duty-cycle policy (thresholds, alerts, bucket timer).
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

#include "radio_duty_cycle_manager.h"
#include "radio_duty_cycle_manager.hpp"
#include "radio_interface.h"
#include "sl_core.h"
#include <openthread-system.h>
#include "common/code_utils.hpp"
#include "common/logging.hpp"

#include <string.h>

// On-air time estimate for the Sub-GHz PHY. Uses the same bitrate, SHR, and
// PHR sizes as radio_interface.h (IEEE802154_SUBGHZ_BITRATE_HZ, SHR_SIZE,
// PHY_HEADER_SIZE).
#define SLI_OT_DUTY_CYCLE_BITRATE_HZ (100000u)
#define SLI_OT_DUTY_CYCLE_BITS_PER_BYTE (8u)
#define SLI_OT_DUTY_CYCLE_SECS_TO_US (1000000u)

static RadioDutyCycleManager duty_cycle;

#ifdef TESTING
static uint64_t sli_ot_duty_cycle_test_now_ms = 0;
#endif

/**
 * Estimate on-air microseconds for a PSDU of @p psdu_len bytes.
 *
 * The frame on air is the synchronization header, the PHY header and the PSDU,
 * so the duration is simply the total bit count divided by the bitrate.
 */
static uint32_t sli_ot_duty_cycle_estimate_us(uint8_t psdu_len)
{
    uint32_t frame_bytes = SHR_SIZE + PHY_HEADER_SIZE + psdu_len;
    uint64_t frame_bits  = (uint64_t)frame_bytes * SLI_OT_DUTY_CYCLE_BITS_PER_BYTE;

    return (uint32_t)((frame_bits * SLI_OT_DUTY_CYCLE_SECS_TO_US) / IEEE802154_SUBGHZ_BITRATE_HZ);
}

/**
 * Current time in microseconds, used to stamp bucket boundaries.
 */
static uint64_t sli_ot_duty_cycle_now_us(void)
{
#ifdef TESTING
    return sli_ot_duty_cycle_test_now_ms * 1000u;
#else
    uint64_t ticks  = sl_sleeptimer_get_tick_count64();
    uint64_t now_ms = 0;

    (void)sl_sleeptimer_tick64_to_ms(ticks, &now_ms);

    return now_ms * 1000u;
#endif
}

/**
 * Sleeptimer expiry for the configured bucket period.
 */
static void sli_ot_duty_cycle_bucket_timer_callback(sl_sleeptimer_timer_handle_t *handle, void *data)
{
    (void)handle;
    (void)data;

    duty_cycle.handle_bucket_timer();
}

static void sli_ot_duty_cycle_notify(const sl_ot_duty_cycle_config_t *config, const sl_ot_duty_cycle_status_t *status)
{
    VerifyOrExit(config->on_alert != nullptr);

    config->on_alert(status, config->on_alert_context);

exit:
    return;
}

static uint32_t sli_ot_duty_cycle_duty_scaled(uint32_t window_tx_us)
{
    return (uint32_t)(((uint64_t)window_tx_us * 1000000ull) / SL_OT_RADIO_TX_TIME_WINDOW_DURATION_US_DEFAULT);
}

void RadioDutyCycleManager::clear_buckets(void)
{
    CORE_DECLARE_IRQ_STATE;

    CORE_ENTER_ATOMIC();
    memset(bucket_tx_us, 0, sizeof(bucket_tx_us));
    current_bucket          = 0;
    last_bucket_boundary_us = 0;
    CORE_EXIT_ATOMIC();
}

sl_status_t RadioDutyCycleManager::record_bucket_us(uint32_t duration_us)
{
    sl_status_t error = SL_STATUS_OK;
    uint8_t     bucket_index;
    uint32_t    bucket_us;
    uint32_t    window_tx_us;
    uint32_t    bucket;
    CORE_DECLARE_IRQ_STATE;

    VerifyOrExit(initialized, error = SL_STATUS_NOT_INITIALIZED);

    CORE_ENTER_ATOMIC();

    bucket                       = bucket_tx_us[current_bucket];
    bucket_tx_us[current_bucket] = (duration_us > (UINT32_MAX - bucket)) ? UINT32_MAX : (bucket + duration_us);

    bucket_index = current_bucket;
    bucket_us    = bucket_tx_us[current_bucket];
    window_tx_us = get_window_us();

    CORE_EXIT_ATOMIC();

    log_tx_window(duration_us, bucket_index, bucket_us, window_tx_us);

exit:
    return error;
}

void RadioDutyCycleManager::switch_bucket(uint64_t now_us)
{
    CORE_DECLARE_IRQ_STATE;

    VerifyOrExit(initialized);

    CORE_ENTER_ATOMIC();

    current_bucket               = (uint8_t)((current_bucket + 1u) % SL_OT_RADIO_TX_TIME_BUCKET_COUNT_DEFAULT);
    bucket_tx_us[current_bucket] = 0;
    last_bucket_boundary_us      = now_us;

    CORE_EXIT_ATOMIC();

exit:
    return;
}

uint32_t RadioDutyCycleManager::get_window_us(void) const
{
    uint8_t  slots             = SL_OT_RADIO_TX_TIME_BUCKET_COUNT_DEFAULT;
    uint8_t  buckets_in_window = SL_OT_RADIO_TX_TIME_BUCKETS_IN_WINDOW_DEFAULT;
    uint64_t total_us          = 0;
    uint8_t  offset;
    uint8_t  index;
    CORE_DECLARE_IRQ_STATE;

    VerifyOrExit(initialized);

    CORE_ENTER_ATOMIC();

    // Walk back from the current bucket. The slot at current + 1 is the spare
    // that switch_bucket clears next, so it is deliberately left out.
    for (offset = 0; offset < buckets_in_window; offset++)
    {
        index = (uint8_t)((current_bucket + slots - offset) % slots);
        total_us += bucket_tx_us[index];
    }

    CORE_EXIT_ATOMIC();

    total_us = (total_us > UINT32_MAX) ? UINT32_MAX : total_us;

exit:
    return (uint32_t)total_us;
}

uint32_t RadioDutyCycleManager::get_current_period_us(void) const
{
    uint32_t current_period_us = 0;
    CORE_DECLARE_IRQ_STATE;

    VerifyOrExit(initialized);

    CORE_ENTER_ATOMIC();
    current_period_us = bucket_tx_us[current_bucket];
    CORE_EXIT_ATOMIC();

exit:
    return current_period_us;
}

sl_status_t RadioDutyCycleManager::get_tx_snapshot(TxWindowSnapshot *snapshot) const
{
    sl_status_t error = SL_STATUS_OK;
    CORE_DECLARE_IRQ_STATE;

    VerifyOrExit(snapshot != nullptr, error = SL_STATUS_INVALID_PARAMETER);
    VerifyOrExit(initialized, error = SL_STATUS_NOT_INITIALIZED);

    // Nested atomic sections restore the caller's IRQ state, so this outer one
    // only guarantees that both reads describe the same bucket layout.
    CORE_ENTER_ATOMIC();

    snapshot->total_tx_us          = get_window_us();
    snapshot->window_duration_us   = SL_OT_RADIO_TX_TIME_WINDOW_DURATION_US_DEFAULT;
    snapshot->current_period_tx_us = get_current_period_us();

    CORE_EXIT_ATOMIC();

exit:
    return error;
}

void RadioDutyCycleManager::log_tx_window(uint32_t charged_us,
                                          uint8_t  bucket_index,
                                          uint32_t bucket_us,
                                          uint32_t window_tx_us) const
{
    uint32_t duty_scaled = sli_ot_duty_cycle_duty_scaled(window_tx_us);

    otLogDebgPlat("Tx +%luus, bucket-%02u tx=%luus total tx=%luus (%lu.%04lu%% of an hr)",
                  (unsigned long)charged_us,
                  (unsigned)bucket_index,
                  (unsigned long)bucket_us,
                  (unsigned long)window_tx_us,
                  (unsigned long)(duty_scaled / 10000u),
                  (unsigned long)(duty_scaled % 10000u));
}

sl_status_t RadioDutyCycleManager::copy_buckets(uint32_t *destination,
                                                uint8_t   capacity,
                                                uint8_t  *out_count,
                                                uint8_t  *current) const
{
    sl_status_t error = SL_STATUS_OK;
    CORE_DECLARE_IRQ_STATE;

    VerifyOrExit((destination != nullptr) && (out_count != nullptr) && (current != nullptr)
                     && (capacity >= SL_OT_RADIO_TX_TIME_BUCKET_COUNT_DEFAULT),
                 error = SL_STATUS_INVALID_PARAMETER);
    VerifyOrExit(initialized, error = SL_STATUS_NOT_INITIALIZED);

    CORE_ENTER_ATOMIC();

    memcpy(destination, bucket_tx_us, sizeof(bucket_tx_us));
    *current   = current_bucket;
    *out_count = SL_OT_RADIO_TX_TIME_BUCKET_COUNT_DEFAULT;

    CORE_EXIT_ATOMIC();

exit:
    return error;
}

sl_status_t RadioDutyCycleManager::fill_status(sl_ot_duty_cycle_status_t *status) const
{
    TxWindowSnapshot snapshot;
    sl_status_t      error;
    uint32_t         max_budget_us;

    SuccessOrExit(error = get_tx_snapshot(&snapshot));

    // 64-bit intermediates: one hour is 3.6e9 us, so the permille multiply
    // overflows 32 bits well before the window does.
    max_budget_us = (uint32_t)(((uint64_t)snapshot.window_duration_us * max_duty_cycle_permille) / 1000u);

    status->total_tx_us          = snapshot.total_tx_us;
    status->current_period_tx_us = snapshot.current_period_tx_us;
    status->max_budget_us        = max_budget_us;
    status->remaining_budget_us  = (max_budget_us > snapshot.total_tx_us) ? (max_budget_us - snapshot.total_tx_us) : 0u;
    status->duty_permille        = (snapshot.window_duration_us != 0u)
                                       ? (uint16_t)(((uint64_t)snapshot.total_tx_us * 1000u) / snapshot.window_duration_us)
                                       : (uint16_t)0u;
    status->max_duty_permille    = max_duty_cycle_permille;
    status->level                = level;

exit:
    return error;
}

/**
 * Map window usage onto an alert band and write it to @p status->level.
 */
static inline void sli_ot_duty_cycle_classify_alert_level(sl_ot_duty_cycle_status_t *status)
{
    uint32_t medium_budget_us =
        (uint32_t)(((uint64_t)status->max_budget_us * SL_OT_DUTY_CYCLE_MEDIUM_PERCENT_DEFAULT) / 100u);
    uint32_t warning_budget_us =
        (uint32_t)(((uint64_t)status->max_budget_us * SL_OT_DUTY_CYCLE_WARNING_PERCENT_DEFAULT) / 100u);
    uint32_t high_budget_us =
        (uint32_t)(((uint64_t)status->max_budget_us * SL_OT_DUTY_CYCLE_HIGH_PERCENT_DEFAULT) / 100u);

    if (status->total_tx_us >= status->max_budget_us)
    {
        status->level = SL_OT_DUTY_CYCLE_ALERT_LEVEL_EXHAUSTED;
    }
    else if (status->total_tx_us >= high_budget_us)
    {
        status->level = SL_OT_DUTY_CYCLE_ALERT_LEVEL_HIGH;
    }
    else if (status->total_tx_us >= warning_budget_us)
    {
        status->level = SL_OT_DUTY_CYCLE_ALERT_LEVEL_WARNING;
    }
    else if (status->total_tx_us >= medium_budget_us)
    {
        status->level = SL_OT_DUTY_CYCLE_ALERT_LEVEL_MEDIUM;
    }
    else
    {
        status->level = SL_OT_DUTY_CYCLE_ALERT_LEVEL_LOW;
    }
}

void RadioDutyCycleManager::evaluate_thresholds(sl_ot_duty_cycle_status_t *status_out)
{
    sl_ot_duty_cycle_status_t      status;
    sl_ot_duty_cycle_alert_level_t previous_level = level;

    SuccessOrExit(fill_status(&status));

    sli_ot_duty_cycle_classify_alert_level(&status);
    level = status.level;

    if (level > previous_level)
    {
        sli_ot_duty_cycle_notify(&config, &status);
    }

    if (status_out != nullptr)
    {
        *status_out = status;
    }

exit:
    return;
}

sl_status_t RadioDutyCycleManager::init(const sl_ot_duty_cycle_config_t *config_in)
{
    sl_status_t error   = SL_STATUS_OK;
    bool        started = false;

    VerifyOrExit(config_in != nullptr, error = SL_STATUS_INVALID_PARAMETER);
    VerifyOrExit(!initialized, error = SL_STATUS_ALREADY_INITIALIZED);

    config  = *config_in;
    level   = SL_OT_DUTY_CYCLE_ALERT_LEVEL_LOW;
    started = true;
#ifdef TESTING
    sli_ot_duty_cycle_test_now_ms = 0;
#endif

    otLogInfoPlat("Initializing duty-cycling");

    reset_ack_tx();
    clear_buckets();

    // The bucket period is coarse, so the callback does not need the high-frequency clocks.
    SuccessOrExit(error =
                      sl_sleeptimer_start_periodic_timer_ms(&tx_time_timer,
                                                            SL_OT_RADIO_TX_TIME_BUCKET_SEC_DEFAULT * 1000u,
                                                            sli_ot_duty_cycle_bucket_timer_callback,
                                                            nullptr,
                                                            0,
                                                            SL_SLEEPTIMER_NO_HIGH_PRECISION_HF_CLOCKS_REQUIRED_FLAG));

    initialized = true;

exit:
    if ((error != SL_STATUS_OK) && started)
    {
        clear_buckets();
        reset_ack_tx();
        config = {};
        level  = SL_OT_DUTY_CYCLE_ALERT_LEVEL_NONE;
    }

    return error;
}

sl_status_t RadioDutyCycleManager::deinit(void)
{
    sl_status_t error = SL_STATUS_OK;

    VerifyOrExit(initialized, error = SL_STATUS_NOT_INITIALIZED);

    // Clear the flag first so a timer expiry racing this call becomes a no-op.
    initialized = false;

    otLogInfoPlat("Deinitializing duty-cycling");

    (void)sl_sleeptimer_stop_timer(&tx_time_timer);
    clear_buckets();

    reset_ack_tx();

    config = {};
    level  = SL_OT_DUTY_CYCLE_ALERT_LEVEL_NONE;
#ifdef TESTING
    sli_ot_duty_cycle_test_now_ms = 0;
#endif

exit:
    return error;
}

sl_status_t RadioDutyCycleManager::check_tx(uint8_t psdu_len)
{
    sl_status_t               error                = SL_STATUS_OK;
    sl_ot_duty_cycle_status_t current_status       = {};
    uint32_t                  estimated_tx_time_us = 0;

    VerifyOrExit(initialized, error = SL_STATUS_NOT_INITIALIZED);

    // Zeroed so an accounting failure leaves no budget and denies the frame.
    evaluate_thresholds(&current_status);
    estimated_tx_time_us = sli_ot_duty_cycle_estimate_us(psdu_len);
    error                = (estimated_tx_time_us <= current_status.remaining_budget_us) ? SL_STATUS_OK : SL_STATUS_FAIL;

exit:
    if (error != SL_STATUS_OK)
    {
        otLogWarnPlat("Tx rejected: %u bytes, estimated tx time %lu us, %lu remaining budget",
                      psdu_len,
                      (unsigned long)estimated_tx_time_us,
                      (unsigned long)current_status.remaining_budget_us);
    }
    return error;
}

sl_status_t RadioDutyCycleManager::record_tx_complete(uint8_t psdu_len)
{
    return record_tx_us(sli_ot_duty_cycle_estimate_us(psdu_len));
}

sl_status_t RadioDutyCycleManager::record_tx_us(uint32_t duration_us)
{
    sl_status_t error = SL_STATUS_OK;

    VerifyOrExit(initialized, error = SL_STATUS_NOT_INITIALIZED);

    SuccessOrExit(error = record_bucket_us(duration_us));

    evaluate_thresholds();

exit:
    return error;
}

// PAL outbound ACK TX accounting.
// Remove this block when duty-cycle accounting moves to the stack.
// The only call into the shared buckets is record_tx_us() below.

void RadioDutyCycleManager::reset_ack_tx(void)
{
    CORE_DECLARE_IRQ_STATE;

    CORE_ENTER_ATOMIC();
    ack_tx_psdu_len       = 0;
    ack_tx_sent           = false;
    ack_tx_accumulated_us = 0;
    CORE_EXIT_ATOMIC();
}

void RadioDutyCycleManager::prepare_ack_tx(uint8_t ack_psdu_len)
{
    bool signal_pending = false;

    CORE_DECLARE_IRQ_STATE;
    CORE_ENTER_ATOMIC();

    // The next ACK can be prepared before the main loop records the previous one.
    if (ack_tx_sent && (ack_tx_psdu_len != 0u))
    {
        ack_tx_accumulated_us += sli_ot_duty_cycle_estimate_us(ack_tx_psdu_len);
        signal_pending = true;
    }

    ack_tx_psdu_len = ack_psdu_len;
    ack_tx_sent     = false;
    CORE_EXIT_ATOMIC();

    if (signal_pending)
    {
        otSysEventSignalPending();
    }
}

void RadioDutyCycleManager::notify_ack_tx_transmitted(void)
{
    bool signal_pending = false;

    CORE_DECLARE_IRQ_STATE;
    CORE_ENTER_ATOMIC();

    if (ack_tx_psdu_len != 0u)
    {
        ack_tx_sent    = true;
        signal_pending = true;
    }

    CORE_EXIT_ATOMIC();

    if (signal_pending)
    {
        otSysEventSignalPending();
    }
}

void RadioDutyCycleManager::discard_ack_tx(void)
{
    CORE_DECLARE_IRQ_STATE;

    CORE_ENTER_ATOMIC();
    // A sent ACK stays until the main loop records it. This also covers an RX
    // error that arrives after TXACK_PACKET_SENT and before process_pending.
    if (!ack_tx_sent)
    {
        ack_tx_psdu_len = 0;
    }
    CORE_EXIT_ATOMIC();
}

void RadioDutyCycleManager::process_pending_ack_tx(void)
{
    uint32_t pending_us = 0;

    CORE_DECLARE_IRQ_STATE;
    CORE_ENTER_ATOMIC();
    pending_us            = ack_tx_accumulated_us;
    ack_tx_accumulated_us = 0;

    if (ack_tx_sent && (ack_tx_psdu_len != 0u))
    {
        pending_us += sli_ot_duty_cycle_estimate_us(ack_tx_psdu_len);
        ack_tx_psdu_len = 0;
        ack_tx_sent     = false;
    }

    CORE_EXIT_ATOMIC();

    if (pending_us != 0u)
    {
        sl_status_t error = record_tx_us(pending_us);

        if (error == SL_STATUS_OK)
        {
            otLogDebgPlat("Ack Tx +%luus", (unsigned long)pending_us);
        }
    }
}

bool RadioDutyCycleManager::is_ack_tx_pending(void) const
{
    bool pending = false;

    CORE_DECLARE_IRQ_STATE;
    CORE_ENTER_ATOMIC();
    pending = (ack_tx_accumulated_us != 0u) || (ack_tx_sent && (ack_tx_psdu_len != 0u));
    CORE_EXIT_ATOMIC();

    return pending;
}

sl_status_t RadioDutyCycleManager::get_status(sl_ot_duty_cycle_status_t *status) const
{
    sl_status_t error = SL_STATUS_OK;

    VerifyOrExit(status != nullptr, error = SL_STATUS_INVALID_PARAMETER);
    VerifyOrExit(initialized, error = SL_STATUS_NOT_INITIALIZED);

    error = fill_status(status);

exit:
    return error;
}

sl_status_t RadioDutyCycleManager::get_buckets(sl_ot_duty_cycle_buckets_t *buckets) const
{
    sl_status_t error = SL_STATUS_OK;

    VerifyOrExit(buckets != nullptr, error = SL_STATUS_INVALID_PARAMETER);

    error = copy_buckets(buckets->bucket_tx_us,
                         SL_OT_RADIO_TX_TIME_BUCKET_COUNT_DEFAULT,
                         &buckets->bucket_count,
                         &buckets->current_bucket);

exit:
    return error;
}

void RadioDutyCycleManager::handle_bucket_timer(void)
{
    VerifyOrExit(initialized);

    switch_bucket(sli_ot_duty_cycle_now_us());

    // The oldest bucket just left the window, so usage can only have dropped.
    evaluate_thresholds();

exit:
    return;
}

extern "C" {

sl_status_t sl_ot_duty_cycle_init(const sl_ot_duty_cycle_config_t *config)
{
    return duty_cycle.init(config);
}

sl_status_t sl_ot_duty_cycle_deinit(void)
{
    return duty_cycle.deinit();
}

sl_status_t sl_ot_duty_cycle_check_tx(uint8_t psdu_len)
{
    return duty_cycle.check_tx(psdu_len);
}

sl_status_t sl_ot_duty_cycle_record_tx_complete(uint8_t psdu_len)
{
    return duty_cycle.record_tx_complete(psdu_len);
}

sl_status_t sl_ot_duty_cycle_get_status(sl_ot_duty_cycle_status_t *status)
{
    return duty_cycle.get_status(status);
}

sl_status_t sl_ot_duty_cycle_get_buckets(sl_ot_duty_cycle_buckets_t *buckets)
{
    return duty_cycle.get_buckets(buckets);
}

const char *sl_ot_duty_cycle_alert_level_to_string(sl_ot_duty_cycle_alert_level_t level)
{
    switch (level)
    {
    case SL_OT_DUTY_CYCLE_ALERT_LEVEL_NONE:
        return "NONE";
    case SL_OT_DUTY_CYCLE_ALERT_LEVEL_LOW:
        return "LOW";
    case SL_OT_DUTY_CYCLE_ALERT_LEVEL_MEDIUM:
        return "MEDIUM";
    case SL_OT_DUTY_CYCLE_ALERT_LEVEL_WARNING:
        return "WARNING";
    case SL_OT_DUTY_CYCLE_ALERT_LEVEL_HIGH:
        return "HIGH";
    case SL_OT_DUTY_CYCLE_ALERT_LEVEL_EXHAUSTED:
        return "EXHAUSTED";
    default:
        return "UNKNOWN";
    }
}

// PAL outbound ACK TX accounting. Remove with the class methods above.
void sli_ot_duty_cycle_ack_tx_reset(void)
{
    duty_cycle.reset_ack_tx();
}

void sli_ot_duty_cycle_ack_tx_prepare(uint8_t ack_psdu_len)
{
    duty_cycle.prepare_ack_tx(ack_psdu_len);
}

void sli_ot_duty_cycle_ack_tx_notify_transmitted(void)
{
    duty_cycle.notify_ack_tx_transmitted();
}

void sli_ot_duty_cycle_ack_tx_discard(void)
{
    duty_cycle.discard_ack_tx();
}

void sli_ot_duty_cycle_ack_tx_process_pending(void)
{
    duty_cycle.process_pending_ack_tx();
}

bool sli_ot_duty_cycle_ack_tx_is_pending(void)
{
    return duty_cycle.is_ack_tx_pending();
}

} // extern "C"

#ifdef TESTING
sl_status_t RadioDutyCycleManager::record_us_for_test(uint32_t duration_us)
{
    return record_bucket_us(duration_us);
}

namespace Testing {
namespace Radio {
namespace DutyCycleTest {
void AdvanceBucketTimer(void)
{
    sli_ot_duty_cycle_test_now_ms += (uint64_t)SL_OT_RADIO_TX_TIME_BUCKET_SEC_DEFAULT * 1000u;
    duty_cycle.handle_bucket_timer();
}

void AdvanceBucketTimers(unsigned count)
{
    for (unsigned i = 0; i < count; i++)
    {
        AdvanceBucketTimer();
    }
}

sl_status_t RecordTxUs(uint32_t duration_us)
{
    return duty_cycle.record_us_for_test(duration_us);
}
} // namespace DutyCycleTest
} // namespace Radio
} // namespace Testing
#endif
