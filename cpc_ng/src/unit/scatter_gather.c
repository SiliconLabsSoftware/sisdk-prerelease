/***************************************************************************/ /**
 * @file
 * @brief Unit tests for CPC scatter-gather buffer allocation.
 ******************************************************************************/

#include <stdlib.h>
#include <string.h>

#include <unity_fixture.h>

#include "unity.h"

#include "sl_cpc_buf.h"
#include "sl_cpc_msgq.h"
#include "sl_status.h"

#include "../sli_cpc.h"
#include "../sli_cpc_bus.h"
#include "../sli_cpc_frame.h"
#include "../sli_cpc_hdr.h"
#include "../sli_cpc_memory.h"
#include "sl_cpc_bus_instances.h"
#include "sl_cpc_drv_mock.h"

#include "../../port/mock/src/mock_cpc_timer.h"
#include "expect.h"
#include "fixture.h"

TEST_GROUP(cpc_scatter_gather);

TEST_SETUP(cpc_scatter_gather)
{
  TEST_CPC_SETUP(true);
}

TEST_TEAR_DOWN(cpc_scatter_gather)
{
  TEST_CPC_TEAR_DOWN();
}

/**
 * @brief sli_cpc_buffer_chain_alloc() on an empty queue returns NULL.
 */
TEST(cpc_scatter_gather, build_buffer_chain_empty_queue_returns_null)
{
  sl_cpc_msgq_t msgq;

  sl_cpc_msgq_init(&msgq);
  TEST_ASSERT_TRUE(sl_cpc_msgq_is_empty(&msgq));

  TEST_ASSERT_NULL(sli_cpc_buffer_chain_alloc(&msgq, 100));
}

/**
 * @brief One buffer with an exact-length payload uses a single segment unchanged.
 */
TEST(cpc_scatter_gather, build_buffer_chain_single_exact_fit)
{
  sl_cpc_msgq_t msgq;
  sl_cpc_buf_t buf;
  sl_cpc_buf_t *out = NULL;
  const size_t mtu = 256;

  sl_cpc_msgq_init(&msgq);

  sl_cpc_buf_init(&buf, NULL, 256);
  sl_cpc_msgq_push(&msgq, &buf);

  // Try to build a buffer chain that consumes exactly
  // a single buffer of the message queue.
  out = sli_cpc_buffer_chain_alloc(&msgq, mtu);
  TEST_ASSERT_EQUAL_PTR(&buf, out);
  TEST_ASSERT_EQUAL_size_t(mtu, out->len);
  TEST_ASSERT_EQUAL_size_t(mtu, out->tot_len);
  TEST_ASSERT_NULL(out->node.node);

  TEST_ASSERT_TRUE(sl_cpc_msgq_is_empty(&msgq));
}

/**
 * @brief One buffer with a short payload trims the segment and updates tot_len.
 */
TEST(cpc_scatter_gather, build_buffer_chain_single_partial)
{
  sl_cpc_msgq_t msgq;
  sl_cpc_buf_t buf;
  sl_cpc_buf_t *out = NULL;
  const uint16_t payload_len = 100;
  const size_t mtu = 256;

  sl_cpc_msgq_init(&msgq);

  // Message queue with a single element of capacity 256
  sl_cpc_buf_init(&buf, NULL, mtu);
  sl_cpc_msgq_push(&msgq, &buf);

  // Want a buffer chain for receiving 100 bytes
  out = sli_cpc_buffer_chain_alloc(&msgq, payload_len);
  TEST_ASSERT_EQUAL_PTR(&buf, out);
  TEST_ASSERT_EQUAL_size_t(payload_len, out->len);
  TEST_ASSERT_EQUAL_size_t(payload_len, out->tot_len);
  TEST_ASSERT_NULL(out->node.node);

  // This should have depleted the msgq
  TEST_ASSERT_TRUE(sl_cpc_msgq_is_empty(&msgq));
}

/**
 * @brief A payload spanning two full buffers chains both segments.
 */
TEST(cpc_scatter_gather, build_buffer_chain_two_buffers_exact)
{
  sl_cpc_msgq_t msgq;
  sl_cpc_buf_t buf1;
  sl_cpc_buf_t buf2;
  sl_cpc_buf_t *out = NULL;
  const size_t mtu = 256;

  sl_cpc_msgq_init(&msgq);

  sl_cpc_buf_init(&buf1, NULL, mtu);
  sl_cpc_msgq_push(&msgq, &buf1);

  sl_cpc_buf_init(&buf2, NULL, mtu);
  sl_cpc_msgq_push(&msgq, &buf2);

  out = sli_cpc_buffer_chain_alloc(&msgq, 2 * mtu);
  // test we got the first buffer first
  TEST_ASSERT_EQUAL_PTR(&buf1, out);
  TEST_ASSERT_EQUAL_size_t(2 * mtu, out->tot_len);
  TEST_ASSERT_EQUAL_size_t(mtu, out->len);

  // and that second buffer is chained
  TEST_ASSERT_EQUAL_PTR(&buf2.node, out->node.node);
  TEST_ASSERT_EQUAL_size_t(mtu, buf2.len);
  TEST_ASSERT_EQUAL_size_t(mtu, buf2.tot_len);
  TEST_ASSERT_NULL(buf2.node.node);

  TEST_ASSERT_TRUE(sl_cpc_msgq_is_empty(&msgq));
}

