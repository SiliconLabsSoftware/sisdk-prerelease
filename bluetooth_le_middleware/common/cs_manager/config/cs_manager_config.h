/***************************************************************************//**
 * @file
 * @brief CS Manager configuration
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

// <<< Use Configuration Wizard in Context Menu >>>

// -----------------------------------------------------------------------------
// Macros

// <h> General

// <q CS_MANAGER_CONFIG_LOG_ENABLE> Log
// <i> Default: 0
#define CS_MANAGER_CONFIG_LOG_ENABLE    (0)

// <o CS_MANAGER_CONFIG_COUNT> Number of CS Manager configurations per connection <1..3>
// <i> Total capacity is SL_BT_CONFIG_MAX_CONNECTIONS * CS_MANAGER_CONFIG_COUNT
// <i> Default: 1
#define CS_MANAGER_CONFIG_COUNT         (1)

// <o CS_MANAGER_CONFIG_MAX_INSTANCES> Maximum number of CS Manager instances <1..32>
// <i> Default: 1
#define CS_MANAGER_CONFIG_MAX_INSTANCES       (1)

// </h>

// <s CS_MANAGER_CONFIG_LOG_PREFIX> Log prefix
// <i> Default: "[CS Manager]"
#define CS_MANAGER_CONFIG_LOG_PREFIX "[CS Manager]"

#endif // CS_MANAGER_CONFIG_H
