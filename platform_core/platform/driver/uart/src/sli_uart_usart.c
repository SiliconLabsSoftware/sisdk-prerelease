/***************************************************************************//**
 * @file
 * @brief USART operations for UART Driver
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

#include "sl_clock_manager.h"
#include "sl_hal_usart.h"
#include "sl_log_helper.h"

#include "sli_uart.h"

/*******************************************************************************
 *******************************   DEFINES   ***********************************
 ******************************************************************************/

/*******************************************************************************
 ***************************   LOCAL FUNCTIONS   *******************************
 ******************************************************************************/

/***************************************************************************//**
 * Converts the UART data bits to the HAL data bits.
 ******************************************************************************/
static inline sl_hal_usart_data_bits_t uart_data_bits_to_usart_hal_data_bits(sl_uart_data_bits_t data_bits)
{
  switch (data_bits) {
    case SL_UART_DATA_BITS_7:
      return SL_HAL_USART_DATA_BITS_7;
    case SL_UART_DATA_BITS_8:
      return SL_HAL_USART_DATA_BITS_8;
    case SL_UART_DATA_BITS_9:
      return SL_HAL_USART_DATA_BITS_9;
    case SL_UART_DATA_BITS_10:
      return SL_HAL_USART_DATA_BITS_10;
    case SL_UART_DATA_BITS_11:
      return SL_HAL_USART_DATA_BITS_11;
    case SL_UART_DATA_BITS_12:
      return SL_HAL_USART_DATA_BITS_12;
    case SL_UART_DATA_BITS_13:
      return SL_HAL_USART_DATA_BITS_13;
    case SL_UART_DATA_BITS_14:
      return SL_HAL_USART_DATA_BITS_14;
    case SL_UART_DATA_BITS_15:
      return SL_HAL_USART_DATA_BITS_15;
    case SL_UART_DATA_BITS_16:
      return SL_HAL_USART_DATA_BITS_16;
    default:
      SL_LOG_DEBUG_ASSERT(false);
      return SL_HAL_USART_DATA_BITS_8;
  }
}

/***************************************************************************//**
 * Converts the UART parity to the HAL parity.
 ******************************************************************************/
static inline sl_hal_usart_parity_t uart_parity_to_usart_hal_parity(sl_uart_parity_t parity)
{
  switch (parity) {
    case SL_UART_PARITY_NONE:
      return SL_HAL_USART_NO_PARITY;
    case SL_UART_PARITY_ODD:
      return SL_HAL_USART_ODD_PARITY;
    case SL_UART_PARITY_EVEN:
      return SL_HAL_USART_EVEN_PARITY;
    default:
      SL_LOG_DEBUG_ASSERT(false);
      return SL_HAL_USART_NO_PARITY;
  }
}

/***************************************************************************//**
 * Converts the UART stop bits to the HAL stop bits.
 ******************************************************************************/
static inline sl_hal_usart_stop_bits_t uart_stop_bits_to_usart_hal_stop_bits(sl_uart_stop_bits_t stop_bits)
{
  switch (stop_bits) {
    case SL_UART_STOP_BITS_0_5:
      return SL_HAL_USART_STOP_BITS_0P5;
    case SL_UART_STOP_BITS_1:
      return SL_HAL_USART_STOP_BITS_1;
    case SL_UART_STOP_BITS_1_5:
      return SL_HAL_USART_STOP_BITS_1P5;
    case SL_UART_STOP_BITS_2:
      return SL_HAL_USART_STOP_BITS_2;
    default:
      SL_LOG_DEBUG_ASSERT(false);
      return SL_HAL_USART_STOP_BITS_1;
  }
}

/***************************************************************************//**
 * Converts the UART hardware flow control to the HAL hardware flow control.
 ******************************************************************************/
static inline sl_hal_usart_hw_flow_control_t uart_hwfc_to_usart_hal_hwfc(sl_uart_flow_control_t flow_control)
{
  switch (flow_control) {
    case SL_UART_FLOW_CONTROL_CTS_RTS:
      return SL_HAL_USART_HW_FLOW_CONTROL_CTS_RTS;
    case SL_UART_FLOW_CONTROL_SOFT:
    case SL_UART_FLOW_CONTROL_NONE:
      return SL_HAL_USART_HW_FLOW_CONTROL_NONE;
    default:
      SL_LOG_DEBUG_ASSERT(false);
      return SL_HAL_USART_HW_FLOW_CONTROL_NONE;
  }
}

