/***************************************************************************//**
 * @file
 * @brief CS Manager - CS Procedure control configuration header
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

#ifndef CS_MANAGER_CS_CONTROL_CONFIG_H
#define CS_MANAGER_CS_CONTROL_CONFIG_H

#include "sl_bt_api.h"

/***************************************************************************//**
 * @addtogroup cs_manager
 * @{
 ******************************************************************************/

// <<< Use Configuration Wizard in Context Menu >>>

// -----------------------------------------------------------------------------
// Macros

// <h> Default procedure parameters

// <o CS_MANAGER_DEFAULT_MAX_PROCEDURE_COUNT> Procedure count <0..65535>
// <i> Maximum number of procedures to execute
// <i> When set to 0, the procedure will run indefinitely until stopped by the application.
// <i> Default: 0
#define CS_MANAGER_DEFAULT_MAX_PROCEDURE_COUNT         0

// <o CS_MANAGER_DEFAULT_MAX_PROCEDURE_DURATION> Max procedure duration <1..65535>
// <i> Maximum duration for each measurement procedure.
// <i> Value in units of 0.625 ms
// <i> Default: 65535
#define CS_MANAGER_DEFAULT_MAX_PROCEDURE_DURATION           65535

// <o CS_MANAGER_DEFAULT_MIN_PROCEDURE_INTERVAL> Minimum delay between CS measurements [connection events] <1..255>
// <i> Default: 38
// <i> Minimum duration in number of connection events between consecutive CS measurement procedures
#define CS_MANAGER_DEFAULT_MIN_PROCEDURE_INTERVAL                     (38)

// <o CS_MANAGER_DEFAULT_MAX_PROCEDURE_INTERVAL> Maximum delay between CS measurements [connection events] <1..255>
// <i> Default: 38
// <i> Maximum duration in number of connection events between consecutive CS measurement procedures
#define CS_MANAGER_DEFAULT_MAX_PROCEDURE_INTERVAL                     (38)

// <o CS_MANAGER_DEFAULT_MIN_SUBEVENT_LEN> Minimum subevent length [us] <1250..3999999>
// <i> Minimum suggested duration for each CS subevent in microseconds.
// <i> Raising this value forces the controller to use longer subevents, reducing the number
// <i> of subevents that fit within a single procedure.
// <i> This value should not exceed the maximum procedure time, which is calculated as:
// <i> max_procedure_time_us = CS_MANAGER_DEFAULT_MAX_PROCEDURE_INTERVAL
// <i>                       * CS_MANAGER_DEFAULT_MAX_CONNECTION_INTERVAL * 1250
// <i> Default: 1250
#define CS_MANAGER_DEFAULT_MIN_SUBEVENT_LEN          1250

// <o CS_MANAGER_DEFAULT_MAX_SUBEVENT_LEN> Maximum subevent length [us] <1250..3999999>
// <i> Maximum suggested duration for each CS subevent in microseconds.
// <i> Reducing this value causes the controller to split CS steps across multiple subevents.
// <i> When a large number of subevents is expected, ensure that the effective procedure interval
// <i> is at least the number of created subevents, plus additional connection events for RAS data transfer.
// <i> The effective procedure interval is determined by optimization or the maximum procedure interval
// <i> when custom scheduling is used.
// <i> The subevent length should be configured to accommodate both Mode 0 steps and main mode steps.
// <i> With default settings, values below ~38500 us result in more than one subevent per procedure.
// <i> Default: 3999999
#define CS_MANAGER_DEFAULT_MAX_SUBEVENT_LEN          3999999

// <o CS_MANAGER_DEFAULT_CS_TONE_ANTENNA_CONFIG_IDX> CS tone antenna configuration
// <i> Use all the available antennas that can be supported
// <i> Value: 0. 1 antenna path, [1:1] antenna [initiator:reflector]
// <i> Value: 1. 2 antenna paths, [2:1] antenna
// <i> Value: 2. 3 antenna paths, [3:1] antenna
// <i> Value: 3. 4 antenna paths, [4:1] antenna
// <i> Value: 4. 2 antenna paths, [1:2] antenna
// <i> Value: 5. 3 antenna paths, [1:3] antenna
// <i> Value: 6. 4 antenna paths, [1:4] antenna
// <i> Value: 7. 4 antenna paths, [2:2] antenna
// <CS_ANTENNA_CONFIG_INDEX_SINGLE_ONLY=> 1 antenna path, [1:1] antenna
// <CS_ANTENNA_CONFIG_INDEX_DUAL_LOCAL_SINGLE_REMOTE=> 2 antenna paths, [2:1] antenna
// <CS_ANTENNA_CONFIG_INDEX_TRIPLE_LOCAL_SINGLE_REMOTE=> 3 antenna paths, [3:1] antenna
// <CS_ANTENNA_CONFIG_INDEX_QUAD_LOCAL_SINGLE_REMOTE=> 4 antenna paths, [4:1] antenna
// <CS_ANTENNA_CONFIG_INDEX_SINGLE_LOCAL_DUAL_REMOTE=> 2 antenna paths, [1:2] antenna
// <CS_ANTENNA_CONFIG_INDEX_SINGLE_LOCAL_TRIPLE_REMOTE=> 3 antenna paths, [1:3] antenna
// <CS_ANTENNA_CONFIG_INDEX_SINGLE_LOCAL_QUAD_REMOTE=> 4 antenna paths, [1:4] antenna
// <CS_ANTENNA_CONFIG_INDEX_DUAL_ONLY=> 4 antenna paths, [2:2] antenna
// <i> Default: CS_ANTENNA_CONFIG_INDEX_DUAL_ONLY
#define CS_MANAGER_DEFAULT_CS_TONE_ANTENNA_CONFIG_IDX   CS_ANTENNA_CONFIG_INDEX_DUAL_ONLY

// <o CS_MANAGER_DEFAULT_PREFERRED_PEER_ANTENNA> Preferred peer antenna
// <1=> First ordered antenna element
// <2=> Second ordered antenna element
// <3=> First and second ordered antenna elements
// <i> Default: 3
#define CS_MANAGER_DEFAULT_PREFERRED_PEER_ANTENNA  3

// <o CS_MANAGER_DEFAULT_TX_PWR_DELTA> Transmit power delta, in signed dB.
// <i> TX power delta relative to the peer
// <i> Default: 0
#define CS_MANAGER_DEFAULT_TX_PWR_DELTA   0

// </h>

// <<< end of configuration section >>>

// SNR control mode for the initiator
#define CS_MANAGER_DEFAULT_SNR_CONTROL_INITIATOR sl_bt_cs_snr_control_adjustment_not_applied

// SNR control mode for the reflector
#define CS_MANAGER_DEFAULT_SNR_CONTROL_REFLECTOR sl_bt_cs_snr_control_adjustment_not_applied

/** @} (end addtogroup cs_manager) */

#endif // CS_MANAGER_CS_CONTROL_CONFIG_H
