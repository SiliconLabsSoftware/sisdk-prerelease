/***************************************************************************/ /**
 * @file
 * @brief Unit tests for the CPC endpoint public API.
 ******************************************************************************/

#include <stdint.h>

#include <unity_fixture.h>

#if defined(SL_COMPONENT_CATALOG_PRESENT)
#include "sl_component_catalog.h"
#endif

#include "../../port/mock/src/mock_cpc_timer.h"

#include "fixture.h"

#include "sl_cpc_bus_instances.h"
#include "sl_cpc_drv_mock.h"
#include "sl_status.h"

#include "../sli_cpc.h"

TEST_GROUP(cpc_api);

TEST_SETUP(cpc_api)
{
  TEST_CPC_SETUP(true);
}

TEST_TEAR_DOWN(cpc_api)
{
  TEST_CPC_TEAR_DOWN();
}

static void api_event_cb(sl_cpc_ep_t *ep, sl_cpc_ep_event_type_t type, const sl_cpc_ep_event_t *event, void *arg)
{
  (void)ep;
  (void)type;
  (void)event;
  (void)arg;
}

TEST(cpc_api, init_and_listen_registers_ep_on_bus)
{
  sl_status_t status;
  sl_cpc_ep_t ep;

  status = sl_cpc_ep_init(&ep, CPC_TEST_EP_ID, CPC_TEST_RX_SIZE, api_event_cb, NULL);
  TEST_ASSERT_EQUAL(SL_STATUS_OK, status);
  TEST_ASSERT_NULL(ep.bus);
  TEST_ASSERT_EQUAL(SLI_CPC_STATE_CLOSED, ep.state);
  TEST_ASSERT_EQUAL(CPC_TEST_EP_ID, ep.id);
  TEST_ASSERT_EQUAL(CPC_TEST_RX_SIZE, ep.mtu);
  TEST_ASSERT_NULL(sli_cpc_bus_find_ep_from_id(g_bus, CPC_TEST_EP_ID));

  status = sl_cpc_ep_listen(&ep, g_bus);
  TEST_ASSERT_EQUAL(SL_STATUS_OK, status);
  TEST_ASSERT_EQUAL_PTR(g_bus, ep.bus);
  TEST_ASSERT_EQUAL(SLI_CPC_STATE_OPEN, ep.state);
  TEST_ASSERT_EQUAL_PTR(&ep, sli_cpc_bus_find_ep_from_id(g_bus, CPC_TEST_EP_ID));

  sl_cpc_ep_close(&ep);
  cpc_test_pump();

  cpc_test_ep_discard(&ep);
}

TEST(cpc_api, listen_rejects_duplicate_ep_id)
{
  sl_cpc_ep_t ep_second;
  sl_cpc_ep_t ep_first;
  sl_status_t status;

  status = sl_cpc_ep_init(&ep_first, CPC_TEST_EP_ID, CPC_TEST_RX_SIZE, api_event_cb, NULL);
  TEST_ASSERT_EQUAL(SL_STATUS_OK, status);

  status = sl_cpc_ep_listen(&ep_first, g_bus);
  TEST_ASSERT_EQUAL(SL_STATUS_OK, status);

  status = sl_cpc_ep_init(&ep_second, CPC_TEST_EP_ID, CPC_TEST_RX_SIZE, api_event_cb, NULL);
  TEST_ASSERT_EQUAL(SL_STATUS_OK, status);

  status = sl_cpc_ep_listen(&ep_second, g_bus);
  TEST_ASSERT_EQUAL(SL_STATUS_ALREADY_INITIALIZED, status);
  TEST_ASSERT_NULL(ep_second.bus);
  TEST_ASSERT_EQUAL_PTR(&ep_first, sli_cpc_bus_find_ep_from_id(g_bus, CPC_TEST_EP_ID));

  sl_cpc_ep_close(&ep_first);
  cpc_test_pump();

  cpc_test_ep_discard(&ep_second);
  cpc_test_ep_discard(&ep_first);
}

