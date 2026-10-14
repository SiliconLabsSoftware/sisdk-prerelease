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

#include "platform-efr32.h"
#include "radio_instance.h"

extern "C" {
#include "sl_core.h"
#include "sl_packet_utils.h"
}

#include SL_OT_MAC_KEY_POLICY_CONFIG_HEADER

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
    uint8_t           ackKeyId;
    uint8_t           keyId;
    volatile uint32_t macFrameCounter;
    volatile uint32_t ackFrameCounter;
    otMacKeyMaterial  keys[static_cast<int>(MacKeyType::COUNT)];
    // Plaintext copies for the platform's TX/Enhanced-ACK encryption.
    // Kept separate from the valid PSA key references above.
    // OpenThread core can read `keys[]` back via `Frame::GetAesKey()`/
    // `otPlatCryptoAesSetKey()` in `TxFrame::RestoreTransmitSecurity()`
    // on MAC-header-IE retries, and can misread clobbered raw key bytes
    // as a faulty PSA handle.
    otMacKeyMaterial rawKeys[static_cast<int>(MacKeyType::COUNT)];
    volatile bool    keyUpdateInProgress;
};

static_assert(static_cast<size_t>(MacKeyType::COUNT) == MacKeyStoragePolicy::kMacKeyCount,
              "MacKeyType::COUNT differs from MacKeyStoragePolicy::kMacKeyCount");

// Per-instance security material
static securityMaterial sMacKeys[RADIO_INTERFACE_COUNT];

// External declarations
extern otExtAddress sExtAddress[RADIO_EXT_ADDR_COUNT];

static void setKeyUpdateInProgress(securityMaterial &aMaterial, bool aInProgress)
{
    CORE_ATOMIC_SECTION(aMaterial.keyUpdateInProgress = aInProgress;)
}

extern "C" {

void sli_ot_radio_security_init(void)
{
    // Initialize security material for all instances
    memset(sMacKeys, 0, sizeof(sMacKeys));
}

void sli_ot_radio_security_deinit(void)
{
    for (securityMaterial &material : sMacKeys)
    {
        setKeyUpdateInProgress(material, true);
    }

#if (OPENTHREAD_CONFIG_CRYPTO_LIB == OPENTHREAD_CONFIG_CRYPTO_LIB_PSA)
    for (const securityMaterial &material : sMacKeys)
    {
        MacKeyStoragePolicy::ReleaseKeys(material.keys);
    }
#endif // PSA crypto lib

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

    otEXPECT_ACTION(!sMacKeys[instanceIndex].keyUpdateInProgress, error = OT_ERROR_INVALID_STATE);

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

    // `mAesKey` points at the PSA keyRef entry that OpenThread core reads
    // back via `Frame::GetAesKey()` on MAC-header-IE retries, so it must
    // never be in plaintext.
    //
    // The plaintext bytes needed for this platform's own HW CCM encryption
    // are passed explicitly below.
    aFrame->mInfo.mTxInfo.mAesKey = &sMacKeys[instanceIndex].keys[keyToUse];

    if (!aFrame->mInfo.mTxInfo.mIsHeaderUpdated)
    {
        uint32_t frameCounter;
        CORE_DECLARE_IRQ_STATE;

        CORE_ENTER_ATOMIC();

        frameCounter                            = sMacKeys[instanceIndex].macFrameCounter;
        sMacKeys[instanceIndex].macFrameCounter = frameCounter + 1;

        if (otMacFrameIsAck(aFrame))
        {
            // Store ack frame counter and ack key ID for receive frame
            sMacKeys[instanceIndex].ackKeyId        = keyId;
            sMacKeys[instanceIndex].ackFrameCounter = frameCounter;
        }

        CORE_EXIT_ATOMIC();

        otMacFrameSetKeyId(aFrame, keyId);
        otMacFrameSetFrameCounter(aFrame, frameCounter);
    }

    error = sli_ot_process_transmit_aes_ccm(aFrame,
                                            &sExtAddress[instanceIndex],
                                            &sMacKeys[instanceIndex].rawKeys[keyToUse]);

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

    // Signal to key users that the key state is invalid if pre-empted before update is complete
    setKeyUpdateInProgress(sMacKeys[index], true);

#if (OPENTHREAD_CONFIG_CRYPTO_LIB == OPENTHREAD_CONFIG_CRYPTO_LIB_PSA)
    // Release previously prepared material before overwriting with new source
    // key refs.
    MacKeyStoragePolicy::ReleaseKeys(sMacKeys[index].keys);
#endif

    sMacKeys[index].keyId = aKeyId;
    memcpy(&sMacKeys[index].keys[static_cast<int>(MacKeyType::PREV)], aPrevKey, sizeof(otMacKeyMaterial));
    memcpy(&sMacKeys[index].keys[static_cast<int>(MacKeyType::CURRENT)], aCurrKey, sizeof(otMacKeyMaterial));
    memcpy(&sMacKeys[index].keys[static_cast<int>(MacKeyType::NEXT)], aNextKey, sizeof(otMacKeyMaterial));

#if (OPENTHREAD_CONFIG_CRYPTO_LIB == OPENTHREAD_CONFIG_CRYPTO_LIB_PSA)
    // Export PSA keyRefs into rawKeys[] for HW CCM while leaving keys[] as
    // valid references for SubMac retx restore.
    MacKeyStoragePolicy::PrepareKeys(sMacKeys[index].keys, sMacKeys[index].rawKeys);
#else
    // RCP builds use CRYPTO_LIB_MBEDTLS and receive literal MAC keys over
    // Spinel. HW CCM always reads rawKeys[], so mirror the literals there.
    // Do not touch keys[] — process_transmit still points mAesKey at keys[].
    memcpy(sMacKeys[index].rawKeys, sMacKeys[index].keys, sizeof(sMacKeys[index].rawKeys));
#endif

    // Signal to key users that the key state is now valid
    setKeyUpdateInProgress(sMacKeys[index], false);

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

} // extern

#endif // (OPENTHREAD_CONFIG_THREAD_VERSION >= OT_THREAD_VERSION_1_2)
