/*******************************************************************************
 * @file
 * @brief
 *******************************************************************************
 * # License
 * <b>Copyright 2025 Silicon Laboratories Inc. www.silabs.com</b>
 *******************************************************************************
 *
 * SPDX-License-Identifier: Zlib
 *
 * The licensor of this software is Silicon Laboratories Inc.
 *
 * This software is provided 'as-is', without any express or implied
 * warranty. In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 *
 ******************************************************************************/
#include <gtest/gtest.h>
#include <errno.h>
extern "C" {
#include "MQTTSi91x_fake_functions.h"
}

namespace {

int g_recv_errno_to_set  = 0;
int g_recv_sequence_call = 0;

ssize_t recv_fail_with_errno(int /*sock*/, void * /*buf*/, size_t /*len*/, int /*flags*/)
{
  errno = g_recv_errno_to_set;
  return -1;
}

/* First recv delivers 3 bytes; second fails with ETIMEDOUT (idle after partial). */
ssize_t recv_partial_then_idle_timeout(int /*sock*/, void *buf, size_t len, int /*flags*/)
{
  if (g_recv_sequence_call++ == 0) {
    unsigned char *p = static_cast<unsigned char *>(buf);
    const size_t n   = (len < 3) ? len : 3;
    for (size_t i = 0; i < n; ++i) {
      p[i] = static_cast<unsigned char>(0xA0 + i);
    }
    return static_cast<ssize_t>(n);
  }
  errno = ETIMEDOUT;
  return -1;
}

void ResetIdleTimeoutRecvFakes()
{
  RESET_FAKE(recv);
  RESET_FAKE(setsockopt);
  g_recv_errno_to_set      = 0;
  g_recv_sequence_call     = 0;
  setsockopt_fake.return_val = 0;
}

bool ConnectDomainTcpForIdleReadTests(Network *n)
{
  uint8_t addr[4] = { 192, 168, 1, 1 };
  memset(n, 0, sizeof(*n));
  n->transport_type = MQTT_TRANSPORT_TCP;
  NetworkInit(n);

  RESET_FAKE(socket);
  RESET_FAKE(bind);
  RESET_FAKE(connect);
  RESET_FAKE(close);
  socket_fake.return_val  = 7;
  bind_fake.return_val    = 0;
  connect_fake.return_val = 0;

  if (sl_paho_network_connect(n, SL_PAHO_NETWORK_FLAG_IPV4, (char *)addr, 1883, 0, false) != SL_NETWORK_SUCCESS) {
    return false;
  }
  return (n->mqttread != nullptr) && (n->socket == 7);
}

} // namespace

// --- NetworkInit Tests ---
TEST(MQTTSi91x_unit_tests, SetsTCPFunctionPointers)
{
  Network n;
  memset(&n, 0, sizeof(n));
  n.transport_type = MQTT_TRANSPORT_TCP;
  NetworkInit(&n);
  EXPECT_NE(n.mqttread, nullptr);
  EXPECT_NE(n.mqttwrite, nullptr);
  EXPECT_NE(n.disconnect, nullptr);
}

TEST(MQTTSi91x_unit_tests, SetsWebSocketFunctionPointers)
{
  Network n;
  memset(&n, 0, sizeof(n));
  n.transport_type = MQTT_TRANSPORT_WEBSOCKET;
  NetworkInit(&n);
  EXPECT_NE(n.mqttread, nullptr);
  EXPECT_NE(n.mqttwrite, nullptr);
  EXPECT_NE(n.disconnect, nullptr);
}

// --- NetworkConnect Tests ---
TEST(MQTTSi91x_unit_tests, NullNetworkReturnsError)
{
  int rc = NetworkConnect(nullptr, 0, (char *)"host", 1883, 0, false);
  EXPECT_EQ(rc, NETWORK_ERROR_NULL_STRUCTURE);
}

