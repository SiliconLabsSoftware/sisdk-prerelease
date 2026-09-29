/***************************************************************************/ /**
 * @file
 * @brief Mock CPC driver used by the CPC unit tests.
 *
 * Implements `sli_cpc_drv_ops_t` against an in-memory pair of frame
 * queues so that tests can drive the CPC core without any real PHY.
 ******************************************************************************/

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unity_fixture.h>

#include "sl_slist.h"
#include "sl_status.h"

#include "../../../src/sli_cpc.h"
#include "../../../src/sli_cpc_atomic.h"
#include "../../../src/sli_cpc_bus.h"
#include "../../../src/sli_cpc_drv.h"
#include "../../../src/sli_cpc_frame_list.h"
#include "../../../src/sli_cpc_hdr.h"
#include "../../../src/sli_cpc_memory.h"
#include "sl_cpc_buf.h"
#include "sl_cpc_drv_instances.h"

#include "../../../src/unit/frame.h"
#include "sl_cpc_drv_mock.h"
#include "unity_internals.h"

/******************************************************************************/
/*                            Buffer chain helpers                            */
/******************************************************************************/

static sl_cpc_buf_t *buf_chain_next(const sl_cpc_buf_t *buf)
{
  if (buf == NULL || buf->node.node == NULL) {
    return NULL;
  }

  return SL_SLIST_ENTRY(buf->node.node, sl_cpc_buf_t, node);
}

static void buf_chain_gather(const sl_cpc_buf_t *head, void *dst, size_t len)
{
  const sl_cpc_buf_t *cur = head;
  size_t offset = 0;

  while (cur != NULL && offset < len) {
    size_t chunk = cur->len;

    if (chunk > (len - offset)) {
      chunk = len - offset;
    }

    if (chunk > 0 && cur->ptr != NULL) {
      memcpy((uint8_t *)dst + offset, cur->ptr, chunk);
    }

    offset += chunk;
    cur = buf_chain_next(cur);
  }
}

static void buf_chain_scatter(sl_cpc_buf_t *head, const void *src, size_t len)
{
  sl_cpc_buf_t *cur = head;
  size_t offset = 0;

  while (cur != NULL && offset < len) {
    size_t chunk = cur->len;

    if (chunk > (len - offset)) {
      chunk = len - offset;
    }

    if (chunk > 0 && cur->ptr != NULL) {
      memcpy(cur->ptr, (const uint8_t *)src + offset, chunk);
    }

    offset += chunk;
    cur = buf_chain_next(cur);
  }
}

/******************************************************************************/
/*                          Captured TX packet queue                          */
/******************************************************************************/

static mock_cpc_tx_packet_t *capture_packet(sl_cpc_frame_t *frame)
{
  const sli_cpc_hdr_t *cpc_hdr = sli_cpc_frame_get_header(frame);
  const sl_cpc_buf_t *payload = frame->payload;
  size_t payload_size = payload ? payload->tot_len : 0;

  // Single allocation for the metadata, the header copy and the payload
  // bytes thanks to the flexible array.
  mock_cpc_tx_packet_t *packet = malloc(sizeof(*packet) + payload_size);
  if (packet == NULL) {
    return NULL;
  }

  packet->node.node = NULL;
  packet->header = *cpc_hdr;
  if (payload_size) {
    buf_chain_gather(payload, packet->payload, payload_size);
  }

  return packet;
}

static void tx_queue_push_back(sl_cpc_drv_mock_t *drv, mock_cpc_tx_packet_t *packet)
{
  sl_slist_push_back(&drv->tx_head, &packet->node);
}

static mock_cpc_tx_packet_t *tx_queue_pop_front(sl_cpc_drv_mock_t *drv)
{
  sl_slist_node_t *node = sl_slist_pop(&drv->tx_head);
  if (node == NULL) {
    return NULL;
  }

  return SL_SLIST_ENTRY(node, mock_cpc_tx_packet_t, node);
}

static void tx_queue_drain(sl_cpc_drv_mock_t *drv)
{
  sl_slist_node_t *node;

  while ((node = sl_slist_pop(&drv->tx_head)) != NULL) {
    free(SL_SLIST_ENTRY(node, mock_cpc_tx_packet_t, node));
  }
}

