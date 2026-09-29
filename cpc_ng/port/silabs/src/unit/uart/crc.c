/***************************************************************************/ /**
 * @file
 * @brief Header and payload CRC tests for sl_cpc_drv_uart.
 ******************************************************************************/

#include <unity_fixture.h>

#include "harness/uart.h"

// Flip header CRC bytes so the driver rejects the frame.
static void corrupt_header_crc(uint8_t *wire)
{
  size_t offset = 1U + SLI_CPC_HEADER_SIZE;

  wire[offset] ^= 0xFFU;
  wire[offset + 1U] ^= 0xFFU;
}

static void corrupt_header_crc_wire(uint8_t *wire, size_t wire_length)
{
  (void)wire_length;
  corrupt_header_crc(wire);
}

// Flip payload CRC bytes; header is still valid so the frame is delivered with a bad CRC flag.
static void corrupt_payload_crc(uint8_t *wire, size_t wire_length)
{
  wire[wire_length - 2U] ^= 0xFFU;
  wire[wire_length - 1U] ^= 0xFFU;
}

static void inject_all_but_last_byte(const uint8_t *wire, size_t wire_length)
{
  uart_inject_bytes(wire, wire_length - 1U);
  uart_inject_bytes(wire + wire_length - 1U, 1U);
}

// Flip a payload byte while leaving the trailing payload CRC unchanged.
static void corrupt_payload_data(uint8_t *wire, size_t wire_length)
{
  size_t payload_offset = 1U + SLI_CPC_HEADER_SIZE + sizeof(uint16_t);

  (void)wire_length;
  wire[payload_offset] ^= 0xFFU;
}

TEST_GROUP(cpc_drv_uart_crc);

TEST_SETUP(cpc_drv_uart_crc)
{
  uart_setup();
}

TEST_TEAR_DOWN(cpc_drv_uart_crc)
{
  uart_teardown();
}

/**
 * @brief Corrupt header CRC only; frame must not reach the endpoint.
 */
TEST(cpc_drv_uart_crc, invalid_header_crc_discarded)
{
  const uint8_t payload[] = {0x01U, 0x02U, 0x03U};
  uint8_t wire[TEST_UART_WIRE_BUFFER_SIZE];
  size_t wire_length;

  cpc_frame_expect_t frame = {
    .flags = CPC_EXPECT_FLAG_ACK_REQ,
    .dst = TEST_UART_EP_ID,
    .seq = 1,
    .ack = 0,
    .wnd = 1,
    .len = sizeof(payload),
    .payload = payload,
  };
  wire_length = encode_uart_frame(wire, sizeof(wire), &frame);
  corrupt_header_crc(wire);
  uart_inject_bytes(wire, wire_length);

  TEST_UART_READ_EXPECT_EMPTY();
}

/**
 * @brief Header block split across injects; bad header CRC must still be rejected.
 */
TEST(cpc_drv_uart_crc, invalid_header_crc_split_inject)
{
  const uint8_t payload[] = {0x10U, 0x20U};
  uint8_t wire[TEST_UART_WIRE_BUFFER_SIZE];
  size_t wire_length;

  cpc_frame_expect_t frame = {
    .flags = CPC_EXPECT_FLAG_ACK_REQ,
    .dst = TEST_UART_EP_ID,
    .seq = 1,
    .ack = 0,
    .wnd = 1,
    .len = sizeof(payload),
    .payload = payload,
  };
  wire_length = encode_uart_frame(wire, sizeof(wire), &frame);
  corrupt_header_crc(wire);

  uart_inject_bytes(wire, TEST_UART_HEADER_BLOCK_SIZE);
  uart_inject_bytes(wire + TEST_UART_HEADER_BLOCK_SIZE, wire_length - TEST_UART_HEADER_BLOCK_SIZE);

  TEST_UART_READ_EXPECT_EMPTY();
}

/**
 * @brief Bad frame followed by a valid re-encode of the same logical frame.
 */
