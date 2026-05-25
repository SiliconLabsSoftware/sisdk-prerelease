/***************************************************************************//**
 * @file
 * @brief CS Initiator example Channel Sounding (CS) logic
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
#include "sl_component_catalog.h"
#include "app_assert.h"

// app content
#include "sl_main_init.h"
#include "app.h"
#include "trace.h"
#include "app_config.h"
#include "app_timer.h"

// Channel Sounding (CS) content
#include "app_cs_discovery.h"
#include "cs_manager.h"
#include "cs_manager_config.h"
#include "cs_rreq.h"
#include "cs_algo.h"
#include "cs_algo_config.h"
#include "cs_configurator.h"
#include "cs_configurator_config.h"
#include "cs_antenna_config.h"

// TODO: remove once ACP completed
#include "cs_ras_client.h"

// other required content
#include "sl_bt_peer_manager_central.h"
#include "sl_bt_peer_manager_filter.h"
#include "sl_clock_manager.h"

// TODO: refactor CLI
#ifdef SL_CATALOG_CS_INITIATOR_CLI_PRESENT
#include "cs_initiator_cli.h"
#endif // SL_CATALOG_CS_INITIATOR_CLI_PRESENT

// -----------------------------------------------------------------------------
// Definitions

// Subfeature bitmask for CS Channel Selection Algorithm #3c
#define CS_CHANNEL_SELECTION_ALGORITHM_3C_SUBFEATURE_BITMASK 2
// Subfeature bitmask for CS Role Initiator
#define CS_ROLE_INITIATOR_SUBFEATURE_BITMASK 0
// Subfeature bitmask for CS Role Reflector
#define CS_ROLE_REFLECTOR_SUBFEATURE_BITMASK 1

// -----------------------------------------------------------------------------
// Static function declarations

static const char *algo_mode_to_str(uint8_t algo_mode);
static const char *antenna_usage_to_str(const cs_manager_instance_config_t *manager_instance_config,
                                        const cs_config_t *cs_config,
                                        const cs_procedure_parameters_t *procedure_parameters);
static uint8_t get_algo_mode(void);
static void select_antennas(uint8_t conn_handle, 
                            uint8_t local_antenna_count,
                            uint8_t remote_antenna_count,
                            cs_algo_config_t *algo_config);

static void app_on_cs_manager_event(uint8_t conn_handle,
                                    uint8_t config_id,
                                    cs_manager_event_type_t event,
                                    sl_status_t status);
static void app_on_cs_rreq_create_complete(uint8_t conn_handle, sl_status_t sc);
static void app_on_cs_rreq_enable_complete(uint8_t conn_handle, 
                                             uint8_t enable, 
                                             sl_status_t sc);
// app_on_cs_rreq_on_error / app_on_cs_algo_on_error / cs_on_error live in
// app_cs_error.c (declared in app.h).

// -----------------------------------------------------------------------------
// Static variables

// RREQ
static cs_rreq_create_config_t rreq_create_config;

// Algo
static cs_algo_config_t algo_config;

// CS Manager
static cs_manager_instance_config_t manager_instance_config;
static cs_config_t cs_config;
static cs_config_data_t config_out;
static cs_procedure_parameters_t procedure_parameters;
static cs_manager_connection_parameters_t connection_parameters;

// CS Configurator variables
static cs_channel_map_preset_t channel_map_preset = CS_CONFIGURATOR_CONFIG_DEFAULT_CHANNEL_MAP_PRESET;
static cs_procedure_scheduling_t procedure_scheduling = CS_CONFIGURATOR_CONFIG_DEFAULT_PROCEDURE_SCHEDULING;

// -----------------------------------------------------------------------------
// Public functions

void app_cs_log_default_config(void)
{
   if (procedure_scheduling != CS_PROCEDURE_SCHEDULING_CUSTOM) {
     log_info(APP_PREFIX "Using %s based procedure scheduling." NL,
              procedure_scheduling == CS_PROCEDURE_SCHEDULING_OPTIMIZED_FOR_FREQUENCY
              ? "frequency update" : "energy consumption");
   } else {
     log_info(APP_PREFIX "Using custom procedure scheduling." NL);
   }
   log_info(APP_PREFIX "%s" NL,
            (procedure_parameters.max_procedure_count == 0) ? "Free running." : "Burst mode.");
   log_info(APP_PREFIX "Antenna offset: wire%s" NL,
            CS_ANTENNA_CONFIG_DEFAULT_ANTENNA_OFFSET == 0 ? "less" : "d");
   log_info(APP_PREFIX "Default CS procedure interval: %u" NL,
            procedure_parameters.min_procedure_interval);
   log_info(APP_PREFIX "CS main mode: %s (%u)" NL,
            (cs_config.main_mode_type == sl_bt_cs_mode_pbr) ? "PBR" : "RTT",
            cs_config.main_mode_type);
   log_info(APP_PREFIX "CS sub mode: %s (%u)" NL,
            (cs_config.sub_mode_type == sl_bt_cs_submode_disabled) ? "Disabled" : "RTT",
            cs_config.sub_mode_type);
   log_info(APP_PREFIX "Requested antenna usage: %s" NL,
            antenna_usage_to_str(&manager_instance_config, &cs_config, &procedure_parameters));
   log_info(APP_PREFIX "Object tracking mode: %s" NL,
            algo_mode_to_str(algo_config.rtl_config.algo_mode));
   log_info(APP_PREFIX "CS channel map preset: %d" NL, channel_map_preset);
   log_info(APP_PREFIX "CS channel map: %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X" NL,
            cs_config.channel_map.data[0],
            cs_config.channel_map.data[1],
            cs_config.channel_map.data[2],
            cs_config.channel_map.data[3],
            cs_config.channel_map.data[4],
            cs_config.channel_map.data[5],
            cs_config.channel_map.data[6],
            cs_config.channel_map.data[7],
            cs_config.channel_map.data[8],
            cs_config.channel_map.data[9]);
   log_info(APP_PREFIX "RSSI reference TX power @ 1m: %d dBm" NL,
            (int)CS_ALGO_CONFIG_DEFAULT_RSSI_REF_TX_POWER);
   log_info("+-------------------------------------------------------+" NL);
}

uint8_t app_cs_get_main_mode(void)
{
  return cs_config.main_mode_type;
}

uint8_t app_cs_get_sub_mode(void)
{
  return cs_config.sub_mode_type;
}

cs_algo_mode_t app_cs_get_algo_mode(void)
{
  return algo_config.rtl_config.algo_mode;
}

cs_channel_map_preset_t app_cs_get_channel_map_preset(void)
{
  return channel_map_preset;
}

void app_on_mtu_changed(uint16_t mtu)
{
  rreq_create_config.mtu = mtu;
  log_info(APP_PREFIX "MTU set to: %u" NL, mtu);
}

/******************************************************************************
 * Check if the CLI has changed any default values of the initiator
 *****************************************************************************/
