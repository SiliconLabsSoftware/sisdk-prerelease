/***************************************************************************/ /**
 * @file
 * @brief Multi-bus unit tests.
 ******************************************************************************/

#include <stdint.h>

#include <unity_fixture.h>

#include "sl_status.h"

#include "../sli_cpc.h"
#include "../sli_cpc_bus.h"
#include "../sli_cpc_ep.h"
#include "../sli_cpc_hdr.h"

#include "expect.h"
#include "fixture.h"

TEST_GROUP(cpc_multi_bus);

TEST_SETUP(cpc_multi_bus)
{
  TEST_CPC_MULTI_BUS_SETUP(true);
}

TEST_TEAR_DOWN(cpc_multi_bus)
{
  TEST_CPC_MULTI_BUS_TEAR_DOWN();
}

/**
 * @brief The same endpoint ID on bus0 and bus1 are independent sessions.
 *
 * Two listen(bus) calls with the same ID on bus0 and bus1 are two sockets.
 * Each SYN is answered only on the bus it arrived on; the sessions must not
 * share state or TX.
 */
TEST(cpc_multi_bus, bus_local_same_id_on_both_buses)
{
  sl_cpc_bus_t *bus0 = CPC_TEST_MOCK_BUS(0);
  sl_cpc_bus_t *bus1 = CPC_TEST_MOCK_BUS(1);

  mock_cpc_tx_packet_t *dut_syn_ack;
  sl_status_t status;
  sl_cpc_ep_t ep_a;
  sl_cpc_ep_t ep_b;

  status = sl_cpc_ep_init(&ep_a, CPC_TEST_EP_ID, CPC_TEST_RX_SIZE, cpc_test_event_cb, NULL);
  TEST_ASSERT_EQUAL(SL_STATUS_OK, status);
  status = sl_cpc_ep_listen(&ep_a, bus0);
  TEST_ASSERT_EQUAL(SL_STATUS_OK, status);

  status = sl_cpc_ep_init(&ep_b, CPC_TEST_EP_ID, CPC_TEST_RX_SIZE, cpc_test_event_cb, NULL);
  TEST_ASSERT_EQUAL(SL_STATUS_OK, status);
  status = sl_cpc_ep_listen(&ep_b, bus1);
  TEST_ASSERT_EQUAL(SL_STATUS_OK, status);

  TEST_INJECT_FRAME(bus0, {
                            .flags = CPC_EXPECT_FLAG_ACK_REQ | CPC_EXPECT_FLAG_SYN,
                            .len = CPC_TEST_PEER_MTU,
                            .dst = CPC_TEST_EP_ID,
                            .seq = CPC_TEST_PEER_SYN_SEQ,
                            .ack = 0,
                            .wnd = CPC_TEST_PEER_RX_WND,
                          });

  dut_syn_ack = TEST_POP_OUTGOING(bus0, {
                                          .flags = CPC_EXPECT_FLAG_ACK_REQ | CPC_EXPECT_FLAG_SYN,
                                          .len = CPC_TEST_RX_SIZE,
                                          .dst = CPC_TEST_EP_ID,
                                          .seq = -1,
                                          .ack = (int)(CPC_TEST_PEER_SYN_SEQ + 1U),
                                          .wnd = 0,
                                        });
  mock_cpc_tx_packet_free(&dut_syn_ack);

  TEST_INJECT_FRAME(bus1, {
                            .flags = CPC_EXPECT_FLAG_ACK_REQ | CPC_EXPECT_FLAG_SYN,
                            .len = CPC_TEST_PEER_MTU,
                            .dst = CPC_TEST_EP_ID,
                            .seq = CPC_TEST_PEER_SYN_SEQ,
                            .ack = 0,
                            .wnd = CPC_TEST_PEER_RX_WND,
                          });

  dut_syn_ack = TEST_POP_OUTGOING(bus1, {
                                          .flags = CPC_EXPECT_FLAG_ACK_REQ | CPC_EXPECT_FLAG_SYN,
                                          .len = CPC_TEST_RX_SIZE,
                                          .dst = CPC_TEST_EP_ID,
                                          .seq = -1,
                                          .ack = (int)(CPC_TEST_PEER_SYN_SEQ + 1U),
                                          .wnd = 0,
                                        });
  mock_cpc_tx_packet_free(&dut_syn_ack);

  TEST_ASSERT_EQUAL(SLI_CPC_STATE_SYN_RCVD, ep_a.state);
  TEST_ASSERT_EQUAL(SLI_CPC_STATE_SYN_RCVD, ep_b.state);
  TEST_ASSERT_EQUAL_PTR(&ep_a, sli_cpc_bus_find_ep_from_id(bus0, CPC_TEST_EP_ID));
  TEST_ASSERT_EQUAL_PTR(&ep_b, sli_cpc_bus_find_ep_from_id(bus1, CPC_TEST_EP_ID));

  sl_cpc_ep_close(&ep_a);
  sl_cpc_ep_close(&ep_b);
  cpc_test_pump(bus0);
  cpc_test_pump(bus1);
  cpc_test_ep_discard(&ep_b);
  cpc_test_ep_discard(&ep_a);
}

