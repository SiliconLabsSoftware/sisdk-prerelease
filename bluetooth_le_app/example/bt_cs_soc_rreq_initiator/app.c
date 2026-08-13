/***************************************************************************//**
 * @file
 * @brief CS SoC RREQ initiator example application logic
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
#include <stdint.h>
#include <stdbool.h>
#include <math.h>
#include "app_config.h"
#include "sl_bluetooth.h"
#include "sl_component_catalog.h"
#include "app_assert.h"

// app content
#include "sl_main_init.h"
#include "app.h"
#include "trace.h"
#include "app_config.h"
#include "app_timer.h"
#include "app_cs_discovery.h"

// CS content
#include "cs_result.h"
#include "cs_algo.h"
#include "cs_result_config.h"
#include "cs_common.h"
#include "cs_manager_config.h"
#include "cs_rreq_display_core.h"
#include "cs_rreq_display.h"
#include "cs_antenna.h"

// other required content
#include "sl_bt_peer_manager_central.h"
#include "sl_bt_peer_manager_filter.h"
#include "sl_clock_manager.h"

// TODO: refactor CLI
#ifdef SL_CATALOG_CS_INITIATOR_CLI_PRESENT
#include "cs_initiator_cli.h"
#endif // SL_CATALOG_CS_INITIATOR_CLI_PRESENT

// Security
#include "sl_bt_peer_security.h"
#include "app_button_press.h"

// -----------------------------------------------------------------------------
// Definitions

#define DISPLAY_REFRESH_RATE_MS             1000u

// -----------------------------------------------------------------------------
// Static function declarations

static sl_status_t save_connection(uint8_t conn_handle);
static void app_timer_callback(app_timer_t *timer, void *data);
static void on_ras_discovery_complete(const app_cs_discovery_result_t *result);
static void try_start_scanning(void);

// -----------------------------------------------------------------------------
// Private variables

static uint8_t num_reflector_connections = 0u;
// Instances
static initiator_instance_t cs_initiator_instances[CS_MANAGER_CONFIG_MAX_INSTANCES];
// Timer instance
static app_timer_t display_timer;
static app_cs_discovery_result_t discovery_config = {
  .conn_handle = SL_BT_INVALID_CONNECTION_HANDLE,
  .service_handle = CS_RAS_INVALID_SERVICE_HANDLE,
  .gattdb_handles = { .array = { CS_RAS_INVALID_CHARACTERISTIC_HANDLE } },
  .status = SL_STATUS_FAIL
};
static uint8_t connection_setup_handle = SL_BT_INVALID_CONNECTION_HANDLE;

// -----------------------------------------------------------------------------
// Public functions

/******************************************************************************
 * Application Init
 *****************************************************************************/
void app_init(void)
{
  sl_status_t sc = SL_STATUS_OK;
  trace_init();

  // initialize initiator instances
  for (uint32_t i = 0u; i < CS_MANAGER_CONFIG_MAX_INSTANCES; i++) {
    cs_initiator_instances[i].conn_handle = SL_BT_INVALID_CONNECTION_HANDLE;
    cs_initiator_instances[i].measurement_cnt = 0u;
    cs_initiator_instances[i].ranging_counter = 0u;
    memset(&cs_initiator_instances[i].measurement_mainmode, 0u, sizeof(cs_measurement_data_t));
    memset(&cs_initiator_instances[i].measurement_submode, 0u, sizeof(cs_measurement_data_t));
    memset(&cs_initiator_instances[i].measurement_progress, 0u, sizeof(cs_intermediate_result_t));
    cs_initiator_instances[i].measurement_arrived = false;
    cs_initiator_instances[i].measurement_progress_changed = false;
    cs_initiator_instances[i].ras_discovery = false;
    cs_initiator_instances[i].read_capabilities = false;
    cs_initiator_instances[i].security_increase = false;
    cs_initiator_instances[i].number_of_measurements = 0u;
  }

  cs_algo_event_callback_t algo_cb = {
    .on_result = app_on_result,
    .on_intermediate_result = app_on_intermediate_result,
    .on_error = app_on_cs_algo_on_error,
  };

  // Set callback for CS Manager, RREQ and CS Algo
  app_cs_set_callbacks(algo_cb);

  // Get default configuration for CS Manager and CS Algo
  app_cs_get_default_config();

  log_info("+-[CS initiator by Silicon Labs]--------------------------+" NL);
  log_info("+---------------------------------------------------------+" NL);
  // Log default parameters
  app_cs_log_default_config();

  sc = cs_rreq_display_init();
  app_assert_status_f(sc, "cs_rreq_display_init failed");
  cs_rreq_display_set_measurement_mode(app_cs_get_main_mode(),
                                       app_cs_get_algo_mode());
  sc = app_timer_start(&display_timer,
                       DISPLAY_REFRESH_RATE_MS,
                       app_timer_callback,
                       NULL,
                       true);
  if (sc != SL_STATUS_OK) {
    cs_on_error(SL_BT_INVALID_CONNECTION_HANDLE,
                CS_APP_ERROR_TIMER_START_FAILED,
                sc);
  }

  /////////////////////////////////////////////////////////////////////////////
  // Put your additional application init code here!                         //
  // This is called once during start-up.                                    //
  /////////////////////////////////////////////////////////////////////////////
}

