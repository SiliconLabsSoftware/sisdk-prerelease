/***************************************************************************//**
 * @file
 * @brief CS ACP host application
 *
 * Reference implementation of a CS host with initiator and reflector support.
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
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <math.h>
#include <stdio.h>
#include "app.h"
#include "ncp_host.h"
#include "app_log.h"
#include "app_log_cli.h"
#include "app_assert.h"
#include "sl_bt_api.h"
#include "host_comm.h"
#include "gatt_db.h"
#include "sl_bt_peer_manager_central.h"
#include "sl_bt_peer_manager_peripheral.h"
#include "sl_bt_peer_manager_filter.h"
#include "sl_bt_peer_security.h"
#include "cs_common.h"
#include "cs_manager.h"
#include "cs_manager_config.h"
#include "cs_ras_common.h"
#include "app_cs_discovery.h"
#include "app_config.h"
#include "cs_acp_common.h"
#include "cs_acp_host.h"

// MAX_CONNECTIONS should be less or equal to the SL_BT_CONFIG_MAX_CONNECTIONS in the CS NCP example.
// Sum of reflector and initiator instances should not exceed MAX_CONNECTIONS.
#define MAX_CONNECTIONS                             CS_MANAGER_CONFIG_MAX_INSTANCES
// MAX_INITIATOR_INSTANCES should be less or equal to the CS_INITIATOR_MAX_CONNECTIONS in the CS NCP example.
#define MAX_INITIATOR_INSTANCES                     MAX_CONNECTIONS
// MAX_REFLECTOR_INSTANCES should be less or equal to the SL_BT_CONFIG_MAX_CONNECTIONS in the CS NCP example.
#define MAX_REFLECTOR_INSTANCES                     MAX_CONNECTIONS

// Connection TX power settings
#define CS_INITIATOR_MIN_TX_POWER_DBM               -3
#define CS_INITIATOR_MAX_TX_POWER_DBM               20

#define SL_BT_CS_DEVICE_NAME_MAX_LEN 32

// Optstring argument for getopt
#define OPTSTRING NCP_HOST_OPTSTRING APP_LOG_OPTSTRING "m:R:I:F:wo:p:a:q:s:d:TPhM:S"

// Usage info
#define USAGE APP_LOG_NL "%s " NCP_HOST_USAGE APP_LOG_USAGE                                                \
  "\n[-m <cs_main_mode>] [-M <cs_sub_mode>] [-R <max_reflector_instances>] [-I <max_initiator_instances>]" \
  "[-F <reflector_ble_address>] [-w] [-o] [-p <channel_map_preset>] [-a <cs_tone_antenna_config_idx_req>]" \
  "[-q <cs_sync_antenna_req>] [-s <cs_procedure_scheduling>] [-d <device_name>] [-T] [-P] [-S] [-h]" APP_LOG_NL

// Detailed argument list
#define CS_HOST_OPTIONS                                                                     \
  "    -m  CS main mode.\n"                                                                 \
  "        <cs_main_mode> Integer representing CS main mode, default: 2, PBR.\n"            \
  "        Used only for initiator instances\n"                                             \
  "        1 : RTT\n"                                                                       \
  "        2 : PBR\n"                                                                       \
  "        Note: see -M\n"                                                                  \
  "    -M  CS sub mode\n"                                                                   \
  "        <cs_sub_mode> Integer representing CS sub mode, default: No sub mode\n"          \
  "        Used only for initiator instances\n"                                             \
  "        1: RTT\n"                                                                        \
  "        255: no sub mode\n"                                                              \
  "        Note: Only main mode = PBR, sub mode = RTT supported now\n"                      \
  "        see -m\n"                                                                        \
  "    -R  Maximum number of reflector instances, default: 1\n"                             \
  "        <max_reflector_instances>\n"                                                     \
  "    -I  Maximum number of initiator instances, default: 1\n"                             \
  "        <max_initiator_instances>\n"                                                     \
  "    -F  Enable reflector BLE address filtering in format:\n"                             \
  "        AA:BB:CC:DD:EE:FF or aabbccddeeff\n"                                             \
  "        <reflector_ble_address>\n"                                                       \
  "        Used only for initiator instances\n"                                             \
  "    -w  Use wired antenna offset\n"                                                      \
  "        Note: now controlled by the target's CS_ANTENNA_CONFIG_DEFAULT_ANTENNA_OFFSET\n" \
  "        build option; the flag is kept for CLI backward compatibility.\n"                \
  "    -o  Object tracking mode, default: 2\n"                                              \
  "        Used only for initiator instances\n"                                             \
  "        0 : Tracking accuracy optimized (suitable for moving targets)\n"                 \
  "            (TRACKING_ACCURACY_OPTIMIZED)\n"                                             \
  "        1 : Stationary (suitable for stationary targets) (STATIONARY)\n"                 \
  "        2 : Tracking latency optimized (suitable for fast moving targets)\n"             \
  "            (TRACKING_LATENCY_OPTIMIZED)\n"                                              \
  "    -p  Pre-set parameters for channel map selection, default: 2\n"                      \
  "        Used only for initiator instances\n"                                             \
  "        1 : medium (channel spacing: 2, number of channels: 38)\n"                       \
  "        2 : high (channel spacing: 1, number of channels: 72)\n"                         \
  "        3 : load custom from configuration macro CS_CUSTOM_CHANNEL_MAP\n"                \
  "    -a  Antenna configuration index for antenna switching, default: 7\n"                 \
  "        Used only for initiator instances\n"                                             \
  "        0 : Single antennas on both sides\n"                                             \
  "        1 : Dual antenna initiator & single antenna reflector\n"                         \
  "        2 : Triple antenna initiator & single antenna reflector\n"                       \
  "        3 : Quad antenna initiator & single antenna reflector\n"                         \
  "        4 : Single antenna initiator & dual antenna reflector\n"                         \
  "        5 : Single antenna initiator & triple antenna reflector\n"                       \
  "        6 : Single antenna initiator & quad antenna reflector\n"                         \
  "        7 : Dual antennas on both sides\n"                                               \
  "        Note: considered only with CS main mode: PBR!\n"                                 \
  "    -q  Antenna usage for CS SYNC packets, default: 0xFE\n"                              \
  "        Used for both initiator and reflector instances\n"                               \
  "        1 : use antenna ID1 only\n"                                                      \
  "        2 : use antenna ID2 only\n"                                                      \
  "        3 : use antenna ID3 only\n"                                                      \
  "        4 : use antenna ID4 only\n"                                                      \
  "        0xFE : Switching between antennas for each channel\n"                            \
  "        Note: considered only with CS main mode: RTT!\n"                                 \
  "    -s  Optimized procedure scheduling\n"                                                \
  "        Used only for initiator instances\n"                                             \
  "        0 : Optimized for frequency update\n"                                            \
  "        1 : Optimized for energy consumption\n"                                          \
  "        2 : Custom\n"                                                                    \
  "        Note: Default is frequency optimized procedure scheduling\n"                     \
  "    -d  Device name.\n"                                                                  \
  "    -T  Enable RTT trace including BGAPI messages and RTL log.\n"                        \
  "        Note that the RTT blocks the target if no client is connected.\n"                \
  "    -P  Use 1M connection PHY\n"                                                         \
  "        Used only for initiator instances\n"                                             \
  "        Note: Default is 2M\n"                                                           \
  "    -S  Enable synchronized mode\n"                                                      \
  "        max_procedure_count = 1\n"

// Options info
#define OPTIONS    \
  "\nOPTIONS\n"    \
  NCP_HOST_OPTIONS \
  APP_LOG_OPTIONS  \
  CS_HOST_OPTIONS  \
  "    -h  Print this help message.\n"

#define BT_ADDR_LEN 6u
#define BT_MAX_CHANNELS 79u

typedef struct {
  bd_addr accepted_bt_address_list[MAX_REFLECTOR_INSTANCES];
  uint8_t accepted_bt_address_count;
  uint8_t max_reflector_instances;
  uint8_t max_initiator_instances;
  bool trace;
  char device_name[SL_BT_CS_DEVICE_NAME_MAX_LEN];
} cs_host_config_t;

typedef struct {
  uint8_t num_reflector_connections;
  uint8_t num_initiator_connections;
  uint8_t reflector_conn_handles[MAX_INITIATOR_INSTANCES];
  uint8_t initiator_conn_handles[MAX_REFLECTOR_INSTANCES];
  bool trace;
} cs_host_state_t;

// CLI parameters are applied after app_cs_get_default_config(), overriding the default configuration
// when CLI arguments are provided.
typedef struct {
  bool main_mode_set;
  uint8_t main_mode;
  bool sub_mode_set;
  uint8_t sub_mode;
  bool conn_phy_1m;
  bool algo_mode_set;
  uint8_t algo_mode;
  bool channel_map_preset_set;
  uint8_t channel_map_preset;
  bool tone_antenna_config_idx_set;
  uint8_t tone_antenna_config_idx;
  bool cs_sync_antenna_set;
  uint8_t cs_sync_antenna;
  bool procedure_scheduling_set;
  uint8_t procedure_scheduling;
  bool synchronized_mode;
  bool use_antenna_wired_offset;       // (kept for compatibility)
} cs_host_cli_overrides_t;

cs_host_config_t cs_host_config = {
  .accepted_bt_address_count = 0u,
  .max_reflector_instances = 0u,
  .max_initiator_instances = 0u,
  .trace = false,
  .device_name = REFLECTOR_DEVICE_NAME
};

cs_host_state_t cs_host_state = {
  .num_reflector_connections = 0u,
  .num_initiator_connections = 0u,
  .reflector_conn_handles = { SL_BT_INVALID_CONNECTION_HANDLE },
  .initiator_conn_handles = { SL_BT_INVALID_CONNECTION_HANDLE },
  .trace = false
};

static cs_host_cli_overrides_t cli_overrides = { 0 };

// Per-connection state for instances where the host plays the Initiator role.
static initiator_instance_t cs_initiator_instances[MAX_INITIATOR_INSTANCES];

// Cached RAS discovery result for the in-progress initiator-role connection
// setup. Populated by on_ras_discovery_complete() and consumed by
// app_cs_create_new_initiator_instance() in
// sl_bt_evt_cs_read_remote_supported_capabilities_complete_id.
static app_cs_discovery_result_t discovery_config = {
  .conn_handle = SL_BT_INVALID_CONNECTION_HANDLE,
  .service_handle = CS_RAS_INVALID_SERVICE_HANDLE,
  .gattdb_handles = { .array = { CS_RAS_INVALID_CHARACTERISTIC_HANDLE } },
  .status = SL_STATUS_FAIL
};

// Connection currently being set up as an initiator-role connection (security
// increase / RAS discovery / capability read). Used to gate scanning so we
// don't open multiple parallel setups.
static uint8_t connection_setup_handle = SL_BT_INVALID_CONNECTION_HANDLE;

static void on_connection_opened_with_reflector(uint8_t conn_handle);
static void on_connection_opened_with_initiator(uint8_t conn_handle);
static void on_connection_closed(uint8_t conn_handle);
static void on_ras_discovery_complete(const app_cs_discovery_result_t *result);
static void try_start_scanning(void);
static void try_start_advertising(void);
static void clear_discovery_config(uint8_t conn_handle);
static void stop_procedure(void);
static void enable_trace(void);

/******************************************************************************
 * Application initialization
 *****************************************************************************/
