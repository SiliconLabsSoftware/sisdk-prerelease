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
 *   This file implements the radio security for the EFR32 platform.
 */

#include "radio_security.h"

#include <openthread-core-config.h>
#include <openthread/platform/radio.h>
#include <openthread/platform/time.h>

#include "common/code_utils.hpp"
#include "common/debug.hpp"
#include "common/logging.hpp"
#include "utils/code_utils.h"
#include "utils/mac_frame.h"

#include "pa_conversions_efr32.h"
#include "platform-efr32.h"
#include "radio_instance.h"
#include "sl_core.h"
#include "sl_packet_utils.h"

// Note: INVALID_INTERFACE_INDEX is defined in radio_instance.h

#if (OPENTHREAD_CONFIG_THREAD_VERSION >= OT_THREAD_VERSION_1_2)

// Security key management
enum class MacKeyType
{
    PREV,
    CURRENT,
    NEXT,
    COUNT
};

struct securityMaterial
{
    uint8_t          ackKeyId;
    uint8_t          keyId;
    uint32_t         macFrameCounter;
    uint32_t         ackFrameCounter;
    otMacKeyMaterial keys[static_cast<int>(MacKeyType::COUNT)];
};

// Per-instance security material
static securityMaterial sMacKeys[RADIO_INTERFACE_COUNT];

// External declarations
extern otExtAddress sExtAddress[RADIO_EXT_ADDR_COUNT];