/******************************************************************************
 * Application Process Action
 *****************************************************************************/
void app_process_action(void)
{
  if (false == app_is_process_required()) {
    return;
  }

  for (uint8_t i = 0u; i < CS_MANAGER_CONFIG_MAX_INSTANCES; i++) {
    if (cs_initiator_instances[i].measurement_arrived) {
      cs_initiator_instances[i].measurement_arrived = false;
      app_print_head_and_data(&cs_initiator_instances[i]);
      cs_rreq_display_update_data(i,
                                  cs_initiator_instances[i].conn_handle,
                                  CS_RREQ_DISPLAY_STATUS_CONNECTED,
                                  cs_initiator_instances[i].measurement_mainmode.distance_filtered,
                                  cs_initiator_instances[i].measurement_mainmode.distance_estimate_rssi,
                                  cs_initiator_instances[i].measurement_mainmode.likeliness,
                                  cs_initiator_instances[i].measurement_mainmode.bit_error_rate,
                                  cs_initiator_instances[i].measurement_mainmode.distance_raw,
                                  cs_initiator_instances[i].measurement_progress.progress_percentage,
                                  app_cs_get_algo_mode(),
                                  app_cs_get_main_mode());
    } else if (cs_initiator_instances[i].measurement_progress_changed) {
      // write measurement progress to the display without changing the last valid
      // measurement results
      cs_initiator_instances[i].measurement_progress_changed = false;
      log_info(APP_INSTANCE_PREFIX "# %04lu ---" NL,
               cs_initiator_instances[i].measurement_progress.connection,
               cs_initiator_instances[i].measurement_cnt);

      log_info(APP_INSTANCE_PREFIX "Estimation in progress: %3u.%02u %%" NL,
               cs_initiator_instances[i].measurement_progress.connection,
               ((uint8_t)cs_initiator_instances[i].measurement_progress.progress_percentage),
               (uint16_t)((uint32_t)(cs_initiator_instances[i].measurement_progress.progress_percentage * 100.f)) % 100);

      cs_rreq_display_update_data(i,
                                  cs_initiator_instances[i].conn_handle,
                                  CS_RREQ_DISPLAY_STATUS_CONNECTED,
                                  cs_initiator_instances[i].measurement_mainmode.distance_filtered,
                                  cs_initiator_instances[i].measurement_mainmode.distance_estimate_rssi,
                                  cs_initiator_instances[i].measurement_mainmode.likeliness,
                                  cs_initiator_instances[i].measurement_mainmode.bit_error_rate,
                                  cs_initiator_instances[i].measurement_mainmode.distance_raw,
                                  cs_initiator_instances[i].measurement_progress.progress_percentage,
                                  app_cs_get_algo_mode(),
                                  app_cs_get_main_mode());
    }
  }
  /////////////////////////////////////////////////////////////////////////////
  // Put your additional application code here!                              //
  // This is called whenever app_proceed() has been called.                  //
  // Do not call blocking functions from here!                               //
  /////////////////////////////////////////////////////////////////////////////
}

