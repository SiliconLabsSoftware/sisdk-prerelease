/***************************************************************************//**
 * @file
 * @brief CS Algo - configuration header
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
#ifndef CS_ALGO_CONFIG_H
#define CS_ALGO_CONFIG_H

/***********************************************************************************************//**
 * @addtogroup cs_algo
 * @{
 **************************************************************************************************/

// <<< Use Configuration Wizard in Context Menu >>>

// <h> Logging

// <e CS_ALGO_CONFIG_LOG> Algo component
// <i> Default: 1
// <i> Enable Algo component logging
#define CS_ALGO_CONFIG_LOG                        1

// <e CS_ALGO_CONFIG_RTL_LOG> RTL Library
// <i> Default: 1
// <i> Enable RTL Library logging
#define CS_ALGO_CONFIG_RTL_LOG                    1

// <s CS_ALGO_CONFIG_LOG_PREFIX> Log prefix
// <i> Default: "[Algo]"
#define CS_ALGO_CONFIG_LOG_PREFIX                 "[Algo]"

// </h>

// <h> General

// <o CS_ALGO_CONFIG_ESTIMATOR_COUNT> Number of connections that own a CS estimator <1..4>
// <i> Default: 1
// <i> Must be in the range [1 .. SL_BT_CONFIG_MAX_CONNECTIONS] and must match
// <i> CS_ALGO_CONFIG_ESTIMATOR_COUNT, since every connection may own 0 or 1 estimator.
#define CS_ALGO_CONFIG_ESTIMATOR_COUNT            1

// <o CS_ALGO_CONFIG_DEFAULT_RSSI_REF_TX_POWER> RSSI reference TX power <-110..30>
// <i> Reference RSSI value of the remote device at 1.0 m distance in dBm
// <i> Default: -40.0F
#define CS_ALGO_CONFIG_DEFAULT_RSSI_REF_TX_POWER  -40.0F

// </e>

// <<< end of configuration section >>>

// Internal define to skip RTL processing
#define CS_ALGO_CONFIG_SKIP_RTL_PROCESS         0

// Default algorithm mode
#define CS_ALGO_CONFIG_DEFAULT_ALGO_MODE        CS_ALGO_MODE_REAL_TIME_FAST

// Default RTL library configuration
#define CS_ALGO_CONFIG_RTL_CONFIG_DEFAULT                   \
  {                                                         \
    .rtl_logging_enabled = (bool)CS_ALGO_CONFIG_RTL_LOG,    \
    .algo_mode           = CS_ALGO_CONFIG_DEFAULT_ALGO_MODE,\
  }

/** @} (end addtogroup cs_algo) */

#endif // CS_ALGO_CONFIG_H
