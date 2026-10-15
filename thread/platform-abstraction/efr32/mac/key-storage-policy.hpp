/*
 *  Copyright (c) 2023, The OpenThread Authors.
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
 *   MAC key storage policies for the radio security key lifecycle.
 *
 *   Each policy is a `KeyStoragePolicy<StackKey, PalKey>` instantiation and
 *   exposes:
 *     `StackKey`     — key material as the stack hands it to the PAL.
 *     `StackKeyList` — `MacKeyList<StackKey>` for PREV/CURRENT/NEXT slots.
 *     `PalKey`       — key material as the PAL keeps it for the HW CCM engine.
 *     `PalKeyList`   — `MacKeyList<PalKey>` for PREV/CURRENT/NEXT slots.
 *     `InstallKeys` writes a `StackKeyList` into a released `PalKeyList`;
 *     returns `OT_ERROR_INVALID_STATE` if the target is not released, and
 *     rolls back on install failure.
 *     `ReleaseKeys` frees the resources owned by `PalKeyList`.
 *
 *   `StackKey::From()` converts an `otMacKeyMaterial` (as delivered by
 *   `otPlatRadioSetMacKey()`) into a `StackKey`.
 */

#ifndef KEY_STORAGE_POLICY_HPP_
#define KEY_STORAGE_POLICY_HPP_

#include <openthread/error.h>

#include "mac-key-types.hpp"

template <typename StackKeyType, typename PalKeyType> class KeyStoragePolicy
{
public:
    static constexpr size_t kMacKeyCount = kMacKeyPalCount;

    using StackKey     = StackKeyType;
    using PalKey       = PalKeyType;
    using StackKeyList = MacKeyList<StackKey>;
    using PalKeyList   = MacKeyList<PalKey>;

    static otError InstallKeys(const StackKeyList &aStackKeys, PalKeyList &aPalKeys);
    static void    ReleaseKeys(PalKeyList &aPalKeys);
};

/// Non-PSA builds: PAL and stack both use plaintext key bytes.
using LiteralMacKeyStoragePolicy = KeyStoragePolicy<MacKeyLiteral, MacKeyLiteral>;

/// PSA build, no KSU: stack holds a PSA key ref, PAL exports it to plaintext.
using PsaPlaintextMacKeyStoragePolicy = KeyStoragePolicy<MacKeyRef, MacKeyLiteral>;

/// PSA + KSU: each stack PSA key ref is copied into a dedicated KSU slot,
/// and the PAL keeps the KSU slot's key ref.
using KsuMacKeyStoragePolicy = KeyStoragePolicy<MacKeyRef, MacKeyRef>;

#endif // KEY_STORAGE_POLICY_HPP_
