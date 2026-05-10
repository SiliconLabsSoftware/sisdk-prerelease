/***************************************************************************//**
 * @file
 * @brief CS Algo - API header
 *******************************************************************************
 * # License
 * <b>Copyright 2025 Silicon Laboratories Inc. www.silabs.com</b>
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
#ifndef CS_ALGO_INTERNAL_H
#define CS_ALGO_INTERNAL_H

#include <stdint.h>
#include "cs_rreq.h"

#ifdef __cplusplus
extern "C"
{
#endif

/***************************************************************************//**
 * Initialize the cs_algo component.
 ******************************************************************************/
void cs_algo_init(void);

/***************************************************************************//**
 * Process RAS data received from the initiator. This is the handler function
 * registered via template_contribution and called by the jinja-generated
 * dispatch code in the initiator.
 *
 * @param[in] conn_handle connection handle.
 * @param[in] ranging_counter procedure ranging counter.
 * @param[in] proc_info procedure info
 * @param[in] ranging_data pointer to the RREQ ranging data result.
 ******************************************************************************/
void cs_algo_process_ras_data(uint8_t conn_handle,
                              uint16_t ranging_counter,
                              cs_rreq_procedure_info_t proc_info,
                              cs_rreq_result_t *ranging_data);

#ifdef __cplusplus
}
#endif

#endif // CS_ALGO_INTERNAL_H
