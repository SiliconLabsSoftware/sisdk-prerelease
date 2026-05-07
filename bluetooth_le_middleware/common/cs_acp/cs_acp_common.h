/***************************************************************************//**
 * @file
 * @brief CS Application Co-Processor common API
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

#ifndef CS_ACP_COMMON_H
#define CS_ACP_COMMON_H

/***************************************************************************//**
 * @addtogroup cs_acp
 * @{
 ******************************************************************************/

#include "sl_status.h"
#include "sl_common.h"
#include "sl_enum.h"
#include "sl_bgapi.h"
#include "sl_component_catalog.h"

#ifdef SL_CATALOG_CS_RREQ_PRESENT
#include "cs_rreq_api.h"
#endif // SL_CATALOG_CS_RREQ_PRESENT

/// @name CS ACP commands
/// @brief Command enumerator defines the available ACP commands
///        between the host application and target device.
SL_ENUM(cs_acp_cmd_e) {
#ifdef SL_CATALOG_CS_RREQ_PRESENT
  CS_ACP_CMD_RREQ_CREATE                  = 0x00,
  CS_ACP_CMD_RREQ_ENABLE                  = 0x01,
  CS_ACP_CMD_RREQ_REMOVE                  = 0x02,
#endif // SL_CATALOG_CS_RREQ_PRESENT
#ifdef SL_CATALOG_CS_ALGO_PRESENT
  CS_ACP_CMD_ALGO_CONFIGURE               = 0x10,
#endif // SL_CATALOG_CS_ALGO_PRESENT
};

/// @name CS ACP events
/// @brief ACP events between the host application and target device.
SL_ENUM(cs_acp_evt_e) {
#ifdef SL_CATALOG_CS_RREQ_PRESENT
  CS_ACP_EVT_RREQ_CREATE_COMPLETE         = 0x00,
  CS_ACP_EVT_RREQ_ENABLE_COMPLETE         = 0x01,
  CS_ACP_EVT_RREQ_ERROR                   = 0x02,
#endif // SL_CATALOG_CS_RREQ_PRESENT
#ifdef SL_CATALOG_CS_ALGO_PRESENT
  CS_ACP_EVT_ALGO_CONFIG_STATUS           = 0x10,
  CS_ACP_EVT_ALGO_RESULT_ID               = 0x11,
  CS_ACP_EVT_ALGO_INTERMEDIATE_RESULT_ID  = 0x12,
  CS_ACP_EVT_ALGO_EXTENDED_RESULT_ID      = 0x13,
#endif // SL_CATALOG_CS_ALGO_PRESENT
};

// -----------------------------------------------------------------------------
// Command structures

#ifdef SL_CATALOG_CS_RREQ_PRESENT
/// @name CS ACP RREQ create command structure
SL_PACK_START(1)
typedef struct {
  uint8_t conn_handle;
  cs_rreq_create_config_t config;
} SL_ATTRIBUTE_PACKED cs_acp_cmd_rreq_create_t;
SL_PACK_END()

/// @name CS ACP RREQ enable command structure
SL_PACK_START(1)
typedef struct {
  uint8_t conn_handle;
  uint8_t enable;
} SL_ATTRIBUTE_PACKED cs_acp_cmd_rreq_enable_t;
SL_PACK_END()

/// @name CS ACP RREQ remove command structure
SL_PACK_START(1)
typedef struct {
  uint8_t conn_handle;
} SL_ATTRIBUTE_PACKED cs_acp_cmd_rreq_remove_t;
SL_PACK_END()
#endif // SL_CATALOG_CS_RREQ_PRESENT

#ifdef SL_CATALOG_CS_ALGO_PRESENT
/// @name CS ACP ALGO configure command structure
SL_PACK_START(1)
typedef struct {
  uint8_t conn_handle;
  uint8_t config_id;
  cs_algo_config_t config;
} SL_ATTRIBUTE_PACKED cs_acp_cmd_algo_configure_t;
SL_PACK_END()
#endif // SL_CATALOG_CS_ALGO_PRESENT

