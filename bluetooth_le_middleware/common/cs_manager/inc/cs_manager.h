/***************************************************************************//**
 * @file
 * @brief CS Manager API
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

#ifndef CS_MANAGER_H
#define CS_MANAGER_H

// -----------------------------------------------------------------------------
// Includes

#include <stdbool.h>
#include <stdint.h>
#include "sl_status.h"
#include "sl_enum.h"
#include "sl_bt_api.h"
#include "cs_manager_config.h"
#include "cs_common.h"

#ifdef __cplusplus
extern "C" {
#endif

// -----------------------------------------------------------------------------
// Definitions

/// Invalid configuration identifier
#define CS_MANAGER_INVALID_CONFIG_ID  0xFF

/// CS Manager lifecycle event type
typedef enum {
  CS_MANAGER_EVENT_INSTANCE_CREATE_COMPLETE, ///< Instance create complete
  CS_MANAGER_EVENT_INSTANCE_REMOVE_COMPLETE, ///< Instance remove complete
  CS_MANAGER_EVENT_CONFIG_REMOVE_COMPLETE, ///< CS configuration remove complete
  CS_MANAGER_EVENT_CONFIG_CREATE_COMPLETE, ///< CS configuration create complete
  CS_MANAGER_EVENT_CONFIG_OVERWRITTEN, ///< Existing configuration overwrite complete
  CS_MANAGER_EVENT_PROCEDURE_START_COMPLETE, ///< CS procedure start complete
  CS_MANAGER_EVENT_PROCEDURE_STOP_COMPLETE, ///< CS procedure stop complete
  CS_MANAGER_EVENT_ERROR, ///< General CS Manager error
} cs_manager_event_type_t;

/// CS Manager error type
SL_ENUM(cs_manager_error_t) {
  CS_MANAGER_ERROR_RTA_INIT_FAILED,               ///< RTA init failed
  CS_MANAGER_ERROR_RTA_ACQUIRE_FAILED,            ///< RTA acquire failed
  CS_MANAGER_ERROR_RTA_RELEASE_FAILED,            ///< RTA release failed
  CS_MANAGER_ERROR_RUNTIME_ERROR,                 ///< Runtime error reported by app_rta
};

typedef struct {
  uint16_t min_connection_interval; ///< Minimum connection interval (1.25 ms)
  uint16_t max_connection_interval; ///< Maximum connection interval (1.25 ms)
  uint16_t latency; ///< Peripheral latency in connection events
  uint16_t timeout; ///< Supervision timeout in units of 10 ms
  uint16_t min_ce_length; ///< Minimum connection event length (0.625 ms)
  uint16_t max_ce_length; ///< Maximum connection event length (0.625 ms)
} cs_manager_connection_parameters_t;

/// CS Manager instance configuration
typedef struct {
  uint8_t cs_sync_antenna; ///< CS sync antenna selection index
  uint8_t conn_phy; ///< Connection PHY (1 = 1M, 2 = 2M, 4 = Coded)
  int8_t max_tx_power_dbm; ///< Maximum TX power in dBm
  bool is_central; ///< ture if the device acts as central, 0 for peripheral
  bool is_initiator; ///< 1 if the device acts as central, 0 for peripheral
} cs_manager_instance_config_t;

/// Stored CS configuration data.
/// This is an alias over the @ref sl_bt_evt_cs_config_complete_t event
/// payload from the Bluetooth stack. 
typedef sl_bt_evt_cs_config_complete_t cs_config_data_t;

/// CS procedure configuration parameters
typedef struct {
  sl_bt_cs_channel_map_t channel_map; ///< CS channel map
  uint8_t channel_map_repetition; ///< Number of channel map repetitions
  uint8_t channel_selection_type; ///< Channel selection algorithm type
  uint8_t main_mode_type; ///< CS main mode type
  uint8_t sub_mode_type; ///< CS sub-mode type
  uint8_t min_main_mode_steps; ///< Minimum number of main mode steps
  uint8_t max_main_mode_steps; ///< Maximum number of main mode steps
  uint8_t main_mode_repetition; ///< Main mode repetitions per procedure
  uint8_t mode_calibration_steps; ///< Number of calibration steps
  uint8_t rtt_type; ///< RTT type
  uint8_t cs_sync_phy; ///< PHY used for CS sync
  uint8_t ch3c_shape; ///< Channel 3C shape
  uint8_t ch3c_jump; ///< Channel 3C jump value
  uint8_t reserved; ///< Reserved; set to zero
} cs_config_t;

/// CS procedure scheduling and PHY parameters
typedef struct {
  uint16_t max_procedure_len; ///< Maximum procedure duration (0.625 ms)
  uint16_t min_procedure_interval; ///< Minimum interval between procedures
  uint16_t max_procedure_interval; ///< Maximum interval between procedures
  uint16_t max_procedure_count; ///< Maximum number of procedures
  uint32_t min_subevent_len; ///< Minimum subevent length in microseconds
  uint32_t max_subevent_len; ///< Maximum subevent length in microseconds
  cs_tone_antenna_config_index_t tone_antenna_config_selection; ///< Tone antenna configuration
  int8_t tx_pwr_delta; ///< TX power delta relative to the peer
  uint8_t preferred_peer_antenna; ///< Preferred peer antenna
  uint8_t snr_control_initiator; ///< SNR control mode for the initiator
  uint8_t snr_control_reflector; ///< SNR control mode for the reflector
} cs_procedure_parameters_t;

/**************************************************************************//**
 * @brief Callback type for CS Manager lifecycle events.
 *
 * @param[in] conn_handle Connection handle.
 * @param[in] config_id   Configuration identifier (relevant for config events).
 * @param[in] event       Type of event that occurred.
 * @param[in] status      Status code associated with the event.
 *****************************************************************************/
