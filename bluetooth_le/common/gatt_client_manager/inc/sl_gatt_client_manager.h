/***************************************************************************//**
 * @file
 * @brief GATT Client Manager - profile discovery and GATT operation public API
 *
 * @warning This component is experimental and under active development. Its
 * API may change, and backward compatibility is not guaranteed. Use it at your
 * own risk.
 *
 * Profile-driven GATT discovery: the application registers a *profile*, which
 * is a set of service/characteristic identifier pairs. During discovery the
 * component emits handle-found events so the application can store handles
 * itself; the per-connection copy is dropped after a profile completes. An
 * optional RAM cache may keep a separate copy for later replay. GATT client
 * operations (read / write / CCCD / confirm) are routed through wrappers that
 * refuse work while discovery is in progress.
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

#ifndef SL_GATT_CLIENT_MANAGER_H
#define SL_GATT_CLIENT_MANAGER_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "sl_status.h"
#include "sl_bt_api.h"
#include "sl_gatt_client_manager_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/***************************************************************************//**
 * @addtogroup gatt_client_manager
 * @{
 *
 * @warning This component is experimental and under active development. Its
 * API may change, and backward compatibility is not guaranteed. Use it at your
 * own risk.
 *
 * @section gcm_features Features
 *
 * GATT Client Manager discovers GATT handles for application-defined
 * *profiles*, optionally caches those results, watches for peer database
 * changes, and exposes read / write / CCCD wrappers. The application is driven by events
 * @ref sl_gatt_client_manager_on_event.
 *
 * @subsection gcm_profiles Profile discovery
 *
 * A profile is a list of service/characteristic UUID pairs
 * (see @ref sl_gatt_client_manager_profile_t). After
 * @ref sl_gatt_client_manager_register_profile, discovery can start
 * automatically on @c sl_bt_evt_connection_opened when
 * @ref SL_GATT_CLIENT_MANAGER_AUTO_START_DISCOVERY is set, or be driven with
 * @ref sl_gatt_client_manager_start_discovery or
 * @ref sl_gatt_client_manager_discover_profile (one profile at a time).
 *
 * Each pair is resolved with primary-service then characteristic discovery.
 * If consecutive pairs in a profile share a service UUID, the handle from the
 * previous pair is reused and the extra primary-service search is skipped.
 * List identical services one after another to take advantage of this.
 * Pair flags ( @ref SL_GATT_CLIENT_MANAGER_FLAG_NOTIFY /
 * @ref SL_GATT_CLIENT_MANAGER_FLAG_INDICATE) cause a CCCD write once the
 * characteristic is found.
 *
 * A peer table may contain the same service more than once. The component
 * keeps **one instance per pair**, the matching service with the **lowest
 * handle**, then its characteristic. @ref sl_gatt_client_manager_discovery_complete_evt_t::match_count
 * reports how many service instances were seen so the application can detect ambiguity.
 *
 * @subsection gcm_handles Handle ownership
 *
 * Discovered handles belong to the application. They are reported as
 * @ref SL_GATT_CLIENT_MANAGER_EVENT_SERVICE_FOUND and
 * @ref SL_GATT_CLIENT_MANAGER_EVENT_CHARACTERISTIC_FOUND, and the component
 * drops its copy once that profile's
 * @ref SL_GATT_CLIENT_MANAGER_EVENT_DISCOVERY_COMPLETE has been delivered.
 * Store whatever you need from those events; there is no getter later.
 *
 * @subsection gcm_events Events
 *
 * All notifications go through @ref sl_gatt_client_manager_event_t (header
 * @c id selects the union member):
 *
 * - @ref SL_GATT_CLIENT_MANAGER_EVENT_SERVICE_FOUND /
 *   @ref SL_GATT_CLIENT_MANAGER_EVENT_CHARACTERISTIC_FOUND — a handle was
 *   resolved (including cache replay).
 * - @ref SL_GATT_CLIENT_MANAGER_EVENT_DISCOVERY_COMPLETE — one profile
 *   finished (success, missing pairs, or a GATT failure that ended the run).
 * - @ref SL_GATT_CLIENT_MANAGER_EVENT_ERROR — discovery, CCCD, or a wrapper
 *   failed. When a profile was already in progress, a discovery-complete
 *   event with the same status follows.
 * - @ref SL_GATT_CLIENT_MANAGER_EVENT_CHARACTERISTIC_VALUE — notification,
 *   indication, or read payload. The value pointer is valid only for the
 *   callback. Attribute it using a handle saved from CHARACTERISTIC_FOUND.
 * - @ref SL_GATT_CLIENT_MANAGER_EVENT_REDISCOVERY_NEEDED — the peer GATT
 *   database changed (see @ref gcm_robust_caching).
 *
 * @subsection gcm_lifecycle One completion per started profile
 *
 * Every profile whose discovery actually starts ends with exactly one
 * @ref SL_GATT_CLIENT_MANAGER_EVENT_DISCOVERY_COMPLETE. Waiting only for that
 * event is therefore safe. The exceptions never start a profile:
 * @ref sl_gatt_client_manager_force_discovery aborting an in-flight run, and
 * errors raised before any profile is selected, which are reported through
 * @ref SL_GATT_CLIENT_MANAGER_EVENT_ERROR.
 *
 * @subsection gcm_cache Optional RAM cache
 *
 * When @ref SL_GATT_CLIENT_MANAGER_CACHE_ENABLE is set, a completed profile
 * is stored keyed by profile identity plus the remote Database Hash (0x2B2A).
 * A later connection whose hash still matches skips GATT discovery and
 * replays the same FOUND events. Peers with no readable Database Hash are
 * never cached. Connection closed does not clear the table;
 * @ref sl_gatt_client_manager_cache_clear_all and
 * @ref sl_gatt_client_manager_cache_clear_profile do. Use
 * @ref sl_gatt_client_manager_refresh_discovery or
 * @ref sl_gatt_client_manager_force_discovery to ignore the cache.
 *
 * @subsection gcm_robust_caching Database change detection
 *
 * Two independent signals produce
 * @ref SL_GATT_CLIENT_MANAGER_EVENT_REDISCOVERY_NEEDED. When
 * @ref SL_GATT_CLIENT_MANAGER_AUTO_REDISCOVERY is set, the component also
 * starts a rediscovery automatically; otherwise the application should call
 * @ref sl_gatt_client_manager_refresh_discovery.
 *
 * **Service Changed (0x2A05).** When
 * @ref SL_GATT_CLIENT_MANAGER_ENABLE_SERVICE_CHANGED is set, the component
 * subscribes to indications once per connection during the Generic Attribute
 * phase. Those indications are consumed internally and treated as a database
 * change (@ref SL_GATT_CLIENT_MANAGER_EVENT_REDISCOVERY_NEEDED). Missing
 * 0x2A05, or a rejected CCCD write, is tolerated. Disable the option to skip
 * the subscribe entirely.
 *
 * **Robust caching (0x2B29).** When
 * @ref SL_GATT_CLIENT_MANAGER_ENABLE_ROBUST_CACHING is set, the component
 * sets the Robust Caching bit in Client Supported Features once per
 * connection, before the Database Hash is read. Only then will a GATT server
 * answer with ATT Database Out Of Sync. Disabling this option drops that
 * path; Service Changed remains available when enabled separately.
 *
 * Both steps are best effort. A peer with no Generic Attribute service, no
 * 0x2A05, no 0x2B29, or that rejects the writes, is discovered normally; it
 * simply cannot report later database changes by that means.
 *
 * @subsection gcm_ops GATT operation wrappers
 *
 * @ref sl_gatt_client_manager_read_characteristic_value,
 * @ref sl_gatt_client_manager_write_characteristic_value,
 * @ref sl_gatt_client_manager_write_characteristic_value_without_response and
 * @ref sl_gatt_client_manager_set_characteristic_notification refuse work
 * while discovery owns the connection. Confirmation
 * ( @ref sl_gatt_client_manager_send_characteristic_confirmation) stays
 * available so an indication during rediscovery can still be acknowledged.
 * Wrappers act only on connections already reported by
 * @c sl_bt_evt_connection_opened.
 *
 * @section gcm_limitations Known limitations
 *
 * - **Duplicate service UUIDs in a profile.** Listing the same service UUID
 *   on several pairs still consumes one pair slot each in the profile table,
 *   per-connection results, and cache. Identical services should be listed
 *   consecutively to allow reuse of previously discovered handles.
 * - **No bonding or identity.** The component does not look at whether the
 *   connection is bonded, nor does it key the cache or Service Changed /
 *   Client Supported Features state by identity address or bonding handle.
 *   Cache entries are shared by any peer that presents the same Database
 *   Hash. When enabled, Service Changed CCCD and robust-caching bits are
 *   offered again on each new connection rather than restored from a prior bond.
 * - **RAM cache only.** Discovery results are not written to NVM. They
 *   survive disconnects until evicted (LRU) or cleared, and are lost on
 *   reset or power cycle.
 * - **Service Changed range ignored.** An indication's start/end handles are
 *   not parsed; any Service Changed or Out Of Sync condition rediscovers the
 *   whole registered set.
 * - **Primary services and characteristics only.** Included services,
 *   descriptors other than CCCD writes, and 32-bit UUIDs are not discovered.
 * - **One GATT procedure at a time** per connection, including the Generic
 *   Attribute phase. Profiles run sequentially.
 * - **No profile unregistration** except @ref sl_gatt_client_manager_reset.
 * - **Fixed tables.** Compile-time caps
 *   ( @ref SL_GATT_CLIENT_MANAGER_MAX_PROFILES,
 *   @ref SL_GATT_CLIENT_MANAGER_MAX_PAIRS_PER_PROFILE,
 *   @ref SL_GATT_CLIENT_MANAGER_MAX_CONNECTIONS,
 *   @ref SL_GATT_CLIENT_MANAGER_MAX_CACHE_ENTRIES) are the only capacity
 *   control.
 ******************************************************************************/