void app_decrement_reflector_connections(void)
{
  num_reflector_connections--;
}

void app_increment_reflector_connections(void)
{
  num_reflector_connections++;
}

void app_clear_instance_data(uint8_t conn_handle)
{
  // Clean application data
  for (uint32_t i = 0u; i < CS_MANAGER_CONFIG_MAX_INSTANCES; i++) {
    if (cs_initiator_instances[i].conn_handle == conn_handle) {
      cs_initiator_instances[i].conn_handle = SL_BT_INVALID_CONNECTION_HANDLE;
      cs_initiator_instances[i].measurement_cnt = 0u;
      memset(&cs_initiator_instances[i].measurement_mainmode, 0u, sizeof(cs_measurement_data_t));
      memset(&cs_initiator_instances[i].measurement_submode, 0u, sizeof(cs_measurement_data_t));
      memset(&cs_initiator_instances[i].measurement_progress, 0u, sizeof(cs_intermediate_result_t));
      cs_initiator_instances[i].measurement_arrived = false;
      cs_initiator_instances[i].measurement_progress_changed = false;
      cs_initiator_instances[i].ras_discovery = false;
      cs_initiator_instances[i].read_capabilities = false;
      cs_initiator_instances[i].security_increase = false;
      break;
    }
  }
}

initiator_instance_t *app_get_instance(uint8_t conn_handle)
{
  for (uint8_t i = 0u; i < CS_MANAGER_CONFIG_MAX_INSTANCES; i++) {
    if (cs_initiator_instances[i].conn_handle == conn_handle) {
      return &cs_initiator_instances[i];
    }
  }
  return NULL;
}

void app_on_cs_setup_complete(void)
{
  connection_setup_handle = SL_BT_INVALID_CONNECTION_HANDLE;
  // Scan for new reflector connections if we have room for more
  if (num_reflector_connections < CS_MANAGER_CONFIG_MAX_INSTANCES) {
    try_start_scanning();
  }
}

// -----------------------------------------------------------------------------
// Private functions

static void try_start_scanning(void)
{
  if (connection_setup_handle != SL_BT_INVALID_CONNECTION_HANDLE) {
    return;
  }
  sl_status_t sc = sl_bt_peer_manager_central_create_connection();
  app_assert_status(sc);
  cs_rreq_display_start_scanning();
  log_info(APP_PREFIX "Scanning started for reflector connections..." NL);
}

static void app_timer_callback(app_timer_t *timer, void *data)
{
  (void)timer;
  (void)data;
  cs_rreq_display_update();
}

/******************************************************************************
 * Save connection
 *****************************************************************************/
static sl_status_t save_connection(uint8_t conn_handle)
{
  for (uint8_t i = 0u; i < CS_MANAGER_CONFIG_MAX_INSTANCES; i++) {
    if (cs_initiator_instances[i].conn_handle
        == SL_BT_INVALID_CONNECTION_HANDLE) {
      cs_initiator_instances[i].conn_handle = conn_handle;
      return SL_STATUS_OK;
    }
  }
  return SL_STATUS_FULL;
}

