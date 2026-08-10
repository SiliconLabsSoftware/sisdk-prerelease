/***************************************************************************//**
 * @file
 * @brief UART Driver
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

#include <stdlib.h>
#include <string.h>

#include "sl_assert.h"
#include "sl_common.h"
#include "sl_core.h"
#include "sl_device_peripheral.h"
#include "sl_device_peripheral_types.h"
#include "sl_dma_manager.h"
#include "sl_dma_channel.h"
#include "sl_dma_channel_device.h"
#include "sl_slist.h"
#include "sl_status.h"
#include "sl_uart.h"

#include "sli_uart.h"
#include "sli_uart_async.h"
#include "sli_uart_async_types.h"
#include "sli_uart_transfer_pool.h"

/*******************************************************************************
 ***************************   LOCAL FUNCTIONS   *******************************
 ******************************************************************************/

/***************************************************************************//**
 * Enable TX.
 *
 * @return Whether the UART changed state.
 ******************************************************************************/
static inline bool uart_async_enable_tx(sl_uart_handle_t *uart_handle)
{
  if (uart_handle->async_tx_state == SL_UART_HANDLE_STATE_ACTIVE) {
    return false;
  }

  uart_handle->async_tx_state = SL_UART_HANDLE_STATE_ACTIVE;

  return true;
}

/***************************************************************************//**
 * Disable TX.
 *
 * @return Whether the UART changed state.
 ******************************************************************************/
static inline bool uart_async_disable_tx(sl_uart_handle_t *uart_handle)
{
  if (uart_handle->async_tx_state == SL_UART_HANDLE_STATE_IDLE) {
    return false;
  }

  uart_handle->async_tx_state = SL_UART_HANDLE_STATE_IDLE;

  return true;
}

/***************************************************************************//**
 * Enable RX.
 *
 * @return Whether the UART changed state.
 ******************************************************************************/
static inline bool uart_async_enable_rx(sl_uart_handle_t *uart_handle)
{
  if (uart_handle->async_rx_state == SL_UART_HANDLE_STATE_ACTIVE) {
    return false;
  }

  uart_handle->async_rx_state = SL_UART_HANDLE_STATE_ACTIVE;

  return true;
}

/***************************************************************************//**
 * Disable RX.
 *
 * @return Whether the UART changed state.
 ******************************************************************************/
static inline bool uart_async_disable_rx(sl_uart_handle_t *uart_handle)
{
  if (uart_handle->async_rx_state == SL_UART_HANDLE_STATE_IDLE) {
    return false;
  }

  uart_handle->async_rx_state = SL_UART_HANDLE_STATE_IDLE;

  return true;
}

/***************************************************************************//**
 * Get a RX transfer.
 ******************************************************************************/
static sli_uart_async_rx_transfer_t *uart_async_get_rx_tfer(sl_uart_handle_t *uart_handle)
{
  return sli_uart_async_rx_transfer_from_node(sl_slist_pop(&uart_handle->async_rx_free_list_head));
}

/***************************************************************************//**
 * Release an RX transfer.
 ******************************************************************************/
static void uart_async_release_rx_tfer(sl_uart_handle_t *uart_handle,
                                       sli_uart_async_rx_transfer_t *tfer)
{
  EFM_ASSERT(tfer->base.node.node == NULL);
  sl_slist_push(&uart_handle->async_rx_free_list_head, &tfer->base.node);
}

/***************************************************************************//**
 * Get a TX transfer.
 ******************************************************************************/
static sli_uart_async_tx_transfer_t *uart_async_get_tx_tfer(sl_uart_handle_t *uart_handle)
{
  return sli_uart_async_tx_transfer_from_node(sl_slist_pop(&uart_handle->async_tx_free_list_head));
}

/***************************************************************************//**
 * Release a TX transfer.
 ******************************************************************************/
static void uart_async_release_tx_tfer(sl_uart_handle_t *uart_handle,
                                       sli_uart_async_tx_transfer_t *tfer)
{
  EFM_ASSERT(tfer->base.node.node == NULL);
  sl_slist_push(&uart_handle->async_tx_free_list_head, &tfer->base.node);
}

/***************************************************************************//**
 * Submit the next DMA chunk for a transfer.
 ******************************************************************************/
static void uart_async_submit_tx_chunk(sl_uart_handle_t *uart_handle,
                                       sli_uart_async_tx_transfer_t *async_tfer)
{
  size_t chunk_size = SL_MIN(async_tfer->base.size - async_tfer->base.bytes_submitted,
                             SL_DMA_CHANNEL_MAX_XFER_UNIT_COUNT);

  sl_status_t status = sl_dma_channel_submit_transfer_m2p(&uart_handle->async_tx_dma_channel,
                                                          async_tfer->base.data + async_tfer->base.bytes_submitted,
                                                          uart_handle->ops->get_tx_register(uart_handle->uart),
                                                          chunk_size,
                                                          SL_DMA_CTRL_SIZE_BYTE,
                                                          &async_tfer->dma_desc);

  // DMA Channel submit shall never fail when the descriptor is pre-allocated and the transfer size
  // has been verified to be at most SL_DMA_CHANNEL_MAX_XFER_UNIT_COUNT.
  if (status != SL_STATUS_OK) {
    EFM_ASSERT(false);
    return;
  }

  async_tfer->base.bytes_submitted += chunk_size;
}

