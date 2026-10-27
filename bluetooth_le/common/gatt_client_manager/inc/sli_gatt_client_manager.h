/***************************************************************************//**
 * @file
 * @brief GATT Client Manager internal API
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

#ifndef SLI_GATT_CLIENT_MANAGER_H
#define SLI_GATT_CLIENT_MANAGER_H

#include <stddef.h>

#include "sl_status.h"
#include "sl_bt_api.h"
#include "app_rta.h"
#include "sl_gatt_client_manager.h"
#include "sli_gatt_client_manager_types.h"

#ifdef __cplusplus
extern "C" {
#endif

// -----------------------------------------------------------------------------
// Logging (active only when app_log is present and LOG is enabled)
#ifdef SL_CATALOG_APP_LOG_PRESENT
#include "app_log.h"
#endif

#if defined(SL_CATALOG_APP_LOG_PRESENT) && SL_GATT_CLIENT_MANAGER_LOG
#define gcm_log_debug(...)   app_log_debug(SL_GATT_CLIENT_MANAGER_LOG_PREFIX __VA_ARGS__)
#define gcm_log_info(...)    app_log_info(SL_GATT_CLIENT_MANAGER_LOG_PREFIX __VA_ARGS__)
#define gcm_log_warning(...) app_log_warning(SL_GATT_CLIENT_MANAGER_LOG_PREFIX __VA_ARGS__)
#define gcm_log_error(...)   app_log_error(SL_GATT_CLIENT_MANAGER_LOG_PREFIX __VA_ARGS__)
#else
// Arguments of the no-op macros are still expanded, so APP_LOG_NL must exist.
#ifndef APP_LOG_NL
#define APP_LOG_NL ""
#endif
#define gcm_log_debug(...)
#define gcm_log_info(...)
#define gcm_log_warning(...)
#define gcm_log_error(...)
#endif

// Additional logging functions
#if defined(SL_CATALOG_APP_LOG_PRESENT) && SL_GATT_CLIENT_MANAGER_LOG
void gcm_log_uuid(const char *label, const sl_gatt_client_manager_uuid_t *uuid);
void gcm_log_bd_addr(const char *label, const bd_addr *address, uint8_t address_type);
#if SL_GATT_CLIENT_MANAGER_CACHE_ENABLE
void gcm_log_hash(const char *label, const uint8_t *hash);
#else
#define gcm_log_hash(...)
#endif
#else
#define gcm_log_uuid(...)
#define gcm_log_bd_addr(...)
#define gcm_log_hash(...)
#endif

// -----------------------------------------------------------------------------
// Constants (Bluetooth SIG assigned numbers)

#define GATT_SERVICE_UUID_16              (0x1801) ///< Generic Attribute service
#define DATABASE_HASH_UUID_16             (0x2B2A)
#define CLIENT_SUPPORTED_FEATURES_UUID_16 (0x2B29) ///< CSF characteristic
#define SERVICE_CHANGED_UUID_16           (0x2A05)
#define ROBUST_CACHING_BIT                (0x01)   ///< Bit 0 of CSF value

// Generic Attribute (0x1801) is visited when at least one of Service Changed,
// robust caching (CSF), or Database Hash (cache) is enabled.
#if (SL_GATT_CLIENT_MANAGER_ENABLE_SERVICE_CHANGED   \
     || SL_GATT_CLIENT_MANAGER_ENABLE_ROBUST_CACHING \
     || SL_GATT_CLIENT_MANAGER_CACHE_ENABLE)
#define GCM_NEEDS_GATT_SERVICE  (1)
#else
#define GCM_NEEDS_GATT_SERVICE  (0)
#endif

// -----------------------------------------------------------------------------
// RTA (Application Runtime Adaptor)

// Guard context protecting shared module state like profile table, connection
// table, cache. It is only acquired by functions in the public API in
// sl_gatt_client_manager.c / sl_gatt_client_manager_gatt_ops.c and the
// Bluetooth stack event task. Internal functions do not acquire it.
// Context is required because both the application task and the Bluetooth stack
// event task need to access the module.
extern app_rta_context_t sli_gcm_ctx;

/******************************************************************************
 * Create the RTA guard context for the GATT Client Manager.
 *
 * @note Invoked by the app_rta framework via the 'app_rta_init' template
 *       contribution. Configures a guard-only context.
 *****************************************************************************/
