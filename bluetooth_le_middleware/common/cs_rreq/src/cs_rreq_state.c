/***************************************************************************//**
 * @file
 * @brief CS RREQ - State Machine implementation
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

#include <stdint.h>
#include <stdbool.h>
#include "sl_status.h"
#include "cs_rreq.h"
#include "cs_rreq_state.h"
#include "cs_rreq_internal.h"
#include "cs_rreq_config.h"
#include "cs_rreq_log.h"

// -----------------------------------------------------------------------------
// Forward declaration of private functions

// Event handler functions
static sl_status_t state_disabled_on_enable(rreq_t *rreq,
                                            sm_evt_data_t *data);
static sl_status_t state_any_on_disable(rreq_t *rreq,
                                        sm_evt_data_t *data);
static sl_status_t state_enabling_on_enable_complete(rreq_t *rreq,
                                                     sm_evt_data_t *data);
static sl_status_t state_disabling_on_enable_complete(rreq_t *rreq,
                                                      sm_evt_data_t *data);
static sl_status_t state_wait_remote_on_ranging_data(rreq_t *rreq,
                                                     sm_evt_data_t *data,
                                                     bool local_complete);
static sl_status_t state_in_procedure_on_cs_result(rreq_t *rreq,
                                                   sm_evt_data_t *data);
static sl_status_t state_in_procedure_on_ranging_data(rreq_t *rreq,
                                                      sm_evt_data_t *data);
static sl_status_t state_init_on_init_completed(rreq_t *rreq,
                                                      sm_evt_data_t *data);
static sl_status_t state_uninitialized_on_init_started(rreq_t *rreq,
                                                       sm_evt_data_t *data);
static sl_status_t state_any_on_error(rreq_t *rreq, sm_evt_data_t *data);


// -----------------------------------------------------------------------------
// Module public functions

/******************************************************************************
 * RREQ state machine
 *****************************************************************************/
sl_status_t sm_on_evt(rreq_t *rreq, sm_evt_t event, sm_evt_data_t *data)
{
  sl_status_t sc = SL_STATUS_FAIL;

  if (rreq == NULL) {
    rreq_log_error(INSTANCE_PREFIX "null reference to RREQ instance!" LOG_NL,
                   (uint8_t)SL_BT_INVALID_CONNECTION_HANDLE);
    rreq_error(rreq,
               CS_RREQ_ERROR_STATE_MACHINE_FAILED,
               SL_STATUS_NULL_POINTER);
    return SL_STATUS_NULL_POINTER;
  }

  rreq_log_debug(INSTANCE_PREFIX "Event: %u [state: %u]" LOG_NL,
                 rreq->conn_handle,
                 event,
                 rreq->state);

  if (event == RREQ_EVT_ERROR) {
    sc = state_any_on_error(rreq, data);
  }

  if (event == RREQ_EVT_DISABLE) {
    sc = state_any_on_disable(rreq, data);
    return sc;
  }

  switch (rreq->state) {
    case RREQ_STATE_UNINITIALIZED:
      if (event == RREQ_EVT_INIT_STARTED) {
        sc = state_uninitialized_on_init_started(rreq, data);
      }
      break;

    case RREQ_STATE_INIT:
      if (event == RREQ_EVT_INIT_COMPLETED) {
        sc = state_init_on_init_completed(rreq, data);
      }
      break;

    case RREQ_STATE_DISABLED:
      if (event == RREQ_EVT_ENABLE) {
        sc = state_disabled_on_enable(rreq, data);
      }
      break;

    case RREQ_STATE_ENABLING:
      if (event == RREQ_EVT_ENABLE_COMPLETED) {
        sc = state_enabling_on_enable_complete(rreq, data);
      }
      break;

    case RREQ_STATE_DISABLING:
      if (event == RREQ_EVT_ENABLE_COMPLETED) {
        sc = state_disabling_on_enable_complete(rreq, data);
      }
      break;

    case RREQ_STATE_IN_PROCEDURE:
      if (event == RREQ_EVT_RANGING_DATA) {
        sc = state_in_procedure_on_ranging_data(rreq, data);
      }
      if ((event == RREQ_EVT_CS_RESULT)
          || (event == RREQ_EVT_CS_RESULT_CONTINUE)) {
        sc = state_in_procedure_on_cs_result(rreq, data);
      }
      break;

    case RREQ_STATE_WAIT_REMOTE_COMPLETE:
      if (event == RREQ_EVT_RANGING_DATA) {
        sc = state_wait_remote_on_ranging_data(rreq, data, true);
      }
      break;

    case RREQ_STATE_WAIT_REMOTE_ABORT:
      if (event == RREQ_EVT_RANGING_DATA) {
        sc = state_wait_remote_on_ranging_data(rreq, data, false);
      }
      break;
    case RREQ_STATE_ERROR:
      break;

    default:
      break;
  }
  return sc;
}

// -----------------------------------------------------------------------------
// Private functions

