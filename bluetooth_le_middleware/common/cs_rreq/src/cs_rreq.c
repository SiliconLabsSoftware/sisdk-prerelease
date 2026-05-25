/***************************************************************************//**
 * @file
 * @brief CS RREQ - core implementation
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
#include "sl_bt_api.h"
#include "sl_bluetooth_connection_config.h"
#include "cs_rreq_api.h"
#include "cs_rreq.h"
#include "cs_rreq_internal.h"
#include "cs_rreq_config.h"
#include "cs_rreq_log.h"
#include "cs_rreq_state.h"
#include "cs_ras_client.h"
#include "app_rta.h"

// -----------------------------------------------------------------------------
// Definitions

#define SL_BT_INVALID_CHARACTERISTIC_HANDLE  0xFFFF
#define SL_BT_INVALID_SERVICE_HANDLE         0xFFFFFFFF

// -----------------------------------------------------------------------------
// Forward declaration of private functions

static void track_subevent(rreq_t *rreq, uint8_t procedure_done_status);
static void on_runtime_error(app_rta_error_t error, sl_status_t result);
static void clear_instance_data(rreq_t *rreq);

// -----------------------------------------------------------------------------
// Internal variables

// Callbacks for events
cs_rreq_event_callback_t callback;
// Result callback
cs_rreq_on_result_t send_result;

// -----------------------------------------------------------------------------
// Private variables

// RREQ instance storage
static rreq_t rreq_instances[CS_RREQ_CONFIG_MAX_CONNECTIONS];

// RTA context guarding shared state
// Externally visible from cs_rreq_internal.h
// Note: The guard is assumed to be recursive
app_rta_context_t cs_rreq_ctx;

// -----------------------------------------------------------------------------
// Public functions

sl_status_t cs_rreq_set_event_callbacks(cs_rreq_event_callback_t *cb)
{
  if ((cb == NULL)
      || (cb->on_create == NULL)
      || (cb->on_enable == NULL)
      || (cb->on_error == NULL)) {
    return SL_STATUS_NULL_POINTER;
  }
  sl_status_t sc = app_rta_acquire(cs_rreq_ctx);
  if (sc != SL_STATUS_OK) {
    rreq_log_error("Failed to acquire RTA context, sc=0x%lx" LOG_NL,
                   (unsigned long)sc);
    return sc;
  }
  callback.on_create = cb->on_create;
  callback.on_enable = cb->on_enable;
  callback.on_error = cb->on_error;
  (void)app_rta_release(cs_rreq_ctx);
  return SL_STATUS_OK;
}

sl_status_t cs_rreq_set_result_callback(cs_rreq_on_result_t on_result)
{
  if (on_result == NULL) {
    return SL_STATUS_NULL_POINTER;
  }
  sl_status_t sc = app_rta_acquire(cs_rreq_ctx);
  if (sc != SL_STATUS_OK) {
    rreq_log_error("Failed to acquire RTA context, sc=0x%lx" LOG_NL,
                   (unsigned long)sc);
    return sc;
  }
  send_result = on_result;
  (void)app_rta_release(cs_rreq_ctx);
  return SL_STATUS_OK;
}

sl_status_t cs_rreq_set_process_finished(uint8_t conn_handle,
                                         uint16_t ranging_counter)
{
  if (conn_handle == SL_BT_INVALID_CONNECTION_HANDLE) {
     return SL_STATUS_INVALID_HANDLE;
  }
  if (ranging_counter == CS_RAS_INVALID_RANGING_COUNTER) {
     return SL_STATUS_INVALID_PARAMETER;
  }
  sl_status_t sc = app_rta_acquire(cs_rreq_ctx);
  if (sc != SL_STATUS_OK) {
    rreq_log_error("Failed to acquire RTA context, sc=0x%lx" LOG_NL,
                   (unsigned long)sc);
    return sc;
  }
  rreq_t *rreq = cs_rreq_find(conn_handle);
  if (rreq == NULL) {
    (void)app_rta_release(cs_rreq_ctx);
    return SL_STATUS_NOT_FOUND;
  }
  // Procedure data processed, clear the subevent data
  reset_subevent_data(rreq, false);
  (void)app_rta_release(cs_rreq_ctx);
  return sc;
}

sl_status_t cs_rreq_create(uint8_t conn_handle,
                           cs_rreq_create_config_t *config)
{
  if (config == NULL) {
    return SL_STATUS_NULL_POINTER;
  }
  if (conn_handle == SL_BT_INVALID_CONNECTION_HANDLE) {
     return SL_STATUS_INVALID_HANDLE;
  }
  if (config->service == SL_BT_INVALID_SERVICE_HANDLE) {
     return SL_STATUS_INVALID_HANDLE;
  }
  #if !defined(CS_RREQ_CONFIG_RAS_ON_DEMAND_SUPPORT) || (CS_RREQ_CONFIG_RAS_ON_DEMAND_SUPPORT == 0)
  // Only real-time RAS mode is supported in RREQ
  if (!config->real_time_mode) {
    return SL_STATUS_NOT_SUPPORTED;
  }
  #endif // !defined(CS_RREQ_CONFIG_RAS_ON_DEMAND_SUPPORT) || (CS_RREQ_CONFIG_RAS_ON_DEMAND_SUPPORT == 0)
  sl_status_t sc = app_rta_acquire(cs_rreq_ctx);
  if (sc != SL_STATUS_OK) {
    rreq_log_error("Failed to acquire RTA context, sc=0x%lx" LOG_NL,
                   (unsigned long)sc);
    return sc;
  }
  rreq_t *rreq = cs_rreq_find(conn_handle);
  if (rreq != NULL) {
    (void)app_rta_release(cs_rreq_ctx);
    return SL_STATUS_ALREADY_INITIALIZED;
  }
  rreq = cs_rreq_find_empty();
  if (rreq == NULL) {
    (void)app_rta_release(cs_rreq_ctx);
    return SL_STATUS_NO_MORE_RESOURCE;
  }

  clear_instance_data(rreq);

  // Assign instance to connection handle
  rreq->conn_handle = conn_handle;
  // Copy config
  rreq->config.real_time_mode = config->real_time_mode;
  rreq->config.ras_config = config->ras_config;
  rreq->config.service = config->service;
  rreq->config.mtu = config->mtu;
  rreq->config.antenna_config = config->antenna_config;
  rreq->config.is_initiator = config->is_initiator;
  memcpy(&rreq->config.gattdb_handles,
         &config->gattdb_handles,
         sizeof(rreq->config.gattdb_handles));

  sc = cs_ras_client_create(rreq->conn_handle,
                            &rreq->config.gattdb_handles,
                            rreq->config.mtu);
  if (sc == SL_STATUS_OK) {
     (void)sm_on_evt(rreq, RREQ_EVT_INIT_STARTED, NULL);
  }
  (void)app_rta_release(cs_rreq_ctx);
  return sc;
}

sl_status_t cs_rreq_enable(uint8_t conn_handle, uint8_t enable)
{
  if (conn_handle == SL_BT_INVALID_CONNECTION_HANDLE) {
     return SL_STATUS_INVALID_HANDLE;
  }
  sl_status_t sc = app_rta_acquire(cs_rreq_ctx);
  if (sc != SL_STATUS_OK) {
    rreq_log_error("Failed to acquire RTA context, sc=0x%lx" LOG_NL,
                   (unsigned long)sc);
    return sc;
  }
  rreq_t *rreq = cs_rreq_find(conn_handle);
  if (rreq == NULL) {
    (void)app_rta_release(cs_rreq_ctx);
    return SL_STATUS_NOT_FOUND;
  }

  if (enable) {
    if (rreq->state != RREQ_STATE_DISABLED) {
      (void)app_rta_release(cs_rreq_ctx);
      return SL_STATUS_INVALID_STATE;
    }

    // Select mode
    sc = cs_ras_client_select_mode(rreq->conn_handle,
                                   rreq->config.real_time_mode
                                     ?CS_RAS_MODE_REAL_TIME_RANGING_DATA
                                     :CS_RAS_MODE_ON_DEMAND_RANGING_DATA);
    if (sc == SL_STATUS_OK) {
      rreq->subevents_per_procedure_counter = 0;
      rreq->drop_counter = 0;
      rreq->ras_overwritten = false;
      rreq->last_subevent_header = NULL;
      (void)sm_on_evt(rreq, RREQ_EVT_ENABLE, NULL);
    }

  } else {
    // Disable
    if (rreq->state == RREQ_STATE_DISABLED) {
      (void)app_rta_release(cs_rreq_ctx);
      return SL_STATUS_INVALID_STATE;
    }

    sc = cs_ras_client_select_mode(rreq->conn_handle,
                                   CS_RAS_MODE_NONE);
    (void)sm_on_evt(rreq, RREQ_EVT_DISABLE, NULL);
  }

  (void)app_rta_release(cs_rreq_ctx);
  return sc;
}

sl_status_t cs_rreq_remove(uint8_t conn_handle)
{
  sl_status_t sc = app_rta_acquire(cs_rreq_ctx);
  if (sc != SL_STATUS_OK) {
    rreq_log_error("Failed to acquire RTA context, sc=0x%lx" LOG_NL,
                   (unsigned long)sc);
    return sc;
  }
  rreq_t * rreq = cs_rreq_find(conn_handle);
  if (rreq == NULL) {
    return SL_STATUS_NOT_FOUND;
  }
  if (rreq->state != RREQ_STATE_DISABLED) {
    return SL_STATUS_INVALID_STATE;
  }
  clear_instance_data(rreq);
  rreq_log_info(INSTANCE_PREFIX "instance deleted" LOG_NL, conn_handle);
  (void)app_rta_release(cs_rreq_ctx);
  return SL_STATUS_OK;
}

// -----------------------------------------------------------------------------
// Internal functions

void cs_rreq_reset(void)
{
  for (uint8_t i = 0; i < CS_RREQ_CONFIG_MAX_CONNECTIONS; i++) {
    clear_instance_data(&rreq_instances[i]);
  }
}

rreq_t *cs_rreq_find(uint8_t conn_handle)
{
  for (uint8_t i = 0; i < CS_RREQ_CONFIG_MAX_CONNECTIONS; i++) {
    if (rreq_instances[i].conn_handle == conn_handle) {
      return &rreq_instances[i];
    }
  }
  return NULL;
}

rreq_t *cs_rreq_find_empty(void)
{
  return cs_rreq_find(SL_BT_INVALID_CONNECTION_HANDLE);
}

void cs_rreq_init(void)
{
  // Reset RREQ slots
  cs_rreq_reset();
}

void cs_rreq_rta_init(void)
{
  app_rta_config_t config = {.requirement.runtime = true,
                             .requirement.guard = true,
                             .requirement.signal = false,
                             .requirement.queue = true,
                             .step = cs_rreq_step,
                             .priority = CS_RREQ_CONFIG_RTA_PRIORITY,
                             .stack_size = CS_RREQ_CONFIG_RTA_STACK_SIZE,
                             .error = on_runtime_error,
                             .wait_for_guard = CS_RREQ_CONFIG_WAIT_FOR_GUARD,
                             .queue_size = CS_RREQ_CONFIG_RTA_QUEUE_SIZE,
                             .queue_element_size = sizeof(cs_rreq_ras_evt_t)};
  sl_status_t sc = app_rta_create_context(&config, &cs_rreq_ctx);
  if (sc != SL_STATUS_OK) {
    rreq_log_error("Failed to create RTA context, sc=0x%lx" LOG_NL,
                   (unsigned long)sc);
  }
}

void cs_rreq_rta_ready(void)
{
  (void)app_rta_proceed(cs_rreq_ctx);
}

void cs_rreq_post_ras_evt(const cs_rreq_ras_evt_t *evt)
{
  if (evt == NULL) {
    return;
  }
  sl_status_t sc = app_rta_queue_push(cs_rreq_ctx,
                                      (uint8_t *)evt,
                                      sizeof(*evt));
  if (sc != SL_STATUS_OK) {
    rreq_log_error("Failed to enqueue RAS event (type=%u, conn=%u), sc=0x%lx" LOG_NL,
                   (unsigned)evt->type,
                   evt->connection,
                   (unsigned long)sc);
    rreq_error(NULL, CS_RREQ_ERROR_RUNTIME_ERROR, sc);
    return;
  }
  // Proceed the RTA context to process the event
  (void)app_rta_proceed(cs_rreq_ctx);
}

static void on_runtime_error(app_rta_error_t error, sl_status_t result)
{
  switch (error) {
    case APP_RTA_ERROR_RUNTIME_INIT_FAILED:
      rreq_log_error("RTA runtime init failed, sc=0x%lx" LOG_NL,
                     (unsigned long)result);
      break;
    case APP_RTA_ERROR_ACQUIRE_FAILED:
      rreq_log_error("RTA acquire failed, sc=0x%lx" LOG_NL,
                     (unsigned long)result);
      break;
    case APP_RTA_ERROR_RELEASE_FAILED:
      rreq_log_error("RTA release failed, sc=0x%lx" LOG_NL,
                     (unsigned long)result);
      break;
    default:
      rreq_log_error("RTA generic error, sc=0x%lx" LOG_NL,
                     (unsigned long)result);
      break;
  }
  rreq_error(NULL, CS_RREQ_ERROR_RUNTIME_ERROR, result);
}
/******************************************************************************
 * Bluetooth stack event handler.
 *****************************************************************************/
