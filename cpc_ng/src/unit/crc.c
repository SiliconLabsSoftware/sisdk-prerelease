/***************************************************************************/ /**
 * @file
 * @brief Unit tests for sli_cpc_crc (CRC-16/XMODEM).
 *
 * Expected values use poly 0x1021, init 0x0000, no reflection, and xorout 0.
 * The bitwise and lookup-table implementations share this contract.
 ******************************************************************************/

#include <stddef.h>
#include <stdint.h>

#include <unity_fixture.h>

#include "sl_cpc_buf.h"

#include "../sli_cpc_crc.h"

TEST_GROUP(cpc_crc);

TEST_SETUP(cpc_crc)
{ /* empty */
}

TEST_TEAR_DOWN(cpc_crc)
{ /* empty */
}

/**
 * @brief A zero-length buffer hashes to the CRC-16/XMODEM init value.
 *
 * A NULL pointer is valid when the length is zero; the buffer is not read.
 */
TEST(cpc_crc, zero_length_is_zero)
{
  const uint8_t unused = 0xFFU;

  TEST_ASSERT_EQUAL_HEX16(0x0000, sli_cpc_crc(NULL, 0U));
  TEST_ASSERT_EQUAL_HEX16(0x0000, sli_cpc_crc(&unused, 0U));
}

/**
 * @brief Published CRC-16/XMODEM check value: ASCII "123456789" is 0x31C3.
 */
TEST(cpc_crc, xmodem_check_value)
{
  static const uint8_t check[] = {'1', '2', '3', '4', '5', '6', '7', '8', '9'};

  TEST_ASSERT_EQUAL_HEX16(0x31C3, sli_cpc_crc(check, sizeof(check)));
}

/**
 * @brief A chain is hashed as one continuous input, in link order.
 *
 * Splitting "123456789" across segments, including a zero-length segment,
 * yields the same CRC-16/XMODEM check value as the contiguous buffer.
 */
TEST(cpc_crc, chained_buffers)
{
  static const uint8_t head_bytes[] = {'1', '2', '3', '4'};
  static const uint8_t mid_byte[] = {'5'};
  static const uint8_t tail_bytes[] = {'6', '7', '8', '9'};
  sl_cpc_buf_t head;
  sl_cpc_buf_t middle;
  sl_cpc_buf_t empty;
  sl_cpc_buf_t tail;

  sl_cpc_buf_init(&head, (uint8_t *)head_bytes, sizeof(head_bytes));
  sl_cpc_buf_init(&middle, (uint8_t *)mid_byte, sizeof(mid_byte));
  sl_cpc_buf_init(&empty, NULL, 0U);
  sl_cpc_buf_init(&tail, (uint8_t *)tail_bytes, sizeof(tail_bytes));

  TEST_ASSERT_EQUAL_HEX16(sli_cpc_crc(head_bytes, sizeof(head_bytes)), sli_cpc_crc_buf(&head));

  sl_cpc_buf_chain(&head, &middle);
  sl_cpc_buf_chain(&head, &empty);
  sl_cpc_buf_chain(&head, &tail);

  TEST_ASSERT_EQUAL_HEX16(0x31C3, sli_cpc_crc_buf(&head));
}

TEST_GROUP_RUNNER(cpc_crc)
{
  RUN_TEST_CASE(cpc_crc, zero_length_is_zero);
  RUN_TEST_CASE(cpc_crc, xmodem_check_value);
  RUN_TEST_CASE(cpc_crc, chained_buffers);
}
