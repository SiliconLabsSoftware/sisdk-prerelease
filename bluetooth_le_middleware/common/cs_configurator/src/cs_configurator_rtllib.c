/***************************************************************************//**
 * @file
 * @brief CS Configurator — RTL library integration
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

#include "cs_configurator_rtllib.h"
#include "cs_configurator_rtllib_parameters.h"

// -----------------------------------------------------------------------------
// Enums, structs, typedefs

typedef enum {
  CS_ALGO_MODE_REAL_TIME_BASIC = 0,
  CS_ALGO_MODE_STATIC_HIGH_ACCURACY,
  CS_ALGO_MODE_REAL_TIME_FAST,
} cs_algo_mode_t;

// -----------------------------------------------------------------------------
// Static function declarations
static sl_status_t validate_rtl_supported_combinations(sl_bt_cs_mode_t main_mode,
                                                       sl_bt_cs_mode_t sub_mode,
                                                       uint8_t algo_mode,
                                                       cs_channel_map_preset_t channel_preset);

// -----------------------------------------------------------------------------
// Private (static) function definitions

static sl_status_t validate_rtl_supported_combinations(sl_bt_cs_mode_t main_mode,
                                                       sl_bt_cs_mode_t sub_mode,
                                                       uint8_t algo_mode,
                                                       cs_channel_map_preset_t channel_preset)
{
  const bool pbr_main = (main_mode == sl_bt_cs_mode_pbr);
  const bool rtt_main = (main_mode == sl_bt_cs_mode_rtt);
  const bool sub_none = (sub_mode == sl_bt_cs_submode_disabled);
  const bool sub_rtt = (sub_mode == sl_bt_cs_mode_rtt);
  const bool custom_map = (channel_preset == CS_CHANNEL_MAP_PRESET_CUSTOM);

  if (algo_mode != CS_ALGO_MODE_REAL_TIME_BASIC
      && algo_mode != CS_ALGO_MODE_REAL_TIME_FAST
      && algo_mode != CS_ALGO_MODE_STATIC_HIGH_ACCURACY) {
    return SL_STATUS_INVALID_PARAMETER;
  }

  if (!pbr_main && !rtt_main) {
    return SL_STATUS_INVALID_PARAMETER;
  }
  if (rtt_main && !sub_none) {
    return SL_STATUS_INVALID_PARAMETER;
  }
  if (pbr_main && !sub_none && !sub_rtt) {
    return SL_STATUS_INVALID_PARAMETER;
  }

  // RTT (no sub-mode): REAL TIME FAST is not supported.
  if (rtt_main && sub_none && algo_mode == CS_ALGO_MODE_REAL_TIME_FAST) {
    return SL_STATUS_INVALID_PARAMETER;
  }

  if (pbr_main && sub_none) {
    if (algo_mode == CS_ALGO_MODE_STATIC_HIGH_ACCURACY) {
      if (custom_map || channel_preset != CS_CHANNEL_MAP_PRESET_HIGH) {
        return SL_STATUS_INVALID_PARAMETER;
      }
    }
    return SL_STATUS_OK;
  }

  if (rtt_main && sub_none) {
    if (custom_map) {
      return SL_STATUS_INVALID_PARAMETER;
    }
    if (algo_mode == CS_ALGO_MODE_REAL_TIME_BASIC
        || algo_mode == CS_ALGO_MODE_STATIC_HIGH_ACCURACY) {
      if (channel_preset != CS_CHANNEL_MAP_PRESET_HIGH) {
        return SL_STATUS_INVALID_PARAMETER;
      }
    }
    return SL_STATUS_OK;
  }

  // PBR + RTT sub-mode: 
  // only HIGH channel map preset for all listed algorithm modes.
  if (pbr_main && sub_rtt) {
    if (custom_map || channel_preset != CS_CHANNEL_MAP_PRESET_HIGH) {
      return SL_STATUS_INVALID_PARAMETER;
    }
    return SL_STATUS_OK;
  }

  return SL_STATUS_INVALID_PARAMETER;
}

static sl_status_t cs_configurator_cycles_to_us(uint64_t cycles,
                                                uint32_t clock_frequency_hz, 
                                                uint32_t *us_out)
{
  if (clock_frequency_hz == 0) {
    return SL_STATUS_FAIL;
  }
  *us_out = (uint32_t)((cycles * 1000000ULL) / clock_frequency_hz + 1);
  return SL_STATUS_OK;
}

// -----------------------------------------------------------------------------
// Public function definitions

sl_status_t cs_configurator_rtllib_get_estimation_time_us(cs_configurator_parameters_t *input,
                                                          uint8_t algo_mode,
                                                          uint32_t clock_frequency_hz,
                                                          uint32_t *estimation_time_us,
                                                          //TODO: remove WIP when CS Manager is ready
                                                          cs_channel_map_preset_t WIP_channel_map_preset,
                                                          sl_bt_cs_mode_t WIP_main_mode,
                                                          sl_bt_cs_mode_t WIP_sub_mode)
{
  uint64_t cycles = 0;
  uint8_t row = 0;
  uint8_t col = 0;
  sl_status_t sc = SL_STATUS_OK;
  cs_channel_map_preset_t channel_map_preset = WIP_channel_map_preset;
  sl_bt_cs_mode_t main_mode = WIP_main_mode;
  sl_bt_cs_mode_t sub_mode = WIP_sub_mode;

  //TODO: Uncomment when CS Manager is ready
  (void)input;
  //if (input == NULL || 
  //    estimation_time_us == NULL || 
  //    input->cs_config == NULL || 
  //    input->cs_instance_config == NULL) {
  //  return SL_STATUS_NULL_POINTER;
  //}

  sc = validate_rtl_supported_combinations(main_mode,
                                           sub_mode,
                                           algo_mode,
                                           channel_map_preset);
  if (sc != SL_STATUS_OK) {
    return sc;
  }

  // Map the Main Mode + Sub Mode combination to the cycle count table row.
  switch (main_mode) {
    case sl_bt_cs_mode_rtt:
      row = CS_CONF_CYCLES_TABLE_RTT_IDX;
      break;

    case sl_bt_cs_mode_pbr:
      if (sub_mode == sl_bt_cs_mode_rtt) {
        row = CS_CONF_CYCLES_TABLE_PBR_W_RTT_SUB_IDX;
      } else {
        row = CS_CONF_CYCLES_TABLE_PBR_IDX;
      }
      break;

    default:
      // Should not happen: covered by validate_rtl_supported_combinations.
      return SL_STATUS_INVALID_PARAMETER;
  }

  // Map the Algorithm Mode to the cycle count table column.
  switch (algo_mode) {
    case CS_ALGO_MODE_REAL_TIME_BASIC:
      col = CS_CONF_CYCLES_TABLE_RT_BASIC_IDX;
      break;

    case CS_ALGO_MODE_REAL_TIME_FAST:
      col = CS_CONF_CYCLES_TABLE_RT_FAST_IDX;
      break;

    case CS_ALGO_MODE_STATIC_HIGH_ACCURACY:
      col = CS_CONF_CYCLES_TABLE_STATIC_HIGH_ACC_IDX;
      break;

    default:
      return SL_STATUS_INVALID_PARAMETER;
  }

  // Get the cycle count from the cycle count table.
  cycles = cs_conf_rtl_lib_cycles_table[row][col];
  return cs_configurator_cycles_to_us(cycles, 
                                      clock_frequency_hz, 
                                      estimation_time_us);
}

sl_status_t cs_configurator_validate_for_rtl(cs_procedure_scheduling_t scheduling,
                                             uint32_t estimation_time_us,
                                             uint8_t peer_count,
                                             uint8_t algo_mode,
                                             cs_configurator_parameters_t *config,
                                             //TODO: remove WIP when CS Manager is ready
                                             cs_channel_map_preset_t WIP_channel_map_preset,
                                             sl_bt_cs_mode_t WIP_main_mode,
                                             sl_bt_cs_mode_t WIP_sub_mode)
{
  sl_status_t sc;
  cs_channel_map_preset_t channel_map_preset = WIP_channel_map_preset;
  sl_bt_cs_mode_t main_mode = WIP_main_mode;
  sl_bt_cs_mode_t sub_mode = WIP_sub_mode;

  if (scheduling != CS_PROCEDURE_SCHEDULING_OPTIMIZED_FOR_FREQUENCY
      && scheduling != CS_PROCEDURE_SCHEDULING_OPTIMIZED_FOR_ENERGY
      && scheduling != CS_PROCEDURE_SCHEDULING_CUSTOM) {
    return SL_STATUS_INVALID_PARAMETER;
  }

  if (peer_count == 0) {
    return SL_STATUS_INVALID_PARAMETER;
  }

  if (estimation_time_us == 0) {
    return SL_STATUS_INVALID_PARAMETER;
  }

  //TODO: Uncomment when CS Manager is ready
  (void)config;
  //if (config == NULL ||
  //    config->cs_config == NULL ||
  //    config->cs_instance_config == NULL ||
  //    config->rreq_config == NULL) {
  //  return SL_STATUS_NULL_POINTER;
  //}

  sc = validate_rtl_supported_combinations(main_mode,
                                           sub_mode,
                                           algo_mode,
                                           channel_map_preset);
  if (sc != SL_STATUS_OK) {
    return sc;
  }

  return SL_STATUS_OK;
}
