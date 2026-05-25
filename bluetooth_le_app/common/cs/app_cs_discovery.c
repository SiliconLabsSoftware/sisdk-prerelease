/***************************************************************************//**
 * @file
 * @brief CS example Bluetooth Manager
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

#include <string.h>
#include "app_cs_discovery.h"
#include "app_log.h"
#include "cs_ras_common.h"

// -----------------------------------------------------------------------------
// Definitions

// Discovery state type
SL_ENUM(app_cs_discovery_state_t) {
  APP_CS_DISCOVERY_STATE_IDLE,
  APP_CS_DISCOVERY_STATE_SERVICE_DISCOVERY,
  APP_CS_DISCOVERY_STATE_CHARACTERISTIC_DISCOVERY,
  APP_CS_DISCOVERY_STATE_COMPLETED
};

// Discovery result and state
typedef struct {
    app_cs_discovery_result_t result;
    app_cs_discovery_state_t state;
    app_cs_discovery_complete_cb_t callback;
  } app_cs_discovery_t;

// -----------------------------------------------------------------------------
// Forward declarations of internal functions

static void discovery_config_reset(app_cs_discovery_t *config);
static sl_status_t handle_gatt_procedure_completed(const sl_bt_msg_t *evt);

// -----------------------------------------------------------------------------
// Constants

static const uint16_t service_uuid = CS_RAS_SERVICE_UUID;
static const uint16_t char_uuids[CS_RAS_CHARACTERISTIC_INDEX_COUNT] = {
  CS_RAS_CHAR_UUID_RAS_FEATURES,
  CS_RAS_CHAR_UUID_REAL_TIME_RANGING_DATA,
  CS_RAS_CHAR_UUID_CONTROL_POINT,
  CS_RAS_CHAR_UUID_RANGING_DATA_READY,
  CS_RAS_CHAR_UUID_RANGING_DATA_OVERWRITTEN,
  CS_RAS_CHAR_UUID_ON_DEMAND_RANGING_DATA
};

// -----------------------------------------------------------------------------
// Private variables

// Discovery configuration and status storage
// We assume only a single discovery can be in progress at a time.
static app_cs_discovery_t discovery_config = {
  .result = {
    .conn_handle = SL_BT_INVALID_CONNECTION_HANDLE,
    .service_handle = CS_RAS_INVALID_SERVICE_HANDLE,
    .gattdb_handles = {
      .array = {CS_RAS_INVALID_CHARACTERISTIC_HANDLE}}
    },
  .state = APP_CS_DISCOVERY_STATE_IDLE,
  .callback = NULL
};

// -----------------------------------------------------------------------------
// Public functions

sl_status_t app_cs_discovery_start_discovery(uint8_t conn_handle,
                                             app_cs_discovery_complete_cb_t cb){
  
  if (conn_handle == SL_BT_INVALID_CONNECTION_HANDLE) {
    return SL_STATUS_INVALID_HANDLE;
  }
  if (cb == NULL) {
    return SL_STATUS_NULL_POINTER;
  }
  if (discovery_config.state != APP_CS_DISCOVERY_STATE_IDLE) {
    return SL_STATUS_IN_PROGRESS;
  }
  discovery_config_reset(&discovery_config);

  discovery_config.callback = cb;
  discovery_config.result.conn_handle = conn_handle;
  discovery_config.state = APP_CS_DISCOVERY_STATE_SERVICE_DISCOVERY;
  sl_status_t sc = sl_bt_gatt_discover_primary_services(conn_handle);
  if (sc != SL_STATUS_OK) {
    app_log_error("Failed to start RAS service discovery: 0x%lx" APP_LOG_NL,
                  (unsigned long)sc);
    discovery_config.state = APP_CS_DISCOVERY_STATE_IDLE;
    return sc;
  }
  return SL_STATUS_OK;
 }

 void app_cs_discovery_on_bt_event(const sl_bt_msg_t *evt) {
   switch (SL_BT_MSG_ID(evt->header)) {
    // --------------------------------
    // Connection closed: abort any in-progress discovery for this connection.
    case sl_bt_evt_connection_closed_id:
      if (evt->data.evt_connection_closed.connection 
          == discovery_config.result.conn_handle) {
        discovery_config_reset(&discovery_config);
      }
      break;
    // --------------------------------
    // GATT procedure completed
    case sl_bt_evt_gatt_procedure_completed_id: {
      // Ignore events for connections we are not currently discovering.
      if (evt->data.evt_gatt_procedure_completed.connection 
          != discovery_config.result.conn_handle) {
        break;
      }
      sl_status_t sc = handle_gatt_procedure_completed(evt);
      // Only notify on terminal states (success or failure). Intermediate
      // transitions (e.g. service -> characteristic discovery) keep the
      // context alive and wait for the next procedure_completed event.
      if (sc == SL_STATUS_IN_PROGRESS) {
        break;
      }
      discovery_config.result.status = sc;
      if (discovery_config.callback != NULL) {
        discovery_config.callback(&discovery_config.result);
        discovery_config.callback = NULL;
      } else {
        app_log_error("Discovery callback not set" APP_LOG_NL);
      }
      discovery_config_reset(&discovery_config);
    }
      break;
    // --------------------------------
    // New GATT characteristic discovered
    case sl_bt_evt_gatt_characteristic_id: {
      if (evt->data.evt_gatt_characteristic.connection 
          != discovery_config.result.conn_handle) {
        break;
      }
      if (evt->data.evt_gatt_characteristic.uuid.len 
          != sizeof(char_uuids[0])) {
        break;
      }
      for (int i = 0; i < CS_RAS_CHARACTERISTIC_INDEX_COUNT; i++) {
        if (memcmp(&char_uuids[i], 
                   evt->data.evt_gatt_characteristic.uuid.data,
                   sizeof(char_uuids[i])) == 0) {
          discovery_config.result.gattdb_handles.array[i] 
            = evt->data.evt_gatt_characteristic.characteristic;
          app_log_debug("Found %u. characteristic: [0x%lx]" APP_LOG_NL, 
                        i,
                        (unsigned long)discovery_config.result.gattdb_handles.array[i]);
        }
      }
    }
      break;
    // --------------------------------
    // New GATT service discovered
    case sl_bt_evt_gatt_service_id: {
      if (evt->data.evt_gatt_service.connection != discovery_config.result.conn_handle) {
        break;
      }
      if (evt->data.evt_gatt_service.uuid.len != sizeof(service_uuid)) {
        break;
      }
      if (memcmp(&service_uuid,
                 evt->data.evt_gatt_service.uuid.data,
                 sizeof(service_uuid)) == 0) {
        discovery_config.result.service_handle = evt->data.evt_gatt_service.service;
        app_log_debug("Found service: %lu" APP_LOG_NL,
                      (unsigned long)discovery_config.result.service_handle);
      }
    }
      break;
   }
 }

 // -----------------------------------------------------------------------------
// Private functions

static sl_status_t handle_gatt_procedure_completed(const sl_bt_msg_t *evt) {
  uint8_t evt_conn_handle = evt->data.evt_gatt_procedure_completed.connection;
  if (evt_conn_handle != discovery_config.result.conn_handle) {
    // Procedure completion for an unrelated connection - ignore.
    return SL_STATUS_IN_PROGRESS;
  }

  uint16_t procedure_result = evt->data.evt_gatt_procedure_completed.result;
  if (procedure_result != SL_STATUS_OK) {
    app_log_error("GATT procedure failed: 0x%04x" APP_LOG_NL, procedure_result);
    return (sl_status_t)procedure_result;
  }

  switch (discovery_config.state) {
    case APP_CS_DISCOVERY_STATE_SERVICE_DISCOVERY: {
      if (discovery_config.result.service_handle == CS_RAS_INVALID_SERVICE_HANDLE) {
        app_log_error("RAS service not found for connection: %u" APP_LOG_NL,
                      discovery_config.result.conn_handle);
        return SL_STATUS_NOT_FOUND;
      }
      sl_status_t sc = sl_bt_gatt_discover_characteristics(
          discovery_config.result.conn_handle,
          discovery_config.result.service_handle);
      if (sc != SL_STATUS_OK) {
        app_log_error("Failed to start RAS characteristic discovery: 0x%lx" APP_LOG_NL,
                      (unsigned long)sc);
        return sc;
      }
      discovery_config.state = APP_CS_DISCOVERY_STATE_CHARACTERISTIC_DISCOVERY;
      return SL_STATUS_IN_PROGRESS;
    }

    case APP_CS_DISCOVERY_STATE_CHARACTERISTIC_DISCOVERY:
      discovery_config.state = APP_CS_DISCOVERY_STATE_COMPLETED;
      return SL_STATUS_OK;

    case APP_CS_DISCOVERY_STATE_IDLE:
    case APP_CS_DISCOVERY_STATE_COMPLETED:
    default:
      return SL_STATUS_IN_PROGRESS;
  }
}

static void discovery_config_reset(app_cs_discovery_t *config) {
  config->result.conn_handle = SL_BT_INVALID_CONNECTION_HANDLE;
  config->result.service_handle = CS_RAS_INVALID_SERVICE_HANDLE;
  for (int i = 0; i < CS_RAS_CHARACTERISTIC_INDEX_COUNT; i++) {
    config->result.gattdb_handles.array[i] = CS_RAS_INVALID_CHARACTERISTIC_HANDLE;
  }
  config->state = APP_CS_DISCOVERY_STATE_IDLE;
  config->result.status = SL_STATUS_FAIL;
}