/**
 * @brief listen(NULL) binds and completes its handshake on the bus that saw the SYN.
 *
 * Until a SYN arrives the endpoint is not attached. A peer that SYNs on bus1
 * must be answered there: CONNECTED belongs to bus1, and bus0 must not
 * register the endpoint or emit a SYN-ACK for a session it never saw.
 */
TEST(cpc_multi_bus, listen_any_bus_syn_on_bus1_binds_there)
{
  sl_cpc_bus_t *bus0 = CPC_TEST_MOCK_BUS(0);
  sl_cpc_bus_t *bus1 = CPC_TEST_MOCK_BUS(1);

  mock_cpc_tx_packet_t *dut_syn_ack;
  uint8_t our_syn_seq;
  sl_status_t status;
  sl_cpc_ep_t ep;

  status = sl_cpc_ep_init(&ep, CPC_TEST_EP_ID, CPC_TEST_RX_SIZE, cpc_test_event_cb, NULL);
  TEST_ASSERT_EQUAL(SL_STATUS_OK, status);

  status = sl_cpc_ep_listen(&ep, NULL);
  TEST_ASSERT_EQUAL(SL_STATUS_OK, status);
  TEST_ASSERT_NULL(ep.bus);

  TEST_INJECT_FRAME(bus1, {
                            .flags = CPC_EXPECT_FLAG_ACK_REQ | CPC_EXPECT_FLAG_SYN,
                            .len = CPC_TEST_PEER_MTU,
                            .dst = CPC_TEST_EP_ID,
                            .seq = CPC_TEST_PEER_SYN_SEQ,
                            .ack = 0,
                            .wnd = CPC_TEST_PEER_RX_WND,
                          });

  dut_syn_ack = TEST_POP_OUTGOING(bus1, {
                                          .flags = CPC_EXPECT_FLAG_ACK_REQ | CPC_EXPECT_FLAG_SYN,
                                          .len = CPC_TEST_RX_SIZE,
                                          .dst = CPC_TEST_EP_ID,
                                          .seq = -1,
                                          .ack = (int)(CPC_TEST_PEER_SYN_SEQ + 1U),
                                          .wnd = 0,
                                        });
  our_syn_seq = dut_syn_ack->header.seq;
  mock_cpc_tx_packet_free(&dut_syn_ack);

  TEST_ASSERT_EQUAL_PTR(bus1, ep.bus);
  TEST_ASSERT_EQUAL_PTR(&ep, sli_cpc_bus_find_ep_from_id(bus1, CPC_TEST_EP_ID));
  TEST_ASSERT_NULL(sli_cpc_bus_find_ep_from_id(bus0, CPC_TEST_EP_ID));
  TEST_ASSERT_EQUAL(SLI_CPC_STATE_SYN_RCVD, ep.state);
  TEST_ASSERT_DRV_PENDING_TX(bus0, 0);

  TEST_INJECT_FRAME(bus1, {
                            .dst = CPC_TEST_EP_ID,
                            .seq = (int)(CPC_TEST_PEER_SYN_SEQ + 1U),
                            .ack = (int)(our_syn_seq + 1U),
                            .wnd = CPC_TEST_PEER_RX_WND,
                          });

  cpc_test_pump(bus0);
  cpc_test_pump(bus1);

  TEST_ASSERT_EQUAL(SLI_CPC_STATE_CONNECTED, ep.state);
  TEST_ASSERT_BIT_HIGH(SL_CPC_EP_EVENT_CONNECTED, cpc_test_event_mask);

  sl_cpc_ep_close(&ep);
  cpc_test_pump(bus0);
  cpc_test_pump(bus1);
  cpc_test_ep_discard(&ep);
}

/**
 * @brief Two listen(NULL) waiters for the same ID bind one-to-one with SYNs on different buses.
 *
 * Peers may open the same endpoint ID on different buses. Waiters are a FIFO:
 * the first SYN (on bus0) binds the oldest listener, the next SYN on bus1
 * binds the next, and neither displaces the other.
 */
