/***************************************************************************//**
 * @file
 * @brief CS RREQ types header
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
#ifndef CS_RREQ_TYPES_H
#define CS_RREQ_TYPES_H

#include <stdint.h>
#include <stdbool.h>
#include "sl_bt_api.h"
#include "sl_status.h"
#include "cs_rreq.h"

// CS procedure completion state type
typedef enum {
  CS_PROCEDURE_STATE_IN_PROGRESS = 0u, // Procedure is still collecting subevent data
  CS_PROCEDURE_STATE_ABORTED,          // Procedure was aborted by the controller
  CS_PROCEDURE_STATE_COMPLETED         // All subevent data collected; procedure done
} cs_procedure_state_t;

// CS result event data extracted from sl_bt_evt_cs_result / sl_bt_evt_cs_result_continue
typedef struct {
  sl_bt_msg_t *cs_event;         // Pointer to the originating BT stack event
  uint8_t procedure_done_status; // Procedure_Done_Status field from the BT stack event
  uint8_t subevent_done_status;  // Subevent_Done_Status field from the BT stack event
  uint8_t num_steps;             // Number of steps in this event
  bool first_cs_result;          // True for sl_bt_evt_cs_result, false for _continue
} cs_result_data_t;

// Ranging data carried in a RREQ_EVT_RANGING_DATA state machine event
typedef struct {
  uint8_t *data;                        // Pointer to the formatted RAS ranging data body
  uint32_t data_size;                   // Number of valid bytes in data
  uint16_t ranging_counter;             // Ranging procedure counter
  cs_procedure_state_t procedure_state; // Whether the procedure completed or was aborted
  bool is_local;                        // True for locally captured (initiator) data,
                                        // false for remotely received (reflector) data
} ranging_data_t;

// RAS client internal transfer state
typedef enum {
  RAS_STATE_IDLE = 0,               // No active transfer
  RAS_STATE_REAL_TIME,              // Receiving Real-Time Ranging Data notifications
  RAS_STATE_ON_DEMAND,              // On-demand transfer in progress
  RAS_STATE_ON_DEMAND_RETRIEVE_LOST,// Retrieving lost segments
  RAS_STATE_ON_DEMAND_ACK,          // Sending acknowledgement
  RAS_STATE_ON_DEMAND_GET,          // Requesting ranging data from the server
  RAS_STATE_ON_DEMAND_ABORT,        // Aborting an on-demand transfer
} ras_state_t;

// RREQ state machine event identifier type
typedef enum {
  RREQ_EVT_INIT_STARTED = 0U,  // Instance created; RAS client initialization started
  RREQ_EVT_INIT_COMPLETED,     // RAS client initialized and server features read
  RREQ_EVT_ENABLE,             // Enable or disable ranging requested
  RREQ_EVT_ENABLE_COMPLETED,   // Enable/disable procedure has completed
  RREQ_EVT_DISABLE,            // Disable ranging requested
  RREQ_EVT_CS_RESULT,          // First CS result subevent received
  RREQ_EVT_CS_RESULT_CONTINUE, // Continuation CS result subevent received
  RREQ_EVT_RANGING_DATA,       // Reflector ranging data received via RAS
  RREQ_EVT_ERROR               // Unrecoverable error occurred
} sm_evt_t;

// RREQ instance state machine state type
typedef enum {
  RREQ_STATE_UNINITIALIZED = 0,     // Not yet created
  RREQ_STATE_INIT,                  // RAS client initialization in progress
  RREQ_STATE_DISABLED,              // Initialized; ranging not active
  RREQ_STATE_ENABLING,              // Enable procedure in progress (e.g., mode change)
  RREQ_STATE_IN_PROCEDURE,          // Ranging active; collecting CS result subevents
  RREQ_STATE_WAIT_REMOTE_COMPLETE,  // Local procedure done; waiting for reflector data
  RREQ_STATE_WAIT_REMOTE_ABORT,     // Local procedure aborted; waiting for reflector abort
  RREQ_STATE_DELETE,                // Teardown in progress
  RREQ_STATE_DISABLING,             // Disable procedure in progress
  RREQ_STATE_ERROR                  // Unrecoverable error; instance is unusable
} rreq_state_t;

// RREQ configuration type
typedef struct {
  uint8_t real_time_mode;                 // Real-time (1) or on-demand (0) RAS mode
  cs_ras_client_config_t ras_config;      // RAS client CCCD configuration
  uint32_t service;                       // Ranging Service GATT handle
  cs_ras_gattdb_handles_t gattdb_handles; // Ranging characteristic GATT database handles
  uint16_t mtu;                           // ATT MTU negotiated for the connection
  uint8_t antenna_config;                 // CS tone antenna configuration index (0-7)
  uint8_t is_initiator;                   // Role: 1 for Initiator, 0 for Reflector
} cs_rreq_config_t;

// RREQ instance type
typedef struct {
  uint8_t conn_handle;                           // Bluetooth connection handle
  uint16_t ranging_counter;                      // Counter of the current ranging procedure
  cs_rreq_result_t data;                         // Accumulated ranging result buffer
  uint8_t num_antenna_path;                      // Number of antenna paths from the last CS result
  cs_rreq_config_t config;                       // Runtime configuration (unpacked copy)
  rreq_state_t state;                            // Current state machine state
  cs_ras_subevent_header_t *last_subevent_header;// Pointer to the last written subevent header
  uint8_t subevents_per_procedure_counter;       // Number of subevents received in current procedure
  uint8_t drop_counter;                          // Consecutive CS results dropped while waiting
  ras_state_t ras_state;                         // Current RAS client transfer state
  bool ras_overwritten;                          // True if ranging data was overwritten on the server
  cs_rreq_procedure_info_t procedure_info;       // Procedure information (timing),
} rreq_t;

// State machine event data payload: active member depends on the event type
typedef union {
  struct {
    cs_rreq_error_t error; // Error event identifier
    sl_status_t status;    // Underlying status code
  } evt_error;                             // Payload for RREQ_EVT_ERROR
  cs_result_data_t evt_cs_result;          // Payload for RREQ_EVT_CS_RESULT / _CONTINUE
  ranging_data_t evt_ranging_data;         // Payload for RREQ_EVT_RANGING_DATA
  sl_status_t evt_init_completed;          // Payload for RREQ_EVT_INIT_COMPLETED (init status)
  struct {
    cs_rreq_enable_t enable; // Requested enable state
    sl_status_t status;      // Result status
  } evt_enable_completed;                  // Payload for RREQ_EVT_ENABLE_COMPLETED
} sm_evt_data_t;

#endif // CS_RREQ_TYPES_H
