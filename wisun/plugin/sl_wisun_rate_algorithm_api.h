/***************************************************************************//**
 * @file sl_wisun_rate_algorithm_api.h
 * @brief wisun rate algorithm apis
 *******************************************************************************
 * # License
 * <b>Copyright 2022 Silicon Laboratories Inc. www.silabs.com</b>
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
//                                   Includes
// -----------------------------------------------------------------------------
#include "sl_assert.h"
#include "sl_wisun_api.h"

/**************************************************************************//**
 * @brief Set the rate algorithm
 * @param[in] type 0-NONE
 *                 1-ONOE
 *                 2-MINSTREL
 *                 3-PCMA
 * @param[in] neighbor_address MAC address of the peer to configure rate algorithm
 *                             Can be **sl_wisun_broadcast_mac** to set rate algorithm for all neighbors
 * @return SL_STATUS_OK if successful, an error code otherwise
 *****************************************************************************/
sl_status_t sl_wisun_config_rate_algorithm(uint8_t type, const sl_wisun_mac_address_t *neighbor_address);

/**************************************************************************//**
 * @brief Get Rate algorithm rates and statistics
 * @param[in] neighbor_address MAC address of the peer to get rate algorithm stats
 * @param[in] rates list of available rates and statistics
 * @param[in,out] rate_count number of rates allocated in "rates" and returned with the effective number of rates available
 * @return SL_STATUS_OK if successful, an error code otherwise
 *****************************************************************************/
sl_status_t sl_wisun_get_rate_algorithm_stats(const sl_wisun_mac_address_t *neighbor_address, sl_wisun_rate_t *rates, uint16_t *rate_count);