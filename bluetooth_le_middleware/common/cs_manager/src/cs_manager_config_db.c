/***************************************************************************//**
 * @file
 * @brief CS Manager configuration database implementation
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

// -----------------------------------------------------------------------------
// Includes

#include "sl_status.h"
#include "sl_common.h"
#include "sl_bt_api.h"
#include "sl_bluetooth_connection_config.h"
#include "cs_manager.h"
#include "cs_manager_internal.h"
#include "cs_manager_config.h"
#include "cs_manager_config_db.h"

// -----------------------------------------------------------------------------
// Macros

#define CS_MANAGER_CONFIG_MAX_CONNECTIONS SL_BT_CONFIG_MAX_CONNECTIONS

// -----------------------------------------------------------------------------
// Enums, structs, typedefs

// -----------------------------------------------------------------------------
// Static function declarations

static sl_bt_evt_cs_config_complete_t *find_entry_by_config_id(uint8_t conn_handle,
                                                          uint8_t config_id);
static sl_bt_evt_cs_config_complete_t *find_entry_to_write(uint8_t conn_handle,
                                                      uint8_t config_id);

// -----------------------------------------------------------------------------
// Static variables

// -----------------------------------------------------------------------------
// Private (static) function definitions

static sl_bt_evt_cs_config_complete_t *find_entry_by_config_id(uint8_t conn_handle,
                                                               uint8_t config_id)
{
  cs_manager_t *instance = cs_manager_get_instance(conn_handle);
  if (instance == NULL) {
    return NULL;
  }
  for (int i = 0; i < CS_MANAGER_CONFIG_COUNT; i++) {
    if (instance->configs[i].config_id == config_id) {
      return &instance->configs[i];
    }
  }
  return NULL;
}

static sl_bt_evt_cs_config_complete_t *find_entry_to_write(uint8_t conn_handle,
                                                           uint8_t config_id)
{
  cs_manager_t *instance = cs_manager_get_instance(conn_handle);
  if (instance == NULL) {
    return NULL;
  }
  for (int i = 0; i < CS_MANAGER_CONFIG_COUNT; i++) {
    if (instance->configs[i].config_id == config_id) {
      return &instance->configs[i];
    }
  }

  for (int i = 0; i < CS_MANAGER_CONFIG_COUNT; i++) {
    if (instance->configs[i].config_id == CS_MANAGER_INVALID_CONFIG_ID) {
      return &instance->configs[i];
    }
  }

  return NULL;
}

// -----------------------------------------------------------------------------
// Public function definitions

sl_status_t cs_manager_config_db_create(const sl_bt_evt_cs_config_complete_t *config)
{
  if (config == NULL) {
    return SL_STATUS_NULL_POINTER;
  }
  sl_bt_evt_cs_config_complete_t *entry = find_entry_to_write(config->connection, 
                                                              config->config_id);
  if (entry == NULL) {
    return SL_STATUS_FULL;
  }
  *entry = *config;
  return SL_STATUS_OK;
}

sl_status_t cs_manager_config_db_get(uint8_t conn_handle,
                                      uint8_t config_id,
                                      sl_bt_evt_cs_config_complete_t *config_out)
{
  if (config_out == NULL) {
    return SL_STATUS_NULL_POINTER;
  }
  sl_bt_evt_cs_config_complete_t *entry = find_entry_by_config_id(conn_handle, config_id);
  if (entry == NULL || 
      entry->config_id == CS_MANAGER_INVALID_CONFIG_ID) {
    return SL_STATUS_NOT_FOUND;
  }
  *config_out = *entry;
  return SL_STATUS_OK;
}

sl_status_t cs_manager_config_db_remove(uint8_t conn_handle,
                                        uint8_t config_id)
{
  sl_bt_evt_cs_config_complete_t *entry = find_entry_by_config_id(conn_handle, config_id);
  if (entry == NULL || 
      entry->config_id == CS_MANAGER_INVALID_CONFIG_ID) {
    return SL_STATUS_NOT_FOUND;
  }
  entry->connection = SL_BT_INVALID_CONNECTION_HANDLE;
  entry->config_id = CS_MANAGER_INVALID_CONFIG_ID;
  return SL_STATUS_OK;
}

void cs_manager_config_db_process_bt_event(const sl_bt_msg_t *evt)
{
  //TODO
}
