/***************************************************************************//**
 * @file
 * @brief CS initiator - state machine implementation
 *******************************************************************************
 * # License
 * <b>Copyright 2025 Silicon Laboratories Inc. www.silabs.com</b>
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
#include "sl_bt_api.h"
#include "sl_component_catalog.h"
#include "app_timer.h"

#include "cs_initiator_common.h"
#include "cs_initiator.h"
#include "cs_initiator_config.h"
#include "cs_initiator_error.h"
#include "cs_initiator_log.h"
#include "cs_algo.h"

#ifdef SL_CATALOG_CS_INITIATOR_REPORT_PRESENT
#include "cs_initiator_report.h"
#else
#define cs_initiator_report(evt)
#endif // SL_CATALOG_CS_INITIATOR_REPORT_PRESENT

#include "cs_initiator_state_machine.h"

// -----------------------------------------------------------------------------
// Macros

// Procedure enable maximum retry count
#define CS_MAX_PROCEDURE_ENABLE_RETRY  3

// -----------------------------------------------------------------------------
// Static function declarations

static sl_status_t state_any_on_error(cs_initiator_t             *initiator,
                                      state_machine_event_data_t *data);
static sl_status_t state_any_on_delete_instance(cs_initiator_t             *initiator,
                                                state_machine_event_data_t *data);
static sl_status_t state_uninitialized_on_init_started(cs_initiator_t             *initiator,
                                                       state_machine_event_data_t *data);
static sl_status_t state_init_on_start_init_completed(cs_initiator_t             *initiator,
                                                      state_machine_event_data_t *data);
static sl_status_t state_start_procedure_on_start_procedure(cs_initiator_t             *initiator,
                                                            state_machine_event_data_t *data);
static sl_status_t state_wait_procedure_enable_complete_on_enable_completed(cs_initiator_t             *initiator,
                                                                            state_machine_event_data_t *data);
static sl_status_t state_wait_procedure_disable_on_procedure_enable_completed(cs_initiator_t             *initiator,
                                                                              state_machine_event_data_t *data);
static sl_status_t state_delete_on_procedure_enable_completed(cs_initiator_t             *initiator,
                                                              state_machine_event_data_t *data);
static void handle_procedure_enable_completed_event_disable(cs_initiator_t *initiator);
static sl_status_t initiator_finalize_cleanup(cs_initiator_t *initiator);

// -----------------------------------------------------------------------------
// Static function definitions

static sl_status_t state_any_on_error(cs_initiator_t             *initiator,
                                      state_machine_event_data_t *data)
{
  app_timer_stop(&initiator->timer_handle);
  initiator_log_error(INSTANCE_PREFIX "Instance new state: ERROR" LOG_NL, initiator->conn_handle);
  initiator->initiator_state = (uint8_t)INITIATOR_STATE_ERROR;
  on_error(initiator,
           data->evt_error.error_type,
           data->evt_error.sc);
  return SL_STATUS_OK;
}

static sl_status_t state_any_on_delete_instance(cs_initiator_t             *initiator,
                                                state_machine_event_data_t *data)
{
  (void)data;
  sl_status_t sc = SL_STATUS_FAIL;
  state_machine_event_data_t data_out;

  sc = sl_bt_cs_procedure_enable(initiator->conn_handle,
                                 sl_bt_cs_procedure_state_disabled,
                                 initiator->config.config_id);

  if (sc == SL_STATUS_OK) {
    initiator_log_info(INSTANCE_PREFIX "Instance new state: DELETING" LOG_NL,
                       initiator->conn_handle);
    initiator->initiator_state = (uint8_t)INITIATOR_STATE_DELETE;
  } else if (sc == SL_STATUS_BT_CTRL_COMMAND_DISALLOWED
             || sc == SL_STATUS_BT_CTRL_INVALID_COMMAND_PARAMETERS
             || sc == SL_STATUS_INVALID_HANDLE) {
    // Procedure is already stopped by stack
    app_timer_stop(&initiator->timer_handle);
    sc = initiator_finalize_cleanup(initiator);
    if (sc == SL_STATUS_OK) {
      initiator_log_info(INSTANCE_PREFIX "Instance new state: UNINITIALIZED" LOG_NL,
                         initiator->conn_handle);
      initiator->initiator_state = (uint8_t)INITIATOR_STATE_UNINITIALIZED;
      return SL_STATUS_OK;
    } else {
      initiator->initiator_state = (uint8_t)INITIATOR_STATE_ERROR;
      data_out.evt_error.error_type = CS_ERROR_EVENT_INITIATOR_FAILED_TO_FINALIZE_CLEANUP;
      data_out.evt_error.sc = sc;
      sc = initiator_state_machine_event_handler(initiator,
                                                 INITIATOR_EVT_ERROR,
                                                 &data_out);
      return sc;
    }
  } else {
    // no other error code will be returned for now
    // if there will be one in the future it will generate an error state
    initiator_log_error(INSTANCE_PREFIX "failed to stop CS procedure! [sc: 0x%lx]" LOG_NL,
                        initiator->conn_handle,
                        (unsigned long)sc);
    initiator->initiator_state = (uint8_t)INITIATOR_STATE_ERROR;
    data_out.evt_error.error_type = CS_ERROR_EVENT_CS_PROCEDURE_STOP_FAILED;
    data_out.evt_error.sc = sc;
    sc = initiator_state_machine_event_handler(initiator,
                                               INITIATOR_EVT_ERROR,
                                               &data_out);
  }

  return sc;
}

static sl_status_t state_uninitialized_on_init_started(cs_initiator_t             *initiator,
                                                       state_machine_event_data_t *data)
{
  (void)data;
  initiator_log_info(INSTANCE_PREFIX "Instance new state: INIT" LOG_NL,
                     initiator->conn_handle);
  initiator->initiator_state = (uint8_t)INITIATOR_STATE_INIT;

  return SL_STATUS_OK;
}

static sl_status_t state_init_on_start_init_completed(cs_initiator_t             *initiator,
                                                      state_machine_event_data_t *data)
{
  sl_status_t sc = SL_STATUS_FAIL;
  state_machine_event_data_t data_out;

  if (data->evt_init_completed) {
    initiator_log_info(INSTANCE_PREFIX "Instance new state: START_PROCEDURE" LOG_NL,
                       initiator->conn_handle);
    initiator->initiator_state = (uint8_t)INITIATOR_STATE_START_PROCEDURE;
    initiator->procedure_enable_retry_counter = 0;
    sc = initiator_state_machine_event_handler(initiator,
                                               INITIATOR_EVT_START_PROCEDURE,
                                               NULL);
  } else {
    initiator->initiator_state = (uint8_t)INITIATOR_STATE_ERROR;
    data_out.evt_error.error_type = CS_ERROR_EVENT_INIT_FAILED;
    data_out.evt_error.sc = SL_STATUS_FAIL;
    sc = initiator_state_machine_event_handler(initiator,
                                               INITIATOR_EVT_ERROR,
                                               &data_out);
  }

  return sc;
}

static sl_status_t state_start_procedure_on_start_procedure(cs_initiator_t             *initiator,
                                                            state_machine_event_data_t *data)
{
  (void)data;
  sl_status_t sc = SL_STATUS_FAIL;
  state_machine_event_data_t data_out;

  initiator_log_debug(INSTANCE_PREFIX "CS procedure - request to start." LOG_NL,
                      initiator->conn_handle);
  // Before enabling procedure, check the security state
  if (!initiator->cs_security_enabled) {
    // Security is not enabled, move to error state
    initiator->initiator_state = (uint8_t)INITIATOR_STATE_ERROR;
    data_out.evt_error.error_type = CS_ERROR_EVENT_INITIATOR_FAILED_TO_ENABLE_CS_SECURITY;
    data_out.evt_error.sc = sc;
    sc = initiator_state_machine_event_handler(initiator,
                                               INITIATOR_EVT_ERROR,
                                               &data_out);
    return sc;
  }
  cs_initiator_report(CS_INITIATOR_REPORT_CS_PROCEDURE_BEGIN);
  sc = sl_bt_cs_procedure_enable(initiator->conn_handle,
                                 sl_bt_cs_procedure_state_enabled,
                                 initiator->config.config_id);

  if (sc == SL_STATUS_OK) {
    initiator_log_info(INSTANCE_PREFIX "Instance new state: WAIT_PROCEDURE_ENABLE_COMPLETE" LOG_NL,
                        initiator->conn_handle);
    initiator->initiator_state = (uint8_t)INITIATOR_STATE_WAIT_PROCEDURE_ENABLE_COMPLETE;
    sc = SL_STATUS_OK;
  } else {
    initiator->initiator_state = (uint8_t)INITIATOR_STATE_ERROR;
    data_out.evt_error.error_type = CS_ERROR_EVENT_CS_PROCEDURE_START_FAILED;
    data_out.evt_error.sc = sc;
    sc = initiator_state_machine_event_handler(initiator,
                                               INITIATOR_EVT_ERROR,
                                               &data_out);
  }

  return sc;
}

static sl_status_t state_wait_procedure_enable_complete_on_enable_completed(cs_initiator_t             *initiator,
                                                                            state_machine_event_data_t *data)
{
  sl_status_t sc = SL_STATUS_FAIL;
  state_machine_event_data_t data_out;

  if (data->evt_procedure_enable_completed->status == SL_STATUS_OK) {
    cs_initiator_report(CS_INITIATOR_REPORT_CS_PROCEDURE_STARTED);
    uint32_t time_tick = sl_sleeptimer_get_tick_count();
    initiator->procedure_start_time_ms =
      sl_sleeptimer_tick_to_ms(time_tick);

    initiator->cs_procedure_config.subevent_len =
      data->evt_procedure_enable_completed->subevent_len;
    initiator->cs_procedure_config.subevents_per_event =
      data->evt_procedure_enable_completed->subevents_per_event;
    initiator->cs_procedure_config.subevent_interval =
      data->evt_procedure_enable_completed->subevent_interval;
    initiator->cs_procedure_config.event_interval =
      data->evt_procedure_enable_completed->event_interval;
    initiator->cs_procedure_config.procedure_interval =
      data->evt_procedure_enable_completed->procedure_interval;
    initiator->cs_procedure_config.procedure_count =
      data->evt_procedure_enable_completed->procedure_count;

    initiator_log_info(INSTANCE_PREFIX "-------------------------------------------" LOG_NL,
                       initiator->conn_handle);
    initiator_log_info(INSTANCE_PREFIX "CS procedure - started @ %lu ms (%lu tick) "
                                       "subevent_len: %lu, "
                                       "subevent_interval: %u, "
                                       "subevents_per_event: %u, "
                                       "event_interval: %u, "
                                       "procedure_interval: %u, "
                                       "procedure_count: %u" LOG_NL,
                       initiator->conn_handle,
                       (unsigned long)initiator->procedure_start_time_ms,
                       (unsigned long)time_tick,
                       initiator->cs_procedure_config.subevent_len,
                       initiator->cs_procedure_config.subevent_interval,
                       initiator->cs_procedure_config.subevents_per_event,
                       initiator->cs_procedure_config.event_interval,
                       initiator->cs_procedure_config.procedure_interval,
                       initiator->cs_procedure_config.procedure_count);

    initiator_log_info(INSTANCE_PREFIX "Instance new state: IN_PROCEDURE" LOG_NL,
                       initiator->conn_handle);
    // Start with a clean slate
    initiator->initiator_state = (uint8_t)INITIATOR_STATE_IN_PROCEDURE;
    sc = SL_STATUS_OK;
  } else {
    initiator_log_error(INSTANCE_PREFIX "CS procedure - start received error response! [status: 0x%x]" LOG_NL,
                        initiator->conn_handle,
                        data->evt_procedure_enable_completed->status);
    initiator->procedure_enable_retry_counter++;
    if (initiator->procedure_enable_retry_counter < CS_MAX_PROCEDURE_ENABLE_RETRY ) {
      initiator_log_error(INSTANCE_PREFIX "CS procedure - setting procedure parameters again..." LOG_NL,
                          initiator->conn_handle);
      sc = sl_bt_cs_set_procedure_parameters(initiator->conn_handle,
                                             initiator->config.config_id,
                                             initiator->config.max_procedure_duration,
                                             initiator->config.min_procedure_interval,
                                             initiator->config.max_procedure_interval,
                                             initiator->config.max_procedure_count,
                                             initiator->config.min_subevent_len,
                                             initiator->config.max_subevent_len,
                                             initiator->config.cs_tone_antenna_config_idx,
                                             initiator->config.conn_phy,
                                             initiator->config.tx_pwr_delta,
                                             initiator->config.preferred_peer_antenna,
                                             initiator->config.snr_control_initiator,
                                             initiator->config.snr_control_reflector);

      if (sc != SL_STATUS_OK) {
        initiator_log_error(INSTANCE_PREFIX "CS procedure - failed to set parameters! "
                                            "[sc: 0x%lx]" LOG_NL,
                            initiator->conn_handle,
                            (unsigned long)sc);
        on_error(initiator,
                 CS_ERROR_EVENT_CS_SET_PROCEDURE_PARAMETERS_FAILED,
                 sc);
        return sc;
      }
      initiator_log_info(INSTANCE_PREFIX "Instance new state: START_PROCEDURE" LOG_NL,
                         initiator->conn_handle);
      initiator->initiator_state = (uint8_t)INITIATOR_STATE_START_PROCEDURE;
      sc = initiator_state_machine_event_handler(initiator,
                                                 INITIATOR_EVT_START_PROCEDURE,
                                                 NULL);
      return sc;
    }
    app_timer_stop(&initiator->timer_handle);
    initiator->initiator_state = (uint8_t)INITIATOR_STATE_ERROR;
    data_out.evt_error.error_type = CS_ERROR_EVENT_CS_PROCEDURE_ENABLE_FAILED;
    data_out.evt_error.sc = SL_STATUS_FAIL;
    sc = initiator_state_machine_event_handler(initiator,
                                               INITIATOR_EVT_ERROR,
                                               &data_out);
  }

  return sc;
}

static sl_status_t state_wait_procedure_disable_on_procedure_enable_completed(cs_initiator_t             *initiator,
                                                                              state_machine_event_data_t *data)
{
  sl_status_t sc = SL_STATUS_FAIL;
  state_machine_event_data_t data_out;
  if (data->evt_procedure_enable_completed->state) {
    return SL_STATUS_OK;
  }
  handle_procedure_enable_completed_event_disable(initiator);
  if (data->evt_procedure_enable_completed->status == SL_STATUS_OK) {
    app_timer_stop(&initiator->timer_handle);
    initiator_log_info(INSTANCE_PREFIX "Instance new state: START_PROCEDURE" LOG_NL,
                       initiator->conn_handle);
    initiator->initiator_state = (uint8_t)INITIATOR_STATE_START_PROCEDURE;
    sc = initiator_state_machine_event_handler(initiator,
                                               INITIATOR_EVT_START_PROCEDURE,
                                               NULL);
    return sc;
  }
  initiator_log_error(INSTANCE_PREFIX "CS procedure - stop received error response! [status: 0x%x]" LOG_NL,
                      initiator->conn_handle,
                      data->evt_procedure_enable_completed->status);
  initiator->initiator_state = (uint8_t)INITIATOR_STATE_ERROR;
  data_out.evt_error.error_type = CS_ERROR_EVENT_CS_PROCEDURE_STOP_FAILED;
  data_out.evt_error.sc = data->evt_procedure_enable_completed->status;
  sc = initiator_state_machine_event_handler(initiator,
                                             INITIATOR_EVT_ERROR,
                                             &data_out);
  return sc;
}

static sl_status_t state_delete_on_procedure_enable_completed(cs_initiator_t             *initiator,
                                                              state_machine_event_data_t *data)
{
  sl_status_t sc = SL_STATUS_FAIL;
  state_machine_event_data_t data_out;
  if (data->evt_procedure_enable_completed->state) {
    return SL_STATUS_OK;
  }
  if (data->evt_procedure_enable_completed->status == SL_STATUS_OK) {
    // Procedure was disabled
    handle_procedure_enable_completed_event_disable(initiator);
    sc = initiator_finalize_cleanup(initiator);
    if (sc == SL_STATUS_OK) {
      initiator_log_info(INSTANCE_PREFIX "Instance new state: UNINITIALIZED" LOG_NL,
                         initiator->conn_handle);
      initiator->initiator_state = (uint8_t)INITIATOR_STATE_UNINITIALIZED;
      return sc;
    }
    initiator->initiator_state = (uint8_t)INITIATOR_STATE_ERROR;
    data_out.evt_error.error_type = CS_ERROR_EVENT_INITIATOR_FAILED_TO_FINALIZE_CLEANUP;
    data_out.evt_error.sc = sc;
    sc = initiator_state_machine_event_handler(initiator,
                                               INITIATOR_EVT_ERROR,
                                               &data_out);
    return sc;
  }
  // Only happens if the remote device rejects the procedure termination - error state
  initiator_log_error(INSTANCE_PREFIX "CS procedure - stop received error response! [status: 0x%x]" LOG_NL,
                      initiator->conn_handle,
                      data->evt_procedure_enable_completed->status);
  initiator->initiator_state = (uint8_t)INITIATOR_STATE_ERROR;
  data_out.evt_error.error_type = CS_ERROR_EVENT_CS_PROCEDURE_STOP_FAILED;
  data_out.evt_error.sc = data->evt_procedure_enable_completed->status;
  sc = initiator_state_machine_event_handler(initiator,
                                             INITIATOR_EVT_ERROR,
                                             &data_out);
  return sc;
}

/******************************************************************************
 * Handle procedure_enable_completed event when procedure was
 * disabled successfully.
 * @param[in] initiator initiator instance
 *****************************************************************************/
