/***************************************************************************//**
 * @file
 * @brief CS RREQ - RAS Client management implementation
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
#include <string.h>
#include "sl_status.h"
#include "app_rta.h"
#include "cs_rreq.h"
#include "cs_rreq_internal.h"
#include "cs_rreq_config.h"
#include "cs_rreq_log.h"
#include "cs_rreq_state.h"
#include "cs_ras_client.h"

// -----------------------------------------------------------------------------
// Forward declaration of private functions

static void process_remote_ranging_data(rreq_t *rreq,
                                        uint8_t *data,
                                        uint32_t data_size);
static void dispatch_evt(rreq_t *rreq, const cs_rreq_ras_evt_t *evt);
static cs_rreq_error_t evt_type_to_error(cs_rreq_ras_evt_type_t type);

#if defined(CS_RREQ_CONFIG_RAS_ON_DEMAND_SUPPORT) && (CS_RREQ_CONFIG_RAS_ON_DEMAND_SUPPORT == 1)
static void get_lost_segments(uint64_t lost_segments,
                              uint8_t *start_segment,
                              uint8_t *end_segment);
#endif // defined(CS_RREQ_CONFIG_RAS_ON_DEMAND_SUPPORT) && (CS_RREQ_CONFIG_RAS_ON_DEMAND_SUPPORT == 1)

// Static event handlers
// Guard must be acquired before calling the event handlers
static void handle_initialized(rreq_t *rreq, const cs_rreq_ras_evt_t *evt);
static void handle_mode_changed(rreq_t *rreq, const cs_rreq_ras_evt_t *evt);
static void handle_reception_finished(rreq_t *rreq, const cs_rreq_ras_evt_t *evt);
#if defined(CS_RREQ_CONFIG_RAS_ON_DEMAND_SUPPORT) && (CS_RREQ_CONFIG_RAS_ON_DEMAND_SUPPORT == 1)
static void handle_ack_finished(rreq_t *rreq, const cs_rreq_ras_evt_t *evt);
static void handle_ranging_data_ready(rreq_t *rreq, const cs_rreq_ras_evt_t *evt);
static void handle_abort_finished(rreq_t *rreq, const cs_rreq_ras_evt_t *evt);
static void handle_ranging_data_overwritten(rreq_t *rreq, const cs_rreq_ras_evt_t *evt);
#endif // CS_RREQ_CONFIG_RAS_ON_DEMAND_SUPPORT
static void handle_timeout(rreq_t *rreq, const cs_rreq_ras_evt_t *evt);

void cs_rreq_step(void)
{
  cs_rreq_ras_evt_t evt;
  size_t size = sizeof(evt);
  sl_status_t sc =
    app_rta_queue_read_and_acquire(cs_rreq_ctx, (uint8_t *)&evt, &size);
  if (sc == SL_STATUS_EMPTY) {
    return;
  }
  if (sc != SL_STATUS_OK) {
    rreq_log_error("Failed to read RAS event queue, sc=0x%lx" LOG_NL,
                   (unsigned long)sc);
    return;
  }

  rreq_t *rreq = cs_rreq_find(evt.connection);
  if (rreq == NULL) {
    rreq_log_error(INSTANCE_PREFIX
                   "RAS - unknown connection id (evt type=%u)!" LOG_NL,
                   evt.connection, (unsigned)evt.type);
    rreq_error(NULL, evt_type_to_error(evt.type), SL_STATUS_NULL_POINTER);
  } else {
    dispatch_evt(rreq, &evt);
  }

  (void)app_rta_release(cs_rreq_ctx);
}

// Helper functions
// Map an event type to the error code reported when the corresponding rreq
// instance can no longer be resolved (e.g. the connection was removed
// between the callback firing and the queue being drained).
static cs_rreq_error_t evt_type_to_error(cs_rreq_ras_evt_type_t type)
{
  switch (type) {
    case CS_RREQ_RAS_EVT_INITIALIZED:
      return CS_RREQ_ERROR_RAS_CLIENT_INIT_FAILED;
    case CS_RREQ_RAS_EVT_MODE_CHANGED:
      return CS_RREQ_ERROR_RAS_CLIENT_MODE_CHANGE_FAILED;
    case CS_RREQ_RAS_EVT_RECEPTION_FINISHED:
      return CS_RREQ_ERROR_RAS_CLIENT_DATA_RECEPTION_FINISH_FAILED;
#if defined(CS_RREQ_CONFIG_RAS_ON_DEMAND_SUPPORT) && (CS_RREQ_CONFIG_RAS_ON_DEMAND_SUPPORT == 1)
    case CS_RREQ_RAS_EVT_ACK_FINISHED:
      return CS_RREQ_ERROR_RAS_CLIENT_ON_ACK_FINISHED_FAILED;
    case CS_RREQ_RAS_EVT_RANGING_DATA_READY:
      return CS_RREQ_ERROR_RAS_CLIENT_RANGING_DATA_READY_FAILED;
    case CS_RREQ_RAS_EVT_ABORT_FINISHED:
      return CS_RREQ_ERROR_RAS_CLIENT_ABORT_FINISHED_FAILED;
    case CS_RREQ_RAS_EVT_RANGING_DATA_OVERWRITTEN:
      return CS_RREQ_ERROR_RAS_CLIENT_RANGING_DATA_OVERWRITTEN_FAILED;
#endif // CS_RREQ_CONFIG_RAS_ON_DEMAND_SUPPORT
    case CS_RREQ_RAS_EVT_TIMEOUT:
      return CS_RREQ_ERROR_RAS_CLIENT_TIMEOUT;
    default:
      return CS_RREQ_ERROR_RAS_CLIENT_INIT_FAILED;
  }
}

// Dispatch one already-dequeued event. Caller holds the cs_rreq guard.
static void dispatch_evt(rreq_t *rreq, const cs_rreq_ras_evt_t *evt)
{
  switch (evt->type) {
    case CS_RREQ_RAS_EVT_INITIALIZED:
      handle_initialized(rreq, evt);
      break;
    case CS_RREQ_RAS_EVT_MODE_CHANGED:
      handle_mode_changed(rreq, evt);
      break;
    case CS_RREQ_RAS_EVT_RECEPTION_FINISHED:
      handle_reception_finished(rreq, evt);
      break;
#if defined(CS_RREQ_CONFIG_RAS_ON_DEMAND_SUPPORT) && (CS_RREQ_CONFIG_RAS_ON_DEMAND_SUPPORT == 1)
    case CS_RREQ_RAS_EVT_ACK_FINISHED:
      handle_ack_finished(rreq, evt);
      break;
    case CS_RREQ_RAS_EVT_RANGING_DATA_READY:
      handle_ranging_data_ready(rreq, evt);
      break;
    case CS_RREQ_RAS_EVT_ABORT_FINISHED:
      handle_abort_finished(rreq, evt);
      break;
    case CS_RREQ_RAS_EVT_RANGING_DATA_OVERWRITTEN:
      handle_ranging_data_overwritten(rreq, evt);
      break;
#endif // CS_RREQ_CONFIG_RAS_ON_DEMAND_SUPPORT
    case CS_RREQ_RAS_EVT_TIMEOUT:
      handle_timeout(rreq, evt);
      break;
    default:
      rreq_log_error(INSTANCE_PREFIX "RAS - unknown event type %u!" LOG_NL,
                     rreq->conn_handle,
                     (unsigned)evt->type);
      break;
  }
}

// -----------------------------------------------------------------------------
// Public cs_ras_client callbacks - queue events only, no guard.

void cs_ras_client_on_initialized(uint8_t connection,
                                  cs_ras_features_t features,
                                  sl_status_t sc_in)
{
  cs_rreq_ras_evt_t evt;
  memset(&evt, 0, sizeof(evt));
  evt.type = CS_RREQ_RAS_EVT_INITIALIZED;
  evt.connection = connection;
  evt.data.initialized.features = features;
  evt.data.initialized.sc = sc_in;
  cs_rreq_post_ras_evt(&evt);
}

void cs_ras_client_on_mode_changed(uint8_t       connection,
                                   cs_ras_mode_t mode,
                                   sl_status_t   sc_in)
{
  cs_rreq_ras_evt_t evt;
  memset(&evt, 0, sizeof(evt));
  evt.type = CS_RREQ_RAS_EVT_MODE_CHANGED;
  evt.connection = connection;
  evt.data.mode_changed.mode = mode;
  evt.data.mode_changed.sc = sc_in;
  cs_rreq_post_ras_evt(&evt);
}

void cs_ras_client_on_ranging_data_reception_finished(uint8_t                         connection,
                                                      bool                            real_time,
                                                      bool                            retrieve_lost,
                                                      sl_status_t                     sc_in,
                                                      cs_ras_cp_response_code_value_t response,
                                                      cs_ras_ranging_counter_t        ranging_counter,
                                                      uint8_t                         start_segment,
                                                      uint8_t                         end_segment,
                                                      bool                            recoverable,
                                                      uint32_t                        size,
                                                      bool                            last_arrived,
                                                      uint8_t                         last_known_segment,
                                                      uint64_t                        lost_segments)
{
  cs_rreq_ras_evt_t evt;
  memset(&evt, 0, sizeof(evt));
  evt.type = CS_RREQ_RAS_EVT_RECEPTION_FINISHED;
  evt.connection = connection;
  evt.data.reception_finished.real_time          = real_time;
  evt.data.reception_finished.retrieve_lost      = retrieve_lost;
  evt.data.reception_finished.sc                 = sc_in;
  evt.data.reception_finished.response           = response;
  evt.data.reception_finished.ranging_counter    = ranging_counter;
  evt.data.reception_finished.start_segment      = start_segment;
  evt.data.reception_finished.end_segment        = end_segment;
  evt.data.reception_finished.recoverable        = recoverable;
  evt.data.reception_finished.size               = size;
  evt.data.reception_finished.last_arrived       = last_arrived;
  evt.data.reception_finished.last_known_segment = last_known_segment;
  evt.data.reception_finished.lost_segments      = lost_segments;
  cs_rreq_post_ras_evt(&evt);
}

#if defined(CS_RREQ_CONFIG_RAS_ON_DEMAND_SUPPORT) && (CS_RREQ_CONFIG_RAS_ON_DEMAND_SUPPORT == 1)
void cs_ras_client_on_ack_finished(uint8_t connection,
                                   sl_status_t sc_in,
                                   cs_ras_cp_response_code_value_t response)
{
  cs_rreq_ras_evt_t evt;
  memset(&evt, 0, sizeof(evt));
  evt.type = CS_RREQ_RAS_EVT_ACK_FINISHED;
  evt.connection = connection;
  evt.data.ack_finished.sc = sc_in;
  evt.data.ack_finished.response = response;
  cs_rreq_post_ras_evt(&evt);
}

void cs_ras_client_on_ranging_data_ready(uint8_t connection,
                                         cs_ras_ranging_counter_t ranging_counter)
{
  cs_rreq_ras_evt_t evt;
  memset(&evt, 0, sizeof(evt));
  evt.type = CS_RREQ_RAS_EVT_RANGING_DATA_READY;
  evt.connection = connection;
  evt.data.ranging_data_ready.ranging_counter = ranging_counter;
  cs_rreq_post_ras_evt(&evt);
}

void cs_ras_client_on_abort_finished(uint8_t connection,
                                     sl_status_t sc_in,
                                     cs_ras_cp_response_code_value_t response)
{
  cs_rreq_ras_evt_t evt;
  memset(&evt, 0, sizeof(evt));
  evt.type = CS_RREQ_RAS_EVT_ABORT_FINISHED;
  evt.connection = connection;
  evt.data.abort_finished.sc = sc_in;
  evt.data.abort_finished.response = response;
  cs_rreq_post_ras_evt(&evt);
}

void cs_ras_client_on_ranging_data_overwritten(uint8_t connection,
                                               cs_ras_ranging_counter_t ranging_counter)
{
  cs_rreq_ras_evt_t evt;
  memset(&evt, 0, sizeof(evt));
  evt.type = CS_RREQ_RAS_EVT_RANGING_DATA_OVERWRITTEN;
  evt.connection = connection;
  evt.data.ranging_data_overwritten.ranging_counter = ranging_counter;
  cs_rreq_post_ras_evt(&evt);
}
#endif // defined(CS_RREQ_CONFIG_RAS_ON_DEMAND_SUPPORT) && (CS_RREQ_CONFIG_RAS_ON_DEMAND_SUPPORT == 1)

bool cs_ras_client_on_timeout(uint8_t connection,
                              cs_ras_client_timeout_t timeout,
                              cs_ras_client_timeout_action_t action)
{
  // Default response: let the RAS client perform the action automatically.
  // The error is reported asynchronously from handle_timeout.
  cs_rreq_ras_evt_t evt;
  memset(&evt, 0, sizeof(evt));
  evt.type = CS_RREQ_RAS_EVT_TIMEOUT;
  evt.connection = connection;
  evt.data.timeout.timeout = timeout;
  evt.data.timeout.action = action;
  cs_rreq_post_ras_evt(&evt);
  return false;
}

// -----------------------------------------------------------------------------
// Static event handlers

static void handle_initialized(rreq_t *rreq, const cs_rreq_ras_evt_t *evt)
{
  sm_evt_data_t sm_evt;
  cs_ras_features_t features = evt->data.initialized.features;
  sl_status_t sc_in = evt->data.initialized.sc;
  sl_status_t sc;

  if (rreq->config.ras_config.real_time_ranging_data_indication
      && !(features & CS_RAS_FEATURE_RT_RANGING_DATA_MASK)) {
    rreq_log_error(INSTANCE_PREFIX "RAS - client initialized - "
                                   "real-time ranging data indication feature not supported!" LOG_NL,
                   rreq->conn_handle);
    rreq_error(rreq,
               CS_RREQ_ERROR_RAS_CLIENT_INIT_FEATURE_NOT_SUPPORTED,
               sc_in);
    sm_evt.evt_init_completed = SL_STATUS_NOT_SUPPORTED;
    (void)sm_on_evt(rreq, RREQ_EVT_INIT_COMPLETED, &sm_evt);
    return;
  }

  if (sc_in != SL_STATUS_OK) {
    rreq_log_error(INSTANCE_PREFIX "RAS - failed to initialize client! [sc: 0x%lx]" LOG_NL,
                   rreq->conn_handle,
                   (unsigned long)sc_in);
    rreq_error(rreq,
               CS_RREQ_ERROR_RAS_CLIENT_INIT_FAILED,
               sc_in);
    sm_evt.evt_init_completed = sc_in;
    (void)sm_on_evt(rreq, RREQ_EVT_INIT_COMPLETED, &sm_evt);
    return;
  }

  rreq_log_info(INSTANCE_PREFIX "RAS - client initialized [features: 0x%08lx]" LOG_NL,
                rreq->conn_handle,
                features);

  sc = cs_ras_client_configure(rreq->conn_handle, rreq->config.ras_config);
  if (sc != SL_STATUS_OK) {
    rreq_log_error(INSTANCE_PREFIX "RAS - failed to configure client! [sc: 0x%lx]" LOG_NL,
                   rreq->conn_handle,
                   (unsigned long)sc);
    rreq_error(rreq,
               CS_RREQ_ERROR_RAS_CLIENT_CONFIG_FAILED,
               sc);
    sm_evt.evt_init_completed = sc;
    (void)sm_on_evt(rreq, RREQ_EVT_INIT_COMPLETED, &sm_evt);
    return;
  }

  rreq_log_debug(INSTANCE_PREFIX
                 "RAS - client configured." LOG_NL
                 " - real-time ranging indication: %s" LOG_NL
                 " - on-demand ranging indication: %s" LOG_NL
                 " - ranging data ready notification: %s" LOG_NL
                 " - ranging data overwritten notification: %s" LOG_NL,
                 rreq->conn_handle,
                 ((rreq->config.ras_config.real_time_ranging_data_indication) ? "on" : "off"),
                 ((rreq->config.ras_config.on_demand_ranging_data_indication) ? "on" : "off"),
                 ((rreq->config.ras_config.ranging_data_ready_notification) ? "on" : "off"),
                 ((rreq->config.ras_config.ranging_data_overwritten_notification) ? "on" : "off"));
  sm_evt.evt_init_completed = SL_STATUS_OK;
  (void)sm_on_evt(rreq, RREQ_EVT_INIT_COMPLETED, &sm_evt);
}

static void handle_mode_changed(rreq_t *rreq, const cs_rreq_ras_evt_t *evt)
{
  sm_evt_data_t sm_evt;
  cs_ras_mode_t mode = evt->data.mode_changed.mode;
  sl_status_t sc_in = evt->data.mode_changed.sc;
  sl_status_t sc;

  if (sc_in != SL_STATUS_OK) {
    rreq_log_error(INSTANCE_PREFIX "RAS - failed to change mode to %u! [sc: 0x%lx]" LOG_NL,
                   rreq->conn_handle,
                   mode,
                   (unsigned long)sc_in);
    rreq_error(rreq,
               CS_RREQ_ERROR_RAS_CLIENT_MODE_CHANGE_FAILED,
               sc_in);
    sm_evt.evt_enable_completed.status = sc_in;
    sm_evt.evt_enable_completed.enable
      = (rreq->state == RREQ_STATE_ENABLING) ? CS_RREQ_ENABLE : CS_RREQ_DISABLE;
    (void)sm_on_evt(rreq, RREQ_EVT_ENABLE_COMPLETED, &sm_evt);
    return;
  }

  rreq_log_debug(INSTANCE_PREFIX "RAS - mode changed to %u" LOG_NL,
                 rreq->conn_handle,
                 mode);

  // Access remote ranging data based on the role
  cs_rreq_ranging_buffer_t *buffer = rreq->config.is_initiator
                                     ? &rreq->data.reflector
                                     : &rreq->data.initiator;

  switch (mode) {
    case CS_RAS_MODE_REAL_TIME_RANGING_DATA:
      sc = cs_ras_client_real_time_receive(rreq->conn_handle,
                                           sizeof(buffer->data),
                                           buffer->data);
      if (sc != SL_STATUS_OK) {
        rreq_log_error(INSTANCE_PREFIX "RAS - failed to receive real-time data! [sc: 0x%lx]" LOG_NL,
                       rreq->conn_handle,
                       (unsigned long)sc);
        rreq_error(rreq,
                   CS_RREQ_ERROR_RAS_CLIENT_REALTIME_RECEIVE_FAILED,
                   sc);
        return;
      }
      rreq_log_debug(INSTANCE_PREFIX "RAS - real-time data reception started" LOG_NL,
                     rreq->conn_handle);
      sm_evt.evt_enable_completed.status = sc;
      sm_evt.evt_enable_completed.enable
        = (rreq->state == RREQ_STATE_ENABLING) ? CS_RREQ_ENABLE : CS_RREQ_DISABLE;
      (void)sm_on_evt(rreq, RREQ_EVT_ENABLE_COMPLETED, &sm_evt);
      break;
    case CS_RAS_MODE_ON_DEMAND_RANGING_DATA:
      sm_evt.evt_enable_completed.status = SL_STATUS_OK;
      sm_evt.evt_enable_completed.enable
        = (rreq->state == RREQ_STATE_ENABLING) ? CS_RREQ_ENABLE : CS_RREQ_DISABLE;
      (void)sm_on_evt(rreq, RREQ_EVT_ENABLE_COMPLETED, &sm_evt);
      break;
    case CS_RAS_MODE_CHANGE_IN_PROGRESS:
      rreq_log_debug(INSTANCE_PREFIX "RAS - mode change in progress ..." LOG_NL,
                     rreq->conn_handle);
      break;
    case CS_RAS_MODE_NONE:
      if (rreq->state != RREQ_STATE_INIT) {
        sm_evt.evt_enable_completed.status = SL_STATUS_OK;
        sm_evt.evt_enable_completed.enable
          = (rreq->state == RREQ_STATE_ENABLING) ? CS_RREQ_ENABLE : CS_RREQ_DISABLE;
        (void)sm_on_evt(rreq, RREQ_EVT_ENABLE_COMPLETED, &sm_evt);
      }
      break;
    default:
      break;
  }
}

static void handle_reception_finished(rreq_t *rreq, const cs_rreq_ras_evt_t *evt)
{
  sl_status_t status;
  bool        real_time     = evt->data.reception_finished.real_time;
  sl_status_t sc_in         = evt->data.reception_finished.sc;
  uint32_t    size          = evt->data.reception_finished.size;
  uint64_t    lost_segments = evt->data.reception_finished.lost_segments;

  // Access remote ranging data based on the role
  cs_rreq_ranging_buffer_t *buffer = rreq->config.is_initiator
                                     ? &rreq->data.reflector
                                     : &rreq->data.initiator;

  if (rreq->config.real_time_mode) {
    // Re-enable reception for Real-Time mode
    status = cs_ras_client_real_time_receive(rreq->conn_handle,
                                             sizeof(buffer->data),
                                             buffer->data);
    if (status != SL_STATUS_OK) {
      rreq_log_error(INSTANCE_PREFIX "RAS - failed to receive real-time data! [sc: 0x%lx]" LOG_NL,
                     rreq->conn_handle,
                     (unsigned long)status);
      rreq_error(rreq,
                 CS_RREQ_ERROR_RAS_CLIENT_REALTIME_RECEIVE_FAILED,
                 status);
      return;
    }

    rreq_log_debug(INSTANCE_PREFIX "RAS - real-time data reception restarted" LOG_NL,
                   rreq->conn_handle);
  }
  if (sc_in != SL_STATUS_OK) {
    rreq_log_error(INSTANCE_PREFIX "RAS - reception finished - failure! [sc: 0x%lx]" LOG_NL,
                   rreq->conn_handle,
                   (unsigned long)sc_in);
    if ((lost_segments > 0) && (sc_in != SL_STATUS_ABORT)) {
      rreq_error(rreq,
                 CS_RREQ_ERROR_RAS_CLIENT_DATA_RECEPTION_FINISH_FAILED,
                 sc_in);
    }
    return;
  }

  rreq_log_debug(INSTANCE_PREFIX "RAS - %s reception finished, "
                                 "lost:%u counter:%u, resp.code:0x%02x, "
                                 "segment: %u -> %u %s, size:%lu, %s, "
                                 "last known segment: %u, lost segments mask: %16llx" LOG_NL,
                 rreq->conn_handle,
                 (real_time ? "real-time" : "on-demand"),
                 evt->data.reception_finished.retrieve_lost,
                 evt->data.reception_finished.ranging_counter,
                 evt->data.reception_finished.response,
                 evt->data.reception_finished.start_segment,
                 evt->data.reception_finished.end_segment,
                 (evt->data.reception_finished.recoverable ? "recoverable" : "non-recoverable"),
                 size,
                 (evt->data.reception_finished.last_arrived ? "last arrived" : "more to come"),
                 evt->data.reception_finished.last_known_segment,
                 lost_segments);
  if (real_time) {
    buffer->data_size = size;
    process_remote_ranging_data(rreq,
                                buffer->data,
                                buffer->data_size);
    return;
  }

  #if defined(CS_RREQ_CONFIG_RAS_ON_DEMAND_SUPPORT) && (CS_RREQ_CONFIG_RAS_ON_DEMAND_SUPPORT == 1)
  bool                            retrieve_lost   = evt->data.reception_finished.retrieve_lost;
  cs_ras_cp_response_code_value_t response        = evt->data.reception_finished.response;
  cs_ras_ranging_counter_t        ranging_counter = evt->data.reception_finished.ranging_counter;
  uint8_t                         start_segment   = evt->data.reception_finished.start_segment;
  uint8_t                         end_segment     = evt->data.reception_finished.end_segment;
  bool                            recoverable     = evt->data.reception_finished.recoverable;

  // Received Complete Ranging Data or Complete Lost Ranging Segment Response
  if (response == CS_RAS_CP_RESPONSE_CODE_SUCCESS) {
    if (lost_segments == 0) {
      if (retrieve_lost) {
        rreq_log_info(
          INSTANCE_PREFIX
          "RAS - Received Complete Lost Ranging Segment Response" LOG_NL,
          rreq->conn_handle);
      } else {
        rreq_log_info(INSTANCE_PREFIX
                      "RAS - Received Complete Ranging Data Response" LOG_NL,
                      rreq->conn_handle);
      }
      // Sending ACK
      status = cs_ras_client_ack(rreq->conn_handle, ranging_counter);
      if (status != SL_STATUS_OK) {
        rreq_log_error(INSTANCE_PREFIX
                       "RAS - failed to send ACK! [sc: 0x%lx]" LOG_NL,
                       rreq->conn_handle, (unsigned long)status);
        rreq_error(rreq, CS_RREQ_ERROR_RAS_CLIENT_ACK_FAILED, status);
        return;
      }
      rreq_log_info(INSTANCE_PREFIX "RAS - ACK was sent!" LOG_NL,
                    rreq->conn_handle);

      buffer->data_size = size;
      process_remote_ranging_data(rreq, buffer->data, buffer->data_size);
      rreq->ras_state = RAS_STATE_ON_DEMAND_ACK;
      return;
    } else if (!retrieve_lost && recoverable) {
      // Get start and end segment
      get_lost_segments(lost_segments, &start_segment, &end_segment);
      // Request lost segments
      status = cs_ras_client_retreive_lost_segments(
        rreq->conn_handle, ranging_counter, start_segment, end_segment,
        sizeof(buffer->data), buffer->data);
      if (status != SL_STATUS_OK) {
        rreq_log_error(
          INSTANCE_PREFIX
          "RAS - failed to request lost segments! [sc: 0x%lx]" LOG_NL,
          rreq->conn_handle, (unsigned long)status);
        rreq_error(rreq, CS_RREQ_ERROR_RAS_CLIENT_REQUEST_LOST_SEGMENTS_FAILED,
                   status);
        return;
      }
      rreq->ras_state = RAS_STATE_ON_DEMAND_RETRIEVE_LOST;
    } else {
      // Complete Lost Ranging Segment Response returned with lost segments
      // Or not recoverable lost segments arrived
      // sending ACK, no calculation
      rreq_log_error(INSTANCE_PREFIX
                     "RAS - unrecoverable lost segments, sending ACK!" LOG_NL,
                     rreq->conn_handle);
      status = cs_ras_client_ack(rreq->conn_handle, ranging_counter);
      if (status != SL_STATUS_OK) {
        rreq_log_error(INSTANCE_PREFIX
                       "RAS - failed to send ACK! [sc: 0x%lx]" LOG_NL,
                       rreq->conn_handle, (unsigned long)status);
        rreq_error(rreq, CS_RREQ_ERROR_RAS_CLIENT_ACK_FAILED, status);
        return;
      }
      rreq->ras_state = RAS_STATE_ON_DEMAND_ACK;
    }
  }
#endif // defined(CS_RREQ_CONFIG_RAS_ON_DEMAND_SUPPORT) &&
  // (CS_RREQ_CONFIG_RAS_ON_DEMAND_SUPPORT == 1)
}

#if defined(CS_RREQ_CONFIG_RAS_ON_DEMAND_SUPPORT) && (CS_RREQ_CONFIG_RAS_ON_DEMAND_SUPPORT == 1)
static void handle_ack_finished(rreq_t *rreq, const cs_rreq_ras_evt_t *evt)
{
  rreq_log_info(INSTANCE_PREFIX "RAS - ACK finished, [sc: 0x%lx], [response: 0x%lx]" LOG_NL,
                rreq->conn_handle,
                (unsigned long)evt->data.ack_finished.sc,
                (unsigned long)evt->data.ack_finished.response);
  rreq->ras_state = RAS_STATE_ON_DEMAND;
}

static void handle_ranging_data_ready(rreq_t *rreq, const cs_rreq_ras_evt_t *evt)
{
  cs_ras_ranging_counter_t ranging_counter = evt->data.ranging_data_ready.ranging_counter;
  sl_status_t sc;

  rreq_log_info(INSTANCE_PREFIX "RAS - ranging data ready, counter: %u" LOG_NL,
                rreq->conn_handle,
                ranging_counter);
  // write GET to RAS CP
  if (((ranging_counter & CS_RAS_RANGING_COUNTER_MASK) == rreq->ranging_counter)
      && (rreq->ras_overwritten == false || rreq->ranging_counter != ranging_counter)) {
    // Access ranging data based on the role
    cs_rreq_ranging_buffer_t *buffer = rreq->config.is_initiator
                                       ? &rreq->data.reflector
                                       : &rreq->data.initiator;
    sc = cs_ras_client_get_ranging_data(rreq->conn_handle,
                                        (uint16_t)ranging_counter,
                                        sizeof(buffer->data),
                                        buffer->data);
    if (sc != SL_STATUS_OK) {
      rreq_log_error(INSTANCE_PREFIX "RAS - failed to get ranging data! [sc: 0x%lx]" LOG_NL,
                     rreq->conn_handle,
                     (unsigned long)sc);
      rreq_error(rreq,
                 CS_RREQ_ERROR_RAS_CLIENT_GET_RANGING_DATA_FAILED,
                 sc);
      return;
    }
    rreq_log_info(INSTANCE_PREFIX "RAS - GET ranging data, counter: %u" LOG_NL,
                  rreq->conn_handle,
                  ranging_counter);
    rreq->ras_state = RAS_STATE_ON_DEMAND_GET;
  }
}

static void handle_abort_finished(rreq_t *rreq, const cs_rreq_ras_evt_t *evt)
{
  rreq_log_info(INSTANCE_PREFIX "RAS - abort finished, [sc: 0x%lx], [response: 0x%lx]" LOG_NL,
                rreq->conn_handle,
                (unsigned long)evt->data.abort_finished.sc,
                (unsigned long)evt->data.abort_finished.response);
  set_state(rreq, RREQ_STATE_IN_PROCEDURE);
  rreq_log_info(INSTANCE_PREFIX "Instance new state: IN_PROCEDURE" LOG_NL,
                rreq->conn_handle);
}

static void handle_ranging_data_overwritten(rreq_t *rreq, const cs_rreq_ras_evt_t *evt)
{
  cs_ras_ranging_counter_t ranging_counter = evt->data.ranging_data_overwritten.ranging_counter;
  rreq->ranging_counter = ranging_counter;
  rreq->ras_overwritten = true;
  rreq_log_info(INSTANCE_PREFIX "RAS - ranging data overwritten, counter: %u" LOG_NL,
                rreq->conn_handle,
                ranging_counter);
}
#endif // defined(CS_RREQ_CONFIG_RAS_ON_DEMAND_SUPPORT) && (CS_RREQ_CONFIG_RAS_ON_DEMAND_SUPPORT == 1)

static void handle_timeout(rreq_t *rreq, const cs_rreq_ras_evt_t *evt)
{
  rreq_log_debug(INSTANCE_PREFIX "RAS timeout: %u, action: %u" LOG_NL,
                 rreq->conn_handle,
                 evt->data.timeout.timeout,
                 evt->data.timeout.action);
  rreq_error(rreq,
             CS_RREQ_ERROR_RAS_CLIENT_TIMEOUT,
             SL_STATUS_TIMEOUT);
}

// -----------------------------------------------------------------------------
// Private functions

static void process_remote_ranging_data(rreq_t *rreq,
                                        uint8_t *data,
                                        uint32_t data_size)
{
  sl_status_t sc;
  cs_ras_ranging_header_t *ranging_header = (cs_ras_ranging_header_t *)data;
  rreq_log_info(INSTANCE_PREFIX "Ranging Data for Procedure %u arrived, size = %lu" LOG_NL,
                rreq->conn_handle,
                ranging_header->ranging_counter,
                data_size);

  // Pass Ranging Data to the state machine
  sm_evt_data_t evt;
  evt.evt_ranging_data.ranging_counter = ranging_header->ranging_counter;
  evt.evt_ranging_data.is_local = false;
  evt.evt_ranging_data.data = data;
  evt.evt_ranging_data.data_size = data_size;
  // Check done status of the procedure
  evt.evt_ranging_data.procedure_state = ranging_data_is_complete(data,
                                                                  data_size,
                                                                  rreq->config.is_initiator,
                                                                  rreq->num_antenna_path);
  sc = sm_on_evt(rreq, RREQ_EVT_RANGING_DATA, &evt);
  if (sc != SL_STATUS_OK) {
    rreq_log_error(INSTANCE_PREFIX "RAS - failed pass remote ranging data [sc: 0x%lx]" LOG_NL,
                   rreq->conn_handle,
                   (unsigned long)sc);
  }
}

#if defined(CS_RREQ_CONFIG_RAS_ON_DEMAND_SUPPORT) && (CS_RREQ_CONFIG_RAS_ON_DEMAND_SUPPORT == 1)
/******************************************************************************
 * Calculate start and end segment for lost segments
 *****************************************************************************/
static void get_lost_segments(uint64_t lost_segments,
                              uint8_t *start_segment,
                              uint8_t *end_segment)
{
  bool found_start_segment = false;
  uint64_t bitmask;
  for (uint64_t i = 0; i < sizeof(uint64_t) * 8; i++) {
    bitmask = 1ULL << i;
    if ((bitmask & lost_segments) > 0) {
      if (!found_start_segment) {
        *start_segment = i;
        *end_segment = i;
        found_start_segment = true;
      } else {
        *end_segment = i;
      }
    }
  }
}
#endif // defined(CS_RREQ_CONFIG_RAS_ON_DEMAND_SUPPORT) && (CS_RREQ_CONFIG_RAS_ON_DEMAND_SUPPORT == 1)
