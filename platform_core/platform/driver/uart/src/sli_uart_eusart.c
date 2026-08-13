/***************************************************************************//**
 * @file
 * @brief EUSART operations for UART Driver
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
#include "sl_hal_eusart.h"
#include "sl_log_helper.h"

#include "sli_uart.h"

/*******************************************************************************
 *******************************   DEFINES   ***********************************
 ******************************************************************************/

/*******************************************************************************
 ***************************   LOCAL FUNCTIONS   *******************************
 ******************************************************************************/

/***************************************************************************//**
 * Converts the UART flow control to the HAL flow control.
 ******************************************************************************/
static inline sl_hal_eusart_hw_flow_control_t uart_hwfc_to_eusart_hal_hwfc(sl_uart_flow_control_t flow_control)
{
  switch (flow_control) {
    case SL_UART_FLOW_CONTROL_CTS_RTS:
      return SL_HAL_EUSART_HW_FLOW_CONTROL_CTS_RTS;
    case SL_UART_FLOW_CONTROL_NONE:
    case SL_UART_FLOW_CONTROL_SOFT:
      return SL_HAL_EUSART_HW_FLOW_CONTROL_NONE;
    default:
      SL_LOG_DEBUG_ASSERT(false);
      return SL_HAL_EUSART_HW_FLOW_CONTROL_NONE;
  }
}

/***************************************************************************//**
 * Converts the UART parity to the HAL parity.
 ******************************************************************************/
static inline sl_hal_eusart_parity_t uart_parity_to_eusart_hal_parity(sl_uart_parity_t parity)
{
  switch (parity) {
    case SL_UART_PARITY_NONE:
      return SL_HAL_EUSART_NO_PARITY;
    case SL_UART_PARITY_ODD:
      return SL_HAL_EUSART_ODD_PARITY;
    case SL_UART_PARITY_EVEN:
      return SL_HAL_EUSART_EVEN_PARITY;
    default:
      SL_LOG_DEBUG_ASSERT(false);
      return SL_HAL_EUSART_NO_PARITY;
  }
}

/***************************************************************************//**
 * Converts the UART stop bits to the HAL stop bits.
 ******************************************************************************/
static inline sl_hal_eusart_stop_bits_t uart_stop_bits_to_eusart_hal_stop_bits(sl_uart_stop_bits_t stop_bits)
{
  switch (stop_bits) {
    case SL_UART_STOP_BITS_0_5:
      return SL_HAL_EUSART_STOP_BITS_0P5;
    case SL_UART_STOP_BITS_1:
      return SL_HAL_EUSART_STOP_BITS_1;
    case SL_UART_STOP_BITS_1_5:
      return SL_HAL_EUSART_STOP_BITS_1P5;
    case SL_UART_STOP_BITS_2:
      return SL_HAL_EUSART_STOP_BITS_2;
    default:
      SL_LOG_DEBUG_ASSERT(false);
      return SL_HAL_EUSART_STOP_BITS_1;
  }
}

/***************************************************************************//**
 * Converts the UART data bits to the HAL data bits.
 ******************************************************************************/
