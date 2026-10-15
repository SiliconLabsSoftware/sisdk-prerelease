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
 *   Transmit AES-CCM entry point (C++).
 */

#ifndef TX_AES_CCM_API_HPP_
#define TX_AES_CCM_API_HPP_

#include <openthread/error.h>
#include <openthread/platform/radio.h>

#include SL_OT_MAC_KEY_POLICY_CONFIG_HEADER
#include "mac-key-types.hpp"

/**
 * Applies AES-CCM to a transmit frame.
 *
 * @param[in,out] aFrame       MAC frame buffer to secure.
 * @param[in]     aExtAddress  Extended address used to build the CCM nonce.
 * @param[in]     aPalKey      PAL-side key material for the selected slot;
 *                             must not be `nullptr`.
 *
 * @retval OT_ERROR_NONE          Frame secured, or security not enabled on the frame.
 * @retval OT_ERROR_INVALID_ARGS  `aPalKey` was `nullptr`.
 * @retval OT_ERROR_PARSE         Frame could not be parsed; do not transmit.
 */
otError sli_ot_process_transmit_aes_ccm(otRadioFrame                      *aFrame,
                                        const otExtAddress                *aExtAddress,
                                        const MacKeyStoragePolicy::PalKey *aPalKey);

#endif // TX_AES_CCM_API_HPP_
