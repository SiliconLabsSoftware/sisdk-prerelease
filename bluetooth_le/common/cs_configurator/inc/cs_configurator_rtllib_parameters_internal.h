/***************************************************************************//**
 * @file
 * @brief CS Configurator - RTL library parameters
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

#ifndef CS_CONFIGURATOR_RTLIB_PARAMETERS_INTERNAL_H
#define CS_CONFIGURATOR_RTLIB_PARAMETERS_INTERNAL_H

// -----------------------------------------------------------------------------
// Includes
#include <stdint.h>

// -----------------------------------------------------------------------------
// Macros

#define CS_CONF_CYCLES_TABLE_RTT_IDX 0
#define CS_CONF_CYCLES_TABLE_PBR_IDX 1
#define CS_CONF_CYCLES_TABLE_PBR_W_RTT_SUB_IDX 2

#define CS_CONF_CYCLES_TABLE_RT_BASIC_IDX 0
#define CS_CONF_CYCLES_TABLE_RT_FAST_IDX 1
#define CS_CONF_CYCLES_TABLE_STATIC_HIGH_ACC_IDX 2

// -----------------------------------------------------------------------------
// Static variables

// Cycle counts for RTL library.
// Revision: 86912ffae9657d8678280980445e7fc03248c28f
static const uint32_t cs_conf_rtl_lib_cycles_table[3][3] = {
  { 39600u, 0u, 39600u },             // RTT (RT_BASIC, RT_FAST, STATIC_HIGH_ACC)
  { 21313000u, 2998000u, 27261000u }, // PBR (RT_BASIC, RT_FAST, STATIC_HIGH_ACC)
  { 21352600u, 3037600u, 27300600u }, // PBR with RTT submode (RT_BASIC, RT_FAST, STATIC_HIGH_ACC)
};

#endif // CS_CONFIGURATOR_RTLIB_PARAMETERS_INTERNAL_H