/***************************************************************************//**
 * Initializes the USART pins.
 ******************************************************************************/
static void usart_uart_init_transport_pins(sl_peripheral_t uart, sl_uart_pin_config_t pin_config)
{
  USART_TypeDef *usart = sl_device_peripheral_usart_get_base_addr(uart);

  GPIO->USARTROUTE[USART_NUM(usart)].TXROUTE = (pin_config.tx.port << _GPIO_USART_TXROUTE_PORT_SHIFT)
                                               | (pin_config.tx.pin << _GPIO_USART_TXROUTE_PIN_SHIFT);
  GPIO->USARTROUTE[USART_NUM(usart)].RXROUTE = (pin_config.rx.port << _GPIO_USART_RXROUTE_PORT_SHIFT)
                                               | (pin_config.rx.pin << _GPIO_USART_RXROUTE_PIN_SHIFT);
  GPIO->USARTROUTE[USART_NUM(usart)].ROUTEEN |= GPIO_USART_ROUTEEN_TXPEN | GPIO_USART_ROUTEEN_RXPEN;
}

/***************************************************************************//**
 * De-initializes the USART transport pins.
 ******************************************************************************/
static void usart_uart_deinit_transport_pins(sl_peripheral_t uart)
{
  USART_TypeDef *usart = sl_device_peripheral_usart_get_base_addr(uart);

  GPIO->USARTROUTE[USART_NUM(usart)].TXROUTE = _GPIO_USART_TXROUTE_RESETVALUE;
  GPIO->USARTROUTE[USART_NUM(usart)].RXROUTE = _GPIO_USART_RXROUTE_RESETVALUE;
  GPIO->USARTROUTE[USART_NUM(usart)].ROUTEEN = _GPIO_USART_ROUTEEN_RESETVALUE;
}

/***************************************************************************//**
 * Initializes the USART hardware flow control pins.
 ******************************************************************************/
static void usart_uart_init_hwfc_pins(sl_peripheral_t uart, sl_uart_pin_config_t pin_config)
{
  USART_TypeDef *usart = sl_device_peripheral_usart_get_base_addr(uart);

  GPIO->USARTROUTE[USART_NUM(usart)].CTSROUTE = (pin_config.cts.port << _GPIO_USART_CTSROUTE_PORT_SHIFT)
                                                | (pin_config.cts.pin << _GPIO_USART_CTSROUTE_PIN_SHIFT);
  GPIO->USARTROUTE[USART_NUM(usart)].RTSROUTE = (pin_config.rts.port << _GPIO_USART_RTSROUTE_PORT_SHIFT)
                                                | (pin_config.rts.pin << _GPIO_USART_RTSROUTE_PIN_SHIFT);
  GPIO->USARTROUTE[USART_NUM(usart)].ROUTEEN |= GPIO_USART_ROUTEEN_RTSPEN;
}

/***************************************************************************//**
 * De-initializes the USART hardware flow control pins.
 ******************************************************************************/
static void usart_uart_deinit_hwfc_pins(sl_peripheral_t uart)
{
  USART_TypeDef *usart = sl_device_peripheral_usart_get_base_addr(uart);

  GPIO->USARTROUTE[USART_NUM(usart)].CTSROUTE = _GPIO_USART_CTSROUTE_RESETVALUE;
  GPIO->USARTROUTE[USART_NUM(usart)].RTSROUTE = _GPIO_USART_RTSROUTE_RESETVALUE;
  GPIO->USARTROUTE[USART_NUM(usart)].ROUTEEN &= ~GPIO_USART_ROUTEEN_RTSPEN;
}

/***************************************************************************//**
 * Resets the USART.
 ******************************************************************************/
static void usart_uart_reset(sl_peripheral_t uart)
{
  USART_TypeDef *usart = sl_device_peripheral_usart_get_base_addr(uart);
  sl_hal_usart_reset(usart);
}

/***************************************************************************//**
 * Initializes the USART when used with the high frequency clock.
 ******************************************************************************/
