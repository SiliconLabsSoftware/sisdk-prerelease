/***************************************************************************//**
 * @file
 * @brief CS Algo - estimation implementation
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

// -----------------------------------------------------------------------------
// Includes

#include "cs_result.h"
#include "cs_algo_log.h"
#include "cs_algo.h"

// -----------------------------------------------------------------------------
// Internal types

// TODO(FreeRTOS): add a unified_ranging_data_t/sl_rtl_ras_procedure field
// here and memcpy the initiator's buffer into it on dispatch (the algo 
// task shouldn't race the initiator reusing its own buffer for the next procedure)
typedef struct {
  uint8_t                    result[CS_RESULT_MAX_BUFFER_SIZE];
  uint8_t                    conn_handle;
  uint8_t                    instance_id;
  bool                       in_use;
  bool                       estimator_created;
  sl_rtl_cs_libitem          rtl_handle;
  cs_algo_config_t           config;
  cs_result_session_data_t   result_data;
} cs_algo_instance_t;

// -----------------------------------------------------------------------------
// Static function declarations
static void show_rtl_api_call_result(cs_algo_instance_t *inst,
                                     enum sl_rtl_error_code err_code);
static void report_result(cs_algo_instance_t *inst,
                          uint16_t ranging_counter,
                          unified_ranging_data_t *ranging_data);
static void report_intermediate_result(cs_algo_instance_t *inst);
static void cs_mode_converter(uint8_t cs_mode_bt,
                              char **cs_mode_str,
                              sl_rtl_cs_mode *cs_mode_rtl);
static void build_rtl_cs_params(const cs_algo_config_t *src, 
                                sl_rtl_cs_params *dst);
static cs_algo_instance_t *cs_algo_get_instance(uint8_t conn_handle);
static cs_algo_instance_t *cs_algo_get_free_slot(void);

// -----------------------------------------------------------------------------
// Static variables

static cs_algo_instance_t algo_instances[CS_ALGO_ESTIMATOR_COUNT];
static cs_algo_app_cb_t algo_app_cb;
static cs_algo_initiator_cb_t algo_initiator_cb;

// -----------------------------------------------------------------------------
// Static function definitions

/******************************************************************************
 * Show error messages based on RTL API call error codes.
 *
 * @param[in] inst cs_algo instance.
 * @param[in] err_code RTL API error code.
 *****************************************************************************/
