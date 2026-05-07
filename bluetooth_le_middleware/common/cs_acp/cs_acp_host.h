/***************************************************************************//**
 * @file
 * @brief CS Application Co-Processor host API
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

#ifndef CS_ACP_HOST_H
#define CS_ACP_HOST_H

/***************************************************************************//**
 * @addtogroup cs_acp
 * @{
 ******************************************************************************/

#include "cs_acp_common.h"

#ifdef __cplusplus
extern "C" {
#endif

/**************************************************************************//**
 * Sends the CS ACP target a command.
 *
 * @param[in] cs_acp_cmd Command data. See: @ref cs_acp_cmd_t.
 * @param[in] max_response_size Size of output buffer passed in @p response
 * @param[out] response_len On return, set to the length of output data written
 *   to @p response
 * @param[out] response The response message
 *
 * @return SL_STATUS_OK if successful. Error code otherwise.
 *****************************************************************************/
sl_status_t cs_acp_send_cmd(const cs_acp_cmd_t *cs_acp_cmd,
                            size_t max_response_size,
                            size_t *response_len,
                            uint8_t *response);

#ifdef __cplusplus
};
#endif

/** @} (end addtogroup cs_acp) */
#endif // CS_ACP_HOST_H
