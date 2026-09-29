/***************************************************************************/ /**
 * @file
 * @brief Unit tests for CPC endpoint connect / SYN handshake.
 ******************************************************************************/

#include <stdlib.h>
#include <string.h>

#include <unity_fixture.h>
#include "unity.h"

#if defined(SL_COMPONENT_CATALOG_PRESENT)
#include "sl_component_catalog.h"
#endif

#include "sl_status.h"

#include "../sli_cpc.h"
#include "../sli_cpc_bus.h"
#include "../sli_cpc_ep.h"
#include "../sli_cpc_hdr.h"
#include "../sli_cpc_memory.h"
#include "sl_cpc_buf.h"
#include "sl_cpc_bus_instances.h"
#include "sl_cpc_drv_mock.h"

#include "../../port/mock/src/mock_cpc_timer.h"
#include "expect.h"
#include "fixture.h"

#define CPC_SYN_DUMMY_PAYLOAD {0xDE, 0xAD, 0xBE, 0xEF}

TEST_GROUP(cpc_syn);

TEST_SETUP(cpc_syn)
{
  TEST_CPC_SETUP(true);
}

TEST_TEAR_DOWN(cpc_syn)
{
  TEST_CPC_TEAR_DOWN();
}

static sl_cpc_buf_t *test_push_recv_buf(sl_cpc_ep_t *ep)
{
  sl_cpc_buf_t *buf;

  buf = sl_cpc_buf_alloc(CPC_TEST_RX_SIZE);
  TEST_ASSERT_NOT_NULL(buf);

  sl_cpc_ep_push_recv_buf(ep, buf);

  return buf;
}

/**
 * @brief Passive happy path: listen() -> CONNECTED.
 *
 * Drives a full three-way handshake from the listener side:
 *
 *   peer -> us: SYN(seq=R)               (matrix: OPEN + SYN -> SYN_RCVD)
 *   us  -> peer: SYN(seq=S, ack=R+1)
 *   peer -> us: ack-only DATA(ack=S+1)   (matrix: SYN_RCVD + DATA -> CONNECTED)
 *
 * Verifies that on each step the endpoint reaches the expected state and that
 * the peer's MTU is recorded during step 1.
 */
TEST(cpc_syn, listen_then_handshake_completes)
{
  sl_status_t status;
  sl_cpc_ep_t ep;

  status = sl_cpc_ep_init(&ep, CPC_TEST_EP_ID, CPC_TEST_RX_SIZE, cpc_test_event_cb, NULL);
  TEST_ASSERT_EQUAL(SL_STATUS_OK, status);

  status = sl_cpc_ep_listen(&ep, g_bus);
  TEST_ASSERT_EQUAL(SL_STATUS_OK, status);
  TEST_ASSERT_EQUAL(SLI_CPC_STATE_OPEN, ep.state);

  // Step 1 -- peer initiates the handshake.
  TEST_INJECT_FRAME({
    .flags = CPC_EXPECT_FLAG_ACK_REQ | CPC_EXPECT_FLAG_SYN,
    .len = CPC_TEST_PEER_MTU,
    .dst = CPC_TEST_EP_ID,
    .seq = CPC_TEST_PEER_SYN_SEQ,
    .ack = 0,
    .wnd = CPC_TEST_PEER_RX_WND,
  });

  // Step 2: Validate the frame emitted by the "Device Under Test", should be a
  // SYN whose ack acknowledges the peer's SYN. The sequence number we picked
  // for our own SYN is implementation-defined, so we capture it for use in the
  // third leg below.
  mock_cpc_tx_packet_t *dut_syn_ack = TEST_POP_OUTGOING({
    .flags = CPC_EXPECT_FLAG_ACK_REQ | CPC_EXPECT_FLAG_SYN,
    .len = CPC_TEST_RX_SIZE,
    .dst = CPC_TEST_EP_ID,
    .seq = -1,                                // can be any random value
    .ack = (int)(CPC_TEST_PEER_SYN_SEQ + 1U), // should acknowledge the injected SYN
    .wnd = 0, // should be the same as the number of RX buffer pushed before the call to listen
  });
  uint8_t our_syn_seq = sli_cpc_header_get_seq(&dut_syn_ack->header);
  mock_cpc_tx_packet_free(&dut_syn_ack);

  TEST_ASSERT_EQUAL(SLI_CPC_STATE_SYN_RCVD, ep.state);

  TEST_ASSERT_EQUAL_UINT16(CPC_TEST_PEER_MTU, ep.remote_mtu);

  // should be ready to sequence the next frame
  TEST_ASSERT_EQUAL_UINT8(our_syn_seq + 1, ep.send_nxt);
  // if SYN has sequence N, the endpoint expects for frame N+1
  TEST_ASSERT_EQUAL_UINT8(CPC_TEST_PEER_SYN_SEQ + 1U, ep.ack);
  TEST_ASSERT_EQUAL_UINT8(CPC_TEST_PEER_RX_WND, ep.send_wnd);

  // Step 3 -- peer completes the handshake with an ack-only DATA frame.
  TEST_INJECT_FRAME({
    .dst = CPC_TEST_EP_ID,
    .seq = (int)(CPC_TEST_PEER_SYN_SEQ + 1U),
    .ack = (int)(our_syn_seq + 1U),
    .wnd = CPC_TEST_PEER_RX_WND + 10,
  });

  cpc_test_pump();

  TEST_ASSERT_EQUAL(SLI_CPC_STATE_CONNECTED, ep.state);
  TEST_ASSERT_DRV_PENDING_TX(0);
  TEST_ASSERT_BIT_HIGH(SL_CPC_EP_EVENT_CONNECTED, cpc_test_event_mask);

  // the window should have been updated by the ACK, other values are the same
  TEST_ASSERT_EQUAL_UINT8(CPC_TEST_PEER_RX_WND + 10, ep.send_wnd);
  TEST_ASSERT_EQUAL_UINT8(our_syn_seq + 1, ep.send_nxt);
  TEST_ASSERT_EQUAL_UINT8(CPC_TEST_PEER_SYN_SEQ + 1U, ep.ack);

  sl_cpc_ep_close(&ep);
  cpc_test_pump();

  cpc_test_ep_discard(&ep);
}

/**
 * @brief Active happy path: connect() -> CONNECTED.
 *
 * Drives a full three-way handshake from the initiator side:
 *
 *   us  -> peer: SYN(seq=S)              (sl_cpc_ep_connect)
 *   peer -> us: SYN(seq=R, ack=S+1)      (matrix: SYN_SENT + SYN -> CONNECTED)
 *   us  -> peer: ack-only DATA(ack=R+1)
 *
 * Verifies that on receiving a SYN with a valid ack we transition to
 * CONNECTED, record the peer's MTU, and emit the third-leg ack-only DATA
 * frame.
 */
