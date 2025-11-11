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
 *   This file implements instance management for single and multi-instance OpenThread configurations,
 *   including instance accessors, transmit queueing, and energy scan deferral.
 */

#include "radio_instance.h"
#include "radio_energy_scan.h"
#include "sl_core.h"
#include "sl_rail.h"
#include "sl_rail_ieee802154.h"

#include <assert.h>
#include "common/code_utils.hpp"
#include "utils/code_utils.h"

// Include OpenThread headers for multipan functionality
#include <openthread-core-config.h>
#if OPENTHREAD_CONFIG_MULTIPLE_INSTANCE_ENABLE
#include <openthread/platform/multipan.h>
#endif

// Include necessary headers for constants
#include "radio_channel_switching.h"
#include "radio_interface.h"
#include "radio_state.h"

// Global state for filter mask (used in multi-instance mode)
#if OPENTHREAD_CONFIG_MULTIPLE_INSTANCE_ENABLE
static uint8_t sRailFilterMask = RADIO_BCAST_PANID_FILTER_MASK;
#endif

#if OPENTHREAD_CONFIG_MULTIPLE_STATIC_INSTANCE_ENABLE
instanceIndex_t sli_ot_radio_instance_get_index(otInstance *aInstance)
{
    return otInstanceGetIndex(aInstance);
}
#else  // !OPENTHREAD_CONFIG_MULTIPLE_STATIC_INSTANCE_ENABLE
instanceIndex_t sli_ot_radio_instance_get_index(otInstance *aInstance)
{
    OT_UNUSED_VARIABLE(aInstance);
    return 0;
}
#endif // OPENTHREAD_CONFIG_MULTIPLE_STATIC_INSTANCE_ENABLE

panIndex_t sli_ot_radio_instance_get_pan_index(otInstance *aInstance)
{
    return static_cast<panIndex_t>(sli_ot_radio_instance_get_index(aInstance));
}

otInstance *sli_ot_radio_instance_get(uint8_t aIndex)
{
#if OPENTHREAD_CONFIG_MULTIPLE_STATIC_INSTANCE_ENABLE
    return otInstanceGetInstance(aIndex);
#else
    OT_UNUSED_VARIABLE(aIndex);
    return otInstanceGetSingle();
#endif
}

bool sli_ot_radio_instance_is_filter_mask_broadcast(uint8_t aFilterMask)
{
    // Check if broadcast PAN ID bit (bit 0) or broadcast address bit (bit 4) is set
    // This indicates the packet should be delivered to all instances
    return ((aFilterMask & RADIO_BCAST_PANID_FILTER_MASK) != 0) || ((aFilterMask & RADIO_BCAST_ADDR_FILTER_MASK) != 0);
}

otInstance *sli_ot_radio_instance_from_filter_mask(uint8_t aFilterMask)
{
    otInstance *instance      = nullptr;
    uint8_t     instanceIndex = 0;
    bool        foundInstance = false;
    uint8_t     panFilterMask;

    // Check if this is a broadcast packet
    otEXPECT(!sli_ot_radio_instance_is_filter_mask_broadcast(aFilterMask));

#if OPENTHREAD_CONFIG_MULTIPLE_INSTANCE_ENABLE
    // We need only the Pan Id masks here, as we are not matching the addresses.
    // Also mask all the unused indices.
    aFilterMask &= sRailFilterMask;
#endif

    // Extract the PAN ID filter bits (bits 0-3) from the filter mask
    // The filter mask structure:
    // | Bit:7 | Bit:6 | Bit:5 | Bit:4 | Bit:3 | Bit:2 | Bit:1 | Bit:0 |
    // | Addr2 | Addr1 | Addr0 | Bcast | Pan2  | Pan1  | Pan0  | Bcast |
    panFilterMask = (uint8_t)(aFilterMask & 0x0F);

    // Find the first set bit in the PAN filter mask
    // Skip bit 0 (broadcast) and look for instance-specific matches
    for (uint8_t i = 1; i < RADIO_INTERFACE_COUNT && i < 8; i++)
    {
        if (panFilterMask & (1 << i))
        {
            instanceIndex = i - 1; // Convert to 0-based index
            foundInstance = true;
            break;
        }
    }

#if OPENTHREAD_CONFIG_MULTIPLE_STATIC_INSTANCE_ENABLE
    // Multi-instance: If no valid instance found or index is out of bounds, exit with nullptr
    otEXPECT(foundInstance && instanceIndex < RADIO_INTERFACE_COUNT);
#else
    // Single-instance: Always return the single instance (backward compatibility)
    OT_UNUSED_VARIABLE(foundInstance);
#endif

    instance = sli_ot_radio_instance_get(instanceIndex);

exit:
    return instance;
}