void app_cli_init(int argc, char *argv[])
{
  sl_status_t sc;
  int cli_opt;
  bool max_initiator_instances_set = false;
  bool max_reflector_instances_set = false;
  unsigned int arg_data;
  int position;

  // Initialize the list of accepted BT addresses
  for (uint32_t i = 0u; i < MAX_REFLECTOR_INSTANCES; i++) {
    memset(cs_host_config.accepted_bt_address_list[i].addr, 0xFF, BT_ADDR_LEN);
  }

  app_log_info("+-[CS Host by Silicon Labs]------------------------+" APP_LOG_NL);
  app_log_info("+--------------------------------------------------+" APP_LOG_NL APP_LOG_NL);

  // Process command line options
  while ((cli_opt = getopt(argc, argv, OPTSTRING)) != -1) {
    switch (cli_opt) {
      // Print help
      case 'h':
        app_log(USAGE, argv[0]);
        app_log(OPTIONS);
        exit(EXIT_SUCCESS);

      case 'm':
        // Mode.
        // 1 - RTT
        // 2 - Phase based
        cli_overrides.main_mode = (uint8_t)atoi(optarg);
        cli_overrides.main_mode_set = true;
        break;

      case 'M':
        cli_overrides.sub_mode = (uint8_t)atoi(optarg);
        cli_overrides.sub_mode_set = true;
        break;

      case 'R':
        cs_host_config.max_reflector_instances = atoi(optarg);
        max_reflector_instances_set = true;
        if (cs_host_config.max_reflector_instances > MAX_REFLECTOR_INSTANCES) {
          app_log_error(APP_PREFIX "Invalid number of 'maximum reflectors' argument (%d) provided, "
                                   "must be in the range of 0 to %u" APP_LOG_NL,
                        cs_host_config.max_reflector_instances,
                        MAX_REFLECTOR_INSTANCES);
          exit(EXIT_FAILURE);
        }
        break;

      case 'I':
        cs_host_config.max_initiator_instances = atoi(optarg);
        max_initiator_instances_set = true;
        if (cs_host_config.max_initiator_instances > MAX_INITIATOR_INSTANCES) {
          app_log_error(APP_PREFIX "Invalid number of 'maximum initiators' argument (%d) provided, "
                                   "must be in the range of 0 to %u" APP_LOG_NL,
                        cs_host_config.max_initiator_instances,
                        MAX_INITIATOR_INSTANCES);
          exit(EXIT_FAILURE);
        }
        break;

      case 'F':
      {
        // Check if we have room for more accepted addresses
        if (cs_host_config.accepted_bt_address_count >= MAX_REFLECTOR_INSTANCES) {
          app_log_error(APP_PREFIX "Maximum number of accepted BLE addresses (%u) reached, "
                                   "ignoring additional addresses" APP_LOG_NL,
                        MAX_REFLECTOR_INSTANCES);
          break;
        }
        // Add the accepted BLE address to the list
        bd_addr *bt_addr_to_add = &cs_host_config.accepted_bt_address_list[cs_host_config.accepted_bt_address_count];
        sc = sl_bt_peer_manager_str_to_address(optarg, bt_addr_to_add);
        if (sc != SL_STATUS_OK) {
          app_log_error(APP_PREFIX "Invalid BLE address filter provided" APP_LOG_NL);
          exit(EXIT_FAILURE);
        }
        app_log_info(APP_PREFIX "BLE address accept filter added for: '%02X:%02X:%02X:%02X:%02X:%02X'" APP_LOG_NL,
                     bt_addr_to_add->addr[5],
                     bt_addr_to_add->addr[4],
                     bt_addr_to_add->addr[3],
                     bt_addr_to_add->addr[2],
                     bt_addr_to_add->addr[1],
                     bt_addr_to_add->addr[0]);
        cs_host_config.accepted_bt_address_count++;
      }
      break;

      case 'w':
        cli_overrides.use_antenna_wired_offset = true;
        break;

      case 'P':
        cli_overrides.conn_phy_1m = true;
        break;

      case 'o':
      {
        int object_tracking_mode = atoi(optarg);
        if (object_tracking_mode != CS_ALGO_MODE_TRACKING_ACCURACY_OPTIMIZED
            && object_tracking_mode != CS_ALGO_MODE_STATIONARY
            && object_tracking_mode != CS_ALGO_MODE_TRACKING_LATENCY_OPTIMIZED) {
          app_log_error(APP_PREFIX "Invalid object tracking mode (%d) provided!" APP_LOG_NL,
                        object_tracking_mode);
          exit(EXIT_FAILURE);
        }
        cli_overrides.algo_mode = (uint8_t)object_tracking_mode;
        cli_overrides.algo_mode_set = true;
      }
      break;

      case 'p':
      {
        int preset = atoi(optarg);
        if (preset != CS_CHANNEL_MAP_PRESET_MEDIUM
            && preset != CS_CHANNEL_MAP_PRESET_HIGH
            && preset != CS_CHANNEL_MAP_PRESET_CUSTOM) {
          app_log_error(APP_PREFIX "Unsupported preset (%d) provided!" APP_LOG_NL, preset);
          exit(EXIT_FAILURE);
        }
        cli_overrides.channel_map_preset = (uint8_t)preset;
        cli_overrides.channel_map_preset_set = true;
        app_log_info(APP_PREFIX "Preset (%d) provided!" APP_LOG_NL, preset);
      }
      break;

      case 'a':
      {
        int cs_tone_antenna_config_idx_req = atoi(optarg);
        if (cs_tone_antenna_config_idx_req != CS_ANTENNA_CONFIG_INDEX_SINGLE_ONLY
            && cs_tone_antenna_config_idx_req != CS_ANTENNA_CONFIG_INDEX_DUAL_LOCAL_SINGLE_REMOTE
            && cs_tone_antenna_config_idx_req != CS_ANTENNA_CONFIG_INDEX_TRIPLE_LOCAL_SINGLE_REMOTE
            && cs_tone_antenna_config_idx_req != CS_ANTENNA_CONFIG_INDEX_QUAD_LOCAL_SINGLE_REMOTE
            && cs_tone_antenna_config_idx_req != CS_ANTENNA_CONFIG_INDEX_SINGLE_LOCAL_DUAL_REMOTE
            && cs_tone_antenna_config_idx_req != CS_ANTENNA_CONFIG_INDEX_SINGLE_LOCAL_TRIPLE_REMOTE
            && cs_tone_antenna_config_idx_req != CS_ANTENNA_CONFIG_INDEX_SINGLE_LOCAL_QUAD_REMOTE
            && cs_tone_antenna_config_idx_req != CS_ANTENNA_CONFIG_INDEX_DUAL_ONLY) {
          app_log_error(APP_PREFIX "Invalid antenna usage for PBR (%d) provided!" APP_LOG_NL,
                        cs_tone_antenna_config_idx_req);
          exit(EXIT_FAILURE);
        }
        cli_overrides.tone_antenna_config_idx = (uint8_t)cs_tone_antenna_config_idx_req;
        cli_overrides.tone_antenna_config_idx_set = true;
      }
      break;

      case 'q':
      {
        int items_read = sscanf(optarg, "%x%n", &arg_data, &position);
        if (items_read != 1 || (size_t)position != strlen(optarg)) {
          app_log_error(APP_PREFIX "Invalid antenna usage for RTT (%s) provided!" APP_LOG_NL, optarg);
          exit(EXIT_FAILURE);
        }
        if (arg_data != CS_SYNC_ANTENNA_1
            && arg_data != CS_SYNC_ANTENNA_2
            && arg_data != CS_SYNC_ANTENNA_3
            && arg_data != CS_SYNC_ANTENNA_4
            && arg_data != CS_SYNC_SWITCHING) {
          app_log_error(APP_PREFIX "Invalid antenna usage for RTT (%u) provided!" APP_LOG_NL, arg_data);
          exit(EXIT_FAILURE);
        }
        cli_overrides.cs_sync_antenna = (uint8_t)arg_data;
        cli_overrides.cs_sync_antenna_set = true;
      }
      break;

      case 'd':
      {
        // Device name
        size_t device_name_len = strlen(optarg);
        if (device_name_len > SL_BT_CS_DEVICE_NAME_MAX_LEN) {
          app_log_error(APP_PREFIX "Device name too long (%llu), maximum length is %d characters!" APP_LOG_NL,
                        ((unsigned long long)device_name_len),
                        SL_BT_CS_DEVICE_NAME_MAX_LEN);
          exit(EXIT_FAILURE);
        }
        strncpy((char *)cs_host_config.device_name, optarg, SL_BT_CS_DEVICE_NAME_MAX_LEN);
        cs_host_config.device_name[SL_BT_CS_DEVICE_NAME_MAX_LEN - 1] = '\0'; // Ensure null termination
      }
      break;

      case 'T':
        cs_host_config.trace = true;
        break;

      case 's':
      {
        int procedure_scheduling = atoi(optarg);
        if (procedure_scheduling != CS_PROCEDURE_SCHEDULING_OPTIMIZED_FOR_FREQUENCY
            && procedure_scheduling != CS_PROCEDURE_SCHEDULING_OPTIMIZED_FOR_ENERGY
            && procedure_scheduling != CS_PROCEDURE_SCHEDULING_CUSTOM) {
          app_log_error(APP_PREFIX "Unsupported procedure scheduling (%d) provided!" APP_LOG_NL,
                        procedure_scheduling);
          exit(EXIT_FAILURE);
        }
        cli_overrides.procedure_scheduling = (uint8_t)procedure_scheduling;
        cli_overrides.procedure_scheduling_set = true;
      }
      break;

      case 'S':
        cli_overrides.synchronized_mode = true;
        break;

      default:
        sc = ncp_host_set_option((char)cli_opt, optarg);
        if (sc == SL_STATUS_NOT_FOUND) {
          sc = app_log_set_option((char)cli_opt, optarg);
        }
        if (sc != SL_STATUS_OK) {
          app_log(USAGE, argv[0]);
          exit(EXIT_FAILURE);
        }
        break;
    }
  }

  // Check for unknown parameters and write them to console if there is any
  if (optind < argc) {
    app_log_critical("Unknown %d parameter", (argc - optind));
    if (argc - optind > 1) {
      app_log_append_critical("s \'");
      for (int i = optind; i < argc; i++) {
        app_log_append_critical("%s", argv[i]);
        if (i < argc - 1) {
          app_log_append_critical(", ");
        }
      }
      app_log_append_critical("\'");
    } else {
      app_log_append_critical(" \'%s\'", argv[optind]);
    }
    app_log_append_critical("! Usage:" APP_LOG_NL);

    app_log(USAGE, argv[0]);
    exit(EXIT_FAILURE);
  }

  // Sanity check of argument combinations
  if ((cs_host_config.max_initiator_instances + cs_host_config.max_reflector_instances) > MAX_CONNECTIONS) {
    app_log_info(APP_PREFIX "Sum of <max_initiator_instances> and <max_reflector_instances> "
                            "exceeds maximum connection count (%u)" APP_LOG_NL,
                 MAX_CONNECTIONS);
    exit(EXIT_FAILURE);
  }

  if (cs_host_config.max_initiator_instances == 0 && cs_host_config.max_reflector_instances == 0) {
    if (!max_initiator_instances_set && !max_reflector_instances_set) {
      cs_host_config.max_initiator_instances = 1;
      cs_host_config.max_reflector_instances = 1;
      app_log_info(APP_PREFIX "Not specified <max_initiator_instances> and "
                              "<max_reflector_instances>. Using 1-1 of each" APP_LOG_NL);
    } else {
      app_log_error(APP_PREFIX "<max_initiator_instances> and <max_reflector_instances>"
                               " cannot be both zero" APP_LOG_NL);
      exit(EXIT_FAILURE);
    }
  }

  if (cs_host_config.max_initiator_instances > 0) {
    uint8_t main_mode = cli_overrides.main_mode_set
                        ? cli_overrides.main_mode
                        : (uint8_t)sl_bt_cs_mode_pbr;
    uint8_t sub_mode = cli_overrides.sub_mode_set
                       ? cli_overrides.sub_mode
                       : (uint8_t)sl_bt_cs_submode_disabled;

    // Currently only main mode = pbr and submode = rtt is supported as a
    // mixed combination. Force preset HIGH if so.
    if (main_mode == sl_bt_cs_mode_pbr && sub_mode == sl_bt_cs_mode_rtt) {
      cli_overrides.channel_map_preset = (uint8_t)CS_CHANNEL_MAP_PRESET_HIGH;
      cli_overrides.channel_map_preset_set = true;
      app_log_info(APP_PREFIX "Channel map preset set to high" APP_LOG_NL);
    }

    if (!((main_mode == (uint8_t)sl_bt_cs_mode_pbr
           && sub_mode == (uint8_t)sl_bt_cs_mode_rtt)
          || sub_mode == (uint8_t)sl_bt_cs_submode_disabled)) {
      app_log_error(APP_PREFIX "Invalid CS mode/submode (%u/%u) provided" APP_LOG_NL,
                    (unsigned)main_mode,
                    (unsigned)sub_mode);
      exit(EXIT_FAILURE);
    }
    if (main_mode == sl_bt_cs_mode_rtt
        && cli_overrides.channel_map_preset_set
        && cli_overrides.channel_map_preset != CS_CHANNEL_MAP_PRESET_HIGH) {
      app_log_error(APP_PREFIX "Only preset HIGH is supported with RTT mode!" APP_LOG_NL);
      exit(EXIT_FAILURE);
    }
    if (main_mode == sl_bt_cs_mode_rtt && sub_mode == sl_bt_cs_mode_rtt) {
      app_log_error(APP_PREFIX "CS main mode RTT and CS sub mode RTT combination is not supported!" APP_LOG_NL);
      exit(EXIT_FAILURE);
    }
    if (cli_overrides.tone_antenna_config_idx_set && main_mode == sl_bt_cs_mode_rtt) {
      app_log_warning(APP_PREFIX "PBR antenna configuration is omitted in RTT mode!" APP_LOG_NL);
    }
    if (cli_overrides.cs_sync_antenna_set && main_mode == sl_bt_cs_mode_pbr) {
      app_log_warning(APP_PREFIX "RTT antenna configuration is omitted in PBR mode!" APP_LOG_NL);
    }
    if (cli_overrides.algo_mode_set
        && cli_overrides.algo_mode == CS_ALGO_MODE_TRACKING_LATENCY_OPTIMIZED
        && main_mode == sl_bt_cs_mode_rtt) {
      app_log_error(APP_PREFIX "Tracking latency optimized mode is not supported with main mode RTT!" APP_LOG_NL);
      exit(EXIT_FAILURE);
    }
    // Log mode based on synchronized flag
    if (cli_overrides.synchronized_mode) {
      app_log_info(APP_PREFIX "Start new procedure after one finished." APP_LOG_NL);
    } else {
      app_log_info(APP_PREFIX "Free running." APP_LOG_NL);
    }
    // Log procedure scheduling
    if (cli_overrides.procedure_scheduling_set) {
      switch (cli_overrides.procedure_scheduling) {
        case CS_PROCEDURE_SCHEDULING_OPTIMIZED_FOR_FREQUENCY:
          app_log_info(APP_PREFIX "Using frequency optimized procedure scheduling." APP_LOG_NL);
          break;
        case CS_PROCEDURE_SCHEDULING_OPTIMIZED_FOR_ENERGY:
          app_log_info(APP_PREFIX "Using energy optimized procedure scheduling." APP_LOG_NL);
          break;
        case CS_PROCEDURE_SCHEDULING_CUSTOM:
        default:
          app_log_info(APP_PREFIX "Using custom procedure scheduling." APP_LOG_NL);
          break;
      }
    }
  } else {
    app_log_info(APP_PREFIX "Only reflector instances - most parameters will be configured by the initiator" APP_LOG_NL);
  }

  if (cli_overrides.use_antenna_wired_offset) {
    app_log_warning(APP_PREFIX "-w is now controlled by the target's "
                               "CS_ANTENNA_CONFIG_DEFAULT_ANTENNA_OFFSET build option." APP_LOG_NL);
  }

  // Log host-level configuration parameters. CS-specific parameters (mode,
  // antenna, scheduling, etc.) are logged later from app_cs_log_default_config()
  // in app_init() — that path reflects the post-override config.
  app_log_info("+-------------------------------------------------------+" APP_LOG_NL);
  app_log_info(APP_PREFIX "Maximum number of reflector instances: %u" APP_LOG_NL,
               cs_host_config.max_reflector_instances);
  app_log_info(APP_PREFIX "Maximum number of initiator instances: %u" APP_LOG_NL,
               cs_host_config.max_initiator_instances);
  if (cs_host_config.max_initiator_instances > 0) {
    if (cs_host_config.accepted_bt_address_count > 0) {
      app_log_info(APP_PREFIX "BLE address filtering: enabled with %u addresses" APP_LOG_NL,
                   cs_host_config.accepted_bt_address_count);
    } else {
      app_log_info(APP_PREFIX "BLE address filtering: disabled" APP_LOG_NL);
    }
  }
  app_log_info("+-------------------------------------------------------+" APP_LOG_NL);

  // Initialize the list of reflector-side connection handles
  for (uint32_t i = 0u; i < MAX_INITIATOR_INSTANCES; i++) {
    cs_host_state.reflector_conn_handles[i] = SL_BT_INVALID_CONNECTION_HANDLE;
  }
  // Initialize the list of initiator-side connection handles
  for (uint32_t i = 0u; i < MAX_REFLECTOR_INSTANCES; i++) {
    cs_host_state.initiator_conn_handles[i] = SL_BT_INVALID_CONNECTION_HANDLE;
  }

  // Initialize the NCP connection
  sc = ncp_host_init();
  if (sc == SL_STATUS_INVALID_PARAMETER) {
    app_log(USAGE, argv[0]);
    exit(EXIT_FAILURE);
  }
  app_assert_status(sc);
  app_log_info(APP_PREFIX "NCP host initialized" APP_LOG_NL);
  app_log_info(APP_PREFIX "Press Crtl+C to quit" APP_LOG_NL);
  app_log_info("+-------------------------------------------------------+" APP_LOG_NL APP_LOG_NL);
}

