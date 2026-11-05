/***************************************************************************/ /**
 * @file uart.c
 * @brief CPC UART driver unit-test helpers.
 *
 * End-to-end UART RX tests against the real sl_cpc_drv_uart driver:
 *
 *   setup → encode wire frame → inject bytes → process driver → read frame → expect
 *
 * Tests suspend the CPC core task and drive the UART driver read path manually
 * so RX handling is deterministic under Unity.
 *
 * Bytes enter the UART RX path through loopback_inject_bytes_at().
 *
 * uart_inject_bytes() waits on RX callback notifications when injected bytes
 * complete one or more DMA buffers.
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

#include <stdalign.h>
#include <string.h>

#include <unity_fixture.h>

#if defined(SL_CATALOG_KERNEL_PRESENT)
#include "cmsis_os2.h"
#endif

#include "sl_cpc_buf.h"
#include "sl_cpc_bus_instances.h"
#include "sl_cpc_drv_instances.h"
#include "sl_cpc_drv_uart_eusart.h"
#include "sl_slist.h"
#include "sl_status.h"
#include "sl_uart.h"

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

#define EP_RX_BUFFER_COUNT 5U
#define EP_RX_BUFFER_SIZE 64U

// Maximum number of iterations to drain queued dispatcher work.
#define MAX_DRAIN_ITERATIONS 1000U

// Maximum time to wait for each UART RX progress notification.
#define UART_RX_PROGRESS_TIMEOUT_MS 1000U

/******************************************************************************/
/*                                   Locals                                   */
/******************************************************************************/

// Endpoint under test
static sl_cpc_ep_t s_ep;

// RX buffers queued on s_ep.
alignas(SL_CPC_BUF_MIN_ALIGNMENT) static uint8_t s_rx_buffer_data[EP_RX_BUFFER_COUNT][EP_RX_BUFFER_SIZE];

// Buffer descriptors that reference s_rx_buffer_data and are queued on s_ep.
static sl_cpc_buf_t s_rx_handles[EP_RX_BUFFER_COUNT];

// Extra frames from driver->read() not yet returned to the test.
static sli_cpc_frame_list_t s_pending_reads;

// Tracks whether setup started the frame-pool leak check so teardown can finish it.
static bool s_frame_pools_started;

// Cumulative bytes reported by the UART RX callback, compared with s_injected_bytes.
static volatile size_t s_rx_bytes_received;

// Cumulative bytes passed to the loopback injector.
static size_t s_injected_bytes;

// Stores the driver's RX callback and context while the test wrapper is installed.
typedef struct {
  // Original callback to invoke from the wrapper.
  sl_uart_async_on_rx_cb_t callback;

  // User data originally associated with the callback.
  void *context;
} uart_rx_callback_t;

// Callback state supplied to the wrapper and restored during teardown.
static uart_rx_callback_t s_driver_rx_callback;

#if defined(SL_CATALOG_KERNEL_PRESENT)
// Tracks whether setup suspended the CPC thread so teardown can resume it.
static bool s_thread_suspended;

// Signals that the UART RX callback made progress while injected bytes are being processed.
static osSemaphoreId_t s_rx_progress_semaphore;
#endif

/******************************************************************************/
/*                              Static functions                              */
/******************************************************************************/

/**
 * @brief Return the UART driver instance used by this test harness.
 */
static sl_cpc_drv_uart_eusart_t *uart_driver_instance(void)
{
  return sl_cpc_drv_uart_eusart_exp;
}

/*******************************************************************************/
/*                          UART RX progress helpers                          */
/*******************************************************************************/

/**
 * @brief Record bytes reported by the UART RX callback and wake waiting tests.
 */
static void on_uart_rx_progress(size_t size)
{
  s_rx_bytes_received += size;
#if defined(SL_CATALOG_KERNEL_PRESENT)
  osSemaphoreRelease(s_rx_progress_semaphore);
#endif
}

/**
 * @brief Forward UART RX events to the driver callback and track released buffers.
 */
static void uart_rx_event(sl_uart_handle_t *uart_handle, void *data, size_t size, void *user_data,
                          sl_uart_async_rx_event_t rx_event)
{
  uart_rx_callback_t *driver_callback = user_data;

  driver_callback->callback(uart_handle, data, size, driver_callback->context, rx_event);

  if ((rx_event & SL_UART_ASYNC_RX_EVENT_BUF_RELEASED) != 0) {
    on_uart_rx_progress(size);
  }
}