/***************************************************************************//**
 * Initialize the GATT Client Manager.
 *
 * Clears every profile registration, connection state and cache entry, then
 * marks the component ready for use. Call once before any other API.
 *
 * @retval SL_STATUS_OK Component initialized.
 ******************************************************************************/
sl_status_t sl_gatt_client_manager_init(void);

/***************************************************************************//**
 * Reset the GATT Client Manager to its power-on initial state.
 *
 * Discards all registered profiles, per-connection state and cache entries, and
 * leaves the component uninitialized. @ref sl_gatt_client_manager_init must be
 * called again before further use. Intended for tests and error recovery.
 *
 * @retval SL_STATUS_OK Component reset.
 ******************************************************************************/
sl_status_t sl_gatt_client_manager_reset(void);

/***************************************************************************//**
 * Register a profile: a set of service/characteristic pairs.
 *
 * The pair array is copied, so the caller's array does not need to outlive the
 * call. Profiles are discovered in registration order.
 *
 * @param[in]  profile            Profile to register.
 * @param[out] out_profile_handle Receives the handle identifying this profile
 *                                in subsequent calls and events.
 *
 * @retval SL_STATUS_OK                Profile registered.
 * @retval SL_STATUS_NOT_INITIALIZED   Component not initialized.
 * @retval SL_STATUS_NULL_POINTER      @p profile, @p out_profile_handle or
 *                                     @p profile->pairs is NULL.
 * @retval SL_STATUS_INVALID_PARAMETER @c pair_count is zero or a UUID is malformed.
 * @retval SL_STATUS_NO_MORE_RESOURCE  Profile table is full, or @c pair_count exceeds
 *   @ref SL_GATT_CLIENT_MANAGER_MAX_PAIRS_PER_PROFILE.
 ******************************************************************************/
