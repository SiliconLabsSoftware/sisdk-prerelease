/*
 *  Copyright (c) 2025, The OpenThread Authors.
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are met:
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *  3. Neither the name of the copyright holder nor the
 *     names of its contributors may be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 *  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * @file
 *   This file implements the radio state management for the EFR32 platform.
 */

#include "radio_state.h"

#include "common/code_utils.hpp"
#include "common/debug.hpp"

#include "sl_core.h"
#include "sl_rail.h"
#include "sl_rail_ieee802154.h"

#include "radio_energy_scan.h"
#include "radio_events.h"
#include "radio_interface.h"

// Internal state flags (private implementation details)
#define FLAG_RADIO_INIT_DONE 0x00000001
#define FLAG_ONGOING_TX_DATA 0x00000002
#define FLAG_ONGOING_TX_ACK 0x00000004
#define FLAG_WAITING_FOR_ACK 0x00000008
#define FLAG_CURRENT_TX_USE_CSMA 0x00000010
#define FLAG_SCHEDULED_RX_PENDING 0x00000020
#define FLAG_SCHEDULED_TX_PENDING 0x00000040

// Internal state variables
static volatile uint32_t sMiscRadioState = 0;
static bool              sEmPendingData  = false;

SL_CODE_CLASSIFY(SL_CODE_COMPONENT_OT_PLATFORM_ABSTRACTION, SL_CODE_CLASS_TIME_CRITICAL)
void sli_ot_radio_state_set_internal_flag(uint32_t aFlag, bool aVal)
{
    CORE_DECLARE_IRQ_STATE;
    CORE_ENTER_ATOMIC();
    sMiscRadioState = (aVal ? (sMiscRadioState | aFlag) : (sMiscRadioState & ~aFlag));
    CORE_EXIT_ATOMIC();
}

SL_CODE_CLASSIFY(SL_CODE_COMPONENT_OT_PLATFORM_ABSTRACTION, SL_CODE_CLASS_TIME_CRITICAL)
bool sli_ot_radio_state_get_internal_flag(uint32_t aFlag)
{
    bool isFlagSet;
    CORE_DECLARE_IRQ_STATE;
    CORE_ENTER_ATOMIC();
    isFlagSet = (sMiscRadioState & aFlag) ? true : false;
    CORE_EXIT_ATOMIC();

    return isFlagSet;
}

SL_CODE_CLASSIFY(SL_CODE_COMPONENT_OT_PLATFORM_ABSTRACTION, SL_CODE_CLASS_TIME_CRITICAL)
void sli_ot_radio_state_set_idle(void)
{
    if (sli_ot_radio_interface_rail_get_radio_state() != SL_RAIL_RF_STATE_IDLE)
    {
        sli_ot_radio_interface_rail_idle();
#ifdef SL_CATALOG_RAIL_UTIL_IEEE802154_STACK_EVENT_PRESENT
        sli_ot_radio_events_handle_phy_stack_event(SL_RAIL_UTIL_IEEE802154_STACK_EVENT_TX_IDLED, 0U);
        sli_ot_radio_events_handle_phy_stack_event(SL_RAIL_UTIL_IEEE802154_STACK_EVENT_RX_IDLED, 0U);
#endif
    }
    sli_ot_radio_interface_rail_yield_radio();
}

bool sli_ot_radio_state_is_transmitting(void)
{
    return (sli_ot_radio_state_is_tx_data_ongoing() || sli_ot_radio_state_is_tx_ack_ongoing());
}

bool sli_ot_radio_state_is_transmitting_or_scanning(void)
{
    return (sli_ot_energy_scan_is_in_progress() || sli_ot_radio_state_is_tx_data_ongoing()
            || sli_ot_radio_state_has_tx_events());
}

SL_CODE_CLASSIFY(SL_CODE_COMPONENT_OT_PLATFORM_ABSTRACTION, SL_CODE_CLASS_TIME_CRITICAL)
bool sli_ot_radio_state_is_waiting_for_ack(void)
{
    return sli_ot_radio_state_is_tx_data_ongoing() && sli_ot_radio_state_get_internal_flag(FLAG_WAITING_FOR_ACK);
}

bool sli_ot_radio_state_is_tx_scheduled(void)
{
    return sli_ot_radio_state_get_internal_flag(FLAG_SCHEDULED_TX_PENDING | EVENT_SCHEDULED_TX_STARTED);
}

void sli_ot_radio_state_set_scheduled_rx_pending(bool aPending)
{
    sli_ot_radio_state_set_internal_flag(FLAG_SCHEDULED_RX_PENDING, aPending);
}

bool sli_ot_radio_state_is_rx_scheduled(void)
{
    return sli_ot_radio_state_get_internal_flag(FLAG_SCHEDULED_RX_PENDING);
}

void sli_ot_radio_state_set_scheduled_rx_started(bool aStarted)
{
    sli_ot_radio_state_set_internal_flag(EVENT_SCHEDULED_RX_STARTED, aStarted);
}

// ============================================================================
// Radio Initialization State
// ============================================================================

bool sli_ot_radio_state_is_initialized(void)
{
    return sli_ot_radio_state_get_internal_flag(FLAG_RADIO_INIT_DONE);
}

void sli_ot_radio_state_mark_initialized(void)
{
    sli_ot_radio_state_set_internal_flag(FLAG_RADIO_INIT_DONE, true);
}

// ============================================================================
// TX Data State
// ============================================================================

bool sli_ot_radio_state_is_tx_data_ongoing(void)
{
    return sli_ot_radio_state_get_internal_flag(FLAG_ONGOING_TX_DATA);
}

