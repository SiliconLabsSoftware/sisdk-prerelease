/***************************************************************************//**
 * @file
 * @brief UART asynchronous DMA echo example
 *******************************************************************************
 * # License
 * <b>Copyright 2026 Silicon Laboratories Inc. www.silabs.com</b>
 *******************************************************************************
 *
 * The licensor of this software is Silicon Laboratories Inc. Your use of this
 * software is governed by the terms of Silicon Labs Master Software License
 * Agreement (MSLA) available at
 * www.silabs.com/about-us/legal/master-software-license-agreement. This
 * software is distributed to you in Source Code format and is governed by the
 * sections of the MSLA applicable to Source Code.
 *
 ******************************************************************************/

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "sl_assert.h"
#include "sl_board_control.h"
#include "sl_status.h"
#include "sl_uart.h"
// The project's vcom UART driver instance is declared in this generated header.
#include "sl_uart_instances.h"

#include "app_uart_echo.h"

/*******************************************************************************
 *******************************   DEFINES   ***********************************
 ******************************************************************************/
/*
 * DMA alternates between two receive buffers. When DMA completely fills one
 * buffer, the application echoes it and requeues it after transmission
 * completes. Increasing this size lengthens the interval before DMA needs a
 * buffer returned, giving the CPU and UART transfer more time to complete,
 * but uses more RAM and increases echo latency.
 */
#define ECHO_BUFFER_COUNT       2
#define ECHO_BUFFER_SIZE        32
#define RX_TIMEOUT_US           100000

/*******************************************************************************
 ***************************  LOCAL VARIABLES   ********************************
 ******************************************************************************/

typedef struct buffer_context {
  uint8_t buffer[ECHO_BUFFER_SIZE];
  size_t received_bytes;            // Total bytes received in this buffer.
  size_t queued_tx_bytes;           // Total bytes submitted for echo.
  size_t transmitted_tx_bytes;      // Total bytes transmitted from this buffer.
} buffer_context_t;

typedef struct echo_context {
  buffer_context_t buffer_contexts[ECHO_BUFFER_COUNT];
  uint8_t active_rx_index;
  uint8_t active_tx_index;
} echo_context_t;

/*******************************************************************************
 ***************************  LOCAL FUNCTIONS   ********************************
 ******************************************************************************/

/***************************************************************************//**
 * Send the banner one byte at a time over UART.
 ******************************************************************************/
static void uart_write_banner(const char *banner, size_t size)
{
  sl_status_t status;

  for (size_t i = 0; i < size; i++) {
    status = sl_uart_write_byte(&UART_TEST_HANDLE, banner[i]);
    EFM_ASSERT(status == SL_STATUS_OK);
  }
}
/***************************************************************************//**
 * Check whether received bytes are still waiting to be queued for echo.
 ******************************************************************************/
static bool context_has_unqueued_bytes(const buffer_context_t *context)
{
  return context->queued_tx_bytes < context->received_bytes;
}

/***************************************************************************//**
 * Get the next buffer index in the buffer chain.
 ******************************************************************************/
static uint8_t get_next_buffer_index(uint8_t index)
{
  return (index + 1U) % ECHO_BUFFER_COUNT;
}

/***************************************************************************//**
 * Queue received bytes that have not yet been submitted for echo.
 ******************************************************************************/
static void queue_echo_bytes(sl_uart_handle_t *uart_handle,
                             buffer_context_t *context)
{
  // Calculate the number of bytes to queue for echo from the last queued byte
  // of the buffer.
  size_t bytes_to_queue = context->received_bytes - context->queued_tx_bytes;
  sl_status_t status = sl_uart_async_write(uart_handle,
                                           &context->buffer[context->queued_tx_bytes],
                                           bytes_to_queue);

  EFM_ASSERT(status == SL_STATUS_OK || status == SL_STATUS_BUSY);

  if (status == SL_STATUS_BUSY) {
    // If TX is busy and sl_uart_async_write returns SL_STATUS_BUSY, keep
    // queued_tx_bytes unchanged. The following TX complete event will queue the remaining bytes.
    return;
  }

  context->queued_tx_bytes += bytes_to_queue;
}

/***************************************************************************//**
 * Update the received byte count and queue any unqueued bytes for echo.
 ******************************************************************************/
static void echo_received_bytes(sl_uart_handle_t *uart_handle,
                                echo_context_t *echo_context,
                                size_t received_bytes)
{
  buffer_context_t *rx_context = &echo_context->buffer_contexts[echo_context->active_rx_index];

  rx_context->received_bytes = received_bytes;

  // Echo unqueued bytes from the received bytes so far if the buffer is active
  // for transmission. If another buffer is active, the TX-complete callback
  // will queue the received bytes after switching to this buffer.
  if (echo_context->active_rx_index == echo_context->active_tx_index
      && context_has_unqueued_bytes(rx_context)) {
    queue_echo_bytes(uart_handle, rx_context);
  }
}

/***************************************************************************//**
 * Handle asynchronous RX events for a buffer context.
 *
 * The driver notifies the application of events through this callback.
 * On a timeout event, data received did not fully fill the buffer and no new data
 * was received since then for the timeout period. The current buffer remains
 * active but the application queues any bytes not yet submitted for echo and
 * continues receiving into the same buffer. When a BUF_RELEASED event is received,
 * the buffer is full and released; queue any remaining bytes for echo and switch
 * to the other buffer. A released buffer is requeued for reception with
 * sl_uart_async_read() only after its echo transmission completes.
 ******************************************************************************/
