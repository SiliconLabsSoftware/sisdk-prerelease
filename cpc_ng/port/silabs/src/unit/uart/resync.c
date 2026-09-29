/***************************************************************************/ /**
 * @file
 * @brief Header resync and preamble search tests for sl_cpc_drv_uart.
 ******************************************************************************/

#include <string.h>

#include <unity_fixture.h>

#include "harness/uart.h"

#define inject_prefix_and_frame_split(_prefix, _prefix_length, _frame, _stream_split) \
  inject_prefix_and_frame_split_at(__LINE__, (_prefix), (_prefix_length), (_frame), (_stream_split))

// Build a preamble-aligned block with an invalid header CRC (0x0000 vs 0xFF header).
static void fill_invalid_header_block(uint8_t *block)
{
  block[0] = TEST_UART_PREAMBLE;
  memset(&block[1], 0xFF, SLI_CPC_HEADER_SIZE);
  block[1U + SLI_CPC_HEADER_SIZE] = 0x00U;
  block[1U + SLI_CPC_HEADER_SIZE + 1U] = 0x00U;
}

// Concatenate prefix bytes and a valid frame, then inject in two parts at stream_split.
static void inject_prefix_and_frame_split_at(unsigned int lineno, const uint8_t *prefix, size_t prefix_length,
                                             cpc_frame_expect_t frame, size_t stream_split)
{
  uint8_t stream[TEST_UART_STREAM_BUFFER_SIZE];
  uint8_t wire[TEST_UART_WIRE_BUFFER_SIZE];
  size_t stream_length;
  size_t wire_length;

  wire_length = encode_uart_frame(wire, sizeof(wire), &frame);
  UNITY_TEST_ASSERT(wire_length > 0U, lineno, "inject_prefix_and_frame_split: encode_uart_frame failed");

  stream_length = prefix_length + wire_length;
  UNITY_TEST_ASSERT(stream_length <= sizeof(stream), lineno, "inject_prefix_and_frame_split: stream buffer overflow");
  UNITY_TEST_ASSERT(stream_split <= stream_length, lineno, "inject_prefix_and_frame_split: invalid stream_split");

  if (prefix_length > 0U) {
    memcpy(stream, prefix, prefix_length);
  }

  memcpy(stream + prefix_length, wire, wire_length);

  uart_inject_bytes(stream, stream_split);
  uart_inject_bytes(stream + stream_split, stream_length - stream_split);
}

// Inject two encoded frames back-to-back on the same byte stream.
static void inject_frames_back_to_back(cpc_frame_expect_t frame_a, cpc_frame_expect_t frame_b)
{
  uint8_t wire_a[TEST_UART_WIRE_BUFFER_SIZE];
  uint8_t wire_b[TEST_UART_WIRE_BUFFER_SIZE];
  size_t len_a;
  size_t len_b;

  len_a = encode_uart_frame(wire_a, sizeof(wire_a), &frame_a);
  len_b = encode_uart_frame(wire_b, sizeof(wire_b), &frame_b);

  uart_inject_bytes(wire_a, len_a);
  uart_inject_bytes(wire_b, len_b);
}

TEST_GROUP(cpc_drv_uart_resync);

TEST_SETUP(cpc_drv_uart_resync)
{
  uart_setup();
}

TEST_TEAR_DOWN(cpc_drv_uart_resync)
{
  uart_teardown();
}

/**
 * @brief A single well-formed frame is parsed and delivered unchanged.
 */
TEST(cpc_drv_uart_resync, valid_frame)
{
  const uint8_t payload[] = "hello";
  cpc_frame_expect_t frame = {
    .flags = CPC_EXPECT_FLAG_ACK_REQ,
    .dst = TEST_UART_EP_ID,
    .seq = 1,
    .ack = 0,
    .wnd = 1,
    .len = sizeof(payload) - 1U,
    .payload = payload,
  };
  uint8_t wire[TEST_UART_WIRE_BUFFER_SIZE];

  uart_inject_frame(wire, sizeof(wire), frame);

  TEST_UART_READ_EXPECT(TEST_UART_EP_ID, payload, sizeof(payload) - 1U, true);
}