static sl_status_t state_any_on_error(rreq_t *rreq, sm_evt_data_t *data)
{
  rreq_log_error(INSTANCE_PREFIX "Instance new state: ERROR" LOG_NL, rreq->conn_handle);
  set_state(rreq, RREQ_STATE_ERROR);
  rreq_error(rreq,
             data->evt_error.error,
             data->evt_error.status);
  return SL_STATUS_OK;
}

static sl_status_t state_disabled_on_enable(rreq_t *rreq,
                                            sm_evt_data_t *data)
{
  (void)data;
  rreq_log_info(INSTANCE_PREFIX "Instance new state: ENABLING" LOG_NL,
                     rreq->conn_handle);
  set_state(rreq, RREQ_STATE_ENABLING);

  return SL_STATUS_OK;
}

static sl_status_t state_any_on_disable(rreq_t *rreq,
                                        sm_evt_data_t *data)
{
  (void)data;
  rreq_log_info(INSTANCE_PREFIX "Instance new state: DISABLED" LOG_NL,
                rreq->conn_handle);
  set_state(rreq, RREQ_STATE_DISABLED);

  return SL_STATUS_OK;
}

static sl_status_t state_enabling_on_enable_complete(rreq_t *rreq,
                                                     sm_evt_data_t *data)
{
  sl_status_t enable_status = data->evt_enable_completed.status;
  // Check result of enabling RAS Client
  if (enable_status == SL_STATUS_OK) {
    // Enable reception if RAS Real-Time mode is used
    if (rreq->config.real_time_mode) {
      rreq->ras_state = RAS_STATE_REAL_TIME;
      rreq_log_info(INSTANCE_PREFIX "Instance new state: IN_PROCEDURE" LOG_NL,
                    rreq->conn_handle);
      set_state(rreq, RREQ_STATE_IN_PROCEDURE);
    } else {
      rreq->ras_state = RAS_STATE_ON_DEMAND;
      rreq->ras_overwritten = false;
    }
  } else {
    rreq_log_info(INSTANCE_PREFIX "Instance new state: DISABLED" LOG_NL,
                  rreq->conn_handle);
    set_state(rreq, RREQ_STATE_DISABLED);
  }
  if (callback.on_enable != NULL) {
    callback.on_enable(rreq->conn_handle, true, enable_status);
  }
  return SL_STATUS_OK;
}

static sl_status_t state_disabling_on_enable_complete(rreq_t *rreq,
                                                      sm_evt_data_t *data)
{
  sl_status_t enable_status = data->evt_enable_completed.status;
  if (enable_status == SL_STATUS_OK) {
    rreq->ras_state = RAS_STATE_IDLE;
    set_state(rreq, RREQ_STATE_DISABLED);
    reset_subevent_data(rreq, false);
  }
  if (callback.on_enable != NULL) {
    callback.on_enable(rreq->conn_handle, false, enable_status);
  }
  return SL_STATUS_OK;
}

static sl_status_t state_uninitialized_on_init_started(rreq_t *rreq,
                                                       sm_evt_data_t *data)
{
  (void)data;
  rreq_log_info(INSTANCE_PREFIX "Instance new state: INIT" LOG_NL,
                rreq->conn_handle);
  set_state(rreq, RREQ_STATE_INIT);
  return SL_STATUS_OK;
}

static sl_status_t state_init_on_init_completed(rreq_t *rreq,
                                                sm_evt_data_t *data)
{
  set_state(rreq, RREQ_STATE_DISABLED);
  if (callback.on_create != NULL) {
    callback.on_create(rreq->conn_handle,
                       data->evt_init_completed);
  }
  return SL_STATUS_OK;
}

static sl_status_t state_in_procedure_on_ranging_data(rreq_t *rreq,
                                                      sm_evt_data_t *data)
{
  sl_status_t sc = SL_STATUS_FAIL;
  // Local data is expected first
  if (!data->evt_ranging_data.is_local) {
    rreq_log_info(INSTANCE_PREFIX "CS - ignoring remote ranging data %u because of the ongoing measurement" LOG_NL,
                  rreq->conn_handle,
                  data->evt_ranging_data.ranging_counter);
    return SL_STATUS_OK;
  }
  // Local data
  if (data->evt_ranging_data.procedure_state == CS_PROCEDURE_STATE_ABORTED) {
    rreq_log_info(INSTANCE_PREFIX "Instance new state: IN_PROCEDURE" LOG_NL,
                rreq->conn_handle);
    set_state(rreq, RREQ_STATE_IN_PROCEDURE);
    // Allow upcoming procedures
    reset_subevent_data(rreq, false);
    sc = SL_STATUS_OK;
  } else if (data->evt_ranging_data.procedure_state == CS_PROCEDURE_STATE_COMPLETED) {

    rreq_log_info(INSTANCE_PREFIX "Instance new state: WAIT_REMOTE_COMPLETE" LOG_NL,
                  rreq->conn_handle);
    set_state(rreq, RREQ_STATE_WAIT_REMOTE_COMPLETE);
    sc = SL_STATUS_OK;
  }
  return sc;
}