void sli_ot_radio_state_set_tx_data_ongoing(bool aOngoing)
{
    sli_ot_radio_state_set_internal_flag(FLAG_ONGOING_TX_DATA, aOngoing);
}

// ============================================================================
// TX ACK State
// ============================================================================

bool sli_ot_radio_state_is_tx_ack_ongoing(void)
{
    return sli_ot_radio_state_get_internal_flag(FLAG_ONGOING_TX_ACK);
}

void sli_ot_radio_state_set_tx_ack_ongoing(bool aOngoing)
{
    sli_ot_radio_state_set_internal_flag(FLAG_ONGOING_TX_ACK, aOngoing);
}

// ============================================================================
// CSMA State
// ============================================================================

bool sli_ot_radio_state_is_using_csma(void)
{
    return sli_ot_radio_state_get_internal_flag(FLAG_CURRENT_TX_USE_CSMA);
}

void sli_ot_radio_state_set_using_csma(bool aUseCsma)
{
    sli_ot_radio_state_set_internal_flag(FLAG_CURRENT_TX_USE_CSMA, aUseCsma);
}

// ============================================================================
// Waiting for ACK State
// ============================================================================

void sli_ot_radio_state_set_waiting_for_ack(bool aWaiting)
{
    sli_ot_radio_state_set_internal_flag(FLAG_WAITING_FOR_ACK, aWaiting);
}

// ============================================================================
// Scheduled TX State
// ============================================================================

void sli_ot_radio_state_set_scheduled_tx_pending(bool aPending)
{
    sli_ot_radio_state_set_internal_flag(FLAG_SCHEDULED_TX_PENDING, aPending);
}

void sli_ot_radio_state_set_scheduled_tx_started(bool aStarted)
{
    sli_ot_radio_state_set_internal_flag(EVENT_SCHEDULED_TX_STARTED, aStarted);
}

// ============================================================================
// TX Events
// ============================================================================

bool sli_ot_radio_state_has_tx_events(void)
{
    return sli_ot_radio_state_get_internal_flag(RADIO_TX_EVENTS);
}

void sli_ot_radio_state_clear_all_tx_events(void)
{
    sli_ot_radio_state_set_internal_flag(RADIO_TX_EVENTS, false);
}

bool sli_ot_radio_state_has_tx_success(void)
{
    return sli_ot_radio_state_get_internal_flag(EVENT_TX_SUCCESS);
}

void sli_ot_radio_state_set_tx_success(bool aSuccess)
{
    sli_ot_radio_state_set_internal_flag(EVENT_TX_SUCCESS, aSuccess);
}

bool sli_ot_radio_state_has_tx_cca_failed(void)
{
    return sli_ot_radio_state_get_internal_flag(EVENT_TX_CCA_FAILED);
}

void sli_ot_radio_state_set_tx_cca_failed(bool aFailed)
{
    sli_ot_radio_state_set_internal_flag(EVENT_TX_CCA_FAILED, aFailed);
}

bool sli_ot_radio_state_has_tx_no_ack(void)
{
    return sli_ot_radio_state_get_internal_flag(EVENT_TX_NO_ACK);
}

void sli_ot_radio_state_set_tx_no_ack(bool aNoAck)
{
    sli_ot_radio_state_set_internal_flag(EVENT_TX_NO_ACK, aNoAck);
}

bool sli_ot_radio_state_has_tx_failed(void)
{
    return sli_ot_radio_state_get_internal_flag(EVENT_TX_FAILED);
}

void sli_ot_radio_state_set_tx_failed(bool aFailed)
{
    sli_ot_radio_state_set_internal_flag(EVENT_TX_FAILED, aFailed);
}

// ============================================================================
// Combined Operations
// ============================================================================

void sli_ot_radio_state_clear_tx_data_and_wait_for_ack(void)
{
    sli_ot_radio_state_set_internal_flag(FLAG_ONGOING_TX_DATA | FLAG_WAITING_FOR_ACK | EVENT_SCHEDULED_TX_STARTED,
                                         false);
}

void sli_ot_radio_state_clear_all_scheduled_events(void)
{
    sli_ot_radio_state_set_internal_flag(FLAG_SCHEDULED_RX_PENDING | FLAG_SCHEDULED_TX_PENDING
                                             | EVENT_SCHEDULED_TX_STARTED,
                                         false);
}

SL_CODE_CLASSIFY(SL_CODE_COMPONENT_OT_PLATFORM_ABSTRACTION, SL_CODE_CLASS_TIME_CRITICAL)
bool sli_ot_radio_state_is_receiving_frame(void)
{
#ifdef SL_CATALOG_RAIL_UTIL_IEEE802154_STACK_EVENT_PRESENT
    sl_rail_handle_t railHandle = sli_ot_radio_interface_get_rail_handle();

    return (sl_rail_get_radio_state(railHandle) & SL_RAIL_RF_STATE_RX_ACTIVE) == SL_RAIL_RF_STATE_RX_ACTIVE;
#else
    return false;
#endif
}

void sli_ot_radio_state_set_em_pending_data(bool aPending)
{
    sEmPendingData = aPending;
}

bool sli_ot_radio_state_get_em_pending_data(void)
{
    return sEmPendingData;
}

void sli_ot_radio_state_init(void)
{
    sMiscRadioState = 0;
    sEmPendingData  = false;
}

void sli_ot_radio_state_deinit(void)
{
    sMiscRadioState = 0;
    sEmPendingData  = false;
}
