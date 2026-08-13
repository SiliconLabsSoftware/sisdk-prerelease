/***************************************************************************//**
 * @file
 * @brief CS Algo - estimation implementation
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

#include <string.h>
#include <stdbool.h>
#include "app_rta.h"
#include "cs_result.h"
#include "sl_rtl_service.h"
#include "cs_result_config.h"
#include "cs_rreq.h"
#include "cs_algo_log_internal.h"
#include "cs_algo_internal.h"
#include "cs_algo_config.h"
#include "cs_algo.h"

// -----------------------------------------------------------------------------
// Internal types

typedef struct {
  sl_rtl_service_cs_inst_t      *rtl_inst;
  cs_algo_result_t              ranging_data;
  cs_algo_config_t              config;
  cs_result_session_data_t      result_data;
  sl_rtl_ras_measurement        initiator_meas;
  sl_rtl_ras_measurement        reflector_meas;
  sl_rtl_ras_procedure          procedure_data;
  bool                          in_use;
  bool                          estimator_created;
  uint16_t                      ranging_counter;
  uint8_t                       conn_handle;
  uint8_t                       result[CS_RESULT_MAX_BUFFER_SIZE];
} cs_algo_instance_t;

// -----------------------------------------------------------------------------
// Static function declarations
static void algo_error(cs_algo_instance_t *inst,
                       cs_algo_error_t evt,
                       sl_status_t sc);
static void show_rtl_api_call_result(cs_algo_instance_t *inst,
                                     enum sl_rtl_error_code err_code);
static void report_result(cs_algo_instance_t *inst,
                          const sl_rtl_service_cs_result_t *result,
                          uint16_t ranging_counter,
                          cs_algo_result_t *ranging_data);
static void report_intermediate_result(cs_algo_instance_t *inst,
                                       const sl_rtl_service_cs_result_t *result);
static void cs_mode_converter(uint8_t cs_mode_bt,
                              char **cs_mode_str,
                              sl_rtl_cs_mode *cs_mode_rtl);
static void build_rtl_cs_params(const cs_algo_config_t *src,
                                sl_rtl_cs_params *dst);
static cs_algo_instance_t *cs_algo_get_instance(uint8_t conn_handle);
static cs_algo_instance_t *cs_algo_get_free_slot(void);
static cs_algo_instance_t *cs_algo_find_by_rtl_inst(sl_rtl_service_cs_inst_t *rtl_inst);
static sl_status_t cs_algo_save_result(const cs_rreq_result_t *src,
                                       cs_algo_instance_t *inst);
static void cs_algo_rtl_service_result_cb(sl_rtl_service_cs_inst_t *rtl_inst,
                                          const sl_rtl_service_cs_result_t *result,
                                          enum sl_rtl_error_code process_status,
                                          void *user_data);
static void on_app_rta_error(app_rta_error_t error, sl_status_t result);

// -----------------------------------------------------------------------------
// Static variables

static cs_algo_instance_t algo_instances[CS_ALGO_CONFIG_ESTIMATOR_COUNT];
static cs_algo_event_callback_t callbacks;
static app_rta_context_t app_rta_ctx;
static sl_rtl_service_cs_ctx_t *algo_rtl_svc_ctx;

// -----------------------------------------------------------------------------
// Static function definitions

/******************************************************************************
 * Call user error callback.
 *
 * @param[in] inst cs_algo instance reference.
 * @param[in] evt  ALGO error event identifier.
 * @param[in] sc   Underlying status code (see @ref cs_algo_error_t).
 *****************************************************************************/
static void algo_error(cs_algo_instance_t *inst,
                       cs_algo_error_t evt,
                       sl_status_t sc)
{
  if (inst == NULL) {
    algo_log_error("[#?] Instance is NULL! (sc: 0x%lx)" LOG_NL,
                   (unsigned long)sc);
    return;
  }
  algo_log_error(INSTANCE_PREFIX "Error occurred (sc: 0x%lx)" LOG_NL,
                 inst->conn_handle,
                 (unsigned long)sc);
  if (callbacks.on_error != NULL) {
    callbacks.on_error(inst->conn_handle, inst->ranging_counter, evt, sc);
  }
}

/******************************************************************************
 * Log a description of an RTL service call outcome, and on non-success
 * forward the matching @ref cs_algo_error_t event to the
 * application via @ref algo_error().
 *
 * @param[in] inst     cs_algo instance.
 * @param[in] err_code RTL API error code returned by the rtl_service call.
 *****************************************************************************/
static void show_rtl_api_call_result(cs_algo_instance_t *inst,
                                     enum sl_rtl_error_code err_code)
{
  cs_algo_error_t app_error;
  uint8_t conn_handle = (inst != NULL) ? inst->conn_handle : 0xFF;