sl_status_t sl_gatt_client_manager_register_profile(
  const sl_gatt_client_manager_profile_t *profile,
  uint16_t *out_profile_handle);

/***************************************************************************//**
 * Discover all registered profiles on a connection, in registration order.
 *
 * Every resolved handle is reported through a
 * @ref SL_GATT_CLIENT_MANAGER_EVENT_SERVICE_FOUND and a
 * @ref SL_GATT_CLIENT_MANAGER_EVENT_CHARACTERISTIC_FOUND event; one
 * @ref SL_GATT_CLIENT_MANAGER_EVENT_DISCOVERY_COMPLETE event follows per
 * profile. Cached results are reused when the peer's Database Hash matches.
 *
 * @param[in] connection Connection handle.
 *
 * @retval SL_STATUS_OK               Discovery started.
 * @retval SL_STATUS_NOT_INITIALIZED  Component not initialized.
 * @retval SL_STATUS_NO_MORE_RESOURCE No free connection slot.
 * @retval SL_STATUS_IN_PROGRESS      Another discovery owns @p connection.
 * @retval SL_STATUS_INVALID_STATE    No profiles registered.
 ******************************************************************************/
sl_status_t sl_gatt_client_manager_start_discovery(uint8_t connection);

/***************************************************************************//**
 * Discover a single profile on a connection.
 *
 * Emits the same event sequence as @ref sl_gatt_client_manager_start_discovery
 * for the selected profile only. Useful when the application drives discovery
 * one profile at a time, typically starting the next profile from the previous
 * discovery-complete event.
 *
 * @param[in] connection     Connection handle.
 * @param[in] profile_handle Profile to discover.
 *
 * @retval SL_STATUS_OK               Discovery started.
 * @retval SL_STATUS_NOT_INITIALIZED  Component not initialized.
 * @retval SL_STATUS_INVALID_HANDLE   @p profile_handle is not registered.
 * @retval SL_STATUS_NO_MORE_RESOURCE No free connection slot.
 * @retval SL_STATUS_IN_PROGRESS      Another discovery owns @p connection.
 ******************************************************************************/