/**
 * @brief A payload spanning two buffers with a trimmed tail updates cumulative tot_len.
 */
TEST(cpc_scatter_gather, build_buffer_chain_two_buffers_partial_tail)
{
  sl_cpc_msgq_t msgq;
  sl_cpc_buf_t buf1;
  sl_cpc_buf_t buf2;
  sl_cpc_buf_t *out = NULL;
  const uint16_t payload_len = 300;
  const size_t mtu = 256;

  sl_cpc_msgq_init(&msgq);

  sl_cpc_buf_init(&buf1, NULL, mtu);
  sl_cpc_msgq_push(&msgq, &buf1);

  sl_cpc_buf_init(&buf2, NULL, mtu);
  sl_cpc_msgq_push(&msgq, &buf2);

  out = sli_cpc_buffer_chain_alloc(&msgq, payload_len);
  TEST_ASSERT_EQUAL_PTR(&buf1, out);
  TEST_ASSERT_EQUAL_size_t(payload_len, out->tot_len);
  TEST_ASSERT_EQUAL_size_t(mtu, out->len);

  TEST_ASSERT_EQUAL_PTR(&buf2.node, out->node.node);
  TEST_ASSERT_EQUAL_size_t(44, buf2.len);
  TEST_ASSERT_EQUAL_size_t(44, buf2.tot_len);
  TEST_ASSERT_NULL(buf2.node.node);

  TEST_ASSERT_TRUE(sl_cpc_msgq_is_empty(&msgq));
}

/**
 * @brief A payload spanning three buffers with a trimmed tail builds the full chain.
 */
TEST(cpc_scatter_gather, build_buffer_chain_three_buffers_partial_tail)
{
  sl_cpc_msgq_t msgq;
  sl_cpc_buf_t buf1;
  sl_cpc_buf_t buf2;
  sl_cpc_buf_t buf3;
  sl_cpc_buf_t *out = NULL;
  const uint16_t payload_len = 600;
  const size_t mtu = 256;

  sl_cpc_msgq_init(&msgq);

  sl_cpc_buf_init(&buf1, NULL, mtu);
  sl_cpc_msgq_push(&msgq, &buf1);

  sl_cpc_buf_init(&buf2, NULL, mtu);
  sl_cpc_msgq_push(&msgq, &buf2);

  sl_cpc_buf_init(&buf3, NULL, mtu);
  sl_cpc_msgq_push(&msgq, &buf3);

  out = sli_cpc_buffer_chain_alloc(&msgq, payload_len);
  TEST_ASSERT_EQUAL_PTR(&buf1, out);
  TEST_ASSERT_EQUAL_size_t(payload_len, out->tot_len);
  TEST_ASSERT_EQUAL_size_t(mtu, buf1.len);

  TEST_ASSERT_EQUAL_PTR(&buf2.node, buf1.node.node);
  TEST_ASSERT_EQUAL_size_t(mtu, buf2.len);
  TEST_ASSERT_EQUAL_size_t(payload_len - mtu, buf2.tot_len);

  TEST_ASSERT_EQUAL_PTR(&buf3.node, buf2.node.node);
  TEST_ASSERT_EQUAL_size_t(88, buf3.len);
  TEST_ASSERT_EQUAL_size_t(88, buf3.tot_len);
  TEST_ASSERT_NULL(buf3.node.node);

  TEST_ASSERT_TRUE(sl_cpc_msgq_is_empty(&msgq));
}

/**
 * @brief When the queue runs out of buffers, popped segments are restored to the queue.
 */
TEST(cpc_scatter_gather, build_buffer_chain_insufficient_buffers_restores_queue)
{
  sl_cpc_msgq_t msgq;
  sl_cpc_buf_t buf;
  sl_cpc_buf_t *out = NULL;
  const size_t mtu = 256;

  sl_cpc_msgq_init(&msgq);

  sl_cpc_buf_init(&buf, NULL, mtu);
  sl_cpc_msgq_push(&msgq, &buf);

  TEST_ASSERT_NULL(sli_cpc_buffer_chain_alloc(&msgq, mtu + 1));
  TEST_ASSERT_EQUAL_size_t(1, sl_cpc_msgq_len(&msgq));

  TEST_ASSERT_TRUE(sl_cpc_msgq_pop(&msgq, &out));
  TEST_ASSERT_EQUAL_PTR(&buf, out);
  TEST_ASSERT_EQUAL_size_t(mtu, out->len);
  TEST_ASSERT_EQUAL_size_t(mtu, out->tot_len);

  TEST_ASSERT_NULL(out->node.node);

  TEST_ASSERT_TRUE(sl_cpc_msgq_is_empty(&msgq));
}