TEST(cpc_api, listen_close_then_listen_reattaches_same_ep)
{
  sl_status_t status;
  sl_cpc_ep_t ep;

  status = sl_cpc_ep_init(&ep, CPC_TEST_EP_ID, CPC_TEST_RX_SIZE, api_event_cb, NULL);
  TEST_ASSERT_EQUAL(SL_STATUS_OK, status);

  status = sl_cpc_ep_listen(&ep, g_bus);
  TEST_ASSERT_EQUAL(SL_STATUS_OK, status);
  TEST_ASSERT_EQUAL(SLI_CPC_STATE_OPEN, ep.state);
  TEST_ASSERT_EQUAL_PTR(&ep, sli_cpc_bus_find_ep_from_id(g_bus, CPC_TEST_EP_ID));

  sl_cpc_ep_close(&ep);
  cpc_test_pump();

  TEST_ASSERT_EQUAL(SLI_CPC_STATE_CLOSED, ep.state);

  status = sl_cpc_ep_listen(&ep, g_bus);
  TEST_ASSERT_EQUAL(SL_STATUS_OK, status);
  TEST_ASSERT_EQUAL(SLI_CPC_STATE_OPEN, ep.state);
  TEST_ASSERT_EQUAL_PTR(g_bus, ep.bus);
  TEST_ASSERT_EQUAL_PTR(&ep, sli_cpc_bus_find_ep_from_id(g_bus, CPC_TEST_EP_ID));

  sl_cpc_ep_close(&ep);
  cpc_test_pump();

  cpc_test_ep_discard(&ep);
}

TEST(cpc_api, listen_and_connect_defer_until_notify_initialized)
{
  sl_cpc_ep_t connect_ep;
  sl_cpc_ep_t listen_ep;
  sl_status_t status;

  g_bus->initialized = false;

  status = sl_cpc_ep_init(&listen_ep, CPC_TEST_EP_ID, CPC_TEST_RX_SIZE, api_event_cb, NULL);
  TEST_ASSERT_EQUAL(SL_STATUS_OK, status);

  status = sl_cpc_ep_listen(&listen_ep, g_bus);
  TEST_ASSERT_EQUAL(SL_STATUS_OK, status);
  TEST_ASSERT_EQUAL(SLI_CPC_STATE_CLOSED, listen_ep.state);
  TEST_ASSERT_EQUAL_PTR(&listen_ep, sli_cpc_bus_find_ep_from_id(g_bus, CPC_TEST_EP_ID));

  status = sl_cpc_ep_init(&connect_ep, CPC_TEST_EP_ID + 1U, CPC_TEST_RX_SIZE, api_event_cb, NULL);
  TEST_ASSERT_EQUAL(SL_STATUS_OK, status);

  status = sl_cpc_ep_connect(&connect_ep, g_bus);
  TEST_ASSERT_EQUAL(SL_STATUS_OK, status);
  TEST_ASSERT_EQUAL(SLI_CPC_STATE_CLOSED, connect_ep.state);
  TEST_ASSERT_EQUAL_PTR(&connect_ep, sli_cpc_bus_find_ep_from_id(g_bus, CPC_TEST_EP_ID + 1U));

  sli_cpc_bus_notify_initialized(g_bus);

  TEST_ASSERT_TRUE(g_bus->initialized);
  TEST_ASSERT_EQUAL(SLI_CPC_STATE_OPEN, listen_ep.state);
  TEST_ASSERT_EQUAL(SLI_CPC_STATE_SYN_SENT, connect_ep.state);

  sl_cpc_ep_close(&listen_ep);
  sl_cpc_ep_close(&connect_ep);
  cpc_test_pump();

  cpc_test_ep_discard(&listen_ep);
  cpc_test_ep_discard(&connect_ep);
}

TEST(cpc_api, init_rejects_rx_size_above_max_payload)
{
  sl_status_t status;
  sl_cpc_ep_t ep;

  status = sl_cpc_ep_init(&ep, CPC_TEST_EP_ID, SL_CPC_EP_MAX_PAYLOAD_SIZE + 1U, api_event_cb, NULL);
  TEST_ASSERT_EQUAL(SL_STATUS_INVALID_PARAMETER, status);
}

TEST(cpc_api, init_rejects_unaligned_rx_size)
{
  const size_t unaligned_rx_size = CPC_TEST_RX_SIZE + 1U;
  sl_status_t status;
  sl_cpc_ep_t ep;

  TEST_ASSERT_NOT_EQUAL(0, unaligned_rx_size % SL_CPC_BUF_MIN_ALIGNMENT);

  status = sl_cpc_ep_init(&ep, CPC_TEST_EP_ID, unaligned_rx_size, api_event_cb, NULL);
  TEST_ASSERT_EQUAL(SL_STATUS_INVALID_PARAMETER, status);
}

TEST(cpc_api, init_then_close_rejects_unopened_ep)
{
  sl_status_t status;
  sl_cpc_ep_t ep;

  status = sl_cpc_ep_init(&ep, CPC_TEST_EP_ID, CPC_TEST_RX_SIZE, api_event_cb, NULL);
  TEST_ASSERT_EQUAL(SL_STATUS_OK, status);

  sl_cpc_ep_close(&ep);

  cpc_test_ep_discard(&ep);
}

TEST(cpc_api, init_rejects_missing_event_callback)
{
  sl_status_t status;
  sl_cpc_ep_t ep;

  status = sl_cpc_ep_init(&ep, CPC_TEST_EP_ID, CPC_TEST_RX_SIZE, NULL, NULL);
  TEST_ASSERT_EQUAL(SL_STATUS_NULL_POINTER, status);
}