typedef void (*cs_manager_event_t)(uint8_t conn_handle,
                                   uint8_t config_id,
                                   cs_manager_event_type_t event,
                                   sl_status_t status);

/**************************************************************************//**
 * @brief Callback invoked when an error occurs during CS Manager operation.
 *
 * @param[in] conn_handle Connection handle (or @ref SL_BT_INVALID_CONNECTION_HANDLE
 *                        for non-connection-bound errors).
 * @param[in] error       Error event identifier (@ref cs_manager_error_t).
 * @param[in] sc          Underlying status code.
 *****************************************************************************/
typedef void (*cs_manager_on_error_t)(uint8_t conn_handle,
                                      cs_manager_error_t error,
                                      sl_status_t sc);

/// Collection type of event callbacks registered with
/// @ref cs_manager_set_event_callbacks
typedef struct {
  cs_manager_event_t    on_event; ///< Lifecycle event callback (required).
  cs_manager_on_error_t on_error; ///< Error event callback (required).
} cs_manager_event_callback_t;

// -----------------------------------------------------------------------------
// Function declarations

/**************************************************************************//**
 * Create a new CS Manager instance for a connection.
 *
 * @param[in] conn_handle Connection handle.
 * @param[in] inst_config Pointer to the instance configuration.
 * @param[in] connection_parameters Pointer to the connection parameters.
 * @return Status of the operation.
 * @retval SL_STATUS_OK               Instance created successfully.
 * @retval SL_STATUS_NULL_POINTER     @p inst_config is NULL.
 * @retval SL_STATUS_INVALID_HANDLE   @p conn_handle is invalid.
 * @retval SL_STATUS_ALREADY_EXISTS   An instance already exists.
 * @retval SL_STATUS_NO_MORE_RESOURCE Maximum number of instances reached.
 * @retval SL_STATUS_NOT_INITIALIZED  Callback is not set.
 *****************************************************************************/
sl_status_t cs_manager_create(uint8_t conn_handle,
                              cs_manager_instance_config_t *inst_config,
                              cs_manager_connection_parameters_t *connection_parameters); ///< Connection parameters);

/**************************************************************************//**
 * Delete the CS Manager instance for a connection.
 *
 * @param[in] conn_handle Connection handle.
 * @return Status of the operation.
 * @retval SL_STATUS_OK        Instance deleted successfully.
 * @retval SL_STATUS_NOT_FOUND No instance found for @p conn_handle.
 *****************************************************************************/
sl_status_t cs_manager_delete(uint8_t conn_handle);

/**************************************************************************//**
 * Check if the CS Manager is full.
 *
 * @return true if the CS Manager is full, false otherwise.
 *****************************************************************************/
bool cs_manager_is_full(void);

/**************************************************************************//**
 * Create a new CS configuration for a connection.
 *
 * @param[in] conn_handle    Connection handle.
 * @param[in] config_id      Configuration identifier to create.
 * @param[in] create_context true to write the configuration on the local
 *                           device, false for the remote device.
 * @param[in] config         Pointer to the CS configuration data.
 * @return Status of the operation.
 * @retval SL_STATUS_OK               Configuration created successfully.
 * @retval SL_STATUS_NULL_POINTER     @p config is NULL.
 * @retval SL_STATUS_NOT_FOUND        No instance found for @p conn_handle.
 * @retval SL_STATUS_NO_MORE_RESOURCE Maximum number of configurations reached.
 *****************************************************************************/
sl_status_t cs_manager_config_create(uint8_t conn_handle,
                                     uint8_t config_id,
                                     bool create_context,
                                     const cs_config_t *config);

/**************************************************************************//**
 * Remove a CS configuration from a CS Manager instance.
 *
 * @param[in] conn_handle Connection handle.
 * @param[in] config_id   Configuration identifier to remove.
 * @return Status of the operation.
 * @retval SL_STATUS_OK            Removal command submitted successfully.
 * @retval SL_STATUS_NOT_FOUND     No instance found for @p conn_handle.
 * @retval SL_STATUS_INVALID_STATE Invalid state.
 *****************************************************************************/