  switch (err_code) {
    case SL_RTL_ERROR_SUCCESS:
      return;

    case SL_RTL_ERROR_ARGUMENT:
      algo_log_error(INSTANCE_PREFIX "RTL - invalid argument! [E: 0x%x]" LOG_NL,
                     conn_handle, err_code);
      app_error = CS_ALGO_ERROR_RTL_ARGUMENT;
      break;

    case SL_RTL_ERROR_OUT_OF_MEMORY:
      algo_log_error(INSTANCE_PREFIX "RTL - memory allocation error! [E: 0x%x]" LOG_NL,
                     conn_handle, err_code);
      app_error = CS_ALGO_ERROR_RTL_OUT_OF_MEMORY;
      break;

    case SL_RTL_ERROR_ESTIMATION_IN_PROGRESS:
      algo_log_error(INSTANCE_PREFIX "RTL - estimation not yet finished! [E: 0x%x]" LOG_NL,
                     conn_handle, err_code);
      app_error = CS_ALGO_ERROR_RTL_ESTIMATION_IN_PROGRESS;
      break;

    case SL_RTL_ERROR_NUMBER_OF_SNAPHOTS_DO_NOT_MATCH:
      algo_log_error(INSTANCE_PREFIX "RTL - initialized and calculated "
                                     "snapshots do not match! [E: 0x%x]" LOG_NL,
                     conn_handle, err_code);
      app_error = CS_ALGO_ERROR_RTL_NUMBER_OF_SNAPSHOTS_DO_NOT_MATCH;
      break;

    case SL_RTL_ERROR_ESTIMATOR_NOT_CREATED:
      algo_log_error(INSTANCE_PREFIX "RTL - estimator not created! [E: 0x%x]" LOG_NL,
                     conn_handle, err_code);
      app_error = CS_ALGO_ERROR_RTL_ESTIMATOR_NOT_CREATED;
      break;

    case SL_RTL_ERROR_ESTIMATOR_ALREADY_CREATED:
      algo_log_error(INSTANCE_PREFIX "RTL - estimator already created! [E: 0x%x]" LOG_NL,
                     conn_handle, err_code);
      app_error = CS_ALGO_ERROR_RTL_ESTIMATOR_ALREADY_CREATED;
      break;

    case SL_RTL_ERROR_NOT_INITIALIZED:
      algo_log_error(INSTANCE_PREFIX "RTL - library item not initialized! [E: 0x%x]" LOG_NL,
                     conn_handle, err_code);
      app_error = CS_ALGO_ERROR_RTL_NOT_INITIALIZED;
      break;

    case SL_RTL_ERROR_INTERNAL:
      algo_log_error(INSTANCE_PREFIX "RTL - internal error! [E: 0x%x]" LOG_NL,
                     conn_handle, err_code);
      app_error = CS_ALGO_ERROR_RTL_INTERNAL;
      break;

    case SL_RTL_ERROR_IQ_SAMPLE_QA:
      algo_log_error(INSTANCE_PREFIX "RTL - IQ sample quality analysis failed! [E: 0x%x]" LOG_NL,
                     conn_handle, err_code);
      app_error = CS_ALGO_ERROR_RTL_IQ_SAMPLE_QA;
      break;

    case SL_RTL_ERROR_FEATURE_NOT_SUPPORTED:
      algo_log_error(INSTANCE_PREFIX "RTL - feature not supported! [E: 0x%x]" LOG_NL,
                     conn_handle, err_code);
      app_error = CS_ALGO_ERROR_RTL_FEATURE_NOT_SUPPORTED;
      break;

    case SL_RTL_ERROR_INCORRECT_MEASUREMENT:
      algo_log_error(INSTANCE_PREFIX "RTL - incorrect measurement! Error of the last"
                                     " measurement was too large! [E: 0x%x]" LOG_NL,
                     conn_handle, err_code);
      app_error = CS_ALGO_ERROR_RTL_INCORRECT_MEASUREMENT;
      break;

    case SL_RTL_ERROR_CS_CHANNEL_MAP_TOO_SPARSE:
      algo_log_error(INSTANCE_PREFIX "RTL - too many skipped channels "
                                     "in the proposed channel map! [E: 0x%x]" LOG_NL,
                     conn_handle, err_code);
      app_error = CS_ALGO_ERROR_RTL_CS_CHANNEL_MAP_TOO_SPARSE;
      break;

    case SL_RTL_ERROR_CS_CHANNEL_MAP_TOO_FEW_CHANNELS:
      algo_log_error(INSTANCE_PREFIX "RTL - too few channels "
                                     "in the proposed channel map! [E: 0x%x]" LOG_NL,
                     conn_handle, err_code);
      app_error = CS_ALGO_ERROR_RTL_CS_CHANNEL_MAP_TOO_FEW_CHANNELS;
      break;

    case SL_RTL_ERROR_CS_CHANNEL_SPACING_TOO_LARGE:
      algo_log_error(INSTANCE_PREFIX "RTL - channel spacing is too large "
                                     "in the proposed channel map! [E: 0x%x]" LOG_NL,
                     conn_handle, err_code);
      app_error = CS_ALGO_ERROR_RTL_CS_CHANNEL_SPACING_TOO_LARGE;
      break;

    case SL_RTL_ERROR_POOR_INPUT_DATA_QUALITY:
      algo_log_error(INSTANCE_PREFIX "RTL - input data quality is poor! [E: 0x%x]" LOG_NL,
                     conn_handle, err_code);
      app_error = CS_ALGO_ERROR_RTL_POOR_INPUT_DATA_QUALITY;
      break;

    case SL_RTL_ERROR_QUEUE_FULL:
      algo_log_error(INSTANCE_PREFIX "RTL - task input queue is full! [E: 0x%x]" LOG_NL,
                     conn_handle, err_code);
      app_error = CS_ALGO_ERROR_RTL_QUEUE_FULL;
      break;

    default:
      algo_log_error(INSTANCE_PREFIX "RTL - unknown error! [E: 0x%x]" LOG_NL,
                     conn_handle, err_code);
      app_error = CS_ALGO_ERROR_RTL_UNKNOWN;
      break;
  }

  algo_error(inst, app_error, (sl_status_t)err_code);
}

/******************************************************************************
 * Handle successful RTL process, and get distance.
 *
 * @param[in] inst cs_algo instance.
 * @param[in] result rtl_service result handle (valid only in callback scope).
 * @param[in] ranging_counter procedure ranging counter.
 * @param[in] ranging_data pointer to the RREQ ranging data buffer.
 *****************************************************************************/