TEST(cpc_multi_bus, listen_any_bus_two_listeners_bind_to_different_buses)
{
  sl_cpc_bus_t *bus0 = CPC_TEST_MOCK_BUS(0);
  sl_cpc_bus_t *bus1 = CPC_TEST_MOCK_BUS(1);

  mock_cpc_tx_packet_t *dut_syn_ack;
  sl_cpc_ep_t ep_second;
  sl_cpc_ep_t ep_first;
  sl_status_t status;

  status = sl_cpc_ep_init(&ep_first, CPC_TEST_EP_ID, CPC_TEST_RX_SIZE, cpc_test_event_cb, NULL);
  TEST_ASSERT_EQUAL(SL_STATUS_OK, status);
  status = sl_cpc_ep_listen(&ep_first, NULL);
  TEST_ASSERT_EQUAL(SL_STATUS_OK, status);

  status = sl_cpc_ep_init(&ep_second, CPC_TEST_EP_ID, CPC_TEST_RX_SIZE, cpc_test_event_cb, NULL);
  TEST_ASSERT_EQUAL(SL_STATUS_OK, status);
  status = sl_cpc_ep_listen(&ep_second, NULL);
  TEST_ASSERT_EQUAL(SL_STATUS_OK, status);

  TEST_INJECT_FRAME(bus0, {
                            .flags = CPC_EXPECT_FLAG_ACK_REQ | CPC_EXPECT_FLAG_SYN,
                            .len = CPC_TEST_PEER_MTU,
                            .dst = CPC_TEST_EP_ID,
                            .seq = CPC_TEST_PEER_SYN_SEQ,
                            .ack = 0,
                            .wnd = CPC_TEST_PEER_RX_WND,
                          });

  dut_syn_ack = TEST_POP_OUTGOING(bus0, {
                                          .flags = CPC_EXPECT_FLAG_ACK_REQ | CPC_EXPECT_FLAG_SYN,
                                          .len = CPC_TEST_RX_SIZE,
                                          .dst = CPC_TEST_EP_ID,
                                          .seq = -1,
                                          .ack = (int)(CPC_TEST_PEER_SYN_SEQ + 1U),
                                          .wnd = 0,
                                        });
  mock_cpc_tx_packet_free(&dut_syn_ack);

  TEST_ASSERT_EQUAL_PTR(bus0, ep_first.bus);
  TEST_ASSERT_EQUAL_PTR(&ep_first, sli_cpc_bus_find_ep_from_id(bus0, CPC_TEST_EP_ID));
  TEST_ASSERT_NULL(ep_second.bus);
  TEST_ASSERT_DRV_PENDING_TX(bus1, 0);

  TEST_INJECT_FRAME(bus1, {
                            .flags = CPC_EXPECT_FLAG_ACK_REQ | CPC_EXPECT_FLAG_SYN,
                            .len = CPC_TEST_PEER_MTU,
                            .dst = CPC_TEST_EP_ID,
                            .seq = CPC_TEST_PEER_SYN_SEQ,
                            .ack = 0,
                            .wnd = CPC_TEST_PEER_RX_WND,
                          });

  dut_syn_ack = TEST_POP_OUTGOING(bus1, {
                                          .flags = CPC_EXPECT_FLAG_ACK_REQ | CPC_EXPECT_FLAG_SYN,
                                          .len = CPC_TEST_RX_SIZE,
                                          .dst = CPC_TEST_EP_ID,
                                          .seq = -1,
                                          .ack = (int)(CPC_TEST_PEER_SYN_SEQ + 1U),
                                          .wnd = 0,
                                        });
  mock_cpc_tx_packet_free(&dut_syn_ack);

  TEST_ASSERT_EQUAL_PTR(bus1, ep_second.bus);
  TEST_ASSERT_EQUAL_PTR(&ep_second, sli_cpc_bus_find_ep_from_id(bus1, CPC_TEST_EP_ID));
  TEST_ASSERT_EQUAL_PTR(&ep_first, sli_cpc_bus_find_ep_from_id(bus0, CPC_TEST_EP_ID));

  sl_cpc_ep_close(&ep_first);
  sl_cpc_ep_close(&ep_second);
  cpc_test_pump(bus0);
  cpc_test_pump(bus1);
  cpc_test_ep_discard(&ep_second);
  cpc_test_ep_discard(&ep_first);
}

/**
 * @brief A bus0-local listener ignores a SYN on bus1; the any-bus waiter binds there.
 *
 * That SYN is what binds the any-bus waiter. If the bus0 endpoint handled it,
 * the peer on bus1 would get no handshake and the bus0 listener would join
 * the wrong session.
 */
TEST(cpc_multi_bus, listen_any_bus_bus_local_on_bus0_syn_on_bus1_takes_any)
{
  sl_cpc_bus_t *bus0 = CPC_TEST_MOCK_BUS(0);
  sl_cpc_bus_t *bus1 = CPC_TEST_MOCK_BUS(1);

  mock_cpc_tx_packet_t *dut_syn_ack;
  sl_cpc_ep_t ep_local;
  sl_cpc_ep_t ep_any;
  sl_status_t status;

  status = sl_cpc_ep_init(&ep_local, CPC_TEST_EP_ID, CPC_TEST_RX_SIZE, cpc_test_event_cb, NULL);
  TEST_ASSERT_EQUAL(SL_STATUS_OK, status);
  status = sl_cpc_ep_listen(&ep_local, bus0);
  TEST_ASSERT_EQUAL(SL_STATUS_OK, status);

  status = sl_cpc_ep_init(&ep_any, CPC_TEST_EP_ID, CPC_TEST_RX_SIZE, cpc_test_event_cb, NULL);
  TEST_ASSERT_EQUAL(SL_STATUS_OK, status);
  status = sl_cpc_ep_listen(&ep_any, NULL);
  TEST_ASSERT_EQUAL(SL_STATUS_OK, status);

  TEST_INJECT_FRAME(bus1, {
                            .flags = CPC_EXPECT_FLAG_ACK_REQ | CPC_EXPECT_FLAG_SYN,
                            .len = CPC_TEST_PEER_MTU,
                            .dst = CPC_TEST_EP_ID,
                            .seq = CPC_TEST_PEER_SYN_SEQ,
                            .ack = 0,
                            .wnd = CPC_TEST_PEER_RX_WND,
                          });

  dut_syn_ack = TEST_POP_OUTGOING(bus1, {
                                          .flags = CPC_EXPECT_FLAG_ACK_REQ | CPC_EXPECT_FLAG_SYN,
                                          .len = CPC_TEST_RX_SIZE,
                                          .dst = CPC_TEST_EP_ID,
                                          .seq = -1,
                                          .ack = (int)(CPC_TEST_PEER_SYN_SEQ + 1U),
                                          .wnd = 0,
                                        });
  mock_cpc_tx_packet_free(&dut_syn_ack);

  TEST_ASSERT_EQUAL_PTR(bus1, ep_any.bus);
  TEST_ASSERT_EQUAL(SLI_CPC_STATE_SYN_RCVD, ep_any.state);
  TEST_ASSERT_EQUAL_PTR(bus0, ep_local.bus);
  TEST_ASSERT_EQUAL(SLI_CPC_STATE_OPEN, ep_local.state);
  TEST_ASSERT_EQUAL_PTR(&ep_local, sli_cpc_bus_find_ep_from_id(bus0, CPC_TEST_EP_ID));
  TEST_ASSERT_DRV_PENDING_TX(bus0, 0);

  sl_cpc_ep_close(&ep_local);
  sl_cpc_ep_close(&ep_any);
  cpc_test_pump(bus0);
  cpc_test_pump(bus1);
  cpc_test_ep_discard(&ep_any);
  cpc_test_ep_discard(&ep_local);
}