TEST(cpc_api, push_recv_buf_rejects_NULL_buffer)
{
  sl_status_t status;
  sl_cpc_ep_t ep;

  status = sl_cpc_ep_init(&ep, CPC_TEST_EP_ID, 32, api_event_cb, NULL);
  TEST_ASSERT_EQUAL(SL_STATUS_OK, status);

  status = sl_cpc_ep_push_recv_buf(&ep, NULL);
  TEST_ASSERT_EQUAL(SL_STATUS_NULL_POINTER, status);
  TEST_ASSERT_TRUE(sl_cpc_msgq_is_empty(&ep.rx_buffer_queue));

  cpc_test_ep_discard(&ep);
}

TEST(cpc_api, push_recv_buf_rejects_NULL_data_ptr)
{
  sl_status_t status;
  sl_cpc_buf_t buf;
  sl_cpc_ep_t ep;

  status = sl_cpc_ep_init(&ep, CPC_TEST_EP_ID, 32, api_event_cb, NULL);
  TEST_ASSERT_EQUAL(SL_STATUS_OK, status);

  sl_cpc_buf_init(&buf, NULL, 0);

  status = sl_cpc_ep_push_recv_buf(&ep, &buf);
  TEST_ASSERT_EQUAL(SL_STATUS_NULL_POINTER, status);
  TEST_ASSERT_TRUE(sl_cpc_msgq_is_empty(&ep.rx_buffer_queue));

  cpc_test_ep_discard(&ep);
}

TEST(cpc_api, push_recv_buf_rejects_unaligned_data_ptr)
{
  sl_status_t status;
  sl_cpc_buf_t buf;
  sl_cpc_ep_t ep;
  unsigned int i;

  status = sl_cpc_ep_init(&ep, CPC_TEST_EP_ID, 32, api_event_cb, NULL);
  TEST_ASSERT_EQUAL(SL_STATUS_OK, status);

  for (i = 1; i < SL_CPC_BUF_MIN_ALIGNMENT; i++) {
    TEST_ASSERT_NOT_EQUAL(0, i % SL_CPC_BUF_MIN_ALIGNMENT);

    sl_cpc_buf_init(&buf, (void *)i, 0);

    status = sl_cpc_ep_push_recv_buf(&ep, &buf);
    TEST_ASSERT_EQUAL(SL_STATUS_INVALID_PARAMETER, status);
    TEST_ASSERT_TRUE(sl_cpc_msgq_is_empty(&ep.rx_buffer_queue));
  }

  TEST_ASSERT_EQUAL(0, i % SL_CPC_BUF_MIN_ALIGNMENT);

  sl_cpc_buf_init(&buf, (void *)i, 0);

  status = sl_cpc_ep_push_recv_buf(&ep, &buf);
  TEST_ASSERT_EQUAL(SL_STATUS_OK, status);
  TEST_ASSERT_EQUAL(1, sl_cpc_msgq_len(&ep.rx_buffer_queue));

  cpc_test_ep_discard(&ep);
}

TEST(cpc_api, push_recv_buf_rejects_unaligned_ptr_in_chain)
{
  sl_cpc_ep_t ep;
  sl_cpc_buf_t head;
  sl_cpc_buf_t middle;
  sl_cpc_buf_t tail;
  sl_status_t status;

  status = sl_cpc_ep_init(&ep, CPC_TEST_EP_ID, 32, api_event_cb, NULL);
  TEST_ASSERT_EQUAL(SL_STATUS_OK, status);

  sl_cpc_buf_init(&head, (void *)0xDEADBEE0, 4);
  sl_cpc_buf_init(&middle, (void *)0xDEADBEE1, 4);
  sl_cpc_buf_init(&tail, (void *)0xDEADBEE0, 4);
  TEST_ASSERT_EQUAL(0, (uintptr_t)head.ptr % SL_CPC_BUF_MIN_ALIGNMENT);
  TEST_ASSERT_NOT_EQUAL(0, (uintptr_t)middle.ptr % SL_CPC_BUF_MIN_ALIGNMENT);
  TEST_ASSERT_EQUAL(0, (uintptr_t)tail.ptr % SL_CPC_BUF_MIN_ALIGNMENT);

  sl_cpc_buf_chain(&head, &middle);
  sl_cpc_buf_chain(&head, &tail);

  status = sl_cpc_ep_push_recv_buf(&ep, &head);
  TEST_ASSERT_EQUAL(SL_STATUS_INVALID_PARAMETER, status);
  TEST_ASSERT_TRUE(sl_cpc_msgq_is_empty(&ep.rx_buffer_queue));

  cpc_test_ep_discard(&ep);
}

