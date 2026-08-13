/***************************************************************************//**
 * @file
 * @brief CS Manager internal API
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

#ifndef CS_MANAGER_INTERNAL_H
#define CS_MANAGER_INTERNAL_H

#include <stdbool.h>
#include <stdint.h>
#include "sl_status.h"
#include "sl_bt_api.h"
#include "app_rta.h"
#include "cs_manager_config.h"
#include "cs_manager.h"

// -----------------------------------------------------------------------------
// Enums, structs, typedefs

// CS Manager state type
typedef enum {
  CS_MANAGER_STATE_SETTING_CONN_PARAMS = 0,
  CS_MANAGER_STATE_SETTING_CONN_PHY = 1,
  CS_MANAGER_STATE_ENABLING_SECURITY = 2,
  CS_MANAGER_STATE_IDLE = 3,
  CS_MANAGER_STATE_PROCEDURE_ENABLING_SECURITY = 4,
  CS_MANAGER_STATE_PROCEDURE_ENABLING = 5,
  CS_MANAGER_STATE_PROCEDURE_ENABLED = 6,
  CS_MANAGER_STATE_PROCEDURE_DISABLING = 7,
  CS_MANAGER_STATE_CONFIGURING = 8,
  CS_MANAGER_STATE_REMOVING_CONFIG = 9
} cs_manager_state_t;

// CS Manager Instance Type
typedef struct {
  uint8_t conn_handle;
  cs_manager_state_t state;
  cs_manager_instance_config_t instance_config;
  cs_manager_connection_parameters_t connection_parameters;
  cs_procedure_parameters_t procedure_parameters;
  cs_procedure_parameter_info_t procedure_parameter_info;
  bool manage_connection_parameters;
  bool security_enabled;
  uint8_t active_config_id;
  cs_config_data_t configs[CS_MANAGER_CONFIG_COUNT];
} cs_manager_t;

// -----------------------------------------------------------------------------
// Module variables

// Event callback
extern cs_manager_event_t on_event;

// User error callback
extern cs_manager_on_error_t on_error;

// RTA guard context — shared between cs_manager.c and cs_manager_cs_control.c
extern app_rta_context_t cs_manager_ctx;

// Default connection parameters
extern cs_manager_connection_parameters_t default_connection_parameters;

#ifdef __cplusplus
extern "C" {
#endif

// -----------------------------------------------------------------------------
// Function declarations

/******************************************************************************
 * Initialize the CS Manager.
 *
 * @return Status of the operation.
 *****************************************************************************/
sl_status_t cs_manager_init(void);

/******************************************************************************
 * Create the app_rta guard context for CS Manager.
 * Called via app_rta_init template contribution.
 *****************************************************************************/
void cs_manager_rta_init(void);

/******************************************************************************
 * Find CS Manager instance for a given connection handle.
 *
 * @param[in] conn_handle The connection handle of the CS Manager instance.
 * @return Pointer to the CS Manager instance.
 *****************************************************************************/
cs_manager_t *cs_manager_find(uint8_t conn_handle);

/******************************************************************************
 * Log an error and dispatch it to the registered user error callback.
 *
 * Mirrors the @c rreq_error helper in cs_rreq. May be safely called with a
 * NULL @p m, in which case @ref SL_BT_INVALID_CONNECTION_HANDLE is reported
 * as the connection handle.
 *
 * @param[in] m   Instance reference (may be NULL for non-instance errors).
 * @param[in] evt CS Manager error event identifier.
 * @param[in] sc  Underlying status code.
 *****************************************************************************/
void cs_manager_error(cs_manager_t *m,
                      cs_manager_error_t evt,
                      sl_status_t sc);

// -----------------------------------------------------------------------------
// Event / callback declarations

/******************************************************************************
 * Process a Bluetooth event.
 *
 * @param[in] evt The Bluetooth event to process.
 *****************************************************************************/
void cs_manager_on_bt_event(const sl_bt_msg_t *evt);

#ifdef __cplusplus
}
#endif

#endif // CS_MANAGER_INTERNAL_H