/**
 * @brief A bus1-local listener ignores a SYN on bus0; the any-bus waiter binds there.
 *
 * bus0 is not privileged. A listener dedicated to bus1 still ignores a SYN on
 * bus0; that SYN is for the any-bus waiter.
 */
TEST(cpc_multi_bus, listen_any_bus_bus_local_on_bus1_syn_on_bus0_takes_any)
{
  sl_cpc_bus_t *bus0 = CPC_TEST_MOCK_BUS(0);
  sl_cpc_bus_t *bus1 = CPC_TEST_MOCK_BUS(1);

  mock_cpc_tx_packet_t *dut_syn_ack;
  sl_cpc_ep_t ep_local;
  sl_cpc_ep_t ep_any;
  sl_status_t status;

  status = sl_cpc_ep_init(&ep_local, CPC_TEST_EP_ID, CPC_TEST_RX_SIZE, cpc_test_event_cb, NULL);
  TEST_ASSERT_EQUAL(SL_STATUS_OK, status);
  status = sl_cpc_ep_listen(&ep_local, bus1);
  TEST_ASSERT_EQUAL(SL_STATUS_OK, status);

  status = sl_cpc_ep_init(&ep_any, CPC_TEST_EP_ID, CPC_TEST_RX_SIZE, cpc_test_event_cb, NULL);
  TEST_ASSERT_EQUAL(SL_STATUS_OK, status);
  status = sl_cpc_ep_listen(&ep_any, NULL);
  TEST_ASSERT_EQUAL(SL_STATUS_OK, status);

  TEST_INJECT_FRAME(bus0, {
                            .flags = CPC_EXPECT_FLAG_ACK_REQ | CPC_EXPECT_FLAG_SYN,
                            .len = CPC_TEST_PEER_MTU,
                            .dst = CPC_TEST_EP_ID,
                            .seq = CPC_TEST_PEER_SYN_SEQ,
                            .ack = 0,
                            .wnd = CPC_TEST_PEER_RX_WND,
                          });

  dut_syn_ack = TEST_POP_OUTGOING(bus0, {
                                          .flags = CPC_EXPECT_FLAG_ACK_REQ | CPC_EXPECT_FLAG_SYN,
                                          .len = CPC_TEST_RX_SIZE,
                                          .dst = CPC_TEST_EP_ID,
                                          .seq = -1,
                                          .ack = (int)(CPC_TEST_PEER_SYN_SEQ + 1U),
                                          .wnd = 0,
                                        });
  mock_cpc_tx_packet_free(&dut_syn_ack);

  TEST_ASSERT_EQUAL_PTR(bus0, ep_any.bus);
  TEST_ASSERT_EQUAL(SLI_CPC_STATE_SYN_RCVD, ep_any.state);
  TEST_ASSERT_EQUAL_PTR(bus1, ep_local.bus);
  TEST_ASSERT_EQUAL(SLI_CPC_STATE_OPEN, ep_local.state);
  TEST_ASSERT_DRV_PENDING_TX(bus1, 0);

  sl_cpc_ep_close(&ep_local);
  sl_cpc_ep_close(&ep_any);
  cpc_test_pump(bus0);
  cpc_test_pump(bus1);
  cpc_test_ep_discard(&ep_any);
  cpc_test_ep_discard(&ep_local);
}

/**
 * @brief On its own bus, a bus-local listener takes the SYN ahead of listen(NULL).
 *
 * Both may wait for the same ID, but a SYN on bus1 belongs to the endpoint
 * that asked for bus1. The any-bus waiter must stay available for a later
 * SYN on bus0.
 */