/**
 * @brief Lone invalid header block must not deliver a frame.
 */
TEST(cpc_drv_uart_resync, invalid_header_block_discarded)
{
  uint8_t block[TEST_UART_HEADER_BLOCK_SIZE];

  fill_invalid_header_block(block);
  uart_inject_bytes(block, sizeof(block));

  TEST_UART_READ_EXPECT_EMPTY();
}

/**
 * @brief Invalid header block split across injects must not deliver.
 */
TEST(cpc_drv_uart_resync, invalid_header_block_split_inject_discarded)
{
  uint8_t block[TEST_UART_HEADER_BLOCK_SIZE];
  size_t first;

  fill_invalid_header_block(block);
  first = TEST_UART_HEADER_BLOCK_SIZE / 2U;
  uart_inject_bytes(block, first);
  uart_inject_bytes(block + first, TEST_UART_HEADER_BLOCK_SIZE - first);

  TEST_UART_READ_EXPECT_EMPTY();
}

/**
 * @brief Preamble plus invalid header/CRC block, then a valid frame on the same stream.
 */
TEST(cpc_drv_uart_resync, invalid_header_block_then_valid_frame)
{
  const uint8_t payload[] = {0x01U, 0x02U, 0x03U};
  uint8_t prefix[TEST_UART_HEADER_BLOCK_SIZE];

  cpc_frame_expect_t frame = {
    .flags = CPC_EXPECT_FLAG_ACK_REQ,
    .dst = TEST_UART_EP_ID,
    .seq = 1,
    .ack = 0,
    .wnd = 1,
    .len = sizeof(payload),
    .payload = payload,
  };
  fill_invalid_header_block(prefix);
  uart_inject_prefix_and_frame(prefix, sizeof(prefix), frame);

  TEST_UART_READ_EXPECT(TEST_UART_EP_ID, payload, sizeof(payload), true);
}

/**
 * @brief Invalid header block split across injects, then a valid frame on the same stream.
 */
TEST(cpc_drv_uart_resync, invalid_header_block_split_inject_then_valid_frame)
{
  const uint8_t payload[] = {0x55U, 0x66U};
  uint8_t prefix[TEST_UART_HEADER_BLOCK_SIZE];

  cpc_frame_expect_t frame = {
    .flags = CPC_EXPECT_FLAG_ACK_REQ,
    .dst = TEST_UART_EP_ID,
    .seq = 1,
    .ack = 0,
    .wnd = 1,
    .len = sizeof(payload),
    .payload = payload,
  };
  fill_invalid_header_block(prefix);
  inject_prefix_and_frame_split(prefix, sizeof(prefix), frame, TEST_UART_HEADER_BLOCK_SIZE / 2U);

  TEST_UART_READ_EXPECT(TEST_UART_EP_ID, payload, sizeof(payload), true);
}

/**
 * @brief Invalid header block in the first inject, valid frame in the second.
 */
TEST(cpc_drv_uart_resync, invalid_header_block_split_at_boundary_then_valid_frame)
{
  const uint8_t payload[] = {0x44U, 0x55U};
  uint8_t prefix[TEST_UART_HEADER_BLOCK_SIZE];

  cpc_frame_expect_t frame = {
    .flags = CPC_EXPECT_FLAG_ACK_REQ,
    .dst = TEST_UART_EP_ID,
    .seq = 1,
    .ack = 0,
    .wnd = 1,
    .len = sizeof(payload),
    .payload = payload,
  };
  fill_invalid_header_block(prefix);
  inject_prefix_and_frame_split(prefix, sizeof(prefix), frame, TEST_UART_HEADER_BLOCK_SIZE);

  TEST_UART_READ_EXPECT(TEST_UART_EP_ID, payload, sizeof(payload), true);
}

/**
 * @brief Invalid header block, then valid frame split inside the frame header block.
 */
