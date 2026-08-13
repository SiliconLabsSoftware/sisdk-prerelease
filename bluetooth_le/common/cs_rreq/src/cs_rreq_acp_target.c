/***************************************************************************//**
 * @file
 * @brief CS RREQ ACP target - core implementation
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
#include "cs_acp_common.h"
#include "cs_acp_target.h"
#include "cs_rreq_api.h"
#include "cs_rreq_acp_types.h"
#include "cs_rreq_acp_target.h"

// Callbacks
static void on_create(uint8_t conn_handle, sl_status_t sc);
static void on_enable(uint8_t conn_handle, uint8_t enable, sl_status_t sc);
static void on_error(uint8_t conn_handle, cs_rreq_error_t error, sl_status_t sc);

static const cs_rreq_event_callback_t callbacks = {
  .on_create = on_create,
  .on_enable = on_enable,
  .on_error = on_error,
};

// Initialize the RREQ ACP target module.
void cs_rreq_acp_target_init(void)
{
  // Subscribe these callbacks to RREQ. Note that these callbacks only invoke
  // the ACP event. When the ACP event arrives on the ACP host, the real
  // callbacks are invoked as well.
  (void)cs_rreq_set_event_callbacks(callbacks);
}

// Called when a CS command is received from the ACP host.
void cs_rreq_acp_target_on_command(const void *data)
{
  uint8array *cmd = (uint8array *)data;
  cs_rreq_acp_cmd_t *cs_acp_cmd = (cs_rreq_acp_cmd_t *)cmd->data;
  sl_status_t sc = SL_STATUS_OK;

  switch (cs_acp_cmd->id) {
    case CS_ACP_CMD_RREQ_CREATE:
    {
      cs_acp_cmd_rreq_create_t cmd = cs_acp_cmd->data.cs_acp_cmd_rreq_create;
      sc = cs_rreq_create(cmd.conn_handle, &cmd.config);
      cs_acp_send_cmd_rsp(sc, 0, NULL);
      break;
    }

    case CS_ACP_CMD_RREQ_ENABLE:
    {
      cs_acp_cmd_rreq_enable_t cmd = cs_acp_cmd->data.cs_acp_cmd_rreq_enable;
      sc = cs_rreq_enable(cmd.conn_handle, cmd.enable);
      cs_acp_send_cmd_rsp(sc, 0, NULL);
      break;
    }

    case CS_ACP_CMD_RREQ_REMOVE:
    {
      cs_acp_cmd_rreq_remove_t cmd = cs_acp_cmd->data.cs_acp_cmd_rreq_remove;
      sc = cs_rreq_remove(cmd.conn_handle);
      cs_acp_send_cmd_rsp(sc, 0, NULL);
      break;
    }

    // The command is not related to RREQ.
    default:
    {
      break;
    }
  }
}

static void on_create(uint8_t conn_handle, sl_status_t sc)
{
  cs_rreq_acp_evt_t cs_rreq_acp_evt = {
    .id = CS_ACP_EVT_RREQ_CREATE_COMPLETE,
    .data.cs_acp_evt_rreq_create_complete = {
      .conn_handle = conn_handle,
      .sc = sc,
    },
  };

  uint8_t len = sizeof(cs_acp_evt_id_t)
                + sizeof(cs_acp_evt_rreq_create_complete_t);
  cs_acp_send_evt((uint8_t *)&cs_rreq_acp_evt, len);
}

static void on_enable(uint8_t conn_handle, uint8_t enable, sl_status_t sc)
{
  cs_rreq_acp_evt_t cs_rreq_acp_evt = {
    .id = CS_ACP_EVT_RREQ_ENABLE_COMPLETE,
    .data.cs_acp_evt_rreq_enable_complete = {
      .conn_handle = conn_handle,
      .enable = enable,
      .sc = sc,
    },
  };

  uint8_t len = sizeof(cs_acp_evt_id_t)
                + sizeof(cs_acp_evt_rreq_enable_complete_t);
  cs_acp_send_evt((uint8_t *)&cs_rreq_acp_evt, len);
}

static void on_error(uint8_t conn_handle, cs_rreq_error_t error, sl_status_t sc)
{
  cs_rreq_acp_evt_t cs_rreq_acp_evt = {
    .id = CS_ACP_EVT_RREQ_ERROR,
    .data.cs_acp_evt_rreq_error = {
      .conn_handle = conn_handle,
      .error = error,
      .sc = sc,
    },
  };

  uint8_t len = sizeof(cs_acp_evt_id_t)
                + sizeof(cs_acp_evt_rreq_error_t);
  cs_acp_send_evt((uint8_t *)&cs_rreq_acp_evt, len);
}