TEST(cpc_multi_bus, listen_any_bus_bus_local_on_bus1_wins_over_any)
{
  sl_cpc_bus_t *bus0 = CPC_TEST_MOCK_BUS(0);
  sl_cpc_bus_t *bus1 = CPC_TEST_MOCK_BUS(1);

  mock_cpc_tx_packet_t *dut_syn_ack;
  sl_cpc_ep_t ep_local;
  sl_cpc_ep_t ep_any;
  sl_status_t status;

  status = sl_cpc_ep_init(&ep_local, CPC_TEST_EP_ID, CPC_TEST_RX_SIZE, cpc_test_event_cb, NULL);
  TEST_ASSERT_EQUAL(SL_STATUS_OK, status);
  status = sl_cpc_ep_listen(&ep_local, bus1);
  TEST_ASSERT_EQUAL(SL_STATUS_OK, status);

  status = sl_cpc_ep_init(&ep_any, CPC_TEST_EP_ID, CPC_TEST_RX_SIZE, cpc_test_event_cb, NULL);
  TEST_ASSERT_EQUAL(SL_STATUS_OK, status);
  status = sl_cpc_ep_listen(&ep_any, NULL);
  TEST_ASSERT_EQUAL(SL_STATUS_OK, status);

  TEST_INJECT_FRAME(bus1, {
                            .flags = CPC_EXPECT_FLAG_ACK_REQ | CPC_EXPECT_FLAG_SYN,
                            .len = CPC_TEST_PEER_MTU,
                            .dst = CPC_TEST_EP_ID,
                            .seq = CPC_TEST_PEER_SYN_SEQ,
                            .ack = 0,
                            .wnd = CPC_TEST_PEER_RX_WND,
                          });

  dut_syn_ack = TEST_POP_OUTGOING(bus1, {
                                          .flags = CPC_EXPECT_FLAG_ACK_REQ | CPC_EXPECT_FLAG_SYN,
                                          .len = CPC_TEST_RX_SIZE,
                                          .dst = CPC_TEST_EP_ID,
                                          .seq = -1,
                                          .ack = (int)(CPC_TEST_PEER_SYN_SEQ + 1U),
                                          .wnd = 0,
                                        });
  mock_cpc_tx_packet_free(&dut_syn_ack);

  TEST_ASSERT_EQUAL(SLI_CPC_STATE_SYN_RCVD, ep_local.state);
  TEST_ASSERT_EQUAL_PTR(&ep_local, sli_cpc_bus_find_ep_from_id(bus1, CPC_TEST_EP_ID));
  TEST_ASSERT_NULL(ep_any.bus);
  TEST_ASSERT_EQUAL(SLI_CPC_STATE_CLOSED, ep_any.state);
  TEST_ASSERT_DRV_PENDING_TX(bus0, 0);

  sl_cpc_ep_close(&ep_local);
  sl_cpc_ep_close(&ep_any);
  cpc_test_pump(bus1);
  cpc_test_ep_discard(&ep_any);
  cpc_test_ep_discard(&ep_local);
}

/**
 * @brief A bus-local SYN does not consume listen(NULL); the other bus can still bind it.
 *
 * A SYN on bus0 belongs to the bus0 listener and must not consume the
 * any-bus waiter. That waiter exists so a peer on bus1 can still open the
 * same ID.
 */
TEST(cpc_multi_bus, listen_any_bus_local_on_bus0_then_any_binds_on_bus1)
{
  sl_cpc_bus_t *bus0 = CPC_TEST_MOCK_BUS(0);
  sl_cpc_bus_t *bus1 = CPC_TEST_MOCK_BUS(1);

  mock_cpc_tx_packet_t *dut_syn_ack;
  sl_cpc_ep_t ep_local;
  sl_cpc_ep_t ep_any;
  sl_status_t status;

  status = sl_cpc_ep_init(&ep_local, CPC_TEST_EP_ID, CPC_TEST_RX_SIZE, cpc_test_event_cb, NULL);
  TEST_ASSERT_EQUAL(SL_STATUS_OK, status);
  status = sl_cpc_ep_listen(&ep_local, bus0);
  TEST_ASSERT_EQUAL(SL_STATUS_OK, status);

  status = sl_cpc_ep_init(&ep_any, CPC_TEST_EP_ID, CPC_TEST_RX_SIZE, cpc_test_event_cb, NULL);
  TEST_ASSERT_EQUAL(SL_STATUS_OK, status);
  status = sl_cpc_ep_listen(&ep_any, NULL);
  TEST_ASSERT_EQUAL(SL_STATUS_OK, status);

  TEST_INJECT_FRAME(bus0, {
                            .flags = CPC_EXPECT_FLAG_ACK_REQ | CPC_EXPECT_FLAG_SYN,
                            .len = CPC_TEST_PEER_MTU,
                            .dst = CPC_TEST_EP_ID,
                            .seq = CPC_TEST_PEER_SYN_SEQ,
                            .ack = 0,
                            .wnd = CPC_TEST_PEER_RX_WND,
                          });

  dut_syn_ack = TEST_POP_OUTGOING(bus0, {
                                          .flags = CPC_EXPECT_FLAG_ACK_REQ | CPC_EXPECT_FLAG_SYN,
                                          .len = CPC_TEST_RX_SIZE,
                                          .dst = CPC_TEST_EP_ID,
                                          .seq = -1,
                                          .ack = (int)(CPC_TEST_PEER_SYN_SEQ + 1U),
                                          .wnd = 0,
                                        });
  mock_cpc_tx_packet_free(&dut_syn_ack);

  TEST_ASSERT_EQUAL(SLI_CPC_STATE_SYN_RCVD, ep_local.state);
  TEST_ASSERT_NULL(ep_any.bus);

  TEST_INJECT_FRAME(bus1, {
                            .flags = CPC_EXPECT_FLAG_ACK_REQ | CPC_EXPECT_FLAG_SYN,
                            .len = CPC_TEST_PEER_MTU,
                            .dst = CPC_TEST_EP_ID,
                            .seq = CPC_TEST_PEER_SYN_SEQ,
                            .ack = 0,
                            .wnd = CPC_TEST_PEER_RX_WND,
                          });

  dut_syn_ack = TEST_POP_OUTGOING(bus1, {
                                          .flags = CPC_EXPECT_FLAG_ACK_REQ | CPC_EXPECT_FLAG_SYN,
                                          .len = CPC_TEST_RX_SIZE,
                                          .dst = CPC_TEST_EP_ID,
                                          .seq = -1,
                                          .ack = (int)(CPC_TEST_PEER_SYN_SEQ + 1U),
                                          .wnd = 0,
                                        });
  mock_cpc_tx_packet_free(&dut_syn_ack);

  TEST_ASSERT_EQUAL_PTR(bus1, ep_any.bus);
  TEST_ASSERT_EQUAL(SLI_CPC_STATE_SYN_RCVD, ep_any.state);

  sl_cpc_ep_close(&ep_local);
  sl_cpc_ep_close(&ep_any);
  cpc_test_pump(bus0);
  cpc_test_pump(bus1);
  cpc_test_ep_discard(&ep_any);
  cpc_test_ep_discard(&ep_local);
}

