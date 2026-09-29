/***************************************************************************/ /**
 * @file
 * @brief Unit tests for the CPC control-endpoint init sequence.
 ******************************************************************************/

#include <stdint.h>
#include <string.h>

#include <unity_fixture.h>

#if defined(SL_COMPONENT_CATALOG_PRESENT)
#include "sl_component_catalog.h"
#endif

#include "sl_status.h"

#include "../sli_cpc.h"
#include "../sli_cpc_bus.h"
#include "../sli_cpc_control.h"
#include "../sli_cpc_endianness.h"
#include "../sli_cpc_ep.h"
#include "../sli_cpc_hdr.h"
#include "sl_cpc_bus_instances.h"
#include "sl_cpc_drv_mock.h"

#include "../../port/mock/src/mock_cpc_timer.h"
#include "expect.h"
#include "fixture.h"

#define TEST_POP_CONTROL_PAYLOAD() \
  TEST_POP_OUTGOING_PAYLOAD({      \
    .flags = -1,                   \
    .len = -1,                     \
    .dst = SL_CPC_EP_ID_CONTROL,   \
    .seq = -1,                     \
    .ack = -1,                     \
    .wnd = -1,                     \
  })

TEST_GROUP(cpc_control);

TEST_SETUP(cpc_control)
{
  TEST_CPC_SETUP(false);
}

TEST_TEAR_DOWN(cpc_control)
{
  TEST_CPC_TEAR_DOWN();
}

/******************************************************************************/
/*                              Local helpers                                 */
/******************************************************************************/

static uint16_t pack_control_command(uint8_t *out, size_t out_cap, uint8_t type, uint16_t request_op_id, uint8_t status,
                                     const void *payload, uint16_t payload_len)
{
  sli_cpc_control_command_t *command = (sli_cpc_control_command_t *)out;
  uint16_t total = (uint16_t)(sizeof(command->header) + payload_len);

  TEST_ASSERT_TRUE(out_cap >= total);

  memset(out, 0, total);
  sli_cpc_control_header_set_payload_size(&command->header, payload_len);
  sli_cpc_control_header_set_op_id(&command->header, request_op_id);
  sli_cpc_control_header_set_type(&command->header, type);
  sli_cpc_control_header_set_status(&command->header, status);

  if (payload_len > 0) {
    TEST_ASSERT_NOT_NULL(payload);
    memcpy(command->data, payload, payload_len);
  }

  return total;
}

static const sli_cpc_control_header_t *control_hdr_from_tx(const mock_cpc_tx_packet_t *packet)
{
  uint16_t payload_len = sli_cpc_header_get_payload_size(&packet->header);

  TEST_ASSERT_TRUE(payload_len >= sizeof(sli_cpc_control_header_t));

  return (const sli_cpc_control_header_t *)packet->payload;
}

static void assert_control_type(const sli_cpc_control_header_t *hdr, uint8_t type, bool is_response)
{
  TEST_ASSERT_EQUAL_UINT8(type, sli_cpc_control_header_get_type(hdr));
  TEST_ASSERT_EQUAL(is_response, sli_cpc_control_header_is_response(hdr));
}

/**
 * @brief Complete the primary (active-open) control-EP SYN handshake.
 *
 * @return Sequence number of the DUT SYN (used to derive subsequent seq/ack).
 */
static uint8_t complete_primary_control_handshake(void)
{
  mock_cpc_tx_packet_t *dut_syn = TEST_POP_OUTGOING({
    .flags = CPC_EXPECT_FLAG_ACK_REQ | CPC_EXPECT_FLAG_SYN,
    .len = sizeof(g_bus->ctrl.rx_data),
    .dst = SL_CPC_EP_ID_CONTROL,
    .seq = -1,
    .ack = -1,
    .wnd = -1,
  });
  uint8_t our_syn_seq = sli_cpc_header_get_seq(&dut_syn->header);

  mock_cpc_tx_packet_free(&dut_syn);

  TEST_INJECT_FRAME({
    .flags = CPC_EXPECT_FLAG_ACK_REQ | CPC_EXPECT_FLAG_SYN,
    .len = CPC_TEST_PEER_MTU,
    .dst = SL_CPC_EP_ID_CONTROL,
    .seq = CPC_TEST_PEER_SYN_SEQ,
    .ack = (int)(our_syn_seq + 1U),
    .wnd = CPC_TEST_PEER_RX_WND,
  });

  // Process peer SYN-ACK and enter CONNECTED (starts the control init sequence).
  cpc_test_pump();

  return our_syn_seq;
}

/**
 * @brief Complete the secondary (passive-open) control-EP SYN handshake.
 *
 * Consumes the startup RESET first. Returns the DUT SYN sequence number.
 */
static uint8_t complete_secondary_control_handshake(void)
{
  mock_cpc_tx_packet_t *dut_syn;
  uint8_t our_syn_seq;

  TEST_ASSERT_OUTGOING({
    .flags = CPC_EXPECT_FLAG_RESET,
    .len = 0,
    .dst = SL_CPC_EP_ID_CONTROL,
    .seq = -1,
    .ack = -1,
    .wnd = -1,
  });

  TEST_INJECT_FRAME({
    .flags = CPC_EXPECT_FLAG_ACK_REQ | CPC_EXPECT_FLAG_SYN,
    .len = CPC_TEST_PEER_MTU,
    .dst = SL_CPC_EP_ID_CONTROL,
    .seq = CPC_TEST_PEER_SYN_SEQ,
    .ack = 0,
    .wnd = CPC_TEST_PEER_RX_WND,
  });

  dut_syn = TEST_POP_OUTGOING({
    .flags = CPC_EXPECT_FLAG_ACK_REQ | CPC_EXPECT_FLAG_SYN,
    .len = sizeof(g_bus->ctrl.rx_data),
    .dst = SL_CPC_EP_ID_CONTROL,
    .seq = -1,
    .ack = (int)(CPC_TEST_PEER_SYN_SEQ + 1U),
    .wnd = -1,
  });
  our_syn_seq = sli_cpc_header_get_seq(&dut_syn->header);
  mock_cpc_tx_packet_free(&dut_syn);

  TEST_INJECT_FRAME({
    .dst = SL_CPC_EP_ID_CONTROL,
    .seq = (int)(CPC_TEST_PEER_SYN_SEQ + 1U),
    .ack = (int)(our_syn_seq + 1U),
    .wnd = CPC_TEST_PEER_RX_WND,
  });

  // Process peer's final handshake ACK and enter CONNECTED.
  cpc_test_pump();

  return our_syn_seq;
}

/**
 * @brief Connect primary control.
 */
static void start_primary_control_connected(void)
{
  mock_cpc_tx_packet_t *packet;
  sl_status_t status;

  status = TEST_CPC_BUS_START();
  TEST_ASSERT_EQUAL(SL_STATUS_OK, status);

  complete_primary_control_handshake();

  packet = TEST_POP_CONTROL_PAYLOAD();
  mock_cpc_tx_packet_free(&packet);
  // Submit standalone ACK left on the TX queue after the handshake request.
  cpc_test_pump();
  // Recycle that ACK's TX-complete so the TX pool is clean at tear-down.
  cpc_test_pump();
}

/**
 * @brief Connect secondary control.
 */
static void start_secondary_control_connected(void)
{
  sl_status_t status;

  cpc_test_set_role_secondary();

  status = TEST_CPC_BUS_START();
  TEST_ASSERT_EQUAL(SL_STATUS_OK, status);

  complete_secondary_control_handshake();
  // Submit standalone ACK left on the TX queue after the handshake.
  cpc_test_pump();
  // Recycle that ACK's TX-complete so the TX pool is clean at tear-down.
  cpc_test_pump();
}

/**
 * @brief Init and mark a user endpoint as already CONNECTED on the test bus.
 */
static void open_connected_user_ep(sl_cpc_ep_t *ep, uint8_t id)
{
  sl_status_t status;

  status = sl_cpc_ep_init(ep, id, CPC_TEST_RX_SIZE, cpc_test_event_cb, NULL);
  TEST_ASSERT_EQUAL(SL_STATUS_OK, status);

  status = cpc_test_set_ep_connected(ep, g_bus, 1U, CPC_TEST_PEER_SYN_SEQ + 1U, CPC_TEST_RX_SIZE, CPC_TEST_PEER_MTU,
                                     CPC_TEST_PEER_RX_WND);
  TEST_ASSERT_EQUAL(SL_STATUS_OK, status);
  TEST_ASSERT_EQUAL(SLI_CPC_STATE_CONNECTED, ep->state);
  TEST_ASSERT_TRUE((ep->flags & SLI_CPC_EP_FLAG_OPENED) != 0);
}

/**
 * @brief Peer RESET on the control EP; pump through on_closed and user ERROR.
 */
static void trigger_control_close_via_reset(void)
{
  TEST_INJECT_FRAME({
    .flags = CPC_EXPECT_FLAG_RESET,
    .len = 0,
    .dst = SL_CPC_EP_ID_CONTROL,
    .seq = 0,
    .ack = 0,
    .wnd = 0,
  });
  // Process peer RESET: terminate control EP.
  cpc_test_pump();
  // Finish control CLOSED / on_closed (error active users, re-attach).
  cpc_test_pump();
  // Drain any follow-up processing from teardown.
  cpc_test_pump();
}

/**
 * @brief Finish primary reconnect so the recovery SYN is ACKed and returned to the TX pool.
 */