static void report_result(cs_algo_instance_t *inst,
                          const sl_rtl_service_cs_result_t *result,
                          uint16_t ranging_counter,
                          cs_algo_result_t *ranging_data)
{
  sl_status_t sc = SL_STATUS_OK;
  bool estimation_valid = false;
  enum sl_rtl_error_code rtl_err = SL_RTL_ERROR_NOT_INITIALIZED;
  sl_rtl_cs_estimator_param param;
  sl_rtl_cs_distance_estimate_mode mode;

  float rtl_value = 0.0f;
  float last_known_distance = 0.0f;

  cs_result_initialize_results_data(&inst->result_data);

  if (inst->config.cs_sub_mode == sl_bt_cs_submode_disabled) {
    mode = SL_RTL_CS_BEST_ESTIMATE;
  } else {
    mode = SL_RTL_CS_MAIN_MODE_ESTIMATE;
  }

  // --------------------------------
  // Get distance
  rtl_err = sl_rtl_service_get_cs_distance_estimate(result,
                                                    SL_RTL_CS_DISTANCE_ESTIMATE_TYPE_FILTERED,
                                                    mode,
                                                    &last_known_distance);
  show_rtl_api_call_result(inst, rtl_err);
  if (rtl_err == SL_RTL_ERROR_SUCCESS) {
    sc = cs_result_append_field(&inst->result_data,
                                CS_RESULT_FIELD_DISTANCE_MAINMODE,
                                (uint8_t *)&last_known_distance,
                                inst->result);
    if (sc != SL_STATUS_OK) {
      algo_log_error(INSTANCE_PREFIX "failed to append distance! [sc: 0x%lx]" LOG_NL,
                     inst->conn_handle,
                     (unsigned long)sc);
      algo_error(inst, CS_ALGO_ERROR_RESULT_APPEND_FAILED, sc);
    } else {
      estimation_valid = true;
    }
  }

  if (inst->config.cs_sub_mode != sl_bt_cs_submode_disabled) {
    rtl_err = sl_rtl_service_get_cs_distance_estimate(result,
                                                      SL_RTL_CS_DISTANCE_ESTIMATE_TYPE_FILTERED,
                                                      SL_RTL_CS_SUB_MODE_ESTIMATE,
                                                      &last_known_distance);
    show_rtl_api_call_result(inst, rtl_err);
    if (rtl_err == SL_RTL_ERROR_SUCCESS) {
      sc = cs_result_append_field(&inst->result_data,
                                  CS_RESULT_FIELD_DISTANCE_SUBMODE,
                                  (uint8_t *)&last_known_distance,
                                  inst->result);
      if (sc != SL_STATUS_OK) {
        algo_log_error(INSTANCE_PREFIX "failed to append sub mode distance! [sc: 0x%lx]" LOG_NL,
                       inst->conn_handle,
                       (unsigned long)sc);
        algo_error(inst, CS_ALGO_ERROR_RESULT_APPEND_FAILED, sc);
      } else {
        estimation_valid = true;
      }
    }
  }

  // --------------------------------
  // Get RAW distance
  rtl_err = sl_rtl_service_get_cs_distance_estimate(result,
                                                    SL_RTL_CS_DISTANCE_ESTIMATE_TYPE_RAW,
                                                    mode,
                                                    &rtl_value);
  show_rtl_api_call_result(inst, rtl_err);
  if (rtl_err == SL_RTL_ERROR_SUCCESS) {
    sc = cs_result_append_field(&inst->result_data,
                                CS_RESULT_FIELD_DISTANCE_RAW_MAINMODE,
                                (uint8_t *)&rtl_value,
                                inst->result);
    if (sc != SL_STATUS_OK) {
      algo_log_error(INSTANCE_PREFIX "failed to append RAW distance! [sc: 0x%lx]" LOG_NL,
                     inst->conn_handle,
                     (unsigned long)sc);
      algo_error(inst, CS_ALGO_ERROR_RESULT_APPEND_FAILED, sc);
    } else {
      estimation_valid = true;
    }
  }

  if (inst->config.cs_sub_mode != sl_bt_cs_submode_disabled) {
    rtl_err = sl_rtl_service_get_cs_distance_estimate(result,
                                                      SL_RTL_CS_DISTANCE_ESTIMATE_TYPE_RAW,
                                                      SL_RTL_CS_SUB_MODE_ESTIMATE,
                                                      &rtl_value);
    show_rtl_api_call_result(inst, rtl_err);
    if (rtl_err == SL_RTL_ERROR_SUCCESS) {
      sc = cs_result_append_field(&inst->result_data,
                                  CS_RESULT_FIELD_DISTANCE_RAW_SUBMODE,
                                  (uint8_t *)&rtl_value,
                                  inst->result);
      if (sc != SL_STATUS_OK) {
        algo_log_error(INSTANCE_PREFIX "failed to append RAW sub mode distance! [sc: 0x%lx]" LOG_NL,
                       inst->conn_handle,
                       (unsigned long)sc);
        algo_error(inst, CS_ALGO_ERROR_RESULT_APPEND_FAILED, sc);
      } else {
        estimation_valid = true;
      }
    }
  }

  // --------------------------------
  // Get distance likeliness
  rtl_err = sl_rtl_service_get_cs_distance_estimate_confidence(result,
                                                               SL_RTL_CS_DISTANCE_ESTIMATE_CONFIDENCE_TYPE_LIKELINESS,
                                                               mode,
                                                               &rtl_value);
  show_rtl_api_call_result(inst, rtl_err);
  if (rtl_err == SL_RTL_ERROR_SUCCESS) {
    sc = cs_result_append_field(&inst->result_data,
                                CS_RESULT_FIELD_LIKELINESS_MAINMODE,
                                (uint8_t *)&rtl_value,
                                inst->result);
    if (sc != SL_STATUS_OK) {
      algo_log_error(INSTANCE_PREFIX "failed to append distance likeliness! [sc: 0x%lx]" LOG_NL,
                     inst->conn_handle,
                     (unsigned long)sc);
      algo_error(inst, CS_ALGO_ERROR_RESULT_APPEND_FAILED, sc);
    } else {
      estimation_valid = true;
    }
  }

  if (inst->config.cs_sub_mode != sl_bt_cs_submode_disabled) {
    rtl_err = sl_rtl_service_get_cs_distance_estimate_confidence(result,
                                                                 SL_RTL_CS_DISTANCE_ESTIMATE_CONFIDENCE_TYPE_LIKELINESS,
                                                                 SL_RTL_CS_SUB_MODE_ESTIMATE,
                                                                 &rtl_value);
    show_rtl_api_call_result(inst, rtl_err);
    if (rtl_err == SL_RTL_ERROR_SUCCESS) {
      sc = cs_result_append_field(&inst->result_data,
                                  CS_RESULT_FIELD_LIKELINESS_SUBMODE,
                                  (uint8_t *)&rtl_value,
                                  inst->result);
      if (sc != SL_STATUS_OK) {
        algo_log_error(INSTANCE_PREFIX "failed to append sub mode distance likeliness! [sc: 0x%lx]" LOG_NL,
                       inst->conn_handle,
                       (unsigned long)sc);
        algo_error(inst, CS_ALGO_ERROR_RESULT_APPEND_FAILED, sc);
      } else {
        estimation_valid = true;
      }
    }
  }

  // --------------------------------
  // Get distance RSSI

  // Set reference TX power for RSSI calculation
  param.type = SL_RTL_REF_TX_POWER;
  param.value.ref_tx_power = inst->config.rssi_ref_tx_power;
  rtl_err = sl_rtl_service_set_cs_estimator_param(inst->rtl_inst, &param);
  show_rtl_api_call_result(inst, rtl_err);

  rtl_err = sl_rtl_service_get_cs_distance_estimate(result,
                                                    SL_RTL_CS_DISTANCE_ESTIMATE_TYPE_RSSI,
                                                    mode,
                                                    &rtl_value);
  show_rtl_api_call_result(inst, rtl_err);
  if (rtl_err == SL_RTL_ERROR_SUCCESS) {
    sc = cs_result_append_field(&inst->result_data,
                                CS_RESULT_FIELD_DISTANCE_RSSI,
                                (uint8_t *)&rtl_value,
                                inst->result);
    if (sc != SL_STATUS_OK) {
      algo_log_error(INSTANCE_PREFIX "failed to append RSSI distance! [sc: 0x%lx]" LOG_NL,
                     inst->conn_handle,
                     (unsigned long)sc);
      algo_error(inst, CS_ALGO_ERROR_RESULT_APPEND_FAILED, sc);
    } else {
      estimation_valid = true;
      param.type = SL_RTL_LAST_KNOWN_DISTANCE;
      param.value.last_known_distance = last_known_distance;
      rtl_err = sl_rtl_service_set_cs_estimator_param(inst->rtl_inst, &param);
      show_rtl_api_call_result(inst, rtl_err);
    }
  }

  // --------------------------------
  // Get velocity
  if (inst->config.rtl_config.algo_mode == SL_RTL_CS_ALGO_MODE_REAL_TIME_FAST
      && inst->config.cs_main_mode == sl_bt_cs_mode_pbr
      && (inst->config.channel_map_preset == CS_CHANNEL_MAP_PRESET_HIGH
          || inst->config.channel_map_preset == CS_CHANNEL_MAP_PRESET_MEDIUM)) {
    rtl_err = sl_rtl_service_get_cs_distance_estimate(result,
                                                      SL_RTL_CS_DISTANCE_ESTIMATE_TYPE_VELOCITY,
                                                      mode,
                                                      &rtl_value);
    show_rtl_api_call_result(inst, rtl_err);
    if (rtl_err == SL_RTL_ERROR_SUCCESS) {
      sc = cs_result_append_field(&inst->result_data,
                                  CS_RESULT_FIELD_VELOCITY_MAINMODE,
                                  (uint8_t *)&rtl_value,
                                  inst->result);
      if (sc != SL_STATUS_OK) {
        algo_log_error(INSTANCE_PREFIX "failed to append velocity! [sc: 0x%lx]" LOG_NL,
                       inst->conn_handle,
                       (unsigned long)sc);
        algo_error(inst, CS_ALGO_ERROR_RESULT_APPEND_FAILED, sc);
      } else {
        estimation_valid = true;
      }
    }
  }

  // --------------------------------
  // Get bit error rate - RTT only
  if (inst->config.cs_main_mode == sl_bt_cs_mode_rtt) {
    rtl_err = sl_rtl_service_get_cs_distance_estimate_confidence(result,
                                                                 SL_RTL_CS_DISTANCE_ESTIMATE_CONFIDENCE_TYPE_BIT_ERROR_RATE,
                                                                 mode,
                                                                 &rtl_value);
    show_rtl_api_call_result(inst, rtl_err);
    if (rtl_err == SL_RTL_ERROR_SUCCESS) {
      sc = cs_result_append_field(&inst->result_data,
                                  CS_RESULT_FIELD_BIT_ERROR_RATE,
                                  (uint8_t *)&rtl_value,
                                  inst->result);
      if (sc != SL_STATUS_OK) {
        algo_log_error(INSTANCE_PREFIX "failed to append BER! [sc: 0x%lx]" LOG_NL,
                       inst->conn_handle,
                       (unsigned long)sc);
        algo_error(inst, CS_ALGO_ERROR_RESULT_APPEND_FAILED, sc);
      } else {
        estimation_valid = true;
      }
    }
  }

  if (estimation_valid) {
    if (callbacks.on_result != NULL) {
      callbacks.on_result(inst->conn_handle,
                          ranging_counter,
                          inst->result,
                          (uint16_t)inst->result_data.size,
                          ranging_data);
    }
  }
}

