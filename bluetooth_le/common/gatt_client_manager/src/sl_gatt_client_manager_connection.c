/***************************************************************************//**
 * @file
 * @brief GATT Client Manager - per-connection state
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

/// Active connections tracked by this component (indexed by free-slot search).
static connection_entry_t connections[SL_GATT_CLIENT_MANAGER_MAX_CONNECTIONS];

connection_entry_t *sli_gcm_find_connection_entry(uint8_t connection)
{
  for (uint8_t i = 0; i < SL_GATT_CLIENT_MANAGER_MAX_CONNECTIONS; i++) {
    if (connections[i].in_use && connections[i].connection == connection) {
      return &connections[i];
    }
  }
  return NULL;
}

void sli_gcm_clear_profile_results(connection_entry_t *entry, uint16_t profile_handle)
{
  for (uint8_t i = 0; i < SL_GATT_CLIENT_MANAGER_MAX_PAIRS_PER_PROFILE; i++) {
    discovery_result_t *result = &entry->results[profile_handle][i];
    result->service_handle = SL_GATT_CLIENT_MANAGER_INVALID_SERVICE_HANDLE;
    result->characteristic_handle =
      SL_GATT_CLIENT_MANAGER_INVALID_CHARACTERISTIC_HANDLE;
    result->service_match_count = 0;
    result->service_found = false;
    result->characteristic_found = false;
  }
}

void sli_gcm_clear_all_results(connection_entry_t *entry)
{
  for (uint16_t p = 0; p < SL_GATT_CLIENT_MANAGER_MAX_PROFILES; p++) {
    sli_gcm_clear_profile_results(entry, p);
  }
  entry->from_cache = false;
}

connection_entry_t *sli_gcm_alloc_connection_entry(uint8_t connection)
{
  connection_entry_t *entry = sli_gcm_find_connection_entry(connection);
  if (entry != NULL) {
    return entry;
  }

  for (uint8_t i = 0; i < SL_GATT_CLIENT_MANAGER_MAX_CONNECTIONS; i++) {
    if (!connections[i].in_use) {
      memset(&connections[i], 0, sizeof(connections[i]));
      connections[i].in_use = true;
      connections[i].connection = connection;
      connections[i].state = discovery_state_idle;
      connections[i].hash_state = db_hash_unknown;
      sli_gcm_clear_all_results(&connections[i]);
      return &connections[i];
    }
  }
  gcm_log_debug("no free connection slot conn=%u (max=%u)" APP_LOG_NL,
                connection,
                (unsigned)SL_GATT_CLIENT_MANAGER_MAX_CONNECTIONS);
  return NULL;
}

void sli_gcm_clear_connection_entry(uint8_t connection)
{
  connection_entry_t *entry = sli_gcm_find_connection_entry(connection);
  if (entry != NULL) {
    memset(entry, 0, sizeof(*entry));
  }
}

void sli_gcm_clear_all_connections(void)
{
  memset(connections, 0, sizeof(connections));
}