static void finish_primary_control_reconnect(void)
{
  mock_cpc_tx_packet_t *packet;

  complete_primary_control_handshake();

  // Discard the PROTOCOL_VERSION request queued by on_connected.
  packet = TEST_POP_CONTROL_PAYLOAD();
  mock_cpc_tx_packet_free(&packet);
  // Submit standalone ACK left on the TX queue after the handshake request.
  cpc_test_pump();
  // Recycle that ACK's TX-complete so the TX pool is clean at tear-down.
  cpc_test_pump();
}

/******************************************************************************/
/*                                  Tests                                     */
/******************************************************************************/

/**
 * @brief Primary happy path: full init sequence through bus bring-up.
 *
 * Exercises PROTOCOL_VERSION → RESET_REASON → PHY_CAPABILITIES → BUS_ENABLE,
 * asserting !g_bus->initialized and !g_bus->ctrl.initialized after each step, and that
 * both flags are raised only after the final BUS_ENABLE response is processed.
 */
TEST(cpc_control, primary_init_sequence)
{
  uint8_t ctrl_payload[sizeof(sli_cpc_control_header_t) + 16];
  sli_cpc_protocol_version_response_t version_resp;
  const sli_cpc_protocol_version_request_t *req;
  sli_cpc_reset_reason_response_t reset_resp;
  const sli_cpc_control_header_t *hdr;
  mock_cpc_tx_packet_t *packet;
  uint16_t request_op_id;
  uint8_t our_syn_seq;
  sl_status_t status;
  uint16_t ctrl_len;
  uint8_t peer_seq;
  uint8_t dut_seq;

  status = TEST_CPC_BUS_START();
  TEST_ASSERT_EQUAL(SL_STATUS_OK, status);
  TEST_ASSERT_FALSE(g_bus->initialized);

  our_syn_seq = complete_primary_control_handshake();
  peer_seq = (uint8_t)(CPC_TEST_PEER_SYN_SEQ + 1U);
  dut_seq = (uint8_t)(our_syn_seq + 1U);
  TEST_ASSERT_FALSE(g_bus->initialized);
  TEST_ASSERT_FALSE(g_bus->ctrl.initialized);

  // PROTOCOL_VERSION request
  packet = TEST_POP_CONTROL_PAYLOAD();
  TEST_ASSERT_EQUAL_UINT8(dut_seq, sli_cpc_header_get_seq(&packet->header));
  hdr = control_hdr_from_tx(packet);
  assert_control_type(hdr, SLI_CPC_CTRL_TYPE_PROTOCOL_VERSION, false);
  TEST_ASSERT_EQUAL_UINT16(sizeof(sli_cpc_protocol_version_request_t), sli_cpc_control_header_get_payload_size(hdr));
  req = (const sli_cpc_protocol_version_request_t *)((const sli_cpc_control_command_t *)packet->payload)->data;
  TEST_ASSERT_EQUAL_UINT8(SL_CPC_VERSION_MAJOR, req->version_major);
  TEST_ASSERT_EQUAL_UINT8(SL_CPC_VERSION_MINOR, req->version_minor);
  request_op_id = sli_cpc_control_header_get_op_id(hdr);
  mock_cpc_tx_packet_free(&packet);

  version_resp.version_major = SL_CPC_VERSION_MAJOR;
  version_resp.version_minor = SL_CPC_VERSION_MINOR;
  ctrl_len = pack_control_command(ctrl_payload, sizeof(ctrl_payload),
                                  (uint8_t)(SLI_CPC_CTRL_TYPE_PROTOCOL_VERSION | SLI_CPC_CTRL_TYPE_RESPONSE_FLAG),
                                  request_op_id, SLI_CPC_CTRL_STATUS_OK, &version_resp, sizeof(version_resp));
  TEST_INJECT_FRAME({
    .flags = CPC_EXPECT_FLAG_ACK_REQ,
    .len = ctrl_len,
    .dst = SL_CPC_EP_ID_CONTROL,
    .seq = peer_seq,
    .ack = (int)(dut_seq + 1U),
    .wnd = CPC_TEST_PEER_RX_WND,
    .payload = ctrl_payload,
  });
  peer_seq++;
  dut_seq++;
  // Process PROTOCOL_VERSION response and advance to RESET_REASON request.
  cpc_test_pump();
  TEST_ASSERT_FALSE(g_bus->initialized);
  TEST_ASSERT_FALSE(g_bus->ctrl.initialized);

  // RESET_REASON request
  packet = TEST_POP_CONTROL_PAYLOAD();
  TEST_ASSERT_EQUAL_UINT8(dut_seq, sli_cpc_header_get_seq(&packet->header));
  hdr = control_hdr_from_tx(packet);
  assert_control_type(hdr, SLI_CPC_CTRL_TYPE_RESET_REASON, false);
  TEST_ASSERT_EQUAL_UINT16(0, sli_cpc_control_header_get_payload_size(hdr));
  request_op_id = sli_cpc_control_header_get_op_id(hdr);
  mock_cpc_tx_packet_free(&packet);

  sli_cpc_u32_to_le(SLI_CPC_RESET_REASON_UNKNOWN, reset_resp.reset_reason_le);
  ctrl_len = pack_control_command(ctrl_payload, sizeof(ctrl_payload),
                                  (uint8_t)(SLI_CPC_CTRL_TYPE_RESET_REASON | SLI_CPC_CTRL_TYPE_RESPONSE_FLAG),
                                  request_op_id, SLI_CPC_CTRL_STATUS_OK, &reset_resp, sizeof(reset_resp));
  TEST_INJECT_FRAME({
    .flags = CPC_EXPECT_FLAG_ACK_REQ,
    .len = ctrl_len,
    .dst = SL_CPC_EP_ID_CONTROL,
    .seq = peer_seq,
    .ack = (int)(dut_seq + 1U),
    .wnd = CPC_TEST_PEER_RX_WND,
    .payload = ctrl_payload,
  });
  peer_seq++;
  dut_seq++;
  // Process RESET_REASON response and advance to PHY_CAPABILITIES request.
  cpc_test_pump();
  TEST_ASSERT_FALSE(g_bus->initialized);
  TEST_ASSERT_FALSE(g_bus->ctrl.initialized);

  // PHY_CAPABILITIES request (empty caps is fine)
  packet = TEST_POP_CONTROL_PAYLOAD();
  TEST_ASSERT_EQUAL_UINT8(dut_seq, sli_cpc_header_get_seq(&packet->header));
  hdr = control_hdr_from_tx(packet);
  assert_control_type(hdr, SLI_CPC_CTRL_TYPE_PHY_CAPABILITIES, false);
  request_op_id = sli_cpc_control_header_get_op_id(hdr);
  mock_cpc_tx_packet_free(&packet);

  ctrl_len = pack_control_command(ctrl_payload, sizeof(ctrl_payload),
                                  (uint8_t)(SLI_CPC_CTRL_TYPE_PHY_CAPABILITIES | SLI_CPC_CTRL_TYPE_RESPONSE_FLAG),
                                  request_op_id, SLI_CPC_CTRL_STATUS_OK, NULL, 0);
  TEST_INJECT_FRAME({
    .flags = CPC_EXPECT_FLAG_ACK_REQ,
    .len = ctrl_len,
    .dst = SL_CPC_EP_ID_CONTROL,
    .seq = peer_seq,
    .ack = (int)(dut_seq + 1U),
    .wnd = CPC_TEST_PEER_RX_WND,
    .payload = ctrl_payload,
  });
  peer_seq++;
  dut_seq++;
  // Process PHY_CAPABILITIES response and advance to BUS_ENABLE request.
  cpc_test_pump();
  TEST_ASSERT_FALSE(g_bus->initialized);
  TEST_ASSERT_FALSE(g_bus->ctrl.initialized);

  // BUS_ENABLE request
  packet = TEST_POP_CONTROL_PAYLOAD();
  TEST_ASSERT_EQUAL_UINT8(dut_seq, sli_cpc_header_get_seq(&packet->header));
  hdr = control_hdr_from_tx(packet);
  assert_control_type(hdr, SLI_CPC_CTRL_TYPE_BUS_ENABLE, false);
  TEST_ASSERT_EQUAL_UINT16(0, sli_cpc_control_header_get_payload_size(hdr));
  request_op_id = sli_cpc_control_header_get_op_id(hdr);
  mock_cpc_tx_packet_free(&packet);

  ctrl_len = pack_control_command(ctrl_payload, sizeof(ctrl_payload),
                                  (uint8_t)(SLI_CPC_CTRL_TYPE_BUS_ENABLE | SLI_CPC_CTRL_TYPE_RESPONSE_FLAG),
                                  request_op_id, SLI_CPC_CTRL_STATUS_OK, NULL, 0);
  TEST_INJECT_FRAME({
    .flags = CPC_EXPECT_FLAG_ACK_REQ,
    .len = ctrl_len,
    .dst = SL_CPC_EP_ID_CONTROL,
    .seq = peer_seq,
    .ack = (int)(dut_seq + 1U),
    .wnd = CPC_TEST_PEER_RX_WND,
    .payload = ctrl_payload,
  });
  // Process BUS_ENABLE response: sets g_bus->ctrl.initialized during RX.
  cpc_test_pump();
  TEST_ASSERT_TRUE(g_bus->ctrl.initialized);
  // Process process_bus_initialized.
  cpc_test_pump();
  TEST_ASSERT_TRUE(g_bus->initialized);

  // ACK the final response so tear-down does not leave frames in retx.
  TEST_INJECT_FRAME({
    .dst = SL_CPC_EP_ID_CONTROL,
    .seq = (int)(peer_seq + 1U),
    .ack = (int)(dut_seq + 1U),
    .wnd = CPC_TEST_PEER_RX_WND,
  });
  // Process peer ACK of the final control response.
  cpc_test_pump();
  // Submit standalone ACK queued when the BUS_ENABLE response was processed.
  cpc_test_pump();
  // Recycle that ACK's TX-complete so the TX pool is clean at tear-down.
  cpc_test_pump();
}