bool cs_rreq_on_bt_event(sl_bt_msg_t *evt)
{
  rreq_t *rreq;
  bool handled = false;
  sm_evt_data_t evt_data;

  sl_status_t sc = app_rta_acquire(cs_rreq_ctx);
  if (sc != SL_STATUS_OK) {
    rreq_log_error("Failed to acquire RTA context, sc=0x%lx" LOG_NL,
                   (unsigned long)sc);
    return !handled;
  }

  switch (SL_BT_MSG_ID(evt->header)) {
    // --------------------------------
    // Connection closed
    case sl_bt_evt_connection_closed_id:
      rreq_log_info("sl_bt_evt_connection_closed_id" LOG_NL);
      rreq = cs_rreq_find(evt->data.evt_connection_closed.connection);
      if (rreq == NULL) {
        break;
      }
      clear_instance_data(rreq);
      rreq_log_info(INSTANCE_PREFIX "instance deleted" LOG_NL, 
                    evt->data.evt_connection_closed.connection);
      break;
    // --------------------------------
    // CS result arrived
    case sl_bt_evt_cs_result_id:
      rreq_log_info("sl_bt_evt_cs_result_id" LOG_NL);
      rreq = cs_rreq_find(evt->data.evt_cs_result.connection);
      if (rreq == NULL) {
        break;
      }
      handled = true;
      #ifdef SL_CATALOG_BLUETOOTH_FEATURE_CS_TEST_PRESENT
      handled = false;
      #endif //SL_CATALOG_BLUETOOTH_FEATURE_CS_TEST_PRESENT
      rreq_log_info(INSTANCE_PREFIX "CS - received first initiator CS result" LOG_NL,
                    evt->data.evt_cs_result.connection);
      if (rreq->ranging_counter == CS_RAS_INVALID_RANGING_COUNTER) {
        if (rreq->state == RREQ_STATE_WAIT_REMOTE_COMPLETE
            || rreq->state == RREQ_STATE_WAIT_REMOTE_ABORT) {
          set_state(rreq, RREQ_STATE_IN_PROCEDURE);
          rreq_log_info(INSTANCE_PREFIX "Instance new state: IN_PROCEDURE" LOG_NL,
                        rreq->conn_handle);
        }
      }
      if (rreq->state == RREQ_STATE_WAIT_REMOTE_COMPLETE
          || rreq->state == RREQ_STATE_WAIT_REMOTE_ABORT) {
        rreq->drop_counter++;
        if (rreq->drop_counter > CS_RREQ_CONFIG_MAX_DROP) {
          rreq->drop_counter = 0;
          rreq->ranging_counter = CS_RAS_INVALID_RANGING_COUNTER;
          reset_subevent_data(rreq, false);
          set_state(rreq, RREQ_STATE_IN_PROCEDURE);
          rreq_log_info(INSTANCE_PREFIX "Instance new state: IN_PROCEDURE" LOG_NL,
                        rreq->conn_handle);
        } else {
          rreq_log_info(INSTANCE_PREFIX "CS - ongoing measurement, drop new result" LOG_NL,
                        evt->data.evt_cs_result.connection);
          // Still count this subevent even though the result is dropped
          rreq->subevents_per_procedure_counter++;
          track_subevent(rreq, evt->data.evt_cs_result.procedure_done_status);
          break;
        }
      }
      evt_data.evt_cs_result.cs_event = evt;
      evt_data.evt_cs_result.first_cs_result = true;
      (void)sm_on_evt(rreq, RREQ_EVT_CS_RESULT, &evt_data);
      track_subevent(rreq, evt->data.evt_cs_result.procedure_done_status);
      break;

    // --------------------------------
    // Consecutive CS result (initiator) arrived
    case sl_bt_evt_cs_result_continue_id:
      rreq_log_info("sl_bt_evt_cs_result_continue_id" LOG_NL);
      rreq = cs_rreq_find(evt->data.evt_cs_result.connection);
      if (rreq == NULL) {
        break;
      }
      handled = true;
      track_subevent(rreq, evt->data.evt_cs_result_continue.procedure_done_status);
      if (rreq->state != RREQ_STATE_WAIT_REMOTE_COMPLETE
          && rreq->state != RREQ_STATE_WAIT_REMOTE_ABORT) {
        rreq_log_info(INSTANCE_PREFIX "CS - received CS result" LOG_NL,
                      evt->data.evt_cs_result_continue.connection);
        evt_data.evt_cs_result.cs_event = evt;
        evt_data.evt_cs_result.first_cs_result = false;
        (void)sm_on_evt(rreq, RREQ_EVT_CS_RESULT_CONTINUE, &evt_data);
      } else {
        rreq_log_info(INSTANCE_PREFIX "CS - ongoing measurement, drop new result continue" LOG_NL,
                      evt->data.evt_cs_result.connection);
      }
      #ifdef SL_CATALOG_BLUETOOTH_FEATURE_CS_TEST_PRESENT
      handled = false;
      #endif //SL_CATALOG_BLUETOOTH_FEATURE_CS_TEST_PRESENT
      break;
    // --------------------------------
    // CS procedure enable completed
    case sl_bt_evt_cs_procedure_enable_complete_id:
      rreq = cs_rreq_find(evt->data.evt_cs_procedure_enable_complete.connection);
      if (rreq == NULL) {
        break;
      }
      if (evt->data.evt_cs_procedure_enable_complete.status != SL_STATUS_OK) {
        break;
      }

      (void)cs_ras_client_procedure_enabled(rreq->conn_handle,
                                            (evt->data.evt_cs_procedure_enable_complete.state == sl_bt_cs_procedure_state_enabled));
      if (evt->data.evt_cs_procedure_enable_complete.state != sl_bt_cs_procedure_state_enabled) {
        break;
      }
      rreq->procedure_info.subevent_len = evt->data.evt_cs_procedure_enable_complete.subevent_len;
      rreq->procedure_info.subevent_interval = evt->data.evt_cs_procedure_enable_complete.subevent_interval;
      rreq->procedure_info.event_interval = evt->data.evt_cs_procedure_enable_complete.event_interval;
      rreq->procedure_info.procedure_interval = evt->data.evt_cs_procedure_enable_complete.procedure_interval;
      rreq->procedure_info.procedure_count = evt->data.evt_cs_procedure_enable_complete.procedure_count;
      rreq->procedure_info.subevents_per_event = evt->data.evt_cs_procedure_enable_complete.subevents_per_event;
      break;
    // --------------------------------
    // Bluetooth stack resource exhausted
    case sl_bt_evt_system_resource_exhausted_id:
      rreq_log_error("BT stack buffers exhausted, "
                     "data loss may have occurred! [buf_discarded:%u, "
                     "buf_alloc_fail:%u, heap_alloc_fail:%u]" LOG_NL,
                     evt->data.evt_system_resource_exhausted.num_buffers_discarded,
                     evt->data.evt_system_resource_exhausted.num_buffer_allocation_failures,
                     evt->data.evt_system_resource_exhausted.num_heap_allocation_failures);
      break;
    default:
      break;

  }

  (void)app_rta_release(cs_rreq_ctx);

  // Return false if the event was handled above.
  return !handled;
}

