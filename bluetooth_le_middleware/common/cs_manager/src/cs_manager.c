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
#include "app_rta.h"
#include "cs_manager.h"
#include "cs_manager_internal.h"
#include "cs_manager_config.h"
#include "cs_manager_config_db_internal.h"
#include "cs_manager_log_internal.h"
#include "cs_common.h"
#include "sl_component_catalog.h"
#ifdef SL_CATALOG_CS_MANAGER_FEATURE_CONTROL_PRESENT
#include "cs_manager_cs_control_internal.h"
#endif // SL_CATALOG_CS_MANAGER_FEATURE_CONTROL_PRESENT
#ifdef SL_CATALOG_CS_MANAGER_FEATURE_CONFIG_PRESENT
#include "cs_manager_cs_config_internal.h"
#endif // SL_CATALOG_CS_MANAGER_FEATURE_CONFIG_PRESENT

// -----------------------------------------------------------------------------
// Forward declaration of private functions

static void handle_cs_config_completed(const cs_config_data_t *evt);
static void handle_cs_config_created(cs_manager_t *m, const cs_config_data_t *evt);
static void handle_cs_config_removed(cs_manager_t *m, const cs_config_data_t *evt);
static bool needs_connection_parameter_update(const cs_manager_connection_parameters_t *params);
static void on_runtime_error(app_rta_error_t error, sl_status_t result);

// -----------------------------------------------------------------------------
// Static variables

// Instance storage
static cs_manager_t cs_manager_instances[CS_MANAGER_CONFIG_MAX_INSTANCES];

// Event callback
cs_manager_event_t on_event;

// User error callback
cs_manager_on_error_t on_error;

// RTA guard context — also extern-declared in cs_manager_internal.h
app_rta_context_t cs_manager_ctx;

// -----------------------------------------------------------------------------
// Public function definitions

