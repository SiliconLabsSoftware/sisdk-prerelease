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
 *   IEEE 802.15.4 frame helper functions.
 */

#include <openthread-core-config.h>

#include "sl_packet_utils.h"

#include "common/code_utils.hpp"
#include "common/encoding.hpp"
#include "mac/mac_frame.hpp"

using namespace ot;

static constexpr uint16_t kFcfPanidCompressionMask = 1 << 6;

static inline Error ParseAddrFields(const otRadioFrame *aFrame, Mac::Frame::ParseInfo &aFrameInfo)
{
    return aFrameInfo.ParseFrom(*static_cast<const Mac::Frame *>(aFrame), Mac::Frame::kParseAddrFields);
}

extern "C" {

bool sli_ot_is_frame_pending(const otRadioFrame *aFrame)
{
    Mac::RxFrame::ParseInfo info;
    bool                    isFramePending = false;

    SuccessOrExit(ParseAddrFields(aFrame, info));
    isFramePending = info.mIsFramePending;

exit:
    return isFramePending;
}

otPanId sli_ot_get_dst_pan_id(const otRadioFrame *aFrame)
{
    Mac::RxFrame::ParseInfo info;
    otPanId                 panId = 0xFFFF;

    SuccessOrExit(ParseAddrFields(aFrame, info));

    if (info.mPanIds.IsDestinationPresent())
    {
        panId = info.mPanIds.GetDestination();
    }

exit:
    return panId;
}

const uint8_t *sli_ot_get_payload(const otRadioFrame *aFrame)
{
    const Mac::RxFrame &frame = static_cast<const Mac::RxFrame &>(*aFrame);
    Mac::Frame::Lengths lengths;
    const uint8_t      *payload = nullptr;

    SuccessOrExit(frame.DetermineLengths(lengths));
    payload = frame.GetPsduStartingAt(lengths.mHeader);

exit:
    return payload;
}

bool sli_ot_frame_is_pan_id_compressed(const otRadioFrame *aFrame)
{
    return (aFrame->mLength >= sizeof(uint16_t))
           && ((LittleEndian::ReadUint16(aFrame->mPsdu) & kFcfPanidCompressionMask) != 0);
}

uint16_t sli_ot_get_frame_version(const otRadioFrame *aFrame)
{
    Mac::RxFrame::ParseInfo info;
    uint16_t                version = 0;

    SuccessOrExit(ParseAddrFields(aFrame, info));
    version = static_cast<uint16_t>(info.mVersion);

exit:
    return version;
}

} // extern "C"
