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
 *   IEEE 802.15.4 packet utilities for the OpenThread platform abstraction layer.
 */

#ifndef SL_PACKET_UTILS_H_
#define SL_PACKET_UTILS_H_

#include <openthread/platform/radio.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Performs AES-CCM on a transmit frame.
 *
 * @param[in,out] aFrame       A pointer to the MAC frame buffer that is going to be sent.
 * @param[in]     aExtAddress  A pointer to the extended address used to generate the AES-CCM nonce.
 * @param[in]     aRawKey      A pointer to the plaintext form of the MAC key
 *
 * @retval OT_ERROR_NONE   The frame was secured, or does not have security enabled.
 * @retval OT_ERROR_PARSE  The frame has security enabled but could not be parsed. The frame is
 *                         left unsecured and must not be transmitted.
 */
otError sli_ot_process_transmit_aes_ccm(otRadioFrame           *aFrame,
                                        const otExtAddress     *aExtAddress,
                                        const otMacKeyMaterial *aRawKey);

/**
 * Returns whether the Frame Pending bit is set.
 *
 * @param[in] aFrame  A pointer to the MAC frame buffer.
 *
 * @retval true   Frame Pending is set.
 * @retval false  Frame Pending is not set.
 */
bool sli_ot_is_frame_pending(const otRadioFrame *aFrame);

/**
 * Returns the Destination PanId, if present.
 *
 * @param[in] aFrame  A pointer to the MAC frame buffer.
 *
 * @returns Destination PanId if present, otherwise the broadcast PanId (0xFFFF).
 */
otPanId sli_ot_get_dst_pan_id(const otRadioFrame *aFrame);

/**
 * Returns a pointer to the start of the 802.15.4 payload.
 *
 * @param[in] aFrame  A pointer to the MAC frame buffer.
 *
 * @returns Pointer to the start of the 802.15.4 payload.
 */
const uint8_t *sli_ot_get_payload(const otRadioFrame *aFrame);

/**
 * Returns whether the PAN ID Compression bit is set.
 *
 * @param[in] aFrame  A pointer to the MAC frame buffer.
 */
bool sli_ot_frame_is_pan_id_compressed(const otRadioFrame *aFrame);

/**
 * Returns the frame version field.
 *
 * @param[in] aFrame  A pointer to the MAC frame buffer.
 */
uint16_t sli_ot_get_frame_version(const otRadioFrame *aFrame);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // SL_PACKET_UTILS_H_