static void scatter_gather_event_cb(sl_cpc_ep_t *ep, sl_cpc_ep_event_type_t type, const sl_cpc_ep_event_t *event,
                                    void *arg)
{
  (void)ep;
  (void)event;
  (void)arg;

  cpc_test_event_mask |= 1 << type;
}

/**
 * @brief Transmitted payloads advance send_nxt by the number of remote reception buffers consumed.
 */
TEST(cpc_scatter_gather, tx_payload_advances_sequence_by_buffer_count)
{
  const uint8_t remote_rx_wnd = 10U;
  const uint16_t remote_mtu = 128U;
  uint8_t payload_large[200];
  uint8_t payload_small[64];
  uint8_t payload_mtu[128];
  uint8_t payload_seq = 1U;

  sl_cpc_ep_t ep;
  sl_cpc_frame_t frame1;
  sl_cpc_frame_t frame2;
  sl_cpc_frame_t frame3;
  sl_cpc_buf_t buf1;
  sl_cpc_buf_t buf2;
  sl_cpc_buf_t buf3;
  sl_status_t status;

  memset(payload_small, 0x11, sizeof(payload_small));
  memset(payload_mtu, 0x22, sizeof(payload_mtu));
  memset(payload_large, 0x33, sizeof(payload_large));

  status = sl_cpc_ep_init(&ep, CPC_TEST_EP_ID, CPC_TEST_RX_SIZE, scatter_gather_event_cb, NULL);
  TEST_ASSERT_EQUAL(SL_STATUS_OK, status);

  status = cpc_test_set_ep_connected(&ep, g_bus, payload_seq, CPC_TEST_PEER_SYN_SEQ + 1U, CPC_TEST_RX_SIZE, remote_mtu,
                                     remote_rx_wnd);
  TEST_ASSERT_EQUAL(SL_STATUS_OK, status);

  // 64-byte payload: consumes one remote reception buffer.
  sl_cpc_buf_init(&buf1, payload_small, sizeof(payload_small));
  status = sl_cpc_ep_send(&ep, &buf1, &frame1, NULL);
  TEST_ASSERT_EQUAL(SL_STATUS_OK, status);

  TEST_ASSERT_OUTGOING({
    .flags = CPC_EXPECT_FLAG_ACK_REQ,
    .len = (ssize_t)sizeof(payload_small),
    .dst = CPC_TEST_EP_ID,
    .seq = (int)payload_seq,
    .ack = CPC_TEST_PEER_SYN_SEQ + 1U,
    .wnd = 0,
    .payload = payload_small,
  });
  TEST_ASSERT_EQUAL_UINT8(payload_seq, ep.send_una);
  TEST_ASSERT_EQUAL_UINT8(payload_seq + 1U, ep.send_nxt);

  TEST_INJECT_FRAME({
    .flags = 0,
    .dst = CPC_TEST_EP_ID,
    .seq = 0,
    .ack = (int)(payload_seq + 1U),
    .wnd = remote_rx_wnd,
  });

  cpc_test_pump();

  TEST_ASSERT_EQUAL_UINT8(payload_seq + 1, ep.send_una);
  TEST_ASSERT_EQUAL_UINT8(payload_seq + 1, ep.send_nxt);

  // 128-byte payload: consumes one remote reception buffer.
  sl_cpc_buf_init(&buf2, payload_mtu, sizeof(payload_mtu));
  status = sl_cpc_ep_send(&ep, &buf2, &frame2, NULL);
  TEST_ASSERT_EQUAL(SL_STATUS_OK, status);

  TEST_ASSERT_OUTGOING({
    .flags = CPC_EXPECT_FLAG_ACK_REQ,
    .len = (ssize_t)sizeof(payload_mtu),
    .dst = CPC_TEST_EP_ID,
    .seq = payload_seq + 1,
    .ack = CPC_TEST_PEER_SYN_SEQ + 1U,
    .wnd = 0,
    .payload = payload_mtu,
  });
  TEST_ASSERT_EQUAL_UINT8(payload_seq + 1, ep.send_una);
  TEST_ASSERT_EQUAL_UINT8(payload_seq + 2, ep.send_nxt);

  TEST_INJECT_FRAME({
    .flags = 0,
    .dst = CPC_TEST_EP_ID,
    .seq = 0,
    .ack = payload_seq + 2,
    .wnd = remote_rx_wnd,
  });

  cpc_test_pump();

  TEST_ASSERT_EQUAL_UINT8(payload_seq + 2, ep.send_una);
  TEST_ASSERT_EQUAL_UINT8(payload_seq + 2, ep.send_nxt);

  // 200-byte payload: consumes two remote reception buffers.
  sl_cpc_buf_init(&buf3, payload_large, sizeof(payload_large));
  status = sl_cpc_ep_send(&ep, &buf3, &frame3, NULL);
  TEST_ASSERT_EQUAL(SL_STATUS_OK, status);

  TEST_ASSERT_OUTGOING({
    .flags = CPC_EXPECT_FLAG_ACK_REQ,
    .len = (ssize_t)sizeof(payload_large),
    .dst = CPC_TEST_EP_ID,
    .seq = payload_seq + 2,
    .ack = CPC_TEST_PEER_SYN_SEQ + 1U,
    .wnd = 0,
    .payload = payload_large,
  });
  TEST_ASSERT_EQUAL_UINT8(payload_seq + 2, ep.send_una);
  TEST_ASSERT_EQUAL_UINT8(payload_seq + 4, ep.send_nxt);

  TEST_INJECT_FRAME({
    .flags = 0,
    .dst = CPC_TEST_EP_ID,
    .seq = 0,
    .ack = payload_seq + 4,
    .wnd = remote_rx_wnd,
  });

  cpc_test_pump();
  TEST_ASSERT_EQUAL_UINT8(payload_seq + 4, ep.send_una);
  TEST_ASSERT_EQUAL_UINT8(payload_seq + 4, ep.send_nxt);

  sl_cpc_ep_close(&ep);

  cpc_test_pump();
  cpc_test_ep_discard(&ep);
}