TEST(cpc_syn, connect_then_handshake_completes)
{
  sl_status_t status;
  sl_cpc_ep_t ep;

  status = sl_cpc_ep_init(&ep, CPC_TEST_EP_ID, CPC_TEST_RX_SIZE, cpc_test_event_cb, NULL);
  TEST_ASSERT_EQUAL(SL_STATUS_OK, status);

  status = sl_cpc_ep_connect(&ep, g_bus);
  TEST_ASSERT_EQUAL(SL_STATUS_OK, status);
  TEST_ASSERT_EQUAL(SLI_CPC_STATE_SYN_SENT, ep.state);

  // Capture the SYN we just sent so the test can echo back a valid ack.
  mock_cpc_tx_packet_t *dut_syn = TEST_POP_OUTGOING({
    .flags = CPC_EXPECT_FLAG_ACK_REQ | CPC_EXPECT_FLAG_SYN,
    .len = CPC_TEST_RX_SIZE,
    .dst = CPC_TEST_EP_ID,
    .seq = -1,
    .ack = -1,
    .wnd = -1,
  });
  uint8_t our_syn_seq = sli_cpc_header_get_seq(&dut_syn->header);
  mock_cpc_tx_packet_free(&dut_syn);

  // Peer responds with a SYN whose ack acknowledges ours.
  TEST_INJECT_FRAME({
    .flags = CPC_EXPECT_FLAG_ACK_REQ | CPC_EXPECT_FLAG_SYN,
    .len = CPC_TEST_PEER_MTU,
    .dst = CPC_TEST_EP_ID,
    .seq = CPC_TEST_PEER_SYN_SEQ,
    .ack = (int)(our_syn_seq + 1U),
    .wnd = CPC_TEST_PEER_RX_WND,
  });

  cpc_test_pump();

  TEST_ASSERT_EQUAL(SLI_CPC_STATE_CONNECTED, ep.state);
  TEST_ASSERT_BIT_HIGH(SL_CPC_EP_EVENT_CONNECTED, cpc_test_event_mask);
  TEST_ASSERT_EQUAL_UINT16(CPC_TEST_PEER_MTU, ep.remote_mtu);

  TEST_ASSERT_EQUAL_UINT8(our_syn_seq + 1, ep.send_nxt);
  TEST_ASSERT_EQUAL_UINT8(CPC_TEST_PEER_SYN_SEQ + 1U, ep.ack);
  TEST_ASSERT_EQUAL_UINT8(CPC_TEST_PEER_RX_WND, ep.send_wnd);

  // We complete the handshake with an ack-only DATA frame acknowledging the
  // peer's SYN.
  TEST_ASSERT_OUTGOING({
    .len = 0,
    .dst = CPC_TEST_EP_ID,
    .seq = -1, // standalone ack might not set the sequence number
    .ack = (int)(CPC_TEST_PEER_SYN_SEQ + 1U),
    .wnd = -1,
  });

  // The mock driver has indicated the frame as being sent but the
  // core still needs to run to get it back and free it
  cpc_test_pump();

  TEST_ASSERT_DRV_PENDING_TX(0);

  sl_cpc_ep_close(&ep);
  cpc_test_pump();

  cpc_test_ep_discard(&ep);
}

static void cpc_syn_event_cb(sl_cpc_ep_t *ep, sl_cpc_ep_event_type_t type, const sl_cpc_ep_event_t *event, void *arg)
{
  const uint8_t payload[] = CPC_SYN_DUMMY_PAYLOAD;

  (void)arg;

  cpc_test_event_mask |= 1 << type;

  if (type == SL_CPC_EP_EVENT_CONNECTED) {
    cpc_test_write_payload(__LINE__, ep, payload, sizeof(payload));
  }

  if (type == SL_CPC_EP_EVENT_SEND_DONE) {
    free(event->send_done.frame);
    sl_cpc_buf_free(event->send_done.buf);
  }
}

/**
 * @brief CONNECTED must run only after the SYN ACK updates send_una/send_wnd.
 *
 * With peer rx_wnd = 1, a connected handler that writes immediately (as the
 * control endpoint does) must transmit on the same pump. If CONNECTED fires
 * before send_una advances past the SYN, the write is held and nothing with
 * payload leaves the stack until more window opens.
 */
TEST(cpc_syn, connect_then_immediate_write_after_syn_ack)
{
  const uint8_t payload[] = CPC_SYN_DUMMY_PAYLOAD;
  mock_cpc_tx_packet_t *dut_syn;
  mock_cpc_tx_packet_t *packet;
  uint8_t our_syn_seq;
  sl_status_t status;
  sl_cpc_ep_t ep;

  status = sl_cpc_ep_init(&ep, CPC_TEST_EP_ID, CPC_TEST_RX_SIZE, cpc_syn_event_cb, NULL);
  TEST_ASSERT_EQUAL(SL_STATUS_OK, status);

  status = sl_cpc_ep_connect(&ep, g_bus);
  TEST_ASSERT_EQUAL(SL_STATUS_OK, status);
  TEST_ASSERT_EQUAL(SLI_CPC_STATE_SYN_SENT, ep.state);

  dut_syn = TEST_POP_OUTGOING({
    .flags = CPC_EXPECT_FLAG_ACK_REQ | CPC_EXPECT_FLAG_SYN,
    .len = CPC_TEST_RX_SIZE,
    .dst = CPC_TEST_EP_ID,
    .seq = -1,
    .ack = -1,
    .wnd = -1,
  });
  our_syn_seq = sli_cpc_header_get_seq(&dut_syn->header);
  mock_cpc_tx_packet_free(&dut_syn);

  TEST_INJECT_FRAME({
    .flags = CPC_EXPECT_FLAG_ACK_REQ | CPC_EXPECT_FLAG_SYN,
    .len = CPC_TEST_PEER_MTU,
    .dst = CPC_TEST_EP_ID,
    .seq = CPC_TEST_PEER_SYN_SEQ,
    .ack = (int)(our_syn_seq + 1U),
    .wnd = CPC_TEST_PEER_RX_WND,
  });

  // Process peer SYN-ACK, window update, CONNECTED, and immediate write.
  cpc_test_pump();

  TEST_ASSERT_EQUAL(SLI_CPC_STATE_CONNECTED, ep.state);
  TEST_ASSERT_BIT_HIGH(SL_CPC_EP_EVENT_CONNECTED, cpc_test_event_mask);
  TEST_ASSERT_EQUAL_UINT8(our_syn_seq + 1U, ep.send_una);
  TEST_ASSERT_EQUAL_UINT8(our_syn_seq + 2U, ep.send_nxt);
  TEST_ASSERT_EQUAL_UINT8(CPC_TEST_PEER_RX_WND, ep.send_wnd);

  packet = TEST_POP_OUTGOING_PAYLOAD({
    .flags = CPC_EXPECT_FLAG_ACK_REQ,
    .len = (int)sizeof(payload),
    .dst = CPC_TEST_EP_ID,
    .seq = (int)(our_syn_seq + 1U),
    .ack = (int)(CPC_TEST_PEER_SYN_SEQ + 1U),
    .wnd = -1,
  });
  TEST_ASSERT_EQUAL_MEMORY(payload, packet->payload, sizeof(payload));
  mock_cpc_tx_packet_free(&packet);

  // Flush third-leg ack and payload send_done.
  cpc_test_pump();

  sl_cpc_ep_close(&ep);

  // Process endpoint close.
  cpc_test_pump();

  cpc_test_ep_discard(&ep);
}

/**
 * @brief Active open interrupted: connect() then peer RST -> error notification.
 */
