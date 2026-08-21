/***************************************************************************//**
 * @file
 * @brief CS ACP host - CS result handling
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
#include <stdint.h>
#include <stdbool.h>
#include "app.h"
#include "cs_result.h"
#include "cs_rreq_api.h"
//#include "trace.h"

// -----------------------------------------------------------------------------
// Private variables

static uint8_t measurement_counter = 0u;

// -----------------------------------------------------------------------------
// Public functions

/******************************************************************************
 * Extract measurement results
 *****************************************************************************/
void app_on_result(uint8_t conn_handle,
                   uint16_t ranging_counter,
                   const uint8_t *result,
                   uint16_t result_size,
                   const cs_algo_result_t *ranging_data)
{
  (void)ranging_data;
  cs_result_session_data_t result_data;
  cs_result_initialize_results_data(&result_data);
  uint8_t main_mode = app_cs_get_main_mode();
  uint8_t sub_mode = app_cs_get_sub_mode();
  cs_algo_mode_t algo_mode = app_cs_get_algo_mode();
  cs_channel_map_preset_t channel_map_preset = app_cs_get_channel_map_preset();
  sl_status_t sc = SL_STATUS_OK;

  if (result != NULL) {
    initiator_instance_t *initiator = app_get_instance(conn_handle);
    if (initiator == NULL) {
      log_error(APP_INSTANCE_PREFIX "Failed to get initiator instance in result callback!" APP_LOG_NL,
                conn_handle);
      return;
    }

    sc = cs_result_create_session_data((uint8_t *)result, result_size, &result_data);
    if (sc != SL_STATUS_OK) {
      log_error(APP_INSTANCE_PREFIX "Failed to create session data! [sc: 0x%lx]" NL,
                conn_handle,
                (unsigned long)sc);
      return;
    }
    sc = cs_result_extract_field(&result_data,
                                 CS_RESULT_FIELD_DISTANCE_MAINMODE,
                                 (uint8_t *)result,
                                 (uint8_t *)&initiator->measurement_mainmode.distance_filtered);
    if (sc != SL_STATUS_OK) {
      log_error(APP_INSTANCE_PREFIX "Failed to extract distance! [sc: 0x%lx]" NL,
                conn_handle,
                (unsigned long)sc);
    }

    if (sub_mode != sl_bt_cs_submode_disabled) {
      sc = cs_result_extract_field(&result_data,
                                   CS_RESULT_FIELD_DISTANCE_SUBMODE,
                                   (uint8_t *)result,
                                   (uint8_t *)&initiator->measurement_submode.distance_filtered);
      if (sc != SL_STATUS_OK) {
        log_error(APP_INSTANCE_PREFIX "Failed to extract sub mode distance! [sc: 0x%lx]" NL,
                  conn_handle,
                  (unsigned long)sc);
      }
    }

    sc = cs_result_extract_field(&result_data,
                                 CS_RESULT_FIELD_DISTANCE_RAW_MAINMODE,
                                 (uint8_t *)result,
                                 (uint8_t *)&initiator->measurement_mainmode.distance_raw);
    if (sc != SL_STATUS_OK) {
      log_error(APP_INSTANCE_PREFIX "Failed to extract RAW distance! [sc: 0x%lx]" NL,
                conn_handle,
                (unsigned long)sc);
    }

    if (sub_mode != sl_bt_cs_submode_disabled) {
      sc = cs_result_extract_field(&result_data,
                                   CS_RESULT_FIELD_DISTANCE_RAW_SUBMODE,
                                   (uint8_t *)result,
                                   (uint8_t *)&initiator->measurement_submode.distance_raw);
      if (sc != SL_STATUS_OK) {
        log_error(APP_INSTANCE_PREFIX "Failed to extract sub mode RAW distance! [sc: 0x%lx]" NL,
                  conn_handle,
                  (unsigned long)sc);
      }
    }

    sc = cs_result_extract_field(&result_data,
                                 CS_RESULT_FIELD_LIKELINESS_MAINMODE,
                                 (uint8_t *)result,
                                 (uint8_t *)&initiator->measurement_mainmode.likeliness);
    if (sc != SL_STATUS_OK) {
      log_error(APP_INSTANCE_PREFIX "Failed to extract likeliness! [sc: 0x%lx]" NL,
                conn_handle,
                (unsigned long)sc);
    }

    if (sub_mode != sl_bt_cs_submode_disabled) {
      sc = cs_result_extract_field(&result_data,
                                   CS_RESULT_FIELD_LIKELINESS_SUBMODE,
                                   (uint8_t *)result,
                                   (uint8_t *)&initiator->measurement_submode.likeliness);
      if (sc != SL_STATUS_OK) {
        log_error(APP_INSTANCE_PREFIX "Failed to extract sub mode likeliness! [sc: 0x%lx]" NL,
                  conn_handle,
                  (unsigned long)sc);
      }
    }

    if (algo_mode == CS_ALGO_MODE_TRACKING_LATENCY_OPTIMIZED
        && main_mode == sl_bt_cs_mode_pbr
        && (channel_map_preset == CS_CHANNEL_MAP_PRESET_HIGH
            || channel_map_preset == CS_CHANNEL_MAP_PRESET_MEDIUM)) {
      sc = cs_result_extract_field(&result_data,
                                   CS_RESULT_FIELD_VELOCITY_MAINMODE,
                                   (uint8_t *)result,
                                   (uint8_t *)&initiator->measurement_mainmode.velocity);
      if (sc != SL_STATUS_OK) {
        log_error(APP_INSTANCE_PREFIX "Failed to extract velocity! [sc: 0x%lx]" NL,
                  conn_handle,
                  (unsigned long)sc);
      }
    }

    // BER is only for RTT
    if (main_mode == sl_bt_cs_mode_rtt) {
      sc = cs_result_extract_field(&result_data,
                                   CS_RESULT_FIELD_BIT_ERROR_RATE,
                                   (uint8_t *)result,
                                   (uint8_t *)&initiator->measurement_mainmode.bit_error_rate);
      if (sc != SL_STATUS_OK) {
        log_error(APP_INSTANCE_PREFIX "Failed to extract BER! [sc: 0x%lx]" NL,
                  conn_handle,
                  (unsigned long)sc);
      }
    }

    // Extract RSSI distance always
    sc = cs_result_extract_field(&result_data,
                                 CS_RESULT_FIELD_DISTANCE_RSSI,
                                 (uint8_t *)result,
                                 (uint8_t *)&initiator->measurement_mainmode.distance_estimate_rssi);
    if (sc != SL_STATUS_OK) {
      log_error(APP_INSTANCE_PREFIX "Failed to extract RSSI distance! [sc: 0x%lx]" NL,
                conn_handle,
                (unsigned long)sc);
    }
    initiator->measurement_arrived = true;
    initiator->measurement_cnt++;
    initiator->ranging_counter = ranging_counter;
  } else {
    log_info(APP_INSTANCE_PREFIX "RTL process skipped!" NL,
             conn_handle);
  }
}

