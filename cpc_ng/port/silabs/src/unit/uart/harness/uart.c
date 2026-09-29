/***************************************************************************/ /**
 * @file uart.c
 * @brief CPC UART driver unit-test helpers.
 *
 * End-to-end UART RX tests against the real sl_cpc_drv_uart driver:
 *
 *   setup → encode wire frame → inject bytes → run dispatcher → read frame → expect
 *
 * Tests suspend the CPC core task and call sli_cpc_dispatcher_process() manually
 * so RX handling is deterministic under Unity. Bytes enter the UART RX path through
 * loopback_inject_bytes(); the read helpers drain the CPC dispatcher when asserting.
 ******************************************************************************/

#include <stdalign.h>
#include <string.h>

#include <unity_fixture.h>

#if defined(SL_CATALOG_KERNEL_PRESENT)
#include "cmsis_os2.h"
#endif

#include "sl_cpc_buf.h"
#include "sl_cpc_bus_instances.h"
#include "sl_slist.h"
#include "sl_status.h"

#include "../../../../../../src/sli_cpc.h"
#include "../../../../../../src/sli_cpc_bus.h"
#include "../../../../../../src/sli_cpc_crc.h"
#include "../../../../../../src/sli_cpc_dispatcher.h"
#include "../../../../../../src/sli_cpc_endianness.h"
#include "../../../../../../src/sli_cpc_frame_list.h"
#include "../../../../../../src/sli_cpc_hdr.h"
#include "../../../../../../src/sli_cpc_timer.h"

#include "../../../../../../src/unit/unity_assert.h"
#include "loopback.h"
#include "uart.h"

#define EP_RX_BUFFER_COUNT 2U
#define EP_RX_BUFFER_SIZE 64U

// Fail tests that would otherwise spin on a stuck dispatcher queue.
#define MAX_DRAIN_ITERATIONS 1000U

/******************************************************************************/
/*                                   Locals                                   */
/******************************************************************************/

// Endpoint and RX buffers created in uart_setup().
static sl_cpc_ep_t s_ep;
alignas(SL_CPC_BUF_MIN_ALIGNMENT) static uint8_t s_rx_buffer_data[EP_RX_BUFFER_COUNT][EP_RX_BUFFER_SIZE];
static sl_cpc_buf_t s_rx_handles[EP_RX_BUFFER_COUNT];

// Extra frames from driver->read() not yet returned to the test.
static sli_cpc_frame_list_t s_pending_reads;

static bool s_frame_pools_started;
#if defined(SL_CATALOG_KERNEL_PRESENT)
static bool s_thread_suspended;
#endif

/******************************************************************************/
/*                              Static functions                              */
/******************************************************************************/

static void uart_event_cb(sl_cpc_ep_t *ep, sl_cpc_ep_event_type_t type, const sl_cpc_ep_event_t *event, void *arg)
{
  (void)ep;
  (void)type;
  (void)event;
  (void)arg;
}

static void release_rx_frame(sl_cpc_frame_t *frame)
{
  if (frame == NULL) {
    return;
  }

  if (frame->payload != NULL && frame->ep != NULL) {
    sl_cpc_ep_push_recv_buf(frame->ep, frame->payload);
    frame->payload = NULL;
  }

  sli_cpc_frame_put_ref(&frame);
}

static void clear_pending_reads(void)
{
  sl_cpc_frame_t *frame;

  while ((frame = sli_cpc_frame_list_pop(&s_pending_reads)) != NULL) {
    release_rx_frame(frame);
  }
}

static void discard_ep(sl_cpc_ep_t *ep)
{
  sli_cpc_timer_stop(&ep->re_transmit_timer);
  (void)sl_cpc_ep_close(ep);

  sli_cpc_bus_process_action(g_bus);

  sl_slist_remove(&g_bus->eps, &ep->node);
  sl_slist_remove(&g_bus->closed_eps, &ep->node);

  sl_cpc_ep_deinit(ep);
}

