/***************************************************************************//**
 * @file
 * @brief Bluetooth HCI transport over Greybus CPC (RTOS only).
 ******************************************************************************/

#include <stdint.h>
#include <string.h>

#include <cmsis_os2.h>

#include "sl_common.h"
#include "sl_status.h"

#include "sl_cpc_instance_handles.h"
#include "sli_cpc.h"
#include "sli_cpc_buffer_handle.h"
#include "sli_cpc_header.h"
#include "sli_cpc_instance.h"
#include "sli_cpc_message.h"
#include "sli_greybus_buffer_handle.h"
#include "sli_greybus_message.h"
#include "sl_greybus.h"

#include "sl_hci_common_transport.h"
#include "sli_hci_cpc.h"
#include "sli_hci_cpc_rtos.h"

#define SL_HCI_GREYBUS_ENDPOINT_ID 1

#define SL_HCI_GREYBUS_RX_SIZE 1536
#define SL_HCI_GREYBUS_RX_BUF_COUNT 3

static __ALIGNED(SL_GB_BUFFER_ALIGNMENT) uint8_t hci_cpc_tx_staging[SL_HCI_GREYBUS_RX_SIZE];

typedef struct {
  sl_cpc_buf_t buf;
  __ALIGNED(SL_GB_BUFFER_ALIGNMENT) uint8_t data[SL_HCI_GREYBUS_RX_SIZE];
} sl_hci_greybus_rx_slot_t;

typedef enum {
  SLI_HCI_CPC_STATE_DISCONNECTED = 0,
  SLI_HCI_CPC_STATE_CONNECTING   = 1,
  SLI_HCI_CPC_STATE_CONNECTED    = 2,
} sli_hci_cpc_state_t;

static sli_hci_cpc_state_t cpc_state = SLI_HCI_CPC_STATE_DISCONNECTED;
static sl_cpc_ep_t *ep = NULL;
static sli_cpc_buffer_handle_list_t rx_pending;
static sl_cpc_buf_t *current_rx_buf = NULL;
static bool transport_ready_signaled;
static sl_hci_greybus_rx_slot_t rx_slots[SL_HCI_GREYBUS_RX_BUF_COUNT];
static bool initialized;
static bool cpc_link_error_pending;
static osMutexId_t rx_pending_mutex;
__ALIGNED(4) static uint8_t rx_pending_mutex_cb[osMutexCbSize];
static const osMutexAttr_t rx_pending_mutex_attr = {
  .name = "sli_hci_cpc_rx",
  .attr_bits = 0U,
  .cb_mem = rx_pending_mutex_cb,
  .cb_size = sizeof(rx_pending_mutex_cb),
};

static void hci_cpc_rx_lock(void)
{
  if (rx_pending_mutex != NULL) {
    (void)osMutexAcquire(rx_pending_mutex, osWaitForever);
  }
}

static void hci_cpc_rx_unlock(void)
{
  if (rx_pending_mutex != NULL) {
    (void)osMutexRelease(rx_pending_mutex);
  }
}

static bool hci_cpc_ep_is_open(const sl_cpc_ep_t *endpoint)
{
  return (endpoint != NULL) && (endpoint->state == SLI_CPC_STATE_OPEN);
}

static void hci_cpc_mark_disconnected(void)
{
  cpc_state = SLI_HCI_CPC_STATE_DISCONNECTED;
  transport_ready_signaled = false;
}

static void hci_cpc_signal_transport_ready(void)
{
  if (transport_ready_signaled || !hci_cpc_ep_is_open(ep)) {
    return;
  }
  transport_ready_signaled = true;
  hci_common_transport_transmit_reconnected();
}

// CPC reported a link fault; do not treat a still-open endpoint as connected.
static void hci_cpc_mark_link_error(void)
{
  cpc_link_error_pending = true;
  hci_cpc_mark_disconnected();
}

static void hci_cpc_mark_connected_ready(void)
{
  cpc_link_error_pending = false;
  cpc_state = SLI_HCI_CPC_STATE_CONNECTED;
  hci_cpc_signal_transport_ready();
}

static void hci_cpc_push_rx_buf_to_ep(sl_cpc_ep_t *endpoint, sl_cpc_buf_t *buf)
{
  if (endpoint == NULL || buf == NULL) {
    return;
  }

  sl_cpc_endpoint_push_rx_buffer(endpoint, buf);
}

