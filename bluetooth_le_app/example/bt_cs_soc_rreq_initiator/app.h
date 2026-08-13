/***************************************************************************//**
 * @file
 * @brief CS SoC RREQ initiator application interface
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

#include <stdint.h>
#include "cs_algo.h"
#include "sl_bt_peer_manager_common.h"
#include "cs_rreq.h"
#include "cs_common.h"
#include "cs_manager.h"
#include "app_cs_discovery.h"

// -----------------------------------------------------------------------------
// Definitions

#define NL                               APP_LOG_NL
#define APP_PREFIX                       "[APP] "
#define INSTANCE_PREFIX                  "[%u] "
#define APP_INSTANCE_PREFIX              APP_PREFIX INSTANCE_PREFIX

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

/**************************************************************************//**
 * Initialize the application task / OS helper objects.
 *
 * Provided by the @c app_os_helper component (one of @c app_freertos.c,
 * @c app_micriumos.c or @c app_bm.c). SLC auto-registers this as an
 * @c internal_init_early event handler in @c sl_event_handler.c, so the
 * declaration only needs to be visible to the generated file - the
 * application itself never calls this directly.
 *****************************************************************************/
void app_init_bt(void);

/**************************************************************************//**
 * Signal the application task that there is new data to process.
 *
 * Wakes the FreeRTOS / Micrium task that runs @ref app_process_action so the
 * polling consumer there can drain @c measurement_arrived /
 * @c measurement_progress_changed flags. Safe to call from ISR context (the
 * FreeRTOS variant uses @c xSemaphoreGiveFromISR internally).
 *
 * Provided by the @c app_os_helper component (one of @c app_freertos.c,
 * @c app_micriumos.c or @c app_bm.c is selected automatically).
 *****************************************************************************/
void app_proceed(void);

/**************************************************************************//**
 * Block until @ref app_proceed has been called.
 *
 * Under RTOS this is a blocking semaphore wait (no CPU spin). Under baremetal
 * it is a non-blocking check of a critical-section-protected counter.
 *
 * Provided by the @c app_os_helper component.
 *
 * @return true when there is work to do, false otherwise.
 *****************************************************************************/
bool app_is_process_required(void);

/**************************************************************************//**
 * Acquire the application mutex protecting shared state.
 *
 * Protects shared variables touched both by @ref app_process_action and by
 * callbacks running on other threads (e.g. the BT event-handler thread that
 * sets @c measurement_arrived).
 *
 * Provided by the @c app_os_helper component.
 *
 * @return true if the mutex was acquired successfully.
 *****************************************************************************/
bool app_mutex_acquire(void);

/**************************************************************************//**
 * Release the application mutex protecting shared state.
 *
 * Provided by the @c app_os_helper component.
 *****************************************************************************/
void app_mutex_release(void);

/**************************************************************************//**
 * Peer manager event handler.
 * @param[in] event Peer manager event to process.
 *****************************************************************************/
void sl_bt_peer_manager_on_event_initiator(sl_bt_peer_manager_evt_type_t *event);

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
 * CS result callback.
 * @param[in] conn_handle     Connection handle the result belongs to.
 * @param[in] ranging_counter Ranging counter identifying the procedure.
 * @param[in] result          Pointer to the raw result buffer.
 * @param[in] result_size     Size of @p result in bytes.
 * @param[in] ranging_data    Pointer to the decoded ranging result.
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
  CS_APP_ERROR_CS_MANAGER_GENERAL_ERROR,
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

typedef enum {
  CS_ERROR_TYPE_RREQ = 0,    ///< value is a @ref cs_rreq_error_t
  CS_ERROR_TYPE_ALGO,        ///< value is a @ref cs_algo_error_t
  CS_ERROR_TYPE_APP,         ///< value is a @ref cs_app_error_t
  CS_ERROR_TYPE_CS_MANAGER   ///< value is a @ref cs_manager_error_t
} cs_error_type_t;

/**************************************************************************//**
 * cs_rreq error callback. Defined in app_cs_error.c.
 *****************************************************************************/
void app_on_cs_rreq_on_error(uint8_t conn_handle,
                             cs_rreq_error_t error,
                             sl_status_t sc);

/**************************************************************************//**
 * cs_algo error callback. Defined in app_cs_error.c.
 *****************************************************************************/
void app_on_cs_algo_on_error(uint8_t conn_handle,
                             uint16_t ranging_counter,
                             cs_algo_error_t error,
                             sl_status_t sc);

