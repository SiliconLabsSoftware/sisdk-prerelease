/***************************************************************************//**
 * @file
 * @brief Clock Manager PERPLL predefined frequency table.
 *******************************************************************************
 * # License
 * <b>Copyright 2025 Silicon Laboratories Inc. www.silabs.com</b>
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

#ifndef SLI_CLOCK_MANAGER_PERPLL_H
#define SLI_CLOCK_MANAGER_PERPLL_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Number of predefined target frequencies.
// Matches sli_clock_manager_perpll_predefined_frequency_t.
#define SLI_CLOCK_MANAGER_PERPLL_FREQ_COUNT           12

// Field indexes in a PERPLL frequency table entry.
#define SLI_CLOCK_MANAGER_PERPLL_FIELD_FREQ           0
#define SLI_CLOCK_MANAGER_PERPLL_FIELD_DIVN           1
#define SLI_CLOCK_MANAGER_PERPLL_FIELD_DIVF           2
#define SLI_CLOCK_MANAGER_PERPLL_FIELD_DCO_DIV        3
#define SLI_CLOCK_MANAGER_PERPLL_FIELD_DIV_2POW       4
#define SLI_CLOCK_MANAGER_PERPLL_FIELD_COUNT          5

// PERPLL divider settings for the compile-time HFXO reference
// (SL_CLOCK_MANAGER_HFXO_FREQ) and each predefined target frequency.
// Indexed as
// [sli_clock_manager_perpll_predefined_frequency_t][SLI_CLOCK_MANAGER_PERPLL_FIELD_*].
// All entries use fractional-N mode. Defined in the Clock Manager runtime HAL.
extern const uint32_t sli_clock_manager_perpll_freq_table[SLI_CLOCK_MANAGER_PERPLL_FREQ_COUNT]
[SLI_CLOCK_MANAGER_PERPLL_FIELD_COUNT];

#ifdef __cplusplus
}
#endif

#endif // SLI_CLOCK_MANAGER_PERPLL_H