static bool dispatcher_has_pending_work(void)
{
  const sli_cpc_dispatcher_context_t *dispatcher = &g_bus->dispatcher;

  return dispatcher->post_process_event_counter != 0U || dispatcher->process_queue != NULL
         || dispatcher->pre_process_event_counter != 0U || dispatcher->pre_process_queue != NULL;
}

static void drain_dispatcher(void)
{
  size_t iterations = 0U;

  while (dispatcher_has_pending_work() && iterations < MAX_DRAIN_ITERATIONS) {
    iterations++;

#if defined(SL_CATALOG_KERNEL_PRESENT)
    osDelay(1);
#endif
    sli_cpc_dispatcher_pre_process(g_bus);
    sli_cpc_dispatcher_process(g_bus);
  }
}

static sl_status_t pop_frame(sl_cpc_frame_t **frame)
{
  sli_cpc_frame_list_t frames;
  sl_status_t status;

  if (!sli_cpc_frame_list_empty(&s_pending_reads)) {
    *frame = sli_cpc_frame_list_pop(&s_pending_reads);
    return SL_STATUS_OK;
  }

  drain_dispatcher();

  sli_cpc_frame_list_init(&frames);

  status = g_bus->drv_ops->read(g_bus, &frames);

  if (status == SL_STATUS_OK) {
    *frame = sli_cpc_frame_list_pop(&frames);
    if (*frame != NULL) {
      sli_cpc_frame_list_extend(&s_pending_reads, &frames);
      return SL_STATUS_OK;
    }
  }

  *frame = NULL;
  return SL_STATUS_EMPTY;
}

static void assert_frame(unsigned int lineno, uint8_t ep_id, const uint8_t *payload, size_t payload_length,
                         sl_cpc_frame_t *frame, bool expect_valid_payload_crc)
{
  UNITY_TEST_ASSERT_NOT_NULL(frame, lineno, "TEST_UART_READ_EXPECT: no frame received");

  UNITY_TEST_ASSERT_EQUAL_UINT16(ep_id, sli_cpc_header_get_address(sli_cpc_frame_get_header(frame)), lineno,
                                 "TEST_UART_READ_EXPECT: endpoint mismatch");
  UNITY_TEST_ASSERT_EQUAL_UINT16(payload_length, sli_cpc_header_get_payload_size(sli_cpc_frame_get_header(frame)),
                                 lineno, "TEST_UART_READ_EXPECT: payload length mismatch");

  if (payload_length > 0U) {
    UNITY_TEST_ASSERT_NOT_NULL(frame->payload, lineno, "TEST_UART_READ_EXPECT: payload buffer is NULL");
    UNITY_TEST_ASSERT_EQUAL_UINT16(payload_length, frame->payload->len, lineno,
                                   "TEST_UART_READ_EXPECT: payload buffer size mismatch");
    UNITY_TEST_ASSERT_EQUAL_UINT8_ARRAY(payload, frame->payload->ptr, payload_length, lineno,
                                        "TEST_UART_READ_EXPECT: payload mismatch");
  } else {
    UNITY_TEST_ASSERT_NULL(frame->payload, lineno, "TEST_UART_READ_EXPECT: unexpected payload buffer");
  }

  UNITY_TEST_ASSERT_EQUAL_INT((int)expect_valid_payload_crc, (int)frame->payload_csum_is_valid, lineno,
                              "TEST_UART_READ_EXPECT: payload CRC flag mismatch");

  release_rx_frame(frame);
}