sl_status_t sl_gatt_client_manager_discover_profile(uint8_t connection,
                                                    uint16_t profile_handle);

/***************************************************************************//**
 * Force rediscovery of all registered profiles, ignoring the cache.
 *
 * Re-reads the peer's Database Hash and performs a full discovery, so the
 * refreshed results are cached under the peer's current database identity. Use
 * when the peer's GATT table is known to have changed.
 *
 * @param[in] connection Connection handle.
 *
 * @retval SL_STATUS_OK               Discovery started.
 * @retval SL_STATUS_NOT_INITIALIZED  Component not initialized.
 * @retval SL_STATUS_NO_MORE_RESOURCE No free connection slot.
 * @retval SL_STATUS_IN_PROGRESS      Another discovery owns @p connection.
 * @retval SL_STATUS_INVALID_STATE    No profiles registered.
 ******************************************************************************/
sl_status_t sl_gatt_client_manager_refresh_discovery(uint8_t connection);

/***************************************************************************//**
 * Force discovery even if discovery is already in progress (recovery / "sudo").
 *
 * Aborts any in-flight discovery on the connection, bypasses the cache, and
 * starts a full rediscovery of all registered profiles. Use to recover from
 * failure states after @ref SL_GATT_CLIENT_MANAGER_EVENT_REDISCOVERY_NEEDED
 * when auto-rediscovery is disabled.
 *
 * @param[in] connection Connection handle.
 *
 * @retval SL_STATUS_OK               Discovery started, or queued behind the
 *                                    aborted run (see note).
 * @retval SL_STATUS_NOT_INITIALIZED  Component not initialized.
 * @retval SL_STATUS_NO_MORE_RESOURCE No free connection slot.
 * @retval SL_STATUS_INVALID_STATE    No profiles registered.
 *
 * @note Unlike the other discovery calls this never returns
 *       @ref SL_STATUS_IN_PROGRESS, because an active run is aborted first. No
 *       discovery-complete event is emitted for the aborted run.
 * @note A GATT procedure already accepted by the stack cannot be cancelled. If
 *       one is outstanding, this call returns once the rediscovery is queued,
 *       and the new run starts from that procedure's completion. Until then
 *       the connection still counts as busy, so the other discovery calls and
 *       the GATT operation wrappers keep returning @ref SL_STATUS_IN_PROGRESS.
 ******************************************************************************/
sl_status_t sl_gatt_client_manager_force_discovery(uint8_t connection);

/***************************************************************************//**
 * Get the remote GATT Database Hash read on a connection (if still known).
 *
 * @param[in]  connection Connection handle.
 * @param[out] out_hash   Buffer of at least
 *                        @ref SL_GATT_CLIENT_MANAGER_DB_HASH_LEN bytes.
 *
 * @retval SL_STATUS_OK              Hash copied into @p out_hash.
 * @retval SL_STATUS_NOT_INITIALIZED Component not initialized.
 * @retval SL_STATUS_NULL_POINTER    @p out_hash is NULL.
 * @retval SL_STATUS_NOT_FOUND       Connection unknown, or the peer exposes no
 *                                   readable Database Hash.
 * @retval SL_STATUS_NOT_AVAILABLE   Caching disabled at compile time.
 ******************************************************************************/
