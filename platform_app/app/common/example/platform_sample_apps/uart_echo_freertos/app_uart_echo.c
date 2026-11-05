/***************************************************************************//**
 * @file
 * @brief UART interrupt-driven echo example (FreeRTOS)
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

#include "FreeRTOS.h"
#include "semphr.h"
#include "task.h"

#include "app_uart_echo.h"

/*******************************************************************************
 *******************************   DEFINES   ***********************************
 ******************************************************************************/
#define ECHO_BUFFER_SIZE          32

#ifndef UART_ECHO_TASK_STACK_SIZE
#define UART_ECHO_TASK_STACK_SIZE 256
#endif

#ifndef UART_ECHO_TASK_PRIO
#define UART_ECHO_TASK_PRIO       (tskIDLE_PRIORITY + 2)
#endif

/*******************************************************************************
 ***************************  LOCAL VARIABLES   ********************************
 ******************************************************************************/
// Echo context passed to the echo task and UART callbacks for tracking the
// echo buffer and events. Semaphores wake the echo task upon UART events.
typedef struct {
  uint8_t buffer[ECHO_BUFFER_SIZE];
  SemaphoreHandle_t rx_ready_semaphore;
  SemaphoreHandle_t tx_complete_semaphore;
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
 * UART RX-ready callback: post a semaphore so the echo task can drain RX.
 ******************************************************************************/
static void on_rx_ready(sl_uart_handle_t *uart_handle, void *user_arg)
{
  echo_context_t *context = user_arg;

  // RX-ready is level-triggered. Disable the interrupt here until the RX FIFO
  // is read and the RX-ready interrupt is re-enabled.
  sl_uart_disable_rx_ready_interrupt(uart_handle);
  xSemaphoreGiveFromISR(context->rx_ready_semaphore, NULL);
}

/***************************************************************************//**
 * UART TX-complete callback: post a semaphore so the echo task can resume RX.
 ******************************************************************************/
static void on_tx_complete(sl_uart_handle_t *uart_handle, void *user_arg)
{
  echo_context_t *context = user_arg;
  (void)uart_handle;

  // Wake the echo task to process the next rx event.
  xSemaphoreGiveFromISR(context->tx_complete_semaphore, NULL);
}

/***************************************************************************//**
 * FreeRTOS task that processes RX and TX events posted from UART callbacks.
 * This task is responsible for reading the RX FIFO into the buffer when a
 * RX ready event signals new data in the UART RX FIFO. It is then transmitted
 * back to the sender by writing the buffer into the UART TX FIFO.
 ******************************************************************************/
static void uart_echo_task(void *pvParameters)
{
  const char banner[] = "UART echo under FreeRTOS\r\n";
  echo_context_t *context = pvParameters;
  size_t read_size;
  size_t written;
  sl_status_t status;
  BaseType_t semaphore_status;

  // Enable TX complete interrupt to signal the echo task when the startup banner is fully transmitted.
  sl_uart_enable_tx_complete_interrupt(&UART_TEST_HANDLE);
  uart_write_banner(banner, strlen(banner));

  // Wait until the startup banner is fully transmitted.
  semaphore_status = xSemaphoreTake(context->tx_complete_semaphore, portMAX_DELAY);
  EFM_ASSERT(semaphore_status == pdTRUE);

  while (1) {
    // Enable RX-ready interrupt to signal the echo task when new data is available.
    sl_uart_enable_rx_ready_interrupt(&UART_TEST_HANDLE);

    // Wait until new data is available to read from the UART RX FIFO.
    semaphore_status = xSemaphoreTake(context->rx_ready_semaphore, portMAX_DELAY);
    EFM_ASSERT(semaphore_status == pdTRUE);

    // Read the new data from the UART RX FIFO into the buffer.
    status = sl_uart_read(&UART_TEST_HANDLE,
                          context->buffer,
                          ECHO_BUFFER_SIZE,
                          &read_size);

    EFM_ASSERT(status == SL_STATUS_OK);

    // Transmit the echo by sending the buffer to the UART TX FIFO.
    status = sl_uart_write(&UART_TEST_HANDLE,
                           context->buffer,
                           read_size,
                           &written);
    EFM_ASSERT((status == SL_STATUS_OK) && (written == read_size));

    // Wait until the echo transmission completes.
    semaphore_status = xSemaphoreTake(context->tx_complete_semaphore, portMAX_DELAY);
    EFM_ASSERT(semaphore_status == pdTRUE);
  }
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
  BaseType_t task_status;

  status = sl_board_configure_vcom();
  EFM_ASSERT(status == SL_STATUS_OK);

  echo_context.rx_ready_semaphore = xSemaphoreCreateBinary();
  echo_context.tx_complete_semaphore = xSemaphoreCreateBinary();
  EFM_ASSERT(echo_context.rx_ready_semaphore != NULL);
  EFM_ASSERT(echo_context.tx_complete_semaphore != NULL);

  sl_uart_set_rx_ready_callback(&UART_TEST_HANDLE, on_rx_ready,
                                &echo_context);
  sl_uart_set_tx_complete_callback(&UART_TEST_HANDLE, on_tx_complete,
                                   &echo_context);

  task_status = xTaskCreate(uart_echo_task,
                            "UART echo",
                            UART_ECHO_TASK_STACK_SIZE,
                            &echo_context,
                            UART_ECHO_TASK_PRIO,
                            NULL);
  EFM_ASSERT(task_status == pdPASS);
}

/***************************************************************************//**
 * UART echo ticking function. The FreeRTOS echo task handles the UART.
 ******************************************************************************/
void app_uart_echo_process_action(void)
{
}
