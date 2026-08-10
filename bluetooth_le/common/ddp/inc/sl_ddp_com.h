/***************************************************************************//**
 * @file
 * @brief Communication interface for DDP
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

#ifndef SL_DDP_COM_H
#define SL_DDP_COM_H

#include <stdint.h>
#include "sl_common.h"
#include "sl_ddp_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/******************************************************************************
 * @addtogroup SL_DDP_COM_API DDP COM API
 *
 * DDP COM API provides a host interface for interacting with the dynamic
 * provisioning application using IO Stream.
 *
 * The host writes a sl_ddp_com_req_t request using the interface, receiving
 * a sl_ddp_com_rsp_t response once the request has been handled. Multiple
 * requests can be written, up to the maximum size of the input buffer.
 *
 * @{
 *****************************************************************************/

// DDP COM command request
SL_PACK_START(1)
typedef struct {
  uint16_t id; // DDP Command ID
  uint16_t data_len; // Length of command specific input structure in bytes
  uint8_t data[]; // Command specific input structure
} SL_ATTRIBUTE_PACKED sl_ddp_com_req_t;
SL_PACK_END()

// DDP COM command response
SL_PACK_START(1)
typedef struct {
  uint32_t status; // Status of the corresponding command request
  uint16_t data_len; // Length of command specific output structure in bytes
  uint8_t data[]; // Command specific output structure
} SL_ATTRIBUTE_PACKED sl_ddp_com_rsp_t;
SL_PACK_END()

/***************************************************************************//**
 * Initialize DPP COM interface.
 *
 * This function is used to initialize the DPP COM interface and it must be
 * called before any other function in this interface. Typically this happens
 * automatically with System Init.
 ******************************************************************************/
void sl_ddp_com_init(void);

/***************************************************************************//**
 * Process DPP COM interface.
 *
 * This function is used to process any incoming and outgoing DPP COM data.
 * In order to continue processing data, it must be called periodically. This
 * is typically called by System Init.
 ******************************************************************************/
void sl_ddp_com_process_action(void);

/** @} (end SL_DDP_COM_API) */

#ifdef __cplusplus
}
#endif

#endif  // SL_DDP_COM_H
