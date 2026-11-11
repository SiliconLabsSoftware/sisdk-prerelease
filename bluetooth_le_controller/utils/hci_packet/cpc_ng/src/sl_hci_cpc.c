/***************************************************************************//**
 * @file
 * @brief Bluetooth HCI transport over CPC-NG (RTOS only).
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
#include <stdint.h>
#include <string.h>

#include <cmsis_os2.h>

#include "sl_assert.h"
#include "sl_component_catalog.h"
#include "sl_status.h"

#include "sl_cpc_buf.h"
#include "sl_cpc_bus_instances.h"
#include "sl_cpc_ep.h"
#include "sl_cpc_frame.h"
#include "sl_cpc_msgq.h"
#include "sl_slist.h"

#include "sl_hci_common_transport.h"
#include "sli_hci_cpc.h"
#include "sli_hci_cpc_rtos.h"

// HCI type (1) + ACL header (4) + max ACL (251) fits in 256; use 512 for headroom / events.
#define SL_HCI_CPC_NG_RX_SIZE SLI_HCI_CPC_NG_MAX_H4_PACKET_SIZE
#define SL_HCI_CPC_NG_RX_BUF_COUNT 4U
#define SL_HCI_CPC_NG_TX_SLOT_COUNT 4U
#define SL_HCI_CPC_NG_TX_MAX_SIZE SL_HCI_CPC_NG_RX_SIZE

typedef struct {
  sl_cpc_buf_t buf;
  alignas(SL_CPC_BUF_MIN_ALIGNMENT) uint8_t data[SL_HCI_CPC_NG_RX_SIZE];
} sli_hci_cpc_ng_rx_slot_t;

typedef struct {
  sl_cpc_buf_t buf;
  sl_cpc_frame_t frame;
  alignas(SL_CPC_BUF_MIN_ALIGNMENT) uint8_t data[SL_HCI_CPC_NG_TX_MAX_SIZE];
  bool in_use;
} sli_hci_cpc_ng_tx_slot_t;

typedef enum {
  SLI_HCI_CPC_STATE_DISCONNECTED = 0,
  SLI_HCI_CPC_STATE_CONNECTING   = 1,
  SLI_HCI_CPC_STATE_CONNECTED    = 2,
} sli_hci_cpc_state_t;

static sli_hci_cpc_state_t cpc_state = SLI_HCI_CPC_STATE_DISCONNECTED;
static sl_cpc_ep_t ep;
static sl_cpc_msgq_t rx_msgq;
static sli_hci_cpc_ng_rx_slot_t rx_slots[SL_HCI_CPC_NG_RX_BUF_COUNT];
static sl_cpc_buf_t *current_rx_buf = NULL;
static bool cpc_link_error_pending = false;
static bool ep_closing = false;
static bool ep_closed_ready = true;
static bool link_close_done = true;

static osMutexId_t rx_mutex;
__ALIGNED(4) static uint8_t rx_mutex_cb[osMutexCbSize];
static const osMutexAttr_t rx_mutex_attr = {
  .name = "sli_hci_cpc_rx",
  .attr_bits = 0U,
  .cb_mem = rx_mutex_cb,
  .cb_size = sizeof(rx_mutex_cb),
};

static sli_hci_cpc_ng_tx_slot_t tx_slots[SL_HCI_CPC_NG_TX_SLOT_COUNT];

static uint8_t rx_staging[SL_HCI_CPC_NG_RX_SIZE * SL_HCI_CPC_NG_RX_BUF_COUNT];

static void hci_cpc_tx_slots_reset(void)
{
  for (uint32_t i = 0; i < SL_HCI_CPC_NG_TX_SLOT_COUNT; i++) {
    tx_slots[i].in_use = false;
  }
}

static void hci_cpc_tx_slot_release_on_send_done(const sl_cpc_ep_event_send_done_t *send_done)
{
  if (send_done == NULL) {
    return;
  }

  for (uint32_t i = 0; i < SL_HCI_CPC_NG_TX_SLOT_COUNT; i++) {
    if (&tx_slots[i].frame == send_done->frame || &tx_slots[i].buf == send_done->buf) {
      tx_slots[i].in_use = false;
      return;
    }
  }
}

uint16_t sli_hci_cpc_h4_packet_size(const uint8_t *data, uint16_t len)
{
  if (data == NULL || len == 0U) {
    return 0U;
  }

  switch (data[0]) {
    case 0x01U:
      return len >= 4U ? (uint16_t)(4U + data[3]) : 0U;
    case 0x02U:
      return len >= 5U ? (uint16_t)(5U + ((uint16_t)data[3] | ((uint16_t)data[4] << 8))) : 0U;
    case 0x03U:
      return len >= 4U ? (uint16_t)(4U + data[3]) : 0U;
    case 0x04U:
      return len >= 3U ? (uint16_t)(3U + data[2]) : 0U;
    default:
      return 0U;
  }
}

static sl_cpc_buf_t *hci_cpc_buf_next(sl_cpc_buf_t *buf)
{
  if (buf == NULL || buf->node.node == NULL) {
    return NULL;
  }

  return SL_SLIST_ENTRY(buf->node.node, sl_cpc_buf_t, node);
}

static bool hci_cpc_buf_is_contiguous(const sl_cpc_buf_t *head)
{
  return (head != NULL && head->node.node == NULL && head->len == head->tot_len);
}

static const uint8_t *hci_cpc_payload_view(sl_cpc_buf_t *head, uint16_t *out_len)
{
  sl_cpc_buf_t *cur;
  uint16_t offset;

  if (head == NULL || head->ptr == NULL || head->tot_len == 0) {
    return NULL;
  }

  if (hci_cpc_buf_is_contiguous(head)) {
    *out_len = head->tot_len;
    return (const uint8_t *)head->ptr;
  }

  if (head->tot_len > sizeof(rx_staging)) {
    return NULL;
  }

  offset = 0;
  for (cur = head; cur != NULL; cur = hci_cpc_buf_next(cur)) {
    if (cur->ptr == NULL || offset + cur->len > head->tot_len || offset + cur->len > sizeof(rx_staging)) {
      return NULL;
    }
    memcpy(rx_staging + offset, cur->ptr, cur->len);
    offset += cur->len;
  }

  if (offset != head->tot_len) {
    return NULL;
  }

  *out_len = head->tot_len;
  return rx_staging;
}

static void hci_cpc_rx_lock(void)
{
  if (rx_mutex != NULL) {
    (void)osMutexAcquire(rx_mutex, osWaitForever);
  }
}

static void hci_cpc_rx_unlock(void)
{
  if (rx_mutex != NULL) {
    (void)osMutexRelease(rx_mutex);
  }
}

static sl_status_t hci_cpc_push_rx_buf(sl_cpc_buf_t *buf)
{
  sl_status_t status;

  if (buf == NULL) {
    return SL_STATUS_NULL_POINTER;
  }

  status = sl_cpc_ep_push_recv_buf(&ep, buf);
  EFM_ASSERT(status == SL_STATUS_OK);
  return status;
}

static sl_status_t hci_cpc_push_all_rx_bufs(void)
{
  sl_status_t status;

  for (uint32_t i = 0; i < SL_HCI_CPC_NG_RX_BUF_COUNT; i++) {
    sl_cpc_buf_init(&rx_slots[i].buf, rx_slots[i].data, SL_HCI_CPC_NG_RX_SIZE);
    status = hci_cpc_push_rx_buf(&rx_slots[i].buf);
    if (status != SL_STATUS_OK) {
      return status;
    }
  }

  return SL_STATUS_OK;
}

static void hci_cpc_reclaim_rx_buffers(void)
{
  sl_cpc_buf_t *buf;
  sl_status_t status;

  hci_cpc_rx_lock();

  if (current_rx_buf != NULL) {
    status = hci_cpc_push_rx_buf(current_rx_buf);
    if (status == SL_STATUS_OK) {
      current_rx_buf = NULL;
    }
  }

  while (sl_cpc_msgq_pop(&rx_msgq, &buf)) {
    status = hci_cpc_push_rx_buf(buf);
    if (status != SL_STATUS_OK) {
      (void)sl_cpc_msgq_push(&rx_msgq, buf);
      break;
    }
  }

  hci_cpc_rx_unlock();
}

static void hci_cpc_start_listen(void)
{
  sl_status_t status;

  if (ep_closing || !ep_closed_ready) {
    return;
  }

  hci_cpc_reclaim_rx_buffers();
  hci_cpc_tx_slots_reset();

  status = sl_cpc_ep_listen(&ep, g_bus);
  if (status == SL_STATUS_OK) {
    cpc_state = SLI_HCI_CPC_STATE_CONNECTING;
    ep_closed_ready = false;
  }
}

static void hci_cpc_request_close(void)
{
  if (ep_closing || ep_closed_ready) {
    return;
  }

  hci_cpc_reclaim_rx_buffers();
  hci_cpc_tx_slots_reset();
  ep_closing = true;
  ep_closed_ready = false;
  sl_cpc_ep_close(&ep);
}

static void hci_cpc_on_ep_event(sl_cpc_ep_t *endpoint,
                                sl_cpc_ep_event_type_t type,
                                const sl_cpc_ep_event_t *event,
                                void *arg)
{
  (void)endpoint;
  (void)arg;

  switch (type) {
    case SL_CPC_EP_EVENT_RECV:
      hci_cpc_rx_lock();
      if (sli_hci_cpc_is_connected() && !ep_closing) {
        sl_cpc_msgq_push(&rx_msgq, event->recv.buf);
        hci_cpc_rx_unlock();
        sli_hci_cpc_rtos_on_transport_notify((uint8_t)SL_CPC_EP_ID_BLUETOOTH_RCP, NULL);
      } else {
        hci_cpc_push_rx_buf(event->recv.buf);
        hci_cpc_rx_unlock();
      }
      break;

    case SL_CPC_EP_EVENT_SEND_DONE:
      hci_cpc_tx_slot_release_on_send_done(&event->send_done);
      hci_common_transport_transmit_complete(event->send_done.status);
      break;

    case SL_CPC_EP_EVENT_CONNECTED:
      cpc_link_error_pending = false;
      link_close_done = true;
      ep_closed_ready = false;
      cpc_state = SLI_HCI_CPC_STATE_CONNECTED;
      hci_common_transport_transmit_reconnected();
      break;

    case SL_CPC_EP_EVENT_ERROR:
      cpc_link_error_pending = true;
      link_close_done = false;
      cpc_state = SLI_HCI_CPC_STATE_DISCONNECTED;
      sli_hci_cpc_rtos_on_transport_notify((uint8_t)SL_CPC_EP_ID_BLUETOOTH_RCP, NULL);
      break;

    case SL_CPC_EP_EVENT_CLOSED:
      ep_closing = false;
      ep_closed_ready = true;
      link_close_done = true;
      cpc_state = SLI_HCI_CPC_STATE_DISCONNECTED;
      sli_hci_cpc_rtos_on_transport_notify((uint8_t)SL_CPC_EP_ID_BLUETOOTH_RCP, NULL);
      break;

    default:
      break;
  }
}

sl_status_t sli_hci_cpc_init(void)
{
  sl_status_t status;

  if (rx_mutex == NULL) {
    rx_mutex = osMutexNew(&rx_mutex_attr);
    if (rx_mutex == NULL) {
      EFM_ASSERT(rx_mutex != NULL);
      return SL_STATUS_FAIL;
    }
  }

  sl_cpc_msgq_init(&rx_msgq);
  current_rx_buf = NULL;
  hci_cpc_tx_slots_reset();
  cpc_link_error_pending = false;
  ep_closing = false;
  ep_closed_ready = true;
  link_close_done = true;
  cpc_state = SLI_HCI_CPC_STATE_DISCONNECTED;

  status = sl_cpc_ep_init(&ep,
                          SL_CPC_EP_ID_BLUETOOTH_RCP,
                          SL_HCI_CPC_NG_RX_SIZE,
                          hci_cpc_on_ep_event,
                          NULL);
  if (status != SL_STATUS_OK) {
    return status;
  }

  status = hci_cpc_push_all_rx_bufs();
  if (status != SL_STATUS_OK) {
    return status;
  }

  status = sl_cpc_ep_listen(&ep, g_bus);
  if (status != SL_STATUS_OK) {
    return status;
  }

  cpc_state = SLI_HCI_CPC_STATE_CONNECTING;
  ep_closed_ready = false;
  return SL_STATUS_OK;
}

int sli_hci_cpc_new_data(void)
{
  int has_data;

  hci_cpc_rx_lock();
  has_data = sl_cpc_msgq_is_empty(&rx_msgq) ? 0 : 1;
  hci_cpc_rx_unlock();
  return has_data;
}

int sli_hci_cpc_read(const uint8_t **payload, sl_cpc_buf_t **rx_buf)
{
  sl_cpc_buf_t *buf;
  uint16_t len;
  const uint8_t *view;

  if (payload == NULL || rx_buf == NULL) {
    return 0;
  }

  hci_cpc_rx_lock();
  if (!sl_cpc_msgq_pop(&rx_msgq, &buf) || buf == NULL) {
    hci_cpc_rx_unlock();
    return 0;
  }

  current_rx_buf = buf;
  hci_cpc_rx_unlock();

  view = hci_cpc_payload_view(buf, &len);
  if (view == NULL || len == 0) {
    sli_hci_cpc_free_rx(buf);
    return 0;
  }

  *payload = view;
  *rx_buf = buf;
  return (int)len;
}

void sli_hci_cpc_free_rx(sl_cpc_buf_t *rx_buf)
{
  sl_status_t status;

  if (rx_buf == NULL) {
    EFM_ASSERT(false);
    return;
  }

  hci_cpc_rx_lock();

  if (current_rx_buf == rx_buf) {
    status = hci_cpc_push_rx_buf(current_rx_buf);
    if (status == SL_STATUS_OK) {
      current_rx_buf = NULL;
    }
    hci_cpc_rx_unlock();
    return;
  }

  hci_cpc_rx_unlock();
  EFM_ASSERT(false);
}

sl_status_t sli_hci_cpc_write(uint8_t *data, uint16_t len)
{
  sl_status_t status;

  if (cpc_state != SLI_HCI_CPC_STATE_CONNECTED || cpc_link_error_pending) {
    return SL_STATUS_NOT_READY;
  }
  if (data == NULL || len == 0U) {
    return SL_STATUS_INVALID_PARAMETER;
  }
  if (len > SL_HCI_CPC_NG_TX_MAX_SIZE) {
    return SL_STATUS_INVALID_PARAMETER;
  }

  for (uint32_t i = 0; i < SL_HCI_CPC_NG_TX_SLOT_COUNT; i++) {
    sli_hci_cpc_ng_tx_slot_t *slot = &tx_slots[i];

    if (slot->in_use) {
      continue;
    }

    memcpy(slot->data, data, len);
    sl_cpc_buf_init(&slot->buf, slot->data, len);
    memset(&slot->frame, 0, sizeof(slot->frame));
    slot->in_use = true;
    status = sl_cpc_ep_send(&ep, &slot->buf, &slot->frame, NULL);
    if (status != SL_STATUS_OK) {
      slot->in_use = false;
    }
    return status;
  }

  return SL_STATUS_NO_MORE_RESOURCE;
}

bool sli_hci_cpc_is_connected(void)
{
  return (cpc_state == SLI_HCI_CPC_STATE_CONNECTED) && !cpc_link_error_pending;
}

void sli_hci_cpc_on_link_down(void)
{
  if (ep_closing) {
    return;
  }

  if (cpc_link_error_pending && !link_close_done) {
    hci_cpc_request_close();
    return;
  }

  hci_cpc_reclaim_rx_buffers();
}

void sli_hci_cpc_reconnect(void)
{
  if (sli_hci_cpc_is_connected()) {
    return;
  }

  if (ep_closing) {
    return;
  }

  if (cpc_link_error_pending) {
    if (!link_close_done) {
      hci_cpc_request_close();
    } else if (ep_closed_ready && cpc_state == SLI_HCI_CPC_STATE_DISCONNECTED) {
      hci_cpc_start_listen();
    }
    return;
  }

  switch (cpc_state) {
    case SLI_HCI_CPC_STATE_DISCONNECTED:
      hci_cpc_start_listen();
      break;

    case SLI_HCI_CPC_STATE_CONNECTING:
    case SLI_HCI_CPC_STATE_CONNECTED:
      break;

    default:
      EFM_ASSERT(false);
      break;
  }
}
