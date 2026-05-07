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
#include "sl_rtl_clib_api.h"
#include "cs_result_config.h"
#include "cs_rreq.h"
// TODO: fix this requirement
#include "cs_initiator_client.h"
#include "cs_initiator.h"

#ifdef __cplusplus
extern "C"
{
#endif

// -----------------------------------------------------------------------------
// Datatypes

/// Contains parameters needed for RTL estimator creation
typedef struct {
  float   rssi_ref_tx_power;
  uint16_t connection_interval;
  uint8_t cs_main_mode;
  uint8_t cs_sub_mode;
  uint8_t min_main_mode_steps;
  uint8_t max_main_mode_steps;
  uint8_t main_mode_repetition;
  uint8_t channel_map_repetition;
  uint8_t channel_selection_type;
  uint8_t ch3c_shape;
  uint8_t ch3c_jump;
  uint8_t rtt_type;
  uint8_t cs_sync_phy;
  uint8_t channel_map_preset;
  uint8_t num_calib_steps;
  uint8_t T_PM_time;
  uint8_t T_IP1_time;
  uint8_t T_IP2_time;
  uint8_t T_FCS_time;
  uint8_t num_antenna_paths;
  sl_bt_cs_channel_map_t channel_map;
  rtl_config_t rtl_config;  // RTL-specific input
} cs_algo_config_t;

typedef void (*cs_algo_on_result_t)(uint8_t conn_handle, uint16_t ranging_counter,
                                    const uint8_t *result, uint16_t result_size,
                                    const cs_rreq_result_t *ranging_data);
typedef void (*cs_algo_on_intermediate_result_t)(const cs_intermediate_result_t *intermediate_result);
typedef void (*cs_algo_on_extended_result_t)(uint8_t conn_handle, uint16_t ranging_counter,
                                             const uint8_t *result, uint16_t result_size,
                                             const cs_rreq_result_t *ranging_data);

typedef void (*cs_algo_on_process_finished_t)(uint8_t conn_handle, uint16_t ranging_counter,
                                              uint32_t status);

typedef struct {
  cs_algo_on_result_t on_result;
  cs_algo_on_intermediate_result_t on_intermediate_result;
  cs_algo_on_extended_result_t on_extended_result;
} cs_algo_app_cb_t;

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
