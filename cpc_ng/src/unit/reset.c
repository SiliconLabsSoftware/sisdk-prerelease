/***************************************************************************/ /**
 * @file
 * @brief Very basic CPC bus bring-up test.
 *
 * Wires a freshly-allocated `sl_cpc_bus_t` to a mock driver, brings
 * the bus up through the public init/start sequence and then waits to
 * observe the reset frame the core is expected to push out on the bus.
 ******************************************************************************/

#include <string.h>

#include <unity_fixture.h>

#if defined(SL_COMPONENT_CATALOG_PRESENT)
#include "sl_component_catalog.h"
#endif

#if defined(SL_CATALOG_KERNEL_PRESENT)
#include "cmsis_os2.h"
#endif

#include "sl_status.h"

#include "../sli_cpc.h"
#include "../sli_cpc_bus.h"
#include "../sli_cpc_ep.h"
#include "../sli_cpc_frame.h"
#include "../sli_cpc_hdr.h"
#include "sl_cpc_bus_instances.h"
#include "sl_cpc_drv_mock.h"
#include "sl_cpc_drv_mock_default_config.h"

#include "../../port/mock/src/mock_cpc_timer.h"
#include "expect.h"
#include "fixture.h"

TEST_GROUP(cpc_reset);

TEST_SETUP(cpc_reset)
{
  TEST_CPC_SETUP(false);
}

TEST_TEAR_DOWN(cpc_reset)
{
  TEST_CPC_TEAR_DOWN();
}

/**
 * @brief Bring up an bus and observe the startup reset frame on the bus.
 */
TEST(cpc_reset, bus_start_sends_reset_frame)
{
  sl_status_t status;

  cpc_test_set_role_secondary();

  status = TEST_CPC_BUS_START();
  TEST_ASSERT_EQUAL(SL_STATUS_OK, status);

  TEST_ASSERT_OUTGOING({
    .flags = CPC_EXPECT_FLAG_RESET,
    .len = 0,
    .dst = 0,
    .seq = -1, // we don't really care about the actual seq value
    .ack = -1, // ack doesn't carry any information on reset frame
    .wnd = -1, // we don't care how many buffers were pushed in this ep
  });

#if defined(SL_CATALOG_KERNEL_PRESENT)
  osDelay(10);
#else
  sli_cpc_bus_process_action(g_bus);
#endif

  TEST_ASSERT_EQUAL(0, mock_cpc_timer_count_running());
  TEST_ASSERT_FALSE(mock_cpc_timer_has_pending());
  TEST_ASSERT_DRV_PENDING_TX(0);

  cpc_test_pump();
}

/**
 * @brief Verify that the startup reset frame is not retransmitted.
 */
TEST(cpc_reset, startup_reset_is_not_retransmitted)
{
  sl_status_t status;

  cpc_test_set_role_secondary();

  status = TEST_CPC_BUS_START();
  TEST_ASSERT_EQUAL(SL_STATUS_OK, status);

  TEST_ASSERT_OUTGOING({
    .flags = CPC_EXPECT_FLAG_RESET,
    .len = 0,
    .dst = 0,
    .wnd = -1, // don't care, implementation is free to change this value
  });

  cpc_test_pump();

  TEST_ASSERT_EQUAL(0, mock_cpc_timer_count_running());
  TEST_ASSERT_FALSE(mock_cpc_timer_has_pending());
  TEST_ASSERT_FALSE(mock_cpc_timer_fire_next());
  TEST_ASSERT_DRV_PENDING_TX(0);
  TEST_ASSERT_NULL(mock_cpc_drv_pop_tx_packet(sl_cpc_drv_mock_default));

  cpc_test_pump();
}

/**
 * @brief Verify that receiving a reset frame does not trigger an outgoing
 *        reset frame from the core (avoids reset ping-pong with the peer).
 */
TEST(cpc_reset, received_reset_does_not_reply_with_reset)
{
  sl_status_t status;

  cpc_test_set_role_secondary();

  status = TEST_CPC_BUS_START();
  TEST_ASSERT_EQUAL(SL_STATUS_OK, status);

  TEST_ASSERT_OUTGOING({
    .flags = CPC_EXPECT_FLAG_RESET,
    .len = 0,
    .dst = 0,
    .seq = -1, // we don't really care about the actual seq value
    .ack = -1, // ack doesn't carry any information on reset frame
    .wnd = -1, // we don't care how many buffers were pushed in this ep
  });

  cpc_test_pump();

  TEST_INJECT_FRAME({
    .flags = CPC_EXPECT_FLAG_RESET,
    .len = 0,
    .dst = 0,
    .seq = 0,
    .ack = 0,
    .wnd = 0,
  });

  cpc_test_pump();

  TEST_ASSERT_DRV_PENDING_TX(0);
  TEST_ASSERT_NULL(mock_cpc_drv_pop_tx_packet(sl_cpc_drv_mock_default));

  cpc_test_pump();
}

/**
 * @brief Verify that a regular data frame received on a closed endpoint
 *        triggers an outgoing reset frame to the peer.
 */
TEST(cpc_reset, data_frame_on_unallocated_ep_sends_reset)
{
  const uint8_t payload[] = {0x01, 0x02, 0x03, 0x04};

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

  TEST_ASSERT_OUTGOING({
    .flags = CPC_EXPECT_FLAG_RESET,
    .len = 0,
    .dst = CPC_TEST_EP_ID,
    .seq = 0,
    .ack = 0,
    .wnd = 0,
  });

  cpc_test_pump();
}