sl_status_t cs_manager_create(uint8_t conn_handle,
                              cs_manager_instance_config_t *inst_config,
                              cs_manager_connection_parameters_t *connection_parameters)
{
  if (conn_handle == SL_BT_INVALID_CONNECTION_HANDLE) {
    return SL_STATUS_INVALID_HANDLE;
  }
  if (inst_config == NULL) {
    return SL_STATUS_NULL_POINTER;
  }
  #if !defined(CS_MANAGER_CONFIG_SET_DEFAULT_CONNECTION_PARAMETERS) \
      || CS_MANAGER_CONFIG_SET_DEFAULT_CONNECTION_PARAMETERS == 0
  if (connection_parameters == NULL) {
    return SL_STATUS_NULL_POINTER;
  }
  #endif
  sl_status_t sc = app_rta_acquire(cs_manager_ctx);
  if (sc != SL_STATUS_OK) {
    return sc;
  }
  if (on_event == NULL) {
    (void)app_rta_release(cs_manager_ctx);
    return SL_STATUS_NOT_INITIALIZED;
  }
  // Check existance
  cs_manager_t *m = cs_manager_find(conn_handle);
  if (m != NULL) {
    (void)app_rta_release(cs_manager_ctx);
    return SL_STATUS_ALREADY_EXISTS;
  }
  // Check free slot
  m = cs_manager_find(SL_BT_INVALID_CONNECTION_HANDLE);
  if (m == NULL) {
    (void)app_rta_release(cs_manager_ctx);
    return SL_STATUS_FULL;
  }

  uint8_t cs_sync_antenna;
  uint8_t num_antennas;

  // Prepare for the CS main mode: RTT antenna usage
  sc = sl_bt_cs_read_local_supported_capabilities(NULL,
                                                  NULL,
                                                  &num_antennas,
                                                  NULL,
                                                  NULL,
                                                  NULL,
                                                  NULL,
                                                  NULL,
                                                  NULL,
                                                  NULL,
                                                  NULL,
                                                  NULL,
                                                  NULL,
                                                  NULL,
                                                  NULL,
                                                  NULL,
                                                  NULL,
                                                  NULL,
                                                  NULL,
                                                  NULL);

  if (sc != SL_STATUS_OK) {
    (void)app_rta_release(cs_manager_ctx);
    return sc;
  }
  cs_sync_antenna = inst_config->cs_sync_antenna;

  switch (inst_config->cs_sync_antenna) {
    case CS_SYNC_ANTENNA_1:
      cs_manager_log_info(INSTANCE_PREFIX "RTT - Using the antenna ID 1" NL,
                          conn_handle);
      break;
    case CS_SYNC_ANTENNA_2:
      if (num_antennas >= 2) {
        cs_manager_log_info(INSTANCE_PREFIX " RTT - 2-antenna device! Using the antenna ID 2" NL,
                            conn_handle);
      } else {
        cs_manager_log_info(INSTANCE_PREFIX " RTT - only 1-antenna device! Using the antenna ID 1" NL,
                            conn_handle);
        cs_sync_antenna = CS_SYNC_ANTENNA_1;
      }
      break;
    case CS_SYNC_SWITCHING:
      cs_manager_log_info(INSTANCE_PREFIX " RTT - switching between %u available antennas" NL,
                          conn_handle,
                          num_antennas);
      cs_sync_antenna = CS_SYNC_SWITCHING;
      break;
    default:
      cs_manager_log_info(INSTANCE_PREFIX " RTT - unknown antenna usage! "
                                          "Using the default setting: antenna ID 1" NL,
                         conn_handle);
      cs_sync_antenna = CS_SYNC_ANTENNA_1;
      break;
  }

  sl_bt_cs_role_status_t initiator_status = sl_bt_cs_role_status_disable;
  sl_bt_cs_role_status_t reflector_status = sl_bt_cs_role_status_disable;
  #ifdef SL_CATALOG_CS_MANAGER_FEATURE_INITIATOR_PRESENT
  if (inst_config->is_initiator) {
    initiator_status = sl_bt_cs_role_status_enable;
  }
  #else // SL_CATALOG_CS_MANAGER_FEATURE_INITIATOR_PRESENT
  if (inst_config->is_initiator) {
    (void)app_rta_release(cs_manager_ctx);
    return SL_STATUS_NOT_SUPPORTED;
  }
  #endif // SL_CATALOG_CS_MANAGER_FEATURE_INITIATOR_PRESENT
  #ifdef SL_CATALOG_CS_MANAGER_FEATURE_REFLECTOR_PRESENT
  if (!inst_config->is_initiator) {
    reflector_status = sl_bt_cs_role_status_enable;
  }
  #else // SL_CATALOG_CS_MANAGER_FEATURE_REFLECTOR_PRESENT
  if (!inst_config->is_initiator) {
    (void)app_rta_release(cs_manager_ctx);
    return SL_STATUS_NOT_SUPPORTED;
  }
  #endif // SL_CATALOG_CS_MANAGER_FEATURE_REFLECTOR_PRESENT

  sc = sl_bt_cs_set_default_settings(conn_handle,
                                     initiator_status,
                                     reflector_status,
                                     cs_sync_antenna,
                                     inst_config->max_tx_power_dbm);
  if (sc != SL_STATUS_OK) {
    cs_manager_log_error(INSTANCE_PREFIX " Error setting CS default settings" NL,
                         conn_handle);
    (void)app_rta_release(cs_manager_ctx);
    return sc;
  }

  // Set initial state
  m->state = CS_MANAGER_STATE_IDLE;
  m->security_enabled = false;
  m->instance_config = *inst_config;
  m->instance_config.cs_sync_antenna = cs_sync_antenna;
  m->conn_handle = conn_handle;
  m->active_config_id = CS_MANAGER_INVALID_CONFIG_ID;
  if (connection_parameters != NULL) {
    m->manage_connection_parameters = true;
    memcpy(&m->connection_parameters,
           connection_parameters,
           sizeof(m->connection_parameters));
  } else {
    m->manage_connection_parameters = false;
  }

  // Set connection parameters before CS security if needed
  if (needs_connection_parameter_update(connection_parameters)) {
    sc = sl_bt_connection_set_parameters(conn_handle,
                                         connection_parameters->min_connection_interval,
                                         connection_parameters->max_connection_interval,
                                         connection_parameters->latency,
                                         connection_parameters->timeout,
                                         connection_parameters->min_ce_length,
                                         connection_parameters->max_ce_length);
    if (sc != SL_STATUS_OK) {
      cs_manager_log_error(INSTANCE_PREFIX "Error setting connection parameters" NL, conn_handle);
      m->conn_handle = SL_BT_INVALID_CONNECTION_HANDLE;
      (void)app_rta_release(cs_manager_ctx);
      return sc;
    }
    cs_manager_log_info(INSTANCE_PREFIX "Setting connection parameters, waiting for confirmation" NL, conn_handle);
    m->state = CS_MANAGER_STATE_SETTING_CONN_PARAMS;
    (void)app_rta_release(cs_manager_ctx);
    return SL_STATUS_OK;
  } else if (connection_parameters != NULL) {
    cs_manager_log_info(INSTANCE_PREFIX "Connection parameters match defaults, skipping update" NL, conn_handle);
  }

  #if defined(CS_MANAGER_CONFIG_ENABLE_CS_SECURITY_BEFORE_CONFIG) && CS_MANAGER_CONFIG_ENABLE_CS_SECURITY_BEFORE_CONFIG == 1
  if (inst_config->is_central) {
    sc = sl_bt_cs_security_enable(conn_handle);
    if (sc != SL_STATUS_OK) {
      cs_manager_log_error(INSTANCE_PREFIX " Error enabling CS security" NL,
                           conn_handle);
      m->conn_handle = SL_BT_INVALID_CONNECTION_HANDLE;
      (void)app_rta_release(cs_manager_ctx);
      return sc;
    }
    cs_manager_log_info(INSTANCE_PREFIX "Enabling CS security before configuration" NL, conn_handle);
    m->state = CS_MANAGER_STATE_ENABLING_SECURITY;
  }
  #endif // defined(CS_MANAGER_CONFIG_ENABLE_CS_SECURITY_BEFORE_CONFIG) && CS_MANAGER_CONFIG_ENABLE_CS_SECURITY_BEFORE_CONFIG == 1

  (void)app_rta_release(cs_manager_ctx);
  return sc;
}