/******************************************************************************
 * Handle progressive RTL process, and get intermediate result.
 *
 * @param[in] inst cs_algo instance.
 * @param[in] result rtl_service result handle.
 *****************************************************************************/
static void report_intermediate_result(cs_algo_instance_t *inst,
                                       const sl_rtl_service_cs_result_t *result)
{
  enum sl_rtl_error_code rtl_err = SL_RTL_ERROR_NOT_INITIALIZED;
  float progress_percentage = 0.0f;

  rtl_err =
    sl_rtl_service_get_cs_distance_estimate_extended_info(result,
                                                          SL_RTL_CS_DISTANCE_ESTIMATE_EXTENDED_INFO_TYPE_PROGRESS_PERCENTAGE,
                                                          &progress_percentage);
  show_rtl_api_call_result(inst, rtl_err);
  if (rtl_err == SL_RTL_ERROR_SUCCESS
      && callbacks.on_intermediate_result != NULL) {
    cs_intermediate_result_t intermediate_result;
    intermediate_result.connection          = inst->conn_handle;
    intermediate_result.progress_percentage = progress_percentage;
    callbacks.on_intermediate_result(&intermediate_result);
  }
}

static void cs_mode_converter(uint8_t cs_mode_bt,
                              char **cs_mode_str,
                              sl_rtl_cs_mode *cs_mode_rtl)
{
  switch (cs_mode_bt) {
    case sl_bt_cs_mode_rtt:
      *cs_mode_str = "RTT";
      *cs_mode_rtl = SL_RTL_CS_MODE_RTT;
      break;
    case sl_bt_cs_mode_pbr:
      *cs_mode_str = "PBR";
      *cs_mode_rtl = SL_RTL_CS_MODE_PBR;
      break;
    case sl_bt_cs_submode_disabled:
      *cs_mode_str = "disabled";
      *cs_mode_rtl = SL_RTL_CS_MODE_NONE;
      break;
    default:
      *cs_mode_str = "unknown";
      *cs_mode_rtl = SL_RTL_CS_MODE_NONE;
      break;
  }
}