extern "C" {

void sli_ot_radio_security_init(void)
{
    // Initialize security material for all instances
    memset(sMacKeys, 0, sizeof(sMacKeys));
}

void sli_ot_radio_security_deinit(void)
{
    // Clear security material for all instances
    memset(sMacKeys, 0, sizeof(sMacKeys));
}

otError sli_ot_radio_security_process_transmit(otRadioFrame *aFrame, otInstance *aInstance)
{
    otError         error = OT_ERROR_NONE;
    uint8_t         keyId;
    uint8_t         keyToUse;
    instanceIndex_t instanceIndex = sli_ot_radio_instance_get_index(aInstance);

    otEXPECT(otMacFrameIsSecurityEnabled(aFrame) && otMacFrameIsKeyIdMode1(aFrame)
             && !aFrame->mInfo.mTxInfo.mIsSecurityProcessed);

    if (otMacFrameIsAck(aFrame))
    {
        keyId = otMacFrameGetKeyId(aFrame);

        otEXPECT_ACTION(keyId != 0, error = OT_ERROR_FAILED);

        if (keyId == sMacKeys[instanceIndex].keyId - 1)
        {
            keyToUse = static_cast<uint8_t>(MacKeyType::PREV);
        }
        else if (keyId == sMacKeys[instanceIndex].keyId)
        {
            keyToUse = static_cast<uint8_t>(MacKeyType::CURRENT);
        }
        else if (keyId == sMacKeys[instanceIndex].keyId + 1)
        {
            keyToUse = static_cast<uint8_t>(MacKeyType::NEXT);
        }
        else
        {
            error = OT_ERROR_SECURITY;
            otEXPECT(false);
        }
    }
    else
    {
        keyId    = sMacKeys[instanceIndex].keyId;
        keyToUse = static_cast<uint8_t>(MacKeyType::CURRENT);
    }

    aFrame->mInfo.mTxInfo.mAesKey = &sMacKeys[instanceIndex].keys[keyToUse];

    if (!aFrame->mInfo.mTxInfo.mIsHeaderUpdated)
    {
        if (otMacFrameIsAck(aFrame))
        {
            // Store ack frame counter and ack key ID for receive frame
            sMacKeys[instanceIndex].ackKeyId        = keyId;
            sMacKeys[instanceIndex].ackFrameCounter = sMacKeys[instanceIndex].macFrameCounter;
        }

        otMacFrameSetKeyId(aFrame, keyId);
        otMacFrameSetFrameCounter(aFrame, sMacKeys[instanceIndex].macFrameCounter++);
    }

    efr32PlatProcessTransmitAesCcm(aFrame, &sExtAddress[instanceIndex]);

exit:
    return error;
}

void sli_ot_radio_security_set_mac_key(otInstance             *aInstance,
                                       uint8_t                 aKeyIdMode,
                                       uint8_t                 aKeyId,
                                       const otMacKeyMaterial *aPrevKey,
                                       const otMacKeyMaterial *aCurrKey,
                                       const otMacKeyMaterial *aNextKey,
                                       otRadioKeyType          aKeyType)
{
    OT_UNUSED_VARIABLE(aKeyIdMode);
    OT_UNUSED_VARIABLE(aKeyType);

    instanceIndex_t index = sli_ot_radio_instance_get_index(aInstance);

    otEXPECT(sl_ot_rtos_task_can_access_pal());
    OT_ASSERT(aPrevKey != nullptr && aCurrKey != nullptr && aNextKey != nullptr);

    // MAC frame counters are reset before updating keys. This order
    // safeguards against issues that can arise when the radio
    // platform handles TX security and counter assignment.  The
    // radio platform might prepare an enhanced ACK to a received
    // frame from an parallel (e.g., ISR) context, which consumes
    // a MAC frame counter value.
    //
    // If the MAC key is updated before the frame counter is cleared,
    // the radio could receive and send an enhanced ACK between these
    // two actions, possibly using the new MAC key with a larger
    // (current) frame counter value. This could then prevent the
    // receiver from accepting subsequent transmissions after the
    // frame counter reset for a long time.
    //
    // While resetting counters first might briefly cause an enhanced
    // ACK to be sent with the old key and a zero counter (which might
    // be rejected by the receiver), this is a transient issue that
    // quickly resolves itself.
    sli_ot_radio_security_set_mac_frame_counter(aInstance, 0);

    sMacKeys[index].keyId = aKeyId;
    memcpy(&sMacKeys[index].keys[static_cast<int>(MacKeyType::PREV)], aPrevKey, sizeof(otMacKeyMaterial));
    memcpy(&sMacKeys[index].keys[static_cast<int>(MacKeyType::CURRENT)], aCurrKey, sizeof(otMacKeyMaterial));
    memcpy(&sMacKeys[index].keys[static_cast<int>(MacKeyType::NEXT)], aNextKey, sizeof(otMacKeyMaterial));

#if (OPENTHREAD_CONFIG_CRYPTO_LIB == OPENTHREAD_CONFIG_CRYPTO_LIB_PSA)
    size_t  aKeyLen;
    otError error;

    error = otPlatCryptoExportKey(sMacKeys[index].keys[static_cast<int>(MacKeyType::PREV)].mKeyMaterial.mKeyRef,
                                  sMacKeys[index].keys[static_cast<int>(MacKeyType::PREV)].mKeyMaterial.mKey.m8,
                                  sizeof(sMacKeys[index].keys[static_cast<int>(MacKeyType::PREV)]),
                                  &aKeyLen);
    OT_ASSERT(error == OT_ERROR_NONE);

    error = otPlatCryptoExportKey(sMacKeys[index].keys[static_cast<int>(MacKeyType::CURRENT)].mKeyMaterial.mKeyRef,
                                  sMacKeys[index].keys[static_cast<int>(MacKeyType::CURRENT)].mKeyMaterial.mKey.m8,
                                  sizeof(sMacKeys[index].keys[static_cast<int>(MacKeyType::CURRENT)]),
                                  &aKeyLen);
    OT_ASSERT(error == OT_ERROR_NONE);

    error = otPlatCryptoExportKey(sMacKeys[index].keys[static_cast<int>(MacKeyType::NEXT)].mKeyMaterial.mKeyRef,
                                  sMacKeys[index].keys[static_cast<int>(MacKeyType::NEXT)].mKeyMaterial.mKey.m8,
                                  sizeof(sMacKeys[index].keys[static_cast<int>(MacKeyType::NEXT)]),
                                  &aKeyLen);
    OT_ASSERT(error == OT_ERROR_NONE);
#endif

exit:
    return;
}

void sli_ot_radio_security_set_mac_frame_counter(otInstance *aInstance, uint32_t aMacFrameCounter)
{
    instanceIndex_t index = sli_ot_radio_instance_get_index(aInstance);

    otEXPECT(sl_ot_rtos_task_can_access_pal());

    CORE_DECLARE_IRQ_STATE;
    CORE_ENTER_ATOMIC();

    sMacKeys[index].macFrameCounter = aMacFrameCounter;

    CORE_EXIT_ATOMIC();

exit:
    return;
}

void sli_ot_radio_security_set_mac_frame_counter_if_larger(otInstance *aInstance, uint32_t aMacFrameCounter)
{
    instanceIndex_t index = sli_ot_radio_instance_get_index(aInstance);
    otEXPECT(sl_ot_rtos_task_can_access_pal());

    CORE_DECLARE_IRQ_STATE;
    CORE_ENTER_ATOMIC();

    if (aMacFrameCounter > sMacKeys[index].macFrameCounter)
    {
        sMacKeys[index].macFrameCounter = aMacFrameCounter;
    }

    CORE_EXIT_ATOMIC();

exit:
    return;
}

uint8_t sli_ot_radio_security_get_ack_key_id(otInstance *aInstance)
{
    instanceIndex_t index = sli_ot_radio_instance_get_index(aInstance);
    return sMacKeys[index].ackKeyId;
}

uint32_t sli_ot_radio_security_get_ack_frame_counter(otInstance *aInstance)
{
    instanceIndex_t index = sli_ot_radio_instance_get_index(aInstance);
    return sMacKeys[index].ackFrameCounter;
}

} // extern "C"

#endif // (OPENTHREAD_CONFIG_THREAD_VERSION >= OT_THREAD_VERSION_1_2)
