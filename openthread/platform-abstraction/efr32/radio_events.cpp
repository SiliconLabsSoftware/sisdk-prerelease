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
 *   This file implements the radio events for handling RAIL event processing.
 */

#include "radio_events.h"
#include "openthread-core-config.h"
#include "platform-efr32.h"
#include "radio_interface.h"
#include "radio_state.h"
#include "sl_core.h"
#include "sl_rail.h"
#include <openthread-system.h>
#include "common/logging.hpp"

#ifdef SL_COMPONENT_CATALOG_PRESENT
#include "sl_component_catalog.h"
#endif // SL_COMPONENT_CATALOG_PRESENT

#ifdef SL_CATALOG_RAIL_MULTIPLEXER_PRESENT
extern "C" {
#include "sl_rail_mux_rename.h"
}
#endif

// External dependencies - these will be resolved through proper module interfaces
extern "C" {
// TX/RX callback functions
extern void packetSentCallback(bool aIsAck);
extern void packetReceivedCallback(void);
extern void txFailedCallback(bool aIsAck, uint32_t aError);
extern void ackTimeoutCallback(void);
extern void schedulerEventCallback(sl_rail_handle_t aRailHandle);
extern void dataRequestCommandCallback(sl_rail_handle_t aRailHandle);

extern bool isReceivingFrame(void);
extern bool txWaitingForAck(void);
extern bool txIsDataRequest(void);
}

// Internal state
static sl_rail_events_t sCurrentEventConfig   = SL_RAIL_EVENTS_NONE;
static bool             sPhyStackEventEnabled = false;

// Forward declarations for internal functions
static void processTxPacketSentEvent(void);
static void processTxChannelBusyEvent(void);
static void processTxBlockedEvent(void);
static void processTxUnderflowAbortedEvent(void);
static void processTxCcaEvents(sl_rail_events_t aEvents);
static void processRxPacketReceivedEvent(void);
#ifdef SL_CATALOG_RAIL_UTIL_IEEE802154_STACK_EVENT_PRESENT
static void processRxSyncDetectedEvent(void);
static void processRxFilterPassedEvent(void);
static void processRxFrameErrorEvent(void);
static void processRxFilteredEvent(void);
#endif // SL_CATALOG_RAIL_UTIL_IEEE802154_STACK_EVENT_PRESENT
static void processAckSentEvent(void);
static void processAckAbortedEvent(void);
static void processAckBlockedEvent(void);
static void processScheduledTxEvent(void);
static void processScheduledTxMissedEvent(void);
static void processScheduledRxEvent(void);
static void processScheduledRxEndMissedEvent(void);

#ifdef SL_CATALOG_RAIL_UTIL_COEX_PRESENT
static void processCoexSignalDetectedEvent(void);
#endif // SL_CATALOG_RAIL_UTIL_COEX_PRESENT

static void processDataRequestCommandEvent(sl_rail_handle_t aRailHandle);

void sli_ot_radio_events_init(void)
{
    sCurrentEventConfig   = SL_RAIL_EVENTS_NONE;
    sPhyStackEventEnabled = false;
}

void sli_ot_radio_events_deinit(void)
{
    sCurrentEventConfig   = SL_RAIL_EVENTS_NONE;
    sPhyStackEventEnabled = false;
}

void sli_ot_radio_events_update_config(sl_rail_events_t mask, sl_rail_events_t values)
{
    sl_rail_status_t status;
    sl_rail_events_t newEventConfig = (sCurrentEventConfig & ~mask) | (values & mask);

    if (newEventConfig != sCurrentEventConfig)
    {
        sl_rail_handle_t railHandle = sli_ot_radio_interface_get_rail_handle();
        if (railHandle != nullptr)
        {
            status = sl_rail_config_events(railHandle, mask, values);

            if (status != SL_RAIL_STATUS_NO_ERROR)
            {
                otLogWarnPlat("Failed to configure radio events: %lu", status);
            }
            sCurrentEventConfig = newEventConfig;
        }
    }
}

sl_rail_events_t sli_ot_radio_events_get_config(void)
{
    return sCurrentEventConfig;
}

