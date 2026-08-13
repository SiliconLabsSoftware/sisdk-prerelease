/***************************************************************************//**
 * @file
 * @brief CS Configurator - RTL library API
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

#ifndef CS_CONFIGURATOR_RTLIB_INTERNAL_H
#define CS_CONFIGURATOR_RTLIB_INTERNAL_H

// -----------------------------------------------------------------------------
// Includes

#include "cs_configurator.h"

// -----------------------------------------------------------------------------
// Function declarations

#ifdef __cplusplus
extern "C" {
#endif

/*******************************************************************************
 * RTL library implementation of CS estimation time
 * (cycle table and clock conversion)
 *
 * Used by the default @ref cs_configurator_get_estimation_time_us weak stub.
 *
 * @param[in] input      Pointer to configurator input parameters (@c cs_config
 *                       and @c cs_instance_config must be non-NULL;
 *                       channel map preset is validated like
 *                       @ref cs_configurator_validate_for_rtl).
 * @param[in] algo_mode  RTL algorithm mode.
 * @param[in] channel_map_preset Channel map preset.
 * @param[in] clock_frequency_hz Clock frequency in Hz.
 * @param[in] num_antenna_paths  Number of antenna paths.
 * @param[out] estimation_time_us Estimated procedure time in microseconds.
 *
 * @return Status of the operation.
 ******************************************************************************/
sl_status_t cs_configurator_rtllib_get_estimation_time_us(cs_configurator_parameters_t *input,
                                                          cs_algo_mode_t algo_mode,
                                                          cs_channel_map_preset_t channel_map_preset,
                                                          uint32_t clock_frequency_hz,
                                                          uint8_t num_antenna_paths,
                                                          uint32_t *estimation_time_us);

/*******************************************************************************
 * Validate CS configuration values against the supported configs in RTL LIB.
 *
 * @param[in] config             Pointer to configuration parameters.
 * @param[in] scheduling         Procedure scheduling mode.
 * @param[in] algo_mode          RTL CS algorithm mode (sl_rtl_cs_algo_mode).
 * @param[in] channel_map_preset Channel map preset.
 * @param[in] estimation_time_us Estimated procedure time in microseconds.
 * @param[in] peer_count         Number of connected peers.
 *
 * @return Status of the validation.
 ******************************************************************************/
sl_status_t cs_configurator_validate_for_rtl(cs_configurator_parameters_t *config,
                                             cs_procedure_scheduling_t scheduling,
                                             cs_algo_mode_t algo_mode,
                                             cs_channel_map_preset_t channel_map_preset,
                                             uint32_t estimation_time_us,
                                             uint8_t peer_count);

#ifdef __cplusplus
}
#endif

#endif // CS_CONFIGURATOR_RTLIB_INTERNAL_H