static inline sl_hal_eusart_data_bits_t uart_data_bits_to_eusart_hal_data_bits(sl_uart_data_bits_t data_bits)
{
  switch (data_bits) {
    case SL_UART_DATA_BITS_7:
      return SL_HAL_EUSART_DATA_BITS_7;
    case SL_UART_DATA_BITS_8:
      return SL_HAL_EUSART_DATA_BITS_8;
    case SL_UART_DATA_BITS_9:
      return SL_HAL_EUSART_DATA_BITS_9;
    #if defined(_EUSART_CFG0_SYNC_MASK)
    case SL_UART_DATA_BITS_10:
      return SL_HAL_EUSART_DATA_BITS_10;
    case SL_UART_DATA_BITS_11:
      return SL_HAL_EUSART_DATA_BITS_11;
    case SL_UART_DATA_BITS_12:
      return SL_HAL_EUSART_DATA_BITS_12;
    case SL_UART_DATA_BITS_13:
      return SL_HAL_EUSART_DATA_BITS_13;
    case SL_UART_DATA_BITS_14:
      return SL_HAL_EUSART_DATA_BITS_14;
    case SL_UART_DATA_BITS_15:
      return SL_HAL_EUSART_DATA_BITS_15;
    case SL_UART_DATA_BITS_16:
      return SL_HAL_EUSART_DATA_BITS_16;
    #else
    case SL_UART_DATA_BITS_10:
    case SL_UART_DATA_BITS_11:
    case SL_UART_DATA_BITS_12:
    case SL_UART_DATA_BITS_13:
    case SL_UART_DATA_BITS_14:
    case SL_UART_DATA_BITS_15:
    case SL_UART_DATA_BITS_16:
    #endif
    default:
      SL_LOG_DEBUG_ASSERT(false);
      return SL_HAL_EUSART_DATA_BITS_8;
  }
}

#if defined(EUSART_PRESENT)

/***************************************************************************//**
 * Initializes the EUSART pins.
 ******************************************************************************/
static void eusart_uart_init_transport_pins(sl_peripheral_t uart, sl_uart_pin_config_t pin_config)
{
  EUSART_TypeDef *eusart = sl_device_peripheral_eusart_get_base_addr(uart);

  GPIO->EUSARTROUTE[EUSART_NUM(eusart)].TXROUTE = (pin_config.tx.port << _GPIO_EUSART_TXROUTE_PORT_SHIFT)
                                                  | (pin_config.tx.pin << _GPIO_EUSART_TXROUTE_PIN_SHIFT);
  GPIO->EUSARTROUTE[EUSART_NUM(eusart)].RXROUTE = (pin_config.rx.port << _GPIO_EUSART_RXROUTE_PORT_SHIFT)
                                                  | (pin_config.rx.pin << _GPIO_EUSART_RXROUTE_PIN_SHIFT);
  GPIO->EUSARTROUTE[EUSART_NUM(eusart)].ROUTEEN |= GPIO_EUSART_ROUTEEN_TXPEN | GPIO_EUSART_ROUTEEN_RXPEN;
}

/***************************************************************************//**
 * De-initializes the EUSART transport pins.
 ******************************************************************************/
static void eusart_uart_deinit_transport_pins(sl_peripheral_t uart)
{
  EUSART_TypeDef *eusart = sl_device_peripheral_eusart_get_base_addr(uart);

  GPIO->EUSARTROUTE[EUSART_NUM(eusart)].TXROUTE = _GPIO_EUSART_TXROUTE_RESETVALUE;
  GPIO->EUSARTROUTE[EUSART_NUM(eusart)].RXROUTE = _GPIO_EUSART_RXROUTE_RESETVALUE;
  GPIO->EUSARTROUTE[EUSART_NUM(eusart)].ROUTEEN = _GPIO_EUSART_ROUTEEN_RESETVALUE;
}

/***************************************************************************//**
 * Initializes the EUSART hardware flow control pins.
 ******************************************************************************/
static void eusart_uart_init_hwfc_pins(sl_peripheral_t uart, sl_uart_pin_config_t pin_config)
{
  EUSART_TypeDef *eusart = sl_device_peripheral_eusart_get_base_addr(uart);

  GPIO->EUSARTROUTE[EUSART_NUM(eusart)].CTSROUTE = (pin_config.cts.port << _GPIO_EUSART_CTSROUTE_PORT_SHIFT)
                                                   | (pin_config.cts.pin << _GPIO_EUSART_CTSROUTE_PIN_SHIFT);
  GPIO->EUSARTROUTE[EUSART_NUM(eusart)].RTSROUTE = (pin_config.rts.port << _GPIO_EUSART_RTSROUTE_PORT_SHIFT)
                                                   | (pin_config.rts.pin << _GPIO_EUSART_RTSROUTE_PIN_SHIFT);
  GPIO->EUSARTROUTE[EUSART_NUM(eusart)].ROUTEEN |= GPIO_EUSART_ROUTEEN_RTSPEN;
}