#if OPENTHREAD_CONFIG_MULTIPLE_INSTANCE_ENABLE

// Modern C++ enum class for better type safety
enum class PendingCommandType : uint8_t
{
    Transmit   = 0,
    EnergyScan = 1
};

// Pending command entry structure
struct PendingCommandEntry
{
    otInstance        *instance;
    PendingCommandType command;
    union
    {
        struct
        {
            otRadioFrame *frame;
        } transmit;
        struct
        {
            uint16_t       channel;
            sl_rail_time_t duration;
        } energy_scan;
    } data;
};

// Global state
static PendingCommandEntry sPendingCommandQueue[RADIO_REQUEST_BUFFER_COUNT];
static uint8_t             sPendingCommandCount = 0;
static void (*sOverflowCallback)(void)          = nullptr;

static volatile bool tx_aborted[RADIO_REQUEST_BUFFER_COUNT] = {false};
static volatile bool tx_busy                                = false;

// Forward declarations for external functions
extern "C" {
extern sl_status_t sli_ot_energy_scan_async(otInstance *instance, uint16_t channel, sl_rail_time_t duration);
extern otError     otPlatRadioTransmit(otInstance *aInstance, otRadioFrame *aFrame);
}

static void txCurrentPacket(otInstance *instance, otRadioFrame *frame) noexcept
{
    // Match radio.c behavior: call otPlatRadioTransmit() to handle all setup
    // (channel config, CSL, security, etc.) before transmitting
    otPlatRadioTransmit(instance, frame);
}

// Command management functions
extern "C" {
bool sli_ot_radio_instance_queue_transmit(otInstance *instance, otRadioFrame *frame)
{
    bool                 success = sPendingCommandCount < RADIO_REQUEST_BUFFER_COUNT;
    PendingCommandEntry *entry   = nullptr;

    otEXPECT_ACTION(success, if (sOverflowCallback) { sOverflowCallback(); });

    entry                      = &sPendingCommandQueue[sPendingCommandCount++];
    entry->instance            = instance;
    entry->command             = PendingCommandType::Transmit;
    entry->data.transmit.frame = frame;

exit:
    return success;
}
}

bool sli_ot_radio_instance_queue_energy_scan(otInstance *instance, uint16_t channel, sl_rail_time_t duration)
{
    bool                 success = sPendingCommandCount < RADIO_REQUEST_BUFFER_COUNT;
    PendingCommandEntry *entry   = nullptr;

    otEXPECT_ACTION(success, if (sOverflowCallback) { sOverflowCallback(); });

    entry                            = &sPendingCommandQueue[sPendingCommandCount++];
    entry->instance                  = instance;
    entry->command                   = PendingCommandType::EnergyScan;
    entry->data.energy_scan.channel  = channel;
    entry->data.energy_scan.duration = duration;

exit:
    return success;
}

// State management functions
extern "C" {
bool sli_ot_radio_instance_is_busy(void)
{
    return sli_ot_radio_state_is_transmitting_or_scanning();
}
}

bool sli_ot_radio_instance_should_defer(otInstance *instance)
{
    OT_UNUSED_VARIABLE(instance);
    // For now, defer if radio is busy
    return sli_ot_radio_instance_is_busy();
}

// Queue management functions
bool sli_ot_radio_instance_is_queue_empty(void)
{
    return sPendingCommandCount == 0;
}

uint8_t sli_ot_radio_instance_get_queue_size(void)
{
    return sPendingCommandCount;
}

void sli_ot_radio_instance_clear_queue(void)
{
    sPendingCommandCount = 0;
}

extern "C" {
void sli_ot_radio_instance_process_commands(void)
{
    while (sPendingCommandCount > 0)
    {
        // Check if radio is busy
        if (sli_ot_radio_instance_is_busy())
        {
            break;
        }

        // Process first command in queue
        const PendingCommandEntry &entry = sPendingCommandQueue[0];

        switch (entry.command)
        {
        case PendingCommandType::Transmit:
            txCurrentPacket(entry.instance, entry.data.transmit.frame);
            break;

        case PendingCommandType::EnergyScan:
            sli_ot_energy_scan_async(entry.instance, entry.data.energy_scan.channel, entry.data.energy_scan.duration);
            break;
        }

        // Remove processed command by shifting remaining commands
        if (sPendingCommandCount > 1)
        {
            // Shift remaining commands forward, ensuring we don't exceed array bounds
            for (uint8_t i = 0; (i < sPendingCommandCount - 1) && (i + 1 < RADIO_REQUEST_BUFFER_COUNT); i++)
            {
                sPendingCommandQueue[i] = sPendingCommandQueue[i + 1];
            }
        }
        sPendingCommandCount--;
    }
}
}

