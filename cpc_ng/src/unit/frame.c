/***************************************************************************/ /**
 * @file
 * @brief CPC frame description helpers for unit tests.
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