/***************************************************************************//**
 * Submit the next DMA chunk for a transfer.
 *
 * @return Whether the current transfer has more chunks to go.
 ******************************************************************************/
static bool uart_async_submit_rx_chunk(sl_uart_handle_t *uart_handle,
                                       sli_uart_async_rx_transfer_t *async_tfer)
{
  size_t chunk_size = SL_MIN(async_tfer->base.size - async_tfer->base.bytes_submitted,
                             SL_DMA_CHANNEL_MAX_XFER_UNIT_COUNT);
  sl_dma_channel_xfer_descriptor_t *desc = &async_tfer->dma_desc[async_tfer->active_desc_index];

  sl_status_t status = sl_dma_channel_submit_transfer_p2m(&uart_handle->async_rx_dma_channel,
                                                          uart_handle->ops->get_rx_register(uart_handle->uart),
                                                          (uint8_t *)async_tfer->base.data + async_tfer->base.bytes_submitted,
                                                          chunk_size,
                                                          SL_DMA_CTRL_SIZE_BYTE,
                                                          desc);

  // DMA Channel submit should never fail when the descriptor is pre-allocated and the transfer size
  // has been verified to be at most SL_DMA_CHANNEL_MAX_XFER_UNIT_COUNT.
  if (status != SL_STATUS_OK) {
    EFM_ASSERT(false);
    return false;
  }

  async_tfer->base.bytes_submitted += chunk_size;
  async_tfer->callback_pending_cnt++;

  // Advance the active descriptor index to the next chunk.
  async_tfer->active_desc_index = (async_tfer->active_desc_index + 1) % SL_ARRAY_SIZE(async_tfer->dma_desc);

  return async_tfer->base.bytes_submitted < async_tfer->base.size;
}

/**
 * @brief Process pending RX transfers.
 *
 * Async RX pipeline DMA chunks through two ping-pong descriptors per
 * transfer. Transfers larger than SL_DMA_CHANNEL_MAX_XFER_UNIT_COUNT are split
 * into max-sized chunks; descriptors are reused as chunks complete.
 *
 * Queuing must not leave gaps in the DMA chain, otherwise data may be dropped.
 *
 * A transfer may be submitted to the DMA channel only after the preceding
 * transfer's final chunk has been submitted. Until then, the new transfer is
 * added to the pending list but no descriptors are queued in DMA for it.
 *
 * In other words, the last descriptor queued for the preceding transfer must be
 * that transfer's final chunk (bytes_submitted + chunk_size == size). Only then
 * may the next transfer's chunk(s) be chained.
 *
 * Transfers larger than SL_DMA_CHANNEL_MAX_XFER_UNIT_COUNT are split into max-sized chunks;
 * descriptors are reused as chunks complete, preventing data-loss due to gaps in the DMA chain.
 * In other words, the driver ensures that no bytes will be dropped, so long as the DMA interrupt
 * is executed within the time it takes to receive SL_DMA_CHANNEL_MAX_XFER_UNIT_COUNT.
 *
 * At 115200 baud Start+8N1: 2048 * (10/115200) = ~177ms
 * At 921600 baud Start+8N1: 2048 * (10/921600) = ~22ms
 *
 * If the system may encounter interrupt latency larger than the above, it is
 * recommended to use HWFC to prevent data-loss.
 */
static void uart_async_process_pending_rx_transfers(sl_uart_handle_t *uart_handle)
{
  sl_slist_node_t **active_list_head = &uart_handle->async_rx_transfer_active_list_head;
  sl_slist_node_t **pending_list_head = &uart_handle->async_rx_transfer_pending_list_head;
  sl_slist_node_t *it;

  SL_SLIST_FOR_EACH(*active_list_head, it) {
    // Find the last transfer submitted to the DMA
    if (it->node == NULL) {
      break;
    }
  }

  // Check if the active list's tail's final chunk has been submitted.
  sli_uart_async_transfer_t *tail = sli_uart_async_transfer_from_node(it);
  if (tail != NULL && tail->bytes_submitted < tail->size) {
    // The tail's final chunk has not been submitted yet. Add the tfer to the pending list for
    // processing once the tail's final chunk has been submitted, in the DMA callback.
    return;
  }

  sli_uart_async_rx_transfer_t *tfer;
  while ((tfer = sli_uart_async_rx_transfer_from_node(sl_slist_pop(pending_list_head)))) {
    // Transfer is now considered active in the DMA chain.
    sli_uart_transfer_list_push_back(active_list_head, &tfer->base);
    bool has_more_chunks = true;

    // Send the transfer's chunks
    for (size_t i = 0; i < SL_ARRAY_SIZE(tfer->dma_desc) && has_more_chunks; i++) {
      has_more_chunks = uart_async_submit_rx_chunk(uart_handle, tfer);
    }

    if (has_more_chunks) {
      // Transfer could not be submitted in its entirety. Its following chunks will be submitted once
      // the first chunk's DMA callback is called.
      break;
    }
  }
}

/***************************************************************************//**
 * Return the next UART async transfer in the list.
 ******************************************************************************/