/**
 * @brief A chained buffer whose total length is below remote_mtu still advances the sequence by one.
 */
TEST(cpc_scatter_gather, tx_chained_buffers_below_mtu_advances_one_sequence)
{
  const uint8_t remote_rx_wnd = 10U;
  const uint16_t remote_mtu = 128U;
  const uint8_t payload_seq = 1U;
  const size_t segment_len = 40U;
  const size_t payload_len = 2U * segment_len;
  uint8_t segment1[40];
  uint8_t segment2[40];

  sl_cpc_frame_t frame;
  sl_cpc_buf_t buf_head;
  sl_cpc_buf_t buf_tail;
  sl_status_t status;
  sl_cpc_ep_t ep;

  memset(segment1, 0xAA, sizeof(segment1));
  memset(segment2, 0xBB, sizeof(segment2));

  sl_cpc_buf_init(&buf_head, segment1, segment_len);
  sl_cpc_buf_init(&buf_tail, segment2, segment_len);
  sl_cpc_buf_chain(&buf_head, &buf_tail);

  status = sl_cpc_ep_init(&ep, CPC_TEST_EP_ID, CPC_TEST_RX_SIZE, scatter_gather_event_cb, NULL);
  TEST_ASSERT_EQUAL(SL_STATUS_OK, status);

  status = cpc_test_set_ep_connected(&ep, g_bus, payload_seq, CPC_TEST_PEER_SYN_SEQ + 1U, CPC_TEST_RX_SIZE, remote_mtu,
                                     remote_rx_wnd);
  TEST_ASSERT_EQUAL(SL_STATUS_OK, status);

  status = sl_cpc_ep_send(&ep, &buf_head, &frame, NULL);
  TEST_ASSERT_EQUAL(SL_STATUS_OK, status);

  TEST_ASSERT_OUTGOING({
    .flags = CPC_EXPECT_FLAG_ACK_REQ,
    .len = (ssize_t)payload_len,
    .dst = CPC_TEST_EP_ID,
    .seq = (int)payload_seq,
    .ack = CPC_TEST_PEER_SYN_SEQ + 1U,
    .wnd = 0,
  });
  TEST_ASSERT_EQUAL_UINT8(payload_seq, ep.send_una);
  TEST_ASSERT_EQUAL_UINT8(payload_seq + 1U, ep.send_nxt);

  TEST_INJECT_FRAME({
    .flags = 0,
    .dst = CPC_TEST_EP_ID,
    .seq = 0,
    .ack = (int)(payload_seq + 1U),
    .wnd = remote_rx_wnd,
  });

  cpc_test_pump();

  TEST_ASSERT_EQUAL_UINT8(payload_seq + 1U, ep.send_una);
  TEST_ASSERT_EQUAL_UINT8(payload_seq + 1U, ep.send_nxt);

  sl_cpc_ep_close(&ep);

  cpc_test_pump();
  cpc_test_ep_discard(&ep);
}

/**
 * @brief A stale ACK that does not advance past send_una leaves the frame unacked and
 *        keeps the retransmit timer armed.
 */