TEST(cpc_api, push_recv_buf_accepts_aligned_chain)
{
  sl_cpc_ep_t ep;
  sl_cpc_buf_t head;
  sl_cpc_buf_t middle;
  sl_cpc_buf_t tail;
  sl_status_t status;

  status = sl_cpc_ep_init(&ep, CPC_TEST_EP_ID, 32, api_event_cb, NULL);
  TEST_ASSERT_EQUAL(SL_STATUS_OK, status);

  sl_cpc_buf_init(&head, (void *)0xDEADBEE0, 4);
  sl_cpc_buf_init(&middle, (void *)0xDEADBEF0, 4);
  sl_cpc_buf_init(&tail, (void *)0xDEADBF00, 4);
  TEST_ASSERT_EQUAL(0, (uintptr_t)head.ptr % SL_CPC_BUF_MIN_ALIGNMENT);
  TEST_ASSERT_EQUAL(0, (uintptr_t)middle.ptr % SL_CPC_BUF_MIN_ALIGNMENT);
  TEST_ASSERT_EQUAL(0, (uintptr_t)tail.ptr % SL_CPC_BUF_MIN_ALIGNMENT);

  sl_cpc_buf_chain(&head, &middle);
  sl_cpc_buf_chain(&head, &tail);

  status = sl_cpc_ep_push_recv_buf(&ep, &head);
  TEST_ASSERT_EQUAL(SL_STATUS_OK, status);
  TEST_ASSERT_EQUAL(3, sl_cpc_msgq_len(&ep.rx_buffer_queue));

  cpc_test_ep_discard(&ep);
}

TEST(cpc_api, push_recv_buf_rejects_valid_buf_when_rx_size_configured_to_0)
{
  sl_status_t status;
  sl_cpc_buf_t buf;
  sl_cpc_ep_t ep;

  status = sl_cpc_ep_init(&ep, CPC_TEST_EP_ID, 0, api_event_cb, NULL);
  TEST_ASSERT_EQUAL(SL_STATUS_OK, status);
  TEST_ASSERT_EQUAL(0, ep.mtu);

  sl_cpc_buf_init(&buf, (void *)0xDEADBEE0, 32);

  status = sl_cpc_ep_push_recv_buf(&ep, &buf);
  TEST_ASSERT_EQUAL(SL_STATUS_NOT_AVAILABLE, status);
  TEST_ASSERT_TRUE(sl_cpc_msgq_is_empty(&ep.rx_buffer_queue));

  cpc_test_ep_discard(&ep);
}

TEST(cpc_api, pop_recv_buf_rejects_NULL_out_buf)
{
  sl_cpc_ep_t ep;
  sl_status_t status;

  status = sl_cpc_ep_init(&ep, CPC_TEST_EP_ID, 32, api_event_cb, NULL);
  TEST_ASSERT_EQUAL(SL_STATUS_OK, status);
  TEST_ASSERT_EQUAL(SLI_CPC_STATE_CLOSED, ep.state);

  status = sl_cpc_ep_pop_recv_buf(&ep, NULL);
  TEST_ASSERT_EQUAL(SL_STATUS_NULL_POINTER, status);
}

TEST(cpc_api, pop_recv_buf_rejects_when_endpoint_not_closed_allowed_when_closed)
{
  sl_cpc_ep_t ep;
  sl_cpc_buf_t buf;
  sl_cpc_buf_t *out_buf = NULL;
  sl_status_t status;

  status = sl_cpc_ep_init(&ep, CPC_TEST_EP_ID, 32, api_event_cb, NULL);
  TEST_ASSERT_EQUAL(SL_STATUS_OK, status);

  sl_cpc_buf_init(&buf, (void *)0xDEADBEE0, 32);
  status = sl_cpc_ep_push_recv_buf(&ep, &buf);
  TEST_ASSERT_EQUAL(SL_STATUS_OK, status);

  status = sl_cpc_ep_listen(&ep, g_bus);
  TEST_ASSERT_EQUAL(SL_STATUS_OK, status);
  TEST_ASSERT_NOT_EQUAL(SLI_CPC_STATE_CLOSED, ep.state);

  status = sl_cpc_ep_pop_recv_buf(&ep, &out_buf);
  TEST_ASSERT_EQUAL(SL_STATUS_INVALID_STATE, status);
  TEST_ASSERT_NULL(out_buf);
  TEST_ASSERT_EQUAL(1, sl_cpc_msgq_len(&ep.rx_buffer_queue));

  sl_cpc_ep_close(&ep);

  cpc_test_pump();

  status = sl_cpc_ep_pop_recv_buf(&ep, &out_buf);
  TEST_ASSERT_EQUAL(SL_STATUS_OK, status);
  TEST_ASSERT_EQUAL_PTR(&buf, out_buf);
  TEST_ASSERT_EQUAL(0, sl_cpc_msgq_len(&ep.rx_buffer_queue));

  cpc_test_ep_discard(&ep);
}

