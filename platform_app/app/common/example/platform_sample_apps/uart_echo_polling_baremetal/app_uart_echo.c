/***************************************************************************//**
 * @file
 * @brief UART polling echo example
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
/*******************************************************************************
 ***************************  LOCAL FUNCTIONS   ********************************
 ******************************************************************************/

/***************************************************************************//**
 * Send the startup banner one byte at a time over UART.
 ******************************************************************************/
static void uart_write_banner(const char *banner, size_t size)
{
  sl_status_t status;

  for (size_t i = 0; i < size; i++) {
    status = sl_uart_write_byte(&UART_TEST_HANDLE, banner[i]);
    EFM_ASSERT(status == SL_STATUS_OK);
  }
}

/*******************************************************************************
 **************************   GLOBAL FUNCTIONS   *******************************
 ******************************************************************************/

/***************************************************************************//**
 * Initialize the UART polling echo example.
 ******************************************************************************/
void app_uart_echo_init(void)
{
  sl_status_t status;
  const char banner[] = "UART polling echo\r\n";

  status = sl_board_configure_vcom();
  EFM_ASSERT(status == SL_STATUS_OK);

  uart_write_banner(banner, strlen(banner));
}

/***************************************************************************//**
 * UART polling echo loop. Read a byte at a time and echo it back.
 ******************************************************************************/
void app_uart_echo_process_action(void)
{
  uint8_t byte;
  sl_status_t status;

  while (1) {
    status = sl_uart_read_byte(&UART_TEST_HANDLE, &byte);
    if (status == SL_STATUS_OK) {
      // Byte read from RX FIFO, write it back to TX FIFO
      status = sl_uart_write_byte(&UART_TEST_HANDLE, byte);
      EFM_ASSERT(status == SL_STATUS_OK);
    } else {
      // Nothing to read.
      EFM_ASSERT(status == SL_STATUS_EMPTY);
    }
  }
}