TEST(cpc_scatter_gather, tx_payload_partial_ack_is_ignored)
{
  const uint8_t expected_seq_advance = 2U;
  const uint16_t remote_mtu = 128U;
  const size_t payload_len = 200U;
  uint8_t payload_seq = 1U;
  uint8_t payload[200];

  sl_cpc_frame_t frame;
  sl_status_t status;
  sl_cpc_buf_t buf;
  sl_cpc_ep_t ep;

  memset(payload, 0xA5, sizeof(payload));
  sl_cpc_buf_init(&buf, payload, payload_len);

  status = sl_cpc_ep_init(&ep, CPC_TEST_EP_ID, CPC_TEST_RX_SIZE, scatter_gather_event_cb, NULL);
  TEST_ASSERT_EQUAL(SL_STATUS_OK, status);

  status
    = cpc_test_set_ep_connected(&ep, g_bus, payload_seq, CPC_TEST_PEER_SYN_SEQ + 1U, CPC_TEST_RX_SIZE, remote_mtu, 10U);
  TEST_ASSERT_EQUAL(SL_STATUS_OK, status);

  status = sl_cpc_ep_send(&ep, &buf, &frame, NULL);
  TEST_ASSERT_EQUAL(SL_STATUS_OK, status);

  TEST_ASSERT_OUTGOING({
    .flags = CPC_EXPECT_FLAG_ACK_REQ,
    .len = (ssize_t)payload_len,
    .dst = CPC_TEST_EP_ID,
    .seq = (int)payload_seq,
    .ack = CPC_TEST_PEER_SYN_SEQ + 1U,
    .wnd = 0,
  });

  TEST_ASSERT_EQUAL_UINT8(payload_seq, ep.send_una);
  TEST_ASSERT_EQUAL_UINT8(payload_seq + expected_seq_advance, ep.send_nxt);

  cpc_test_pump();

  cpc_test_wait_retx_timer();
  TEST_ASSERT_EQUAL(1, mock_cpc_timer_count_running());

  // The core has sent a frame with seq=payload_seq, and this frame spans over
  // two reception buffers.
  //   send_una = payload_seq;
  //   send_nxt = payload_seq + 2
  // So the expect ack number should be payload_seq + 2, but let's not do that.
  // First, check our partial_ack is sensible, to make sure the test actually
  // tests what it's supposed to.
  uint8_t partial_ack = payload_seq + 1;
  TEST_ASSERT_GREATER_THAN_UINT8(ep.send_una, partial_ack);
  TEST_ASSERT_LESS_THAN_UINT8(ep.send_nxt, partial_ack);

  // Inject this partial ack. This acks only the first half of the frame,
  // this should be ignored.
  TEST_INJECT_FRAME({
    .flags = 0,
    .dst = CPC_TEST_EP_ID,
    .seq = 0,
    .ack = (int)partial_ack,
    .wnd = 10,
  });

  cpc_test_pump();

  // Endpoint state should be the same as before injecting the ack
  TEST_ASSERT_EQUAL_UINT8(payload_seq, ep.send_una);
  TEST_ASSERT_EQUAL_UINT8(payload_seq + expected_seq_advance, ep.send_nxt);
  TEST_ASSERT_EQUAL(1, mock_cpc_timer_count_running());

  uint8_t complete_ack = payload_seq + expected_seq_advance;
  TEST_ASSERT_EQUAL_UINT8(complete_ack, ep.send_nxt);

  // Inject full frame ack
  TEST_INJECT_FRAME({
    .flags = 0,
    .dst = CPC_TEST_EP_ID,
    .seq = 0,
    .ack = (int)complete_ack,
    .wnd = 10,
  });
  cpc_test_pump();

  // This should be accepted by the core
  TEST_ASSERT_EQUAL_UINT8(ep.send_una, complete_ack);
  TEST_ASSERT_EQUAL_UINT8(ep.send_nxt, complete_ack);
  TEST_ASSERT_EQUAL(0, mock_cpc_timer_count_running());

  sl_cpc_ep_close(&ep);

  cpc_test_pump();
  cpc_test_ep_discard(&ep);
}

/**
 * @brief Two multi-buffer frames near the sequence wrap point are cumulatively
 *        acknowledged by a single control ACK.
 */
TEST(cpc_scatter_gather, tx_two_multi_buffer_frames_single_ack)
{
  const uint8_t seq_advance_per_frame = 2U;
  const uint8_t remote_rx_wnd = 10U;
  const uint16_t remote_mtu = 128U;
  const uint8_t payload_seq = 253U;
  const size_t payload_len = 200U;
  uint8_t payload1[200];
  uint8_t payload2[200];

  sl_cpc_frame_t frame1;
  sl_cpc_frame_t frame2;
  sl_status_t status;
  sl_cpc_buf_t buf1;
  sl_cpc_buf_t buf2;
  sl_cpc_ep_t ep;

  memset(payload1, 0x11, sizeof(payload1));
  memset(payload2, 0x22, sizeof(payload2));
  sl_cpc_buf_init(&buf1, payload1, payload_len);
  sl_cpc_buf_init(&buf2, payload2, payload_len);

  status = sl_cpc_ep_init(&ep, CPC_TEST_EP_ID, CPC_TEST_RX_SIZE, scatter_gather_event_cb, NULL);
  TEST_ASSERT_EQUAL(SL_STATUS_OK, status);

  status = cpc_test_set_ep_connected(&ep, g_bus, payload_seq, CPC_TEST_PEER_SYN_SEQ + 1U, CPC_TEST_RX_SIZE, remote_mtu,
                                     remote_rx_wnd);
  TEST_ASSERT_EQUAL(SL_STATUS_OK, status);

  status = sl_cpc_ep_send(&ep, &buf1, &frame1, NULL);
  TEST_ASSERT_EQUAL(SL_STATUS_OK, status);

  status = sl_cpc_ep_send(&ep, &buf2, &frame2, NULL);
  TEST_ASSERT_EQUAL(SL_STATUS_OK, status);

  cpc_test_pump();

  TEST_ASSERT_OUTGOING({
    .flags = CPC_EXPECT_FLAG_ACK_REQ,
    .len = (ssize_t)payload_len,
    .dst = CPC_TEST_EP_ID,
    .seq = (int)payload_seq,
    .ack = CPC_TEST_PEER_SYN_SEQ + 1U,
    .wnd = 0,
    .payload = payload1,
  });

  TEST_ASSERT_OUTGOING({
    .flags = CPC_EXPECT_FLAG_ACK_REQ,
    .len = (ssize_t)payload_len,
    .dst = CPC_TEST_EP_ID,
    .seq = (int)(payload_seq + seq_advance_per_frame),
    .ack = CPC_TEST_PEER_SYN_SEQ + 1U,
    .wnd = 0,
    .payload = payload2,
  });

  TEST_ASSERT_EQUAL_UINT8(payload_seq, ep.send_una);
  TEST_ASSERT_EQUAL_UINT8(payload_seq + (2U * seq_advance_per_frame), ep.send_nxt);

  cpc_test_wait_retx_timer();
  TEST_ASSERT_EQUAL(1, mock_cpc_timer_count_running());

  TEST_INJECT_FRAME({
    .flags = 0,
    .dst = CPC_TEST_EP_ID,
    .seq = 0,
    .ack = payload_seq + (2 * seq_advance_per_frame),
    .wnd = remote_rx_wnd,
  });

  cpc_test_pump();

  TEST_ASSERT_EQUAL_UINT8(ep.send_nxt, ep.send_una);
  TEST_ASSERT_EQUAL(0, mock_cpc_timer_count_running());

  sl_cpc_ep_close(&ep);

  cpc_test_pump();
  cpc_test_ep_discard(&ep);
}

