/***************************************************************************//**
 * @file sl_rail_sdk_simple_cpc.h
 * @brief Simple RAIL CPC for NCP projects Component
 *******************************************************************************
 * # License
 * <b>Copyright 2020 Silicon Laboratories Inc. www.silabs.com</b>
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

#ifndef SL_RAIL_SIMPLE_CPC_H
#define SL_RAIL_SIMPLE_CPC_H

#include <stdint.h>
#include "sl_status.h"

/**
 * \addtogroup rail_sdk_extension
 * @{
 */
/**
 * \addtogroup rail_sdk_simple_cpc
 * @{
 */

/**
 * @brief Initializes Simple CPC for RAIL NCP projects.
 */
void sl_rail_sdk_simple_cpc_init(void);

/**
 * @brief Services CPC in baremetal applications.
 */
void sl_rail_sdk_simple_cpc_step(void);

/**
 * @brief Transmits a buffer over CPC.
 *
 * @param[in] len Length of the data in bytes.
 * @param[in] data Pointer to the payload to transmit.
 */
void sl_rail_sdk_simple_cpc_transmit(uint32_t len, const uint8_t *data);

/**
 * @brief Callback invoked when a CPC transmit completes.
 *
 * @param[in] status Result of the transmit operation.
 */
void sl_rail_sdk_simple_cpc_transmit_cb(sl_status_t status);

/**
 * @brief Polls CPC for received data.
 */
void sl_rail_sdk_simple_cpc_receive(void);

/**
 * @brief Callback invoked when CPC data is received.
 *
 * @param[in] status Result of the receive operation.
 * @param[in] len Length of the received data in bytes.
 * @param[in] data Pointer to the received payload.
 */
void sl_rail_sdk_simple_cpc_receive_cb(sl_status_t status, uint32_t len, uint8_t *data);

/**
 * @brief Initializes the OS task that services CPC when a kernel is present.
 */
void sl_rail_sdk_simple_cpc_os_task_init(void);

/**
 * @brief Notifies the CPC OS task to run when a kernel is present.
 */
void sl_rail_sdk_simple_cpc_os_task_proceed(void);

/** @} */ // end of rail_sdk_simple_cpc group
/** @} */ // end of extension group

#endif // SL_RAIL_SIMPLE_CPC_H
