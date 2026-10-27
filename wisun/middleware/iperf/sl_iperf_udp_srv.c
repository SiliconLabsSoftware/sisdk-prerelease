/***************************************************************************//**
 * @file sl_iperf_udp_srv.c
 * @brief iPerf UDP server
 *******************************************************************************
 * # License
 * <b>Copyright 2022 Silicon Laboratories Inc. www.silabs.com</b>
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

// -----------------------------------------------------------------------------
//                                   Includes
// -----------------------------------------------------------------------------
#include <string.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "sl_assert.h"
#include "cmsis_os2.h"
#include "sl_iperf_types.h"
#include "sl_iperf_network_interface.h"
#include "sl_iperf.h"
#include "sl_iperf_util.h"
#include "sl_iperf_udp_srv.h"
#include "socket/socket.h"
#include "arpa/inet.h"
#include "sl_sleeptimer.h"
#include "sl_wisun_types.h"
#include "netinet/in.h"

// -----------------------------------------------------------------------------
//                              Macros and Typedefs
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
//                          Static Function Declarations
// -----------------------------------------------------------------------------

/**************************************************************************//**
 * @brief iPerf UDP FINACK packet send
 * @details Assembling and sending final ack packet
 * @param[in,out] test Test
 *****************************************************************************/
static void _iperf_udp_finack(sl_iperf_test_t * const test);

/**************************************************************************//**
 * @brief iPerf get timestamp from packet
 * @details get timestamp from packet and convert to millisec
 * @param[in] dtg Datagram header
 * @return sl_iperf_ts_ms_t Converted timestamp in ms
 *****************************************************************************/
__STATIC_INLINE sl_iperf_ts_ms_t _get_ms_ts_from_clnt_header(const sl_iperf_udp_clnt_hdr_t * const hdr);

/**************************************************************************//**
 * @brief iPerf get absolute delta time
 * @details Helper function for Jitter calculation
 * @param[in] test Test
 * @return uint32_t Delta time value
 *****************************************************************************/
static uint32_t _calc_jitter_abs_delta(const sl_iperf_test_t * const test);

/**************************************************************************//**
 * @brief Jitter calculation
 * @details Calculate inter arrival estimated jitter based on RFC 1889
 * @param[in,out] test Test
 * @param[in] pkt_id Received packet ID
 *****************************************************************************/
static void _iperf_calc_jitter(sl_iperf_test_t * const test, const int32_t pkt_id);

static int32_t _set_multicast_sock_opt(const int32_t sockid,
                                       const sockaddr_in6_t * const multicast_addr,
                                       const sl_wisun_multicast_group_action_t action);

// -----------------------------------------------------------------------------
//                                Static Variables
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
//                          Public Function Definitions
// -----------------------------------------------------------------------------

static void status_timer_callback(sl_sleeptimer_timer_handle_t *handle, void *data)
{
  (void)handle;
  (void)data;
  sl_iperf_status_update();
}