sl_status_t cs_manager_delete(uint8_t conn_handle)
{
  if (conn_handle == SL_BT_INVALID_CONNECTION_HANDLE) {
    return SL_STATUS_INVALID_HANDLE;
  }
  sl_status_t sc = app_rta_acquire(cs_manager_ctx);
  if (sc != SL_STATUS_OK) {
    return sc;
  }
  cs_manager_t *m = cs_manager_find(conn_handle);
  if (m == NULL) {
    (void)app_rta_release(cs_manager_ctx);
    return SL_STATUS_NOT_FOUND;
  }
  (void)cs_manager_stop(conn_handle);
  // Clear handle
  m->conn_handle = SL_BT_INVALID_CONNECTION_HANDLE;
  for (int j = 0; j < CS_MANAGER_CONFIG_COUNT; j++) {
    m->configs[j].connection = SL_BT_INVALID_CONNECTION_HANDLE;
    m->configs[j].config_id = CS_MANAGER_INVALID_CONFIG_ID;
  }
  (void)app_rta_release(cs_manager_ctx);
  return SL_STATUS_OK;
}

// Assign callback
sl_status_t cs_manager_set_event_callbacks(cs_manager_event_callback_t *cb)
{
  if ((cb == NULL)
      || (cb->on_event == NULL)
      || (cb->on_error == NULL)) {
    return SL_STATUS_NULL_POINTER;
  }
  sl_status_t sc = app_rta_acquire(cs_manager_ctx);
  if (sc != SL_STATUS_OK) {
    return sc;
  }
  on_event = cb->on_event;
  on_error = cb->on_error;
  (void)app_rta_release(cs_manager_ctx);
  return SL_STATUS_OK;
}

sl_status_t cs_manager_get_default_instance_config(bool is_initiator,
                                                   bool is_central,
                                                   cs_manager_instance_config_t *instance_config)
{
  if (instance_config == NULL) {
    return SL_STATUS_NULL_POINTER;
  }
  memset(instance_config, 0, sizeof(*instance_config));
  instance_config->is_initiator = is_initiator;
  instance_config->is_central = is_central;
  instance_config->max_tx_power_dbm = CS_MANAGER_CONFIG_DEFAULT_MAX_TX_POWER_DBM;
  instance_config->cs_sync_antenna = CS_MANAGER_CONFIG_DEFAULT_CS_SYNC_ANTENNA;
  instance_config->conn_phy = CS_MANAGER_CONFIG_DEFAULT_CONN_PHY;
  return SL_STATUS_OK;
}

bool cs_manager_is_full(void)
{
  sl_status_t sc = app_rta_acquire(cs_manager_ctx);
  if (sc != SL_STATUS_OK) {
    return true;
  }
  cs_manager_t *m = cs_manager_find(SL_BT_INVALID_CONNECTION_HANDLE);
  bool full = (m == NULL);
  (void)app_rta_release(cs_manager_ctx);
  return full;
}

sl_status_t cs_manager_get_default_connection_parameters(cs_manager_connection_parameters_t *params)
{
  if (params == NULL) {
    return SL_STATUS_NULL_POINTER;
  }
  params->min_connection_interval = CS_MANAGER_CONFIG_DEFAULT_MIN_CONNECTION_INTERVAL;
  params->max_connection_interval = CS_MANAGER_CONFIG_DEFAULT_MAX_CONNECTION_INTERVAL;
  params->latency = CS_MANAGER_CONFIG_DEFAULT_CONNECTION_PERIPHERAL_LATENCY;
  params->timeout = CS_MANAGER_CONFIG_DEFAULT_CONNECTION_TIMEOUT;
  params->min_ce_length = CS_MANAGER_CONFIG_DEFAULT_MIN_CE_LENGTH;
  params->max_ce_length = CS_MANAGER_CONFIG_DEFAULT_MAX_CE_LENGTH;
  return SL_STATUS_OK;
}