/**
 * @brief Secondary happy path: peer-driven init sequence through bus bring-up.
 *
 * Exercises the responder-side init chain, asserting !g_bus->initialized and
 * !g_bus->ctrl.initialized after each step. Control initialized is set from
 * send_done on the BUS_ENABLE response, not on RX as on the primary.
 */
TEST(cpc_control, secondary_init_sequence)
{
  uint8_t ctrl_payload[sizeof(sli_cpc_control_header_t) + 16];
  sli_cpc_protocol_version_request_t version_req;
  const sli_cpc_control_header_t *hdr;
  mock_cpc_tx_packet_t *packet;
  uint16_t request_op_id = 0;
  uint8_t our_syn_seq;
  sl_status_t status;
  uint16_t ctrl_len;
  uint8_t peer_seq;
  uint8_t dut_seq;

  cpc_test_set_role_secondary();

  status = TEST_CPC_BUS_START();
  TEST_ASSERT_EQUAL(SL_STATUS_OK, status);
  TEST_ASSERT_FALSE(g_bus->initialized);

  our_syn_seq = complete_secondary_control_handshake();
  // Ack-only third leg does not take sequence space (no ACK_REQ), so the next
  // reliable peer frame still uses the seq that completed the handshake.
  peer_seq = (uint8_t)(CPC_TEST_PEER_SYN_SEQ + 1U);
  dut_seq = (uint8_t)(our_syn_seq + 1U);
  TEST_ASSERT_FALSE(g_bus->initialized);
  TEST_ASSERT_FALSE(g_bus->ctrl.initialized);

  // PROTOCOL_VERSION request → response
  version_req.version_major = SL_CPC_VERSION_MAJOR;
  version_req.version_minor = SL_CPC_VERSION_MINOR;
  ctrl_len = pack_control_command(ctrl_payload, sizeof(ctrl_payload), SLI_CPC_CTRL_TYPE_PROTOCOL_VERSION, request_op_id,
                                  SLI_CPC_CTRL_STATUS_OK, &version_req, sizeof(version_req));
  TEST_INJECT_FRAME({
    .flags = CPC_EXPECT_FLAG_ACK_REQ,
    .len = ctrl_len,
    .dst = SL_CPC_EP_ID_CONTROL,
    .seq = peer_seq,
    .ack = dut_seq,
    .wnd = CPC_TEST_PEER_RX_WND,
    .payload = ctrl_payload,
  });
  peer_seq++;
  // Process PROTOCOL_VERSION request and send the response.
  cpc_test_pump();
  TEST_ASSERT_FALSE(g_bus->initialized);
  TEST_ASSERT_FALSE(g_bus->ctrl.initialized);

  packet = TEST_POP_CONTROL_PAYLOAD();
  TEST_ASSERT_EQUAL_UINT8(dut_seq, sli_cpc_header_get_seq(&packet->header));
  hdr = control_hdr_from_tx(packet);
  assert_control_type(hdr, SLI_CPC_CTRL_TYPE_PROTOCOL_VERSION, true);
  TEST_ASSERT_EQUAL_UINT8(SLI_CPC_CTRL_STATUS_OK, sli_cpc_control_header_get_status(hdr));
  TEST_ASSERT_EQUAL_UINT16(request_op_id, sli_cpc_control_header_get_op_id(hdr));
  const sli_cpc_protocol_version_response_t *resp
    = (const sli_cpc_protocol_version_response_t *)((const sli_cpc_control_command_t *)packet->payload)->data;
  TEST_ASSERT_EQUAL_UINT8(SL_CPC_VERSION_MAJOR, resp->version_major);
  TEST_ASSERT_EQUAL_UINT8(SL_CPC_VERSION_MINOR, resp->version_minor);
  mock_cpc_tx_packet_free(&packet);
  dut_seq++;
  request_op_id++;

  // RESET_REASON request → response
  ctrl_len = pack_control_command(ctrl_payload, sizeof(ctrl_payload), SLI_CPC_CTRL_TYPE_RESET_REASON, request_op_id,
                                  SLI_CPC_CTRL_STATUS_OK, NULL, 0);
  TEST_INJECT_FRAME({
    .flags = CPC_EXPECT_FLAG_ACK_REQ,
    .len = ctrl_len,
    .dst = SL_CPC_EP_ID_CONTROL,
    .seq = peer_seq,
    .ack = dut_seq,
    .wnd = CPC_TEST_PEER_RX_WND,
    .payload = ctrl_payload,
  });
  peer_seq++;
  // Process RESET_REASON request and send the response.
  cpc_test_pump();
  TEST_ASSERT_FALSE(g_bus->initialized);
  TEST_ASSERT_FALSE(g_bus->ctrl.initialized);

  packet = TEST_POP_CONTROL_PAYLOAD();
  TEST_ASSERT_EQUAL_UINT8(dut_seq, sli_cpc_header_get_seq(&packet->header));
  hdr = control_hdr_from_tx(packet);
  assert_control_type(hdr, SLI_CPC_CTRL_TYPE_RESET_REASON, true);
  TEST_ASSERT_EQUAL_UINT8(SLI_CPC_CTRL_STATUS_OK, sli_cpc_control_header_get_status(hdr));
  TEST_ASSERT_EQUAL_UINT16(sizeof(sli_cpc_reset_reason_response_t), sli_cpc_control_header_get_payload_size(hdr));
  mock_cpc_tx_packet_free(&packet);
  dut_seq++;
  request_op_id++;

  // PHY_CAPABILITIES request → response
  ctrl_len = pack_control_command(ctrl_payload, sizeof(ctrl_payload), SLI_CPC_CTRL_TYPE_PHY_CAPABILITIES, request_op_id,
                                  SLI_CPC_CTRL_STATUS_OK, NULL, 0);
  TEST_INJECT_FRAME({
    .flags = CPC_EXPECT_FLAG_ACK_REQ,
    .len = ctrl_len,
    .dst = SL_CPC_EP_ID_CONTROL,
    .seq = peer_seq,
    .ack = dut_seq,
    .wnd = CPC_TEST_PEER_RX_WND,
    .payload = ctrl_payload,
  });
  peer_seq++;
  // Process PHY_CAPABILITIES request and send the response.
  cpc_test_pump();
  TEST_ASSERT_FALSE(g_bus->initialized);
  TEST_ASSERT_FALSE(g_bus->ctrl.initialized);

  packet = TEST_POP_CONTROL_PAYLOAD();
  TEST_ASSERT_EQUAL_UINT8(dut_seq, sli_cpc_header_get_seq(&packet->header));
  hdr = control_hdr_from_tx(packet);
  assert_control_type(hdr, SLI_CPC_CTRL_TYPE_PHY_CAPABILITIES, true);
  TEST_ASSERT_EQUAL_UINT8(SLI_CPC_CTRL_STATUS_OK, sli_cpc_control_header_get_status(hdr));
  mock_cpc_tx_packet_free(&packet);
  dut_seq++;
  request_op_id++;

  // BUS_ENABLE request → response → initialized on TX complete
  ctrl_len = pack_control_command(ctrl_payload, sizeof(ctrl_payload), SLI_CPC_CTRL_TYPE_BUS_ENABLE, request_op_id,
                                  SLI_CPC_CTRL_STATUS_OK, NULL, 0);
  TEST_INJECT_FRAME({
    .flags = CPC_EXPECT_FLAG_ACK_REQ,
    .len = ctrl_len,
    .dst = SL_CPC_EP_ID_CONTROL,
    .seq = peer_seq,
    .ack = dut_seq,
    .wnd = CPC_TEST_PEER_RX_WND,
    .payload = ctrl_payload,
  });
  // Process BUS_ENABLE request and send the response.
  cpc_test_pump();
  TEST_ASSERT_FALSE(g_bus->initialized);
  TEST_ASSERT_FALSE(g_bus->ctrl.initialized);

  packet = TEST_POP_CONTROL_PAYLOAD();
  TEST_ASSERT_EQUAL_UINT8(dut_seq, sli_cpc_header_get_seq(&packet->header));
  hdr = control_hdr_from_tx(packet);
  assert_control_type(hdr, SLI_CPC_CTRL_TYPE_BUS_ENABLE, true);
  TEST_ASSERT_EQUAL_UINT8(SLI_CPC_CTRL_STATUS_OK, sli_cpc_control_header_get_status(hdr));
  mock_cpc_tx_packet_free(&packet);

  TEST_INJECT_FRAME({
    .dst = SL_CPC_EP_ID_CONTROL,
    .seq = (int)(peer_seq + 1U),
    .ack = (int)(dut_seq + 1U),
    .wnd = CPC_TEST_PEER_RX_WND,
  });

  // Peer ACK queues BUS_ENABLE response write-completion (send_done not yet run).
  cpc_test_pump();
  // send_done sets g_bus->ctrl.initialized; process_bus_initialized can raise g_bus->initialized.
  cpc_test_pump();
  TEST_ASSERT_TRUE(g_bus->ctrl.initialized);
  TEST_ASSERT_TRUE(g_bus->initialized);
}

/**
 * @brief Primary control reconnects after peer RESET aborts an in-flight SYN.
 *
 * Generic SYN/RESET behavior lives in syn.c; this verifies control EP
 * auto-reconnect (re-attach and connect) after secondary startup RESET
 * interrupts the primary's outstanding SYN.
 */