static sl_status_t state_in_procedure_on_cs_result(rreq_t *rreq,
                                                   sm_evt_data_t *data)
{
  sl_status_t sc = SL_STATUS_OK;
  cs_procedure_state_t procedure_state;
  sm_evt_data_t data_out;

  procedure_state = extract_cs_result_data(rreq, &data->evt_cs_result);

  // Stay in procedure state
  if (procedure_state == CS_PROCEDURE_STATE_IN_PROGRESS) {
    set_state(rreq, RREQ_STATE_IN_PROCEDURE);
    return sc;
  }

  if (procedure_state == CS_PROCEDURE_STATE_ABORTED) {
    rreq_log_info(INSTANCE_PREFIX "Local ranging data %u aborted" LOG_NL,
                  rreq->conn_handle,
                  rreq->ranging_counter);
  } else {
    rreq_log_info(INSTANCE_PREFIX "Local ranging data %u complete" LOG_NL,
                  rreq->conn_handle,
                  rreq->ranging_counter);
  }

  // Access ranging data based on the role
  cs_rreq_ranging_buffer_t *buffer = rreq->config.is_initiator
                                     ? &rreq->data.initiator
                                     : &rreq->data.reflector;

  // Pass a ranging data event
  data_out.evt_ranging_data.is_local = true;
  data_out.evt_ranging_data.ranging_counter = rreq->ranging_counter;
  data_out.evt_ranging_data.data = buffer->data;
  data_out.evt_ranging_data.data_size = buffer->data_size;
  data_out.evt_ranging_data.procedure_state = procedure_state;
  sc = sm_on_evt(rreq, RREQ_EVT_RANGING_DATA, &data_out);
  return sc;
}

static sl_status_t state_wait_remote_on_ranging_data(rreq_t *rreq,
                                                     sm_evt_data_t *data,
                                                     bool local_complete)
{
  sl_status_t sc = SL_STATUS_FAIL;

  if (data->evt_ranging_data.is_local) {
    rreq_log_info(INSTANCE_PREFIX "CS - ignoring local ranging data %u because %u is in progress" LOG_NL,
                       rreq->conn_handle,
                       data->evt_ranging_data.ranging_counter,
                       rreq->ranging_counter);
    return SL_STATUS_OK;
  }
  #if defined(CS_RREQ_CONFIG_LOG_DATA) && (CS_RREQ_CONFIG_LOG_DATA == 1)
  rreq_log_debug(INSTANCE_PREFIX "Remote Ranging Data %u ready" LOG_NL,
                 rreq->conn_handle,
                 data->evt_ranging_data.ranging_counter);
  rreq_log_hexdump_debug((data->evt_ranging_data.data),
                         (data->evt_ranging_data.data_size));
  rreq_log_append_debug(LOG_NL);
  #endif // defined(CS_RREQ_CONFIG_LOG_DATA) && (CS_RREQ_CONFIG_LOG_DATA == 1)
  if (data->evt_ranging_data.ranging_counter != rreq->ranging_counter) {
    rreq_log_info(INSTANCE_PREFIX "CS - ignoring remote ranging data %u because %u is in progress" LOG_NL,
                       rreq->conn_handle,
                       data->evt_ranging_data.ranging_counter,
                       rreq->ranging_counter);

    return SL_STATUS_OK;
  }
  rreq_log_info(INSTANCE_PREFIX "Instance new state: IN_PROCEDURE" LOG_NL,
                rreq->conn_handle);
  set_state(rreq, RREQ_STATE_IN_PROCEDURE);
  // Check complete of remote and local data
  if ((data->evt_ranging_data.procedure_state == CS_PROCEDURE_STATE_COMPLETED)
      && local_complete) {
    cs_rreq_dispatch_ras_data(rreq->conn_handle,
                              rreq->ranging_counter,
                              rreq->procedure_info,
                              &rreq->data);

    if (send_result != NULL) {
      send_result(rreq->conn_handle,
                  rreq->ranging_counter,
                  rreq->procedure_info,
                  &rreq->data);
    }
  } else {
    rreq_log_info(INSTANCE_PREFIX "Procedure not completed: %u" LOG_NL,
                  rreq->conn_handle,
                  data->evt_ranging_data.ranging_counter);
    // Procedure data can be cleared
    reset_subevent_data(rreq, false);
  }
  sc = SL_STATUS_OK;
  return sc;
}

void set_state(rreq_t *rreq, rreq_state_t state)
{
  rreq_state_t old_state = rreq->state;
  rreq->state = state;
  rreq_log_debug(INSTANCE_PREFIX "State change: %u -> %u" LOG_NL,
                 rreq->conn_handle,
                 old_state,
                 state);
}