void app_cs_check_cli_values(void)
{
#ifdef SL_CATALOG_CS_INITIATOR_CLI_PRESENT
  if (cs_initiator_cli_get_antenna_config_index() 
      != procedure_parameters.tone_antenna_config_selection) {
    antenna_set_pbr = true;
  }
  procedure_parameters.tone_antenna_config_selection 
    = cs_initiator_cli_get_antenna_config_index();
  if (cs_initiator_cli_get_cs_sync_antenna_usage() 
      != manager_instance_config.cs_sync_antenna) {
    antenna_set_rtt = true;
  }
  cs_config.sub_mode_type = cs_initiator_cli_get_sub_mode();
  manager_instance_config.cs_sync_antenna 
    = cs_initiator_cli_get_cs_sync_antenna_usage();
  cs_config.main_mode_type = cs_initiator_cli_get_mode();
  manager_instance_config.conn_phy = cs_initiator_cli_get_conn_phy();
  procedure_parameters.max_procedure_count 
    = cs_initiator_cli_get_procedure_counter();
  algo_config.rtl_config.algo_mode = cs_initiator_cli_get_algo_mode();
  channel_map_preset = cs_initiator_cli_get_preset();
  cs_configurator_apply_channel_map_preset(channel_map_preset,
                                           cs_config.channel_map.data);
#endif // SL_CATALOG_CS_INITIATOR_CLI_PRESENT
}

/******************************************************************************
 * Create new initiator instance
 *****************************************************************************/
sl_status_t app_cs_create_new_initiator_instance(uint8_t conn_handle,
                                                 app_cs_discovery_result_t *discovery_config)
{
  sl_status_t sc;
  if (discovery_config == NULL) {
    return SL_STATUS_NULL_POINTER;
  }
  if (discovery_config->status != SL_STATUS_OK) {
    log_error(APP_INSTANCE_PREFIX "RAS discovery failed for connection." NL,
              conn_handle);
    return discovery_config->status;
  }
  // Create a new CS Manager instance
  sc = cs_manager_create(conn_handle,
                         &manager_instance_config,
                         &connection_parameters);
  if (sc != SL_STATUS_OK) {
    log_error(APP_INSTANCE_PREFIX "CS Manager creation failed." NL, conn_handle);
    return sc;
  } else {
    app_increment_reflector_connections();
  }
  //Save the result of the discovery to the RREQ config
  rreq_create_config.service = discovery_config->service_handle;
  for (int i = 0; i < CS_RAS_CHARACTERISTIC_INDEX_COUNT; i++) {
    rreq_create_config.gattdb_handles.array[i] 
      = discovery_config->gattdb_handles.array[i];
  }
  return sc;
}