TEST(cpc_control, primary_recovers_from_reset_during_syn)
{
  mock_cpc_tx_packet_t *dut_syn;
  const sl_cpc_ep_t *ep;
  sl_status_t status;

  status = TEST_CPC_BUS_START();
  TEST_ASSERT_EQUAL(SL_STATUS_OK, status);
  TEST_ASSERT_FALSE(g_bus->initialized);

  // First active-open SYN from primary control.
  dut_syn = TEST_POP_OUTGOING({
    .flags = CPC_EXPECT_FLAG_ACK_REQ | CPC_EXPECT_FLAG_SYN,
    .len = sizeof(g_bus->ctrl.rx_data),
    .dst = SL_CPC_EP_ID_CONTROL,
    .seq = -1,
    .ack = -1,
    .wnd = -1,
  });
  mock_cpc_tx_packet_free(&dut_syn);
  // Let the first SYN sit outstanding before the peer RESET arrives.
  cpc_test_pump();

  // Peer RESET aborts the handshake (secondary startup does this).
  TEST_INJECT_FRAME({
    .flags = CPC_EXPECT_FLAG_RESET,
    .len = 0,
    .dst = SL_CPC_EP_ID_CONTROL,
    .seq = 0,
    .ack = 0,
    .wnd = 0,
  });
  // Process peer RESET and tear down the outstanding SYN handshake.
  cpc_test_pump();

  // Primary control reconnects and completes a fresh SYN handshake.
  complete_primary_control_handshake();

  // Submit standalone ACK left on the TX queue after the handshake.
  cpc_test_pump();
  // Recycle that ACK's TX-complete so the TX pool is clean at tear-down.
  cpc_test_pump();

  ep = sli_cpc_bus_find_ep_from_id(g_bus, SL_CPC_EP_ID_CONTROL);
  TEST_ASSERT_NOT_NULL(ep);
  TEST_ASSERT_EQUAL(SLI_CPC_STATE_CONNECTED, ep->state);
}

/**
 * @brief Primary reconnects after the SYN retransmit retries is exhausted.
 *
 * Leaves the initial control SYN unacked so the retransmit timer fires
 * SLI_CPC_RE_TRANSMIT times, then once more to raise SL_STATUS_TIMEOUT.
 * The control endpoint must close, re-attach, and connect again so a
 * fresh SYN handshake can complete.
 */
TEST(cpc_control, primary_recovers_from_syn_retransmit_timeout)
{
  mock_cpc_tx_packet_t *dut_syn;
  const sl_cpc_ep_t *ep;
  sl_status_t status;

  status = TEST_CPC_BUS_START();
  TEST_ASSERT_EQUAL(SL_STATUS_OK, status);
  TEST_ASSERT_FALSE(g_bus->initialized);

  // First active-open SYN from primary control; leave it unacked.
  dut_syn = TEST_POP_OUTGOING({
    .flags = CPC_EXPECT_FLAG_ACK_REQ | CPC_EXPECT_FLAG_SYN,
    .len = sizeof(g_bus->ctrl.rx_data),
    .dst = SL_CPC_EP_ID_CONTROL,
    .seq = -1,
    .ack = -1,
    .wnd = -1,
  });
  mock_cpc_tx_packet_free(&dut_syn);
  cpc_test_pump();
  cpc_test_wait_retx_timer();

  // Exhaust the retransmit retries: each expiry retransmits the outstanding SYN.
  for (unsigned int i = 0; i < SLI_CPC_RE_TRANSMIT; i++) {
    TEST_ASSERT_TRUE_MESSAGE(mock_cpc_timer_fire_next(), "Expected retransmit timer to be armed");
    cpc_test_pump();

    TEST_ASSERT_OUTGOING({
      .flags = CPC_EXPECT_FLAG_ACK_REQ | CPC_EXPECT_FLAG_SYN,
      .len = sizeof(g_bus->ctrl.rx_data),
      .dst = SL_CPC_EP_ID_CONTROL,
      .seq = -1,
      .ack = -1,
      .wnd = -1,
    });
    cpc_test_wait_retx_timer();
  }

  // Final expiry: packet_re_transmit_count >= SLI_CPC_RE_TRANSMIT -> TIMEOUT.
  TEST_ASSERT_TRUE_MESSAGE(mock_cpc_timer_fire_next(), "Expected final retransmit timer to be armed");
  // Process timeout: terminate EP and notify ERROR (primary_on_error closes).
  cpc_test_pump();
  // Finish close / re-attach / connect so a fresh SYN is queued.
  cpc_test_pump();

  TEST_ASSERT_FALSE(g_bus->initialized);
  TEST_ASSERT_FALSE(g_bus->ctrl.initialized);

  // Primary control reconnects and completes a fresh SYN handshake.
  complete_primary_control_handshake();

  // Submit standalone ACK left on the TX queue after the handshake.
  cpc_test_pump();
  // Recycle that ACK's TX-complete so the TX pool is clean at tear-down.
  cpc_test_pump();

  ep = sli_cpc_bus_find_ep_from_id(g_bus, SL_CPC_EP_ID_CONTROL);
  TEST_ASSERT_NOT_NULL(ep);
  TEST_ASSERT_EQUAL(SLI_CPC_STATE_CONNECTED, ep->state);
}

/**
 * @brief Primary closes and reconnects when protocol-version negotiation fails.
 *
 * Early init failure path: a failed PROTOCOL_VERSION response at step 1 must
 * close the EP, re-attach, reconnect, and restart the sequence without
 * leaving a stale request_op_id or a half-connected control endpoint.
 */
TEST(cpc_control, primary_recovers_from_version_negotiation_failure)
{
  uint8_t ctrl_payload[sizeof(sli_cpc_control_header_t) + 16];
  const sli_cpc_control_header_t *hdr;
  mock_cpc_tx_packet_t *packet;
  uint16_t request_op_id;
  const sl_cpc_ep_t *ep;
  uint8_t our_syn_seq;
  sl_status_t status;
  uint16_t ctrl_len;
  uint8_t peer_seq;
  uint8_t dut_seq;

  status = TEST_CPC_BUS_START();
  TEST_ASSERT_EQUAL(SL_STATUS_OK, status);
  TEST_ASSERT_FALSE(g_bus->initialized);

  our_syn_seq = complete_primary_control_handshake();
  peer_seq = (uint8_t)(CPC_TEST_PEER_SYN_SEQ + 1U);
  dut_seq = (uint8_t)(our_syn_seq + 1U);

  packet = TEST_POP_CONTROL_PAYLOAD();
  TEST_ASSERT_EQUAL_UINT8(dut_seq, sli_cpc_header_get_seq(&packet->header));
  hdr = control_hdr_from_tx(packet);
  assert_control_type(hdr, SLI_CPC_CTRL_TYPE_PROTOCOL_VERSION, false);
  request_op_id = sli_cpc_control_header_get_op_id(hdr);
  mock_cpc_tx_packet_free(&packet);

  // Peer rejects the version negotiation.
  ctrl_len = pack_control_command(ctrl_payload, sizeof(ctrl_payload),
                                  (uint8_t)(SLI_CPC_CTRL_TYPE_PROTOCOL_VERSION | SLI_CPC_CTRL_TYPE_RESPONSE_FLAG),
                                  request_op_id, SLI_CPC_CTRL_STATUS_NOT_SUPPORTED, NULL, 0);
  TEST_INJECT_FRAME({
    .flags = CPC_EXPECT_FLAG_ACK_REQ,
    .len = ctrl_len,
    .dst = SL_CPC_EP_ID_CONTROL,
    .seq = peer_seq,
    .ack = (int)(dut_seq + 1U),
    .wnd = CPC_TEST_PEER_RX_WND,
    .payload = ctrl_payload,
  });
  // Process the failed response: close control EP and start reconnect.
  cpc_test_pump();
  // Finish close / re-attach / connect so a fresh SYN is queued.
  cpc_test_pump();

  // Drain the ACK flushed on close for the rejected PROTOCOL_VERSION response.
  TEST_ASSERT_OUTGOING({
    .flags = 0,
    .len = 0,
    .dst = SL_CPC_EP_ID_CONTROL,
    .seq = 0,
    .ack = (int)(peer_seq + 1U),
    .wnd = -1,
  });

  TEST_ASSERT_FALSE(g_bus->initialized);
  TEST_ASSERT_FALSE(g_bus->ctrl.initialized);

  // Reconnect and restart the init sequence from PROTOCOL_VERSION.
  our_syn_seq = complete_primary_control_handshake();
  dut_seq = (uint8_t)(our_syn_seq + 1U);

  packet = TEST_POP_CONTROL_PAYLOAD();
  TEST_ASSERT_EQUAL_UINT8(dut_seq, sli_cpc_header_get_seq(&packet->header));
  hdr = control_hdr_from_tx(packet);
  assert_control_type(hdr, SLI_CPC_CTRL_TYPE_PROTOCOL_VERSION, false);
  mock_cpc_tx_packet_free(&packet);

  // Submit standalone ACK left on the TX queue after the handshake/request.
  cpc_test_pump();
  // Recycle that ACK's TX-complete so the TX pool is clean at tear-down.
  cpc_test_pump();

  ep = sli_cpc_bus_find_ep_from_id(g_bus, SL_CPC_EP_ID_CONTROL);
  TEST_ASSERT_NOT_NULL(ep);
  TEST_ASSERT_EQUAL(SLI_CPC_STATE_CONNECTED, ep->state);
  TEST_ASSERT_FALSE(g_bus->initialized);
}