/**
 * @brief A single-buffer frame followed by a two-buffer frame are cumulatively
 *        acknowledged by a single control ACK across the sequence wrap point.
 */
TEST(cpc_scatter_gather, tx_one_buffer_then_two_buffers_single_ack)
{
  const size_t single_buffer_payload_len = 64U;
  const size_t two_buffer_payload_len = 200U;
  const uint8_t seq_advance_second = 2U;
  const uint8_t seq_advance_first = 1U;
  const uint8_t remote_rx_wnd = 10U;
  const uint16_t remote_mtu = 128U;
  const uint8_t payload_seq = 253U;
  uint8_t payload2[200];
  uint8_t payload1[64];

  sl_cpc_frame_t frame1;
  sl_cpc_frame_t frame2;
  sl_status_t status;
  sl_cpc_buf_t buf1;
  sl_cpc_buf_t buf2;
  sl_cpc_ep_t ep;

  memset(payload1, 0x11, sizeof(payload1));
  memset(payload2, 0x22, sizeof(payload2));
  sl_cpc_buf_init(&buf1, payload1, single_buffer_payload_len);
  sl_cpc_buf_init(&buf2, payload2, two_buffer_payload_len);

  status = sl_cpc_ep_init(&ep, CPC_TEST_EP_ID, CPC_TEST_RX_SIZE, scatter_gather_event_cb, NULL);
  TEST_ASSERT_EQUAL(SL_STATUS_OK, status);

  status = cpc_test_set_ep_connected(&ep, g_bus, payload_seq, CPC_TEST_PEER_SYN_SEQ + 1U, CPC_TEST_RX_SIZE, remote_mtu,
                                     remote_rx_wnd);
  TEST_ASSERT_EQUAL(SL_STATUS_OK, status);

  status = sl_cpc_ep_send(&ep, &buf1, &frame1, NULL);
  TEST_ASSERT_EQUAL(SL_STATUS_OK, status);

  status = sl_cpc_ep_send(&ep, &buf2, &frame2, NULL);
  TEST_ASSERT_EQUAL(SL_STATUS_OK, status);

  cpc_test_pump();

  TEST_ASSERT_OUTGOING({
    .flags = CPC_EXPECT_FLAG_ACK_REQ,
    .len = (ssize_t)single_buffer_payload_len,
    .dst = CPC_TEST_EP_ID,
    .seq = (int)payload_seq,
    .ack = CPC_TEST_PEER_SYN_SEQ + 1U,
    .wnd = 0,
    .payload = payload1,
  });

  TEST_ASSERT_OUTGOING({
    .flags = CPC_EXPECT_FLAG_ACK_REQ,
    .len = (ssize_t)two_buffer_payload_len,
    .dst = CPC_TEST_EP_ID,
    .seq = (int)(payload_seq + seq_advance_first),
    .ack = CPC_TEST_PEER_SYN_SEQ + 1U,
    .wnd = 0,
    .payload = payload2,
  });

  TEST_ASSERT_EQUAL_UINT8(payload_seq, ep.send_una);
  TEST_ASSERT_EQUAL_UINT8(payload_seq + seq_advance_first + seq_advance_second, ep.send_nxt);

  cpc_test_wait_retx_timer();
  TEST_ASSERT_EQUAL(1, mock_cpc_timer_count_running());

  TEST_INJECT_FRAME({
    .flags = 0,
    .dst = CPC_TEST_EP_ID,
    .seq = 0,
    .ack = payload_seq + seq_advance_first + seq_advance_second,
    .wnd = remote_rx_wnd,
  });

  cpc_test_pump();

  TEST_ASSERT_EQUAL_UINT8(ep.send_nxt, ep.send_una);
  TEST_ASSERT_EQUAL(0, mock_cpc_timer_count_running());

  sl_cpc_ep_close(&ep);

  cpc_test_pump();
  cpc_test_ep_discard(&ep);
}

