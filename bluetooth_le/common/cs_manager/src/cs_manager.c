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
#include <inttypes.h>
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
static bool needs_connection_parameter_update(const cs_manager_connection_parameters_t *params);
static void on_runtime_error(app_rta_error_t error, sl_status_t result);
static void handle_cs_procedure_enable_completed(const sl_bt_evt_cs_procedure_enable_complete_t *evt);
static void send_instance_create_failed(cs_manager_t *m, sl_status_t status);
static void enable_security(cs_manager_t *m);
static sl_status_t update_phy(cs_manager_t *m);

// -----------------------------------------------------------------------------
// Static variables

// Instance storage
static cs_manager_t cs_manager_instances[CS_MANAGER_CONFIG_MAX_INSTANCES];

// Active default connection parameters. Initialized from the configuration
// macros and overridable at runtime via
// cs_manager_set_default_connection_parameters(). Used both for the boot-time
// sl_bt_connection_set_default_parameters() call and as the baseline that
// needs_connection_parameter_update() compares against, so connections opened
// with matching parameters skip the per-connection update.
cs_manager_connection_parameters_t default_connection_parameters = {
  .min_connection_interval = CS_MANAGER_CONFIG_DEFAULT_MIN_CONNECTION_INTERVAL,
  .max_connection_interval = CS_MANAGER_CONFIG_DEFAULT_MAX_CONNECTION_INTERVAL,
  .latency                 = CS_MANAGER_CONFIG_DEFAULT_CONNECTION_PERIPHERAL_LATENCY,
  .timeout                 = CS_MANAGER_CONFIG_DEFAULT_CONNECTION_TIMEOUT,
  .min_ce_length           = CS_MANAGER_CONFIG_DEFAULT_MIN_CE_LENGTH,
  .max_ce_length           = CS_MANAGER_CONFIG_DEFAULT_MAX_CE_LENGTH,
};

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
  // Check existence
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
    cs_manager_log_debug(INSTANCE_PREFIX
                         "Instructed to manage connection parameters. "
                         "Min. conn. interval: %u, "
                         "Max. conn. interval: %u, "
                         "Latency: %u, "
                         "Timeout: %u, "
                         "Min. CE length: %u, "
                         "Max. CE length: %u" NL,
                         conn_handle,
                         connection_parameters->min_connection_interval,
                         connection_parameters->max_connection_interval,
                         connection_parameters->latency,
                         connection_parameters->timeout,
                         connection_parameters->min_ce_length,
                         connection_parameters->max_ce_length);
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

  sc = update_phy(m);
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
  instance_config->actual_conn_phy = CS_MANAGER_CONFIG_DEFAULT_CONN_PHY;
  instance_config->request_conn_phy = CS_MANAGER_CONFIG_REQUEST_CONN_PHY;
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
  *params = default_connection_parameters;
  return SL_STATUS_OK;
}