// UDP Server test
void sl_iperf_test_udp_server(sl_iperf_test_t * test)
{
  int32_t r                             = SOCKET_RETVAL_ERROR;
  sl_iperf_udp_clnt_hdr_t *clnt_hdr     = NULL;
  int32_t pkt_id                        = 0;
  sl_iperf_time_t time                  = { 0U };
  static sockaddr_in6_t mc_addr = { 0U };
  sl_sleeptimer_timer_handle_t status_timer = { 0U };
  socklen_t addrlen                     = sizeof(sockaddr_in6_t);
  sockaddr_in6_t *srv_addr              = &test->conn.srv_addr;

  // reset error, set status queued
  sl_iperf_test_set_err_and_stat(test, SL_IPERF_ERR_NONE,
                                 SL_IPERF_TEST_STATUS_QUEUED);
  sl_iperf_test_log_verbose(test, "UDP Server: started.\n");

  test->statistic.ts_start_ms = (sl_iperf_ts_ms_t)sl_sleeptimer_tick_to_ms(sl_sleeptimer_get_tick_count());

  if (test->conn.buff == NULL || !test->conn.buff_size) {
    sl_iperf_test_log_verbose(test, "UDP Server: connection buffer error.\n");
    return;
  }

  test->conn.socket_id  = socket(AF_INET6, SOCK_DGRAM | SOCK_NONBLOCK, IPPROTO_UDP);
  if (test->conn.socket_id  == SOCKET_RETVAL_ERROR) {
    sl_iperf_test_set_err_and_stat(test, SL_IPERF_ERR_SERVER_SOCK_OPEN, SL_IPERF_TEST_STATUS_ERR);
    sl_iperf_test_log(test, "UDP Server: sl_iperf_socket_create failed.\n");
    return;
  }
  sl_iperf_test_log_verbose(test, "UDP Server: socket created.\n");

  if (!test->opt.port) {
    test->opt.port = SL_IPERF_SERVER_DEFAULT_PORT;
  }

  sl_iperf_test_log(test, "Server listening on UDP port %u\n", test->opt.port);
  sl_iperf_test_log(test, "UDP buffer size: %lu bytes\n", test->conn.buff_size);

  srv_addr->sin6_family = AF_INET6;
  srv_addr->sin6_addr = in6addr_any;
  srv_addr->sin6_port = htons(test->opt.port);
  setsockopt(test->conn.socket_id,
             SOL_SOCKET,
             SO_RCVBUF,
             &test->opt.socket_buffer_size,
             sizeof(uint32_t));

  r = bind(test->conn.socket_id, (const struct sockaddr *)&test->conn.srv_addr, sizeof(sockaddr_in6_t));
  if (r == SOCKET_RETVAL_ERROR) {
    sl_iperf_test_set_err_and_stat(test, SL_IPERF_ERR_SERVER_SOCK_BIND, SL_IPERF_TEST_STATUS_ERR);
    sl_iperf_test_log(test, "UDP Server: bind failed.\n");
    close(test->conn.socket_id);
    return;
  }
  sl_iperf_test_log_verbose(test, "UDP Server: bind done.\n");

  // Join multicast group if it is required
  if (test->opt.multicast) {
    inet_pton(AF_INET6, test->opt.remote_addr, &mc_addr.sin6_addr);
    r = _set_multicast_sock_opt(test->conn.socket_id, &mc_addr, (sl_wisun_multicast_group_action_t)IPV6_JOIN_GROUP);
    if (r == SOCKET_RETVAL_ERROR) {
      sl_iperf_test_log(test, "UDP Server: joining to multicast group failed.\n");
      close(test->conn.socket_id);
      return;
    }
  } else {
    (void) mc_addr;
  }

  test->statistic.nbr_rcv_snt_packets = 0U;

  while (sl_iperf_test_check_time(test)) {
    r = recvfrom(test->conn.socket_id,
                 test->conn.buff,
                 test->conn.buff_size,
                 0U,
                 (struct sockaddr *) &test->conn.clnt_addr,
                 &addrlen);
    // Empty buff or error
    if (!r || r == SOCKET_RETVAL_ERROR) {
      osDelay(2);
      continue;
    }

    sl_iperf_stats_lock();

    // store previous timestamps
    test->statistic.ts_prev_recv_ms = test->statistic.ts_curr_recv_ms;
    test->statistic.ts_prev_sent_ms = test->statistic.ts_curr_sent_ms;

    // get current time stamp
    test->statistic.ts_curr_recv_ms = (sl_iperf_ts_ms_t)sl_sleeptimer_tick_to_ms(sl_sleeptimer_get_tick_count());

    if (!test->statistic.nbr_rcv_snt_packets++) {
      // Reset status on first packet reception.
      sl_iperf_test_update_status(test, true);
      if (test->opt.interval_ms > 0U) {
        sl_sleeptimer_start_periodic_timer_ms(&status_timer, test->opt.interval_ms, status_timer_callback, test, 0, 0);
      }
    }
    test->statistic.nbr_calls++;
    // store currently received packet counts, update resets it
    test->statistic.last_recv_pkt_cnt++;
    test->statistic.bytes += r;
    if (test->conn.run == false) {
      test->statistic.ts_start_ms = test->statistic.ts_curr_recv_ms;
      test->statistic.ts_end_ms = 0;
      test->conn.run = true;
    }

    clnt_hdr = (sl_iperf_udp_clnt_hdr_t *) test->conn.buff;
#if SL_IPERF_VERBOSE_MODE
    sl_iperf_print_test_clnt_header_json(test, clnt_hdr);
#endif
    // get sent timestamp
    test->statistic.ts_curr_sent_ms = _get_ms_ts_from_clnt_header(clnt_hdr);

    pkt_id = ntohl(clnt_hdr->dtg.id);
    sl_iperf_test_log_verbose(test, "UDP Server: packet received. pkt_id = %d (%d bytes).\n", pkt_id, r);

    _iperf_calc_jitter(test, pkt_id);

    // 1. First packet received
    if (test->statistic.nbr_rcv_snt_packets == 1U) {
      sl_iperf_test_print_udp_conn_str(test);
      sl_iperf_test_print_udp_srv_report_hdr(test);
      sl_iperf_test_log_verbose(test, "UDP Server: first packet received.\n");
      test->statistic.udp_rx_last_pkt = pkt_id;
    }
    // 2. Received old upd fin ack (discard packet)
    else if (test->statistic.nbr_rcv_snt_packets == 1U && pkt_id < 0) {
      sl_iperf_test_log_verbose(test, "UDP Server: Received old upd fin ack (discard packet).\n");
      test->statistic.nbr_rcv_snt_packets = 0U;
    }
    // 3. Received end packet
    else if (pkt_id < 0) {
      sl_iperf_test_log_verbose(test, "UDP Server: Received end packet.\n");
      test->statistic.ts_end_ms = (sl_iperf_ts_ms_t)sl_sleeptimer_tick_to_ms(sl_sleeptimer_get_tick_count());
      test->conn.run = false;
      sl_sleeptimer_stop_timer(&status_timer);
      sl_iperf_stats_unlock();
      _iperf_udp_finack(test);
      break;
    }
    // 4. Packet out of order
    else if (pkt_id != (test->statistic.udp_rx_last_pkt + 1)
             && test->statistic.nbr_rcv_snt_packets > 1U) {
      if (pkt_id < test->statistic.udp_rx_last_pkt + 1) {
        sl_iperf_test_log_verbose(test, "UDP Server: Packet out of order.\n");
        // Packet out of order
        ++test->statistic.udp_out_of_order;
      } else {
        // Packet lost
        sl_iperf_test_log_verbose(test, "UDP Server: Packet lost.\n");
        test->statistic.udp_lost_pkt += (pkt_id - (test->statistic.udp_rx_last_pkt + 1U));
      }
    }
    test->statistic.udp_rx_last_pkt = pkt_id;

    sl_iperf_stats_unlock();
  }

  // calculate total packets
  test->statistic.tot_packets = test->statistic.nbr_rcv_snt_packets + test->statistic.udp_lost_pkt;

  if (test->conn.run || !test->statistic.tot_packets) {
    test->statistic.ts_end_ms = (sl_iperf_ts_ms_t)sl_sleeptimer_tick_to_ms(sl_sleeptimer_get_tick_count());
    test->conn.run = false;
    sl_sleeptimer_stop_timer(&status_timer);
    if (test->statistic.tot_packets) {
      _iperf_udp_finack(test);
    }
  }

  // calculate final band width
  sl_iperf_test_calculate_average_bandwidth(test);

  sl_iperf_test_log_verbose(test, "UDP Server: close socket.\n");
  sl_iperf_calc_time_from_ms(&time, sl_iperf_test_calc_time_duration_ms(test));
  sl_iperf_test_log_verbose(test, "UDP Server: Done (Time: %lu.%.4lus)\n", time.sec, time.usec);

  // leave multicast group if it is required
  if (test->opt.multicast) {
    r = _set_multicast_sock_opt(test->conn.socket_id, &mc_addr, (sl_wisun_multicast_group_action_t)IPV6_LEAVE_GROUP);
    if (r == SOCKET_RETVAL_ERROR) {
      sl_iperf_test_log(test, "UDP Server: leaving multicast group failed.\n");
    }
  }

  // close socket
  r = close(test->conn.socket_id);
  if (r == SOCKET_RETVAL_ERROR) {
    sl_iperf_test_set_err_and_stat(test, SL_IPERF_ERR_SERVER_SOCK_CLOSE, SL_IPERF_TEST_STATUS_ERR);
  } else {
    sl_iperf_test_set_err_and_stat(test, SL_IPERF_ERR_NONE, SL_IPERF_TEST_STATUS_DONE);
  }
}