TEST(cpc_drv_uart_crc, invalid_header_crc_then_valid_frame)
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
  uart_inject_corrupt_then_valid(frame, corrupt_header_crc_wire);

  TEST_UART_READ_EXPECT(TEST_UART_EP_ID, payload, sizeof(payload), true);
}

/**
 * @brief Payload CRC wrong; frame arrives with payload_csum_is_valid == false.
 */
TEST(cpc_drv_uart_crc, invalid_payload_crc_flagged)
{
  const uint8_t payload[] = {0x04U, 0x05U, 0x06U};
  uint8_t wire[TEST_UART_WIRE_BUFFER_SIZE];
  size_t wire_length;

  cpc_frame_expect_t frame = {
    .flags = CPC_EXPECT_FLAG_ACK_REQ,
    .dst = TEST_UART_EP_ID,
    .seq = 1,
    .ack = 0,
    .wnd = 1,
    .len = sizeof(payload),
    .payload = payload,
  };
  wire_length = encode_uart_frame(wire, sizeof(wire), &frame);
  corrupt_payload_crc(wire, wire_length);
  uart_inject_bytes(wire, wire_length);

  TEST_UART_READ_EXPECT(TEST_UART_EP_ID, payload, sizeof(payload), false);
}

/**
 * @brief Bad payload CRC frame, then valid; expect flag false then true.
 */
TEST(cpc_drv_uart_crc, invalid_payload_crc_then_valid_frame)
{
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
  uart_inject_corrupt_then_valid(frame, corrupt_payload_crc);

  TEST_UART_READ_EXPECT(TEST_UART_EP_ID, payload, sizeof(payload), false);
  TEST_UART_READ_EXPECT(TEST_UART_EP_ID, payload, sizeof(payload), true);
}

/**
 * @brief Split inject with a bad payload CRC still flags payload_csum_is_valid false.
 */
TEST(cpc_drv_uart_crc, invalid_payload_crc_split_inject)
{
  const uint8_t payload[] = {0x09U, 0x0AU};
  uint8_t wire[TEST_UART_WIRE_BUFFER_SIZE];
  size_t wire_length;

  cpc_frame_expect_t frame = {
    .flags = CPC_EXPECT_FLAG_ACK_REQ,
    .dst = TEST_UART_EP_ID,
    .seq = 1,
    .ack = 0,
    .wnd = 1,
    .len = sizeof(payload),
    .payload = payload,
  };
  wire_length = encode_uart_frame(wire, sizeof(wire), &frame);
  corrupt_payload_crc(wire, wire_length);
  inject_all_but_last_byte(wire, wire_length);

  TEST_UART_READ_EXPECT(TEST_UART_EP_ID, payload, sizeof(payload), false);
}

/**
 * @brief Max-sized payload with a corrupted payload CRC field.
 */
TEST(cpc_drv_uart_crc, invalid_payload_crc_long_payload)
{
  uint8_t payload[TEST_UART_MAX_TEST_PAYLOAD_LENGTH];
  uint8_t wire[TEST_UART_WIRE_BUFFER_SIZE];
  size_t wire_length;

  for (size_t i = 0; i < sizeof(payload); i++) {
    payload[i] = (uint8_t)(i ^ 0x5AU);
  }

  cpc_frame_expect_t frame = {
    .flags = CPC_EXPECT_FLAG_ACK_REQ,
    .dst = TEST_UART_EP_ID,
    .seq = 1,
    .ack = 0,
    .wnd = 1,
    .len = sizeof(payload),
    .payload = payload,
  };
  wire_length = encode_uart_frame(wire, sizeof(wire), &frame);
  corrupt_payload_crc(wire, wire_length);
  uart_inject_bytes(wire, wire_length);

  TEST_UART_READ_EXPECT(TEST_UART_EP_ID, payload, sizeof(payload), false);
}

/**
 * @brief Smallest non-zero payload still validates payload CRC.
 */
