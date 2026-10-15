#ifndef EXPECT_H
#define EXPECT_H

#include <stdint.h>

#include "sl_cpc_drv_mock.h"

#include "frame.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Assert a captured CPC header matches `expect` field-for-field.
 *
 * On mismatch the current Unity test is failed with a message naming the
 * first field that did not match.
 *
 * @param lineno Line number of the assert (for Unity failure reports).
 * @param hdr Captured header to inspect. Must not be NULL.
 * @param expect Expected packet description. Must not be NULL.
 */
void test_assert_packet(unsigned int lineno, const sli_cpc_hdr_t *hdr, const cpc_frame_expect_t *expect);

/**
 * @brief Wait for the next outgoing frame on `drv` and match it against `expect`.
 *
 * Pops the next captured packet from the mock driver's TX queue (waiting
 * up to `timeout` units for one to arrive), runs `test_assert_packet()`,
 * then releases the packet.
 *
 * On FreeRTOS builds the timeout is in OS ticks; on baremetal it is the
 * maximum number of `sli_cpc_bus_process_action()` iterations.
 */
void test_assert_outgoing(unsigned int lineno, uint32_t timeout, sl_cpc_drv_mock_t *drv,
                          const cpc_frame_expect_t expect);

/**
 * @brief Same as `test_assert_outgoing()`, but returns the captured packet
 *        to the caller instead of releasing it.
 *
 * Use when a test needs to inspect on-wire fields of the popped packet
 * (e.g. to derive an ACK number from its sequence number). Ownership of
 * the returned packet is transferred to the caller, who must release it
 * with `mock_cpc_tx_packet_free()`.
 */
mock_cpc_tx_packet_t *test_pop_outgoing(unsigned int lineno, uint32_t timeout, sl_cpc_drv_mock_t *drv,
                                        const cpc_frame_expect_t expect);

/**
 * @brief Same as `test_pop_outgoing()`, but skips standalone ACK frames (zero
 *        payload) that may precede a data frame on the control endpoint.
 *
 * At most `timeout` empty frames are discarded before the test fails.
 */
mock_cpc_tx_packet_t *test_pop_outgoing_payload(unsigned int lineno, uint32_t timeout, sl_cpc_drv_mock_t *drv,
                                                const cpc_frame_expect_t expect);

/// Default polling budget for @ref TEST_ASSERT_OUTGOING and @ref TEST_POP_OUTGOING.
#define TEST_ASSERT_OUTGOING_DEFAULT_TIMEOUT 10

/**
 * @brief Assert the next outgoing frame on @p bus matches the expect.
 *
 * @p bus is any CPC bus owned by a mock driver (e.g. @ref CPC_TEST_MOCK_BUS).
 * The mock drv is recovered via @ref sl_cpc_drv_mock_from_bus.
 *
 * Example:
 * @code
 *   TEST_ASSERT_OUTGOING(bus, {
 *     .flags = CPC_EXPECT_FLAG_ACK_REQ | CPC_EXPECT_FLAG_RESET,
 *   });
 * @endcode
 */
#define TEST_ASSERT_OUTGOING(bus, ...)                                                                \
  test_assert_outgoing(__LINE__, TEST_ASSERT_OUTGOING_DEFAULT_TIMEOUT, sl_cpc_drv_mock_from_bus(bus), \
                       (cpc_frame_expect_t)__VA_ARGS__)

/**
 * @brief Wait for, assert, and return the next outgoing frame on @p bus.
 *
 * Mirrors @ref TEST_ASSERT_OUTGOING but yields the captured packet so the
 * caller can inspect it (e.g. to read a sequence number for crafting an
 * injected ACK). The caller owns the returned packet and must free it
 * with `mock_cpc_tx_packet_free()`.
 */
#define TEST_POP_OUTGOING(bus, ...)                                                                \
  test_pop_outgoing(__LINE__, TEST_ASSERT_OUTGOING_DEFAULT_TIMEOUT, sl_cpc_drv_mock_from_bus(bus), \
                    (cpc_frame_expect_t)__VA_ARGS__)

/**
 * @brief Wait for the next non-empty outgoing frame on @p bus, skipping
 *        standalone ACKs.
 *
 * Uses the same timeout budget as @ref TEST_POP_OUTGOING for both waiting
 * on the driver and discarding zero-payload frames.
 */
#define TEST_POP_OUTGOING_PAYLOAD(bus, ...)                                                                \
  test_pop_outgoing_payload(__LINE__, TEST_ASSERT_OUTGOING_DEFAULT_TIMEOUT, sl_cpc_drv_mock_from_bus(bus), \
                            (cpc_frame_expect_t)__VA_ARGS__)

#define TEST_ASSERT_DRV_PENDING_TX(bus, count) \
  TEST_ASSERT_EQUAL(count, mock_cpc_drv_pending_tx_count(sl_cpc_drv_mock_from_bus(bus)))

/**
 * @brief Inject a CPC frame on @p bus as if it had been received from the wire.
 *
 * Takes the same designated-initializer syntax as @ref TEST_ASSERT_OUTGOING:
 * the caller supplies the braces, the macro wraps them into a
 * `cpc_frame_expect_t` compound literal.
 *
 * Example -- acknowledge a frame the test just observed:
 * @code
 *   TEST_INJECT_FRAME(bus, { .ack = 1 });
 * @endcode
 */
#define TEST_INJECT_FRAME(bus, ...) \
  mock_cpc_drv_inject_frame(__LINE__, sl_cpc_drv_mock_from_bus(bus), &(cpc_frame_expect_t)__VA_ARGS__)

#ifdef __cplusplus
}
#endif

#endif // EXPECT_H