TEST(MQTTSi91x_unit_tests, NullAddressReturnsError)
{
  Network n;
  memset(&n, 0, sizeof(n));
  int rc = NetworkConnect(&n, 0, nullptr, 1883, 0, false);
  EXPECT_EQ(rc, NETWORK_ERROR_NULL_ADDRESS);
}

TEST(MQTTSi91x_unit_tests, TCP_CallsTCPHandler_Effect)
{
  Network n;
  memset(&n, 0, sizeof(n));
  n.transport_type = MQTT_TRANSPORT_TCP;
  int rc           = NetworkConnect(&n, 1, (char *)"host", 1883, 1234, true);
  EXPECT_EQ(rc, 0);
}

TEST(MQTTSi91x_unit_tests, WebSocket_CallsWebSocketHandler)
{
  Network n;
  memset(&n, 0, sizeof(n));
  n.transport_type = MQTT_TRANSPORT_WEBSOCKET;
  int rc           = NetworkConnect(&n, 1, (char *)"host", 1883, 1234, true);
  EXPECT_EQ(rc, 0);
}

TEST(MQTTSi91x_unit_tests, sl_paho_network_connect_WebSocket_Succeeds)
{
  Network n;
  memset(&n, 0, sizeof(n));
  n.transport_type = MQTT_TRANSPORT_WEBSOCKET;
  int rc           = sl_paho_network_connect(&n, 0, (char *)"host", 1883, 1234, true);
  EXPECT_EQ(rc, SL_NETWORK_SUCCESS);
}

TEST(MQTTSi91x_unit_tests, InvalidTypeReturnsError)
{
  Network n;
  memset(&n, 0, sizeof(n));
  n.transport_type = (mqtt_transport_t)0xFF; // Invalid type
  int rc           = NetworkConnect(&n, 0, (char *)"host", 1883, 0, false);
  EXPECT_EQ(rc, NETWORK_ERROR_INVALID_TYPE);
}

// --- Domain error mapping (sl_paho_error_codes.h / sl_paho_network_connect) ---

TEST(MQTTSi91x_unit_tests, sl_paho_map_websocket_error_Success)
{
  EXPECT_EQ(sl_paho_map_websocket_error(0), SL_NETWORK_SUCCESS);
}

TEST(MQTTSi91x_unit_tests, sl_paho_map_websocket_error_KnownWiseConnectCodes)
{
  /* WiseConnect (-N) maps to (-100 - N): -1 -> -101 ... -9 -> -109 */
  EXPECT_EQ(sl_paho_map_websocket_error(-1), SL_NETWORK_WS_ERR_SOCKET_CREATION);
  EXPECT_EQ(sl_paho_map_websocket_error(-2), SL_NETWORK_WS_ERR_SOCKET_BIND);
  EXPECT_EQ(sl_paho_map_websocket_error(-3), SL_NETWORK_WS_ERR_SOCKET_CONNECT);
  EXPECT_EQ(sl_paho_map_websocket_error(-4), SL_NETWORK_WS_ERR_SEND_FRAME);
  EXPECT_EQ(sl_paho_map_websocket_error(-5), SL_NETWORK_WS_ERR_RECEIVE_FRAME);
  EXPECT_EQ(sl_paho_map_websocket_error(-6), SL_NETWORK_WS_ERR_CLOSE_FRAME);
  EXPECT_EQ(sl_paho_map_websocket_error(-7), SL_NETWORK_WS_ERR_SSL_SETSOCKOPT);
  EXPECT_EQ(sl_paho_map_websocket_error(-8), SL_NETWORK_WS_ERR_INVALID_PARAMETER);
  EXPECT_EQ(sl_paho_map_websocket_error(-9), SL_NETWORK_WS_ERR_MAX_LENGTH_EXCEEDED);
}

