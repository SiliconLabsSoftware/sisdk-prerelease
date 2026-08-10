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

#include "em_device.h"

#include "sl_assert.h"
#include "sl_clock_manager.h"
#include "sl_core.h"
#include "sl_interrupt_manager.h"
#include "sl_device_peripheral.h"
#include "sl_device_peripheral_types.h"
#include "sl_device_uart.h"
#include "sl_hal_gpio.h"
#include "sl_status.h"
#include "sl_uart.h"

#include "sli_uart.h"
#include "sli_uart_async.h"

#if defined(SL_CATALOG_POWER_MANAGER_PRESENT)
#include "sl_power_manager.h"
#endif

#if defined(EUART_PRESENT) || defined(EUSART_PRESENT)
#include "sli_uart_eusart.h"
#endif

#if defined(USART_PRESENT)
#include "sli_uart_usart.h"
#endif

/*******************************************************************************
 *******************************   DEFINES   ***********************************
 ******************************************************************************/

/*******************************************************************************
 ***************************   LOCAL FUNCTIONS   *******************************
 ******************************************************************************/

/***************************************************************************//**
 * Brings up UART clocks, peripheral, pins, and IRQ when the handle is suspended.
 ******************************************************************************/
static void uart_init_hw(sl_uart_handle_t *uart_handle, sl_uart_pin_config_t pin_config)
{
  sli_uart_init_clocks(uart_handle);
  sli_uart_reset(uart_handle);
  sli_uart_init_transport_pins(uart_handle, pin_config);
  sli_uart_init_irq(uart_handle);
}

/***************************************************************************//**
 * Tears down UART clocks, peripheral, pins, and IRQ.
 ******************************************************************************/
static void uart_deinit_hw(sl_uart_handle_t *uart_handle)
{
  sli_uart_deinit_irq(uart_handle->uart);
  sli_uart_deinit_peripheral(uart_handle);
  sli_uart_deinit_clocks(uart_handle);
  sli_uart_deinit_transport_pins(uart_handle);

  if (SLI_UART_CONFIG_IS_VALID(uart_handle->config)
      && uart_handle->config.flow_control == SL_UART_FLOW_CONTROL_CTS_RTS) {
    sli_uart_deinit_hwfc_pins(uart_handle);
  }
}

/*******************************************************************************
**************************   GLOBAL FUNCTIONS   *******************************
*******************************************************************************/

/***************************************************************************//**
 * Allocates a UART handle.
 ******************************************************************************/
