/***************************************************************************//**
 * @file
 * @brief CS Configurator API
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

#ifndef CS_CONFIGURATOR_H
#define CS_CONFIGURATOR_H

/***************************************************************************//**
 * @addtogroup cs_configurator
 * @{
 ******************************************************************************/

// -----------------------------------------------------------------------------
// Includes

#include <stdint.h>
#include <stdbool.h>
#include "sl_status.h"
#include "sl_enum.h"
#include "sl_bt_api.h"
#include "cs_rreq_api.h"
#include "cs_manager.h"
#include "cs_common.h"

// -----------------------------------------------------------------------------
// Definitions

/// CS configurator input parameter type
typedef struct {
  cs_manager_instance_config_t *cs_instance_config; ///< CS instance settings.
  cs_config_t *cs_config;                           ///< CS procedure settings.
  cs_rreq_create_config_t *rreq_config;             ///< RREQ creation settings.
  cs_procedure_parameters_t *cs_procedure_parameters; ///< CS procedure parameters.
  cs_manager_connection_parameters_t *connection_parameters; ///< CS connection parameters.
} cs_configurator_parameters_t;

// -----------------------------------------------------------------------------
// Function declarations

#ifdef __cplusplus
extern "C" {
#endif

/***************************************************************************//**
 * Calculate CS estimation time for the provided input parameters.
 *
 * When using the Silicon Labs RTL CS library, the default weak implementation
 * forwards to @ref cs_configurator_rtllib_get_estimation_time_us (see
 * cs_configurator_rtllib.h).
 *
 * @param[in] input               Pointer to configurator input parameters.
 * @param[in] algo_mode           RTL algorithm mode.
 * @param[in] channel_map_preset Channel map preset.
 * @param[in] clock_frequency_hz Clock frequency in Hz.
 * @param[in] num_antenna_paths  Number of antenna paths.
 * @param[out] estimation_time_us_out Estimated procedure time in microseconds.
 *
 * @return Status of the operation.
 ******************************************************************************/
sl_status_t cs_configurator_get_estimation_time_us(cs_configurator_parameters_t *input,
                                                   cs_algo_mode_t algo_mode,
                                                   cs_channel_map_preset_t channel_map_preset,
                                                   uint32_t clock_frequency_hz,
                                                   uint8_t num_antenna_paths,
                                                   uint32_t *estimation_time_us_out);

/***************************************************************************//**
 * Validate CS configuration values against timing and peer constraints.
 *
 * @param[in] config             Pointer to configuration parameters.
 * @param[in] channel_map_preset Channel map preset.
 * @param[in] estimation_time_us Estimated procedure time in microseconds.
 * @param[in] peer_count         Number of connected peers.
 * @param[in] num_antenna_paths  Number of antenna paths.
 *
 * @return Status of the validation.
 ******************************************************************************/
sl_status_t cs_configurator_validate(cs_configurator_parameters_t *config,
                                     cs_channel_map_preset_t channel_map_preset,
                                     uint32_t estimation_time_us,
                                     uint8_t peer_count,
                                     uint8_t num_antenna_paths);

/***************************************************************************//**
 * Optimize CS configuration values based on scheduling and topology.
 *
 * @param[in] scheduling         Procedure scheduling mode.
 * @param[in] channel_map_preset Channel map preset.
 * @param[in] estimation_time_us Estimated procedure time in microseconds.
 * @param[in] peer_count         Number of connected peers.
 * @param[in] num_antenna_paths  Number of antenna paths.
 * @param[in,out] parameters_inout Pointer to parameters to be updated.
 *
 * @return Status of the optimization.
 ******************************************************************************/
sl_status_t cs_configurator_optimize(cs_procedure_scheduling_t scheduling,
                                     cs_channel_map_preset_t channel_map_preset,
                                     uint32_t estimation_time_us,
                                     uint8_t peer_count,
                                     uint8_t num_antenna_paths,
                                     cs_configurator_parameters_t *parameters_inout);

/***************************************************************************//**
 * Apply a preset to a channel map data array
 *
 * @param[in]  preset       Channel map preset
 * @param[out] channel_map Channel map data array
 ******************************************************************************/
void cs_configurator_apply_channel_map_preset(cs_channel_map_preset_t preset,
                                              uint8_t *channel_map);
#ifdef __cplusplus
}
#endif

/** @} (end addtogroup cs_configurator) */
#endif // CS_CONFIGURATOR_H