TEST(MQTTSi91x_unit_tests, sl_paho_map_websocket_error_UnknownReturnsUnknown)
{
  EXPECT_EQ(sl_paho_map_websocket_error(-10), SL_NETWORK_WS_ERR_UNKNOWN);
  EXPECT_EQ(sl_paho_map_websocket_error(1), SL_NETWORK_WS_ERR_UNKNOWN);
  EXPECT_EQ(sl_paho_map_websocket_error(-100), SL_NETWORK_WS_ERR_UNKNOWN);
}

TEST(MQTTSi91x_unit_tests, sl_paho_network_connect_NullNetworkReturnsDomainError)
{
  int rc = sl_paho_network_connect(nullptr, SL_PAHO_NETWORK_FLAG_IPV4, (char *)"host", 1883, 0, false);
  EXPECT_EQ(rc, SL_NETWORK_ERR_NULL_STRUCTURE);
}

TEST(MQTTSi91x_unit_tests, sl_paho_network_connect_NullAddressReturnsDomainError)
{
  Network n;
  memset(&n, 0, sizeof(n));
  n.transport_type = MQTT_TRANSPORT_TCP;
  int rc           = sl_paho_network_connect(&n, SL_PAHO_NETWORK_FLAG_IPV4, nullptr, 1883, 0, false);
  EXPECT_EQ(rc, SL_NETWORK_ERR_NULL_ADDRESS);
}

TEST(MQTTSi91x_unit_tests, sl_paho_network_connect_InvalidTypeReturnsDomainError)
{
  Network n;
  memset(&n, 0, sizeof(n));
  n.transport_type = (mqtt_transport_t)0xFF;
  int rc           = sl_paho_network_connect(&n, SL_PAHO_NETWORK_FLAG_IPV4, (char *)"host", 1883, 0, false);
  EXPECT_EQ(rc, SL_NETWORK_ERR_INVALID_TYPE);
}

TEST(MQTTSi91x_unit_tests, sl_paho_network_connect_InvalidFlagsNeitherOrBoth)
{
  Network n;
  uint8_t addr[4] = { 192, 168, 1, 1 };
  memset(&n, 0, sizeof(n));
  n.transport_type = MQTT_TRANSPORT_TCP;

  int rc_neither = sl_paho_network_connect(&n, 0, (char *)addr, 1883, 0, false);
  EXPECT_EQ(rc_neither, SL_NETWORK_ERR_INVALID_FLAGS);

  int rc_both =
    sl_paho_network_connect(&n, (uint8_t)(SL_PAHO_NETWORK_FLAG_IPV4 | SL_PAHO_NETWORK_FLAG_IPV6), (char *)addr, 1883, 0, false);
  EXPECT_EQ(rc_both, SL_NETWORK_ERR_INVALID_FLAGS);
}

#ifndef SLI_SI91X_ENABLE_IPV6
TEST(MQTTSi91x_unit_tests, sl_paho_network_connect_Ipv6FlagOnIpv4OnlyBuild)
{
  Network n;
  uint8_t addr[16] = { 0 };
  memset(&n, 0, sizeof(n));
  n.transport_type = MQTT_TRANSPORT_TCP;

  int rc = sl_paho_network_connect(&n, SL_PAHO_NETWORK_FLAG_IPV6, (char *)addr, 1883, 0, false);
  EXPECT_EQ(rc, SL_NETWORK_ERR_INVALID_FLAGS);
}
#endif

TEST(MQTTSi91x_unit_tests, sl_paho_network_connect_TcpSocketCreateFailure)
{
  Network n;
  uint8_t addr[4] = { 192, 168, 1, 1 };
  memset(&n, 0, sizeof(n));
  n.transport_type = MQTT_TRANSPORT_TCP;

  RESET_FAKE(socket);
  RESET_FAKE(close);
  socket_fake.return_val = -1;

  int rc = sl_paho_network_connect(&n, SL_PAHO_NETWORK_FLAG_IPV4, (char *)addr, 1883, 0, false);
  EXPECT_EQ(rc, SL_NETWORK_TCP_ERR_SOCKET_CREATE);
  EXPECT_EQ(socket_fake.call_count, 1);
}