static void on_ras_discovery_complete(const app_cs_discovery_result_t *result)
{
  if (result == NULL) {
    log_error("RAS discovery failed! [result is NULL]" NL);
    connection_setup_handle = SL_BT_INVALID_CONNECTION_HANDLE;
    return;
  }

  if (result->status != SL_STATUS_OK) {
    log_error(APP_INSTANCE_PREFIX "RAS discovery failed! [sc: 0x%lx]" NL,
              result->conn_handle,
              (unsigned long)result->status);
    cs_on_error(result->conn_handle, CS_APP_ERROR_RAS_DISCOVERY_FAILED,
                result->status);
    return;
  }
  log_info(APP_INSTANCE_PREFIX "RAS discovery complete" NL,
           result->conn_handle);
  //Save the config for rreq settings
  memcpy(&discovery_config, result, sizeof(app_cs_discovery_result_t));

  // Discovery done — now safe to read remote CS capabilities.
  initiator_instance_t *initiator = app_get_instance(result->conn_handle);
  if (initiator == NULL) {
    log_error(APP_INSTANCE_PREFIX "Failed to get instance for connection after discovery!" NL,
              result->conn_handle);
    cs_on_error(result->conn_handle,
                CS_APP_ERROR_INITIATOR_INSTANCE_NOT_FOUND,
                SL_STATUS_NOT_FOUND);
    return;
  }
  sl_status_t sc = sl_bt_cs_read_remote_supported_capabilities(result->conn_handle);
  if (sc != SL_STATUS_OK) {
    log_error(APP_INSTANCE_PREFIX "Failed to read remote CS capabilities! [sc: 0x%lx]" NL,
              result->conn_handle,
              (unsigned long)sc);
    cs_on_error(result->conn_handle,
                CS_APP_ERROR_CS_READ_REMOTE_CAPABILITIES_FAILED,
                sc);
    return;
  }
  initiator->read_capabilities = true;

  log_info(APP_INSTANCE_PREFIX "Reading remote CS capabilities..." NL,
           result->conn_handle);
}
// -----------------------------------------------------------------------------
// Event / callback definitions

/**************************************************************************//**
 * Bluetooth stack event handler
 *
 * @param[in] evt Event coming from the Bluetooth stack.
 *****************************************************************************/
