/***************************************************************************//**
 * @file
 * @brief CS RREQ - configuration header
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
#ifndef CS_RREQ_CONFIG_H
#define CS_RREQ_CONFIG_H

/***********************************************************************************************//**
 * @addtogroup cs_rreq
 * @{
 **************************************************************************************************/

// <<< Use Configuration Wizard in Context Menu >>>

// <h> Logging

// <e CS_RREQ_CONFIG_LOG> Logging
// <i> Default: 1
// <i> Enable RREQ component logging
#define CS_RREQ_CONFIG_LOG                              1

// <q CS_RREQ_CONFIG_LOG_DATA> Data logging
// <i> Default: 0
// <i> Enable RREQ component data logging
#define CS_RREQ_CONFIG_LOG_DATA                         0

// <s CS_RREQ_CONFIG_LOG_PREFIX> Log prefix
// <i> Default: "[RREQ]"
#define CS_RREQ_CONFIG_LOG_PREFIX                       "[RREQ] "

// </e>

// </h>

// <h> General
// <o CS_RREQ_CONFIG_MAX_CONNECTIONS> Maximum connections <1..4>
// <i> If more than 1 RREQ instances are created SL_BT_CONFIG_BUFFER_SIZE shall be increased.
// <i> Default: 1
#define CS_RREQ_CONFIG_MAX_CONNECTIONS                  1

// <q CS_RREQ_CONFIG_RAS_ON_DEMAND_SUPPORT> RAS On-Demand Support
// <i> Enable RAS On-Demand support
// <i> Default: 0
#define CS_RREQ_CONFIG_RAS_ON_DEMAND_SUPPORT            0

// <q CS_RREQ_CONFIG_EXTERNAL_RANGING_DATA_BUFFER> External ranging data buffer
// <i> Use external buffer instead of allocating internally.
// <i> Default: 0
#define CS_RREQ_CONFIG_EXTERNAL_RANGING_DATA_BUFFER     0

// <o CS_RREQ_CONFIG_MAX_RANGING_DATA_SIZE> Maximum ranging data size <32..2700>
// <i> The optimal value of "Maximum ranging data size" is dependent on several configuration values,
// <i> and can be calculated by the following equation:
// <i> ranging_max_size = 4 + (subevents * 8) + (subevents * mode0_steps * mode0_size) +
// <i> channels * ( ( 1 + ( antenna_paths + 1 ) * 4) + 1 )
// <i> where
// <i> - subevents is the number of CS subevents per procedure (range: 1..32), determined by the
// <i>   controller based on CS_MANAGER_DEFAULT_MIN_SUBEVENT_LEN and CS_MANAGER_DEFAULT_MAX_SUBEVENT_LEN.
// <i>   Shorter subevent lengths allow more subevents per procedure.
// <i> - mode0_size is
// <i>   - 4 for Reflector and
// <i>   - 6 for Initiator,
// <i> - mode0_steps value is the configuration "Mode 0 steps",
// <i> - channels value means the number of channels from the channel mask that can be
// <i> derived from the "Channel map preset" settings:
// <i>   - "High"   - 72 (default),
// <i>   - "Medium" - 37,
// <i>   - "Low"    - 20,
// <i>   - "Custom" - Number of 1s in channel mask,
// <i> - antenna_paths value is controlled by the "Antenna configuration", and limited by number of
// <i> antennas presented on each board (capabilities). Maximum can be calculated using the product
// <i> of used Initiator and Reflector antennae. The default maximum value for antenna_paths is 4.
// <i> These settings were selected by assuming that the controller creates the maximum number of subevents (32),
// <i> and the measuring mode is PBR. In RTT mode, far less data is created.
// <i> Addition to that, if you use RTT as submode, you should add the following equation to calculate the
// <i> size.
// <i> (1 + mode1_size) * channels / main_mode_steps
// <i> where
// <i> mode1_size is 6, and main_mode_steps is 2. The later can be changed in the config.
// <i> The default is calculated by using the constants and settings above using the worst case scenario,
// <i> which gives 2672 bytes.
// <i> RAM consumption can be reduced by changing the affected settings and reducing
// <i> "Procedure maximum length" accordingly.
// <i> Default: 2672
#define CS_RREQ_CONFIG_MAX_RANGING_DATA_SIZE            2672

// <o CS_RREQ_CONFIG_MAX_DROP> Maximum dropped procedures <2..10>
// <i> Maximum number of dropped procedures if current ranging data is not received.
// <i> Default: 10
#define CS_RREQ_CONFIG_MAX_DROP                         10

// <o CS_RREQ_CONFIG_ERROR_TIMEOUT_MS> Error timeout [msec] <100..5000>
// <i> Timeout value in order to avoid stuck in error state indefinitely.
// <i> Once the time elapses the RREQ instance's error callback executes to
// <i> inform the user about the issue.
// <i> Default: 3000
#define CS_RREQ_CONFIG_ERROR_TIMEOUT_MS                 3000

// <o CS_RREQ_CONFIG_PROCEDURE_TIMEOUT_MS> CS procedure timeout [msec] <100..5000>
// <i> Timeout value for procedures - in order to avoid getting stuck in a procedure indefinitely.
// <i> Once the time elapses the RREQ instance's error callback executes to
// <i> inform the user about the issue.
// <i> Default: 3000
#define CS_RREQ_CONFIG_PROCEDURE_TIMEOUT_MS             3000

// </h>

// <<< end of configuration section >>>

/** @} (end addtogroup cs_rreq) */
#endif // CS_RREQ_CONFIG_H
