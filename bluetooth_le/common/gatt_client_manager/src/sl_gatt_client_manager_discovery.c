/***************************************************************************//**
 * @file
 * @brief GATT Client Manager - discovery state machine
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

#include "sl_bt_api.h"
#include "sli_gatt_client_manager.h"

// Profile discovery helpers (service → characteristic → optional CCCD).
static sl_status_t discover_service_for_current(connection_entry_t *entry);
static sl_status_t discover_characteristic_for_current(connection_entry_t *entry);
static sl_status_t advance_pair_or_cccd(connection_entry_t *entry);
static sl_status_t configure_next_cccd(connection_entry_t *entry);
static sl_status_t start_current_profile(connection_entry_t *entry);
static void fail_current_profile(connection_entry_t *entry,
                                 sl_status_t sc,
                                 uint8_t pair_context,
                                 uint8_t generation);
static void continue_after_step(connection_entry_t *entry,
                                sl_status_t sc,
                                uint8_t pair_context,
                                uint8_t generation);

#if GCM_NEEDS_GATT_SERVICE
// Generic Attribute (0x1801) phase: CSF / Service Changed / Database Hash.
static bool gatt_service_phase_needed(const connection_entry_t *entry);
static sl_status_t start_gatt_service_discovery(connection_entry_t *entry);
static void gatt_service_unavailable(connection_entry_t *entry);
static void continue_gatt_service_phase(connection_entry_t *entry);
#endif

bool sli_gcm_discovery_busy(const connection_entry_t *entry)
{
  return entry != NULL && entry->state != discovery_state_idle;
}

/// True when a GATT procedure has been handed to the stack and its
/// procedure_completed event has not arrived yet. Also true while already
/// draining one, so repeated aborts keep waiting for the same completion.
bool sli_gcm_procedure_outstanding(const connection_entry_t *entry)
{
  switch (entry->state) {
    case discovery_state_gatt_service:
    case discovery_state_csf_characteristic:
    case discovery_state_csf_write:
    case discovery_state_svc_changed_characteristic:
    case discovery_state_svc_changed_cccd:
    case discovery_state_hash_read:
    case discovery_state_service:
    case discovery_state_characteristic:
    case discovery_state_cccd:
    case discovery_state_aborting:
      return true;
    default:
      return false;
  }
}

/// True while a registered profile is mid-discovery (including a cache restore).
/// The Generic Attribute phase does not count: no profile has started yet.
bool sli_gcm_profile_discovery_in_progress(const connection_entry_t *entry)
{
  switch (entry->state) {
    case discovery_state_restoring:
    case discovery_state_service:
    case discovery_state_characteristic:
    case discovery_state_cccd:
      return true;
    default:
      return false;
  }
}

/// Abandon the in-flight run. Bumping run_generation invalidates any
/// continuation already scheduled for the aborted run: when that
/// continuation eventually executes, its run_generation check fails and it
/// leaves the connection alone instead of acting on stale state.
///
/// A procedure already accepted by the stack cannot be cancelled, so the
/// connection enters the aborting state and waits for its procedure_completed
/// before anything new is issued. Pass @p drain as false when the caller is
/// itself handling that completion, in which case nothing is left to wait for.
void sli_gcm_abort_run(connection_entry_t *entry, bool drain)
{
  entry->run_all = false;
  entry->pending_run_valid = false;
  entry->run_generation++;

  if (drain && sli_gcm_procedure_outstanding(entry)) {
    gcm_log_debug("draining aborted run conn=%u state=%u" APP_LOG_NL,
                  entry->connection,
                  entry->state);
    entry->state = discovery_state_aborting;
  } else {
    entry->state = discovery_state_idle;
  }
}

#if GCM_NEEDS_GATT_SERVICE
/// True if CSF, Service Changed, or Database Hash still need work this connection.
static bool gatt_service_phase_needed(const connection_entry_t *entry)
{
  bool needed = false;
#if SL_GATT_CLIENT_MANAGER_CACHE_ENABLE
  needed = needed || (entry->hash_state == db_hash_unknown);
#endif
#if SL_GATT_CLIENT_MANAGER_ENABLE_ROBUST_CACHING
  needed = needed || !entry->csf_written;
#endif
#if SL_GATT_CLIENT_MANAGER_ENABLE_SERVICE_CHANGED
  needed = needed || !entry->svc_changed_subscribed;
#endif
  return needed;
}

/// Begin discovering Generic Attribute (0x1801); resets per-connection GATT handles.
static sl_status_t start_gatt_service_discovery(connection_entry_t *entry)
{
  uint8_t uuid_bytes[2] = {
    (uint8_t)(GATT_SERVICE_UUID_16 & 0xFF),
    (uint8_t)((GATT_SERVICE_UUID_16 >> 8) & 0xFF)
  };

  entry->gatt_service_found = false;
  entry->csf_char_found = false;
  entry->csf_handle_from_cache = false;
  entry->gatt_service_handle = SL_GATT_CLIENT_MANAGER_INVALID_SERVICE_HANDLE;
  entry->csf_char_handle = SL_GATT_CLIENT_MANAGER_INVALID_CHARACTERISTIC_HANDLE;
  entry->state = discovery_state_gatt_service;

  gcm_log_debug("discovering generic attribute service conn=%u" APP_LOG_NL,
                entry->connection);
  return sl_bt_gatt_discover_primary_services_by_uuid(entry->connection,
                                                      sizeof(uuid_bytes),
                                                      uuid_bytes);
}

/// Peer has no Generic Attribute service (or it is unusable): skip the phase.
static void gatt_service_unavailable(connection_entry_t *entry)
{
  entry->hash_state = db_hash_absent;
  entry->csf_written = true;
  entry->svc_changed_subscribed = true;
  entry->state = discovery_state_idle;
  gcm_log_info("no generic attribute service conn=%u; cache, robust caching"
               " and service changed disabled for this connection" APP_LOG_NL,
               entry->connection);
}

/// Next Generic Attribute step: CSF write/discover, Service Changed, then hash.
static void continue_gatt_service_phase(connection_entry_t *entry)
{
  sl_status_t sc;

#if SL_GATT_CLIENT_MANAGER_ENABLE_ROBUST_CACHING
  if (!entry->csf_written) {
    static const uint8_t features = ROBUST_CACHING_BIT;

#if SL_GATT_CLIENT_MANAGER_CACHE_ENABLE
    // Reuse a Client Supported Features handle learned on a prior connection
    // to this peer. Unchanged database layout keeps the handle valid; a failed
    // write falls back to discovery below.
    {
      uint16_t cached_csf;
      if (sli_gcm_cache_find_csf_for_peer(entry, &cached_csf)) {
        entry->csf_char_handle = cached_csf;
        entry->csf_char_found = true;
        entry->csf_handle_from_cache = true;
        entry->state = discovery_state_csf_write;
        sc = sl_bt_gatt_write_characteristic_value(entry->connection,
                                                   entry->csf_char_handle,
                                                   sizeof(features),
                                                   &features);
        if (sc == SL_STATUS_OK) {
          gcm_log_debug("client supported features from cache conn=%u handle=0x%04X"
                        APP_LOG_NL,
                        entry->connection,
                        entry->csf_char_handle);
          return;
        }
        gcm_log_debug("cached client supported features write failed to start"
                      " conn=%u status=0x%04lx; discovering" APP_LOG_NL,
                      entry->connection,
                      (unsigned long)sc);
        entry->csf_char_found = false;
        entry->csf_handle_from_cache = false;
        entry->csf_char_handle =
          SL_GATT_CLIENT_MANAGER_INVALID_CHARACTERISTIC_HANDLE;
      }
    }
#endif

    {
      uint8_t uuid_bytes[2] = {
        (uint8_t)(CLIENT_SUPPORTED_FEATURES_UUID_16 & 0xFF),
        (uint8_t)((CLIENT_SUPPORTED_FEATURES_UUID_16 >> 8) & 0xFF)
      };
      entry->csf_handle_from_cache = false;
      entry->state = discovery_state_csf_characteristic;
      sc = sl_bt_gatt_discover_characteristics_by_uuid(entry->connection,
                                                       entry->gatt_service_handle,
                                                       sizeof(uuid_bytes),
                                                       uuid_bytes);
      if (sc == SL_STATUS_OK) {
        return;
      }
      gcm_log_debug("client supported features discovery failed to start conn=%u"
                    " status=0x%04lx" APP_LOG_NL,
                    entry->connection,
                    (unsigned long)sc);
      entry->csf_written = true;
    }
  }
#endif

#if SL_GATT_CLIENT_MANAGER_ENABLE_SERVICE_CHANGED
  if (!entry->svc_changed_subscribed) {
    uint8_t uuid_bytes[2] = {
      (uint8_t)(SERVICE_CHANGED_UUID_16 & 0xFF),
      (uint8_t)((SERVICE_CHANGED_UUID_16 >> 8) & 0xFF)
    };
    entry->svc_changed_char_found = false;
    entry->svc_changed_char_handle = SL_GATT_CLIENT_MANAGER_INVALID_CHARACTERISTIC_HANDLE;
    entry->state = discovery_state_svc_changed_characteristic;
    sc = sl_bt_gatt_discover_characteristics_by_uuid(entry->connection,
                                                     entry->gatt_service_handle,
                                                     sizeof(uuid_bytes),
                                                     uuid_bytes);
    if (sc == SL_STATUS_OK) {
      return;
    }
    gcm_log_debug("service changed discovery failed to start conn=%u"
                  " status=0x%04lx" APP_LOG_NL,
                  entry->connection,
                  (unsigned long)sc);
    entry->svc_changed_subscribed = true;
  }
#endif

#if SL_GATT_CLIENT_MANAGER_CACHE_ENABLE
  if (entry->hash_state == db_hash_unknown) {
    uint8_t uuid_bytes[2] = {
      (uint8_t)(DATABASE_HASH_UUID_16 & 0xFF),
      (uint8_t)((DATABASE_HASH_UUID_16 >> 8) & 0xFF)
    };
    entry->state = discovery_state_hash_read;
    sc = sl_bt_gatt_read_characteristic_value_by_uuid(entry->connection,
                                                      entry->gatt_service_handle,
                                                      sizeof(uuid_bytes),
                                                      uuid_bytes);
    if (sc != SL_STATUS_OK) {
      gatt_service_unavailable(entry);
      sli_gcm_run_profiles(entry);
    }
    return;
  }
#endif

  entry->state = discovery_state_idle;
  sli_gcm_run_profiles(entry);
}
#endif // GCM_NEEDS_GATT_SERVICE

// -----------------------------------------------------------------------------
// Private helpers - discovery / CCCD state machine

uint8_t sli_gcm_subscription_flag_to_gatt(uint8_t flags)
{
  if ((flags & SL_GATT_CLIENT_MANAGER_FLAG_INDICATE) != 0) {
    return sl_bt_gatt_indication;
  }
  if ((flags & SL_GATT_CLIENT_MANAGER_FLAG_NOTIFY) != 0) {
    return sl_bt_gatt_notification;
  }
  return sl_bt_gatt_disable;
}

/// Discover the characteristic UUID for entry->current_pair under its service.
static sl_status_t discover_characteristic_for_current(connection_entry_t *entry)
{
  uint8_t uuid_bytes[16];
  size_t uuid_len = 0;
  uint16_t profile_handle = entry->current_profile;
  uint8_t pair = entry->current_pair;
  const profile_entry_t *profile_entry = sli_gcm_get_profile_mutable(profile_handle);
  const sl_gatt_client_manager_interest_t *interest = &profile_entry->pairs[pair];

  sli_gcm_uuid_to_bytes(&interest->characteristic, uuid_bytes, &uuid_len);
  entry->state = discovery_state_characteristic;
  gcm_log_debug("discover characteristic conn=%u profile=%u pair=%u" APP_LOG_NL,
                entry->connection,
                profile_handle,
                pair);
  gcm_log_uuid("char", &interest->characteristic);
  return sl_bt_gatt_discover_characteristics_by_uuid(
    entry->connection,
    entry->results[profile_handle][pair].service_handle,
    (uint8_t)uuid_len,
    uuid_bytes);
}

/// After a pair finishes: next pair, or start CCCD writes when all pairs are done.
static sl_status_t advance_pair_or_cccd(connection_entry_t *entry)
{
  entry->current_pair++;
  if (entry->current_pair >= sli_gcm_get_profile_mutable(entry->current_profile)->pair_count) {
    entry->current_pair = 0;
    return configure_next_cccd(entry);
  }
  return discover_service_for_current(entry);
}

/// Discover the service for the current pair (or reuse the previous pair's
/// handle when the service UUID matches), then its characteristic.
static sl_status_t discover_service_for_current(connection_entry_t *entry)
{
  uint8_t uuid_bytes[16];
  size_t uuid_len = 0;
  uint16_t profile_handle = entry->current_profile;
  uint8_t pair = entry->current_pair;
  const profile_entry_t *profile = sli_gcm_get_profile_mutable(profile_handle);
  const sl_gatt_client_manager_uuid_t *wanted;
  discovery_result_t *result;

  if (pair >= profile->pair_count) {
    entry->current_pair = 0;
    return configure_next_cccd(entry);
  }

  wanted = &profile->pairs[pair].service;

  // Same-service pairs are expected to be listed consecutively. Only the
  // previous pair is checked so reuse stays O(1).
  if (pair > 0) {
    const uint8_t prev = (uint8_t)(pair - 1);
    const discovery_result_t *found = &entry->results[profile_handle][prev];
    uint8_t generation;

    if (found->service_found
        && sli_gcm_uuid_equal(&profile->pairs[prev].service, wanted)) {
      result = &entry->results[profile_handle][pair];
      result->service_handle = found->service_handle;
      result->service_found = true;
      result->service_match_count = found->service_match_count;
      gcm_log_debug("reuse service conn=%u profile=%u pair=%u handle=0x%08lx"
                    APP_LOG_NL,
                    entry->connection,
                    profile_handle,
                    pair,
                    (unsigned long)result->service_handle);
      generation = entry->run_generation;
      sli_gcm_emit_service_found(entry->connection,
                                 profile_handle,
                                 pair,
                                 result->service_handle);
      if (entry->run_generation != generation) {
        return SL_STATUS_OK;
      }
      return discover_characteristic_for_current(entry);
    }
  }

  sli_gcm_uuid_to_bytes(wanted, uuid_bytes, &uuid_len);
  entry->state = discovery_state_service;
  gcm_log_debug("discover service conn=%u profile=%u pair=%u" APP_LOG_NL,
                entry->connection,
                profile_handle,
                pair);
  gcm_log_uuid("svc", wanted);
  return sl_bt_gatt_discover_primary_services_by_uuid(entry->connection,
                                                      (uint8_t)uuid_len,
                                                      uuid_bytes);
}

/// Write CCCDs for the remaining pairs of the current profile. When none are
/// left, the profile is stored in the cache and completed.
static sl_status_t configure_next_cccd(connection_entry_t *entry)
{
  uint16_t profile_handle = entry->current_profile;
  const profile_entry_t *profile = sli_gcm_get_profile_mutable(profile_handle);

  while (entry->current_pair < profile->pair_count) {
    uint8_t pair = entry->current_pair;
    uint8_t flags = profile->pairs[pair].flags;
    discovery_result_t *result = &entry->results[profile_handle][pair];

    if (!result->characteristic_found
        || (flags & (SL_GATT_CLIENT_MANAGER_FLAG_NOTIFY
                     | SL_GATT_CLIENT_MANAGER_FLAG_INDICATE)) == 0) {
      entry->current_pair++;
      continue;
    }

    entry->state = discovery_state_cccd;
    gcm_log_debug("configure CCCD conn=%u profile=%u pair=%u char=0x%04X flags=0x%02X"
                  APP_LOG_NL,
                  entry->connection,
                  profile_handle,
                  pair,
                  result->characteristic_handle,
                  flags);
    return sl_bt_gatt_set_characteristic_notification(
      entry->connection,
      result->characteristic_handle,
      sli_gcm_subscription_flag_to_gatt(flags));
  }

#if SL_GATT_CLIENT_MANAGER_CACHE_ENABLE
  if (!entry->from_cache) {
    sli_gcm_cache_store(entry);
  }
#endif
  {
    // The application owns discovered handles from here on: the component
    // does not retain them past the discovery-complete event. Sample the
    // generation first, because the application is allowed to start another
    // run on this connection synchronously from inside the callback; if it
    // did (and reused this same profile), that new run already owns the
    // results table and must not be wiped out here.
    uint8_t generation = entry->run_generation;
    sli_gcm_emit_discovery_complete(entry, SL_STATUS_OK);
    if (entry->run_generation == generation) {
      sli_gcm_clear_profile_results(entry, profile_handle);
    }
  }
  return SL_STATUS_OK;
}

/// Start the profile selected in entry->current_profile.
static sl_status_t start_current_profile(connection_entry_t *entry)
{
  uint16_t profile_handle = entry->current_profile;

  if (!sli_gcm_profile_handle_is_valid(profile_handle)) {
    gcm_log_debug("start profile failed: invalid handle %u conn=%u" APP_LOG_NL,
                  profile_handle,
                  entry->connection);
    return SL_STATUS_INVALID_HANDLE;
  }

  sli_gcm_clear_profile_results(entry, profile_handle);
  entry->current_pair = 0;
  entry->from_cache = false;
  // Claim the connection before anything can emit an event.
  entry->state = discovery_state_restoring;

  gcm_log_info("begin profile %u conn=%u pairs=%u force_refresh=%u" APP_LOG_NL,
               profile_handle,
               entry->connection,
               sli_gcm_get_profile_mutable(profile_handle)->pair_count,
               entry->force_refresh);

#if SL_GATT_CLIENT_MANAGER_CACHE_ENABLE
  {
    // Guard against force_discovery() being called re-entrantly from a
    // callback triggered by cache_try_restore() below.
    uint8_t generation = entry->run_generation;
    bool restored = !entry->force_refresh && sli_gcm_cache_try_restore(entry);
    if (entry->run_generation != generation) {
      return SL_STATUS_OK;
    }
    if (restored) {
      entry->current_pair = 0;
      return configure_next_cccd(entry);
    }
  }
#endif

  return discover_service_for_current(entry);
}

/// Report a run that ended before the current profile could be finished. The
/// profile still gets its discovery-complete event, carrying @p sc, so an
/// application that only waits for completions is never left hanging.
///
/// @param[in] generation entry->run_generation sampled before the failing step.
static void fail_current_profile(connection_entry_t *entry,
                                 sl_status_t sc,
                                 uint8_t pair_context,
                                 uint8_t generation)
{
  uint16_t profile_handle = entry->current_profile;
  bool completable = sli_gcm_profile_handle_is_valid(profile_handle);

  // Release the connection first: the application may start a new run from
  // either callback, and it must not look like a procedure is still pending.
  entry->state = discovery_state_idle;
  entry->run_all = false;

  sli_gcm_emit_error(entry->connection, profile_handle, pair_context, sc);
  if (entry->run_generation != generation || !completable) {
    return;
  }

  sli_gcm_emit_discovery_complete(entry, sc);
  if (entry->run_generation == generation) {
    sli_gcm_clear_profile_results(entry, profile_handle);
  }
}

/// Drive profiles until one of them needs to wait for stack events.
void sli_gcm_run_profiles(connection_entry_t *entry)
{
  for (;; ) {
    uint8_t generation = entry->run_generation;
    sl_status_t sc = start_current_profile(entry);

    if (sc != SL_STATUS_OK) {
      fail_current_profile(entry,
                           sc,
                           SL_GATT_CLIENT_MANAGER_INVALID_PAIR_INDEX,
                           generation);
      return;
    }
    if (entry->run_generation != generation) {
      // The application started another run from the completion callback.
      return;
    }
    if (entry->state != discovery_state_idle) {
      // Waiting for a GATT procedure to complete.
      return;
    }
    // The profile finished synchronously (cache hit without CCCD writes).
    if (!entry->run_all || !sli_gcm_select_next_profile(entry)) {
      entry->run_all = false;
      return;
    }
  }
}

/// Common tail after a state machine step: propagate errors, or continue with
/// the next profile when the current one just finished.
///
/// @param[in] generation entry->run_generation sampled before the step ran.
static void continue_after_step(connection_entry_t *entry,
                                sl_status_t sc,
                                uint8_t pair_context,
                                uint8_t generation)
{
  if (sc != SL_STATUS_OK) {
    fail_current_profile(entry, sc, pair_context, generation);
    return;
  }
  if (entry->run_generation != generation) {
    // The step emitted a completion event and the application started a new
    // run from the callback; that run now owns the connection.
    return;
  }
  if (entry->state != discovery_state_idle) {
    return;
  }
  if (entry->run_all && sli_gcm_select_next_profile(entry)) {
    sli_gcm_run_profiles(entry);
  } else {
    entry->run_all = false;
  }
}

/// Entry point for every discovery run.
sl_status_t sli_gcm_begin_run(connection_entry_t *entry,
                              bool run_all,
                              uint16_t profile_handle,
                              bool force_refresh)
{
  uint16_t first;

  if (entry->state != discovery_state_idle) {
    gcm_log_warning("discovery already in progress conn=%u" APP_LOG_NL,
                    entry->connection);
    return SL_STATUS_IN_PROGRESS;
  }

  if (run_all) {
    if (sli_gcm_get_profile_count() == 0 || !sli_gcm_find_profile_from(0, &first)) {
      gcm_log_warning("discovery skipped: no profiles registered conn=%u" APP_LOG_NL,
                      entry->connection);
      return SL_STATUS_INVALID_STATE;
    }
  } else {
    if (!sli_gcm_profile_handle_is_valid(profile_handle)) {
      gcm_log_debug("discovery skipped: invalid profile %u conn=%u" APP_LOG_NL,
                    profile_handle,
                    entry->connection);
      return SL_STATUS_INVALID_HANDLE;
    }
    first = profile_handle;
  }

  entry->run_all = run_all;
  entry->force_refresh = force_refresh;
  entry->current_profile = first;
  entry->run_generation++;

#if GCM_NEEDS_GATT_SERVICE
#if SL_GATT_CLIENT_MANAGER_CACHE_ENABLE
  if (force_refresh) {
    // Re-read the hash so the refreshed results are cached under the current
    // database identity.
    entry->hash_state = db_hash_unknown;
  }
#else
  (void)force_refresh;
#endif
  if (gatt_service_phase_needed(entry)) {
    sl_status_t sc = start_gatt_service_discovery(entry);
    if (sc != SL_STATUS_OK) {
      gcm_log_warning("generic attribute discovery failed to start conn=%u status=0x%04lx"
                      APP_LOG_NL,
                      entry->connection,
                      (unsigned long)sc);
      gatt_service_unavailable(entry);
      sli_gcm_run_profiles(entry);
    }
    return SL_STATUS_OK;
  }
#else
  (void)force_refresh;
#endif

  sli_gcm_run_profiles(entry);
  return SL_STATUS_OK;
}

/// Start a run now, or remember it until the aborted run it replaces has
/// drained. Deferred runs are started from handle_procedure_completed().
sl_status_t sli_gcm_request_run(connection_entry_t *entry,
                                bool run_all,
                                uint16_t profile_handle,
                                bool force_refresh)
{
  if (entry->state != discovery_state_aborting) {
    return sli_gcm_begin_run(entry, run_all, profile_handle, force_refresh);
  }

  entry->pending_run_valid = true;
  entry->pending_run_all = run_all;
  entry->pending_profile = profile_handle;
  entry->pending_force_refresh = force_refresh;
  gcm_log_debug("run deferred until the aborted procedure drains conn=%u"
                APP_LOG_NL,
                entry->connection);
  return SL_STATUS_OK;
}

/// Common reaction to the peer reporting that its GATT database changed.
///
/// The caller must already have dealt with any run that was in flight.
void sli_gcm_trigger_rediscovery(connection_entry_t *entry, sl_status_t status)
{
#if SL_GATT_CLIENT_MANAGER_CACHE_ENABLE
  // The stored hash describes the database the peer just invalidated. Forget
  // it so a later non-forced discovery re-reads it instead of restoring
  // handles that are keyed by an identity the peer no longer has.
  entry->hash_state = db_hash_unknown;
#endif

  sli_gcm_emit_rediscovery_needed(entry->connection,
                                  status,
                                  SL_GATT_CLIENT_MANAGER_AUTO_REDISCOVERY);
#if SL_GATT_CLIENT_MANAGER_AUTO_REDISCOVERY
  (void)sli_gcm_request_run(entry,
                            true,
                            SL_GATT_CLIENT_MANAGER_INVALID_PROFILE_HANDLE,
                            true);
#endif
}

void sli_gcm_handle_procedure_completed(uint8_t connection, uint16_t result)
{
  connection_entry_t *entry = sli_gcm_find_connection_entry(connection);
#if GCM_NEEDS_GATT_SERVICE
  sl_status_t sc;
#endif
  uint8_t pair;
  uint8_t generation;

  if (entry == NULL) {
    return;
  }

  if (entry->state == discovery_state_aborting) {
    // The abandoned run's procedure has drained, whatever its result. The
    // connection is free again, so any run deferred meanwhile can start.
    gcm_log_debug("aborted run drained conn=%u result=0x%04x" APP_LOG_NL,
                  connection,
                  result);
    entry->state = discovery_state_idle;
    if (entry->pending_run_valid) {
      entry->pending_run_valid = false;
      (void)sli_gcm_begin_run(entry,
                              entry->pending_run_all,
                              entry->pending_profile,
                              entry->pending_force_refresh);
    }
    return;
  }

  if (entry->state == discovery_state_idle) {
    // Nothing is being discovered, so this completion belongs to an operation
    // the application issued. Only a changed peer database matters here: the
    // stack reports it through the completion, not through the call result.
    if (result == (uint16_t)SL_STATUS_BT_ATT_OUT_OF_SYNC) {
      sli_gcm_trigger_rediscovery(entry, (sl_status_t)result);
    }
    return;
  }

#if GCM_NEEDS_GATT_SERVICE
  // Generic Attribute phase: internal traffic on 0x1801 that runs before any
  // profile is discovered.
  if (entry->state == discovery_state_gatt_service
      || entry->state == discovery_state_csf_characteristic
      || entry->state == discovery_state_csf_write
      || entry->state == discovery_state_svc_changed_characteristic
      || entry->state == discovery_state_svc_changed_cccd
      || entry->state == discovery_state_hash_read) {
#if SL_GATT_CLIENT_MANAGER_ENABLE_ROBUST_CACHING
    // The Client Supported Features steps are best effort. A peer without
    // 0x2B29, or one that refuses the write, just loses the ability to report
    // that its database changed; that is no reason to fail the run.
    if (entry->state == discovery_state_csf_characteristic) {
      // The spec forbids clearing a bit the client has already set. Only this
      // component ever writes the characteristic, and it always writes the
      // same value, so a plain write cannot regress it.
      static const uint8_t features = ROBUST_CACHING_BIT;

      if (result != 0 || !entry->csf_char_found) {
        gcm_log_debug("no client supported features conn=%u result=0x%04x"
                      APP_LOG_NL,
                      connection,
                      result);
        entry->csf_written = true;
        continue_gatt_service_phase(entry);
        return;
      }
      entry->state = discovery_state_csf_write;
      sc = sl_bt_gatt_write_characteristic_value(connection,
                                                 entry->csf_char_handle,
                                                 sizeof(features),
                                                 &features);
      if (sc != SL_STATUS_OK) {
        gcm_log_debug("robust caching write failed to start conn=%u status=0x%04lx"
                      APP_LOG_NL,
                      connection,
                      (unsigned long)sc);
        entry->csf_written = true;
        continue_gatt_service_phase(entry);
      }
      return;
    }

    if (entry->state == discovery_state_csf_write) {
      if (result != 0) {
        gcm_log_debug("robust caching write rejected conn=%u result=0x%04x"
                      APP_LOG_NL,
                      connection,
                      result);
#if SL_GATT_CLIENT_MANAGER_CACHE_ENABLE
        // A stale cached handle can fail after a database change. Fall back to
        // discovering Client Supported Features once before giving up.
        if (entry->csf_handle_from_cache) {
          uint8_t uuid_bytes[2] = {
            (uint8_t)(CLIENT_SUPPORTED_FEATURES_UUID_16 & 0xFF),
            (uint8_t)((CLIENT_SUPPORTED_FEATURES_UUID_16 >> 8) & 0xFF)
          };
          entry->csf_handle_from_cache = false;
          entry->csf_char_found = false;
          entry->csf_char_handle =
            SL_GATT_CLIENT_MANAGER_INVALID_CHARACTERISTIC_HANDLE;
          entry->state = discovery_state_csf_characteristic;
          sc = sl_bt_gatt_discover_characteristics_by_uuid(connection,
                                                           entry->gatt_service_handle,
                                                           sizeof(uuid_bytes),
                                                           uuid_bytes);
          if (sc == SL_STATUS_OK) {
            return;
          }
          gcm_log_debug("client supported features discovery failed to start"
                        " conn=%u status=0x%04lx" APP_LOG_NL,
                        connection,
                        (unsigned long)sc);
        }
#endif
      } else {
        gcm_log_info("robust caching enabled conn=%u" APP_LOG_NL, connection);
      }
      entry->csf_handle_from_cache = false;
      entry->csf_written = true;
      continue_gatt_service_phase(entry);
      return;
    }
#endif // SL_GATT_CLIENT_MANAGER_ENABLE_ROBUST_CACHING

#if SL_GATT_CLIENT_MANAGER_ENABLE_SERVICE_CHANGED
    // Service Changed is best effort, like Client Supported Features.
    if (entry->state == discovery_state_svc_changed_characteristic) {
      if (result != 0 || !entry->svc_changed_char_found) {
        gcm_log_debug("no service changed characteristic conn=%u result=0x%04x"
                      APP_LOG_NL,
                      connection,
                      result);
        entry->svc_changed_subscribed = true;
        continue_gatt_service_phase(entry);
        return;
      }
      entry->state = discovery_state_svc_changed_cccd;
      sc = sl_bt_gatt_set_characteristic_notification(connection,
                                                      entry->svc_changed_char_handle,
                                                      sl_bt_gatt_indication);
      if (sc != SL_STATUS_OK) {
        gcm_log_debug("service changed subscribe failed to start conn=%u"
                      " status=0x%04lx" APP_LOG_NL,
                      connection,
                      (unsigned long)sc);
        entry->svc_changed_subscribed = true;
        continue_gatt_service_phase(entry);
      }
      return;
    }

    if (entry->state == discovery_state_svc_changed_cccd) {
      if (result != 0) {
        gcm_log_debug("service changed subscribe rejected conn=%u result=0x%04x"
                      APP_LOG_NL,
                      connection,
                      result);
      } else {
        gcm_log_info("service changed subscribed conn=%u" APP_LOG_NL, connection);
      }
      entry->svc_changed_subscribed = true;
      continue_gatt_service_phase(entry);
      return;
    }
#endif // SL_GATT_CLIENT_MANAGER_ENABLE_SERVICE_CHANGED

    // The remaining steps are about the Database Hash, where any failure
    // simply means "no cache for this peer".
    if (result != 0) {
      gcm_log_debug("generic attribute step failed conn=%u state=%u result=0x%04x"
                    APP_LOG_NL,
                    connection,
                    entry->state,
                    result);
      gatt_service_unavailable(entry);
      sli_gcm_run_profiles(entry);
      return;
    }

    if (entry->state == discovery_state_gatt_service) {
      if (!entry->gatt_service_found) {
        gatt_service_unavailable(entry);
        sli_gcm_run_profiles(entry);
        return;
      }
      continue_gatt_service_phase(entry);
      return;
    }

#if SL_GATT_CLIENT_MANAGER_CACHE_ENABLE
    // discovery_state_hash_read completed
    if (entry->hash_state != db_hash_valid) {
      gatt_service_unavailable(entry);
    } else {
      entry->state = discovery_state_idle;
      gcm_log_info("database hash read conn=%u" APP_LOG_NL, connection);
      gcm_log_hash("hash", entry->db_hash);
    }
    sli_gcm_run_profiles(entry);
#endif // SL_GATT_CLIENT_MANAGER_CACHE_ENABLE
    return;
  }
#endif // GCM_NEEDS_GATT_SERVICE

  pair = entry->current_pair;
  generation = entry->run_generation;

  if (result == (uint16_t)SL_STATUS_BT_ATT_OUT_OF_SYNC) {
    // The peer's GATT database changed since it was last discovered. A profile
    // that already started still owes the application DISCOVERY_COMPLETE; the
    // Generic Attribute phase has no profile yet, so only REDISCOVERY_NEEDED
    // is emitted there. The procedure that reported this is the one completing
    // here, so there is nothing to drain.
    if (sli_gcm_profile_discovery_in_progress(entry)) {
      fail_current_profile(entry, (sl_status_t)result, pair, generation);
    } else {
      sli_gcm_abort_run(entry, false);
    }
    sli_gcm_trigger_rediscovery(entry, (sl_status_t)result);
    return;
  }

  if (result != 0) {
    gcm_log_error("GATT procedure failed conn=%u profile=%u pair=%u state=%u result=0x%04x"
                  APP_LOG_NL,
                  connection,
                  entry->current_profile,
                  pair,
                  entry->state,
                  result);
    fail_current_profile(entry, (sl_status_t)result, pair, generation);
    return;
  }

  if (entry->state == discovery_state_service) {
    if (!entry->results[entry->current_profile][pair].service_found) {
      // A missing service does not abort the profile: the remaining pairs are
      // still probed so the application learns everything that is missing.
      gcm_log_debug("service not found conn=%u profile=%u pair=%u" APP_LOG_NL,
                    connection,
                    entry->current_profile,
                    pair);
      continue_after_step(entry, advance_pair_or_cccd(entry), pair, generation);
      return;
    }

    gcm_log_debug("service found conn=%u profile=%u pair=%u handle=0x%08lx" APP_LOG_NL,
                  connection,
                  entry->current_profile,
                  pair,
                  (unsigned long)entry->results[entry->current_profile][pair].service_handle);
    continue_after_step(entry,
                        discover_characteristic_for_current(entry),
                        pair,
                        generation);
    return;
  }

  if (entry->state == discovery_state_characteristic) {
    gcm_log_debug("characteristic %s conn=%u profile=%u pair=%u" APP_LOG_NL,
                  entry->results[entry->current_profile][pair].characteristic_found
                  ? "found" : "not found",
                  connection,
                  entry->current_profile,
                  pair);
    continue_after_step(entry, advance_pair_or_cccd(entry), pair, generation);
    return;
  }

  if (entry->state == discovery_state_cccd) {
    gcm_log_debug("CCCD configured conn=%u profile=%u pair=%u" APP_LOG_NL,
                  connection,
                  entry->current_profile,
                  pair);
    entry->current_pair++;
    continue_after_step(entry,
                        configure_next_cccd(entry),
                        entry->current_pair,
                        generation);
  }
}

void sli_gcm_handle_characteristic_value(sl_bt_evt_gatt_characteristic_value_t *evt)
{
  connection_entry_t *entry = sli_gcm_find_connection_entry(evt->connection);
  sl_gatt_client_manager_event_t event;

  if (entry == NULL) {
    return;
  }

  if (entry->state == discovery_state_aborting
      && (evt->att_opcode == sl_bt_gatt_read_response
          || evt->att_opcode == sl_bt_gatt_read_by_type_response)) {
    // Late response belonging to the abandoned run (typically its Database
    // Hash read). Unsolicited notifications and indications still get through.
    gcm_log_debug("dropping read response from aborted run conn=%u char=0x%04X"
                  APP_LOG_NL,
                  evt->connection,
                  evt->characteristic);
    return;
  }

#if SL_GATT_CLIENT_MANAGER_CACHE_ENABLE
  // Database Hash read response is consumed internally.
  if (entry->state == discovery_state_hash_read
      && (evt->att_opcode == sl_bt_gatt_read_response
          || evt->att_opcode == sl_bt_gatt_read_by_type_response)) {
    if (evt->value.len == SL_GATT_CLIENT_MANAGER_DB_HASH_LEN) {
      memcpy(entry->db_hash, evt->value.data, SL_GATT_CLIENT_MANAGER_DB_HASH_LEN);
      entry->hash_state = db_hash_valid;
    } else {
      gcm_log_warning("unexpected database hash length %u conn=%u" APP_LOG_NL,
                      evt->value.len,
                      evt->connection);
    }
    return;
  }
#endif

#if SL_GATT_CLIENT_MANAGER_ENABLE_SERVICE_CHANGED
  if (entry->svc_changed_char_found
      && evt->characteristic == entry->svc_changed_char_handle
      && (evt->att_opcode == sl_bt_gatt_handle_value_indication
          || evt->att_opcode == sl_bt_gatt_handle_value_notification)) {
    sl_status_t sc = SL_STATUS_ABORT;

    gcm_log_info("service changed indication conn=%u" APP_LOG_NL,
                 evt->connection);
    (void)sl_bt_gatt_send_characteristic_confirmation(evt->connection);
    if (sli_gcm_discovery_busy(entry)) {
      bool was_profile = sli_gcm_profile_discovery_in_progress(entry);
      uint16_t profile_handle = entry->current_profile;
      uint8_t pair = entry->current_pair;
      discovery_state_t drain_state;

      sli_gcm_abort_run(entry, true);
      if (was_profile && sli_gcm_profile_handle_is_valid(profile_handle)) {
        drain_state = entry->state;
        sli_gcm_emit_error(entry->connection, profile_handle, pair, sc);
        sli_gcm_emit_discovery_complete(entry, sc);
        entry->state = drain_state;
        sli_gcm_clear_profile_results(entry, profile_handle);
      }
    }
    sli_gcm_trigger_rediscovery(entry, sc);
    return;
  }
#endif // SL_GATT_CLIENT_MANAGER_ENABLE_SERVICE_CHANGED

  // Notifications and indications are always forwarded to the application,
  // which attributes them using handles captured from discovery events.
  // Read responses that are not the internal Database Hash read are also
  // forwarded (e.g. results of sl_gatt_client_manager_read_characteristic_value).
  if (evt->att_opcode == sl_bt_gatt_handle_value_indication) {
#if SL_GATT_CLIENT_MANAGER_AUTO_CONFIRM_INDICATIONS
    (void)sl_bt_gatt_send_characteristic_confirmation(evt->connection);
#endif
  }

  memset(&event, 0, sizeof(event));
  event.header.id = SL_GATT_CLIENT_MANAGER_EVENT_CHARACTERISTIC_VALUE;
  event.header.connection = evt->connection;
  event.data.characteristic_value.characteristic = evt->characteristic;
  event.data.characteristic_value.att_opcode = evt->att_opcode;
  event.data.characteristic_value.value_len = evt->value.len;
  event.data.characteristic_value.value = evt->value.data;
  sli_gatt_client_manager_dispatch_event(&event);
}

// -----------------------------------------------------------------------------

void sli_gcm_on_gatt_service(connection_entry_t *entry, uint32_t service_handle)
{
  uint16_t profile_handle;
  uint8_t pair;

#if GCM_NEEDS_GATT_SERVICE
  if (entry->state == discovery_state_gatt_service) {
    if (!entry->gatt_service_found) {
      entry->gatt_service_handle = service_handle;
      entry->gatt_service_found = true;
    }
    return;
  }
#endif
  if (entry->state != discovery_state_service) {
    return;
  }
  profile_handle = entry->current_profile;
  pair = entry->current_pair;
  if (!entry->results[profile_handle][pair].service_found) {
    entry->results[profile_handle][pair].service_handle = service_handle;
    entry->results[profile_handle][pair].service_found = true;
    sli_gcm_emit_service_found(entry->connection,
                               profile_handle,
                               pair,
                               entry->results[profile_handle][pair].service_handle);
  }
  if (entry->results[profile_handle][pair].service_match_count < UINT8_MAX) {
    entry->results[profile_handle][pair].service_match_count++;
  }
}

void sli_gcm_on_gatt_characteristic(connection_entry_t *entry,
                                    uint16_t characteristic_handle)
{
  uint16_t profile_handle;
  uint8_t pair;

#if SL_GATT_CLIENT_MANAGER_ENABLE_ROBUST_CACHING
  if (entry->state == discovery_state_csf_characteristic) {
    if (!entry->csf_char_found) {
      entry->csf_char_handle = characteristic_handle;
      entry->csf_char_found = true;
    }
    return;
  }
#endif
#if SL_GATT_CLIENT_MANAGER_ENABLE_SERVICE_CHANGED
  if (entry->state == discovery_state_svc_changed_characteristic) {
    if (!entry->svc_changed_char_found) {
      entry->svc_changed_char_handle = characteristic_handle;
      entry->svc_changed_char_found = true;
    }
    return;
  }
#endif
  if (entry->state != discovery_state_characteristic) {
    return;
  }
  profile_handle = entry->current_profile;
  pair = entry->current_pair;
  if (!entry->results[profile_handle][pair].characteristic_found) {
    entry->results[profile_handle][pair].characteristic_handle = characteristic_handle;
    entry->results[profile_handle][pair].characteristic_found = true;
    sli_gcm_emit_characteristic_found(entry->connection,
                                      profile_handle,
                                      pair,
                                      entry->results[profile_handle][pair].service_handle,
                                      entry->results[profile_handle][pair].characteristic_handle);
  }
}

sl_status_t sli_gcm_on_connection_opened(connection_entry_t *entry)
{
  if (sli_gcm_discovery_busy(entry)) {
    gcm_log_debug("connection opened: discovery already in progress conn=%u"
                  " state=%u" APP_LOG_NL,
                  entry->connection,
                  entry->state);
    return SL_STATUS_OK;
  }
  sli_gcm_clear_all_results(entry);
  entry->state = discovery_state_idle;
  entry->hash_state = db_hash_unknown;
  entry->run_all = false;
#if SL_GATT_CLIENT_MANAGER_AUTO_START_DISCOVERY
  return sli_gcm_begin_run(entry,
                           true,
                           SL_GATT_CLIENT_MANAGER_INVALID_PROFILE_HANDLE,
                           false);
#else
  gcm_log_debug("auto-start discovery disabled; waiting for the application"
                APP_LOG_NL);
  return SL_STATUS_OK;
#endif
}
