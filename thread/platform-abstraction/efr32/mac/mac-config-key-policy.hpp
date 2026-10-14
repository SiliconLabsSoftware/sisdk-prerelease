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
 *   IEEE 802.15.4 MAC configuration key policy definitions.
 */

#ifndef MAC_CONFIG_KEY_POLICY_HPP_
#define MAC_CONFIG_KEY_POLICY_HPP_

#include <openthread-core-config.h>

#include "key-storage-policy.hpp"

#if defined(RADIOAES_PRESENT)
using MacKeyStoragePolicy = PlaintextMacKeyStoragePolicy;

#elif defined(LPWAES_PRESENT)
#include "tx-aes-ccm-lpwaes.hpp"

#if defined(KSU_PRESENT) && (OPENTHREAD_CONFIG_CRYPTO_LIB == OPENTHREAD_CONFIG_CRYPTO_LIB_PSA)
using MacKeyStoragePolicy = KsuMacKeyStoragePolicy;
#else
using MacKeyStoragePolicy = PlaintextMacKeyStoragePolicy;
#endif

using LpwAesTransmitAesCcm = LpwAesTransmitAesCcmT<MacKeyStoragePolicy>;

#endif // RADIOAES_PRESENT / LPWAES_PRESENT

#endif // MAC_CONFIG_KEY_POLICY_HPP_