/**
 * @brief RST defers via PEND_RST when the TX frame pool is empty and the ep exists.
 *
 * Sequence:
 *   1. listen(), steal N-1 TX slots, hold the last with a deferred TX-complete RST.
 *   2. A second send_reset_frame fails — PEND_RST is armed.
 *   3. Completing the held TX returns the slot via process_transmit_complete;
 *      process_pending_flags retries the deferred RST on the same bus wake.
 *   4. Deferred RST is transmitted and PEND_RST is cleared.
 */
TEST(cpc_reset, reset_defers_when_tx_pool_exhausted)
{
  sl_cpc_frame_t *frames[SL_CPC_DRV_MOCK_DEFAULT_TX_FRAME_POOL_COUNT - 1U];
  size_t frame_count = 0;
  sl_status_t status;
  sl_cpc_ep_t ep;
  size_t i;

  status = sl_cpc_ep_init(&ep, CPC_TEST_EP_ID, CPC_TEST_RX_SIZE, cpc_test_event_cb, NULL);
  TEST_ASSERT_EQUAL(SL_STATUS_OK, status);

  status = sl_cpc_ep_listen(&ep, g_bus);
  TEST_ASSERT_EQUAL(SL_STATUS_OK, status);

  mock_cpc_drv_set_defer_tx_complete(sl_cpc_drv_mock_default, true);

  for (i = 0; i < SL_CPC_DRV_MOCK_DEFAULT_TX_FRAME_POOL_COUNT - 1U; i++) {
    frames[i] = sli_cpc_frame_new(g_bus, false);
    TEST_ASSERT_NOT_NULL(frames[i]);
    frame_count++;
  }

  sli_cpc_send_reset_frame(g_bus, CPC_TEST_EP_ID);
  cpc_test_pump();
  TEST_ASSERT_EQUAL(1, mock_cpc_drv_held_tx_count(sl_cpc_drv_mock_default));
  TEST_ASSERT_NULL(sli_cpc_frame_new(g_bus, false));
  TEST_ASSERT_OUTGOING({
    .flags = CPC_EXPECT_FLAG_RESET,
    .len = 0,
    .dst = CPC_TEST_EP_ID,
    .seq = 0,
    .ack = 0,
    .wnd = 0,
  });

  sli_cpc_send_reset_frame(g_bus, CPC_TEST_EP_ID);
  TEST_ASSERT_TRUE((ep.flags & SLI_CPC_EP_FLAG_PEND_RST) != 0);
  TEST_ASSERT_DRV_PENDING_TX(0);

  mock_cpc_drv_set_defer_tx_complete(sl_cpc_drv_mock_default, false);
  TEST_ASSERT_EQUAL(1, mock_cpc_drv_complete_held_tx(sl_cpc_drv_mock_default, 1));

  TEST_ASSERT_OUTGOING({
    .flags = CPC_EXPECT_FLAG_RESET,
    .len = 0,
    .dst = CPC_TEST_EP_ID,
    .seq = 0,
    .ack = 0,
    .wnd = 0,
  });

  TEST_ASSERT_TRUE((ep.flags & SLI_CPC_EP_FLAG_PEND_RST) == 0);

  for (i = 0; i < frame_count; i++) {
    sli_cpc_frame_free(frames[i]);
  }

  sl_cpc_ep_close(&ep);
  cpc_test_pump();
  cpc_test_ep_discard(&ep);
}

/**
 * @brief RST to an unallocated endpoint is best-effort when the TX pool is empty.
 *
 * With no endpoint to arm PEND_RST on, allocation failure drops the RST.
 * Returning a TX slot later must not resurrect it.
 */
TEST(cpc_reset, reset_on_unallocated_ep_dropped_when_tx_pool_exhausted)
{
  sl_cpc_frame_t *frames[SL_CPC_DRV_MOCK_DEFAULT_TX_FRAME_POOL_COUNT];
  const uint8_t payload[] = {0x01, 0x02, 0x03, 0x04};
  size_t frame_count = 0;
  size_t i;

  for (i = 0; i < SL_CPC_DRV_MOCK_DEFAULT_TX_FRAME_POOL_COUNT; i++) {
    frames[i] = sli_cpc_frame_new(g_bus, false);
    TEST_ASSERT_NOT_NULL(frames[i]);
    frame_count++;
  }
  TEST_ASSERT_NULL(sli_cpc_frame_new(g_bus, false));

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

  TEST_ASSERT_DRV_PENDING_TX(0);

  frame_count--;
  sli_cpc_frame_free(frames[frame_count]);
  frames[frame_count] = NULL;

  cpc_test_pump();

  // No endpoint to retry from — RST stays dropped.
  TEST_ASSERT_DRV_PENDING_TX(0);
  TEST_ASSERT_NULL(mock_cpc_drv_pop_tx_packet(sl_cpc_drv_mock_default));

  for (i = 0; i < frame_count; i++) {
    sli_cpc_frame_free(frames[i]);
  }

  cpc_test_pump();
}

TEST_GROUP_RUNNER(cpc_reset)
{
  RUN_TEST_CASE(cpc_reset, bus_start_sends_reset_frame);
  RUN_TEST_CASE(cpc_reset, startup_reset_is_not_retransmitted);
  RUN_TEST_CASE(cpc_reset, received_reset_does_not_reply_with_reset);
  RUN_TEST_CASE(cpc_reset, data_frame_on_unallocated_ep_sends_reset);
  RUN_TEST_CASE(cpc_reset, reset_defers_when_tx_pool_exhausted);
  RUN_TEST_CASE(cpc_reset, reset_on_unallocated_ep_dropped_when_tx_pool_exhausted);
}
