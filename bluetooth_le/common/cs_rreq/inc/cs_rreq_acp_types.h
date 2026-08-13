/***************************************************************************//**
 * @file
 * @brief CS RREQ ACP types header
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
#ifndef CS_RREQ_ACP_TYPES_H
#define CS_RREQ_ACP_TYPES_H

#include "sl_common.h"
#include "sl_status.h"
#include "cs_acp_common.h"
#include "cs_rreq_api.h"

// -----------------------------------------------------------------------------
// Command structures

/// CS ACP RREQ create command structure
SL_PACK_START(1)
typedef struct {
  uint8_t conn_handle;            ///< Connection handle the RREQ instance is created for.
  cs_rreq_create_config_t config; ///< RREQ configuration to apply.
} SL_ATTRIBUTE_PACKED cs_acp_cmd_rreq_create_t;
SL_PACK_END()

/// CS ACP RREQ enable command structure
SL_PACK_START(1)
typedef struct {
  uint8_t conn_handle;            ///< Connection handle of the target RREQ instance.
  uint8_t enable;                 ///< 1 to enable ranging, 0 to disable.
} SL_ATTRIBUTE_PACKED cs_acp_cmd_rreq_enable_t;
SL_PACK_END()

/// CS ACP RREQ remove command structure
SL_PACK_START(1)
typedef struct {
  uint8_t conn_handle;            ///< Connection handle of the RREQ instance to remove.
} SL_ATTRIBUTE_PACKED cs_acp_cmd_rreq_remove_t;
SL_PACK_END()

/// CS ACP RREQ command structure
SL_PACK_START(1)
typedef struct {
  cs_acp_cmd_id_t id;             ///< Command identifier (@ref cs_acp_cmd_id_t).
  union {
    cs_acp_cmd_rreq_create_t cs_acp_cmd_rreq_create; ///< Payload for @ref CS_ACP_CMD_RREQ_CREATE.
    cs_acp_cmd_rreq_enable_t cs_acp_cmd_rreq_enable; ///< Payload for @ref CS_ACP_CMD_RREQ_ENABLE.
    cs_acp_cmd_rreq_remove_t cs_acp_cmd_rreq_remove; ///< Payload for @ref CS_ACP_CMD_RREQ_REMOVE.
  } data;                         ///< Command payload union.
} SL_ATTRIBUTE_PACKED cs_rreq_acp_cmd_t;
SL_PACK_END()

// -----------------------------------------------------------------------------
// Event structures

/// CS ACP RREQ create complete event structure
SL_PACK_START(1)
typedef struct {
  sl_status_t sc;                 ///< Status of the create operation.
  uint8_t conn_handle;            ///< Connection handle the RREQ instance was created for.
} SL_ATTRIBUTE_PACKED cs_acp_evt_rreq_create_complete_t;
SL_PACK_END()

/// CS ACP RREQ enable complete event structure
SL_PACK_START(1)
typedef struct {
  sl_status_t sc;                 ///< Status of the enable/disable operation.
  uint8_t conn_handle;            ///< Connection handle of the affected RREQ instance.
  uint8_t enable;                 ///< 1 if ranging was enabled, 0 if disabled.
} SL_ATTRIBUTE_PACKED cs_acp_evt_rreq_enable_complete_t;
SL_PACK_END()

/// CS ACP RREQ error event structure
SL_PACK_START(1)
typedef struct {
  sl_status_t sc;                 ///< Underlying status code.
  uint8_t conn_handle;            ///< Connection handle the error is associated with.
  cs_rreq_error_t error;          ///< RREQ error code.
} SL_ATTRIBUTE_PACKED cs_acp_evt_rreq_error_t;
SL_PACK_END()

/// CS ACP RREQ event structure
SL_PACK_START(1)
typedef struct {
  cs_acp_evt_id_t id;             ///< Event identifier (@ref cs_acp_evt_id_t).
  union {
    cs_acp_evt_rreq_create_complete_t cs_acp_evt_rreq_create_complete; ///< Payload for @ref CS_ACP_EVT_RREQ_CREATE_COMPLETE.
    cs_acp_evt_rreq_enable_complete_t cs_acp_evt_rreq_enable_complete; ///< Payload for @ref CS_ACP_EVT_RREQ_ENABLE_COMPLETE.
    cs_acp_evt_rreq_error_t cs_acp_evt_rreq_error;                     ///< Payload for @ref CS_ACP_EVT_RREQ_ERROR.
  } data;                         ///< Event payload union.
} SL_ATTRIBUTE_PACKED cs_rreq_acp_evt_t;
SL_PACK_END()

#endif // CS_RREQ_ACP_TYPES_H