/**
 * @brief Non-SYN traffic does not bind listen(NULL); a later SYN still can.
 *
 * Payload for an ID that is not allocated on bus1 is rejected (RST) and must
 * not attach the waiter. A later SYN on that bus is still the connect the
 * application asked for — otherwise garbage could consume a listener meant
 * for a real peer.
 */
TEST(cpc_multi_bus, listen_any_bus_non_syn_on_bus1_does_not_consume_listener)
{
  sl_cpc_bus_t *bus1 = CPC_TEST_MOCK_BUS(1);

  const uint8_t payload[] = {0x01, 0x02, 0x03, 0x04};
  mock_cpc_tx_packet_t *dut_syn_ack;
  sl_status_t status;
  sl_cpc_ep_t ep;

  status = sl_cpc_ep_init(&ep, CPC_TEST_EP_ID, CPC_TEST_RX_SIZE, cpc_test_event_cb, NULL);
  TEST_ASSERT_EQUAL(SL_STATUS_OK, status);

  status = sl_cpc_ep_listen(&ep, NULL);
  TEST_ASSERT_EQUAL(SL_STATUS_OK, status);

  TEST_INJECT_FRAME(bus1, {
                            .flags = CPC_EXPECT_FLAG_ACK_REQ,
                            .len = 4,
                            .dst = CPC_TEST_EP_ID,
                            .seq = 1,
                            .ack = 0,
                            .wnd = 8,
                            .payload = payload,
                          });

  cpc_test_pump(bus1);

  TEST_ASSERT_EQUAL(SLI_CPC_STATE_CLOSED, ep.state);
  TEST_ASSERT_NULL(ep.bus);
  TEST_ASSERT_BITS_HIGH(SLI_CPC_EP_FLAG_OPENED, ep.flags);

  TEST_ASSERT_OUTGOING(bus1, {
                               .flags = CPC_EXPECT_FLAG_RESET,
                               .len = 0,
                               .dst = CPC_TEST_EP_ID,
                               .seq = 0,
                               .ack = 0,
                               .wnd = 0,
                             });

  TEST_INJECT_FRAME(bus1, {
                            .flags = CPC_EXPECT_FLAG_ACK_REQ | CPC_EXPECT_FLAG_SYN,
                            .len = CPC_TEST_PEER_MTU,
                            .dst = CPC_TEST_EP_ID,
                            .seq = CPC_TEST_PEER_SYN_SEQ,
                            .ack = 0,
                            .wnd = CPC_TEST_PEER_RX_WND,
                          });

  dut_syn_ack = TEST_POP_OUTGOING(bus1, {
                                          .flags = CPC_EXPECT_FLAG_ACK_REQ | CPC_EXPECT_FLAG_SYN,
                                          .len = CPC_TEST_RX_SIZE,
                                          .dst = CPC_TEST_EP_ID,
                                          .seq = -1,
                                          .ack = (int)(CPC_TEST_PEER_SYN_SEQ + 1U),
                                          .wnd = 0,
                                        });
  mock_cpc_tx_packet_free(&dut_syn_ack);

  TEST_ASSERT_EQUAL_PTR(bus1, ep.bus);
  TEST_ASSERT_EQUAL(SLI_CPC_STATE_SYN_RCVD, ep.state);

  sl_cpc_ep_close(&ep);
  cpc_test_pump(bus1);
  cpc_test_ep_discard(&ep);
}

/**
 * @brief After close, a later SYN on any bus is unknown (RST).
 *
 * listen(NULL) is not on a bus yet, so close cannot use a single-bus
 * teardown path. The endpoint must leave the any-bus list; a SYN on bus1
 * after that is unknown (RST), and bus0 stays quiet.
 */
