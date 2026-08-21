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

/***************************************************************************//**
 * @addtogroup cs_algo
 * @{
 ******************************************************************************/

// <<< Use Configuration Wizard in Context Menu >>>

// <h> General

// <o CS_ALGO_CONFIG_ESTIMATOR_COUNT> Number of connections that own a CS estimator <1..4>
// <i> A connection owns 0 or 1 estimator, so this value must not exceed the
// <i> maximum number of simultaneous connections, therefore should be in
// <i> the range [1 .. CS_RREQ_CONFIG_MAX_CONNECTIONS].
// <i> Default: 1
#define CS_ALGO_CONFIG_ESTIMATOR_COUNT            1

// <o CS_ALGO_CONFIG_DEFAULT_RSSI_REF_TX_POWER> RSSI reference TX power <-110..30>
// <i> Reference RSSI value of the remote device at 1.0 m distance in dBm
// <i> Default: -40.0F
#define CS_ALGO_CONFIG_DEFAULT_RSSI_REF_TX_POWER  -40.0F

// </h>

// <h> Logging

// <e CS_ALGO_CONFIG_LOG> Enable
// <i> Default: 1
// <i> Enable Algo component logging
#define CS_ALGO_CONFIG_LOG                        1

// <s CS_ALGO_CONFIG_LOG_PREFIX> Prefix
// <i> Default: "[Algo]"
#define CS_ALGO_CONFIG_LOG_PREFIX                 "[Algo] "

// <q CS_ALGO_CONFIG_RTL_LOG> RTL Library log
// <i> Default: 1
// <i> Enable RTL Library logging
#define CS_ALGO_CONFIG_RTL_LOG                    1

// </e>

// </h>

// <h> Runtime settings

// <o CS_ALGO_CONFIG_CONFIG_WAIT> Timeout for guard (in ticks)
// <i> Default: 10
#define CS_ALGO_CONFIG_CONFIG_WAIT                10

// </h>

// <h> Algorithm

// <o CS_ALGO_CONFIG_DEFAULT_ALGO_MODE> Object tracking mode
// <CS_ALGO_MODE_TRACKING_ACCURACY_OPTIMIZED=> Tracking accuracy optimized (suitable for moving targets)
// <CS_ALGO_MODE_STATIONARY=> Stationary (suitable for stationary targets)
// <CS_ALGO_MODE_TRACKING_LATENCY_OPTIMIZED=> Tracking latency optimized (suitable for fast moving targets)
// <i> Default: CS_ALGO_MODE_TRACKING_LATENCY_OPTIMIZED
#define CS_ALGO_CONFIG_DEFAULT_ALGO_MODE        CS_ALGO_MODE_TRACKING_LATENCY_OPTIMIZED

// </h>

// <<< end of configuration section >>>

// Internal define to skip RTL processing
#define CS_ALGO_CONFIG_SKIP_RTL_PROCESS         0

/** @} (end addtogroup cs_algo) */
#endif // CS_ALGO_CONFIG_H
