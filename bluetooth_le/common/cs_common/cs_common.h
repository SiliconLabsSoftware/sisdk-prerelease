/***************************************************************************//**
 * @file
 * @brief CS - Common definitions
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
#ifndef CS_COMMON_H
#define CS_COMMON_H

#include "sl_enum.h"

/// CS channel map preset
SL_ENUM(cs_channel_map_preset_t) {
  CS_CHANNEL_MAP_PRESET_MEDIUM = 1, //< Medium channel map preset
  CS_CHANNEL_MAP_PRESET_HIGH   = 2, //< High channel map preset
  CS_CHANNEL_MAP_PRESET_CUSTOM = 3, //< Custom channel map preset
};

/// Procedure scheduling type
SL_ENUM(cs_procedure_scheduling_t) {
  CS_PROCEDURE_SCHEDULING_OPTIMIZED_FOR_FREQUENCY = 0, //< Optimized for measurement frequency
  CS_PROCEDURE_SCHEDULING_OPTIMIZED_FOR_ENERGY, //< Optimized for energy consumption
  CS_PROCEDURE_SCHEDULING_CUSTOM, //< Custom scheduling
};

/// CS algorithm mode
SL_ENUM(cs_algo_mode_t) {
  CS_ALGO_MODE_TRACKING_ACCURACY_OPTIMIZED = 0, //< Tracking accuracy optimized (suitable for moving targets)
  CS_ALGO_MODE_STATIONARY =                  1, //< Stationary (suitable for stationary targets)
  CS_ALGO_MODE_TRACKING_LATENCY_OPTIMIZED =  2, //< Tracking latency optimized (suitable for fast moving targets)
  CS_ALGO_MODE_INVALID =                  0xFF, //< Invalid algorithm mode.
};

#define CS_ALGO_MODE_REAL_TIME_BASIC      CS_ALGO_MODE_TRACKING_ACCURACY_OPTIMIZED
#define CS_ALGO_MODE_STATIC_HIGH_ACCURACY CS_ALGO_MODE_STATIONARY
#define CS_ALGO_MODE_REAL_TIME_FAST       CS_ALGO_MODE_TRACKING_LATENCY_OPTIMIZED

/// CS antenna configuration index
SL_ENUM(cs_tone_antenna_config_index_t) {
  CS_ANTENNA_CONFIG_INDEX_SINGLE_ONLY = 0, ///< Single only
  CS_ANTENNA_CONFIG_INDEX_DUAL_LOCAL_SINGLE_REMOTE = 1, ///< Dual local single remote
  CS_ANTENNA_CONFIG_INDEX_SINGLE_LOCAL_DUAL_REMOTE = 4, ///< Single local dual remote
  CS_ANTENNA_CONFIG_INDEX_DUAL_ONLY = 7, ///< Dual only
};

/// CS antenna usage for CS SYNC packets
SL_ENUM(cs_sync_antenna_t) {
  CS_SYNC_ANTENNA_1 = 1,
  CS_SYNC_ANTENNA_2 = 2,
  CS_SYNC_SWITCHING = 0xfe
};

#endif // CS_COMMON_H