void sli_gcm_rta_init(void);

/******************************************************************************
 * RTA error callback and shared handler for acquire/release failures.
 *****************************************************************************/
void sli_gcm_on_runtime_error(app_rta_error_t error, sl_status_t result);

// -----------------------------------------------------------------------------
// Module state (profiles table + init flag)

/// True after sl_gatt_client_manager_init; false after reset / clear.
bool sli_gcm_is_initialized(void);
/// Wipe registered profiles (does not touch connections or cache).
void sli_gcm_clear_module_state(void);

const profile_entry_t *sli_gcm_get_profile(uint16_t profile_handle);
profile_entry_t *sli_gcm_get_profile_mutable(uint16_t profile_handle);
uint16_t sli_gcm_get_profile_count(void);
void sli_gcm_set_profile_count(uint16_t count);

bool sli_gcm_profile_handle_is_valid(uint16_t profile_handle);
/// Find the next in-use profile handle at or after @p from.
bool sli_gcm_find_profile_from(uint16_t from, uint16_t *out_handle);
/// Advance entry->current_profile to the next registered profile.
bool sli_gcm_select_next_profile(connection_entry_t *entry);
/// Content equality of two interest arrays (cache key / clear-by-profile).
bool sli_gcm_pairs_equal(const sl_gatt_client_manager_interest_t *a,
                         const sl_gatt_client_manager_interest_t *b,
                         uint8_t pair_count);
bool sli_gcm_uuid_is_valid(const sl_gatt_client_manager_uuid_t *uuid);
bool sli_gcm_uuid_equal(const sl_gatt_client_manager_uuid_t *a,
                        const sl_gatt_client_manager_uuid_t *b);
/// Encode a UUID into little-endian bytes for stack discover/read APIs.
void sli_gcm_uuid_to_bytes(const sl_gatt_client_manager_uuid_t *uuid,
                           uint8_t *out,
                           size_t *out_len);
/// Reserve the next free profile slot; returns NULL if the table is full.
profile_entry_t *sli_gcm_register_profile_storage(uint16_t *out_index);

// -----------------------------------------------------------------------------
// Connection table

connection_entry_t *sli_gcm_find_connection_entry(uint8_t connection);
connection_entry_t *sli_gcm_alloc_connection_entry(uint8_t connection);
void sli_gcm_clear_connection_entry(uint8_t connection);
void sli_gcm_clear_profile_results(connection_entry_t *entry,
                                   uint16_t profile_handle);
void sli_gcm_clear_all_results(connection_entry_t *entry);
void sli_gcm_clear_all_connections(void);

// -----------------------------------------------------------------------------
// Events

/// Log one line for @p event (gated by SL_GATT_CLIENT_MANAGER_LOG_EVENTS).
void sli_gcm_log_event(const sl_gatt_client_manager_event_t *event);

void sli_gcm_emit_error(uint8_t connection,
                        uint16_t profile_handle,
                        uint8_t pair_index,
                        sl_status_t status);
/// Build DISCOVERY_COMPLETE from entry->results for the current profile.
void sli_gcm_emit_discovery_complete(connection_entry_t *entry,
                                     sl_status_t failure_status);
void sli_gcm_emit_service_found(uint8_t connection,
                                uint16_t profile_handle,
                                uint8_t pair_index,
                                uint32_t service_handle);
void sli_gcm_emit_characteristic_found(uint8_t connection,
                                       uint16_t profile_handle,
                                       uint8_t pair_index,
                                       uint32_t service_handle,
                                       uint16_t characteristic_handle);
