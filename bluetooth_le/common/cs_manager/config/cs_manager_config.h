/***************************************************************************//**
 * @file
 * @brief CS Manager - configuration header
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

#ifndef CS_MANAGER_CONFIG_H
#define CS_MANAGER_CONFIG_H

/***************************************************************************//**
 * @addtogroup cs_manager
 * @{
 ******************************************************************************/

// <<< Use Configuration Wizard in Context Menu >>>

// -----------------------------------------------------------------------------
// Macros

// <h> General

// <o CS_MANAGER_CONFIG_MAX_INSTANCES> Maximum number of CS Manager instances <1..32>
// <i> Default: 1
#define CS_MANAGER_CONFIG_MAX_INSTANCES               (1)

// <o CS_MANAGER_CONFIG_COUNT> Number of CS Manager configurations per connection <1..3>
// <i> Total capacity is SL_BT_CONFIG_MAX_CONNECTIONS * CS_MANAGER_CONFIG_COUNT
// <i> Default: 1
#define CS_MANAGER_CONFIG_COUNT                       (1)

// </h>

// <h> Logging

// <q CS_MANAGER_CONFIG_LOG_ENABLE> Enable
// <i> Default: 1
// <i> Enable CS Manager component logging
#define CS_MANAGER_CONFIG_LOG_ENABLE                  (1)

// <s CS_MANAGER_CONFIG_LOG_PREFIX> Prefix
// <i> Default: "[CS Manager]"
#define CS_MANAGER_CONFIG_LOG_PREFIX                  "[CS Manager] "

// </h>

// <h> CS Security

// <q CS_MANAGER_CONFIG_ENABLE_CS_SECURITY_BEFORE_CONFIG> Security before configuration
// <i> Enable CS security before configuration
// <i> Default: 1
#define CS_MANAGER_CONFIG_ENABLE_CS_SECURITY_BEFORE_CONFIG    1

// </h>

// <h> Connection parameters

// <q CS_MANAGER_CONFIG_SET_DEFAULT_CONNECTION_PARAMETERS> Set defaults
// <i> Set default connection parameters on boot
// <i> Default: 1
#define CS_MANAGER_CONFIG_SET_DEFAULT_CONNECTION_PARAMETERS       1

// <o CS_MANAGER_CONFIG_DEFAULT_MIN_CONNECTION_INTERVAL> Minimum connection interval (in 1.25 ms steps) <6..3200>
// <i> Default: 6
#define CS_MANAGER_CONFIG_DEFAULT_MIN_CONNECTION_INTERVAL         6

// <o CS_MANAGER_CONFIG_DEFAULT_MAX_CONNECTION_INTERVAL> Maximum connection interval (in 1.25 ms steps) <6..3200>
// <i> Default: 6
#define CS_MANAGER_CONFIG_DEFAULT_MAX_CONNECTION_INTERVAL         6

// <o CS_MANAGER_CONFIG_DEFAULT_PROCEDURE_TIMEOUT_MS> CS procedure timeout [msec] <100..5000>
// <i> Timeout value for procedures - in order to avoid getting stuck in a procedure indefinitely.
// <i> Once the time elapses the initiator instance's error callback executes to
// <i> inform the user about the issue.
// <i> Default: 3000
#define CS_MANAGER_CONFIG_DEFAULT_PROCEDURE_TIMEOUT_MS            3000

// <o CS_MANAGER_CONFIG_DEFAULT_CONNECTION_PERIPHERAL_LATENCY> Connection peripheral latency
// <i> Peripheral latency, which defines how many connection
// <i> intervals the peripheral can skip if it has no data to send
// <i> Default: 0
#define CS_MANAGER_CONFIG_DEFAULT_CONNECTION_PERIPHERAL_LATENCY   0

// <o CS_MANAGER_CONFIG_DEFAULT_CONNECTION_TIMEOUT> Supervision timeout <10..3200>
// <i> Connection supervision timeout in the units of 10 ms (from 100 ms to 32 s)
// <i> Default: 500
#define CS_MANAGER_CONFIG_DEFAULT_CONNECTION_TIMEOUT   500

// <o CS_MANAGER_CONFIG_DEFAULT_MIN_CE_LENGTH> Minimum length of the connection event <0..65535>
// <i> Value in units of 0.625 ms
// <i> Default: 0
#define CS_MANAGER_CONFIG_DEFAULT_MIN_CE_LENGTH        0

// <o CS_MANAGER_CONFIG_DEFAULT_MAX_CE_LENGTH> Maximum length of the connection event <0..65535>
// <i> Value in units of 0.625 ms
// <i> Default: 65535
#define CS_MANAGER_CONFIG_DEFAULT_MAX_CE_LENGTH        65535

// </h>

// <h> Instance config defaults

// <h> TX power

// <o CS_MANAGER_CONFIG_DEFAULT_MIN_TX_POWER_DBM> Minimum transmit power of the radio [dBm] <-127..20>
// <i> Default: -3
// <i> Minimum transmit power of the reflector radio
#define CS_MANAGER_CONFIG_DEFAULT_MIN_TX_POWER_DBM      (-3)

// <o CS_MANAGER_CONFIG_DEFAULT_MAX_TX_POWER_DBM> Maximum transmit power of the radio [dBm] <-127..20>
// <i> Default: 20
// <i> Maximum transmit power of the reflector radio
#define CS_MANAGER_CONFIG_DEFAULT_MAX_TX_POWER_DBM      (20)

// <q CS_MANAGER_CONFIG_SET_DEFAULT_CONN_PHY> Set default connection PHY on boot
// <i> Set default preferred connection PHY on boot
// <i> Default: 1
#define CS_MANAGER_CONFIG_SET_DEFAULT_CONN_PHY       1

// <q CS_MANAGER_CONFIG_REQUEST_CONN_PHY> Request connection PHY update
// <i> Request connection PHY update on creation∂
// <i> Default: 1
#define CS_MANAGER_CONFIG_REQUEST_CONN_PHY           1

// <o CS_MANAGER_CONFIG_DEFAULT_CONN_PHY> Connection PHY
// <sl_bt_gap_phy_1m=> 1M
// <sl_bt_gap_phy_2m=> 2M
// <i> Default: sl_bt_gap_phy_2m
#define CS_MANAGER_CONFIG_DEFAULT_CONN_PHY         sl_bt_gap_phy_2m

// </h>

// <h> Antenna configuration

// <o CS_MANAGER_CONFIG_DEFAULT_CS_SYNC_ANTENNA> Select antenna for CS sync packets
// <CS_SYNC_ANTENNA_1=> Antenna 1
// <CS_SYNC_ANTENNA_2=> Antenna 2
// <CS_SYNC_ANTENNA_3=> Antenna 3
// <CS_SYNC_ANTENNA_4=> Antenna 4
// <CS_SYNC_SWITCHING=> Switching between all antennas
// <i> Default: CS_SYNC_SWITCHING
#define CS_MANAGER_CONFIG_DEFAULT_CS_SYNC_ANTENNA      CS_SYNC_SWITCHING

// </h>

// </h>

// <h> Runtime settings

// <o CS_MANAGER_CONFIG_RTA_WAIT_FOR_GUARD> Timeout for guard acquisition (in ticks) <0..65535>
// <i> Number of RTOS ticks to wait when acquiring the RTA guard mutex.
// <i> Default: 10
#define CS_MANAGER_CONFIG_RTA_WAIT_FOR_GUARD  10

// </h>

// <<< end of configuration section >>>

/** @} (end addtogroup cs_manager) */

#endif // CS_MANAGER_CONFIG_H
