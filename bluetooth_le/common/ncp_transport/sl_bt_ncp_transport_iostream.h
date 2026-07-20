/***************************************************************************//**
 * @file
 * @brief Bluetooth NCP transport layer over IO Stream header
 *******************************************************************************
 * # License
 * <b>Copyright 2025 Silicon Laboratories Inc. www.silabs.com</b>
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

#ifndef SL_BT_NCP_TRANSPORT_IOSTREAM_H
#define SL_BT_NCP_TRANSPORT_IOSTREAM_H

#include "sl_iostream.h"

#ifdef __cplusplus
extern "C" {
#endif

/**************************************************************************//**
 * Get the currently selected IO stream handle used by the NCP transport.
 *
 * @return IO stream handle used by the transport.
 *****************************************************************************/
sl_iostream_t *sl_bt_ncp_transport_get_stream_handle(void);

/**************************************************************************//**
 * Override the IO stream handle used by the NCP transport.
 *
 * The caller is responsible for ensuring that the transport is idle before
 * changing the stream handle. Switching the stream while a transmission is
 * ongoing, or while new BGAPI traffic can still be generated, may route a
 * packet to the wrong transport or corrupt the host-controller communication.
 *
 * It is recommended to reconfigure the stream only when the application has
 * quiesced Bluetooth activity. For example, stop scanners and advertisers,
 * close active connections, and make sure no other code path can trigger NCP
 * traffic until the new stream handle is in place.
 *
 * @param stream_handle IO stream handle to use for transport operations.
 *****************************************************************************/
void sl_bt_ncp_transport_set_stream_handle(sl_iostream_t *stream_handle);

#ifdef __cplusplus
}
#endif

#endif // SL_BT_NCP_TRANSPORT_IOSTREAM_H