void app_init(void)
{
  // Initialize per-connection initiator instance state.
  for (uint32_t i = 0u; i < MAX_INITIATOR_INSTANCES; i++) {
    cs_initiator_instances[i].conn_handle = SL_BT_INVALID_CONNECTION_HANDLE;
    cs_initiator_instances[i].measurement_cnt = 0u;
    cs_initiator_instances[i].ranging_counter = 0u;
    memset(&cs_initiator_instances[i].measurement_mainmode, 0, sizeof(cs_measurement_data_t));
    memset(&cs_initiator_instances[i].measurement_submode, 0, sizeof(cs_measurement_data_t));
    memset(&cs_initiator_instances[i].measurement_progress, 0, sizeof(cs_intermediate_result_t));
    cs_initiator_instances[i].measurement_arrived = false;
    cs_initiator_instances[i].measurement_progress_changed = false;
    cs_initiator_instances[i].ras_discovery = false;
    cs_initiator_instances[i].read_capabilities = false;
    cs_initiator_instances[i].security_increase = false;
    cs_initiator_instances[i].number_of_measurements = 0u;
  }

  // Wire CS Manager / RREQ / Algo callbacks (registered against the host's
  // shared cs_manager event handler in app_cs.c).
  cs_algo_event_callback_t algo_cb = {
    .on_result = app_on_result,
    .on_intermediate_result = app_on_intermediate_result,
    .on_error = app_on_cs_algo_on_error,
  };
  app_cs_set_callbacks(algo_cb);

  // Initiator-role default config (applied to host as central / initiator).
  app_cs_get_default_config();

  // Apply CLI overrides on top of the default configuration
  if (cli_overrides.main_mode_set) {
    app_cs_set_main_mode(cli_overrides.main_mode);
  }
  if (cli_overrides.sub_mode_set) {
    app_cs_set_sub_mode(cli_overrides.sub_mode);
  }
  if (cli_overrides.conn_phy_1m) {
    app_cs_set_conn_phy(sl_bt_gap_phy_1m);
  }
  if (cli_overrides.algo_mode_set) {
    app_cs_set_algo_mode(cli_overrides.algo_mode);
  }
  if (cli_overrides.channel_map_preset_set) {
    app_cs_set_channel_map_preset((cs_channel_map_preset_t)cli_overrides.channel_map_preset);
  }
  if (cli_overrides.tone_antenna_config_idx_set) {
    app_cs_set_tone_antenna_config_idx(cli_overrides.tone_antenna_config_idx);
  }
  if (cli_overrides.cs_sync_antenna_set) {
    app_cs_set_cs_sync_antenna(cli_overrides.cs_sync_antenna);
  }
  if (cli_overrides.procedure_scheduling_set) {
    app_cs_set_procedure_scheduling((cs_procedure_scheduling_t)cli_overrides.procedure_scheduling);
  }
  if (cli_overrides.synchronized_mode) {
    app_cs_set_max_procedure_count(1u);
  }

  app_cs_log_default_config();
}