// Callback management
extern "C" {
void sli_ot_radio_instance_set_overflow_callback(void (*callback)(void))
{
    sOverflowCallback = callback;
}
}

static void pushPendingCommand(PendingCommandType aCmdType, otInstance *aInstance, void *aCmdParams)
{
    bool success = false;

    if (aCmdType == PendingCommandType::Transmit)
    {
        otRadioFrame *txFrame = (otRadioFrame *)aCmdParams;
        success               = sli_ot_radio_instance_queue_transmit(aInstance, txFrame);
    }
    else if (aCmdType == PendingCommandType::EnergyScan)
    {
        const EnergyScanParams *energyScanReq = (EnergyScanParams *)aCmdParams;
        success                               = sli_ot_radio_instance_queue_energy_scan(aInstance,
                                                          (uint16_t)energyScanReq->scanChannel,
                                                          (sl_rail_time_t)energyScanReq->scanDuration);
    }

    assert(success);
}

// Multi-instance state management functions
extern "C" {
void sli_ot_radio_instance_set_tx_aborted(uint8_t index, bool aborted)
{
    otEXPECT(index < RADIO_REQUEST_BUFFER_COUNT);

    tx_aborted[index] = aborted;

exit:
    return;
}
}

extern "C" {
bool sli_ot_radio_instance_get_tx_aborted(uint8_t index)
{
    bool aborted = false;

    otEXPECT(index < RADIO_REQUEST_BUFFER_COUNT);

    aborted = tx_aborted[index];

exit:
    return aborted;
}
}

extern "C" {
void sli_ot_radio_instance_set_tx_busy(bool busy)
{
    tx_busy = busy;
}
}

bool sli_ot_radio_instance_get_tx_busy(void)
{
    return tx_busy;
}

uint8_t sli_ot_radio_instance_get_rail_filter_mask(void)
{
    return sRailFilterMask;
}

void sli_ot_radio_instance_set_rail_filter_mask(uint8_t mask)
{
    sRailFilterMask = mask;
}

extern "C" {
void sli_ot_radio_instance_update_rail_filter_mask_for_pan_id(uint16_t aPanId, uint8_t aPanIndex)
{
    // We already have bit 0 enabled in filtermask to track BCAST Packets, so
    // track only unique PanIds.
    // Filter mask mapping: bit 0 = broadcast, bit 1 = instance 0, bit 2 = instance 1, bit 3 = instance 2
    if (aPanId != RADIO_BCAST_PANID)
    {
        sRailFilterMask |= RADIO_GET_FILTER_MASK(aPanIndex + 1);
    }
}
}

#if FAST_CHANNEL_SWITCHING_SUPPORT
bool sl_is_multi_channel_enabled(void)
{
    return sli_ot_radio_channel_switching_is_multi_channel_enabled();
}

otError sl_get_channel_switching_cfg(sl_rail_ieee802154_rx_channel_switching_cfg_t *channelSwitchingCfg)
{
    return sli_ot_radio_channel_switching_get_config(channelSwitchingCfg);
}

#endif // FAST_CHANNEL_SWITCHING_SUPPORT

#endif // OPENTHREAD_CONFIG_MULTIPLE_INSTANCE_ENABLE

bool sli_ot_radio_instance_energy_scan_should_defer(otInstance *aInstance)
{
    OT_UNUSED_VARIABLE(aInstance);
#if OPENTHREAD_CONFIG_MULTIPLE_INSTANCE_ENABLE
    return sli_ot_radio_instance_is_busy();
#else
    return false; // Single instance: never defer
#endif
}

void sli_ot_radio_instance_energy_scan_defer(otInstance *aInstance, uint8_t aScanChannel, uint16_t aScanDuration)
{
#if OPENTHREAD_CONFIG_MULTIPLE_INSTANCE_ENABLE
    EnergyScanParams params = {aScanChannel, aScanDuration};
    pushPendingCommand(PendingCommandType::EnergyScan, aInstance, &params);
#else
    OT_UNUSED_VARIABLE(aInstance);
    OT_UNUSED_VARIABLE(aScanChannel);
    OT_UNUSED_VARIABLE(aScanDuration);
    // Single instance: this should never be called since should_defer returns false
#endif
}

//------------------------------------------------------------------------------
// OpenThread Multipan API Functions

otError otPlatMultipanGetActiveInstance(otInstance **aInstance)
{
    OT_UNUSED_VARIABLE(aInstance);

    return OT_ERROR_INVALID_COMMAND;
}

otError otPlatMultipanSetActiveInstance(otInstance *aInstance, bool aCompletePending)
{
    OT_UNUSED_VARIABLE(aInstance);
    OT_UNUSED_VARIABLE(aCompletePending);

    return OT_ERROR_INVALID_COMMAND;
}