// -----------------------------------------------------------------------------
//                          Static Function Definitions
// -----------------------------------------------------------------------------

static void _iperf_udp_finack(sl_iperf_test_t * const test)
{
  static sl_iperf_udp_srv_hdr_t hdr = { 0 };
  sl_iperf_time_t time              = { 0U };
  int32_t r                         = SOCKET_RETVAL_ERROR;
  socklen_t addrlen                 = sizeof(sockaddr_in6_t);

  // assert minimum buff size for fin ack
  EFM_ASSERT(test->conn.buff_size >= SL_IPERF_UDP_SERVER_FIN_ACK_SIZE);

  if (test->conn.buff == NULL || !test->conn.buff_size) {
    sl_iperf_test_log(test, "UDP Server: error, connection buffer is NULL.\n");
    return;
  }

  // reset datagram
  memset(&hdr.dtg, 0, sizeof(sl_iperf_udp_datagram_t));
  hdr.flags           = htonl(SL_IPERF_HEADER_VERSION1);
  hdr.tot_len_u        = 0U;
  hdr.tot_len_l        = htonl(test->statistic.bytes);
  sl_iperf_calc_time_from_ms(&time, sl_iperf_test_calc_time_duration_ms(test));
  hdr.stop_sec         = htonl(time.sec);
  hdr.stop_usec        = htonl(time.usec);
  hdr.lost_pkt_cnt     = htonl(test->statistic.udp_lost_pkt);
  hdr.out_of_order_cnt = htonl(test->statistic.udp_out_of_order);
  hdr.packet_cnt       = htonl(test->statistic.nbr_rcv_snt_packets + test->statistic.udp_lost_pkt);
  sl_iperf_calc_time_from_ms(&time, (sl_iperf_ts_ms_t)(test->statistic.udp_jitter));
  hdr.jitter_sec       = htonl(time.sec);  // sec
  hdr.jitter_usec      = htonl(time.usec); // usec

#if SL_IPERF_VERBOSE_MODE
  sl_iperf_print_test_srv_header_json(test, &hdr);
#endif

  for (uint32_t i = 0; i < SL_IPERF_SERVER_UDP_TX_FINACK_COUNT; ++i) {
    memcpy(test->conn.buff, &hdr, sizeof(sl_iperf_udp_srv_hdr_t));

    if (sendto(test->conn.socket_id,
               test->conn.buff,
               SL_IPERF_UDP_SERVER_FIN_ACK_SIZE,
               0U,
               (const struct sockaddr *) &test->conn.clnt_addr,
               sizeof(sockaddr_in6_t)) == SOCKET_RETVAL_ERROR) {
      sl_iperf_test_log_verbose(test, "UDP Server: FINACK send error.\n");
      continue;
    }
    osDelay(1000UL);
    r = recvfrom(test->conn.socket_id,
                 test->conn.buff,
                 test->conn.buff_size,
                 0U,
                 (struct sockaddr *) &test->conn.clnt_addr,
                 &addrlen);
    if (r <= 0) {
      break;
    }
  }

  sl_iperf_test_log_verbose(test, "UDP Server: FINACK has been sent.\n");
}