/******************************************************************************/
/*                      sli_cpc_drv_ops_t implementation                      */
/******************************************************************************/

static sl_status_t mock_init(sl_cpc_bus_t *bus)
{
  sl_cpc_drv_mock_t *drv = container_of(bus, sl_cpc_drv_mock_t, bus);

  drv->init_count++;

  return SL_STATUS_OK;
}

static sl_status_t mock_start_rx(sl_cpc_bus_t *bus)
{
  sl_cpc_drv_mock_t *drv = container_of(bus, sl_cpc_drv_mock_t, bus);

  drv->rx_enabled = true;
  drv->start_rx_count++;

  return SL_STATUS_OK;
}

static sl_status_t mock_read(sl_cpc_bus_t *bus, sli_cpc_frame_list_t *frames)
{
  sl_cpc_drv_mock_t *drv = container_of(bus, sl_cpc_drv_mock_t, bus);
  sl_status_t status = SL_STATUS_EMPTY;

  MCU_DECLARE_IRQ_STATE;
  MCU_ENTER_ATOMIC();

  if (!sli_cpc_frame_list_empty(&drv->rx_pending_queue)) {
    sli_cpc_frame_list_extend(frames, &drv->rx_pending_queue);
    status = SL_STATUS_OK;
  }

  MCU_EXIT_ATOMIC();

  return status;
}

static uint32_t mock_write(sl_cpc_bus_t *bus, sli_cpc_frame_list_t *frames)
{
  sl_cpc_drv_mock_t *drv = container_of(bus, sl_cpc_drv_mock_t, bus);
  sli_cpc_frame_list_t completed_list;
  uint32_t available = drv->available_write_frame_slots;
  uint32_t pending = sli_cpc_frame_list_get_len(frames);
  uint32_t queued = 0;

  if (available == 0 || pending == 0) {
    return 0;
  }

  sli_cpc_frame_list_init(&completed_list);

  while (queued < pending && queued < available) {
    sl_cpc_frame_t *frame;

    MCU_ATOMIC_SECTION(frame = sli_cpc_frame_list_pop(frames);)
    if (frame == NULL) {
      break;
    }

    if (drv->tx_validate_fn != NULL) {
      int ret = drv->tx_validate_fn(frame);
      SLI_CPC_ASSERT(ret == 0);
    }

    // Capture an independent copy of the on-wire frame so the test can
    // inspect it after the core has reused the source frame.
    mock_cpc_tx_packet_t *packet = capture_packet(frame);
    if (packet != NULL) {
      MCU_ATOMIC_SECTION(tx_queue_push_back(drv, packet);)
    }

    if (drv->defer_tx_complete) {
      sli_cpc_frame_list_push_back(&drv->held_tx_frames, frame);
      queued++;
      continue;
    }

    // Hand the frame back to the core as transmitted; the core will
    // recycle it through its usual TX-completion path.
    sli_cpc_frame_list_push_back(&completed_list, frame);
    queued++;
  }

  if (!drv->defer_tx_complete) {
    sli_cpc_bus_notify_tx_data_by_drv(&drv->bus, &completed_list);
  }

  return queued;
}

static uint32_t mock_get_available_write_frame_slots(sl_cpc_bus_t *bus)
{
  const sl_cpc_drv_mock_t *drv = container_of(bus, sl_cpc_drv_mock_t, bus);

  return drv->available_write_frame_slots;
}

static void mock_on_rx_frame_free(sl_cpc_bus_t *bus)
{
  sl_cpc_drv_mock_t *drv = container_of(bus, sl_cpc_drv_mock_t, bus);

  drv->on_rx_frame_free_count++;
}

static void mock_get_local_capabilities(sl_cpc_bus_t *bus, const void **caps_p, uint16_t *caps_size_p)
{
  const sl_cpc_drv_mock_t *drv = container_of(bus, sl_cpc_drv_mock_t, bus);

  if (caps_p != NULL) {
    *caps_p = drv->local_capabilities;
  }
  if (caps_size_p != NULL) {
    *caps_size_p = drv->local_capabilities_size;
  }
}

