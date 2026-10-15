/***************************************************************************//**
 * @file
 * @brief GATT Client Manager public types
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

#ifndef SL_GATT_CLIENT_MANAGER_TYPES_H
#define SL_GATT_CLIENT_MANAGER_TYPES_H

#include <stdint.h>
#include <stdbool.h>
#include "sl_status.h"
#include "sl_bt_api.h"
#include "sl_gatt_client_manager_config.h"

/// Bit width of @ref sl_gatt_client_manager_discovery_complete_evt_t::missing_mask.
#define SL_GATT_CLIENT_MANAGER_MISSING_MASK_BITS            (32)

#if SL_GATT_CLIENT_MANAGER_MAX_PROFILES > 32
#error "SL_GATT_CLIENT_MANAGER_MAX_PROFILES must be less than or equal to 32"
#endif
#if SL_GATT_CLIENT_MANAGER_MAX_PAIRS_PER_PROFILE > SL_GATT_CLIENT_MANAGER_MISSING_MASK_BITS
#error "SL_GATT_CLIENT_MANAGER_MAX_PAIRS_PER_PROFILE must fit in missing_mask"
#endif
#if SL_GATT_CLIENT_MANAGER_MAX_CONNECTIONS > 32
#error "SL_GATT_CLIENT_MANAGER_MAX_CONNECTIONS must be less than or equal to 32"
#endif
#if SL_GATT_CLIENT_MANAGER_MAX_CACHE_ENTRIES > 16
#error "SL_GATT_CLIENT_MANAGER_MAX_CACHE_ENTRIES must be less than or equal to 16"
#endif

#ifdef __cplusplus
extern "C" {
#endif

/***************************************************************************//**
 * @addtogroup gatt_client_manager
 * @{
 ******************************************************************************/

/// Invalid profile handle
#define SL_GATT_CLIENT_MANAGER_INVALID_PROFILE_HANDLE       ((uint16_t)0xFFFF)

/// Invalid pair index within a profile
#define SL_GATT_CLIENT_MANAGER_INVALID_PAIR_INDEX           ((uint8_t)0xFF)

/// Invalid GATT service handle
#define SL_GATT_CLIENT_MANAGER_INVALID_SERVICE_HANDLE       ((uint32_t)0xFFFFFFFF)

/// Invalid GATT characteristic handle
#define SL_GATT_CLIENT_MANAGER_INVALID_CHARACTERISTIC_HANDLE ((uint16_t)0xFFFF)

/// Pair CCCD / subscription flags (bitmask)
#define SL_GATT_CLIENT_MANAGER_FLAG_NONE       (0x00) ///< No CCCD write
#define SL_GATT_CLIENT_MANAGER_FLAG_NOTIFY     (0x01) ///< Enable notifications
#define SL_GATT_CLIENT_MANAGER_FLAG_INDICATE   (0x02) ///< Enable indications

/// Length of the remote GATT Database Hash (Bluetooth SIG: 128-bit)
#define SL_GATT_CLIENT_MANAGER_DB_HASH_LEN                  (16)

/// UUID descriptor (16-bit only, or 16/128-bit when configured)
typedef struct {
#if SL_GATT_CLIENT_MANAGER_SUPPORT_128BIT_UUID
  bool is_128bit; ///< true if @c value.uuid128 is valid
  union {
    uint16_t uuid16;  ///< Host-endian 16-bit UUID
    uuid_128 uuid128; ///< 128-bit UUID
  } value;
#else
  uint16_t uuid16; ///< Host-endian 16-bit UUID
#endif
} sl_gatt_client_manager_uuid_t;

/// One service/characteristic identifier pair inside a profile
typedef struct {
  sl_gatt_client_manager_uuid_t service;        ///< Service UUID
  sl_gatt_client_manager_uuid_t characteristic; ///< Characteristic UUID
  uint8_t flags; ///< @ref SL_GATT_CLIENT_MANAGER_FLAG_NOTIFY / INDICATE / NONE
} sl_gatt_client_manager_interest_t;

/// A profile: the set of pairs that together form one discoverable feature.
typedef struct {
  const sl_gatt_client_manager_interest_t *pairs; ///< Pair array (copied)
  uint8_t pair_count;                             ///< Number of pairs (>= 1)
} sl_gatt_client_manager_profile_t;

#if SL_GATT_CLIENT_MANAGER_SUPPORT_128BIT_UUID
/// Helper: build a 16-bit UUID descriptor
#define SL_GATT_CLIENT_MANAGER_UUID16(uuid16_host) \
  ((sl_gatt_client_manager_uuid_t){                \
    .is_128bit = false,                            \
    .value = { .uuid16 = (uuid16_host) }           \
  })

/// Helper: build a 128-bit UUID descriptor
#define SL_GATT_CLIENT_MANAGER_UUID128(uuid128_val) \
  ((sl_gatt_client_manager_uuid_t){                 \
    .is_128bit = true,                              \
    .value = { .uuid128 = (uuid128_val) }           \
  })
#else
/// Helper: build a 16-bit UUID descriptor (128-bit support disabled)
#define SL_GATT_CLIENT_MANAGER_UUID16(uuid16_host) \
  ((sl_gatt_client_manager_uuid_t){ .uuid16 = (uuid16_host) })
#endif