static sl_status_t usart_uart_init(sl_peripheral_t uart, sl_uart_config_t config)
{
  sl_clock_branch_t clock_branch = sl_device_peripheral_get_clock_branch(uart);
  USART_TypeDef *usart = sl_device_peripheral_usart_get_base_addr(uart);
  sl_status_t status;
  uint32_t freq;

  status = sl_clock_manager_get_clock_branch_frequency(clock_branch, &freq);
  if (status != SL_STATUS_OK) {
    return status;
  }

  sl_hal_usart_async_init_t init = SL_HAL_USART_INIT_ASYNC_DEFAULT;
  init.data_bits = uart_data_bits_to_usart_hal_data_bits(config.data_bits);
  init.parity = uart_parity_to_usart_hal_parity(config.parity);
  init.stop_bits = uart_stop_bits_to_usart_hal_stop_bits(config.stop_bits);
  init.hw_flow_control = uart_hwfc_to_usart_hal_hwfc(config.flow_control);
  init.clock_div = sl_hal_usart_async_calculate_clock_div(freq, config.baudrate, init.oversampling);
  /**
   * USART has a two-slot TX FIFO, and TXBL is used to indicate how many slots are used. The threshold
   * can be configured using TXBIL, which can be set to either EMPTY, meaning there are now bytes in
   * the FIFO, or HALFFULL, meaning there is one byte in the FIFO. Set TXBIL to HALFFULL so that
   * we are notified when there is at least one slot in the FIFO, otherwise we waste a slot.
   */
  init.tx_buffer_interrupt_level = SL_HAL_USART_TX_BUFFER_INTERRUPT_LEVEL_HALFFULL;

  uint32_t enabled_interrupts = sl_hal_usart_get_enabled_interrupts(usart);

  sl_hal_usart_reset(usart);

  sl_hal_usart_init_async(usart, &init);
  sl_hal_usart_enable(usart);
  sl_hal_usart_enable_tx(usart);
  sl_hal_usart_enable_rx(usart);

  if (enabled_interrupts != 0U) {
    sl_hal_usart_enable_interrupts(usart, enabled_interrupts);
  }

  return SL_STATUS_OK;
}

static void usart_uart_deinit(sl_peripheral_t uart)
{
  USART_TypeDef *usart = sl_device_peripheral_usart_get_base_addr(uart);
  sl_hal_usart_reset(usart);
}

/***************************************************************************//**
 * Checks if data is availble to be read in the USART RX FIFO.
 ******************************************************************************/
static inline bool usart_uart_is_rx_data_available(sl_peripheral_t uart)
{
  USART_TypeDef *usart = sl_device_peripheral_usart_get_base_addr(uart);
  return (sl_hal_usart_get_status(usart) & USART_STATUS_RXDATAV) != 0;
}

/***************************************************************************//**
 * Clear the specified interrupt from the USART peripheral.
 ******************************************************************************/
static void usart_uart_clear_irq(sl_peripheral_t uart, uint32_t irq)
{
  USART_TypeDef *usart = sl_device_peripheral_usart_get_base_addr(uart);

  sl_hal_usart_clear_interrupts(usart, irq);
}

/***************************************************************************//**
 * Enables or disables USART interrupt flags.
 ******************************************************************************/
static void usart_uart_set_enable_irq(sl_peripheral_t uart, bool enabled, uint32_t irq)
{
  USART_TypeDef *usart = sl_device_peripheral_usart_get_base_addr(uart);

  if (enabled) {
    sl_hal_usart_enable_interrupts(usart, irq);
  } else {
    sl_hal_usart_disable_interrupts(usart, irq);
  }
}

/***************************************************************************//**
 * Reads a single character on USART.
 ******************************************************************************/