static void handle_procedure_enable_completed_event_disable(cs_initiator_t *initiator)
{
  uint32_t time_tick = sl_sleeptimer_get_tick_count();
  initiator->procedure_stop_time_ms =
    sl_sleeptimer_tick_to_ms(time_tick);
  initiator_log_info(INSTANCE_PREFIX "CS procedure - stopped @ %lu ms "
                                     "(%lu tick)" LOG_NL,
                     initiator->conn_handle,
                     (unsigned long)initiator->procedure_stop_time_ms,
                     (unsigned long)time_tick);
}

/******************************************************************************
 * Initiator finalize cleanup. Remove the configuration and deinit RTL lib.
 * This function is called after the procedure was stopped.
 * @param[in] initiator pointer to the initiator instance.
 * @return SL_STATUS_OK if cleanup was successful.
 * @return SL_STATUS_FAIL if cleanup failed.
 *****************************************************************************/
static sl_status_t initiator_finalize_cleanup(cs_initiator_t *initiator)
{
  sl_status_t sc = SL_STATUS_OK;
  (void)sl_bt_cs_remove_config(initiator->conn_handle, initiator->config.config_id);
  
  sl_status_t algo_sc = cs_algo_remove(initiator->conn_handle);
  if (algo_sc != SL_STATUS_OK && algo_sc != SL_STATUS_NOT_FOUND) {
    initiator_log_error(INSTANCE_PREFIX
                        "cs_algo_remove failed! [sc: 0x%lx]" LOG_NL,
                        initiator->conn_handle,
                        (unsigned long)algo_sc);
    sc = SL_STATUS_FAIL;
  }

  initiator_log_debug(INSTANCE_PREFIX "deleting instance" LOG_NL,
                      initiator->conn_handle);

  memset(initiator, 0, sizeof(cs_initiator_t));
  initiator_log_info(INSTANCE_PREFIX "instance deleted" LOG_NL, initiator->conn_handle);
  initiator->conn_handle = SL_BT_INVALID_CONNECTION_HANDLE;

  initiator->ras_client.real_time_mode = CS_INITIATOR_RAS_MODE_USE_REAL_TIME_MODE;

  initiator->ras_client.service = 0xFFFFFFFF;
  initiator->ras_client.mtu = ATT_MTU_MIN;

  initiator->ras_client.config.real_time_ranging_data_indication =
    CS_INITIATOR_RAS_REAL_TIME_INDICATION;
  initiator->ras_client.config.on_demand_ranging_data_indication =
    CS_INITIATOR_RAS_ON_DEMAND_INDICATION;
  initiator->ras_client.config.ranging_data_ready_notification =
    CS_INITIATOR_RAS_DATA_READY_NOTIFICATION;
  initiator->ras_client.config.ranging_data_overwritten_notification =
    CS_INITIATOR_RAS_DATA_OVERWRITTEN_NOTIFICATION;

  return sc;
}