/**
 * @brief Wrap the driver's RX callback so tests can observe RX progress.
 */
static void install_uart_rx_callback(sl_uart_handle_t *uart_handle, unsigned int lineno)
{
  s_driver_rx_callback.callback = uart_handle->async_rx_cb;
  s_driver_rx_callback.context = uart_handle->async_rx_cb_user_data;
  UNITY_TEST_ASSERT_EQUAL_INT(1, s_driver_rx_callback.callback != NULL, lineno,
                              "uart_setup: driver RX callback is NULL");

  sl_uart_async_set_rx_event_callback(uart_handle, uart_rx_event, &s_driver_rx_callback);
}

/**
 * @brief Restore the original driver RX callback after the test.
 */
static void restore_uart_rx_callback(sl_uart_handle_t *uart_handle)
{
  if (uart_handle->async_rx_cb == uart_rx_event) {
    sl_uart_async_set_rx_event_callback(uart_handle, s_driver_rx_callback.callback, s_driver_rx_callback.context);
  }
}

/**
 * @brief Return the number of bytes needed to complete the driver's current RX transfer.
 */
static size_t next_uart_rx_transfer_size(const struct sli_cpc_drv_uart_rx *rx)
{
  if (!rx->rx_started || rx->waiting_for_frame || rx->suspended) {
    return 0U;
  }

  switch (rx->state) {
    case SLI_CPC_DRV_UART_RX_STATE_RECEIVING_HEADER_BLOCK:
      return rx->hdr_bytes_received < sizeof(rx->header) ? sizeof(rx->header) - rx->hdr_bytes_received : 0U;

    case SLI_CPC_DRV_UART_RX_STATE_RECEIVING_PAYLOAD:
      return (rx->frame != NULL && rx->frame->payload != NULL) ? rx->frame->payload->len : 0U;

    case SLI_CPC_DRV_UART_RX_STATE_RECEIVING_PAYLOAD_CRC:
      return rx->frame != NULL ? sizeof(rx->frame->payload_csum_le) : 0U;

    case SLI_CPC_DRV_UART_RX_STATE_DISCARDING_PAYLOAD:
      // The driver may receive a large discarded payload in several chunks.
      // Waiting for the remaining payload is conservative and avoids assuming
      // the private size of its temporary DMA buffer.
      return rx->payload_bytes_remaining;

    default:
      return 0U;
  }
}

/**
 * @brief Wait until injected bytes advance the UART RX path or the wait times out.
 */
static void wait_for_uart_rx_progress(unsigned int lineno)
{
  const struct sli_cpc_drv_uart_rx *rx = &uart_driver_instance()->rx;
#if defined(SL_CATALOG_KERNEL_PRESENT)
  const uint32_t timeout_ticks = (osKernelGetTickFreq() * UART_RX_PROGRESS_TIMEOUT_MS + 999U) / 1000U;
#else
  const uint32_t timeout_ticks = sli_cpc_timer_ms_to_tick((uint16_t)UART_RX_PROGRESS_TIMEOUT_MS);
#endif

  for (;;) {
    const size_t received = s_rx_bytes_received;
    const size_t pending = s_injected_bytes > received ? s_injected_bytes - received : 0U;
    const size_t next_transfer_size = next_uart_rx_transfer_size(rx);

    // A short final fragment is intentional in several tests. It cannot
    // produce another callback until more bytes are injected, so leave it
    // queued for the next injection instead of waiting for it here.
    if (next_transfer_size == 0U || pending < next_transfer_size) {
      return;
    }

#if defined(SL_CATALOG_KERNEL_PRESENT)
    UNITY_TEST_ASSERT_EQUAL_INT(osOK, (int)osSemaphoreAcquire(s_rx_progress_semaphore, timeout_ticks), lineno,
                                "uart_inject_bytes: timed out waiting for UART RX progress");
#else
    const size_t received_before_wait = s_rx_bytes_received;
    const uint32_t wait_start_tick = sli_cpc_timer_get_tick_count();

    if (received_before_wait != received) {
      continue;
    }

    while (s_rx_bytes_received == received_before_wait) {
      const uint32_t elapsed_ticks = sli_cpc_timer_get_tick_count() - wait_start_tick;
      UNITY_TEST_ASSERT_EQUAL_INT(1, (int)(elapsed_ticks < timeout_ticks), lineno,
                                  "uart_inject_bytes: timed out waiting for UART RX progress");
    }
#endif
  }
}

