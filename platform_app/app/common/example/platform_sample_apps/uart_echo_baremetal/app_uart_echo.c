/***************************************************************************//**
 * @file
 * @brief UART interrupt-driven echo example
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
#define ECHO_BUFFER_SIZE          32

/*******************************************************************************
 ***************************  LOCAL VARIABLES   ********************************
 ******************************************************************************/

// Echo context passed to the UART callbacks for tracking the echo buffer and
// RX/TX state. The main loop processes UART events outside interrupt context.
typedef struct {
  uint8_t buffer[ECHO_BUFFER_SIZE];
  volatile bool rx_pending;
  volatile bool tx_active;
} echo_context_t;

static echo_context_t echo_context = { 0 };

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
 * UART RX-ready callback: notify the main loop that received data is pending.
 ******************************************************************************/
static void on_rx_ready(sl_uart_handle_t *uart_handle, void *user_arg)
{
  echo_context_t *context = user_arg;

  // RX-ready is level-triggered: with unread data in the RX FIFO, this
  // interrupt would keep firing until sl_uart_read() reads the data. Disable
  // it here until the main loop processes the pending receive.
  sl_uart_disable_rx_ready_interrupt(uart_handle);
  context->rx_pending = true;
}

/***************************************************************************//**
 * UART TX-complete callback: allow the main loop to resume RX processing.
 ******************************************************************************/
static void on_tx_complete(sl_uart_handle_t *uart_handle, void *user_arg)
{
  echo_context_t *context = user_arg;

  // Finished echoing the received data to the remote.
  // Re-enable the RX Ready interrupt to process more data.
  context->tx_active = false;
  sl_uart_enable_rx_ready_interrupt(uart_handle);
}

/*******************************************************************************
 **************************   GLOBAL FUNCTIONS   *******************************
 ******************************************************************************/

/***************************************************************************//**
 * Initialize the UART echo example.
 ******************************************************************************/
void app_uart_echo_init(void)
{
  sl_status_t status;
  const char banner[] = "UART echo\r\n";

  status = sl_board_configure_vcom();
  EFM_ASSERT(status == SL_STATUS_OK);

  // Send the startup banner before enabling UART interrupts.
  uart_write_banner(banner, strlen(banner));

  // Register callbacks that signal RX-ready and TX-complete events.
  sl_uart_set_rx_ready_callback(&UART_TEST_HANDLE, on_rx_ready,
                                &echo_context);
  sl_uart_set_tx_complete_callback(&UART_TEST_HANDLE, on_tx_complete,
                                   &echo_context);

  // Enable RX-ready and TX-complete interrupts.
  sl_uart_enable_rx_ready_interrupt(&UART_TEST_HANDLE);
  sl_uart_enable_tx_complete_interrupt(&UART_TEST_HANDLE);
}

/***************************************************************************//**
 * Process pending UART reception outside interrupt context.
 *
 * This function reads the UART RX FIFO into the echo buffer when an RX-ready
 * event signals new data. The buffer is then transmitted to the UART TX FIFO.
 * RX remains disabled until the echo transmission is complete.
 ******************************************************************************/
void app_uart_echo_process_action(void)
{
  size_t read_size;
  size_t written;
  sl_status_t status;

  // Wait until new data is available and the previous echo is complete.
  if (!echo_context.rx_pending || echo_context.tx_active) {
    return;
  }

  // Read the new data from the UART RX FIFO into the echo buffer.
  status = sl_uart_read(&UART_TEST_HANDLE,
                        echo_context.buffer,
                        ECHO_BUFFER_SIZE,
                        &read_size);

  EFM_ASSERT(status == SL_STATUS_OK);

  // Start transmitting the received data by sending the buffer to the UART TX FIFO.
  // Marks the transfer as active until on_tx_complete event clears it.
  echo_context.rx_pending = false;
  echo_context.tx_active = true;
  status = sl_uart_write(&UART_TEST_HANDLE,
                         echo_context.buffer,
                         read_size,
                         &written);
  EFM_ASSERT((status == SL_STATUS_OK) && (written == read_size));
}