TEST(MQTTSi91x_unit_tests, sl_paho_network_connect_TcpConnectFailure)
{
  Network n;
  uint8_t addr[4] = { 192, 168, 1, 1 };
  memset(&n, 0, sizeof(n));
  n.transport_type = MQTT_TRANSPORT_TCP;

  RESET_FAKE(socket);
  RESET_FAKE(bind);
  RESET_FAKE(connect);
  RESET_FAKE(close);
  socket_fake.return_val  = 3;
  bind_fake.return_val    = 0;
  connect_fake.return_val = -1;

  int rc = sl_paho_network_connect(&n, SL_PAHO_NETWORK_FLAG_IPV4, (char *)addr, 1883, 1234, false);
  EXPECT_EQ(rc, SL_NETWORK_TCP_ERR_SOCKET_CONNECT);
  EXPECT_GE(close_fake.call_count, 1);
  EXPECT_EQ(n.socket, -1); /* No stale FD after close on connect failure */
}

TEST(MQTTSi91x_unit_tests, sl_paho_network_connect_TcpBindFailure)
{
  Network n;
  uint8_t addr[4] = { 192, 168, 1, 1 };
  memset(&n, 0, sizeof(n));
  n.transport_type = MQTT_TRANSPORT_TCP;

  RESET_FAKE(socket);
  RESET_FAKE(bind);
  RESET_FAKE(close);
  socket_fake.return_val = 3;
  /* mqtt_tcp_disconnect closes when socket >= 0; bind failure uses mqtt_tcp_disconnect */
  bind_fake.return_val = -1;

  int rc = sl_paho_network_connect(&n, SL_PAHO_NETWORK_FLAG_IPV4, (char *)addr, 1883, 1234, false);
  EXPECT_EQ(rc, SL_NETWORK_TCP_ERR_SOCKET_BIND);
  EXPECT_EQ(n.socket, -1); /* mqtt_tcp_disconnect clears FD after close */
}

TEST(MQTTSi91x_unit_tests, sl_paho_network_connect_WebSocketInitFailureMapped)
{
  Network n;
  memset(&n, 0, sizeof(n));
  n.transport_type = MQTT_TRANSPORT_WEBSOCKET;

  RESET_FAKE(sl_websocket_init);
  /* WiseConnect SL_WEBSOCKET_ERR_SOCKET_CREATION (-1) -> SL_NETWORK_WS_ERR_SOCKET_CREATION (-101) */
  sl_websocket_init_fake.return_val = -1;

  int rc = sl_paho_network_connect(&n, 0, (char *)"host", 1883, 1234, false);
  EXPECT_EQ(rc, SL_NETWORK_WS_ERR_SOCKET_CREATION);
}

TEST(MQTTSi91x_unit_tests, NetworkConnect_WebSocketInitFailureRemainsLegacy)
{
  Network n;
  memset(&n, 0, sizeof(n));
  n.transport_type = MQTT_TRANSPORT_WEBSOCKET;

  RESET_FAKE(sl_websocket_init);
  sl_websocket_init_fake.return_val = -1;

  int rc = NetworkConnect(&n, 0, (char *)"host", 1883, 1234, false);
  EXPECT_EQ(rc, -1); /* Legacy WiseConnect value, not SL_NETWORK_WS_ERR_* */
}

// --- Idle-timeout contract (SI91X-22774): sl_paho_network_connect only ---
// Legacy NetworkConnect / NetworkInit callbacks keep the old idle → -1 behavior.

TEST(MQTTSi91x_unit_tests, DomainTcpRead_IdleEagainReturnsZero)
{
  Network n;
  unsigned char buf[16] = { 0 };
  ASSERT_TRUE(ConnectDomainTcpForIdleReadTests(&n));

  ResetIdleTimeoutRecvFakes();
  recv_fake.custom_fake = recv_fail_with_errno;
  g_recv_errno_to_set   = EAGAIN;

  EXPECT_EQ(n.mqttread(&n, buf, (int)sizeof(buf), 1000), 0);
  EXPECT_GE(recv_fake.call_count, 1);
  EXPECT_EQ(setsockopt_fake.call_count, 1);
}

