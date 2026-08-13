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
 *   RADIOAES transmit AES-CCM backend.
 */

#include "em_device.h"
#include <openthread-core-config.h>

#if defined(RADIOAES_PRESENT)

#include "sli_protocol_crypto.h"
#include "tx-security-level.hpp"

#include "common/code_utils.hpp"
#include "common/debug.hpp"
#include "crypto/aes_ccm.hpp"
#include "mac/mac_frame.hpp"
#include "openthread/platform/radio.h"

using namespace ot;

class TxSecurityProcessing
{
public:
    enum
    {
        kBlockSize = 16, ///< AES-128 block size (bytes).
        kKeyBits   = 128
    };

    void    SetKey(const uint8_t (&aKey)[OT_MAC_KEY_SIZE]);
    void    Init(uint32_t       aHeaderLength,
                 uint32_t       aPlainTextLength,
                 const uint8_t  aTagLength,
                 const uint8_t *aNonce,
                 const uint8_t  aNonceLength);
    void    Header(const uint8_t *aHeader, const uint32_t aHeaderLength);
    void    Payload(const uint8_t *aPlainText, uint8_t *aCipherText, const uint32_t aLength);
    void    PayloadAuthenticateOnly(const uint8_t *aPlainText, const uint32_t aLength);
    uint8_t GetTagLength(void) const;
    void    Finalize(uint8_t *aTag);

private:
    void AuthenticatePayloadByte(const uint8_t aByte);
    void CompletePayloadProcessing(void);

    uint8_t        mBlock[kBlockSize];
    uint8_t        mCtr[kBlockSize];
    uint8_t        mCtrPad[kBlockSize];
    const uint8_t *mKey;
    uint32_t       mHeaderLength;
    uint32_t       mHeaderCur;
    uint32_t       mPlainTextLength;
    uint32_t       mPlainTextCur;
    uint16_t       mBlockLength;
    uint16_t       mCtrLength;
    uint8_t        mNonceLength;
    uint8_t        mTagLength;
};

void TxSecurityProcessing::SetKey(const uint8_t (&aKey)[OT_MAC_KEY_SIZE])
{
    mKey = aKey;
}

void TxSecurityProcessing::Init(uint32_t       aHeaderLength,
                                uint32_t       aPlainTextLength,
                                const uint8_t  aTagLength,
                                const uint8_t *aNonce,
                                const uint8_t  aNonceLength)
{
    const uint8_t *const nonceBytes  = aNonce;
    uint8_t              blockLength = 0;
    uint32_t             len;
    uint8_t              L;
    uint8_t              i;

    // Tag length must be even and within [kMinTagLength, kMaxTagLength]
    OT_ASSERT(((aTagLength & 0x1) == 0) && (Crypto::AesCcm::kMinTagLength <= aTagLength)
              && (aTagLength <= Crypto::AesCcm::kMaxTagLength));

    L = 0;

    for (len = aPlainTextLength; len; len >>= 8)
    {
        L++;
    }

    if (L <= 1)
    {
        L = 2;
    }

    uint8_t nonceLength = aNonceLength;

    if (nonceLength > 13)
    {
        nonceLength = 13;
    }

    // increase L to match nonce len
    if (L < (15 - nonceLength))
    {
        L = 15 - nonceLength;
    }

    // decrease nonceLength to match L
    if (nonceLength > (15 - L))
    {
        nonceLength = 15 - L;
    }

    // setup initial block

    // write flags
    mBlock[0] = (static_cast<uint8_t>((aHeaderLength != 0) << 6) | static_cast<uint8_t>(((aTagLength - 2) >> 1) << 3)
                 | static_cast<uint8_t>(L - 1));

    // write nonce
    memcpy(&mBlock[1], nonceBytes, nonceLength);

    // write len
    len = aPlainTextLength;

    for (i = sizeof(mBlock) - 1; i > nonceLength; i--)
    {
        mBlock[i] = len & 0xff;
        len >>= 8;
    }

    // encrypt initial block
    sli_aes_crypt_ecb_radio(true, mKey, kKeyBits, mBlock, mBlock);

    // process header
    if (aHeaderLength > 0)
    {
        // process length
        if (aHeaderLength < (65536U - 256U))
        {
            mBlock[blockLength++] ^= aHeaderLength >> 8;
            mBlock[blockLength++] ^= aHeaderLength >> 0;
        }
        else
        {
            mBlock[blockLength++] ^= 0xff;
            mBlock[blockLength++] ^= 0xfe;
            mBlock[blockLength++] ^= aHeaderLength >> 24;
            mBlock[blockLength++] ^= aHeaderLength >> 16;
            mBlock[blockLength++] ^= aHeaderLength >> 8;
            mBlock[blockLength++] ^= aHeaderLength >> 0;
        }
    }

    // init counter
    mCtr[0] = L - 1;
    memcpy(&mCtr[1], nonceBytes, nonceLength);
    memset(&mCtr[nonceLength + 1], 0, sizeof(mCtr) - nonceLength - 1);

    mNonceLength     = nonceLength;
    mHeaderLength    = aHeaderLength;
    mHeaderCur       = 0;
    mPlainTextLength = aPlainTextLength;
    mPlainTextCur    = 0;
    mBlockLength     = blockLength;
    mCtrLength       = sizeof(mCtrPad);
    mTagLength       = aTagLength;
}