TEST(cpc_drv_uart_crc, valid_payload_crc_single_byte_payload)
{
  uint8_t wire[TEST_UART_WIRE_BUFFER_SIZE];
  const uint8_t payload[] = {0x42U};

  cpc_frame_expect_t frame = {
    .flags = CPC_EXPECT_FLAG_ACK_REQ,
    .dst = TEST_UART_EP_ID,
    .seq = 1,
    .ack = 0,
    .wnd = 1,
    .len = sizeof(payload),
    .payload = payload,
  };
  uart_inject_frame(wire, sizeof(wire), frame);

  TEST_UART_READ_EXPECT(TEST_UART_EP_ID, payload, sizeof(payload), true);
}

/**
 * @brief Final payload CRC byte arrives in a second inject; a valid frame still validates.
 */
TEST(cpc_drv_uart_crc, valid_payload_crc_split_inject)
{
  const uint8_t payload[] = {0x06U, 0x07U, 0x08U};
  uint8_t wire[TEST_UART_WIRE_BUFFER_SIZE];
  size_t wire_length;

  cpc_frame_expect_t frame = {
    .flags = CPC_EXPECT_FLAG_ACK_REQ,
    .dst = TEST_UART_EP_ID,
    .seq = 1,
    .ack = 0,
    .wnd = 1,
    .len = sizeof(payload),
    .payload = payload,
  };
  wire_length = encode_uart_frame(wire, sizeof(wire), &frame);
  inject_all_but_last_byte(wire, wire_length);

  TEST_UART_READ_EXPECT(TEST_UART_EP_ID, payload, sizeof(payload), true);
}

/**
 * @brief Max-sized payload with a valid payload CRC.
 */
TEST(cpc_drv_uart_crc, valid_payload_crc_long_payload)
{
  uint8_t payload[TEST_UART_MAX_TEST_PAYLOAD_LENGTH];
  uint8_t wire[TEST_UART_WIRE_BUFFER_SIZE];

  for (size_t i = 0; i < sizeof(payload); i++) {
    payload[i] = (uint8_t)i;
  }

  cpc_frame_expect_t frame = {
    .flags = CPC_EXPECT_FLAG_ACK_REQ,
    .dst = TEST_UART_EP_ID,
    .seq = 1,
    .ack = 0,
    .wnd = 1,
    .len = sizeof(payload),
    .payload = payload,
  };
  uart_inject_frame(wire, sizeof(wire), frame);

  TEST_UART_READ_EXPECT(TEST_UART_EP_ID, payload, sizeof(payload), true);
}

/**
 * @brief Payload bytes corrupted but wire CRC left intact; checksum must not validate.
 */
TEST(cpc_drv_uart_crc, corrupt_payload_data_crc_flagged)
{
  const uint8_t corrupted_payload[] = {0x01U ^ 0xFFU, 0x02U, 0x03U};
  const uint8_t payload[] = {0x01U, 0x02U, 0x03U};
  uint8_t wire[TEST_UART_WIRE_BUFFER_SIZE];
  size_t wire_length;

  cpc_frame_expect_t frame = {
    .flags = CPC_EXPECT_FLAG_ACK_REQ,
    .dst = TEST_UART_EP_ID,
    .seq = 1,
    .ack = 0,
    .wnd = 1,
    .len = sizeof(payload),
    .payload = payload,
  };
  wire_length = encode_uart_frame(wire, sizeof(wire), &frame);
  corrupt_payload_data(wire, wire_length);
  uart_inject_bytes(wire, wire_length);

  TEST_UART_READ_EXPECT(TEST_UART_EP_ID, corrupted_payload, sizeof(corrupted_payload), false);
}

/**
 * @brief Corrupt payload data, then deliver a valid re-encode of the same logical frame.
 */