// -----------------------------------------------------------------------------
// Public Control functions

sl_status_t cs_manager_start(uint8_t conn_handle,
                             uint8_t config_id,
                             cs_procedure_parameters_t *params)
{
  sl_status_t sc;
  #ifdef SL_CATALOG_CS_MANAGER_FEATURE_CONTROL_PRESENT
  sc = app_rta_acquire(cs_manager_ctx);
  if (sc != SL_STATUS_OK) {
    return sc;
  }
  sc = cs_manager_cs_control_start(conn_handle, config_id, params);
  (void)app_rta_release(cs_manager_ctx);
  #else // SL_CATALOG_CS_MANAGER_FEATURE_CONTROL_PRESENT
  (void)conn_handle;
  (void)config_id;
  (void)params;
  sc = SL_STATUS_NOT_SUPPORTED;
  #endif // SL_CATALOG_CS_MANAGER_FEATURE_CONTROL_PRESENT
  return sc;
}

sl_status_t cs_manager_stop(uint8_t conn_handle)
{
  sl_status_t sc;
  #ifdef SL_CATALOG_CS_MANAGER_FEATURE_CONTROL_PRESENT
  sc = app_rta_acquire(cs_manager_ctx);
  if (sc != SL_STATUS_OK) {
    return sc;
  }
  sc = cs_manager_cs_control_stop(conn_handle);
  (void)app_rta_release(cs_manager_ctx);
  #else // SL_CATALOG_CS_MANAGER_FEATURE_CONTROL_PRESENT
  (void)conn_handle;
  sc = SL_STATUS_NOT_SUPPORTED;
  #endif // SL_CATALOG_CS_MANAGER_FEATURE_CONTROL_PRESENT
  return sc;
}

sl_status_t cs_manager_get_default_procedure_parameters(cs_procedure_parameters_t *params)
{
  #ifdef SL_CATALOG_CS_MANAGER_FEATURE_CONTROL_PRESENT
  return cs_manager_cs_control_get_default_procedure_parameters(params);
  #else // SL_CATALOG_CS_MANAGER_FEATURE_CONTROL_PRESENT
  (void)params;
  return SL_STATUS_NOT_SUPPORTED;
  #endif // SL_CATALOG_CS_MANAGER_FEATURE_CONTROL_PRESENT
}

sl_status_t cs_manager_get_default_config(cs_config_t *config)
{
  #ifdef SL_CATALOG_CS_MANAGER_FEATURE_CONFIG_PRESENT
  return cs_manager_cs_config_get_default_config(config);
  #else // SL_CATALOG_CS_MANAGER_FEATURE_CONFIG_PRESENT
  (void)config;
  return SL_STATUS_NOT_SUPPORTED;
  #endif // SL_CATALOG_CS_MANAGER_FEATURE_CONFIG_PRESENT
}

// -----------------------------------------------------------------------------
// Public Config functions

sl_status_t cs_manager_config_create(uint8_t conn_handle,
                                     uint8_t config_id,
                                     bool create_context,
                                     const cs_config_t *config)
{
  sl_status_t sc;
  #ifdef SL_CATALOG_CS_MANAGER_FEATURE_CONFIG_PRESENT
  sc = app_rta_acquire(cs_manager_ctx);
  if (sc != SL_STATUS_OK) {
    return sc;
  }
  sc = cs_manager_cs_config_create(conn_handle, config_id, create_context, config);
  (void)app_rta_release(cs_manager_ctx);
  #else // SL_CATALOG_CS_MANAGER_FEATURE_CONFIG_PRESENT
  (void)conn_handle;
  (void)config_id;
  (void)create_context;
  (void)config;
  sc = SL_STATUS_NOT_SUPPORTED;
  #endif // SL_CATALOG_CS_MANAGER_FEATURE_CONFIG_PRESENT
  return sc;
}

sl_status_t cs_manager_config_remove(uint8_t conn_handle, uint8_t config_id)
{
  sl_status_t sc;
  #ifdef SL_CATALOG_CS_MANAGER_FEATURE_CONFIG_PRESENT
  sc = app_rta_acquire(cs_manager_ctx);
  if (sc != SL_STATUS_OK) {
    return sc;
  }
  sc = cs_manager_cs_config_remove(conn_handle, config_id);
  (void)app_rta_release(cs_manager_ctx);
  #else // SL_CATALOG_CS_MANAGER_FEATURE_CONFIG_PRESENT
  (void)conn_handle;
  (void)config_id;
  sc = SL_STATUS_NOT_SUPPORTED;
  #endif // SL_CATALOG_CS_MANAGER_FEATURE_CONFIG_PRESENT
  return sc;
}