#ifdef SL_CATALOG_RAIL_UTIL_IEEE802154_STACK_EVENT_PRESENT
sl_rail_util_ieee802154_stack_event_t sli_ot_radio_events_handle_phy_stack_event_with_status(
    sl_rail_util_ieee802154_stack_event_t stackEvent,
    uint32_t                              supplement)
{
    if (!sPhyStackEventEnabled)
    {
        return SL_RAIL_UTIL_IEEE802154_STACK_EVENT_TX_IDLED;
    }

#ifdef SL_CATALOG_RAIL_MULTIPLEXER_PRESENT
    sl_rail_handle_t railHandle = sli_ot_radio_interface_get_rail_handle();
    if (railHandle != nullptr)
    {
        return sl_rail_mux_ieee802154_on_event(railHandle, stackEvent, supplement);
    }
#else
    OT_UNUSED_VARIABLE(stackEvent);
    OT_UNUSED_VARIABLE(supplement);
#endif
    return SL_RAIL_UTIL_IEEE802154_STACK_EVENT_TX_IDLED;
}

void sli_ot_radio_events_handle_phy_stack_event(sl_rail_util_ieee802154_stack_event_t stackEvent, uint32_t supplement)
{
    sli_ot_radio_events_handle_phy_stack_event_with_status(stackEvent, supplement);
}
#endif // SL_CATALOG_RAIL_UTIL_IEEE802154_STACK_EVENT_PRESENT

bool sli_ot_radio_events_is_phy_stack_enabled(void)
{
    return sPhyStackEventEnabled;
}

void sli_ot_radio_events_set_phy_stack_enabled(bool enabled)
{
    sPhyStackEventEnabled = enabled;
}