/**
 * @brief Primary closes and reconnects when BUS_ENABLE negotiation fails.
 *
 * Late init failure path: walks the full sequence through BUS_ENABLE, then
 * verifies close → reconnect → restart when the peer rejects bus enable.
 * Complements the early failure covered by version_negotiation_failure.
 */
TEST(cpc_control, primary_recovers_from_bus_enable_failure)
{
  uint8_t ctrl_payload[sizeof(sli_cpc_control_header_t) + 16];
  sli_cpc_protocol_version_response_t version_resp;
  sli_cpc_reset_reason_response_t reset_resp;
  const sli_cpc_control_header_t *hdr;
  mock_cpc_tx_packet_t *packet;
  uint16_t request_op_id;
  const sl_cpc_ep_t *ep;
  uint8_t our_syn_seq;
  sl_status_t status;
  uint16_t ctrl_len;
  uint8_t peer_seq;
  uint8_t dut_seq;

  status = TEST_CPC_BUS_START();
  TEST_ASSERT_EQUAL(SL_STATUS_OK, status);
  TEST_ASSERT_FALSE(g_bus->initialized);

  our_syn_seq = complete_primary_control_handshake();
  peer_seq = (uint8_t)(CPC_TEST_PEER_SYN_SEQ + 1U);
  dut_seq = (uint8_t)(our_syn_seq + 1U);

  packet = TEST_POP_CONTROL_PAYLOAD();
  hdr = control_hdr_from_tx(packet);
  request_op_id = sli_cpc_control_header_get_op_id(hdr);
  mock_cpc_tx_packet_free(&packet);

  version_resp.version_major = SL_CPC_VERSION_MAJOR;
  version_resp.version_minor = SL_CPC_VERSION_MINOR;
  ctrl_len = pack_control_command(ctrl_payload, sizeof(ctrl_payload),
                                  (uint8_t)(SLI_CPC_CTRL_TYPE_PROTOCOL_VERSION | SLI_CPC_CTRL_TYPE_RESPONSE_FLAG),
                                  request_op_id, SLI_CPC_CTRL_STATUS_OK, &version_resp, sizeof(version_resp));
  TEST_INJECT_FRAME({
    .flags = CPC_EXPECT_FLAG_ACK_REQ,
    .len = ctrl_len,
    .dst = SL_CPC_EP_ID_CONTROL,
    .seq = peer_seq,
    .ack = (int)(dut_seq + 1U),
    .wnd = CPC_TEST_PEER_RX_WND,
    .payload = ctrl_payload,
  });
  peer_seq++;
  dut_seq++;
  // Process PROTOCOL_VERSION response and advance to RESET_REASON request.
  cpc_test_pump();

  packet = TEST_POP_CONTROL_PAYLOAD();
  hdr = control_hdr_from_tx(packet);
  request_op_id = sli_cpc_control_header_get_op_id(hdr);
  mock_cpc_tx_packet_free(&packet);

  sli_cpc_u32_to_le(SLI_CPC_RESET_REASON_UNKNOWN, reset_resp.reset_reason_le);
  ctrl_len = pack_control_command(ctrl_payload, sizeof(ctrl_payload),
                                  (uint8_t)(SLI_CPC_CTRL_TYPE_RESET_REASON | SLI_CPC_CTRL_TYPE_RESPONSE_FLAG),
                                  request_op_id, SLI_CPC_CTRL_STATUS_OK, &reset_resp, sizeof(reset_resp));
  TEST_INJECT_FRAME({
    .flags = CPC_EXPECT_FLAG_ACK_REQ,
    .len = ctrl_len,
    .dst = SL_CPC_EP_ID_CONTROL,
    .seq = peer_seq,
    .ack = (int)(dut_seq + 1U),
    .wnd = CPC_TEST_PEER_RX_WND,
    .payload = ctrl_payload,
  });
  peer_seq++;
  dut_seq++;
  // Process RESET_REASON response and advance to PHY_CAPABILITIES request.
  cpc_test_pump();

  packet = TEST_POP_CONTROL_PAYLOAD();
  hdr = control_hdr_from_tx(packet);
  request_op_id = sli_cpc_control_header_get_op_id(hdr);
  mock_cpc_tx_packet_free(&packet);

  ctrl_len = pack_control_command(ctrl_payload, sizeof(ctrl_payload),
                                  (uint8_t)(SLI_CPC_CTRL_TYPE_PHY_CAPABILITIES | SLI_CPC_CTRL_TYPE_RESPONSE_FLAG),
                                  request_op_id, SLI_CPC_CTRL_STATUS_OK, NULL, 0);
  TEST_INJECT_FRAME({
    .flags = CPC_EXPECT_FLAG_ACK_REQ,
    .len = ctrl_len,
    .dst = SL_CPC_EP_ID_CONTROL,
    .seq = peer_seq,
    .ack = (int)(dut_seq + 1U),
    .wnd = CPC_TEST_PEER_RX_WND,
    .payload = ctrl_payload,
  });
  peer_seq++;
  dut_seq++;
  // Process PHY_CAPABILITIES response and advance to BUS_ENABLE request.
  cpc_test_pump();

  packet = TEST_POP_CONTROL_PAYLOAD();
  hdr = control_hdr_from_tx(packet);
  assert_control_type(hdr, SLI_CPC_CTRL_TYPE_BUS_ENABLE, false);
  request_op_id = sli_cpc_control_header_get_op_id(hdr);
  mock_cpc_tx_packet_free(&packet);

  ctrl_len = pack_control_command(ctrl_payload, sizeof(ctrl_payload),
                                  (uint8_t)(SLI_CPC_CTRL_TYPE_BUS_ENABLE | SLI_CPC_CTRL_TYPE_RESPONSE_FLAG),
                                  request_op_id, SLI_CPC_CTRL_STATUS_NOT_SUPPORTED, NULL, 0);
  TEST_INJECT_FRAME({
    .flags = CPC_EXPECT_FLAG_ACK_REQ,
    .len = ctrl_len,
    .dst = SL_CPC_EP_ID_CONTROL,
    .seq = peer_seq,
    .ack = (int)(dut_seq + 1U),
    .wnd = CPC_TEST_PEER_RX_WND,
    .payload = ctrl_payload,
  });
  // Process the failed BUS_ENABLE response: close control EP and start reconnect.
  cpc_test_pump();
  // Finish close / re-attach / connect so a fresh SYN is queued.
  cpc_test_pump();

  // Drain the ACK flushed on close for the rejected BUS_ENABLE response.
  TEST_ASSERT_OUTGOING({
    .flags = 0,
    .len = 0,
    .dst = SL_CPC_EP_ID_CONTROL,
    .seq = 0,
    .ack = (int)(peer_seq + 1U),
    .wnd = -1,
  });

  TEST_ASSERT_FALSE(g_bus->initialized);
  TEST_ASSERT_FALSE(g_bus->ctrl.initialized);

  our_syn_seq = complete_primary_control_handshake();
  dut_seq = (uint8_t)(our_syn_seq + 1U);

  packet = TEST_POP_CONTROL_PAYLOAD();
  TEST_ASSERT_EQUAL_UINT8(dut_seq, sli_cpc_header_get_seq(&packet->header));
  hdr = control_hdr_from_tx(packet);
  assert_control_type(hdr, SLI_CPC_CTRL_TYPE_PROTOCOL_VERSION, false);
  mock_cpc_tx_packet_free(&packet);

  // Submit standalone ACK left on the TX queue after the reconnect handshake.
  cpc_test_pump();
  // Recycle that ACK's TX-complete so the TX pool is clean at tear-down.
  cpc_test_pump();

  ep = sli_cpc_bus_find_ep_from_id(g_bus, SL_CPC_EP_ID_CONTROL);
  TEST_ASSERT_NOT_NULL(ep);
  TEST_ASSERT_EQUAL(SLI_CPC_STATE_CONNECTED, ep->state);
  TEST_ASSERT_FALSE(g_bus->initialized);
}

/**
 * @brief Primary ignores control responses with an unmatched response_op_id during init.
 *
 * Exercises primary_on_response response_op_id filtering: a stale or wrong response
 * is dropped and init continues once the matching response arrives.
 */