/**************************************************************************//**
 * cs_manager error callback. Defined in app_cs_error.c.
 *****************************************************************************/
void app_on_cs_manager_on_error(uint8_t conn_handle,
                                cs_manager_error_t error,
                                sl_status_t sc);

/**************************************************************************//**
 * Generic CS application error handler. Defined in app_cs_error.c.
 *
 * @param[in] conn_handle Connection handle the error is associated with
 *                        (@c SL_BT_INVALID_CONNECTION_HANDLE if no
 *                        connection is associated yet).
 * @param[in] error       App error (@ref cs_app_error_t).
 * @param[in] sc          Underlying @c sl_status_t.
 *****************************************************************************/
void cs_on_error(uint8_t conn_handle,
                 cs_app_error_t error,
                 sl_status_t sc);

/**************************************************************************//**
 * Get initiator instance.
 * @param[in] conn_handle Connection handle
 * @return Initiator instance
 *****************************************************************************/
initiator_instance_t *app_get_instance(uint8_t conn_handle);

/**************************************************************************//**
 * Set CS Manager, RREQ and CS Algo callbacks.
 *****************************************************************************/
void app_cs_set_callbacks(cs_algo_event_callback_t algo_cb);

/**************************************************************************//**
 * Get default configuration for CS Manager and CS Algo.
 *****************************************************************************/
void app_cs_get_default_config(void);

/**************************************************************************//**
 * Optimize parameters for CS Manager and CS Algo.
 * @param[in] connection Connection handle
 *****************************************************************************/
void app_cs_optimize_parameters(uint8_t connection);

/**************************************************************************//**
 * Set the default connection parameters for new connections.
 *
 * Selects the default connection interval based on the procedure scheduling and
 * whether more than one CS Manager instance is configured, then applies it via
 * sl_bt_connection_set_default_parameters(). Must be called after the Bluetooth
 * stack has booted and after app_cs_get_default_config() has run.
 *****************************************************************************/
void app_cs_set_default_connection_parameters(void);

/**************************************************************************//**
 * Print head and data.
 * @param[in] initiator Initiator instance
 *****************************************************************************/
void app_print_head_and_data(initiator_instance_t *initiator);

/**************************************************************************//**
 * Create new initiator instance.
 * @param[in] conn_handle Connection handle
 *****************************************************************************/
sl_status_t app_cs_create_new_initiator_instance(uint8_t conn_handle,
                                                 app_cs_discovery_result_t *discovery_config);

/**************************************************************************//**
 * Delete initiator instance.
 * @param[in] conn_handle Connection handle
 *****************************************************************************/
void app_cs_delete_initiator_instance(uint8_t conn_handle);

/**************************************************************************//**
 * Check supported capabilities.
 * @param[in] evt Event coming from the Bluetooth stack.
 *****************************************************************************/
void app_cs_check_supported_capabilities(const sl_bt_msg_t *evt);

/**************************************************************************//**
 * Check CLI values.
 *****************************************************************************/
void app_cs_check_cli_values(void);

/**************************************************************************//**
 * Log default configuration for CS Manager and CS Algo.
 *****************************************************************************/
void app_cs_log_default_config(void);

/**************************************************************************//**
 * Get main mode.
 * @return CS main mode
 *****************************************************************************/
uint8_t app_cs_get_main_mode(void);

/**************************************************************************//**
 * Get sub mode.
 * @return CS sub mode
 *****************************************************************************/
uint8_t app_cs_get_sub_mode(void);

/**************************************************************************//**
 * Get algorithm mode.
 * @return RTL algorithm mode
 *****************************************************************************/
cs_algo_mode_t app_cs_get_algo_mode(void);

/**************************************************************************//**
 * Get channel map preset.
 * @return Configured channel map preset
 *****************************************************************************/
cs_channel_map_preset_t app_cs_get_channel_map_preset(void);

/**************************************************************************//**
 * On MTU changed event.
 * @param[in] mtu MTU value
 *****************************************************************************/
void app_on_mtu_changed(uint16_t mtu);

/**************************************************************************//**
 * Update actual connection PHY in CS configs.
 * @param[in] phy Connection PHY value
 *****************************************************************************/
void app_on_connection_phy_changed(uint8_t phy);

/**************************************************************************//**
 * CS setup complete event handler.
 *****************************************************************************/
void app_on_cs_setup_complete();

#endif // APP_H
