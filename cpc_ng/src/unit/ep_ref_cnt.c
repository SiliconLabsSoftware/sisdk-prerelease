/***************************************************************************/ /**
 * @file
 * @brief Unit tests for CPC endpoint reference counting on close.
 ******************************************************************************/

#include <string.h>

#include <unity_fixture.h>

#if defined(SL_COMPONENT_CATALOG_PRESENT)
#include "sl_component_catalog.h"
#endif

#include "sl_status.h"

#include "../sli_cpc.h"
#include "../sli_cpc_bus.h"
#include "../sli_cpc_ep.h"
#include "../sli_cpc_frame.h"
#include "../sli_cpc_hdr.h"
#include "../sli_cpc_memory.h"
#include "sl_cpc_bus_instances.h"
#include "sl_cpc_drv_mock.h"
#include "sl_cpc_drv_mock_default_config.h"

#include "../../port/mock/src/mock_cpc_timer.h"
#include "expect.h"
#include "fixture.h"

TEST_GROUP(cpc_ep_ref_cnt);

TEST_SETUP(cpc_ep_ref_cnt)
{
  TEST_CPC_SETUP(true);
}

TEST_TEAR_DOWN(cpc_ep_ref_cnt)
{
  TEST_CPC_TEAR_DOWN();
}

/**
 * @brief listen() + close() calls on the event callback with endpoint closed event.
 */
TEST(cpc_ep_ref_cnt, listen_then_close_notifies_closed)
{
  sl_status_t status;
  sl_cpc_ep_t ep;

  status = sl_cpc_ep_init(&ep, CPC_TEST_EP_ID, CPC_TEST_RX_SIZE, cpc_test_event_cb, NULL);
  TEST_ASSERT_EQUAL(SL_STATUS_OK, status);

  status = sl_cpc_ep_listen(&ep, g_bus);
  TEST_ASSERT_EQUAL(SL_STATUS_OK, status);

  TEST_ASSERT_EQUAL(SLI_CPC_STATE_OPEN, ep.state);

  sl_cpc_ep_close(&ep);

  cpc_test_pump();

  TEST_ASSERT_BIT_HIGH(SL_CPC_EP_EVENT_CLOSED, cpc_test_event_mask);
  TEST_ASSERT_EQUAL(SLI_CPC_STATE_CLOSED, ep.state);

  sl_cpc_ep_deinit(&ep);
}

/**
 * @brief connect() + close() calls on the event callback with endpoint closed event.
 */
TEST(cpc_ep_ref_cnt, connect_then_close_notifies_closed)
{
  sl_status_t status;
  sl_cpc_ep_t ep;

  status = sl_cpc_ep_init(&ep, CPC_TEST_EP_ID, CPC_TEST_RX_SIZE, cpc_test_event_cb, NULL);
  TEST_ASSERT_EQUAL(SL_STATUS_OK, status);

  status = sl_cpc_ep_connect(&ep, g_bus);
  TEST_ASSERT_EQUAL(SL_STATUS_OK, status);
  TEST_ASSERT_EQUAL(SLI_CPC_STATE_SYN_SENT, ep.state);

  TEST_ASSERT_OUTGOING({
    .flags = CPC_EXPECT_FLAG_ACK_REQ | CPC_EXPECT_FLAG_SYN,
    .len = CPC_TEST_RX_SIZE,
    .dst = CPC_TEST_EP_ID,
    .seq = -1,
    .ack = -1,
    .wnd = -1,
  });

  sl_cpc_ep_close(&ep);

  cpc_test_pump();

  TEST_ASSERT_BIT_HIGH(SL_CPC_EP_EVENT_CLOSED, cpc_test_event_mask);
  TEST_ASSERT_EQUAL(SLI_CPC_STATE_CLOSED, ep.state);

  cpc_test_ep_discard(&ep);
}

/**
 * @brief connect() + RX reset frame goes into CLOSING state.
 *        CLOSING state + close() goes into CLOSED state and event callback is called.
 */
TEST(cpc_ep_ref_cnt, connect_then_reset_then_close_reaches_closed)
{
  sl_status_t status;
  sl_cpc_ep_t ep;

  status = sl_cpc_ep_init(&ep, CPC_TEST_EP_ID, CPC_TEST_RX_SIZE, cpc_test_event_cb, NULL);
  TEST_ASSERT_EQUAL(SL_STATUS_OK, status);

  status = sl_cpc_ep_connect(&ep, g_bus);
  TEST_ASSERT_EQUAL(SL_STATUS_OK, status);
  TEST_ASSERT_EQUAL(SLI_CPC_STATE_SYN_SENT, ep.state);

  TEST_ASSERT_OUTGOING({
    .flags = CPC_EXPECT_FLAG_ACK_REQ | CPC_EXPECT_FLAG_SYN,
    .len = CPC_TEST_RX_SIZE,
    .dst = CPC_TEST_EP_ID,
    .seq = -1,
    .ack = -1,
    .wnd = -1,
  });

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
  TEST_ASSERT_BIT_LOW(SL_CPC_EP_EVENT_CLOSED, cpc_test_event_mask);

  sl_cpc_ep_close(&ep);

  cpc_test_pump();

  TEST_ASSERT_EQUAL(SLI_CPC_STATE_CLOSED, ep.state);
  TEST_ASSERT_BIT_HIGH(SL_CPC_EP_EVENT_CLOSED, cpc_test_event_mask);

  cpc_test_ep_discard(&ep);
}

