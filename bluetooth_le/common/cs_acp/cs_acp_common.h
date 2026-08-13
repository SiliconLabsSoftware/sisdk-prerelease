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

#include "sl_enum.h"
#include "sl_common.h"

/***************************************************************************//**
 * @addtogroup cs_acp
 * @{
 ******************************************************************************/

/// CS ACP commands
/// Command enumerator defines the available ACP commands
/// between the host application and target device.
SL_ENUM(cs_acp_cmd_id_t) {
  CS_ACP_CMD_RREQ_CREATE                  = 0x00, ///< Create a Ranging Requester (RREQ) instance.
  CS_ACP_CMD_RREQ_ENABLE                  = 0x01, ///< Enable or disable ranging on an RREQ instance.
  CS_ACP_CMD_RREQ_REMOVE                  = 0x02, ///< Remove a Ranging Requester (RREQ) instance.
  CS_ACP_CMD_ALGO_CREATE                  = 0x10, ///< Create a ranging algorithm instance.
  CS_ACP_CMD_ALGO_REMOVE                  = 0x11, ///< Remove a ranging algorithm instance.
  CS_ACP_CMD_ENABLE_TRACE                 = 0x20, ///< Enable or disable target-side trace output.
};

/// CS ACP events
/// Event enumerator defines the available ACP events
/// between the host application and target device.
SL_ENUM(cs_acp_evt_id_t) {
  CS_ACP_EVT_RREQ_CREATE_COMPLETE         = 0x00, ///< RREQ instance creation completed.
  CS_ACP_EVT_RREQ_ENABLE_COMPLETE         = 0x01, ///< RREQ enable/disable completed.
  CS_ACP_EVT_RREQ_ERROR                   = 0x02, ///< RREQ reported an error.
  CS_ACP_EVT_ALGO_ON_RESULT               = 0x10, ///< Algo produced a final result.
  CS_ACP_EVT_ALGO_ON_INTERMEDIATE_RESULT  = 0x11, ///< Algo produced an intermediate result.
  CS_ACP_EVT_ALGO_ON_ERROR                = 0x12, ///< Algo reported an error.
};

/// ACP command message exchanged between the host and the target.
SL_PACK_START(1)
typedef struct {
  cs_acp_cmd_id_t id;             ///< Command identifier (@ref cs_acp_cmd_id_t).
  union {
    bool cs_acp_cmd_enable_trace; ///< Trace enable flag (@ref CS_ACP_CMD_ENABLE_TRACE).
  } data;                         ///< Command payload union.
} SL_ATTRIBUTE_PACKED cs_acp_cmd_t;
SL_PACK_END()

/** @} (end addtogroup cs_acp) */
#endif // CS_ACP_COMMON_H