static sl_status_t mock_set_remote_capabilities(sl_cpc_bus_t *bus, const void *caps, uint16_t caps_size)
{
  sl_cpc_drv_mock_t *drv = container_of(bus, sl_cpc_drv_mock_t, bus);

  drv->remote_capabilities = caps;
  drv->remote_capabilities_size = caps_size;

  return SL_STATUS_OK;
}

/******************************************************************************/
/*                                 Public API                                 */
/******************************************************************************/

sl_status_t sl_cpc_drv_mock_init(sl_cpc_drv_mock_t *drv, const sl_cpc_drv_mock_config_t *cfg,
                                 const sl_cpc_bus_config_t *bus_cfg)
{
  static const sli_cpc_drv_ops_t ops = {
    .init = &mock_init,
    .start_rx = &mock_start_rx,
    .read = &mock_read,
    .write = &mock_write,
    .get_available_write_frame_slots = &mock_get_available_write_frame_slots,
    .on_rx_frame_free = &mock_on_rx_frame_free,
    .get_local_capabilities = &mock_get_local_capabilities,
    .set_remote_capabilities = &mock_set_remote_capabilities,
  };

  sl_status_t status;

  SLI_CPC_ASSERT(drv != NULL);

  if (cfg == NULL || bus_cfg == NULL) {
    return SL_STATUS_NULL_POINTER;
  }

  memset(drv, 0, sizeof(*drv));

  sl_slist_init(&drv->tx_head);
  sli_cpc_frame_list_init(&drv->rx_pending_queue);
  sli_cpc_frame_list_init(&drv->held_tx_frames);
  drv->available_write_frame_slots = UINT32_MAX;

  status = sli_cpc_bus_init(&drv->bus, bus_cfg, &ops);
  if (status != SL_STATUS_OK) {
    return status;
  }

  drv->hw_init_count++;
  return SL_STATUS_OK;
}

void mock_cpc_drv_deinit(sl_cpc_drv_mock_t *drv)
{
  SLI_CPC_ASSERT(drv != NULL);

  // Release any captured packets the test never consumed.
  tx_queue_drain(drv);

  sli_cpc_frame_list_init(&drv->held_tx_frames);

  sli_cpc_bus_deinit(&drv->bus);

  memset(drv, 0, sizeof(*drv));
}

mock_cpc_tx_packet_t *mock_cpc_drv_pop_tx_packet(sl_cpc_drv_mock_t *drv)
{
  mock_cpc_tx_packet_t *packet;

  MCU_ATOMIC_SECTION(packet = tx_queue_pop_front(drv);)

  return packet;
}

void mock_cpc_tx_packet_free(mock_cpc_tx_packet_t **packet)
{
  if (packet == NULL || *packet == NULL) {
    return;
  }
  free(*packet);
  *packet = NULL;
}

uint32_t mock_cpc_drv_pending_tx_count(sl_cpc_drv_mock_t *drv)
{
  sl_slist_node_t *it;
  uint32_t count = 0;
  MCU_DECLARE_IRQ_STATE;

  MCU_ENTER_ATOMIC();

  if (drv->tx_head == NULL) {
    goto exit_atomic;
  }

  SL_SLIST_FOR_EACH(drv->tx_head, it)
  {
    count++;
  }

exit_atomic:
  MCU_EXIT_ATOMIC();

  return count;
}

void mock_cpc_drv_push_rx_frame(sl_cpc_drv_mock_t *drv, sl_cpc_frame_t *frame)
{
  MCU_ATOMIC_SECTION(sli_cpc_frame_list_push_back(&drv->rx_pending_queue, frame);)

  sli_cpc_bus_signal_event(&drv->bus, SLI_CPC_SIGNAL_RX);
}

// Range-check a single header field against `[0, max]` and fail the current
// Unity test on violation. Centralized so every field gets the same shape of
// error message.
static void inject_check_range(unsigned int lineno, const char *field, long value, long max)
{
  char err_msg[96];
  snprintf(err_msg, sizeof(err_msg), "error field: %s", field);

  UNITY_TEST_ASSERT_GREATER_OR_EQUAL_INT32(0, value, lineno, err_msg);
  UNITY_TEST_ASSERT_SMALLER_OR_EQUAL_INT32(max, value, lineno, err_msg);
}