TEST(cpc_syn, connect_then_reset_notifies_error)
{
  sl_status_t status;
  sl_cpc_ep_t ep;

  status = sl_cpc_ep_init(&ep, CPC_TEST_EP_ID, CPC_TEST_RX_SIZE, cpc_test_event_cb, NULL);
  TEST_ASSERT_EQUAL(SL_STATUS_OK, status);

  status = sl_cpc_ep_connect(&ep, g_bus);
  TEST_ASSERT_EQUAL(SL_STATUS_OK, status);
  TEST_ASSERT_EQUAL(SLI_CPC_STATE_SYN_SENT, ep.state);

  mock_cpc_tx_packet_t *dut_syn = TEST_POP_OUTGOING({
    .flags = CPC_EXPECT_FLAG_ACK_REQ | CPC_EXPECT_FLAG_SYN,
    .len = CPC_TEST_RX_SIZE,
    .dst = CPC_TEST_EP_ID,
    .seq = -1,
    .ack = -1,
    .wnd = -1,
  });
  mock_cpc_tx_packet_free(&dut_syn);
  cpc_test_pump();

  TEST_INJECT_FRAME({
    .flags = CPC_EXPECT_FLAG_RESET,
    .len = 0,
    .dst = CPC_TEST_EP_ID,
    .seq = 0,
    .ack = 0,
    .wnd = 0,
  });

  cpc_test_pump();

  TEST_ASSERT_EQUAL(SLI_CPC_STATE_CLOSING, ep.state);
  TEST_ASSERT_BIT_HIGH(SL_CPC_EP_EVENT_ERROR, cpc_test_event_mask);
  TEST_ASSERT_DRV_PENDING_TX(0);
  TEST_ASSERT_NULL(mock_cpc_drv_pop_tx_packet(sl_cpc_drv_mock_default));

  sl_cpc_ep_close(&ep);
  cpc_test_pump();

  cpc_test_ep_discard(&ep);
}

/**
 * @brief Passive open: listen() then peer RST is ignored, endpoint stays OPEN.
 */
TEST(cpc_syn, listen_then_reset_is_ignored)
{
  sl_status_t status;
  sl_cpc_ep_t ep;

  status = sl_cpc_ep_init(&ep, CPC_TEST_EP_ID, CPC_TEST_RX_SIZE, cpc_test_event_cb, NULL);
  TEST_ASSERT_EQUAL(SL_STATUS_OK, status);

  status = sl_cpc_ep_listen(&ep, g_bus);
  TEST_ASSERT_EQUAL(SL_STATUS_OK, status);
  TEST_ASSERT_EQUAL(SLI_CPC_STATE_OPEN, ep.state);

  TEST_INJECT_FRAME({
    .flags = CPC_EXPECT_FLAG_RESET,
    .len = 0,
    .dst = CPC_TEST_EP_ID,
    .seq = 0,
    .ack = 0,
    .wnd = 0,
  });

  cpc_test_pump();

  TEST_ASSERT_EQUAL(SLI_CPC_STATE_OPEN, ep.state);
  TEST_ASSERT_DRV_PENDING_TX(0);
  TEST_ASSERT_NULL(mock_cpc_drv_pop_tx_packet(sl_cpc_drv_mock_default));

  sl_cpc_ep_close(&ep);
  cpc_test_pump();

  cpc_test_ep_discard(&ep);
}

/**
 * @brief Active open: connect() then unexpected DATA -> RST and error.
 */
TEST(cpc_syn, connect_then_data_with_expected_seq_notifies_error)
{
  sl_status_t status;
  sl_cpc_ep_t ep;

  status = sl_cpc_ep_init(&ep, CPC_TEST_EP_ID, CPC_TEST_RX_SIZE, cpc_test_event_cb, NULL);
  TEST_ASSERT_EQUAL(SL_STATUS_OK, status);

  status = sl_cpc_ep_connect(&ep, g_bus);
  TEST_ASSERT_EQUAL(SL_STATUS_OK, status);
  TEST_ASSERT_EQUAL(SLI_CPC_STATE_SYN_SENT, ep.state);

  mock_cpc_tx_packet_t *dut_syn = TEST_POP_OUTGOING({
    .flags = CPC_EXPECT_FLAG_ACK_REQ | CPC_EXPECT_FLAG_SYN,
    .len = CPC_TEST_RX_SIZE,
    .dst = CPC_TEST_EP_ID,
    .seq = -1,
    .ack = -1,
    .wnd = -1,
  });
  uint8_t expected_syn_ack = sli_cpc_header_get_seq(&dut_syn->header) + 1;
  uint8_t expected_syn_seq = sli_cpc_header_get_ack(&dut_syn->header);
  mock_cpc_tx_packet_free(&dut_syn);
  cpc_test_pump();

  uint8_t payload[] = {0xAA, 0xBB, 0xCC, 0xDD};

  // Inject a data frame that has ACK value that a valid SYN frame would have.
  // Let the SEQ be what the endpoint expects, to check if it lets it pass or not.
  TEST_INJECT_FRAME({
    .flags = CPC_EXPECT_FLAG_ACK_REQ,
    .len = 4,
    .dst = CPC_TEST_EP_ID,
    .seq = expected_syn_seq,
    .ack = expected_syn_ack,
    .wnd = 8,
    .payload = payload,
  });

  cpc_test_pump();

  TEST_ASSERT_EQUAL(SLI_CPC_STATE_CLOSING, ep.state);
  TEST_ASSERT_BIT_HIGH(SL_CPC_EP_EVENT_ERROR, cpc_test_event_mask);

  TEST_ASSERT_OUTGOING({
    .flags = CPC_EXPECT_FLAG_RESET,
    .len = 0,
    .dst = CPC_TEST_EP_ID,
    .seq = 0,
    .ack = 0,
    .wnd = 0,
  });

  cpc_test_pump();

  sl_cpc_ep_close(&ep);
  cpc_test_pump();

  cpc_test_ep_discard(&ep);
}

/**
 * @brief Active open: SYN handshake with a data frame (w/ payload) instead of
 *                     expected SYN+ACK shall set the endpoint in error state
 *                     and abort the connection.
 */
TEST(cpc_syn, connect_then_data_with_random_seq_notifies_error)
{
  sl_status_t status;
  sl_cpc_ep_t ep;

  status = sl_cpc_ep_init(&ep, CPC_TEST_EP_ID, CPC_TEST_RX_SIZE, cpc_test_event_cb, NULL);
  TEST_ASSERT_EQUAL(SL_STATUS_OK, status);

  status = sl_cpc_ep_connect(&ep, g_bus);
  TEST_ASSERT_EQUAL(SL_STATUS_OK, status);
  TEST_ASSERT_EQUAL(SLI_CPC_STATE_SYN_SENT, ep.state);

  mock_cpc_tx_packet_t *dut_syn = TEST_POP_OUTGOING({
    .flags = CPC_EXPECT_FLAG_ACK_REQ | CPC_EXPECT_FLAG_SYN,
    .len = CPC_TEST_RX_SIZE,
    .dst = CPC_TEST_EP_ID,
    .seq = -1,
    .ack = -1,
    .wnd = -1,
  });
  uint8_t expected_syn_ack = sli_cpc_header_get_seq(&dut_syn->header) + 1;
  uint8_t expected_syn_seq = sli_cpc_header_get_ack(&dut_syn->header);
  mock_cpc_tx_packet_free(&dut_syn);
  cpc_test_pump();

  uint8_t payload[] = {0xAA, 0xBB, 0xCC, 0xDD};

  // This frame has a random sequence number but a valid ack. A valid ACK on a SYN frame would let
  // the endpoint transition to the CONNECTED state, so we want to make sure it doesn't happen here
  TEST_INJECT_FRAME({
    .flags = CPC_EXPECT_FLAG_ACK_REQ,
    .len = 4,
    .dst = CPC_TEST_EP_ID,
    .seq = expected_syn_seq + 128,
    .ack = expected_syn_ack,
    .wnd = 8,
    .payload = payload,
  });

  cpc_test_pump();

  TEST_ASSERT_EQUAL(SLI_CPC_STATE_CLOSING, ep.state);
  TEST_ASSERT_BIT_HIGH(SL_CPC_EP_EVENT_ERROR, cpc_test_event_mask);

  TEST_ASSERT_OUTGOING({
    .flags = CPC_EXPECT_FLAG_RESET,
    .len = 0,
    .dst = CPC_TEST_EP_ID,
    .seq = 0,
    .ack = 0,
    .wnd = 0,
  });

  cpc_test_pump();

  sl_cpc_ep_close(&ep);
  cpc_test_pump();

  cpc_test_ep_discard(&ep);
}

