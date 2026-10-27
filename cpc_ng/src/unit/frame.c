/***************************************************************************/ /**
 * @file frame.c
 * @brief CPC frame description helpers for unit tests.
 *******************************************************************************
 * # License
 * <b>Copyright 2026 Silicon Laboratories Inc. www.silabs.com</b>
 *******************************************************************************
 *
 * SPDX-License-Identifier: Zlib
 *
 * The licensor of this software is Silicon Laboratories Inc.
 *
 * This software is provided 'as-is', without any express or implied
 * warranty. In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 *
 ******************************************************************************/

#include <string.h>

#include "../sli_cpc_hdr.h"

#include "frame.h"

void frame_to_header(const cpc_frame_expect_t *frame, sli_cpc_hdr_t *hdr)
{
  memset(hdr, 0, sizeof(*hdr));
  hdr->ctrl = (uint8_t)(frame->flags & CPC_EXPECT_FLAGS_MASK);
  sli_cpc_header_set_address(hdr, (uint16_t)frame->dst);
  hdr->seq = (uint8_t)frame->seq;
  hdr->ack = (uint8_t)frame->ack;
  hdr->rx_wnd = (uint8_t)frame->wnd;
  sli_cpc_header_set_payload_size(hdr, (uint16_t)frame->len);
}
