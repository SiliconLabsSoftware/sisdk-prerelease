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
 *   sli_crypto_descriptor_t builders for the LPWAES transmit security backend.
 */

#include "em_device.h"
#include <openthread-core-config.h>

#if defined(LPWAES_PRESENT)

#include <openthread/platform/crypto.h>

#include "sli-crypto-key-desc.hpp"

#if defined(KSU_PRESENT)
#include <psa/crypto.h>

#include "security_manager.h"
#include "common/debug.hpp"
#endif

sli_crypto_descriptor_t LpwAesKeyDescFromPlaintext(const otMacKeyMaterial &aRawKey)
{
    sli_crypto_descriptor_t keyDesc;

    keyDesc.engine                           = SLI_CRYPTO_LPWAES;
    keyDesc.yield                            = false;
    keyDesc.location                         = SLI_CRYPTO_KEY_LOCATION_PLAINTEXT;
    keyDesc.key.plaintext_key.buffer.pointer = const_cast<uint8_t *>(aRawKey.mKeyMaterial.mKey.m8);
    keyDesc.key.plaintext_key.buffer.size    = OT_MAC_KEY_SIZE;
    keyDesc.key.plaintext_key.key_size       = OT_MAC_KEY_SIZE;

    return keyDesc;
}

#if defined(KSU_PRESENT)

sli_crypto_descriptor_t LpwAesKeyDescFromKsuSlot(const otMacKeyMaterial &aKey)
{
    sli_crypto_descriptor_t keyDesc;
    uint8_t                 ksuSlot = 0xFF;
    psa_status_t            status;

    keyDesc.engine = SLI_CRYPTO_LPWAES;
    keyDesc.yield  = false;

    OT_ASSERT(aKey.mKeyMaterial.mKeyRef != 0);

    status = sl_sec_man_get_ksu_slot_for_key(aKey.mKeyMaterial.mKeyRef, &ksuSlot);
    OT_ASSERT(status == PSA_SUCCESS);

    keyDesc.location     = SLI_CRYPTO_KEY_LOCATION_KSU;
    keyDesc.key.key_slot = ksuSlot;

    return keyDesc;
}

#endif // defined(KSU_PRESENT)

#endif // defined(LPWAES_PRESENT)