/******************************************************************************
 * Check if the remote device supports the required capabilities
 *****************************************************************************/
void app_cs_check_supported_capabilities(const sl_bt_msg_t *evt)
{
  sl_status_t sc;
  uint8_t local_cs_sync_phy;
  uint16_t local_subfeatures;
  uint8_t local_roles;
  uint8_t local_rtt_aa_only;
  uint8_t local_rtt_sounding;
  uint8_t local_rtt_random;
  uint8_t num_antennas;
  sc = sl_bt_cs_read_local_supported_capabilities(NULL,
                                                  NULL,
                                                  &num_antennas,
                                                  NULL,
                                                  &local_roles,
                                                  NULL,
                                                  NULL,
                                                  &local_rtt_aa_only,
                                                  &local_rtt_sounding,
                                                  &local_rtt_random,
                                                  NULL,
                                                  NULL,
                                                  &local_cs_sync_phy,
                                                  &local_subfeatures,
                                                  NULL,
                                                  NULL,
                                                  NULL,
                                                  NULL,
                                                  NULL,
                                                  NULL);
  app_assert_status(sc);
  uint8_t conn_handle = evt->data.evt_cs_read_remote_supported_capabilities_complete.connection;
  // initiator config is set to CS_SYNC_PHY 2M
  // but local/remote device only supports CS_SYNC_PHY 1M
  if (cs_config.cs_sync_phy == sl_bt_gap_phy_2m
      && (local_cs_sync_phy == 0 
          || evt->data.evt_cs_read_remote_supported_capabilities_complete.cs_sync_phys == 0)) {
    cs_on_error(conn_handle,
                CS_APP_ERROR_CS_SYNC_PHY_NOT_SUPPORTED,
                SL_STATUS_NOT_SUPPORTED);
    cs_config.cs_sync_phy = sl_bt_gap_phy_1m;
    app_log_info(APP_PREFIX "Using CS SYNC PHY %d instead" APP_LOG_NL,
                 cs_config.cs_sync_phy);
  }
  // initiator config algorithm #3c is selected but local/remote device doesn't support it
  if (cs_config.channel_selection_type == sl_bt_cs_channel_selection_algorithm_3c
      && ((local_subfeatures >> CS_CHANNEL_SELECTION_ALGORITHM_3C_SUBFEATURE_BITMASK & 0x01) == 0
          || (evt->data.evt_cs_read_remote_supported_capabilities_complete.subfeatures
              >> CS_CHANNEL_SELECTION_ALGORITHM_3C_SUBFEATURE_BITMASK & 0x01) == 0)) {
    app_assert(false, 
               APP_PREFIX "Requested CS channel selection algorithm "
               " #3c is not supported by the local/remote device" APP_LOG_NL);
  }
  // local device doesn't support initiator role
  // or remote device doesn't support reflector role
  if ((local_roles >> CS_ROLE_INITIATOR_SUBFEATURE_BITMASK & 0x01) == 0
      || (evt->data.evt_cs_read_remote_supported_capabilities_complete.roles
          >> CS_ROLE_REFLECTOR_SUBFEATURE_BITMASK & 0x01) == 0) {
    app_assert(false, 
               APP_PREFIX "Requested CS role is not supported by the "
               "local/remote device" APP_LOG_NL);
  }
  // if mode is RTT check the RTT capabilities
  if (cs_config.main_mode_type == sl_bt_cs_mode_rtt) {
    // [RTT AA only] is set but local/remote device doesn't support it
    if ((cs_config.rtt_type == sl_bt_cs_rtt_type_aa_only)
        && (local_rtt_aa_only == 0
            || evt->data.evt_cs_read_remote_supported_capabilities_complete.rtt_aa_only == 0)) {
      app_assert(false,
                 APP_PREFIX "RTT AA only is not supported by the local/ "
                 "remote device" APP_LOG_NL);
    }
    // [RTT Sounding] is set but local/remote device doesn't support it
    if ((cs_config.rtt_type == sl_bt_cs_rtt_type_fractional_32_bit_sounding
         || cs_config.rtt_type == sl_bt_cs_rtt_type_fractional_96_bit_sounding)
        && (local_rtt_sounding == 0
            || evt->data.evt_cs_read_remote_supported_capabilities_complete.rtt_sounding == 0)) {
      app_assert(false,
                 APP_PREFIX "RTT sounding is not supported by the local/remote "
                 "device" APP_LOG_NL);
    }
    // [RTT Random] is set but local/remote device doesn't support it
    if ((cs_config.rtt_type == sl_bt_cs_rtt_type_fractional_32_bit_random
         || cs_config.rtt_type == sl_bt_cs_rtt_type_fractional_64_bit_random
         || cs_config.rtt_type == sl_bt_cs_rtt_type_fractional_96_bit_random
         || cs_config.rtt_type == sl_bt_cs_rtt_type_fractional_128_bit_random)
        && (local_rtt_random == 0
            || evt->data.evt_cs_read_remote_supported_capabilities_complete.rtt_random_payload == 0)) {
      app_assert(false,
                 APP_PREFIX "RTT random is not supported by the local/ "
                 "remote device" APP_LOG_NL);
    }
  }
  // Select antennas
  select_antennas(conn_handle,
                  num_antennas,
                  evt->data.evt_cs_read_remote_supported_capabilities_complete.num_antennas,
                  &algo_config);
}