/******************************************************************************
 * Build sl_rtl_cs_params struct from the rtllib-independent cs_algo_config_t
 *
 * @param[in]  src cs_algo configuration to translate.
 * @param[out] dst sl_rtl_cs_params struct to populate.
 *****************************************************************************/
static void build_rtl_cs_params(const cs_algo_config_t *src, sl_rtl_cs_params *dst)
{
  memset(dst, 0, sizeof(*dst));
  // Runtime values from cs_config_complete
  dst->connection_interval = src->connection_interval;
  memcpy(dst->channel_map, src->channel_map.data, sizeof(dst->channel_map));
  dst->num_calib_steps     = src->num_calib_steps;
  dst->T_PM_time           = src->T_PM_time;
  dst->T_IP1_time          = src->T_IP1_time;
  dst->T_IP2_time          = src->T_IP2_time;
  dst->T_FCS_time          = src->T_FCS_time;
  dst->num_antenna_paths   = src->num_antenna_paths;
  dst->min_main_mode_steps    = src->min_main_mode_steps;
  dst->max_main_mode_steps    = src->max_main_mode_steps;
  dst->main_mode_repetition   = src->main_mode_repetition;
  dst->rtt_type               = (sl_rtl_cs_rtt_type)src->rtt_type;
  dst->cs_sync_phy            = src->cs_sync_phy;
  dst->channel_map_repetition = src->channel_map_repetition;
  dst->channel_selection_type = src->channel_selection_type;
  dst->ch3c_shape             = src->ch3c_shape;
  dst->ch3c_jump              = src->ch3c_jump;
}

static cs_algo_instance_t *cs_algo_get_instance(uint8_t conn_handle)
{
  for (uint8_t i = 0; i < CS_ALGO_CONFIG_ESTIMATOR_COUNT; i++) {
    if (algo_instances[i].in_use
        && algo_instances[i].conn_handle == conn_handle) {
      return &algo_instances[i];
    }
  }
  return NULL;
}

static cs_algo_instance_t *cs_algo_get_free_slot(void)
{
  for (uint8_t i = 0; i < CS_ALGO_CONFIG_ESTIMATOR_COUNT; i++) {
    if (!algo_instances[i].in_use) {
      return &algo_instances[i];
    }
  }
  return NULL;
}

static cs_algo_instance_t *cs_algo_find_by_rtl_inst(sl_rtl_service_cs_inst_t *rtl_inst)
{
  for (uint8_t i = 0; i < CS_ALGO_CONFIG_ESTIMATOR_COUNT; i++) {
    if (algo_instances[i].in_use
        && algo_instances[i].rtl_inst == rtl_inst) {
      return &algo_instances[i];
    }
  }
  return NULL;
}

/******************************************************************************
 * Alias the buffers of a cs_rreq_result_t into the cs_algo_result_t embedded
 * in @p inst (shallow, zero-copy).
 *
 * The pointers in @p inst->ranging_data are aliased onto the caller-owned
 * buffers in @p src, so @p src must outlive any use of @p inst->ranging_data.
 * In practice the RREQ buffer is owned by cs_rreq and is only released when
 * cs_rreq_set_process_finished() returns, which cs_algo defers until after the
 * RTL result callback has consumed the data.
 *
 * @param[in]     src  Source RREQ ranging result.
 * @param[in,out] inst Target algo instance.
 * @return SL_STATUS_OK on success,
 *         SL_STATUS_NULL_POINTER if @p src or @p inst is NULL.
 *****************************************************************************/
static sl_status_t cs_algo_save_result(const cs_rreq_result_t *src,
                                       cs_algo_instance_t *inst)
{
  if (src == NULL || inst == NULL) {
    return SL_STATUS_NULL_POINTER;
  }

  cs_algo_result_t *dst = &inst->ranging_data;

  dst->num_steps = src->num_steps;
  dst->step_channels = src->step_channels;

  dst->initiator.data_size = src->initiator.data_size;
  dst->initiator.data = src->initiator.data;

  dst->reflector.data_size = src->reflector.data_size;
  dst->reflector.data = src->reflector.data;

  return SL_STATUS_OK;
}

/******************************************************************************
 * Result callback registered with rtl_service. Routes the rtl_service
 * outcome (success / in-progress / error) to the callbacks.
 *
 * @param[in] rtl_inst       rtl_service instance
 * @param[in] result         rtl_service result handle. Valid only for the
 *                           duration of this callback. Forwarded to
 *                           report_result() / report_intermediate_result().
 * @param[in] process_status rtl_service processing status.
 * @param[in] user_data      User pointer registered with rtl_service.
 *****************************************************************************/
static void cs_algo_rtl_service_result_cb(sl_rtl_service_cs_inst_t *rtl_inst,
                                          const sl_rtl_service_cs_result_t *result,
                                          enum sl_rtl_error_code process_status,
                                          void *user_data)
{
  (void)user_data;
  sl_status_t sc;

  sc = app_rta_acquire(app_rta_ctx);
  if (sc == SL_STATUS_OK) {
    cs_algo_instance_t *inst = cs_algo_find_by_rtl_inst(rtl_inst);
    if (inst == NULL) {
      (void)app_rta_release(app_rta_ctx);
      return;
    }

    show_rtl_api_call_result(inst, process_status);

    switch (process_status) {
      case SL_RTL_ERROR_SUCCESS:
        report_result(inst, result, inst->ranging_counter, &inst->ranging_data);
        break;

      case SL_RTL_ERROR_ESTIMATION_IN_PROGRESS:
        report_intermediate_result(inst, result);
        break;

      default:
        break;
    }

    (void)cs_rreq_set_process_finished(inst->conn_handle, inst->ranging_counter);
    (void)app_rta_release(app_rta_ctx);
  } else {
    on_app_rta_error(APP_RTA_ERROR_ACQUIRE_FAILED, sc);
  }
}

/******************************************************************************
 * App RTA error handler.
 *****************************************************************************/