// -----------------------------------------------------------------------------
// Event / callback definitions

/******************************************************************************
 * Initiator state machine
 *****************************************************************************/
sl_status_t initiator_state_machine_event_handler(cs_initiator_t *initiator,
                                                  state_machine_event_t      event,
                                                  state_machine_event_data_t *data)
{
  sl_status_t sc = SL_STATUS_FAIL;

  if (initiator == NULL) {
    initiator_log_error(INSTANCE_PREFIX "null reference to initiator instance!" LOG_NL,
                        (uint8_t)SL_BT_INVALID_CONNECTION_HANDLE);
    on_error(initiator,
             CS_ERROR_EVENT_STATE_MACHINE_FAILED,
             SL_STATUS_NULL_POINTER);
    return SL_STATUS_NULL_POINTER;
  }

  if (event == INITIATOR_EVT_ERROR) {
    sc = state_any_on_error(initiator, data);
  }

  if (event == INITIATOR_EVT_DELETE_INSTANCE) {
    sc = state_any_on_delete_instance(initiator, data);
    return sc;
  }

  switch (initiator->initiator_state) {
    case INITIATOR_STATE_UNINITIALIZED:
      if (event == INITIATOR_EVT_INIT_STARTED) {
        sc = state_uninitialized_on_init_started(initiator, data);
      }
      break;

    case INITIATOR_STATE_INIT:
      if (event == INITIATOR_EVT_INIT_COMPLETED) {
        sc = state_init_on_start_init_completed(initiator, data);
      }
      break;

    case INITIATOR_STATE_START_PROCEDURE:
      if (event == INITIATOR_EVT_START_PROCEDURE) {
        sc = state_start_procedure_on_start_procedure(initiator, data);
      }
      break;

    case INITIATOR_STATE_WAIT_PROCEDURE_ENABLE_COMPLETE:
      if (event == INITIATOR_EVT_PROCEDURE_ENABLE_COMPLETED) {
        sc = state_wait_procedure_enable_complete_on_enable_completed(initiator,
                                                                      data);
      }
      break;

    case INITIATOR_STATE_IN_PROCEDURE:
      break;

    case INITIATOR_STATE_WAIT_PROCEDURE_DISABLE_COMPLETE:
      if (event == INITIATOR_EVT_PROCEDURE_ENABLE_COMPLETED) {
        sc = state_wait_procedure_disable_on_procedure_enable_completed(initiator,
                                                                        data);
      }
      break;

    case INITIATOR_STATE_DELETE:
      if (event == INITIATOR_EVT_PROCEDURE_ENABLE_COMPLETED) {
        sc = state_delete_on_procedure_enable_completed(initiator, data);
      }
      break;

    case INITIATOR_STATE_ERROR:
      break;

    default:
      break;
  }
  return sc;
}