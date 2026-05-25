/***************************************************************************//**
 * @file
 * @brief CS Manager - CS Procedure control implementation
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
#include "sl_common.h"
#include "cs_manager.h"
#include "cs_manager_internal.h"
#include "cs_manager_config.h"
#include "cs_manager_config_db_internal.h"
#include "cs_manager_log_internal.h"
#include "cs_manager_cs_control_config.h"

// -----------------------------------------------------------------------------
// Forward declaration of private functions

static void handle_cs_procedure_enable_completed(const sl_bt_evt_cs_procedure_enable_complete_t *evt);

// -----------------------------------------------------------------------------
// Public functions

// Start procedure using a config and parameters
sl_status_t cs_manager_cs_control_start(uint8_t conn_handle,
                                        uint8_t config_id,
                                        cs_procedure_parameters_t *params)
{
  sl_status_t sc;

  // Check handle
  if (conn_handle == SL_BT_INVALID_CONNECTION_HANDLE) {
    return SL_STATUS_INVALID_HANDLE;
  }

  // Check pointer
  if (params == NULL) {
    return SL_STATUS_NULL_POINTER;
  }

  // Check Connection
  cs_manager_t *m = cs_manager_find(conn_handle);
  if (m == NULL) {
    return SL_STATUS_NOT_FOUND;
  }

  // Check state
  if (m->state != CS_MANAGER_STATE_IDLE) {
    return SL_STATUS_INVALID_STATE;
  }
  // Check if CS configuration exists in config DB
  cs_config_data_t config;
  sc = cs_manager_config_db_get(conn_handle, config_id, &config);
  if (sc != SL_STATUS_OK) {
    return sc;
  }
  // If security is not enabled, enable it first
  if (!m->security_enabled) { 
    // Procedure requires security to be enabled
    sc = sl_bt_cs_security_enable(conn_handle);
    if (sc != SL_STATUS_OK) {
      return sc;
    }
    cs_manager_log_info(INSTANCE_PREFIX "Enabling CS security before procedure" NL, conn_handle);
    m->state = CS_MANAGER_STATE_PROCEDURE_ENABLING_SECURITY;
    m->procedure_parameters = *params;
    m->active_config_id = config_id;
    return sc;
  }

  sc = sl_bt_cs_set_procedure_parameters(conn_handle,
                                         config_id,
                                         params->max_procedure_len,
                                         params->min_procedure_interval,
                                         params->max_procedure_interval,
                                         params->max_procedure_count,
                                         params->min_subevent_len,
                                         params->max_subevent_len,
                                         params->tone_antenna_config_selection,
                                         m->instance_config.conn_phy,
                                         params->tx_pwr_delta,
                                         params->preferred_peer_antenna,
                                         params->snr_control_initiator,
                                         params->snr_control_reflector);
  if (sc != SL_STATUS_OK) {
    return sc;
  }
  sc = sl_bt_cs_procedure_enable(conn_handle,
                                 sl_bt_cs_procedure_state_enabled,
                                 config_id);
  if (sc != SL_STATUS_OK) {
    return sc;
  }
  // Set state to PROCEDURE_ENABLING
  m->state = CS_MANAGER_STATE_PROCEDURE_ENABLING;
  m->active_config_id = config_id;
  m->procedure_parameters = *params;

  cs_manager_log_info(INSTANCE_PREFIX " Enabling CS procedure" NL,
                      conn_handle);
  return sc;
}

// Stop measurement
sl_status_t cs_manager_cs_control_stop(uint8_t conn_handle)
{
  sl_status_t sc;

  // Check handle
  if (conn_handle == SL_BT_INVALID_CONNECTION_HANDLE) {
    return SL_STATUS_INVALID_HANDLE;
  }

  // Check Connection
  cs_manager_t *m = cs_manager_find(conn_handle);
  if (m == NULL) {
    return SL_STATUS_NOT_FOUND;
  }

  // Check state
  if (m->state != CS_MANAGER_STATE_PROCEDURE_ENABLED) { 
    return SL_STATUS_INVALID_STATE;
  }

  sc = sl_bt_cs_procedure_enable(conn_handle,
                                 sl_bt_cs_procedure_state_disabled,
                                 m->active_config_id);
  if (sc != SL_STATUS_OK) { 
    return sc;
  }
  // Set state to PROCEDURE_DISABLING
  m->state = CS_MANAGER_STATE_PROCEDURE_DISABLING;

  cs_manager_log_info(INSTANCE_PREFIX " Disabling CS procedure" NL,
                      conn_handle);

  return sc;
}

sl_status_t cs_manager_cs_control_get_default_procedure_parameters(cs_procedure_parameters_t *params)
{
  if (params == NULL) {
    return SL_STATUS_NULL_POINTER;
  }
  params->max_procedure_len = CS_MANAGER_DEFAULT_MAX_PROCEDURE_DURATION;
  params->min_procedure_interval = CS_MANAGER_DEFAULT_MIN_PROCEDURE_INTERVAL;
  params->max_procedure_interval = CS_MANAGER_DEFAULT_MAX_PROCEDURE_INTERVAL;
  params->max_procedure_count = CS_MANAGER_DEFAULT_MAX_PROCEDURE_COUNT;
  params->min_subevent_len = CS_MANAGER_DEFAULT_MIN_SUBEVENT_LEN;
  params->max_subevent_len = CS_MANAGER_DEFAULT_MAX_SUBEVENT_LEN;
  params->tone_antenna_config_selection = CS_MANAGER_DEFAULT_CS_TONE_ANTENNA_CONFIG_IDX;
  params->tx_pwr_delta = CS_MANAGER_DEFAULT_TX_PWR_DELTA;
  params->preferred_peer_antenna = CS_MANAGER_DEFAULT_PREFERRED_PEER_ANTENNA;
  params->snr_control_initiator = CS_MANAGER_DEFAULT_SNR_CONTROL_INITIATOR;
  params->snr_control_reflector = CS_MANAGER_DEFAULT_SNR_CONTROL_REFLECTOR;
  return SL_STATUS_OK;
}

void cs_manager_cs_control_on_bt_event(const sl_bt_msg_t *evt)
{
  switch (SL_BT_MSG_ID(evt->header)) {
    case sl_bt_evt_cs_procedure_enable_complete_id: {
      handle_cs_procedure_enable_completed(&evt->data.evt_cs_procedure_enable_complete);
      break;
    }
  }
}

// -----------------------------------------------------------------------------
// Private functions

static void handle_cs_procedure_enable_completed(const sl_bt_evt_cs_procedure_enable_complete_t *evt)
{
  cs_manager_t *m = cs_manager_find(evt->connection);
  if (m == NULL) {
    cs_manager_log_error("Procedure enable completed for unknown connection %u" NL,
                        evt->connection);
    return;
  }
  if (evt->status != SL_STATUS_OK) {
    cs_manager_log_error("CS procedure enable failed, status = 0x%04x" NL,
                        evt->status);
    if (m->state == CS_MANAGER_STATE_PROCEDURE_ENABLING) {
      m->state = CS_MANAGER_STATE_IDLE;
      on_event(m->conn_handle,
               m->active_config_id,
               CS_MANAGER_EVENT_PROCEDURE_START_COMPLETE, 
               evt->status);
    } else if (m->state == CS_MANAGER_STATE_PROCEDURE_DISABLING) {
      on_event(m->conn_handle,
               m->active_config_id,
               CS_MANAGER_EVENT_PROCEDURE_STOP_COMPLETE, 
               evt->status);
    }
    return;
  }
  if (evt->state == sl_bt_cs_procedure_state_enabled) {
    if (m->state == CS_MANAGER_STATE_PROCEDURE_ENABLING) {
      m->state = CS_MANAGER_STATE_PROCEDURE_ENABLED;
    }
    cs_manager_log_info(INSTANCE_PREFIX " CS procedure enabled" NL,
                        m->conn_handle);
    on_event(m->conn_handle,
             m->active_config_id,
             CS_MANAGER_EVENT_PROCEDURE_START_COMPLETE, 
             SL_STATUS_OK);
  } else if (evt->state == sl_bt_cs_procedure_state_disabled) {
    if (m->state == CS_MANAGER_STATE_PROCEDURE_ENABLING) {
      m->state = CS_MANAGER_STATE_IDLE;
    }
    cs_manager_log_info(INSTANCE_PREFIX " CS procedure disabled" NL,
                        m->conn_handle);
    on_event(m->conn_handle,
             m->active_config_id,
             CS_MANAGER_EVENT_PROCEDURE_STOP_COMPLETE, 
             SL_STATUS_OK);
  }
}