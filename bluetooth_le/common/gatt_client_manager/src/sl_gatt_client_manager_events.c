/***************************************************************************//**
 * @file
 * @brief GATT Client Manager - event emission
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

#include <string.h>

#include "sli_gatt_client_manager.h"

static void emit_event(const sl_gatt_client_manager_event_t *event)
{
  sli_gatt_client_manager_dispatch_event(event);
}

void sli_gcm_log_event(const sl_gatt_client_manager_event_t *event)
{
#if defined(SL_CATALOG_APP_LOG_PRESENT) && SL_GATT_CLIENT_MANAGER_LOG \
  && SL_GATT_CLIENT_MANAGER_LOG_EVENTS
  if (event == NULL) {
    return;
  }

  switch (event->header.id) {
    case SL_GATT_CLIENT_MANAGER_EVENT_DISCOVERY_COMPLETE:
      gcm_log_debug("event DISCOVERY_COMPLETE conn=%u profile=%u found=%u/%u"
                    " match=%u mask=0x%08lx from_cache=%u status=0x%04lx"
                    APP_LOG_NL,
                    event->header.connection,
                    event->data.discovery_complete.profile_handle,
                    event->data.discovery_complete.found_count,
                    event->data.discovery_complete.pair_count,
                    event->data.discovery_complete.match_count,
                    (unsigned long)event->data.discovery_complete.missing_mask,
                    event->data.discovery_complete.from_cache,
                    (unsigned long)event->data.discovery_complete.status);
      break;

    case SL_GATT_CLIENT_MANAGER_EVENT_ERROR:
      gcm_log_debug("event ERROR conn=%u profile=%u pair=%u status=0x%04lx"
                    APP_LOG_NL,
                    event->header.connection,
                    event->data.error.profile_handle,
                    event->data.error.pair_index,
                    (unsigned long)event->data.error.status);
      break;

    case SL_GATT_CLIENT_MANAGER_EVENT_CHARACTERISTIC_VALUE:
      gcm_log_debug("event CHARACTERISTIC_VALUE conn=%u char=0x%04X"
                    " opcode=0x%02X len=%u" APP_LOG_NL,
                    event->header.connection,
                    event->data.characteristic_value.characteristic,
                    event->data.characteristic_value.att_opcode,
                    event->data.characteristic_value.value_len);
      break;

    case SL_GATT_CLIENT_MANAGER_EVENT_SERVICE_FOUND:
      gcm_log_debug("event SERVICE_FOUND conn=%u profile=%u pair=%u"
                    " service=0x%08lx" APP_LOG_NL,
                    event->header.connection,
                    event->data.service_found.profile_handle,
                    event->data.service_found.pair_index,
                    (unsigned long)event->data.service_found.service_handle);
      break;

    case SL_GATT_CLIENT_MANAGER_EVENT_CHARACTERISTIC_FOUND:
      gcm_log_debug("event CHARACTERISTIC_FOUND conn=%u profile=%u pair=%u"
                    " service=0x%08lx characteristic=0x%04X" APP_LOG_NL,
                    event->header.connection,
                    event->data.characteristic_found.profile_handle,
                    event->data.characteristic_found.pair_index,
                    (unsigned long)event->data.characteristic_found.service_handle,
                    event->data.characteristic_found.characteristic_handle);
      break;

    case SL_GATT_CLIENT_MANAGER_EVENT_REDISCOVERY_NEEDED:
      gcm_log_debug("event REDISCOVERY_NEEDED conn=%u auto_started=%u" APP_LOG_NL,
                    event->header.connection,
                    event->data.rediscovery_needed.auto_started);
      break;

    default:
      gcm_log_debug("event id=%u conn=%u" APP_LOG_NL,
                    (unsigned)event->header.id,
                    event->header.connection);
      break;
  }
#else
  (void)event;
#endif
}

void sli_gcm_emit_error(uint8_t connection,
                        uint16_t profile_handle,
                        uint8_t pair_index,
                        sl_status_t status)
{
  sl_gatt_client_manager_event_t event;

  memset(&event, 0, sizeof(event));
  event.header.id = SL_GATT_CLIENT_MANAGER_EVENT_ERROR;
  event.header.connection = connection;
  event.data.error.status = status;
  event.data.error.profile_handle = profile_handle;
  event.data.error.pair_index = pair_index;
  gcm_log_error("error conn=%u profile=%u pair=%u status=0x%04lx" APP_LOG_NL,
                connection,
                profile_handle,
                pair_index,
                (unsigned long)status);
  emit_event(&event);
}

void sli_gcm_emit_discovery_complete(connection_entry_t *entry,
                                     sl_status_t failure_status)
{
  sl_gatt_client_manager_event_t event;
  uint16_t profile_handle = entry->current_profile;
  const profile_entry_t *profile = sli_gcm_get_profile(profile_handle);
  uint8_t found = 0;
  uint8_t match_count = 0;
  uint8_t missing_index = SL_GATT_CLIENT_MANAGER_INVALID_PAIR_INDEX;
  uint32_t missing_mask = 0;

  if (profile == NULL) {
    return;
  }

  memset(&event, 0, sizeof(event));
  event.header.id = SL_GATT_CLIENT_MANAGER_EVENT_DISCOVERY_COMPLETE;
  event.header.connection = entry->connection;

  for (uint8_t i = 0; i < profile->pair_count; i++) {
    const discovery_result_t *result = &entry->results[profile_handle][i];
    if (result->service_found && result->characteristic_found) {
      found++;
    } else if (i < SL_GATT_CLIENT_MANAGER_MISSING_MASK_BITS) {
      missing_mask |= (uint32_t)1 << i;
    }
    if (result->service_match_count > match_count) {
      match_count = result->service_match_count;
    }
  }

  if (missing_mask != 0) {
    for (uint8_t i = 0; i < SL_GATT_CLIENT_MANAGER_MISSING_MASK_BITS; i++) {
      if ((missing_mask & ((uint32_t)1 << i)) != 0) {
        missing_index = i;
        break;
      }
    }
  }

  event.data.discovery_complete.status =
    (failure_status != SL_STATUS_OK)
    ? failure_status
    : ((found == profile->pair_count && profile->pair_count > 0)
       ? SL_STATUS_OK
       : SL_STATUS_NOT_FOUND);
  event.data.discovery_complete.profile_handle = profile_handle;
  event.data.discovery_complete.found_count = found;
  event.data.discovery_complete.pair_count = profile->pair_count;
  event.data.discovery_complete.match_count = match_count;
  event.data.discovery_complete.missing_mask = missing_mask;
  event.data.discovery_complete.from_cache = entry->from_cache;

  gcm_log_info("profile %u complete conn=%u found=%u/%u matches=%u from_cache=%u status=0x%04lx"
               APP_LOG_NL,
               profile_handle,
               entry->connection,
               found,
               profile->pair_count,
               match_count,
               entry->from_cache,
               (unsigned long)event.data.discovery_complete.status);
  if (missing_index != SL_GATT_CLIENT_MANAGER_INVALID_PAIR_INDEX) {
    gcm_log_warning("profile %u missing pair %u (mask=0x%08lx)" APP_LOG_NL,
                    profile_handle,
                    missing_index,
                    (unsigned long)missing_mask);
    gcm_log_uuid("missing svc", &profile->pairs[missing_index].service);
    gcm_log_uuid("missing char", &profile->pairs[missing_index].characteristic);
  }
  if (match_count > 1) {
    gcm_log_warning("profile %u matched %u instances; lowest-handle instance used"
                    APP_LOG_NL,
                    profile_handle,
                    match_count);
  }

  entry->state = discovery_state_idle;
  emit_event(&event);
}

void sli_gcm_emit_service_found(uint8_t connection,
                                uint16_t profile_handle,
                                uint8_t pair_index,
                                uint32_t service_handle)
{
  sl_gatt_client_manager_event_t event;

  memset(&event, 0, sizeof(event));
  event.header.id = SL_GATT_CLIENT_MANAGER_EVENT_SERVICE_FOUND;
  event.header.connection = connection;
  event.data.service_found.profile_handle = profile_handle;
  event.data.service_found.pair_index = pair_index;
  event.data.service_found.service_handle = service_handle;

  emit_event(&event);
}

void sli_gcm_emit_characteristic_found(uint8_t connection,
                                       uint16_t profile_handle,
                                       uint8_t pair_index,
                                       uint32_t service_handle,
                                       uint16_t characteristic_handle)
{
  sl_gatt_client_manager_event_t event;

  memset(&event, 0, sizeof(event));
  event.header.id = SL_GATT_CLIENT_MANAGER_EVENT_CHARACTERISTIC_FOUND;
  event.header.connection = connection;
  event.data.characteristic_found.profile_handle = profile_handle;
  event.data.characteristic_found.pair_index = pair_index;
  event.data.characteristic_found.service_handle = service_handle;
  event.data.characteristic_found.characteristic_handle = characteristic_handle;

  emit_event(&event);
}

void sli_gcm_emit_rediscovery_needed(uint8_t connection,
                                     sl_status_t status,
                                     bool auto_started)
{
  sl_gatt_client_manager_event_t event;

  memset(&event, 0, sizeof(event));
  event.header.id = SL_GATT_CLIENT_MANAGER_EVENT_REDISCOVERY_NEEDED;
  event.header.connection = connection;
  event.data.rediscovery_needed.auto_started = auto_started;

  gcm_log_warning("rediscovery needed conn=%u status=0x%04lx auto_started=%u"
                  APP_LOG_NL,
                  connection,
                  (unsigned long)status,
                  auto_started);

  emit_event(&event);
}