TEST(cpc_multi_bus, listen_any_bus_close_then_syn_on_bus1_is_reset)
{
  sl_cpc_bus_t *bus0 = CPC_TEST_MOCK_BUS(0);
  sl_cpc_bus_t *bus1 = CPC_TEST_MOCK_BUS(1);

  sl_status_t status;
  sl_cpc_ep_t ep;

  status = sl_cpc_ep_init(&ep, CPC_TEST_EP_ID, CPC_TEST_RX_SIZE, cpc_test_event_cb, NULL);
  TEST_ASSERT_EQUAL(SL_STATUS_OK, status);

  status = sl_cpc_ep_listen(&ep, NULL);
  TEST_ASSERT_EQUAL(SL_STATUS_OK, status);

  sl_cpc_ep_close(&ep);
  TEST_ASSERT_EQUAL(SLI_CPC_STATE_CLOSED, ep.state);
  TEST_ASSERT_BIT_HIGH(SL_CPC_EP_EVENT_CLOSED, cpc_test_event_mask);

  TEST_INJECT_FRAME(bus1, {
                            .flags = CPC_EXPECT_FLAG_ACK_REQ | CPC_EXPECT_FLAG_SYN,
                            .len = CPC_TEST_PEER_MTU,
                            .dst = CPC_TEST_EP_ID,
                            .seq = CPC_TEST_PEER_SYN_SEQ,
                            .ack = 0,
                            .wnd = CPC_TEST_PEER_RX_WND,
                          });
  cpc_test_pump(bus1);

  TEST_ASSERT_OUTGOING(bus1, {
                               .flags = CPC_EXPECT_FLAG_RESET,
                               .len = 0,
                               .dst = CPC_TEST_EP_ID,
                               .seq = 0,
                               .ack = 0,
                               .wnd = 0,
                             });
  TEST_ASSERT_DRV_PENDING_TX(bus0, 0);

  cpc_test_pump(bus1);
  cpc_test_ep_discard(&ep);
}

/**
 * @brief listen(NULL) stays unbound for a SYN on uninitialized bus1.
 *
 * A peer SYN on bus1 before that bus is initialized leaves the endpoint with
 * no bus and no TX on either bus. After bus1 is initialized, a later SYN on
 * bus1 attaches the endpoint there and replies with SYN-ACK.
 */
TEST(cpc_multi_bus, listen_any_bus_drops_syn_on_bus1_until_initialized)
{
  sl_cpc_bus_t *bus0 = CPC_TEST_MOCK_BUS(0);
  sl_cpc_bus_t *bus1 = CPC_TEST_MOCK_BUS(1);

  mock_cpc_tx_packet_t *dut_syn_ack;
  sl_status_t status;
  sl_cpc_ep_t ep;

  bus1->initialized = false;

  status = sl_cpc_ep_init(&ep, CPC_TEST_EP_ID, CPC_TEST_RX_SIZE, cpc_test_event_cb, NULL);
  TEST_ASSERT_EQUAL(SL_STATUS_OK, status);

  status = sl_cpc_ep_listen(&ep, NULL);
  TEST_ASSERT_EQUAL(SL_STATUS_OK, status);

  TEST_INJECT_FRAME(bus1, {
                            .flags = CPC_EXPECT_FLAG_ACK_REQ | CPC_EXPECT_FLAG_SYN,
                            .len = CPC_TEST_PEER_MTU,
                            .dst = CPC_TEST_EP_ID,
                            .seq = CPC_TEST_PEER_SYN_SEQ,
                            .ack = 0,
                            .wnd = CPC_TEST_PEER_RX_WND,
                          });
  cpc_test_pump(bus0);
  cpc_test_pump(bus1);

  TEST_ASSERT_EQUAL(SLI_CPC_STATE_CLOSED, ep.state);
  TEST_ASSERT_NULL(ep.bus);
  TEST_ASSERT_NULL(sli_cpc_bus_find_ep_from_id(bus1, CPC_TEST_EP_ID));
  TEST_ASSERT_NULL(sli_cpc_bus_find_ep_from_id(bus0, CPC_TEST_EP_ID));
  TEST_ASSERT_DRV_PENDING_TX(bus1, 0);
  TEST_ASSERT_DRV_PENDING_TX(bus0, 0);

  sli_cpc_bus_notify_initialized(bus1);

  TEST_ASSERT_TRUE(bus1->initialized);
  TEST_ASSERT_EQUAL(SLI_CPC_STATE_CLOSED, ep.state);
  TEST_ASSERT_NULL(ep.bus);

  TEST_INJECT_FRAME(bus1, {
                            .flags = CPC_EXPECT_FLAG_ACK_REQ | CPC_EXPECT_FLAG_SYN,
                            .len = CPC_TEST_PEER_MTU,
                            .dst = CPC_TEST_EP_ID,
                            .seq = CPC_TEST_PEER_SYN_SEQ,
                            .ack = 0,
                            .wnd = CPC_TEST_PEER_RX_WND,
                          });

  dut_syn_ack = TEST_POP_OUTGOING(bus1, {
                                          .flags = CPC_EXPECT_FLAG_ACK_REQ | CPC_EXPECT_FLAG_SYN,
                                          .len = CPC_TEST_RX_SIZE,
                                          .dst = CPC_TEST_EP_ID,
                                          .seq = -1,
                                          .ack = (int)(CPC_TEST_PEER_SYN_SEQ + 1U),
                                          .wnd = 0,
                                        });
  mock_cpc_tx_packet_free(&dut_syn_ack);

  TEST_ASSERT_EQUAL_PTR(bus1, ep.bus);
  TEST_ASSERT_EQUAL(SLI_CPC_STATE_SYN_RCVD, ep.state);

  sl_cpc_ep_close(&ep);
  cpc_test_pump(bus0);
  cpc_test_pump(bus1);
  cpc_test_ep_discard(&ep);
}