sl_status_t cs_manager_set_default_connection_parameters(const cs_manager_connection_parameters_t *params)
{
  if (params == NULL) {
    return SL_STATUS_NULL_POINTER;
  }
  sl_status_t sc = app_rta_acquire(cs_manager_ctx);
  if (sc != SL_STATUS_OK) {
    return sc;
  }
  default_connection_parameters = *params;
  sc = sl_bt_connection_set_default_parameters(default_connection_parameters.min_connection_interval,
                                               default_connection_parameters.max_connection_interval,
                                               default_connection_parameters.latency,
                                               default_connection_parameters.timeout,
                                               default_connection_parameters.min_ce_length,
                                               default_connection_parameters.max_ce_length);
  if (sc == SL_STATUS_OK) {
    cs_manager_log_info("Default connection parameters updated: min_interval: %u, max_interval: %u, "
                        "latency: %u, timeout: %u, min_ce_length: %u, max_ce_length: %u" NL,
                        default_connection_parameters.min_connection_interval,
                        default_connection_parameters.max_connection_interval,
                        default_connection_parameters.latency,
                        default_connection_parameters.timeout,
                        default_connection_parameters.min_ce_length,
                        default_connection_parameters.max_ce_length);
  } else {
    cs_manager_log_error("Failed to update default connection parameters" NL);
  }
  (void)app_rta_release(cs_manager_ctx);
  return sc;
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

sl_status_t cs_manager_procedure_parameter_info_get(uint8_t conn_handle,
                                                    cs_procedure_parameter_info_t *info)
{
  if (info == NULL) {
    return SL_STATUS_NULL_POINTER;
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
  if (m->state != CS_MANAGER_STATE_PROCEDURE_ENABLED) {
    (void)app_rta_release(cs_manager_ctx);
    return SL_STATUS_INVALID_STATE;
  }
  memcpy(info, &m->procedure_parameter_info, sizeof(m->procedure_parameter_info));
  (void)app_rta_release(cs_manager_ctx);
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
        cs_manager_log_error("Failed to set system TX power" NL);
        on_event(SL_BT_INVALID_CONNECTION_HANDLE,
                 CS_MANAGER_INVALID_CONFIG_ID,
                 CS_MANAGER_EVENT_ERROR,
                 sc);
      }
      #if defined(CS_MANAGER_CONFIG_SET_DEFAULT_CONNECTION_PARAMETERS) && CS_MANAGER_CONFIG_SET_DEFAULT_CONNECTION_PARAMETERS == 1
      sc = sl_bt_connection_set_default_parameters(default_connection_parameters.min_connection_interval,
                                                   default_connection_parameters.max_connection_interval,
                                                   default_connection_parameters.latency,
                                                   default_connection_parameters.timeout,
                                                   default_connection_parameters.min_ce_length,
                                                   default_connection_parameters.max_ce_length);
      if (sc == SL_STATUS_OK) {
        cs_manager_log_info("Default connection parameters set: min_interval: %u, max_interval: %u, latency: %u, "
                            "timeout: %u, min_ce_length: %u, max_ce_length: %u" NL,
                            default_connection_parameters.min_connection_interval,
                            default_connection_parameters.max_connection_interval,
                            default_connection_parameters.latency,
                            default_connection_parameters.timeout,
                            default_connection_parameters.min_ce_length,
                            default_connection_parameters.max_ce_length);
      } else {
        cs_manager_log_error("Failed to set default connection parameters" NL);
        on_event(SL_BT_INVALID_CONNECTION_HANDLE,
                 CS_MANAGER_INVALID_CONFIG_ID,
                 CS_MANAGER_EVENT_ERROR,
                 sc);
      }
      #endif // defined(CS_MANAGER_CONFIG_SET_DEFAULT_CONNECTION_PARAMETERS) && CS_MANAGER_CONFIG_SET_DEFAULT_CONNECTION_PARAMETERS == 1
      #if defined(CS_MANAGER_CONFIG_SET_DEFAULT_CONN_PHY) && CS_MANAGER_CONFIG_SET_DEFAULT_CONN_PHY == 1
      sc = sl_bt_connection_set_default_preferred_phy(CS_MANAGER_CONFIG_DEFAULT_CONN_PHY,
                                                      sl_bt_gap_phy_any);
      if (sc == SL_STATUS_OK) {
        cs_manager_log_info("Default connection PHY set: %u" NL,
                            (unsigned)CS_MANAGER_CONFIG_DEFAULT_CONN_PHY);
      } else {
        cs_manager_log_error("Failed to set default connection PHY" NL);
        on_event(SL_BT_INVALID_CONNECTION_HANDLE,
                 CS_MANAGER_INVALID_CONFIG_ID,
                 CS_MANAGER_EVENT_ERROR,
                 sc);
      }
      #endif // defined(CS_MANAGER_CONFIG_SET_DEFAULT_CONN_PHY) && CS_MANAGER_CONFIG_SET_DEFAULT_CONN_PHY == 1
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
          cs_manager_log_error(INSTANCE_PREFIX "Failed to start procedure: 0x%" PRIx32 NL,
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
      m->instance_config.actual_conn_phy = evt->data.evt_connection_phy_status.phy;
      if (m->state == CS_MANAGER_STATE_SETTING_CONN_PHY) {
        if (m->instance_config.actual_conn_phy != m->instance_config.conn_phy) {
          cs_manager_log_error(INSTANCE_PREFIX
                               "Connection PHY mismatch: actual=%u (requested %u)" NL,
                               m->conn_handle,
                               m->instance_config.actual_conn_phy,
                               m->instance_config.conn_phy);
          send_instance_create_failed(m, SL_STATUS_INVALID_PARAMETER);
        } else {
          cs_manager_log_info(INSTANCE_PREFIX "Connection PHY %u confirmed" NL,
                              m->conn_handle,
                              m->instance_config.actual_conn_phy);
          enable_security(m);
        }
      }
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
          send_instance_create_failed(m, SL_STATUS_INVALID_PARAMETER);
          break;
        }
      }
      (void)update_phy(m);
      break;
    }
    case sl_bt_evt_cs_config_complete_id: {
      handle_cs_config_completed(&evt->data.evt_cs_config_complete);
      break;
    }
    case sl_bt_evt_cs_procedure_enable_complete_id: {
      handle_cs_procedure_enable_completed(&evt->data.evt_cs_procedure_enable_complete);
      break;
    }
    default:
      break;
  }
  (void)app_rta_release(cs_manager_ctx);
}