/**
 * @brief connect() but mock driver holds on the SYN frame
 *        close() goes into the CLOSING state
 *        validate that endpoint goes into CLOSED state when the mock driver
 *        finally "completes" the transfer of held SYN frame.
 */
TEST(cpc_ep_ref_cnt, connect_then_close_waits_for_driver_tx_complete)
{
  sl_status_t status;
  sl_cpc_ep_t ep;

  mock_cpc_drv_set_defer_tx_complete(sl_cpc_drv_mock_default, true);

  status = sl_cpc_ep_init(&ep, CPC_TEST_EP_ID, CPC_TEST_RX_SIZE, cpc_test_event_cb, NULL);
  TEST_ASSERT_EQUAL(SL_STATUS_OK, status);

  status = sl_cpc_ep_connect(&ep, g_bus);
  TEST_ASSERT_EQUAL(SL_STATUS_OK, status);
  TEST_ASSERT_EQUAL(SLI_CPC_STATE_SYN_SENT, ep.state);

  cpc_test_pump();

  TEST_ASSERT_EQUAL(1, mock_cpc_drv_held_tx_count(sl_cpc_drv_mock_default));

  TEST_ASSERT_OUTGOING({
    .flags = CPC_EXPECT_FLAG_ACK_REQ | CPC_EXPECT_FLAG_SYN,
    .len = CPC_TEST_RX_SIZE,
    .dst = CPC_TEST_EP_ID,
    .seq = -1,
    .ack = -1,
    .wnd = -1,
  });

  sl_cpc_ep_close(&ep);

  cpc_test_pump();

  TEST_ASSERT_EQUAL(SLI_CPC_STATE_CLOSING, ep.state);
  TEST_ASSERT_BIT_LOW(SL_CPC_EP_EVENT_CLOSED, cpc_test_event_mask);

  mock_cpc_drv_complete_held_tx(sl_cpc_drv_mock_default, -1);
  cpc_test_pump();

  TEST_ASSERT_EQUAL(SLI_CPC_STATE_CLOSED, ep.state);
  TEST_ASSERT_BIT_HIGH(SL_CPC_EP_EVENT_CLOSED, cpc_test_event_mask);

  cpc_test_ep_discard(&ep);
}

/**
 * Close must run from the recv callback to keep the test deterministic.
 *
 * In baremetal: With sli_cpc_bus_process_action(), PEND_ACK is set and RECV is delivered
 * synchronously, followed by process_pending_flags() which sends out the pending ACK.
 * Therefore calling close from the test body before OR after forcing a process action pass
 * provides no guarantee that the ACK is sent out during ep teardown.
 *
 * In RTOS: the same interleaving is timing-sensitive instead. Another context would have to
 * call close in that window (between RECV and process_pending_flags()) in order to catch
 * the ACK being sent out during ep teardown.
 */
static void close_on_recv_cb(sl_cpc_ep_t *ep, sl_cpc_ep_event_type_t type, const sl_cpc_ep_event_t *event, void *arg)
{
  (void)arg;

  cpc_test_event_mask |= 1 << type;

  if (type == SL_CPC_EP_EVENT_RECV) {
    sl_cpc_buf_free(event->recv.buf);
    sl_cpc_ep_close(ep);
  }
}

/**
 * @brief Closing an endpoint must send out any pending ACKs.
 *
 * Sequence:
 *   1. Peer sends a reliable data frame.
 *   2. Core advances ep->ack, sets PEND_ACK, then delivers RECV synchronously.
 *   3. The RECV callback closes the endpoint.
 *   4. The peer must still observe a standalone ACK for that frame.
 */
TEST(cpc_ep_ref_cnt, close_on_recv_still_sends_ack)
{
  const uint8_t peer_seq = CPC_TEST_PEER_SYN_SEQ + 1U;
  const uint8_t payload[] = {0xA1, 0xB2, 0xC3, 0xD4};
  sl_cpc_buf_t *rx_buf;
  sl_status_t status;
  sl_cpc_ep_t ep;

  status = sl_cpc_ep_init(&ep, CPC_TEST_EP_ID, CPC_TEST_RX_SIZE, close_on_recv_cb, NULL);
  TEST_ASSERT_EQUAL(SL_STATUS_OK, status);

  rx_buf = sl_cpc_buf_alloc(CPC_TEST_RX_SIZE);
  TEST_ASSERT_NOT_NULL(rx_buf);
  status = sl_cpc_ep_push_recv_buf(&ep, rx_buf);
  TEST_ASSERT_EQUAL(SL_STATUS_OK, status);

  status
    = cpc_test_set_ep_connected(&ep, g_bus, 1U, peer_seq, CPC_TEST_RX_SIZE, CPC_TEST_PEER_MTU, CPC_TEST_PEER_RX_WND);
  TEST_ASSERT_EQUAL(SL_STATUS_OK, status);

  TEST_INJECT_FRAME({
    .flags = CPC_EXPECT_FLAG_ACK_REQ,
    .len = (ssize_t)sizeof(payload),
    .dst = CPC_TEST_EP_ID,
    .seq = (int)peer_seq,
    .ack = 0,
    .wnd = CPC_TEST_PEER_RX_WND,
    .payload = payload,
  });

  TEST_ASSERT_OUTGOING({
    .flags = 0,
    .len = 0,
    .dst = CPC_TEST_EP_ID,
    .seq = 0,
    .ack = (int)(peer_seq + 1U),
    .wnd = 0,
  });

  TEST_ASSERT_BIT_HIGH(SL_CPC_EP_EVENT_RECV, cpc_test_event_mask);

  cpc_test_pump();
  cpc_test_ep_discard(&ep);
}