TEST(MQTTSi91x_unit_tests, DomainTcpRead_IdleEwouldblockReturnsZero)
{
  Network n;
  unsigned char buf[16] = { 0 };
  ASSERT_TRUE(ConnectDomainTcpForIdleReadTests(&n));

  ResetIdleTimeoutRecvFakes();
  recv_fake.custom_fake = recv_fail_with_errno;
  g_recv_errno_to_set   = EWOULDBLOCK;

  EXPECT_EQ(n.mqttread(&n, buf, (int)sizeof(buf), 1000), 0);
}

TEST(MQTTSi91x_unit_tests, DomainTcpRead_IdleEtimedoutReturnsZero)
{
  Network n;
  unsigned char buf[16] = { 0 };
  ASSERT_TRUE(ConnectDomainTcpForIdleReadTests(&n));

  ResetIdleTimeoutRecvFakes();
  recv_fake.custom_fake = recv_fail_with_errno;
  g_recv_errno_to_set   = ETIMEDOUT;

  EXPECT_EQ(n.mqttread(&n, buf, (int)sizeof(buf), 1000), 0);
}

TEST(MQTTSi91x_unit_tests, DomainTcpRead_HardErrorReturnsMinusOne)
{
  Network n;
  unsigned char buf[16] = { 0 };
  ASSERT_TRUE(ConnectDomainTcpForIdleReadTests(&n));

  ResetIdleTimeoutRecvFakes();
  recv_fake.custom_fake = recv_fail_with_errno;
  g_recv_errno_to_set   = ECONNABORTED;

  EXPECT_EQ(n.mqttread(&n, buf, (int)sizeof(buf), 1000), -1);
}

TEST(MQTTSi91x_unit_tests, DomainTcpRead_PartialThenIdleTimeoutReturnsBytesSoFar)
{
  Network n;
  unsigned char buf[16] = { 0 };
  ASSERT_TRUE(ConnectDomainTcpForIdleReadTests(&n));

  ResetIdleTimeoutRecvFakes();
  recv_fake.custom_fake = recv_partial_then_idle_timeout;

  EXPECT_EQ(n.mqttread(&n, buf, (int)sizeof(buf), 1000), 3);
  EXPECT_EQ(buf[0], 0xA0);
  EXPECT_EQ(buf[1], 0xA1);
  EXPECT_EQ(buf[2], 0xA2);
  EXPECT_EQ(recv_fake.call_count, 2);
}

TEST(MQTTSi91x_unit_tests, LegacyTcpRead_IdleEtimedoutRemainsMinusOne)
{
  Network n;
  unsigned char buf[16] = { 0 };
  memset(&n, 0, sizeof(n));
  n.transport_type = MQTT_TRANSPORT_TCP;
  NetworkInit(&n);
  n.socket = 7;

  ResetIdleTimeoutRecvFakes();
  recv_fake.custom_fake = recv_fail_with_errno;
  g_recv_errno_to_set   = ETIMEDOUT;

  EXPECT_EQ(n.mqttread(&n, buf, (int)sizeof(buf), 1000), -1);
}

TEST(MQTTSi91x_unit_tests, LegacyWsRead_IdleEtimedoutRemainsMinusOne)
{
  Network n;
  unsigned char buf[16] = { 0 };
  memset(&n, 0, sizeof(n));
  n.transport_type = MQTT_TRANSPORT_WEBSOCKET;
  NetworkInit(&n);
  n.socket = 7;

  ResetIdleTimeoutRecvFakes();
  recv_fake.custom_fake = recv_fail_with_errno;
  g_recv_errno_to_set   = ETIMEDOUT;

  EXPECT_EQ(n.mqttread(&n, buf, (int)sizeof(buf), 1000), -1);
}