TEST(cpc_control, primary_ignores_unmatched_control_response_op_id)
{
  uint8_t ctrl_payload[sizeof(sli_cpc_control_header_t) + 16];
  sli_cpc_protocol_version_response_t version_resp;
  const sli_cpc_control_header_t *hdr;
  mock_cpc_tx_packet_t *packet;
  uint16_t request_op_id;
  uint8_t our_syn_seq;
  sl_status_t status;
  uint16_t ctrl_len;
  uint8_t peer_seq;
  uint8_t dut_seq;

  status = TEST_CPC_BUS_START();
  TEST_ASSERT_EQUAL(SL_STATUS_OK, status);

  our_syn_seq = complete_primary_control_handshake();
  peer_seq = (uint8_t)(CPC_TEST_PEER_SYN_SEQ + 1U);
  dut_seq = (uint8_t)(our_syn_seq + 1U);

  packet = TEST_POP_CONTROL_PAYLOAD();
  hdr = control_hdr_from_tx(packet);
  request_op_id = sli_cpc_control_header_get_op_id(hdr);
  mock_cpc_tx_packet_free(&packet);

  version_resp.version_major = SL_CPC_VERSION_MAJOR;
  version_resp.version_minor = SL_CPC_VERSION_MINOR;
  ctrl_len = pack_control_command(
    ctrl_payload, sizeof(ctrl_payload), (uint8_t)(SLI_CPC_CTRL_TYPE_PROTOCOL_VERSION | SLI_CPC_CTRL_TYPE_RESPONSE_FLAG),
    (uint16_t)(request_op_id + 100U), SLI_CPC_CTRL_STATUS_OK, &version_resp, sizeof(version_resp));
  TEST_INJECT_FRAME({
    .flags = CPC_EXPECT_FLAG_ACK_REQ,
    .len = ctrl_len,
    .dst = SL_CPC_EP_ID_CONTROL,
    .seq = peer_seq,
    .ack = (int)(dut_seq + 1U),
    .wnd = CPC_TEST_PEER_RX_WND,
    .payload = ctrl_payload,
  });
  // Process the unmatched response; primary_on_response ignores it.
  cpc_test_pump();

  packet = TEST_POP_OUTGOING({
    .flags = -1,
    .len = 0,
    .dst = SL_CPC_EP_ID_CONTROL,
    .seq = -1,
    .ack = -1,
    .wnd = -1,
  });
  mock_cpc_tx_packet_free(&packet);

  ctrl_len = pack_control_command(ctrl_payload, sizeof(ctrl_payload),
                                  (uint8_t)(SLI_CPC_CTRL_TYPE_PROTOCOL_VERSION | SLI_CPC_CTRL_TYPE_RESPONSE_FLAG),
                                  request_op_id, SLI_CPC_CTRL_STATUS_OK, &version_resp, sizeof(version_resp));
  TEST_INJECT_FRAME({
    .flags = CPC_EXPECT_FLAG_ACK_REQ,
    .len = ctrl_len,
    .dst = SL_CPC_EP_ID_CONTROL,
    .seq = (int)(peer_seq + 1U),
    .ack = (int)(dut_seq + 1U),
    .wnd = CPC_TEST_PEER_RX_WND,
    .payload = ctrl_payload,
  });
  // Process the matching PROTOCOL_VERSION response and advance to RESET_REASON.
  cpc_test_pump();

  packet = TEST_POP_CONTROL_PAYLOAD();
  hdr = control_hdr_from_tx(packet);
  assert_control_type(hdr, SLI_CPC_CTRL_TYPE_RESET_REASON, false);
  mock_cpc_tx_packet_free(&packet);

  // Submit standalone ACK left on the TX queue after the RESET_REASON request.
  cpc_test_pump();
  // Recycle that ACK's TX-complete so the TX pool is clean at tear-down.
  cpc_test_pump();
}

/**
 * @brief Truncated control commands close the EP and primary reconnects.
 *
 * A header/payload length mismatch triggers command validation, EP close, and
 * primary reconnect. Distinct from RESET-during-SYN recovery and from init
 * step failure paths.
 */
TEST(cpc_control, truncated_control_command_closes_ep_and_primary_reconnects)
{
  uint8_t ctrl_payload[sizeof(sli_cpc_control_header_t)];
  sli_cpc_control_command_t *command = (sli_cpc_control_command_t *)ctrl_payload;
  const sli_cpc_control_header_t *hdr;
  mock_cpc_tx_packet_t *packet;
  uint16_t request_op_id;
  const sl_cpc_ep_t *ep;
  uint8_t our_syn_seq;
  sl_status_t status;
  uint16_t ctrl_len;
  uint8_t peer_seq;
  uint8_t dut_seq;

  status = TEST_CPC_BUS_START();
  TEST_ASSERT_EQUAL(SL_STATUS_OK, status);

  our_syn_seq = complete_primary_control_handshake();
  peer_seq = (uint8_t)(CPC_TEST_PEER_SYN_SEQ + 1U);
  dut_seq = (uint8_t)(our_syn_seq + 1U);

  packet = TEST_POP_CONTROL_PAYLOAD();
  hdr = control_hdr_from_tx(packet);
  request_op_id = sli_cpc_control_header_get_op_id(hdr);
  mock_cpc_tx_packet_free(&packet);
  // Submit standalone ACK left on the TX queue after the PROTOCOL_VERSION request.
  cpc_test_pump();

  memset(ctrl_payload, 0, sizeof(ctrl_payload));
  sli_cpc_control_header_set_payload_size(&command->header, 32U);
  sli_cpc_control_header_set_op_id(&command->header, request_op_id);
  sli_cpc_control_header_set_type(&command->header, SLI_CPC_CTRL_TYPE_PROTOCOL_VERSION);
  sli_cpc_control_header_set_status(&command->header, SLI_CPC_CTRL_STATUS_OK);
  ctrl_len = (uint16_t)sizeof(ctrl_payload);

  TEST_INJECT_FRAME({
    .flags = CPC_EXPECT_FLAG_ACK_REQ,
    .len = ctrl_len,
    .dst = SL_CPC_EP_ID_CONTROL,
    .seq = peer_seq,
    .ack = dut_seq,
    .wnd = CPC_TEST_PEER_RX_WND,
    .payload = ctrl_payload,
  });
  // Process the truncated frame: close control EP and start reconnect.
  cpc_test_pump();
  // Finish close / re-attach / connect so a fresh SYN is queued.
  cpc_test_pump();

  // Drain the ACK flushed on close for the truncated control frame.
  TEST_ASSERT_OUTGOING({
    .flags = 0,
    .len = 0,
    .dst = SL_CPC_EP_ID_CONTROL,
    .seq = 0,
    .ack = (int)(peer_seq + 1U),
    .wnd = -1,
  });

  packet = TEST_POP_OUTGOING({
    .flags = CPC_EXPECT_FLAG_ACK_REQ | CPC_EXPECT_FLAG_SYN,
    .len = sizeof(g_bus->ctrl.rx_data),
    .dst = SL_CPC_EP_ID_CONTROL,
    .seq = -1,
    .ack = -1,
    .wnd = -1,
  });
  our_syn_seq = sli_cpc_header_get_seq(&packet->header);
  mock_cpc_tx_packet_free(&packet);

  TEST_INJECT_FRAME({
    .flags = CPC_EXPECT_FLAG_ACK_REQ | CPC_EXPECT_FLAG_SYN,
    .len = CPC_TEST_PEER_MTU,
    .dst = SL_CPC_EP_ID_CONTROL,
    .seq = CPC_TEST_PEER_SYN_SEQ,
    .ack = (int)(our_syn_seq + 1U),
    .wnd = CPC_TEST_PEER_RX_WND,
  });

  // Process peer SYN-ACK and enter CONNECTED after reconnect.
  cpc_test_pump();

  ep = sli_cpc_bus_find_ep_from_id(g_bus, SL_CPC_EP_ID_CONTROL);
  TEST_ASSERT_NOT_NULL(ep);
  TEST_ASSERT_EQUAL(SLI_CPC_STATE_CONNECTED, ep->state);
  TEST_ASSERT_FALSE(g_bus->initialized);
  TEST_ASSERT_FALSE(g_bus->ctrl.initialized);

  packet = TEST_POP_CONTROL_PAYLOAD();
  mock_cpc_tx_packet_free(&packet);

  // Submit standalone ACK left on the TX queue after the reconnect handshake.
  cpc_test_pump();
  // Recycle that ACK's TX-complete so the TX pool is clean at tear-down.
  cpc_test_pump();
}

/**
 * @brief Secondary on_closed re-listens without emitting a second startup RESET.
 *
 * After RESET on a connected secondary control EP, recovery must re-listen only.
 * A second startup RESET would abort the primary's recovery SYN and stall
 * bring-up in a loop.
 */
TEST(cpc_control, secondary_recovers_without_startup_reset)
{
  mock_cpc_tx_packet_t *dut_syn;
  const sl_cpc_ep_t *ep;
  uint8_t our_syn_seq;
  sl_status_t status;

  cpc_test_set_role_secondary();

  status = TEST_CPC_BUS_START();
  TEST_ASSERT_EQUAL(SL_STATUS_OK, status);

  complete_secondary_control_handshake();

  ep = sli_cpc_bus_find_ep_from_id(g_bus, SL_CPC_EP_ID_CONTROL);
  TEST_ASSERT_NOT_NULL(ep);
  TEST_ASSERT_EQUAL(SLI_CPC_STATE_CONNECTED, ep->state);

  TEST_INJECT_FRAME({
    .flags = CPC_EXPECT_FLAG_RESET,
    .len = 0,
    .dst = SL_CPC_EP_ID_CONTROL,
    .seq = 0,
    .ack = 0,
    .wnd = 0,
  });

  // Process peer RESET: close control EP and re-listen.
  cpc_test_pump();
  // Finish on_closed attach/listen recovery.
  cpc_test_pump();

  TEST_ASSERT_NULL(mock_cpc_drv_pop_tx_packet(sl_cpc_drv_mock_default));

  ep = sli_cpc_bus_find_ep_from_id(g_bus, SL_CPC_EP_ID_CONTROL);
  TEST_ASSERT_NOT_NULL(ep);
  TEST_ASSERT_EQUAL(SLI_CPC_STATE_OPEN, ep->state);
  TEST_ASSERT_FALSE(g_bus->ctrl.initialized);

  TEST_INJECT_FRAME({
    .flags = CPC_EXPECT_FLAG_ACK_REQ | CPC_EXPECT_FLAG_SYN,
    .len = CPC_TEST_PEER_MTU,
    .dst = SL_CPC_EP_ID_CONTROL,
    .seq = CPC_TEST_PEER_SYN_SEQ,
    .ack = 0,
    .wnd = CPC_TEST_PEER_RX_WND,
  });

  dut_syn = TEST_POP_OUTGOING({
    .flags = CPC_EXPECT_FLAG_ACK_REQ | CPC_EXPECT_FLAG_SYN,
    .len = sizeof(g_bus->ctrl.rx_data),
    .dst = SL_CPC_EP_ID_CONTROL,
    .seq = -1,
    .ack = (int)(CPC_TEST_PEER_SYN_SEQ + 1U),
    .wnd = -1,
  });
  our_syn_seq = sli_cpc_header_get_seq(&dut_syn->header);
  mock_cpc_tx_packet_free(&dut_syn);

  TEST_INJECT_FRAME({
    .dst = SL_CPC_EP_ID_CONTROL,
    .seq = (int)(CPC_TEST_PEER_SYN_SEQ + 1U),
    .ack = (int)(our_syn_seq + 1U),
    .wnd = CPC_TEST_PEER_RX_WND,
  });

  // Process peer's final handshake ACK and enter CONNECTED.
  cpc_test_pump();

  ep = sli_cpc_bus_find_ep_from_id(g_bus, SL_CPC_EP_ID_CONTROL);
  TEST_ASSERT_NOT_NULL(ep);
  TEST_ASSERT_EQUAL(SLI_CPC_STATE_CONNECTED, ep->state);

  // Submit standalone ACK left on the TX queue after the handshake.
  cpc_test_pump();
  // Recycle that ACK's TX-complete so the TX pool is clean at tear-down.
  cpc_test_pump();
}