/***************************************************************************//**
 * De-initializes the EUSART hardware flow control pins.
 ******************************************************************************/
static void eusart_uart_deinit_hwfc_pins(sl_peripheral_t uart)
{
  EUSART_TypeDef *eusart = sl_device_peripheral_eusart_get_base_addr(uart);

  GPIO->EUSARTROUTE[EUSART_NUM(eusart)].CTSROUTE = _GPIO_EUSART_CTSROUTE_RESETVALUE;
  GPIO->EUSARTROUTE[EUSART_NUM(eusart)].RTSROUTE = _GPIO_EUSART_RTSROUTE_RESETVALUE;
  GPIO->EUSARTROUTE[EUSART_NUM(eusart)].ROUTEEN &= ~GPIO_EUSART_ROUTEEN_RTSPEN;
}

/***************************************************************************//**
 * Returns if the EUSART peripheral is in low frequency mode.
 ******************************************************************************/
static inline bool is_lf_mode(EUSART_TypeDef *eusart, uint32_t freq)
{
  uint32_t lf_freq;
  sl_status_t status = sl_clock_manager_get_oscillator_frequency(SL_OSCILLATOR_LFRCO, &lf_freq);
  EFM_ASSERT(status == SL_STATUS_OK);

  return ((freq <= lf_freq) && (eusart == EUSART0));
}

#endif // EUSART_PRESENT

#if defined(EUART_PRESENT)

/***************************************************************************//**
 * Initializes the EUART pins.
 ******************************************************************************/
static void euart_uart_init_transport_pins(sl_peripheral_t uart, sl_uart_pin_config_t pin_config)
{
  (void) uart;

  GPIO->EUARTROUTE->TXROUTE = (pin_config.tx.port << _GPIO_EUART_TXROUTE_PORT_SHIFT)
                              | (pin_config.tx.pin << _GPIO_EUART_TXROUTE_PIN_SHIFT);
  GPIO->EUARTROUTE->ROUTEEN = GPIO_EUART_ROUTEEN_TXPEN;
  GPIO->EUARTROUTE->RXROUTE = (pin_config.rx.port << _GPIO_EUART_RXROUTE_PORT_SHIFT)
                              | (pin_config.rx.pin << _GPIO_EUART_RXROUTE_PIN_SHIFT);
}

/***************************************************************************//**
 * De-initializes the EUART transport pins.
 ******************************************************************************/
static void euart_uart_deinit_transport_pins(sl_peripheral_t uart)
{
  (void) uart;

  GPIO->EUARTROUTE->TXROUTE = _GPIO_EUART_TXROUTE_RESETVALUE;
  GPIO->EUARTROUTE->RXROUTE = _GPIO_EUART_RXROUTE_RESETVALUE;
  GPIO->EUARTROUTE->ROUTEEN = _GPIO_EUART_ROUTEEN_RESETVALUE;
}

/***************************************************************************//**
 * Initializes the EUART hardware flow control pins.
 ******************************************************************************/
static void euart_uart_init_hwfc_pins(sl_peripheral_t uart, sl_uart_pin_config_t pin_config)
{
  (void) uart;

  GPIO->EUARTROUTE->CTSROUTE = (pin_config.cts.port << _GPIO_EUART_CTSROUTE_PORT_SHIFT)
                               | (pin_config.cts.pin << _GPIO_EUART_CTSROUTE_PIN_SHIFT);
  GPIO->EUARTROUTE->RTSROUTE = (pin_config.rts.port << _GPIO_EUART_RTSROUTE_PORT_SHIFT)
                               | (pin_config.rts.pin << _GPIO_EUART_RTSROUTE_PIN_SHIFT);
  GPIO->EUARTROUTE->ROUTEEN |= GPIO_EUART_ROUTEEN_RTSPEN;
}

