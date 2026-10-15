/***************************************************************************//**
 * @file
 * @brief GATT Client Manager — internal types
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

#ifndef SLI_GATT_CLIENT_MANAGER_TYPES_H
#define SLI_GATT_CLIENT_MANAGER_TYPES_H

#include <stdint.h>
#include <stdbool.h>

#include "sl_bt_api.h"
#include "sl_gatt_client_manager_config.h"
#include "sl_gatt_client_manager_types.h"

#ifdef __cplusplus
extern "C" {
#endif

// Abbreviations used below:
//   CSF  = Client Supported Features (0x2B29), used to enable robust caching
//   CCCD = Client Characteristic Configuration Descriptor
//   GCM  = GATT Client Manager

/// Per-connection discovery / Generic Attribute phase state machine.
typedef enum {
  discovery_state_idle = 0,              ///< No procedure in flight
  discovery_state_restoring,             ///< Replaying handles from the RAM cache
  discovery_state_gatt_service,          ///< Discovering Generic Attribute (0x1801)
  discovery_state_csf_characteristic,    ///< Discovering Client Supported Features
  discovery_state_csf_write,             ///< Writing the Robust Caching bit to CSF
  discovery_state_svc_changed_characteristic, ///< Discovering Service Changed (0x2A05)
  discovery_state_svc_changed_cccd,      ///< Enabling Service Changed indications
  discovery_state_hash_read,             ///< Reading Database Hash (0x2B2A)
  discovery_state_service,               ///< Discovering a profile pair's service
  discovery_state_characteristic,        ///< Discovering a profile pair's characteristic
  discovery_state_cccd,                  ///< Writing a profile pair's CCCD
  discovery_state_aborting,              ///< Waiting for the stack to finish a cancelled procedure
} discovery_state_t;

/// Whether this connection has a usable remote Database Hash.
typedef enum {
  db_hash_unknown = 0, ///< Not read yet on this connection
  db_hash_valid,       ///< Hash bytes in connection_entry_t::db_hash are current
  db_hash_absent,      ///< Peer has no readable Database Hash (caching disabled for it)
} db_hash_state_t;

/// One discovered service/characteristic pair for a profile on a connection.
typedef struct {
  uint32_t service_handle;         ///< Stack opaque service handle (start/end packed)
  uint16_t characteristic_handle;  ///< ATT characteristic value handle
  uint8_t service_match_count;     ///< How many service instances matched this UUID
  bool service_found : 1;
  bool characteristic_found : 1;
} discovery_result_t;

/// One registered profile (copied interest pairs + bookkeeping).
typedef struct {
  sl_gatt_client_manager_interest_t pairs[SL_GATT_CLIENT_MANAGER_MAX_PAIRS_PER_PROFILE];
  uint8_t pair_count;
  bool in_use : 1;
} profile_entry_t;

/// Per-connection runtime state: discovery progress, Generic Attribute handles,
/// and results for every registered profile.
typedef struct {
  /// results[profile][pair] — live discovery outcome for this connection.
  discovery_result_t results[SL_GATT_CLIENT_MANAGER_MAX_PROFILES]
  [SL_GATT_CLIENT_MANAGER_MAX_PAIRS_PER_PROFILE];
  bd_addr peer_address;
  /// Generic Attribute service (0x1801). Owns Service Changed, CSF, Database Hash.
  uint32_t gatt_service_handle;
  uint16_t csf_char_handle;            ///< Client Supported Features characteristic
  uint16_t svc_changed_char_handle;    ///< Service Changed characteristic
  uint16_t current_profile;            ///< Profile handle being discovered
  uint16_t pending_profile;            ///< Deferred run's profile (see pending_run_*)
  uint8_t connection;                  ///< Stack connection handle
  uint8_t peer_address_type;
  uint8_t current_pair;                ///< Pair index within current_profile
  /// Bumped on abort so late completions from the old run are ignored.
  uint8_t run_generation;
  uint8_t db_hash[SL_GATT_CLIENT_MANAGER_DB_HASH_LEN];
  db_hash_state_t hash_state;
  discovery_state_t state;
  bool in_use : 1;
  bool peer_address_valid : 1;
  bool run_all : 1;                    ///< Discover every registered profile in order
  bool force_refresh : 1;              ///< Skip cache restore for this run
  bool from_cache : 1;                 ///< Current profile's handles came from cache
  bool pending_run_valid : 1;          ///< A run is queued until abort drain finishes
  bool pending_run_all : 1;
  bool pending_force_refresh : 1;
  bool gatt_service_found : 1;
  bool csf_char_found : 1;
  bool csf_handle_from_cache : 1;      ///< CSF write used a handle from a prior cache entry
  bool csf_written : 1;                ///< Robust-caching offer done (or skipped) this connection
  bool svc_changed_char_found : 1;
  bool svc_changed_subscribed : 1;     ///< Service Changed CCCD write done (or skipped)
} connection_entry_t;

#if SL_GATT_CLIENT_MANAGER_CACHE_ENABLE
/// One RAM cache slot: keyed by profile pair content + Database Hash.
/// Peer address / CSF handle are stored so a reconnect can skip CSF rediscovery.
typedef struct {
  /// Profile identity (copied pairs). Lookup uses content equality, not handle.
  sl_gatt_client_manager_interest_t pairs[SL_GATT_CLIENT_MANAGER_MAX_PAIRS_PER_PROFILE];
  discovery_result_t results[SL_GATT_CLIENT_MANAGER_MAX_PAIRS_PER_PROFILE];
  bd_addr peer_address;                ///< Peer that produced this entry (for CSF reuse)
  uint16_t last_used;                  ///< LRU stamp from cache_clock
  uint16_t csf_char_handle;            ///< Client Supported Features handle when known
  uint8_t db_hash[SL_GATT_CLIENT_MANAGER_DB_HASH_LEN];
  uint8_t pair_count;
  uint8_t peer_address_type;
  bool peer_address_valid : 1;
  bool csf_char_valid : 1;
  bool valid : 1;
} cache_entry_t;
#endif

#ifdef __cplusplus
}
#endif

#endif // SLI_GATT_CLIENT_MANAGER_TYPES_H