/*******************************************************************************/
/*                               Utility helpers                               */
/*******************************************************************************/

/**
 * @brief Ignore endpoint events that are not relevant to the UART driver tests.
 */
static void ep_event_cb(sl_cpc_ep_t *ep, sl_cpc_ep_event_type_t type, const sl_cpc_ep_event_t *event, void *arg)
{
  (void)ep;
  (void)type;
  (void)event;
  (void)arg;
}

/**
 * @brief Return a received frame's payload buffer and release the frame.
 */
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

/**
 * @brief Release all frames held in the pending read list.
 */
static void clear_pending_reads(void)
{
  sl_cpc_frame_t *frame;

  while ((frame = sli_cpc_frame_list_pop(&s_pending_reads)) != NULL) {
    release_rx_frame(frame);
  }
}

/**
 * @brief Close and deinitialize the test endpoint, removing it from bus lists.
 */
static void discard_ep(sl_cpc_ep_t *ep)
{
  sli_cpc_timer_stop(&ep->re_transmit_timer);
  (void)sl_cpc_ep_close(ep);

  sli_cpc_bus_process_action(g_bus);

  sl_slist_remove(&g_bus->eps, &ep->node);
  sl_slist_remove(&g_bus->closed_eps, &ep->node);

  TEST_ASSERT_EQUAL(SL_STATUS_OK, sl_cpc_ep_deinit(ep));
}

/**
 * @brief Check whether either dispatcher queue still has work to process.
 */
static bool dispatcher_has_pending_work(void)
{
  const sli_cpc_dispatcher_context_t *dispatcher = &g_bus->dispatcher;

  return dispatcher->post_process_event_counter != 0U || dispatcher->process_queue != NULL
         || dispatcher->pre_process_event_counter != 0U || dispatcher->pre_process_queue != NULL;
}

/**
 * @brief Process queued dispatcher work until it drains or reaches the iteration limit.
 */
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

/**
 * @brief Read the next available frame, preserving any additional frames for later reads.
 */
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

/**
 * @brief Check a received frame's endpoint, payload, and CRC state, then release it.
 */
static void assert_frame(unsigned int lineno, uint8_t ep_id, const uint8_t *payload, size_t payload_length,
                         sl_cpc_frame_t *frame, bool expect_valid_payload_crc)
{
  UNITY_TEST_ASSERT_NOT_NULL(frame, lineno, "TEST_UART_READ_EXPECT: no frame received");

  UNITY_TEST_ASSERT_EQUAL_UINT16(ep_id, sli_cpc_header_get_address(&frame->hdr), lineno,
                                 "TEST_UART_READ_EXPECT: endpoint mismatch");
  UNITY_TEST_ASSERT_EQUAL_UINT16(payload_length, sli_cpc_header_get_payload_size(&frame->hdr), lineno,
                                 "TEST_UART_READ_EXPECT: payload length mismatch");

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

/**
 * @brief Drain and release frames and dispatcher work left over from the test.
 */
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

/*******************************************************************************/
/*                           Wire frame encoding                              */
/*******************************************************************************/

/**
 * @brief Encode a test frame into the UART wire format, including both CRCs.
 */
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

  out[0] = (uint8_t)TEST_UART_SOF;
  memcpy(&out[1], &hdr, SLI_CPC_HEADER_SIZE);

  header_crc = sli_cpc_crc(&out[1], SLI_CPC_HEADER_SIZE);
  sli_cpc_u16_to_le(header_crc, &out[1U + SLI_CPC_HEADER_SIZE]);

  if (payload_length == 0U) {
    return total_length;
  }

  memcpy(&out[TEST_UART_HEADER_BLOCK_SIZE], frame->payload, payload_length);

  payload_crc = sli_cpc_crc(frame->payload, payload_length);
  sli_cpc_u16_to_le(payload_crc, &out[TEST_UART_HEADER_BLOCK_SIZE + payload_length]);

  return total_length;
}

/******************************************************************************/
/*                                 Public API                                 */
/******************************************************************************/

/**
 * @brief Initialize the endpoint, RX buffers, loopback, and test driver state.
 */