TEST(cpc_api, pop_recv_buf_rejects_when_queue_empty)
{
  sl_cpc_ep_t ep;
  sl_cpc_buf_t *out_buf = (sl_cpc_buf_t *)0x1;
  sl_status_t status;

  status = sl_cpc_ep_init(&ep, CPC_TEST_EP_ID, 32, api_event_cb, NULL);
  TEST_ASSERT_EQUAL(SL_STATUS_OK, status);
  TEST_ASSERT_EQUAL(SLI_CPC_STATE_CLOSED, ep.state);
  TEST_ASSERT_TRUE(sl_cpc_msgq_is_empty(&ep.rx_buffer_queue));

  status = sl_cpc_ep_pop_recv_buf(&ep, &out_buf);
  TEST_ASSERT_EQUAL(SL_STATUS_NO_MORE_RESOURCE, status);
}

TEST(cpc_api, push_pop_recv_buf_pops_buffer_when_never_connected)
{
  sl_cpc_ep_t ep;
  sl_cpc_buf_t buf;
  sl_cpc_buf_t *out_buf = NULL;
  sl_status_t status;

  status = sl_cpc_ep_init(&ep, CPC_TEST_EP_ID, 32, api_event_cb, NULL);
  TEST_ASSERT_EQUAL(SL_STATUS_OK, status);
  TEST_ASSERT_EQUAL(SLI_CPC_STATE_CLOSED, ep.state);

  sl_cpc_buf_init(&buf, (void *)0xDEADBEE0, 32);
  status = sl_cpc_ep_push_recv_buf(&ep, &buf);
  TEST_ASSERT_EQUAL(SL_STATUS_OK, status);
  TEST_ASSERT_EQUAL(1, sl_cpc_msgq_len(&ep.rx_buffer_queue));

  status = sl_cpc_ep_pop_recv_buf(&ep, &out_buf);
  TEST_ASSERT_EQUAL(SL_STATUS_OK, status);
  TEST_ASSERT_EQUAL_PTR(&buf, out_buf);
  TEST_ASSERT_TRUE(sl_cpc_msgq_is_empty(&ep.rx_buffer_queue));

  cpc_test_ep_discard(&ep);
}

TEST(cpc_api, write_rejects_null_frame)
{
  uint8_t payload[4];
  sl_status_t status;
  sl_cpc_buf_t buf;
  sl_cpc_ep_t ep;

  status = sl_cpc_ep_init(&ep, CPC_TEST_EP_ID, CPC_TEST_RX_SIZE, api_event_cb, NULL);
  TEST_ASSERT_EQUAL(SL_STATUS_OK, status);

  status = cpc_test_set_ep_connected(&ep, g_bus, 1U, CPC_TEST_PEER_SYN_SEQ + 1U, CPC_TEST_RX_SIZE, CPC_TEST_PEER_MTU,
                                     CPC_TEST_PEER_RX_WND);
  TEST_ASSERT_EQUAL(SL_STATUS_OK, status);

  sl_cpc_buf_init(&buf, payload, sizeof(payload));

  status = sl_cpc_ep_send(&ep, &buf, NULL, NULL);
  TEST_ASSERT_EQUAL(SL_STATUS_NULL_POINTER, status);

  sl_cpc_ep_close(&ep);

  cpc_test_pump();

  cpc_test_ep_discard(&ep);
}

TEST(cpc_api, write_rejects_null_buffer)
{
  sl_cpc_frame_t frame;
  sl_status_t status;
  sl_cpc_ep_t ep;

  status = sl_cpc_ep_init(&ep, CPC_TEST_EP_ID, CPC_TEST_RX_SIZE, api_event_cb, NULL);
  TEST_ASSERT_EQUAL(SL_STATUS_OK, status);

  status = cpc_test_set_ep_connected(&ep, g_bus, 1U, CPC_TEST_PEER_SYN_SEQ + 1U, CPC_TEST_RX_SIZE, CPC_TEST_PEER_MTU,
                                     CPC_TEST_PEER_RX_WND);
  TEST_ASSERT_EQUAL(SL_STATUS_OK, status);

  status = sl_cpc_ep_send(&ep, NULL, &frame, NULL);
  TEST_ASSERT_EQUAL(SL_STATUS_NULL_POINTER, status);

  sl_cpc_ep_close(&ep);

  cpc_test_pump();

  cpc_test_ep_discard(&ep);
}