/**
 * @brief Passive open: SYN handshake completed by a DATA frame carrying payload.
 */
TEST(cpc_syn, listen_then_data_with_valid_seq_ack_payload_completes_handshake)
{
  sl_cpc_buf_t *handles[2];
  const uint8_t payload[] = {0x10, 0x20, 0x30, 0x40};
  sl_status_t status;
  sl_cpc_ep_t ep;

  status = sl_cpc_ep_init(&ep, CPC_TEST_EP_ID, CPC_TEST_RX_SIZE, cpc_test_event_cb, NULL);
  TEST_ASSERT_EQUAL(SL_STATUS_OK, status);

  // Push two buffers to avoid an RX window update from messing with the tests
  handles[0] = test_push_recv_buf(&ep);
  handles[1] = test_push_recv_buf(&ep);

  status = sl_cpc_ep_listen(&ep, g_bus);
  TEST_ASSERT_EQUAL(SL_STATUS_OK, status);

  TEST_INJECT_FRAME({
    .flags = CPC_EXPECT_FLAG_ACK_REQ | CPC_EXPECT_FLAG_SYN,
    .len = CPC_TEST_PEER_MTU,
    .dst = CPC_TEST_EP_ID,
    .seq = CPC_TEST_PEER_SYN_SEQ,
    .ack = 0,
    .wnd = CPC_TEST_PEER_RX_WND,
  });

  mock_cpc_tx_packet_t *dut_syn_ack = TEST_POP_OUTGOING({
    .flags = CPC_EXPECT_FLAG_ACK_REQ | CPC_EXPECT_FLAG_SYN,
    .len = CPC_TEST_RX_SIZE,
    .dst = CPC_TEST_EP_ID,
    .seq = -1,
    .ack = (int)(CPC_TEST_PEER_SYN_SEQ + 1U),
    .wnd = 2,
  });
  uint8_t expected_data_ack = sli_cpc_header_get_seq(&dut_syn_ack->header) + 1;
  mock_cpc_tx_packet_free(&dut_syn_ack);

  TEST_ASSERT_EQUAL(SLI_CPC_STATE_SYN_RCVD, ep.state);

  TEST_INJECT_FRAME({
    .flags = CPC_EXPECT_FLAG_ACK_REQ,
    .len = 4,
    .dst = CPC_TEST_EP_ID,
    .seq = (int)(CPC_TEST_PEER_SYN_SEQ + 1U),
    .ack = (int)(expected_data_ack),
    .wnd = CPC_TEST_PEER_RX_WND + 10,
    .payload = payload,
  });

  TEST_ASSERT_OUTGOING({
    .len = 0,
    .dst = CPC_TEST_EP_ID,
    .seq = 0,
    .ack = (int)(CPC_TEST_PEER_SYN_SEQ + 2U),
    .wnd = 1,
  });

  cpc_test_pump();

  TEST_ASSERT_EQUAL(0, mock_cpc_timer_count_running());
  TEST_ASSERT_FALSE(mock_cpc_timer_has_pending());

  TEST_ASSERT_EQUAL(SLI_CPC_STATE_CONNECTED, ep.state);
  TEST_ASSERT_BIT_HIGH(SL_CPC_EP_EVENT_CONNECTED, cpc_test_event_mask);
  TEST_ASSERT_BIT_HIGH(SL_CPC_EP_EVENT_RECV, cpc_test_event_mask);
  TEST_ASSERT_DRV_PENDING_TX(0);

  sl_cpc_ep_close(&ep);
  cpc_test_pump();

  cpc_test_ep_discard(&ep);

  for (int i = 0; i < 2; i++) {
    sl_cpc_buf_free(handles[i]);
  }
}

/**
 * @brief Passive open: SYN handshake with a data frame (w/ payload) with invalid seq
 */
TEST(cpc_syn, listen_then_data_with_invalid_seq_valid_ack_payload_is_noop)
{
  sl_cpc_buf_t *handles[2];
  const uint8_t payload[] = {0x10, 0x20, 0x30, 0x40};
  sl_status_t status;
  sl_cpc_ep_t ep;

  status = sl_cpc_ep_init(&ep, CPC_TEST_EP_ID, CPC_TEST_RX_SIZE, cpc_test_event_cb, NULL);
  TEST_ASSERT_EQUAL(SL_STATUS_OK, status);

  // Push two buffers to avoid an RX window update from messing with the tests
  handles[0] = test_push_recv_buf(&ep);
  handles[1] = test_push_recv_buf(&ep);

  status = sl_cpc_ep_listen(&ep, g_bus);
  TEST_ASSERT_EQUAL(SL_STATUS_OK, status);

  TEST_INJECT_FRAME({
    .flags = CPC_EXPECT_FLAG_ACK_REQ | CPC_EXPECT_FLAG_SYN,
    .len = CPC_TEST_PEER_MTU,
    .dst = CPC_TEST_EP_ID,
    .seq = CPC_TEST_PEER_SYN_SEQ,
    .ack = 0,
    .wnd = CPC_TEST_PEER_RX_WND,
  });

  mock_cpc_tx_packet_t *dut_syn_ack = TEST_POP_OUTGOING({
    .flags = CPC_EXPECT_FLAG_ACK_REQ | CPC_EXPECT_FLAG_SYN,
    .len = CPC_TEST_RX_SIZE,
    .dst = CPC_TEST_EP_ID,
    .seq = -1,
    .ack = (int)(CPC_TEST_PEER_SYN_SEQ + 1U),
    .wnd = 2,
  });
  uint8_t expected_data_ack = sli_cpc_header_get_seq(&dut_syn_ack->header) + 1;
  mock_cpc_tx_packet_free(&dut_syn_ack);

  TEST_ASSERT_EQUAL(SLI_CPC_STATE_SYN_RCVD, ep.state);

  TEST_INJECT_FRAME({
    .flags = CPC_EXPECT_FLAG_ACK_REQ,
    .len = 4,
    .dst = CPC_TEST_EP_ID,
    // invalid sequence value but valid ack
    .seq = (int)(CPC_TEST_PEER_SYN_SEQ + 128U),
    .ack = (int)(expected_data_ack),
    .wnd = CPC_TEST_PEER_RX_WND,
    .payload = payload,
  });

  // The core should send a regular ack
  TEST_ASSERT_OUTGOING({
    .len = 0,
    .dst = CPC_TEST_EP_ID,
    .seq = 0,
    .ack = (int)(CPC_TEST_PEER_SYN_SEQ + 1U),
    .wnd = 2,
  });

  cpc_test_pump();

  TEST_ASSERT_EQUAL(1, mock_cpc_timer_count_running());
  TEST_ASSERT_TRUE(mock_cpc_timer_has_pending());

  TEST_ASSERT_EQUAL(SLI_CPC_STATE_SYN_RCVD, ep.state);
  TEST_ASSERT_BIT_LOW(SL_CPC_EP_EVENT_CONNECTED, cpc_test_event_mask);
  TEST_ASSERT_BIT_LOW(SL_CPC_EP_EVENT_RECV, cpc_test_event_mask);
  TEST_ASSERT_BIT_LOW(SL_CPC_EP_EVENT_CLOSED, cpc_test_event_mask);
  TEST_ASSERT_DRV_PENDING_TX(0);

  sl_cpc_ep_close(&ep);

  cpc_test_pump();

  TEST_ASSERT_BIT_HIGH(SL_CPC_EP_EVENT_CLOSED, cpc_test_event_mask);

  cpc_test_ep_discard(&ep);

  for (int i = 0; i < 2; i++) {
    sl_cpc_buf_free(handles[i]);
  }
}