sl_status_t cs_manager_config_get(uint8_t conn_handle,
                                  uint8_t config_id,
                                  cs_config_data_t *config_out)
{
  sl_status_t sc = app_rta_acquire(cs_manager_ctx);
  if (sc != SL_STATUS_OK) {
    return sc;
  }
  sc = cs_manager_config_db_get(conn_handle, config_id, config_out);
  (void)app_rta_release(cs_manager_ctx);
  return sc;
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

cs_manager_t *cs_manager_find(uint8_t conn_handle)
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
  sl_status_t sc = app_rta_acquire(cs_manager_ctx);
  if (sc != SL_STATUS_OK) {
    on_runtime_error(APP_RTA_ERROR_ACQUIRE_FAILED, sc);
    return;
  }

  switch (SL_BT_MSG_ID(evt->header)) {
    case sl_bt_evt_system_boot_id: {
      int16_t min_tx_power_x10 = CS_MANAGER_CONFIG_DEFAULT_MIN_TX_POWER_DBM * 10;
      int16_t max_tx_power_x10 = CS_MANAGER_CONFIG_DEFAULT_MAX_TX_POWER_DBM * 10;
      sc = sl_bt_system_set_tx_power(min_tx_power_x10,
                                     max_tx_power_x10,
                                     &min_tx_power_x10,
                                     &max_tx_power_x10);
      if (sc == SL_STATUS_OK) {
        cs_manager_log_info("Set minimum transmit power to: %d dBm" APP_LOG_NL, min_tx_power_x10 / 10);
        cs_manager_log_info("Set maximum transmit power to: %d dBm" APP_LOG_NL, max_tx_power_x10 / 10);
      } else {
        cs_manager_log_error("Failed to set system TX power" APP_LOG_NL);
        on_event(SL_BT_INVALID_CONNECTION_HANDLE,
                 CS_MANAGER_INVALID_CONFIG_ID,
                 CS_MANAGER_EVENT_ERROR, 
                 sc);
      }
      #if defined(CS_MANAGER_CONFIG_SET_DEFAULT_CONNECTION_PARAMETERS) && CS_MANAGER_CONFIG_SET_DEFAULT_CONNECTION_PARAMETERS == 1
      sc = sl_bt_connection_set_default_parameters(CS_MANAGER_CONFIG_DEFAULT_MIN_CONNECTION_INTERVAL,
                                                   CS_MANAGER_CONFIG_DEFAULT_MAX_CONNECTION_INTERVAL,
                                                   CS_MANAGER_CONFIG_DEFAULT_CONNECTION_PERIPHERAL_LATENCY,
                                                   CS_MANAGER_CONFIG_DEFAULT_CONNECTION_TIMEOUT,
                                                   CS_MANAGER_CONFIG_DEFAULT_MIN_CE_LENGTH,
                                                   CS_MANAGER_CONFIG_DEFAULT_MAX_CE_LENGTH);
      if (sc == SL_STATUS_OK) { 
        cs_manager_log_info("Default connection parametersset: min_interval: %u, max_interval: %u, latency: %u, "
                            "timeout: %u, min_ce_length: %u, max_ce_length: %u" APP_LOG_NL,
                            CS_MANAGER_CONFIG_DEFAULT_MIN_CONNECTION_INTERVAL,
                            CS_MANAGER_CONFIG_DEFAULT_MAX_CONNECTION_INTERVAL,
                            CS_MANAGER_CONFIG_DEFAULT_CONNECTION_PERIPHERAL_LATENCY,
                            CS_MANAGER_CONFIG_DEFAULT_CONNECTION_TIMEOUT,
                            CS_MANAGER_CONFIG_DEFAULT_MIN_CE_LENGTH,
                            CS_MANAGER_CONFIG_DEFAULT_MAX_CE_LENGTH);
      } else {
        cs_manager_log_error("Failed to set default connection parameters" APP_LOG_NL);
        on_event(SL_BT_INVALID_CONNECTION_HANDLE,
                 CS_MANAGER_INVALID_CONFIG_ID,
                 CS_MANAGER_EVENT_ERROR, 
                 sc);
      }
      #endif // defined(CS_MANAGER_CONFIG_SET_DEFAULT_CONNECTION_PARAMETERS) && CS_MANAGER_CONFIG_SET_DEFAULT_CONNECTION_PARAMETERS == 1
      break;
    }
     case sl_bt_evt_cs_security_enable_complete_id: {
      cs_manager_t *m = cs_manager_find(evt->data.evt_cs_security_enable_complete.connection);
      if (m == NULL) {
        break;
      }
      cs_manager_log_info(INSTANCE_PREFIX "CS security enabled" NL, m->conn_handle);
      m->security_enabled = true;
      if (m->state == CS_MANAGER_STATE_ENABLING_SECURITY) {
        m->state = CS_MANAGER_STATE_IDLE;
        on_event(m->conn_handle,
                 m->active_config_id,
                 CS_MANAGER_EVENT_INSTANCE_CREATE_COMPLETE, 
                 SL_STATUS_OK);
      } else if (m->state == CS_MANAGER_STATE_PROCEDURE_ENABLING_SECURITY) {
        #ifdef SL_CATALOG_CS_MANAGER_FEATURE_CONTROL_PRESENT
        sc = cs_manager_cs_control_start(m->conn_handle,
                                         m->active_config_id, 
                                         &m->procedure_parameters);
        if (sc != SL_STATUS_OK) {
          cs_manager_log_error(INSTANCE_PREFIX "Failed to start procedure: 0x%04lx" NL,
                               m->conn_handle,
                               sc);
          on_event(m->conn_handle,
                   CS_MANAGER_INVALID_CONFIG_ID,
                   CS_MANAGER_EVENT_PROCEDURE_START_COMPLETE, 
                   sc);
          m->state = CS_MANAGER_STATE_IDLE;
        } else {
          m->state = CS_MANAGER_STATE_PROCEDURE_ENABLING;
        }
        #endif // SL_CATALOG_CS_MANAGER_FEATURE_CONTROL_PRESENT
      }
      break;
    }
    case sl_bt_evt_connection_phy_status_id: {
      cs_manager_t *m = cs_manager_find(evt->data.evt_connection_phy_status.connection);
      if (m == NULL) {
        break;
      }
      // Save connection PHY
      m->instance_config.conn_phy = evt->data.evt_connection_phy_status.phy;
      break;
    }
    case sl_bt_evt_connection_parameters_id: {
      cs_manager_t *m = cs_manager_find(evt->data.evt_connection_parameters.connection);
      if (m == NULL || m->state != CS_MANAGER_STATE_SETTING_CONN_PARAMS) {
        break;
      }
      cs_manager_log_info(INSTANCE_PREFIX "Connection parameters confirmed" NL, m->conn_handle);
      if (m->manage_connection_parameters) {
        const sl_bt_evt_connection_parameters_t *p = &evt->data.evt_connection_parameters;
        bool params_ok =
          (p->interval >= m->connection_parameters.min_connection_interval)
          && (p->interval <= m->connection_parameters.max_connection_interval)
          && (p->latency == m->connection_parameters.latency)
          && (p->timeout == m->connection_parameters.timeout);
        if (!params_ok) {
          cs_manager_log_error(INSTANCE_PREFIX
            "Connection parameters mismatch: "
            "interval=%u (req [%u,%u]), latency=%u (req %u), timeout=%u (req %u)" NL,
            m->conn_handle,
            p->interval,
            m->connection_parameters.min_connection_interval,
            m->connection_parameters.max_connection_interval,
            p->latency, m->connection_parameters.latency,
            p->timeout, m->connection_parameters.timeout);
          on_event(m->conn_handle,
                   CS_MANAGER_INVALID_CONFIG_ID,
                   CS_MANAGER_EVENT_INSTANCE_CREATE_COMPLETE,
                   SL_STATUS_INVALID_PARAMETER);
          m->state = CS_MANAGER_STATE_IDLE;
          break;
        }
      }
      #if defined(CS_MANAGER_CONFIG_ENABLE_CS_SECURITY_BEFORE_CONFIG) && CS_MANAGER_CONFIG_ENABLE_CS_SECURITY_BEFORE_CONFIG == 1
      if (m->instance_config.is_central) {
        sc = sl_bt_cs_security_enable(m->conn_handle);
        if (sc != SL_STATUS_OK) {
          cs_manager_log_error(INSTANCE_PREFIX "Error enabling CS security" NL, m->conn_handle);
          on_event(m->conn_handle, CS_MANAGER_INVALID_CONFIG_ID, CS_MANAGER_EVENT_INSTANCE_CREATE_COMPLETE, sc);
          m->state = CS_MANAGER_STATE_IDLE;
          break;
        }
        cs_manager_log_info(INSTANCE_PREFIX "Enabling CS security before configuration" NL, m->conn_handle);
        m->state = CS_MANAGER_STATE_ENABLING_SECURITY;
        break;
      }
      #endif // defined(CS_MANAGER_CONFIG_ENABLE_CS_SECURITY_BEFORE_CONFIG) && CS_MANAGER_CONFIG_ENABLE_CS_SECURITY_BEFORE_CONFIG == 1
      m->state = CS_MANAGER_STATE_IDLE;
      on_event(m->conn_handle, m->active_config_id, CS_MANAGER_EVENT_INSTANCE_CREATE_COMPLETE, SL_STATUS_OK);
      break;
    }
    case sl_bt_evt_cs_config_complete_id: {
      handle_cs_config_completed(&evt->data.evt_cs_config_complete);
      break;
    }
    default:
      break;
  }
  (void)app_rta_release(cs_manager_ctx);
}

