/*
 *  Copyright (c) 2026, The OpenThread Authors.
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
 *   LPWAES transmit AES-CCM backend.
 */

#ifndef TX_AES_CCM_LPWAES_HPP_
#define TX_AES_CCM_LPWAES_HPP_

#include "key-storage-policy.hpp"
#include "sli-crypto-key-desc.hpp"
#include "tx-security-level.hpp"

#include "sli_crypto.h"
#include "common/const_cast.hpp"
#include "common/debug.hpp"
#include "crypto/aes_ccm.hpp"
#include "mac/mac_frame.hpp"
#include "openthread/platform/radio.h"

template <typename KeyStoragePolicy> struct LpwAesKeyDescBuilder;

template <> struct LpwAesKeyDescBuilder<PlaintextMacKeyStoragePolicy>
{
    static sli_crypto_descriptor_t Build(const otMacKeyMaterial & /* aKey */, const otMacKeyMaterial &aRawKey)
    {
        return LpwAesKeyDescFromPlaintext(aRawKey);
    }
};

#if defined(KSU_PRESENT)
template <> struct LpwAesKeyDescBuilder<KsuMacKeyStoragePolicy>
{
    static sli_crypto_descriptor_t Build(const otMacKeyMaterial &aKey, const otMacKeyMaterial & /* aRawKey */)
    {
        return LpwAesKeyDescFromKsuSlot(aKey);
    }
};
#endif

template <typename KeyStoragePolicy> class LpwAesTransmitAesCcmT
{
public:
    static void Process(otRadioFrame                       &aFrame,
                        ot::Crypto::AesCcm::Nonce           aNonce,
                        const uint8_t                       aTagLength,
                        const ot::Mac::Frame::SecurityLevel aSecurityLevel,
                        const otMacKeyMaterial             *aRawKey);
};

template <typename KeyStoragePolicy>
void LpwAesTransmitAesCcmT<KeyStoragePolicy>::Process(otRadioFrame                       &aFrame,
                                                      ot::Crypto::AesCcm::Nonce           aNonce,
                                                      const uint8_t                       aTagLength,
                                                      const ot::Mac::Frame::SecurityLevel aSecurityLevel,
                                                      const otMacKeyMaterial             *aRawKey)
{
    ot::Mac::TxFrame       &txFrame = static_cast<ot::Mac::TxFrame &>(aFrame);
    ot::Mac::Frame::Lengths lengths;
    ot::FrameData           payload;

    SuccessOrAssert(txFrame.DetermineLengths(lengths));
    SuccessOrAssert(txFrame.GetPayload(payload));

    const uint32_t       payloadLength  = lengths.mPayload;
    unsigned char *const payloadBytes   = ot::AsNonConst(payload.GetBytes());
    const bool           encryptPayload = TxSecurityLevel::EncryptsPayload(aSecurityLevel);
    const bool includePayloadInCcm      = (payloadLength > 0) && TxSecurityLevel::IncludesPayloadInCcm(aSecurityLevel);
    const otMacKeyMaterial &aesKey      = *aFrame.mInfo.mTxInfo.mAesKey;
    sli_crypto_descriptor_t keyDesc     = LpwAesKeyDescBuilder<KeyStoragePolicy>::Build(aesKey, *aRawKey);
    sl_status_t             ret;

    ret = sli_crypto_ccm(&keyDesc,
                         true,
                         payloadBytes,
                         includePayloadInCcm ? payloadLength : 0,
                         encryptPayload ? payloadBytes : nullptr,
                         reinterpret_cast<const unsigned char *>(&aNonce),
                         sizeof(aNonce),
                         txFrame.GetPsdu(),
                         lengths.mHeader,
                         payloadBytes + payloadLength,
                         aTagLength);

    OT_ASSERT(ret == SL_STATUS_OK);
}

#endif // TX_AES_CCM_LPWAES_HPP_