void sl_bt_on_event(sl_bt_msg_t * evt)
{
  sl_status_t sc;
  const char* device_name = REFLECTOR_DEVICE_NAME;
  initiator_instance_t *initiator;

  app_cs_discovery_on_bt_event(evt);
  switch (SL_BT_MSG_ID(evt->header)) {
    // -------------------------------
    // This event indicates the device has started and the radio is ready.
    // Do not call any stack command before receiving this boot event!
    case sl_bt_evt_system_boot_id:
    {
      // Set TX power
      int16_t min_tx_power_x10 = SYSTEM_MIN_TX_POWER_DBM * 10;
      int16_t max_tx_power_x10 = SYSTEM_MAX_TX_POWER_DBM * 10;
      sc = sl_bt_system_set_tx_power(min_tx_power_x10,
                                     max_tx_power_x10,
                                     &min_tx_power_x10,
                                     &max_tx_power_x10);
      app_assert_status(sc);
      log_info(APP_PREFIX "Minimum system TX power is set to: %d dBm" NL, min_tx_power_x10 / 10);
      log_info(APP_PREFIX "Maximum system TX power is set to: %d dBm" NL, max_tx_power_x10 / 10);

      // Reset to initial state
      sl_bt_peer_manager_central_init();
      sl_bt_peer_manager_filter_init();

      // Print the Bluetooth address
      bd_addr address;
      uint8_t address_type;
      sc = sl_bt_gap_get_identity_address(&address, &address_type);
      app_assert_status(sc);
      log_info(APP_PREFIX "Bluetooth %s address: %02X:%02X:%02X:%02X:%02X:%02X\n",
               address_type ? "static random" : "public device",
               address.addr[5],
               address.addr[4],
               address.addr[3],
               address.addr[2],
               address.addr[1],
               address.addr[0]);

      // Filter for advertised name (CS_RFLCT)
      sc = sl_bt_peer_manager_set_filter_device_name(device_name,
                                                     strlen(device_name),
                                                     false);
      app_assert_status(sc);

      uint16_t ras_service_uuid = CS_RAS_SERVICE_UUID;
      sc = sl_bt_peer_manager_set_filter_service_uuid16((sl_bt_uuid_16_t *)&ras_service_uuid);
      app_assert_status(sc);

      // Set the default connection parameters for new connections based on the
      // procedure scheduling and the configured CS Manager instance count.
      app_cs_set_default_connection_parameters();

#ifndef SL_CATALOG_CS_INITIATOR_CLI_PRESENT
      try_start_scanning();
#else
      log_info("CS CLI is active." NL);
#endif // SL_CATALOG_CS_INITIATOR_CLI_PRESENT
      break;
    }
    case sl_bt_evt_connection_parameters_id:
    {
      uint8_t connection = evt->data.evt_connection_parameters.connection;
      initiator = app_get_instance(connection);
      if (initiator == NULL) {
        break;
      }
      if (evt->data.evt_connection_parameters.security_mode
          != sl_bt_connection_mode1_level1) {
        if (!initiator->ras_discovery) {
          connection_setup_handle = connection;
          sc = app_cs_discovery_start_discovery(connection,
                                                on_ras_discovery_complete);
          if (sc != SL_STATUS_OK && sc != SL_STATUS_IN_PROGRESS) {
            log_error(APP_INSTANCE_PREFIX "Failed to start RAS discovery! [sc: 0x%lx]" NL,
                      connection,
                      (unsigned long)sc);
            connection_setup_handle = SL_BT_INVALID_CONNECTION_HANDLE;
            cs_on_error(connection, CS_APP_ERROR_RAS_DISCOVERY_FAILED, sc);
            break;
          }
          // Discovery started
          initiator->ras_discovery = true;
        }
      } else {
        if (!initiator->security_increase) {
          log_info(APP_INSTANCE_PREFIX "Increasing security..." NL, connection);
          sc = sl_bt_sm_increase_security(connection);
          app_assert_status(sc);
          initiator->security_increase = true;
        }
      }
      break;
    }

    // --------------------------------
    // PHY update completed.
    case sl_bt_evt_connection_phy_status_id:
    {
      app_on_connection_phy_changed(evt->data.evt_connection_phy_status.phy);
      break;
    }

    // --------------------------------
    // MTU exchange event
    case sl_bt_evt_gatt_mtu_exchanged_id:
    {
      // Update RREQ MTU config
      app_on_mtu_changed(evt->data.evt_gatt_mtu_exchanged.mtu);
    }
    break;

    case sl_bt_evt_cs_read_remote_supported_capabilities_complete_id:
    {
      uint8_t connection = evt->data.evt_cs_read_remote_supported_capabilities_complete.connection;
      log_info(APP_INSTANCE_PREFIX "Remote capabilities arrived." NL, connection);
      initiator = app_get_instance(connection);
      if (initiator == NULL) {
        log_error(APP_INSTANCE_PREFIX "Failed to get instance number for new connection!" NL, connection);
        break;
      }
      if (!initiator->read_capabilities) {
        log_warning(APP_INSTANCE_PREFIX "Capabilities read by the remote" NL, connection);
        break;
      }
      initiator->read_capabilities = false;
      // Check supported capabilities
      app_cs_check_supported_capabilities(evt);
      // Optimize parameters
      app_cs_optimize_parameters(connection);

      // RAS discovery is guaranteed complete at this point, safe to create initiator instance
      log_info(APP_INSTANCE_PREFIX "Creating new initiator instance" NL, connection);
      // Check if we can accept one more reflector connection
      if (num_reflector_connections >= CS_MANAGER_CONFIG_MAX_INSTANCES) {
        log_error(APP_INSTANCE_PREFIX "Maximum number of initiator instances (%u) reached, "
                                      "dropping connection..." NL,
                  connection,
                  CS_MANAGER_CONFIG_MAX_INSTANCES);
        cs_on_error(connection, CS_APP_ERROR_INITIATOR_INSTANCE_LIST_FULL, SL_STATUS_FULL);
        break;
      }

      // Store the new initiator instance
      initiator->measurement_cnt = 0u;
      memset(&initiator->measurement_mainmode, 0u, sizeof(cs_measurement_data_t));
      memset(&initiator->measurement_submode, 0u, sizeof(cs_measurement_data_t));
      memset(&initiator->measurement_progress, 0u, sizeof(cs_intermediate_result_t));

      // Create new initiator instance
      sc = app_cs_create_new_initiator_instance(connection, &discovery_config);
      if (sc != SL_STATUS_OK) {
        log_error(APP_INSTANCE_PREFIX "Failed to create initiator instance, "
                                      "error:0x%lx" NL,
                  connection,
                  (unsigned long)sc);
        cs_on_error(connection, CS_APP_ERROR_INITIATOR_INSTANCE_CREATE_FAILED, sc);
      } else {
        log_info(APP_INSTANCE_PREFIX "New initiator instance created" NL,
                 connection);
      }
      break;
    }

    // -------------------------------
    // This event indicates that the BT stack buffer resources were exhausted
    case sl_bt_evt_system_resource_exhausted_id:
      log_error(APP_PREFIX "BT stack buffers exhausted, data loss may have occurred! "
                           "buf_discarded='%u' buf_alloc_fail='%u' heap_alloc_fail='%u'" APP_LOG_NL,
                evt->data.evt_system_resource_exhausted.num_buffers_discarded,
                evt->data.evt_system_resource_exhausted.num_buffer_allocation_failures,
                evt->data.evt_system_resource_exhausted.num_heap_allocation_failures);
      break;
    default:
      break;
  }
}

