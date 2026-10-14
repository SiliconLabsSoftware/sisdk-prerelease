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
 *   Shared transmit AES-CCM orchestration (nonce generation and backend dispatch).
 */

#include <openthread-core-config.h>

#include "sl_packet_utils.h"
#include "tx-aes-ccm.hpp"

#include "common/code_utils.hpp"
#include "crypto/aes_ccm.hpp"
#include "mac/mac_frame.hpp"

using namespace ot;

extern "C" {

otError sli_ot_process_transmit_aes_ccm(otRadioFrame           *aFrame,
                                        const otExtAddress     *aExtAddress,
                                        const otMacKeyMaterial *aRawKey)
{
#if (OPENTHREAD_RADIO && (OPENTHREAD_CONFIG_THREAD_VERSION < OT_THREAD_VERSION_1_2))
    OT_UNUSED_VARIABLE(aFrame);
    OT_UNUSED_VARIABLE(aExtAddress);
    OT_UNUSED_VARIABLE(aRawKey);

    return OT_ERROR_NONE;
#else

    otError                   error        = OT_ERROR_NONE;
    uint32_t                  frameCounter = 0;
    Mac::Frame::SecurityLevel securityLevel;
    Crypto::AesCcm::Nonce     nonce;
    Mac::TxFrame             &txFrame = *static_cast<Mac::TxFrame *>(aFrame);

    VerifyOrExit(txFrame.GetSecurityEnabled());

    SuccessOrExit(error = txFrame.GetSecurityLevel(securityLevel));
    SuccessOrExit(error = txFrame.GetFrameCounter(frameCounter));

    {
        Mac::Frame::Lengths lengths;

        SuccessOrExit(error = txFrame.DetermineLengths(lengths));

        const uint8_t tagLength = static_cast<uint8_t>(lengths.mFooter - txFrame.GetFcsSize());

        nonce.InitFrom(*static_cast<const Mac::ExtAddress *>(aExtAddress),
                       frameCounter,
                       static_cast<uint8_t>(securityLevel));

        sli_ot_process_transmit_aes_ccm_device(txFrame, nonce, tagLength, securityLevel, aRawKey);
    }

    txFrame.SetIsSecurityProcessed(true);

exit:
    return error;
#endif // OPENTHREAD_RADIO && (OPENTHREAD_CONFIG_THREAD_VERSION < OT_THREAD_VERSION_1_2)
}

} // extern "C"
