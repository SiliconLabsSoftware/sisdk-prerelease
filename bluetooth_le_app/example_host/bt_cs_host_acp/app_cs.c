/***************************************************************************//**
 * @file
 * @brief CS ACP host - Channel Sounding (CS) logic
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
#include "app_config.h"
#include "app_timer.h"

// Channel Sounding (CS) content
#include "app_cs_discovery.h"
#include "app_cs_capabilities.h"
#if defined(CS_CAPABILITIES_LOG) && CS_CAPABILITIES_LOG
#include "app_cs_capabilities_log.h"
#else
#define app_cs_log_supported_capabilities(conn_handle, local, remote)
#endif
#include "cs_manager.h"
#include "cs_manager_config.h"
#include "cs_rreq_api.h"
#include "cs_algo.h"
#include "cs_algo_config.h"
#include "cs_configurator.h"
#include "cs_configurator_config.h"
#ifdef SL_CATALOG_CS_ANTENNA_PRESENT
#include "cs_antenna_config.h"
#endif // SL_CATALOG_CS_ANTENNA_PRESENT

// other required content
#include "sl_bt_peer_manager_central.h"
#include "sl_bt_peer_manager_filter.h"

// Using 8MHz, as an estimate, but the real PC frequency is much higher
#define CS_HOST_REFERENCE_HCLK_HZ 80000000UL

// TODO: needs testing
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

// Antenna path count assumed when deriving the default connection parameters
// at boot via the CS Configurator, before per-connection capability
// negotiation. The default tone antenna configuration is dual-on-both-sides
// (CS_ANTENNA_CONFIG_INDEX_DUAL_ONLY), i.e. 4 antenna paths. This only affects
// the single-peer result; the multiple-connection bounds are independent of
// the antenna path count.
#define APP_CS_BOOT_OPTIMIZE_NUM_ANTENNA_PATHS   4

// -----------------------------------------------------------------------------
// Static function declarations

static const char *algo_mode_to_str(uint8_t algo_mode);
static const char *antenna_usage_to_str(const cs_manager_instance_config_t *manager_instance_config,
                                        const cs_config_t *cs_config,
                                        const cs_procedure_parameters_t *procedure_parameters);
static uint8_t get_algo_mode(void);
static void set_rreq_default_config(void);

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
static uint8_t negotiated_num_antenna_paths;

// CS Manager
static cs_manager_instance_config_t manager_instance_config;
static cs_manager_instance_config_t reflector_instance_config;
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
   #ifdef SL_CATALOG_CS_ANTENNA_PRESENT
  log_info(APP_PREFIX "Antenna offset: wire%s" NL,
           CS_ANTENNA_CONFIG_DEFAULT_ANTENNA_OFFSET == 0 ? "less" : "d");
   #endif // SL_CATALOG_CS_ANTENNA_PRESENT
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

void app_on_connection_phy_changed(uint8_t phy)
{
  manager_instance_config.actual_conn_phy = phy;
  reflector_instance_config.actual_conn_phy = phy;
  log_info(APP_PREFIX "Connection PHY set to: %u" NL, phy);
}

// -----------------------------------------------------------------------------
// CLI override setters. Apply after app_cs_get_default_config() has populated
// the static config from component defaults. These are used by the host CLI
// (-m/-M/-P/-o/-p/-a/-q/-s/-S) to override the initiator-side configuration.

void app_cs_set_main_mode(uint8_t main_mode)
{
  cs_config.main_mode_type = main_mode;
}

void app_cs_set_sub_mode(uint8_t sub_mode)
{
  cs_config.sub_mode_type = sub_mode;
}

void app_cs_set_conn_phy(uint8_t phy)
{
  manager_instance_config.conn_phy = phy;
}

void app_cs_set_algo_mode(uint8_t algo_mode_value)
{
  algo_config.rtl_config.algo_mode = algo_mode_value;
}

void app_cs_set_channel_map_preset(cs_channel_map_preset_t preset)
{
  channel_map_preset = preset;
  // Re-derive the channel map bytes for the new preset.
  cs_configurator_apply_channel_map_preset(channel_map_preset,
                                           cs_config.channel_map.data);
}

void app_cs_set_tone_antenna_config_idx(uint8_t idx)
{
  procedure_parameters.tone_antenna_config_selection = idx;
}

void app_cs_set_cs_sync_antenna(uint8_t cs_sync_antenna)
{
  manager_instance_config.cs_sync_antenna = cs_sync_antenna;
  reflector_instance_config.cs_sync_antenna = cs_sync_antenna;
}

void app_cs_set_procedure_scheduling(cs_procedure_scheduling_t scheduling)
{
  procedure_scheduling = scheduling;
}

void app_cs_set_max_procedure_count(uint16_t max_procedure_count)
{
  procedure_parameters.max_procedure_count = max_procedure_count;
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
  // Save the result of the discovery to the RREQ config
  rreq_create_config.service = discovery_config->service_handle;
  for (uint8_t i = 0; i < CS_RAS_CHARACTERISTIC_INDEX_COUNT; i++) {
    rreq_create_config.gattdb_handles.array[i]
      = discovery_config->gattdb_handles.array[i];
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
  return sc;
}

/******************************************************************************
 * Create new reflector instance
 *****************************************************************************/