sl_status_t sl_uart_handle_alloc(sl_uart_handle_t **uart_handle)
{
  EFM_ASSERT(uart_handle != NULL);

  *uart_handle = calloc(1, sl_uart_handle_get_size());
  if (*uart_handle == NULL) {
    return SL_STATUS_ALLOCATION_FAILED;
  }

  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Frees a UART handle.
 ******************************************************************************/
sl_status_t sl_uart_handle_free(sl_uart_handle_t *uart_handle)
{
  EFM_ASSERT(uart_handle != NULL);
  EFM_ASSERT(uart_handle->uart == NULL);

  free(uart_handle);

  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Initializes the UART instance and its peripheral.
 ******************************************************************************/
sl_status_t sl_uart_init(sl_uart_handle_t *uart_handle, sl_peripheral_t uart, sl_uart_pin_config_t pin_config)
{
  sl_status_t status;

  if (uart_handle->uart != NULL) {
    return SL_STATUS_ALREADY_INITIALIZED;
  }

  sli_uart_init_core(uart_handle, uart, pin_config);

  if (SLI_UART_HANDLE_IS_ASYNC(uart_handle)) {
    status = sli_uart_async_init(uart_handle);
    if (status != SL_STATUS_OK) {
      sli_uart_deinit_core(uart_handle);

      return status;
    }
  }

  #if !defined(__ZEPHYR__)
  status = sl_uart_resume(uart_handle);
  if (status != SL_STATUS_OK) {
    if (SLI_UART_HANDLE_IS_ASYNC(uart_handle)) {
      sl_status_t _status = sli_uart_async_deinit(uart_handle);
      EFM_ASSERT(_status == SL_STATUS_OK);
    }

    sli_uart_deinit_core(uart_handle);

    return status;
  }
  #endif

  return SL_STATUS_OK;
}

/***************************************************************************//**
 * De-initializes the UART instance and its underlying peripheral.
 ******************************************************************************/
sl_status_t sl_uart_deinit(sl_uart_handle_t *uart_handle)
{
  EFM_ASSERT(uart_handle != NULL);
  sl_status_t status;

  if (uart_handle->uart == NULL) {
    return SL_STATUS_NOT_INITIALIZED;
  }

  if (SLI_UART_HANDLE_IS_ASYNC(uart_handle)) {
    status = sli_uart_async_deinit(uart_handle);
    if (status != SL_STATUS_OK) {
      return status;
    }
  }

  // Hardware is already down when suspended; skip teardown in that case.
  if (!SLI_UART_HANDLE_IS_SUSPENDED(uart_handle)) {
    uart_deinit_hw(uart_handle);
    #if defined(SL_CATALOG_POWER_MANAGER_PRESENT)
    sl_power_manager_remove_em_requirement(uart_handle->ops->get_em_requirement(uart_handle->uart));
    #endif
  }

  sli_uart_deinit_core(uart_handle);

  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Suspends the UART peripheral hardware.
 ******************************************************************************/
sl_status_t sl_uart_suspend(sl_uart_handle_t *uart_handle)
{
  SLI_UART_ASSERT_VALID_HANDLE(uart_handle);
  sl_status_t status;

  if (SLI_UART_HANDLE_IS_SUSPENDED(uart_handle)) {
    return SL_STATUS_OK;
  }

  if (!uart_handle->ops->is_idle(uart_handle->uart)) {
    return SL_STATUS_BUSY;
  }

  if (SLI_UART_HANDLE_IS_ASYNC(uart_handle)) {
    status = sli_uart_async_suspend(uart_handle);
    if (status != SL_STATUS_OK) {
      return status;
    }
  }

  uart_deinit_hw(uart_handle);

  uart_handle->state = SL_UART_HANDLE_STATE_SUSPENDED;

  #if defined(SL_CATALOG_POWER_MANAGER_PRESENT)
  sl_power_manager_remove_em_requirement(uart_handle->ops->get_em_requirement(uart_handle->uart));
  #endif

  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Resumes the UART peripheral hardware.
 ******************************************************************************/
sl_status_t sl_uart_resume(sl_uart_handle_t *uart_handle)
{
  SLI_UART_ASSERT_VALID_HANDLE(uart_handle);
  sl_status_t status;

  if (!SLI_UART_HANDLE_IS_SUSPENDED(uart_handle)) {
    return SL_STATUS_OK;
  }

  uart_init_hw(uart_handle, uart_handle->pin_config);

  if (SLI_UART_HANDLE_IS_ASYNC(uart_handle)) {
    status = sli_uart_async_resume(uart_handle);
    if (status != SL_STATUS_OK) {
      uart_deinit_hw(uart_handle);
      return status;
    }
  }

  #if defined(SL_CATALOG_POWER_MANAGER_PRESENT)
  sl_power_manager_add_em_requirement(uart_handle->ops->get_em_requirement(uart_handle->uart));
  #endif

  uart_handle->state = SL_UART_HANDLE_STATE_IDLE;

  if (!SLI_UART_CONFIG_IS_VALID(uart_handle->config)) {
    // Suspended before getting a valid configuration, nothing else to do.
    return SL_STATUS_OK;
  }

  // Zero-out the instance configuration to force re-apply the configuration.
  sl_uart_config_t config = uart_handle->config;
  memset(&uart_handle->config, 0, sizeof(uart_handle->config));

  // Restoring the configuration should always succeed, since it was successfully applied before suspension.
  status = sl_uart_configure_line(uart_handle, config);
  EFM_ASSERT(status == SL_STATUS_OK);

  sli_uart_enable_irq(uart_handle, uart_handle->enabled_irq);

  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Configures the UART line for the underlying peripheral.
 ******************************************************************************/
sl_status_t sl_uart_configure_line(sl_uart_handle_t *uart_handle, sl_uart_config_t config)
{
  CORE_DECLARE_IRQ_STATE;
  sl_status_t status;

  SLI_UART_ASSERT_VALID_HANDLE(uart_handle);
  EFM_ASSERT(SLI_UART_CONFIG_IS_VALID(config));

  if (SLI_UART_HANDLE_IS_SUSPENDED(uart_handle)) {
    return SL_STATUS_INVALID_STATE;
  }

  if (config.baudrate == SL_UART_BAUDRATE_AUTO
      || config.flow_control == SL_UART_FLOW_CONTROL_SOFT) {
    // CPC-3222: Add support for auto baud rate detection and software flow control.
    return SL_STATUS_NOT_SUPPORTED;
  }

  sl_peripheral_t uart = uart_handle->uart;

  bool changed_fc = (config.flow_control != uart_handle->config.flow_control);

  // Apply config atomically to avoid interrupt state mismatch.
  CORE_ENTER_ATOMIC();

  if (changed_fc && (config.flow_control == SL_UART_FLOW_CONTROL_CTS_RTS)) {
    sli_uart_init_hwfc_pins(uart_handle, uart_handle->pin_config);
  }

  status = uart_handle->ops->init(uart, config);
  if (status != SL_STATUS_OK) {
    goto err;
  }

  if (changed_fc && (config.flow_control != SL_UART_FLOW_CONTROL_CTS_RTS)) {
    sli_uart_deinit_hwfc_pins(uart_handle);
  }

  CORE_EXIT_ATOMIC();

  uart_handle->config = config;

  return SL_STATUS_OK;

  err:
  if (changed_fc && (config.flow_control == SL_UART_FLOW_CONTROL_CTS_RTS)) {
    sli_uart_deinit_hwfc_pins(uart_handle);
  }

  CORE_EXIT_ATOMIC();

  return status;
}

/***************************************************************************//**
 * Reads a single character on UART.
 ******************************************************************************/
sl_status_t sl_uart_read_byte(sl_uart_handle_t *uart_handle, uint8_t * byte)
{
  SLI_UART_ASSERT_VALID_HANDLE(uart_handle);
  EFM_ASSERT(SLI_UART_CONFIG_IS_VALID(uart_handle->config));
  EFM_ASSERT(SLI_UART_HANDLE_IS_SYNC(uart_handle));
  EFM_ASSERT(byte != NULL);
  EFM_ASSERT(uart_handle->config.data_bits <= SL_UART_DATA_BITS_8);

  if (SLI_UART_HANDLE_IS_SUSPENDED(uart_handle)) {
    return SL_STATUS_INVALID_STATE;
  }

  return uart_handle->ops->read_byte(uart_handle->uart, byte);
}

/***************************************************************************//**
 * Writes a single character on UART.
 ******************************************************************************/
sl_status_t sl_uart_write_byte(sl_uart_handle_t *uart_handle, uint8_t byte)
{
  SLI_UART_ASSERT_VALID_HANDLE(uart_handle);
  EFM_ASSERT(SLI_UART_CONFIG_IS_VALID(uart_handle->config));
  EFM_ASSERT(SLI_UART_HANDLE_IS_SYNC(uart_handle));

  EFM_ASSERT(uart_handle->config.data_bits <= SL_UART_DATA_BITS_8);

  if (SLI_UART_HANDLE_IS_SUSPENDED(uart_handle)) {
    return SL_STATUS_INVALID_STATE;
  }

  sl_status_t (*write_byte)(sl_peripheral_t, uint8_t) = uart_handle->ops->write_byte;

  while (write_byte(uart_handle->uart, byte) != SL_STATUS_OK) {
    // Wait until byte is written to TX FIFO.
  }

  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Reads data from RX FIFO.
 ******************************************************************************/
sl_status_t sl_uart_read(sl_uart_handle_t *uart_handle, void *data, const size_t size, size_t *read_size)
{
  SLI_UART_ASSERT_VALID_HANDLE(uart_handle);
  EFM_ASSERT(SLI_UART_CONFIG_IS_VALID(uart_handle->config));
  EFM_ASSERT(SLI_UART_HANDLE_IS_SYNC(uart_handle));

  EFM_ASSERT(data != NULL);
  EFM_ASSERT(size > 0);
  EFM_ASSERT(read_size != NULL);

  if (SLI_UART_HANDLE_IS_SUSPENDED(uart_handle)) {
    return SL_STATUS_INVALID_STATE;
  }

  return uart_handle->ops->read_buffer(uart_handle->uart, (uint8_t *)data, size, read_size);
}

/***************************************************************************//**
 * Writes data to TX FIFO.
 ******************************************************************************/
sl_status_t sl_uart_write(sl_uart_handle_t *uart_handle,
                          const void *data,
                          const size_t size,
                          size_t *write_size)
{
  SLI_UART_ASSERT_VALID_HANDLE(uart_handle);
  EFM_ASSERT(SLI_UART_CONFIG_IS_VALID(uart_handle->config));
  EFM_ASSERT(SLI_UART_HANDLE_IS_SYNC(uart_handle));

  EFM_ASSERT(size == 0 || data != NULL);
  EFM_ASSERT(write_size != NULL);

  if (SLI_UART_HANDLE_IS_SUSPENDED(uart_handle)) {
    return SL_STATUS_INVALID_STATE;
  }

  return uart_handle->ops->write_buffer(uart_handle->uart, (const uint8_t *)data, size, write_size);
}

/***************************************************************************//**
 * Gets the line configuration.
 ******************************************************************************/
sl_status_t sl_uart_get_line_configuration(sl_uart_handle_t *uart_handle, sl_uart_config_t * config)
{
  SLI_UART_ASSERT_VALID_HANDLE(uart_handle);
  EFM_ASSERT(config != NULL);
  EFM_ASSERT(SLI_UART_CONFIG_IS_VALID(uart_handle->config));

  *config = uart_handle->config;

  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Registers a callback that will be called when a RX error happens.
 ******************************************************************************/
void sl_uart_set_rx_err_callback(sl_uart_handle_t *uart_handle,
                                 sl_uart_rx_err_cb_t rx_err_cb,
                                 void *user_arg)
{
  CORE_DECLARE_IRQ_STATE;
  SLI_UART_ASSERT_VALID_HANDLE(uart_handle);
  EFM_ASSERT(rx_err_cb != NULL);
  EFM_ASSERT(!SLI_UART_HANDLE_IS_SUSPENDED(uart_handle));

  const sli_uart_ops_t *ops = uart_handle->ops;

  CORE_ENTER_ATOMIC();
  uart_handle->rx_err_cb = rx_err_cb;
  uart_handle->rx_err_cb_arg = user_arg;

  sli_uart_enable_irq(uart_handle, ops->irq_rx_err_flag);

  CORE_EXIT_ATOMIC();
}

/***************************************************************************//**
 * Registers a RX ready callback.
 ******************************************************************************/
void sl_uart_set_rx_ready_callback(sl_uart_handle_t *uart_handle,
                                   sl_uart_rx_ready_cb_t rx_ready_cb,
                                   void *user_arg)
{
  CORE_DECLARE_IRQ_STATE;
  SLI_UART_ASSERT_VALID_HANDLE(uart_handle);
  EFM_ASSERT(SLI_UART_HANDLE_IS_SYNC(uart_handle));

  EFM_ASSERT(rx_ready_cb != NULL);

  CORE_ENTER_ATOMIC();
  uart_handle->rx_ready_cb = rx_ready_cb;
  uart_handle->rx_ready_cb_arg = user_arg;
  CORE_EXIT_ATOMIC();
}

/***************************************************************************//**
 * Registers a TX ready callback.
 ******************************************************************************/
void sl_uart_set_tx_ready_callback(sl_uart_handle_t *uart_handle,
                                   sl_uart_tx_ready_cb_t tx_ready_cb,
                                   void *user_arg)
{
  CORE_DECLARE_IRQ_STATE;
  SLI_UART_ASSERT_VALID_HANDLE(uart_handle);
  EFM_ASSERT(SLI_UART_HANDLE_IS_SYNC(uart_handle));

  EFM_ASSERT(tx_ready_cb != NULL);

  CORE_ENTER_ATOMIC();
  uart_handle->tx_ready_cb = tx_ready_cb;
  uart_handle->tx_ready_cb_arg = user_arg;
  CORE_EXIT_ATOMIC();
}

/***************************************************************************//**
 * Registers a TX complete callback.
 ******************************************************************************/
void sl_uart_set_tx_complete_callback(sl_uart_handle_t *uart_handle,
                                      sl_uart_tx_complete_cb_t tx_complete_cb,
                                      void *user_arg)
{
  CORE_DECLARE_IRQ_STATE;
  SLI_UART_ASSERT_VALID_HANDLE(uart_handle);
  EFM_ASSERT(SLI_UART_HANDLE_IS_SYNC(uart_handle));

  EFM_ASSERT(tx_complete_cb != NULL);

  CORE_ENTER_ATOMIC();
  uart_handle->tx_complete_cb = tx_complete_cb;
  uart_handle->tx_complete_cb_arg = user_arg;
  CORE_EXIT_ATOMIC();
}

/***************************************************************************//**
 * Enables RX ready interrupt.
 ******************************************************************************/
void sl_uart_enable_rx_ready_interrupt(sl_uart_handle_t *uart_handle)
{
  SLI_UART_ASSERT_VALID_HANDLE(uart_handle);
  EFM_ASSERT(SLI_UART_CONFIG_IS_VALID(uart_handle->config));
  EFM_ASSERT(SLI_UART_HANDLE_IS_SYNC(uart_handle));
  EFM_ASSERT(!SLI_UART_HANDLE_IS_SUSPENDED(uart_handle));

  EFM_ASSERT(uart_handle->rx_ready_cb != NULL);

  const sli_uart_ops_t *ops = uart_handle->ops;

  sli_uart_enable_irq(uart_handle, ops->irq_rx_ready_flag);
}

/***************************************************************************//**
 * Enables TX ready interrupt.
 ******************************************************************************/
void sl_uart_enable_tx_ready_interrupt(sl_uart_handle_t *uart_handle)
{
  SLI_UART_ASSERT_VALID_HANDLE(uart_handle);
  EFM_ASSERT(SLI_UART_CONFIG_IS_VALID(uart_handle->config));
  EFM_ASSERT(SLI_UART_HANDLE_IS_SYNC(uart_handle));
  EFM_ASSERT(!SLI_UART_HANDLE_IS_SUSPENDED(uart_handle));

  EFM_ASSERT(uart_handle->tx_ready_cb != NULL);

  const sli_uart_ops_t *ops = uart_handle->ops;

  sli_uart_enable_irq(uart_handle, ops->irq_tx_ready_flag);
}

/***************************************************************************//**
 * Enables TX complete interrupt.
 ******************************************************************************/
void sl_uart_enable_tx_complete_interrupt(sl_uart_handle_t *uart_handle)
{
  SLI_UART_ASSERT_VALID_HANDLE(uart_handle);
  EFM_ASSERT(SLI_UART_CONFIG_IS_VALID(uart_handle->config));
  EFM_ASSERT(SLI_UART_HANDLE_IS_SYNC(uart_handle));
  EFM_ASSERT(!SLI_UART_HANDLE_IS_SUSPENDED(uart_handle));

  EFM_ASSERT(uart_handle->tx_complete_cb != NULL);

  const sli_uart_ops_t *ops = uart_handle->ops;

  sli_uart_enable_irq(uart_handle, ops->irq_tx_complete_flag);
}

/***************************************************************************//**
 * Disables RX ready interrupt.
 ******************************************************************************/
void sl_uart_disable_rx_ready_interrupt(sl_uart_handle_t *uart_handle)
{
  SLI_UART_ASSERT_VALID_HANDLE(uart_handle);
  EFM_ASSERT(SLI_UART_HANDLE_IS_SYNC(uart_handle));
  EFM_ASSERT(!SLI_UART_HANDLE_IS_SUSPENDED(uart_handle));

  const sli_uart_ops_t *ops = uart_handle->ops;

  sli_uart_disable_irq(uart_handle, ops->irq_rx_ready_flag);
}

/***************************************************************************//**
 * Disables TX ready interrupt.
 ******************************************************************************/
void sl_uart_disable_tx_ready_interrupt(sl_uart_handle_t *uart_handle)
{
  SLI_UART_ASSERT_VALID_HANDLE(uart_handle);
  EFM_ASSERT(SLI_UART_HANDLE_IS_SYNC(uart_handle));
  EFM_ASSERT(!SLI_UART_HANDLE_IS_SUSPENDED(uart_handle));

  const sli_uart_ops_t *ops = uart_handle->ops;

  sli_uart_disable_irq(uart_handle, ops->irq_tx_ready_flag);
}

/***************************************************************************//**
 * Disables TX complete interrupt.
 ******************************************************************************/
void sl_uart_disable_tx_complete_interrupt(sl_uart_handle_t *uart_handle)
{
  SLI_UART_ASSERT_VALID_HANDLE(uart_handle);
  EFM_ASSERT(SLI_UART_HANDLE_IS_SYNC(uart_handle));
  EFM_ASSERT(!SLI_UART_HANDLE_IS_SUSPENDED(uart_handle));

  const sli_uart_ops_t *ops = uart_handle->ops;

  sli_uart_disable_irq(uart_handle, ops->irq_tx_complete_flag);
}

/*******************************************************************************
 **************************   INTERNAL FUNCTIONS   *****************************
 ******************************************************************************/
void sli_uart_init_core(sl_uart_handle_t *uart_handle, sl_peripheral_t uart, sl_uart_pin_config_t pin_config)
{
  EFM_ASSERT(uart_handle != NULL);
  EFM_ASSERT(uart != NULL);
  EFM_ASSERT(uart_handle->uart == NULL);

  uart_handle->uart = uart;
  uart_handle->pin_config = pin_config;

  switch (sl_device_peripheral_get_serial_ip_type(uart)) {
    #if defined(EUART_PRESENT)
    case SL_PERIPHERAL_SERIAL_TYPE_EUSART:
      uart_handle->ops = &sli_uart_euart_ops;
      break;
    #elif defined(EUSART_PRESENT)
    case SL_PERIPHERAL_SERIAL_TYPE_EUSART:
      uart_handle->ops = &sli_uart_eusart_ops;
      break;
    #endif
    #if defined(USART_PRESENT)
    case SL_PERIPHERAL_SERIAL_TYPE_USART:
      uart_handle->ops = &sli_uart_usart_ops;
      break;
    #endif
    default:
      EFM_ASSERT(false);
      uart_handle->ops = NULL;
      return;
  }
}

void sli_uart_deinit_core(sl_uart_handle_t *uart_handle)
{
  EFM_ASSERT(uart_handle != NULL);

  #if defined(SL_CATALOG_UART_ASYNC_PRESENT)
  // Save the pre-init configuration to restore it after clearing the handle.
  sl_uart_preinit_config_t preinit_config = uart_handle->preinit_config;
  #endif

  memset(uart_handle, 0, sizeof(sl_uart_handle_t));

  #if defined(SL_CATALOG_UART_ASYNC_PRESENT)
  uart_handle->preinit_config = preinit_config;
  #endif
}

void sli_uart_deinit_peripheral(sl_uart_handle_t *uart_handle)
{
  uart_handle->ops->deinit(uart_handle->uart);
}

SL_WEAK void sli_uart_init_transport_pins(sl_uart_handle_t *uart_handle, sl_uart_pin_config_t pin_config)
{
  SLI_UART_ASSERT_VALID_HANDLE(uart_handle);

  // Initialize the TX pin last so that we can use the same pin for RX and TX in loopback mode.
  sl_hal_gpio_set_pin_mode(&pin_config.rx, SL_GPIO_MODE_INPUT_PULL, 1);
  sl_hal_gpio_set_pin_mode(&pin_config.tx, SL_GPIO_MODE_PUSH_PULL, 1);

  uart_handle->ops->init_transport_pins(uart_handle->uart, pin_config);

  uart_handle->pin_config = pin_config;
}

SL_WEAK void sli_uart_deinit_transport_pins(sl_uart_handle_t *uart_handle)
{
  SLI_UART_ASSERT_VALID_HANDLE(uart_handle);

  sl_uart_pin_config_t pin_config = uart_handle->pin_config;

  sl_hal_gpio_set_pin_mode(&pin_config.rx, SL_GPIO_MODE_DISABLED, 0);
  sl_hal_gpio_set_pin_mode(&pin_config.tx, SL_GPIO_MODE_DISABLED, 0);

  uart_handle->ops->deinit_transport_pins(uart_handle->uart);
}

SL_WEAK void sli_uart_init_hwfc_pins(sl_uart_handle_t *uart_handle, sl_uart_pin_config_t pin_config)
{
  sl_hal_gpio_set_pin_mode(&pin_config.cts, SL_GPIO_MODE_INPUT, 1);
  sl_hal_gpio_set_pin_mode(&pin_config.rts, SL_GPIO_MODE_PUSH_PULL, 0);

  uart_handle->ops->init_hwfc_pins(uart_handle->uart, pin_config);
}

SL_WEAK void sli_uart_deinit_hwfc_pins(sl_uart_handle_t *uart_handle)
{
  sl_uart_pin_config_t pin_config = uart_handle->pin_config;

  sl_hal_gpio_set_pin_mode(&pin_config.cts, SL_GPIO_MODE_DISABLED, 0);
  sl_hal_gpio_set_pin_mode(&pin_config.rts, SL_GPIO_MODE_DISABLED, 0);

  uart_handle->ops->deinit_hwfc_pins(uart_handle->uart);
}

SL_WEAK void sli_uart_init_clocks(sl_uart_handle_t *uart_handle)
{
  SLI_UART_ASSERT_VALID_HANDLE(uart_handle);

  sl_clock_manager_enable_bus_clock(SL_BUS_CLOCK_GPIO);

  sl_bus_clock_t uart_clock = sl_device_peripheral_get_bus_clock(uart_handle->uart);
  sl_clock_manager_enable_bus_clock(uart_clock);
}

SL_WEAK void sli_uart_deinit_clocks(sl_uart_handle_t *uart_handle)
{
  sl_bus_clock_t uart_clock = sl_device_peripheral_get_bus_clock(uart_handle->uart);
  sl_clock_manager_disable_bus_clock(uart_clock);
}

void sli_uart_reset(sl_uart_handle_t *uart_handle)
{
  EFM_ASSERT(uart_handle != NULL);
  EFM_ASSERT(uart_handle->uart != NULL);

  uart_handle->ops->reset(uart_handle->uart);
}

/***************************************************************************//**
 * Enables NVIC RX and TX interrupts for the given UART peripheral.
 ******************************************************************************/
SL_WEAK void sli_uart_init_irq(sl_uart_handle_t *uart_handle)
{
  CORE_DECLARE_IRQ_STATE;
  sl_peripheral_t uart = uart_handle->uart;

  CORE_ENTER_ATOMIC();
  IRQn_Type rx_irq = (IRQn_Type)sl_device_peripheral_get_serial_rx_irqn(uart);
  sl_interrupt_manager_clear_irq_pending(rx_irq);
  sl_interrupt_manager_enable_irq(rx_irq);

  IRQn_Type tx_irq = (IRQn_Type)sl_device_peripheral_get_serial_tx_irqn(uart);
  sl_interrupt_manager_clear_irq_pending(tx_irq);
  sl_interrupt_manager_enable_irq(tx_irq);
  CORE_EXIT_ATOMIC();
}

/***************************************************************************//**
 * Disables NVIC RX and TX interrupts for the given UART peripheral.
 ******************************************************************************/
SL_WEAK void sli_uart_deinit_irq(sl_peripheral_t uart)
{
  CORE_DECLARE_IRQ_STATE;
  CORE_ENTER_ATOMIC();
  IRQn_Type rx_irq = (IRQn_Type)sl_device_peripheral_get_serial_rx_irqn(uart);
  IRQn_Type tx_irq = (IRQn_Type)sl_device_peripheral_get_serial_tx_irqn(uart);

  sl_interrupt_manager_disable_irq(rx_irq);
  sl_interrupt_manager_disable_irq(tx_irq);
  CORE_EXIT_ATOMIC();
}

/***************************************************************************//**
 * Handles the RX interrupt for the given UART instance.
 ******************************************************************************/
void sli_uart_rx_irq_handler(sl_uart_handle_t *uart_handle)
{
  const sli_uart_ops_t *ops;
  sl_peripheral_t uart;
  uint32_t irq_status;
  uint32_t errors;

  SLI_UART_ASSERT_VALID_HANDLE(uart_handle);

  ops = uart_handle->ops;
  uart = uart_handle->uart;
  irq_status = ops->get_enabled_pending_irq(uart) & (ops->irq_rx_err_flag | ops->irq_rx_ready_flag);
  errors = irq_status & ops->irq_rx_err_flag;

  if (errors && SLI_UART_HANDLE_IS_ASYNC(uart_handle)) {
    sl_uart_async_disable_rx(uart_handle);
  }

  if (errors && uart_handle->rx_err_cb) {
    uart_handle->rx_err_cb(uart_handle,
                           ops->rx_err_from_irq_status(errors),
                           uart_handle->rx_err_cb_arg);
  }

  if ((irq_status & ops->irq_rx_ready_flag) && uart_handle->rx_ready_cb) {
    uart_handle->rx_ready_cb(uart_handle, uart_handle->rx_ready_cb_arg);
  }

  ops->clear_irq(uart, irq_status);
}

/***************************************************************************//**
 * Handles the TX interrupt for the given UART instance.
 ******************************************************************************/
void sli_uart_tx_irq_handler(sl_uart_handle_t *uart_handle)
{
  const sli_uart_ops_t *ops;
  sl_peripheral_t uart;
  uint32_t irq_status;

  SLI_UART_ASSERT_VALID_HANDLE(uart_handle);

  ops = uart_handle->ops;
  uart = uart_handle->uart;
  irq_status = ops->get_enabled_pending_irq(uart) & (ops->irq_tx_complete_flag | ops->irq_tx_ready_flag);

  if ((irq_status & ops->irq_tx_complete_flag) && SLI_UART_HANDLE_IS_ASYNC(uart_handle)) {
    sli_uart_async_transmit_complete(uart_handle);
  }

  if ((irq_status & ops->irq_tx_complete_flag) && uart_handle->tx_complete_cb) {
    uart_handle->tx_complete_cb(uart_handle, uart_handle->tx_complete_cb_arg);
  }

  if ((irq_status & ops->irq_tx_ready_flag) && uart_handle->tx_ready_cb) {
    uart_handle->tx_ready_cb(uart_handle, uart_handle->tx_ready_cb_arg);
  }

  ops->clear_irq(uart, irq_status);
}