void mock_cpc_drv_inject_frame(unsigned int lineno, sl_cpc_drv_mock_t *drv, const cpc_frame_expect_t *expect)
{
  sl_status_t status;
  cpc_frame_expect_t normalized;

  UNITY_TEST_ASSERT_NOT_NULL(drv, lineno, "mock_cpc_drv_inject_frame: drv is NULL");
  UNITY_TEST_ASSERT_NOT_NULL(expect, lineno, "mock_cpc_drv_inject_frame: expect is NULL");

  // Copy before normalizing: expect is const input; do not mutate the caller's descriptor.
  normalized = *expect;
  normalized.seq %= 256;
  normalized.ack %= 256;

  inject_check_range(lineno, "len", (long)normalized.len, (long)UINT16_MAX);
  inject_check_range(lineno, "dst", (long)normalized.dst, (long)UINT8_MAX);
  inject_check_range(lineno, "wnd", (long)normalized.wnd, (long)UINT8_MAX);

  if (normalized.payload != NULL) {
    UNITY_TEST_ASSERT_GREATER_THAN_INT(0, (int)normalized.len, lineno,
                                       "mock_cpc_drv_inject_frame: payload requires frame len > 0");
  }

  sl_cpc_frame_t *frame = sli_cpc_frame_new(&drv->bus, true);
  UNITY_TEST_ASSERT_NOT_NULL(frame, lineno, "mock_cpc_drv_inject_frame: RX frame pool exhausted");

  sli_cpc_hdr_t *hdr = sli_cpc_frame_get_header(frame);

  frame_to_header(&normalized, hdr);

  if (sli_cpc_header_get_payload_size(hdr)) {
    UNITY_TEST_ASSERT_NOT_NULL(normalized.payload, lineno,
                               "mock_cpc_drv_inject_frame: payload ptr is NULL but len is non-zero");
  }

  status = sli_cpc_alloc_rx_payload(&drv->bus, frame);
  if (status == SL_STATUS_OK && sli_cpc_header_get_payload_size(hdr)) {
    buf_chain_scatter(frame->payload, normalized.payload, (uint16_t)normalized.len);
    frame->payload_csum_is_valid = true;
  }

  mock_cpc_drv_push_rx_frame(drv, frame);
}

void mock_cpc_drv_set_available_write_frame_slots(sl_cpc_drv_mock_t *drv, uint32_t slots)
{
  drv->available_write_frame_slots = slots;
}

void mock_cpc_drv_set_defer_tx_complete(sl_cpc_drv_mock_t *drv, bool defer)
{
  drv->defer_tx_complete = defer;
}

unsigned int mock_cpc_drv_complete_held_tx(sl_cpc_drv_mock_t *drv, int count)
{
  sli_cpc_frame_list_t completed_list;
  sl_cpc_frame_t *frame;
  unsigned int i = 0;
  unsigned int max_iterations = count > 0 ? (unsigned int)count : UINT32_MAX;

  sli_cpc_frame_list_init(&completed_list);

  while (i < max_iterations) {
    MCU_ATOMIC_SECTION(frame = sli_cpc_frame_list_pop(&drv->held_tx_frames);)
    if (frame == NULL) {
      break;
    }

    sli_cpc_frame_list_push_back(&completed_list, frame);
    i++;
  }

  if (i) {
    sli_cpc_bus_notify_tx_data_by_drv(&drv->bus, &completed_list);
  }

  return i;
}

uint32_t mock_cpc_drv_held_tx_count(const sl_cpc_drv_mock_t *drv)
{
  return sli_cpc_frame_list_get_len(&drv->held_tx_frames);
}

void mock_cpc_drv_set_tx_validation(sl_cpc_drv_mock_t *drv, int (*validate_fn)(sl_cpc_frame_t *))
{
  drv->tx_validate_fn = validate_fn;
}

void mock_cpc_drv_set_local_capabilities(sl_cpc_drv_mock_t *drv, const void *caps, uint16_t caps_size)
{
  drv->local_capabilities = caps;
  drv->local_capabilities_size = caps_size;
}

sl_cpc_drv_mock_t sl_cpc_drv_mock_instances[SL_CPC_DRV_MOCK_INSTANCES_COUNT];