sl_status_t sl_gatt_client_manager_get_database_hash(uint8_t connection,
                                                     uint8_t *out_hash);

/***************************************************************************//**
 * Clear all cache entries.
 *
 * @retval SL_STATUS_OK              Cache cleared.
 * @retval SL_STATUS_NOT_INITIALIZED Component not initialized.
 * @retval SL_STATUS_NOT_AVAILABLE   Caching disabled at compile time.
 ******************************************************************************/
sl_status_t sl_gatt_client_manager_cache_clear_all(void);

/***************************************************************************//**
 * Clear all cache entries belonging to one profile.
 *
 * @param[in] profile_handle Profile whose cached results should be dropped.
 *
 * @retval SL_STATUS_OK              At least one entry was cleared.
 * @retval SL_STATUS_NOT_INITIALIZED Component not initialized.
 * @retval SL_STATUS_INVALID_HANDLE  @p profile_handle is not registered.
 * @retval SL_STATUS_NOT_FOUND       No cache entry matched the profile.
 * @retval SL_STATUS_NOT_AVAILABLE   Caching disabled at compile time.
 ******************************************************************************/
sl_status_t sl_gatt_client_manager_cache_clear_profile(uint16_t profile_handle);

// -----------------------------------------------------------------------------
// GATT operation wrappers (blocked while discovery is in progress)
//
// Every wrapper needs a connection the component already knows, so it acts
// only on connections reported by sl_bt_evt_connection_opened; anything else
// is rejected with SL_STATUS_NOT_FOUND rather than taking up a slot.
//
// They refuse to start while a discovery run owns the connection and forward
// the stack status otherwise. Sending an indication confirmation is the sole
// exception: it stays available during discovery, because subscriptions from
// an earlier run remain active across a rediscovery.
//
// An out-of-sync peer emits SL_GATT_CLIENT_MANAGER_EVENT_REDISCOVERY_NEEDED and
// starts rediscovery when SL_GATT_CLIENT_MANAGER_AUTO_REDISCOVERY is enabled.
// This is detected both when the call itself returns
// SL_STATUS_BT_ATT_OUT_OF_SYNC and when the stack reports it later through the
// procedure completion, which is the usual path for an accepted operation.
//
// Characteristic handles come from
// SL_GATT_CLIENT_MANAGER_EVENT_CHARACTERISTIC_FOUND; the component does not
// retain them after discovery completes.

/***************************************************************************//**
 * Read a characteristic value from the peer.
 *
 * The value arrives as a
 * @ref SL_GATT_CLIENT_MANAGER_EVENT_CHARACTERISTIC_VALUE event carrying the ATT
 * read-response opcode.
 *
 * @param[in] connection     Connection handle.
 * @param[in] characteristic Characteristic handle.
 *
 * @retval SL_STATUS_OK               Read procedure started.
 * @retval SL_STATUS_NOT_INITIALIZED  Component not initialized.
 * @retval SL_STATUS_NOT_FOUND        @p connection is not open.
 * @retval SL_STATUS_IN_PROGRESS      Discovery is running on @p connection.
 *
 * @note Any other status is passed through from the Bluetooth stack.
 ******************************************************************************/
sl_status_t sl_gatt_client_manager_read_characteristic_value(
  uint8_t connection,
  uint16_t characteristic);

/***************************************************************************//**
 * Write a characteristic value (with response).
 *
 * @param[in] connection     Connection handle.
 * @param[in] characteristic Characteristic handle.
 * @param[in] value_len      Number of bytes in @p value.
 * @param[in] value          Value to write.
 *
 * @retval SL_STATUS_OK               Write procedure started.
 * @retval SL_STATUS_NOT_INITIALIZED  Component not initialized.
 * @retval SL_STATUS_NOT_FOUND        @p connection is not open.
 * @retval SL_STATUS_IN_PROGRESS      Discovery is running on @p connection.
 *
 * @note Any other status is passed through from the Bluetooth stack.
 ******************************************************************************/
sl_status_t sl_gatt_client_manager_write_characteristic_value(
  uint8_t connection,
  uint16_t characteristic,
  size_t value_len,
  const uint8_t *value);

