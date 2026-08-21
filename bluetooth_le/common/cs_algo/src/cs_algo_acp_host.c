/***************************************************************************//**
 * @file
 * @brief CS Algo ACP host - core implementation
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
#include "sl_bt_api.h"
#include "ncp_host.h"
#include "cs_acp_common.h"
#include "cs_acp_host.h"
#include "cs_algo.h"
#include "cs_algo_acp_types.h"
#include "cs_algo_acp_host.h"

// Callbacks for events.
static cs_algo_event_callback_t callbacks;

static void parse_result_data(const uint8_t *data,
                              uint32_t data_len,
                              const uint8_t **result,
                              uint16_t *result_size,
                              cs_algo_result_t *ranging_data);
static const uint8_t *read_item(const uint8_t *cursor,
                                const uint8_t *end,
                                void *length,
                                size_t length_size,
                                const uint8_t **value);

// Configure cs_algo for a given (connection, config) pair. Initializes the
// RTL library instance and creates estimator for the passed configuration.
sl_status_t cs_algo_create(uint8_t conn_handle, cs_algo_config_t config)
{
  cs_acp_cmd_algo_create_t cs_acp_cmd_algo_create;
  cs_acp_cmd_algo_create.conn_handle = conn_handle;
  cs_acp_cmd_algo_create.config.rssi_ref_tx_power = config.rssi_ref_tx_power;
  cs_acp_cmd_algo_create.config.connection_interval = config.connection_interval;
  cs_acp_cmd_algo_create.config.cs_main_mode = config.cs_main_mode;
  cs_acp_cmd_algo_create.config.cs_sub_mode = config.cs_sub_mode;
  cs_acp_cmd_algo_create.config.min_main_mode_steps = config.min_main_mode_steps;
  cs_acp_cmd_algo_create.config.max_main_mode_steps = config.max_main_mode_steps;
  cs_acp_cmd_algo_create.config.main_mode_repetition = config.main_mode_repetition;
  cs_acp_cmd_algo_create.config.channel_map_repetition = config.channel_map_repetition;
  cs_acp_cmd_algo_create.config.channel_selection_type = config.channel_selection_type;
  cs_acp_cmd_algo_create.config.ch3c_shape = config.ch3c_shape;
  cs_acp_cmd_algo_create.config.ch3c_jump = config.ch3c_jump;
  cs_acp_cmd_algo_create.config.rtt_type = config.rtt_type;
  cs_acp_cmd_algo_create.config.cs_sync_phy = config.cs_sync_phy;
  cs_acp_cmd_algo_create.config.channel_map_preset = config.channel_map_preset;
  cs_acp_cmd_algo_create.config.num_calib_steps = config.num_calib_steps;
  cs_acp_cmd_algo_create.config.T_PM_time = config.T_PM_time;
  cs_acp_cmd_algo_create.config.T_IP1_time = config.T_IP1_time;
  cs_acp_cmd_algo_create.config.T_IP2_time = config.T_IP2_time;
  cs_acp_cmd_algo_create.config.T_FCS_time = config.T_FCS_time;
  cs_acp_cmd_algo_create.config.remote_t_sw_us = config.remote_t_sw_us;
  cs_acp_cmd_algo_create.config.tone_antenna_config_selection = config.tone_antenna_config_selection;
  memcpy(cs_acp_cmd_algo_create.config.channel_map.data,
         config.channel_map.data,
         sizeof(cs_acp_cmd_algo_create.config.channel_map.data));
  cs_acp_cmd_algo_create.config.rtl_config.rtl_logging_enabled = config.rtl_config.rtl_logging_enabled;
  cs_acp_cmd_algo_create.config.rtl_config.algo_mode = config.rtl_config.algo_mode;

  cs_algo_acp_cmd_t cs_acp_cmd = {
    .id = CS_ACP_CMD_ALGO_CREATE,
    .data.cs_acp_cmd_algo_create = cs_acp_cmd_algo_create,
  };

  size_t len = sizeof(cs_acp_cmd_id_t)
               + sizeof(cs_acp_cmd_algo_create_t);

  return cs_acp_send_cmd(len,
                         (uint8_t *)&cs_acp_cmd,
                         0,
                         NULL,
                         NULL);
}

// Remove the cs_algo instance associated with a connection.
sl_status_t cs_algo_remove(uint8_t conn_handle)
{
  cs_acp_cmd_algo_remove_t cs_acp_cmd_algo_remove = {
    .conn_handle = conn_handle,
  };

  cs_algo_acp_cmd_t cs_acp_cmd = {
    .id = CS_ACP_CMD_ALGO_REMOVE,
    .data.cs_acp_cmd_algo_remove = cs_acp_cmd_algo_remove,
  };

  size_t len = sizeof(cs_acp_cmd_id_t)
               + sizeof(cs_acp_cmd_algo_remove_t);

  return cs_acp_send_cmd(len,
                         (uint8_t *)&cs_acp_cmd,
                         0,
                         NULL,
                         NULL);
}

// Register callback functions for Algo events.
sl_status_t cs_algo_set_event_callbacks(cs_algo_event_callback_t cb)
{
  if (cb.on_result != NULL) {
    if (callbacks.on_result != NULL) {
      return SL_STATUS_ALREADY_INITIALIZED;
    }
    callbacks.on_result = cb.on_result;
  }

  if (cb.on_intermediate_result != NULL) {
    if (callbacks.on_intermediate_result != NULL) {
      return SL_STATUS_ALREADY_INITIALIZED;
    }
    callbacks.on_intermediate_result = cb.on_intermediate_result;
  }

  if (cb.on_error != NULL) {
    if (callbacks.on_error != NULL) {
      return SL_STATUS_ALREADY_INITIALIZED;
    }
    callbacks.on_error = cb.on_error;
  }

  return SL_STATUS_OK;
}

// Internal Bluetooth stack event handler for CS Algo ACP.
void cs_algo_acp_host_event(sl_bt_msg_t *evt)
{
  switch (SL_BT_MSG_ID(evt->header)) {
    // --------------------------------
    // Channel Sounding user event
    case sl_bt_evt_user_cs_service_message_to_host_id:
    {
      cs_algo_acp_evt_t *cs_algo_acp_evt = (cs_algo_acp_evt_t *)evt->data.evt_user_cs_service_message_to_host.message.data;
      switch (cs_algo_acp_evt->id) {
        case CS_ACP_EVT_ALGO_ON_RESULT:
        {
          cs_acp_evt_algo_on_result_t *data = &(cs_algo_acp_evt->data.cs_acp_evt_algo_on_result);
          if (callbacks.on_result != NULL) {
            const uint8_t *result;
            uint16_t result_size;
            cs_algo_result_t ranging_data;
            parse_result_data(data->data,
                              data->data_len,
                              &result,
                              &result_size,
                              &ranging_data);

            callbacks.on_result(data->conn_handle,
                                data->ranging_counter,
                                result,
                                result_size,
                                &ranging_data);
          }
          break;
        }

        case CS_ACP_EVT_ALGO_ON_INTERMEDIATE_RESULT:
        {
          cs_acp_evt_algo_on_intermediate_result_t *data = &(cs_algo_acp_evt->data.cs_acp_evt_algo_on_intermediate_result);
          cs_intermediate_result_t cs_intermediate_result = {
            .progress_percentage = data->progress_percentage,
            .connection = data->conn_handle,
          };

          if (callbacks.on_intermediate_result != NULL) {
            callbacks.on_intermediate_result(&cs_intermediate_result);
          }
          break;
        }

        case CS_ACP_EVT_ALGO_ON_ERROR:
        {
          cs_acp_evt_algo_on_error_t *data = &(cs_algo_acp_evt->data.cs_acp_evt_algo_on_error);
          if (callbacks.on_error != NULL) {
            callbacks.on_error(data->conn_handle, data->ranging_counter, data->error, data->sc);
          }

          break;
        }

        default:
        {
          break;
        }
      }
      break;
    }

    default:
      break;
  }
}

// Walk the serialized blob and populate the deserialized fields.
// Wire layout is a sequence of length-prefixed items in the following order:
//   1. result            (uint16_t length prefix)
//   2. step_channels     (uint8_t  length prefix)
//   3. initiator ranging data (uint32_t length prefix)
//   4. reflector ranging data (uint32_t length prefix)
// Each length prefix is sized to match the natural width of the corresponding
// field in `cs_algo_result_t` (and `result_size` from the callback signature),
// which is known to both the ACP host and ACP target.
// The value pointers stored in the output (result, ranging_data->step_channels,
// ranging_data->initiator.data, ranging_data->reflector.data) alias into the
// source buffer (zero-copy), so the caller must keep that buffer alive while
// it uses them.
static void parse_result_data(const uint8_t *data,
                              uint32_t data_len,
                              const uint8_t **result,
                              uint16_t *result_size,
                              cs_algo_result_t *ranging_data)
{
  *result = NULL;
  *result_size = 0U;
  memset(ranging_data, 0, sizeof(*ranging_data));

  const uint8_t *cursor = data;
  const uint8_t *end = data + data_len;

  const uint8_t *value;

  // result
  cursor = read_item(cursor, end, result_size, sizeof(*result_size), &value);
  if (cursor == NULL) {
    return;
  }
  *result = value;

  // step_channels
  cursor = read_item(cursor,
                     end,
                     &ranging_data->num_steps,
                     sizeof(ranging_data->num_steps),
                     &value);
  if (cursor == NULL) {
    return;
  }
  ranging_data->step_channels = value;

  // initiator ranging data
  cursor = read_item(cursor,
                     end,
                     &ranging_data->initiator.data_size,
                     sizeof(ranging_data->initiator.data_size),
                     &value);
  if (cursor == NULL) {
    return;
  }
  ranging_data->initiator.data = value;

  // reflector ranging data
  cursor = read_item(cursor,
                     end,
                     &ranging_data->reflector.data_size,
                     sizeof(ranging_data->reflector.data_size),
                     &value);
  if (cursor == NULL) {
    return;
  }
  ranging_data->reflector.data = value;
}

// Read a single length-prefixed item from a serialization buffer.
// Layout: length (`length_size` bytes) | value bytes (count given by the
// just-read length, interpreted as an unsigned little-endian integer of
// `length_size` bytes).
// `length` is written using the caller's natural type (uint8_t, uint16_t or
// uint32_t); `length_size` must equal `sizeof(*length)` at the call site.
// On return, `*value` points inside the source buffer (zero-copy) or is NULL
// when the decoded length is 0. Returns the cursor advanced past the item
// just read so calls can be chained, or NULL if the buffer is truncated.
static const uint8_t *read_item(const uint8_t *cursor,
                                const uint8_t *end,
                                void *length,
                                size_t length_size,
                                const uint8_t **value)
{
  if ((size_t)(end - cursor) < length_size) {
    return NULL;
  }
  memcpy(length, cursor, length_size);

  // Decode the just-read length as a size_t so we can advance and bounds-check
  // the cursor without caring about the underlying type. Relies on the same
  // little-endian wire layout the rest of the ACP path already assumes for
  // packed structs.
  size_t value_size = 0U;
  memcpy(&value_size, cursor, length_size);
  cursor += length_size;

  if ((size_t)(end - cursor) < value_size) {
    return NULL;
  }
  *value = (value_size > 0U) ? cursor : NULL;
  cursor += value_size;

  return cursor;
}
