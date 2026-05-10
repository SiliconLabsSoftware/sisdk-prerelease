/***************************************************************************//**
 * @file
 * @brief CS Manager - Core implementation
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
#include "cs_manager.h"
#include "cs_manager_internal.h"
#include "cs_manager_config.h"
#include "cs_manager_config_db.h"
#include "cs_manager_log.h"

// -----------------------------------------------------------------------------
// Static variables

static cs_manager_t cs_manager_instances[CS_MANAGER_CONFIG_MAX_INSTANCES];

// -----------------------------------------------------------------------------
// Public function definitions

sl_status_t cs_manager_create(uint8_t conn_handle)
{
  for (int i = 0; i < CS_MANAGER_CONFIG_MAX_INSTANCES; i++) {
    if (cs_manager_instances[i].conn_handle == SL_BT_INVALID_CONNECTION_HANDLE) {
      cs_manager_instances[i].conn_handle = conn_handle;
      return SL_STATUS_OK;
    }
  }
  return SL_STATUS_FULL;
}

sl_status_t cs_manager_delete(uint8_t conn_handle)
{
  for (int i = 0; i < CS_MANAGER_CONFIG_MAX_INSTANCES; i++) {
    if (cs_manager_instances[i].conn_handle == conn_handle) {
      cs_manager_instances[i].conn_handle = SL_BT_INVALID_CONNECTION_HANDLE;
      return SL_STATUS_OK;
    }
  }
  return SL_STATUS_NOT_FOUND;
}

sl_status_t cs_manager_config_create(uint8_t conn_handle,
                                     uint8_t config_id,
                                     bool create_context,
                                     const sl_bt_evt_cs_config_complete_t *config)
{
  (void)conn_handle;
  (void)config_id;
  (void)create_context;
  (void)config;
  // TODO: This will lead to the sl_bt_evt_cs_config_complete_id event being sent.
  return SL_STATUS_OK;
}

sl_status_t cs_manager_config_remove(uint8_t conn_handle, uint8_t config_id)
{
  sl_status_t sc = cs_manager_config_db_remove(conn_handle,
                                               config_id);
  if (sc != SL_STATUS_OK) {
    cs_manager_log_error(INSTANCE_PREFIX "Failed to remove config" NL,
                         conn_handle,
                         config_id);
    return sc;
  }
  cs_manager_on_event(CS_MANAGER_EVENT_CONFIG_REMOVED,
                      conn_handle,
                      config_id);
  return SL_STATUS_OK;
}

// -----------------------------------------------------------------------------
// Internal function definitions

sl_status_t cs_manager_init(void)
{
  for (int i = 0; i < CS_MANAGER_CONFIG_MAX_INSTANCES; i++) {
    cs_manager_instances[i].conn_handle = SL_BT_INVALID_CONNECTION_HANDLE;
    for (int j = 0; j < CS_MANAGER_CONFIG_COUNT; j++) {
      cs_manager_instances[i].configs[j].connection = SL_BT_INVALID_CONNECTION_HANDLE;
      cs_manager_instances[i].configs[j].config_id = CS_MANAGER_INVALID_CONFIG_ID;
    }
  }
  return SL_STATUS_OK;
}

cs_manager_t *cs_manager_get_instance(uint8_t conn_handle)
{
  for (int i = 0; i < CS_MANAGER_CONFIG_MAX_INSTANCES; i++) {
    if (cs_manager_instances[i].conn_handle == conn_handle) {
      return &cs_manager_instances[i];
    }
  }
  return NULL;
}

// -----------------------------------------------------------------------------
// Event / callback definitions

void cs_manager_on_bt_event(const sl_bt_msg_t *evt)
{
  switch (SL_BT_MSG_ID(evt->header)) {
    case sl_bt_evt_cs_config_complete_id: {
      sl_status_t sc = cs_manager_config_db_create(&evt->data.evt_cs_config_complete);
      if (sc != SL_STATUS_OK) {
        cs_manager_on_event(CS_MANAGER_EVENT_CONFIG_ERROR,
                            evt->data.evt_cs_config_complete.connection,
                            evt->data.evt_cs_config_complete.config_id);
        break;
      }
      cs_manager_on_event(CS_MANAGER_EVENT_CONFIG_CREATED,
                          evt->data.evt_cs_config_complete.connection,
                          evt->data.evt_cs_config_complete.config_id);
      break;
    }
    default:
      break;
  }
}

SL_WEAK void cs_manager_on_event(cs_manager_event_type_t event,
                                 uint8_t conn_handle,
                                 uint8_t config_id)
{
  (void)event;
  (void)conn_handle;
  (void)config_id;
}
