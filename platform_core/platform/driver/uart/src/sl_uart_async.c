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
#include "sli_uart_transfer_pool.h"

/*******************************************************************************
 *******************************   DEFINES   ***********************************
 ******************************************************************************/

/*******************************************************************************
 ***************************   LOCAL FUNCTIONS   *******************************
 ******************************************************************************/

/***************************************************************************//**
 * Submit a TX DMA transfer.
 ******************************************************************************/
static sl_status_t uart_async_submit_tx_dma(sl_uart_handle_t *uart_handle,
                                            sli_uart_async_transfer_t *async_tfer)
{
  sl_dma_channel_transfer_t dma_tfer = {
    .source = async_tfer->data,
    .destination = uart_handle->ops->get_tx_register(uart_handle->uart),
    .size = async_tfer->size,
    .unit_size = SL_DMA_CTRL_SIZE_BYTE,
    .block_size = SL_DMA_CTRL_BLOCK_SIZE_UNIT_1,
    .increment_source = true,
    .increment_destination = false,
    .block_handshake_mode = true,
    .callback_on_complete = true,
    .descriptor = &async_tfer->dma_desc,
  };

  return sl_dma_channel_submit_transfer_list(&uart_handle->async_tx_dma_channel,
                                             &dma_tfer);
}

/***************************************************************************//**
 * Submit an RX DMA transfer.
 ******************************************************************************/
static sl_status_t uart_async_submit_rx_dma(sl_uart_handle_t *uart_handle,
                                            sli_uart_async_transfer_t *async_tfer)
{
  sl_dma_channel_transfer_t dma_tfer = {
    .source = uart_handle->ops->get_rx_register(uart_handle->uart),
    .destination = async_tfer->data,
    .size = async_tfer->size,
    .unit_size = SL_DMA_CTRL_SIZE_BYTE,
    .block_size = SL_DMA_CTRL_BLOCK_SIZE_UNIT_1,
    .increment_source = false,
    .increment_destination = true,
    .block_handshake_mode = true,
    .callback_on_complete = true,
    .descriptor = &async_tfer->dma_desc,
  };

  return sl_dma_channel_submit_transfer_list(&uart_handle->async_rx_dma_channel,
                                             &dma_tfer);
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
 * Call the user provided callback & update the transfer list head.
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

  sl_slist_node_t **list_head = &uart_handle->async_rx_submitted_list_head;
  size_t size;
  (void)handle;

  sli_uart_async_transfer_t *tfer = sli_uart_async_transfer_from_node(sl_slist_pop(list_head));
  EFM_ASSERT(tfer != NULL);

  size = tfer->size;

  if (error || aborted) {
    size = tfer->bytes_completed;
    rx_event |= SL_UART_ASYNC_RX_EVENT_ABORTED;
  }

  sli_uart_async_transfer_t *next_tfer = sli_uart_async_transfer_from_node(*list_head);
  if (!(rx_event & SL_UART_ASYNC_RX_EVENT_ABORTED)
      && (next_tfer != NULL)
      && (uart_async_transfer_next(next_tfer) == NULL)) {
    // Starting the last RX transfer, notify the user to provide more buffers for continuous reception.
    rx_event |= SL_UART_ASYNC_RX_EVENT_BUF_NEEDED;
  }

  if (uart_handle->async_rx_cb != NULL) {
    uart_handle->async_rx_cb(uart_handle,
                             tfer->data,
                             size,
                             uart_handle->async_rx_cb_user_data,
                             rx_event);
  }

  sli_uart_transfer_list_push(&uart_handle->async_rx_pool, tfer);
}

/***************************************************************************//**
 * Call the user provided callback & update the transfer list head.
 *
 * @note Callback may be called from thread mode when the DMA is aborted.
 ******************************************************************************/
