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
 *   Key Storage Unit (KSU) MAC key storage policy implementation.
 */

#include "em_device.h"
#include <openthread-core-config.h>

#if defined(LPWAES_PRESENT) && defined(KSU_PRESENT) \
    && (OPENTHREAD_CONFIG_CRYPTO_LIB == OPENTHREAD_CONFIG_CRYPTO_LIB_PSA)

#include "key-storage-policy.hpp"
#include "security_manager.h"

#include "common/debug.hpp"
#include "utils/code_utils.h"

#include <psa/crypto.h>

namespace {

bool AreKsuKeyRefsUnique(const KsuMacKeyStoragePolicy::PalKeyList &aPalKeys)
{
    bool uniqueRefs = true;

    for (size_t i = 0; i < KsuMacKeyStoragePolicy::kMacKeyCount; ++i)
    {
        for (size_t j = i + 1; j < KsuMacKeyStoragePolicy::kMacKeyCount; ++j)
        {
            otEXPECT_ACTION(aPalKeys[i].mKeyRef != aPalKeys[j].mKeyRef, uniqueRefs = false);
        }
    }

exit:
    return uniqueRefs;
}

bool IsReleased(const KsuMacKeyStoragePolicy::PalKeyList &aPalKeys)
{
    for (const KsuMacKeyStoragePolicy::PalKey &key : aPalKeys)
    {
        if (key.mKeyRef != 0)
        {
            return false;
        }
    }
    return true;
}

} // namespace

template <> void KsuMacKeyStoragePolicy::ReleaseKeys(PalKeyList &aPalKeys)
{
    for (PalKey &key : aPalKeys)
    {
        if (key.mKeyRef != 0)
        {
            sl_sec_man_unregister_ksu_key(key.mKeyRef);
            key.mKeyRef = 0;
        }
    }
}

template <> otError KsuMacKeyStoragePolicy::InstallKeys(const StackKeyList &aStackKeys, PalKeyList &aPalKeys)
{
    otError error = OT_ERROR_NONE;

    otEXPECT_ACTION(IsReleased(aPalKeys), error = OT_ERROR_INVALID_STATE);

    for (size_t i = 0; i < kMacKeyCount; i++)
    {
        psa_key_id_t       ksuKeyId = 0;
        uint8_t            ksuSlot  = 0xFF;
        const psa_status_t status   = sl_sec_man_copy_key_to_ksu(aStackKeys[i].mKeyRef, &ksuKeyId, &ksuSlot);

        otEXPECT_ACTION(status == PSA_SUCCESS && ksuKeyId != 0, error = OT_ERROR_FAILED);

        aPalKeys[i].mKeyRef = ksuKeyId;
    }

    OT_ASSERT(AreKsuKeyRefsUnique(aPalKeys));

exit:
    // Roll back partial install; aPalKeys is untouched on OT_ERROR_INVALID_STATE.
    if (error != OT_ERROR_NONE && error != OT_ERROR_INVALID_STATE)
    {
        ReleaseKeys(aPalKeys);
    }
    return error;
}

#endif // defined(LPWAES_PRESENT) && defined(KSU_PRESENT) && PSA