/**
 * @brief Secondary PROTOCOL_VERSION replies with its own version.
 *
 * Empty payload returns INVALID_PARAMETER. A well-formed request with a
 * different major version still returns OK plus the secondary's version;
 * compatibility is the primary's decision. The init sequence tests cover
 * the matching-version exchange.
 */
TEST(cpc_control, secondary_protocol_version_replies_with_own_version)
{
  uint8_t ctrl_payload[sizeof(sli_cpc_control_header_t) + 16];
  sli_cpc_protocol_version_request_t version_req;
  const sli_cpc_control_header_t *hdr;
  mock_cpc_tx_packet_t *packet;
  uint16_t request_op_id = 0;
  uint8_t our_syn_seq;
  sl_status_t status;
  uint16_t ctrl_len;
  uint8_t peer_seq;
  uint8_t dut_seq;

  cpc_test_set_role_secondary();

  status = TEST_CPC_BUS_START();
  TEST_ASSERT_EQUAL(SL_STATUS_OK, status);

  our_syn_seq = complete_secondary_control_handshake();
  peer_seq = (uint8_t)(CPC_TEST_PEER_SYN_SEQ + 1U);
  dut_seq = (uint8_t)(our_syn_seq + 1U);

  ctrl_len = pack_control_command(ctrl_payload, sizeof(ctrl_payload), SLI_CPC_CTRL_TYPE_PROTOCOL_VERSION, request_op_id,
                                  SLI_CPC_CTRL_STATUS_OK, NULL, 0);
  TEST_INJECT_FRAME({
    .flags = CPC_EXPECT_FLAG_ACK_REQ,
    .len = ctrl_len,
    .dst = SL_CPC_EP_ID_CONTROL,
    .seq = peer_seq,
    .ack = dut_seq,
    .wnd = CPC_TEST_PEER_RX_WND,
    .payload = ctrl_payload,
  });
  // Process the invalid empty PROTOCOL_VERSION request and send the response.
  cpc_test_pump();

  packet = TEST_POP_CONTROL_PAYLOAD();
  hdr = control_hdr_from_tx(packet);
  assert_control_type(hdr, SLI_CPC_CTRL_TYPE_PROTOCOL_VERSION, true);
  TEST_ASSERT_EQUAL_UINT8(SLI_CPC_CTRL_STATUS_INVALID_PARAMETER, sli_cpc_control_header_get_status(hdr));
  TEST_ASSERT_EQUAL_UINT8(dut_seq, sli_cpc_header_get_seq(&packet->header));
  mock_cpc_tx_packet_free(&packet);

  TEST_INJECT_FRAME({
    .dst = SL_CPC_EP_ID_CONTROL,
    .seq = peer_seq,
    .ack = (int)(dut_seq + 1U),
    .wnd = CPC_TEST_PEER_RX_WND,
  });
  peer_seq++;
  dut_seq++;
  // ACK the first response so the next reply can be transmitted (wnd=1).
  cpc_test_pump();

  request_op_id++;

  version_req.version_major = SL_CPC_VERSION_MAJOR + 1U;
  version_req.version_minor = SL_CPC_VERSION_MINOR;
  ctrl_len = pack_control_command(ctrl_payload, sizeof(ctrl_payload), SLI_CPC_CTRL_TYPE_PROTOCOL_VERSION, request_op_id,
                                  SLI_CPC_CTRL_STATUS_OK, &version_req, sizeof(version_req));
  TEST_INJECT_FRAME({
    .flags = CPC_EXPECT_FLAG_ACK_REQ,
    .len = ctrl_len,
    .dst = SL_CPC_EP_ID_CONTROL,
    .seq = peer_seq,
    .ack = (int)dut_seq,
    .wnd = CPC_TEST_PEER_RX_WND,
    .payload = ctrl_payload,
  });
  // Process a well-formed request with a different major version.
  cpc_test_pump();

  packet = TEST_POP_CONTROL_PAYLOAD();
  hdr = control_hdr_from_tx(packet);
  assert_control_type(hdr, SLI_CPC_CTRL_TYPE_PROTOCOL_VERSION, true);
  TEST_ASSERT_EQUAL_UINT8(SLI_CPC_CTRL_STATUS_OK, sli_cpc_control_header_get_status(hdr));
  TEST_ASSERT_EQUAL_UINT16(sizeof(sli_cpc_protocol_version_response_t), sli_cpc_control_header_get_payload_size(hdr));
  const sli_cpc_protocol_version_response_t *resp
    = (const sli_cpc_protocol_version_response_t *)((const sli_cpc_control_command_t *)packet->payload)->data;
  TEST_ASSERT_EQUAL_UINT8(SL_CPC_VERSION_MAJOR, resp->version_major);
  TEST_ASSERT_EQUAL_UINT8(SL_CPC_VERSION_MINOR, resp->version_minor);
  mock_cpc_tx_packet_free(&packet);

  // Submit standalone ACK left on the TX queue after the response.
  cpc_test_pump();
  // Recycle that ACK's TX-complete so the TX pool is clean at tear-down.
  cpc_test_pump();
}

/**
 * @brief Secondary answers unknown control requests with NOT_SUPPORTED.
 *
 * Exercises the default branch of secondary_on_request for types outside the
 * init sequence. Primary and secondary use different request dispatch paths.
 */
TEST(cpc_control, secondary_unknown_control_request_responds_not_supported)
{
  uint8_t ctrl_payload[sizeof(sli_cpc_control_header_t) + 16];
  const uint16_t unknown_request_op_id = 42U;
  const sli_cpc_control_header_t *hdr;
  mock_cpc_tx_packet_t *packet;
  uint8_t our_syn_seq;
  sl_status_t status;
  uint16_t ctrl_len;
  uint8_t peer_seq;
  uint8_t dut_seq;

  cpc_test_set_role_secondary();

  status = TEST_CPC_BUS_START();
  TEST_ASSERT_EQUAL(SL_STATUS_OK, status);

  our_syn_seq = complete_secondary_control_handshake();
  peer_seq = (uint8_t)(CPC_TEST_PEER_SYN_SEQ + 1U);
  dut_seq = (uint8_t)(our_syn_seq + 1U);

  ctrl_len = pack_control_command(ctrl_payload, sizeof(ctrl_payload), 0x7FU, unknown_request_op_id,
                                  SLI_CPC_CTRL_STATUS_OK, NULL, 0);
  TEST_INJECT_FRAME({
    .flags = CPC_EXPECT_FLAG_ACK_REQ,
    .len = ctrl_len,
    .dst = SL_CPC_EP_ID_CONTROL,
    .seq = peer_seq,
    .ack = dut_seq,
    .wnd = CPC_TEST_PEER_RX_WND,
    .payload = ctrl_payload,
  });
  // Process the unknown request and send the NOT_SUPPORTED response.
  cpc_test_pump();

  packet = TEST_POP_CONTROL_PAYLOAD();
  hdr = control_hdr_from_tx(packet);
  assert_control_type(hdr, 0x7FU, true);
  TEST_ASSERT_EQUAL_UINT8(SLI_CPC_CTRL_STATUS_NOT_SUPPORTED, sli_cpc_control_header_get_status(hdr));
  TEST_ASSERT_EQUAL_UINT16(unknown_request_op_id, sli_cpc_control_header_get_op_id(hdr));
  mock_cpc_tx_packet_free(&packet);

  // Submit standalone ACK left on the TX queue after the response.
  cpc_test_pump();
  // Recycle that ACK's TX-complete so the TX pool is clean at tear-down.
  cpc_test_pump();
}

/**
 * @brief Primary answers unknown control requests with NOT_SUPPORTED.
 *
 * Exercises primary_on_request while init is still in progress (outstanding
 * PROTOCOL_VERSION). Primary routes all inbound requests through
 * sli_cpc_control_on_unknown_request, unlike the secondary dispatch table.
 */
