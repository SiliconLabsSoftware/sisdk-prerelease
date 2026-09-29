#ifndef FRAME_H
#define FRAME_H

#include "../sli_cpc_hdr.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Mnemonics for the bits set in `cpc_frame_expect_t::flags`.
 *
 * `flags` is matched against the masked control byte exactly, so the test
 * must include every flag bit that is expected to be set on the wire.
 */
#define CPC_EXPECT_FLAG_ACK_REQ SLI_CPC_CONTROL_REQUEST_ACK_Msk ///< Requests an ACK.
#define CPC_EXPECT_FLAG_RESET SLI_CPC_CONTROL_RESET_Msk         ///< Reset bit set.
#define CPC_EXPECT_FLAG_SYN SLI_CPC_CONTROL_SYN_Msk             ///< SYN bit set.

/// Mask of flag bits recognized by the matcher; bits outside this mask are
/// ignored when computing the actual control-byte value.
#define CPC_EXPECT_FLAGS_MASK (CPC_EXPECT_FLAG_ACK_REQ | CPC_EXPECT_FLAG_RESET | CPC_EXPECT_FLAG_SYN)

/**
 * @brief Description of an expected CPC frame.
 *
 * Every field is checked against the matching field of the captured frame
 * with an exact equality test. Use designated initializers to set only the
 * fields that matter; unset fields default to 0, which is usually the right
 * expectation for control frames.
 */
typedef struct cpc_frame_expect {
  /// Expected CPC control-byte flag bits. Bits outside `CPC_EXPECT_FLAGS_MASK`
  /// are ignored.
  int flags;
  /// Expected payload size in bytes.
  int len;
  /// Expected destination address (a.k.a. CPC endpoint id).
  int dst;
  /// Expected sequence number.
  int seq;
  /// Expected acknowledgement number.
  int ack;
  /// Expected RX window.
  int wnd;
  /// Expected payload, length must match @ref len.
  const void *payload;
} cpc_frame_expect_t;

/**
 * @brief Populate a CPC header from a frame description.
 */
void frame_to_header(const cpc_frame_expect_t *frame, sli_cpc_hdr_t *hdr);

#ifdef __cplusplus
}
#endif

#endif // FRAME_H