SL_PACK_START(1)
typedef struct {
  cs_acp_cmd_e id;
  // Union of commands.
  union {
#ifdef SL_CATALOG_CS_RREQ_PRESENT
    cs_acp_cmd_rreq_create_t cs_acp_cmd_rreq_create;
    cs_acp_cmd_rreq_enable_t cs_acp_cmd_rreq_enable;
    cs_acp_cmd_rreq_remove_t cs_acp_cmd_rreq_remove;
#endif // SL_CATALOG_CS_RREQ_PRESENT
#ifdef SL_CATALOG_CS_ALGO_PRESENT
    cs_acp_cmd_algo_configure_t cs_acp_cmd_algo_configure;
#endif // SL_CATALOG_CS_ALGO_PRESENT
  } data;
} SL_ATTRIBUTE_PACKED cs_acp_cmd_t;
SL_PACK_END()

// -----------------------------------------------------------------------------
// Event structures

#ifdef SL_CATALOG_CS_RREQ_PRESENT
/// @name CS ACP RREQ create complete event structure
SL_PACK_START(1)
typedef struct {
  uint8_t conn_handle;
  sl_status_t sc;
} SL_ATTRIBUTE_PACKED cs_acp_evt_rreq_create_complete_t;
SL_PACK_END()

/// @name CS ACP RREQ enable complete event structure
SL_PACK_START(1)
typedef struct {
  uint8_t conn_handle;
  uint8_t enable;
  sl_status_t sc;
} SL_ATTRIBUTE_PACKED cs_acp_evt_rreq_enable_complete_t;
SL_PACK_END()

/// @name CS ACP RREQ error event structure
SL_PACK_START(1)
typedef struct {
  uint8_t conn_handle;
  cs_rreq_error_t error;
  sl_status_t sc;
} SL_ATTRIBUTE_PACKED cs_acp_evt_rreq_error_t;
SL_PACK_END()
#endif // SL_CATALOG_CS_RREQ_PRESENT

#ifdef SL_CATALOG_CS_ALGO_PRESENT
/// @name CS ACP ALGO config status event structure
SL_PACK_START(1)
typedef struct {
  uint8_t conn_handle;
  uint8_t config_id;
} SL_ATTRIBUTE_PACKED cs_acp_evt_algo_config_status_t;
SL_PACK_END()

/// @name CS ACP ALGO result ID event structure
SL_PACK_START(1)
typedef struct {
  uint8_t conn_handle;
  uint8_t type_value_list[CS_RESULT_MAX_BUFFER_SIZE];
} SL_ATTRIBUTE_PACKED cs_acp_evt_algo_result_id_t;
SL_PACK_END()

/// @name CS ACP ALGO intermediate result ID event structure
SL_PACK_START(1)
typedef struct {
  uint8_t conn_handle;
  float progress_percentage;
} SL_ATTRIBUTE_PACKED cs_acp_evt_algo_intermediate_result_id_t;
SL_PACK_END()

/// @name CS ACP ALGO extended result ID event structure
SL_PACK_START(1)
typedef struct {
  uint8_t conn_handle;
  uint8_t fragments_left;
  uint8array fragment;
} SL_ATTRIBUTE_PACKED cs_acp_evt_algo_extended_result_id_t;
SL_PACK_END()
#endif // SL_CATALOG_CS_ALGO_PRESENT

SL_PACK_START(1)
typedef struct {
  cs_acp_evt_e id;
  // Union of commands.
  union {
#ifdef SL_CATALOG_CS_RREQ_PRESENT
    cs_acp_evt_rreq_create_complete_t cs_acp_evt_rreq_create_complete;
    cs_acp_evt_rreq_enable_complete_t cs_acp_evt_rreq_enable_complete;
    cs_acp_evt_rreq_error_t cs_acp_evt_rreq_error;
#endif // SL_CATALOG_CS_RREQ_PRESENT
#ifdef SL_CATALOG_CS_ALGO_PRESENT
    cs_acp_evt_algo_config_status_t cs_acp_evt_algo_config_status;
    cs_acp_evt_algo_result_id_t cs_acp_evt_algo_result_id;
    cs_acp_evt_algo_intermediate_result_id_t cs_acp_evt_algo_intermediate_result_id;
    cs_acp_evt_algo_extended_result_id_t cs_acp_evt_algo_extended_result_id;
#endif // SL_CATALOG_CS_ALGO_PRESENT
  } data;
} SL_ATTRIBUTE_PACKED cs_acp_evt_t;
SL_PACK_END()

/** @} (end addtogroup cs_acp) */
#endif // CS_ACP_COMMON_H