/******************************************************************************
 * Delete initiator instance.
 *****************************************************************************/
void app_cs_delete_initiator_instance(uint8_t conn_handle)
{
  sl_status_t sc;
  // Removing RREQ instance is automatic
  // Remove algo instance
  sc = cs_algo_remove(conn_handle);
  if (sc != SL_STATUS_OK) {
    cs_on_error(conn_handle, CS_APP_ERROR_ALGO_REMOVE_FAILED, sc);
  }
  // Remove CS Manager instance
  sc = cs_manager_delete(conn_handle);
  if ((sc == SL_STATUS_NOT_FOUND) || (sc == SL_STATUS_INVALID_HANDLE)) {
    log_info(APP_INSTANCE_PREFIX "CS Manager instance not found" NL, conn_handle);
  } else {
    app_assert_status(sc);
    app_decrement_reflector_connections();
    log_info(APP_INSTANCE_PREFIX "CS Manager instance removed" NL, conn_handle);
  }
  app_clear_instance_data(conn_handle);
}

// -----------------------------------------------------------------------------
// Private functions

/******************************************************************************
 * Return runtime configurable value for object tracking mode
 *****************************************************************************/
 #if (SL_SIMPLE_BUTTON_COUNT > 1)
 #if CS_ALGO_CONFIG_DEFAULT_ALGO_MODE == CS_ALGO_MODE_REAL_TIME_FAST
   #define ALTERNATIVE_ALGO_MODE CS_ALGO_MODE_STATIC_HIGH_ACCURACY
 #else
   #define ALTERNATIVE_ALGO_MODE CS_ALGO_MODE_REAL_TIME_FAST
 #endif
static uint8_t get_algo_mode(void)
{
 if (sl_button_get_state(SL_SIMPLE_BUTTON_INSTANCE(1)) == SL_SIMPLE_BUTTON_PRESSED) {
   return ALTERNATIVE_ALGO_MODE;
 }
 return CS_ALGO_CONFIG_DEFAULT_ALGO_MODE;
}
#else
static uint8_t get_algo_mode(void)
{
 return CS_ALGO_CONFIG_DEFAULT_ALGO_MODE;
}
#endif

/******************************************************************************
* Get requested antenna usage configuration as string
*****************************************************************************/
static const char *antenna_usage_to_str(const cs_manager_instance_config_t *manager_instance_config,
                                       const cs_config_t *cs_config,
                                       const cs_procedure_parameters_t *procedure_parameters)
{
 if (cs_config->main_mode_type == sl_bt_cs_mode_rtt) {
   switch (manager_instance_config->cs_sync_antenna) {
     case CS_SYNC_ANTENNA_1:
       return "antenna ID 1";
     case CS_SYNC_ANTENNA_2:
       return "antenna ID 2";
     case CS_SYNC_SWITCHING:
       return "switch between all antenna IDs";
     default:
       return "unknown";
   }
 } else {
   switch (procedure_parameters->tone_antenna_config_selection) {
     case CS_ANTENNA_CONFIG_INDEX_SINGLE_ONLY:
       return "single antenna on both sides (1:1)";
     case CS_ANTENNA_CONFIG_INDEX_DUAL_LOCAL_SINGLE_REMOTE:
       return "dual antenna initiator & single antenna reflector (2:1)";
     case CS_ANTENNA_CONFIG_INDEX_SINGLE_LOCAL_DUAL_REMOTE:
       return "single antenna initiator & dual antenna reflector (1:2)";
     case CS_ANTENNA_CONFIG_INDEX_DUAL_ONLY:
       return "dual antennas on both sides (2:2)";
     default:
       return "unknown";
   }
 }
}

/******************************************************************************
* Get algo mode as string
*****************************************************************************/
static const char *algo_mode_to_str(uint8_t algo_mode)
{
 switch (algo_mode) {
   case CS_ALGO_MODE_REAL_TIME_BASIC:
     return "real time basic (moving)";
   case CS_ALGO_MODE_STATIC_HIGH_ACCURACY:
     return "stationary object tracking";
   case CS_ALGO_MODE_REAL_TIME_FAST:
     return "real time fast (moving)";
   default:
     return "unknown";
 }
}


