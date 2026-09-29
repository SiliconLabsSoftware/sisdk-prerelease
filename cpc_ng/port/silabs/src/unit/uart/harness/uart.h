/***************************************************************************/ /**
 * @file uart.h
 * @brief CPC UART driver unit-test helpers.
 ******************************************************************************/

#ifndef CPC_DRV_UART_TEST_UART_H
#define CPC_DRV_UART_TEST_UART_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "../../../../../../src/unit/frame.h"

#ifdef __cplusplus
extern "C" {
#endif

#define TEST_UART_EP_ID 42U

// Must match SLI_CPC_DRV_UART_PREAMBLE in sl_cpc_drv_uart.c.
#define TEST_UART_PREAMBLE 0xEBU

// Preamble + CPC header + header CRC; matches the UART driver header block size.
#define TEST_UART_HEADER_BLOCK_SIZE (1U + SLI_CPC_HEADER_SIZE + sizeof(uint16_t))

// Sized for the frames used in these tests (preamble + header + CRCs + payload).
#define TEST_UART_WIRE_BUFFER_SIZE 64U

#define TEST_UART_STREAM_BUFFER_SIZE 64U

#define TEST_UART_MAX_TEST_PAYLOAD_LENGTH (TEST_UART_WIRE_BUFFER_SIZE - TEST_UART_HEADER_BLOCK_SIZE - sizeof(uint16_t))

typedef void (*uart_wire_corrupt_fn)(uint8_t *wire, size_t wire_length);

#define uart_setup() uart_setup_at(__LINE__)
void uart_setup_at(unsigned int lineno);

void uart_teardown(void);

size_t encode_uart_frame(uint8_t *out, size_t out_capacity, const cpc_frame_expect_t *frame);
size_t uart_inject_frame(uint8_t *wire, size_t wire_capacity, cpc_frame_expect_t frame);
#define uart_inject_bytes(_data, _length) uart_inject_bytes_at(__LINE__, (_data), (_length))
void uart_inject_bytes_at(unsigned int lineno, const uint8_t *data, size_t length);
void uart_inject_corrupt_then_valid(cpc_frame_expect_t frame, uart_wire_corrupt_fn corrupt);
void uart_inject_prefix_and_frame(const uint8_t *prefix, size_t prefix_length, cpc_frame_expect_t frame);
void uart_inject_frame_in_two_parts(cpc_frame_expect_t frame);

#define TEST_UART_READ_EXPECT_EMPTY() test_uart_read_expect_empty_at(__LINE__)
void test_uart_read_expect_empty_at(unsigned int lineno);

#define TEST_UART_READ_EXPECT(_ep_id, _payload, _payload_length, _expect_valid_payload_crc) \
  test_uart_read_expect_at(__LINE__, (_ep_id), (_payload), (_payload_length), (_expect_valid_payload_crc))
void test_uart_read_expect_at(unsigned int lineno, uint8_t ep_id, const uint8_t *payload, size_t payload_length,
                              bool expect_valid_payload_crc);

#ifdef __cplusplus
}
#endif

#endif
