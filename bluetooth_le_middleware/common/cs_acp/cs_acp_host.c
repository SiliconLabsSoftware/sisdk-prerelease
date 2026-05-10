/***************************************************************************//**
 * @file
 * @brief CS Application Co-Processor host - core implementation
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
#include "sl_component_catalog.h"
#include "ncp_host.h"
#include "cs_acp_common.h"
#include "cs_acp_host.h"

// Sends the CS ACP target a command.
sl_status_t cs_acp_send_cmd(const cs_acp_cmd_t *cs_acp_cmd,
                            size_t max_response_size,
                            size_t *response_len,
                            uint8_t *response)
{
  uint8_t len = sizeof(cs_acp_cmd_e);

  switch (cs_acp_cmd->id) {
#ifdef SL_CATALOG_CS_RREQ_PRESENT
    case CS_ACP_CMD_RREQ_CREATE:
      len += sizeof(cs_acp_cmd_rreq_create_t);
      break;

    case CS_ACP_CMD_RREQ_ENABLE:
      len += sizeof(cs_acp_cmd_rreq_enable_t);
      break;

    case CS_ACP_CMD_RREQ_REMOVE:
      len += sizeof(cs_acp_cmd_rreq_remove_t);
      break;
#endif // SL_CATALOG_CS_RREQ_PRESENT

#ifdef SL_CATALOG_CS_ALGO_PRESENT
    case CS_ACP_CMD_ALGO_CONFIGURE:
      len += sizeof(cs_acp_cmd_algo_configure_t);
      break;
#endif // SL_CATALOG_CS_ALGO_PRESENT

    default:
      break;
  }


  return sl_bt_user_cs_service_message_to_target(len,
                                                 (uint8_t *)cs_acp_cmd,
                                                 max_response_size,
                                                 response_len,
                                                 response);
}
