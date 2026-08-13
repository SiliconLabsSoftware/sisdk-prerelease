/***************************************************************************//**
 * @file
 * @brief CS ACP host application interface
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

#ifndef APP_H
#define APP_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include "app_log.h"
#include "cs_algo.h"
#include "cs_manager.h"
#include "cs_rreq_api.h"
#include "cs_common.h"
#include "sl_bt_peer_manager_common.h"
#include "app_cs_discovery.h"

// -----------------------------------------------------------------------------
// Definitions

#define NL                               APP_LOG_NL
#define APP_PREFIX                       "[APP] "
#define INSTANCE_PREFIX                  "[%u] "
#define APP_INSTANCE_PREFIX              APP_PREFIX INSTANCE_PREFIX

#define log_debug(...)   app_log_debug(__VA_ARGS__)
#define log_info(...)    app_log_info(__VA_ARGS__)
#define log_warning(...) app_log_warning(__VA_ARGS__)
#define log_error(...)   app_log_error(__VA_ARGS__)
#define log_append(...)  app_log_append(__VA_ARGS__)

// -----------------------------------------------------------------------------
// Types

// Measurement structure
typedef struct {
  float distance_filtered;
  float distance_raw;
  float likeliness;
  float distance_estimate_rssi;
  float velocity;
  float bit_error_rate;
} cs_measurement_data_t;

// CS initiator instance
typedef struct {
  cs_measurement_data_t measurement_mainmode;
  cs_measurement_data_t measurement_submode;
  cs_intermediate_result_t measurement_progress;
  uint32_t measurement_cnt;
  uint32_t ranging_counter;
  uint8_t conn_handle;
  uint8_t number_of_measurements;
  bool measurement_arrived;
  bool measurement_progress_changed;
  bool ras_discovery;
  bool security_increase;
  bool read_capabilities;
} initiator_instance_t;

/// Application-level CS error events reported via @ref cs_on_error().
///
/// Covers app-integration error cases that are NOT already reported via the
/// component-level callbacks:
///   - cs_rreq_error_t  -> @ref app_on_cs_rreq_on_error
///   - cs_algo_error_t  -> @ref app_on_cs_algo_on_error
SL_ENUM(cs_app_error_t) {
  // ---- cs_manager / cs_configurator call errors ----
  CS_APP_ERROR_CS_MANAGER_INSTANCE_CREATE_FAILED = 0,
  CS_APP_ERROR_CS_MANAGER_INSTANCE_REMOVE_FAILED,
  CS_APP_ERROR_CS_CONFIG_CREATE_FAILED,
  CS_APP_ERROR_CS_CONFIG_GET_DATA_FAILED,
  CS_APP_ERROR_CS_CONFIG_REMOVE_FAILED,
  CS_APP_ERROR_CS_READ_REMOTE_CAPABILITIES_FAILED,
  CS_APP_ERROR_PROCEDURE_START_FAILED,
  CS_APP_ERROR_PROCEDURE_STOP_FAILED,

  // ---- Synchronous cs_rreq / cs_algo call errors ----
  CS_APP_ERROR_RREQ_CREATE_FAILED,
  CS_APP_ERROR_RREQ_ENABLE_FAILED,
  CS_APP_ERROR_RREQ_DISABLE_FAILED,
  CS_APP_ERROR_ALGO_CREATE_FAILED,
  CS_APP_ERROR_ALGO_REMOVE_FAILED,

  // ---- Local / remote CS capability mismatches ----
  CS_APP_ERROR_CS_SYNC_PHY_NOT_SUPPORTED,

  // ---- Parameter optimization ----
  CS_APP_ERROR_PARAM_OPTIMIZATION_NOT_SUPPORTED,
  CS_APP_ERROR_PARAM_OPTIMIZATION_INVALID_INPUT,

  // ---- Timer ----
  CS_APP_ERROR_TIMER_START_FAILED,

  // ---- Initiator instance ----
  CS_APP_ERROR_INITIATOR_INSTANCE_CREATE_FAILED,
  CS_APP_ERROR_INITIATOR_INSTANCE_LIST_FULL,
  CS_APP_ERROR_INITIATOR_INSTANCE_NOT_FOUND,

  // ---- RAS / Discovery ----
  CS_APP_ERROR_RAS_DISCOVERY_NOT_COMPLETE,
  CS_APP_ERROR_RAS_DISCOVERY_FAILED
};

/// Identifies which error enumeration an error code value belongs to.
typedef enum {
  CS_ERROR_TYPE_RREQ = 0,    ///< value is a @ref cs_rreq_error_t
  CS_ERROR_TYPE_ALGO,        ///< value is a @ref cs_algo_error_t
  CS_ERROR_TYPE_APP,         ///< value is a @ref cs_app_error_t
  CS_ERROR_TYPE_CS_MANAGER   ///< value is a @ref cs_manager_error_t
} cs_error_type_t;

// -----------------------------------------------------------------------------
// Host peer-manager glue

/**************************************************************************//**
 * Peer manager event handler (host).
 * @param[in] event Peer manager event to process.
 *****************************************************************************/
