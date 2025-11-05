/***************************************************************************//**
 * @file app_decode.h
 * @brief Bluetooth Mesh message decoder example
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

#ifndef APP_DECODE_H
#define APP_DECODE_H

#ifdef __cplusplus
extern "C" {
#endif

// -----------------------------------------------------------------------------
// Enums, structs, typedefs

struct sl_mesh_decoder;

// -----------------------------------------------------------------------------
// Function declarations

/**
 * @brief Receive Mesh network PDU
 */
sl_status_t app_rx_net(struct sl_mesh_decoder *context,
                       uint32_t current_ivindex,
                       const uint8_t *ptr,
                       size_t len);

/**
 * @brief Receive Mesh network beacon
 */
sl_status_t app_rx_beacon(struct sl_mesh_decoder *context,
                          const uint8_t *ptr,
                          size_t len);

#ifdef __cplusplus
};
#endif

#endif // APP_DECODE_H