TEST(MQTTSi91x_unit_tests, LegacyWsRead_HardErrorReturnsMinusOne)
{
  Network n;
  unsigned char buf[16] = { 0 };
  memset(&n, 0, sizeof(n));
  n.transport_type = MQTT_TRANSPORT_WEBSOCKET;
  NetworkInit(&n);
  n.socket = 7;

  ResetIdleTimeoutRecvFakes();
  recv_fake.custom_fake = recv_fail_with_errno;
  g_recv_errno_to_set   = ECONNABORTED;

  EXPECT_EQ(n.mqttread(&n, buf, (int)sizeof(buf), 1000), -1);
}

TEST(MQTTSi91x_unit_tests, DomainWsRead_IdleEtimedoutReturnsZero)
{
  Network n;
  unsigned char buf[16] = { 0 };
  memset(&n, 0, sizeof(n));
  n.transport_type = MQTT_TRANSPORT_WEBSOCKET;

  RESET_FAKE(sl_websocket_init);
  RESET_FAKE(sli_websocket_set_subprotocol);
  RESET_FAKE(sli_websocket_connect_sync);
  sl_websocket_init_fake.return_val             = 0;
  sli_websocket_set_subprotocol_fake.return_val = 0;
  sli_websocket_connect_sync_fake.return_val    = 0;

  ASSERT_EQ(sl_paho_network_connect(&n, 0, (char *)"host", 1883, 1234, false), SL_NETWORK_SUCCESS);
  ASSERT_NE(n.mqttread, nullptr);

  ResetIdleTimeoutRecvFakes();
  recv_fake.custom_fake = recv_fail_with_errno;
  g_recv_errno_to_set   = ETIMEDOUT;

  EXPECT_EQ(n.mqttread(&n, buf, (int)sizeof(buf), 1000), 0);
}

TEST(MQTTSi91x_unit_tests, DomainWsRead_HardErrorReturnsWsReceiveFrame)
{
  Network n;
  unsigned char buf[16] = { 0 };
  memset(&n, 0, sizeof(n));
  n.transport_type = MQTT_TRANSPORT_WEBSOCKET;

  RESET_FAKE(sl_websocket_init);
  RESET_FAKE(sli_websocket_set_subprotocol);
  RESET_FAKE(sli_websocket_connect_sync);
  sl_websocket_init_fake.return_val             = 0;
  sli_websocket_set_subprotocol_fake.return_val = 0;
  sli_websocket_connect_sync_fake.return_val    = 0;

  ASSERT_EQ(sl_paho_network_connect(&n, 0, (char *)"host", 1883, 1234, false), SL_NETWORK_SUCCESS);
  ASSERT_NE(n.mqttread, nullptr);

  ResetIdleTimeoutRecvFakes();
  recv_fake.custom_fake = recv_fail_with_errno;
  g_recv_errno_to_set   = ECONNABORTED;

  EXPECT_EQ(n.mqttread(&n, buf, (int)sizeof(buf), 1000), SL_NETWORK_WS_ERR_RECEIVE_FRAME);
}

// --- NetworkDisconnect Tests ---
TEST(MQTTSi91x_unit_tests, NullNetworkPointer_DoesNothing)
{
  NetworkDisconnect(nullptr); // No assertion needed, just ensure no crash
}

TEST(MQTTSi91x_unit_tests, NullDisconnectPointer_DoesNothing)
{
  Network n    = {};
  n.disconnect = nullptr;
  NetworkDisconnect(&n); // No assertion needed, just ensure no crash
}

TEST(MQTTSi91x_unit_tests, CallsDisconnectFunction)
{
  Network n    = {};
  n.disconnect = mqtt_fake_disconnect;
  RESET_FAKE(mqtt_fake_disconnect);
  NetworkDisconnect(&n);
  EXPECT_EQ(mqtt_fake_disconnect_fake.call_count, 1);
  EXPECT_EQ(mqtt_fake_disconnect_fake.arg0_val, &n);
}