// -----------------------------------------------------------------------------
// Private functions

static void send_instance_create_failed(cs_manager_t *m, sl_status_t status)
{
  uint8_t conn_handle = m->conn_handle;

  on_event(conn_handle,
           CS_MANAGER_INVALID_CONFIG_ID,
           CS_MANAGER_EVENT_INSTANCE_CREATE_COMPLETE,
           status);
  m->conn_handle = SL_BT_INVALID_CONNECTION_HANDLE;
}

static void enable_security(cs_manager_t *m)
{
  #if defined(CS_MANAGER_CONFIG_ENABLE_CS_SECURITY_BEFORE_CONFIG) && CS_MANAGER_CONFIG_ENABLE_CS_SECURITY_BEFORE_CONFIG == 1
  if (m->instance_config.is_central) {
    sl_status_t sc = sl_bt_cs_security_enable(m->conn_handle);
    if (sc != SL_STATUS_OK) {
      cs_manager_log_error(INSTANCE_PREFIX "Error enabling CS security" NL,
                           m->conn_handle);
      send_instance_create_failed(m, sc);
      return;
    }
    cs_manager_log_info(INSTANCE_PREFIX "Enabling CS security before configuration" NL,
                        m->conn_handle);
    m->state = CS_MANAGER_STATE_ENABLING_SECURITY;
    return;
  }
  #endif // defined(CS_MANAGER_CONFIG_ENABLE_CS_SECURITY_BEFORE_CONFIG) && CS_MANAGER_CONFIG_ENABLE_CS_SECURITY_BEFORE_CONFIG == 1

  m->state = CS_MANAGER_STATE_IDLE;
  on_event(m->conn_handle,
           m->active_config_id,
           CS_MANAGER_EVENT_INSTANCE_CREATE_COMPLETE,
           SL_STATUS_OK);
}

static sl_status_t update_phy(cs_manager_t *m)
{
  sl_status_t sc;

  if (m->instance_config.conn_phy == m->instance_config.actual_conn_phy) {
    cs_manager_log_info(INSTANCE_PREFIX "Connection PHY %u matches requested" NL,
                        m->conn_handle,
                        m->instance_config.actual_conn_phy);
    enable_security(m);
    return SL_STATUS_OK;
  }

  if (m->instance_config.request_conn_phy == 0u) {
    cs_manager_log_warning(INSTANCE_PREFIX "Connection PHY mismatch: actual=%u (requested %u), "
                                           "but not requested to update" NL,
                           m->conn_handle,
                           m->instance_config.actual_conn_phy,
                           m->instance_config.conn_phy);
    enable_security(m);
    return SL_STATUS_OK;
  }

  sc = sl_bt_connection_set_preferred_phy(m->conn_handle,
                                          m->instance_config.conn_phy,
                                          sl_bt_gap_phy_any);
  if (sc != SL_STATUS_OK) {
    cs_manager_log_error(INSTANCE_PREFIX "Error setting connection PHY" NL,
                         m->conn_handle);
    send_instance_create_failed(m, sc);
    return sc;
  }

  cs_manager_log_info(INSTANCE_PREFIX "Setting connection PHY to %u" NL,
                      m->conn_handle,
                      m->instance_config.conn_phy);
  m->state = CS_MANAGER_STATE_SETTING_CONN_PHY;
  return SL_STATUS_OK;
}

static bool needs_connection_parameter_update(const cs_manager_connection_parameters_t *params)
{
  if (params == NULL) {
    return false;
  }
  #if !defined(CS_MANAGER_CONFIG_SET_DEFAULT_CONNECTION_PARAMETERS) \
  || CS_MANAGER_CONFIG_SET_DEFAULT_CONNECTION_PARAMETERS == 0
  return true;
  #else
  // Compare against the active default connection parameters (which may have
  // been overridden at runtime), so connections opened with matching
  // parameters skip the per-connection update.
  return (params->min_connection_interval != default_connection_parameters.min_connection_interval)
         || (params->max_connection_interval != default_connection_parameters.max_connection_interval)
         || (params->latency != default_connection_parameters.latency)
         || (params->timeout != default_connection_parameters.timeout)
         || (params->min_ce_length != default_connection_parameters.min_ce_length)
         || (params->max_ce_length != default_connection_parameters.max_ce_length);
  #endif
}