sl_status_t app_cs_create_new_reflector_instance(uint8_t conn_handle)
{
  sl_status_t sc = cs_manager_create(conn_handle, &reflector_instance_config, NULL);
  if (sc != SL_STATUS_OK) {
    log_error(APP_INSTANCE_PREFIX "CS Manager creation failed." NL, conn_handle);
  }
  return sc;
}

/******************************************************************************
 * Delete reflector instance
 *****************************************************************************/
void app_cs_delete_reflector_instance(uint8_t conn_handle)
{
  sl_status_t sc = cs_manager_delete(conn_handle);
  if ((sc == SL_STATUS_NOT_FOUND) || (sc == SL_STATUS_INVALID_HANDLE)) {
    log_info(APP_INSTANCE_PREFIX "CS Manager instance not found" NL, conn_handle);
  } else {
    app_assert_status(sc);
    log_info(APP_INSTANCE_PREFIX "CS Manager instance removed" NL, conn_handle);
  }
}

/******************************************************************************
 * Check if the remote device supports the required capabilities
 *****************************************************************************/
void app_cs_check_supported_capabilities(const sl_bt_msg_t *evt)
{
  sl_status_t sc;
  app_cs_capabilities_t local_caps = { 0 };
  const sl_bt_evt_cs_read_remote_supported_capabilities_complete_t *remote_evt =
    &evt->data.evt_cs_read_remote_supported_capabilities_complete;
  uint8_t conn_handle;

  sc = sl_bt_cs_read_local_supported_capabilities(&local_caps.num_config,
                                                  &local_caps.max_consecutive_procedures,
                                                  &local_caps.num_antennas,
                                                  &local_caps.max_antenna_paths,
                                                  &local_caps.roles,
                                                  &local_caps.modes,
                                                  &local_caps.rtt_capability,
                                                  &local_caps.rtt_aa_only,
                                                  &local_caps.rtt_sounding,
                                                  &local_caps.rtt_random_payload,
                                                  &local_caps.nadm_sounding_capability,
                                                  &local_caps.nadm_random_capability,
                                                  &local_caps.cs_sync_phys,
                                                  &local_caps.subfeatures,
                                                  &local_caps.t_ip1_times,
                                                  &local_caps.t_ip2_times,
                                                  &local_caps.t_fcs_times,
                                                  &local_caps.t_pm_times,
                                                  &local_caps.t_sw_times,
                                                  &local_caps.tx_snr_capability);
  app_assert_status(sc);
  conn_handle = remote_evt->connection;
  app_cs_log_supported_capabilities(conn_handle, &local_caps, remote_evt);

  // Save the remote antenna switching time capability so it can be used to
  // compute T_SW_time in CS Algo.
  algo_config.remote_t_sw_us = remote_evt->t_sw_times;

  // initiator config is set to CS_SYNC_PHY 2M
  // but local/remote device only supports CS_SYNC_PHY 1M
  if (cs_config.cs_sync_phy == sl_bt_gap_phy_2m
      && (local_caps.cs_sync_phys == 0
          || remote_evt->cs_sync_phys == 0)) {
    cs_on_error(conn_handle,
                CS_APP_ERROR_CS_SYNC_PHY_NOT_SUPPORTED,
                SL_STATUS_NOT_SUPPORTED);
    cs_config.cs_sync_phy = sl_bt_gap_phy_1m;
    app_log_info(APP_PREFIX "Using CS SYNC PHY %d instead" APP_LOG_NL,
                 cs_config.cs_sync_phy);
  }
  // initiator config algorithm #3c is selected but local/remote device doesn't support it
  if (cs_config.channel_selection_type == sl_bt_cs_channel_selection_algorithm_3c
      && ((local_caps.subfeatures >> CS_CHANNEL_SELECTION_ALGORITHM_3C_SUBFEATURE_BITMASK & 0x01) == 0
          || (remote_evt->subfeatures
              >> CS_CHANNEL_SELECTION_ALGORITHM_3C_SUBFEATURE_BITMASK & 0x01) == 0)) {
    app_assert(false,
               APP_PREFIX "Requested CS channel selection algorithm "
                          " #3c is not supported by the local/remote device" APP_LOG_NL);
  }
  // local device doesn't support initiator role
  // or remote device doesn't support reflector role
  if ((local_caps.roles >> CS_ROLE_INITIATOR_SUBFEATURE_BITMASK & 0x01) == 0
      || (remote_evt->roles
          >> CS_ROLE_REFLECTOR_SUBFEATURE_BITMASK & 0x01) == 0) {
    app_assert(false,
               APP_PREFIX "Requested CS role is not supported by the "
                          "local/remote device" APP_LOG_NL);
  }
  // [RTT AA only] is set but local/remote device doesn't support it
  if ((cs_config.rtt_type == sl_bt_cs_rtt_type_aa_only)
      && (local_caps.rtt_aa_only == 0
          || remote_evt->rtt_aa_only == 0)) {
    app_assert(false,
               APP_PREFIX "RTT AA only is not supported by the local/ "
                          "remote device" APP_LOG_NL);
  }
  // [RTT Sounding] is set but local/remote device doesn't support it
  if ((cs_config.rtt_type == sl_bt_cs_rtt_type_fractional_32_bit_sounding
       || cs_config.rtt_type == sl_bt_cs_rtt_type_fractional_96_bit_sounding)
      && (local_caps.rtt_sounding == 0
          || remote_evt->rtt_sounding == 0)) {
    app_assert(false,
               APP_PREFIX "RTT sounding is not supported by the local/remote "
                          "device" APP_LOG_NL);
  }
  // [RTT Random] is set but local/remote device doesn't support it
  if ((cs_config.rtt_type == sl_bt_cs_rtt_type_fractional_32_bit_random
       || cs_config.rtt_type == sl_bt_cs_rtt_type_fractional_64_bit_random
       || cs_config.rtt_type == sl_bt_cs_rtt_type_fractional_96_bit_random
       || cs_config.rtt_type == sl_bt_cs_rtt_type_fractional_128_bit_random)
      && (local_caps.rtt_random_payload == 0
          || remote_evt->rtt_random_payload == 0)) {
    app_assert(false,
               APP_PREFIX "RTT random is not supported by the local/ "
                          "remote device" APP_LOG_NL);
  }
  // Select antennas, limited by the minimum supported antenna paths
  uint8_t max_antenna_paths =
    (local_caps.max_antenna_paths < remote_evt->max_antenna_paths)
    ? local_caps.max_antenna_paths
    : remote_evt->max_antenna_paths;
  (void)cs_manager_select_antennas(conn_handle,
                                   cs_config.main_mode_type,
                                   local_caps.num_antennas,
                                   remote_evt->num_antennas,
                                   max_antenna_paths,
                                   &procedure_parameters,
                                   &negotiated_num_antenna_paths);
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
 #if CS_ALGO_CONFIG_DEFAULT_ALGO_MODE == CS_ALGO_MODE_TRACKING_LATENCY_OPTIMIZED
   #define ALTERNATIVE_ALGO_MODE CS_ALGO_MODE_STATIONARY
 #else
   #define ALTERNATIVE_ALGO_MODE CS_ALGO_MODE_TRACKING_LATENCY_OPTIMIZED
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
      case CS_SYNC_ANTENNA_3:
        return "antenna ID 3";
      case CS_SYNC_ANTENNA_4:
        return "antenna ID 4";
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
      case CS_ANTENNA_CONFIG_INDEX_TRIPLE_LOCAL_SINGLE_REMOTE:
        return "triple antenna initiator & single antenna reflector (3:1)";
      case CS_ANTENNA_CONFIG_INDEX_QUAD_LOCAL_SINGLE_REMOTE:
        return "quad antenna initiator & single antenna reflector (4:1)";
      case CS_ANTENNA_CONFIG_INDEX_SINGLE_LOCAL_DUAL_REMOTE:
        return "single antenna initiator & dual antenna reflector (1:2)";
      case CS_ANTENNA_CONFIG_INDEX_SINGLE_LOCAL_TRIPLE_REMOTE:
        return "single antenna initiator & triple antenna reflector (1:3)";
      case CS_ANTENNA_CONFIG_INDEX_SINGLE_LOCAL_QUAD_REMOTE:
        return "single antenna initiator & quad antenna reflector (1:4)";
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
    case CS_ALGO_MODE_TRACKING_ACCURACY_OPTIMIZED:
      return "Tracking accuracy optimized (suitable for moving targets)";
    case CS_ALGO_MODE_STATIONARY:
      return "Stationary (suitable for stationary targets)";
    case CS_ALGO_MODE_TRACKING_LATENCY_OPTIMIZED:
      return "Tracking latency optimized (suitable for fast moving targets)";
    default:
      return "unknown";
  }
}