// -----------------------------------------------------------------------------
// Private functions

static bool needs_connection_parameter_update(const cs_manager_connection_parameters_t *params)
{
  if (params == NULL) {
    return false;
  }
  #if !defined(CS_MANAGER_CONFIG_SET_DEFAULT_CONNECTION_PARAMETERS) \
      || CS_MANAGER_CONFIG_SET_DEFAULT_CONNECTION_PARAMETERS == 0
  return true;
  #else
  return (params->min_connection_interval != CS_MANAGER_CONFIG_DEFAULT_MIN_CONNECTION_INTERVAL)
         || (params->max_connection_interval != CS_MANAGER_CONFIG_DEFAULT_MAX_CONNECTION_INTERVAL)
         || (params->latency != CS_MANAGER_CONFIG_DEFAULT_CONNECTION_PERIPHERAL_LATENCY)
         || (params->timeout != CS_MANAGER_CONFIG_DEFAULT_CONNECTION_TIMEOUT)
         || (params->min_ce_length != CS_MANAGER_CONFIG_DEFAULT_MIN_CE_LENGTH)
         || (params->max_ce_length != CS_MANAGER_CONFIG_DEFAULT_MAX_CE_LENGTH);
  #endif
}

static void handle_cs_config_completed(const cs_config_data_t *evt)
{
  cs_manager_t *m = cs_manager_find(evt->connection);
  if (m == NULL) {
    return;
  }

  switch (evt->config_state) {
    case sl_bt_cs_config_state_created:
      handle_cs_config_created(m, evt);
      break;
    case sl_bt_cs_config_state_removed:
      handle_cs_config_removed(m, evt);
      break;
    default:
      break;
  }
}

