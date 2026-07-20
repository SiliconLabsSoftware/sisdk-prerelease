/***************************************************************************//**
 * @file
 * @brief Serial Port Profile IO Stream configuration
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

#ifndef SPP_IOSTREAM_CONFIG_H
#define SPP_IOSTREAM_CONFIG_H

/***************************************************************************//**
 * @addtogroup spp_iostream
 * @{
 ******************************************************************************/

// <<< Use Configuration Wizard in Context Menu >>>

// <h> SPP IO Stream Configuration

// <s SPP_IOSTREAM_INSTANCE_NAME> IO Stream instance name
// <i> Default: "spp"
#define SPP_IOSTREAM_INSTANCE_NAME       "spp"

// <o SPP_IOSTREAM_RX_BUFFER_SIZE> Receive buffer size (bytes) <1-2048>
// <i> Define the size of the local receive buffer in bytes.
// <i> Default: 512
#define SPP_IOSTREAM_RX_BUFFER_SIZE      512

// </h>

// <<< end of configuration section >>>

/** @} (end addtogroup spp_iostream) */

#endif // SPP_IOSTREAM_CONFIG_H