TEST(cpc_control, unknown_control_request_responds_not_supported)
{
  uint8_t ctrl_payload[sizeof(sli_cpc_control_header_t) + 16];
  const uint16_t unknown_request_op_id = 99U;
  const sli_cpc_control_header_t *hdr;
  mock_cpc_tx_packet_t *packet;
  uint8_t our_syn_seq;
  sl_status_t status;
  uint16_t ctrl_len;
  uint8_t peer_seq;
  uint8_t dut_seq;

  status = TEST_CPC_BUS_START();
  TEST_ASSERT_EQUAL(SL_STATUS_OK, status);

  our_syn_seq = complete_primary_control_handshake();
  peer_seq = (uint8_t)(CPC_TEST_PEER_SYN_SEQ + 1U);
  dut_seq = (uint8_t)(our_syn_seq + 1U);

  packet = TEST_POP_CONTROL_PAYLOAD();
  mock_cpc_tx_packet_free(&packet);

  ctrl_len = pack_control_command(ctrl_payload, sizeof(ctrl_payload), 0x7FU, unknown_request_op_id,
                                  SLI_CPC_CTRL_STATUS_OK, NULL, 0);
  TEST_INJECT_FRAME({
    .flags = CPC_EXPECT_FLAG_ACK_REQ,
    .len = ctrl_len,
    .dst = SL_CPC_EP_ID_CONTROL,
    .seq = peer_seq,
    .ack = (int)(dut_seq + 1U),
    .wnd = CPC_TEST_PEER_RX_WND,
    .payload = ctrl_payload,
  });
  // Process the unknown request and send the NOT_SUPPORTED response.
  cpc_test_pump();

  packet = TEST_POP_CONTROL_PAYLOAD();
  hdr = control_hdr_from_tx(packet);
  assert_control_type(hdr, 0x7FU, true);
  TEST_ASSERT_EQUAL_UINT8(SLI_CPC_CTRL_STATUS_NOT_SUPPORTED, sli_cpc_control_header_get_status(hdr));
  TEST_ASSERT_EQUAL_UINT16(unknown_request_op_id, sli_cpc_control_header_get_op_id(hdr));
  mock_cpc_tx_packet_free(&packet);

  // Submit standalone ACK left on the TX queue after the response.
  cpc_test_pump();
  // Recycle that ACK's TX-complete so the TX pool is clean at tear-down.
  cpc_test_pump();
}

/**
 * @brief Closing the primary control EP aborts connected user endpoints.
 *
 * Active user endpoints are torn down via ERROR (SL_STATUS_ABORT) and enter
 * CLOSING with OPENED still set; the app must close() to reach CLOSED.
 */
TEST(cpc_control, primary_control_closes_connected_user_endpoints)
{
  sl_cpc_ep_t user_ep_a;
  sl_cpc_ep_t user_ep_b;

  start_primary_control_connected();
  g_bus->initialized = true;
  g_bus->ctrl.initialized = true;

  open_connected_user_ep(&user_ep_a, CPC_TEST_EP_ID);
  open_connected_user_ep(&user_ep_b, CPC_TEST_EP_ID + 1U);

  cpc_test_event_reset();
  trigger_control_close_via_reset();

  TEST_ASSERT_FALSE(g_bus->initialized);
  TEST_ASSERT_FALSE(g_bus->ctrl.initialized);
  TEST_ASSERT_EQUAL(SLI_CPC_STATE_CLOSING, user_ep_a.state);
  TEST_ASSERT_TRUE((user_ep_a.flags & SLI_CPC_EP_FLAG_OPENED) != 0);
  TEST_ASSERT_EQUAL(SLI_CPC_STATE_CLOSING, user_ep_b.state);
  TEST_ASSERT_TRUE((user_ep_b.flags & SLI_CPC_EP_FLAG_OPENED) != 0);
  TEST_ASSERT_BIT_HIGH(SL_CPC_EP_EVENT_ERROR, cpc_test_event_mask);
  TEST_ASSERT_BIT_LOW(SL_CPC_EP_EVENT_CLOSED, cpc_test_event_mask);

  sl_cpc_ep_close(&user_ep_a);
  sl_cpc_ep_close(&user_ep_b);
  cpc_test_pump();

  TEST_ASSERT_EQUAL(SLI_CPC_STATE_CLOSED, user_ep_a.state);
  TEST_ASSERT_EQUAL(SLI_CPC_STATE_CLOSED, user_ep_b.state);
  TEST_ASSERT_BIT_HIGH(SL_CPC_EP_EVENT_CLOSED, cpc_test_event_mask);

  finish_primary_control_reconnect();

  cpc_test_ep_discard(&user_ep_a);
  cpc_test_ep_discard(&user_ep_b);
}

/**
 * @brief Closing primary control leaves deferred user endpoints pending.
 *
 * Deferred opens stay CLOSED + USER_OPENED so bus init can complete them after
 * control recovers.
 */
TEST(cpc_control, primary_control_close_leaves_deferred_user_endpoints)
{
  sl_cpc_ep_t user_ep;
  sl_status_t status;

  start_primary_control_connected();
  TEST_ASSERT_FALSE(g_bus->initialized);

  status = sl_cpc_ep_init(&user_ep, CPC_TEST_EP_ID, CPC_TEST_RX_SIZE, cpc_test_event_cb, NULL);
  TEST_ASSERT_EQUAL(SL_STATUS_OK, status);

  status = sl_cpc_ep_listen(&user_ep, g_bus);
  TEST_ASSERT_EQUAL(SL_STATUS_OK, status);
  TEST_ASSERT_EQUAL(SLI_CPC_STATE_CLOSED, user_ep.state);
  TEST_ASSERT_TRUE((user_ep.flags & SLI_CPC_EP_FLAG_OPENED) != 0);

  cpc_test_event_reset();
  trigger_control_close_via_reset();

  TEST_ASSERT_FALSE(g_bus->initialized);
  TEST_ASSERT_FALSE(g_bus->ctrl.initialized);
  TEST_ASSERT_EQUAL(SLI_CPC_STATE_CLOSED, user_ep.state);
  TEST_ASSERT_TRUE((user_ep.flags & SLI_CPC_EP_FLAG_OPENED) != 0);
  TEST_ASSERT_BIT_LOW(SL_CPC_EP_EVENT_CLOSED, cpc_test_event_mask);

  finish_primary_control_reconnect();

  sl_cpc_ep_close(&user_ep);
  // Deliver CLOSED so discard can deinit.
  cpc_test_pump();
  cpc_test_ep_discard(&user_ep);
}

/**
 * @brief Closing the secondary control EP aborts connected user endpoints.
 *
 * Active user endpoints are torn down via ERROR (SL_STATUS_ABORT) and enter
 * CLOSING with OPENED still set; the app must close() to reach CLOSED.
 */
TEST(cpc_control, secondary_control_closes_connected_user_endpoints)
{
  const sl_cpc_ep_t *ctrl_ep;
  sl_cpc_ep_t user_ep;

  start_secondary_control_connected();
  g_bus->initialized = true;
  g_bus->ctrl.initialized = true;

  open_connected_user_ep(&user_ep, CPC_TEST_EP_ID);

  cpc_test_event_reset();
  trigger_control_close_via_reset();

  TEST_ASSERT_FALSE(g_bus->initialized);
  TEST_ASSERT_FALSE(g_bus->ctrl.initialized);
  TEST_ASSERT_EQUAL(SLI_CPC_STATE_CLOSING, user_ep.state);
  TEST_ASSERT_TRUE((user_ep.flags & SLI_CPC_EP_FLAG_OPENED) != 0);
  TEST_ASSERT_BIT_HIGH(SL_CPC_EP_EVENT_ERROR, cpc_test_event_mask);
  TEST_ASSERT_BIT_LOW(SL_CPC_EP_EVENT_CLOSED, cpc_test_event_mask);

  // Secondary re-listens without emitting another startup RESET.
  TEST_ASSERT_NULL(mock_cpc_drv_pop_tx_packet(sl_cpc_drv_mock_default));
  ctrl_ep = sli_cpc_bus_find_ep_from_id(g_bus, SL_CPC_EP_ID_CONTROL);
  TEST_ASSERT_NOT_NULL(ctrl_ep);
  TEST_ASSERT_EQUAL(SLI_CPC_STATE_OPEN, ctrl_ep->state);

  sl_cpc_ep_close(&user_ep);
  cpc_test_pump();

  TEST_ASSERT_EQUAL(SLI_CPC_STATE_CLOSED, user_ep.state);
  TEST_ASSERT_BIT_HIGH(SL_CPC_EP_EVENT_CLOSED, cpc_test_event_mask);

  cpc_test_ep_discard(&user_ep);
}

TEST_GROUP_RUNNER(cpc_control)
{
  RUN_TEST_CASE(cpc_control, primary_init_sequence);
  RUN_TEST_CASE(cpc_control, secondary_init_sequence);
  RUN_TEST_CASE(cpc_control, primary_recovers_from_reset_during_syn);
  RUN_TEST_CASE(cpc_control, primary_recovers_from_syn_retransmit_timeout);
  RUN_TEST_CASE(cpc_control, primary_recovers_from_version_negotiation_failure);
  RUN_TEST_CASE(cpc_control, primary_recovers_from_bus_enable_failure);
  RUN_TEST_CASE(cpc_control, primary_ignores_unmatched_control_response_op_id);
  RUN_TEST_CASE(cpc_control, truncated_control_command_closes_ep_and_primary_reconnects);
  RUN_TEST_CASE(cpc_control, secondary_recovers_without_startup_reset);
  RUN_TEST_CASE(cpc_control, secondary_protocol_version_replies_with_own_version);
  RUN_TEST_CASE(cpc_control, secondary_unknown_control_request_responds_not_supported);
  RUN_TEST_CASE(cpc_control, unknown_control_request_responds_not_supported);
  RUN_TEST_CASE(cpc_control, primary_control_closes_connected_user_endpoints);
  RUN_TEST_CASE(cpc_control, primary_control_close_leaves_deferred_user_endpoints);
  RUN_TEST_CASE(cpc_control, secondary_control_closes_connected_user_endpoints);
}