void app_sl_bt_peer_manager_on_event(sl_bt_peer_manager_evt_type_t *event);

// -----------------------------------------------------------------------------
// Initiator instance bookkeeping (implemented in app.c)

/**************************************************************************//**
 * Look up the initiator instance for a given connection handle.
 * @param[in] conn_handle Connection handle to look up.
 * @return Pointer to the matching instance, or NULL if none exists.
 *****************************************************************************/
initiator_instance_t *app_get_instance(uint8_t conn_handle);

/**************************************************************************//**
 * Clear cached state for an initiator instance.
 * @param[in] conn_handle Connection handle whose instance data is cleared.
 *****************************************************************************/
void app_clear_instance_data(uint8_t conn_handle);

/**************************************************************************//**
 * Increment the active reflector connection count.
 *****************************************************************************/
void app_increment_reflector_connections(void);

/**************************************************************************//**
 * Decrement the active reflector connection count.
 *****************************************************************************/
void app_decrement_reflector_connections(void);

/**************************************************************************//**
 * Get the maximum number of simultaneous connections supported.
 * @return Maximum number of connections.
 *****************************************************************************/
uint8_t app_get_max_connections(void);

/**************************************************************************//**
 * Check whether a connection is a reflector-role connection.
 *
 * Returns true when @p conn_handle belongs to a connection where the host
 * plays the Reflector role.
 *
 * @param[in] conn_handle Connection handle to test.
 * @return true if the connection is a reflector-role connection, false
 *         otherwise (initiator role or unknown handle).
 *****************************************************************************/
bool app_is_reflector_connection(uint8_t conn_handle);

/**************************************************************************//**
 * CS setup-complete notification
 *****************************************************************************/
void app_on_cs_setup_complete(void);

// -----------------------------------------------------------------------------
// CS result callbacks (implemented in app_cs_result.c)

/**************************************************************************//**
 * CS result callback.
 * @param[in] conn_handle     Connection handle the result belongs to.
 * @param[in] ranging_counter Ranging counter identifying the procedure.
 * @param[in] result          Pointer to the raw result buffer.
 * @param[in] result_size     Size of @p result in bytes.
 * @param[in] ranging_data    Pointer to the ranging result.
 *****************************************************************************/
void app_on_result(uint8_t conn_handle,
                   uint16_t ranging_counter,
                   const uint8_t *result,
                   uint16_t result_size,
                   const cs_algo_result_t *ranging_data);

/**************************************************************************//**
 * CS intermediate result callback.
 * @param[in] intermediate_result Pointer to the intermediate result.
 *****************************************************************************/
void app_on_intermediate_result(const cs_intermediate_result_t *intermediate_result);

/**************************************************************************//**
 * Print measurement headers and data.
 * @param[in] initiator Initiator instance whose measurement is printed.
 *****************************************************************************/
void app_print_head_and_data(initiator_instance_t *initiator);

// -----------------------------------------------------------------------------
// CS error callbacks

/**************************************************************************//**
 * cs_rreq error callback.
 * @param[in] conn_handle Connection handle the error is associated with.
 * @param[in] error       RREQ error code.
 * @param[in] sc          Underlying status code.
 *****************************************************************************/