/**
 * @brief Passive open: DATA received in OPEN -> RST reply, endpoint stays OPEN.
 */
TEST(cpc_syn, listen_then_data_replies_with_reset)
{
  const uint8_t payload[] = {0x01, 0x02, 0x03, 0x04};
  sl_status_t status;
  sl_cpc_ep_t ep;

  status = sl_cpc_ep_init(&ep, CPC_TEST_EP_ID, CPC_TEST_RX_SIZE, cpc_test_event_cb, NULL);
  TEST_ASSERT_EQUAL(SL_STATUS_OK, status);

  status = sl_cpc_ep_listen(&ep, g_bus);
  TEST_ASSERT_EQUAL(SL_STATUS_OK, status);
  TEST_ASSERT_EQUAL(SLI_CPC_STATE_OPEN, ep.state);

  TEST_INJECT_FRAME({
    .flags = CPC_EXPECT_FLAG_ACK_REQ,
    .len = 4,
    .dst = CPC_TEST_EP_ID,
    .seq = 1,
    .ack = 0,
    .wnd = 8,
    .payload = payload,
  });

  cpc_test_pump();

  TEST_ASSERT_EQUAL(SLI_CPC_STATE_OPEN, ep.state);

  TEST_ASSERT_OUTGOING({
    .flags = CPC_EXPECT_FLAG_RESET,
    .len = 0,
    .dst = CPC_TEST_EP_ID,
    .seq = 0,
    .ack = 0,
    .wnd = 0,
  });

  cpc_test_pump();

  sl_cpc_ep_close(&ep);
  cpc_test_pump();

  cpc_test_ep_discard(&ep);
}

/**
 * @brief SYN_RCVD: ack-only DATA with expected seq but ACK that does not
 *        complete the handshake should be ignored.
 *        This one has an ack that is before the valid window of ack values.
 */
TEST(cpc_syn, listen_then_standalone_ack_with_valid_seq_wrong_ack_past_is_ignored)
{
  uint8_t expected_ack;
  sl_status_t status;
  sl_cpc_ep_t ep;

  status = sl_cpc_ep_init(&ep, CPC_TEST_EP_ID, CPC_TEST_RX_SIZE, cpc_test_event_cb, NULL);
  TEST_ASSERT_EQUAL(SL_STATUS_OK, status);

  status = sl_cpc_ep_listen(&ep, g_bus);
  TEST_ASSERT_EQUAL(SL_STATUS_OK, status);
  TEST_ASSERT_EQUAL(SLI_CPC_STATE_OPEN, ep.state);

  TEST_INJECT_FRAME({
    .flags = CPC_EXPECT_FLAG_ACK_REQ | CPC_EXPECT_FLAG_SYN,
    .len = CPC_TEST_PEER_MTU,
    .dst = CPC_TEST_EP_ID,
    .seq = CPC_TEST_PEER_SYN_SEQ,
    .ack = 0,
    .wnd = CPC_TEST_PEER_RX_WND,
  });

  mock_cpc_tx_packet_t *dut_syn_ack = TEST_POP_OUTGOING({
    .flags = CPC_EXPECT_FLAG_ACK_REQ | CPC_EXPECT_FLAG_SYN,
    .len = CPC_TEST_RX_SIZE,
    .dst = CPC_TEST_EP_ID,
    .seq = -1,
    .ack = (int)(CPC_TEST_PEER_SYN_SEQ + 1U),
    .wnd = 0,
  });
  expected_ack = sli_cpc_header_get_seq(&dut_syn_ack->header);
  mock_cpc_tx_packet_free(&dut_syn_ack);

  TEST_ASSERT_EQUAL(SLI_CPC_STATE_SYN_RCVD, ep.state);
  TEST_ASSERT_DRV_PENDING_TX(0);

  // This frame is not supposed to ack the syn, as the ack number is the same as the sequence number
  // of the outgoing syn. To actually ack, it would have to be expected_ack + 1. The goal is to
  // check that this frame doesn't make the connection sequence progress, this frame should be
  // ignored.
  TEST_INJECT_FRAME({
    .dst = CPC_TEST_EP_ID,
    .seq = (int)(CPC_TEST_PEER_SYN_SEQ + 1U),
    .ack = (int)expected_ack,
    .wnd = CPC_TEST_PEER_RX_WND,
  });

  cpc_test_pump();

  TEST_ASSERT_EQUAL(SLI_CPC_STATE_SYN_RCVD, ep.state);
  TEST_ASSERT_BIT_LOW(SL_CPC_EP_EVENT_CLOSED, cpc_test_event_mask);

  // the SYN frame is lingering in endpoint's re_transmit_list, so we must close the endpoint to
  // free that frame
  sl_cpc_ep_close(&ep);

  cpc_test_pump();

  TEST_ASSERT_BIT_HIGH(SL_CPC_EP_EVENT_CLOSED, cpc_test_event_mask);

  cpc_test_ep_discard(&ep);
}

/**
 * @brief SYN_RCVD: ack-only DATA with expected seq but ACK that does not
 *        complete the handshake should be ignored.
 *        This one has an ack that is after the valid window of ack values.
 */
