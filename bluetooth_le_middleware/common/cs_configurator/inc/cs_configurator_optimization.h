/***************************************************************************//**
 * @file
 * @brief CS Configurator - configuration header
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

 #ifndef CS_CONFIGURATOR_OPTIMIZATION_H
 #define CS_CONFIGURATOR_OPTIMIZATION_H

/***************************************************************************//**
 * @addtogroup cs_configurator
 * @{
 ******************************************************************************/

// -----------------------------------------------------------------------------
// Macros

// Ratio scaling

// Ratio fixed-point scale
// Time scaling ratios below are in parts per this value 
// (e.g. 1340 means 1/1.340 when scale is 1000).
#define CS_CONFIGURATOR_RATIO_DIVISOR                         (1000UL)

// Scale time in place:
// time = (time) / (ratio) * CS_CONFIGURATOR_RATIO_DIVISOR.
#define CS_CONFIGURATOR_APPLY_TIME_RATIO(time, ratio) \
                                                ((time) = (time) / \
                                                (ratio) *           \
                                                CS_CONFIGURATOR_RATIO_DIVISOR)

// Mode 0 timing

// Mode 0 base duration [us]
#define CS_CONFIGURATOR_MODE0_BASE_US                         (2900UL)

// Measurement time

// Base measurement duration [us]
#define CS_CONFIGURATOR_MEASUREMENT_BASE_US                   (39000UL)

// Measurement ratio: 4 to 2 antenna paths
// Per CS_CONFIGURATOR_RATIO_DIVISOR
// Applied when using single-dual or dual-single antenna configuration
#define CS_CONFIGURATOR_MEASUREMENT_RATIO_ANTENNA_PATHS_4_TO_2 (1340UL)

// Measurement ratio: 4 to 1 antenna path
// Per CS_CONFIGURATOR_RATIO_DIVISOR
// Applied for single-antenna-only configuration
#ifndef CS_CONFIGURATOR_MEASUREMENT_RATIO_ANTENNA_PATHS_4_TO_1
#define CS_CONFIGURATOR_MEASUREMENT_RATIO_ANTENNA_PATHS_4_TO_1 (1180UL)
#endif

// Measurement ratio: high to medium channel map
// Per CS_CONFIGURATOR_RATIO_DIVISOR
// Applied when channel map is not the high preset
#define CS_CONFIGURATOR_MEASUREMENT_RATIO_CHANNEL_MAP_HIGH_TO_MEDIUM (1900UL)

// RAS data timing

// Base RAS data duration [us]
// Excludes RAS protocol overhead
// Used for procedure budgeting
#define CS_CONFIGURATOR_RAS_DATA_BASE_US                      (13500UL)

// RAS data ratio: 4 to 2 antenna paths
// Per CS_CONFIGURATOR_RATIO_DIVISOR
// Applied when not using dual-antenna-only configuration
#define CS_CONFIGURATOR_RAS_DATA_RATIO_ANTENNA_PATHS_4_TO_2   (1300UL)

// RAS data ratio: 4 to 1 antenna path
// Per CS_CONFIGURATOR_RATIO_DIVISOR
// Applied for single-antenna-only configuration.
#define CS_CONFIGURATOR_RAS_DATA_RATIO_ANTENNA_PATHS_4_TO_1   (1600UL)

// RAS data ratio: high to medium channel map
// Per CS_CONFIGURATOR_RATIO_DIVISOR 
// Applied when channel map is not the high preset.
#define CS_CONFIGURATOR_RAS_DATA_RATIO_CHANNEL_MAP_HIGH_TO_MEDIUM (1500UL)

// Procedure connection events

// Empty connection events after measurement and before RAS
#define CS_CONFIGURATOR_EMPTY_BEFORE_RAS_CE                   (1U)

// On-demand RAS overhead [connection events]
// Added when not using real-time RAS mode.
#define CS_CONFIGURATOR_RAS_ON_DEMAND_OVERHEAD_CE             (8U)

// Procedure safety padding [connection events]
#define CS_CONFIGURATOR_PADDING_CE                            (4U)

// Distance calculation padding [connection events]
// The entire procedure time must be at least this many connection events 
// longer than the estimation time.
#define CS_CONFIGURATOR_DISTANCE_CALCULATION_PADDING_CE           (2U)

#endif // CS_CONFIGURATOR_OPTIMIZATION_H