/***************************************************************************/ /**
 * @file
 * @brief Internal UART driver asynchronous API
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

#ifndef SLI_UART_ASYNC_H
#define SLI_UART_ASYNC_H

#include "sl_dma_channel_device.h"
#include "sl_slist.h"
#include "sli_uart.h"

/*******************************************************************************
 *****************************   DATA TYPES   **********************************
 ******************************************************************************/
typedef struct sli_uart_async_transfer sli_uart_async_transfer_t;

///< UART Async Transfer structure
struct sli_uart_async_transfer {
  sl_slist_node_t node;                         ///< Slist node for queued transfers.
  void *data;                                   ///< Data for the transfer.
  size_t size;                                  ///< Size of the transfer.
  size_t bytes_completed;                       ///< Number of bytes transferred via DMA.
  sl_dma_channel_xfer_descriptor_t dma_desc;    ///< DMA Channel Driver descriptors.
};

/*******************************************************************************
 *****************************   PROTOTYPES   **********************************
 ******************************************************************************/

/***************************************************************************//**
 * Initializes the DMA for the given UART instance.
 *
 * @param[in]  uart_handle Handle to UART.
 *
 * @return @ref SL_STATUS_OK if successful.
 *         Error code otherwise.
 ******************************************************************************/
sl_status_t sli_uart_async_init(sl_uart_handle_t *uart_handle);

/***************************************************************************//**
 * Frees the DMA channel and de-initializes the DMA for the given UART instance.
 *
 * @param[in]  uart_handle Handle to UART.
 *
 * @return @ref SL_STATUS_OK if successful.
 *         Error code otherwise.
 ******************************************************************************/
sl_status_t sli_uart_async_deinit(sl_uart_handle_t *uart_handle);

/***************************************************************************//**
 * Handles the TX complete interrupt for the given UART instance.
 *
 * @param[in]  uart_handle Handle to UART.
 ******************************************************************************/
void sli_uart_async_transmit_complete(sl_uart_handle_t *uart_handle);

/***************************************************************************//**
 * Return the UART async transfer that owns a list node.
 *
 * @param[in]  node Slist node embedded in a transfer.
 *
 * @return Pointer to the transfer, or NULL if @p node is NULL.
 ******************************************************************************/
static inline sli_uart_async_transfer_t *sli_uart_async_transfer_from_node(sl_slist_node_t *node)
{
  return node ? SL_SLIST_ENTRY(node, sli_uart_async_transfer_t, node) : NULL;
}

/***************************************************************************//**
 * Pop a transfer from a list.
 *
 * @note Caller must invoke this from an atomic section.
 *
 * @param[in]  list_head Transfer list head.
 *
 * @return Pointer to a transfer, or NULL if the list is empty.
 ******************************************************************************/
static inline sli_uart_async_transfer_t *sli_uart_transfer_list_pop(sl_slist_node_t **list_head)
{
  EFM_ASSERT(list_head != NULL);

  return sli_uart_async_transfer_from_node(sl_slist_pop(list_head));
}

/***************************************************************************//**
 * Push a transfer onto a list.
 *
 * @param[in]  list_head Transfer list head.
 *
 * @param[in]  async_tfer Transfer to release.
 ******************************************************************************/
static inline void sli_uart_transfer_list_push(sl_slist_node_t **list_head,
                                               sli_uart_async_transfer_t *async_tfer)
{
  EFM_ASSERT(list_head != NULL);
  EFM_ASSERT(async_tfer != NULL);

  sl_slist_push(list_head, &async_tfer->node);
}

#endif // SLI_UART_ASYNC_H
