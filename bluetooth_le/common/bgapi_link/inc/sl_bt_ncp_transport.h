/***************************************************************************//**
 * @file
 * @brief Bluetooth NCP transport interface.
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
#ifndef SL_BT_NCP_TRANSPORT_H
#define SL_BT_NCP_TRANSPORT_H

#include <stdint.h>
#include "sl_status.h"

/// Bluetooth NCP transport error type.
typedef enum {
  SL_BT_NCP_TRANSPORT_ERROR_INIT        = 0, ///< Transport initialization failed.
  SL_BT_NCP_TRANSPORT_ERROR_RUNTIME     = 1, ///< Runtime context reported an error.
  SL_BT_NCP_TRANSPORT_ERROR_COM_HANDLE  = 2, ///< Communication handle is invalid.
  SL_BT_NCP_TRANSPORT_ERROR_TX_OVERFLOW = 3, ///< Transmit path overflowed.
  SL_BT_NCP_TRANSPORT_ERROR_CLOCK       = 4, ///< Clock configuration failed.
  SL_BT_NCP_TRANSPORT_ERROR_CLOSE       = 5, ///< Transport close failed.
  SL_BT_NCP_TRANSPORT_ERROR_RX          = 6  ///< Receive path failed.
} sl_bt_ncp_transport_error_t;

/**************************************************************************//**
 * Transmit one complete NCP payload on the selected transport.
 *
 * @param[in] length Number of bytes in @p data.
 * @param[in] data Bytes to transmit.
 *****************************************************************************/
void sl_bt_ncp_transport_transmit(uint32_t length, const uint8_t *data);

/**************************************************************************//**
 * Called when a transmit started by @ref sl_bt_ncp_transport_transmit completes.
 *
 * @param[in] status SL_STATUS_OK on success, otherwise an error code.
 *****************************************************************************/
void sl_bt_ncp_transport_on_transmit(sl_status_t status);

/**************************************************************************//**
 * Drive receive processing on transports that poll for incoming data.
 *
 * The CPC binding does not use this entry point; reception is delivered
 * through @ref sl_bt_ncp_transport_on_receive from the link callback.
 *****************************************************************************/
void sl_bt_ncp_transport_receive(void);

/**************************************************************************//**
 * Deliver a received NCP payload to the NCP protocol layer.
 *
 * @param[in] status SL_STATUS_OK on success, otherwise an error code.
 * @param[in] length Number of bytes in @p data.
 * @param[in] data Received bytes. Valid only for the duration of the call.
 *****************************************************************************/
void sl_bt_ncp_transport_on_receive(sl_status_t status,
                                    uint32_t length,
                                    uint8_t *data);

/**************************************************************************//**
 * Report a transport error to the application.
 *
 * @param[in] error Error class.
 * @param[in] status Status code from the failing operation.
 *****************************************************************************/
void sl_bt_ncp_transport_on_error(sl_bt_ncp_transport_error_t error,
                                  sl_status_t status);

#endif // SL_BT_NCP_TRANSPORT_H
