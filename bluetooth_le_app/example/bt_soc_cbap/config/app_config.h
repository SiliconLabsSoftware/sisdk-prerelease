/***************************************************************************//**
 * @file
 * @brief Certificate Based Authentication and Pairing application configuration
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

#ifndef APP_CONFIG_H
#define APP_CONFIG_H

#include "sl_bt_api.h"

// <<< Use Configuration Wizard in Context Menu >>>

// <o CONNECTION_ROLE> Connection role
//   <sl_bt_connection_role_peripheral=> Peripheral
//   <sl_bt_connection_role_central=> Central
// <i> Default: Peripheral
#define CONNECTION_ROLE   sl_bt_connection_role_peripheral

// <e ADDR_ENABLE> Scanning configuration
// <i> Default: 0
#define ADDR_ENABLE       0

// <s.17 ADDR> Target Address
// <i> Can only take effect if the central role is selected.
// <i> Default: "00:00:00:00:00:00"
#define ADDR              "00:00:00:00:00:00"

// </e>

// <o DISALLOWLIST_SIZE> Disallowlist size <1..255>
// <i> Number of remote devices the application can refuse to authenticate
// <i> again. A device is added to the disallowlist when the CBAP procedure with
// <i> it fails, and the application does not start a new procedure with a
// <i> device on the list. The execution is asserted if a device has to be added
// <i> to a full disallowlist.
// <i> Default: 4
#define DISALLOWLIST_SIZE 4

// <<< end of configuration section >>>

#endif // APP_CONFIG_H