/***************************************************************************//**
 * Write a characteristic value without response.
 *
 * @param[in] connection     Connection handle.
 * @param[in] characteristic Characteristic handle.
 * @param[in] value_len      Number of bytes in @p value.
 * @param[in] value          Value to write.
 *
 * @retval SL_STATUS_OK               Value handed to the stack.
 * @retval SL_STATUS_NOT_INITIALIZED  Component not initialized.
 * @retval SL_STATUS_NOT_FOUND        @p connection is not open.
 * @retval SL_STATUS_IN_PROGRESS      Discovery is running on @p connection.
 *
 * @note Any other status is passed through from the Bluetooth stack. The number
 *       of bytes actually sent is not reported; use
 *       @ref sl_gatt_client_manager_write_characteristic_value when the write
 *       must be acknowledged.
 ******************************************************************************/
sl_status_t sl_gatt_client_manager_write_characteristic_value_without_response(
  uint8_t connection,
  uint16_t characteristic,
  size_t value_len,
  const uint8_t *value);

/***************************************************************************//**
 * Enable or disable notifications / indications on a characteristic.
 *
 * @param[in] connection     Connection handle.
 * @param[in] characteristic Characteristic handle.
 * @param[in] flags          @ref SL_GATT_CLIENT_MANAGER_FLAG_NOTIFY,
 *                           @ref SL_GATT_CLIENT_MANAGER_FLAG_INDICATE, or
 *                           @ref SL_GATT_CLIENT_MANAGER_FLAG_NONE to
 *                           unsubscribe. Indications win when both are set.
 *
 * @retval SL_STATUS_OK               CCCD write started.
 * @retval SL_STATUS_NOT_INITIALIZED  Component not initialized.
 * @retval SL_STATUS_NOT_FOUND        @p connection is not open.
 * @retval SL_STATUS_IN_PROGRESS      Discovery is running on @p connection.
 *
 * @note Any other status is passed through from the Bluetooth stack. Pairs that
 *       declare a subscription flag are subscribed automatically during
 *       discovery; this call is for subscriptions the application manages.
 ******************************************************************************/
sl_status_t sl_gatt_client_manager_set_characteristic_notification(
  uint8_t connection,
  uint16_t characteristic,
  uint8_t flags);

/***************************************************************************//**
 * Send a characteristic confirmation for an indication.
 *
 * Only needed when @ref SL_GATT_CLIENT_MANAGER_AUTO_CONFIRM_INDICATIONS is
 * disabled; otherwise the component confirms indications itself.
 *
 * @param[in] connection Connection handle.
 *
 * @retval SL_STATUS_OK               Confirmation sent.
 * @retval SL_STATUS_NOT_INITIALIZED  Component not initialized.
 * @retval SL_STATUS_NOT_FOUND        @p connection is not open.
 *
 * @note Any other status is passed through from the Bluetooth stack.
 * @note This is the one wrapper that still works while discovery is running,
 *       so an indication arriving during a rediscovery can be acknowledged
 *       instead of stalling ATT on the connection.
 ******************************************************************************/
sl_status_t sl_gatt_client_manager_send_characteristic_confirmation(
  uint8_t connection);

/***************************************************************************//**
 * GATT Client Manager event handler.
 *
 * Weak no-op default. Override it in the application to receive discovery,
 * value and rediscovery notifications, the same way
 * sl_bt_on_event() overrides the weak stack event handler.
 *
 * @param[in] event Event to handle. The event and every pointer it carries are
 *                  only valid for the duration of this call.
 ******************************************************************************/
void sl_gatt_client_manager_on_event(const sl_gatt_client_manager_event_t *event);

/***************************************************************************//**
 * Bluetooth stack event handler.
 *
 * Drives the discovery state machine and turns stack events into GATT Client
 * Manager events. Wired automatically through bluetooth_on_event; call directly
 * only when the application dispatches stack events itself.
 *
 * @param[in] evt Bluetooth stack event. Ignored when NULL or when the component
 *                is not initialized.
 ******************************************************************************/
void sl_gatt_client_manager_on_bt_event(sl_bt_msg_t *evt);

/** @} (end addtogroup gatt_client_manager) */

#ifdef __cplusplus
}
#endif

#endif // SL_GATT_CLIENT_MANAGER_H