TEST(cpc_api, write_rejects_null_data_ptr)
{
  sl_cpc_frame_t frame;
  sl_status_t status;
  sl_cpc_buf_t buf;
  sl_cpc_ep_t ep;

  status = sl_cpc_ep_init(&ep, CPC_TEST_EP_ID, CPC_TEST_RX_SIZE, api_event_cb, NULL);
  TEST_ASSERT_EQUAL(SL_STATUS_OK, status);

  status = cpc_test_set_ep_connected(&ep, g_bus, 1U, CPC_TEST_PEER_SYN_SEQ + 1U, CPC_TEST_RX_SIZE, CPC_TEST_PEER_MTU,
                                     CPC_TEST_PEER_RX_WND);
  TEST_ASSERT_EQUAL(SL_STATUS_OK, status);

  sl_cpc_buf_init(&buf, NULL, 4);

  status = sl_cpc_ep_send(&ep, &buf, &frame, NULL);
  TEST_ASSERT_EQUAL(SL_STATUS_NULL_POINTER, status);

  sl_cpc_ep_close(&ep);

  cpc_test_pump();

  cpc_test_ep_discard(&ep);
}

TEST(cpc_api, write_rejects_zero_tot_len)
{
  sl_cpc_frame_t frame;
  uint8_t payload[4];
  sl_status_t status;
  sl_cpc_buf_t buf;
  sl_cpc_ep_t ep;

  status = sl_cpc_ep_init(&ep, CPC_TEST_EP_ID, CPC_TEST_RX_SIZE, api_event_cb, NULL);
  TEST_ASSERT_EQUAL(SL_STATUS_OK, status);

  status = cpc_test_set_ep_connected(&ep, g_bus, 1U, CPC_TEST_PEER_SYN_SEQ + 1U, CPC_TEST_RX_SIZE, CPC_TEST_PEER_MTU,
                                     CPC_TEST_PEER_RX_WND);
  TEST_ASSERT_EQUAL(SL_STATUS_OK, status);

  sl_cpc_buf_init(&buf, payload, 0);

  status = sl_cpc_ep_send(&ep, &buf, &frame, NULL);
  TEST_ASSERT_EQUAL(SL_STATUS_INVALID_PARAMETER, status);

  sl_cpc_ep_close(&ep);

  cpc_test_pump();

  cpc_test_ep_discard(&ep);
}

TEST(cpc_api, write_rejects_unaligned_ptr_in_chain)
{
  sl_cpc_ep_t ep;
  sl_cpc_frame_t frame;
  sl_cpc_buf_t head;
  sl_cpc_buf_t middle;
  sl_cpc_buf_t tail;
  sl_status_t status;

  status = sl_cpc_ep_init(&ep, CPC_TEST_EP_ID, CPC_TEST_RX_SIZE, api_event_cb, NULL);
  TEST_ASSERT_EQUAL(SL_STATUS_OK, status);

  status = cpc_test_set_ep_connected(&ep, g_bus, 1U, CPC_TEST_PEER_SYN_SEQ + 1U, CPC_TEST_RX_SIZE, CPC_TEST_PEER_MTU,
                                     CPC_TEST_PEER_RX_WND);
  TEST_ASSERT_EQUAL(SL_STATUS_OK, status);

  sl_cpc_buf_init(&head, (void *)0xDEADBEE0, 4);
  sl_cpc_buf_init(&middle, (void *)0xDEADBEE1, 4);
  sl_cpc_buf_init(&tail, (void *)0xDEADBEE0, 4);
  TEST_ASSERT_EQUAL(0, (uintptr_t)head.ptr % SL_CPC_BUF_MIN_ALIGNMENT);
  TEST_ASSERT_NOT_EQUAL(0, (uintptr_t)middle.ptr % SL_CPC_BUF_MIN_ALIGNMENT);
  TEST_ASSERT_EQUAL(0, (uintptr_t)tail.ptr % SL_CPC_BUF_MIN_ALIGNMENT);

  sl_cpc_buf_chain(&head, &middle);
  sl_cpc_buf_chain(&head, &tail);

  status = sl_cpc_ep_send(&ep, &head, &frame, NULL);
  TEST_ASSERT_EQUAL(SL_STATUS_INVALID_PARAMETER, status);

  sl_cpc_ep_close(&ep);

  cpc_test_pump();

  cpc_test_ep_discard(&ep);
}

