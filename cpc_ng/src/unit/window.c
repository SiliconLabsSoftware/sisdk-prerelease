/***************************************************************************/ /**
 * @file
 * @brief Unit tests for CPC endpoint TX window handling.
 ******************************************************************************/

#include <stdlib.h>
#include <string.h>

#include <unity_fixture.h>

#if defined(SL_COMPONENT_CATALOG_PRESENT)
#include "sl_component_catalog.h"
#endif

#include "sl_status.h"

#include "../sli_cpc.h"
#include "../sli_cpc_bus.h"
#include "../sli_cpc_hdr.h"
#include "../sli_cpc_memory.h"
#include "../sli_cpc_timer.h"
#include "sl_cpc_bus_instances.h"
#include "sl_cpc_drv_mock.h"

#include "../../port/mock/src/mock_cpc_timer.h"
#include "expect.h"
#include "fixture.h"

#define TEST_PAYLOAD_SIZE 4U

TEST_GROUP(cpc_window);

TEST_SETUP(cpc_window)
{
  TEST_CPC_SETUP(true);
}

TEST_TEAR_DOWN(cpc_window)
{
  TEST_CPC_TEAR_DOWN();
}

/**
 * @brief Three queued payloads are held until the remote window opens, then
 *        sent incrementally as ACKs and window updates arrive.
 */
