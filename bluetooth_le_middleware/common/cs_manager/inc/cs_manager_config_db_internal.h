/***************************************************************************//**
 * @file
 * @brief CS Manager configuration database (internal)
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

#ifndef CS_MANAGER_CONFIG_DB_INTERNAL_H
#define CS_MANAGER_CONFIG_DB_INTERNAL_H

// -----------------------------------------------------------------------------
// Includes

#include "sl_status.h"
#include "sl_bt_api.h"

#define CS_MANAGER_INVALID_CONFIG_ID 0xFF

#ifdef __cplusplus
extern "C" {
#endif

// -----------------------------------------------------------------------------
// Function declarations

/**************************************************************************//**
 * Create a new configuration in the database.
 *
 * @param[in] config Pointer to the new configuration.
 *
 * @return SL_STATUS_OK if created, SL_STATUS_FULL if database is full, 
 *         SL_STATUS_NULL_POINTER if config is NULL.
 *****************************************************************************/
sl_status_t cs_manager_config_db_create(const cs_config_data_t *config);

/**************************************************************************//**
 * Get a stored configuration from the database.
 *
 * @param[in] conn_handle Connection handle.
 * @param[in] config_id Configuration identifier.
 * @param[out] config_out Pointer to the configuration to store the result in.
 *
 * @return SL_STATUS_OK if found, SL_STATUS_NOT_FOUND if no matching entry.
 *****************************************************************************/
sl_status_t cs_manager_config_db_get(uint8_t conn_handle,
                                     uint8_t config_id,
                                     cs_config_data_t *config_out);

/**************************************************************************//**
 * Remove a stored configuration from the database.
 *
 * On success, sets @p *config_out to the configuration backing store for that
 * slot inside the CS Manager DB, marks the slot inactive, then returns. The
 * struct remains in memory until the slot is repurposed—do not cache the pointer
 * past the lifetime of that slot assignment.
 *
 * @param[in] conn_handle Connection handle.
 * @param[in] config_id Configuration identifier.
 *
 * @return SL_STATUS_OK if removed, SL_STATUS_NOT_FOUND if no matching entry.
 *****************************************************************************/
sl_status_t cs_manager_config_db_remove(uint8_t conn_handle,
                                        uint8_t config_id);

#ifdef __cplusplus
};
#endif

#endif // CS_MANAGER_CONFIG_DB_INTERNAL_H
