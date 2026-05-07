/***************************************************************************//**
 * @file
 * @brief CS Manager API
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

#ifndef CS_MANAGER_H
#define CS_MANAGER_H

// -----------------------------------------------------------------------------
// Includes

#include <stdbool.h>
#include <stdint.h>
#include "sl_status.h"
#include "sl_bt_api.h"
#include "cs_manager_config.h"

#ifdef __cplusplus
extern "C" {
#endif

// -----------------------------------------------------------------------------
// Enums, structs, typedefs

typedef enum {
  CS_MANAGER_EVENT_INSTANCE_CREATED,
  CS_MANAGER_EVENT_INSTANCE_REMOVED,
  CS_MANAGER_EVENT_INSTANCE_ERROR,
  CS_MANAGER_EVENT_CONFIG_REMOVED,
  CS_MANAGER_EVENT_CONFIG_CREATED,
  CS_MANAGER_EVENT_CONFIG_OVERWRITTEN,
  CS_MANAGER_EVENT_CONFIG_ERROR,
  CS_MANAGER_EVENT_PROCEDURE_STARTED,
  CS_MANAGER_EVENT_PROCEDURE_STOPPED,
  CS_MANAGER_EVENT_PROCEDURE_ERROR,
  CS_MANAGER_EVENT_ERROR,
} cs_manager_event_type_t;

// -----------------------------------------------------------------------------
// Function declarations

/**************************************************************************//**
 * Create a new CS Manager instance.
 *
 * @param[in] conn_handle the connection handle to create the instance for
 *
 * @return status of the operation
 *****************************************************************************/
sl_status_t cs_manager_create(uint8_t conn_handle);

/**************************************************************************//**
 * Delete a CS Manager instance.
 *
 * @param[in] conn_handle the connection handle to delete the instance for
 *
 * @return status of the operation
 *****************************************************************************/
sl_status_t cs_manager_delete(uint8_t conn_handle);

/**************************************************************************//**
 * Create a new CS configuration.
 *
 * @param[in] conn_handle the connection handle to create the configuration for
 * @param[in] config_id the configuration identifier to create
 * @param[in] create_context Defines in which device the created configuration
 *                           will be written
 * @param[in] config the CS configuration to create
 *
 * @return status of the operation
 *****************************************************************************/
sl_status_t cs_manager_config_create(uint8_t conn_handle,
                                     uint8_t config_id,
                                     bool create_context,
                                     const sl_bt_evt_cs_config_complete_t *config);

/**************************************************************************//**
 * Remove a CS configuration from the CS Manager.
 *
 * @param[in] conn_handle the connection handle to remove the configuration from
 * @param[in] config_id the configuration identifier to remove
 *
 * @return status of the operation
 *****************************************************************************/
sl_status_t cs_manager_config_remove(uint8_t conn_handle, uint8_t config_id);

// -----------------------------------------------------------------------------
// Event / callback declarations (starts with on_)

/**************************************************************************//**
 * Optional application notification for CS Manager lifecycle events (configs
 * stored, updated, removed). Provide a non-weak definition in application code.
 *
 * @param[in] event Kind of event (@ref cs_manager_event_type_t).
 * @param[in] conn_handle Bluetooth connection handle.
 * @param[in] config_id Configuration identifier (meaningful for config events).
 *****************************************************************************/
void cs_manager_on_event(cs_manager_event_type_t event,
                         uint8_t conn_handle,
                         uint8_t config_id);

#ifdef __cplusplus
};
#endif

#endif // CS_MANAGER_H