TEST(cpc_window, three_payloads_transmit_as_window_opens)
{
  static const uint8_t payloads[3][TEST_PAYLOAD_SIZE] = {
    {0x01, 0x02, 0x03, 0x04},
    {0x11, 0x12, 0x13, 0x14},
    {0x21, 0x22, 0x23, 0x24},
  };

  const uint8_t first_payload_seq = 1U;
  sl_status_t status;
  sl_cpc_ep_t ep;

  status = sl_cpc_ep_init(&ep, CPC_TEST_EP_ID, CPC_TEST_RX_SIZE, cpc_test_event_cb, NULL);
  TEST_ASSERT_EQUAL(SL_STATUS_OK, status);

  status = cpc_test_set_ep_connected(&ep, g_bus, first_payload_seq, CPC_TEST_PEER_SYN_SEQ + 1U, CPC_TEST_RX_SIZE,
                                     CPC_TEST_PEER_MTU, 0U);
  TEST_ASSERT_EQUAL(SL_STATUS_OK, status);
  TEST_ASSERT_EQUAL_UINT8(0, ep.send_wnd);

  for (size_t i = 0; i < 3; i++) {
    TEST_WRITE_PAYLOAD(&ep, payloads[i], TEST_PAYLOAD_SIZE);
  }

  cpc_test_pump();

  // The schedule window probe should be armed
  TEST_ASSERT_EQUAL(1, mock_cpc_timer_count_running());
  TEST_ASSERT_EQUAL(0, mock_cpc_drv_pending_tx_count(sl_cpc_drv_mock_default));

  TEST_ASSERT_TRUE(mock_cpc_timer_fire_next());

  // Window Probe
  TEST_ASSERT_OUTGOING({
    .flags = CPC_EXPECT_FLAG_ACK_REQ,
    .len = 0,
    .dst = CPC_TEST_EP_ID,
    .seq = (int)first_payload_seq,
    .ack = CPC_TEST_PEER_SYN_SEQ + 1U,
    .wnd = 0,
  });

  cpc_test_pump();

  TEST_ASSERT_EQUAL(1, mock_cpc_timer_count_running());
  TEST_ASSERT_EQUAL(0, mock_cpc_drv_pending_tx_count(sl_cpc_drv_mock_default));

  // Our SYN+ACK advertised a window of 0, now advertise a window of 2. Expect
  // the core to send the next two payloads that were queued just above.
  TEST_INJECT_FRAME({
    .dst = CPC_TEST_EP_ID,
    .seq = CPC_TEST_PEER_SYN_SEQ + 1U,
    .ack = first_payload_seq + 1,
    .wnd = 2,
  });

  cpc_test_pump();
  cpc_test_wait_retx_timer();

  TEST_ASSERT_EQUAL(1, mock_cpc_timer_count_running());
  TEST_ASSERT_EQUAL(2, mock_cpc_drv_pending_tx_count(sl_cpc_drv_mock_default));

  TEST_ASSERT_OUTGOING({
    .flags = CPC_EXPECT_FLAG_ACK_REQ,
    .len = TEST_PAYLOAD_SIZE,
    .dst = CPC_TEST_EP_ID,
    .seq = first_payload_seq + 1,
    .ack = CPC_TEST_PEER_SYN_SEQ + 1U,
    .wnd = 0,
    .payload = payloads[0],
  });

  TEST_ASSERT_OUTGOING({
    .flags = CPC_EXPECT_FLAG_ACK_REQ,
    .len = TEST_PAYLOAD_SIZE,
    .dst = CPC_TEST_EP_ID,
    .seq = first_payload_seq + 2U,
    .ack = CPC_TEST_PEER_SYN_SEQ + 1U,
    .wnd = 0,
    .payload = payloads[1],
  });

  // Advertise a 0 window that acks the two previous payload.
  // As the window is 0, and the window probe is not yet implemented
  // the endpoint should not have transmitted the third payload nor
  // armed the retransmit timer, as the frame hasn't been transmitted
  TEST_INJECT_FRAME({
    .dst = CPC_TEST_EP_ID,
    .seq = CPC_TEST_PEER_SYN_SEQ + 1U,
    .ack = first_payload_seq + 3U,
    .wnd = 0,
  });

  // Let the core process this ack
  cpc_test_pump();

  // The schedule window probe should be armed as the window is 0.
  TEST_ASSERT_EQUAL(1, mock_cpc_timer_count_running());
  TEST_ASSERT_EQUAL(0, mock_cpc_drv_pending_tx_count(sl_cpc_drv_mock_default));

  TEST_ASSERT_TRUE(mock_cpc_timer_fire_next());

  // Window Probe
  TEST_ASSERT_OUTGOING({
    .flags = CPC_EXPECT_FLAG_ACK_REQ,
    .len = 0,
    .dst = CPC_TEST_EP_ID,
    .seq = first_payload_seq + 3,
    .ack = CPC_TEST_PEER_SYN_SEQ + 1U,
    .wnd = 0,
  });

  cpc_test_pump();

  TEST_ASSERT_EQUAL(1, mock_cpc_timer_count_running());
  TEST_ASSERT_EQUAL(0, mock_cpc_drv_pending_tx_count(sl_cpc_drv_mock_default));

  // Reopen the window for the next payload
  TEST_INJECT_FRAME({
    .dst = CPC_TEST_EP_ID,
    .seq = CPC_TEST_PEER_SYN_SEQ + 1U,
    .ack = first_payload_seq + 4U,
    .wnd = 1,
  });

  TEST_ASSERT_OUTGOING({
    .flags = CPC_EXPECT_FLAG_ACK_REQ,
    .len = TEST_PAYLOAD_SIZE,
    .dst = CPC_TEST_EP_ID,
    .seq = first_payload_seq + 4U,
    .ack = CPC_TEST_PEER_SYN_SEQ + 1U,
    .wnd = 0,
    .payload = payloads[2],
  });

  cpc_test_pump();

  sl_cpc_ep_close(&ep);

  cpc_test_pump();

  cpc_test_ep_discard(&ep);
}

/**
 * @brief After a retransmit, a peer ACK with window zero must block newly
 *        queued payloads even though an earlier frame is still unacked.
 *
 * This test mimics a peer that takes a long time to ack, so long that a
 * retransmit timer expires and consumes another RX buffer. This results
 * in the available window actually shrinking, we must make sure that
 * the core handles that safely.
 */