static void clear_instance_data(rreq_t *rreq)
{
  memset(rreq, 0, sizeof(rreq_t));
  reset_subevent_data(rreq, false);
  rreq->conn_handle = SL_BT_INVALID_CONNECTION_HANDLE;
  rreq->config.real_time_mode = true;
  rreq->config.service = SL_BT_INVALID_SERVICE_HANDLE;
  rreq->config.mtu = ATT_MTU_MIN;
  rreq->config.ras_config.real_time_ranging_data_indication = false;
  rreq->config.ras_config.on_demand_ranging_data_indication = false;
  rreq->config.ras_config.ranging_data_ready_notification = true;
  rreq->config.ras_config.ranging_data_overwritten_notification = true;
  rreq->data.initiator.data_size = 0;
  rreq->data.reflector.data_size = 0;
  set_state(rreq, RREQ_STATE_UNINITIALIZED);
}

/******************************************************************************
 * Call user error callback on error if possible
 *
 * @param rreq instance reference
 * @param evt RREQ error event
 * @param sc status code
 *****************************************************************************/
void rreq_error(rreq_t *rreq, cs_rreq_error_t evt, sl_status_t sc)
{
  uint8_t conn_handle = SL_BT_INVALID_CONNECTION_HANDLE;
  if (rreq != NULL) {
    conn_handle = rreq->conn_handle;
  }
  rreq_log_error(INSTANCE_PREFIX "Error occurred (sc: 0x%lx)" LOG_NL,
                 conn_handle,
                 (unsigned long)sc);
  if (callback.on_error != NULL) {
    callback.on_error(conn_handle, evt, sc);
  }
}


/******************************************************************************
 * Log the number of successfully created subevents when CS procedure is completed
 * or aborted, then reset the counter for the next procedure.
 *
 * @param[in] rreq                  RREQ instance.
 * @param[in] procedure_done_status Done status of the current CS procedure
 *                                  (sl_bt_cs_done_status_complete or
 *                                   sl_bt_cs_done_status_aborted).
 *****************************************************************************/
static void track_subevent(rreq_t *rreq, uint8_t procedure_done_status)
{
  if ((procedure_done_status == sl_bt_cs_done_status_complete)
      || (procedure_done_status == sl_bt_cs_done_status_aborted)) {
    rreq_log_info(INSTANCE_PREFIX "Created subevents in %s procedure %u: %u" LOG_NL,
                  rreq->conn_handle,
                  (procedure_done_status == sl_bt_cs_done_status_complete)
                    ? "completed"
                    : "aborted",
                  rreq->ranging_counter & CS_RAS_RANGING_COUNTER_MASK,
                  rreq->subevents_per_procedure_counter);
    rreq->subevents_per_procedure_counter = 0u;
  }
}
