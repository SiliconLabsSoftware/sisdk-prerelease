/***************************************************************************//**
 * @file
 * @brief CS Manager - Core implementation
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

// -----------------------------------------------------------------------------
// Includes

#include <string.h>
#include "sl_status.h"
#include "sl_common.h"
#include "cs_manager.h"
#include "cs_manager_internal.h"
#include "cs_manager_config.h"
#include "cs_manager_cs_config_config.h"
#include "cs_manager_config_db_internal.h"
#include "cs_manager_log_internal.h"

// -----------------------------------------------------------------------------
// Public functions

sl_status_t cs_manager_cs_config_create(uint8_t conn_handle,
                                        uint8_t config_id,
                                        bool create_context,
                                        const cs_config_t *config)
{
  if (config == NULL) {
    return SL_STATUS_NULL_POINTER;
  }

  cs_manager_t *m = cs_manager_find(conn_handle);
  if (m == NULL) {
    return SL_STATUS_NOT_FOUND;
  }

  // Creating a CS configuration is only allowed when the instance is idle
  if (m->state != CS_MANAGER_STATE_IDLE) {
    cs_manager_log_error(INSTANCE_PREFIX "Cannot create CS configuration in "
                                         "state %u (config_id=%u)" NL,
                         conn_handle,
                         (unsigned)m->state,
                         (unsigned)config_id);
    return SL_STATUS_INVALID_STATE;
  }

  uint8_t role = m->instance_config.is_initiator
                 ? (uint8_t)sl_bt_cs_role_initiator
                 : (uint8_t)sl_bt_cs_role_reflector;

  // The completion of this command is reported asynchronously via
  // sl_bt_evt_cs_config_complete_id.
  sl_status_t sc = sl_bt_cs_create_config(conn_handle,
                                          config_id,
                                          create_context ? 1u : 0u,
                                          config->main_mode_type,
                                          config->sub_mode_type,
                                          config->min_main_mode_steps,
                                          config->max_main_mode_steps,
                                          config->main_mode_repetition,
                                          config->mode_calibration_steps,
                                          role,
                                          config->rtt_type,
                                          config->cs_sync_phy,
                                          &config->channel_map,
                                          config->channel_map_repetition,
                                          config->channel_selection_type,
                                          config->ch3c_shape,
                                          config->ch3c_jump,
                                          0u);
  if (sc != SL_STATUS_OK) {
    cs_manager_log_error(INSTANCE_PREFIX "Failed to start CS configuration "
                                         "(config_id=%u, sc=0x%lx)" NL,
                         conn_handle,
                         (unsigned)config_id,
                         (unsigned long)sc);
    return sc;
  } else {
    m->state = CS_MANAGER_STATE_CONFIGURING;
    cs_manager_log_info(INSTANCE_PREFIX "CS configuration creation started "
                                        "(config_id=%u)" NL,
                        conn_handle,
                        (unsigned)config_id);
  }

  return sc;
}

sl_status_t cs_manager_cs_config_remove(uint8_t conn_handle,
                                        uint8_t config_id)
{
  sl_status_t sc;
  cs_manager_t *m = cs_manager_find(conn_handle);
  if (m == NULL) {
    return SL_STATUS_NOT_FOUND;
  }

  // A configuration may only be removed when the instance is idle.
  if (m->state != CS_MANAGER_STATE_IDLE) {
    cs_manager_log_error(INSTANCE_PREFIX "Cannot remove CS configuration in "
                                         "state %u (config_id=%u)" NL,
                         conn_handle,
                         (unsigned)m->state,
                         (unsigned)config_id);
    return SL_STATUS_INVALID_STATE;
  }

  cs_manager_log_info(INSTANCE_PREFIX "Removing CS configuration "
                                      "(config_id=%u)" NL,
                      conn_handle,
                      (unsigned)config_id);

  // The completion of this command is reported asynchronously via
  // sl_bt_evt_cs_config_complete_id.
  sc = sl_bt_cs_remove_config(conn_handle, config_id);
  if (sc != SL_STATUS_OK) {
    cs_manager_log_error(INSTANCE_PREFIX "Failed to start CS configuration "
                                         "removal (config_id=%u, sc=0x%lx)" NL,
                         conn_handle,
                         (unsigned)config_id,
                         (unsigned long)sc);
    return sc;
  }

  m->state = CS_MANAGER_STATE_REMOVING_CONFIG;
  return SL_STATUS_OK;
}

sl_status_t cs_manager_cs_config_get_default_config(cs_config_t *config)
{
  if (config == NULL) {
    return SL_STATUS_NULL_POINTER;
  }
  static const sl_bt_cs_channel_map_t default_channel_map = {
    .data = CS_MANAGER_DEFAULT_CHANNEL_MAP
  };

  memset(config, 0, sizeof(*config));
  config->channel_map            = default_channel_map;
  config->channel_map_repetition = CS_MANAGER_DEFAULT_CHANNEL_MAP_REPETITION;
  config->channel_selection_type = CS_MANAGER_DEFAULT_CHANNEL_SELECTION_TYPE;
  config->main_mode_type         = CS_MANAGER_DEFAULT_MAIN_MODE_TYPE;
  config->sub_mode_type          = CS_MANAGER_DEFAULT_SUB_MODE_TYPE;
  config->min_main_mode_steps    = CS_MANAGER_DEFAULT_MIN_MAIN_MODE_STEPS;
  config->max_main_mode_steps    = CS_MANAGER_DEFAULT_MAX_MAIN_MODE_STEPS;
  config->main_mode_repetition   = CS_MANAGER_DEFAULT_MAIN_MODE_REPETITION;
  config->mode_calibration_steps = CS_MANAGER_DEFAULT_MODE_CALIBRATION_STEPS;
  config->rtt_type               = CS_MANAGER_DEFAULT_RTT_TYPE;
  config->cs_sync_phy            = CS_MANAGER_DEFAULT_CS_SYNC_PHY;
  config->ch3c_shape             = CS_MANAGER_DEFAULT_CH3C_SHAPE;
  config->ch3c_jump              = CS_MANAGER_DEFAULT_CH3C_JUMP;
  config->reserved               = 0;
  return SL_STATUS_OK;
}
