/***************************************************************************//**
 * @file
 * @brief CS Algo ACP target - core implementation
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

#include <string.h>
#include "sl_status.h"
#include "sl_memory_manager.h"
#include "app_rta.h"
#include "cs_acp_common.h"
#include "cs_acp_target.h"
#include "cs_algo.h"
#include "cs_algo_acp_types.h"
#include "cs_algo_acp_target.h"
#include "cs_algo_acp_target_config.h"

#define EVT_DATA_BUFFER_MAX_SIZE  (       \
    sizeof(cs_acp_evt_id_t)               \
    + sizeof(cs_acp_evt_algo_on_result_t) \
    + sizeof(uint16_t) + UINT8_MAX        \
    )

/// Event buffer
static uint8_t evt_data_buffer[EVT_DATA_BUFFER_MAX_SIZE];

/// App RTA context
static app_rta_context_t app_rta_ctx;

static void on_result(uint8_t conn_handle,
                      uint16_t ranging_counter,
                      const uint8_t *result,
                      uint16_t result_size,
                      const cs_algo_result_t *ranging_data);
static void on_intermediate_result(const cs_intermediate_result_t *intermediate_result);
static void on_error(uint8_t conn_handle,
                     uint16_t ranging_counter,
                     cs_algo_error_t error, sl_status_t sc);

static void cs_algo_acp_target_step(void);
static void on_app_rta_error(app_rta_error_t error, sl_status_t result);

static const cs_algo_event_callback_t callbacks = {
  .on_result = on_result,
  .on_intermediate_result = on_intermediate_result,
  .on_error = on_error,
};

// Initialize the Algo ACP target module.
void cs_algo_acp_target_init(void)
{
  // Create context
  sl_status_t sc;
  app_rta_config_t config = {
    .requirement.runtime = true,
    .requirement.guard   = true,
    .requirement.signal  = true,
    .step                = cs_algo_acp_target_step,
    .priority            = CS_ALGO_ACP_TARGET_CONFIG_CONFIG_PRIORITY,
    .stack_size          = CS_ALGO_ACP_TARGET_CONFIG_CONFIG_STACK,
    .error               = on_app_rta_error,
    .wait_for_guard      = CS_ALGO_ACP_TARGET_CONFIG_CONFIG_WAIT
  };
  sc = app_rta_create_context(&config, &app_rta_ctx);
  if (sc != SL_STATUS_OK) {
    on_app_rta_error(APP_RTA_ERROR_RUNTIME_INIT_FAILED, sc);
  }
}

// Finalize Algo ACP target module initialization.
void cs_algo_acp_target_ready(void)
{
  // Subscribe these callbacks to Algo. Note that these callbacks only invoke
  // the ACP event. When the ACP event arrives on the ACP host, the real
  // callbacks are invoked as well.
  (void)cs_algo_set_event_callbacks(callbacks);
}

static void cs_algo_acp_target_step(void)
{
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

// Called when a CS command is received from the ACP host.
void cs_algo_acp_target_on_command(const void *data)
{
  uint8array *cmd = (uint8array *)data;
  cs_algo_acp_cmd_t *cs_acp_cmd = (cs_algo_acp_cmd_t *)cmd->data;
  sl_status_t sc = SL_STATUS_OK;

  switch (cs_acp_cmd->id) {
    case CS_ACP_CMD_ALGO_CREATE:
    {
      cs_acp_cmd_algo_create_t cmd;
      cmd.conn_handle = cs_acp_cmd->data.cs_acp_cmd_algo_create.conn_handle;
      cmd.config.rssi_ref_tx_power = cs_acp_cmd->data.cs_acp_cmd_algo_create.config.rssi_ref_tx_power;
      cmd.config.connection_interval = cs_acp_cmd->data.cs_acp_cmd_algo_create.config.connection_interval;
      cmd.config.cs_main_mode = cs_acp_cmd->data.cs_acp_cmd_algo_create.config.cs_main_mode;
      cmd.config.cs_sub_mode = cs_acp_cmd->data.cs_acp_cmd_algo_create.config.cs_sub_mode;
      cmd.config.min_main_mode_steps = cs_acp_cmd->data.cs_acp_cmd_algo_create.config.min_main_mode_steps;
      cmd.config.max_main_mode_steps = cs_acp_cmd->data.cs_acp_cmd_algo_create.config.max_main_mode_steps;
      cmd.config.main_mode_repetition = cs_acp_cmd->data.cs_acp_cmd_algo_create.config.main_mode_repetition;
      cmd.config.channel_map_repetition = cs_acp_cmd->data.cs_acp_cmd_algo_create.config.channel_map_repetition;
      cmd.config.channel_selection_type = cs_acp_cmd->data.cs_acp_cmd_algo_create.config.channel_selection_type;
      cmd.config.ch3c_shape = cs_acp_cmd->data.cs_acp_cmd_algo_create.config.ch3c_shape;
      cmd.config.ch3c_jump = cs_acp_cmd->data.cs_acp_cmd_algo_create.config.ch3c_jump;
      cmd.config.rtt_type = cs_acp_cmd->data.cs_acp_cmd_algo_create.config.rtt_type;
      cmd.config.cs_sync_phy = cs_acp_cmd->data.cs_acp_cmd_algo_create.config.cs_sync_phy;
      cmd.config.channel_map_preset = cs_acp_cmd->data.cs_acp_cmd_algo_create.config.channel_map_preset;
      cmd.config.num_calib_steps = cs_acp_cmd->data.cs_acp_cmd_algo_create.config.num_calib_steps;
      cmd.config.T_PM_time = cs_acp_cmd->data.cs_acp_cmd_algo_create.config.T_PM_time;
      cmd.config.T_IP1_time = cs_acp_cmd->data.cs_acp_cmd_algo_create.config.T_IP1_time;
      cmd.config.T_IP2_time = cs_acp_cmd->data.cs_acp_cmd_algo_create.config.T_IP2_time;
      cmd.config.T_FCS_time = cs_acp_cmd->data.cs_acp_cmd_algo_create.config.T_FCS_time;
      cmd.config.remote_t_sw_us = cs_acp_cmd->data.cs_acp_cmd_algo_create.config.remote_t_sw_us;
      cmd.config.tone_antenna_config_selection = cs_acp_cmd->data.cs_acp_cmd_algo_create.config.tone_antenna_config_selection;
      memcpy(cmd.config.channel_map.data,
             cs_acp_cmd->data.cs_acp_cmd_algo_create.config.channel_map.data,
             sizeof(cmd.config.channel_map.data));
      cmd.config.rtl_config.rtl_logging_enabled = cs_acp_cmd->data.cs_acp_cmd_algo_create.config.rtl_config.rtl_logging_enabled;
      cmd.config.rtl_config.algo_mode = cs_acp_cmd->data.cs_acp_cmd_algo_create.config.rtl_config.algo_mode;

      sc = cs_algo_create(cmd.conn_handle, cmd.config);
      cs_acp_send_cmd_rsp(sc, 0, NULL);
      break;
    }

    case CS_ACP_CMD_ALGO_REMOVE:
    {
      cs_acp_cmd_algo_remove_t cmd = cs_acp_cmd->data.cs_acp_cmd_algo_remove;
      sc = cs_algo_remove(cmd.conn_handle);
      cs_acp_send_cmd_rsp(sc, 0, NULL);
      break;
    }

    // The command is not related to Algo.
    default:
    {
      break;
    }
  }
}

static void on_result(uint8_t conn_handle,
                      uint16_t ranging_counter,
                      const uint8_t *result,
                      uint16_t result_size,
                      const cs_algo_result_t *ranging_data)

{
  (void)ranging_data;
  if ((result_size > 0) && (result == NULL)) {
    return;
  }

  uint32_t data_len = sizeof(result_size) + result_size;

  // Allocate the event together with the inline flexible payload, so the
  // serialized blob lives in one contiguous buffer.
  size_t evt_size = sizeof(cs_acp_evt_id_t)
                    + sizeof(cs_acp_evt_algo_on_result_t)
                    + data_len;

  if (evt_size > sizeof(evt_data_buffer)) {
    return;
  }

  cs_algo_acp_evt_t *cs_algo_acp_evt = (cs_algo_acp_evt_t *)evt_data_buffer;

  cs_algo_acp_evt->id = CS_ACP_EVT_ALGO_ON_RESULT;
  cs_algo_acp_evt->data.cs_acp_evt_algo_on_result.conn_handle = conn_handle;
  cs_algo_acp_evt->data.cs_acp_evt_algo_on_result.ranging_counter = ranging_counter;
  cs_algo_acp_evt->data.cs_acp_evt_algo_on_result.data_len = data_len;

  uint8_t *data = cs_algo_acp_evt->data.cs_acp_evt_algo_on_result.data;

  // Serialize result
  memcpy(data, &result_size, sizeof(result_size));
  data += sizeof(result_size);
  memcpy(data, result, result_size);
  data += result_size;

  cs_acp_send_evt((uint8_t *)cs_algo_acp_evt, (uint8_t)evt_size);
}

static void on_intermediate_result(const cs_intermediate_result_t *intermediate_result)
{
  cs_algo_acp_evt_t cs_algo_acp_evt = {
    .id = CS_ACP_EVT_ALGO_ON_INTERMEDIATE_RESULT,
    .data.cs_acp_evt_algo_on_intermediate_result = {
      .conn_handle = intermediate_result->connection,
      .progress_percentage = intermediate_result->progress_percentage,
    },
  };

  uint8_t evt_size = sizeof(cs_acp_evt_id_t)
                     + sizeof(cs_acp_evt_algo_on_intermediate_result_t);
  cs_acp_send_evt((uint8_t *)&cs_algo_acp_evt, evt_size);
}

static void on_error(uint8_t conn_handle,
                     uint16_t ranging_counter,
                     cs_algo_error_t error,
                     sl_status_t sc)
{
  cs_algo_acp_evt_t cs_algo_acp_evt = {
    .id = CS_ACP_EVT_ALGO_ON_ERROR,
    .data.cs_acp_evt_algo_on_error = {
      .conn_handle = conn_handle,
      .ranging_counter = ranging_counter,
      .error = error,
      .sc = sc,
    },
  };

  uint8_t evt_size = sizeof(cs_acp_evt_id_t)
                     + sizeof(cs_acp_evt_algo_on_error_t);
  cs_acp_send_evt((uint8_t *)&cs_algo_acp_evt, evt_size);
}
