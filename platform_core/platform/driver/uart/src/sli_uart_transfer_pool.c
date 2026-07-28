/***************************************************************************//**
 * @file
 * @brief UART Async Transfer Pool Implementation
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

#include "sl_assert.h"
#include "sl_memory_manager.h"
#include "sl_slist.h"
#include "sl_uart.h"

#include "sli_uart.h"
#include "sli_uart_async.h"
#include "sli_uart_transfer_pool.h"

/*******************************************************************************
 ***************************   LOCAL FUNCTIONS   *******************************
 ******************************************************************************/

/***************************************************************************//**
 * Free the transfer pool.
 ******************************************************************************/
static void uart_transfer_pool_free(sl_slist_node_t **pool)
{
  EFM_ASSERT(pool != NULL);

  sl_slist_node_t *node;

  while ((node = sl_slist_pop(pool))) {
    sl_memory_free(node);
  }

  *pool = NULL;
}

/***************************************************************************//**
 * Allocate transfers and add them to the pool.
 ******************************************************************************/
static sl_status_t uart_transfer_pool_alloc(size_t transfer_count,
                                            size_t transfer_size,
                                            sl_slist_node_t **pool)
{
  sl_status_t status;

  EFM_ASSERT(pool != NULL);

  sl_slist_init(pool);

  for (size_t i = 0; i < transfer_count; i++) {
    sli_uart_async_transfer_t *tfer;

    status = sl_memory_calloc(1,
                              transfer_size,
                              BLOCK_TYPE_LONG_TERM,
                              (void**)&tfer);
    if (status != SL_STATUS_OK) {
      // Allocation could have failed after a few elements were allocated. Free it to be safe.
      uart_transfer_pool_free(pool);
      return status;
    }

    sli_uart_transfer_list_push(pool, tfer);
  }

  return SL_STATUS_OK;
}

/*******************************************************************************
**************************   GLOBAL FUNCTIONS   *******************************
*******************************************************************************/
sl_status_t sli_uart_transfer_pool_init(sl_uart_handle_t *uart_handle)
{
  sl_status_t status;

  EFM_ASSERT(SLI_UART_HANDLE_IS_ASYNC(uart_handle));
  EFM_ASSERT(SLI_UART_HANDLE_IS_ASYNC(uart_handle));

  status = uart_transfer_pool_alloc(uart_handle->preinit_config.async_tx_transfer_count,
                                    sizeof(sli_uart_async_tx_transfer_t),
                                    &uart_handle->async_tx_free_list_head);
  if (status != SL_STATUS_OK) {
    return status;
  }

  status = uart_transfer_pool_alloc(uart_handle->preinit_config.async_rx_transfer_count,
                                    sizeof(sli_uart_async_rx_transfer_t),
                                    &uart_handle->async_rx_free_list_head);
  if (status != SL_STATUS_OK) {
    uart_transfer_pool_free(&uart_handle->async_tx_free_list_head);
    return status;
  }

  return SL_STATUS_OK;
}

void sli_uart_transfer_pool_deinit(sl_uart_handle_t *uart_handle)
{
  EFM_ASSERT(SLI_UART_HANDLE_IS_ASYNC(uart_handle));
  EFM_ASSERT(SLI_UART_HANDLE_IS_ASYNC(uart_handle));

  EFM_ASSERT(sl_slist_is_empty(uart_handle->async_tx_transfer_submitted_list_head));
  EFM_ASSERT(sl_slist_is_empty(uart_handle->async_rx_transfer_active_list_head));
  EFM_ASSERT(sl_slist_is_empty(uart_handle->async_rx_transfer_pending_list_head));

  uart_transfer_pool_free(&uart_handle->async_tx_free_list_head);
  uart_transfer_pool_free(&uart_handle->async_rx_free_list_head);
}
