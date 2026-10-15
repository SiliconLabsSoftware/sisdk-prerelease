/***************************************************************************//**
 * @file
 * @brief
 *******************************************************************************
 * # License
 * <b>Copyright 2022 Silicon Laboratories Inc. www.silabs.com</b>
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

#ifndef SL_RAIL_SDK_PACKET_ASSISTANT_CONFIG_H
#define SL_RAIL_SDK_PACKET_ASSISTANT_CONFIG_H

/**************************************************************************//**
 * @defgroup rail_sdk_packet_assistant_defines Configurations
 * @ingroup rail_sdk_packet_assistant
 * @{
 *****************************************************************************/

// <<< Use Configuration Wizard in Context Menu >>>

// <h> Assistant print settings
// <o SL_PACKET_ASSISTANT_PRINT_PACKET_INFO> Enable assistant log prints
// <i> Default: 0
// <i> 1 enabled, 0 disabled
#define SL_PACKET_ASSISTANT_PRINT_PACKET_INFO      (0) ///< Enable assistant log prints

// </h> Assistant print settings

// <h> TX frame size settings
// <o SL_PACKET_ASSISTANT_MAX_TX_FRAME_SIZE> Maximum TX payload size (bytes)
// <i> Default: 256
// <i> Maximum payload length accepted by sl_packet_assistant_prepare_packet().
#define SL_PACKET_ASSISTANT_MAX_TX_FRAME_SIZE      (256) ///< Maximum TX payload size in bytes

// </h> TX frame size settings

// <h> SUN FSK header settings
// <o SL_PACKET_ASSISTANT_SUN_FSK_FCS_TYPE> FCS type
// <0=> 4-byte FCS
// <1=> 2-byte FCS
// <i> Default: 0
// <i> 0 = 4-byte FCS, 1 = 2-byte FCS
#define SL_PACKET_ASSISTANT_SUN_FSK_FCS_TYPE      (0) ///< SUN FSK FCS type: 0 = 4-byte, 1 = 2-byte

// <o SL_PACKET_ASSISTANT_SUN_FSK_WHITENING> Whitening is on/off
// <i> Default: 1
// <i> Whitening is on/off
#define SL_PACKET_ASSISTANT_SUN_FSK_WHITENING      (1) ///< SUN FSK Whitening is on/off

// </h> SUN FSK header settings

// <h> SUN OFDM header settings
// <o SL_PACKET_ASSISTANT_SUN_OFDM_RATE> Default Radio Configuration
// <i> Default: 6
#define SL_PACKET_ASSISTANT_SUN_OFDM_RATE  (6) ///< SUN OFDM rate Configuration

// <o SL_PACKET_ASSISTANT_SUN_OFDM_SCRAMBLER> 2 bits wide, The Scrambler field (S1-S0) specifies the scrambling seed
// <i> Default: 0
// <i> 2 bits wide, The Scrambler field (S1-S0) specifies the scrambling seed
#define SL_PACKET_ASSISTANT_SUN_OFDM_SCRAMBLER      (0) ///< SUN OFDM Scrambler Configuration

// </h> SUN OFDM header settings

// <h> SUN OQPSK header settings
// <o SL_PACKET_ASSISTANT_SUN_OQPSK_SPREADINGMODE> spreading mode
// <i> Default: 0
// <i> spreading mode
#define SL_PACKET_ASSISTANT_SUN_OQPSK_SPREADINGMODE      (0) ///< SUN OQPSK spreading mode Configuration

// <o SL_PACKET_ASSISTANT_SUN_OQPSK_RATEMODE> rate mode: 2 bits wide
// <i> Default: 1
// <i> rate mode: 2 bits wide
#define SL_PACKET_ASSISTANT_SUN_OQPSK_RATEMODE      (0) ///< SUN OQPSK rate mode Configuration

// </h> SUN OQPSK header settings

// <h> SideWalk FSK header settings
// <o SL_PACKET_ASSISTANT_SIDEWALK_FSK_FCS_TYPE> FCS type
// <0=> 4-byte FCS
// <1=> 2-byte FCS
// <i> Default: 1
// <i> 0 = 4-byte FCS, 1 = 2-byte FCS
#define SL_PACKET_ASSISTANT_SIDEWALK_FSK_FCS_TYPE      (1) ///< SideWalk FSK FCS type: 0 = 4-byte, 1 = 2-byte

// <o SL_PACKET_ASSISTANT_SIDEWALK_FSK_WHITENING> Whitening is on/off
// <i> Default: 1
// <i> Whitening is on/off
#define SL_PACKET_ASSISTANT_SIDEWALK_FSK_WHITENING      (1) ///< SideWalk FSK Whitening is on/off

// </h> SideWalk FSK header settings

// <<< end of configuration section >>>

/** @}*/

#endif // SL_RAIL_SDK_PACKET_ASSISTANT_CONFIG_H