TEST(cpc_drv_uart_resync, invalid_header_block_split_inside_frame_then_valid_frame)
{
  const uint8_t payload[] = {0x66U, 0x77U};
  uint8_t prefix[TEST_UART_HEADER_BLOCK_SIZE];

  cpc_frame_expect_t frame = {
    .flags = CPC_EXPECT_FLAG_ACK_REQ,
    .dst = TEST_UART_EP_ID,
    .seq = 1,
    .ack = 0,
    .wnd = 1,
    .len = sizeof(payload),
    .payload = payload,
  };
  fill_invalid_header_block(prefix);
  inject_prefix_and_frame_split(prefix, sizeof(prefix), frame, sizeof(prefix) + TEST_UART_HEADER_BLOCK_SIZE / 2U);

  TEST_UART_READ_EXPECT(TEST_UART_EP_ID, payload, sizeof(payload), true);
}

/**
 * @brief Two invalid header blocks, then a valid frame; only the last frame is delivered.
 */
TEST(cpc_drv_uart_resync, invalid_header_block_twice_then_valid_frame)
{
  uint8_t prefix[2U * TEST_UART_HEADER_BLOCK_SIZE];
  const uint8_t payload[] = {0x10U, 0x20U};

  cpc_frame_expect_t frame = {
    .flags = CPC_EXPECT_FLAG_ACK_REQ,
    .dst = TEST_UART_EP_ID,
    .seq = 1,
    .ack = 0,
    .wnd = 1,
    .len = sizeof(payload),
    .payload = payload,
  };
  fill_invalid_header_block(prefix);
  fill_invalid_header_block(prefix + TEST_UART_HEADER_BLOCK_SIZE);
  uart_inject_prefix_and_frame(prefix, sizeof(prefix), frame);

  TEST_UART_READ_EXPECT(TEST_UART_EP_ID, payload, sizeof(payload), true);
}

/**
 * @brief Invalid header block followed by two valid frames; both frames must arrive.
 */
TEST(cpc_drv_uart_resync, invalid_header_block_then_two_valid_frames)
{
  const uint8_t payload_b[] = {0x0CU, 0x0DU, 0x0EU};
  const uint8_t payload_a[] = {0x0AU, 0x0BU};
  uint8_t prefix[TEST_UART_HEADER_BLOCK_SIZE];

  cpc_frame_expect_t frame_a = {
    .flags = CPC_EXPECT_FLAG_ACK_REQ,
    .dst = TEST_UART_EP_ID,
    .seq = 1,
    .ack = 0,
    .wnd = 1,
    .len = sizeof(payload_a),
    .payload = payload_a,
  };
  cpc_frame_expect_t frame_b = {
    .flags = CPC_EXPECT_FLAG_ACK_REQ,
    .dst = TEST_UART_EP_ID,
    .seq = 1,
    .ack = 0,
    .wnd = 1,
    .len = sizeof(payload_b),
    .payload = payload_b,
  };
  fill_invalid_header_block(prefix);
  uart_inject_bytes(prefix, sizeof(prefix));
  inject_frames_back_to_back(frame_a, frame_b);

  TEST_UART_READ_EXPECT(TEST_UART_EP_ID, payload_a, sizeof(payload_a), true);
  TEST_UART_READ_EXPECT(TEST_UART_EP_ID, payload_b, sizeof(payload_b), true);
  TEST_UART_READ_EXPECT_EMPTY();
}

/**
 * @brief Partial false sync (not a full header block), then a valid frame.
 */
TEST(cpc_drv_uart_resync, false_preamble_then_valid_frame)
{
  const uint8_t prefix[] = {TEST_UART_PREAMBLE, 0x00U, 0x00U};
  const uint8_t payload[] = {0xAAU, 0xBBU};

  cpc_frame_expect_t frame = {
    .flags = CPC_EXPECT_FLAG_ACK_REQ,
    .dst = TEST_UART_EP_ID,
    .seq = 1,
    .ack = 0,
    .wnd = 1,
    .len = sizeof(payload),
    .payload = payload,
  };
  uart_inject_prefix_and_frame(prefix, sizeof(prefix), frame);

  TEST_UART_READ_EXPECT(TEST_UART_EP_ID, payload, sizeof(payload), true);
}