static void handle_cs_config_created(cs_manager_t *m, const cs_config_data_t *evt)
{
  // The completion event can be the result of a local creation
  // (the instance is in CS_MANAGER_STATE_CONFIGURING), or a
  // remote creation (any other state).
  bool initiated_locally = (m->state == CS_MANAGER_STATE_CONFIGURING);

  if ((sl_status_t)evt->status != SL_STATUS_OK) {
    cs_manager_log_error(INSTANCE_PREFIX "CS config create reported error "
                                         "(config_id=%u, status=0x%x)" NL,
                         evt->connection,
                         (unsigned)evt->config_id,
                         (unsigned)evt->status);
    if (initiated_locally) {
      m->state = CS_MANAGER_STATE_IDLE;
    }
    on_event(evt->connection,
             evt->config_id,
             CS_MANAGER_EVENT_CONFIG_CREATE_COMPLETE,
             (sl_status_t)evt->status);
    return;
  }

  sl_status_t sc = cs_manager_config_db_create(evt);
  if ((sc != SL_STATUS_OK) && (sc != SL_STATUS_ALREADY_EXISTS)) {
    cs_manager_log_error(INSTANCE_PREFIX "Failed to store CS configuration "
                                         "(config_id=%u, sc=0x%lx)" NL,
                         evt->connection,
                         (unsigned)evt->config_id,
                         (unsigned long)sc);
    if (initiated_locally) {
      m->state = CS_MANAGER_STATE_IDLE;
    }
    on_event(evt->connection,
             evt->config_id,
             CS_MANAGER_EVENT_CONFIG_CREATE_COMPLETE,
             sc);
    return;
  }

  if (initiated_locally) {
    m->state = CS_MANAGER_STATE_IDLE;
    cs_manager_log_info(INSTANCE_PREFIX "Local CS configuration created "
                                        "(config_id=%u)" NL,
                        evt->connection,
                        (unsigned)evt->config_id);
  } else {
    cs_manager_log_info(INSTANCE_PREFIX "Remote-initiated CS configuration "
                                        "created (config_id=%u, state=%u)" NL,
                        evt->connection,
                        (unsigned)evt->config_id,
                        (unsigned)m->state);
  }

  on_event(evt->connection,
           evt->config_id,
           (sc == SL_STATUS_ALREADY_EXISTS) ? CS_MANAGER_EVENT_CONFIG_OVERWRITTEN
                                            : CS_MANAGER_EVENT_CONFIG_CREATE_COMPLETE,
           SL_STATUS_OK);
}

