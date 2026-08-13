/***************************************************************************//**
 * @file
 * @brief CS Initiator example configuration
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

// <<< Use Configuration Wizard in Context Menu >>>

// <o SYSTEM_MIN_TX_POWER_DBM> Connection minimum TX Power <-127..20>
// <i> Connection minimum TX Power in dBm
// <i> Default: -3
#define SYSTEM_MIN_TX_POWER_DBM               -3

// <o SYSTEM_MAX_TX_POWER_DBM> Connection maximum TX Power <-127..20>
// <i> Connection minimum TX Power in dBm. Must be greater than the minimum value.
// <i> Default: 20
#define SYSTEM_MAX_TX_POWER_DBM               20

// <q ALWAYS_INIT_TRACE> Enable trace on init
// <i> If enabled, this option bypasses trace initialization with push button.
// <i> Default: 0
#define ALWAYS_INIT_TRACE                     0

// <q CS_INITIATOR_UART_LOG> Enable initiator log on UART
// <i> Default: 1
// <i> Enable or disable VCOM logging alongside RTT logging to save power.
#define CS_INITIATOR_UART_LOG                 1

// <s CS_INITIATOR_DEVICE_NAME> Device name
// <i> Default: "CS RFLCT"
#define REFLECTOR_DEVICE_NAME "CS RFLCT"

// <o CS_HEADER_LOG> Header log of measurements results <1..20>
// <i> Sets how many measurements are written between header logs
// <i> Default: 5
#define CS_HEADER_LOG                      5

// <q CS_CAPABILITIES_LOG> Log local/remote CS supported capabilities table
// <i> Enable human-readable capability comparison on remote capabilities read.
// <i> Output uses log_debug; runtime filtering is handled by app_log.
// <i> Default: 1
#define CS_CAPABILITIES_LOG                 1

// <o CS_ANTENNA_OFFSET> Specify antenna offset
// <0=> Wireless antenna offset
// <1=> Wired antenna offset
// <i> Default: 0
#define CS_ANTENNA_OFFSET                  0

// <<< end of configuration section >>>

#endif // APP_CONFIG_H