TEST(cpc_syn, listen_then_standalone_ack_with_valid_seq_wrong_ack_future_is_ignored)
{
  uint8_t expected_ack;
  sl_status_t status;
  sl_cpc_ep_t ep;

  status = sl_cpc_ep_init(&ep, CPC_TEST_EP_ID, CPC_TEST_RX_SIZE, cpc_test_event_cb, NULL);
  TEST_ASSERT_EQUAL(SL_STATUS_OK, status);

  status = sl_cpc_ep_listen(&ep, g_bus);
  TEST_ASSERT_EQUAL(SL_STATUS_OK, status);
  TEST_ASSERT_EQUAL(SLI_CPC_STATE_OPEN, ep.state);

  TEST_INJECT_FRAME({
    .flags = CPC_EXPECT_FLAG_ACK_REQ | CPC_EXPECT_FLAG_SYN,
    .len = CPC_TEST_PEER_MTU,
    .dst = CPC_TEST_EP_ID,
    .seq = CPC_TEST_PEER_SYN_SEQ,
    .ack = 0,
    .wnd = CPC_TEST_PEER_RX_WND,
  });

  mock_cpc_tx_packet_t *dut_syn_ack = TEST_POP_OUTGOING({
    .flags = CPC_EXPECT_FLAG_ACK_REQ | CPC_EXPECT_FLAG_SYN,
    .len = CPC_TEST_RX_SIZE,
    .dst = CPC_TEST_EP_ID,
    .seq = -1,
    .ack = (int)(CPC_TEST_PEER_SYN_SEQ + 1U),
    .wnd = 0,
  });
  expected_ack = sli_cpc_header_get_seq(&dut_syn_ack->header);
  mock_cpc_tx_packet_free(&dut_syn_ack);

  TEST_ASSERT_EQUAL(SLI_CPC_STATE_SYN_RCVD, ep.state);
  TEST_ASSERT_DRV_PENDING_TX(0);

  // This frame is not supposed to ack the syn, as the ack is for a frame that was not even sent.
  // To actually ack, it would have to be expected_ack + 1. The goal is to check that this frame
  // doesn't make the connection sequence progress, this frame should be ignored.
  TEST_INJECT_FRAME({
    .dst = CPC_TEST_EP_ID,
    .seq = (int)(CPC_TEST_PEER_SYN_SEQ + 1U),
    .ack = (int)expected_ack + 2,
    .wnd = CPC_TEST_PEER_RX_WND,
  });

  cpc_test_pump();

  TEST_ASSERT_EQUAL(SLI_CPC_STATE_SYN_RCVD, ep.state);
  TEST_ASSERT_BIT_LOW(SL_CPC_EP_EVENT_CLOSED, cpc_test_event_mask);

  // the SYN frame is lingering in endpoint's re_transmit_list, so we must close the endpoint to
  // free that frame
  sl_cpc_ep_close(&ep);

  cpc_test_pump();

  TEST_ASSERT_BIT_HIGH(SL_CPC_EP_EVENT_CLOSED, cpc_test_event_mask);

  cpc_test_ep_discard(&ep);
}

/**
 * @brief SYN_RCVD: reliable DATA with payload and with expected seq but ACK that does not
 *        complete the handshake is ignored.
 */
TEST(cpc_syn, listen_then_payload_data_with_valid_seq_wrong_ack_is_ignored)
{
  const uint8_t payload[] = {0x10, 0x20, 0x30, 0x40};
  sl_cpc_buf_t *handles[2];
  uint8_t expected_ack;
  sl_status_t status;
  sl_cpc_ep_t ep;

  status = sl_cpc_ep_init(&ep, CPC_TEST_EP_ID, CPC_TEST_RX_SIZE, cpc_test_event_cb, NULL);
  TEST_ASSERT_EQUAL(SL_STATUS_OK, status);

  handles[0] = test_push_recv_buf(&ep);
  handles[1] = test_push_recv_buf(&ep);

  status = sl_cpc_ep_listen(&ep, g_bus);
  TEST_ASSERT_EQUAL(SL_STATUS_OK, status);
  TEST_ASSERT_EQUAL(SLI_CPC_STATE_OPEN, ep.state);

  TEST_INJECT_FRAME({
    .flags = CPC_EXPECT_FLAG_ACK_REQ | CPC_EXPECT_FLAG_SYN,
    .len = CPC_TEST_PEER_MTU,
    .dst = CPC_TEST_EP_ID,
    .seq = CPC_TEST_PEER_SYN_SEQ,
    .ack = 0,
    .wnd = CPC_TEST_PEER_RX_WND,
  });

  mock_cpc_tx_packet_t *dut_syn_ack = TEST_POP_OUTGOING({
    .flags = CPC_EXPECT_FLAG_ACK_REQ | CPC_EXPECT_FLAG_SYN,
    .len = CPC_TEST_RX_SIZE,
    .dst = CPC_TEST_EP_ID,
    .seq = -1,
    .ack = (int)(CPC_TEST_PEER_SYN_SEQ + 1U),
    .wnd = 2,
  });
  expected_ack = sli_cpc_header_get_seq(&dut_syn_ack->header);
  mock_cpc_tx_packet_free(&dut_syn_ack);

  TEST_ASSERT_EQUAL(SLI_CPC_STATE_SYN_RCVD, ep.state);

  TEST_INJECT_FRAME({
    .flags = CPC_EXPECT_FLAG_ACK_REQ,
    .len = 4,
    .dst = CPC_TEST_EP_ID,
    .seq = (int)(CPC_TEST_PEER_SYN_SEQ + 1U),
    // actual expected ack is this variable + 1, this one acks a frame that was never sent
    .ack = (int)expected_ack,
    .wnd = CPC_TEST_PEER_RX_WND,
    .payload = payload,
  });

  cpc_test_pump();

  TEST_ASSERT_EQUAL(SLI_CPC_STATE_SYN_RCVD, ep.state);
  TEST_ASSERT_BIT_LOW(SL_CPC_EP_EVENT_CLOSED, cpc_test_event_mask);

  // the SYN frame is lingering in endpoint's re_transmit_list, so we must close the endpoint to
  // free that frame
  sl_cpc_ep_close(&ep);

  cpc_test_pump();

  TEST_ASSERT_BIT_HIGH(SL_CPC_EP_EVENT_CLOSED, cpc_test_event_mask);

  cpc_test_ep_discard(&ep);

  for (int i = 0; i < 2; i++) {
    sl_cpc_buf_free(handles[i]);
  }
}

/**
 * @brief Deferred listen drops peer SYN until bus_initialized is set.
 *
 * Primary can finish control and SYN an app endpoint while secondary still has
 * that endpoint deferred (CLOSED+USER_OPENED).
 */
TEST(cpc_syn, deferred_listen_drops_syn_until_bus_initialized)
{
  sl_status_t status;
  sl_cpc_ep_t ep;

  g_bus->initialized = false;

  status = sl_cpc_ep_init(&ep, CPC_TEST_EP_ID, CPC_TEST_RX_SIZE, cpc_test_event_cb, NULL);
  TEST_ASSERT_EQUAL(SL_STATUS_OK, status);

  status = sl_cpc_ep_listen(&ep, g_bus);
  TEST_ASSERT_EQUAL(SL_STATUS_OK, status);
  TEST_ASSERT_EQUAL(SLI_CPC_STATE_CLOSED, ep.state);
  TEST_ASSERT_TRUE((ep.flags & SLI_CPC_EP_FLAG_OPENED) != 0);

  TEST_INJECT_FRAME({
    .flags = CPC_EXPECT_FLAG_ACK_REQ | CPC_EXPECT_FLAG_SYN,
    .len = CPC_TEST_PEER_MTU,
    .dst = CPC_TEST_EP_ID,
    .seq = CPC_TEST_PEER_SYN_SEQ,
    .ack = 0,
    .wnd = CPC_TEST_PEER_RX_WND,
  });

  cpc_test_pump();

  TEST_ASSERT_EQUAL(SLI_CPC_STATE_CLOSED, ep.state);
  TEST_ASSERT_BIT_LOW(SL_CPC_EP_EVENT_ERROR, cpc_test_event_mask);
  TEST_ASSERT_NULL(mock_cpc_drv_pop_tx_packet(sl_cpc_drv_mock_default));

  sli_cpc_bus_notify_initialized(g_bus);

  TEST_ASSERT_TRUE(g_bus->initialized);
  TEST_ASSERT_EQUAL(SLI_CPC_STATE_OPEN, ep.state);

  TEST_INJECT_FRAME({
    .flags = CPC_EXPECT_FLAG_ACK_REQ | CPC_EXPECT_FLAG_SYN,
    .len = CPC_TEST_PEER_MTU,
    .dst = CPC_TEST_EP_ID,
    .seq = CPC_TEST_PEER_SYN_SEQ,
    .ack = 0,
    .wnd = CPC_TEST_PEER_RX_WND,
  });

  mock_cpc_tx_packet_t *dut_syn_ack = TEST_POP_OUTGOING({
    .flags = CPC_EXPECT_FLAG_ACK_REQ | CPC_EXPECT_FLAG_SYN,
    .len = CPC_TEST_RX_SIZE,
    .dst = CPC_TEST_EP_ID,
    .seq = -1,
    .ack = (int)(CPC_TEST_PEER_SYN_SEQ + 1U),
    .wnd = 0,
  });

  TEST_ASSERT_EQUAL(SLI_CPC_STATE_SYN_RCVD, ep.state);
  TEST_ASSERT_BIT_LOW(SL_CPC_EP_EVENT_ERROR, cpc_test_event_mask);

  mock_cpc_tx_packet_free(&dut_syn_ack);
  cpc_test_pump();

  sl_cpc_ep_close(&ep);
  cpc_test_pump();
  cpc_test_ep_discard(&ep);
}

