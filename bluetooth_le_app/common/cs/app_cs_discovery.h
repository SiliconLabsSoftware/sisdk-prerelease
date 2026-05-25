/***************************************************************************//**
 * @file
 * @brief CS example Bluetooth Manager
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

#ifndef APP_CS_DISCOVERY_H
#define APP_CS_DISCOVERY_H

#ifdef __cplusplus
extern "C" {
#endif

#include "sl_bt_api.h"
#include "cs_ras_common.h"

//Types

typedef struct {
  uint8_t conn_handle;
  uint32_t service_handle;
  cs_ras_gattdb_handles_t gattdb_handles;
  sl_status_t status;
} app_cs_discovery_result_t;

typedef void (*app_cs_discovery_complete_cb_t)(const app_cs_discovery_result_t *result);

/**************************************************************************//**
 * Start the discovery.
 *
 * @param[in] conn_handle Connection handle.
 * @param[in] cb Callback to be called when the discovery is complete.
 * @return Status of the operation.
 * @retval SL_STATUS_OK Discovery started successfully.
 * @retval SL_STATUS_INVALID_HANDLE Connection handle is invalid.
 * @retval SL_STATUS_NULL_POINTER Callback is NULL.
 *****************************************************************************/
sl_status_t app_cs_discovery_start_discovery(uint8_t conn_handle, 
                                             app_cs_discovery_complete_cb_t cb);

/**************************************************************************//**
 * Handle a Bluetooth event.
 *
 * @param[in] evt Bluetooth event.
 *****************************************************************************/
void app_cs_discovery_on_bt_event(const sl_bt_msg_t *evt);

#ifdef __cplusplus
}
#endif

#endif // APP_CS_DISCOVERY_H