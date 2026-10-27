/***************************************************************************//**
 * @file
 * @brief BGAPI over cpc-ng component configuration macros.
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
#ifndef SL_BT_BGAPI_LINK_CONFIG_H
#define SL_BT_BGAPI_LINK_CONFIG_H

/***********************************************************************************************//**
 * @addtogroup sl_bt_bgapi_link
 * @{
 **************************************************************************************************/

// <<< Use Configuration Wizard in Context Menu >>>

// <h> BGAPI over cpc-ng configuration

// <o SL_BT_BGAPI_LINK_ENDPOINT_ID> Endpoint identifier <1-255>
// <i> The same endpoint identifier must be configured on both devices.
// <i> Endpoint 2 is conventionally assigned to Bluetooth BGAPI.
// <i> Default: 2
#define SL_BT_BGAPI_LINK_ENDPOINT_ID                   2

// <o SL_BT_BGAPI_LINK_RX_DEPTH> Receive message depth <1-16>
// <i> Number of complete BGAPI messages that may be held concurrently.
// <i> Default: 2
#define SL_BT_BGAPI_LINK_RX_DEPTH                      2

// <o SL_BT_BGAPI_LINK_MESSAGE_SIZE> Maximum BGAPI message size [bytes] <4-2051>
// <i> Size of each receive and transmit buffer, including the BGAPI header.
// <i> This may be smaller than the protocol maximum configured by
// <i> SL_BGAPI_MAX_PAYLOAD_SIZE.
// <i> Default: 260
#define SL_BT_BGAPI_LINK_MESSAGE_SIZE                   260

// </h>

// <<< end of configuration section >>>

/** @} (end addtogroup sl_bt_bgapi_link) */

#endif // SL_BT_BGAPI_LINK_CONFIG_H