static void handle_cs_config_removed(cs_manager_t *m, const cs_config_data_t *evt)
{
  bool initiated_locally = (m->state == CS_MANAGER_STATE_REMOVING_CONFIG);

  if ((sl_status_t)evt->status != SL_STATUS_OK) {
    cs_manager_log_error(INSTANCE_PREFIX "CS config remove reported error "
                                         "(config_id=%u, status=0x%x)" NL,
                         evt->connection,
                         (unsigned)evt->config_id,
                         (unsigned)evt->status);
    if (initiated_locally) {
      m->state = CS_MANAGER_STATE_IDLE;
    }
    on_event(evt->connection,
             evt->config_id,
             CS_MANAGER_EVENT_CONFIG_REMOVE_COMPLETE,
             (sl_status_t)evt->status);
    return;
  }

  // Drop the entry from the local DB (SL_STATUS_NOT_FOUND is treated as
  // success)
  sl_status_t sc = cs_manager_config_db_remove(evt->connection, evt->config_id);
  if ((sc != SL_STATUS_OK) && (sc != SL_STATUS_NOT_FOUND)) {
    cs_manager_log_error(INSTANCE_PREFIX "Failed to drop CS configuration "
                                         "from DB (config_id=%u, sc=0x%lx)" NL,
                         evt->connection,
                         (unsigned)evt->config_id,
                         (unsigned long)sc);
    if (initiated_locally) {
      m->state = CS_MANAGER_STATE_IDLE;
    }
    on_event(evt->connection,
             evt->config_id,
             CS_MANAGER_EVENT_CONFIG_REMOVE_COMPLETE,
             sc);
    return;
  }

  if (initiated_locally) {
    m->state = CS_MANAGER_STATE_IDLE;
    cs_manager_log_info(INSTANCE_PREFIX "Local CS configuration removed "
                                        "(config_id=%u)" NL,
                        evt->connection,
                        (unsigned)evt->config_id);
  } else {
    cs_manager_log_info(INSTANCE_PREFIX "Remote-initiated CS configuration "
                                        "removed (config_id=%u, state=%u)" NL,
                        evt->connection,
                        (unsigned)evt->config_id,
                        (unsigned)m->state);
  }

  on_event(evt->connection,
           evt->config_id,
           CS_MANAGER_EVENT_CONFIG_REMOVE_COMPLETE,
           SL_STATUS_OK);
}

// -----------------------------------------------------------------------------
// Internal error helper

void cs_manager_error(cs_manager_t *m,
                      cs_manager_error_t evt,
                      sl_status_t sc)
{
  uint8_t conn_handle = SL_BT_INVALID_CONNECTION_HANDLE;
  if (m != NULL) {
    conn_handle = m->conn_handle;
  }
  cs_manager_log_error(INSTANCE_PREFIX "Error occurred (evt: %u, sc: 0x%lx)" NL,
                       conn_handle,
                       (unsigned)evt,
                       (unsigned long)sc);
  if (on_error != NULL) {
    on_error(conn_handle, evt, sc);
  }
}

// -----------------------------------------------------------------------------
// RTA lifecycle functions

void cs_manager_rta_init(void)
{
  app_rta_config_t config = {
    .requirement.runtime = false,
    .requirement.guard   = true,
    .requirement.signal  = false,
    .step                = NULL,
    .priority            = 0,
    .stack_size          = 0,
    .error               = on_runtime_error,
    .wait_for_guard      = CS_MANAGER_CONFIG_RTA_WAIT_FOR_GUARD
  };
  sl_status_t sc = app_rta_create_context(&config, &cs_manager_ctx);
  if (sc != SL_STATUS_OK) {
    cs_manager_log_error("Failed to create RTA context, sc=0x%lx" APP_LOG_NL, sc);
  }
}

static void on_runtime_error(app_rta_error_t error, sl_status_t result)
{
  (void)result;
  cs_manager_error_t evt;
  switch (error) {
    case APP_RTA_ERROR_RUNTIME_INIT_FAILED:
      cs_manager_log_error("RTA runtime init failed, sc=0x%lx" APP_LOG_NL, result);
      evt = CS_MANAGER_ERROR_RTA_INIT_FAILED;
      break;
    case APP_RTA_ERROR_ACQUIRE_FAILED:
      cs_manager_log_error("RTA acquire failed, sc=0x%lx" APP_LOG_NL, result);
      evt = CS_MANAGER_ERROR_RTA_ACQUIRE_FAILED;
      break;
    case APP_RTA_ERROR_RELEASE_FAILED:
      cs_manager_log_error("RTA release failed, sc=0x%lx" APP_LOG_NL, result);
      evt = CS_MANAGER_ERROR_RTA_RELEASE_FAILED;
      break;
    default:
      evt = CS_MANAGER_ERROR_RUNTIME_ERROR;
      break;
  }
  cs_manager_error(NULL, evt, result);
}