static void on_app_rta_error(app_rta_error_t error, sl_status_t result)
{
  (void)error;
  if (callbacks.on_error != NULL) {
    callbacks.on_error(0,
                       0,
                       CS_ALGO_ERROR_RUNTIME_FAILED,
                       result);
  }
}

// -----------------------------------------------------------------------------
// Internal function definitions

/******************************************************************************
 * Configure a freshly created rtl_service instance: set algo mode, CS mode,
 * CS params, optionally enable logging, and create the estimator.
 *
 * @param[in] conn_handle       Connection handle.
 * @param[in] rtl_inst          rtl_service instance to configure.
 * @param[in] config            cs_algo configuration whose fields drive the
 *                              rtl_service setup.
 * @param[in] rtl_cs_parameters Pre-built sl_rtl_cs_params struct
 * @return SL_RTL_ERROR_SUCCESS on success, otherwise the first rtl_service
 *         error encountered.
 *****************************************************************************/
static enum sl_rtl_error_code rtl_service_configure_instance(const uint8_t conn_handle,
                                                             sl_rtl_service_cs_inst_t *rtl_inst,
                                                             cs_algo_config_t *config,
                                                             sl_rtl_cs_params *rtl_cs_parameters)
{
  enum sl_rtl_error_code rtl_err;

  cs_algo_instance_t *inst = cs_algo_find_by_rtl_inst(rtl_inst);

  if (config->rtl_config.rtl_logging_enabled) {
    rtl_err = sl_rtl_service_enable_cs_log(rtl_inst);
    if (rtl_err != SL_RTL_ERROR_SUCCESS) {
      algo_log_error(INSTANCE_PREFIX "RTL - failed to enable log! "
                                     "[E: 0x%x]" LOG_NL,
                     conn_handle,
                     rtl_err);
      algo_error(inst,
                 CS_ALGO_ERROR_CONFIGURE_FAILED,
                 (sl_status_t)rtl_err);
      return rtl_err;
    }
  }

  rtl_err = sl_rtl_service_set_cs_algo_mode(rtl_inst, config->rtl_config.algo_mode);
  if (rtl_err != SL_RTL_ERROR_SUCCESS) {
    algo_log_error(INSTANCE_PREFIX "RTL - set algo mode failed! "
                                   "[E: 0x%x]" LOG_NL,
                   conn_handle,
                   rtl_err);
    algo_error(inst,
               CS_ALGO_ERROR_CONFIGURE_FAILED,
               (sl_status_t)rtl_err);
    return rtl_err;
  }

  sl_rtl_cs_mode rtl_main_mode;
  sl_rtl_cs_mode rtl_sub_mode;
  char *main_mode_str;
  char *sub_mode_str;

  cs_mode_converter(config->cs_main_mode, &main_mode_str, &rtl_main_mode);
  cs_mode_converter(config->cs_sub_mode, &sub_mode_str, &rtl_sub_mode);

  algo_log_debug(INSTANCE_PREFIX "CS mode: %s, Submode: %s" LOG_NL,
                 conn_handle,
                 main_mode_str,
                 sub_mode_str);
  rtl_err = sl_rtl_service_set_cs_mode(rtl_inst, rtl_main_mode, rtl_sub_mode);
  if (rtl_err != SL_RTL_ERROR_SUCCESS) {
    algo_log_error(INSTANCE_PREFIX "RTL - failed to set CS mode and sub mode! "
                                   "[E: 0x%x]" LOG_NL,
                   conn_handle,
                   rtl_err);
    algo_error(inst,
               CS_ALGO_ERROR_CONFIGURE_FAILED,
               (sl_status_t)rtl_err);
    return rtl_err;
  }
  rtl_err = sl_rtl_service_set_cs_params(rtl_inst, rtl_cs_parameters);
  if (rtl_err != SL_RTL_ERROR_SUCCESS) {
    algo_log_error(INSTANCE_PREFIX "RTL - failed to set CS parameters! "
                                   "[E: 0x%x]" LOG_NL,
                   conn_handle,
                   rtl_err);
    algo_error(inst,
               CS_ALGO_ERROR_CONFIGURE_FAILED,
               (sl_status_t)rtl_err);
    return rtl_err;
  }

  rtl_err = sl_rtl_service_create_cs_estimator(rtl_inst);
  if (rtl_err != SL_RTL_ERROR_SUCCESS) {
    algo_log_error(INSTANCE_PREFIX "RTL - failed to create estimator! [E: 0x%x]" LOG_NL,
                   conn_handle,
                   rtl_err);
    algo_error(inst,
               CS_ALGO_ERROR_CONFIGURE_FAILED,
               (sl_status_t)rtl_err);
    return rtl_err;
  }
  (void)conn_handle;
  return rtl_err;
}

// Initialize the cs_algo component.
void cs_algo_init(void)
{
  memset(algo_instances, 0, sizeof(algo_instances));
  memset(&callbacks, 0, sizeof(callbacks));

  // Create App RTA context
  sl_status_t sc;
  app_rta_config_t config = {
    .requirement.runtime = false,
    .requirement.guard   = true,
    .requirement.signal  = false,
    .step                = NULL,
    .priority            = 0,
    .stack_size          = 0,
    .error               = on_app_rta_error,
    .wait_for_guard      = CS_ALGO_CONFIG_CONFIG_WAIT
  };
  sc = app_rta_create_context(&config, &app_rta_ctx);
  if (sc != SL_STATUS_OK) {
    on_app_rta_error(APP_RTA_ERROR_RUNTIME_INIT_FAILED, sc);
  }
}

// Finalize initializing the cs_algo component.
void cs_algo_ready(void)
{
  sl_rtl_service_cs_config_t svc_cfg = {
    .result_cb = cs_algo_rtl_service_result_cb,
    .user_data = NULL,
  };
  enum sl_rtl_error_code rtl_err = sl_rtl_service_init_cs(&svc_cfg, &algo_rtl_svc_ctx);
  if (rtl_err != SL_RTL_ERROR_SUCCESS) {
    algo_log_error("RTL service init failed! [E: 0x%x]" LOG_NL, rtl_err);
    algo_rtl_svc_ctx = NULL;
  }
}

// -----------------------------------------------------------------------------
// Public function definitions

