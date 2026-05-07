/***************************************************************************//**
 * @file
 * @brief CS Application Co-Processor target - core implementation
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
#include "sl_ncp.h"
#include "sl_component_catalog.h"
#include "cs_acp_common.h"
#include "cs_acp_target.h"

#ifdef SL_CATALOG_CS_RREQ_PRESENT
#include "cs_rreq_api.h"
#endif // SL_CATALOG_CS_RREQ_PRESENT

/**************************************************************************//**
 * CS user command (message_to_target) handler callback.
 *
 * Handle CS defined user commands received from NCP-host.
 *
 * @param[in] data Data received from NCP through UART.
 *
 * @note This overrides the default weak implementation.
 *****************************************************************************/
/*
// TODO: refactor implementation from bluetooth_le_app/example/refactor_bt_cs_ncp/app.c
void sl_ncp_user_cs_cmd_message_to_target_cb(const void *data)
{
  uint8array *cmd = (uint8array *)data;
  cs_acp_cmd_t *cs_acp_cmd = (cs_acp_cmd_t *)cmd->data;
  sl_status_t sc = SL_STATUS_OK;

  switch (cs_acp_cmd->id) {
#ifdef SL_CATALOG_CS_RREQ_PRESENT
    case CS_ACP_CMD_RREQ_CREATE:
    {
      cs_acp_cmd_rreq_create_t cmd = cs_acp_cmd->data.cs_acp_cmd_rreq_create;
      sc = cs_rreq_create(cmd.conn_handle, &cmd.config);

      // Send response to user command to NCP host.
      sl_bt_send_rsp_user_cs_service_message_to_target(sc, 0, NULL);
      break;
    }

    case CS_ACP_CMD_RREQ_ENABLE:
    {
      cs_acp_cmd_rreq_enable_t cmd = cs_acp_cmd->data.cs_acp_cmd_rreq_enable;
      sc = cs_rreq_enable(cmd.conn_handle, cmd.enable);

      // Send response to user command to NCP host.
      sl_bt_send_rsp_user_cs_service_message_to_target(sc, 0, NULL);
      break;
    }

    case CS_ACP_CMD_RREQ_REMOVE:
    {
      cs_acp_cmd_rreq_remove_t cmd = cs_acp_cmd->data.cs_acp_cmd_rreq_remove;
      sc = cs_rreq_remove(cmd.conn_handle);

      // Send response to user command to NCP host.
      sl_bt_send_rsp_user_cs_service_message_to_target(sc, 0, NULL);
      break;
    }
#endif // SL_CATALOG_CS_RREQ_PRESENT

#ifdef SL_CATALOG_CS_ALGO_PRESENT
    case CS_ACP_CMD_ALGO_CONFIGURE:
    {
      cs_acp_cmd_algo_configure_t cmd = cs_acp_cmd->data.cs_acp_cmd_algo_configure;

      // TODO

      // Send response to user command to NCP host.
      sl_bt_send_rsp_user_cs_service_message_to_target(sc, 0, NULL);
      break;
    }
#endif // SL_CATALOG_CS_ALGO_PRESENT

    // Unknown user command.
    default:
    {
      // Send error response to NCP host.
      sl_bt_send_rsp_user_cs_service_message_to_target(SL_STATUS_INVALID_PARAMETER, 0, NULL);
      break;
    }
  }
}
*/

// Sends the CS ACP host an event.
void cs_acp_send_evt(const cs_acp_evt_t *cs_acp_evt)
{
  uint8_t len = sizeof(cs_acp_evt_e);

  switch (cs_acp_evt->id) {
#ifdef SL_CATALOG_CS_RREQ_PRESENT
    case CS_ACP_EVT_RREQ_CREATE_COMPLETE:
      len += sizeof(cs_acp_evt_rreq_create_complete_t);
      break;

    case CS_ACP_EVT_RREQ_ENABLE_COMPLETE:
      len += sizeof(cs_acp_evt_rreq_enable_complete_t);
      break;

    case CS_ACP_EVT_RREQ_ERROR:
      len += sizeof(cs_acp_evt_rreq_error_t);
      break;
#endif // SL_CATALOG_CS_RREQ_PRESENT

#ifdef SL_CATALOG_CS_ALGO_PRESENT
    case CS_ACP_EVT_ALGO_CONFIG_STATUS:
      len += sizeof(cs_acp_evt_algo_config_status_t);
      break;

    case CS_ACP_EVT_ALGO_RESULT_ID:
      len += sizeof(cs_acp_evt_algo_result_id_t);
      break;

    case CS_ACP_EVT_ALGO_INTERMEDIATE_RESULT_ID:
      len += sizeof(cs_acp_evt_algo_intermediate_result_id_t);
      break;

    case CS_ACP_EVT_ALGO_EXTENDED_RESULT_ID:
      len += sizeof(cs_acp_evt_algo_extended_result_id_t);
      break;
#endif // SL_CATALOG_CS_ALGO_PRESENT

    default:
      break;
  }

  sl_bt_send_evt_user_cs_service_message_to_host(len, (uint8_t *)cs_acp_evt);
}