void sl_bt_on_event(sl_bt_msg_t *evt)
{
  sl_status_t sc;
  initiator_instance_t *initiator;

  // Forward every event to the RAS discovery state machine
  app_cs_discovery_on_bt_event(evt);

  switch (SL_BT_MSG_ID(evt->header)) {
    // --------------------------------
    case sl_bt_evt_system_boot_id:
    {
      if (cs_host_config.trace) {
        enable_trace();
      }

      // Set TX power
      int16_t min_tx_power_x10 = CS_INITIATOR_MIN_TX_POWER_DBM * 10;
      int16_t max_tx_power_x10 = CS_INITIATOR_MAX_TX_POWER_DBM * 10;
      sc = sl_bt_system_set_tx_power(min_tx_power_x10,
                                     max_tx_power_x10,
                                     &min_tx_power_x10,
                                     &max_tx_power_x10);
      app_assert_status(sc);
      app_log_debug(APP_PREFIX "Minimum system TX power is set to: %d dBm" APP_LOG_NL, min_tx_power_x10 / 10);
      app_log_debug(APP_PREFIX "Maximum system TX power is set to: %d dBm" APP_LOG_NL, max_tx_power_x10 / 10);

      sl_bt_peer_manager_central_init();
      sl_bt_peer_manager_peripheral_init();
      sl_bt_peer_manager_filter_init();

      // Print the Bluetooth address
      bd_addr address;
      uint8_t address_type;
      sc = sl_bt_gap_get_identity_address(&address, &address_type);
      app_assert_status(sc);
      app_log_info(APP_PREFIX "Bluetooth %s address: %02X:%02X:%02X:%02X:%02X:%02X" APP_LOG_NL,
                   address_type ? "static random" : "public device",
                   address.addr[5],
                   address.addr[4],
                   address.addr[3],
                   address.addr[2],
                   address.addr[1],
                   address.addr[0]);

      // Filter for advertised name
      sc = sl_bt_peer_manager_set_filter_device_name(cs_host_config.device_name,
                                                     strlen(cs_host_config.device_name),
                                                     false);
      app_assert_status(sc);

      // Filter for BLE address if enabled
      if (cs_host_config.accepted_bt_address_count > 0) {
        sl_bt_peer_manager_set_filter_bt_address(true);
        for (uint32_t i = 0u; i < cs_host_config.accepted_bt_address_count; i++) {
          sc = sl_bt_peer_manager_add_allowed_bt_address(&cs_host_config.accepted_bt_address_list[i]);
          app_assert_status(sc);
        }
      }

      // Derive optimized default connection parameters and apply them as the
      // CS Manager baseline before any connection is opened, so connections
      // open at the optimized interval without a per-connection update.
      app_cs_set_default_connection_parameters();

      // Initiator role: start scanning for reflector connections.
      if (cs_host_config.max_initiator_instances > 0) {
        try_start_scanning();
      }

      // Reflector role: start advertising for initiator connections.
      if (cs_host_config.max_reflector_instances > 0) {
        try_start_advertising();
      }

      break;
    }
    // --------------------------------
    // Connection parameters update.
    //
    // For initiator-role connections (host = central): drives security
    // increase + RAS discovery + remote-capability read
    //
    // For reflector-role connections (host = peripheral): nothing to do;
    // the remote initiator drives the procedure.
    case sl_bt_evt_connection_parameters_id:
    {
      uint8_t connection = evt->data.evt_connection_parameters.connection;
      if (app_is_reflector_connection(connection)) {
        app_log_debug(APP_INSTANCE_PREFIX "Connection parameters changed (reflector role)" APP_LOG_NL,
                      connection);
        break;
      }
      initiator = app_get_instance(connection);
      if (initiator == NULL) {
        break;
      }
      if (evt->data.evt_connection_parameters.security_mode != sl_bt_connection_mode1_level1) {
        if (!initiator->ras_discovery) {
          connection_setup_handle = connection;
          sc = app_cs_discovery_start_discovery(connection,
                                                on_ras_discovery_complete);
          if (sc != SL_STATUS_OK && sc != SL_STATUS_IN_PROGRESS) {
            app_log_error(APP_INSTANCE_PREFIX "Failed to start RAS discovery! [sc: 0x%lx]" APP_LOG_NL,
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
          app_log_info(APP_INSTANCE_PREFIX "Increasing security..." APP_LOG_NL, connection);
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
      uint8_t connection = evt->data.evt_connection_phy_status.connection;
      app_on_connection_phy_changed(evt->data.evt_connection_phy_status.phy);
      app_log_debug(APP_INSTANCE_PREFIX "PHY update procedure completed" APP_LOG_NL, connection);
      break;
    }

    // --------------------------------
    // Remote CS capabilities read completed (initiator role only).
    case sl_bt_evt_cs_read_remote_supported_capabilities_complete_id:
    {
      uint8_t connection =
        evt->data.evt_cs_read_remote_supported_capabilities_complete.connection;
      log_info(APP_INSTANCE_PREFIX "Remote capabilities arrived." NL, connection);
      initiator = app_get_instance(connection);
      if (initiator == NULL) {
        app_log_error(APP_INSTANCE_PREFIX "Failed to get instance number for new connection!" APP_LOG_NL,
                      connection);
        break;
      }

      if (!initiator->read_capabilities) {
        app_log_warning(APP_INSTANCE_PREFIX "Capabilities read by the remote" NL, connection);
        break;
      }
      initiator->read_capabilities = false;

      // Check supported capabilities
      app_cs_check_supported_capabilities(evt);
      // Optimize parameters
      app_cs_optimize_parameters(connection);

      app_log_info(APP_INSTANCE_PREFIX "Creating new initiator instance" APP_LOG_NL, connection);
      if (cs_host_state.num_reflector_connections >= cs_host_config.max_initiator_instances) {
        app_log_error(APP_INSTANCE_PREFIX "Maximum number of initiator instances (%u) reached, "
                                          "dropping connection..." APP_LOG_NL,
                      connection,
                      cs_host_config.max_initiator_instances);
        cs_on_error(connection, CS_APP_ERROR_INITIATOR_INSTANCE_LIST_FULL, SL_STATUS_FULL);
        break;
      }

      initiator->measurement_cnt = 0u;
      memset(&initiator->measurement_mainmode, 0, sizeof(cs_measurement_data_t));
      memset(&initiator->measurement_submode, 0, sizeof(cs_measurement_data_t));
      memset(&initiator->measurement_progress, 0, sizeof(cs_intermediate_result_t));

      sc = app_cs_create_new_initiator_instance(connection, &discovery_config);
      if (sc != SL_STATUS_OK) {
        app_log_error(APP_INSTANCE_PREFIX "Failed to create initiator instance, error:0x%lx" APP_LOG_NL,
                      connection,
                      (unsigned long)sc);
        cs_on_error(connection, CS_APP_ERROR_INITIATOR_INSTANCE_CREATE_FAILED, sc);
      } else {
        app_log_info(APP_INSTANCE_PREFIX "New initiator instance created" APP_LOG_NL,
                     connection);
      }
      break;
    }

    // --------------------------------
    case sl_bt_evt_gatt_mtu_exchanged_id:

      app_on_mtu_changed(evt->data.evt_gatt_mtu_exchanged.mtu);
      break;

    // --------------------------------
    case sl_bt_evt_user_message_to_host_id:
    {
      sl_bt_evt_user_message_to_host_t *msg = &evt->data.evt_user_message_to_host;
      if (msg->message.data[0] == 1) {
        sl_bt_peer_security_send_confirmation(true);
      } else {
        sl_bt_peer_security_send_confirmation(false);
      }
      break;
    }
    // -------------------------------
    // This event indicates that the BT stack buffer resources were exhausted
    case sl_bt_evt_system_resource_exhausted_id:
      app_log_error(APP_PREFIX "BT stack buffers exhausted, data loss may have occurred! "
                               "buf_discarded='%u' buf_alloc_fail='%u' heap_alloc_fail='%u'" APP_LOG_NL,
                    evt->data.evt_system_resource_exhausted.num_buffers_discarded,
                    evt->data.evt_system_resource_exhausted.num_buffer_allocation_failures,
                    evt->data.evt_system_resource_exhausted.num_heap_allocation_failures);
      break;
    default:
      break;
  }
}

/******************************************************************************
 * Application process action
 *****************************************************************************/
void app_process_action(void)
{
  for (uint8_t i = 0u; i < MAX_INITIATOR_INSTANCES; i++) {
    if (cs_initiator_instances[i].measurement_arrived) {
      cs_initiator_instances[i].measurement_arrived = false;
      app_print_head_and_data(&cs_initiator_instances[i]);
    } else if (cs_initiator_instances[i].measurement_progress_changed) {
      cs_initiator_instances[i].measurement_progress_changed = false;
      app_log_info(APP_INSTANCE_PREFIX "# %04lu ---" APP_LOG_NL,
                   cs_initiator_instances[i].measurement_progress.connection,
                   (unsigned long)cs_initiator_instances[i].measurement_cnt);
      app_log_info(APP_INSTANCE_PREFIX "Estimation in progress: %3u.%02u %%" APP_LOG_NL,
                   cs_initiator_instances[i].measurement_progress.connection,
                   (unsigned)cs_initiator_instances[i].measurement_progress.progress_percentage,
                   (unsigned)((uint32_t)(cs_initiator_instances[i].measurement_progress.progress_percentage * 100.f) % 100));
    }
  }
}

/******************************************************************************
 * Application deinit
 *****************************************************************************/
void app_deinit(void)
{
  stop_procedure();
}

// -----------------------------------------------------------------------------
// Initiator instance bookkeeping (declared in app.h, shared with app_cs.c).

initiator_instance_t *app_get_instance(uint8_t conn_handle)
{
  for (uint8_t i = 0u; i < MAX_INITIATOR_INSTANCES; i++) {
    if (cs_initiator_instances[i].conn_handle == conn_handle) {
      return &cs_initiator_instances[i];
    }
  }
  return NULL;
}

void app_clear_instance_data(uint8_t conn_handle)
{
  for (uint32_t i = 0u; i < MAX_INITIATOR_INSTANCES; i++) {
    if (cs_initiator_instances[i].conn_handle == conn_handle) {
      cs_initiator_instances[i].conn_handle = SL_BT_INVALID_CONNECTION_HANDLE;
      cs_initiator_instances[i].measurement_cnt = 0u;
      memset(&cs_initiator_instances[i].measurement_mainmode, 0, sizeof(cs_measurement_data_t));
      memset(&cs_initiator_instances[i].measurement_submode, 0, sizeof(cs_measurement_data_t));
      memset(&cs_initiator_instances[i].measurement_progress, 0, sizeof(cs_intermediate_result_t));
      cs_initiator_instances[i].measurement_arrived = false;
      cs_initiator_instances[i].measurement_progress_changed = false;
      cs_initiator_instances[i].ras_discovery = false;
      cs_initiator_instances[i].read_capabilities = false;
      cs_initiator_instances[i].security_increase = false;
      break;
    }
  }
}

void app_increment_reflector_connections(void)
{
  cs_host_state.num_reflector_connections++;
}

void app_decrement_reflector_connections(void)
{
  if (cs_host_state.num_reflector_connections > 0u) {
    cs_host_state.num_reflector_connections--;
  }
}

void app_on_cs_setup_complete(void)
{
  connection_setup_handle = SL_BT_INVALID_CONNECTION_HANDLE;
  // Initiator-role: scan for new reflector connections if we have room.
  if (cs_host_state.num_reflector_connections < cs_host_config.max_initiator_instances) {
    try_start_scanning();
  }
}

bool app_is_reflector_connection(uint8_t conn_handle)
{
  if (conn_handle == SL_BT_INVALID_CONNECTION_HANDLE) {
    return false;
  }
  for (uint32_t i = 0u; i < MAX_REFLECTOR_INSTANCES; i++) {
    if (cs_host_state.initiator_conn_handles[i] == conn_handle) {
      return true;
    }
  }
  return false;
}

uint8_t app_get_max_connections(void)
{
  return (cs_host_config.max_initiator_instances
          + cs_host_config.max_reflector_instances);
}

// -----------------------------------------------------------------------------
// Internal helpers

static void enable_trace(void)
{
  if (cs_host_state.trace) {
    return;
  }
  sl_status_t sc;
  cs_acp_cmd_t cmd;
  cmd.id = CS_ACP_CMD_ENABLE_TRACE;
  cmd.data.cs_acp_cmd_enable_trace = 1;
  const size_t cmd_len = sizeof(cmd.id) + sizeof(cmd.data.cs_acp_cmd_enable_trace);

  sc = cs_acp_send_cmd(cmd_len,
                       (uint8_t *)&cmd,
                       0,
                       NULL,
                       NULL);

  if (sc == SL_STATUS_OK) {
    cs_host_state.trace = true;
  } else {
    app_log_status_error_f(sc, APP_PREFIX "Failed to enable trace" APP_LOG_NL);
  }
}

static void try_start_scanning(void)
{
  if (connection_setup_handle != SL_BT_INVALID_CONNECTION_HANDLE) {
    return;
  }
  if (cs_host_state.num_reflector_connections >= cs_host_config.max_initiator_instances) {
    return;
  }
  sl_status_t sc = sl_bt_peer_manager_central_create_connection();
  if (sc != SL_STATUS_OK) {
    app_log_status_error_f(sc, APP_PREFIX "Failed to start scanning" APP_LOG_NL);
    return;
  }
  app_log_info(APP_PREFIX "Scanning started for reflector connections..." APP_LOG_NL);
}

static void try_start_advertising(void)
{
  if (cs_host_state.num_initiator_connections >= cs_host_config.max_reflector_instances) {
    return;
  }
  if (cs_manager_is_full()) {
    return;
  }
  sl_status_t sc = sl_bt_peer_manager_peripheral_start_advertising(SL_BT_INVALID_ADVERTISING_SET_HANDLE);
  if (sc != SL_STATUS_OK) {
    app_log_status_error_f(sc, APP_PREFIX "Failed to start advertising" APP_LOG_NL);
    return;
  }
  app_log_info(APP_PREFIX "Advertising started for initiator connections..." APP_LOG_NL);
}

static void clear_discovery_config(uint8_t conn_handle)
{
  if (discovery_config.conn_handle == conn_handle) {
    discovery_config.conn_handle = SL_BT_INVALID_CONNECTION_HANDLE;
    discovery_config.service_handle = CS_RAS_INVALID_SERVICE_HANDLE;
    for (int i = 0; i < CS_RAS_CHARACTERISTIC_INDEX_COUNT; i++) {
      discovery_config.gattdb_handles.array[i] = CS_RAS_INVALID_CHARACTERISTIC_HANDLE;
    }
    discovery_config.status = SL_STATUS_FAIL;
  }
}

static void on_ras_discovery_complete(const app_cs_discovery_result_t *result)
{
  if (result == NULL) {
    app_log_error(APP_PREFIX "RAS discovery failed [result is NULL]" APP_LOG_NL);
    connection_setup_handle = SL_BT_INVALID_CONNECTION_HANDLE;
    return;
  }

  if (result->status != SL_STATUS_OK) {
    app_log_error(APP_INSTANCE_PREFIX "RAS discovery failed [sc: 0x%lx]" APP_LOG_NL,
                  result->conn_handle,
                  (unsigned long)result->status);
    cs_on_error(result->conn_handle,
                CS_APP_ERROR_RAS_DISCOVERY_FAILED,
                result->status);
    return;
  }
  app_log_info(APP_INSTANCE_PREFIX "RAS discovery complete" APP_LOG_NL,
               result->conn_handle);
  // Cache the result so app_cs_create_new_initiator_instance() can populate
  // the RREQ configuration with the discovered handles.
  memcpy(&discovery_config, result, sizeof(app_cs_discovery_result_t));

  initiator_instance_t *initiator = app_get_instance(result->conn_handle);
  if (initiator == NULL) {
    app_log_error(APP_INSTANCE_PREFIX "Failed to get instance for connection after discovery!" APP_LOG_NL,
                  result->conn_handle);
    cs_on_error(result->conn_handle,
                CS_APP_ERROR_INITIATOR_INSTANCE_NOT_FOUND,
                SL_STATUS_NOT_FOUND);
    return;
  }
  sl_status_t sc = sl_bt_cs_read_remote_supported_capabilities(result->conn_handle);
  if (sc != SL_STATUS_OK) {
    app_log_error(APP_INSTANCE_PREFIX "Failed to read remote CS capabilities! [sc: 0x%lx]" APP_LOG_NL,
                  result->conn_handle,
                  (unsigned long)sc);
    cs_on_error(result->conn_handle,
                CS_APP_ERROR_CS_READ_REMOTE_CAPABILITIES_FAILED,
                sc);
    return;
  }
  initiator->read_capabilities = true;

  app_log_info(APP_INSTANCE_PREFIX "Reading remote CS capabilities..." APP_LOG_NL,
               result->conn_handle);
}

/******************************************************************************
 * Connection opened as central — remote peer is a CS Reflector. The host
 * plays the Initiator role for this connection.
 *****************************************************************************/
static void on_connection_opened_with_reflector(uint8_t conn_handle)
{
  // Check if we can accept one more initiator-role connection
  if (cs_host_state.num_reflector_connections >= cs_host_config.max_initiator_instances) {
    app_log_error(APP_PREFIX "Maximum number of initiator instances (%u) reached, "
                             "dropping connection..." APP_LOG_NL,
                  cs_host_config.max_initiator_instances);
    (void)sl_bt_connection_close(conn_handle);
    return;
  }

  // Allocate an initiator-side bookkeeping slot for this connection.
  bool could_store = false;
  for (uint32_t i = 0u; i < MAX_INITIATOR_INSTANCES; i++) {
    if (cs_host_state.reflector_conn_handles[i] == SL_BT_INVALID_CONNECTION_HANDLE) {
      cs_host_state.reflector_conn_handles[i] = conn_handle;
      could_store = true;
      break;
    }
  }
  if (!could_store) {
    app_log_error(APP_PREFIX "Failed to store reflector connection handle" APP_LOG_NL);
    (void)sl_bt_connection_close(conn_handle);
    return;
  }

  // Reserve an initiator_instance_t slot for this connection.
  initiator_instance_t *initiator = NULL;
  for (uint8_t i = 0u; i < MAX_INITIATOR_INSTANCES; i++) {
    if (cs_initiator_instances[i].conn_handle == SL_BT_INVALID_CONNECTION_HANDLE) {
      cs_initiator_instances[i].conn_handle = conn_handle;
      initiator = &cs_initiator_instances[i];
      break;
    }
  }
  if (initiator == NULL) {
    app_log_error(APP_INSTANCE_PREFIX "No free initiator instance slot!" APP_LOG_NL,
                  conn_handle);
    cs_on_error(conn_handle, CS_APP_ERROR_INITIATOR_INSTANCE_LIST_FULL, SL_STATUS_FULL);
    (void)sl_bt_connection_close(conn_handle);
    return;
  }
  initiator->security_increase = false;
  initiator->ras_discovery = false;
}

/******************************************************************************
 * Connection opened as peripheral — remote peer is a CS Initiator. The host
 * plays the Reflector role for this connection.
 *****************************************************************************/
static void on_connection_opened_with_initiator(uint8_t conn_handle)
{
  sl_status_t sc;

  // Check if we can accept one more reflector-role connection.
  if (cs_host_state.num_initiator_connections >= cs_host_config.max_reflector_instances) {
    app_log_error(APP_PREFIX "Maximum number of reflector instances (%u) reached, "
                             "dropping connection..." APP_LOG_NL,
                  cs_host_config.max_reflector_instances);
    (void)sl_bt_connection_close(conn_handle);
    return;
  }

  // Store the new reflector-role connection handle so app_is_reflector_connection()
  // can route subsequent cs_manager events.
  uint32_t slot_idx = MAX_REFLECTOR_INSTANCES;
  for (uint32_t i = 0u; i < MAX_REFLECTOR_INSTANCES; i++) {
    if (cs_host_state.initiator_conn_handles[i] == SL_BT_INVALID_CONNECTION_HANDLE) {
      cs_host_state.initiator_conn_handles[i] = conn_handle;
      cs_host_state.num_initiator_connections++;
      slot_idx = i;
      break;
    }
  }
  if (slot_idx == MAX_REFLECTOR_INSTANCES) {
    app_log_error(APP_PREFIX "Failed to store initiator connection handle" APP_LOG_NL);
    (void)sl_bt_connection_close(conn_handle);
    return;
  }

  // Create the CS Manager instance with reflector-role config.
  sc = app_cs_create_new_reflector_instance(conn_handle);
  if (sc != SL_STATUS_OK) {
    app_log_error(APP_INSTANCE_PREFIX "Failed to create reflector instance [sc: 0x%lx]" APP_LOG_NL,
                  conn_handle,
                  (unsigned long)sc);
    cs_host_state.initiator_conn_handles[slot_idx] = SL_BT_INVALID_CONNECTION_HANDLE;
    if (cs_host_state.num_initiator_connections > 0u) {
      cs_host_state.num_initiator_connections--;
    }
    (void)sl_bt_peer_manager_peripheral_close_connection(conn_handle);
    return;
  }

  app_log_info(APP_PREFIX "Reflector instances in use: %u/%u" APP_LOG_NL,
               cs_host_state.num_initiator_connections,
               cs_host_config.max_reflector_instances);

  // Restart advertising for new initiator connections if there is room.
  try_start_advertising();
}

/******************************************************************************
 * Connection closed handler — dispatches by role.
 *
 *   Initiator role (host = central, remote = reflector): use
 *   app_cs_delete_initiator_instance to do cleanup cs_manager, cs_rreq and cs_algo.
 *
 *   Reflector role (host = peripheral, remote = initiator): call
 *   app_cs_delete_reflector_instance() and restart advertising
 *****************************************************************************/
static void on_connection_closed(uint8_t conn_handle)
{
  // ---- Initiator role (remote peer is a Reflector) ----
  for (uint32_t i = 0u; i < MAX_INITIATOR_INSTANCES; i++) {
    if (cs_host_state.reflector_conn_handles[i] == conn_handle) {
      cs_host_state.reflector_conn_handles[i] = SL_BT_INVALID_CONNECTION_HANDLE;
      app_log_info(APP_INSTANCE_PREFIX "Reflector connection closed" APP_LOG_NL, conn_handle);

      clear_discovery_config(conn_handle);
      app_cs_delete_initiator_instance(conn_handle);

      // If this was the connection currently being set up, unblock scanning.
      if (connection_setup_handle == conn_handle) {
        connection_setup_handle = SL_BT_INVALID_CONNECTION_HANDLE;
      }
      try_start_scanning();

      app_log_info(APP_PREFIX "Initiator instances in use: %u/%u" APP_LOG_NL,
                   cs_host_state.num_reflector_connections,
                   cs_host_config.max_initiator_instances);
      return;
    }
  }

  // ---- Reflector role (remote peer is an Initiator) ----
  for (uint32_t i = 0u; i < MAX_REFLECTOR_INSTANCES; i++) {
    if (cs_host_state.initiator_conn_handles[i] == conn_handle) {
      cs_host_state.initiator_conn_handles[i] = SL_BT_INVALID_CONNECTION_HANDLE;
      app_log_info(APP_INSTANCE_PREFIX "Initiator connection closed" APP_LOG_NL, conn_handle);

      app_cs_delete_reflector_instance(conn_handle);

      if (cs_host_state.num_initiator_connections > 0u) {
        cs_host_state.num_initiator_connections--;
      }

      try_start_advertising();

      app_log_info(APP_PREFIX "Reflector instances in use: %u/%u" APP_LOG_NL,
                   cs_host_state.num_initiator_connections,
                   cs_host_config.max_reflector_instances);
      return;
    }
  }
}

void app_sl_bt_peer_manager_on_event(sl_bt_peer_manager_evt_type_t *event)
{
  const bd_addr *address;
  switch (event->evt_id) {
    case SL_BT_PEER_MANAGER_ON_CONN_OPENED_CENTRAL:
      address = sl_bt_peer_manager_get_bt_address(event->connection_id);
      app_log_info(APP_INSTANCE_PREFIX "Connection opened as central with CS Reflector"
                                       " '%02X:%02X:%02X:%02X:%02X:%02X'" APP_LOG_NL,
                   event->connection_id,
                   address->addr[5],
                   address->addr[4],
                   address->addr[3],
                   address->addr[2],
                   address->addr[1],
                   address->addr[0]);
      on_connection_opened_with_reflector(event->connection_id);
      break;

    case SL_BT_PEER_MANAGER_ON_CONN_OPENED_PERIPHERAL:
      address = sl_bt_peer_manager_get_bt_address(event->connection_id);
      app_log_info(APP_INSTANCE_PREFIX "Connection opened as peripheral with CS Initiator"
                                       " '%02X:%02X:%02X:%02X:%02X:%02X'" APP_LOG_NL,
                   event->connection_id,
                   address->addr[5],
                   address->addr[4],
                   address->addr[3],
                   address->addr[2],
                   address->addr[1],
                   address->addr[0]);
      on_connection_opened_with_initiator(event->connection_id);
      break;

    case SL_BT_PEER_MANAGER_ON_CONN_CLOSED:
      app_log_info(APP_INSTANCE_PREFIX "Connection closed" APP_LOG_NL, event->connection_id);
      on_connection_closed(event->connection_id);
      break;

    case SL_BT_PEER_MANAGER_ON_ADV_STOPPED:
      app_log_info(APP_INSTANCE_PREFIX "Advertisement stopped" APP_LOG_NL, event->connection_id);
      break;

    case SL_BT_PEER_MANAGER_ERROR:
      app_log_error(APP_INSTANCE_PREFIX "Peer Manager error" APP_LOG_NL, event->connection_id);
      break;

    default:
      app_log_info(APP_INSTANCE_PREFIX "Unhandled Peer Manager event (%u)" APP_LOG_NL, event->connection_id, event->evt_id);
      break;
  }
}

/******************************************************************************
 * Tear down all live CS instances and close all open connections. Used at
 * application shutdown.
 *****************************************************************************/
static void stop_procedure(void)
{
  sl_status_t sc;
  uint8_t conn_handle;

  // Close all initiator-role connections (host = central, remote = reflector).
  for (uint32_t i = 0u; i < MAX_INITIATOR_INSTANCES; i++) {
    conn_handle = cs_host_state.reflector_conn_handles[i];
    if (conn_handle != SL_BT_INVALID_CONNECTION_HANDLE) {
      app_log_info(APP_INSTANCE_PREFIX "Removing initiator instance" APP_LOG_NL, conn_handle);
      cs_host_state.reflector_conn_handles[i] = SL_BT_INVALID_CONNECTION_HANDLE;
      clear_discovery_config(conn_handle);
      app_cs_delete_initiator_instance(conn_handle);
      sc = sl_bt_peer_manager_central_close_connection(conn_handle);
      if (sc != SL_STATUS_OK && sc != SL_STATUS_NOT_FOUND && sc != SL_STATUS_INVALID_HANDLE) {
        app_log_status_error_f(sc, APP_INSTANCE_PREFIX "Failed to close reflector connection" APP_LOG_NL,
                               conn_handle);
      }
    }
  }

  // Close all reflector-role connections (host = peripheral, remote = initiator).
  for (uint32_t i = 0u; i < MAX_REFLECTOR_INSTANCES; i++) {
    conn_handle = cs_host_state.initiator_conn_handles[i];
    if (conn_handle != SL_BT_INVALID_CONNECTION_HANDLE) {
      app_log_info(APP_INSTANCE_PREFIX "Removing reflector instance" APP_LOG_NL, conn_handle);
      cs_host_state.initiator_conn_handles[i] = SL_BT_INVALID_CONNECTION_HANDLE;
      app_cs_delete_reflector_instance(conn_handle);
      sc = sl_bt_peer_manager_peripheral_close_connection(conn_handle);
      if (sc != SL_STATUS_OK && sc != SL_STATUS_NOT_FOUND && sc != SL_STATUS_INVALID_HANDLE) {
        app_log_status_error_f(sc, APP_INSTANCE_PREFIX "Failed to close initiator connection" APP_LOG_NL,
                               conn_handle);
      }
      if (cs_host_state.num_initiator_connections > 0u) {
        cs_host_state.num_initiator_connections--;
      }
    }
  }
}

void sl_bt_peer_security_on_event(uint8_t handle)
{
  app_log_info(APP_INSTANCE_PREFIX "Security process started" APP_LOG_NL, handle);
}
