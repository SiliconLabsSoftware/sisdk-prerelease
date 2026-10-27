/***************************************************************************//**
 * @file
 * @brief GATT Client Manager - cache management
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

#if SL_GATT_CLIENT_MANAGER_CACHE_ENABLE
/// RAM cache of complete profile discoveries, keyed by (pairs, db_hash).
static cache_entry_t cache[SL_GATT_CLIENT_MANAGER_MAX_CACHE_ENTRIES];
/// Monotonic stamp copied into cache_entry_t::last_used for LRU eviction.
static uint16_t cache_clock;

static uint16_t clock_tick(void)
{
  if (cache_clock > (UINT16_MAX - 1)) {
    // Reset cache last used timestamps to 0 to avoid errors caused by overflow.
    for (uint8_t i = 0; i < SL_GATT_CLIENT_MANAGER_MAX_CACHE_ENTRIES; i++) {
      cache[i].last_used = 0;
    }
    cache_clock = 0;
  }
  return ++cache_clock;
}

/// Find an entry matching profile pair content and Database Hash.
cache_entry_t *sli_gcm_cache_find(const profile_entry_t *profile, const uint8_t *hash)
{
  for (uint8_t i = 0; i < SL_GATT_CLIENT_MANAGER_MAX_CACHE_ENTRIES; i++) {
    if (cache[i].valid
        && cache[i].pair_count == profile->pair_count
        && memcmp(cache[i].db_hash, hash, SL_GATT_CLIENT_MANAGER_DB_HASH_LEN) == 0
        && sli_gcm_pairs_equal(cache[i].pairs, profile->pairs, profile->pair_count)) {
      return &cache[i];
    }
  }
  return NULL;
}

/// Return a free slot, or the least-recently-used valid entry when full.
cache_entry_t *sli_gcm_cache_alloc(void)
{
  cache_entry_t *lru = &cache[0];

  for (uint8_t i = 0; i < SL_GATT_CLIENT_MANAGER_MAX_CACHE_ENTRIES; i++) {
    if (!cache[i].valid) {
      return &cache[i];
    }
    if (cache[i].last_used < lru->last_used) {
      lru = &cache[i];
    }
  }
  gcm_log_debug("cache full, evicting least recently used" APP_LOG_NL);
  return lru;
}

void sli_gcm_cache_clear_all(void)
{
  memset(cache, 0, sizeof(cache));
  cache_clock = 0;
}

void sli_gcm_cache_store(connection_entry_t *conn)
{
  const profile_entry_t *profile = sli_gcm_get_profile(conn->current_profile);
  cache_entry_t *entry;
  uint8_t found = 0;

  if (profile == NULL) {
    return;
  }

  if (conn->hash_state != db_hash_valid) {
    gcm_log_debug("cache store skipped: no database hash conn=%u profile=%u"
                  APP_LOG_NL,
                  conn->connection,
                  conn->current_profile);
    return;
  }

  for (uint8_t i = 0; i < profile->pair_count; i++) {
    const discovery_result_t *result = &conn->results[conn->current_profile][i];
    if (result->service_found && result->characteristic_found) {
      found++;
    }
  }
  if (found != profile->pair_count || profile->pair_count == 0) {
    gcm_log_debug("cache store skipped: incomplete profile=%u found=%u/%u" APP_LOG_NL,
                  conn->current_profile,
                  found,
                  profile->pair_count);
    return;
  }

  entry = sli_gcm_cache_find(profile, conn->db_hash);
  if (entry == NULL) {
    entry = sli_gcm_cache_alloc();
    memset(entry, 0, sizeof(*entry));
  }

  entry->valid = true;
  entry->last_used = clock_tick();
  memcpy(entry->db_hash, conn->db_hash, SL_GATT_CLIENT_MANAGER_DB_HASH_LEN);
  entry->pair_count = profile->pair_count;
  entry->peer_address_valid = conn->peer_address_valid;
  entry->peer_address_type = conn->peer_address_type;
  entry->peer_address = conn->peer_address;
  entry->csf_char_valid = conn->csf_char_found;
  entry->csf_char_handle = conn->csf_char_handle;
  for (uint8_t i = 0; i < profile->pair_count; i++) {
    entry->pairs[i] = profile->pairs[i];
    entry->results[i] = conn->results[conn->current_profile][i];
  }

  gcm_log_info("cache STORE conn=%u profile=%u pairs=%u csf=0x%04X" APP_LOG_NL,
               conn->connection,
               conn->current_profile,
               profile->pair_count,
               entry->csf_char_valid ? entry->csf_char_handle : 0);
  gcm_log_hash("hash", conn->db_hash);
}

bool sli_gcm_cache_try_restore(connection_entry_t *conn)
{
  const profile_entry_t *profile = sli_gcm_get_profile(conn->current_profile);
  cache_entry_t *entry;
  uint8_t found = 0;

  if (profile == NULL) {
    return false;
  }

  if (conn->hash_state != db_hash_valid) {
    gcm_log_info("cache MISS conn=%u profile=%u reason=no_database_hash" APP_LOG_NL,
                 conn->connection,
                 conn->current_profile);
    return false;
  }

  entry = sli_gcm_cache_find(profile, conn->db_hash);
  if (entry == NULL) {
    gcm_log_info("cache MISS conn=%u profile=%u reason=no_entry" APP_LOG_NL,
                 conn->connection,
                 conn->current_profile);
    gcm_log_hash("hash", conn->db_hash);
    return false;
  }

  sli_gcm_clear_profile_results(conn, conn->current_profile);
  for (uint8_t i = 0; i < profile->pair_count; i++) {
    conn->results[conn->current_profile][i] = entry->results[i];
    if (entry->results[i].service_found && entry->results[i].characteristic_found) {
      found++;
    }
  }
  if (entry->csf_char_valid) {
    conn->csf_char_handle = entry->csf_char_handle;
    conn->csf_char_found = true;
  }
  entry->last_used = clock_tick();
  conn->from_cache = true;

  gcm_log_info("cache HIT conn=%u profile=%u found=%u/%u" APP_LOG_NL,
               conn->connection,
               conn->current_profile,
               found,
               profile->pair_count);

  {
    // Guard against force_discovery() being called re-entrantly from the
    // FOUND handler below. The check is required because such a call aborts
    // this run and starts a new one on the same connection_entry_t; without
    // it, we would keep emitting for a run that no longer exists, or index
    // into results the new run has already started overwriting.
    uint8_t generation = conn->run_generation;
    uint16_t profile_handle = conn->current_profile;

    for (uint8_t i = 0; i < profile->pair_count; i++) {
      const discovery_result_t *result = &conn->results[profile_handle][i];
      if (result->service_found) {
        sli_gcm_emit_service_found(conn->connection,
                                   profile_handle,
                                   i,
                                   result->service_handle);
        if (conn->run_generation != generation) {
          return true;
        }
      }
      if (result->characteristic_found) {
        sli_gcm_emit_characteristic_found(conn->connection,
                                          profile_handle,
                                          i,
                                          result->service_handle,
                                          result->characteristic_handle);
        if (conn->run_generation != generation) {
          return true;
        }
      }
    }
  }
  return true;
}

sl_status_t sli_gcm_cache_clear_profile(uint16_t profile_handle)
{
  const profile_entry_t *profile = sli_gcm_get_profile(profile_handle);
  bool cleared = false;

  if (profile == NULL) {
    return SL_STATUS_INVALID_HANDLE;
  }

  for (uint8_t i = 0; i < SL_GATT_CLIENT_MANAGER_MAX_CACHE_ENTRIES; i++) {
    if (cache[i].valid
        && cache[i].pair_count == profile->pair_count
        && sli_gcm_pairs_equal(cache[i].pairs, profile->pairs, profile->pair_count)) {
      memset(&cache[i], 0, sizeof(cache[i]));
      cleared = true;
    }
  }
  return cleared ? SL_STATUS_OK : SL_STATUS_NOT_FOUND;
}

#if SL_GATT_CLIENT_MANAGER_ENABLE_ROBUST_CACHING
bool sli_gcm_cache_find_csf_for_peer(const connection_entry_t *conn,
                                     uint16_t *out_handle)
{
  if (conn == NULL || out_handle == NULL || !conn->peer_address_valid) {
    return false;
  }

  for (uint8_t i = 0; i < SL_GATT_CLIENT_MANAGER_MAX_CACHE_ENTRIES; i++) {
    if (!cache[i].valid || !cache[i].csf_char_valid || !cache[i].peer_address_valid) {
      continue;
    }
    if (cache[i].peer_address_type == conn->peer_address_type
        && memcmp(&cache[i].peer_address,
                  &conn->peer_address,
                  sizeof(bd_addr)) == 0) {
      *out_handle = cache[i].csf_char_handle;
      return true;
    }
  }
  return false;
}
#endif // SL_GATT_CLIENT_MANAGER_ENABLE_ROBUST_CACHING
#endif // SL_GATT_CLIENT_MANAGER_CACHE_ENABLE