TEST(cpc_api, write_rejects_chain_with_tail_still_linked)
{
  sl_cpc_ep_t ep;
  sl_cpc_frame_t frame;
  sl_cpc_buf_t head;
  sl_cpc_buf_t extra;
  sl_status_t status;

  status = sl_cpc_ep_init(&ep, CPC_TEST_EP_ID, CPC_TEST_RX_SIZE, api_event_cb, NULL);
  TEST_ASSERT_EQUAL(SL_STATUS_OK, status);

  status = cpc_test_set_ep_connected(&ep, g_bus, 1U, CPC_TEST_PEER_SYN_SEQ + 1U, CPC_TEST_RX_SIZE, CPC_TEST_PEER_MTU,
                                     CPC_TEST_PEER_RX_WND);
  TEST_ASSERT_EQUAL(SL_STATUS_OK, status);

  sl_cpc_buf_init(&head, (void *)0xDEADBEE0, 4);
  sl_cpc_buf_init(&extra, (void *)0xDEADBEF0, 4);
  TEST_ASSERT_EQUAL(head.len, head.tot_len);

  /* Claims to be the last segment, but still points at another buffer. */
  head.node.node = &extra.node;

  status = sl_cpc_ep_send(&ep, &head, &frame, NULL);
  TEST_ASSERT_EQUAL(SL_STATUS_INVALID_PARAMETER, status);

  sl_cpc_ep_close(&ep);

  cpc_test_pump();

  cpc_test_ep_discard(&ep);
}

TEST(cpc_api, write_rejects_chain_missing_next_segment)
{
  sl_cpc_ep_t ep;
  sl_cpc_frame_t frame;
  sl_cpc_buf_t head;
  sl_status_t status;

  status = sl_cpc_ep_init(&ep, CPC_TEST_EP_ID, CPC_TEST_RX_SIZE, api_event_cb, NULL);
  TEST_ASSERT_EQUAL(SL_STATUS_OK, status);

  status = cpc_test_set_ep_connected(&ep, g_bus, 1U, CPC_TEST_PEER_SYN_SEQ + 1U, CPC_TEST_RX_SIZE, CPC_TEST_PEER_MTU,
                                     CPC_TEST_PEER_RX_WND);
  TEST_ASSERT_EQUAL(SL_STATUS_OK, status);

  sl_cpc_buf_init(&head, (void *)0xDEADBEE0, 4);
  /* Claims more payload remains, but has no next segment. */
  head.tot_len = 8;
  TEST_ASSERT_NULL(head.node.node);

  status = sl_cpc_ep_send(&ep, &head, &frame, NULL);
  TEST_ASSERT_EQUAL(SL_STATUS_INVALID_PARAMETER, status);

  sl_cpc_ep_close(&ep);

  cpc_test_pump();

  cpc_test_ep_discard(&ep);
}

TEST(cpc_api, write_rejects_unconnected_ep)
{
  sl_cpc_frame_t frame;
  uint8_t payload[4];
  sl_status_t status;
  sl_cpc_buf_t buf;
  sl_cpc_ep_t ep;

  status = sl_cpc_ep_init(&ep, CPC_TEST_EP_ID, CPC_TEST_RX_SIZE, api_event_cb, NULL);
  TEST_ASSERT_EQUAL(SL_STATUS_OK, status);
  status = sl_cpc_ep_listen(&ep, g_bus);
  TEST_ASSERT_EQUAL(SL_STATUS_OK, status);

  sl_cpc_buf_init(&buf, payload, sizeof(payload));

  status = sl_cpc_ep_send(&ep, &buf, &frame, NULL);
  TEST_ASSERT_EQUAL(SL_STATUS_INVALID_STATE, status);

  sl_cpc_ep_close(&ep);

  cpc_test_pump();

  cpc_test_ep_discard(&ep);
}

TEST(cpc_api, write_rejects_zero_remote_mtu)
{
  sl_cpc_frame_t frame;
  uint8_t payload[4];
  sl_status_t status;
  sl_cpc_buf_t buf;
  sl_cpc_ep_t ep;

  status = sl_cpc_ep_init(&ep, CPC_TEST_EP_ID, CPC_TEST_RX_SIZE, api_event_cb, NULL);
  TEST_ASSERT_EQUAL(SL_STATUS_OK, status);

  status = cpc_test_set_ep_connected(&ep, g_bus, 1U, CPC_TEST_PEER_SYN_SEQ + 1U, CPC_TEST_RX_SIZE, 0U, 10U);
  TEST_ASSERT_EQUAL(SL_STATUS_OK, status);

  sl_cpc_buf_init(&buf, payload, sizeof(payload));

  status = sl_cpc_ep_send(&ep, &buf, &frame, NULL);
  TEST_ASSERT_EQUAL(SL_STATUS_NOT_AVAILABLE, status);

  sl_cpc_ep_close(&ep);

  cpc_test_pump();

  cpc_test_ep_discard(&ep);
}