void TxSecurityProcessing::Header(const uint8_t *aHeader, const uint32_t aHeaderLength)
{
    const uint8_t *const headerBytes = aHeader;

    OT_ASSERT(mHeaderCur + aHeaderLength <= mHeaderLength);

    // process header
    for (unsigned i = 0; i < aHeaderLength; i++)
    {
        if (mBlockLength == sizeof(mBlock))
        {
            sli_aes_crypt_ecb_radio(true, mKey, kKeyBits, mBlock, mBlock);

            mBlockLength = 0;
        }

        mBlock[mBlockLength++] ^= headerBytes[i];
    }

    mHeaderCur += aHeaderLength;

    if (mHeaderCur == mHeaderLength)
    {
        // process remainder
        if (mBlockLength != 0)
        {
            sli_aes_crypt_ecb_radio(true, mKey, kKeyBits, mBlock, mBlock);
        }

        mBlockLength = 0;
    }
}

void TxSecurityProcessing::AuthenticatePayloadByte(const uint8_t aByte)
{
    if (mBlockLength == sizeof(mBlock))
    {
        sli_aes_crypt_ecb_radio(true, mKey, kKeyBits, mBlock, mBlock);

        mBlockLength = 0;
    }

    mBlock[mBlockLength++] ^= aByte;
}

void TxSecurityProcessing::CompletePayloadProcessing(void)
{
    if (mPlainTextCur >= mPlainTextLength)
    {
        if (mBlockLength != 0)
        {
            sli_aes_crypt_ecb_radio(true, mKey, kKeyBits, mBlock, mBlock);
        }

        // reset counter
        if (mNonceLength + 1 < kBlockSize)
        {
            memset(&mCtr[mNonceLength + 1], 0, sizeof(mCtr) - mNonceLength - 1);
        }
    }
}

void TxSecurityProcessing::Payload(const uint8_t *aPlainText, uint8_t *aCipherText, const uint32_t aLength)
{
    OT_ASSERT(mPlainTextCur + aLength <= mPlainTextLength);

    for (unsigned i = 0; i < aLength; i++)
    {
        if (mCtrLength == 16)
        {
            for (int j = sizeof(mCtr) - 1; j > mNonceLength; j--)
            {
                if (++mCtr[j])
                {
                    break;
                }
            }

            sli_aes_crypt_ecb_radio(true, mKey, kKeyBits, mCtr, mCtrPad);

            mCtrLength = 0;
        }

        const uint8_t byte = aPlainText[i];

        aCipherText[i] = byte ^ mCtrPad[mCtrLength++];
        AuthenticatePayloadByte(byte);
    }

    mPlainTextCur += aLength;
    CompletePayloadProcessing();
}

void TxSecurityProcessing::PayloadAuthenticateOnly(const uint8_t *aPlainText, const uint32_t aLength)
{
    OT_ASSERT(mPlainTextCur + aLength <= mPlainTextLength);

    for (unsigned i = 0; i < aLength; i++)
    {
        AuthenticatePayloadByte(aPlainText[i]);
    }

    mPlainTextCur += aLength;
    CompletePayloadProcessing();
}

uint8_t TxSecurityProcessing::GetTagLength(void) const
{
    return mTagLength;
}

void TxSecurityProcessing::Finalize(uint8_t *aTag)
{
    OT_ASSERT(mPlainTextCur == mPlainTextLength);

    sli_aes_crypt_ecb_radio(true, mKey, kKeyBits, mCtr, mCtrPad);

    for (int i = 0; i < mTagLength; i++)
    {
        aTag[i] = mBlock[i] ^ mCtrPad[i];
    }
}

void sli_ot_process_transmit_aes_ccm_device(otRadioFrame &aFrame,
                                            const uint8_t (&aNonce)[Crypto::AesCcm::kNonceSize],
                                            const uint8_t                   aTagLength,
                                            const Mac::Frame::SecurityLevel aSecurityLevel)
{
    Mac::TxFrame           &txFrame       = static_cast<Mac::TxFrame &>(aFrame);
    const uint32_t          payloadLength = txFrame.GetPayloadLength();
    const uint8_t *const    payload       = txFrame.GetPayload();
    const otMacKeyMaterial &aesKey        = *aFrame.mInfo.mTxInfo.mAesKey;

    TxSecurityProcessing handler;

    handler.SetKey(aesKey.mKeyMaterial.mKey.m8);
    handler.Init(txFrame.GetHeaderLength(), payloadLength, aTagLength, aNonce, Crypto::AesCcm::kNonceSize);
    handler.Header(txFrame.GetHeader(), txFrame.GetHeaderLength());

    if (payloadLength > 0)
    {
        if (TxSecurityLevel::EncryptsPayload(aSecurityLevel))
        {
            handler.Payload(payload, txFrame.GetPayload(), payloadLength);
        }
        else if (TxSecurityLevel::AuthenticatesPayloadWithoutEncryption(aSecurityLevel))
        {
            handler.PayloadAuthenticateOnly(payload, payloadLength);
        }
    }

    handler.Finalize(txFrame.GetFooter());
}

#endif // defined(RADIOAES_PRESENT)
