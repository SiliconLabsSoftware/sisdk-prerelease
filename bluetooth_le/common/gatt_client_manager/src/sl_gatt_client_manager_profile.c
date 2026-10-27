/***************************************************************************//**
 * @file
 * @brief GATT Client Manager - profile management
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

/// Registered profiles; index is the public profile_handle.
static profile_entry_t profiles[SL_GATT_CLIENT_MANAGER_MAX_PROFILES];
/// Number of slots that have ever been allocated (append-only until reset).
static uint16_t profile_count;

void sli_gcm_clear_module_state(void)
{
  memset(profiles, 0, sizeof(profiles));
  profile_count = 0;
}

const profile_entry_t *sli_gcm_get_profile(uint16_t profile_handle)
{
  if (!sli_gcm_profile_handle_is_valid(profile_handle)) {
    return NULL;
  }
  return &profiles[profile_handle];
}

profile_entry_t *sli_gcm_get_profile_mutable(uint16_t profile_handle)
{
  if (!sli_gcm_profile_handle_is_valid(profile_handle)) {
    return NULL;
  }
  return &profiles[profile_handle];
}

uint16_t sli_gcm_get_profile_count(void)
{
  return profile_count;
}

void sli_gcm_set_profile_count(uint16_t count)
{
  profile_count = count;
}

bool sli_gcm_profile_handle_is_valid(uint16_t profile_handle)
{
  return profile_handle < SL_GATT_CLIENT_MANAGER_MAX_PROFILES
         && profiles[profile_handle].in_use;
}

bool sli_gcm_find_profile_from(uint16_t from, uint16_t *out_handle)
{
  for (uint16_t p = from; p < SL_GATT_CLIENT_MANAGER_MAX_PROFILES; p++) {
    if (profiles[p].in_use) {
      *out_handle = p;
      return true;
    }
  }
  return false;
}

bool sli_gcm_select_next_profile(connection_entry_t *entry)
{
  uint16_t next;

  if (entry->current_profile + 1 >= SL_GATT_CLIENT_MANAGER_MAX_PROFILES) {
    return false;
  }
  if (!sli_gcm_find_profile_from((uint16_t)(entry->current_profile + 1), &next)) {
    return false;
  }
  entry->current_profile = next;
  return true;
}

bool sli_gcm_uuid_is_valid(const sl_gatt_client_manager_uuid_t *uuid)
{
  return uuid != NULL;
}

bool sli_gcm_uuid_equal(const sl_gatt_client_manager_uuid_t *a,
                        const sl_gatt_client_manager_uuid_t *b)
{
  if (a == NULL || b == NULL) {
    return false;
  }
#if SL_GATT_CLIENT_MANAGER_SUPPORT_128BIT_UUID
  if (a->is_128bit != b->is_128bit) {
    return false;
  }
  if (!a->is_128bit) {
    return a->value.uuid16 == b->value.uuid16;
  }
  return memcmp(a->value.uuid128.data, b->value.uuid128.data, 16) == 0;
#else
  return a->uuid16 == b->uuid16;
#endif
}

void sli_gcm_uuid_to_bytes(const sl_gatt_client_manager_uuid_t *uuid,
                           uint8_t *out,
                           size_t *out_len)
{
#if SL_GATT_CLIENT_MANAGER_SUPPORT_128BIT_UUID
  if (uuid->is_128bit == false) {
    out[0] = (uint8_t)(uuid->value.uuid16 & 0xFF);
    out[1] = (uint8_t)((uuid->value.uuid16 >> 8) & 0xFF);
    *out_len = 2;
  } else {
    memcpy(out, uuid->value.uuid128.data, 16);
    *out_len = 16;
  }
#else
  out[0] = (uint8_t)(uuid->uuid16 & 0xFF);
  out[1] = (uint8_t)((uuid->uuid16 >> 8) & 0xFF);
  *out_len = 2;
#endif
}

bool sli_gcm_pairs_equal(const sl_gatt_client_manager_interest_t *a,
                         const sl_gatt_client_manager_interest_t *b,
                         uint8_t pair_count)
{
  for (uint8_t i = 0; i < pair_count; i++) {
    if (!sli_gcm_uuid_equal(&a[i].service, &b[i].service)
        || !sli_gcm_uuid_equal(&a[i].characteristic, &b[i].characteristic)
        || a[i].flags != b[i].flags) {
      return false;
    }
  }
  return true;
}

profile_entry_t *sli_gcm_register_profile_storage(uint16_t *out_index)
{
  if (profile_count >= SL_GATT_CLIENT_MANAGER_MAX_PROFILES) {
    return NULL;
  }
  *out_index = profile_count;
  return &profiles[profile_count];
}