/**
 * @brief Partial false sync split across injects, then a valid frame.
 */
TEST(cpc_drv_uart_resync, false_preamble_split_inject_then_valid_frame)
{
  const uint8_t prefix[] = {TEST_UART_PREAMBLE, 0x00U, 0x00U};
  const uint8_t payload[] = {0x99U, 0xAAU};

  cpc_frame_expect_t frame = {
    .flags = CPC_EXPECT_FLAG_ACK_REQ,
    .dst = TEST_UART_EP_ID,
    .seq = 1,
    .ack = 0,
    .wnd = 1,
    .len = sizeof(payload),
    .payload = payload,
  };
  inject_prefix_and_frame_split(prefix, sizeof(prefix), frame, 1U);

  TEST_UART_READ_EXPECT(TEST_UART_EP_ID, payload, sizeof(payload), true);
}

/**
 * @brief Non-preamble bytes before the valid frame; driver realigns on the frame preamble.
 */
TEST(cpc_drv_uart_resync, garbage_prefix_then_valid_frame)
{
  const uint8_t payload[] = {0x5AU, 0x5BU};
  const uint8_t prefix[] = {0x12U, 0x34U};

  cpc_frame_expect_t frame = {
    .flags = CPC_EXPECT_FLAG_ACK_REQ,
    .dst = TEST_UART_EP_ID,
    .seq = 1,
    .ack = 0,
    .wnd = 1,
    .len = sizeof(payload),
    .payload = payload,
  };
  uart_inject_prefix_and_frame(prefix, sizeof(prefix), frame);

  TEST_UART_READ_EXPECT(TEST_UART_EP_ID, payload, sizeof(payload), true);
}

/**
 * @brief Full header block of garbage with no preamble byte, split across injects, then a valid frame.
 */
TEST(cpc_drv_uart_resync, garbage_prefix_split_inject_then_valid_frame)
{
  const uint8_t payload[] = {0x77U, 0x88U};
  uint8_t prefix[TEST_UART_HEADER_BLOCK_SIZE];

  cpc_frame_expect_t frame = {
    .flags = CPC_EXPECT_FLAG_ACK_REQ,
    .dst = TEST_UART_EP_ID,
    .seq = 1,
    .ack = 0,
    .wnd = 1,
    .len = sizeof(payload),
    .payload = payload,
  };
  memset(prefix, 0x00U, sizeof(prefix));
  inject_prefix_and_frame_split(prefix, sizeof(prefix), frame, TEST_UART_HEADER_BLOCK_SIZE / 2U);

  TEST_UART_READ_EXPECT(TEST_UART_EP_ID, payload, sizeof(payload), true);
}

/**
 * @brief Invalid header block after leading garbage, then a valid frame.
 */
TEST(cpc_drv_uart_resync, invalid_header_block_after_garbage_then_valid_frame)
{
  uint8_t prefix[2U + TEST_UART_HEADER_BLOCK_SIZE];
  const uint8_t payload[] = {0x2AU, 0x2BU};

  memset(prefix, 0xAAU, sizeof(prefix));
  fill_invalid_header_block(prefix + 2U);
  cpc_frame_expect_t frame = {
    .flags = CPC_EXPECT_FLAG_ACK_REQ,
    .dst = TEST_UART_EP_ID,
    .seq = 1,
    .ack = 0,
    .wnd = 1,
    .len = sizeof(payload),
    .payload = payload,
  };
  uart_inject_prefix_and_frame(prefix, sizeof(prefix), frame);

  TEST_UART_READ_EXPECT(TEST_UART_EP_ID, payload, sizeof(payload), true);
}

/**
 * @brief Spurious sync byte inside a garbage header block, then a valid frame.
 */