// Return app-held RX buffers to CPC; requires endpoint != NULL.
static void hci_cpc_reclaim_rx_buffers(sl_cpc_ep_t *endpoint)
{
  sl_cpc_buf_t *buf;

  if (endpoint == NULL) {
    return;
  }

  hci_cpc_rx_lock();

  if (current_rx_buf != NULL) {
    hci_cpc_push_rx_buf_to_ep(endpoint, current_rx_buf);
    current_rx_buf = NULL;
  }

  while (!sli_cpc_buffer_handle_list_empty(&rx_pending)) {
    buf = (sl_cpc_buf_t *)sli_cpc_buffer_handle_list_pop(&rx_pending);
    hci_cpc_push_rx_buf_to_ep(endpoint, buf);
  }

  hci_cpc_rx_unlock();
}

// Drop HCI RX queued locally; does not push to CPC (endpoint may be gone). */
static void hci_cpc_reset_local_rx_queues(void)
{
  hci_cpc_rx_lock();

  while (!sli_cpc_buffer_handle_list_empty(&rx_pending)) {
    (void)sli_cpc_buffer_handle_list_pop(&rx_pending);
  }
  current_rx_buf = NULL;

  hci_cpc_rx_unlock();
}

static sl_status_t hci_cpc_teardown_endpoint(void)
{
  sl_cpc_ep_t *inst_ep;
  sl_status_t status;

  inst_ep = sli_cpc_instance_find_endpoint_from_id(&g_instance, SL_HCI_GREYBUS_ENDPOINT_ID);
  ep = inst_ep;

  if (inst_ep == NULL) {
    ep = NULL;
    hci_cpc_reset_local_rx_queues();
    return SL_STATUS_NOT_FOUND;
  }

  hci_cpc_reclaim_rx_buffers(inst_ep);

  if (inst_ep->state == SLI_CPC_STATE_CLOSED) {
    status = sl_cpc_endpoint_free(inst_ep);
    if (status == SL_STATUS_OK) {
      ep = NULL;
    }
    hci_cpc_reset_local_rx_queues();
    return status;
  }

  status = sli_cpc_endpoint_terminate(inst_ep, 0);
  if (status == SL_STATUS_IN_PROGRESS) {
    return SL_STATUS_IN_PROGRESS;
  }
  if (status != SL_STATUS_OK) {
    return status;
  }

  if (inst_ep->state == SLI_CPC_STATE_CLOSED) {
    status = sl_cpc_endpoint_free(inst_ep);
    if (status == SL_STATUS_OK) {
      ep = NULL;
    }
  }

  hci_cpc_reset_local_rx_queues();
  return status;
}

static sl_status_t hci_cpc_alloc_tx_message(uint16_t len,
                                            uint8_t *payload,
                                            sl_cpc_frame_t **out_msg)
{
  sli_gb_buffer_handle_t *handle;
  sli_cpc_message_t *cpc_msg;
  sli_cpc_header_t *cpc_hdr;
  sl_cpc_frame_t *msg;

  msg = sli_gb_message_new(ep->inst->data_link, SLI_GB_MESSAGE_ALLOC_TX);
  if (msg == NULL) {
    return SL_STATUS_NO_MORE_RESOURCE;
  }

  if (len > 0U) {
    handle = sli_gb_buffer_handle_new();
    if (handle == NULL) {
      sl_cpc_msg_free(&msg);
      return SL_STATUS_ALLOCATION_FAILED;
    }
    handle->data = payload;
    handle->size = len;
    msg->payload = handle;
  }

  cpc_msg = sli_cpc_message_get(msg);
  cpc_hdr = sli_cpc_message_get_header(cpc_msg);
  sli_cpc_header_set_payload_size(cpc_hdr, len);

  *out_msg = msg;
  return SL_STATUS_OK;
}

static void hci_cpc_free_tx_message(sl_cpc_frame_t **msg)
{
  if (msg != NULL && *msg != NULL) {
    if ((*msg)->payload != NULL) {
      sli_gb_buffer_handle_free(&(*msg)->payload);
    }
    sl_cpc_msg_free(msg);
  }
}

