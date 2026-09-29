#ifndef CPC_FIXTURE_H
#define CPC_FIXTURE_H

#include <stdbool.h>
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

/// True after @ref cpc_test_bus_start succeeds; cleared by @ref cpc_test_bus_stop.
extern bool cpc_test_bus_started;

/**
 * @brief Drive the bus forward one scheduling step.
 *
 * On FreeRTOS builds this yields to the CPC task; on baremetal it runs
 * `sli_cpc_bus_process_action()` once.
 */
void cpc_test_pump(void);

/**
 * @brief Yield until the mock retransmit timer is armed.
 *
 * After a reliable frame is transmitted the core may still be wrapping up
 * the TX-completion path. Poll a few times before asserting on the timer
 * queue or firing the timer.
 */
void cpc_test_wait_retx_timer(void);

/**
 * @brief Bring up a fresh bus wired to the mock driver.
 *
 * Resets the virtual timer, initializes the bus kernel and core, and
 * clears @ref cpc_test_event_mask.
 *
 * @param[in] lineno       Call-site line for Unity assertions.
 * @param[in] initialized  Initial @c g_bus->initialized value. Pass true to skip
 *                         control bring-up; false when the test will start the
 *                         bus and drive that sequence.
 */
void cpc_test_setup(unsigned int lineno, bool initialized);

/**
 * @brief Convenience macro to setup test environment.
 */
#define TEST_CPC_SETUP(_initialized) cpc_test_setup(__LINE__, (_initialized))

/**
 * @brief Tear down the bus created by @ref cpc_test_setup.
 */
void cpc_test_tear_down(unsigned int lineno);

/**
 * @brief Convenience macro to tear down test environment.
 */
#define TEST_CPC_TEAR_DOWN() cpc_test_tear_down(__LINE__)

/**
 * @brief Start the bus control endpoint for tests that need the init sequence.
 *
 * Sets @ref cpc_test_bus_started on success so teardown can call
 * @ref cpc_test_bus_stop.
 */
sl_status_t cpc_test_bus_start(unsigned int lineno);

/**
 * @brief Convenience macro: start the bus with failures reported at the call site.
 */
#define TEST_CPC_BUS_START() cpc_test_bus_start(__LINE__)

/**
 * @brief Stop the bus control endpoint started by @ref cpc_test_bus_start.
 *
 * No-op if the bus was not started. Clears @ref cpc_test_bus_started.
 */
void cpc_test_bus_stop(unsigned int lineno);

/**
 * @brief Convenience macro: stop the bus with failures reported at the call site.
 */
#define TEST_CPC_BUS_STOP() cpc_test_bus_stop(__LINE__)

/**
 * @brief Clear @ref cpc_test_event_mask.
 */
void cpc_test_event_reset(void);

/**
 * @brief Bind the mock bus to the primary control ops.
 */
void cpc_test_set_role_primary(void);

/**
 * @brief Bind the mock bus to the secondary control ops.
 */
void cpc_test_set_role_secondary(void);

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
#define TEST_WRITE_PAYLOAD(_ep, _payload, _len) cpc_test_write_payload(__LINE__, (_ep), (_payload), (_len))

/**
 * @brief Register an endpoint and place it directly in the CONNECTED state.
 *
 * Skips the SYN handshake. Intended for tests that exercise post-connect
 * behavior (TX/RX, windows, scatter-gather, etc.) without driving the full
 * connection sequence.
 *
 * @param[in] ep          Initialized endpoint in @c SLI_CPC_STATE_CLOSED.
 * @param[in] bus        Bus to register the endpoint on.
 * @param[in] local_seq   Next local TX sequence (`send_nxt` and `send_una`).
 * @param[in] remote_seq  Next sequence expected from the peer (`ack`).
 * @param[in] local_mtu   Local receive MTU (`mtu`).
 * @param[in] remote_mtu  Peer receive MTU (`remote_mtu`).
 * @param[in] remote_rx_wnd Remote receive window advertised to us (stored in `send_wnd`).
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
