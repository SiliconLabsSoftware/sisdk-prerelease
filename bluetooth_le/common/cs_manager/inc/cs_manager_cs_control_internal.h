/***************************************************************************//**
 * @file
 * @brief CS Manager - CS Procedure control internal API
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

#ifndef CS_MANAGER_CS_CONTROL_INTERNAL_H
#define CS_MANAGER_CS_CONTROL_INTERNAL_H

#include <stdbool.h>
#include <stdint.h>
#include "sl_status.h"
#include "cs_manager.h"

#ifdef __cplusplus
extern "C" {
#endif

/******************************************************************************
 * Start a CS ranging procedure on a connection.
 *
 * @param[in] conn_handle          Connection handle.
 * @param[in] config_id            Configuration identifier to use.
 * @param[in] params               Pointer to the procedure scheduling and
 *                                 PHY parameters.
 * @return Status of the operation.
 * @retval SL_STATUS_OK            Procedure started successfully.
 * @retval SL_STATUS_NULL_POINTER  @p params is NULL.
 * @retval SL_STATUS_NOT_FOUND     No instance or configuration found for the
 *                                 given @p conn_handle and @p config_id.
 * @retval SL_STATUS_INVALID_STATE The instance is not in a state that
 *                                 allows starting a procedure.
 *****************************************************************************/
sl_status_t cs_manager_cs_control_start(uint8_t conn_handle,
                                        uint8_t config_id,
                                        cs_procedure_parameters_t *params);

/******************************************************************************
 * Stop the currently running CS ranging procedure on a connection.
 *
 * @param[in] conn_handle Connection handle.
 * @return Status of the operation.
 * @retval SL_STATUS_OK        Procedure stopped successfully.
 * @retval SL_STATUS_NOT_FOUND No active instance found for @p conn_handle.
 *****************************************************************************/
sl_status_t cs_manager_cs_control_stop(uint8_t conn_handle);

/******************************************************************************
 * Get default procedure parameters.
 *
 * @param[out] params Pointer to the procedure parameters.
 * @return Status of the operation.
 * @retval SL_STATUS_OK               Parameters populated successfully.
 * @retval SL_STATUS_NULL_POINTER     @p params is NULL.
 *****************************************************************************/
sl_status_t cs_manager_cs_control_get_default_procedure_parameters(cs_procedure_parameters_t *params);

/******************************************************************************
 * Process a Bluetooth event.
 *
 * @param[in] evt The Bluetooth event to process.
 *****************************************************************************/
void cs_manager_cs_control_on_bt_event(const sl_bt_msg_t *evt);

#ifdef __cplusplus
}
#endif

#endif // CS_MANAGER_CS_CONTROL_INTERNAL_H