static void handle_cs_config_completed(const cs_config_data_t *evt)
{
  cs_manager_t *m = cs_manager_find(evt->connection);
  if (m == NULL) {
    return;
  }
  bool initiated_locally = (m->state == CS_MANAGER_STATE_CONFIGURING)
                           || (m->state == CS_MANAGER_STATE_REMOVING_CONFIG);
  // Operation state
  bool operation_successful = ((sl_status_t)evt->status == SL_STATUS_OK);
  //  State
  bool state_created = (evt->config_state == sl_bt_cs_config_state_created);
  bool state_removed = (evt->config_state == sl_bt_cs_config_state_removed);

  if (!operation_successful) {
    cs_manager_log_error(INSTANCE_PREFIX "%s requested CS config %s failed "
                                         "(config_id=%u, status=0x%x)" NL,
                         evt->connection,
                         initiated_locally ? "Local" : "Remote",
                         state_removed ? "create" : "remove",
                         (unsigned)evt->config_id,
                         (unsigned)evt->status);
    if (initiated_locally) {
      m->state = CS_MANAGER_STATE_IDLE;
    }
    on_event(evt->connection,
             evt->config_id,
             state_removed
             ? CS_MANAGER_EVENT_CONFIG_CREATE_COMPLETE
             : CS_MANAGER_EVENT_CONFIG_REMOVE_COMPLETE,
             (sl_status_t)evt->status);
    return;
  }
  if (initiated_locally) {
    m->state = CS_MANAGER_STATE_IDLE;
  }
  sl_status_t sc;
  bool db_success = true;
  if (state_created) {
    sc = cs_manager_config_db_create(evt);
    if ((sc != SL_STATUS_OK) && (sc != SL_STATUS_ALREADY_EXISTS)) {
      db_success = false;
    }
  } else {
    sc = cs_manager_config_db_remove(evt->connection, evt->config_id);
    if (sc != SL_STATUS_OK && (sc != SL_STATUS_NOT_FOUND)) {
      db_success = false;
    }
  }
  if (!db_success) {
    cs_manager_log_error(INSTANCE_PREFIX "%s requested CS config %s failed (db error) "
                                         "(config_id=%u, sc=0x%lx)" NL,
                         evt->connection,
                         initiated_locally ? "Local" : "Remote",
                         state_created ? "create" : "remove",
                         (unsigned)evt->config_id,
                         (unsigned long)sc);
  } else {
    cs_manager_log_info(INSTANCE_PREFIX "%s requested CS config %s succeeded "
                                        "(config_id=%u)" NL,
                        evt->connection,
                        initiated_locally ? "Local" : "Remote",
                        state_created ? "create" : "remove",
                        (unsigned)evt->config_id);
  }
  cs_manager_event_type_t evt_type = CS_MANAGER_EVENT_CONFIG_REMOVE_COMPLETE;
  if (state_created) {
    evt_type = ((sc == SL_STATUS_ALREADY_EXISTS)
                ? CS_MANAGER_EVENT_CONFIG_OVERWRITTEN
                : CS_MANAGER_EVENT_CONFIG_CREATE_COMPLETE);
  }
  on_event(evt->connection,
           evt->config_id,
           evt_type,
           sc);
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
    cs_manager_log_error("Failed to create RTA context, sc=0x%lx" APP_LOG_NL, (unsigned long)sc);
  }
}

static void on_runtime_error(app_rta_error_t error, sl_status_t result)
{
  (void)result;
  cs_manager_error_t evt;
  switch (error) {
    case APP_RTA_ERROR_RUNTIME_INIT_FAILED:
      cs_manager_log_error("RTA runtime init failed, sc=0x%lx" APP_LOG_NL, (unsigned long)result);
      evt = CS_MANAGER_ERROR_RTA_INIT_FAILED;
      break;
    case APP_RTA_ERROR_ACQUIRE_FAILED:
      cs_manager_log_error("RTA acquire failed, sc=0x%lx" APP_LOG_NL, (unsigned long)result);
      evt = CS_MANAGER_ERROR_RTA_ACQUIRE_FAILED;
      break;
    case APP_RTA_ERROR_RELEASE_FAILED:
      cs_manager_log_error("RTA release failed, sc=0x%lx" APP_LOG_NL, (unsigned long)result);
      evt = CS_MANAGER_ERROR_RTA_RELEASE_FAILED;
      break;
    default:
      evt = CS_MANAGER_ERROR_RUNTIME_ERROR;
      break;
  }
  cs_manager_error(NULL, evt, result);
}

