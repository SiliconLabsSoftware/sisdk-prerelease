/***************************************************************************//**
 * @file
 * @brief CS NCP target core application logic.
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

#include "app.h"
#include "sli_bgapi_trace.h"
#include "iostream_bgapi_trace.h"
#include "sl_status.h"
#include "sl_main_init.h"
#include "app_config.h"
#include "app_log.h"
#include "cs_algo.h"
#include "cs_algo_rtl_log.h"
#include "cs_acp_common.h"
#include "cs_acp_target.h"

// -----------------------------------------------------------------------------
// Public function definitions

/*******************************************************************************
 * Application Init
 ******************************************************************************/
void app_init(void)
{
  app_log_iostream_set(iostream_bgapi_trace_handle);
  /////////////////////////////////////////////////////////////////////////////
  // Put your additional application init code here!                         //
  // This is called once during start-up.                                    //
  /////////////////////////////////////////////////////////////////////////////
}

/******************************************************************************
 * Application Process Action
 *****************************************************************************/
void app_process_action(void)
{
  if (false == app_is_process_required()) {
    return;
  }
  ///////////////////////////////////////////////////////////////////////////
  // Put your additional application code here!                            //
  // This will run each time app_proceed() is called.                      //
  // Do not call blocking functions from here!                             //
  ///////////////////////////////////////////////////////////////////////////
}

// Called when a CS command is received from the ACP host.
void app_on_acp_command(const void *data)
{
  sl_status_t sc;

  uint8array *data_arr = (uint8array *)data;
  cs_acp_cmd_t *cs_cmd = (cs_acp_cmd_t *)(data_arr->data);

  switch (cs_cmd->id) {
    case CS_ACP_CMD_ENABLE_TRACE:
      if (cs_cmd->data.cs_acp_cmd_enable_trace == 0) {
        cs_algo_rtl_log_deinit();
        sli_bgapi_trace_stop();
      } else {
        sli_bgapi_trace_start();
        cs_algo_rtl_log_init();
      }
      sc = SL_STATUS_OK;
      cs_acp_send_cmd_rsp(sc, 0, NULL);
      break;
    default:
      // The command is not related to this handler.
      return;
  }
}