static void show_rtl_api_call_result(cs_algo_instance_t *inst,
                                     enum sl_rtl_error_code err_code)
{
  switch (err_code) {
    case SL_RTL_ERROR_SUCCESS:
      break;
    case SL_RTL_ERROR_ARGUMENT:
      algo_log_error(INSTANCE_PREFIX "RTL - invalid argument! [E: 0x%x]" LOG_NL,
                     inst->conn_handle,
                     err_code);
      break;

    case SL_RTL_ERROR_OUT_OF_MEMORY:
      algo_log_error(INSTANCE_PREFIX "RTL - memory allocation error! [E: 0x%x]" LOG_NL,
                     inst->conn_handle,
                     err_code);
      break;

    case SL_RTL_ERROR_ESTIMATION_IN_PROGRESS:
      algo_log_error(INSTANCE_PREFIX "RTL - estimation not yet finished! [E: 0x%x]" LOG_NL,
                    inst->conn_handle,
                    err_code);
      break;

    case SL_RTL_ERROR_NUMBER_OF_SNAPHOTS_DO_NOT_MATCH:
      algo_log_error(INSTANCE_PREFIX "RTL - initialized and calculated "
                                          "snapshots do not match! [E: 0x%x]" LOG_NL,
                     inst->conn_handle,
                     err_code);
      break;

    case SL_RTL_ERROR_ESTIMATOR_NOT_CREATED:
      algo_log_error(INSTANCE_PREFIX "RTL - estimator not created! [E: 0x%x]" LOG_NL,
                     inst->conn_handle,
                     err_code);
      break;

    case SL_RTL_ERROR_ESTIMATOR_ALREADY_CREATED:
      algo_log_error(INSTANCE_PREFIX "RTL - estimator already created! [E: 0x%x]" LOG_NL,
                     inst->conn_handle,
                     err_code);
      break;

    case SL_RTL_ERROR_NOT_INITIALIZED:
      algo_log_error(INSTANCE_PREFIX "RTL - library item not initialized! [E: 0x%x]" LOG_NL,
                     inst->conn_handle,
                     err_code);
      break;

    case SL_RTL_ERROR_INTERNAL:
      algo_log_error(INSTANCE_PREFIX "RTL - internal error! [E: 0x%x]" LOG_NL,
                     inst->conn_handle,
                     err_code);
      break;

    case SL_RTL_ERROR_IQ_SAMPLE_QA:
      algo_log_error(INSTANCE_PREFIX "RTL - IQ sample quality analysis failed! [E: 0x%x]" LOG_NL,
                     inst->conn_handle,
                     err_code);
      break;

    case SL_RTL_ERROR_FEATURE_NOT_SUPPORTED:
      algo_log_error(INSTANCE_PREFIX "RTL - feature not supported! [E: 0x%x]" LOG_NL,
                     inst->conn_handle,
                     err_code);
      break;

    case SL_RTL_ERROR_INCORRECT_MEASUREMENT:
      algo_log_error(INSTANCE_PREFIX "RTL - incorrect measurement! Error of the last"
                                     " measurement was too large! [E: 0x%x]" LOG_NL,
                     inst->conn_handle,
                     err_code);
      break;

    case SL_RTL_ERROR_CS_CHANNEL_MAP_TOO_SPARSE:
      algo_log_error(INSTANCE_PREFIX "RTL - too many skipped channels "
                                     "in the proposed channel map! [E: 0x%x]" LOG_NL,
                     inst->conn_handle,
                     err_code);
      break;

    case SL_RTL_ERROR_CS_CHANNEL_MAP_TOO_FEW_CHANNELS:
      algo_log_error(INSTANCE_PREFIX "RTL - too few channels "
                                     "in the proposed channel map! [E: 0x%x]" LOG_NL,
                     inst->conn_handle,
                     err_code);
      break;

    case SL_RTL_ERROR_CS_CHANNEL_SPACING_TOO_LARGE:
      algo_log_error(INSTANCE_PREFIX "RTL - channel spacing is too large "
                                     "in the proposed channel map! [E: 0x%x]" LOG_NL,
                     inst->conn_handle,
                     err_code);
      break;

    case SL_RTL_ERROR_POOR_INPUT_DATA_QUALITY:
      algo_log_error(INSTANCE_PREFIX "RTL - input data quality is poor! [E: 0x%x]" LOG_NL,
                     inst->conn_handle,
                     err_code);
      break;

    default:
      algo_log_error(INSTANCE_PREFIX "RTL - unknown error! [E: 0x%x]" LOG_NL,
                     inst->conn_handle,
                     err_code);
      break;
  }
  (void)inst;
}

/******************************************************************************
 * Handle successful RTL process, and get distance.
 *
 * @param[in] inst cs_algo instance.
 * @param[in] ranging_counter procedure ranging counter.
 * @param[in] ranging_data pointer to the unified ranging data buffer.
 *****************************************************************************/