static void handle_cs_procedure_enable_completed(const sl_bt_evt_cs_procedure_enable_complete_t *evt)
{
  cs_manager_t *m = cs_manager_find(evt->connection);
  if (m == NULL) {
    cs_manager_log_error("Procedure enable completed for unknown connection %u" NL,
                         evt->connection);
    return;
  }
  cs_manager_log_debug(INSTANCE_PREFIX "CS procedure enable complete: "
                                       "config_id: %u, status: 0x%04x, state: %s, "
                                       "antenna_config: %u, tx_power: %d, subevent_len: %" PRIu32 ", "
                                                                                                  "subevents_per_event: %u, subevent_interval: %u, "
                                                                                                  "event_interval: %u, procedure_interval: %u, "
                                                                                                  "procedure_count: %u, max_procedure_len: %u" NL,
                       m->conn_handle,
                       evt->config_id,
                       evt->status,
                       (evt->state == sl_bt_cs_procedure_state_enabled) ? "enabled" : "disabled",
                       evt->antenna_config,
                       (int)evt->tx_power,
                       evt->subevent_len,
                       evt->subevents_per_event,
                       evt->subevent_interval,
                       evt->event_interval,
                       evt->procedure_interval,
                       evt->procedure_count,
                       evt->max_procedure_len);
  if (evt->status != SL_STATUS_OK) {
    if (evt->status == SL_STATUS_BT_CTRL_REMOTE_USER_TERMINATED) {
      cs_manager_log_warning(INSTANCE_PREFIX "CS procedure terminated, status = 0x%04x, state = %s" NL,
                             m->conn_handle,
                             evt->status,
                             (evt->state == sl_bt_cs_procedure_state_enabled) ? "enabled" : "disabled");
    } else {
      cs_manager_log_error(INSTANCE_PREFIX "CS procedure control failed, status = 0x%04x, state = %s" NL,
                           m->conn_handle,
                           evt->status,
                           (evt->state == sl_bt_cs_procedure_state_enabled) ? "enabled" : "disabled");
    }
    if (m->state == CS_MANAGER_STATE_PROCEDURE_ENABLING) {
      m->state = CS_MANAGER_STATE_IDLE;
      on_event(m->conn_handle,
               m->active_config_id,
               CS_MANAGER_EVENT_PROCEDURE_START_COMPLETE,
               evt->status);
    } else if (m->state == CS_MANAGER_STATE_PROCEDURE_DISABLING) {
      m->state = CS_MANAGER_STATE_IDLE;
      on_event(m->conn_handle,
               m->active_config_id,
               CS_MANAGER_EVENT_PROCEDURE_STOP_COMPLETE,
               evt->status);
    }
    return;
  }
  if (evt->state == sl_bt_cs_procedure_state_enabled) {
    m->state = CS_MANAGER_STATE_PROCEDURE_ENABLED;
    m->procedure_parameter_info.antenna_config = evt->antenna_config;
    m->procedure_parameter_info.tx_power = evt->tx_power;
    m->procedure_parameter_info.subevent_len = evt->subevent_len;
    m->procedure_parameter_info.subevents_per_event = evt->subevents_per_event;
    m->procedure_parameter_info.subevent_interval = evt->subevent_interval;
    m->procedure_parameter_info.event_interval = evt->event_interval;
    m->procedure_parameter_info.procedure_interval = evt->procedure_interval;
    m->procedure_parameter_info.procedure_count = evt->procedure_count;
    m->procedure_parameter_info.max_procedure_len = evt->max_procedure_len;
    cs_manager_log_info(INSTANCE_PREFIX " CS procedure enabled. ACI: %u" NL,
                        m->conn_handle,
                        evt->antenna_config);
    on_event(m->conn_handle,
             m->active_config_id,
             CS_MANAGER_EVENT_PROCEDURE_START_COMPLETE,
             SL_STATUS_OK);
  } else if (evt->state == sl_bt_cs_procedure_state_disabled) {
    m->state = CS_MANAGER_STATE_IDLE;
    cs_manager_log_info(INSTANCE_PREFIX " CS procedure disabled" NL,
                        m->conn_handle);
    on_event(m->conn_handle,
             m->active_config_id,
             CS_MANAGER_EVENT_PROCEDURE_STOP_COMPLETE,
             SL_STATUS_OK);
  }
}