sl_status_t cs_manager_config_remove(uint8_t conn_handle, uint8_t config_id);

/**************************************************************************//**
 * Retrieve a stored CS configuration.
 *
 * @param[in]  conn_handle Connection handle.
 * @param[in]  config_id   Configuration identifier.
 * @param[out] config_out  Pointer to the structure to populate.
 * @return Status of the operation.
 * @retval SL_STATUS_OK            Configuration retrieved successfully.
 * @retval SL_STATUS_NULL_POINTER  @p config_out is NULL.
 * @retval SL_STATUS_NOT_FOUND     No matching configuration found for the
 *                                 given @p conn_handle and @p config_id.
 *****************************************************************************/
sl_status_t cs_manager_config_get(uint8_t conn_handle,
                                  uint8_t config_id,
                                  cs_config_data_t *config_out);

/**************************************************************************//**
 * Register the event and error callbacks for CS Manager.
 *
 * @note Replaces any previously registered callbacks. All callbacks in @p cb
 *       are required; pass NULL members to clear them is not supported.
 *
 * @param[in] cb Pointer to a populated callback collection.
 * @return Status of the operation.
 * @retval SL_STATUS_OK               Callbacks set successfully.
 * @retval SL_STATUS_NULL_POINTER     @p cb or any of its members is NULL.
 *****************************************************************************/
sl_status_t cs_manager_set_event_callbacks(cs_manager_event_callback_t *cb);

/**************************************************************************//**
 * Populate a cs_config_t structure with default CS configuration values.
 *
 * @param[out] config Pointer to the configuration structure to populate.
 * @return Status of the operation.
 * @retval SL_STATUS_OK               Parameters populated successfully.
 * @retval SL_STATUS_NULL_POINTER     @p params is NULL.
 *****************************************************************************/
sl_status_t cs_manager_get_default_config(cs_config_t *config);

/**************************************************************************//**
 * Populate a cs_manager_instance_config_t structure with default instance 
 * configuration values.
 *
 * @param[in] is_initiator True if the device acts as initiator, false 
 *                         otherwise.
 * @param[in] is_central   True if the device acts as central, false otherwise.
 * @param[out] instance_config Pointer to the instance configuration structure 
 *                             ato populate.
 * @return Status of the operation.
 * @retval SL_STATUS_OK               Parameters populated successfully.
 * @retval SL_STATUS_NULL_POINTER     @p params is NULL.
 *****************************************************************************/
sl_status_t cs_manager_get_default_instance_config(bool is_initiator,
                                                   bool is_central,
                                                   cs_manager_instance_config_t *instance_config);

/**************************************************************************//**
 * Start a CS ranging procedure on a connection.
 *
 * @param[out] params              Pointer to the procedure parameters.
 * @return Status of the operation.
 * @retval SL_STATUS_OK               Parameters populated successfully.
 * @retval SL_STATUS_NULL_POINTER     @p params is NULL.
 *****************************************************************************/
 sl_status_t cs_manager_get_default_procedure_parameters(cs_procedure_parameters_t *params);

/**************************************************************************//**
 * Retrieve default connection parameters.
 *
 * @param[out] params Pointer to the connection parameters.
 * @return Status of the operation.
 * @retval SL_STATUS_OK               Parameters applied successfully.
 * @retval SL_STATUS_NULL_POINTER     @p params is NULL.
 *****************************************************************************/
 sl_status_t cs_manager_get_default_connection_parameters(cs_manager_connection_parameters_t *params);

/**************************************************************************//**
 * Start a CS ranging procedure on a connection.
 *
 * @param[in] conn_handle         Connection handle.
 * @param[in] config_id           Configuration identifier to use.
 * @param[in] params              Pointer to the procedure parameters.
 * @return Status of the operation.
 * @retval SL_STATUS_OK             Procedure started successfully.
 * @retval SL_STATUS_NULL_POINTER   @p params is NULL.
 * @retval SL_STATUS_NOT_FOUND      No instance or configuration found.
 * @retval SL_STATUS_INVALID_STATE  Instance is not in the expected state.
 *****************************************************************************/
sl_status_t cs_manager_start(uint8_t conn_handle,
                             uint8_t config_id,
                             cs_procedure_parameters_t *params);

/**************************************************************************//**
 * Stop the currently running CS ranging procedure on a connection.
 *
 * @param[in] conn_handle Connection handle.
 * @return Status of the operation.
 * @retval SL_STATUS_OK        Procedure stopped successfully.
 * @retval SL_STATUS_NOT_FOUND No active procedure for @p conn_handle.
 *****************************************************************************/
sl_status_t cs_manager_stop(uint8_t conn_handle);

#ifdef __cplusplus
};
#endif

#endif // CS_MANAGER_H