/**
 * @brief Duplicate SYN in SYN_RCVD is ignored while SYN+ACK retransmits.
 */
TEST(cpc_syn, duplicate_syn_in_syn_rcvd_is_ignored)
{
  uint8_t our_syn_seq;
  sl_status_t status;
  sl_cpc_ep_t ep;

  status = sl_cpc_ep_init(&ep, CPC_TEST_EP_ID, CPC_TEST_RX_SIZE, cpc_test_event_cb, NULL);
  TEST_ASSERT_EQUAL(SL_STATUS_OK, status);

  status = sl_cpc_ep_listen(&ep, g_bus);
  TEST_ASSERT_EQUAL(SL_STATUS_OK, status);

  TEST_INJECT_FRAME({
    .flags = CPC_EXPECT_FLAG_ACK_REQ | CPC_EXPECT_FLAG_SYN,
    .len = CPC_TEST_PEER_MTU,
    .dst = CPC_TEST_EP_ID,
    .seq = CPC_TEST_PEER_SYN_SEQ,
    .ack = 0,
    .wnd = CPC_TEST_PEER_RX_WND,
  });

  mock_cpc_tx_packet_t *dut_syn_ack = TEST_POP_OUTGOING({
    .flags = CPC_EXPECT_FLAG_ACK_REQ | CPC_EXPECT_FLAG_SYN,
    .len = CPC_TEST_RX_SIZE,
    .dst = CPC_TEST_EP_ID,
    .seq = -1,
    .ack = (int)(CPC_TEST_PEER_SYN_SEQ + 1U),
    .wnd = 0,
  });
  TEST_ASSERT_EQUAL(SLI_CPC_STATE_SYN_RCVD, ep.state);
  our_syn_seq = sli_cpc_header_get_seq(&dut_syn_ack->header);
  mock_cpc_tx_packet_free(&dut_syn_ack);
  cpc_test_pump();
  cpc_test_wait_retx_timer();

  TEST_INJECT_FRAME({
    .flags = CPC_EXPECT_FLAG_ACK_REQ | CPC_EXPECT_FLAG_SYN,
    .len = CPC_TEST_PEER_MTU,
    .dst = CPC_TEST_EP_ID,
    .seq = CPC_TEST_PEER_SYN_SEQ,
    .ack = 0,
    .wnd = CPC_TEST_PEER_RX_WND,
  });

  cpc_test_pump();

  TEST_ASSERT_EQUAL(SLI_CPC_STATE_SYN_RCVD, ep.state);
  TEST_ASSERT_BIT_LOW(SL_CPC_EP_EVENT_ERROR, cpc_test_event_mask);
  TEST_ASSERT_NULL(mock_cpc_drv_pop_tx_packet(sl_cpc_drv_mock_default));

  TEST_ASSERT_TRUE_MESSAGE(mock_cpc_timer_fire_next(), "Expected SYN+ACK retransmit timer to be armed");
  cpc_test_pump();

  TEST_ASSERT_OUTGOING({
    .flags = CPC_EXPECT_FLAG_ACK_REQ | CPC_EXPECT_FLAG_SYN,
    .len = CPC_TEST_RX_SIZE,
    .dst = CPC_TEST_EP_ID,
    .seq = our_syn_seq,
    .ack = (int)(CPC_TEST_PEER_SYN_SEQ + 1U),
    .wnd = 0,
  });

  sl_cpc_ep_close(&ep);
  cpc_test_pump();
  cpc_test_ep_discard(&ep);
}

static uint8_t complete_active_handshake(sl_cpc_ep_t *ep)
{
  mock_cpc_tx_packet_t *dut_syn;
  uint8_t our_syn_seq;
  sl_status_t status;

  status = sl_cpc_ep_init(ep, CPC_TEST_EP_ID, CPC_TEST_RX_SIZE, cpc_test_event_cb, NULL);
  TEST_ASSERT_EQUAL(SL_STATUS_OK, status);

  status = sl_cpc_ep_connect(ep, g_bus);
  TEST_ASSERT_EQUAL(SL_STATUS_OK, status);

  dut_syn = TEST_POP_OUTGOING({
    .flags = CPC_EXPECT_FLAG_ACK_REQ | CPC_EXPECT_FLAG_SYN,
    .len = CPC_TEST_RX_SIZE,
    .dst = CPC_TEST_EP_ID,
    .seq = -1,
    .ack = 0,
    .wnd = 0,
  });
  our_syn_seq = sli_cpc_header_get_seq(&dut_syn->header);
  mock_cpc_tx_packet_free(&dut_syn);

  TEST_INJECT_FRAME({
    .flags = CPC_EXPECT_FLAG_ACK_REQ | CPC_EXPECT_FLAG_SYN,
    .len = CPC_TEST_PEER_MTU,
    .dst = CPC_TEST_EP_ID,
    .seq = CPC_TEST_PEER_SYN_SEQ,
    .ack = (int)(our_syn_seq + 1U),
    .wnd = CPC_TEST_PEER_RX_WND,
  });

  cpc_test_pump();

  TEST_ASSERT_EQUAL(SLI_CPC_STATE_CONNECTED, ep->state);
  TEST_ASSERT_OUTGOING({
    .len = 0,
    .dst = CPC_TEST_EP_ID,
    .seq = 0,
    .ack = (int)(CPC_TEST_PEER_SYN_SEQ + 1U),
    .wnd = 0,
  });
  cpc_test_pump();

  return our_syn_seq;
}

/**
 * @brief An exact duplicate SYN+ACK replays the final ACK without an error.
 */
