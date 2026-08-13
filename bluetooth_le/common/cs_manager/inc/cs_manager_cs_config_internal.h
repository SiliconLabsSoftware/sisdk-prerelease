/***************************************************************************//**
 * @file
 * @brief CS Manager - CS configuration internal API
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

#ifndef CS_MANAGER_CS_CONFIG_INTERNAL_H
#define CS_MANAGER_CS_CONFIG_INTERNAL_H

#include <stdbool.h>
#include <stdint.h>
#include "sl_status.h"
#include "sl_bt_api.h"
#include "cs_manager_config.h"
#include "cs_manager.h"

#ifdef __cplusplus
extern "C" {
#endif

/******************************************************************************
 * Create a new CS configuration for a connection.
 *
 * @param[in] conn_handle    Connection handle.
 * @param[in] config_id      Configuration identifier to create.
 * @param[in] create_context true to create the configuration on the local
 *                           device, false for the remote device.
 * @param[in] config         Pointer to the CS config complete event data.
 * @return Status of the operation.
 * @retval SL_STATUS_OK               Configuration creation started.
 * @retval SL_STATUS_NULL_POINTER     @p config is NULL.
 * @retval SL_STATUS_NOT_FOUND        No instance found for @p conn_handle.
 * @retval SL_STATUS_INVALID_STATE    Instance is not in
 *                                    @ref CS_MANAGER_STATE_IDLE.
 * @retval SL_STATUS_NO_MORE_RESOURCE Maximum number of configurations reached.
 *****************************************************************************/
sl_status_t cs_manager_cs_config_create(uint8_t conn_handle,
                                        uint8_t config_id,
                                        bool create_context,
                                        const cs_config_t *config);

/******************************************************************************
 * Remove a CS configuration from a CS Manager instance.
 *
 * @param[in] conn_handle Connection handle.
 * @param[in] config_id   Configuration identifier to remove.
 * @return Status of the operation.
 * @retval SL_STATUS_OK            Configuration removed successfully.
 * @retval SL_STATUS_NOT_FOUND     No instance or configuration found for the
 *                                 given @p conn_handle and @p config_id.
 * @retval SL_STATUS_INVALID_STATE Instance is not in
 *                                 @ref CS_MANAGER_STATE_IDLE.
 *****************************************************************************/
sl_status_t cs_manager_cs_config_remove(uint8_t conn_handle,
                                        uint8_t config_id);

/******************************************************************************
 * Populate a cs_config_t structure with default CS configuration values.
 *
 * @param[out] config Pointer to the configuration structure to populate.
 * @return Status of the operation.
 * @retval SL_STATUS_OK               Parameters populated successfully.
 * @retval SL_STATUS_NULL_POINTER     @p config is NULL.
 *****************************************************************************/
sl_status_t cs_manager_cs_config_get_default_config(cs_config_t *config);

#ifdef __cplusplus
}
#endif

#endif // CS_MANAGER_CS_CONFIG_INTERNAL_H