static inline sli_uart_async_transfer_t *uart_async_transfer_next(const sli_uart_async_transfer_t *async_tfer)
{
  if (async_tfer == NULL) {
    return NULL;
  }

  return sli_uart_async_transfer_from_node(async_tfer->node.node);
}

/***************************************************************************//**
 * TX DMA channel callback, only used to abort the transfer in the case of a DMA
 * error.
 ******************************************************************************/
static void tx_dma_channel_callback(sl_dma_channel_handle_t * handle,
                                    void *user_data,
                                    bool error,
                                    bool aborted)
{
  sl_uart_handle_t *uart_handle = (sl_uart_handle_t *)user_data;
  (void)handle;
  (void)aborted;

  if (error) {
    // DMA Channel errors should not never occur.
    sl_status_t status = sl_uart_async_abort_tx(uart_handle);
    EFM_ASSERT(status == SL_STATUS_OK);
    return;
  }
}

/***************************************************************************//**
 * Process the abort RX callacks. This function is called after all the
 * DMA callbacks have been processed for the active transfers.
 ******************************************************************************/
static void uart_async_abort_rx_transfers(sl_uart_handle_t *uart_handle)
{
  sl_slist_node_t *aborted_list_head = uart_handle->async_rx_transfer_aborted_list_head;
  sl_slist_node_t *pending_list_head = uart_handle->async_rx_transfer_pending_list_head;

  uart_handle->async_rx_transfer_aborted_list_head = NULL;
  uart_handle->async_rx_transfer_pending_list_head = NULL;

  // Abort all transfers that were queued prior to the DMA abort, including any that were still pending
  // getting processed by the DMA.
  sl_slist_join(&aborted_list_head, &pending_list_head);

  while (!sl_slist_is_empty(aborted_list_head)) {
    sli_uart_async_rx_transfer_t *tfer = sli_uart_async_rx_transfer_from_node(sl_slist_pop(&aborted_list_head));

    // Release the transfer prior to calling the user callback to allow submitting a new transfer
    // from the callback.
    uint8_t *data = tfer->base.data;
    size_t size = tfer->base.bytes_completed;
    void *user_data = uart_handle->async_rx_cb_user_data;

    uart_async_release_rx_tfer(uart_handle, tfer);

    if (uart_handle->async_rx_cb != NULL) {
      uart_handle->async_rx_cb(uart_handle,
                               data,
                               size,
                               user_data,
                               SL_UART_ASYNC_RX_EVENT_ABORTED | SL_UART_ASYNC_RX_EVENT_BUF_RELEASED);
    }
  }
}

/***************************************************************************//**
 * Call the user provided callback & update the transfer list head.
 *
 * The callback is responsible for queueing any following transfer's chunk to
 * the DMA, until all chunks are submitted, after which the transfer is considered complete
 * and the user is notified.
 ******************************************************************************/
static void rx_dma_channel_callback(sl_dma_channel_handle_t * handle,
                                    void *user_data,
                                    bool error,
                                    bool aborted)
{
  sl_uart_async_rx_event_t rx_event = SL_UART_ASYNC_RX_EVENT_BUF_RELEASED;
  sl_uart_handle_t *uart_handle = (sl_uart_handle_t *)user_data;
  SLI_UART_ASSERT_VALID_HANDLE(uart_handle);
  EFM_ASSERT(SLI_UART_HANDLE_IS_ASYNC(uart_handle));

  sl_slist_node_t **active_list_head = &uart_handle->async_rx_transfer_active_list_head;
  (void)handle;
  (void)error;

  // Only peek the list, as the transfer may not have completed yet if it
  // was larger than the DMA channel's max transfer unit count.
  sli_uart_async_rx_transfer_t *tfer = sli_uart_async_rx_transfer_from_node(*active_list_head);
  EFM_ASSERT(tfer != NULL);

  tfer->callback_pending_cnt--;

  if (aborted) {
    // Make sure to only call the user callback when the last descriptor's callback has been
    // processed, otherwise the user callback will be called multiple times for the same transfer.
    if (tfer->callback_pending_cnt != 0) {
      // Wait for the last callback to complete before notifying the user.
      return;
    }

    sl_slist_node_t **aborted_list_head = &uart_handle->async_rx_transfer_aborted_list_head;

    if (sl_slist_is_empty(*aborted_list_head)) {
      // Aborting the first transfer, increment its bytes completed to take into account the partially completed
      // transfer.
      tfer->base.bytes_completed += uart_handle->async_rx_aborted_bytes_completed;
      uart_handle->async_rx_aborted_bytes_completed = 0;
    }

    sl_slist_remove(active_list_head, &tfer->base.node);
    sl_slist_push_back(aborted_list_head, &tfer->base.node);

    // Wait for all the active transfers to be aborted before notifying the user. This allows users
    // to submit new transfers from their callback, enabling error recovery. Otherwise, the transfer
    // submitted by the uses would get aborted before it would even start.
    if (!sl_slist_is_empty(*active_list_head)) {
      return;
    }

    sl_status_t status = sl_uart_async_disable_rx(uart_handle);
    EFM_ASSERT(status == SL_STATUS_OK);

    return;
  }

  // Successful chunk completion, update the number of completed bytes and advance the active descriptor index.
  tfer->base.bytes_completed += SL_MIN(tfer->base.bytes_submitted - tfer->base.bytes_completed,
                                       SL_DMA_CHANNEL_MAX_XFER_UNIT_COUNT);

  // Handle multi-chunk transfers.
  if (tfer->base.bytes_submitted < tfer->base.size) {
    // Transfer's chunks have not yet all been submitted, submit the next chunk to DMA.
    bool has_more_chunks = uart_async_submit_rx_chunk(uart_handle, tfer);

    if (!has_more_chunks) {
      // Final chunk was just submitted to DMA, queue the following transfer in the pending list.
      uart_async_process_pending_rx_transfers(uart_handle);
    }
  }

  if (tfer->base.bytes_completed != tfer->base.size) {
    // Final chunk hasn't yet completed.
    return;
  }

  sl_slist_remove(active_list_head, &tfer->base.node);

  sli_uart_async_rx_transfer_t *next_tfer = sli_uart_async_rx_transfer_from_node(*active_list_head);
  if ((next_tfer != NULL)
      && (uart_async_transfer_next(&next_tfer->base) == NULL)
      && sl_slist_is_empty(uart_handle->async_rx_transfer_pending_list_head)) {
    // Starting the last RX transfer, notify the user to provide more buffers for continuous reception.
    rx_event |= SL_UART_ASYNC_RX_EVENT_BUF_NEEDED;
  }

  if (uart_handle->async_rx_cb != NULL) {
    uart_handle->async_rx_cb(uart_handle,
                             tfer->base.data,
                             tfer->base.bytes_completed,
                             uart_handle->async_rx_cb_user_data,
                             rx_event);
  }

  uart_async_release_rx_tfer(uart_handle, tfer);

  if (sl_slist_is_empty(*active_list_head)) {
    (void)uart_async_disable_rx(uart_handle);
  }
}