// Register callback functions for Algo events.
sl_status_t cs_algo_set_event_callbacks(cs_algo_event_callback_t cb)
{
  sl_status_t sc;
  sc = app_rta_acquire(app_rta_ctx);
  if (sc == SL_STATUS_OK) {
    if (cb.on_result != NULL) {
      if (callbacks.on_result != NULL) {
        (void)app_rta_release(app_rta_ctx);
        return SL_STATUS_ALREADY_INITIALIZED;
      }
      callbacks.on_result = cb.on_result;
    }

    if (cb.on_intermediate_result != NULL) {
      if (callbacks.on_intermediate_result != NULL) {
        (void)app_rta_release(app_rta_ctx);
        return SL_STATUS_ALREADY_INITIALIZED;
      }
      callbacks.on_intermediate_result = cb.on_intermediate_result;
    }

    if (cb.on_error != NULL) {
      if (callbacks.on_error != NULL) {
        (void)app_rta_release(app_rta_ctx);
        return SL_STATUS_ALREADY_INITIALIZED;
      }
      callbacks.on_error = cb.on_error;
    }

    (void)app_rta_release(app_rta_ctx);
  } else {
    on_app_rta_error(APP_RTA_ERROR_ACQUIRE_FAILED, sc);
  }

  return SL_STATUS_OK;
}

sl_status_t cs_algo_create(uint8_t conn_handle, cs_algo_config_t config)
{
  sl_status_t sc;
  sc = app_rta_acquire(app_rta_ctx);
  if (sc == SL_STATUS_OK) {
    if (conn_handle == SL_BT_INVALID_CONNECTION_HANDLE) {
      (void)app_rta_release(app_rta_ctx);
      return SL_STATUS_INVALID_HANDLE;
    }
    if (algo_rtl_svc_ctx == NULL) {
      algo_log_error(INSTANCE_PREFIX "RTL service context not initialized!" LOG_NL,
                     conn_handle);
      algo_error(NULL,
                 CS_ALGO_ERROR_CREATE_FAILED,
                 SL_STATUS_NOT_INITIALIZED);
      (void)app_rta_release(app_rta_ctx);
      return SL_STATUS_NOT_INITIALIZED;
    }

    cs_algo_instance_t *inst = cs_algo_get_instance(conn_handle);
    if (inst != NULL) {
      algo_log_info(INSTANCE_PREFIX "Instance already exists, reconfiguring" LOG_NL,
                    conn_handle);
      cs_algo_remove(conn_handle);
    }

    inst = cs_algo_get_free_slot();
    if (inst == NULL) {
      algo_log_error(INSTANCE_PREFIX "No free algo slots!" LOG_NL, conn_handle);
      algo_error(NULL, CS_ALGO_ERROR_CREATE_FAILED, SL_STATUS_FULL);
      (void)app_rta_release(app_rta_ctx);
      return SL_STATUS_FULL;
    }

    memset(inst, 0, sizeof(cs_algo_instance_t));
    inst->in_use            = true;
    inst->conn_handle       = conn_handle;
    inst->estimator_created = false;
    inst->config            = config;

    enum sl_rtl_error_code rtl_err = sl_rtl_service_create_cs_instance(algo_rtl_svc_ctx,
                                                                       &inst->rtl_inst);
    if (rtl_err != SL_RTL_ERROR_SUCCESS) {
      algo_log_error(INSTANCE_PREFIX "RTL service - create instance failed! [E: 0x%x]" LOG_NL,
                     conn_handle,
                     rtl_err);
      algo_error(inst,
                 CS_ALGO_ERROR_CREATE_FAILED,
                 (sl_status_t)rtl_err);
      inst->in_use = false;
      inst->rtl_inst = NULL;
      (void)app_rta_release(app_rta_ctx);
      return SL_STATUS_FAIL;
    }

    // Translate the rtllib-independent cs_algo_config_t into an sl_rtl_cs_params
    sl_rtl_cs_params rtl_cs_parameters;
    build_rtl_cs_params(&inst->config, &rtl_cs_parameters);

    rtl_err = rtl_service_configure_instance(conn_handle,
                                             inst->rtl_inst,
                                             &inst->config,
                                             &rtl_cs_parameters);
    if (rtl_err != SL_RTL_ERROR_SUCCESS) {
      algo_log_error(INSTANCE_PREFIX "RTL - configure instance failed! [E: 0x%x]" LOG_NL,
                     conn_handle, rtl_err);
      (void)sl_rtl_service_destroy_cs_instance(inst->rtl_inst);
      inst->rtl_inst = NULL;
      inst->in_use = false;
      (void)app_rta_release(app_rta_ctx);
      return SL_STATUS_FAIL;
    }
    inst->estimator_created = true;

    algo_log_info(INSTANCE_PREFIX "configured - "
                                  "algo_mode=%u ant:%u conn_int:%u mode:%u sub:%u" LOG_NL,
                  conn_handle,
                  inst->config.rtl_config.algo_mode,
                  inst->config.num_antenna_paths,
                  inst->config.connection_interval,
                  inst->config.cs_main_mode,
                  inst->config.cs_sub_mode);
    (void)app_rta_release(app_rta_ctx);
  } else {
    on_app_rta_error(APP_RTA_ERROR_ACQUIRE_FAILED, sc);
  }

  return SL_STATUS_OK;
}

sl_status_t cs_algo_remove(uint8_t conn_handle)
{
  sl_status_t sc;
  sc = app_rta_acquire(app_rta_ctx);
  if (sc == SL_STATUS_OK) {
    cs_algo_instance_t *inst = cs_algo_get_instance(conn_handle);
    if (inst == NULL) {
      (void)app_rta_release(app_rta_ctx);
      return SL_STATUS_NOT_FOUND;
    }

    if (inst->rtl_inst != NULL) {
      enum sl_rtl_error_code rtl_err = sl_rtl_service_destroy_cs_instance(inst->rtl_inst);
      if (rtl_err != SL_RTL_ERROR_SUCCESS) {
        algo_log_error(INSTANCE_PREFIX "RTL service - destroy instance failed! [E: 0x%x]" LOG_NL,
                       conn_handle, rtl_err);
        algo_error(inst,
                   CS_ALGO_ERROR_REMOVE_FAILED,
                   (sl_status_t)rtl_err);
      }
      inst->rtl_inst = NULL;
    }

    inst->in_use            = false;
    inst->estimator_created = false;
    algo_log_info(INSTANCE_PREFIX "Connection deinitialized" LOG_NL, conn_handle);

    (void)app_rta_release(app_rta_ctx);
  } else {
    on_app_rta_error(APP_RTA_ERROR_ACQUIRE_FAILED, sc);
  }

  return SL_STATUS_OK;
}

