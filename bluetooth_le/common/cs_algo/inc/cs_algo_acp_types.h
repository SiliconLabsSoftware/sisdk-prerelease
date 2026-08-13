/***************************************************************************//**
 * @file
 * @brief CS Algo ACP types header
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
#ifndef CS_ALGO_ACP_TYPES_H
#define CS_ALGO_ACP_TYPES_H

#include "sl_common.h"
#include "sl_enum.h"
#include "sl_status.h"
#include "sl_bt_api.h"
#include "cs_acp_common.h"
#include "cs_algo.h"

// -----------------------------------------------------------------------------
// Command structures

/// CS ACP ALGO create command structure
SL_PACK_START(1)
typedef struct {
  uint8_t conn_handle;     ///< Connection handle
  cs_algo_config_t config; ///< Algorithm configuration to apply.
} SL_ATTRIBUTE_PACKED cs_acp_cmd_algo_create_t;
SL_PACK_END()

/// CS ACP ALGO remove command structure
SL_PACK_START(1)
typedef struct {
  uint8_t conn_handle;     ///< Connection handle
} SL_ATTRIBUTE_PACKED cs_acp_cmd_algo_remove_t;
SL_PACK_END()

/// CS ACP ALGO command message (command id plus per-command payload)
SL_PACK_START(1)
typedef struct {
  cs_acp_cmd_id_t id;      ///< Command identifier (@ref cs_acp_cmd_id_t).
  union {
    cs_acp_cmd_algo_create_t cs_acp_cmd_algo_create; ///< Payload for @ref CS_ACP_CMD_ALGO_CREATE.
    cs_acp_cmd_algo_remove_t cs_acp_cmd_algo_remove; ///< Payload for @ref CS_ACP_CMD_ALGO_REMOVE.
  } data;                  ///< Command payload union.
} SL_ATTRIBUTE_PACKED cs_algo_acp_cmd_t;
SL_PACK_END()

// -----------------------------------------------------------------------------
// Event structures

/// CS ACP ALGO on result event structure
SL_PACK_START(1)
typedef struct {
  uint8_t conn_handle;       ///< Connection handle the result belongs to.
  uint16_t ranging_counter;  ///< Ranging counter identifying the procedure.
  uint32_t data_len;         ///< Number of valid bytes in @ref data.
  uint8_t data[];            ///< Serialized algorithm result payload (@ref data_len bytes).
} SL_ATTRIBUTE_PACKED cs_acp_evt_algo_on_result_t;
SL_PACK_END()

/// CS ACP ALGO on intermediate result event structure
SL_PACK_START(1)
typedef struct {
  uint8_t conn_handle;        ///< Connection handle the result belongs to.
  float progress_percentage;  ///< Measurement progress [%].
} SL_ATTRIBUTE_PACKED cs_acp_evt_algo_on_intermediate_result_t;
SL_PACK_END()

/// CS ACP ALGO on error event structure
SL_PACK_START(1)
typedef struct {
  uint8_t conn_handle;       ///< Connection handle the error is associated with.
  uint16_t ranging_counter;  ///< Ranging counter of the affected procedure.
  cs_algo_error_t error;     ///< Algorithm error code.
  sl_status_t sc;            ///< Underlying status code.
} SL_ATTRIBUTE_PACKED cs_acp_evt_algo_on_error_t;
SL_PACK_END()

/// CS ACP ALGO event message (event id plus per-event payload)
SL_PACK_START(1)
typedef struct {
  cs_acp_evt_id_t id;        ///< Event identifier (@ref cs_acp_evt_id_t).
  union {
    cs_acp_evt_algo_on_result_t cs_acp_evt_algo_on_result;                           ///< Payload for @ref CS_ACP_EVT_ALGO_ON_RESULT.
    cs_acp_evt_algo_on_intermediate_result_t cs_acp_evt_algo_on_intermediate_result; ///< Payload for @ref CS_ACP_EVT_ALGO_ON_INTERMEDIATE_RESULT.
    cs_acp_evt_algo_on_error_t cs_acp_evt_algo_on_error;                             ///< Payload for @ref CS_ACP_EVT_ALGO_ON_ERROR.
  } data;                    ///< Event payload union.
} SL_ATTRIBUTE_PACKED cs_algo_acp_evt_t;
SL_PACK_END()

#endif // CS_ALGO_ACP_TYPES_H