void app_on_cs_rreq_on_error(uint8_t conn_handle,
                             cs_rreq_error_t error,
                             sl_status_t sc);

/**************************************************************************//**
 * cs_algo error callback.
 * @param[in] conn_handle     Connection handle the error is associated with.
 * @param[in] ranging_counter Ranging counter of the affected procedure.
 * @param[in] error           Algo error code.
 * @param[in] sc              Underlying status code.
 *****************************************************************************/
void app_on_cs_algo_on_error(uint8_t conn_handle,
                             uint16_t ranging_counter,
                             cs_algo_error_t error,
                             sl_status_t sc);

/**************************************************************************//**
 * Generic CS application error handler.
 * @param[in] conn_handle Connection handle the error is associated with.
 * @param[in] error       Application error code.
 * @param[in] sc          Underlying status code.
 *****************************************************************************/
void cs_on_error(uint8_t conn_handle,
                 cs_app_error_t error,
                 sl_status_t sc);

/**************************************************************************//**
 * cs_manager error callback. Defined in app_cs_error.c.
 * @param[in] conn_handle Connection handle the error is associated with.
 * @param[in] error       CS Manager error code.
 * @param[in] sc          Underlying status code.
 *****************************************************************************/
void app_on_cs_manager_on_error(uint8_t conn_handle,
                                cs_manager_error_t error,
                                sl_status_t sc);

/**************************************************************************//**
 * NCP target error handler.
 *
 * Used for errors reported by the remote NCP target via the high-level ACP
 * status event (@ref CS_ACP_EVT_STATUS_ID). The error code is the raw value
 * that the target reported (typically a @c cs_error_event_t value); we keep it
 * as a @c uint8_t here to avoid pulling the target-side enum into the host
 * error contract.
 *
 * @param[in] conn_handle  Connection handle the error is associated with.
 * @param[in] target_error Raw error code reported by the target.
 * @param[in] sc           Status code reported by the target.
 *****************************************************************************/
void cs_on_target_error(uint8_t conn_handle,
                        uint8_t target_error,
                        sl_status_t sc);

// -----------------------------------------------------------------------------
// CS configuration

/**************************************************************************//**
 * Set CS Manager, RREQ and CS Algo callbacks.
 * @param[in] algo_cb CS Algo event callbacks to register.
 *****************************************************************************/
void app_cs_set_callbacks(cs_algo_event_callback_t algo_cb);

/**************************************************************************//**
 * Get default configuration for CS Manager and CS Algo.
 *****************************************************************************/
void app_cs_get_default_config(void);

/**************************************************************************//**
 * Optimize CS scheduling parameters for the given connection.
 * @param[in] connection Connection handle to optimize parameters for.
 *****************************************************************************/
void app_cs_optimize_parameters(uint8_t connection);

/**************************************************************************//**
 * Derive and apply optimized default connection parameters.
 *
 * Derives optimized default connection parameters at boot and applies them as
 * the CS Manager baseline, so connections open at the optimized interval
 * without a per-connection parameter update.
 *****************************************************************************/
void app_cs_set_default_connection_parameters(void);

/**************************************************************************//**
 * Create a new initiator instance for a freshly discovered RAS service.
 * @param[in] conn_handle      Connection handle of the discovered peer.
 * @param[in] discovery_config Discovery result describing the RAS service.
 * @return SL_STATUS_OK on success, error code otherwise.
 *****************************************************************************/
sl_status_t app_cs_create_new_initiator_instance(uint8_t conn_handle,
                                                 app_cs_discovery_result_t *discovery_config);

/**************************************************************************//**
 * Delete an initiator instance.
 * @param[in] conn_handle Connection handle of the instance to delete.
 *****************************************************************************/
void app_cs_delete_initiator_instance(uint8_t conn_handle);

/**************************************************************************//**
 * Create a new reflector instance for an incoming initiator connection.
 *****************************************************************************/
sl_status_t app_cs_create_new_reflector_instance(uint8_t conn_handle);

