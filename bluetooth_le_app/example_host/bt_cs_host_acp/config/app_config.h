/***************************************************************************//**
 * @file
 * @brief CS Reflector configuration
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

#ifndef APP_CONFIG_H
#define APP_CONFIG_H

// <s REFLECTOR_DEVICE_NAME> Device name
// <i> Default: "CS RFLCT"
#define REFLECTOR_DEVICE_NAME "CS RFLCT"

// <o CS_HOST_HEADER_LOG> Header log of measurements results <1..20>
// <i> Sets how many measurements are written between header logs
// <i> Default: 5
#define CS_HOST_HEADER_LOG                5

// <q CS_CAPABILITIES_LOG> Log local/remote CS supported capabilities table
// <i> Enable human-readable capability comparison on remote capabilities read.
// <i> Output uses log_debug; runtime filtering is handled by app_log.
// <i> Default: 1
#define CS_CAPABILITIES_LOG                 1

// <o CS_NCP_CLOCK_FREQUENCY_HZ> CS NCP clock frequency <40000000..160000000>
// <i> The clock frequency of the CS NCP application
// <i> This value is used to calculate the estimation time for the CS algorithm
// <i> Default: 80000000
#define CS_NCP_CLOCK_FREQUENCY_HZ             80000000

#endif // CS_REFLECTOR_CONFIG_H