static void select_antennas(uint8_t conn_handle, 
                            uint8_t local_antenna_count,
                            uint8_t remote_antenna_count,
                            cs_algo_config_t *algo_config)
{
  uint8_t antenna_config = procedure_parameters.tone_antenna_config_selection;
  // Prepare for the CS main mode: PBR antenna usage
  if (cs_config.main_mode_type == sl_bt_cs_mode_pbr) {
    switch (procedure_parameters.tone_antenna_config_selection) {
      case CS_ANTENNA_CONFIG_INDEX_SINGLE_ONLY:
       algo_config->num_antenna_paths = 1;
        break;
      case CS_ANTENNA_CONFIG_INDEX_DUAL_LOCAL_SINGLE_REMOTE:
        if (local_antenna_count < 2) {
          log_warning(APP_INSTANCE_PREFIX "CS - PBR - 1:1 antenna usage is possible only!" NL, 
                      conn_handle);
          antenna_config = CS_ANTENNA_CONFIG_INDEX_SINGLE_ONLY;
          algo_config->num_antenna_paths = 1;
        } else {
          algo_config->num_antenna_paths = 2;
          log_info(APP_INSTANCE_PREFIX "CS - PBR - 2:1 antenna usage set" NL, 
                   conn_handle);
        }
        break;
      case CS_ANTENNA_CONFIG_INDEX_SINGLE_LOCAL_DUAL_REMOTE:
        if (remote_antenna_count < 2) {
          log_warning(APP_INSTANCE_PREFIX "CS - PBR - 1:1 antenna usage is possible only!" NL,
                      conn_handle);
          antenna_config = CS_ANTENNA_CONFIG_INDEX_SINGLE_ONLY;
          algo_config->num_antenna_paths = 1;
        } else {
          algo_config->num_antenna_paths = 2;
          log_info(APP_INSTANCE_PREFIX "CS - PBR - 1:2 antenna usage set" NL,
                   conn_handle);
        }
        break;
      case CS_ANTENNA_CONFIG_INDEX_DUAL_ONLY:
        if (remote_antenna_count >= 2 && local_antenna_count >= 2) {
          algo_config->num_antenna_paths = 4;
          log_info(APP_INSTANCE_PREFIX "CS - PBR - 2:2 antenna usage set" NL,
                   conn_handle);
        } else {
          if (remote_antenna_count == 1 && local_antenna_count == 2) {
            algo_config->num_antenna_paths = 2;
            antenna_config = CS_ANTENNA_CONFIG_INDEX_DUAL_LOCAL_SINGLE_REMOTE;
            log_warning(APP_INSTANCE_PREFIX "CS - PBR - 2:1 antenna usage set" NL,
                        conn_handle);
          } else if (remote_antenna_count == 2 && local_antenna_count == 1) {
            algo_config->num_antenna_paths = 2;
            antenna_config = CS_ANTENNA_CONFIG_INDEX_SINGLE_LOCAL_DUAL_REMOTE;
            log_warning(APP_INSTANCE_PREFIX "CS - PBR - 1:2 antenna usage set" NL,
                        conn_handle);
          } else {
            log_warning(APP_INSTANCE_PREFIX "CS - PBR - 1:1 antenna usage is possible only!" NL,
                        conn_handle);
            antenna_config = CS_ANTENNA_CONFIG_INDEX_SINGLE_ONLY;
            algo_config->num_antenna_paths = 1;
          }
        }
        break;
      default:
        log_warning(APP_INSTANCE_PREFIX "CS - PBR - unknown antenna usage! "
                    "Using the default setting: 1:1 antenna" NL,
                    conn_handle);
        antenna_config = CS_ANTENNA_CONFIG_INDEX_SINGLE_ONLY;
        algo_config->num_antenna_paths = 1;
        break;
    }
    log_debug(APP_INSTANCE_PREFIX "CS - PBR - using %u antenna paths" NL,
              conn_handle,
              algo_config->num_antenna_paths);
  }

  procedure_parameters.tone_antenna_config_selection = antenna_config;
  log_info(APP_INSTANCE_PREFIX "Using tone antenna configuration index: %u" NL,
           conn_handle,
           antenna_config);
}

