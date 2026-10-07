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
 *   Plaintext MAC key storage policy implementations.
 */

#include <openthread-core-config.h>

#include <string.h>

#include "key-storage-policy.hpp"

#include "utils/code_utils.h"

#include <openthread/platform/crypto.h>

namespace {

bool IsReleased(const MacKeyList<MacKeyLiteral> &aPalKeys)
{
    for (const MacKeyLiteral &key : aPalKeys)
    {
        for (uint8_t byte : key.mBytes)
        {
            if (byte != 0)
            {
                return false;
            }
        }
    }
    return true;
}

} // namespace

template <> void LiteralMacKeyStoragePolicy::ReleaseKeys(PalKeyList &aPalKeys)
{
    memset(aPalKeys, 0, sizeof(aPalKeys));
}

template <> otError LiteralMacKeyStoragePolicy::InstallKeys(const StackKeyList &aStackKeys, PalKeyList &aPalKeys)
{
    otError error = OT_ERROR_NONE;

    otEXPECT_ACTION(IsReleased(aPalKeys), error = OT_ERROR_INVALID_STATE);

    for (size_t i = 0; i < kMacKeyCount; i++)
    {
        aPalKeys[i] = aStackKeys[i];
    }

exit:
    return error;
}

#if (OPENTHREAD_CONFIG_CRYPTO_LIB == OPENTHREAD_CONFIG_CRYPTO_LIB_PSA)

template <> void PsaPlaintextMacKeyStoragePolicy::ReleaseKeys(PalKeyList &aPalKeys)
{
    memset(aPalKeys, 0, sizeof(aPalKeys));
}

template <> otError PsaPlaintextMacKeyStoragePolicy::InstallKeys(const StackKeyList &aStackKeys, PalKeyList &aPalKeys)
{
    otError error = OT_ERROR_NONE;

    otEXPECT_ACTION(IsReleased(aPalKeys), error = OT_ERROR_INVALID_STATE);

    for (size_t i = 0; i < kMacKeyCount; i++)
    {
        size_t keyLen = 0;

        error = otPlatCryptoExportKey(aStackKeys[i].mKeyRef, aPalKeys[i].mBytes, OT_MAC_KEY_SIZE, &keyLen);
        otEXPECT(error == OT_ERROR_NONE);
        otEXPECT_ACTION(keyLen == OT_MAC_KEY_SIZE, error = OT_ERROR_FAILED);
    }

exit:
    // Roll back partial install; aPalKeys is untouched on OT_ERROR_INVALID_STATE.
    if (error != OT_ERROR_NONE && error != OT_ERROR_INVALID_STATE)
    {
        ReleaseKeys(aPalKeys);
    }
    return error;
}

#endif // OPENTHREAD_CONFIG_CRYPTO_LIB == OPENTHREAD_CONFIG_CRYPTO_LIB_PSA