void cs_algo_process_ras_data(uint8_t conn_handle,
                              uint16_t ranging_counter,
                              cs_rreq_procedure_info_t proc_info,
                              cs_rreq_result_t *ranging_data)
{
  if (ranging_data == NULL) {
    algo_log_error(INSTANCE_PREFIX "Null RAS data!" LOG_NL, conn_handle);
    algo_error(NULL, CS_ALGO_ERROR_INVALID_RAS_DATA, SL_STATUS_NULL_POINTER);
    (void)cs_rreq_set_process_finished(conn_handle, ranging_counter);
    return;
  }

  cs_algo_instance_t *inst = cs_algo_get_instance(conn_handle);
  if (inst == NULL) {
    algo_log_error(INSTANCE_PREFIX "No algo instance for connection!" LOG_NL,
                   conn_handle);
    algo_error(NULL, CS_ALGO_ERROR_INVALID_RAS_DATA, SL_STATUS_NOT_FOUND);
    (void)cs_rreq_set_process_finished(conn_handle, ranging_counter);
    return;
  }

  if (!inst->estimator_created) {
    algo_log_error(INSTANCE_PREFIX "Estimator not created - "
                                   "cs_algo_create() must run first!" LOG_NL,
                   conn_handle);
    algo_error(inst,
               CS_ALGO_ERROR_INVALID_RAS_DATA,
               SL_STATUS_NOT_INITIALIZED);
    (void)cs_rreq_set_process_finished(conn_handle, ranging_counter);
    return;
  }

  // Alias the caller's ranging data into the algo instance (zero-copy). The
  // RREQ buffer remains owned by cs_rreq and stays valid until we call
  // cs_rreq_set_process_finished(), which we only do once the RTL result
  // callback has consumed the data (see cs_algo_rtl_service_result_cb()) or
  // when we bail out below before submitting to rtl_service.
  sl_status_t sc = cs_algo_save_result(ranging_data, inst);
  if (sc != SL_STATUS_OK) {
    algo_log_error(INSTANCE_PREFIX "Failed to save ranging data! [sc: 0x%lx]" LOG_NL,
                   conn_handle,
                   sc);
    (void)cs_rreq_set_process_finished(conn_handle, ranging_counter);
    return;
  }

  inst->ranging_counter = ranging_counter;

  inst->initiator_meas.ranging_data_body
    = (sl_rtl_ras_ranging_data_body *)ranging_data->initiator.data;
  inst->initiator_meas.ranging_data_body_len = ranging_data->initiator.data_size;

  inst->reflector_meas.ranging_data_body
    = (sl_rtl_ras_ranging_data_body *)ranging_data->reflector.data;
  inst->reflector_meas.ranging_data_body_len = ranging_data->reflector.data_size;

  inst->procedure_data.cs_procedure_config.subevent_len        = proc_info.subevent_len;
  inst->procedure_data.cs_procedure_config.subevent_interval   = proc_info.subevent_interval;
  inst->procedure_data.cs_procedure_config.event_interval      = proc_info.event_interval;
  inst->procedure_data.cs_procedure_config.procedure_interval  = proc_info.procedure_interval;
  inst->procedure_data.cs_procedure_config.procedure_count     = proc_info.procedure_count;
  inst->procedure_data.cs_procedure_config.subevents_per_event = proc_info.subevents_per_event;

  inst->procedure_data.ras_info.num_antenna_paths  = inst->config.num_antenna_paths;
  inst->procedure_data.ras_info.num_steps_reported = ranging_data->num_steps;
  inst->procedure_data.ras_info.step_channels      = ranging_data->step_channels;
  inst->procedure_data.initiator_measurement_type  = SL_RTL_RAS;
  inst->procedure_data.initiator_ras_measurement   = &inst->initiator_meas;
  inst->procedure_data.reflector_measurement_type  = SL_RTL_RAS;
  inst->procedure_data.reflector_ras_measurement   = &inst->reflector_meas;

  algo_log_info(INSTANCE_PREFIX "RAS process start - "
                                "ant:%u steps:%u init_data_size:%lu refl_data_size:%lu" LOG_NL,
                inst->conn_handle,
                inst->config.num_antenna_paths,
                ranging_data->num_steps,
                (unsigned long)ranging_data->initiator.data_size,
                (unsigned long)ranging_data->reflector.data_size);

  // Submit to rtl_service, the result is dispatched via
  // cs_algo_rtl_service_result_cb, which fires synchronously (bare-metal)
  // or asynchronously (RTOS)
  #if !defined(CS_ALGO_CONFIG_SKIP_RTL_PROCESS) || (CS_ALGO_CONFIG_SKIP_RTL_PROCESS == 0)
  enum sl_rtl_error_code rtl_err = sl_rtl_service_process_ras(inst->rtl_inst,
                                                              &inst->procedure_data);
  algo_log_info(INSTANCE_PREFIX "RTL service RAS submit done [E: 0x%x]" LOG_NL,
                inst->conn_handle, rtl_err);
  show_rtl_api_call_result(inst, rtl_err);
  if ((rtl_err != SL_RTL_ERROR_SUCCESS)
      && (rtl_err != SL_RTL_ERROR_ESTIMATION_IN_PROGRESS)) {
    // Submit failed and the result callback will not fire, release the cs_rreq buffer
    (void)cs_rreq_set_process_finished(inst->conn_handle, inst->ranging_counter);
  }
  #else
  algo_log_debug(INSTANCE_PREFIX "RTL process skipped" LOG_NL, inst->conn_handle);
  (void)cs_rreq_set_process_finished(inst->conn_handle, inst->ranging_counter);
  #endif // !defined(CS_ALGO_CONFIG_SKIP_RTL_PROCESS) || (CS_ALGO_CONFIG_SKIP_RTL_PROCESS == 0)
}