static void tx_dma_channel_callback(sl_dma_channel_handle_t * handle,
                                    void *user_data,
                                    bool error,
                                    bool aborted)
{
  sl_uart_handle_t *uart_handle = (sl_uart_handle_t *)user_data;
  SLI_UART_ASSERT_VALID_HANDLE(uart_handle);
  EFM_ASSERT(SLI_UART_HANDLE_IS_ASYNC(uart_handle));

  sl_slist_node_t **list_head = &uart_handle->async_tx_submitted_list_head;

  (void)handle;

  sli_uart_async_transfer_t *tfer = sli_uart_async_transfer_from_node(*list_head);
  EFM_ASSERT(tfer != NULL);

  if (!error && !aborted) {
    // DMA filled the TX FIFO. The user callback is invoked from the UART TX complete interrupt.
    return;
  }

  // Handle DMA error and abortion.
  // Remove the transfer from the list and invoke the user callback.
  sl_slist_remove(list_head, &tfer->node);

  if (uart_handle->async_tx_complete_cb != NULL) {
    uart_handle->async_tx_complete_cb(uart_handle,
                                      tfer->data,
                                      tfer->bytes_completed,
                                      uart_handle->async_tx_complete_cb_user_data,
                                      true);
  }

  sli_uart_transfer_list_push(&uart_handle->async_tx_pool, tfer);
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

  EFM_ASSERT(data != NULL);

  sli_uart_async_transfer_t *async_tfer;

  CORE_ENTER_ATOMIC();

  async_tfer = sli_uart_transfer_list_pop(&uart_handle->async_tx_pool);

  if (async_tfer == NULL) {
    status = SL_STATUS_BUSY;
    goto exit;
  }

  memset(async_tfer, 0, sizeof(*async_tfer));
  async_tfer->data = (void*)data;
  async_tfer->size = size;

  bool start_dma = sl_slist_is_empty(uart_handle->async_tx_submitted_list_head);

  sl_slist_push_back(&uart_handle->async_tx_submitted_list_head, &async_tfer->node);

  // Only submit the DMA transfer if this is the first transfer in the list, otherwise queue
  // the transfer and schedule it for transmission once the previous one has been transmitted
  // entirely over the bus.
  if (start_dma) {
    status = uart_async_submit_tx_dma(uart_handle, async_tfer);

    if (status != SL_STATUS_OK) {
      sl_slist_remove(&uart_handle->async_tx_submitted_list_head, &async_tfer->node);
      sli_uart_transfer_list_push(&uart_handle->async_tx_pool, async_tfer);
      goto exit;
    }
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

  sl_slist_node_t **list_head = &uart_handle->async_tx_submitted_list_head;
  sl_dma_channel_status_t dma_status;
  sl_status_t status = SL_STATUS_OK;

  if (sl_slist_is_empty(*list_head)) {
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

  status = sl_dma_channel_suspend(&uart_handle->async_tx_dma_channel);
  EFM_ASSERT(status == SL_STATUS_OK);
  __DMB();

  size_t tx_fifo_bytes = uart_handle->ops->clear_tx_fifo(uart);

  status = sl_dma_channel_get_status(&uart_handle->async_tx_dma_channel, &dma_status);
  EFM_ASSERT(status == SL_STATUS_OK);

  // Bytes on the wire = DMA progress into the TX FIFO minus bytes still in the FIFO
  // (completed_bytes = dma_bytes_completed - tx_fifo_bytes).

  // Only peek the list, as the callbacks are responsible for removing the transfer from the
  // list and freeing it.
  sli_uart_async_transfer_t *curr_tfer = sli_uart_async_transfer_from_node(*list_head);

  if (dma_status.enabled) {
    // When the DMA transfer is done but the FIFO has not drained, status reports zero
    // bytes completed. Use the active transfer size instead.
    EFM_ASSERT(dma_status.bytes_completed >= tx_fifo_bytes);
    curr_tfer->bytes_completed += dma_status.bytes_completed - tx_fifo_bytes;

    // Calls the user callback for the active transfer.
    status = sl_dma_channel_abort(&uart_handle->async_tx_dma_channel);
    if (status != SL_STATUS_OK) {
      goto resume;
    }
  } else {
    // DMA has already completed. The active transfer was fully written to the TX FIFO.
    EFM_ASSERT(curr_tfer->size >= tx_fifo_bytes);
    curr_tfer->bytes_completed = curr_tfer->size - tx_fifo_bytes;
  }

  // Abort all remaining transfers in the list.
  while ((curr_tfer = sli_uart_async_transfer_from_node(sl_slist_pop(list_head)))) {
    if (uart_handle->async_tx_complete_cb != NULL) {
      uart_handle->async_tx_complete_cb(uart_handle,
                                        curr_tfer->data,
                                        curr_tfer->bytes_completed,
                                        uart_handle->async_tx_complete_cb_user_data,
                                        true);
    }

    sli_uart_transfer_list_push(&uart_handle->async_tx_pool, curr_tfer);
  }

  EFM_ASSERT(sl_slist_is_empty(uart_handle->async_tx_submitted_list_head));

  resume:
  uart_handle->ops->set_tx_enable(uart, true);
  sl_dma_channel_resume(&uart_handle->async_tx_dma_channel);

  return status;
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

  sl_status_t status;

  if (size == 0) {
    return SL_STATUS_EMPTY;
  }
  EFM_ASSERT(data != NULL);

  sli_uart_async_transfer_t *async_tfer;

  CORE_DECLARE_IRQ_STATE;

  CORE_ENTER_ATOMIC();
  async_tfer = sli_uart_transfer_list_pop(&uart_handle->async_rx_pool);

  if (async_tfer == NULL) {
    status = SL_STATUS_BUSY;
    goto exit;
  }

  memset(async_tfer, 0, sizeof(*async_tfer));
  async_tfer->data = data;
  async_tfer->size = size;

  bool buf_needed = sl_slist_is_empty(uart_handle->async_rx_submitted_list_head);
  sl_slist_push_back(&uart_handle->async_rx_submitted_list_head, &async_tfer->node);

  status = uart_async_submit_rx_dma(uart_handle, async_tfer);
  if (status != SL_STATUS_OK) {
    sl_slist_remove(&uart_handle->async_rx_submitted_list_head, &async_tfer->node);
    sli_uart_transfer_list_push(&uart_handle->async_rx_pool, async_tfer);
    goto exit;
  }

  // Notify the user to provide more buffers for continuous reception.
  if (buf_needed && uart_handle->async_rx_cb != NULL) {
    uart_handle->async_rx_cb(uart_handle,
                             NULL,
                             0,
                             uart_handle->async_rx_cb_user_data,
                             SL_UART_ASYNC_RX_EVENT_BUF_NEEDED);
  }

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

  sl_dma_channel_handle_t *dma_channel = &uart_handle->async_rx_dma_channel;
  sl_dma_channel_status_t dma_status;
  sl_status_t status;

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

  sli_uart_async_transfer_t *curr_tfer = sli_uart_async_transfer_from_node(uart_handle->async_rx_submitted_list_head);

  if (!dma_status.enabled) {
    EFM_ASSERT(curr_tfer == NULL);
    goto resume;
  }

  EFM_ASSERT(curr_tfer != NULL);
  curr_tfer->bytes_completed += dma_status.bytes_completed;

  // Abort the current DMA transfer. This will call process the active transfer and move the list
  // head to the next transfer. The following transfers are aborted below.
  status = sl_dma_channel_abort(dma_channel);
  EFM_ASSERT(status == SL_STATUS_OK);

  resume:
  sl_dma_channel_resume(dma_channel);

  // Abort callback handles the active transfer. Release any queued transfers.
  sl_slist_node_t **list_head = &uart_handle->async_rx_submitted_list_head;

  while ((curr_tfer = sli_uart_async_transfer_from_node(sl_slist_pop(list_head))) != NULL) {
    if (uart_handle->async_rx_cb != NULL) {
      uart_handle->async_rx_cb(uart_handle,
                               curr_tfer->data,
                               0,
                               uart_handle->async_rx_cb_user_data,
                               SL_UART_ASYNC_RX_EVENT_BUF_RELEASED
                               | SL_UART_ASYNC_RX_EVENT_ABORTED);
    }

    sli_uart_transfer_list_push(&uart_handle->async_rx_pool, curr_tfer);
  }

  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Update size of the current read.
 ******************************************************************************/
sl_status_t sl_uart_async_update_current_read_size(sl_uart_handle_t *uart_handle,
                                                   const size_t new_size)
{
  SLI_UART_ASSERT_VALID_HANDLE(uart_handle);
  EFM_ASSERT(SLI_UART_HANDLE_IS_ASYNC(uart_handle));

  sl_dma_channel_handle_t *dma_channel = &uart_handle->async_rx_dma_channel;
  sl_status_t status;
  sl_status_t ret;

  if (sl_slist_is_empty(uart_handle->async_rx_submitted_list_head)) {
    return SL_STATUS_INVALID_STATE;
  }

  status = sl_dma_channel_suspend(dma_channel);
  EFM_ASSERT(status == SL_STATUS_OK);
  __DMB();

  sli_uart_async_transfer_t *active_tfer = sli_uart_async_transfer_from_node(uart_handle->async_rx_submitted_list_head);

  if (new_size < active_tfer->bytes_completed) {
    status = sl_dma_channel_resume(dma_channel);
    EFM_ASSERT(status == SL_STATUS_OK);
    return SL_STATUS_INVALID_PARAMETER;
  }

  // sl_dma_channel_update_active_transfer may call the DMA channel callback immediately on success
  // if the update completes the transfer. Update the size now, and restore it if the update fails.
  size_t original_size = active_tfer->size;
  active_tfer->size = new_size;

  size_t tfer_size = new_size - active_tfer->bytes_completed;

  ret = sl_dma_channel_update_active_transfer(dma_channel, tfer_size);
  if (ret != SL_STATUS_OK) {
    active_tfer->size = original_size;
  }

  status = sl_dma_channel_resume(dma_channel);
  EFM_ASSERT(status == SL_STATUS_OK);

  return ret;
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
  sl_peripheral_t uart = uart_handle->uart;
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

  status = sl_dma_channel_init(&uart_handle->async_rx_dma_channel,
                               NULL,
                               rx_channel,
                               rx_dma_channel_callback,
                               uart_handle);
  if (status != SL_STATUS_OK) {
    goto free_tx_channel;
  }

  status = sl_dma_channel_init(&uart_handle->async_tx_dma_channel,
                               NULL,
                               tx_channel,
                               tx_dma_channel_callback,
                               uart_handle);
  if (status != SL_STATUS_OK) {
    goto rx_channel_deinit;
  }

  sl_dma_signal_t rx_signal = sl_device_peripheral_get_serial_dma_signal_rx_trigger(uart);
  status = sl_dma_channel_set_peripheral_signal(&uart_handle->async_rx_dma_channel, rx_signal);
  if (status != SL_STATUS_OK) {
    goto tx_channel_deinit;
  }

  sl_dma_signal_t tx_signal = sl_device_peripheral_get_serial_dma_signal_tx_trigger(uart);
  status = sl_dma_channel_set_peripheral_signal(&uart_handle->async_tx_dma_channel, tx_signal);
  if (status != SL_STATUS_OK) {
    goto tx_channel_deinit;
  }

  return status;

  tx_channel_deinit:
  sl_dma_channel_deinit(&uart_handle->async_tx_dma_channel);
  rx_channel_deinit:
  sl_dma_channel_deinit(&uart_handle->async_rx_dma_channel);
  free_tx_channel:
  if (uart_handle->preinit_config.async_tx_dma_channel_number == SL_UART_ASYNC_DMA_CHANNEL_CONFIG_AUTO) {
    // Only free the channel if it was allocated as part of this function call.
    sl_dma_manager_free_channel(NULL, tx_channel);
  }
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

  status = sl_dma_channel_deinit(&uart_handle->async_tx_dma_channel);
  if (status != SL_STATUS_OK) {
    return status;
  }

  status = sl_dma_channel_deinit(&uart_handle->async_rx_dma_channel);
  if (status != SL_STATUS_OK) {
    return status;
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

  sl_slist_node_t **list_head = &uart_handle->async_tx_submitted_list_head;
  sli_uart_async_transfer_t *completed_tfer = sli_uart_async_transfer_from_node(*list_head);
  EFM_ASSERT(completed_tfer != NULL);

  sl_slist_remove(list_head, &completed_tfer->node);

  if (uart_handle->async_tx_complete_cb != NULL) {
    uart_handle->async_tx_complete_cb(uart_handle,
                                      completed_tfer->data,
                                      completed_tfer->size,
                                      uart_handle->async_tx_complete_cb_user_data,
                                      false);
  }

  sli_uart_transfer_list_push(&uart_handle->async_tx_pool, completed_tfer);

  while (!sl_slist_is_empty(*list_head)) {
    sli_uart_async_transfer_t *queued_tfer = sli_uart_async_transfer_from_node(*list_head);

    if (uart_async_submit_tx_dma(uart_handle, queued_tfer) == SL_STATUS_OK) {
      break;
    }

    sl_slist_remove(list_head, &queued_tfer->node);

    if (uart_handle->async_tx_complete_cb != NULL) {
      uart_handle->async_tx_complete_cb(uart_handle,
                                        queued_tfer->data,
                                        queued_tfer->bytes_completed,
                                        uart_handle->async_tx_complete_cb_user_data,
                                        true);
    }

    sli_uart_transfer_list_push(&uart_handle->async_tx_pool, queued_tfer);
  }
}