TEST(cpc_window, retransmit_shrinks_available_window)
{
  static const uint8_t payloads[3][TEST_PAYLOAD_SIZE] = {
    {0x01, 0x02, 0x03, 0x04},
    {0x11, 0x12, 0x13, 0x14},
    {0x21, 0x22, 0x23, 0x24},
  };

  const uint8_t our_seq = 253U;
  const uint8_t first_payload_seq = our_seq + 1U;
  sl_status_t status;
  sl_cpc_ep_t ep;

  status = sl_cpc_ep_init(&ep, CPC_TEST_EP_ID, CPC_TEST_RX_SIZE, cpc_test_event_cb, NULL);
  TEST_ASSERT_EQUAL(SL_STATUS_OK, status);

  status = cpc_test_set_ep_connected(&ep, g_bus, first_payload_seq, CPC_TEST_PEER_SYN_SEQ + 1U, CPC_TEST_RX_SIZE,
                                     CPC_TEST_PEER_MTU, 3U);
  TEST_ASSERT_EQUAL(SL_STATUS_OK, status);
  TEST_ASSERT_EQUAL_UINT8(3, ep.send_wnd);

  // write two payloads
  TEST_WRITE_PAYLOAD(&ep, payloads[0], TEST_PAYLOAD_SIZE);
  TEST_WRITE_PAYLOAD(&ep, payloads[1], TEST_PAYLOAD_SIZE);

  cpc_test_pump();
  cpc_test_wait_retx_timer();

  TEST_ASSERT_EQUAL(1, mock_cpc_timer_count_running());

  // first payload: seq=our_seq + 1
  TEST_ASSERT_OUTGOING({
    .flags = CPC_EXPECT_FLAG_ACK_REQ,
    .len = TEST_PAYLOAD_SIZE,
    .dst = CPC_TEST_EP_ID,
    .seq = our_seq + 1U,
    .ack = CPC_TEST_PEER_SYN_SEQ + 1U,
    .wnd = 0,
    .payload = payloads[0],
  });

  // second payload: seq=our_seq + 2
  TEST_ASSERT_OUTGOING({
    .flags = CPC_EXPECT_FLAG_ACK_REQ,
    .len = TEST_PAYLOAD_SIZE,
    .dst = CPC_TEST_EP_ID,
    .seq = our_seq + 2U,
    .ack = CPC_TEST_PEER_SYN_SEQ + 1U,
    .wnd = 0,
    .payload = payloads[1],
  });

  // Let the retransmit timer fire, the first frame should be retransmitted
  TEST_ASSERT_TRUE_MESSAGE(mock_cpc_timer_fire_next(), "Expected retransmit timer to be armed");
  cpc_test_pump();

  // Retransmit the first payload: seq=our_seq + 1
  TEST_ASSERT_OUTGOING({
    .flags = CPC_EXPECT_FLAG_ACK_REQ,
    .len = TEST_PAYLOAD_SIZE,
    .dst = CPC_TEST_EP_ID,
    .seq = our_seq + 1U,
    .ack = CPC_TEST_PEER_SYN_SEQ + 1U,
    .wnd = 0,
    .payload = payloads[0],
  });

  // This is the actual core of the test. We simulate a device that has
  // received the three frames (2 original transmissions + 1 retranmission)
  // but it has only processed the first one. So we've used all peer's RX
  // buffers, that we were aware of.
  // We have a shortcut in the function that checks if there's space in the
  // window if the advertised window is 0, so to make this test actually do
  // something, let's simulate a scenario where the user pushed an additional
  // RX buffer in the endpoint.
  // Now we're at a point where it sends ack = our_seq + 2 with a window of 1.
  // That means the core should only be able to send the frame with sequence
  // `our_seq + 2`. Compare that to the first advertised window, we were
  // allowed to send up to `our_seq + 3`. Effectively, this is a window
  // shrinking. For retransmission, it's not yet clear if they should honor
  // this new window information or not, but for new frame, they should
  // obviously not be transmitted.
  TEST_INJECT_FRAME({
    .dst = CPC_TEST_EP_ID,
    .seq = CPC_TEST_PEER_SYN_SEQ + 1U,
    .ack = our_seq + 2U,
    .wnd = 1,
  });

  cpc_test_pump();

  // A wrap around is even more interesting as the available window is
  // essentially una + window - nxt, 255 + 1 - 0 here, so we make sure that
  // the core takes the wrap around into account.
  TEST_ASSERT_EQUAL(255, ep.send_una);
  TEST_ASSERT_EQUAL(0, ep.send_nxt);
  TEST_ASSERT_EQUAL_UINT8(1, ep.send_wnd);

  // write a new payload. This should not fit in the newly computed window
  // and be kept in the holding queue.
  TEST_WRITE_PAYLOAD(&ep, payloads[2], TEST_PAYLOAD_SIZE);

  // force process action to run and make sure
  // that the core hasn't send the payload
  cpc_test_pump();
  TEST_ASSERT_EQUAL(0, mock_cpc_drv_pending_tx_count(sl_cpc_drv_mock_default));

  sl_cpc_ep_close(&ep);

  cpc_test_pump();

  cpc_test_ep_discard(&ep);
}

