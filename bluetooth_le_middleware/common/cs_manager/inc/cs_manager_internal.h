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
#include "cs_manager_config.h"
#include "cs_manager.h"

// -----------------------------------------------------------------------------
// Enums, structs, typedefs

// CS Manager state type
typedef enum {
 CS_MANAGER_STATE_SETTING_CONN_PARAMS = 0,
 CS_MANAGER_STATE_ENABLING_SECURITY = 1,
 CS_MANAGER_STATE_IDLE = 2,
 CS_MANAGER_STATE_PROCEDURE_ENABLING_SECURITY = 3,
 CS_MANAGER_STATE_PROCEDURE_ENABLING = 4,
 CS_MANAGER_STATE_PROCEDURE_ENABLED = 5,
 CS_MANAGER_STATE_PROCEDURE_DISABLING = 6,
 CS_MANAGER_STATE_CONFIGURING = 7,
 CS_MANAGER_STATE_REMOVING_CONFIG = 8
} cs_manager_state_t;

// CS Manager Instance Type
typedef struct {
  uint8_t conn_handle;
  cs_manager_state_t state;
  cs_manager_instance_config_t instance_config;
  cs_manager_connection_parameters_t connection_parameters;
  cs_procedure_parameters_t procedure_parameters;
  bool manage_connection_parameters;
  bool security_enabled;
  uint8_t active_config_id;
  cs_config_data_t configs[CS_MANAGER_CONFIG_COUNT];
  //TODO: states, etc.
} cs_manager_t;


// -----------------------------------------------------------------------------
// Module variables

// Event callback
extern cs_manager_event_t on_event;

#ifdef __cplusplus
extern "C" {
#endif

// -----------------------------------------------------------------------------
// Function declarations

/**************************************************************************//**
 * Initialize the CS Manager.
 *
 * @return Status of the operation.
 *****************************************************************************/
sl_status_t cs_manager_init(void);

/**************************************************************************//**
 * Find CS Manager instance for a given connection handle.
 * 
 * @param[in] conn_handle The connection handle of the CS Manager instance.
 * @return Pointer to the CS Manager instance.
 *****************************************************************************/
cs_manager_t *cs_manager_find(uint8_t conn_handle);

// -----------------------------------------------------------------------------
// Event / callback declarations

/**************************************************************************//**
 * Process a Bluetooth event.
 *
 * @param[in] evt The Bluetooth event to process.
 *****************************************************************************/
void cs_manager_on_bt_event(const sl_bt_msg_t *evt);

#ifdef __cplusplus
};
#endif

#endif // CS_MANAGER_INTERNAL_H