TEST(cpc_drv_uart_resync, garbage_prefix_spurious_preamble_then_valid_frame)
{
  const uint8_t payload[] = {0x3CU, 0x3DU};
  uint8_t prefix[TEST_UART_HEADER_BLOCK_SIZE];

  cpc_frame_expect_t frame = {
    .flags = CPC_EXPECT_FLAG_ACK_REQ,
    .dst = TEST_UART_EP_ID,
    .seq = 1,
    .ack = 0,
    .wnd = 1,
    .len = sizeof(payload),
    .payload = payload,
  };
  memset(prefix, 0x00U, sizeof(prefix));
  prefix[1U] = TEST_UART_PREAMBLE;
  uart_inject_prefix_and_frame(prefix, sizeof(prefix), frame);

  TEST_UART_READ_EXPECT(TEST_UART_EP_ID, payload, sizeof(payload), true);
}

/**
 * @brief Valid frame is parsed and delivered unchanged.
 */
TEST(cpc_drv_uart_resync, valid_header_sync)
{
  uint8_t wire[TEST_UART_WIRE_BUFFER_SIZE];
  const uint8_t payload[] = "hello";

  cpc_frame_expect_t frame = {
    .flags = CPC_EXPECT_FLAG_ACK_REQ,
    .dst = TEST_UART_EP_ID,
    .seq = 1,
    .ack = 0,
    .wnd = 1,
    .len = sizeof(payload) - 1U,
    .payload = payload,
  };
  uart_inject_frame(wire, sizeof(wire), frame);

  TEST_UART_READ_EXPECT(TEST_UART_EP_ID, payload, sizeof(payload) - 1U, true);
}

/**
 * @brief Header block split across injects without prior garbage still delivers.
 */
TEST(cpc_drv_uart_resync, valid_header_sync_split_inject)
{
  const uint8_t payload[] = {0x01U, 0x02U, 0x03U};

  cpc_frame_expect_t frame = {
    .flags = CPC_EXPECT_FLAG_ACK_REQ,
    .dst = TEST_UART_EP_ID,
    .seq = 1,
    .ack = 0,
    .wnd = 1,
    .len = sizeof(payload),
    .payload = payload,
  };
  uart_inject_frame_in_two_parts(frame);

  TEST_UART_READ_EXPECT(TEST_UART_EP_ID, payload, sizeof(payload), true);
}

TEST_GROUP_RUNNER(cpc_drv_uart_resync)
{
  RUN_TEST_CASE(cpc_drv_uart_resync, valid_frame);
  RUN_TEST_CASE(cpc_drv_uart_resync, invalid_header_block_discarded);
  RUN_TEST_CASE(cpc_drv_uart_resync, invalid_header_block_split_inject_discarded);
  RUN_TEST_CASE(cpc_drv_uart_resync, invalid_header_block_then_valid_frame);
  RUN_TEST_CASE(cpc_drv_uart_resync, invalid_header_block_split_inject_then_valid_frame);
  RUN_TEST_CASE(cpc_drv_uart_resync, invalid_header_block_split_at_boundary_then_valid_frame);
  RUN_TEST_CASE(cpc_drv_uart_resync, invalid_header_block_split_inside_frame_then_valid_frame);
  RUN_TEST_CASE(cpc_drv_uart_resync, invalid_header_block_twice_then_valid_frame);
  RUN_TEST_CASE(cpc_drv_uart_resync, invalid_header_block_after_garbage_then_valid_frame);
  RUN_TEST_CASE(cpc_drv_uart_resync, invalid_header_block_then_two_valid_frames);
  RUN_TEST_CASE(cpc_drv_uart_resync, false_preamble_then_valid_frame);
  RUN_TEST_CASE(cpc_drv_uart_resync, false_preamble_split_inject_then_valid_frame);
  RUN_TEST_CASE(cpc_drv_uart_resync, garbage_prefix_then_valid_frame);
  RUN_TEST_CASE(cpc_drv_uart_resync, garbage_prefix_split_inject_then_valid_frame);
  RUN_TEST_CASE(cpc_drv_uart_resync, garbage_prefix_spurious_preamble_then_valid_frame);
  RUN_TEST_CASE(cpc_drv_uart_resync, valid_header_sync);
  RUN_TEST_CASE(cpc_drv_uart_resync, valid_header_sync_split_inject);
}
