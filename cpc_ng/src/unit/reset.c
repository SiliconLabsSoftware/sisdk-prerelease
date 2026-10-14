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
#include "../sli_cpc_hdr.h"
#include "sl_cpc_bus_instances.h"
#include "sl_cpc_drv_mock.h"

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

TEST_GROUP_RUNNER(cpc_reset)
{
  RUN_TEST_CASE(cpc_reset, bus_start_sends_reset_frame);
  RUN_TEST_CASE(cpc_reset, startup_reset_is_not_retransmitted);
  RUN_TEST_CASE(cpc_reset, received_reset_does_not_reply_with_reset);
  RUN_TEST_CASE(cpc_reset, data_frame_on_unallocated_ep_sends_reset);
}