TEST(cpc_api, listen_then_double_close_rejects_second_call)
{
  sl_status_t status;
  sl_cpc_ep_t ep;

  status = sl_cpc_ep_init(&ep, CPC_TEST_EP_ID, CPC_TEST_RX_SIZE, api_event_cb, NULL);
  TEST_ASSERT_EQUAL(SL_STATUS_OK, status);
  status = sl_cpc_ep_listen(&ep, g_bus);
  TEST_ASSERT_EQUAL(SL_STATUS_OK, status);

  sl_cpc_ep_close(&ep);

  sl_cpc_ep_close(&ep);

  cpc_test_pump();
  cpc_test_ep_discard(&ep);
}

TEST(cpc_api, connect_then_double_close_rejects_second_call)
{
  sl_status_t status;
  sl_cpc_ep_t ep;

  status = sl_cpc_ep_init(&ep, CPC_TEST_EP_ID, CPC_TEST_RX_SIZE, api_event_cb, NULL);
  TEST_ASSERT_EQUAL(SL_STATUS_OK, status);
  status = sl_cpc_ep_connect(&ep, g_bus);
  TEST_ASSERT_EQUAL(SL_STATUS_OK, status);

  sl_cpc_ep_close(&ep);

  sl_cpc_ep_close(&ep);

  cpc_test_pump();
  cpc_test_ep_discard(&ep);
}

TEST(cpc_api, public_types_sizeof)
{
  TEST_ASSERT_TRUE(sizeof(sl_cpc_buf_t) > 0U);
  TEST_ASSERT_TRUE(sizeof(sl_cpc_bus_t) > 0U);
  TEST_ASSERT_TRUE(sizeof(sl_cpc_ep_t) > 0U);
  TEST_ASSERT_TRUE(sizeof(sl_cpc_frame_t) > 0U);
  TEST_ASSERT_TRUE(sizeof(sl_cpc_msgq_t) > 0U);
}

TEST_GROUP_RUNNER(cpc_api)
{
  RUN_TEST_CASE(cpc_api, init_and_listen_registers_ep_on_bus);
  RUN_TEST_CASE(cpc_api, listen_rejects_duplicate_ep_id);
  RUN_TEST_CASE(cpc_api, listen_close_then_listen_reattaches_same_ep);
  RUN_TEST_CASE(cpc_api, listen_and_connect_defer_until_notify_initialized);
  RUN_TEST_CASE(cpc_api, init_rejects_rx_size_above_max_payload);
  RUN_TEST_CASE(cpc_api, init_rejects_unaligned_rx_size);
  RUN_TEST_CASE(cpc_api, init_then_close_rejects_unopened_ep);
  RUN_TEST_CASE(cpc_api, init_rejects_missing_event_callback);
  RUN_TEST_CASE(cpc_api, push_recv_buf_rejects_NULL_buffer);
  RUN_TEST_CASE(cpc_api, push_recv_buf_rejects_NULL_data_ptr);
  RUN_TEST_CASE(cpc_api, push_recv_buf_rejects_unaligned_data_ptr);
  RUN_TEST_CASE(cpc_api, push_recv_buf_rejects_unaligned_ptr_in_chain);
  RUN_TEST_CASE(cpc_api, push_recv_buf_accepts_aligned_chain);
  RUN_TEST_CASE(cpc_api, push_recv_buf_rejects_valid_buf_when_rx_size_configured_to_0);
  RUN_TEST_CASE(cpc_api, pop_recv_buf_rejects_NULL_out_buf);
  RUN_TEST_CASE(cpc_api, pop_recv_buf_rejects_when_endpoint_not_closed_allowed_when_closed);
  RUN_TEST_CASE(cpc_api, pop_recv_buf_rejects_when_queue_empty);
  RUN_TEST_CASE(cpc_api, push_pop_recv_buf_pops_buffer_when_never_connected);
  RUN_TEST_CASE(cpc_api, write_rejects_null_frame);
  RUN_TEST_CASE(cpc_api, write_rejects_null_buffer);
  RUN_TEST_CASE(cpc_api, write_rejects_null_data_ptr);
  RUN_TEST_CASE(cpc_api, write_rejects_zero_tot_len);
  RUN_TEST_CASE(cpc_api, write_rejects_unaligned_ptr_in_chain);
  RUN_TEST_CASE(cpc_api, write_rejects_chain_with_tail_still_linked);
  RUN_TEST_CASE(cpc_api, write_rejects_chain_missing_next_segment);
  RUN_TEST_CASE(cpc_api, write_rejects_unconnected_ep);
  RUN_TEST_CASE(cpc_api, write_rejects_zero_remote_mtu);
  RUN_TEST_CASE(cpc_api, listen_then_double_close_rejects_second_call);
  RUN_TEST_CASE(cpc_api, connect_then_double_close_rejects_second_call);
  RUN_TEST_CASE(cpc_api, public_types_sizeof);
}
