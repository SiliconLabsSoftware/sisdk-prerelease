/***************************************************************************/ /**
 * @file flow_control.c
 * @brief RX frame-pool flow-control tests for sl_cpc_drv_uart.
 *
 * The UART test configuration provides a fixed RX frame pool. These tests keep
 * the pool frames unreleased, then inject one additional frame. The driver must hold
 * the additional frame's complete header instead of accepting it without an RX
 * frame, and must resume reception when the CPC core releases a frame through
 * on_rx_frame_free().
 *
 * Test coverage:
 * - resumes_max_payload_after_rx_frame_is_freed: resumes and preserves a maximum payload.
 * - resumes_empty_payload_after_rx_frame_is_freed: resumes a header-only frame.
 * - invalid_header_does_not_consume_rx_frame: discards a bad header without consuming a frame.
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

#include <unity_fixture.h>

#include "sl_cpc_drv_uart_eusart.h"
#include "sl_cpc_drv_uart_eusart_vcom_config.h"

#include "harness/uart.h"

#define FLOW_CONTROL_FRAME_COUNT (SL_CPC_DRV_UART_EUSART_VCOM_RX_FRAME_POOL_COUNT + 1U)

static const uint8_t s_payloads[FLOW_CONTROL_FRAME_COUNT][2] = {
  {0x10U, 0x11U}, {0x20U, 0x21U}, {0x30U, 0x31U}, {0x40U, 0x41U}, {0x50U, 0x51U},
};

static cpc_frame_expect_t make_frame(size_t index)
{
  return (cpc_frame_expect_t){
    .flags = CPC_EXPECT_FLAG_ACK_REQ,
    .dst = TEST_UART_EP_ID,
    .seq = 1,
    .ack = 0,
    .wnd = 1,
    .len = sizeof(s_payloads[index]),
    .payload = s_payloads[index],
  };
}

static void inject_frames_until_full(uint8_t *wire)
{
  for (size_t i = 0; i < FLOW_CONTROL_FRAME_COUNT - 1U; i++) {
    cpc_frame_expect_t frame = make_frame(i);

    uart_inject_frame(wire, TEST_UART_WIRE_BUFFER_SIZE, frame);
  }
}

static size_t encode_frame(uint8_t *wire, size_t index)
{
  cpc_frame_expect_t frame = make_frame(index);

  return encode_uart_frame(wire, TEST_UART_WIRE_BUFFER_SIZE, &frame);
}

static void corrupt_header_crc(uint8_t *wire)
{
  size_t offset = 1U + SLI_CPC_HEADER_SIZE;

  wire[offset] ^= 0xFFU;
  wire[offset + 1U] ^= 0xFFU;
}

TEST_GROUP(cpc_drv_uart_flow_control);

TEST_SETUP(cpc_drv_uart_flow_control)
{
  uart_setup();
}

TEST_TEAR_DOWN(cpc_drv_uart_flow_control)
{
  uart_teardown();
}

/**
 * @brief Resume a maximum-size payload after the RX pool is filled.
 *
 * The fifth frame uses the largest test payload to verify that flow-control
 * recovery preserves payload data.
 */
TEST(cpc_drv_uart_flow_control, resumes_max_payload_after_rx_frame_is_freed)
{
  uint8_t max_payload[TEST_UART_MAX_TEST_PAYLOAD_LENGTH];
  uint8_t wire[TEST_UART_WIRE_BUFFER_SIZE];
  cpc_frame_expect_t frame = make_frame(FLOW_CONTROL_FRAME_COUNT - 1U);
  size_t wire_length;

  for (size_t i = 0; i < sizeof(max_payload); i++) {
    max_payload[i] = (uint8_t)i;
  }

  inject_frames_until_full(wire);

  frame.len = sizeof(max_payload);
  frame.payload = max_payload;
  wire_length = encode_uart_frame(wire, sizeof(wire), &frame);
  uart_inject_bytes(wire, TEST_UART_HEADER_BLOCK_SIZE);

  TEST_ASSERT_TRUE(sl_cpc_drv_uart_eusart_instances[0].rx.waiting_for_frame);
  TEST_UART_READ_EXPECT(TEST_UART_EP_ID, s_payloads[0], sizeof(s_payloads[0]), true);
  uart_inject_bytes(wire + TEST_UART_HEADER_BLOCK_SIZE, wire_length - TEST_UART_HEADER_BLOCK_SIZE);

  TEST_UART_READ_EXPECT(TEST_UART_EP_ID, s_payloads[1], sizeof(s_payloads[1]), true);
  TEST_UART_READ_EXPECT(TEST_UART_EP_ID, s_payloads[2], sizeof(s_payloads[2]), true);
  TEST_UART_READ_EXPECT(TEST_UART_EP_ID, s_payloads[3], sizeof(s_payloads[3]), true);
  TEST_UART_READ_EXPECT(TEST_UART_EP_ID, max_payload, sizeof(max_payload), true);
  TEST_UART_READ_EXPECT_EMPTY();
}