static void report_result(cs_algo_instance_t *inst,
                          uint16_t ranging_counter,
                          unified_ranging_data_t *ranging_data)
{
  sl_status_t sc = SL_STATUS_OK;
  bool estimation_valid = false;
  enum sl_rtl_error_code rtl_err = SL_RTL_ERROR_NOT_INITIALIZED;
  sl_rtl_cs_estimator_param param;
  sl_rtl_cs_distance_estimate_mode mode;

  float rtl_value = 0.0f;
  float last_known_distance = 0.0f;

  // initialize result data
  cs_result_initialize_results_data(&inst->result_data);

  if (inst->config.cs_sub_mode == sl_bt_cs_submode_disabled) {
    mode = SL_RTL_CS_BEST_ESTIMATE;
  } else {
    mode = SL_RTL_CS_MAIN_MODE_ESTIMATE;
  }

  // --------------------------------
  // Get distance
  rtl_err = sl_rtl_cs_get_distance_estimate(&inst->rtl_handle,
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
    } else {
      estimation_valid = true;
    }
  }

  if (inst->config.cs_sub_mode != sl_bt_cs_submode_disabled) {
    // Submode requested
    rtl_err = sl_rtl_cs_get_distance_estimate(&inst->rtl_handle,
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
      } else {
        estimation_valid = true;
      }
    }
  }

  // --------------------------------
  // Get RAW distance
  rtl_err = sl_rtl_cs_get_distance_estimate(&inst->rtl_handle,
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
    } else {
      estimation_valid = true;
    }
  }

  if (inst->config.cs_sub_mode != sl_bt_cs_submode_disabled) {
    rtl_err = sl_rtl_cs_get_distance_estimate(&inst->rtl_handle,
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
      } else {
        estimation_valid = true;
      }
    }
  }

  // --------------------------------
  // Get distance likeliness
  rtl_err = sl_rtl_cs_get_distance_estimate_confidence(&inst->rtl_handle,
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
    } else {
      estimation_valid = true;
    }
  }

  if (inst->config.cs_sub_mode != sl_bt_cs_submode_disabled) {
    rtl_err = sl_rtl_cs_get_distance_estimate_confidence(&inst->rtl_handle,
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
  rtl_err = sl_rtl_cs_set_estimator_param(&inst->rtl_handle, &param);
  show_rtl_api_call_result(inst, rtl_err);

  rtl_err = sl_rtl_cs_get_distance_estimate(&inst->rtl_handle,
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
    } else {
      estimation_valid = true;
      param.type = SL_RTL_LAST_KNOWN_DISTANCE;
      param.value.last_known_distance = last_known_distance;
      rtl_err = sl_rtl_cs_set_estimator_param(&inst->rtl_handle, &param);
      show_rtl_api_call_result(inst, rtl_err);
    }
  }

  // --------------------------------
  // Get velocity
  if (inst->config.rtl_config.algo_mode == SL_RTL_CS_ALGO_MODE_REAL_TIME_FAST
      && inst->config.cs_main_mode == sl_bt_cs_mode_pbr
      && (inst->config.channel_map_preset == CS_CHANNEL_MAP_PRESET_HIGH
          || inst->config.channel_map_preset == CS_CHANNEL_MAP_PRESET_MEDIUM)) {
    rtl_err = sl_rtl_cs_get_distance_estimate(&inst->rtl_handle,
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
      } else {
        estimation_valid = true;
      }
    }
  }

  // --------------------------------
  // Get bit error rate - RTT only
  if (inst->config.cs_main_mode == sl_bt_cs_mode_rtt) {
    rtl_err = sl_rtl_cs_get_distance_estimate_confidence(&inst->rtl_handle,
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
      } else {
        estimation_valid = true;
      }
    }
  }

  if (estimation_valid) {
    // Get ranging data so the app can consume it
    cs_ranging_data_t cs_ranging_data = {
      .num_steps     = ranging_data->num_steps,
      .step_channels = ranging_data->step_channels,
      .initiator = {
        .ranging_data_size = ranging_data->initiator.ranging_data_size,
        .ranging_data      = ranging_data->initiator.ranging_data,
      },
      .reflector = {
        .ranging_data_size = ranging_data->reflector.ranging_data_size,
        .ranging_data      = ranging_data->reflector.ranging_data,
      },
    };
    if (algo_app_cb.on_result != NULL) {
      algo_app_cb.on_result(inst->conn_handle,
                                  ranging_counter,
                                  inst->result,
                                  (uint16_t)inst->result_data.size,
                                  &cs_ranging_data);
    }
    if (algo_app_cb.on_extended_result != NULL) {
      algo_app_cb.on_extended_result(inst->conn_handle,
                                          ranging_counter,
                                          inst->result,
                                          (uint16_t)inst->result_data.size,
                                          &cs_ranging_data);
    }
  }

  if (algo_initiator_cb.on_process_finished!= NULL) {
    algo_initiator_cb.on_process_finished(inst->conn_handle, ranging_counter, SL_STATUS_OK);
  }
}