/**
 * @brief A blocked endpoint schedules a probe but does not transmit it until
 *        the schedule timer expires.
 */
TEST(cpc_window, probe_not_sent_until_schedule_timer_fires)
{
  static const uint8_t payload[TEST_PAYLOAD_SIZE] = {0x01, 0x02, 0x03, 0x04};

  const uint8_t first_payload_seq = 1U;
  sl_cpc_ep_t ep;
  sl_status_t status;

  status = sl_cpc_ep_init(&ep, CPC_TEST_EP_ID, CPC_TEST_RX_SIZE, cpc_test_event_cb, NULL);
  TEST_ASSERT_EQUAL(SL_STATUS_OK, status);

  status = cpc_test_set_ep_connected(&ep, g_bus, first_payload_seq, CPC_TEST_PEER_SYN_SEQ + 1U, CPC_TEST_RX_SIZE,
                                     CPC_TEST_PEER_MTU, 0U);
  TEST_ASSERT_EQUAL(SL_STATUS_OK, status);

  TEST_WRITE_PAYLOAD(&ep, payload, TEST_PAYLOAD_SIZE);

  cpc_test_pump();

  TEST_ASSERT_EQUAL(1, mock_cpc_timer_count_running());
  TEST_ASSERT_EQUAL(0, mock_cpc_drv_pending_tx_count(sl_cpc_drv_mock_default));

  TEST_ASSERT_TRUE(mock_cpc_timer_fire_next());

  TEST_ASSERT_OUTGOING({
    .flags = CPC_EXPECT_FLAG_ACK_REQ,
    .len = 0,
    .dst = CPC_TEST_EP_ID,
    .seq = (int)first_payload_seq,
    .ack = CPC_TEST_PEER_SYN_SEQ + 1U,
    .wnd = 0,
  });

  sl_cpc_ep_close(&ep);

  cpc_test_pump();

  cpc_test_ep_discard(&ep);
}

static uint32_t test_next_timer_delay_ms(void)
{
  uint64_t expiration_tick;

  TEST_ASSERT_TRUE(mock_cpc_timer_next_expiration_tick(&expiration_tick));

  return sli_cpc_timer_tick_to_ms((uint32_t)(expiration_tick - mock_cpc_timer_get_tick_count()));
}

/**
 * @brief Each transmitted window probe doubles the delay before the next
 *        scheduled probe.
 */
TEST(cpc_window, probe_schedule_backoff_doubles_after_probe_sent)
{
  static const uint8_t payload[TEST_PAYLOAD_SIZE] = {0x01, 0x02, 0x03, 0x04};

  const uint8_t first_payload_seq = 1U;
  const uint8_t peer_seq = CPC_TEST_PEER_SYN_SEQ + 1U;
  sl_cpc_ep_t ep;
  sl_status_t status;

  status = sl_cpc_ep_init(&ep, CPC_TEST_EP_ID, CPC_TEST_RX_SIZE, cpc_test_event_cb, NULL);
  TEST_ASSERT_EQUAL(SL_STATUS_OK, status);

  status = cpc_test_set_ep_connected(&ep, g_bus, first_payload_seq, peer_seq, CPC_TEST_RX_SIZE, CPC_TEST_PEER_MTU, 0U);
  TEST_ASSERT_EQUAL(SL_STATUS_OK, status);

  TEST_WRITE_PAYLOAD(&ep, payload, TEST_PAYLOAD_SIZE);

  cpc_test_pump();

  TEST_ASSERT_UINT32_WITHIN(2U, SLI_CPC_INIT_WINDOW_PROBE_TIMEOUT_MS, test_next_timer_delay_ms());

  TEST_ASSERT_TRUE(mock_cpc_timer_fire_next());

  TEST_ASSERT_OUTGOING({
    .flags = CPC_EXPECT_FLAG_ACK_REQ,
    .len = 0,
    .dst = CPC_TEST_EP_ID,
    .seq = (int)first_payload_seq,
    .ack = (int)peer_seq,
    .wnd = 0,
  });

  TEST_INJECT_FRAME({
    .dst = CPC_TEST_EP_ID,
    .seq = (int)peer_seq,
    .ack = first_payload_seq + 1U,
    .wnd = 0,
  });

  cpc_test_pump();

  TEST_ASSERT_EQUAL(0, mock_cpc_drv_pending_tx_count(sl_cpc_drv_mock_default));
  TEST_ASSERT_TRUE(mock_cpc_timer_has_pending());

  TEST_ASSERT_UINT32_WITHIN(2U, SLI_CPC_INIT_WINDOW_PROBE_TIMEOUT_MS * 2U, test_next_timer_delay_ms());

  TEST_ASSERT_TRUE(mock_cpc_timer_fire_next());

  TEST_ASSERT_OUTGOING({
    .flags = CPC_EXPECT_FLAG_ACK_REQ,
    .len = 0,
    .dst = CPC_TEST_EP_ID,
    .seq = first_payload_seq + 1U,
    .ack = (int)peer_seq,
    .wnd = 0,
  });

  sl_cpc_ep_close(&ep);

  cpc_test_pump();

  cpc_test_ep_discard(&ep);
}