/**
 * @brief After bind on one bus, a SYN for the same ID on the other bus is RST.
 *
 * Once a SYN on bus1 has attached the endpoint, it is a normal bus1 socket.
 * A second peer SYNing the same ID on bus0 is not that session, so bus0 must
 * RST. The handshake on bus1 is unaffected.
 */
TEST(cpc_multi_bus, listen_any_bus_after_bind_on_bus1_syn_on_bus0_is_reset)
{
  sl_cpc_bus_t *bus0 = CPC_TEST_MOCK_BUS(0);
  sl_cpc_bus_t *bus1 = CPC_TEST_MOCK_BUS(1);

  mock_cpc_tx_packet_t *dut_syn_ack;
  sl_status_t status;
  sl_cpc_ep_t ep;

  status = sl_cpc_ep_init(&ep, CPC_TEST_EP_ID, CPC_TEST_RX_SIZE, cpc_test_event_cb, NULL);
  TEST_ASSERT_EQUAL(SL_STATUS_OK, status);

  status = sl_cpc_ep_listen(&ep, NULL);
  TEST_ASSERT_EQUAL(SL_STATUS_OK, status);

  TEST_INJECT_FRAME(bus1, {
                            .flags = CPC_EXPECT_FLAG_ACK_REQ | CPC_EXPECT_FLAG_SYN,
                            .len = CPC_TEST_PEER_MTU,
                            .dst = CPC_TEST_EP_ID,
                            .seq = CPC_TEST_PEER_SYN_SEQ,
                            .ack = 0,
                            .wnd = CPC_TEST_PEER_RX_WND,
                          });

  dut_syn_ack = TEST_POP_OUTGOING(bus1, {
                                          .flags = CPC_EXPECT_FLAG_ACK_REQ | CPC_EXPECT_FLAG_SYN,
                                          .len = CPC_TEST_RX_SIZE,
                                          .dst = CPC_TEST_EP_ID,
                                          .seq = -1,
                                          .ack = (int)(CPC_TEST_PEER_SYN_SEQ + 1U),
                                          .wnd = 0,
                                        });
  mock_cpc_tx_packet_free(&dut_syn_ack);

  TEST_ASSERT_EQUAL_PTR(bus1, ep.bus);

  TEST_INJECT_FRAME(bus0, {
                            .flags = CPC_EXPECT_FLAG_ACK_REQ | CPC_EXPECT_FLAG_SYN,
                            .len = CPC_TEST_PEER_MTU,
                            .dst = CPC_TEST_EP_ID,
                            .seq = CPC_TEST_PEER_SYN_SEQ,
                            .ack = 0,
                            .wnd = CPC_TEST_PEER_RX_WND,
                          });

  TEST_ASSERT_OUTGOING(bus0, {
                               .flags = CPC_EXPECT_FLAG_RESET,
                               .len = 0,
                               .dst = CPC_TEST_EP_ID,
                               .seq = 0,
                               .ack = 0,
                               .wnd = 0,
                             });

  TEST_ASSERT_EQUAL_PTR(bus1, ep.bus);
  TEST_ASSERT_EQUAL(SLI_CPC_STATE_SYN_RCVD, ep.state);
  TEST_ASSERT_EQUAL_PTR(&ep, sli_cpc_bus_find_ep_from_id(bus1, CPC_TEST_EP_ID));
  TEST_ASSERT_DRV_PENDING_TX(bus1, 0);

  cpc_test_pump(bus0);
  sl_cpc_ep_close(&ep);
  cpc_test_pump(bus1);
  cpc_test_ep_discard(&ep);
}

TEST_GROUP_RUNNER(cpc_multi_bus)
{
  RUN_TEST_CASE(cpc_multi_bus, bus_local_same_id_on_both_buses);
  RUN_TEST_CASE(cpc_multi_bus, listen_any_bus_syn_on_bus1_binds_there);
  RUN_TEST_CASE(cpc_multi_bus, listen_any_bus_two_listeners_bind_to_different_buses);
  RUN_TEST_CASE(cpc_multi_bus, listen_any_bus_bus_local_on_bus0_syn_on_bus1_takes_any);
  RUN_TEST_CASE(cpc_multi_bus, listen_any_bus_bus_local_on_bus1_syn_on_bus0_takes_any);
  RUN_TEST_CASE(cpc_multi_bus, listen_any_bus_bus_local_on_bus1_wins_over_any);
  RUN_TEST_CASE(cpc_multi_bus, listen_any_bus_local_on_bus0_then_any_binds_on_bus1);
  RUN_TEST_CASE(cpc_multi_bus, listen_any_bus_non_syn_on_bus1_does_not_consume_listener);
  RUN_TEST_CASE(cpc_multi_bus, listen_any_bus_close_then_syn_on_bus1_is_reset);
  RUN_TEST_CASE(cpc_multi_bus, listen_any_bus_drops_syn_on_bus1_until_initialized);
  RUN_TEST_CASE(cpc_multi_bus, listen_any_bus_after_bind_on_bus1_syn_on_bus0_is_reset);
}