static void app_on_cs_manager_event(uint8_t conn_handle,
                                    uint8_t config_id,
                                    cs_manager_event_type_t event,
                                    sl_status_t status)
{
  sl_status_t sc;

  // The cs_manager has a single global event callback. On the host we run
  // both Initiator and Reflector roles concurrently, so we route by role:
  // for reflector-role connections we just log (no RREQ/Algo creation).
  // Mirrors the SoC reflector's on_cs_manager_event() flow.
  if (app_is_reflector_connection(conn_handle)) {
    switch (event) {
      case CS_MANAGER_EVENT_INSTANCE_CREATE_COMPLETE:
        if (status != SL_STATUS_OK) {
          log_error(APP_INSTANCE_PREFIX "Failed to create CS Manager (reflector) "
                                        "instance [sc: 0x%lx]" NL,
                    conn_handle, (unsigned long)status);
          return;
        }
        log_info(APP_INSTANCE_PREFIX "CS Manager (reflector) instance created" NL,
                 conn_handle);
        break;
      case CS_MANAGER_EVENT_INSTANCE_REMOVE_COMPLETE:
        log_info(APP_INSTANCE_PREFIX "CS Manager (reflector) instance removed" NL,
                 conn_handle);
        break;
      case CS_MANAGER_EVENT_CONFIG_CREATE_COMPLETE:
        log_info(APP_INSTANCE_PREFIX "CS configuration created by initiator" NL,
                 conn_handle);
        break;
      case CS_MANAGER_EVENT_CONFIG_OVERWRITTEN:
        log_info(APP_INSTANCE_PREFIX "CS configuration overwritten by initiator" NL,
                 conn_handle);
        break;
      case CS_MANAGER_EVENT_CONFIG_REMOVE_COMPLETE:
        log_info(APP_INSTANCE_PREFIX "CS configuration removed (reflector)" NL,
                 conn_handle);
        break;
      case CS_MANAGER_EVENT_PROCEDURE_START_COMPLETE:
        log_info(APP_INSTANCE_PREFIX "CS procedure started (reflector)" NL,
                 conn_handle);
        break;
      case CS_MANAGER_EVENT_PROCEDURE_STOP_COMPLETE:
        log_info(APP_INSTANCE_PREFIX "CS procedure stopped (reflector)" NL,
                 conn_handle);
        break;
      case CS_MANAGER_EVENT_ERROR:
        log_error(APP_INSTANCE_PREFIX "CS Manager (reflector) general error "
                                      "[sc: 0x%lx]" NL,
                  conn_handle, (unsigned long)status);
        break;
      default:
        log_debug(APP_INSTANCE_PREFIX "Unhandled CS Manager (reflector) event "
                                      "(%u) [sc: 0x%lx]" NL,
                  conn_handle, (unsigned)event, (unsigned long)status);
        break;
    }
    return;
  }

  // ---- Initiator role ----
  switch (event) {
    case CS_MANAGER_EVENT_INSTANCE_CREATE_COMPLETE:
      if (status != SL_STATUS_OK) {
        cs_on_error(conn_handle,
                    CS_APP_ERROR_CS_MANAGER_INSTANCE_CREATE_FAILED,
                    status);
        return;
      }

      // Create RREQ
      // Fixed settings are initialized in set_rreq_default_config(); only the
      // connection-specific antenna_config is set here.
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
      algo_config.tone_antenna_config_selection = procedure_parameters.tone_antenna_config_selection;
      // Antenna path has already been set in cs_manager_select_antennas

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
                enable
                ? CS_APP_ERROR_RREQ_ENABLE_FAILED
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

void app_cs_set_callbacks(cs_algo_event_callback_t algo_cb)
{
  sl_status_t sc;

  cs_manager_event_callback_t manager_callbacks = {
    .on_event = app_on_cs_manager_event,
    .on_error = app_on_cs_manager_on_error,
  };
  // Assign CS manager event and error callbacks
  sc = cs_manager_set_event_callbacks(&manager_callbacks);
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
  sc = cs_rreq_set_event_callbacks(rreq_callbacks);
  if (sc != SL_STATUS_OK) {
    log_error(APP_PREFIX "Failed to register RREQ event callbacks! [sc: 0x%lx]" NL,
              (unsigned long)sc);
    app_assert_status(sc);
  }

  sc = cs_algo_set_event_callbacks(algo_cb);
  if (sc != SL_STATUS_OK) {
    log_error(APP_PREFIX "Failed to register cs_algo app callbacks! [sc: 0x%lx]" NL,
              (unsigned long)sc);
    app_assert_status(sc);
  }
}

/******************************************************************************
 * Set the fixed default values of the RREQ create configuration.
 * Runtime fields (antenna_config, mtu, service, gattdb_handles) are populated
 * later, once they are known for the connection.
 *****************************************************************************/
static void set_rreq_default_config(void)
{
#if defined(CS_RREQ_CONFIG_RAS_ON_DEMAND_SUPPORT) \
  && (CS_RREQ_CONFIG_RAS_ON_DEMAND_SUPPORT == 1)
  // On-demand RAS mode
  rreq_create_config.real_time_mode = false;
  rreq_create_config.ras_config.real_time_ranging_data_indication = false;
  rreq_create_config.ras_config.on_demand_ranging_data_indication = false;
  rreq_create_config.ras_config.ranging_data_ready_notification = true;
  rreq_create_config.ras_config.ranging_data_overwritten_notification = true;
#else
  // Real-time RAS mode
  rreq_create_config.real_time_mode = true;
  rreq_create_config.ras_config.real_time_ranging_data_indication = false;
  rreq_create_config.ras_config.on_demand_ranging_data_indication = false;
  rreq_create_config.ras_config.ranging_data_ready_notification = false;
  rreq_create_config.ras_config.ranging_data_overwritten_notification = false;
#endif
  rreq_create_config.is_initiator = true;
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
  manager_instance_config.request_conn_phy = true;

  if (sc != SL_STATUS_OK) {
    log_error(APP_PREFIX "Failed to get default CS Manager instance configuration, "
                         "[sc: 0x%lx]" NL,
              (unsigned long)sc);
    app_assert_status(sc);
  }

  // Get default instance configuration for Reflector and Peripheral role
  sc = cs_manager_get_default_instance_config(false, // Reflector CS role
                                              false, // Peripheral role
                                              &reflector_instance_config);
  reflector_instance_config.request_conn_phy = false;

  if (sc != SL_STATUS_OK) {
    log_error(APP_PREFIX "Failed to get default CS Manager reflector instance configuration, "
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

  // Get default RREQ configuration
  set_rreq_default_config();

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
  sl_status_t sc;

  uint8_t max_connections = app_get_max_connections();

  // Set configurator parameters for optimization
  cs_configurator_parameters_t configurator_parameters = {
    .cs_instance_config = &manager_instance_config,
    .cs_config = &cs_config,
    .rreq_config = &rreq_create_config,
    .cs_procedure_parameters = &procedure_parameters,
    .connection_parameters = &connection_parameters,
  };

  sc = cs_configurator_get_estimation_time_us(&configurator_parameters,
                                              algo_config.rtl_config.algo_mode,
                                              channel_map_preset,
                                              CS_HOST_REFERENCE_HCLK_HZ,
                                              negotiated_num_antenna_paths,
                                              &estimation_time_us);

  app_assert_status(sc);
  log_info(APP_INSTANCE_PREFIX "Estimation time: %lu us" NL, connection,
           (unsigned long)estimation_time_us);
  sc = cs_configurator_optimize(procedure_scheduling,
                                channel_map_preset, estimation_time_us,
                                max_connections,
                                negotiated_num_antenna_paths,
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
                                procedure_scheduling,
                                algo_config.rtl_config.algo_mode,
                                channel_map_preset,
                                estimation_time_us,
                                max_connections,
                                negotiated_num_antenna_paths);
  app_assert_status(sc);
  log_info(APP_INSTANCE_PREFIX "Validated parameters for connection interval "
                               "and procedure interval." NL,
           connection);
}

void app_cs_set_default_connection_parameters(void)
{
  sl_status_t sc;
  uint32_t estimation_time_us;
  uint8_t num_antenna_paths = APP_CS_BOOT_OPTIMIZE_NUM_ANTENNA_PATHS;
  uint8_t max_connections = app_get_max_connections();

  // Let the CS Configurator derive the connection interval from the procedure
  // scheduling and the configured connection count. Passing the max connection
  // count as the peer count means optimize selects the multiple-connection
  // bounds when more than one connection is configured, and the single-peer
  // bounds otherwise.
  cs_configurator_parameters_t configurator_parameters = {
    .cs_instance_config = &manager_instance_config,
    .cs_config = &cs_config,
    .rreq_config = &rreq_create_config,
    .cs_procedure_parameters = &procedure_parameters,
    .connection_parameters = &connection_parameters,
  };

  sc = cs_configurator_get_estimation_time_us(&configurator_parameters,
                                              algo_config.rtl_config.algo_mode,
                                              channel_map_preset,
                                              CS_HOST_REFERENCE_HCLK_HZ,
                                              num_antenna_paths,
                                              &estimation_time_us);
  if (sc != SL_STATUS_OK) {
    log_warning(APP_PREFIX "Could not estimate procedure time; keeping default "
                           "connection parameters [sc: 0x%lx]" NL,
                (unsigned long)sc);
  } else {
    sc = cs_configurator_optimize(procedure_scheduling,
                                  channel_map_preset,
                                  estimation_time_us,
                                  max_connections,
                                  num_antenna_paths,
                                  &configurator_parameters);
    if (sc == SL_STATUS_IDLE) {
      // Custom scheduling: optimize leaves connection_parameters unchanged, so
      // the configured CS Manager default is applied below.
      log_info(APP_PREFIX "Custom procedure scheduling; using default "
                          "connection parameters" NL);
    } else if (sc != SL_STATUS_OK) {
      log_warning(APP_PREFIX "Could not optimize default connection parameters; "
                             "keeping defaults [sc: 0x%lx]" NL,
                  (unsigned long)sc);
    }
  }

  // Apply via the CS Manager so the optimized values also become the manager's
  // baseline for needs_connection_parameter_update(); this lets connections
  // open at the optimized interval without a per-connection parameter update.
  sc = cs_manager_set_default_connection_parameters(&connection_parameters);
  if (sc != SL_STATUS_OK) {
    log_error(APP_PREFIX "Failed to set default connection parameters "
                         "[sc: 0x%lx]" NL,
              (unsigned long)sc);
    app_assert_status(sc);
    return;
  }
  log_info(APP_PREFIX "Default connection interval set to %u (%u max "
                      "connection(s) configured)" NL,
           connection_parameters.max_connection_interval,
           max_connections);
}
