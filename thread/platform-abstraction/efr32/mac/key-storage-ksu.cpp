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
 *   KSU MAC key storage policy implementation.
 */

#include <openthread-core-config.h>

#if defined(LPWAES_PRESENT) && defined(KSU_PRESENT)

#include "em_device.h"
#include "key-storage-policy.hpp"
#include "security_manager.h"
#include "common/debug.hpp"
#include "utils/code_utils.h"

#include <psa/crypto.h>

namespace {

bool AreKeyRefsUnique(const otMacKeyMaterial (&aKeys)[KsuMacKeyStoragePolicy::kMacKeyCount])
{
    bool uniqueRefs = true;
    for (size_t i = 0; i < KsuMacKeyStoragePolicy::kMacKeyCount; ++i)
    {
        for (size_t j = i + 1; j < KsuMacKeyStoragePolicy::kMacKeyCount; ++j)
        {
            otEXPECT_ACTION(aKeys[i].mKeyMaterial.mKeyRef != aKeys[j].mKeyMaterial.mKeyRef, uniqueRefs = false);
        }
    }

exit:
    return uniqueRefs;
}

} // namespace

void KsuMacKeyStoragePolicy::PrepareKeys(otMacKeyMaterial (&aKeys)[kMacKeyCount],
                                         const otMacKeyMaterial (& /* aRawKeys */)[kMacKeyCount])
{
    for (otMacKeyMaterial &key : aKeys)
    {
        const psa_key_id_t sourceKeyId = key.mKeyMaterial.mKeyRef;
        psa_key_id_t       ksuKeyId    = 0;
        uint8_t            ksuSlot     = 0xFF;
        const psa_status_t status      = sl_sec_man_copy_key_to_ksu(sourceKeyId, &ksuKeyId, &ksuSlot);

        OT_ASSERT(status == PSA_SUCCESS && ksuKeyId != 0);

        key.mKeyMaterial.mKeyRef = ksuKeyId;
    }

    OT_ASSERT(AreKeyRefsUnique(aKeys));
}

void KsuMacKeyStoragePolicy::ReleaseKeys(const otMacKeyMaterial (&aKeys)[kMacKeyCount])
{
    for (const otMacKeyMaterial &key : aKeys)
    {
        const psa_key_id_t keyRef = key.mKeyMaterial.mKeyRef;

        if (keyRef != 0)
        {
            sl_sec_man_unregister_ksu_key(keyRef);
        }
    }
}

#endif // defined(LPWAES_PRESENT) && defined(KSU_PRESENT)
