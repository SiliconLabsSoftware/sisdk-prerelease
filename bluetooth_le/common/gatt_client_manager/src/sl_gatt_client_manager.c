/***************************************************************************//**
 * @file
 * @brief GATT Client Manager - public API and Bluetooth event dispatch
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

#include "sl_common.h"
#include "sl_bt_api.h"
#include "app_rta.h"
#include "sl_gatt_client_manager.h"
#include "sli_gatt_client_manager.h"

// -----------------------------------------------------------------------------
// Static variables

/// Set by init, cleared by reset / clear_internal_state. Public APIs check this.
static bool module_initialized;
/// Shared RTA guard; see sli_gatt_client_manager.h.
app_rta_context_t sli_gcm_ctx;

SL_WEAK void sl_gatt_client_manager_on_event(
  const sl_gatt_client_manager_event_t *event)
{
  (void)event;
}

bool sli_gcm_is_initialized(void)
{
  return module_initialized;
}

/// Wipe profiles, connections, and cache; leave the component uninitialized.
static void clear_internal_state(void)
{
  sli_gcm_clear_module_state();
  sli_gcm_clear_all_connections();
#if SL_GATT_CLIENT_MANAGER_CACHE_ENABLE
  sli_gcm_cache_clear_all();
#endif
  module_initialized = false;
}

sl_status_t sl_gatt_client_manager_init(void)
{
  sl_status_t sc = app_rta_acquire(sli_gcm_ctx);
  if (sc != SL_STATUS_OK) {
    sli_gcm_on_runtime_error(APP_RTA_ERROR_ACQUIRE_FAILED, sc);
    return sc;
  }
  clear_internal_state();
  module_initialized = true;
  gcm_log_info("initialized (cache=%u robust_caching=%u service_changed=%u"
               " auto_discovery=%u max_profiles=%u max_pairs=%u max_conn=%u)"
               APP_LOG_NL,
               (unsigned)SL_GATT_CLIENT_MANAGER_CACHE_ENABLE,
               (unsigned)SL_GATT_CLIENT_MANAGER_ENABLE_ROBUST_CACHING,
               (unsigned)SL_GATT_CLIENT_MANAGER_ENABLE_SERVICE_CHANGED,
               (unsigned)SL_GATT_CLIENT_MANAGER_AUTO_START_DISCOVERY,
               (unsigned)SL_GATT_CLIENT_MANAGER_MAX_PROFILES,
               (unsigned)SL_GATT_CLIENT_MANAGER_MAX_PAIRS_PER_PROFILE,
               (unsigned)SL_GATT_CLIENT_MANAGER_MAX_CONNECTIONS);
  (void)app_rta_release(sli_gcm_ctx);
  return SL_STATUS_OK;
}

sl_status_t sl_gatt_client_manager_reset(void)
{
  sl_status_t sc = app_rta_acquire(sli_gcm_ctx);
  if (sc != SL_STATUS_OK) {
    sli_gcm_on_runtime_error(APP_RTA_ERROR_ACQUIRE_FAILED, sc);
    return sc;
  }
  clear_internal_state();
  gcm_log_info("reset to initial state" APP_LOG_NL);
  (void)app_rta_release(sli_gcm_ctx);
  return SL_STATUS_OK;
}

sl_status_t sl_gatt_client_manager_register_profile(
  const sl_gatt_client_manager_profile_t *profile,
  uint16_t *out_profile_handle)
{
  profile_entry_t *entry;
  uint16_t index;
  sl_status_t sc;

  if (profile == NULL || out_profile_handle == NULL || profile->pairs == NULL) {
    gcm_log_debug("register_profile: null pointer" APP_LOG_NL);
    return SL_STATUS_NULL_POINTER;
  }
  if (profile->pair_count == 0) {
    gcm_log_debug("register_profile: invalid pair_count=0" APP_LOG_NL);
    return SL_STATUS_INVALID_PARAMETER;
  }
  if (profile->pair_count > SL_GATT_CLIENT_MANAGER_MAX_PAIRS_PER_PROFILE) {
  gcm_log_debug("register_profile: pair_count exceeds max=%u" APP_LOG_NL,
                (unsigned)SL_GATT_CLIENT_MANAGER_MAX_PAIRS_PER_PROFILE);
  return SL_STATUS_NO_MORE_RESOURCE;
}
  for (uint8_t i = 0; i < profile->pair_count; i++) {
    if (!sli_gcm_uuid_is_valid(&profile->pairs[i].service)
        || !sli_gcm_uuid_is_valid(&profile->pairs[i].characteristic)) {
      gcm_log_debug("register_profile: invalid UUID at pair %u" APP_LOG_NL, i);
      return SL_STATUS_INVALID_PARAMETER;
    }
  }

  sc = app_rta_acquire(sli_gcm_ctx);
  if (sc != SL_STATUS_OK) {
    sli_gcm_on_runtime_error(APP_RTA_ERROR_ACQUIRE_FAILED, sc);
    return sc;
  }

  if (!sli_gcm_is_initialized()) {
    gcm_log_debug("register_profile: not initialized" APP_LOG_NL);
    (void)app_rta_release(sli_gcm_ctx);
    return SL_STATUS_NOT_INITIALIZED;
  }

  entry = sli_gcm_register_profile_storage(&index);
  if (entry == NULL) {
    gcm_log_debug("register_profile: no free profile slot (max=%u)" APP_LOG_NL,
                  (unsigned)SL_GATT_CLIENT_MANAGER_MAX_PROFILES);
    (void)app_rta_release(sli_gcm_ctx);
    return SL_STATUS_NO_MORE_RESOURCE;
  }

  entry->in_use = true;
  entry->pair_count = profile->pair_count;
  for (uint8_t i = 0; i < profile->pair_count; i++) {
    entry->pairs[i] = profile->pairs[i];
  }

  *out_profile_handle = index;
  gcm_log_info("registered profile %u with %u pair(s)" APP_LOG_NL,
               index,
               profile->pair_count);
  for (uint8_t i = 0; i < profile->pair_count; i++) {
    gcm_log_uuid("svc", &entry->pairs[i].service);
    gcm_log_uuid("char", &entry->pairs[i].characteristic);
  }
  sli_gcm_set_profile_count(index + 1);
  (void)app_rta_release(sli_gcm_ctx);
  return SL_STATUS_OK;
}

sl_status_t sl_gatt_client_manager_start_discovery(uint8_t connection)
{
  connection_entry_t *entry;
  sl_status_t sc = app_rta_acquire(sli_gcm_ctx);

  if (sc != SL_STATUS_OK) {
    sli_gcm_on_runtime_error(APP_RTA_ERROR_ACQUIRE_FAILED, sc);
    return sc;
  }

  if (!sli_gcm_is_initialized()) {
    gcm_log_debug("start_discovery: not initialized conn=%u" APP_LOG_NL,
                  connection);
    (void)app_rta_release(sli_gcm_ctx);
    return SL_STATUS_NOT_INITIALIZED;
  }

  entry = sli_gcm_alloc_connection_entry(connection);
  if (entry == NULL) {
    (void)app_rta_release(sli_gcm_ctx);
    return SL_STATUS_NO_MORE_RESOURCE;
  }
  sc = sli_gcm_begin_run(entry, true, SL_GATT_CLIENT_MANAGER_INVALID_PROFILE_HANDLE, false);
  (void)app_rta_release(sli_gcm_ctx);
  return sc;
}

sl_status_t sl_gatt_client_manager_discover_profile(uint8_t connection,
                                                    uint16_t profile_handle)
{
  connection_entry_t *entry;
  sl_status_t sc = app_rta_acquire(sli_gcm_ctx);

  if (sc != SL_STATUS_OK) {
    sli_gcm_on_runtime_error(APP_RTA_ERROR_ACQUIRE_FAILED, sc);
    return sc;
  }

  if (!sli_gcm_is_initialized()) {
    gcm_log_debug("discover_profile: not initialized conn=%u profile=%u"
                  APP_LOG_NL,
                  connection,
                  profile_handle);
    (void)app_rta_release(sli_gcm_ctx);
    return SL_STATUS_NOT_INITIALIZED;
  }
  if (!sli_gcm_profile_handle_is_valid(profile_handle)) {
    gcm_log_debug("discover_profile: invalid profile %u conn=%u" APP_LOG_NL,
                  profile_handle,
                  connection);
    (void)app_rta_release(sli_gcm_ctx);
    return SL_STATUS_INVALID_HANDLE;
  }

  entry = sli_gcm_alloc_connection_entry(connection);
  if (entry == NULL) {
    (void)app_rta_release(sli_gcm_ctx);
    return SL_STATUS_NO_MORE_RESOURCE;
  }
  sc = sli_gcm_begin_run(entry, false, profile_handle, false);
  (void)app_rta_release(sli_gcm_ctx);
  return sc;
}

sl_status_t sl_gatt_client_manager_refresh_discovery(uint8_t connection)
{
  connection_entry_t *entry;
  sl_status_t sc = app_rta_acquire(sli_gcm_ctx);

  if (sc != SL_STATUS_OK) {
    sli_gcm_on_runtime_error(APP_RTA_ERROR_ACQUIRE_FAILED, sc);
    return sc;
  }

  if (!sli_gcm_is_initialized()) {
    gcm_log_debug("refresh_discovery: not initialized conn=%u" APP_LOG_NL,
                  connection);
    (void)app_rta_release(sli_gcm_ctx);
    return SL_STATUS_NOT_INITIALIZED;
  }

  entry = sli_gcm_alloc_connection_entry(connection);
  if (entry == NULL) {
    (void)app_rta_release(sli_gcm_ctx);
    return SL_STATUS_NO_MORE_RESOURCE;
  }
  sc = sli_gcm_begin_run(entry, true, SL_GATT_CLIENT_MANAGER_INVALID_PROFILE_HANDLE, true);
  (void)app_rta_release(sli_gcm_ctx);
  return sc;
}

sl_status_t sl_gatt_client_manager_force_discovery(uint8_t connection)
{
  connection_entry_t *entry;
  sl_status_t sc = app_rta_acquire(sli_gcm_ctx);

  if (sc != SL_STATUS_OK) {
    sli_gcm_on_runtime_error(APP_RTA_ERROR_ACQUIRE_FAILED, sc);
    return sc;
  }

  if (!sli_gcm_is_initialized()) {
    gcm_log_debug("force_discovery: not initialized conn=%u" APP_LOG_NL,
                  connection);
    (void)app_rta_release(sli_gcm_ctx);
    return SL_STATUS_NOT_INITIALIZED;
  }

  entry = sli_gcm_alloc_connection_entry(connection);
  if (entry == NULL) {
    (void)app_rta_release(sli_gcm_ctx);
    return SL_STATUS_NO_MORE_RESOURCE;
  }
  if (sli_gcm_get_profile_count() == 0) {
    gcm_log_warning("force discovery skipped: no profiles registered conn=%u"
                    APP_LOG_NL,
                    connection);
    (void)app_rta_release(sli_gcm_ctx);
    return SL_STATUS_INVALID_STATE;
  }
  if (sli_gcm_discovery_busy(entry)) {
    gcm_log_warning("force discovery: aborting in-flight run conn=%u" APP_LOG_NL,
                    connection);
    sli_gcm_abort_run(entry, true);
  }
  sc = sli_gcm_request_run(entry,
                           true,
                           SL_GATT_CLIENT_MANAGER_INVALID_PROFILE_HANDLE,
                           true);
  (void)app_rta_release(sli_gcm_ctx);
  return sc;
}

sl_status_t sl_gatt_client_manager_get_database_hash(uint8_t connection,
                                                     uint8_t *out_hash)
{
#if SL_GATT_CLIENT_MANAGER_CACHE_ENABLE
  connection_entry_t *entry;
#endif
  sl_status_t sc;

  if (out_hash == NULL) {
    gcm_log_debug("get_database_hash: null out_hash conn=%u" APP_LOG_NL,
                  connection);
    return SL_STATUS_NULL_POINTER;
  }

  sc = app_rta_acquire(sli_gcm_ctx);
  if (sc != SL_STATUS_OK) {
    sli_gcm_on_runtime_error(APP_RTA_ERROR_ACQUIRE_FAILED, sc);
    return sc;
  }

  if (!sli_gcm_is_initialized()) {
    gcm_log_debug("get_database_hash: not initialized conn=%u" APP_LOG_NL,
                  connection);
    (void)app_rta_release(sli_gcm_ctx);
    return SL_STATUS_NOT_INITIALIZED;
  }
#if SL_GATT_CLIENT_MANAGER_CACHE_ENABLE
  entry = sli_gcm_find_connection_entry(connection);
  if (entry == NULL) {
    gcm_log_debug("get_database_hash: unknown connection %u" APP_LOG_NL,
                  connection);
    (void)app_rta_release(sli_gcm_ctx);
    return SL_STATUS_NOT_FOUND;
  }
  if (entry->hash_state != db_hash_valid) {
    gcm_log_debug("get_database_hash: hash not available conn=%u state=%u"
                  APP_LOG_NL,
                  connection,
                  entry->hash_state);
    (void)app_rta_release(sli_gcm_ctx);
    return SL_STATUS_NOT_FOUND;
  }
  memcpy(out_hash, entry->db_hash, SL_GATT_CLIENT_MANAGER_DB_HASH_LEN);
  (void)app_rta_release(sli_gcm_ctx);
  return SL_STATUS_OK;
#else
  (void)connection;
  gcm_log_debug("get_database_hash: cache disabled" APP_LOG_NL);
  (void)app_rta_release(sli_gcm_ctx);
  return SL_STATUS_NOT_AVAILABLE;
#endif
}

sl_status_t sl_gatt_client_manager_cache_clear_all(void)
{
  sl_status_t sc = app_rta_acquire(sli_gcm_ctx);

  if (sc != SL_STATUS_OK) {
    sli_gcm_on_runtime_error(APP_RTA_ERROR_ACQUIRE_FAILED, sc);
    return sc;
  }
  if (!sli_gcm_is_initialized()) {
    gcm_log_debug("cache_clear_all: not initialized" APP_LOG_NL);
    (void)app_rta_release(sli_gcm_ctx);
    return SL_STATUS_NOT_INITIALIZED;
  }
#if SL_GATT_CLIENT_MANAGER_CACHE_ENABLE
  sli_gcm_cache_clear_all();
  gcm_log_info("cache cleared (all)" APP_LOG_NL);
  (void)app_rta_release(sli_gcm_ctx);
  return SL_STATUS_OK;
#else
  gcm_log_debug("cache_clear_all: cache disabled" APP_LOG_NL);
  (void)app_rta_release(sli_gcm_ctx);
  return SL_STATUS_NOT_AVAILABLE;
#endif
}

sl_status_t sl_gatt_client_manager_cache_clear_profile(uint16_t profile_handle)
{
  sl_status_t sc = app_rta_acquire(sli_gcm_ctx);

  if (sc != SL_STATUS_OK) {
    sli_gcm_on_runtime_error(APP_RTA_ERROR_ACQUIRE_FAILED, sc);
    return sc;
  }
  if (!sli_gcm_is_initialized()) {
    gcm_log_debug("cache_clear_profile: not initialized profile=%u" APP_LOG_NL,
                  profile_handle);
    (void)app_rta_release(sli_gcm_ctx);
    return SL_STATUS_NOT_INITIALIZED;
  }
  if (!sli_gcm_profile_handle_is_valid(profile_handle)) {
    gcm_log_debug("cache_clear_profile: invalid profile %u" APP_LOG_NL,
                  profile_handle);
    (void)app_rta_release(sli_gcm_ctx);
    return SL_STATUS_INVALID_HANDLE;
  }
#if SL_GATT_CLIENT_MANAGER_CACHE_ENABLE
  sc = sli_gcm_cache_clear_profile(profile_handle);
  if (sc == SL_STATUS_OK) {
    gcm_log_info("cache cleared (profile %u)" APP_LOG_NL, profile_handle);
  } else {
    gcm_log_debug("cache_clear_profile: no entry for profile %u" APP_LOG_NL,
                  profile_handle);
  }
  (void)app_rta_release(sli_gcm_ctx);
  return sc;
#else
  gcm_log_debug("cache_clear_profile: cache disabled" APP_LOG_NL);
  (void)app_rta_release(sli_gcm_ctx);
  return SL_STATUS_NOT_AVAILABLE;
#endif
}

void sl_gatt_client_manager_on_bt_event(sl_bt_msg_t *evt)
{
  connection_entry_t *entry;
  sl_status_t sc;

  if (evt == NULL) {
    return;
  }
  // The guard is recursive, so an application callback (caused by a FOUND or DISCOVERY_COMPLETE event) that steps back into 
  // a public API function nests safely without deadlocking.
  sc = app_rta_acquire(sli_gcm_ctx);
  if (sc != SL_STATUS_OK) {
    sli_gcm_on_runtime_error(APP_RTA_ERROR_ACQUIRE_FAILED, sc);
    return;
  }

  if (!sli_gcm_is_initialized()) {
    (void)app_rta_release(sli_gcm_ctx);
    return;
  }

  switch (SL_BT_MSG_ID(evt->header)) {
    case sl_bt_evt_connection_opened_id:
      entry = sli_gcm_alloc_connection_entry(evt->data.evt_connection_opened.connection);
      if (entry == NULL) {
        gcm_log_error("connection opened but no free slot conn=%u" APP_LOG_NL,
                      evt->data.evt_connection_opened.connection);
        sli_gcm_emit_error(evt->data.evt_connection_opened.connection,
                           SL_GATT_CLIENT_MANAGER_INVALID_PROFILE_HANDLE,
                           SL_GATT_CLIENT_MANAGER_INVALID_PAIR_INDEX,
                           SL_STATUS_NO_MORE_RESOURCE);
        break;
      }
      entry->peer_address = evt->data.evt_connection_opened.address;
      entry->peer_address_type = evt->data.evt_connection_opened.address_type;
      entry->peer_address_valid = true;
      gcm_log_info("connection opened conn=%u" APP_LOG_NL, entry->connection);
      gcm_log_bd_addr("peer", &entry->peer_address, entry->peer_address_type);
      sc = sli_gcm_on_connection_opened(entry);
      if (sc != SL_STATUS_OK) {
        sli_gcm_emit_error(evt->data.evt_connection_opened.connection,
                           SL_GATT_CLIENT_MANAGER_INVALID_PROFILE_HANDLE,
                           SL_GATT_CLIENT_MANAGER_INVALID_PAIR_INDEX,
                           sc);
      }
      break;

    case sl_bt_evt_connection_closed_id:
      gcm_log_info("connection closed conn=%u (cache retained)" APP_LOG_NL,
                   evt->data.evt_connection_closed.connection);
      sli_gcm_clear_connection_entry(evt->data.evt_connection_closed.connection);
      break;

    case sl_bt_evt_gatt_service_id:
      entry = sli_gcm_find_connection_entry(evt->data.evt_gatt_service.connection);
      if (entry != NULL) {
        sli_gcm_on_gatt_service(entry, evt->data.evt_gatt_service.service);
      }
      break;

    case sl_bt_evt_gatt_characteristic_id:
      entry = sli_gcm_find_connection_entry(evt->data.evt_gatt_characteristic.connection);
      if (entry != NULL) {
        sli_gcm_on_gatt_characteristic(entry,
                                       evt->data.evt_gatt_characteristic.characteristic);
      }
      break;

    case sl_bt_evt_gatt_procedure_completed_id:
      sli_gcm_handle_procedure_completed(evt->data.evt_gatt_procedure_completed.connection,
                                         evt->data.evt_gatt_procedure_completed.result);
      break;

    case sl_bt_evt_gatt_characteristic_value_id:
      sli_gcm_handle_characteristic_value(&evt->data.evt_gatt_characteristic_value);
      break;

    default:
      break;
  }
  (void)app_rta_release(sli_gcm_ctx);
}

// -----------------------------------------------------------------------------
// RTA lifecycle functions

void sli_gcm_rta_init(void)
{
  app_rta_config_t config = {
    .requirement.runtime = false,
    .requirement.guard   = true,
    .requirement.signal  = false,
    .step                = NULL,
    .priority            = 0,
    .stack_size          = 0,
    .error               = sli_gcm_on_runtime_error,
    .wait_for_guard      = SL_GATT_CLIENT_MANAGER_RTA_WAIT_FOR_GUARD
  };
  sl_status_t sc = app_rta_create_context(&config, &sli_gcm_ctx);
  if (sc != SL_STATUS_OK) {
    gcm_log_error("Failed to create RTA context, sc=0x%lx" APP_LOG_NL,
                  (unsigned long)sc);
  }
}

/// RTA callback and shared path for acquire failures from public APIs.
void sli_gcm_on_runtime_error(app_rta_error_t error, sl_status_t result)
{
  switch (error) {
    case APP_RTA_ERROR_RUNTIME_INIT_FAILED:
      gcm_log_error("RTA runtime init failed, sc=0x%lx" APP_LOG_NL,
                    (unsigned long)result);
      break;
    case APP_RTA_ERROR_ACQUIRE_FAILED:
      gcm_log_error("RTA acquire failed, sc=0x%lx" APP_LOG_NL,
                    (unsigned long)result);
      break;
    case APP_RTA_ERROR_RELEASE_FAILED:
      gcm_log_error("RTA release failed, sc=0x%lx" APP_LOG_NL,
                    (unsigned long)result);
      break;
    default:
      gcm_log_error("RTA generic error, sc=0x%lx" APP_LOG_NL,
                    (unsigned long)result);
      break;
  }
  sli_gcm_emit_error(SL_BT_INVALID_CONNECTION_HANDLE,
                     SL_GATT_CLIENT_MANAGER_INVALID_PROFILE_HANDLE,
                     SL_GATT_CLIENT_MANAGER_INVALID_PAIR_INDEX,
                     result);
}
