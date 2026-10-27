/***************************************************************************/ /**
 * @file fixture.h
 * @brief Shared fixture helpers for CPC unit tests.
 *******************************************************************************
 * # License
 * <b>Copyright 2026 Silicon Laboratories Inc. www.silabs.com</b>
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

#ifndef CPC_FIXTURE_H
#define CPC_FIXTURE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "../sli_cpc.h"
#include "../sli_cpc_bus.h"

#ifdef __cplusplus
extern "C" {
#endif

#define CPC_TEST_EP_ID 42U
#define CPC_TEST_RX_SIZE 256U

#define CPC_TEST_PEER_SYN_SEQ 100U
#define CPC_TEST_PEER_MTU 128U
#define CPC_TEST_PEER_RX_WND 1U

/// Bitmask of endpoint events observed by @ref cpc_test_event_cb.
extern int cpc_test_event_mask;

/**
 * @brief Drive @p bus forward one scheduling step.
 *
 * FreeRTOS: yield so the bus's CPC task can run. Baremetal: process @p bus once.
 */
void cpc_test_pump(sl_cpc_bus_t *bus);

/**
 * @brief Yield until the mock retransmit timer is armed, pumping @p bus.
 *
 * After a reliable frame is transmitted the core may still be wrapping up
 * the TX-completion path. Poll a few times before asserting on the timer
 * queue or firing the timer.
 */
void cpc_test_wait_retx_timer(sl_cpc_bus_t *bus);

/**
 * @brief Initialize mock instance 0, or every mock when @p all is true.
 *
 * @param[in] lineno       Call-site line for Unity assertions.
 * @param[in] initialized  Initial bus @c initialized flag. Pass true to skip
 *                         control bring-up; false when the test will start the
 *                         bus and drive that sequence.
 * @param[in] all          When true, initialize every entry in
 *                         @c sl_cpc_drv_mock_instances (requires count >= 2).
 */
void cpc_test_setup(unsigned int lineno, bool initialized, bool all);

/**
 * @brief Convenience macro for @ref cpc_test_setup on mock instance 0.
 */
#define TEST_CPC_SETUP(initialized) cpc_test_setup(__LINE__, (initialized), false)

/**
 * @brief Convenience macro for @ref cpc_test_setup on every mock instance.
 */
#define TEST_CPC_MULTI_BUS_SETUP(initialized) cpc_test_setup(__LINE__, (initialized), true)

/**
 * @brief Stop and deinit mock instance 0, or every mock when @p all is true.
 */
void cpc_test_tear_down(unsigned int lineno, bool all);

/**
 * @brief Convenience macro for @ref cpc_test_tear_down on mock instance 0.
 */
#define TEST_CPC_TEAR_DOWN() cpc_test_tear_down(__LINE__, false)

/**
 * @brief Convenience macro for @ref cpc_test_tear_down on every mock instance.
 */
#define TEST_CPC_MULTI_BUS_TEAR_DOWN() cpc_test_tear_down(__LINE__, true)

/**
 * @brief Return the mock bus at @p index in @c sl_cpc_drv_mock_instances.
 */
sl_cpc_bus_t *cpc_test_mock_bus(unsigned int lineno, size_t index);

#define CPC_TEST_MOCK_BUS(index) cpc_test_mock_bus(__LINE__, (index))

/**
 * @brief Start control on @p bus.
 */
sl_status_t cpc_test_bus_start(unsigned int lineno, sl_cpc_bus_t *bus);

#define TEST_CPC_BUS_START(bus) cpc_test_bus_start(__LINE__, (bus))

/**
 * @brief Stop control on @p bus if the fixture started it.
 */
void cpc_test_bus_stop(unsigned int lineno, sl_cpc_bus_t *bus);

#define TEST_CPC_BUS_STOP(bus) cpc_test_bus_stop(__LINE__, (bus))

/**
 * @brief Clear @ref cpc_test_event_mask.
 */
void cpc_test_event_reset(void);

/**
 * @brief Bind @p bus to the primary control ops.
 */
void cpc_test_set_role_primary(sl_cpc_bus_t *bus);

/**
 * @brief Bind @p bus to the secondary control ops.
 */
void cpc_test_set_role_secondary(sl_cpc_bus_t *bus);

/**
 * @brief Record endpoint events into @ref cpc_test_event_mask.
 *
 * Suitable for passing to `sl_cpc_ep_init()`.
 */
void cpc_test_event_cb(sl_cpc_ep_t *ep, sl_cpc_ep_event_type_t type, const sl_cpc_ep_event_t *event, void *arg);

/**
 * @brief Queue a payload on an endpoint via @ref sl_cpc_ep_send.
 *
 * Allocates frame and buffer handles and copies `payload` into the buffer.
 * Failures are attributed to `lineno` (pass `__LINE__` from the test site).
 */
void cpc_test_write_payload(unsigned int lineno, sl_cpc_ep_t *ep, const uint8_t *payload, uint16_t len);

/**
 * @brief Convenience macro: queue a payload with failures reported at the call site.
 */
#define TEST_WRITE_PAYLOAD(ep, payload, len) cpc_test_write_payload(__LINE__, (ep), (payload), (len))

/**
 * @brief Register an endpoint and place it directly in the CONNECTED state.
 *
 * Skips the SYN handshake. Intended for tests that exercise post-connect
 * behavior (TX/RX, windows, scatter-gather, etc.) without driving the full
 * connection sequence.
 *
 * @param[in] ep          Initialized endpoint in @c SLI_CPC_STATE_CLOSED.
 * @param[in] bus        Bus to register the endpoint on.
 * @param[in] local_seq   Next local TX sequence (`send.nxt` and `send.una`).
 * @param[in] remote_seq  Next sequence expected from the peer (`ack`).
 * @param[in] local_mtu   Local receive MTU (`mtu`).
 * @param[in] remote_mtu  Peer receive MTU (`remote_mtu`).
 * @param[in] remote_rx_wnd Remote receive window advertised to us (stored in `send.wnd`).
 *
 * @return @c SL_STATUS_OK on success, or the status returned by
 *         @ref sl_cpc_ep_listen() when registration fails.
 */
sl_status_t cpc_test_set_ep_connected(sl_cpc_ep_t *ep, sl_cpc_bus_t *bus, uint8_t local_seq, uint8_t remote_seq,
                                      uint16_t local_mtu, uint16_t remote_mtu, uint8_t remote_rx_wnd);

/**
 * @brief Tear down an endpoint allocated for a unit test.
 *
 * Stops any running retransmit timer, then deallocates it. Deinit removes the
 * endpoint from the bus list if it is still attached.
 *
 * @param ep Endpoint to discard. Must not be NULL.
 */
void cpc_test_ep_discard(sl_cpc_ep_t *ep);

#ifdef __cplusplus
}
#endif

#endif // CPC_FIXTURE_H