void sli_gcm_emit_rediscovery_needed(uint8_t connection,
                                     sl_status_t status,
                                     bool auto_started);

// -----------------------------------------------------------------------------
// Discovery

/// Any non-idle discovery state (Generic Attribute phase or profile work).
bool sli_gcm_discovery_busy(const connection_entry_t *entry);
/// Stack procedure in flight (or aborting); do not start another yet.
bool sli_gcm_procedure_outstanding(const connection_entry_t *entry);
/// True only while a registered profile is mid-discovery / cache restore.
bool sli_gcm_profile_discovery_in_progress(const connection_entry_t *entry);
/// Cancel the current run; @p drain waits for procedure_completed if needed.
void sli_gcm_abort_run(connection_entry_t *entry, bool drain);
/// Start discovering @p profile_handle, or all profiles when @p run_all is set.
sl_status_t sli_gcm_begin_run(connection_entry_t *entry,
                              bool run_all,
                              uint16_t profile_handle,
                              bool force_refresh);
/// Like begin_run, but defers if the connection is still draining an abort.
sl_status_t sli_gcm_request_run(connection_entry_t *entry,
                                bool run_all,
                                uint16_t profile_handle,
                                bool force_refresh);
/// Drive the next profile step (cache restore or GATT discovery).
void sli_gcm_run_profiles(connection_entry_t *entry);
void sli_gcm_handle_procedure_completed(uint8_t connection, uint16_t result);
void sli_gcm_handle_characteristic_value(
  sl_bt_evt_gatt_characteristic_value_t *evt);
/// Peer DB changed (Service Changed / Out Of Sync); may auto-restart discovery.
void sli_gcm_trigger_rediscovery(connection_entry_t *entry, sl_status_t status);
void sli_gcm_on_gatt_service(connection_entry_t *entry,
                             uint32_t service_handle);
void sli_gcm_on_gatt_characteristic(connection_entry_t *entry,
                                    uint16_t characteristic_handle);
/// Auto-start discovery on connect when configured.
sl_status_t sli_gcm_on_connection_opened(connection_entry_t *entry);

// -----------------------------------------------------------------------------
// Cache

#if SL_GATT_CLIENT_MANAGER_CACHE_ENABLE
/// Store the current profile's complete results under (pairs, db_hash).
void sli_gcm_cache_store(connection_entry_t *conn);
/// If a matching entry exists, copy handles into @p conn and emit FOUND events.
bool sli_gcm_cache_try_restore(connection_entry_t *conn);
void sli_gcm_cache_clear_all(void);
/// Drop every entry whose pair content matches the given profile.
sl_status_t sli_gcm_cache_clear_profile(uint16_t profile_handle);
#if SL_GATT_CLIENT_MANAGER_ENABLE_ROBUST_CACHING
/// Look up a previously cached Client Supported Features handle for this peer.
bool sli_gcm_cache_find_csf_for_peer(const connection_entry_t *conn,
                                     uint16_t *out_handle);
#endif
#endif

// -----------------------------------------------------------------------------
// GATT operation helpers

/// Map SL_GATT_CLIENT_MANAGER_FLAG_* to sl_bt_gatt notification/indication flags.
uint8_t sli_gcm_subscription_flag_to_gatt(uint8_t flags);
/// React to Out Of Sync on a public GATT wrapper call result.
void sli_gcm_handle_gatt_call_result(connection_entry_t *entry, sl_status_t sc);

/***************************************************************************//**
 * Deliver one event to every subscriber.
 *
 * Defined by the autogenerated dispatcher, which calls the handlers contributed
 * to `gatt_client_manager_on_event` in priority order and then
 * @ref sl_gatt_client_manager_on_event.
 *
 * @param[in] event Event to deliver.
 ******************************************************************************/
void sli_gatt_client_manager_dispatch_event(
  const sl_gatt_client_manager_event_t *event);

#ifdef __cplusplus
}
#endif

#endif // SLI_GATT_CLIENT_MANAGER_H