static void discard_leftover_rx(void)
{
  sli_cpc_frame_list_t frames;
  sl_cpc_frame_t *frame;
  size_t iterations = 0U;

  clear_pending_reads();

  // Drain completed frames through the driver read path.
  for (;;) {
    if (iterations >= MAX_DRAIN_ITERATIONS) {
      break;
    }

    iterations++;
    drain_dispatcher();

    sli_cpc_frame_list_init(&frames);

    if (g_bus->drv_ops->read(g_bus, &frames) != SL_STATUS_OK) {
      break;
    }

    while ((frame = sli_cpc_frame_list_pop(&frames)) != NULL) {
      release_rx_frame(frame);
    }
  }

  // Keep pumping the UART RX dispatcher so leftover stream bytes are consumed by
  // sl_cpc_drv_uart rather than read directly from the CPC UART iostream.
  for (iterations = 0U; iterations < MAX_DRAIN_ITERATIONS; iterations++) {
    drain_dispatcher();

    sli_cpc_frame_list_init(&frames);
    if (g_bus->drv_ops->read(g_bus, &frames) == SL_STATUS_OK) {
      while ((frame = sli_cpc_frame_list_pop(&frames)) != NULL) {
        release_rx_frame(frame);
      }
    }

    if (!dispatcher_has_pending_work()) {
      break;
    }
  }
}

size_t encode_uart_frame(uint8_t *out, size_t out_capacity, const cpc_frame_expect_t *frame)
{
  size_t payload_length;
  sli_cpc_hdr_t hdr;
  uint16_t payload_crc;
  uint16_t header_crc;
  size_t total_length;

  if (out == NULL || frame == NULL) {
    return 0U;
  }

  if (frame->len < 0) {
    return 0U;
  }

  payload_length = (uint16_t)frame->len;

  if (payload_length > 0U && frame->payload == NULL) {
    return 0U;
  }

  total_length = TEST_UART_HEADER_BLOCK_SIZE;
  if (payload_length > 0U) {
    total_length += payload_length + sizeof(payload_crc);
  }

  if (out_capacity < total_length) {
    return 0U;
  }

  frame_to_header(frame, &hdr);

  out[0] = (uint8_t)TEST_UART_PREAMBLE;
  memcpy(&out[1], &hdr, SLI_CPC_HEADER_SIZE);

  header_crc = sli_cpc_get_crc_sw(&out[1], SLI_CPC_HEADER_SIZE);
  sli_cpc_u16_to_le(header_crc, &out[1U + SLI_CPC_HEADER_SIZE]);

  if (payload_length == 0U) {
    return total_length;
  }

  memcpy(&out[TEST_UART_HEADER_BLOCK_SIZE], frame->payload, payload_length);

  payload_crc = sli_cpc_get_crc_sw(frame->payload, payload_length);
  sli_cpc_u16_to_le(payload_crc, &out[TEST_UART_HEADER_BLOCK_SIZE + payload_length]);

  return total_length;
}

/******************************************************************************/
/*                                 Public API                                 */
/******************************************************************************/

void uart_setup_at(unsigned int lineno)
{
  sl_status_t status;

  g_bus->initialized = true;

  s_frame_pools_started = false;
#if defined(SL_CATALOG_KERNEL_PRESENT)
  s_thread_suspended = false;
#endif

  status = sl_cpc_ep_init(&s_ep, TEST_UART_EP_ID, EP_RX_BUFFER_SIZE, uart_event_cb, NULL);
  UNITY_TEST_ASSERT_EQUAL_INT(SL_STATUS_OK, (int)status, lineno, "uart_setup: endpoint init failed");

  status = sl_cpc_ep_listen(&s_ep, g_bus);
  UNITY_TEST_ASSERT_EQUAL_INT(SL_STATUS_OK, (int)status, lineno, "uart_setup: endpoint listen failed");

  for (size_t i = 0; i < EP_RX_BUFFER_COUNT; i++) {
    sl_cpc_buf_init(&s_rx_handles[i], s_rx_buffer_data[i], EP_RX_BUFFER_SIZE);
    status = sl_cpc_ep_push_recv_buf(&s_ep, &s_rx_handles[i]);
    UNITY_TEST_ASSERT_EQUAL_INT(SL_STATUS_OK, (int)status, lineno, "uart_setup: push recv buffer failed");
  }

  status = loopback_bind();
  UNITY_TEST_ASSERT_EQUAL_INT(SL_STATUS_OK, (int)status, lineno, "uart_setup: loopback_bind failed");

#if defined(SL_CATALOG_KERNEL_PRESENT)
  if (g_bus->thread_id != NULL) {
    osThreadSuspend(g_bus->thread_id);
    s_thread_suspended = true;
  }
#endif

  sli_cpc_bus_process_action(g_bus);
  drain_dispatcher();

  CPC_FRAME_POOLS_START_TEST(g_bus);
  s_frame_pools_started = true;
  clear_pending_reads();
}