/***************************************************************************//**
 * De-initializes the EUART hardware flow control pins.
 ******************************************************************************/
static void euart_uart_deinit_hwfc_pins(sl_peripheral_t uart)
{
  (void) uart;

  GPIO->EUARTROUTE->CTSROUTE = _GPIO_EUART_CTSROUTE_RESETVALUE;
  GPIO->EUARTROUTE->RTSROUTE = _GPIO_EUART_RTSROUTE_RESETVALUE;
  GPIO->EUARTROUTE->ROUTEEN &= ~GPIO_EUART_ROUTEEN_RTSPEN;
}

/***************************************************************************//**
 * Returns if the EUART peripheral is in low frequency mode.
 ******************************************************************************/
static inline bool is_lf_mode(EUSART_TypeDef *euart, uint32_t freq)
{
  uint32_t lf_freq;
  (void) euart;

  sl_status_t status = sl_clock_manager_get_oscillator_frequency(SL_OSCILLATOR_LFRCO, &lf_freq);
  EFM_ASSERT(status == SL_STATUS_OK);

  return (freq <= lf_freq);
}

#endif // EUART_PRESENT

/***************************************************************************//**
 * Resets the EUSART.
 ******************************************************************************/
static void eusart_uart_reset(sl_peripheral_t uart)
{
  EUSART_TypeDef *eusart = sl_device_peripheral_eusart_get_base_addr(uart);
  sl_hal_eusart_reset(eusart);
}

/***************************************************************************//**
 * Initializes the EUSART when used with the high frequency clock.
 ******************************************************************************/
static sl_status_t eusart_uart_init(sl_peripheral_t uart, sl_uart_config_t config)
{
  sl_clock_branch_t clock_branch = sl_device_peripheral_get_clock_branch(uart);
  EUSART_TypeDef *eusart = sl_device_peripheral_eusart_get_base_addr(uart);
  uint32_t freq;
  sl_status_t status;

  status = sl_clock_manager_get_clock_branch_frequency(clock_branch, &freq);
  if (status != SL_STATUS_OK) {
    return status;
  }

  sl_hal_eusart_uart_advanced_config_t advanced_config = SL_HAL_EUSART_UART_ADVANCED_INIT_DEFAULT;
  advanced_config.hw_flow_control_mode = uart_hwfc_to_eusart_hal_hwfc(config.flow_control);

  #if defined(EUSART_CFG1_RXTIMEOUT_DEFAULT)
  // EUART does not have the CFG1 register, so we simply cannot set the RX timeout value.
  // Attempting to call sl_uart_async_read_set_timeout on an EUART-based UART instance will return
  // SL_STATUS_NOT_SUPPORTED.
  advanced_config.rx_timeout = _EUSART_CFG1_RXTIMEOUT_SEVENFRAMES;
  #endif

  bool lf_mode = is_lf_mode(eusart, freq);

  sl_hal_eusart_uart_init_t init = lf_mode ? (sl_hal_eusart_uart_init_t)SL_HAL_EUSART_UART_INIT_DEFAULT_LF
                                   : (sl_hal_eusart_uart_init_t)SL_HAL_EUSART_UART_INIT_DEFAULT_HF;

  init.data_bits = uart_data_bits_to_eusart_hal_data_bits(config.data_bits);
  init.parity = uart_parity_to_eusart_hal_parity(config.parity);
  init.stop_bits = uart_stop_bits_to_eusart_hal_stop_bits(config.stop_bits);
  init.clock_div = sl_hal_eusart_uart_calculate_clock_div(freq, config.baudrate, init.oversampling);
  init.advanced_config = &advanced_config;

  uint32_t enabled_interrupts = sl_hal_eusart_get_enabled_interrupts(eusart);

  sl_hal_eusart_reset(eusart);

  if (lf_mode) {
    sl_hal_eusart_init_uart_lf(eusart, &init);
  } else {
    sl_hal_eusart_init_uart_hf(eusart, &init);
  }

  sl_hal_eusart_enable(eusart);
  sl_hal_eusart_enable_rx(eusart);
  sl_hal_eusart_enable_tx(eusart);

  // Restore the previously enabled interrupts.
  sl_hal_eusart_enable_interrupts(eusart, enabled_interrupts);

  return SL_STATUS_OK;
}