void sli_ot_radio_events_process_callback(sl_rail_handle_t aRailHandle, sl_rail_events_t aEvents)
{
    // Process RX sync detection events first
#ifdef SL_CATALOG_RAIL_UTIL_IEEE802154_STACK_EVENT_PRESENT
    if (aEvents & (SL_RAIL_EVENT_RX_SYNC_0_DETECT | SL_RAIL_EVENT_RX_SYNC_1_DETECT))
    {
        processRxSyncDetectedEvent();
    }
#endif

    // Process coexistence events
#ifdef SL_CATALOG_RAIL_UTIL_COEX_PRESENT
    if (aEvents & SL_RAIL_EVENT_SIGNAL_DETECTED)
    {
        processCoexSignalDetectedEvent();
    }
#endif

    // Process data request command events
    if ((aEvents & SL_RAIL_EVENT_IEEE802154_DATA_REQUEST_COMMAND)
#ifdef SL_CATALOG_RAIL_UTIL_COEX_PRESENT
        && !sl_rail_is_rx_auto_ack_paused(aRailHandle)
#endif
    )
    {
        processDataRequestCommandEvent(aRailHandle);
    }

    // Process RX filter passed events
#ifdef SL_CATALOG_RAIL_UTIL_IEEE802154_STACK_EVENT_PRESENT
    if (aEvents & SL_RAIL_EVENT_RX_FILTER_PASSED)
    {
        processRxFilterPassedEvent();
    }
#endif

    // Process TX events
    sli_ot_radio_events_process_tx_events(aEvents);

    // Process scheduled events for Thread 1.2+
#if OPENTHREAD_CONFIG_THREAD_VERSION >= OT_THREAD_VERSION_1_2
    if (sli_ot_radio_state_is_rx_scheduled())
    {
        sli_ot_radio_events_process_scheduled_rx_events(aEvents);
    }
    else
    {
        sli_ot_radio_events_process_scheduled_tx_events(aEvents);
    }
#endif

    // Process RX packet received events
    if (aEvents & SL_RAIL_EVENT_RX_PACKET_RECEIVED)
    {
        processRxPacketReceivedEvent();
    }

    // Process RX error events
#ifdef SL_CATALOG_RAIL_UTIL_IEEE802154_STACK_EVENT_PRESENT
    if (aEvents & SL_RAIL_EVENT_RX_FRAME_ERROR)
    {
        processRxFrameErrorEvent();
    }

    if (aEvents
        & (SL_RAIL_EVENT_RX_PACKET_ABORTED | SL_RAIL_EVENT_RX_ADDRESS_FILTERED | SL_RAIL_EVENT_RX_FIFO_OVERFLOW))
    {
        processRxFilteredEvent();
    }
#endif

    // Process ACK events
    sli_ot_radio_events_process_ack_events(aEvents);

    // Deal with ACK timeout after possible RX completion in case RAIL
    // notifies us of the ACK and the timeout simultaneously -- we want
    // the ACK to win over the timeout.
    if ((aEvents & SL_RAIL_EVENT_RX_ACK_TIMEOUT) && (sli_ot_radio_state_get_internal_flag(FLAG_WAITING_FOR_ACK)))
    {
        ackTimeoutCallback();
    }

    if (aEvents & SL_RAIL_EVENT_CONFIG_UNSCHEDULED)
    {
        sli_ot_radio_events_handle_phy_stack_event(SL_RAIL_UTIL_IEEE802154_STACK_EVENT_RX_IDLED, 0U);
#if RADIO_CONFIG_DEBUG_COUNTERS_SUPPORT
        railDebugCounters.mRailEventConfigUnScheduled++;
#endif
    }

    if (aEvents & SL_RAIL_EVENT_CONFIG_SCHEDULED)
    {
#if RADIO_CONFIG_DEBUG_COUNTERS_SUPPORT
        railDebugCounters.mRailEventConfigScheduled++;
#endif
    }

    if (aEvents & SL_RAIL_EVENT_SCHEDULER_STATUS)
    {
        schedulerEventCallback(aRailHandle);
    }

    if (aEvents & SL_RAIL_EVENT_CAL_NEEDED)
    {
        sl_rail_status_t status;

        status = sl_rail_calibrate(aRailHandle, NULL, SL_RAIL_CAL_ALL_PENDING);
        // Non-RTOS DMP case fails but is unsupported
#if (!defined(SL_CATALOG_BLUETOOTH_PRESENT) || defined(SL_CATALOG_KERNEL_PRESENT))
        // TEMPORARY - this asserts on Mux - OT_ASSERT(status == SL_RAIL_STATUS_NO_ERROR);
        OT_UNUSED_VARIABLE(status);
#else
        OT_UNUSED_VARIABLE(status);
#endif

#if RADIO_CONFIG_DEBUG_COUNTERS_SUPPORT
        railDebugCounters.mRailEventCalNeeded++;
#endif
    }

    // scheduled and unscheduled config events happen very often,
    // especially in a DMP situation where there is an active BLE connection.
    // Waking up the OT RTOS task on every one of these occurrences causes
    // a lower priority Serial task to starve and makes it appear like a code lockup
    // There is no reason to wake the OT task for these events!
    if (!(aEvents & SL_RAIL_EVENT_CONFIG_SCHEDULED) && !(aEvents & SL_RAIL_EVENT_CONFIG_UNSCHEDULED))
    {
        otSysEventSignalPending();
    }
}

void sli_ot_radio_events_process_tx_events(sl_rail_events_t aEvents)
{
    if (aEvents & SL_RAIL_EVENT_TX_PACKET_SENT)
    {
        processTxPacketSentEvent();
    }
    else if (aEvents & SL_RAIL_EVENT_TX_CHANNEL_BUSY)
    {
        processTxChannelBusyEvent();
    }
    else if (aEvents & SL_RAIL_EVENT_TX_BLOCKED)
    {
        processTxBlockedEvent();
    }
    else if (aEvents & (SL_RAIL_EVENT_TX_UNDERFLOW | SL_RAIL_EVENT_TX_ABORTED))
    {
        processTxUnderflowAbortedEvent();
    }
    else
    {
        // Process CCA-related events
        processTxCcaEvents(aEvents);
    }
}

void sli_ot_radio_events_process_rx_events(sl_rail_events_t aEvents)
{
    // RX events are processed in the main callback
    // This function is provided for future extensibility
    OT_UNUSED_VARIABLE(aEvents);
}

void sli_ot_radio_events_process_scheduled_tx_events(sl_rail_events_t aEvents)
{
    if (aEvents & SL_RAIL_EVENT_TX_SCHEDULED_TX_STARTED)
    {
        processScheduledTxEvent();
    }
    else if (aEvents & SL_RAIL_EVENT_TX_SCHEDULED_TX_MISSED)
    {
        processScheduledTxMissedEvent();
    }
}

