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
 *   LPWAES device hook for 802.15.4 transmit AES-CCM.
 */

#include "em_device.h"
#include <openthread-core-config.h>

#if defined(LPWAES_PRESENT)

#include SL_OT_MAC_KEY_POLICY_CONFIG_HEADER

void sli_ot_process_transmit_aes_ccm_device(otRadioFrame &aFrame,
                                            const uint8_t (&aNonce)[ot::Crypto::AesCcm::kNonceSize],
                                            const uint8_t                       aTagLength,
                                            const ot::Mac::Frame::SecurityLevel aSecurityLevel)
{
    LpwAesTransmitAesCcm::Process(aFrame, aNonce, aTagLength, aSecurityLevel);
}

template void LpwAesTransmitAesCcmT<PlaintextMacKeyStoragePolicy>::Process(
    otRadioFrame &aFrame,
    const uint8_t (&aNonce)[ot::Crypto::AesCcm::kNonceSize],
    const uint8_t                       aTagLength,
    const ot::Mac::Frame::SecurityLevel aSecurityLevel);

#if defined(KSU_PRESENT)
template void LpwAesTransmitAesCcmT<KsuMacKeyStoragePolicy>::Process(
    otRadioFrame &aFrame,
    const uint8_t (&aNonce)[ot::Crypto::AesCcm::kNonceSize],
    const uint8_t                       aTagLength,
    const ot::Mac::Frame::SecurityLevel aSecurityLevel);
#endif

#endif // defined(LPWAES_PRESENT)