static void app_on_cs_manager_event(uint8_t conn_handle,
                                    uint8_t config_id,
                                    cs_manager_event_type_t event,
                                    sl_status_t status)
{
  sl_status_t sc;
  switch (event) {
    case CS_MANAGER_EVENT_INSTANCE_CREATE_COMPLETE:
      if (status != SL_STATUS_OK) {
        cs_on_error(conn_handle,
                    CS_APP_ERROR_CS_MANAGER_INSTANCE_CREATE_FAILED,
                    status);
        return;
      }

      // Create RREQ
      rreq_create_config.real_time_mode = true;
      rreq_create_config.is_initiator = true;
      rreq_create_config.ras_config.real_time_ranging_data_indication = false;
      rreq_create_config.ras_config.on_demand_ranging_data_indication = false;
      rreq_create_config.ras_config.ranging_data_ready_notification = true;
      rreq_create_config.ras_config.ranging_data_overwritten_notification = true;
      rreq_create_config.antenna_config = procedure_parameters.tone_antenna_config_selection;
      // RAS GATT database handles has already been saved

      sc = cs_rreq_create(conn_handle, &rreq_create_config);
      if (sc != SL_STATUS_OK) {
        cs_on_error(conn_handle, CS_APP_ERROR_RREQ_CREATE_FAILED, sc);
      }
      break;
    case CS_MANAGER_EVENT_INSTANCE_REMOVE_COMPLETE:
      if (status != SL_STATUS_OK) {
        cs_on_error(conn_handle,
                    CS_APP_ERROR_CS_MANAGER_INSTANCE_REMOVE_FAILED,
                    status);
        return;
      }
      log_info(APP_INSTANCE_PREFIX "CS Manager instance removed" NL, conn_handle);
      (void)sl_bt_peer_manager_central_close_connection(conn_handle);
      break;
    case CS_MANAGER_EVENT_CONFIG_OVERWRITTEN:
    case CS_MANAGER_EVENT_CONFIG_CREATE_COMPLETE:
      if (status != SL_STATUS_OK) {
        cs_on_error(conn_handle, CS_APP_ERROR_CS_CONFIG_CREATE_FAILED, status);
        return;
      }
      if (event == CS_MANAGER_EVENT_CONFIG_OVERWRITTEN) {
        log_info(APP_INSTANCE_PREFIX "CS configuration overwritten" NL, conn_handle);
      } else {
        log_info(APP_INSTANCE_PREFIX "CS configuration created" NL, conn_handle);
      } 
      sc = cs_manager_config_get(conn_handle, config_id, &config_out);
      if (sc != SL_STATUS_OK) {
        cs_on_error(conn_handle, CS_APP_ERROR_CS_CONFIG_GET_DATA_FAILED, sc);
        return;
      }

      algo_config.connection_interval = connection_parameters.max_connection_interval;
      algo_config.cs_main_mode = config_out.main_mode_type;
      algo_config.cs_sub_mode = config_out.sub_mode_type;
      algo_config.min_main_mode_steps = config_out.min_main_mode_steps;
      algo_config.max_main_mode_steps = config_out.max_main_mode_steps;
      algo_config.main_mode_repetition = config_out.main_mode_repetition;
      algo_config.channel_map_repetition = config_out.channel_map_repetition;
      algo_config.channel_selection_type = config_out.channel_selection_type;
      algo_config.ch3c_shape = config_out.ch3c_shape;
      algo_config.ch3c_jump = config_out.ch3c_jump;
      algo_config.rtt_type = config_out.rtt_type;
      algo_config.cs_sync_phy = config_out.cs_sync_phy;
      algo_config.channel_map_preset = channel_map_preset;
      algo_config.num_calib_steps = config_out.mode_calibration_steps;
      algo_config.T_PM_time = config_out.t_pm_time;
      algo_config.T_IP1_time = config_out.t_ip1_time;
      algo_config.T_IP2_time = config_out.t_ip2_time;
      algo_config.T_FCS_time = config_out.t_fcs_time;
      algo_config.channel_map = config_out.channel_map;
      // Antenna path has already been set in the select_antennas function

      // Create algo instance
      sc = cs_algo_create(conn_handle, algo_config);
      if (sc != SL_STATUS_OK) {
        cs_on_error(conn_handle, CS_APP_ERROR_ALGO_CREATE_FAILED, sc);
        break;
      }
      // Start procedure
      sc = cs_manager_start(conn_handle, config_id, &procedure_parameters);
      if (sc != SL_STATUS_OK) {
        cs_on_error(conn_handle, CS_APP_ERROR_PROCEDURE_START_FAILED, sc);
        break;
      }
      break;
    case CS_MANAGER_EVENT_CONFIG_REMOVE_COMPLETE:
      if (status != SL_STATUS_OK) {
        cs_on_error(conn_handle, CS_APP_ERROR_CS_CONFIG_REMOVE_FAILED, status);
        return;
      }
      log_info(APP_INSTANCE_PREFIX "CS configuration removed" NL, conn_handle);
      (void)sl_bt_peer_manager_central_close_connection(conn_handle);
      break;
    case CS_MANAGER_EVENT_PROCEDURE_START_COMPLETE:
      if (status != SL_STATUS_OK) {
        cs_on_error(conn_handle, CS_APP_ERROR_PROCEDURE_START_FAILED, status);
        return;
      }
      log_info(APP_INSTANCE_PREFIX "CS procedure started" NL, conn_handle);
      app_on_cs_setup_complete();
      break;
    case CS_MANAGER_EVENT_PROCEDURE_STOP_COMPLETE:
      if (status != SL_STATUS_OK) {
        cs_on_error(conn_handle, CS_APP_ERROR_PROCEDURE_STOP_FAILED, status);
        return;
      }
      log_info(APP_INSTANCE_PREFIX "CS procedure stopped" NL, conn_handle);
      break;
    default:
      break;
  }
}

