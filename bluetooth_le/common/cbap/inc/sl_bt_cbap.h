/***************************************************************************//**
 * @file
 * @brief Certificate Based Authentication and Pairing header
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
#ifndef SL_BT_CBAP_H
#define SL_BT_CBAP_H

// -----------------------------------------------------------------------------
// Includes

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "sl_status.h"
#include "sl_bt_api.h"
#include "sl_bt_cbap_config.h"

// -----------------------------------------------------------------------------
// Defines

/// GATT UUIDs
#define SL_BT_CBAP_SERVICE_UUID                   \
  0x10, 0x56, 0x28, 0xd0, 0x40, 0xdd, 0x8e, 0x91, \
  0x4d, 0x41, 0x47, 0x81, 0xc6, 0x8c, 0x81, 0xd8
#define SL_BT_CBAP_BATCH_CERT_SERVER_CHAR_UUID    \
  0xe9, 0x4c, 0x3d, 0x5b, 0x8e, 0x9e, 0xb5, 0x81, \
  0x2e, 0x40, 0x1a, 0x19, 0x37, 0x69, 0x18, 0x31
#define SL_BT_CBAP_BATCH_CERT_CLIENT_CHAR_UUID    \
  0xca, 0x25, 0xf3, 0xa2, 0xf6, 0xda, 0xbd, 0xb6, \
  0x69, 0x4a, 0xaa, 0x08, 0xf9, 0xd0, 0x5f, 0x31
#define SL_BT_CBAP_DEVICE_CERT_SERVER_CHAR_UUID   \
  0x16, 0xef, 0xa6, 0xce, 0x9b, 0xdc, 0x64, 0x9b, \
  0xc3, 0x48, 0xe7, 0x8c, 0x11, 0x08, 0x98, 0xbb
#define SL_BT_CBAP_DEVICE_CERT_CLIENT_CHAR_UUID   \
  0x66, 0x50, 0xfd, 0x84, 0x4d, 0xad, 0xa2, 0x99, \
  0xc9, 0x4f, 0xf5, 0x16, 0x9e, 0xda, 0xf6, 0x0c
#define SL_BT_CBAP_OOB_DATA_SERVER_CHAR_UUID      \
  0x1c, 0x90, 0x0f, 0xe0, 0xfb, 0x08, 0xa1, 0x90, \
  0xef, 0x4d, 0xe4, 0xc4, 0xcd, 0x6c, 0x5c, 0xc0
#define SL_BT_CBAP_OOB_DATA_CLIENT_CHAR_UUID      \
  0x2c, 0x19, 0xf1, 0xeb, 0x85, 0xcd, 0xb6, 0x8a, \
  0x2c, 0x4e, 0x7d, 0x89, 0x51, 0x57, 0xd3, 0xe8

typedef enum {
  SL_BT_CBAP_CHAR_BATCH_CERT_SERVER,
  SL_BT_CBAP_CHAR_BATCH_CERT_CLIENT,
  SL_BT_CBAP_CHAR_DEVICE_CERT_SERVER,
  SL_BT_CBAP_CHAR_DEVICE_CERT_CLIENT,
  SL_BT_CBAP_CHAR_OOB_DATA_SERVER,
  SL_BT_CBAP_CHAR_OOB_DATA_CLIENT,
  SL_BT_CBAP_CHAR_COUNT
} sl_bt_cbap_characteristics_t;

/// Connection properties
typedef struct {
  uint8_t handle;
  bd_addr address;
} sl_bt_cbap_conn_t;

/**************************************************************************//**
 * Callback function type
 *****************************************************************************/
typedef void (*sl_bt_cbap_cb_t)(sl_bt_cbap_conn_t conn, sl_status_t sc);

// -----------------------------------------------------------------------------
// Public function declarations

#ifdef __cplusplus
extern "C" {
#endif

/**************************************************************************//**
 * Initialize CBAP BLE module.
 *
 * @param[in] cb Callback function pointer
 * @param[in] central Set to true if the device shall be in the central
 * (client) role. For peripheral (server) role, set to false.
 *
 * @return SL_STATUS_OK on success, error code otherwise.
 *****************************************************************************/
sl_status_t sl_bt_cbap_init(sl_bt_cbap_cb_t cb, bool central);

/**************************************************************************//**
 * Check whether the component currently owns a CBAP procedure.
 *
 * A procedure remains in progress from connection opening until it completes,
 * or until the failed connection has been cleaned up. An authenticated
 * connection that was handed over to the application does not keep CBAP busy.
 *
 * @return true if a CBAP procedure is in progress, false otherwise.
 *****************************************************************************/
bool sl_bt_cbap_is_procedure_in_progress(void);

#ifdef __cplusplus
};
#endif

#endif // SL_BT_CBAP_H