/***************************************************************************//**
 * De-initializes the EUSART when used with the high frequency clock.
 ******************************************************************************/
static void eusart_uart_deinit(sl_peripheral_t uart)
{
  EUSART_TypeDef *eusart = sl_device_peripheral_eusart_get_base_addr(uart);
  sl_hal_eusart_reset(eusart);
}

/***************************************************************************//**
 * Checks if data is availble to be read in the EUSART RX FIFO.
 *
 * @note No need to account for RXFL greater than one as this driver does not use it.
 ******************************************************************************/
static inline bool eusart_uart_is_rx_data_available(sl_peripheral_t uart)
{
  EUSART_TypeDef *eusart = sl_device_peripheral_eusart_get_base_addr(uart);
  return (sl_hal_eusart_get_status(eusart) & EUSART_STATUS_RXFL) != 0;
}

/***************************************************************************//**
 * Clear the specified interrupt from the EUSART peripheral.
 ******************************************************************************/
static void eusart_uart_clear_irq(sl_peripheral_t uart, uint32_t irq)
{
  EUSART_TypeDef *eusart = sl_device_peripheral_eusart_get_base_addr(uart);

  sl_hal_eusart_clear_interrupts(eusart, irq);
}

/***************************************************************************//**
 * Enables or disables EUSART interrupt flags.
 ******************************************************************************/
static void eusart_uart_set_enable_irq(sl_peripheral_t uart, bool enabled, uint32_t irq)
{
  EUSART_TypeDef *eusart = sl_device_peripheral_eusart_get_base_addr(uart);

  if (enabled) {
    sl_hal_eusart_enable_interrupts(eusart, irq);
  } else {
    sl_hal_eusart_disable_interrupts(eusart, irq);
  }
}

/***************************************************************************//**
 * Reads a single character on EUSART.
 ******************************************************************************/