static void app_on_cs_rreq_create_complete(uint8_t conn_handle, sl_status_t sc)
{
  if (sc != SL_STATUS_OK) {
    cs_on_error(conn_handle, CS_APP_ERROR_RREQ_CREATE_FAILED, sc);
    return;
  }
  log_info(APP_INSTANCE_PREFIX "RREQ create complete" NL, conn_handle);
  sc = cs_rreq_enable(conn_handle, true);
  if (sc != SL_STATUS_OK) {
    cs_on_error(conn_handle, CS_APP_ERROR_RREQ_ENABLE_FAILED, sc);
    return;
  }
}

static void app_on_cs_rreq_enable_complete(uint8_t conn_handle, 
                                           uint8_t enable,
                                           sl_status_t sc)
{
  if (sc != SL_STATUS_OK) {
    cs_on_error(conn_handle,
                enable ? CS_APP_ERROR_RREQ_ENABLE_FAILED
                       : CS_APP_ERROR_RREQ_DISABLE_FAILED,
                sc);
    return;
  }
  log_info(APP_INSTANCE_PREFIX "RREQ %s complete" NL,
          conn_handle,
          enable ? "enable" : "disable");
  if (enable == true) {
    sl_status_t status = cs_manager_config_create(conn_handle, 0, true, &cs_config);
    if (status != SL_STATUS_OK) {
      cs_on_error(conn_handle, CS_APP_ERROR_CS_CONFIG_CREATE_FAILED, status);
    }
  }
}

void app_cs_set_callbacks(cs_algo_app_cb_t *algo_cb) 
{
  sl_status_t sc;
  
  // Assign CS manager event callback
  sc = cs_manager_set_callback(app_on_cs_manager_event);
  if (sc != SL_STATUS_OK) {
    log_error(APP_PREFIX "Failed to register CS Manager event callback! [sc: 0x%lx]" NL,
              (unsigned long)sc);
    app_assert_status(sc);
  }

  cs_rreq_event_callback_t rreq_callbacks = {
    .on_create = app_on_cs_rreq_create_complete,
    .on_enable = app_on_cs_rreq_enable_complete,
    .on_error = app_on_cs_rreq_on_error,
  };
  // Set RREQ event callbacks
  sc = cs_rreq_set_event_callbacks(&rreq_callbacks);
  if (sc != SL_STATUS_OK) {
    log_error(APP_PREFIX "Failed to register RREQ event callbacks! [sc: 0x%lx]" NL,
              (unsigned long)sc);
    app_assert_status(sc);
  }

  algo_cb->on_error = app_on_cs_algo_on_error;
  
  sc = cs_algo_app_set_callback(algo_cb);
  if (sc != SL_STATUS_OK) {
    log_error(APP_PREFIX "Failed to register cs_algo app callbacks! [sc: 0x%lx]" NL,
              (unsigned long)sc);
    app_assert_status(sc);
  }
}

