/***************************************************************************//**
 * @file
 * @brief CS Algo - configuration header
 *******************************************************************************
 * # License
 * <b>Copyright 2024 Silicon Laboratories Inc. www.silabs.com</b>
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

// <e CS_ALGO_LOG> Algo component
// <i> Default: 1
// <i> Enable Algo component logging
#ifndef CS_ALGO_LOG
#define CS_ALGO_LOG                              (1)
#endif

// <s CS_ALGO_LOG_PREFIX> Log prefix
// <i> Default: "[Algo]"
#ifndef CS_ALGO_LOG_PREFIX
#define CS_ALGO_LOG_PREFIX                       "[Algo]"
#endif

// <o CS_ALGO_ESTIMATOR_COUNT> Number of connections that own a CS estimator <1..4>
// <i> Default: 1
// <i> Must be in the range [1 .. SL_BT_CONFIG_MAX_CONNECTIONS] and must match
// <i> CS_INITIATOR_MAX_CONNECTIONS, since every initiator connection may
// <i> allocate 0 or 1 estimators.
#ifndef CS_ALGO_ESTIMATOR_COUNT
#define CS_ALGO_ESTIMATOR_COUNT 1
#endif

// Internal define to skip RTL processing
#ifndef CS_ALGO_SKIP_RTL_PROCESS
#define CS_ALGO_SKIP_RTL_PROCESS         0
#endif

// </e>

// </h>

// <<< end of configuration section >>>

/** @} (end addtogroup cs_algo) */

#endif // CS_ALGO_CONFIG_H