/**
 * @brief Resume a header-only frame after the RX frame pool is filled.
 *
 * The configured RX frame pool fills. The next frame's header waits for
 * on_rx_frame_free(), then the header-only frame is delivered.
 */
TEST(cpc_drv_uart_flow_control, resumes_empty_payload_after_rx_frame_is_freed)
{
  uint8_t wire[TEST_UART_WIRE_BUFFER_SIZE];
  cpc_frame_expect_t frame = make_frame(FLOW_CONTROL_FRAME_COUNT - 1U);

  frame.len = 0U;
  frame.payload = NULL;

  inject_frames_until_full(wire);
  (void)encode_uart_frame(wire, sizeof(wire), &frame);
  uart_inject_bytes(wire, TEST_UART_HEADER_BLOCK_SIZE);

  TEST_ASSERT_TRUE(sl_cpc_drv_uart_eusart_instances[0].rx.waiting_for_frame);
  TEST_UART_READ_EXPECT(TEST_UART_EP_ID, s_payloads[0], sizeof(s_payloads[0]), true);
  TEST_UART_READ_EXPECT(TEST_UART_EP_ID, s_payloads[1], sizeof(s_payloads[1]), true);
  TEST_UART_READ_EXPECT(TEST_UART_EP_ID, s_payloads[2], sizeof(s_payloads[2]), true);
  TEST_UART_READ_EXPECT(TEST_UART_EP_ID, s_payloads[3], sizeof(s_payloads[3]), true);
  TEST_UART_READ_EXPECT(TEST_UART_EP_ID, NULL, 0U, false);
  TEST_UART_READ_EXPECT_EMPTY();
}

/**
 * @brief Discard an invalid header without consuming an RX frame.
 *
 * A malformed header must not consume one of the pool frames. The
 * following valid frame must still wait for on_rx_frame_free() and
 * arrive successfully.
 */
TEST(cpc_drv_uart_flow_control, invalid_header_does_not_consume_rx_frame)
{
  uint8_t wire[TEST_UART_WIRE_BUFFER_SIZE];
  size_t wire_length;

  inject_frames_until_full(wire);

  wire_length = encode_frame(wire, FLOW_CONTROL_FRAME_COUNT - 1U);
  corrupt_header_crc(wire);
  uart_inject_bytes(wire, TEST_UART_HEADER_BLOCK_SIZE);
  TEST_ASSERT_FALSE(sl_cpc_drv_uart_eusart_instances[0].rx.waiting_for_frame);

  wire_length = encode_frame(wire, FLOW_CONTROL_FRAME_COUNT - 1U);
  uart_inject_bytes(wire, TEST_UART_HEADER_BLOCK_SIZE);

  TEST_ASSERT_TRUE(sl_cpc_drv_uart_eusart_instances[0].rx.waiting_for_frame);
  TEST_UART_READ_EXPECT(TEST_UART_EP_ID, s_payloads[0], sizeof(s_payloads[0]), true);
  uart_inject_bytes(wire + TEST_UART_HEADER_BLOCK_SIZE, wire_length - TEST_UART_HEADER_BLOCK_SIZE);

  TEST_UART_READ_EXPECT(TEST_UART_EP_ID, s_payloads[1], sizeof(s_payloads[1]), true);
  TEST_UART_READ_EXPECT(TEST_UART_EP_ID, s_payloads[2], sizeof(s_payloads[2]), true);
  TEST_UART_READ_EXPECT(TEST_UART_EP_ID, s_payloads[3], sizeof(s_payloads[3]), true);
  TEST_UART_READ_EXPECT(TEST_UART_EP_ID, s_payloads[4], sizeof(s_payloads[4]), true);
  TEST_UART_READ_EXPECT_EMPTY();
}

TEST_GROUP_RUNNER(cpc_drv_uart_flow_control)
{
  RUN_TEST_CASE(cpc_drv_uart_flow_control, resumes_max_payload_after_rx_frame_is_freed);
  RUN_TEST_CASE(cpc_drv_uart_flow_control, resumes_empty_payload_after_rx_frame_is_freed);
  RUN_TEST_CASE(cpc_drv_uart_flow_control, invalid_header_does_not_consume_rx_frame);
}
