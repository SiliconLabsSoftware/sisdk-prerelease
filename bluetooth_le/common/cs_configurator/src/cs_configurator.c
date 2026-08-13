/***************************************************************************//**
 * @file
 * @brief CS Configurator implementation
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

#include "sl_common.h"
#include "cs_configurator.h"
#include "cs_configurator_optimization_internal.h"
#include "cs_configurator_rtllib_internal.h"
#include "cs_common.h"

// -----------------------------------------------------------------------------
// Definitions

#define CS_CONN_INTERVAL_TIME_RESOLUTION_US   (1250UL)

// Defined by BT Spec
#define CS_CONFIGURATOR_MINIMUM_CONNECTION_INTERVAL (6U)

#define CS_CONFIGURATOR_OPTIMIZE_INITIAL_CONN_INTERVAL (0xFFFF)
#define CS_CONFIGURATOR_OPTIMIZE_INITIAL_PROC_INTERVAL (0xFFFF)

#define CS_CONFIGURATOR_OPTIMIZE_FREQUENCY_CONN_INTERVAL_LOWER_BOUND (6U)
#define CS_CONFIGURATOR_OPTIMIZE_FREQUENCY_CONN_INTERVAL_UPPER_BOUND (10U)
#define CS_CONFIGURATOR_OPTIMIZE_FREQUENCY_CONN_INTERVAL_MULTIPLE_PEERS_LOWER_BOUND (10U)
#define CS_CONFIGURATOR_OPTIMIZE_FREQUENCY_CONN_INTERVAL_MULTIPLE_PEERS_UPPER_BOUND (10U)
#define CS_CONFIGURATOR_OPTIMIZE_ENERGY_CONN_INTERVAL_LOWER_BOUND (15U)
#define CS_CONFIGURATOR_OPTIMIZE_ENERGY_CONN_INTERVAL_UPPER_BOUND (20U)
#define CS_CONFIGURATOR_OPTIMIZE_ENERGY_CONN_INTERVAL_MULTIPLE_PEERS_LOWER_BOUND (16U)
#define CS_CONFIGURATOR_OPTIMIZE_ENERGY_CONN_INTERVAL_MULTIPLE_PEERS_UPPER_BOUND (16U)

// -----------------------------------------------------------------------------
// Forward declaration of private functions

static uint16_t calc_min_proc_intv_for_conn_intv(uint32_t measurement_time_us,
                                                 uint32_t ras_data_time_us,
                                                 uint16_t conn_interval,
                                                 bool use_real_time_ras,
                                                 uint8_t peer_count);

static uint16_t calc_min_proc_intv_for_est_time(uint32_t estimation_time_us,
                                                uint16_t conn_interval,
                                                uint8_t peer_count);

static uint16_t calc_min_proc_interval(uint32_t estimation_time_us,
                                       uint32_t measurement_time_us,
                                       uint32_t ras_data_time_us,
                                       bool use_real_time_ras,
                                       uint16_t conn_interval,
                                       uint8_t peer_count);

static uint32_t calc_measurement_time_us(cs_channel_map_preset_t channel_map_preset,
                                         uint8_t num_antenna_paths);
static uint32_t calc_ras_data_time_us(cs_channel_map_preset_t channel_map_preset,
                                      uint8_t num_antenna_paths);

// -----------------------------------------------------------------------------
// Public function definitions

void cs_configurator_apply_channel_map_preset(cs_channel_map_preset_t preset,
                                              uint8_t *channel_map)
{
  switch (preset) {
    case CS_CHANNEL_MAP_PRESET_MEDIUM:
    {
      uint8_t channel_map_medium[10] = { 0x54, 0x55, 0x55, 0x54, 0x55, 0x55, 0x55, 0x55, 0x55, 0x15 };
      memcpy(channel_map, channel_map_medium, sizeof(channel_map_medium));
    }
    break;
    case CS_CHANNEL_MAP_PRESET_HIGH:
    {
      uint8_t channel_map_high[10] = { 0xFC, 0xFF, 0x7F, 0xFC, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x1F };
      memcpy(channel_map, channel_map_high, sizeof(channel_map_high));
    }
    break;
    case CS_CHANNEL_MAP_PRESET_CUSTOM:
    {
      // Do nothing
    }
    break;
    default:
      // No other values allowed
      break;
  }
}

sl_status_t cs_configurator_get_estimation_time_us(cs_configurator_parameters_t *input,
                                                   cs_algo_mode_t algo_mode,
                                                   cs_channel_map_preset_t channel_map_preset,
                                                   uint32_t clock_frequency_hz,
                                                   uint8_t num_antenna_paths,
                                                   uint32_t *estimation_time_us_out)
{
  return cs_configurator_rtllib_get_estimation_time_us(input,
                                                       algo_mode,
                                                       channel_map_preset,
                                                       clock_frequency_hz,
                                                       num_antenna_paths,
                                                       estimation_time_us_out);
}

sl_status_t cs_configurator_validate(cs_configurator_parameters_t *config,
                                     cs_channel_map_preset_t channel_map_preset,
                                     uint32_t estimation_time_us,
                                     uint8_t peer_count,
                                     uint8_t num_antenna_paths)
{
  if ((config == NULL)
      || (config->cs_instance_config == NULL)
      || (config->cs_procedure_parameters == NULL)
      || (config->connection_parameters == NULL)) {
    return SL_STATUS_NULL_POINTER;
  }

  if (peer_count == 0) {
    return SL_STATUS_INVALID_PARAMETER;
  }
  uint16_t min_procedure_interval = config->cs_procedure_parameters->min_procedure_interval;
  uint16_t max_procedure_interval = config->cs_procedure_parameters->max_procedure_interval;
  uint16_t min_connection_interval = config->connection_parameters->min_connection_interval;
  uint16_t max_connection_interval = config->connection_parameters->max_connection_interval;

  // Validate the connection interval
  if (min_connection_interval > max_connection_interval) {
    return SL_STATUS_INVALID_PARAMETER;
  }
  if (min_connection_interval < CS_CONFIGURATOR_MINIMUM_CONNECTION_INTERVAL) {
    return SL_STATUS_INVALID_PARAMETER;
  }

  // Validate the procedure interval
  if (min_procedure_interval > max_procedure_interval) {
    return SL_STATUS_INVALID_PARAMETER;
  }

  // Calculate estimation time and RAS data transfer time
  uint32_t measurement_time_us = calc_measurement_time_us(channel_map_preset,
                                                          num_antenna_paths);
  uint32_t ras_data_time_us = calc_ras_data_time_us(channel_map_preset,
                                                    num_antenna_paths);
  uint16_t min_proc_interval_for_config = calc_min_proc_interval(estimation_time_us,
                                                                 measurement_time_us,
                                                                 ras_data_time_us,
                                                                 config->rreq_config->real_time_mode,
                                                                 min_connection_interval,
                                                                 peer_count);
  if (min_proc_interval_for_config > min_procedure_interval) {
    return SL_STATUS_INVALID_PARAMETER;
  }

  if ((config->cs_procedure_parameters->max_procedure_len)
      > (config->cs_procedure_parameters->min_procedure_interval
         * min_connection_interval * 2)) {
    return SL_STATUS_INVALID_PARAMETER;
  }
  // Subevent length check
  if (config->cs_procedure_parameters->max_subevent_len
      > (config->cs_procedure_parameters->max_procedure_len * 625UL)) {
    return SL_STATUS_INVALID_PARAMETER;
  }

  return SL_STATUS_OK;
}

sl_status_t cs_configurator_optimize(cs_procedure_scheduling_t scheduling,
                                     cs_channel_map_preset_t channel_map_preset,
                                     uint32_t estimation_time_us,
                                     uint8_t peer_count,
                                     uint8_t num_antenna_paths,
                                     cs_configurator_parameters_t *parameters_inout)
{
  if ((parameters_inout == NULL)
      || (parameters_inout->connection_parameters == NULL)
      || (parameters_inout->cs_procedure_parameters == NULL)
      || (parameters_inout->rreq_config == NULL)) {
    return SL_STATUS_NULL_POINTER;
  }

  uint32_t conn_interval = CS_CONFIGURATOR_OPTIMIZE_INITIAL_CONN_INTERVAL;
  uint32_t proc_interval = CS_CONFIGURATOR_OPTIMIZE_INITIAL_PROC_INTERVAL;
  bool use_real_time_ras = parameters_inout->rreq_config->real_time_mode;

  if (scheduling != CS_PROCEDURE_SCHEDULING_CUSTOM) {
    if (peer_count == 0) {
      return SL_STATUS_INVALID_PARAMETER;
    }

    uint32_t measurement_time_us = calc_measurement_time_us(channel_map_preset,
                                                            num_antenna_paths);
    // Excluding RAS overhead
    uint32_t ras_data_time_us = calc_ras_data_time_us(channel_map_preset,
                                                      num_antenna_paths);

    // Set bounds for conn_interval based on scheduling
    uint32_t conn_interval_lower_bound = 0;
    uint32_t conn_interval_upper_bound = 0;
    if (peer_count > 1) {
      if (scheduling == CS_PROCEDURE_SCHEDULING_OPTIMIZED_FOR_FREQUENCY) {
        conn_interval_lower_bound = CS_CONFIGURATOR_OPTIMIZE_FREQUENCY_CONN_INTERVAL_MULTIPLE_PEERS_LOWER_BOUND;
        conn_interval_upper_bound = CS_CONFIGURATOR_OPTIMIZE_FREQUENCY_CONN_INTERVAL_MULTIPLE_PEERS_UPPER_BOUND;
      } else {
        conn_interval_lower_bound = CS_CONFIGURATOR_OPTIMIZE_ENERGY_CONN_INTERVAL_MULTIPLE_PEERS_LOWER_BOUND;
        conn_interval_upper_bound = CS_CONFIGURATOR_OPTIMIZE_ENERGY_CONN_INTERVAL_MULTIPLE_PEERS_UPPER_BOUND;
      }
    } else {
      if (scheduling == CS_PROCEDURE_SCHEDULING_OPTIMIZED_FOR_FREQUENCY) {
        conn_interval_lower_bound = CS_CONFIGURATOR_OPTIMIZE_FREQUENCY_CONN_INTERVAL_LOWER_BOUND;
        conn_interval_upper_bound = CS_CONFIGURATOR_OPTIMIZE_FREQUENCY_CONN_INTERVAL_UPPER_BOUND;
      } else {
        conn_interval_lower_bound = CS_CONFIGURATOR_OPTIMIZE_ENERGY_CONN_INTERVAL_LOWER_BOUND;
        conn_interval_upper_bound = CS_CONFIGURATOR_OPTIMIZE_ENERGY_CONN_INTERVAL_UPPER_BOUND;
      }
    }

    // Search for the best conn_interval and proc_interval combination
    for (uint32_t conn_interval_temp = conn_interval_lower_bound;
         conn_interval_temp <= conn_interval_upper_bound;
         conn_interval_temp++) {
      uint32_t proc_interval_temp = calc_min_proc_interval(estimation_time_us,
                                                           measurement_time_us,
                                                           ras_data_time_us,
                                                           use_real_time_ras,
                                                           conn_interval_temp,
                                                           peer_count);
      if (proc_interval_temp * conn_interval_temp <= proc_interval * conn_interval) {
        conn_interval = conn_interval_temp;
        proc_interval = proc_interval_temp;
      }
    }

    if (proc_interval >= CS_CONFIGURATOR_OPTIMIZE_INITIAL_PROC_INTERVAL
        || conn_interval >= CS_CONFIGURATOR_OPTIMIZE_INITIAL_CONN_INTERVAL) {
      return SL_STATUS_FAIL;
    }
    if (conn_interval > UINT16_MAX) {
      return SL_STATUS_WOULD_OVERFLOW;
    }
    if (proc_interval > UINT16_MAX) {
      return SL_STATUS_WOULD_OVERFLOW;
    }

    // Write to parameters_inout
    parameters_inout->connection_parameters->max_connection_interval = (uint16_t)conn_interval;
    parameters_inout->connection_parameters->min_connection_interval = (uint16_t)conn_interval;
    parameters_inout->cs_procedure_parameters->max_procedure_interval = (uint16_t)proc_interval;
    parameters_inout->cs_procedure_parameters->min_procedure_interval = (uint16_t)proc_interval;
  }
  // Procedure length adjustment
  if ((parameters_inout->cs_procedure_parameters->max_procedure_len)
      > (parameters_inout->cs_procedure_parameters->max_procedure_interval
         * parameters_inout->connection_parameters->max_connection_interval * 2)) {
    parameters_inout->cs_procedure_parameters->max_procedure_len
      = (uint32_t)(parameters_inout->cs_procedure_parameters->max_procedure_interval
                   * parameters_inout->connection_parameters->max_connection_interval * 2);
  }
  // Subevent length adjustment
  if (parameters_inout->cs_procedure_parameters->max_subevent_len
      > (parameters_inout->cs_procedure_parameters->max_procedure_len * 625UL)) {
    parameters_inout->cs_procedure_parameters->max_subevent_len
      = (uint32_t)(parameters_inout->cs_procedure_parameters->max_procedure_len * 625UL);
  }
  // Signal custom scheduling
  if (scheduling == CS_PROCEDURE_SCHEDULING_CUSTOM) {
    return SL_STATUS_IDLE;
  }
  return SL_STATUS_OK;
}

// -----------------------------------------------------------------------------
// Private (static) function definitions

static uint16_t calc_min_proc_intv_for_conn_intv(uint32_t measurement_time_us,
                                                 uint32_t ras_data_time_us,
                                                 uint16_t conn_interval,
                                                 bool use_real_time_ras,
                                                 uint8_t peer_count)
{
  // Using uint32_t to avoid issues from calculating with different data types
  uint32_t proc_interval = 0;
  // Scale measurement time by the peer count
  measurement_time_us *= peer_count;
  // Scale RAS data time by the peer count
  ras_data_time_us *= peer_count;
  // How many connection events are needed for the measurement
  proc_interval += measurement_time_us
                   / (conn_interval * CS_CONN_INTERVAL_TIME_RESOLUTION_US) + 1;
  // How many connection events are needed for the RAS data transfer
  proc_interval += ras_data_time_us
                   / (conn_interval * CS_CONN_INTERVAL_TIME_RESOLUTION_US) + 1;
  // Account for the empty connection event between the measurement - data transfer
  // In case of multiple peers this does not scale linearly
  if (peer_count > 1) {
    proc_interval += CS_CONFIGURATOR_EMPTY_BEFORE_RAS_CE_MULTIPLE_PEERS;
  } else {
    proc_interval += CS_CONFIGURATOR_EMPTY_BEFORE_RAS_CE;
  }
  // Add the on-demand RAS overhead (in connection events)
  if (!use_real_time_ras) {
    proc_interval += CS_CONFIGURATOR_RAS_ON_DEMAND_OVERHEAD_CE * peer_count;
  }
  // Add safety padding to be more robust against jitter/errors
  proc_interval += CS_CONFIGURATOR_PADDING_CE;
  // Convert back to uint16_t
  return (uint16_t)(proc_interval & 0xFFFF);
}

static uint16_t calc_min_proc_intv_for_est_time(uint32_t estimation_time_us,
                                                uint16_t conn_interval,
                                                uint8_t peer_count)
{
  // Using uint32_t to avoid issues from calculating with different data types
  uint32_t proc_interval = 0;
  // How many connection events are needed for the estimation time
  proc_interval += (estimation_time_us / CS_CONN_INTERVAL_TIME_RESOLUTION_US)
                   / conn_interval + 1;
  // Multiply by the peer count
  proc_interval *= peer_count;
  // Padding so the procedure is longer than the estimation time
  proc_interval += CS_CONFIGURATOR_DISTANCE_CALCULATION_PADDING_CE;
  // Convert back to uint16_t
  return (uint16_t)(proc_interval & 0xFFFF);
}

static uint16_t calc_min_proc_interval(uint32_t estimation_time_us,
                                       uint32_t measurement_time_us,
                                       uint32_t ras_data_time_us,
                                       bool use_real_time_ras,
                                       uint16_t conn_interval,
                                       uint8_t peer_count)
{
  // Calculate the minimum procedure interval for the connection interval
  uint16_t min_proc_interval_for_conn_interval =
    calc_min_proc_intv_for_conn_intv(measurement_time_us,
                                     ras_data_time_us,
                                     conn_interval,
                                     use_real_time_ras,
                                     peer_count);
  // Calculate the minimum procedure interval for the estimation time
  uint16_t min_proc_interval_for_estimation_time =
    calc_min_proc_intv_for_est_time(estimation_time_us,
                                    conn_interval,
                                    peer_count);
  // Return the maximum of the two minimum procedure intervals
  if (min_proc_interval_for_conn_interval > min_proc_interval_for_estimation_time) {
    return min_proc_interval_for_conn_interval;
  } else {
    return min_proc_interval_for_estimation_time;
  }
}

static uint32_t calc_measurement_time_us(cs_channel_map_preset_t channel_map_preset,
                                         uint8_t num_antenna_paths)
{
  // Start with the base measurement time (for high channel map, dual antenna, pbr)
  uint32_t measurement_time_us = CS_CONFIGURATOR_MEASUREMENT_BASE_US;

  // If not using the high channel map, apply the time ratio for the medium channel map
  if (channel_map_preset != CS_CHANNEL_MAP_PRESET_HIGH) {
    CS_CONFIGURATOR_APPLY_TIME_RATIO(measurement_time_us,
                                     CS_CONFIGURATOR_MEASUREMENT_RATIO_CHANNEL_MAP_HIGH_TO_MEDIUM);
  }
  // Apply the time ratio for the antenna paths
  switch (num_antenna_paths) {
    case 1:
      CS_CONFIGURATOR_APPLY_TIME_RATIO(measurement_time_us,
                                       CS_CONFIGURATOR_MEASUREMENT_RATIO_ANTENNA_PATHS_4_TO_1);
      break;
    case 2:
      CS_CONFIGURATOR_APPLY_TIME_RATIO(measurement_time_us,
                                       CS_CONFIGURATOR_MEASUREMENT_RATIO_ANTENNA_PATHS_4_TO_2);
      break;
    case 4:
    default:
      //Keep default values
      break;
  }

  // Account for Mode0 Steps
  measurement_time_us += CS_CONFIGURATOR_MODE0_BASE_US;

  return measurement_time_us;
}

static uint32_t calc_ras_data_time_us(cs_channel_map_preset_t channel_map_preset,
                                      uint8_t num_antenna_paths)
{
  // Start with the base RAS data time
  // (for high channel map, dual antenna, pbr)
  // Excluding RAS On-Demand overhead
  uint32_t ras_data_time_excluding_on_demand_overhead_us = CS_CONFIGURATOR_RAS_DATA_BASE_US;

  // If not using the high channel map, apply the time ratio for the medium channel map
  if (channel_map_preset != CS_CHANNEL_MAP_PRESET_HIGH) {
    CS_CONFIGURATOR_APPLY_TIME_RATIO(ras_data_time_excluding_on_demand_overhead_us,
                                     CS_CONFIGURATOR_RAS_DATA_RATIO_CHANNEL_MAP_HIGH_TO_MEDIUM);
  }
  // Apply the time ratio for the antenna paths
  switch (num_antenna_paths) {
    case 1:
      CS_CONFIGURATOR_APPLY_TIME_RATIO(ras_data_time_excluding_on_demand_overhead_us,
                                       CS_CONFIGURATOR_RAS_DATA_RATIO_ANTENNA_PATHS_4_TO_1);
      break;
    case 2:
      CS_CONFIGURATOR_APPLY_TIME_RATIO(ras_data_time_excluding_on_demand_overhead_us,
                                       CS_CONFIGURATOR_RAS_DATA_RATIO_ANTENNA_PATHS_4_TO_2);
      break;
    case 4:
    default:
      //Keep default values
      break;
  }

  return ras_data_time_excluding_on_demand_overhead_us;
}