/**
 * @brief A two-buffer frame followed by a single-buffer frame are cumulatively
 *        acknowledged by a single control ACK across the sequence wrap point.
 */
TEST(cpc_scatter_gather, tx_two_buffers_then_one_buffer_single_ack)
{
  const size_t single_buffer_payload_len = 64U;
  const size_t two_buffer_payload_len = 200U;
  const uint8_t seq_advance_second = 1U;
  const uint8_t seq_advance_first = 2U;
  const uint8_t remote_rx_wnd = 10U;
  const uint16_t remote_mtu = 128U;
  const uint8_t payload_seq = 253U;
  uint8_t payload1[200];
  uint8_t payload2[64];

  sl_cpc_frame_t frame1;
  sl_cpc_frame_t frame2;
  sl_status_t status;
  sl_cpc_buf_t buf1;
  sl_cpc_buf_t buf2;
  sl_cpc_ep_t ep;

  memset(payload1, 0x11, sizeof(payload1));
  memset(payload2, 0x22, sizeof(payload2));
  sl_cpc_buf_init(&buf1, payload1, two_buffer_payload_len);
  sl_cpc_buf_init(&buf2, payload2, single_buffer_payload_len);

  status = sl_cpc_ep_init(&ep, CPC_TEST_EP_ID, CPC_TEST_RX_SIZE, scatter_gather_event_cb, NULL);
  TEST_ASSERT_EQUAL(SL_STATUS_OK, status);

  status = cpc_test_set_ep_connected(&ep, g_bus, payload_seq, CPC_TEST_PEER_SYN_SEQ + 1U, CPC_TEST_RX_SIZE, remote_mtu,
                                     remote_rx_wnd);
  TEST_ASSERT_EQUAL(SL_STATUS_OK, status);

  status = sl_cpc_ep_send(&ep, &buf1, &frame1, NULL);
  TEST_ASSERT_EQUAL(SL_STATUS_OK, status);

  status = sl_cpc_ep_send(&ep, &buf2, &frame2, NULL);
  TEST_ASSERT_EQUAL(SL_STATUS_OK, status);

  cpc_test_pump();

  TEST_ASSERT_OUTGOING({
    .flags = CPC_EXPECT_FLAG_ACK_REQ,
    .len = (ssize_t)two_buffer_payload_len,
    .dst = CPC_TEST_EP_ID,
    .seq = (int)payload_seq,
    .ack = CPC_TEST_PEER_SYN_SEQ + 1U,
    .wnd = 0,
    .payload = payload1,
  });

  TEST_ASSERT_OUTGOING({
    .flags = CPC_EXPECT_FLAG_ACK_REQ,
    .len = (ssize_t)single_buffer_payload_len,
    .dst = CPC_TEST_EP_ID,
    .seq = (int)(payload_seq + seq_advance_first),
    .ack = CPC_TEST_PEER_SYN_SEQ + 1U,
    .wnd = 0,
    .payload = payload2,
  });

  TEST_ASSERT_EQUAL_UINT8(payload_seq, ep.send_una);
  TEST_ASSERT_EQUAL_UINT8(payload_seq + seq_advance_first + seq_advance_second, ep.send_nxt);

  cpc_test_wait_retx_timer();
  TEST_ASSERT_EQUAL(1, mock_cpc_timer_count_running());

  TEST_INJECT_FRAME({
    .flags = 0,
    .dst = CPC_TEST_EP_ID,
    .seq = 0,
    .ack = payload_seq + seq_advance_first + seq_advance_second,
    .wnd = remote_rx_wnd,
  });

  cpc_test_pump();

  TEST_ASSERT_EQUAL_UINT8(ep.send_nxt, ep.send_una);
  TEST_ASSERT_EQUAL(0, mock_cpc_timer_count_running());

  sl_cpc_ep_close(&ep);

  cpc_test_pump();
  cpc_test_ep_discard(&ep);
}

/**
 * @brief Received payloads advance ep->ack by the number of local RX buffers consumed.
 */
