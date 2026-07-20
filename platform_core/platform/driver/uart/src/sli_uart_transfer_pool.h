/***************************************************************************//**
 * @file
 * @brief UART Async Transfer Pool API
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

#ifndef SLI_UART_TRANSFER_POOL_H
#define SLI_UART_TRANSFER_POOL_H

#include "sl_uart.h"

#ifdef __cplusplus
extern "C" {
#endif

/***************************************************************************//**
 * Initializes the async transfer pools for the given UART instance.
 *
 * @param[in]  uart_handle Handle to UART.
 *
 * @return @ref SL_STATUS_OK if successful. Error code otherwise.
 ******************************************************************************/
sl_status_t sli_uart_transfer_pool_init(sl_uart_handle_t *uart_handle);

/***************************************************************************//**
 * Frees the async transfer pools for the given UART instance.
 *
 * @param[in]  uart_handle Handle to UART.
 ******************************************************************************/
void sli_uart_transfer_pool_deinit(sl_uart_handle_t *uart_handle);

#ifdef __cplusplus
}
#endif

#endif // SLI_UART_TRANSFER_POOL_H