/// Event identifiers. Append new values; do not reorder existing ones except
/// when intentionally breaking the ABI (CACHE_HIT was removed as redundant).
typedef enum {
  SL_GATT_CLIENT_MANAGER_EVENT_DISCOVERY_COMPLETE = 0, ///< One profile finished
  SL_GATT_CLIENT_MANAGER_EVENT_ERROR,                  ///< Discovery / CCCD / op error
  SL_GATT_CLIENT_MANAGER_EVENT_CHARACTERISTIC_VALUE,   ///< Notify / indicate / read payload
  SL_GATT_CLIENT_MANAGER_EVENT_SERVICE_FOUND,          ///< Service handle resolved
  SL_GATT_CLIENT_MANAGER_EVENT_CHARACTERISTIC_FOUND,   ///< Characteristic handle resolved
  SL_GATT_CLIENT_MANAGER_EVENT_REDISCOVERY_NEEDED,     ///< Peer GATT DB out of sync
} sl_gatt_client_manager_event_id_t;

/// Common event header (always present)
typedef struct {
  sl_gatt_client_manager_event_id_t id; ///< Which union member is valid
  uint8_t connection;                   ///< Connection handle
} sl_gatt_client_manager_event_header_t;

/// Payload for @ref SL_GATT_CLIENT_MANAGER_EVENT_DISCOVERY_COMPLETE
/// @note status is SL_STATUS_OK only when every pair was found,
///       SL_STATUS_NOT_FOUND when the peer was probed but something was
///       missing, and the failing status when a GATT error ended the run
///       early. In the last case the pair counts describe how far it got.
typedef struct {
  uint32_t missing_mask;      ///< Bit i set when pair i was not fully found (@ref SL_GATT_CLIENT_MANAGER_MISSING_MASK_BITS)
  uint16_t profile_handle;    ///< Profile this result belongs to
  uint8_t found_count;        ///< Pairs with both service and characteristic found
  uint8_t pair_count;         ///< Pairs in the profile
  uint8_t match_count;        ///< Highest number of service instances seen for any pair
  sl_status_t status;         ///< SL_STATUS_OK only if every pair was found
  bool from_cache;            ///< true if handles were restored from cache
} sl_gatt_client_manager_discovery_complete_evt_t;

/// Payload for @ref SL_GATT_CLIENT_MANAGER_EVENT_ERROR
/// @note When the error interrupted a profile being discovered, a
///       @ref SL_GATT_CLIENT_MANAGER_EVENT_DISCOVERY_COMPLETE carrying the
///       same status follows it.
typedef struct {
  uint16_t profile_handle; ///< Related profile, or INVALID_PROFILE_HANDLE
  uint8_t pair_index;      ///< Related pair, or INVALID_PAIR_INDEX
  sl_status_t status;      ///< Underlying error code
} sl_gatt_client_manager_error_evt_t;

/// Payload for @ref SL_GATT_CLIENT_MANAGER_EVENT_CHARACTERISTIC_VALUE
/// @note value pointer is only valid for the duration of the event callback.
/// @note Applications should use the characteristic handle captured from
///       @ref SL_GATT_CLIENT_MANAGER_EVENT_CHARACTERISTIC_FOUND to attribute
///       the value to a profile pair.
typedef struct {
  const uint8_t *value;    ///< Value bytes (valid during callback only)
  uint16_t characteristic; ///< Characteristic handle
  uint16_t value_len;      ///< Length of value
  uint8_t att_opcode;      ///< ATT opcode (notification / indication / read / ...)
} sl_gatt_client_manager_characteristic_value_evt_t;

/// Payload for @ref SL_GATT_CLIENT_MANAGER_EVENT_SERVICE_FOUND
typedef struct {
  uint32_t service_handle; ///< Discovered service handle
  uint16_t profile_handle; ///< Profile being discovered
  uint8_t pair_index;      ///< Pair within the profile
} sl_gatt_client_manager_service_found_evt_t;

/// Payload for @ref SL_GATT_CLIENT_MANAGER_EVENT_CHARACTERISTIC_FOUND
typedef struct {
  uint32_t service_handle;        ///< Parent service handle
  uint16_t profile_handle;        ///< Profile being discovered
  uint16_t characteristic_handle; ///< Discovered characteristic handle
  uint8_t pair_index;             ///< Pair within the profile
} sl_gatt_client_manager_characteristic_found_evt_t;

/// Payload for @ref SL_GATT_CLIENT_MANAGER_EVENT_REDISCOVERY_NEEDED
/// @note Emitted for Service Changed indications and for ATT Out Of Sync when
///       robust caching is enabled. See @ref gcm_robust_caching.
typedef struct {
  bool auto_started; ///< true if the component started rediscovery itself
} sl_gatt_client_manager_rediscovery_needed_evt_t;

/// Expandable event: header selects active union member
typedef struct {
  sl_gatt_client_manager_event_header_t header;
  union {
    sl_gatt_client_manager_discovery_complete_evt_t discovery_complete;
    sl_gatt_client_manager_error_evt_t error;
    sl_gatt_client_manager_characteristic_value_evt_t characteristic_value;
    sl_gatt_client_manager_service_found_evt_t service_found;
    sl_gatt_client_manager_characteristic_found_evt_t characteristic_found;
    sl_gatt_client_manager_rediscovery_needed_evt_t rediscovery_needed;
  } data;
} sl_gatt_client_manager_event_t;

/** @} (end addtogroup gatt_client_manager) */

#ifdef __cplusplus
}
#endif

#endif // SL_GATT_CLIENT_MANAGER_TYPES_H