/***************************************************************************//**
 * Deinitialize the DMA channels for the given UART instance.
 ******************************************************************************/
static sl_status_t uart_async_deinit_dma(sl_uart_handle_t *uart_handle)
{
  // DMA deinit clears the channel number, save it so we can re-init the channel
  // without having the store the channel number individually.
  uint8_t tx_channel = uart_handle->async_tx_dma_channel.channel_number;
  uint8_t rx_channel = uart_handle->async_rx_dma_channel.channel_number;

  sl_status_t status = sl_dma_channel_deinit(&uart_handle->async_tx_dma_channel);
  if (status != SL_STATUS_OK) {
    return status;
  }

  uart_handle->async_tx_dma_channel.channel_number = tx_channel;

  status = sl_dma_channel_deinit(&uart_handle->async_rx_dma_channel);
  if (status != SL_STATUS_OK) {
    return status;
  }

  uart_handle->async_rx_dma_channel.channel_number = rx_channel;

  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Initialize the DMA channels for the given UART instance.
 ******************************************************************************/
static sl_status_t uart_async_init_hw(sl_uart_handle_t *uart_handle)
{
  uint8_t rx_channel = uart_handle->async_rx_dma_channel.channel_number;
  uint8_t tx_channel = uart_handle->async_tx_dma_channel.channel_number;
  sl_peripheral_t uart = uart_handle->uart;
  sl_status_t status, _status;

  status = sl_dma_channel_init(&uart_handle->async_rx_dma_channel,
                               NULL,
                               rx_channel,
                               rx_dma_channel_callback,
                               uart_handle);
  if (status != SL_STATUS_OK) {
    return status;
  }

  status = sl_dma_channel_init(&uart_handle->async_tx_dma_channel,
                               NULL,
                               tx_channel,
                               tx_dma_channel_callback,
                               uart_handle);
  if (status != SL_STATUS_OK) {
    goto deinit;
  }

  sl_dma_signal_t rx_signal = sl_device_peripheral_get_serial_dma_signal_rx_trigger(uart);
  status = sl_dma_channel_set_peripheral_signal(&uart_handle->async_rx_dma_channel, rx_signal);
  if (status != SL_STATUS_OK) {
    goto deinit;
  }

  sl_dma_signal_t tx_signal = sl_device_peripheral_get_serial_dma_signal_tx_trigger(uart);
  status = sl_dma_channel_set_peripheral_signal(&uart_handle->async_tx_dma_channel, tx_signal);
  if (status != SL_STATUS_OK) {
    goto deinit;
  }

  // Enable TXC callback, as it is used to notify users the transfer has completed.
  // Clear any latent TXC interrupt that could lead to dereferencing stale lists.
  uart_handle->ops->clear_irq(uart_handle->uart, uart_handle->ops->irq_tx_complete_flag);
  sli_uart_enable_irq(uart_handle, uart_handle->ops->irq_tx_complete_flag);

  return status;

  deinit:
  // uart_async_deinit_dma restores channel numbers after deinit zeroes the handles,
  // so a failed resume (handle stays SUSPENDED) can still free the right channels.
  _status = uart_async_deinit_dma(uart_handle);
  EFM_ASSERT(_status == SL_STATUS_OK);

  return status;
}

/*******************************************************************************
**************************   GLOBAL FUNCTIONS   *******************************
*******************************************************************************/

/***************************************************************************//**
 * Registers a TX complete callback.
 ******************************************************************************/
void sl_uart_async_set_tx_complete_callback(sl_uart_handle_t *uart_handle,
                                            sl_uart_async_on_tx_complete_cb_t on_tx_complete,
                                            void *user_data)
{
  SLI_UART_ASSERT_VALID_HANDLE(uart_handle);
  EFM_ASSERT(SLI_UART_HANDLE_IS_ASYNC(uart_handle));
  EFM_ASSERT(on_tx_complete != NULL);

  CORE_DECLARE_IRQ_STATE;
  CORE_ENTER_ATOMIC();
  uart_handle->async_tx_complete_cb = on_tx_complete;
  uart_handle->async_tx_complete_cb_user_data = user_data;
  CORE_EXIT_ATOMIC();
}

/***************************************************************************//**
 * Registers a RX event callback.
 ******************************************************************************/
void sl_uart_async_set_rx_event_callback(sl_uart_handle_t *uart_handle,
                                         sl_uart_async_on_rx_cb_t on_rx_event,
                                         void *user_data)
{
  SLI_UART_ASSERT_VALID_HANDLE(uart_handle);
  EFM_ASSERT(SLI_UART_HANDLE_IS_ASYNC(uart_handle));
  EFM_ASSERT(on_rx_event != NULL);

  CORE_DECLARE_IRQ_STATE;
  CORE_ENTER_ATOMIC();
  uart_handle->async_rx_cb = on_rx_event;
  uart_handle->async_rx_cb_user_data = user_data;
  CORE_EXIT_ATOMIC();
}

/***************************************************************************//**
 * Start a TX DMA transfer on the specified UART handle.
 ******************************************************************************/
sl_status_t sl_uart_async_write(sl_uart_handle_t *uart_handle,
                                const void *data,
                                const size_t size)
{
  SLI_UART_ASSERT_VALID_HANDLE(uart_handle);
  EFM_ASSERT(SLI_UART_HANDLE_IS_ASYNC(uart_handle));

  sl_status_t status = SL_STATUS_OK;

  CORE_DECLARE_IRQ_STATE;

  if (size == 0) {
    return SL_STATUS_EMPTY;
  }

  if (SLI_UART_HANDLE_IS_SUSPENDED(uart_handle)) {
    return SL_STATUS_INVALID_STATE;
  }

  EFM_ASSERT(data != NULL);

  CORE_ENTER_ATOMIC();

  sli_uart_async_tx_transfer_t *tfer = uart_async_get_tx_tfer(uart_handle);

  if (tfer == NULL) {
    status = SL_STATUS_BUSY;
    goto exit;
  }

  memset(tfer, 0, sizeof(*tfer));
  tfer->base.data = (void*)data;
  tfer->base.size = size;

  sl_slist_push_back(&uart_handle->async_tx_transfer_submitted_list_head, &tfer->base.node);

  // In TX, we can only queue the following transfer once the previous once has been fully sent
  // over the bus, as indicated by the UART peripheral's TX complete interrupt. In other words,
  // there can only be on transfer in the active list at a time.
  if (uart_async_enable_tx(uart_handle)) {
    // Send the transfer's chunk until either all chunks are submitted, or we run out of descriptors.
    uart_async_submit_tx_chunk(uart_handle, tfer);
  } else {
    // The DMA is already running. The transfer will be submitted to DMA when the TX complete
    // interrupt of the previous transfer triggers.
  }

  exit:
  CORE_EXIT_ATOMIC();
  return status;
}

/***************************************************************************//**
 * Aborts TX all the pending TX transfers.
 ******************************************************************************/
sl_status_t sl_uart_async_abort_tx(sl_uart_handle_t *uart_handle)
{
  SLI_UART_ASSERT_VALID_HANDLE(uart_handle);
  EFM_ASSERT(SLI_UART_HANDLE_IS_ASYNC(uart_handle));

  CORE_DECLARE_IRQ_STATE;
  CORE_ENTER_ATOMIC();

  if (!uart_async_disable_tx(uart_handle)) {
    // Nothing to abort.
    CORE_EXIT_ATOMIC();
    return SL_STATUS_OK;
  }

  /**
   * The DMA channel driver does not provide users with the number of bytes in the transfer callback.
   * However, the number of transferred bytes can be extracted by:
   *   1. Pausing the UART peripheral's operation.
   *   2. Pausing the DMA channel.
   *   2. Pausing the UART peripheral's operation.
   *   3. Checking how many bytes are in the UART peripheral's FIFO.
   *   3. Checking the DMA status, giving us the number of transferred bytes to the FIFO.
   *   4. Updating the number of transferred bytes in the transfer list head to the number of transferred bytes.
   *   5. Aborting the DMA transfer, which will call the user callback for all transfers.
   *   6. Resuming the UART peripheral & DMA for further operation.
   */

  sl_peripheral_t uart = uart_handle->uart;
  uart_handle->ops->set_tx_enable(uart, false);

  // Clear any latent TXC interrupt that could lead to dereferencing stale lists.
  uart_handle->ops->clear_irq(uart, uart_handle->ops->irq_tx_complete_flag);

  sl_dma_channel_handle_t *dma_channel = &uart_handle->async_tx_dma_channel;
  sl_status_t status = sl_dma_channel_suspend(dma_channel);
  EFM_ASSERT(status == SL_STATUS_OK);
  __DMB();

  // Get a snapshot of the transfer list prior to aborting the DMA, so that transfers submitted
  // in the abort callback are not lost.
  sl_slist_node_t *list_head = uart_handle->async_tx_transfer_submitted_list_head;
  uart_handle->async_tx_transfer_submitted_list_head = NULL;

  if (sl_slist_is_empty(list_head)) {
    // Nothing to abort.
    goto resume;
  }

  size_t tx_fifo_bytes = uart_handle->ops->clear_tx_fifo(uart);

  // Clear any latent TXC interrupt that could lead to dereferencing stale lists.
  uart_handle->ops->clear_irq(uart, uart_handle->ops->irq_tx_complete_flag);

  sl_dma_channel_status_t dma_status;
  status = sl_dma_channel_get_status(dma_channel, &dma_status);
  EFM_ASSERT(status == SL_STATUS_OK);

  // Bytes on the wire = DMA progress into the TX FIFO minus bytes still in the FIFO
  // (completed_bytes = dma_bytes_completed - tx_fifo_bytes).

  // Only peek the list, as the callbacks are responsible for removing the transfer from the
  // list and freeing it.
  sli_uart_async_tx_transfer_t *active_tfer = sli_uart_async_tx_transfer_from_node(list_head);

  if (sl_dma_manager_get_pending_errors(dma_channel->channel_number)) {
    // DMA channel has encountered an error. It's impossible to reliably know how many bytes were
    // successfully transferred from the previous chunk, assume no bytes were transferred.
  } else if (dma_status.enabled) {
    // When the DMA transfer is done but the FIFO has not drained, status reports zero
    // bytes completed. Use the active transfer size instead.
    EFM_ASSERT(dma_status.bytes_completed >= tx_fifo_bytes);
    active_tfer->base.bytes_completed += dma_status.bytes_completed - tx_fifo_bytes;
  } else {
    // DMA has already completed. Since there is only ever one active transfer at a time in TX,
    // the number of completed bytes corresponds to the number of bytes submitted, minus any
    // bytes still in the FIFO.
    active_tfer->base.bytes_completed = active_tfer->base.bytes_submitted - tx_fifo_bytes;
  }

  // Abort any pending transfers and reset the DMA to its initial state.
  status = sl_dma_channel_abort(dma_channel);
  EFM_ASSERT(status == SL_STATUS_OK);

  // Abort all remaining transfers in the list.
  while ((active_tfer = sli_uart_async_tx_transfer_from_node(sl_slist_pop(&list_head)))) {
    // Free the transfer before invoking the callback so it can be reused if the user submits
    // a new transfer from its
    uint8_t *data = active_tfer->base.data;
    size_t size = active_tfer->base.bytes_completed;
    void *user_data = uart_handle->async_tx_complete_cb_user_data;

    uart_async_release_tx_tfer(uart_handle, active_tfer);

    if (uart_handle->async_tx_complete_cb != NULL) {
      uart_handle->async_tx_complete_cb(uart_handle,
                                        data,
                                        size,
                                        user_data,
                                        true);
    }
  }

  resume:
  uart_handle->ops->set_tx_enable(uart, true);
  sl_dma_channel_resume(dma_channel);

  CORE_EXIT_ATOMIC();

  return status;
}

/***************************************************************************//**
 * Retrieves TX status.
 ******************************************************************************/
bool sl_uart_async_is_tx_active(sl_uart_handle_t *uart_handle)
{
  SLI_UART_ASSERT_VALID_HANDLE(uart_handle);
  EFM_ASSERT(SLI_UART_HANDLE_IS_ASYNC(uart_handle));

  return uart_handle->async_tx_state == SL_UART_HANDLE_STATE_ACTIVE;
}

/***************************************************************************//**
 * Start a RX DMA transfer on the specified UART handle.
 ******************************************************************************/
sl_status_t sl_uart_async_read(sl_uart_handle_t *uart_handle,
                               void *data,
                               const size_t size)
{
  SLI_UART_ASSERT_VALID_HANDLE(uart_handle);
  EFM_ASSERT(SLI_UART_HANDLE_IS_ASYNC(uart_handle));

  if (SLI_UART_HANDLE_IS_SUSPENDED(uart_handle)) {
    return SL_STATUS_INVALID_STATE;
  }

  sl_status_t status = SL_STATUS_OK;

  if (size == 0) {
    return SL_STATUS_EMPTY;
  }
  EFM_ASSERT(data != NULL);

  CORE_DECLARE_IRQ_STATE;

  CORE_ENTER_ATOMIC();
  sli_uart_async_rx_transfer_t *tfer = uart_async_get_rx_tfer(uart_handle);

  if (tfer == NULL) {
    status = SL_STATUS_BUSY;
    goto exit;
  }

  memset(tfer, 0, sizeof(*tfer));
  tfer->base.data = data;
  tfer->base.size = size;

  sl_slist_push_back(&uart_handle->async_rx_transfer_pending_list_head, &tfer->base.node);

  // Notify the user to provide more buffers for continuous reception.
  if (uart_async_enable_rx(uart_handle)) {
    if (uart_handle->async_rx_cb != NULL) {
      uart_handle->async_rx_cb(uart_handle,
                               NULL,
                               0,
                               uart_handle->async_rx_cb_user_data,
                               SL_UART_ASYNC_RX_EVENT_BUF_NEEDED);
    }
  }

  // Attempt to submit the transfer's chunks to DMA.
  uart_async_process_pending_rx_transfers(uart_handle);

  exit:
  CORE_EXIT_ATOMIC();
  return status;
}

/***************************************************************************//**
 * Aborts all the pending RX transfers.
 ******************************************************************************/
sl_status_t sl_uart_async_disable_rx(sl_uart_handle_t *uart_handle)
{
  SLI_UART_ASSERT_VALID_HANDLE(uart_handle);
  EFM_ASSERT(SLI_UART_HANDLE_IS_ASYNC(uart_handle));

  CORE_DECLARE_IRQ_STATE;

  sl_dma_channel_handle_t *dma_channel = &uart_handle->async_rx_dma_channel;
  sl_dma_channel_status_t dma_status;
  sl_status_t status;

  CORE_ENTER_ATOMIC();

  if (!uart_async_disable_rx(uart_handle)) {
    // Nothing to abort.
    CORE_EXIT_ATOMIC();
    return SL_STATUS_OK;
  }

  /**
   * The DMA channel driver does not provide users with the number of bytes in the transfer callback.
   * However, the number of transferred bytes can be extracted by:
   *   1. Pausing the DMA channel.
   *   2. Checking the DMA status, giving us the number of transferred bytes to the FIFO.
   *   3. Aborting the DMA transfer, which will call the user callback for all transfers.
   *   4. Resuming the DMA for further operation.
   */

  status = sl_dma_channel_suspend(dma_channel);
  EFM_ASSERT(status == SL_STATUS_OK);
  __DMB();

  status = sl_dma_channel_get_status(dma_channel, &dma_status);
  EFM_ASSERT(status == SL_STATUS_OK);

  if (dma_status.enabled) {
    // When the DMA is aborted, the number of bytes completed is lost in the partial transfer are lost.
    // Store the number here, and add it to the bytes completed of the first transfer that gets
    // aborted.
    uart_handle->async_rx_aborted_bytes_completed = dma_status.bytes_completed;
  }

  // Abort all the active transfers that were submitted to the DMA. This will move all the active
  // transfers to the aborted list, and reset the DMA to its initial state, readying it for new transfers.
  status = sl_dma_channel_abort(dma_channel);
  EFM_ASSERT(status == SL_STATUS_OK);

  uart_async_abort_rx_transfers(uart_handle);

  sl_dma_channel_resume(dma_channel);

  CORE_EXIT_ATOMIC();

  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Retrieves RX status.
 ******************************************************************************/
bool sl_uart_async_is_rx_active(sl_uart_handle_t *uart_handle)
{
  SLI_UART_ASSERT_VALID_HANDLE(uart_handle);
  EFM_ASSERT(SLI_UART_HANDLE_IS_ASYNC(uart_handle));

  return uart_handle->async_rx_state == SL_UART_HANDLE_STATE_ACTIVE;
}

/*******************************************************************************
 **************************   INTERNAL FUNCTIONS   *****************************
 ******************************************************************************/

/***************************************************************************//**
 * Initializes the DMA channels for the given UART instance.
 *
 * @param[in]  uart_handle Handle to UART.
 ******************************************************************************/
sl_status_t sli_uart_async_init(sl_uart_handle_t *uart_handle)
{
  SLI_UART_ASSERT_VALID_HANDLE(uart_handle);
  EFM_ASSERT(SLI_UART_HANDLE_IS_ASYNC(uart_handle));
  uint8_t rx_channel = uart_handle->preinit_config.async_rx_dma_channel_number;
  uint8_t tx_channel = uart_handle->preinit_config.async_tx_dma_channel_number;
  sl_status_t status;

  status = sli_uart_transfer_pool_init(uart_handle);
  if (status != SL_STATUS_OK) {
    return status;
  }

  if (uart_handle->preinit_config.async_rx_dma_channel_number == SL_UART_ASYNC_DMA_CHANNEL_CONFIG_AUTO) {
    status = sl_dma_manager_allocate_channel(NULL, &rx_channel);
    if (status != SL_STATUS_OK) {
      goto transfer_pool_deinit;
    }
  }

  if (uart_handle->preinit_config.async_tx_dma_channel_number == SL_UART_ASYNC_DMA_CHANNEL_CONFIG_AUTO) {
    status = sl_dma_manager_allocate_channel(NULL, &tx_channel);
    if (status != SL_STATUS_OK) {
      goto free_rx_channel;
    }
  }

  uart_handle->async_rx_dma_channel.channel_number = rx_channel;
  uart_handle->async_tx_dma_channel.channel_number = tx_channel;

  uart_handle->async_tx_state = SL_UART_HANDLE_STATE_IDLE;
  uart_handle->async_rx_state = SL_UART_HANDLE_STATE_IDLE;

  return status;

  free_rx_channel:
  if (uart_handle->preinit_config.async_rx_dma_channel_number == SL_UART_ASYNC_DMA_CHANNEL_CONFIG_AUTO) {
    // Only free the channel if it was allocated as part of this function call.
    sl_dma_manager_free_channel(NULL, rx_channel);
  }
  transfer_pool_deinit:
  sli_uart_transfer_pool_deinit(uart_handle);

  return status;
}

/***************************************************************************//**
 * Frees the DMA channel and de-initializes the DMA for the given UART instance.
 *
 * @param[in]  uart_handle Handle to UART.
 ******************************************************************************/
sl_status_t sli_uart_async_deinit(sl_uart_handle_t *uart_handle)
{
  SLI_UART_ASSERT_VALID_HANDLE(uart_handle);
  EFM_ASSERT(SLI_UART_HANDLE_IS_ASYNC(uart_handle));
  sl_status_t status;

  status = sl_uart_async_abort_tx(uart_handle);
  if (status != SL_STATUS_OK) {
    return status;
  }

  status = sl_uart_async_disable_rx(uart_handle);
  if (status != SL_STATUS_OK) {
    return status;
  }

  // DMA Channel Deinit zeroes out the handle structure, save the channel number in order to free them.
  uint8_t rx_channel = uart_handle->async_rx_dma_channel.channel_number;
  uint8_t tx_channel = uart_handle->async_tx_dma_channel.channel_number;

  if (!SLI_UART_HANDLE_IS_SUSPENDED(uart_handle)) {
    sli_uart_disable_irq(uart_handle, uart_handle->ops->irq_tx_complete_flag);

    status = uart_async_deinit_dma(uart_handle);
    if (status != SL_STATUS_OK) {
      return status;
    }
  }

  if (uart_handle->preinit_config.async_tx_dma_channel_number == SL_UART_ASYNC_DMA_CHANNEL_CONFIG_AUTO) {
    status = sl_dma_manager_free_channel(NULL, tx_channel);
    if (status != SL_STATUS_OK) {
      return status;
    }
  }

  if (uart_handle->preinit_config.async_rx_dma_channel_number == SL_UART_ASYNC_DMA_CHANNEL_CONFIG_AUTO) {
    status = sl_dma_manager_free_channel(NULL, rx_channel);
    if (status != SL_STATUS_OK) {
      return status;
    }
  }

  sli_uart_transfer_pool_deinit(uart_handle);

  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Suspends the UART peripheral async hardware (i.e.: DMA channel).
 ******************************************************************************/
sl_status_t sli_uart_async_suspend(sl_uart_handle_t *uart_handle)
{
  SLI_UART_ASSERT_VALID_HANDLE(uart_handle);
  EFM_ASSERT(SLI_UART_HANDLE_IS_ASYNC(uart_handle));

  if (sl_uart_async_is_tx_active(uart_handle)
      || sl_uart_async_is_rx_active(uart_handle)) {
    return SL_STATUS_BUSY;
  }

  return uart_async_deinit_dma(uart_handle);
}

/***************************************************************************//**
 * Resumes the UART peripheral hardware.
 ******************************************************************************/
sl_status_t sli_uart_async_resume(sl_uart_handle_t *uart_handle)
{
  SLI_UART_ASSERT_VALID_HANDLE(uart_handle);
  EFM_ASSERT(SLI_UART_HANDLE_IS_ASYNC(uart_handle));

  return uart_async_init_hw(uart_handle);
}

/***************************************************************************//**
 * Handles the UART peripheral TX complete interrupt for the given UART instance
 * and queues the following transfer.
 *
 * @param[in]  uart_handle Handle to UART.
 *
 * @note This function is called from an ISR context.
 *
 * @note User callback is invoked from the UART peripheral callback rather
 *       than the DMA callback, otherwise it would be called while data was still
 *       being transferred over the bus.
 ******************************************************************************/
void sli_uart_async_transmit_complete(sl_uart_handle_t *uart_handle)
{
  SLI_UART_ASSERT_VALID_HANDLE(uart_handle);
  EFM_ASSERT(SLI_UART_HANDLE_IS_ASYNC(uart_handle));

  sl_slist_node_t **list_head = &uart_handle->async_tx_transfer_submitted_list_head;
  sli_uart_async_tx_transfer_t *tfer = sli_uart_async_tx_transfer_from_node(*list_head);
  EFM_ASSERT(tfer != NULL);

  tfer->base.bytes_completed = tfer->base.bytes_submitted;
  if (tfer->base.bytes_completed != tfer->base.size) {
    // Transfer still has more chunks to go. Submit the next chunk to DMA and wait for its completion.
    uart_async_submit_tx_chunk(uart_handle, tfer);
    return;
  }

  // Successfully sent all of the transfer's chunks. Notify the user, and queue the next transfer.
  sl_slist_remove(list_head, &tfer->base.node);

  if (uart_handle->async_tx_complete_cb != NULL) {
    uart_handle->async_tx_complete_cb(uart_handle,
                                      tfer->base.data,
                                      tfer->base.size,
                                      uart_handle->async_tx_complete_cb_user_data,
                                      false);
  }

  uart_async_release_tx_tfer(uart_handle, tfer);

  tfer = sli_uart_async_tx_transfer_from_node(*list_head);
  if (tfer == NULL) {
    // No more transfers to send.
    (void)uart_async_disable_tx(uart_handle);
    return;
  }

  // Send the next transfer's first chunk until to DMA.
  uart_async_submit_tx_chunk(uart_handle, tfer);
}