/******************************************************************************
 * Extract intermediate results between measurement results
 * Note: only called when Stationary mode used
 *****************************************************************************/
void app_on_intermediate_result(const cs_intermediate_result_t * intermediate_result)
{
  if (intermediate_result == NULL) {
    log_error(APP_INSTANCE_PREFIX "Intermediate result is NULL!" NL, SL_BT_INVALID_CONNECTION_HANDLE);
    return;
  }

  initiator_instance_t *initiator = app_get_instance(intermediate_result->connection);

  if (initiator == NULL) {
    log_error(APP_INSTANCE_PREFIX "Failed to get initiator instance in intermediate result callback!" NL,
              intermediate_result->connection);
    return;
  }
  memcpy(&initiator->measurement_progress,
         intermediate_result,
         sizeof(cs_intermediate_result_t));
  initiator->measurement_progress_changed = true;
}

/******************************************************************************
 * Write measurement results to the display and to the iostream
 *****************************************************************************/
void app_print_head_and_data(initiator_instance_t *initiator)
{
  uint8_t sub_mode = app_cs_get_sub_mode();
  const bd_addr *bt_address = sl_bt_peer_manager_get_bt_address(initiator->conn_handle);
  for (uint8_t is_data = ((measurement_counter % CS_HOST_HEADER_LOG) > 0); is_data <= 1; is_data++) {
    log_info(APP_INSTANCE_PREFIX, initiator->conn_handle);
    cs_result_print_bt_address(!is_data, bt_address);

    cs_result_print(CS_RESULT_FIELD_DISTANCE_MAINMODE,
                    !is_data,
                    &(initiator->measurement_mainmode.distance_filtered));
    // Distance submode
    if (sub_mode != sl_bt_cs_submode_disabled) {
      cs_result_print(CS_RESULT_FIELD_DISTANCE_SUBMODE,
                      !is_data,
                      &(initiator->measurement_submode.distance_filtered));
    }
    // Distance RAW
    cs_result_print(CS_RESULT_FIELD_DISTANCE_RAW_MAINMODE,
                    !is_data,
                    &(initiator->measurement_mainmode.distance_raw));
    // Distance submode RAW
    if (sub_mode != sl_bt_cs_submode_disabled) {
      cs_result_print(CS_RESULT_FIELD_DISTANCE_RAW_SUBMODE,
                      !is_data,
                      &(initiator->measurement_submode.distance_raw));
    }
    // Likeliness
    cs_result_print(CS_RESULT_FIELD_LIKELINESS_MAINMODE,
                    !is_data,
                    &(initiator->measurement_mainmode.likeliness));
    // Likeliness submode
    if (sub_mode != sl_bt_cs_submode_disabled) {
      cs_result_print(CS_RESULT_FIELD_LIKELINESS_SUBMODE,
                      !is_data,
                      &(initiator->measurement_submode.likeliness));
    }
    // RSSI distance
    cs_result_print(CS_RESULT_FIELD_DISTANCE_RSSI,
                    !is_data,
                    &(initiator->measurement_mainmode.distance_estimate_rssi));
    // Velocity
    cs_result_print(CS_RESULT_FIELD_VELOCITY_MAINMODE,
                    !is_data,
                    &(initiator->measurement_mainmode.velocity));

    // BER
    cs_result_print(CS_RESULT_FIELD_BIT_ERROR_RATE,
                    !is_data,
                    &(initiator->measurement_mainmode.bit_error_rate));
    log_append(APP_LOG_NL);
  }

  measurement_counter++;
}
