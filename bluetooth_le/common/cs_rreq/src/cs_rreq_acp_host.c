/***************************************************************************//**
 * @file
 * @brief CS RREQ ACP host - core implementation
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

#include "sl_status.h"
#include "sl_bt_api.h"
#include "ncp_host.h"
#include "cs_acp_common.h"
#include "cs_acp_host.h"
#include "cs_rreq_api.h"
#include "cs_rreq_acp_types.h"
#include "cs_rreq_acp_host.h"

// Callbacks for events.
static cs_rreq_event_callback_t callbacks;

// Create RREQ instance.
sl_status_t cs_rreq_create(uint8_t conn_handle, cs_rreq_create_config_t *config)
{
  cs_rreq_acp_cmd_t cs_acp_cmd;
  cs_acp_cmd.id = CS_ACP_CMD_RREQ_CREATE;
  cs_acp_cmd.data.cs_acp_cmd_rreq_create.conn_handle = conn_handle;
  memcpy(&cs_acp_cmd.data.cs_acp_cmd_rreq_create.config, config, sizeof(cs_rreq_create_config_t));

  size_t len = sizeof(cs_acp_cmd_id_t)
               + sizeof(cs_acp_cmd_rreq_create_t);

  return cs_acp_send_cmd(len,
                         (uint8_t *)&cs_acp_cmd,
                         0,
                         NULL,
                         NULL);
}

// Enable or disable RREQ (Read RAS features of the RAS Client for the first enable).
sl_status_t cs_rreq_enable(uint8_t conn_handle, uint8_t enable)
{
  cs_acp_cmd_rreq_enable_t cs_acp_cmd_rreq_enable = {
    .conn_handle = conn_handle,
    .enable = enable,
  };

  cs_rreq_acp_cmd_t cs_acp_cmd = {
    .id = CS_ACP_CMD_RREQ_ENABLE,
    .data.cs_acp_cmd_rreq_enable = cs_acp_cmd_rreq_enable,
  };

  size_t len = sizeof(cs_acp_cmd_id_t)
               + sizeof(cs_acp_cmd_rreq_enable_t);

  return cs_acp_send_cmd(len,
                         (uint8_t *)&cs_acp_cmd,
                         0,
                         NULL,
                         NULL);
}

// Remove RREQ instance.
sl_status_t cs_rreq_remove(uint8_t conn_handle)
{
  cs_acp_cmd_rreq_remove_t cs_acp_cmd_rreq_remove = {
    .conn_handle = conn_handle,
  };

  cs_rreq_acp_cmd_t cs_acp_cmd = {
    .id = CS_ACP_CMD_RREQ_REMOVE,
    .data.cs_acp_cmd_rreq_remove = cs_acp_cmd_rreq_remove,
  };

  size_t len = sizeof(cs_acp_cmd_id_t)
               + sizeof(cs_acp_cmd_rreq_remove_t);

  return cs_acp_send_cmd(len,
                         (uint8_t *)&cs_acp_cmd,
                         0,
                         NULL,
                         NULL);
}

// Set event callbacks for RREQ operations.
sl_status_t cs_rreq_set_event_callbacks(cs_rreq_event_callback_t cb)
{
  if ((cb.on_create == NULL)
      || (cb.on_enable == NULL)
      || (cb.on_error == NULL)) {
    return SL_STATUS_NULL_POINTER;
  }

  if ((callbacks.on_create != NULL)
      || (callbacks.on_enable != NULL)
      || (callbacks.on_error != NULL)) {
    return SL_STATUS_ALREADY_INITIALIZED;
  }

  callbacks.on_create = cb.on_create;
  callbacks.on_enable = cb.on_enable;
  callbacks.on_error = cb.on_error;
  return SL_STATUS_OK;
}

// Internal Bluetooth stack event handler for CS RREQ ACP.
void cs_rreq_acp_host_event(sl_bt_msg_t *evt)
{
  switch (SL_BT_MSG_ID(evt->header)) {
    // --------------------------------
    // Channel Sounding user event
    case sl_bt_evt_user_cs_service_message_to_host_id:
    {
      cs_rreq_acp_evt_t *cs_rreq_acp_evt = (cs_rreq_acp_evt_t *)evt->data.evt_user_cs_service_message_to_host.message.data;
      switch (cs_rreq_acp_evt->id) {
        case CS_ACP_EVT_RREQ_CREATE_COMPLETE:
        {
          cs_acp_evt_rreq_create_complete_t data;
          data.sc = cs_rreq_acp_evt->data.cs_acp_evt_rreq_create_complete.sc;
          data.conn_handle = cs_rreq_acp_evt->data.cs_acp_evt_rreq_create_complete.conn_handle;

          if (callbacks.on_create != NULL) {
            callbacks.on_create(data.conn_handle, data.sc);
          }
          break;
        }

        case CS_ACP_EVT_RREQ_ENABLE_COMPLETE:
        {
          cs_acp_evt_rreq_enable_complete_t data;
          data.sc = cs_rreq_acp_evt->data.cs_acp_evt_rreq_enable_complete.sc;
          data.conn_handle = cs_rreq_acp_evt->data.cs_acp_evt_rreq_enable_complete.conn_handle;
          data.enable = cs_rreq_acp_evt->data.cs_acp_evt_rreq_enable_complete.enable;

          if (callbacks.on_enable != NULL) {
            callbacks.on_enable(data.conn_handle, data.enable, data.sc);
          }
          break;
        }

        case CS_ACP_EVT_RREQ_ERROR:
        {
          cs_acp_evt_rreq_error_t data;
          data.sc = cs_rreq_acp_evt->data.cs_acp_evt_rreq_error.sc;
          data.conn_handle = cs_rreq_acp_evt->data.cs_acp_evt_rreq_error.conn_handle;
          data.error = cs_rreq_acp_evt->data.cs_acp_evt_rreq_error.error;

          if (callbacks.on_error != NULL) {
            callbacks.on_error(data.conn_handle, data.error, data.sc);
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