static sl_status_t eusart_uart_read_byte(sl_peripheral_t uart, uint8_t *byte)
{
  EUSART_TypeDef *eusart = sl_device_peripheral_eusart_get_base_addr(uart);

  if (!eusart_uart_is_rx_data_available(uart)) {
    return SL_STATUS_EMPTY;
  }

  *byte = (uint8_t)sl_hal_eusart_rx(eusart);
  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Reads up to @p size bytes from the EUSART RX FIFO.
 *
 * @return SL_STATUS_OK if one or more bytes were read,
 *         SL_STATUS_EMPTY if no data was available to read.
 ******************************************************************************/
static sl_status_t eusart_uart_read_buffer(sl_peripheral_t uart,
                                           uint8_t *data,
                                           size_t size,
                                           size_t *bytes_read)
{
  sl_status_t status = SL_STATUS_OK;
  size_t i;

  for (i = 0; i < size; i++) {
    status = eusart_uart_read_byte(uart, &data[i]);
    if (status != SL_STATUS_OK) {
      status = (i == 0) ? SL_STATUS_EMPTY : SL_STATUS_OK;
      break;
    }
  }

  *bytes_read = i;
  return status;
}

/***************************************************************************//**
 * Writes a single character on EUSART.
 ******************************************************************************/
static sl_status_t eusart_uart_write_byte(sl_peripheral_t uart, uint8_t byte)
{
  EUSART_TypeDef *eusart = sl_device_peripheral_eusart_get_base_addr(uart);

  if ((sl_hal_eusart_get_status(eusart) & EUSART_STATUS_TXFL) == 0) {
    return SL_STATUS_FULL;
  }

  sl_hal_eusart_tx(eusart, byte);

  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Writes up to @p size bytes to the EUSART TX FIFO.
 *
 * @return SL_STATUS_OK if all @p size bytes were written,
 *         SL_STATUS_FULL if the TX FIFO became full before the buffer was exhausted.
 ******************************************************************************/
static sl_status_t eusart_uart_write_buffer(sl_peripheral_t uart,
                                            const uint8_t *data,
                                            size_t size,
                                            size_t *bytes_written)
{
  sl_status_t status = SL_STATUS_OK;
  size_t i;

  for (i = 0; i < size; i++) {
    status = eusart_uart_write_byte(uart, data[i]);
    if (status != SL_STATUS_OK) {
      *bytes_written = i;
      return status;
    }
  }

  *bytes_written = i;
  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Converts the EUSART interrupt status to the RX errors.
 ******************************************************************************/
static sl_uart_rx_err_t eusart_uart_rx_err_from_irq_status(uint32_t irq_status)
{
  return ((irq_status & EUSART_IF_FERR) ? SL_UART_RX_ERR_FRAMING : 0u)
         | ((irq_status & EUSART_IF_PERR) ? SL_UART_RX_ERR_PARITY : 0u)
         | ((irq_status & EUSART_IF_RXOF) ? SL_UART_RX_ERR_OVERFLOW : 0u);
}

/***************************************************************************//**
 * Returns enabled and pending EUSART interrupt flags.
 ******************************************************************************/
static uint32_t eusart_uart_get_enabled_pending_irq(sl_peripheral_t uart)
{
  EUSART_TypeDef *eusart = sl_device_peripheral_eusart_get_base_addr(uart);

  return sl_hal_eusart_get_enabled_pending_interrupts(eusart);
}

/***************************************************************************//**
 * Returns enabled EUSART interrupt flags.
 ******************************************************************************/
static uint32_t eusart_uart_get_enabled_irq(sl_peripheral_t uart)
{
  EUSART_TypeDef *eusart = sl_device_peripheral_eusart_get_base_addr(uart);

  return sl_hal_eusart_get_enabled_interrupts(eusart);
}

/***************************************************************************//**
 * Checks that both the shift registers and FIFOs are empty.
 ******************************************************************************/
static bool eusart_uart_is_idle(sl_peripheral_t uart)
{
  EUSART_TypeDef *eusart = sl_device_peripheral_eusart_get_base_addr(uart);
  uint32_t status = sl_hal_eusart_get_status(eusart);

  return !(status & EUSART_STATUS_RXFL)
         && status & EUSART_STATUS_RXIDLE
         && !(status & _EUSART_STATUS_TXFCNT_MASK)
         && status & EUSART_STATUS_TXIDLE;
}

#if defined(SL_CATALOG_POWER_MANAGER_PRESENT)

/***************************************************************************//**
 * Returns the EM requirement for the EUART.
 ******************************************************************************/
static sl_power_manager_em_t eusart_uart_get_em_requirement(sl_peripheral_t uart)
{
  sl_clock_branch_t clock_branch = sl_device_peripheral_get_clock_branch(uart);
  EUSART_TypeDef *eusart = sl_device_peripheral_eusart_get_base_addr(uart);
  sl_status_t status;
  uint32_t freq;

  status = sl_clock_manager_get_clock_branch_frequency(clock_branch, &freq);
  EFM_ASSERT(status == SL_STATUS_OK);

  bool lf = is_lf_mode(eusart, freq);

  return lf ? SL_POWER_MANAGER_EM2 : SL_POWER_MANAGER_EM1;
}

#endif

#if defined(SL_CATALOG_UART_ASYNC_PRESENT)

/***************************************************************************//**
 * Returns the TX register for the given UART instance.
 ******************************************************************************/
static void *eusart_uart_get_tx_register(sl_peripheral_t uart)
{
  EUSART_TypeDef *eusart = sl_device_peripheral_eusart_get_base_addr(uart);
  return (void *)&eusart->TXDATA;
}

/***************************************************************************//**
 * Returns the RX register for the given UART instance.
 ******************************************************************************/
static void *eusart_uart_get_rx_register(sl_peripheral_t uart)
{
  EUSART_TypeDef *eusart = sl_device_peripheral_eusart_get_base_addr(uart);
  return (void *)&eusart->RXDATA;
}

/***************************************************************************//**
 * Enables or disables EUSART transmitter.
 ******************************************************************************/
static void eusart_uart_set_tx_enable(sl_peripheral_t uart, bool en)
{
  EUSART_TypeDef *eusart = sl_device_peripheral_eusart_get_base_addr(uart);

  if (en) {
    sl_hal_eusart_enable_tx(eusart);
  } else {
    sl_hal_eusart_disable_tx(eusart);
  }
}

/***************************************************************************//**
 * Clears the EUSART TX FIFO and returns the number of bytes cleared.
 *
 * @note The transmitter must have been disabled with @ref eusart_uart_set_tx_enable
 *       prior to calling.
 ******************************************************************************/
static size_t eusart_uart_clear_tx_fifo(sl_peripheral_t uart)
{
  EUSART_TypeDef *eusart = sl_device_peripheral_eusart_get_base_addr(uart);

  size_t count = (sl_hal_eusart_get_status(eusart) & _EUSART_STATUS_TXFCNT_MASK) >> _EUSART_STATUS_TXFCNT_SHIFT;

  sl_hal_eusart_clear_tx(eusart);

  return count;
}

#if defined(EUSART_PRESENT)

/***************************************************************************//**
 * Attempts to set the RX timeout for EUSART.
 * Returns the actual timeout set in microseconds.
 ******************************************************************************/
static uint32_t eusart_uart_set_rx_timeout(sl_uart_handle_t *uart_handle, uint32_t timeout_us)
{
  (void)timeout_us;

  uint32_t baudrate = uart_handle->config.baudrate;
  uint32_t frame_size = sl_uart_config_get_frame_size(uart_handle->config);
  uint32_t frame_time_us = (frame_size * 1000000UL) / baudrate;

  // EUSART requires to be disabled when changing the CFGx registers. This means that in order to
  // change the RX timeout value, we would need to disable the EUSART peripheral, potentially risking
  // data loss. To avoid this, we always set the RX timeout to its maximum value of 7 frames, making
  // it the minimum HW timeout. For example, at 115200 8N1, the minimal HW timeout will be
  // 7*10/115200 ~= 608us.
  const uint8_t timeout_frames = _EUSART_CFG1_RXTIMEOUT_SEVENFRAMES;

  return timeout_frames * frame_time_us;
}

#endif // EUSART_PRESENT

#if defined(EUART_PRESENT)

/***************************************************************************//**
 * EUART does not support HW RX timeout.
 ******************************************************************************/
static uint32_t euart_uart_set_rx_timeout(sl_uart_handle_t *uart_handle, uint32_t timeout_us)
{
  (void)uart_handle;
  (void)timeout_us;

  return 0;
}

#endif // EUART_PRESENT
#endif // SL_CATALOG_UART_ASYNC_PRESENT

#if defined(EUSART_PRESENT)

const sli_uart_ops_t sli_uart_eusart_ops = {
  .reset = eusart_uart_reset,
  .init = eusart_uart_init,
  .deinit = eusart_uart_deinit,
  .init_transport_pins = eusart_uart_init_transport_pins,
  .deinit_transport_pins = eusart_uart_deinit_transport_pins,
  .init_hwfc_pins = eusart_uart_init_hwfc_pins,
  .deinit_hwfc_pins = eusart_uart_deinit_hwfc_pins,
  .read_byte = eusart_uart_read_byte,
  .read_buffer = eusart_uart_read_buffer,
  .write_byte = eusart_uart_write_byte,
  .write_buffer = eusart_uart_write_buffer,
  .set_enable_irq = eusart_uart_set_enable_irq,
  .clear_irq = eusart_uart_clear_irq,
  .get_enabled_pending_irq = eusart_uart_get_enabled_pending_irq,
  .get_enabled_irq = eusart_uart_get_enabled_irq,
  .rx_err_from_irq_status = eusart_uart_rx_err_from_irq_status,
  .is_idle = eusart_uart_is_idle,
#if defined(SL_CATALOG_POWER_MANAGER_PRESENT)
  .get_em_requirement = eusart_uart_get_em_requirement,
#endif
#if defined(SL_CATALOG_UART_ASYNC_PRESENT)
  .get_tx_register = eusart_uart_get_tx_register,
  .get_rx_register = eusart_uart_get_rx_register,
  .clear_tx_fifo = eusart_uart_clear_tx_fifo,
  .set_tx_enable = eusart_uart_set_tx_enable,
  .set_rx_timeout = eusart_uart_set_rx_timeout,
  .irq_rx_timeout_flag = EUSART_IF_RXTO,
#endif
  .irq_rx_err_flag = EUSART_IF_FERR | EUSART_IF_PERR | EUSART_IF_RXOF,
  .irq_rx_ready_flag = EUSART_IF_RXFL,
  .irq_tx_ready_flag = EUSART_IF_TXFL,
  .irq_tx_complete_flag = EUSART_IF_TXC,
};

#elif defined(EUART_PRESENT)

const sli_uart_ops_t sli_uart_euart_ops = {
  .reset = eusart_uart_reset,
  .init = eusart_uart_init,
  .deinit = eusart_uart_deinit,
  .init_transport_pins = euart_uart_init_transport_pins,
  .deinit_transport_pins = euart_uart_deinit_transport_pins,
  .init_hwfc_pins = euart_uart_init_hwfc_pins,
  .deinit_hwfc_pins = euart_uart_deinit_hwfc_pins,
  .read_byte = eusart_uart_read_byte,
  .read_buffer = eusart_uart_read_buffer,
  .write_byte = eusart_uart_write_byte,
  .write_buffer = eusart_uart_write_buffer,
  .set_enable_irq = eusart_uart_set_enable_irq,
  .clear_irq = eusart_uart_clear_irq,
  .get_enabled_pending_irq = eusart_uart_get_enabled_pending_irq,
  .get_enabled_irq = eusart_uart_get_enabled_irq,
  .rx_err_from_irq_status = eusart_uart_rx_err_from_irq_status,
  .is_idle = eusart_uart_is_idle,
#if defined(SL_CATALOG_POWER_MANAGER_PRESENT)
  .get_em_requirement = eusart_uart_get_em_requirement,
#endif
#if defined(SL_CATALOG_UART_ASYNC_PRESENT)
  .get_tx_register = eusart_uart_get_tx_register,
  .get_rx_register = eusart_uart_get_rx_register,
  .clear_tx_fifo = eusart_uart_clear_tx_fifo,
  .set_tx_enable = eusart_uart_set_tx_enable,
  // EUART does not support HW RX timeout.
  .set_rx_timeout = euart_uart_set_rx_timeout,
  .irq_rx_timeout_flag = 0,
#endif
  .irq_rx_err_flag = EUSART_IF_FERR | EUSART_IF_PERR | EUSART_IF_RXOF,
  .irq_rx_ready_flag = EUSART_IF_RXFL,
  .irq_tx_ready_flag = EUSART_IF_TXFL,
  .irq_tx_complete_flag = EUSART_IF_TXC,
};

#endif // EUART_PRESENT
