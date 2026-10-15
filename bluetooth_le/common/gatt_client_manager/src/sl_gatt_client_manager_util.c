/***************************************************************************//**
 * @file
 * @brief GATT Client Manager - logging helpers
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

#include "sl_gatt_client_manager.h"
#include "sli_gatt_client_manager.h"
#include "sl_common.h"
#include "sl_component_catalog.h"

#if defined(SL_CATALOG_APP_LOG_PRESENT) && SL_GATT_CLIENT_MANAGER_LOG
void gcm_log_uuid(const char *label, const sl_gatt_client_manager_uuid_t *uuid)
{
  if (uuid == NULL) {
    gcm_log_debug("%s=<null>" APP_LOG_NL, label);
    return;
  }
#if SL_GATT_CLIENT_MANAGER_SUPPORT_128BIT_UUID
  if (uuid->is_128bit == false) {
    gcm_log_debug("%s=0x%04X" APP_LOG_NL, label, uuid->value.uuid16);
  } else {
    gcm_log_debug("%s=%02X%02X%02X%02X-%02X%02X-%02X%02X-%02X%02X-%02X%02X%02X%02X%02X%02X"
                  APP_LOG_NL,
                  label,
                  uuid->value.uuid128.data[15], uuid->value.uuid128.data[14],
                  uuid->value.uuid128.data[13], uuid->value.uuid128.data[12],
                  uuid->value.uuid128.data[11], uuid->value.uuid128.data[10],
                  uuid->value.uuid128.data[9], uuid->value.uuid128.data[8],
                  uuid->value.uuid128.data[7], uuid->value.uuid128.data[6],
                  uuid->value.uuid128.data[5], uuid->value.uuid128.data[4],
                  uuid->value.uuid128.data[3], uuid->value.uuid128.data[2],
                  uuid->value.uuid128.data[1], uuid->value.uuid128.data[0]);
  }
#else
  gcm_log_debug("%s=0x%04X" APP_LOG_NL, label, uuid->uuid16);
#endif
}

void gcm_log_bd_addr(const char *label, const bd_addr *address, uint8_t address_type)
{
  if (address == NULL) {
    gcm_log_debug("%s=<null>" APP_LOG_NL, label);
    return;
  }
  gcm_log_debug("%s=%02X:%02X:%02X:%02X:%02X:%02X type=%u" APP_LOG_NL,
                label,
                address->addr[5], address->addr[4], address->addr[3],
                address->addr[2], address->addr[1], address->addr[0],
                address_type);
}

#if SL_GATT_CLIENT_MANAGER_CACHE_ENABLE
void gcm_log_hash(const char *label, const uint8_t *hash)
{
  gcm_log_debug("%s=%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X"
                APP_LOG_NL,
                label,
                hash[0], hash[1], hash[2], hash[3], hash[4], hash[5],
                hash[6], hash[7], hash[8], hash[9], hash[10], hash[11],
                hash[12], hash[13], hash[14], hash[15]);
}
#endif
#endif