static sl_status_t usart_uart_read_byte(sl_peripheral_t uart, uint8_t *byte)
{
  USART_TypeDef *usart = sl_device_peripheral_usart_get_base_addr(uart);

  if (!usart_uart_is_rx_data_available(uart)) {
    return SL_STATUS_EMPTY;
  }

  *byte = (uint8_t)sl_hal_usart_rx(usart);
  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Reads up to @p size bytes from the USART RX FIFO.
 *
 * @return SL_STATUS_OK if one or more bytes were read,
 *         SL_STATUS_EMPTY if no data was available to read.
 ******************************************************************************/
static sl_status_t usart_uart_read_buffer(sl_peripheral_t uart,
                                          uint8_t *data,
                                          size_t size,
                                          size_t *bytes_read)
{
  sl_status_t status = SL_STATUS_OK;
  size_t i;

  for (i = 0; i < size; i++) {
    status = usart_uart_read_byte(uart, &data[i]);
    if (status != SL_STATUS_OK) {
      status = (i == 0) ? SL_STATUS_EMPTY : SL_STATUS_OK;
      break;
    }
  }

  *bytes_read = i;
  return status;
}

/***************************************************************************//**
 * Writes a single character on USART.
 ******************************************************************************/
static sl_status_t usart_uart_write_byte(sl_peripheral_t uart, uint8_t byte)
{
  USART_TypeDef *usart = sl_device_peripheral_usart_get_base_addr(uart);

  if ((sl_hal_usart_get_status(usart) & USART_STATUS_TXBL) == 0U) {
    return SL_STATUS_FULL;
  }

  sl_hal_usart_tx(usart, byte);

  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Writes up to @p size bytes to the USART TX FIFO.
 *
 * @return SL_STATUS_OK if all @p size bytes were written,
 *         SL_STATUS_FULL if the TX FIFO became full before the buffer was exhausted.
 ******************************************************************************/
static sl_status_t usart_uart_write_buffer(sl_peripheral_t uart,
                                           const uint8_t *data,
                                           size_t size,
                                           size_t *bytes_written)
{
  sl_status_t status = SL_STATUS_OK;
  size_t i;

  for (i = 0; i < size; i++) {
    status = usart_uart_write_byte(uart, data[i]);
    if (status != SL_STATUS_OK) {
      *bytes_written = i;
      return status;
    }
  }

  *bytes_written = i;
  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Converts the USART interrupt status to the RX errors.
 ******************************************************************************/
static sl_uart_rx_err_t usart_uart_rx_err_from_irq_status(uint32_t irq_status)
{
  return ((irq_status & USART_IF_FERR) ? SL_UART_RX_ERR_FRAMING : 0u)
         | ((irq_status & USART_IF_PERR) ? SL_UART_RX_ERR_PARITY : 0u)
         | ((irq_status & USART_IF_RXOF) ? SL_UART_RX_ERR_OVERFLOW : 0u);
}

/***************************************************************************//**
 * Returns enabled and pending USART interrupt flags.
 ******************************************************************************/
static uint32_t usart_uart_get_enabled_pending_irq(sl_peripheral_t uart)
{
  USART_TypeDef *usart = sl_device_peripheral_usart_get_base_addr(uart);

  return sl_hal_usart_get_enabled_pending_interrupts(usart);
}

/***************************************************************************//**
 * Returns enabled USART interrupt flags.
 ******************************************************************************/
static uint32_t usart_uart_get_enabled_irq(sl_peripheral_t uart)
{
  USART_TypeDef *usart = sl_device_peripheral_usart_get_base_addr(uart);

  return sl_hal_usart_get_enabled_interrupts(usart);
}

/***************************************************************************//**
 * Returns if the USART is idle.
 *
 * USART does not allow checking if the RX shift register is empty, so there is
 * a small window where the peripheral will be deemed idle even though data is
 * still being received.
 ******************************************************************************/
static bool usart_uart_is_idle(sl_peripheral_t uart)
{
  USART_TypeDef *usart = sl_device_peripheral_usart_get_base_addr(uart);
  uint32_t status = sl_hal_usart_get_status(usart);

  return !(status & USART_STATUS_RXDATAV)
         && !(status & _USART_STATUS_TXBUFCNT_MASK)
         && status & USART_STATUS_TXIDLE;
}

#if defined(SL_CATALOG_POWER_MANAGER_PRESENT)

/***************************************************************************//**
 * Returns the EM requirement for the USART.
 ******************************************************************************/
static sl_power_manager_em_t usart_uart_get_em_requirement(sl_peripheral_t uart)
{
  (void) uart;

  return SL_POWER_MANAGER_EM1;
}

#endif

#if defined(SL_CATALOG_UART_ASYNC_PRESENT)

/***************************************************************************//**
 * Returns the RX register for the given UART instance.
 ******************************************************************************/
static void *usart_uart_get_rx_register(sl_peripheral_t uart)
{
  USART_TypeDef *usart = sl_device_peripheral_usart_get_base_addr(uart);
  return (void *)&usart->RXDATA;
}
/***************************************************************************//**
 * Returns the TX register for the given UART instance.
 ******************************************************************************/
static void *usart_uart_get_tx_register(sl_peripheral_t uart)
{
  USART_TypeDef *usart = sl_device_peripheral_usart_get_base_addr(uart);
  return (void *)&usart->TXDATA;
}

/***************************************************************************//**
 * Enables or disables USART transmitter.
 ******************************************************************************/
static void usart_uart_set_tx_enable(sl_peripheral_t uart, bool en)
{
  USART_TypeDef *usart = sl_device_peripheral_usart_get_base_addr(uart);

  if (en) {
    sl_hal_usart_enable_tx(usart);
  } else {
    sl_hal_usart_disable_tx(usart);
  }
}

/***************************************************************************//**
 * Clears the USART TX FIFO and returns the number of bytes cleared.
 *
 * @note The transmitter must have been disabled with @ref usart_uart_set_tx_enable
 *       prior to calling.
 ******************************************************************************/
static size_t usart_uart_clear_tx_fifo(sl_peripheral_t uart)
{
  USART_TypeDef *usart = sl_device_peripheral_usart_get_base_addr(uart);

  size_t count = (sl_hal_usart_get_status(usart) & _USART_STATUS_TXBUFCNT_MASK) >> _USART_STATUS_TXBUFCNT_SHIFT;

  sl_hal_usart_clear_tx(usart);

  return count;
}

/***************************************************************************//**
 * Attempts to set the RX timeout for USART.
 * Returns the actual timeout set in microseconds.
 ******************************************************************************/
static uint32_t usart_uart_set_rx_timeout(sl_uart_handle_t *uart_handle, uint32_t timeout_us)
{
  USART_TypeDef *usart = sl_device_peripheral_usart_get_base_addr(uart_handle->uart);

  // Use the TCMP register to setup the RX timeout. As per the reference manual, this
  // can be implemented by setting up TMCP to:
  // TSTART = RXEOF
  // TSTOP = RXACT
  // TCMPVAL = timeout in baud times
  // With this configuration, a timeout interrupt will get triggered every time there is an idle
  // period greater than TCMPVAL baud times between two frames.

  // The maximum timeout value is 255 baud times. Find the closest value to the specified
  // timeout and set the timeout register.
  uint32_t baudrate = uart_handle->config.baudrate;
  uint32_t timeout_baud_time = SL_MIN(SL_DIV_ROUND_UP((timeout_us * baudrate), 1000000UL),
                                      _USART_TIMECMP0_TCMPVAL_MASK);

  usart->TIMECMP0 = USART_TIMECMP0_TSTART_RXEOF
                    | USART_TIMECMP0_TSTOP_RXACT
                    | (timeout_baud_time << _USART_TIMECMP0_TCMPVAL_SHIFT);

  return (timeout_baud_time * 1000000UL) / baudrate;
}

#endif // SL_CATALOG_UART_ASYNC_PRESENT

const sli_uart_ops_t sli_uart_usart_ops = {
  .reset = usart_uart_reset,
  .init = usart_uart_init,
  .deinit = usart_uart_deinit,
  .init_transport_pins = usart_uart_init_transport_pins,
  .deinit_transport_pins = usart_uart_deinit_transport_pins,
  .init_hwfc_pins = usart_uart_init_hwfc_pins,
  .deinit_hwfc_pins = usart_uart_deinit_hwfc_pins,
  .read_byte = usart_uart_read_byte,
  .read_buffer = usart_uart_read_buffer,
  .write_byte = usart_uart_write_byte,
  .write_buffer = usart_uart_write_buffer,
  .set_enable_irq = usart_uart_set_enable_irq,
  .clear_irq = usart_uart_clear_irq,
  .get_enabled_pending_irq = usart_uart_get_enabled_pending_irq,
  .get_enabled_irq = usart_uart_get_enabled_irq,
  .rx_err_from_irq_status = usart_uart_rx_err_from_irq_status,
  .is_idle = usart_uart_is_idle,
#if defined(SL_CATALOG_POWER_MANAGER_PRESENT)
  .get_em_requirement = usart_uart_get_em_requirement,
#endif
#if defined(SL_CATALOG_UART_ASYNC_PRESENT)
  .get_rx_register = usart_uart_get_rx_register,
  .get_tx_register = usart_uart_get_tx_register,
  .clear_tx_fifo = usart_uart_clear_tx_fifo,
  .set_tx_enable = usart_uart_set_tx_enable,
  .set_rx_timeout = usart_uart_set_rx_timeout,
  .irq_rx_timeout_flag = USART_IF_TCMP0,
#endif
  .irq_rx_err_flag = USART_IF_FERR | USART_IF_PERR | USART_IF_RXOF,
  .irq_rx_ready_flag = USART_IF_RXDATAV,
  .irq_tx_ready_flag = USART_IF_TXBL,
  .irq_tx_complete_flag = USART_IF_TXC,
};