void app_cs_get_default_config(void)
{
  sl_status_t sc;

  // Get default configuration for CS Configurator
  channel_map_preset = CS_CONFIGURATOR_CONFIG_DEFAULT_CHANNEL_MAP_PRESET;
  procedure_scheduling = CS_CONFIGURATOR_CONFIG_DEFAULT_PROCEDURE_SCHEDULING;

  // Get default connection parameters
  sc = cs_manager_get_default_connection_parameters(&connection_parameters);
  if (sc != SL_STATUS_OK) {
    log_error(APP_PREFIX "Failed to get default CS Manager connection parameters, "
              "[sc: 0x%lx]" NL,
              (unsigned long)sc);
    app_assert_status(sc);
  }

  // Get default instance configuration for Initiator and Central role
  sc = cs_manager_get_default_instance_config(true, // Initiator role
                                              true, // Central role
                                              &manager_instance_config);

  if (sc != SL_STATUS_OK) {
    log_error(APP_PREFIX "Failed to get default CS Manager instance configuration, "
              "[sc: 0x%lx]" NL,
              (unsigned long)sc);
    app_assert_status(sc);
  }

  // Get default CS configuration
  sc = cs_manager_get_default_config(&cs_config);
  if (sc != SL_STATUS_OK) {
    log_error(APP_PREFIX "Failed to get default CS config, "
              "[sc: 0x%lx]" NL,
              (unsigned long)sc);
    app_assert_status(sc);
  }

  // Get default CS procedure parameters
  sc = cs_manager_get_default_procedure_parameters(&procedure_parameters);
  if (sc != SL_STATUS_OK) {
    log_error(APP_PREFIX "Failed to get default CS procedure parameters, "
              "[sc: 0x%lx]" NL,
              (unsigned long)sc);
    app_assert_status(sc);
  }
  
  // Get default RTL configuration
  algo_config.rtl_config.rtl_logging_enabled = CS_ALGO_CONFIG_RTL_LOG;
  algo_config.rtl_config.algo_mode = get_algo_mode();
  algo_config.rssi_ref_tx_power = CS_ALGO_CONFIG_DEFAULT_RSSI_REF_TX_POWER;

  // Set configuration parameters
  if ((cs_config.main_mode_type == sl_bt_cs_mode_pbr)
      && (cs_config.sub_mode_type == sl_bt_cs_mode_rtt)) {
    // Currently, only main mode = pbr and submode = rtt is supported
    channel_map_preset = CS_CHANNEL_MAP_PRESET_HIGH;
    app_log_info(APP_PREFIX "Channel map preset set to high" NL);
  }

  // Apply channel map preset
  cs_configurator_apply_channel_map_preset(channel_map_preset,
                                           cs_config.channel_map.data);

}

void app_cs_optimize_parameters(uint8_t connection)
{
  uint32_t estimation_time_us;
  uint32_t clock_frequency_hz = 0;
  sl_status_t sc;

  // Set configurator parameters for optimization
  cs_configurator_parameters_t configurator_parameters = {
    .cs_instance_config = &manager_instance_config,
    .cs_config = &cs_config,
    .rreq_config = &rreq_create_config,
    .cs_procedure_parameters = &procedure_parameters,
    .connection_parameters = &connection_parameters,
  };

  sc = sl_clock_manager_get_clock_branch_frequency(SL_CLOCK_BRANCH_HCLK,
                                                   &clock_frequency_hz);
  app_assert_status(sc);
  sc = cs_configurator_get_estimation_time_us(&configurator_parameters,
                                              algo_config.rtl_config.algo_mode,
                                              channel_map_preset,
                                              clock_frequency_hz,
                                              algo_config.num_antenna_paths,
                                              &estimation_time_us);

  app_assert_status(sc);
  log_info(APP_INSTANCE_PREFIX "Estimation time: %lu us" NL, connection,
           (unsigned long)estimation_time_us);
  sc = cs_configurator_optimize(procedure_scheduling,
                                channel_map_preset, estimation_time_us,
                                CS_MANAGER_CONFIG_MAX_INSTANCES,
                                algo_config.num_antenna_paths,
                                &configurator_parameters);
  if (sc == SL_STATUS_NOT_SUPPORTED) {
    cs_on_error(connection,
                CS_APP_ERROR_PARAM_OPTIMIZATION_NOT_SUPPORTED,
                sc);
  } else if (sc == SL_STATUS_IDLE) {
    log_info(APP_PREFIX "No optimization - using custom procedure scheduling" NL);
  } else if (sc == SL_STATUS_OK) {
    log_info(APP_INSTANCE_PREFIX "Optimized parameters for connection interval "
             "and procedure interval." NL, connection);
  } else {
    cs_on_error(connection,
                CS_APP_ERROR_PARAM_OPTIMIZATION_INVALID_INPUT,
                sc);
  }
  float period_ms = connection_parameters.max_connection_interval 
                      * 1.25f * procedure_parameters.max_procedure_interval;
  log_info(APP_INSTANCE_PREFIX "Connection interval: %u  Procedure interval: %u  "
           "Period: %d ms  Frequency: %u.%03u Hz" NL,
            connection,
            connection_parameters.max_connection_interval,
            procedure_parameters.max_procedure_interval,
            (int)period_ms,
            (uint16_t)(1000.0f / period_ms),
            (((uint16_t)(1000000.0f / period_ms)) % 1000));
  // Validate the parameters
  sc = cs_configurator_validate(&configurator_parameters,
                                channel_map_preset,
                                estimation_time_us,
                                CS_MANAGER_CONFIG_MAX_INSTANCES,
                                algo_config.num_antenna_paths);
  app_assert_status(sc);
  log_info(APP_INSTANCE_PREFIX "Validated parameters for connection interval "
           "and procedure interval." NL,
           connection);
}