void sl_bt_peer_security_on_event(uint8_t handle)
{
  log_info(APP_INSTANCE_PREFIX "Security process started" NL, handle);
}

void app_button_press_cb(uint8_t button, uint8_t duration)
{
  (void)duration;
  if (button == 0) {
    sl_bt_peer_security_send_confirmation(true);
  } else if (button == 1) {
    sl_bt_peer_security_send_confirmation(false);
  }
}

/******************************************************************************
 * BLE peer manager event handler
 *
 * @param[in] evt Event coming from the peer manager.
 *****************************************************************************/
void sl_bt_peer_manager_on_event_initiator(sl_bt_peer_manager_evt_type_t * event)
{
  sl_status_t sc;
  bd_addr *address;

  switch (event->evt_id) {
    case SL_BT_PEER_MANAGER_ON_CONN_OPENED_CENTRAL:
      sc = save_connection(event->connection_id);
      if (sc != SL_STATUS_OK) {
        log_error(APP_INSTANCE_PREFIX "Error finding a slot for connection: "
                                      "dropping connection..." NL,
                  event->connection_id);
        cs_on_error(event->connection_id, CS_APP_ERROR_INITIATOR_INSTANCE_LIST_FULL, SL_STATUS_FULL);
        break;
      }
      address = sl_bt_peer_manager_get_bt_address(event->connection_id);
      log_info(APP_INSTANCE_PREFIX "Connection opened as central with CS Reflector"
                                   " '%02X:%02X:%02X:%02X:%02X:%02X'" NL,
               event->connection_id,
               address->addr[5],
               address->addr[4],
               address->addr[3],
               address->addr[2],
               address->addr[1],
               address->addr[0]);
      // Load default configuration
      app_cs_get_default_config();
      // Check if the CLI has changed any default values
      // TODO: needs testing
      app_cs_check_cli_values();
      cs_rreq_display_set_measurement_mode(app_cs_get_main_mode(), app_cs_get_algo_mode());

      break;
    case SL_BT_PEER_MANAGER_ON_CONN_CLOSED:
      log_info(APP_INSTANCE_PREFIX "Connection closed" NL, event->connection_id);

      //reset discovery config
      if (discovery_config.conn_handle == event->connection_id) {
        discovery_config.conn_handle = SL_BT_INVALID_CONNECTION_HANDLE;
        discovery_config.service_handle = CS_RAS_INVALID_SERVICE_HANDLE;
        for (int i = 0; i < CS_RAS_CHARACTERISTIC_INDEX_COUNT; i++) {
          discovery_config.gattdb_handles.array[i] = CS_RAS_INVALID_CHARACTERISTIC_HANDLE;
        }
        discovery_config.status = SL_STATUS_FAIL;
      }
      app_cs_delete_initiator_instance(event->connection_id);
      // If this was the connection being set up, unblock scanning.
      if (connection_setup_handle == event->connection_id) {
        connection_setup_handle = SL_BT_INVALID_CONNECTION_HANDLE;
      }
      // Restart scanning for new reflector connections
      try_start_scanning();
      break;

    case SL_BT_PEER_MANAGER_ERROR:
      log_error(APP_INSTANCE_PREFIX "Peer Manager error" NL,
                event->connection_id);
      break;

    default:
      log_info(APP_INSTANCE_PREFIX "Unhandled Peer Manager event (%u)" NL,
               event->connection_id,
               event->evt_id);
      break;
  }
}
