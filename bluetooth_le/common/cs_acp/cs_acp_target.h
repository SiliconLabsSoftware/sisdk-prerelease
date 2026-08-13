/***************************************************************************//**
 * @file
 * @brief CS Application Co-Processor target API
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

#ifndef CS_ACP_TARGET_H
#define CS_ACP_TARGET_H

#include <stdint.h>
#include "sl_status.h"

/***************************************************************************//**
 * @addtogroup cs_acp
 * @{
 ******************************************************************************/

#ifdef __cplusplus
extern "C" {
#endif

/***************************************************************************//**
 * Respond to a CS command arriving from the ACP host.
 *
 * @param[in] status Status of the command.
 * @param[in] data_len Size of the response data.
 * @param[in] data Response data.
 ******************************************************************************/
void cs_acp_send_cmd_rsp(sl_status_t status, uint8_t data_len, uint8_t *data);

/***************************************************************************//**
 * Sends the CS ACP host an event.
 *
 * @param[in] data Event data.
 * @param[in] data_len Size of the event data.
 ******************************************************************************/
void cs_acp_send_evt(uint8_t *data, uint8_t data_len);

#ifdef __cplusplus
};
#endif

/** @} (end addtogroup cs_acp) */
#endif // CS_ACP_TARGET_H
