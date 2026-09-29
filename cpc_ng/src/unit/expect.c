/***************************************************************************/ /**
 * @file
 * @brief CPC frame matching and mock-driver expect helpers for unit tests.
 ******************************************************************************/

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <unity_fixture.h>

#if defined(SL_COMPONENT_CATALOG_PRESENT)
#include "sl_component_catalog.h"
#endif

#if defined(SL_CATALOG_KERNEL_PRESENT)
#include "cmsis_os2.h"
#else
#include "../sli_cpc_bus.h"
#endif

#include "../sli_cpc_hdr.h"
#include "sl_cpc_bus_instances.h"
#include "sl_cpc_drv_mock.h"

#include "expect.h"
#include "frame.h"

/******************************************************************************/
/*    Flag formatting (used only on mismatch for readable error messages)     */
/******************************************************************************/

static const struct {
  char letter;
  uint8_t bit_mask;
} cpc_flag_letters[] = {
  {'A', CPC_EXPECT_FLAG_ACK_REQ},
  {'R', CPC_EXPECT_FLAG_RESET},
  {'S', CPC_EXPECT_FLAG_SYN},
};

#define CPC_FLAG_LETTERS_COUNT (sizeof(cpc_flag_letters) / sizeof(cpc_flag_letters[0]))

static void format_flags(uint8_t ctrl, char *out, size_t out_size)
{
  size_t pos = 0;
  for (size_t i = 0; i < CPC_FLAG_LETTERS_COUNT; i++) {
    if (pos + 1U >= out_size) {
      break;
    }
    if ((ctrl & cpc_flag_letters[i].bit_mask) != 0U) {
      out[pos++] = cpc_flag_letters[i].letter;
    }
  }
  out[pos] = '\0';
}

/******************************************************************************/
/*                                  Matching                                  */
/******************************************************************************/

static void check_field_uint(const char *label, int expected_value, long actual_value, unsigned int lineno)
{
  unsigned long expected = (unsigned long)(expected_value);
  unsigned long actual = (unsigned long)(actual_value);

  if (expected_value >= 0 && actual != expected) {
    char err_msg[96];
    snprintf(err_msg, sizeof(err_msg), "%s mismatch: expected %lu got %lu", label, expected, actual);
    UNITY_TEST_FAIL(lineno, err_msg);
  }
}

void test_assert_packet(unsigned int lineno, const sli_cpc_hdr_t *hdr, const cpc_frame_expect_t *expect)
{
  UNITY_TEST_ASSERT_NOT_NULL(hdr, lineno, "test_assert_packet: hdr is NULL");
  UNITY_TEST_ASSERT_NOT_NULL(expect, lineno, "test_assert_packet: expect is NULL");

  if (expect->flags >= 0) {
    uint8_t expected_flags = (uint8_t)(expect->flags & CPC_EXPECT_FLAGS_MASK);
    uint8_t actual_flags = hdr->ctrl;

    if (actual_flags != expected_flags) {
      char expected_str[CPC_FLAG_LETTERS_COUNT + 1U];
      char actual_str[CPC_FLAG_LETTERS_COUNT + 1U];
      char err_msg[96];

      format_flags(expected_flags, expected_str, sizeof(expected_str));
      format_flags(actual_flags, actual_str, sizeof(actual_str));

      // Use '-' rather than an empty string when no flags are set, so the
      // failure message keeps a stable shape.
      const char *expected_disp = expected_str[0] != '\0' ? expected_str : "-";
      const char *actual_disp = actual_str[0] != '\0' ? actual_str : "-";
      snprintf(err_msg, sizeof(err_msg), "flags mismatch: expected %s got %s", expected_disp, actual_disp);

      UNITY_TEST_FAIL(lineno, err_msg);
    }
  }

  check_field_uint("len", expect->len, sli_cpc_u16_from_le(hdr->size_le), lineno);
  check_field_uint("dst", expect->dst, sli_cpc_header_get_address(hdr), lineno);
  check_field_uint("seq", expect->seq, sli_cpc_header_get_seq(hdr), lineno);
  check_field_uint("ack", expect->ack, sli_cpc_header_get_ack(hdr), lineno);
  check_field_uint("wnd", expect->wnd, sli_cpc_header_get_rx_wnd(hdr), lineno);
}

mock_cpc_tx_packet_t *test_pop_outgoing(unsigned int lineno, uint32_t timeout, sl_cpc_drv_mock_t *drv,
                                        const cpc_frame_expect_t expect)
{
#if defined(SL_CATALOG_KERNEL_PRESENT)
  uint32_t deadline = osKernelGetTickCount() + timeout;
  while (mock_cpc_drv_pending_tx_count(drv) == 0 && osKernelGetTickCount() < deadline) {
    osDelay(1);
  }
#else
  // In baremetal builds the bus is driven by the foreground loop, so
  // pump the dispatcher a handful of times to give it a chance to enqueue
  // the outgoing frame.
  for (uint32_t i = 0; i < timeout && mock_cpc_drv_pending_tx_count(drv) == 0; i++) {
    sli_cpc_bus_process_action(g_bus);
  }
#endif

  mock_cpc_tx_packet_t *packet = mock_cpc_drv_pop_tx_packet(drv);
  UNITY_TEST_ASSERT_NOT_NULL(packet, lineno, "Mock driver TX queue is empty");

  test_assert_packet(lineno, &packet->header, &expect);

  return packet;
}

mock_cpc_tx_packet_t *test_pop_outgoing_payload(unsigned int lineno, uint32_t timeout, sl_cpc_drv_mock_t *drv,
                                                const cpc_frame_expect_t expect)
{
  mock_cpc_tx_packet_t *packet;
  uint32_t empty_frames = 0;

  for (;;) {
    while (mock_cpc_drv_pending_tx_count(drv) > 0) {
      packet = mock_cpc_drv_pop_tx_packet(drv);
      UNITY_TEST_ASSERT_NOT_NULL(packet, lineno, "Mock driver TX queue is empty");

      if (sli_cpc_header_get_payload_size(&packet->header) > 0) {
        test_assert_packet(lineno, &packet->header, &expect);
        return packet;
      }

      mock_cpc_tx_packet_free(&packet);
      empty_frames++;

      if (empty_frames >= timeout) {
        UNITY_TEST_FAIL(lineno, "timed out waiting for a non-empty payload");
      }
    }

    packet = test_pop_outgoing(lineno, timeout, drv, expect);

    if (sli_cpc_header_get_payload_size(&packet->header) > 0) {
      return packet;
    }

    mock_cpc_tx_packet_free(&packet);
    empty_frames++;

    if (empty_frames >= timeout) {
      UNITY_TEST_FAIL(lineno, "timed out waiting for a non-empty payload");
    }
  }
}

void test_assert_outgoing(unsigned int lineno, uint32_t timeout, sl_cpc_drv_mock_t *drv,
                          const cpc_frame_expect_t expect)
{
  mock_cpc_tx_packet_t *packet = test_pop_outgoing(lineno, timeout, drv, expect);
  mock_cpc_tx_packet_free(&packet);
}