/**
 * @brief sli_cpc_ep_send_ack defers via PEND_ACK when the TX frame pool is empty.
 *
 * Sequence:
 *   1. Exhaust the TX frame pool.
 *   2. send_ack fails and arms PEND_ACK.
 *   3. Free one frame and signal the bus.
 *   4. TEST_ASSERT_OUTGOING waits until the deferred ACK is transmitted.
 *   5. PEND_ACK is cleared once the ACK has been sent.
 */
TEST(cpc_ep_ref_cnt, send_ack_defers_when_tx_pool_exhausted)
{
  sl_cpc_frame_t *frames[SL_CPC_DRV_MOCK_DEFAULT_TX_FRAME_POOL_COUNT];
  const uint8_t peer_seq = CPC_TEST_PEER_SYN_SEQ + 1U;
  size_t frame_count = 0;
  sl_cpc_buf_t *rx_buf;
  sl_status_t status;
  sl_cpc_ep_t ep;
  size_t i;

  status = sl_cpc_ep_init(&ep, CPC_TEST_EP_ID, CPC_TEST_RX_SIZE, cpc_test_event_cb, NULL);
  TEST_ASSERT_EQUAL(SL_STATUS_OK, status);

  rx_buf = sl_cpc_buf_alloc(CPC_TEST_RX_SIZE);
  TEST_ASSERT_NOT_NULL(rx_buf);
  status = sl_cpc_ep_push_recv_buf(&ep, rx_buf);
  TEST_ASSERT_EQUAL(SL_STATUS_OK, status);

  status
    = cpc_test_set_ep_connected(&ep, g_bus, 1U, peer_seq, CPC_TEST_RX_SIZE, CPC_TEST_PEER_MTU, CPC_TEST_PEER_RX_WND);
  TEST_ASSERT_EQUAL(SL_STATUS_OK, status);

  ep.ack = peer_seq;

  for (i = 0; i < SL_CPC_DRV_MOCK_DEFAULT_TX_FRAME_POOL_COUNT; i++) {
    frames[i] = sli_cpc_frame_new(g_bus, false);
    TEST_ASSERT_NOT_NULL(frames[i]);
    frame_count++;
  }
  TEST_ASSERT_NULL(sli_cpc_frame_new(g_bus, false));

  sli_cpc_ep_send_ack(&ep);
  TEST_ASSERT_TRUE((ep.flags & SLI_CPC_EP_FLAG_PEND_ACK) != 0);

  frame_count--;
  sli_cpc_frame_free(frames[frame_count]);
  frames[frame_count] = NULL;

  sli_cpc_bus_signal_event(g_bus, SLI_CPC_SIGNAL_SYSTEM);

  TEST_ASSERT_OUTGOING({
    .flags = 0,
    .len = 0,
    .dst = CPC_TEST_EP_ID,
    .seq = 0,
    .ack = (int)peer_seq,
    .wnd = 1,
  });

  TEST_ASSERT_TRUE((ep.flags & SLI_CPC_EP_FLAG_PEND_ACK) == 0);

  for (i = 0; i < frame_count; i++) {
    sli_cpc_frame_free(frames[i]);
  }

  sl_cpc_ep_close(&ep);
  cpc_test_pump();
  cpc_test_ep_discard(&ep);
}

TEST_GROUP_RUNNER(cpc_ep_ref_cnt)
{
  RUN_TEST_CASE(cpc_ep_ref_cnt, listen_then_close_notifies_closed);
  RUN_TEST_CASE(cpc_ep_ref_cnt, connect_then_close_notifies_closed);
  RUN_TEST_CASE(cpc_ep_ref_cnt, connect_then_reset_then_close_reaches_closed);
  RUN_TEST_CASE(cpc_ep_ref_cnt, connect_then_close_waits_for_driver_tx_complete);
  RUN_TEST_CASE(cpc_ep_ref_cnt, close_on_recv_still_sends_ack);
  RUN_TEST_CASE(cpc_ep_ref_cnt, send_ack_defers_when_tx_pool_exhausted);
}