static void on_endpoint_event(sl_cpc_ep_t *endpoint,
                              sl_cpc_event_type_t type,
                              sl_cpc_event_t *event,
                              void *arg)
{
  (void)endpoint;
  (void)arg;

  switch (type) {
    case SL_CPC_EVENT_ON_FRAME_RECEIVED:
      hci_cpc_rx_lock();
      sli_cpc_buffer_handle_list_push(&rx_pending, event->frame_received.buf);
      hci_cpc_rx_unlock();
      sli_hci_cpc_rtos_on_transport_notify(SL_HCI_GREYBUS_ENDPOINT_ID, NULL);
      break;

    case SL_CPC_EVENT_ON_WRITE_COMPLETED:
      if (event->write_completed.frame != NULL) {
        sl_cpc_frame_t *frame = event->write_completed.frame;
        if (frame->payload != NULL) {
          sli_gb_buffer_handle_free(&frame->payload);
        }
        frame->payload = NULL;
        sl_cpc_msg_free(&frame);
      }
      hci_common_transport_transmit_complete(event->write_completed.status);
      if (event->write_completed.status != SL_STATUS_OK) {
        hci_common_transport_transmit_reconnected();
      }
      break;

    case SL_CPC_EVENT_ON_ERROR:
      hci_cpc_mark_link_error();
      sli_hci_cpc_rtos_on_transport_notify(SL_HCI_GREYBUS_ENDPOINT_ID, NULL);
      break;

    default:
      break;
  }
}

static sl_status_t hci_cpc_open_transport(void)
{
  sl_cpc_ep_t *inst_ep;
  sl_status_t status;

  inst_ep = sli_cpc_instance_find_endpoint_from_id(&g_instance, SL_HCI_GREYBUS_ENDPOINT_ID);
  if (inst_ep != NULL) {
    ep = inst_ep;
    if (hci_cpc_ep_is_open(ep)) {
      if (cpc_link_error_pending) {
        return SL_STATUS_NOT_READY;
      }
      hci_cpc_mark_connected_ready();
      return SL_STATUS_OK;
    }
    return SL_STATUS_NOT_READY;
  }

  if (hci_cpc_ep_is_open(ep)) {
    if (cpc_link_error_pending) {
      return SL_STATUS_NOT_READY;
    }
    hci_cpc_mark_connected_ready();
    return SL_STATUS_OK;
  }

  hci_cpc_reset_local_rx_queues();

  status = sl_cpc_alloc_endpoint(&g_instance, &ep);
  if (status != SL_STATUS_OK) {
    ep = NULL;
    return status;
  }

  status = sl_cpc_endpoint_open(ep,
                                &g_instance,
                                SL_HCI_GREYBUS_ENDPOINT_ID,
                                SL_HCI_GREYBUS_RX_SIZE);
  if (status != SL_STATUS_OK) {
    (void)sl_cpc_endpoint_free(ep);
    ep = NULL;
    return status;
  }

  for (uint32_t i = 0; i < SL_HCI_GREYBUS_RX_BUF_COUNT; i++) {
    rx_slots[i].buf.data = rx_slots[i].data;
    rx_slots[i].buf.size = SL_HCI_GREYBUS_RX_SIZE;
    rx_slots[i].buf.node.node = NULL;
    sl_cpc_endpoint_push_rx_buffer(ep, &rx_slots[i].buf);
  }

  sl_cpc_endpoint_set_event_cb(ep, on_endpoint_event, NULL);

  hci_cpc_mark_connected_ready();
  return SL_STATUS_OK;
}

sl_status_t sli_hci_cpc_init(void)
{
  if (!initialized) {
    if (rx_pending_mutex == NULL) {
      rx_pending_mutex = osMutexNew(&rx_pending_mutex_attr);
      if (rx_pending_mutex == NULL) {
        EFM_ASSERT(rx_pending_mutex != NULL);
        return SL_STATUS_FAIL;
      }
    }
    sli_cpc_buffer_handle_list_init(&rx_pending);
    current_rx_buf = NULL;
    cpc_state = SLI_HCI_CPC_STATE_DISCONNECTED;
    cpc_link_error_pending = false;
    transport_ready_signaled = false;
    initialized = true;
  }

  (void)hci_cpc_open_transport();
  return SL_STATUS_OK;
}

int sli_hci_cpc_new_data(void)
{
  int has_data;

  hci_cpc_rx_lock();
  has_data = sli_cpc_buffer_handle_list_empty(&rx_pending) ? 0 : 1;
  hci_cpc_rx_unlock();
  return has_data;
}

int sli_hci_cpc_read(uint8_t **read_buf)
{
  sl_cpc_buf_t *buf;

  hci_cpc_rx_lock();
  buf = (sl_cpc_buf_t *)sli_cpc_buffer_handle_list_pop(&rx_pending);
  hci_cpc_rx_unlock();

  if (buf == NULL || buf->data == NULL) {
    return 0;
  }

  current_rx_buf = buf;
  *read_buf = (uint8_t *)buf->data;
  // CPC sets buf->size to the received payload length in sli_cpc_alloc_rx_payload().
  return (int)buf->size;
}

