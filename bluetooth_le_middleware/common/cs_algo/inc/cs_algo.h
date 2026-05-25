/***************************************************************************//**
 * @file
 * @brief CS Algo - API header
 *******************************************************************************
 * # License
 * <b>Copyright 2025 Silicon Laboratories Inc. www.silabs.com</b>
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
#ifndef CS_ALGO_H
#define CS_ALGO_H

// -----------------------------------------------------------------------------
// Includes

#include <stdint.h>
#include <stdbool.h>
#include "sl_status.h"
#include "sl_common.h"
#include "sl_bt_api.h"
#include "sl_rtl_service.h"
#include "cs_result_config.h"
#include "cs_rreq.h"
#include "cs_common.h"

// -----------------------------------------------------------------------------
// Definitions


/// ALGO error event identifiers reported via @ref cs_algo_on_error_t.
SL_ENUM(cs_algo_error_t) {
  //cs_algo component level errors
  CS_ALGO_ERROR_CREATE_FAILED = 0,
  CS_ALGO_ERROR_CONFIGURE_FAILED,
  CS_ALGO_ERROR_REMOVE_FAILED,
  CS_ALGO_ERROR_INVALID_ARGUMENT,
  CS_ALGO_ERROR_RESULT_APPEND_FAILED,

  //RTL library errors
  CS_ALGO_ERROR_RTL_ARGUMENT,                         ///< SL_RTL_ERROR_ARGUMENT
  CS_ALGO_ERROR_RTL_OUT_OF_MEMORY,                    ///< SL_RTL_ERROR_OUT_OF_MEMORY
  CS_ALGO_ERROR_RTL_ESTIMATION_IN_PROGRESS,           ///< SL_RTL_ERROR_ESTIMATION_IN_PROGRESS (intermediate progress, see on_intermediate_result)
  CS_ALGO_ERROR_RTL_NUMBER_OF_SNAPSHOTS_DO_NOT_MATCH, ///< SL_RTL_ERROR_NUMBER_OF_SNAPHOTS_DO_NOT_MATCH
  CS_ALGO_ERROR_RTL_ESTIMATOR_NOT_CREATED,            ///< SL_RTL_ERROR_ESTIMATOR_NOT_CREATED
  CS_ALGO_ERROR_RTL_ESTIMATOR_ALREADY_CREATED,        ///< SL_RTL_ERROR_ESTIMATOR_ALREADY_CREATED
  CS_ALGO_ERROR_RTL_NOT_INITIALIZED,                  ///< SL_RTL_ERROR_NOT_INITIALIZED
  CS_ALGO_ERROR_RTL_INTERNAL,                         ///< SL_RTL_ERROR_INTERNAL
  CS_ALGO_ERROR_RTL_IQ_SAMPLE_QA,                     ///< SL_RTL_ERROR_IQ_SAMPLE_QA
  CS_ALGO_ERROR_RTL_FEATURE_NOT_SUPPORTED,            ///< SL_RTL_ERROR_FEATURE_NOT_SUPPORTED
  CS_ALGO_ERROR_RTL_INCORRECT_MEASUREMENT,            ///< SL_RTL_ERROR_INCORRECT_MEASUREMENT
  CS_ALGO_ERROR_RTL_CS_CHANNEL_MAP_TOO_SPARSE,        ///< SL_RTL_ERROR_CS_CHANNEL_MAP_TOO_SPARSE
  CS_ALGO_ERROR_RTL_CS_CHANNEL_MAP_TOO_FEW_CHANNELS,  ///< SL_RTL_ERROR_CS_CHANNEL_MAP_TOO_FEW_CHANNELS
  CS_ALGO_ERROR_RTL_CS_CHANNEL_SPACING_TOO_LARGE,     ///< SL_RTL_ERROR_CS_CHANNEL_SPACING_TOO_LARGE
  CS_ALGO_ERROR_RTL_POOR_INPUT_DATA_QUALITY,          ///< SL_RTL_ERROR_POOR_INPUT_DATA_QUALITY
  CS_ALGO_ERROR_RTL_QUEUE_FULL,                       ///< SL_RTL_ERROR_QUEUE_FULL
  CS_ALGO_ERROR_RTL_UNKNOWN,                          ///< Any other sl_rtl_error_code value
};

/// RTL library configuration
SL_PACK_START(1)
typedef struct {
  bool rtl_logging_enabled; //< Enable RTL logging
  cs_algo_mode_t algo_mode; //< Algorithm mode
} SL_ATTRIBUTE_PACKED rtl_config_t;
SL_PACK_END()

/// Intermediate result type
typedef struct {
  float progress_percentage; //< Progress percentage
  uint8_t connection; //< Connection handle
} cs_intermediate_result_t;

/// Contains parameters needed for RTL estimator creation
typedef struct {
  float rssi_ref_tx_power; //< Reference TX power in dBm
  uint16_t connection_interval; //< Connection interval
  uint8_t cs_main_mode; //< CS main mode
  uint8_t cs_sub_mode; //< CS sub mode
  uint8_t min_main_mode_steps; //< Minimum main mode steps
  uint8_t max_main_mode_steps; //< Maximum main mode steps
  uint8_t main_mode_repetition; //< Main mode repetition
  uint8_t channel_map_repetition; //< Channel map repetition
  uint8_t channel_selection_type; //< Channel selection type
  uint8_t ch3c_shape; //< Channel 3C shape
  uint8_t ch3c_jump; //< Channel 3C jump
  uint8_t rtt_type; //< RTT type
  uint8_t cs_sync_phy; //< CS sync PHY
  uint8_t channel_map_preset; //< Channel map preset
  uint8_t num_calib_steps; //< Number of calibration steps
  uint8_t T_PM_time; //< PM time in milliseconds
  uint8_t T_IP1_time; //< IP1 time in milliseconds
  uint8_t T_IP2_time; //< IP2 time in milliseconds
  uint8_t T_FCS_time; //< FCS time in milliseconds
  uint8_t num_antenna_paths; //< Number of antenna paths
  sl_bt_cs_channel_map_t channel_map; //< Channel map
  rtl_config_t rtl_config; //< RTL-specific configuration
} cs_algo_config_t;


#ifdef __cplusplus
extern "C"
{
#endif

/***************************************************************************//**
* Callback function types for reporting results.
* @param[in] conn_handle connection handle.
* @param[in] ranging_counter ranging counter.
* @param[in] result pointer to the result.
* @param[in] result_size size of the result.
* @param[in] ranging_data pointer to the ranging data.
 ******************************************************************************/