/******************************************************************************
 * Handle progressive RTL process, and get intermediate result.
 *
 * @param[in] inst cs_algo instance.
 *****************************************************************************/
static void report_intermediate_result(cs_algo_instance_t *inst)
{
  enum sl_rtl_error_code rtl_err = SL_RTL_ERROR_NOT_INITIALIZED;
  float progress_percentage = 0.0f;

  rtl_err =
    sl_rtl_cs_get_distance_estimate_extended_info(&inst->rtl_handle,
                                                  SL_RTL_CS_DISTANCE_ESTIMATE_EXTENDED_INFO_TYPE_PROGRESS_PERCENTAGE,
                                                  &progress_percentage);
  show_rtl_api_call_result(inst, rtl_err);
  if (rtl_err == SL_RTL_ERROR_SUCCESS
      && algo_app_cb.on_intermediate_result != NULL) {
    cs_intermediate_result_t intermediate_result;
    intermediate_result.connection          = inst->conn_handle;
    intermediate_result.progress_percentage = progress_percentage;
    algo_app_cb.on_intermediate_result(&intermediate_result);
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
  for (uint8_t i = 0; i < CS_ALGO_ESTIMATOR_COUNT; i++) {
    if (algo_instances[i].in_use
        && algo_instances[i].conn_handle == conn_handle) {
      return &algo_instances[i];
    }
  }
  return NULL;
}

static cs_algo_instance_t *cs_algo_get_free_slot(void)
{
  for (uint8_t i = 0; i < CS_ALGO_ESTIMATOR_COUNT; i++) {
    if (!algo_instances[i].in_use) {
      return &algo_instances[i];
    }
  }
  return NULL;
}

// -----------------------------------------------------------------------------
// Internal function definitions

/******************************************************************************
 * RTL Lib init function
 *****************************************************************************/
static enum sl_rtl_error_code rtl_library_init(const uint8_t      conn_handle,
                                               sl_rtl_cs_libitem  *handle,
                                               cs_algo_config_t   *config,
                                               uint8_t            *instance_id)
{
  enum sl_rtl_error_code rtl_err;

  if (handle != NULL && *handle != NULL) {
    rtl_err = sl_rtl_cs_deinit(handle);
    if (rtl_err != SL_RTL_ERROR_SUCCESS) {
      algo_log_error(INSTANCE_PREFIX "RTL - failed to deinit lib! "
                                     "[E: 0x%x]" LOG_NL,
                     conn_handle,
                     rtl_err);
      return rtl_err;
    }
    *handle = NULL;
  }

  rtl_err = sl_rtl_cs_init(handle);
  if (rtl_err != SL_RTL_ERROR_SUCCESS) {
    algo_log_error(INSTANCE_PREFIX "RTL - failed to init lib! "
                                   "[E: 0x%x]" LOG_NL,
                   conn_handle,
                   rtl_err);
    return rtl_err;
  }

  rtl_err = sl_rtl_cs_log_get_instance_id(handle, instance_id);
  if (rtl_err != SL_RTL_ERROR_SUCCESS) {
    algo_log_error(INSTANCE_PREFIX "RTL - failed to get instance id! "
                                   "[E: 0x%x]" LOG_NL,
                   conn_handle,
                   rtl_err);
    return rtl_err;
  }

  if (config->rtl_config.rtl_logging_enabled) {
    rtl_err = sl_rtl_cs_log_enable(handle);
    if (rtl_err != SL_RTL_ERROR_SUCCESS) {
      algo_log_error(INSTANCE_PREFIX "RTL - failed to enable log! "
                                     "[E: 0x%x]" LOG_NL,
                     conn_handle,
                     rtl_err);
      return rtl_err;
    }
  }
  (void)conn_handle;
  return rtl_err;
}

static enum sl_rtl_error_code rtl_library_create_estimator(const uint8_t      conn_handle,
                                                           sl_rtl_cs_libitem  *handle,
                                                           const uint8_t      algo_mode,
                                                           const uint8_t      cs_main_mode,
                                                           const uint8_t      cs_sub_mode,
                                                           sl_rtl_cs_params   *rtl_cs_parameters)
{
  enum sl_rtl_error_code rtl_err;

  rtl_err = sl_rtl_cs_set_algo_mode(handle, algo_mode);
  if (rtl_err != SL_RTL_ERROR_SUCCESS) {
    algo_log_error(INSTANCE_PREFIX "RTL - set algo mode failed! "
                                   "[E: 0x%x]" LOG_NL,
                   conn_handle,
                   rtl_err);
    return rtl_err;
  }
  sl_rtl_cs_mode rtl_main_mode;
  sl_rtl_cs_mode rtl_sub_mode;
  char *main_mode_str;
  char *sub_mode_str;

  cs_mode_converter(cs_main_mode, &main_mode_str, &rtl_main_mode);
  cs_mode_converter(cs_sub_mode, &sub_mode_str, &rtl_sub_mode);

  algo_log_debug(INSTANCE_PREFIX "CS mode: %s, Submode: %s" LOG_NL,
                 conn_handle,
                 main_mode_str,
                 sub_mode_str);
  rtl_err = sl_rtl_cs_set_cs_mode(handle, rtl_main_mode, rtl_sub_mode);
  if (rtl_err != SL_RTL_ERROR_SUCCESS) {
    algo_log_error(INSTANCE_PREFIX "RTL - failed to set CS mode and sub mode! "
                                   "[E: 0x%x]" LOG_NL,
                   conn_handle,
                   rtl_err);
    return rtl_err;
  }
  rtl_err = sl_rtl_cs_set_cs_params(handle, rtl_cs_parameters);
  if (rtl_err != SL_RTL_ERROR_SUCCESS) {
    algo_log_error(INSTANCE_PREFIX "RTL - failed to set CS parameters! "
                                   "[E: 0x%x]" LOG_NL,
                   conn_handle,
                   rtl_err);
    return rtl_err;
  }

  rtl_err = sl_rtl_cs_create_estimator(handle);
  if (rtl_err != SL_RTL_ERROR_SUCCESS) {
    algo_log_error(INSTANCE_PREFIX "RTL - failed to create estimator! [E: 0x%x]" LOG_NL,
                   conn_handle,
                   rtl_err);
    return rtl_err;
  }
  (void)conn_handle;
  return rtl_err;
}

// -----------------------------------------------------------------------------
// Public function definitions

void cs_algo_init(void)
{
  memset(algo_instances, 0, sizeof(algo_instances));
  memset(&algo_app_cb, 0, sizeof(algo_app_cb));
  memset(&algo_initiator_cb, 0, sizeof(algo_initiator_cb));
}

sl_status_t cs_algo_initiator_set_callback(cs_algo_initiator_cb_t *cb)
{
  if (cb == NULL) {
    return SL_STATUS_NULL_POINTER;
  }
  memcpy(&algo_initiator_cb, cb, sizeof(cs_algo_initiator_cb_t));
  return SL_STATUS_OK;
}

sl_status_t cs_algo_app_set_callback(cs_algo_app_cb_t *cb)
{
  if (cb == NULL) {
    return SL_STATUS_NULL_POINTER;
  }
  memcpy(&algo_app_cb, cb, sizeof(cs_algo_app_cb_t));
  return SL_STATUS_OK;
}

sl_status_t cs_algo_create(uint8_t conn_handle, cs_algo_config_t config)
{
  if (conn_handle == SL_BT_INVALID_CONNECTION_HANDLE) {
    return SL_STATUS_INVALID_HANDLE;
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
    return SL_STATUS_FULL;
  }

  memset(inst, 0, sizeof(cs_algo_instance_t));
  inst->in_use            = true;
  inst->conn_handle       = conn_handle;
  inst->estimator_created = false;
  inst->config            = config;

  // Translate the rtllib-independent cs_algo_config_t into an sl_rtl_cs_params
  sl_rtl_cs_params rtl_cs_parameters;
  build_rtl_cs_params(&inst->config, &rtl_cs_parameters);

  enum sl_rtl_error_code rtl_err = rtl_library_init(conn_handle,
                                                    &inst->rtl_handle,
                                                    &inst->config,
                                                    &inst->instance_id);
  if (rtl_err != SL_RTL_ERROR_SUCCESS) {
    algo_log_error(INSTANCE_PREFIX "RTL - init failed! [E: 0x%x]" LOG_NL,
                        conn_handle, rtl_err);
    inst->in_use = false;
    return SL_STATUS_FAIL;
  }

  rtl_err = rtl_library_create_estimator(conn_handle,
                                         &inst->rtl_handle,
                                         inst->config.rtl_config.algo_mode,
                                         inst->config.cs_main_mode,
                                         inst->config.cs_sub_mode,
                                         &rtl_cs_parameters);
  if (rtl_err != SL_RTL_ERROR_SUCCESS) {
    algo_log_error(INSTANCE_PREFIX "RTL - create estimator failed! [E: 0x%x]" LOG_NL,
                        conn_handle, rtl_err);
    (void)sl_rtl_cs_deinit(&inst->rtl_handle);
    inst->in_use = false;
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

  return SL_STATUS_OK;
}

sl_status_t cs_algo_remove(uint8_t conn_handle)
{
  cs_algo_instance_t *inst = cs_algo_get_instance(conn_handle);
  if (inst == NULL) {
    return SL_STATUS_NOT_FOUND;
  }

  if (inst->rtl_handle != NULL) {
    enum sl_rtl_error_code rtl_err = sl_rtl_cs_deinit(&inst->rtl_handle);
    if (rtl_err != SL_RTL_ERROR_SUCCESS) {
      algo_log_error(INSTANCE_PREFIX "RTL - deinit failed! [E: 0x%x]" LOG_NL,
                     conn_handle, rtl_err);
    }
  }

  memset(inst, 0, sizeof(cs_algo_instance_t));
  algo_log_info(INSTANCE_PREFIX "Connection deinitialized" LOG_NL, conn_handle);
  return SL_STATUS_OK;
}

/******************************************************************************
 * Calculate distance between initiator and reflector using RTL library.
 *
 * @param[in] conn_handle connection handle.
 * @param[in] ranging_counter procedure ranging counter.
 * @param[in] proc_info pointer to the RAS data structure.
 *****************************************************************************/
void cs_algo_process_ras_data(uint8_t conn_handle,
                              uint16_t ranging_counter,
                              cs_algo_procedure_info_t proc_info,
                              unified_ranging_data_t *ranging_data)
{
  if (ranging_data == NULL) {
    algo_log_error(INSTANCE_PREFIX "Null RAS data!" LOG_NL, conn_handle);
    return;
  }

  cs_algo_instance_t *inst = cs_algo_get_instance(conn_handle);
  if (inst == NULL) {
    algo_log_error(INSTANCE_PREFIX "No algo instance for connection!" LOG_NL,
                        conn_handle);
    return;
  }

  if (!inst->estimator_created) {
    algo_log_error(INSTANCE_PREFIX "Estimator not created - "
                        "cs_algo_create() must run first!" LOG_NL,
                        conn_handle);
    if (algo_initiator_cb.on_process_finished != NULL) {
      algo_initiator_cb.on_process_finished(conn_handle, ranging_counter, SL_STATUS_NOT_READY);
    }
    return;
  }

  enum sl_rtl_error_code rtl_err;

  // Initiator: Measurement data
  sl_rtl_ras_measurement initiator_measurement = {
    .ranging_data_body
      = (sl_rtl_ras_ranging_data_body *)ranging_data->initiator.ranging_data,
    .ranging_data_body_len = ranging_data->initiator.ranging_data_size
  };

  // Reflector: Measurement data
  sl_rtl_ras_measurement reflector_measurement = {
    .ranging_data_body
      = (sl_rtl_ras_ranging_data_body *)ranging_data->reflector.ranging_data,
    .ranging_data_body_len = ranging_data->reflector.ranging_data_size
  };

  // Translate procedure_config fields in cs_algo_procedure_info_t into
  // the sl_rtl_cs_procedure_config expected by the RTL library
  sl_rtl_cs_procedure_config procedure_config = {
    .subevent_len        = proc_info.subevent_len,
    .subevent_interval   = proc_info.subevent_interval,
    .event_interval      = proc_info.event_interval,
    .procedure_interval  = proc_info.procedure_interval,
    .procedure_count     = proc_info.procedure_count,
    .subevents_per_event = proc_info.subevents_per_event,
  };

  // Create procedure data structure
  sl_rtl_ras_procedure procedure_data = {
    .cs_procedure_config = procedure_config,
    .ras_info = {
      .num_antenna_paths = proc_info.num_antenna_paths,
      .num_steps_reported = ranging_data->num_steps,
      .step_channels = ranging_data->step_channels
    },
    .initiator_measurement_type = SL_RTL_RAS,
    .initiator_ras_measurement = &initiator_measurement,
    .reflector_measurement_type = SL_RTL_RAS,
    .reflector_ras_measurement = &reflector_measurement,
  };

  algo_log_info(INSTANCE_PREFIX "RAS process start - "
                     "ant:%u steps:%u i_sz:%lu r_sz:%lu" LOG_NL,
                     inst->conn_handle,
                     proc_info.num_antenna_paths,
                     ranging_data->num_steps,
                     (unsigned long)ranging_data->initiator.ranging_data_size,
                     (unsigned long)ranging_data->reflector.ranging_data_size);

  // Start estimation
  // Note: procedure count is always 1.
  #if defined (CS_ALGO_SKIP_RTL_PROCESS) && (CS_ALGO_SKIP_RTL_PROCESS == 0)
  rtl_err = sl_rtl_ras_process(&inst->rtl_handle,
                               1,
                               &procedure_data);
  algo_log_info(INSTANCE_PREFIX "RTL RAS process done [E: 0x%x]" LOG_NL,
                     inst->conn_handle, rtl_err);
  show_rtl_api_call_result(inst, rtl_err);
  switch (rtl_err) {
    case SL_RTL_ERROR_SUCCESS:
      report_result(inst, ranging_counter, ranging_data);
      break;
    case SL_RTL_ERROR_ESTIMATION_IN_PROGRESS:
      report_intermediate_result(inst);
      break;
    default:
      if (algo_initiator_cb.on_process_finished != NULL) {
        algo_initiator_cb.on_process_finished(conn_handle, ranging_counter, (uint32_t)rtl_err);
      }
      break;
  }
  #else
  algo_log_debug(INSTANCE_PREFIX "RTL process skipped" LOG_NL, inst->conn_handle);
  if (algo_initiator_cb.on_process_finished != NULL) {
    algo_initiator_cb.on_process_finished(conn_handle, ranging_counter, SL_STATUS_OK);
  }
  #endif // CS_ALGO_SKIP_RTL_PROCESS
}