TEST(cpc_drv_uart_crc, corrupt_payload_data_then_valid_frame)
{
  const uint8_t corrupted_payload[] = {0xCCU ^ 0xFFU, 0xDDU};
  const uint8_t payload[] = {0xCCU, 0xDDU};

  cpc_frame_expect_t frame = {
    .flags = CPC_EXPECT_FLAG_ACK_REQ,
    .dst = TEST_UART_EP_ID,
    .seq = 1,
    .ack = 0,
    .wnd = 1,
    .len = sizeof(payload),
    .payload = payload,
  };
  uart_inject_corrupt_then_valid(frame, corrupt_payload_data);

  TEST_UART_READ_EXPECT(TEST_UART_EP_ID, corrupted_payload, sizeof(corrupted_payload), false);
  TEST_UART_READ_EXPECT(TEST_UART_EP_ID, payload, sizeof(payload), true);
}

/**
 * @brief Corrupt payload data with the final wire byte arriving in a second inject.
 */
TEST(cpc_drv_uart_crc, corrupt_payload_data_split_inject)
{
  const uint8_t corrupted_payload[] = {0x11U ^ 0xFFU, 0x22U};
  const uint8_t payload[] = {0x11U, 0x22U};
  uint8_t wire[TEST_UART_WIRE_BUFFER_SIZE];
  size_t wire_length;

  cpc_frame_expect_t frame = {
    .flags = CPC_EXPECT_FLAG_ACK_REQ,
    .dst = TEST_UART_EP_ID,
    .seq = 1,
    .ack = 0,
    .wnd = 1,
    .len = sizeof(payload),
    .payload = payload,
  };
  wire_length = encode_uart_frame(wire, sizeof(wire), &frame);
  corrupt_payload_data(wire, wire_length);
  inject_all_but_last_byte(wire, wire_length);

  TEST_UART_READ_EXPECT(TEST_UART_EP_ID, corrupted_payload, sizeof(corrupted_payload), false);
}

/**
 * @brief Zero-length payload: header-only frames keep payload_csum_is_valid false.
 */
TEST(cpc_drv_uart_crc, header_only_payload_crc_flag_false)
{
  uint8_t wire[TEST_UART_WIRE_BUFFER_SIZE];

  cpc_frame_expect_t frame = {
    .flags = CPC_EXPECT_FLAG_ACK_REQ,
    .dst = TEST_UART_EP_ID,
    .seq = 1,
    .ack = 0,
    .wnd = 1,
    .len = 0U,
    .payload = NULL,
  };
  uart_inject_frame(wire, sizeof(wire), frame);

  TEST_UART_READ_EXPECT(TEST_UART_EP_ID, NULL, 0U, false);
}

TEST_GROUP_RUNNER(cpc_drv_uart_crc)
{
  RUN_TEST_CASE(cpc_drv_uart_crc, invalid_header_crc_discarded);
  RUN_TEST_CASE(cpc_drv_uart_crc, invalid_header_crc_split_inject);
  RUN_TEST_CASE(cpc_drv_uart_crc, invalid_header_crc_then_valid_frame);
  RUN_TEST_CASE(cpc_drv_uart_crc, invalid_payload_crc_flagged);
  RUN_TEST_CASE(cpc_drv_uart_crc, invalid_payload_crc_then_valid_frame);
  RUN_TEST_CASE(cpc_drv_uart_crc, invalid_payload_crc_split_inject);
  RUN_TEST_CASE(cpc_drv_uart_crc, invalid_payload_crc_long_payload);
  RUN_TEST_CASE(cpc_drv_uart_crc, valid_payload_crc_single_byte_payload);
  RUN_TEST_CASE(cpc_drv_uart_crc, valid_payload_crc_split_inject);
  RUN_TEST_CASE(cpc_drv_uart_crc, valid_payload_crc_long_payload);
  RUN_TEST_CASE(cpc_drv_uart_crc, corrupt_payload_data_crc_flagged);
  RUN_TEST_CASE(cpc_drv_uart_crc, corrupt_payload_data_then_valid_frame);
  RUN_TEST_CASE(cpc_drv_uart_crc, corrupt_payload_data_split_inject);
  RUN_TEST_CASE(cpc_drv_uart_crc, header_only_payload_crc_flag_false);
}