__STATIC_INLINE sl_iperf_ts_ms_t _get_ms_ts_from_clnt_header(const sl_iperf_udp_clnt_hdr_t * const hdr)
{
  return ntohl(hdr->dtg.time_var_sec) * SL_IPERF_TIME_S_TO_MS_ML
         + ntohl(hdr->dtg.time_var_usec) / SL_IPERF_TIME_MS_TO_US_ML;
}

static uint32_t _calc_jitter_abs_delta(const sl_iperf_test_t * const test)
{
  int64_t sender_d   = 0L;
  int64_t receiver_d = 0L;
  int64_t delta      = 0L;

  // Signed arithmetic: a reordered packet gives a negative sender delta,
  // which would wrap if computed on unsigned types.
  sender_d   = (int64_t)(test->statistic.ts_curr_sent_ms - test->statistic.ts_prev_sent_ms);
  receiver_d = (int64_t)(test->statistic.ts_curr_recv_ms - test->statistic.ts_prev_recv_ms);
  delta      = receiver_d - sender_d;

  return (uint32_t)(delta < 0L ? -delta : delta);
}

static void _iperf_calc_jitter(sl_iperf_test_t * const test, const int32_t pkt_id)
{
  // Jitter calculation by RFC-1889
  // Estimated jitter type is int64_t
  // Time stamps format is ms
  // D(i,j)=(Rj-Ri)-(Sj-Si)=(Rj-Sj)-(Ri-Si)
  // J=J+(|D(i-1,i)|-J)/16

  uint32_t abs_d = 0;
  int64_t v1 = 0;

  // skip if there is not previous packet info
  if (test->statistic.nbr_rcv_snt_packets <= 1) {
    return;
  }

  // Only consecutive packets carry a meaningful transit time delta.
  // Skips reordered/lost sequences and the negative ID end packet.
  if (pkt_id != (test->statistic.udp_rx_last_pkt + 1)) {
    return;
  }

  abs_d = _calc_jitter_abs_delta(test);
  v1 = ((int64_t)abs_d - test->statistic.udp_jitter);
  v1 = ((v1 + 8L) >> 4UL);
  test->statistic.udp_jitter += v1;
}

static int32_t _set_multicast_sock_opt(const int32_t sockid,
                                       const sockaddr_in6_t * const multicast_addr,
                                       const sl_wisun_multicast_group_action_t action)
{
  int32_t r = SOCKET_RETVAL_ERROR;

  #ifdef __GNUC__
  #pragma GCC diagnostic push
  #pragma GCC diagnostic ignored "-Wpragmas"
  #pragma GCC diagnostic ignored "-Waddress-of-packed-member"
  #elif defined __ICCARM__
  #pragma diag_suppress=Pa039
  #endif

  SL_ALIGN(4) sl_wisun_socket_option_data_t opt SL_ATTRIBUTE_ALIGN(4) =
  {
    0U
  };

  memcpy(&opt.ipv6_address, &multicast_addr->sin6_addr, sizeof(in6_addr_t));

  r = setsockopt(sockid, SOL_SOCKET, action, &opt, sizeof(opt));

  #ifdef __GNUC__
  #pragma GCC diagnostic pop
  #elif defined __ICCARM__
  #pragma diag_default=Pa039
  #endif

  return r;
}