TEST(cpc_scatter_gather, rx_payload_advances_ack_by_buffer_count)
{
  const uint8_t peer_seq = CPC_TEST_PEER_SYN_SEQ + 1U;
  const uint16_t local_mtu = 32;
  uint8_t rx_data[4][local_mtu];
  uint8_t payload_small[local_mtu - 16];
  uint8_t payload_large[local_mtu + 16];
  uint8_t payload_mtu[local_mtu];
  sl_cpc_buf_t rx_bufs[4];

  sl_status_t status;
  sl_cpc_ep_t ep;

  memset(payload_small, 0x11, sizeof(payload_small));
  memset(payload_mtu, 0x22, sizeof(payload_mtu));
  memset(payload_large, 0x33, sizeof(payload_large));

  status = sl_cpc_ep_init(&ep, CPC_TEST_EP_ID, local_mtu, scatter_gather_event_cb, NULL);
  TEST_ASSERT_EQUAL(SL_STATUS_OK, status);

  for (size_t i = 0; i < 4; i++) {
    sl_cpc_buf_init(&rx_bufs[i], rx_data[i], local_mtu);
    sl_cpc_ep_push_recv_buf(&ep, &rx_bufs[i]);
  }

  status = cpc_test_set_ep_connected(&ep, g_bus, 1U, peer_seq, local_mtu, CPC_TEST_PEER_MTU, 10U);
  TEST_ASSERT_EQUAL(SL_STATUS_OK, status);
  TEST_ASSERT_EQUAL_UINT8(peer_seq, ep.ack);

  // 64-byte payload: consumes one reception buffer.
  TEST_INJECT_FRAME({
    .flags = CPC_EXPECT_FLAG_ACK_REQ,
    .len = (ssize_t)sizeof(payload_small),
    .dst = CPC_TEST_EP_ID,
    .seq = (int)peer_seq,
    .ack = 0,
    .wnd = 0,
    .payload = payload_small,
  });

  TEST_ASSERT_OUTGOING({
    .flags = 0,
    .len = 0,
    .dst = CPC_TEST_EP_ID,
    .seq = 0,
    .ack = (int)(peer_seq + 1),
    .wnd = 3,
  });

  cpc_test_pump();
  TEST_ASSERT_EQUAL_UINT8(peer_seq + 1, ep.ack);

  // 128-byte payload: consumes one reception buffer.
  TEST_INJECT_FRAME({
    .flags = CPC_EXPECT_FLAG_ACK_REQ,
    .len = (ssize_t)sizeof(payload_mtu),
    .dst = CPC_TEST_EP_ID,
    .seq = (int)peer_seq + 1,
    .ack = 0,
    .wnd = 0,
    .payload = payload_mtu,
  });

  TEST_ASSERT_OUTGOING({
    .flags = 0,
    .len = 0,
    .dst = CPC_TEST_EP_ID,
    .seq = 0,
    .ack = (int)(peer_seq + 2U),
    .wnd = 2,
  });

  cpc_test_pump();
  TEST_ASSERT_EQUAL_UINT8(peer_seq + 2, ep.ack);

  // 200-byte payload: consumes two reception buffers.
  TEST_INJECT_FRAME({
    .flags = CPC_EXPECT_FLAG_ACK_REQ,
    .len = (ssize_t)sizeof(payload_large),
    .dst = CPC_TEST_EP_ID,
    .seq = (int)peer_seq + 2,
    .ack = 0,
    .wnd = 0,
    .payload = payload_large,
  });

  TEST_ASSERT_OUTGOING({
    .flags = 0,
    .len = 0,
    .dst = CPC_TEST_EP_ID,
    .seq = 0,
    .ack = (int)(peer_seq + 4),
    .wnd = 0,
  });

  cpc_test_pump();
  TEST_ASSERT_EQUAL_UINT8(peer_seq + 4, ep.ack);

  sl_cpc_ep_close(&ep);

  cpc_test_pump();
  cpc_test_ep_discard(&ep);
}

TEST_GROUP_RUNNER(cpc_scatter_gather)
{
  RUN_TEST_CASE(cpc_scatter_gather, build_buffer_chain_empty_queue_returns_null);
  RUN_TEST_CASE(cpc_scatter_gather, build_buffer_chain_single_exact_fit);
  RUN_TEST_CASE(cpc_scatter_gather, build_buffer_chain_single_partial);
  RUN_TEST_CASE(cpc_scatter_gather, build_buffer_chain_two_buffers_exact);
  RUN_TEST_CASE(cpc_scatter_gather, build_buffer_chain_two_buffers_partial_tail);
  RUN_TEST_CASE(cpc_scatter_gather, build_buffer_chain_three_buffers_partial_tail);
  RUN_TEST_CASE(cpc_scatter_gather, build_buffer_chain_insufficient_buffers_restores_queue);
  RUN_TEST_CASE(cpc_scatter_gather, tx_payload_advances_sequence_by_buffer_count);
  RUN_TEST_CASE(cpc_scatter_gather, tx_chained_buffers_below_mtu_advances_one_sequence);
  RUN_TEST_CASE(cpc_scatter_gather, tx_payload_partial_ack_is_ignored);
  RUN_TEST_CASE(cpc_scatter_gather, tx_two_multi_buffer_frames_single_ack);
  RUN_TEST_CASE(cpc_scatter_gather, tx_one_buffer_then_two_buffers_single_ack);
  RUN_TEST_CASE(cpc_scatter_gather, tx_two_buffers_then_one_buffer_single_ack);
  RUN_TEST_CASE(cpc_scatter_gather, rx_payload_advances_ack_by_buffer_count);
}