void sli_ot_radio_events_process_scheduled_rx_events(sl_rail_events_t aEvents)
{
    if (aEvents & SL_RAIL_EVENT_RX_SCHEDULED_RX_STARTED)
    {
        processScheduledRxEvent();
    }

    if (aEvents & SL_RAIL_EVENT_RX_SCHEDULED_RX_END || aEvents & SL_RAIL_EVENT_RX_SCHEDULED_RX_MISSED)
    {
        processScheduledRxEndMissedEvent();
    }
}

void sli_ot_radio_events_process_ack_events(sl_rail_events_t aEvents)
{
    if (aEvents & SL_RAIL_EVENT_TXACK_PACKET_SENT)
    {
        processAckSentEvent();
    }

    if (aEvents & (SL_RAIL_EVENT_TXACK_ABORTED | SL_RAIL_EVENT_TXACK_UNDERFLOW))
    {
        processAckAbortedEvent();
    }

    if (aEvents & SL_RAIL_EVENT_TXACK_BLOCKED)
    {
        processAckBlockedEvent();
    }
}

void sli_ot_radio_events_process_coex_events(sl_rail_events_t aEvents)
{
    // Coexistence events are processed in the main callback
    // This function is provided for future extensibility
    OT_UNUSED_VARIABLE(aEvents);
}

void sli_ot_radio_events_process_data_request_events(sl_rail_handle_t aRailHandle, sl_rail_events_t aEvents)
{
    if (aEvents & SL_RAIL_EVENT_IEEE802154_DATA_REQUEST_COMMAND)
    {
        processDataRequestCommandEvent(aRailHandle);
    }
}

void sli_ot_radio_events_process_error_events(sl_rail_events_t aEvents)
{
    // Error events are processed in the main callback
    // This function is provided for future extensibility
    OT_UNUSED_VARIABLE(aEvents);
}

// Internal event processing functions
static void processTxPacketSentEvent(void)
{
    packetSentCallback(false);
}

static void processTxChannelBusyEvent(void)
{
    sli_ot_radio_events_handle_phy_stack_event(SL_RAIL_UTIL_IEEE802154_STACK_EVENT_TX_BLOCKED,
                                               static_cast<uint32_t>(txWaitingForAck()));
    txFailedCallback(false, EVENT_TX_CCA_FAILED);
}

static void processTxBlockedEvent(void)
{
    sli_ot_radio_events_handle_phy_stack_event(SL_RAIL_UTIL_IEEE802154_STACK_EVENT_TX_BLOCKED,
                                               static_cast<uint32_t>(txWaitingForAck()));
    txFailedCallback(false, EVENT_TX_FAILED);
}

static void processTxUnderflowAbortedEvent(void)
{
    sli_ot_radio_events_handle_phy_stack_event(SL_RAIL_UTIL_IEEE802154_STACK_EVENT_TX_ABORTED,
                                               static_cast<uint32_t>(txWaitingForAck()));
    txFailedCallback(false, EVENT_TX_FAILED);
}

static void processTxCcaEvents(sl_rail_events_t aEvents)
{
#ifdef SL_CATALOG_RAIL_UTIL_IEEE802154_STACK_EVENT_PRESENT
    if (aEvents & SL_RAIL_EVENT_TX_START_CCA)
    {
        sli_ot_radio_events_handle_phy_stack_event(SL_RAIL_UTIL_IEEE802154_STACK_EVENT_TX_CCA_SOON, 0U);
    }

    if (aEvents & SL_RAIL_EVENT_TX_CCA_RETRY)
    {
        sl_rail_handle_t railHandle = sli_ot_radio_interface_get_rail_handle();
        sli_ot_radio_events_handle_phy_stack_event(SL_RAIL_UTIL_IEEE802154_STACK_EVENT_TX_CCA_BUSY,
                                                   static_cast<uint32_t>(sl_rail_is_next_cca_now(railHandle)));
    }

    if (aEvents & SL_RAIL_EVENT_TX_CHANNEL_CLEAR)
    {
        sli_ot_radio_events_handle_phy_stack_event(SL_RAIL_UTIL_IEEE802154_STACK_EVENT_TX_STARTED, 0U);
    }
#else
    OT_UNUSED_VARIABLE(aEvents);
#endif
}