void uart_setup_at(unsigned int lineno)
{
  sl_cpc_drv_uart_eusart_t *uart_driver = uart_driver_instance();
  sl_status_t status;

  g_bus->initialized = true;

  s_frame_pools_started = false;
#if defined(SL_CATALOG_KERNEL_PRESENT)
  s_thread_suspended = false;
#endif

  status = sl_cpc_ep_init(&s_ep, TEST_UART_EP_ID, EP_RX_BUFFER_SIZE, ep_event_cb, NULL);
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
  s_rx_progress_semaphore = osSemaphoreNew(1U, 0U, NULL);
  UNITY_TEST_ASSERT_NOT_NULL(s_rx_progress_semaphore, lineno, "uart_setup: RX progress semaphore creation failed");
#endif

  install_uart_rx_callback(&uart_driver->uart_handle, lineno);

#if defined(SL_CATALOG_KERNEL_PRESENT)
  if (g_bus->kernel.thread_id != NULL) {
    osThreadSuspend(g_bus->kernel.thread_id);
    s_thread_suspended = true;
  }
#endif

  sli_cpc_bus_process_action(g_bus);
  drain_dispatcher();

  CPC_FRAME_POOLS_START_TEST(g_bus);
  s_frame_pools_started = true;
  clear_pending_reads();
}

/**
 * @brief Release test resources and restore the UART callback and thread state.
 */
void uart_teardown(void)
{
  sl_cpc_drv_uart_eusart_t *uart_driver = uart_driver_instance();

  discard_leftover_rx();

  discard_ep(&s_ep);

  if (s_frame_pools_started) {
    CPC_FRAME_POOLS_END_TEST(g_bus);
    s_frame_pools_started = false;
  }

#if defined(SL_CATALOG_KERNEL_PRESENT)
  if (s_thread_suspended && g_bus->kernel.thread_id != NULL) {
    osThreadResume(g_bus->kernel.thread_id);
    s_thread_suspended = false;
  }
#endif

  restore_uart_rx_callback(&uart_driver->uart_handle);

#if defined(SL_CATALOG_KERNEL_PRESENT)
  if (s_rx_progress_semaphore != NULL) {
    UNITY_TEST_ASSERT_EQUAL_INT(osOK, (int)osSemaphoreDelete(s_rx_progress_semaphore), __LINE__,
                                "uart_teardown: RX progress semaphore deletion failed");
    s_rx_progress_semaphore = NULL;
  }
#endif
}

/**
 * @brief Encode and inject one test frame, returning its wire length.
 */
size_t uart_inject_frame(uint8_t *wire, size_t wire_capacity, cpc_frame_expect_t frame)
{
  size_t wire_length = encode_uart_frame(wire, wire_capacity, &frame);

  uart_inject_bytes(wire, wire_length);

  return wire_length;
}

/**
 * @brief Inject raw wire bytes and wait for the UART RX path to process available data.
 */
void uart_inject_bytes_at(unsigned int lineno, const uint8_t *data, size_t length)
{
  if (length == 0U) {
    return;
  }

  UNITY_TEST_ASSERT_NOT_NULL(data, lineno, "uart_inject_bytes: data is NULL");

  loopback_inject_bytes_at(lineno, data, length);

  s_injected_bytes += length;
  wait_for_uart_rx_progress(lineno);
}

/**
 * @brief Inject a corrupted frame followed by a valid copy of the same frame.
 */
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

/**
 * @brief Inject arbitrary prefix bytes followed by an encoded frame.
 */
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

/**
 * @brief Inject an encoded frame split after its header block.
 */
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

/**
 * @brief Assert that the driver has no frame ready to read.
 */
void test_uart_read_expect_empty_at(unsigned int lineno)
{
  sl_cpc_frame_t *frame = NULL;
  sl_status_t status = pop_frame(&frame);

  if (frame != NULL) {
    release_rx_frame(frame);
  }

  UNITY_TEST_ASSERT_EQUAL_INT(SL_STATUS_EMPTY, status, lineno, "TEST_UART_READ_EXPECT_EMPTY: unexpected frame");
}

/**
 * @brief Read a frame and assert that its endpoint, payload, and CRC state match.
 */
void test_uart_read_expect_at(unsigned int lineno, uint8_t ep_id, const uint8_t *payload, size_t payload_length,
                              bool expect_valid_payload_crc)
{
  sl_cpc_frame_t *frame = NULL;

  UNITY_TEST_ASSERT_EQUAL_INT(SL_STATUS_OK, pop_frame(&frame), lineno, "TEST_UART_READ_EXPECT: frame read failed");
  assert_frame(lineno, ep_id, payload, payload_length, frame, expect_valid_payload_crc);
}