void sli_hci_cpc_free(void *buf)
{
  sl_cpc_ep_t *inst_ep;

  inst_ep = sli_cpc_instance_find_endpoint_from_id(&g_instance, SL_HCI_GREYBUS_ENDPOINT_ID);
  if (inst_ep != NULL) {
    ep = inst_ep;
  }

  if (ep == NULL) {
    hci_cpc_rx_lock();
    current_rx_buf = NULL;
    hci_cpc_rx_unlock();
    return;
  }

  hci_cpc_rx_lock();
  if (current_rx_buf != NULL && current_rx_buf->data == buf) {
    sl_cpc_endpoint_push_rx_buffer(ep, current_rx_buf);
    current_rx_buf = NULL;
    hci_cpc_rx_unlock();
    return;
  }
  hci_cpc_rx_unlock();

  for (uint32_t i = 0; i < SL_HCI_GREYBUS_RX_BUF_COUNT; i++) {
    if (rx_slots[i].data == buf) {
      sl_cpc_endpoint_push_rx_buffer(ep, &rx_slots[i].buf);
      return;
    }
  }
}

void sli_hci_cpc_on_link_down(void)
{
  sl_cpc_ep_t *inst_ep;

  inst_ep = sli_cpc_instance_find_endpoint_from_id(&g_instance, SL_HCI_GREYBUS_ENDPOINT_ID);
  if (inst_ep != NULL) {
    ep = inst_ep;
  }

  if (hci_cpc_ep_is_open(ep)) {
    hci_cpc_reclaim_rx_buffers(ep);
  } else {
    hci_cpc_reset_local_rx_queues();
  }
}

sl_status_t sli_hci_cpc_write(uint8_t *data, uint16_t len)
{
  sl_status_t status;
  sl_cpc_frame_t *msg = NULL;

  if (!hci_cpc_ep_is_open(ep) || cpc_link_error_pending
      || cpc_state != SLI_HCI_CPC_STATE_CONNECTED) {
    return SL_STATUS_NOT_READY;
  }
  if (data == NULL || len == 0U) {
    return SL_STATUS_INVALID_PARAMETER;
  }
  if (len > sizeof(hci_cpc_tx_staging)) {
    return SL_STATUS_INVALID_PARAMETER;
  }

  memcpy(hci_cpc_tx_staging, data, len);

  status = hci_cpc_alloc_tx_message(len, hci_cpc_tx_staging, &msg);
  if (status != SL_STATUS_OK) {
    return status;
  }

  status = sl_cpc_ep_send(ep, (sl_cpc_buf_t *)msg->payload, msg, hci_cpc_tx_staging);
  if (status != SL_STATUS_OK) {
    hci_cpc_free_tx_message(&msg);
  }
  return status;
}

bool sli_hci_cpc_is_connected(void)
{
  sl_cpc_ep_t *inst_ep;

  inst_ep = sli_cpc_instance_find_endpoint_from_id(&g_instance, SL_HCI_GREYBUS_ENDPOINT_ID);
  if (inst_ep != NULL) {
    ep = inst_ep;
  }

  if (!hci_cpc_ep_is_open(ep) || cpc_link_error_pending) {
    return false;
  }

  if (cpc_state != SLI_HCI_CPC_STATE_CONNECTED) {
    hci_cpc_mark_connected_ready();
  }

  return true;
}

static bool hci_cpc_endpoint_registered(void)
{
  return sli_cpc_instance_find_endpoint_from_id(&g_instance, SL_HCI_GREYBUS_ENDPOINT_ID) != NULL;
}

void sli_hci_cpc_reconnect(void)
{
  sl_status_t status;
  sl_cpc_ep_t *inst_ep;

  if (sli_hci_cpc_is_connected()) {
    return;
  }

  hci_cpc_mark_disconnected();

  inst_ep = sli_cpc_instance_find_endpoint_from_id(&g_instance, SL_HCI_GREYBUS_ENDPOINT_ID);
  if (inst_ep != NULL) {
    if (hci_cpc_ep_is_open(inst_ep) && !cpc_link_error_pending) {
      ep = inst_ep;
      hci_cpc_mark_connected_ready();
      return;
    }

    status = hci_cpc_teardown_endpoint();
    if (status == SL_STATUS_IN_PROGRESS) {
      cpc_state = SLI_HCI_CPC_STATE_CONNECTING;
      return;
    }
    if (status != SL_STATUS_OK && status != SL_STATUS_NOT_FOUND) {
      return;
    }

    if (hci_cpc_endpoint_registered()) {
      cpc_state = SLI_HCI_CPC_STATE_CONNECTING;
      return;
    }
  } else {
    hci_cpc_reset_local_rx_queues();
  }

  hci_cpc_open_transport();
}