TEST(cpc_syn, duplicate_syn_ack_in_connected_replays_ack)
{
  sl_cpc_ep_t ep;
  uint8_t our_syn_seq = complete_active_handshake(&ep);

  cpc_test_event_reset();

  TEST_INJECT_FRAME({
    .flags = CPC_EXPECT_FLAG_ACK_REQ | CPC_EXPECT_FLAG_SYN,
    .len = CPC_TEST_PEER_MTU,
    .dst = CPC_TEST_EP_ID,
    .seq = CPC_TEST_PEER_SYN_SEQ,
    .ack = (int)(our_syn_seq + 1U),
    .wnd = CPC_TEST_PEER_RX_WND,
  });

  cpc_test_pump();

  TEST_ASSERT_EQUAL(SLI_CPC_STATE_CONNECTED, ep.state);
  TEST_ASSERT_BIT_LOW(SL_CPC_EP_EVENT_CONNECTED, cpc_test_event_mask);
  TEST_ASSERT_BIT_LOW(SL_CPC_EP_EVENT_ERROR, cpc_test_event_mask);
  TEST_ASSERT_OUTGOING({
    .len = 0,
    .dst = CPC_TEST_EP_ID,
    .seq = 0,
    .ack = (int)(CPC_TEST_PEER_SYN_SEQ + 1U),
    .wnd = 0,
  });
  cpc_test_pump();

  sl_cpc_ep_close(&ep);
  cpc_test_pump();
  cpc_test_ep_discard(&ep);
}

/**
 * @brief A duplicate SYN+ACK with a changed seq, ack, or window is fatal.
 */
TEST(cpc_syn, inconsistent_duplicate_syn_ack_is_protocol_error)
{
  static const struct {
    uint8_t seq_delta;
    uint8_t ack_delta;
    uint8_t wnd_delta;
  } mismatches[] = {
    {.seq_delta = 1U},
    {.ack_delta = 1U},
    {.wnd_delta = 1U},
  };

  for (size_t i = 0; i < sizeof(mismatches) / sizeof(mismatches[0]); i++) {
    sl_cpc_ep_t ep;
    uint8_t our_syn_seq = complete_active_handshake(&ep);

    cpc_test_event_reset();

    TEST_INJECT_FRAME({
      .flags = CPC_EXPECT_FLAG_ACK_REQ | CPC_EXPECT_FLAG_SYN,
      .len = CPC_TEST_PEER_MTU,
      .dst = CPC_TEST_EP_ID,
      .seq = (int)(CPC_TEST_PEER_SYN_SEQ + mismatches[i].seq_delta),
      .ack = (int)(our_syn_seq + 1U + mismatches[i].ack_delta),
      .wnd = (int)(CPC_TEST_PEER_RX_WND + mismatches[i].wnd_delta),
    });

    cpc_test_pump();

    TEST_ASSERT_EQUAL(SLI_CPC_STATE_CLOSING, ep.state);
    TEST_ASSERT_BIT_HIGH(SL_CPC_EP_EVENT_ERROR, cpc_test_event_mask);
    TEST_ASSERT_OUTGOING({
      .flags = CPC_EXPECT_FLAG_RESET,
      .len = 0,
      .dst = CPC_TEST_EP_ID,
      .seq = 0,
      .ack = 0,
      .wnd = 0,
    });
    cpc_test_pump();

    sl_cpc_ep_close(&ep);
    cpc_test_pump();
    cpc_test_ep_discard(&ep);
  }
}

/**
 * @brief Repeating the final ACK after passive open is harmless.
 */
TEST(cpc_syn, duplicate_final_ack_in_connected_is_ignored)
{
  sl_cpc_ep_t ep;
  uint8_t our_syn_seq;
  sl_status_t status;

  status = sl_cpc_ep_init(&ep, CPC_TEST_EP_ID, CPC_TEST_RX_SIZE, cpc_test_event_cb, NULL);
  TEST_ASSERT_EQUAL(SL_STATUS_OK, status);
  status = sl_cpc_ep_listen(&ep, g_bus);
  TEST_ASSERT_EQUAL(SL_STATUS_OK, status);

  TEST_INJECT_FRAME({
    .flags = CPC_EXPECT_FLAG_ACK_REQ | CPC_EXPECT_FLAG_SYN,
    .len = CPC_TEST_PEER_MTU,
    .dst = CPC_TEST_EP_ID,
    .seq = CPC_TEST_PEER_SYN_SEQ,
    .ack = 0,
    .wnd = CPC_TEST_PEER_RX_WND,
  });

  mock_cpc_tx_packet_t *dut_syn_ack = TEST_POP_OUTGOING({
    .flags = CPC_EXPECT_FLAG_ACK_REQ | CPC_EXPECT_FLAG_SYN,
    .len = CPC_TEST_RX_SIZE,
    .dst = CPC_TEST_EP_ID,
    .seq = -1,
    .ack = (int)(CPC_TEST_PEER_SYN_SEQ + 1U),
    .wnd = 0,
  });
  our_syn_seq = sli_cpc_header_get_seq(&dut_syn_ack->header);
  mock_cpc_tx_packet_free(&dut_syn_ack);

  for (unsigned int i = 0; i < 2U; i++) {
    TEST_INJECT_FRAME({
      .dst = CPC_TEST_EP_ID,
      .seq = (int)(CPC_TEST_PEER_SYN_SEQ + 1U),
      .ack = (int)(our_syn_seq + 1U),
      .wnd = CPC_TEST_PEER_RX_WND,
    });
    cpc_test_pump();

    TEST_ASSERT_EQUAL(SLI_CPC_STATE_CONNECTED, ep.state);
    TEST_ASSERT_BIT_LOW(SL_CPC_EP_EVENT_ERROR, cpc_test_event_mask);
  }

  TEST_ASSERT_NULL(mock_cpc_drv_pop_tx_packet(sl_cpc_drv_mock_default));

  sl_cpc_ep_close(&ep);
  cpc_test_pump();
  cpc_test_ep_discard(&ep);
}

TEST_GROUP_RUNNER(cpc_syn)
{
  RUN_TEST_CASE(cpc_syn, listen_then_handshake_completes);
  RUN_TEST_CASE(cpc_syn, connect_then_handshake_completes);
  RUN_TEST_CASE(cpc_syn, connect_then_immediate_write_after_syn_ack);
  RUN_TEST_CASE(cpc_syn, connect_then_reset_notifies_error);
  RUN_TEST_CASE(cpc_syn, listen_then_reset_is_ignored);
  RUN_TEST_CASE(cpc_syn, connect_then_data_with_expected_seq_notifies_error);
  RUN_TEST_CASE(cpc_syn, connect_then_data_with_random_seq_notifies_error);
  RUN_TEST_CASE(cpc_syn, listen_then_data_with_valid_seq_ack_payload_completes_handshake);
  RUN_TEST_CASE(cpc_syn, listen_then_data_with_invalid_seq_valid_ack_payload_is_noop)
  RUN_TEST_CASE(cpc_syn, listen_then_data_replies_with_reset);
  RUN_TEST_CASE(cpc_syn, listen_then_standalone_ack_with_valid_seq_wrong_ack_past_is_ignored);
  RUN_TEST_CASE(cpc_syn, listen_then_standalone_ack_with_valid_seq_wrong_ack_future_is_ignored);
  RUN_TEST_CASE(cpc_syn, listen_then_payload_data_with_valid_seq_wrong_ack_is_ignored);
  RUN_TEST_CASE(cpc_syn, deferred_listen_drops_syn_until_bus_initialized);
  RUN_TEST_CASE(cpc_syn, duplicate_syn_in_syn_rcvd_is_ignored);
  RUN_TEST_CASE(cpc_syn, duplicate_syn_ack_in_connected_replays_ack);
  RUN_TEST_CASE(cpc_syn, inconsistent_duplicate_syn_ack_is_protocol_error);
  RUN_TEST_CASE(cpc_syn, duplicate_final_ack_in_connected_is_ignored);
}