/**
 * @brief After SLI_CPC_WINDOW_PROBE_MAX probes are ACKed with a zero window,
 *        the endpoint enters the error state instead of sending another probe.
 */
TEST(cpc_window, max_window_probes_errors_endpoint)
{
  static const uint8_t payload[TEST_PAYLOAD_SIZE] = {0x01, 0x02, 0x03, 0x04};

  const uint8_t first_payload_seq = 1U;
  const uint8_t peer_seq = CPC_TEST_PEER_SYN_SEQ + 1U;
  uint8_t probe_seq = first_payload_seq;
  sl_cpc_ep_t ep;
  sl_status_t status;

  status = sl_cpc_ep_init(&ep, CPC_TEST_EP_ID, CPC_TEST_RX_SIZE, cpc_test_event_cb, NULL);
  TEST_ASSERT_EQUAL(SL_STATUS_OK, status);

  status = cpc_test_set_ep_connected(&ep, g_bus, first_payload_seq, peer_seq, CPC_TEST_RX_SIZE, CPC_TEST_PEER_MTU, 0U);
  TEST_ASSERT_EQUAL(SL_STATUS_OK, status);

  TEST_WRITE_PAYLOAD(&ep, payload, TEST_PAYLOAD_SIZE);

  cpc_test_pump();

  for (unsigned int i = 0; i < SLI_CPC_WINDOW_PROBE_MAX; i++) {
    TEST_ASSERT_TRUE(mock_cpc_timer_fire_next());

    TEST_ASSERT_OUTGOING({
      .flags = CPC_EXPECT_FLAG_ACK_REQ,
      .len = 0,
      .dst = CPC_TEST_EP_ID,
      .seq = (int)probe_seq,
      .ack = (int)peer_seq,
      .wnd = 0,
    });

    probe_seq++;

    TEST_INJECT_FRAME({
      .dst = CPC_TEST_EP_ID,
      .seq = (int)peer_seq,
      .ack = (int)probe_seq,
      .wnd = 0,
    });

    cpc_test_pump();
  }

  TEST_ASSERT_EQUAL_UINT8(SLI_CPC_WINDOW_PROBE_MAX, ep.wnd_probe.count);
  TEST_ASSERT_TRUE(mock_cpc_timer_has_pending());

  TEST_ASSERT_TRUE(mock_cpc_timer_fire_next());
  cpc_test_pump();

  TEST_ASSERT_EQUAL(SLI_CPC_STATE_CLOSING, ep.state);
  TEST_ASSERT_BIT_HIGH(SL_CPC_EP_EVENT_ERROR, cpc_test_event_mask);
  TEST_ASSERT_EQUAL(0, mock_cpc_drv_pending_tx_count(sl_cpc_drv_mock_default));

  sl_cpc_ep_close(&ep);

  cpc_test_pump();

  cpc_test_ep_discard(&ep);
}

TEST_GROUP_RUNNER(cpc_window)
{
  RUN_TEST_CASE(cpc_window, three_payloads_transmit_as_window_opens);
  RUN_TEST_CASE(cpc_window, retransmit_shrinks_available_window);
  RUN_TEST_CASE(cpc_window, probe_not_sent_until_schedule_timer_fires);
  RUN_TEST_CASE(cpc_window, probe_schedule_backoff_doubles_after_probe_sent);
  RUN_TEST_CASE(cpc_window, max_window_probes_errors_endpoint);
}