static void on_rx_event(sl_uart_handle_t *uart_handle,
                        void *data,
                        size_t size,
                        void *user_data,
                        sl_uart_async_rx_event_t rx_event)
{
  echo_context_t *context = user_data;
  EFM_ASSERT(data == context->buffer_contexts[context->active_rx_index].buffer);

  // On a timeout or buffer release event, size indicates the number of bytes
  // received so far that should be queued for echo.
  echo_received_bytes(uart_handle, context, size);

  if (rx_event & SL_UART_ASYNC_RX_EVENT_TIMEOUT) {
    // The timeout does not release the buffer, keep filling the active
    // partly-filled buffer.
    return;
  }

  // BUF_RELEASED indicates that the RX buffer is fully filled. After queuing the
  // received bytes, advance the active RX index to the next buffer. The
  // buffer will be reusable only after its last echo transfer completes.
  if (rx_event & SL_UART_ASYNC_RX_EVENT_BUF_RELEASED) {
    context->active_rx_index = get_next_buffer_index(context->active_rx_index);
    return;
  }

  // These other events are not handled in this sample application.
  if (rx_event & SL_UART_ASYNC_RX_EVENT_STOPPED) {
    // STOPPED is reported with BUF_RELEASED when this was the last queued
    // buffer. RX will resume when the TX-complete callback requeues the
    // released buffer after its echo finishes.
    return;
  }
  if (rx_event & SL_UART_ASYNC_RX_EVENT_BUF_NEEDED) {
    // The driver has started reception on its last queued RX buffer. The
    // application will resubmit the completed RX buffer once its contents have
    // been echoed in on_tx_complete().
    return;
  }
  if (rx_event & SL_UART_ASYNC_RX_EVENT_ABORTED) {
    // The aborted event should not happen as sl_uart_async_disable_rx is not used here.
    return;
  }
}

/***************************************************************************//**
 * UART asynchronous TX-complete callback: runs after received data has been
 * sent to the remote device. If more data has arrived while the previous echo
 * was being transmitted, queue it for transmission. Once all data from a
 * receive buffer has been echoed, add the buffer back to the reception pool
 * with sl_uart_async_read().
 ******************************************************************************/
static void on_tx_complete(sl_uart_handle_t *uart_handle,
                           const void *data,
                           size_t size,
                           void *user_data,
                           bool aborted)
{
  echo_context_t *context = user_data;
  buffer_context_t *active_tx_context = &context->buffer_contexts[context->active_tx_index];
  (void) data;

  // The aborted flag should never be set as we are not using sl_uart_async_abort_tx.
  EFM_ASSERT(!aborted);

  active_tx_context->transmitted_tx_bytes += size;

  // RX may have received more bytes after the previous echo chunk was
  // queued, or the previous queue attempt may have returned SL_STATUS_BUSY.
  // Retry all received bytes that have not yet been submitted for echo now
  // that this TX transfer has completed.
  if (context_has_unqueued_bytes(active_tx_context)) {
    queue_echo_bytes(uart_handle, active_tx_context);
    return;
  }

  // Each queued transmission generates a TXC event. Only requeue the buffer if
  // the entire buffer has been transmitted.
  if (active_tx_context->transmitted_tx_bytes != ECHO_BUFFER_SIZE) {
    return;
  }

  // RX has released this buffer and its final echo has completed.
  // Reset the buffer context and requeue it for reception.
  active_tx_context->received_bytes = 0;
  active_tx_context->queued_tx_bytes = 0;
  active_tx_context->transmitted_tx_bytes = 0;
  sl_status_t status = sl_uart_async_read(uart_handle,
                                          active_tx_context->buffer,
                                          ECHO_BUFFER_SIZE);
  EFM_ASSERT(status == SL_STATUS_OK);

  context->active_tx_index = get_next_buffer_index(context->active_tx_index);

  // The next buffer may already have bytes waiting for echo.
  active_tx_context = &context->buffer_contexts[context->active_tx_index];
  if (context_has_unqueued_bytes(active_tx_context)) {
    queue_echo_bytes(uart_handle, active_tx_context);
  }
}

/*******************************************************************************
 **************************   GLOBAL FUNCTIONS   *******************************
 ******************************************************************************/

/***************************************************************************//**
 * Initialize the UART asynchronous echo example.
 ******************************************************************************/
void app_uart_echo_init(void)
{
  // Initialize the buffer contexts and active indices to zero.
  // Reception and transmission start on the first queued buffer.
  static echo_context_t echo_context = { 0 };
  sl_status_t status;
  const char banner[] = "UART async echo\r\n";

  status = sl_board_configure_vcom();
  EFM_ASSERT(status == SL_STATUS_OK);

  sl_uart_async_set_rx_event_callback(&UART_TEST_HANDLE,
                                      on_rx_event,
                                      &echo_context);
  sl_uart_async_set_tx_complete_callback(&UART_TEST_HANDLE,
                                         on_tx_complete,
                                         &echo_context);

  // RX timeout fires when the bus stays idle for RX_TIMEOUT_US after a symbol
  // is received. It will not fire again until another byte arrives and the bus
  // goes idle for RX_TIMEOUT_US once more.
  status = sl_uart_async_read_set_timeout(&UART_TEST_HANDLE,
                                          RX_TIMEOUT_US);
  EFM_ASSERT(status == SL_STATUS_OK);

  uart_write_banner(banner, strlen(banner));

  // Queue the receive buffers. A buffer is requeued for receiving when the
  // TX event notifies its final echo has completed.
  for (size_t i = 0; i < ECHO_BUFFER_COUNT; i++) {
    status = sl_uart_async_read(&UART_TEST_HANDLE,
                                echo_context.buffer_contexts[i].buffer,
                                ECHO_BUFFER_SIZE);
    EFM_ASSERT(status == SL_STATUS_OK);
  }
}

/***************************************************************************//**
 * No polling is needed; UART callbacks handle reception and echoing.
 ******************************************************************************/
void app_uart_echo_process_action(void)
{
}
