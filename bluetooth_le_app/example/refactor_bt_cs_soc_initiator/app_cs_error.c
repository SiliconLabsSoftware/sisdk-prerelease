/***************************************************************************//**
 * @file
 * @brief CS Initiator example CS error handling
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
#include <stdint.h>
#include <stdbool.h>
#include "sl_status.h"
#include "app.h"
#include "trace.h"
#include "cs_rreq.h"
#include "cs_algo.h"
#include "sl_bt_peer_manager_central.h"

// -----------------------------------------------------------------------------
// Static function declarations

static bool rreq_is_fatal(cs_rreq_error_t error);
static bool algo_is_fatal(cs_algo_error_t error);
static bool app_is_fatal(cs_app_error_t error);
static void report(uint8_t conn_handle,
                   cs_error_type_t type,
                   uint8_t error_code,
                   sl_status_t sc,
                   bool fatal);

// -----------------------------------------------------------------------------
// Public callbacks (registered with cs_rreq / cs_algo / used by app)

void app_on_cs_rreq_on_error(uint8_t conn_handle,
                             cs_rreq_error_t error,
                             sl_status_t sc)
{
  report(conn_handle,
         CS_ERROR_TYPE_RREQ,
         (uint8_t)error,
         sc,
         rreq_is_fatal(error));
}

void app_on_cs_algo_on_error(uint8_t conn_handle,
                             cs_algo_error_t error,
                             sl_status_t sc)
{
  report(conn_handle,
         CS_ERROR_TYPE_ALGO,
         (uint8_t)error,
         sc,
         algo_is_fatal(error));
}

void cs_on_error(uint8_t conn_handle,
                 cs_app_error_t error,
                 sl_status_t sc)
{
  report(conn_handle,
         CS_ERROR_TYPE_APP,
         (uint8_t)error,
         sc,
         app_is_fatal(error));
}

// -----------------------------------------------------------------------------
// Private functions

/// Non-fatal cs_rreq events. Anything not listed is fatal.
static bool rreq_is_fatal(cs_rreq_error_t error)
{
  switch (error) {
    case CS_RREQ_ERROR_RAS_CLIENT_REALTIME_RECEIVE_FAILED:
    case CS_RREQ_ERROR_RAS_CLIENT_DATA_RECEPTION_FINISH_FAILED:
    case CS_RREQ_ERROR_RAS_CLIENT_RANGING_DATA_READY_FAILED:
    case CS_RREQ_ERROR_RAS_CLIENT_GET_RANGING_DATA_FAILED:
    case CS_RREQ_ERROR_RAS_CLIENT_RANGING_DATA_OVERWRITTEN_FAILED:
    case CS_RREQ_ERROR_RAS_CLIENT_ABORT_FINISHED_FAILED:
    case CS_RREQ_ERROR_RAS_CLIENT_ACK_FAILED:
    case CS_RREQ_ERROR_RAS_CLIENT_REQUEST_LOST_SEGMENTS_FAILED:
    case CS_RREQ_ERROR_RAS_CLIENT_ON_ACK_FINISHED_FAILED:
      return false;
    default:
      return true;
  }
}

/// Fatal cs_algo events. Anything not listed is non-fatal.
static bool algo_is_fatal(cs_algo_error_t error)
{
  switch (error) {
    case CS_ALGO_ERROR_CREATE_FAILED:
    case CS_ALGO_ERROR_CONFIGURE_FAILED:
      return true;
    default:
      return false;
  }
}

/// Non-fatal app-level events. Anything not listed is fatal.
static bool app_is_fatal(cs_app_error_t error)
{
  switch (error) {
    case CS_APP_ERROR_ALGO_REMOVE_FAILED:
    case CS_APP_ERROR_CS_SYNC_PHY_NOT_SUPPORTED:
    case CS_APP_ERROR_PARAM_OPTIMIZATION_NOT_SUPPORTED:
    case CS_APP_ERROR_PARAM_OPTIMIZATION_INVALID_INPUT:
    case CS_APP_ERROR_TIMER_START_FAILED:
    case CS_APP_ERROR_RAS_DISCOVERY_NOT_COMPLETE:
      return false;
    default:
      return true;
  }
}

/// Common reporter function for all three error sources.

static void report(uint8_t conn_handle,
                   cs_error_type_t type,
                   uint8_t error_code,
                   sl_status_t sc,
                   bool fatal)
{
  static const char *const type_str[] = { "RREQ", "ALGO", "APP" };

  log_error(APP_INSTANCE_PREFIX "%s error (%u), status code (0x%04lx)" NL,
            conn_handle,
            type_str[type],
            (unsigned)error_code,
            (unsigned long)sc);

  if (fatal) {
    (void)sl_bt_peer_manager_central_close_connection(conn_handle);
  }
}