static void processRxPacketReceivedEvent(void)
{
    packetReceivedCallback();
}

#ifdef SL_CATALOG_RAIL_UTIL_IEEE802154_STACK_EVENT_PRESENT
static void processRxSyncDetectedEvent(void)
{
    sli_ot_radio_events_handle_phy_stack_event(SL_RAIL_UTIL_IEEE802154_STACK_EVENT_RX_STARTED,
                                               static_cast<uint32_t>(sli_ot_radio_state_is_receiving_frame()));
}

static void processRxFilterPassedEvent(void)
{
    sli_ot_radio_events_handle_phy_stack_event(SL_RAIL_UTIL_IEEE802154_STACK_EVENT_RX_ACCEPTED,
                                               static_cast<uint32_t>(sli_ot_radio_state_is_receiving_frame()));
}

static void processRxFrameErrorEvent(void)
{
    sli_ot_radio_events_handle_phy_stack_event(SL_RAIL_UTIL_IEEE802154_STACK_EVENT_RX_CORRUPTED,
                                               static_cast<uint32_t>(sli_ot_radio_state_is_receiving_frame()));
}

static void processRxFilteredEvent(void)
{
    sli_ot_radio_events_handle_phy_stack_event(SL_RAIL_UTIL_IEEE802154_STACK_EVENT_RX_FILTERED,
                                               static_cast<uint32_t>(sli_ot_radio_state_is_receiving_frame()));
}
#endif // SL_CATALOG_RAIL_UTIL_IEEE802154_STACK_EVENT_PRESENT

static void processAckSentEvent(void)
{
    sli_ot_radio_events_handle_phy_stack_event(SL_RAIL_UTIL_IEEE802154_STACK_EVENT_RX_ACK_SENT,
                                               static_cast<uint32_t>(sli_ot_radio_state_is_receiving_frame()));
    packetSentCallback(true);
}

static void processAckAbortedEvent(void)
{
    sli_ot_radio_events_handle_phy_stack_event(SL_RAIL_UTIL_IEEE802154_STACK_EVENT_RX_ACK_ABORTED,
                                               static_cast<uint32_t>(sli_ot_radio_state_is_receiving_frame()));
    txFailedCallback(true, 0xFF);
}

static void processAckBlockedEvent(void)
{
    sli_ot_radio_events_handle_phy_stack_event(SL_RAIL_UTIL_IEEE802154_STACK_EVENT_RX_ACK_BLOCKED,
                                               static_cast<uint32_t>(sli_ot_radio_state_is_receiving_frame()));
}

static void processScheduledTxEvent(void)
{
    sli_ot_radio_state_set_internal_flag(EVENT_SCHEDULED_TX_STARTED, true);
    sli_ot_radio_state_set_internal_flag(FLAG_SCHEDULED_TX_PENDING, false);
}

static void processScheduledTxMissedEvent(void)
{
    sli_ot_radio_state_set_internal_flag(FLAG_SCHEDULED_TX_PENDING, false);
    txFailedCallback(false, EVENT_TX_SCHEDULER_ERROR);
}

static void processScheduledRxEvent(void)
{
    sli_ot_radio_state_set_internal_flag(EVENT_SCHEDULED_RX_STARTED, true);
}

static void processScheduledRxEndMissedEvent(void)
{
    sli_ot_radio_state_set_internal_flag(FLAG_SCHEDULED_RX_PENDING | EVENT_SCHEDULED_RX_STARTED, false);
    sli_ot_radio_state_set_idle();
}

#ifdef SL_CATALOG_RAIL_UTIL_COEX_PRESENT
static void processCoexSignalDetectedEvent(void)
{
    sli_ot_radio_events_handle_phy_stack_event(SL_RAIL_UTIL_IEEE802154_STACK_EVENT_SIGNAL_DETECTED, 0U);
}
#endif // SL_CATALOG_RAIL_UTIL_COEX_PRESENT

static void processDataRequestCommandEvent(sl_rail_handle_t aRailHandle)
{
    dataRequestCommandCallback(aRailHandle);
}