typedef void (*cs_algo_on_result_t)(uint8_t conn_handle,
                                    uint16_t ranging_counter,
                                    const uint8_t *result,
                                    uint16_t result_size,
                                    const cs_rreq_result_t *ranging_data);

/***************************************************************************//**
* Callback function type for reporting intermediate results.
* @param[in] intermediate_result pointer to the intermediate result.
 ******************************************************************************/
typedef void (*cs_algo_on_intermediate_result_t)(const cs_intermediate_result_t *intermediate_result);

/***************************************************************************//**
* Callback function type for reporting extended results.
* @param[in] conn_handle connection handle.
* @param[in] ranging_counter ranging counter.
* @param[in] result pointer to the result.
* @param[in] result_size size of the result.
* @param[in] ranging_data pointer to the ranging data.
 ******************************************************************************/
typedef void (*cs_algo_on_extended_result_t)(uint8_t conn_handle,
                                             uint16_t ranging_counter,
                                             const uint8_t *result,
                                             uint16_t result_size,
                                             const cs_rreq_result_t *ranging_data);

/**************************************************************************//**
 * @brief Callback invoked when an error occurs during ALGO operation.
 *
 * @param[in] conn_handle Connection handle.
 * @param[in] error       Error event identifier (@ref cs_algo_error_t).
 * @param[in] sc          Status code.
 *****************************************************************************/
typedef void (*cs_algo_on_error_t)(uint8_t conn_handle,
                                   cs_algo_error_t error,
                                   sl_status_t sc);
/// Callback structre for result
typedef struct {
  cs_algo_on_result_t on_result; //< Callback function for reporting results.
  cs_algo_on_intermediate_result_t on_intermediate_result; //< Callback function for reporting intermediate results.
  cs_algo_on_extended_result_t on_extended_result; //< Callback function for reporting extended results.
  cs_algo_on_error_t on_error;
} cs_algo_app_cb_t;

/***************************************************************************//**
* Callback function type for reporting process finished.
* @param[in] conn_handle connection handle.
* @param[in] ranging_counter ranging counter.
* @param[in] status status of the process.
 ******************************************************************************/
 typedef void (*cs_algo_on_process_finished_t)(uint8_t conn_handle,
                                               uint16_t ranging_counter,
                                               uint32_t status);

// -----------------------------------------------------------------------------
// Public API

/***************************************************************************//**
 * Register app callback functions for cs_algo events.
 * @param[in] cb pointer to the callback structure.
 * @return status of the operation.
 ******************************************************************************/
sl_status_t cs_algo_app_set_callback(cs_algo_app_cb_t *cb);

/***************************************************************************//**
 * Configure cs_algo for a given (connection, config) pair. Initializes the
 * RTL library instance and creates estimator for the passed configuration.
 *
 * @param[in] conn_handle connection handle.
 * @param[in] config algorithm configuration
 * @return status of the operation.
 ******************************************************************************/
sl_status_t cs_algo_create(uint8_t conn_handle, cs_algo_config_t config);

/***************************************************************************//**
 * Remove the cs_algo instance associated with a connection.
 *
 * @param[in] conn_handle connection handle of the instance to remove.
 * @return SL_STATUS_OK on success,
 *         SL_STATUS_NOT_FOUND if no instance matches
 ******************************************************************************/
sl_status_t cs_algo_remove(uint8_t conn_handle);

#ifdef __cplusplus
}
#endif

#endif // CS_ALGO_H