void uart_teardown(void)
{
  discard_leftover_rx();

  discard_ep(&s_ep);

  if (s_frame_pools_started) {
    CPC_FRAME_POOLS_END_TEST(g_bus);
    s_frame_pools_started = false;
  }

#if defined(SL_CATALOG_KERNEL_PRESENT)
  if (s_thread_suspended && g_bus->thread_id != NULL) {
    osThreadResume(g_bus->thread_id);
    s_thread_suspended = false;
  }
#endif
}

size_t uart_inject_frame(uint8_t *wire, size_t wire_capacity, cpc_frame_expect_t frame)
{
  size_t wire_length = encode_uart_frame(wire, wire_capacity, &frame);

  uart_inject_bytes(wire, wire_length);

  return wire_length;
}

void uart_inject_bytes_at(unsigned int lineno, const uint8_t *data, size_t length)
{
  loopback_inject_bytes_at(lineno, data, length);
}

void uart_inject_corrupt_then_valid(cpc_frame_expect_t frame, uart_wire_corrupt_fn corrupt)
{
  uint8_t good_wire[TEST_UART_WIRE_BUFFER_SIZE];
  uint8_t bad_wire[TEST_UART_WIRE_BUFFER_SIZE];
  size_t good_length;
  size_t bad_length;

  bad_length = encode_uart_frame(bad_wire, sizeof(bad_wire), &frame);
  good_length = encode_uart_frame(good_wire, sizeof(good_wire), &frame);

  corrupt(bad_wire, bad_length);
  uart_inject_bytes(bad_wire, bad_length);
  uart_inject_bytes(good_wire, good_length);
}

void uart_inject_prefix_and_frame(const uint8_t *prefix, size_t prefix_length, cpc_frame_expect_t frame)
{
  uint8_t wire[TEST_UART_WIRE_BUFFER_SIZE];
  size_t wire_length;

  if (prefix_length > 0U) {
    uart_inject_bytes(prefix, prefix_length);
  }

  wire_length = encode_uart_frame(wire, sizeof(wire), &frame);
  uart_inject_bytes(wire, wire_length);
}

void uart_inject_frame_in_two_parts(cpc_frame_expect_t frame)
{
  uint8_t wire[TEST_UART_WIRE_BUFFER_SIZE];
  size_t wire_length;
  size_t split;

  wire_length = encode_uart_frame(wire, sizeof(wire), &frame);

  split = TEST_UART_HEADER_BLOCK_SIZE;
  uart_inject_bytes(wire, split);
  uart_inject_bytes(wire + split, wire_length - split);
}

void test_uart_read_expect_empty_at(unsigned int lineno)
{
  sl_cpc_frame_t *frame = NULL;
  sl_status_t status = pop_frame(&frame);

  if (frame != NULL) {
    release_rx_frame(frame);
  }

  UNITY_TEST_ASSERT_EQUAL_INT(SL_STATUS_EMPTY, status, lineno, "TEST_UART_READ_EXPECT_EMPTY: unexpected frame");
}

void test_uart_read_expect_at(unsigned int lineno, uint8_t ep_id, const uint8_t *payload, size_t payload_length,
                              bool expect_valid_payload_crc)
{
  sl_cpc_frame_t *frame = NULL;

  UNITY_TEST_ASSERT_EQUAL_INT(SL_STATUS_OK, pop_frame(&frame), lineno, "TEST_UART_READ_EXPECT: frame read failed");
  assert_frame(lineno, ep_id, payload, payload_length, frame, expect_valid_payload_crc);
}
