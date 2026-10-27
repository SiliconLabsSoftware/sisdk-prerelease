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
 *   Transmit AES-CCM orchestration shared by RADIOAES and LPWAES backends.
 */

#include <openthread-core-config.h>

#include SL_OT_MAC_KEY_POLICY_CONFIG_HEADER
#include "mac-key-types.hpp"
#include "tx-aes-ccm-api.hpp"
#include "tx-aes-ccm.hpp"

#include "common/code_utils.hpp"
#include "crypto/aes_ccm.hpp"
#include "mac/mac_frame.hpp"

using namespace ot;

otError sli_ot_process_transmit_aes_ccm(otRadioFrame                      *aFrame,
                                        const otExtAddress                *aExtAddress,
                                        const MacKeyStoragePolicy::PalKey *aPalKey)
{
#if (OPENTHREAD_RADIO && (OPENTHREAD_CONFIG_THREAD_VERSION < OT_THREAD_VERSION_1_2))
    OT_UNUSED_VARIABLE(aFrame);
    OT_UNUSED_VARIABLE(aExtAddress);
    OT_UNUSED_VARIABLE(aPalKey);

    return OT_ERROR_NONE;
#else

    otError                 error   = OT_ERROR_NONE;
    const Mac::TxFrame     &txFrame = *static_cast<const Mac::TxFrame *>(aFrame);
    Mac::TxFrame::ParseInfo info;

    error = info.ParseFrom(txFrame, Mac::Frame::kParseSecurityHeader);
    if (error == OT_ERROR_NOT_FOUND)
    {
        ExitNow(error = OT_ERROR_NONE);
    }
    SuccessOrExit(error);

    error = sli_ot_process_transmit_aes_ccm_with_parse_info(aFrame, aExtAddress, aPalKey, &info);

exit:
    return error;
#endif // OPENTHREAD_RADIO && (OPENTHREAD_CONFIG_THREAD_VERSION < OT_THREAD_VERSION_1_2)
}

otError sli_ot_process_transmit_aes_ccm_with_parse_info(otRadioFrame                      *aFrame,
                                                        const otExtAddress                *aExtAddress,
                                                        const MacKeyStoragePolicy::PalKey *aPalKey,
                                                        const Mac::TxFrame::ParseInfo     *aParseInfo)
{
#if (OPENTHREAD_RADIO && (OPENTHREAD_CONFIG_THREAD_VERSION < OT_THREAD_VERSION_1_2))
    OT_UNUSED_VARIABLE(aFrame);
    OT_UNUSED_VARIABLE(aExtAddress);
    OT_UNUSED_VARIABLE(aPalKey);
    OT_UNUSED_VARIABLE(aParseInfo);

    return OT_ERROR_NONE;
#else

    otError               error = OT_ERROR_NONE;
    Crypto::AesCcm::Nonce nonce;
    Mac::TxFrame         &txFrame = *static_cast<Mac::TxFrame *>(aFrame);

    VerifyOrExit(aParseInfo != nullptr, error = OT_ERROR_INVALID_ARGS);

    // Nothing to secure if the frame has no Auxiliary Security Header.
    VerifyOrExit(aParseInfo->mParsedSecurityHeader);

    VerifyOrExit(aPalKey != nullptr, error = OT_ERROR_INVALID_ARGS);

    nonce.InitFrom(*static_cast<const Mac::ExtAddress *>(aExtAddress),
                   aParseInfo->mFrameCounter,
                   static_cast<uint8_t>(aParseInfo->mSecurityLevel));

    sli_ot_process_transmit_aes_ccm_device(txFrame, nonce, aParseInfo->mMicSize, aParseInfo->mSecurityLevel, *aPalKey);

    txFrame.SetIsSecurityProcessed(true);

exit:
    return error;
#endif // OPENTHREAD_RADIO && (OPENTHREAD_CONFIG_THREAD_VERSION < OT_THREAD_VERSION_1_2)
}
