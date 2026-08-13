/***************************************************************************//**
 * @file
 * @brief CS Manager - CS configuration feature configuration header
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

#ifndef CS_MANAGER_CS_CONFIG_CONFIG_H
#define CS_MANAGER_CS_CONFIG_CONFIG_H

#include "sl_bt_api.h"

/***************************************************************************//**
 * @addtogroup cs_manager
 * @{
 ******************************************************************************/

// <<< Use Configuration Wizard in Context Menu >>>

// -----------------------------------------------------------------------------
// Macros

// <h> CS Manager Config

// <h> Channels

// <a.10 CS_MANAGER_DEFAULT_CHANNEL_MAP> Channel map <0..255> <f.h>
// <i> Default: { 0xFC, 0xFF, 0x7F, 0xFC, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x1F }
#define CS_MANAGER_DEFAULT_CHANNEL_MAP  { 0xFC, 0xFF, 0x7F, 0xFC, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x1F }

// <o CS_MANAGER_DEFAULT_CHANNEL_MAP_REPETITION> Channel map repetition <1..3>
// <i> The number of times the channel_map field will
// <i> be cycled through for non-Mode 0 steps within a CS procedure.
// <i> Default: 1
#define CS_MANAGER_DEFAULT_CHANNEL_MAP_REPETITION   (1)

// <o CS_MANAGER_DEFAULT_CHANNEL_SELECTION_TYPE> Channel selection type
// <sl_bt_cs_channel_selection_algorithm_3b=> Algorithm 3b
// <sl_bt_cs_channel_selection_algorithm_3c=> Algorithm 3c
// <sl_bt_cs_channel_selection_algorithm_user_shape_interleaved=> Algorithm user shape interleaved
// <i> Default: sl_bt_cs_channel_selection_algorithm_3b
#define CS_MANAGER_DEFAULT_CHANNEL_SELECTION_TYPE          sl_bt_cs_channel_selection_algorithm_3b

// <o CS_MANAGER_DEFAULT_CH3C_SHAPE> Ch3c shape
// <sl_bt_cs_ch3c_shape_hat=> Hat
// <sl_bt_cs_chc3_shape_interleaved=> Interleaved
// <i> Default: sl_bt_cs_ch3c_shape_hat
#define CS_MANAGER_DEFAULT_CH3C_SHAPE               sl_bt_cs_ch3c_shape_hat

// <o CS_MANAGER_DEFAULT_CH3C_JUMP> Ch3c jump <2..8>
// <i> Number of channels skipped in each rising and falling sequence when
// <i> Algorithm 3c is selected.
// <i> Default: 2
#define CS_MANAGER_DEFAULT_CH3C_JUMP                (2)

// </h>

// <h> Modes

// <o CS_MANAGER_DEFAULT_MAIN_MODE_TYPE> Main mode
// <sl_bt_cs_mode_rtt=> RTT
// <sl_bt_cs_mode_pbr=> PBR
// <i> Default: sl_bt_cs_mode_pbr
#define CS_MANAGER_DEFAULT_MAIN_MODE_TYPE           sl_bt_cs_mode_pbr

// <o CS_MANAGER_DEFAULT_SUB_MODE_TYPE> Sub mode
// <sl_bt_cs_mode_rtt=> RTT
// <sl_bt_cs_submode_disabled=> No submode
// <i> Default: sl_bt_cs_submode_disabled
#define CS_MANAGER_DEFAULT_SUB_MODE_TYPE            sl_bt_cs_submode_disabled

// <o CS_MANAGER_DEFAULT_MIN_MAIN_MODE_STEPS> Minimum main-mode steps <2..160>
// <i> Ignored when sub-mode is disabled.
// <i> Default: 2
#define CS_MANAGER_DEFAULT_MIN_MAIN_MODE_STEPS      (2)

// <o CS_MANAGER_DEFAULT_MAX_MAIN_MODE_STEPS> Maximum main-mode steps <2..160>
// <i> Ignored when sub-mode is disabled.
// <i> Default: 2
#define CS_MANAGER_DEFAULT_MAX_MAIN_MODE_STEPS      (2)

// <o CS_MANAGER_DEFAULT_MAIN_MODE_REPETITION> Main-mode repetition <0..3>
// <i> The number of main mode steps taken from the end of the last CS subevent
// <i> to be repeated at the beginning of the current CS subevent directly after
// <i> the last Mode 0 step of that event.
// <i> Default: 0
#define CS_MANAGER_DEFAULT_MAIN_MODE_REPETITION     (0)

// <o CS_MANAGER_DEFAULT_MODE_CALIBRATION_STEPS> Mode 0 (calibration) steps <1..3>
// <i> Number of Mode 0 steps included at the beginning of each CS subevent.
// <i> Default: 3
#define CS_MANAGER_DEFAULT_MODE_CALIBRATION_STEPS   (3)

// </h>

// <h> RTT and PHYs

// <o CS_MANAGER_DEFAULT_RTT_TYPE> RTT type
// <sl_bt_cs_rtt_type_aa_only=> RTT Access Address (AA) only
// <sl_bt_cs_rtt_type_fractional_32_bit_sounding=> RTT Fractional with 32-bit Sounding Sequence
// <sl_bt_cs_rtt_type_fractional_96_bit_sounding=> RTT Fractional with 96-bit Sounding Sequence
// <sl_bt_cs_rtt_type_fractional_32_bit_random=> RTT Fractional with 32-bit Random Sequence
// <sl_bt_cs_rtt_type_fractional_64_bit_random=> RTT Fractional with 64-bit Random Sequence
// <sl_bt_cs_rtt_type_fractional_96_bit_random=> RTT Fractional with 96-bit Random Sequence
// <sl_bt_cs_rtt_type_fractional_128_bit_random=> RTT Fractional with 128-bit Random Sequence
// <i> Default: sl_bt_cs_rtt_type_fractional_96_bit_sounding
#define CS_MANAGER_DEFAULT_RTT_TYPE                 sl_bt_cs_rtt_type_fractional_96_bit_sounding

// <o CS_MANAGER_DEFAULT_CS_SYNC_PHY> CS sync PHY
// <sl_bt_gap_phy_1m=> 1M
// <sl_bt_gap_phy_2m=> 2M
// <i> Default: sl_bt_gap_phy_1m
#define CS_MANAGER_DEFAULT_CS_SYNC_PHY              sl_bt_gap_phy_1m

// </h>

// </h>

// <<< end of configuration section >>>

/** @} (end addtogroup cs_manager) */

#endif // CS_MANAGER_CS_CONFIG_CONFIG_H