/**************************************************************************//**
 * Delete a reflector instance.
 *****************************************************************************/
void app_cs_delete_reflector_instance(uint8_t conn_handle);

/**************************************************************************//**
 * Check that the local and remote support the requested CS capabilities.
 * @param[in] evt Bluetooth stack event carrying the remote capabilities.
 *****************************************************************************/
void app_cs_check_supported_capabilities(const sl_bt_msg_t *evt);

/**************************************************************************//**
 * Pull current values from CLI overrides (no-op if CLI is not built).
 *****************************************************************************/
void app_cs_check_cli_values(void);

/**************************************************************************//**
 * Log the current default CS configuration.
 *****************************************************************************/
void app_cs_log_default_config(void);

// -----------------------------------------------------------------------------
// CS configuration getters

/**************************************************************************//**
 * Get the configured CS main mode.
 * @return The CS main mode.
 *****************************************************************************/
uint8_t app_cs_get_main_mode(void);

/**************************************************************************//**
 * Get the configured CS sub mode.
 * @return The CS sub mode.
 *****************************************************************************/
uint8_t app_cs_get_sub_mode(void);

/**************************************************************************//**
 * Get the configured CS algorithm mode.
 * @return The CS algorithm mode.
 *****************************************************************************/
cs_algo_mode_t app_cs_get_algo_mode(void);

/**************************************************************************//**
 * Get the configured channel map preset.
 * @return The channel map preset.
 *****************************************************************************/
cs_channel_map_preset_t app_cs_get_channel_map_preset(void);

/**************************************************************************//**
 * MTU update notification (forwarded to RREQ config in app_cs.c).
 * @param[in] mtu Negotiated ATT MTU.
 *****************************************************************************/
void app_on_mtu_changed(uint16_t mtu);

/**************************************************************************//**
 * Update actual connection PHY in CS configs.
 * @param[in] phy Connection PHY value
 *****************************************************************************/
void app_on_connection_phy_changed(uint8_t phy);

// -----------------------------------------------------------------------------
// CLI override setters

/**************************************************************************//**
 * Override the CS main mode.
 * @param[in] main_mode CS main mode to use.
 *****************************************************************************/
void app_cs_set_main_mode(uint8_t main_mode);

/**************************************************************************//**
 * Override the CS sub mode.
 * @param[in] sub_mode CS sub mode to use.
 *****************************************************************************/
void app_cs_set_sub_mode(uint8_t sub_mode);

/**************************************************************************//**
 * Override the CS connection PHY.
 * @param[in] phy Connection PHY to use.
 *****************************************************************************/
void app_cs_set_conn_phy(uint8_t phy);

/**************************************************************************//**
 * Override the CS algorithm mode.
 * @param[in] algo_mode CS algorithm mode to use.
 *****************************************************************************/
void app_cs_set_algo_mode(uint8_t algo_mode);

/**************************************************************************//**
 * Override the channel map preset.
 * @param[in] preset Channel map preset to use.
 *****************************************************************************/
void app_cs_set_channel_map_preset(cs_channel_map_preset_t preset);

/**************************************************************************//**
 * Override the tone antenna configuration index.
 * @param[in] idx Tone antenna configuration index to use.
 *****************************************************************************/
void app_cs_set_tone_antenna_config_idx(uint8_t idx);

/**************************************************************************//**
 * Override the CS SYNC antenna selection.
 * @param[in] cs_sync_antenna CS SYNC antenna to use.
 *****************************************************************************/
void app_cs_set_cs_sync_antenna(uint8_t cs_sync_antenna);

/**************************************************************************//**
 * Override the procedure scheduling mode.
 * @param[in] scheduling Procedure scheduling mode to use.
 *****************************************************************************/
void app_cs_set_procedure_scheduling(cs_procedure_scheduling_t scheduling);

/**************************************************************************//**
 * Override the maximum procedure count.
 * @param[in] max_procedure_count Maximum number of procedures to run.
 *****************************************************************************/
void app_cs_set_max_procedure_count(uint16_t max_procedure_count);

#ifdef __cplusplus
}
#endif

#endif // APP_H
