/***************************************************************************//**
 * @file
 * @brief Protocol-agnostic message link over a cpc-ng endpoint.
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
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "sl_common.h"
#include "sl_cpc_buf.h"
#include "sl_cpc_ep.h"
#include "sl_cpc_frame.h"
#include "sl_bt_link_config.h"
#include "sl_bt_link.h"
#include "sli_bt_link_runtime.h"

typedef enum {
  link_state_free = 0,
  link_state_created,
  link_state_opening,
  link_state_open,
  link_state_closing,
  link_state_recovering,
} link_state_t;

struct sli_bt_link {
  sl_cpc_ep_t               endpoint;
  sl_bt_link_config_t       config;
  sl_bt_link_receive_slot_t *message_head;
  sl_bt_link_receive_slot_t *message_tail;
  link_state_t               state;
};

static struct sli_bt_link link_pool[SL_BT_LINK_COUNT];

static bool sl_bt_link_is_allocated(const sl_bt_link_t *link)
{
  size_t i;

  if (link == NULL) {
    return false;
  }

  for (i = 0; i < SL_BT_LINK_COUNT; ++i) {
    if ((link == &link_pool[i])
        && (link->state != link_state_free)) {
      return true;
    }
  }

  return false;
}

static sl_bt_link_receive_slot_t *sl_bt_link_message_to_slot(
  const sl_bt_link_message_t *message)
{
  return (sl_bt_link_receive_slot_t *)(uintptr_t)message;
}

static void sl_bt_link_receive_slot_reset(sl_bt_link_receive_slot_t *slot)
{
  uint8_t *data = slot->data;
  sl_cpc_buf_t buffer = slot->buffer;

  memset(slot, 0, sizeof(*slot));
  slot->data = data;
  slot->buffer = buffer;
}

static void sl_bt_link_transmit_slot_reset(sl_bt_link_transmit_slot_t *slot)
{
  uint8_t *data = slot->data;

  memset(slot, 0, sizeof(*slot));
  slot->data = data;
}

static bool sl_bt_link_payload_is_acceptable(const sl_bt_link_t *link,
                                             const uint8_t *data,
                                             size_t length)
{
  if ((data == NULL)
      || (length == 0)
      || (length > link->config.storage->message_size)) {
    return false;
  }

  if (link->config.is_valid != NULL) {
    return link->config.is_valid(data, length);
  }

  return true;
}

static sl_status_t sl_bt_link_message_transmit_start(sl_bt_link_t *link)
{
  sl_bt_link_transmit_slot_t *slot = link->config.storage->transmit_slot;
  sl_status_t status = SL_STATUS_OK;

  if ((slot->allocated)
      && !(slot->active)
      && (link->state == link_state_open)) {
    sl_cpc_buf_init(&slot->buffer,
                    slot->data,
                    (uint16_t)slot->length);
    memset(&slot->frame, 0, sizeof(slot->frame));
    status = sl_cpc_ep_send(&link->endpoint,
                            &slot->buffer,
                            &slot->frame,
                            link);
    if (status == SL_STATUS_OK) {
      slot->active = true;
    }
  }

  return status;
}

static sl_bt_link_receive_slot_t *sl_bt_link_receive_slot_from_buffer(
  sl_bt_link_t *link,
  sl_cpc_buf_t *buffer)
{
  sl_bt_link_storage_t *storage = link->config.storage;
  sl_bt_link_receive_slot_t *message = NULL;
  size_t i;

  for (i = 0; i < storage->receive_count; ++i) {
    if ((&storage->receive_slot[i].buffer == buffer)
        && !(storage->receive_slot[i].allocated)) {
      message = &storage->receive_slot[i];
      message->allocated = true;
      break;
    }
  }

  return message;
}

static void sl_bt_link_push(sl_bt_link_t *link,
                            sl_bt_link_receive_slot_t *message)
{
  message->next = NULL;
  if (link->message_tail == NULL) {
    link->message_head = message;
  } else {
    link->message_tail->next = message;
  }
  link->message_tail = message;
}

static void sl_bt_link_discard_messages(sl_bt_link_t *link)
{
  sl_bt_link_receive_slot_t *message;

  while (link->message_head != NULL) {
    message = link->message_head;
    link->message_head = message->next;
    if (message->buffer.ptr != NULL) {
      (void)sl_cpc_ep_push_recv_buf(&link->endpoint, &message->buffer);
    }
    sl_bt_link_receive_slot_reset(message);
  }
  link->message_tail = NULL;
}

static void sl_bt_link_endpoint_event(sl_cpc_ep_t *endpoint,
                                      sl_cpc_ep_event_type_t type,
                                      const sl_cpc_ep_event_t *endpoint_event,
                                      void *user)
{
  sl_bt_link_t *link = (sl_bt_link_t *)user;
  sli_bt_link_event_t event = {
    .link = link,
    .buffer = NULL,
    .type = sli_bt_link_event_error,
    .status = SL_STATUS_FAIL,
  };
  sl_status_t status;

  switch (type) {
    case SL_CPC_EP_EVENT_RECV:
      event.buffer = endpoint_event->recv.buf;
      event.type = sli_bt_link_event_receive;
      event.status = SL_STATUS_OK;
      break;

    case SL_CPC_EP_EVENT_SEND_DONE:
      event.buffer = endpoint_event->send_done.buf;
      event.type = sli_bt_link_event_transmit;
      event.status = endpoint_event->send_done.frame->status;
      break;

    case SL_CPC_EP_EVENT_CONNECTED:
      event.type = sli_bt_link_event_connected;
      event.status = SL_STATUS_OK;
      break;

    case SL_CPC_EP_EVENT_CLOSED:
      event.type = sli_bt_link_event_closed;
      event.status = SL_STATUS_OK;
      break;

    case SL_CPC_EP_EVENT_ERROR:
      event.type = sli_bt_link_event_error;
      event.status = endpoint_event->error.status;
      break;

    default:
      return;
  }

  status = sli_bt_link_runtime_post(link->config.runtime, &event);
  if (status != SL_STATUS_OK) {
    // A receive buffer not handed to the runtime must be returned immediately.
    if ((type == SL_CPC_EP_EVENT_RECV)
        && (endpoint_event->recv.buf != NULL)) {
      (void)sl_cpc_ep_push_recv_buf(endpoint,
                                    endpoint_event->recv.buf);
    }
    if (link->config.on_error != NULL) {
      link->config.on_error(link, status, link->config.user);
    }
  }
}

sl_status_t sl_bt_link_create(const sl_bt_link_config_t *config,
                              sl_bt_link_t **link)
{
  sl_bt_link_storage_t *storage;
  sl_bt_link_t *slot = NULL;
  uint16_t receive_size;
  sl_status_t status;
  size_t i;

  if ((config == NULL) || (link == NULL)) {
    return SL_STATUS_NULL_POINTER;
  }

  storage = config->storage;
  if ((config->bus == NULL)
      || (config->runtime == NULL)
      || (storage == NULL)
      || (storage->receive_slot == NULL)
      || (storage->transmit_slot == NULL)
      || (storage->receive_data == NULL)
      || (storage->transmit_data == NULL)
      || (storage->message_size == 0)
      || (storage->buffer_stride < storage->message_size)
      || (storage->buffer_stride > UINT16_MAX)
      || ((storage->buffer_stride % SL_CPC_BUF_MIN_ALIGNMENT) != 0)
      || (((uintptr_t)storage->receive_data % SL_CPC_BUF_MIN_ALIGNMENT) != 0)
      || (((uintptr_t)storage->transmit_data % SL_CPC_BUF_MIN_ALIGNMENT) != 0)
      || (storage->receive_count == 0)
      || (storage->allocated)) {
    return SL_STATUS_INVALID_PARAMETER;
  }

  for (i = 0; i < SL_BT_LINK_COUNT; ++i) {
    if ((link_pool[i].state != link_state_free)
        && (link_pool[i].config.endpoint_id == config->endpoint_id)) {
      return SL_STATUS_INVALID_PARAMETER;
    }
  }

  for (i = 0; i < SL_BT_LINK_COUNT; ++i) {
    if (link_pool[i].state == link_state_free) {
      slot = &link_pool[i];
      break;
    }
  }

  if (slot == NULL) {
    return SL_STATUS_NO_MORE_RESOURCE;
  }

  status = sli_bt_link_runtime_reserve(config->runtime,
                                       storage->receive_count);
  if (status != SL_STATUS_OK) {
    return status;
  }

  memset(slot, 0, sizeof(*slot));
  slot->config = *config;
  slot->state = link_state_created;
  storage->allocated = true;
  receive_size = (uint16_t)storage->buffer_stride;

  memset(storage->receive_slot,
         0,
         storage->receive_count * sizeof(storage->receive_slot[0]));
  memset(storage->transmit_slot, 0, sizeof(*storage->transmit_slot));
  for (i = 0; i < storage->receive_count; ++i) {
    storage->receive_slot[i].data = storage->receive_data
                                    + (i * storage->buffer_stride);
  }
  storage->transmit_slot->data = storage->transmit_data;

  status = sl_cpc_ep_init(&slot->endpoint,
                          config->endpoint_id,
                          receive_size,
                          sl_bt_link_endpoint_event,
                          slot);
  if (status != SL_STATUS_OK) {
    storage->allocated = false;
    memset(slot, 0, sizeof(*slot));
    (void)sli_bt_link_runtime_unreserve(config->runtime,
                                        storage->receive_count);
    return status;
  }

  for (i = 0; i < storage->receive_count; ++i) {
    sl_cpc_buf_init(&storage->receive_slot[i].buffer,
                    storage->receive_slot[i].data,
                    receive_size);
    status = sl_cpc_ep_push_recv_buf(&slot->endpoint,
                                     &storage->receive_slot[i].buffer);
    if (status != SL_STATUS_OK) {
      sl_cpc_ep_deinit(&slot->endpoint);
      storage->allocated = false;
      memset(slot, 0, sizeof(*slot));
      (void)sli_bt_link_runtime_unreserve(config->runtime,
                                          storage->receive_count);
      return status;
    }
  }

  *link = slot;

  return SL_STATUS_OK;
}

sl_status_t sl_bt_link_destroy(sl_bt_link_t *link)
{
  sl_bt_link_storage_t *storage;
  sl_status_t status;
  size_t i;

  if (!sl_bt_link_is_allocated(link)) {
    return SL_STATUS_INVALID_HANDLE;
  }

  if ((link->config.storage->transmit_slot->allocated)
      || (link->state != link_state_created)) {
    return SL_STATUS_INVALID_STATE;
  }

  storage = link->config.storage;
  for (i = 0; i < storage->receive_count; ++i) {
    if (storage->receive_slot[i].allocated) {
      return SL_STATUS_BUSY;
    }
  }

  status = sli_bt_link_runtime_unreserve(link->config.runtime,
                                         storage->receive_count);
  if (status != SL_STATUS_OK) {
    return status;
  }

  sl_cpc_ep_deinit(&link->endpoint);
  storage->allocated = false;
  memset(link, 0, sizeof(*link));

  return SL_STATUS_OK;
}

sl_status_t sl_bt_link_start(sl_bt_link_t *link)
{
  sl_status_t status;
  sl_status_t release_status;

  if (!sl_bt_link_is_allocated(link)) {
    return SL_STATUS_INVALID_HANDLE;
  }

  status = sli_bt_link_runtime_acquire(link->config.runtime);
  if (status != SL_STATUS_OK) {
    return status;
  }

  if (link->state != link_state_created) {
    status = SL_STATUS_ALREADY_INITIALIZED;
  } else if (link->config.role == sl_bt_link_role_primary) {
    status = sl_cpc_ep_connect(&link->endpoint, link->config.bus);
  } else {
    status = sl_cpc_ep_listen(&link->endpoint, link->config.bus);
  }

  if (status == SL_STATUS_OK) {
    link->state = link_state_opening;
  }

  release_status = sli_bt_link_runtime_release(link->config.runtime);

  return (status == SL_STATUS_OK) ? release_status : status;
}

void sl_bt_link_close(sl_bt_link_t *link)
{
  sl_status_t status;

  if (!sl_bt_link_is_allocated(link)) {
    return;
  }

  status = sli_bt_link_runtime_acquire(link->config.runtime);
  if (status == SL_STATUS_OK) {
    if ((link->state == link_state_opening)
        || (link->state == link_state_open)) {
      link->state = link_state_closing;
      sl_cpc_ep_close(&link->endpoint);
    } else if (link->state == link_state_recovering) {
      // The endpoint is already closing; only cancel automatic recovery.
      link->state = link_state_closing;
    }
    (void)sli_bt_link_runtime_release(link->config.runtime);
  }
}

sl_status_t sl_bt_link_send(sl_bt_link_t *link,
                            const uint8_t *data,
                            size_t length)
{
  sl_bt_link_storage_t *storage;
  sl_bt_link_transmit_slot_t *slot;
  sl_status_t status;
  sl_status_t release_status;

  if (!sl_bt_link_is_allocated(link)) {
    return SL_STATUS_INVALID_HANDLE;
  }

  if (!sl_bt_link_payload_is_acceptable(link, data, length)) {
    return SL_STATUS_INVALID_PARAMETER;
  }

  status = sli_bt_link_runtime_acquire(link->config.runtime);
  if (status != SL_STATUS_OK) {
    return status;
  }

  storage = link->config.storage;
  slot = storage->transmit_slot;

  if (slot->allocated) {
    status = SL_STATUS_BUSY;
  } else {
    memcpy(slot->data, data, length);
    slot->length = length;
    slot->allocated = true;
    status = sl_bt_link_message_transmit_start(link);
    if (status != SL_STATUS_OK) {
      sl_bt_link_transmit_slot_reset(slot);
    }
  }

  release_status = sli_bt_link_runtime_release(link->config.runtime);

  return (status == SL_STATUS_OK) ? release_status : status;
}

sl_status_t sl_bt_link_pop(sl_bt_link_t *link,
                           sl_bt_link_message_t **message)
{
  sl_status_t status;
  sl_status_t release_status;

  if (!sl_bt_link_is_allocated(link)) {
    return SL_STATUS_INVALID_HANDLE;
  }

  if (message == NULL) {
    return SL_STATUS_NULL_POINTER;
  }

  status = sli_bt_link_runtime_acquire(link->config.runtime);
  if (status != SL_STATUS_OK) {
    return status;
  }

  if (link->message_head == NULL) {
    status = SL_STATUS_EMPTY;
  } else {
    sl_bt_link_receive_slot_t *slot = link->message_head;

    link->message_head = link->message_head->next;
    if (link->message_head == NULL) {
      link->message_tail = NULL;
    }
    slot->next = NULL;
    *message = (sl_bt_link_message_t *)slot;
    status = SL_STATUS_OK;
  }

  release_status = sli_bt_link_runtime_release(link->config.runtime);

  return (status == SL_STATUS_OK) ? release_status : status;
}

sl_status_t sl_bt_link_release(sl_bt_link_t *link,
                               sl_bt_link_message_t *message)
{
  sl_bt_link_receive_slot_t *slot;
  sl_cpc_buf_t *buffer;
  sl_status_t status;
  sl_status_t release_status;

  if (!sl_bt_link_is_allocated(link)) {
    return SL_STATUS_INVALID_HANDLE;
  }

  slot = sl_bt_link_message_to_slot(message);
  if ((message == NULL)
      || (slot->owner != link)
      || !(slot->allocated)) {
    return SL_STATUS_INVALID_PARAMETER;
  }

  status = sli_bt_link_runtime_acquire(link->config.runtime);
  if (status != SL_STATUS_OK) {
    return status;
  }

  buffer = &slot->buffer;
  sl_bt_link_receive_slot_reset(slot);
  status = sl_cpc_ep_push_recv_buf(&link->endpoint, buffer);
  release_status = sli_bt_link_runtime_release(link->config.runtime);

  return (status == SL_STATUS_OK) ? release_status : status;
}

bool sl_bt_link_is_open(sl_bt_link_t *link)
{
  bool is_open = false;

  if ((sl_bt_link_is_allocated(link))
      && (sli_bt_link_runtime_acquire(link->config.runtime) == SL_STATUS_OK)) {
    is_open = link->state == link_state_open;
    (void)sli_bt_link_runtime_release(link->config.runtime);
  }

  return is_open;
}

size_t sl_bt_link_get_length(const sl_bt_link_message_t *message)
{
  sl_bt_link_receive_slot_t *slot = sl_bt_link_message_to_slot(message);

  return (((message == NULL) || !(slot->allocated))
          ? 0
          : slot->length);
}

size_t sl_bt_link_get_remaining(const sl_bt_link_message_t *message)
{
  sl_bt_link_receive_slot_t *slot = sl_bt_link_message_to_slot(message);

  return (((message == NULL) || !(slot->allocated))
          ? 0
          : (slot->length - slot->offset));
}

const uint8_t *sl_bt_link_get_data(const sl_bt_link_message_t *message)
{
  sl_bt_link_receive_slot_t *slot = sl_bt_link_message_to_slot(message);

  return (((message == NULL) || !(slot->allocated))
          ? NULL
          : (const uint8_t *)slot->buffer.ptr);
}

sl_status_t sl_bt_link_read(sl_bt_link_message_t *message,
                            uint8_t *data,
                            size_t length)
{
  sl_bt_link_receive_slot_t *slot;
  const uint8_t *source;

  if ((message == NULL) || (data == NULL)) {
    return SL_STATUS_NULL_POINTER;
  }

  slot = sl_bt_link_message_to_slot(message);
  if (!(slot->allocated)
      || (length > sl_bt_link_get_remaining(message))) {
    return SL_STATUS_WOULD_OVERFLOW;
  }

  source = (const uint8_t *)slot->buffer.ptr;
  memcpy(data, source + slot->offset, length);
  slot->offset += length;

  return SL_STATUS_OK;
}

void sli_bt_link_process_event(const sli_bt_link_event_t *event)
{
  sl_bt_link_t *link;
  sl_bt_link_receive_slot_t *message;
  sl_bt_link_transmit_slot_t *transmit;
  sl_status_t status = SL_STATUS_OK;
  sl_status_t transmit_status = SL_STATUS_OK;
  bool notify_message = false;
  bool notify_transmit = false;
  bool notify_error = false;
  bool notify_connected = false;
  bool reconnect = false;

  if ((event == NULL) || !(sl_bt_link_is_allocated(event->link))) {
    return;
  }

  link = event->link;
  status = sli_bt_link_runtime_acquire(link->config.runtime);
  if (status != SL_STATUS_OK) {
    return;
  }

  switch (event->type) {
    case sli_bt_link_event_receive:
      if ((event->buffer == NULL)
          || !(sl_bt_link_payload_is_acceptable(
                 link,
                 (const uint8_t *)event->buffer->ptr,
                 event->buffer->len))) {
        if (event->buffer != NULL) {
          (void)sl_cpc_ep_push_recv_buf(&link->endpoint, event->buffer);
        }
        status = SL_STATUS_INVALID_PARAMETER;
        notify_error = true;
        break;
      }

      message = sl_bt_link_receive_slot_from_buffer(link, event->buffer);
      if (message == NULL) {
        (void)sl_cpc_ep_push_recv_buf(&link->endpoint, event->buffer);
        status = SL_STATUS_NO_MORE_RESOURCE;
        notify_error = true;
        break;
      }

      message->next = NULL;
      message->owner = link;
      message->length = event->buffer->len;
      message->offset = 0;
      sl_bt_link_push(link, message);
      notify_message = true;
      break;

    case sli_bt_link_event_transmit:
      // A failed endpoint retains ownership of an active transfer until this
      // completion. Do not release or reuse its storage during recovery.
      transmit = link->config.storage->transmit_slot;
      if ((event->buffer != &transmit->buffer)
          || !(transmit->active)) {
        break;
      }

      transmit_status = event->status;
      sl_bt_link_transmit_slot_reset(transmit);
      notify_transmit = true;
      break;

    case sli_bt_link_event_connected:
      if (link->state != link_state_opening) {
        break;
      }
      link->state = link_state_open;
      notify_connected = true;
      status = sl_bt_link_message_transmit_start(link);
      if (status != SL_STATUS_OK) {
        sl_bt_link_transmit_slot_reset(link->config.storage->transmit_slot);
        transmit_status = status;
        notify_transmit = true;
        notify_error = true;
      }
      break;

    case sli_bt_link_event_closed:
      if (link->state == link_state_created) {
        break;
      }
      reconnect = link->state != link_state_closing;
      sl_bt_link_discard_messages(link);
      transmit = link->config.storage->transmit_slot;
      if ((transmit->allocated) && !(transmit->active)) {
        sl_bt_link_transmit_slot_reset(transmit);
        transmit_status = SL_STATUS_ABORT;
        notify_transmit = true;
      }
      link->state = link_state_created;
      break;

    case sli_bt_link_event_error:
      // A SYN from a restarted peer is an expected CPC error. Close, then
      // listen or connect again without resetting the other MCU.
      sl_bt_link_discard_messages(link);
      status = event->status;
      notify_error = true;
      transmit = link->config.storage->transmit_slot;
      if ((transmit->allocated) && !(transmit->active)) {
        sl_bt_link_transmit_slot_reset(transmit);
        transmit_status = status;
        notify_transmit = true;
      }
      if ((link->state == link_state_opening)
          || (link->state == link_state_open)) {
        link->state = link_state_recovering;
        sl_cpc_ep_close(&link->endpoint);
      }
      break;

    default:
      break;
  }

  (void)sli_bt_link_runtime_release(link->config.runtime);

  if ((notify_connected) && (link->config.on_connected != NULL)) {
    link->config.on_connected(link, link->config.user);
  }
  if ((notify_message) && (link->config.on_message != NULL)) {
    link->config.on_message(link, link->config.user);
  }
  if ((notify_transmit) && (link->config.on_transmit != NULL)) {
    link->config.on_transmit(link,
                             transmit_status,
                             link->config.user);
  }
  if ((notify_error) && (link->config.on_error != NULL)) {
    link->config.on_error(link, status, link->config.user);
  }
  if (reconnect) {
    status = sl_bt_link_start(link);
    if ((status != SL_STATUS_OK) && (link->config.on_error != NULL)) {
      link->config.on_error(link, status, link->config.user);
    }
  }
}
