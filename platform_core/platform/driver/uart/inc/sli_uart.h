/***************************************************************************/ /**
 * @file
 * @brief UART driver API
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

#ifndef SLI_UART_H
#define SLI_UART_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

#include "sl_assert.h"
#include "sl_component_catalog.h"
#include "sl_status.h"
#include "sl_device_uart.h"
#include "sl_uart.h"

#if defined(SL_CATALOG_POWER_MANAGER_PRESENT)
#include "sl_power_manager.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

/*******************************************************************************
 *******************************   DEFINES   ***********************************
 ******************************************************************************/
#define SLI_UART_ASSERT_VALID_HANDLE(uart_handle) \
        EFM_ASSERT((uart_handle) != NULL); \
        EFM_ASSERT((uart_handle)->uart != NULL)

#define SLI_UART_CONFIG_IS_VALID(config) (config.baudrate != 0                       \
                                          && config.parity <= SL_UART_PARITY_EVEN    \
                                          && config.stop_bits <= SL_UART_STOP_BITS_2 \
                                          && config.data_bits <= SL_UART_DATA_BITS_9 \
                                          && config.flow_control <= SL_UART_FLOW_CONTROL_SOFT)
#define SLI_UART_HANDLE_IS_SUSPENDED(uart_handle) \
        ((uart_handle)->state == SL_UART_HANDLE_STATE_SUSPENDED)

#if defined(SL_CATALOG_UART_ASYNC_PRESENT)
#define SLI_UART_HANDLE_IS_ASYNC(uart_handle) \
        ((uart_handle)->preinit_config.async_tx_transfer_count != 0 \
         || (uart_handle)->preinit_config.async_rx_transfer_count != 0)
#define SLI_UART_HANDLE_IS_SYNC(uart_handle) \
        ((uart_handle)->preinit_config.async_tx_transfer_count == 0 \
         && (uart_handle)->preinit_config.async_rx_transfer_count == 0)
#else
#define SLI_UART_HANDLE_IS_ASYNC(uart_handle) (false)
#define SLI_UART_HANDLE_IS_SYNC(uart_handle) (true)
#endif

/*******************************************************************************
 *****************************   DATA TYPES   **********************************
 ******************************************************************************/

///< UART backend operations selected at init based on peripheral IP type.
struct sli_uart_ops {
  void (*reset)(sl_peripheral_t uart);
  sl_status_t (*init)(sl_peripheral_t uart, sl_uart_config_t config);
  void (*deinit)(sl_peripheral_t uart);
  void (*init_transport_pins)(sl_peripheral_t uart, sl_uart_pin_config_t pin_config);
  void (*deinit_transport_pins)(sl_peripheral_t uart);
  void (*init_hwfc_pins)(sl_peripheral_t uart, sl_uart_pin_config_t pin_config);
  void (*deinit_hwfc_pins)(sl_peripheral_t uart);
  sl_status_t (*read_byte)(sl_peripheral_t uart, uint8_t *byte);
  sl_status_t (*read_buffer)(sl_peripheral_t uart, uint8_t *data, size_t size, size_t *bytes_read);
  sl_status_t (*write_byte)(sl_peripheral_t uart, uint8_t byte);
  sl_status_t (*write_buffer)(sl_peripheral_t uart, const uint8_t *data, size_t size, size_t *bytes_written);
  void (*set_enable_irq)(sl_peripheral_t uart, bool enabled, uint32_t irq);
  void (*clear_irq)(sl_peripheral_t uart, uint32_t irq);
  uint32_t (*get_enabled_pending_irq)(sl_peripheral_t uart);
  uint32_t (*get_enabled_irq)(sl_peripheral_t uart);
  sl_uart_rx_err_t (*rx_err_from_irq_status)(uint32_t irq_status);
  bool (*is_idle)(sl_peripheral_t uart);
#if defined(SL_CATALOG_POWER_MANAGER_PRESENT)
  sl_power_manager_em_t (*get_em_requirement)(sl_peripheral_t uart);
#endif
#if defined(SL_CATALOG_UART_ASYNC_PRESENT)
  void *(*get_tx_register)(sl_peripheral_t uart);
  void *(*get_rx_register)(sl_peripheral_t uart);
  size_t (*clear_tx_fifo)(sl_peripheral_t uart);
  void (*set_tx_enable)(sl_peripheral_t uart, bool en);
#endif
  uint32_t irq_rx_err_flag;
  uint32_t irq_rx_ready_flag;
  uint32_t irq_tx_ready_flag;
  uint32_t irq_tx_complete_flag;
};

typedef struct sli_uart_ops sli_uart_ops_t;

/*******************************************************************************
 *****************************   PROTOTYPES   **********************************
 ******************************************************************************/

/***************************************************************************//**
 * Resets the UART peripheral.
 *
 * @param[in]  uart_handle Handle to UART.
 ******************************************************************************/
void sli_uart_reset(sl_uart_handle_t *uart_handle);

/***************************************************************************//**
 * Initializes the core of the driver for the given UART instance. This also
 * initializes the UART peripheral.
 *
 * @param[in]  uart_handle Handle to UART.
 * @param[in]  uart UART peripheral to use with this handle.
 * @param[in]  pin_config Pin assignment for the specified UART.
 ******************************************************************************/
void sli_uart_init_core(sl_uart_handle_t *uart_handle, sl_peripheral_t uart, sl_uart_pin_config_t pin_config);

/***************************************************************************//**
 * De-initializes the core of the driver for the given UART instance.
 *
 * @param[in]  uart_handle Handle to UART.
 ******************************************************************************/
void sli_uart_deinit_core(sl_uart_handle_t *uart_handle);

/***************************************************************************//**
 * De-initializes the UART peripheral and its associated GPIO routes.
 *
 * @param[in]  uart_handle Handle to UART.
 ******************************************************************************/
void sli_uart_deinit_peripheral(sl_uart_handle_t *uart_handle);

/***************************************************************************//**
 * Initializes the pins for the given UART instance.
 *
 * @param[in]  pin_config Pin assignment for the specified UART.
 ******************************************************************************/
void sli_uart_init_transport_pins(sl_uart_handle_t *uart_handle, sl_uart_pin_config_t pin_config);

/***************************************************************************//**
 * De-initializes the transport pins for the given UART instance.
 *
 * @param[in]  uart_handle Handle to UART.
 ******************************************************************************/
void sli_uart_deinit_transport_pins(sl_uart_handle_t *uart_handle);

/***************************************************************************//**
 * Initializes the pins for the given UART instance.
 *
 * @param[in]  pin_config Pin assignment for the specified UART.
 ******************************************************************************/
void sli_uart_init_hwfc_pins(sl_uart_handle_t *uart_handle, sl_uart_pin_config_t pin_config);

/***************************************************************************//**
 * De-initializes the hardware flow control pins for the given UART instance.
 *
 * @param[in]  uart_handle Handle to UART.
 ******************************************************************************/
void sli_uart_deinit_hwfc_pins(sl_uart_handle_t *uart_handle);

/***************************************************************************//**
 * Initializes the UART instance.
 *
 * @param[in]  uart_handle Handle to UART.
 ******************************************************************************/
void sli_uart_init_clocks(sl_uart_handle_t *uart_handle);

/***************************************************************************//**
 * De-initializes the clocks for the given UART instance.
 *
 * @param[in]  uart_handle Handle to UART.
 ******************************************************************************/
void sli_uart_deinit_clocks(sl_uart_handle_t *uart_handle);

/***************************************************************************//**
 * Enables NVIC RX and TX interrupts for the given UART peripheral.
 ******************************************************************************/
void sli_uart_init_irq(sl_uart_handle_t *uart_handle);

/***************************************************************************//**
 * Enables an IRQ for the given UART instance.
 ******************************************************************************/
static inline void sli_uart_enable_irq(sl_uart_handle_t *uart_handle, uint32_t irq)
{
  uart_handle->enabled_irq |= irq;
  uart_handle->ops->set_enable_irq(uart_handle->uart, true, irq);
}

/***************************************************************************//**
 * Disables an IRQ for the given UART instance.
 ******************************************************************************/
static inline void sli_uart_disable_irq(sl_uart_handle_t *uart_handle, uint32_t irq)
{
  uart_handle->enabled_irq &= ~irq;
  uart_handle->ops->set_enable_irq(uart_handle->uart, false, irq);
}

/***************************************************************************//**
 * Disables NVIC RX and TX interrupts for the given UART peripheral.
 ******************************************************************************/
void sli_uart_deinit_irq(sl_peripheral_t uart);

/***************************************************************************//**
 * Handles the RX interrupt for the given UART instance.
 *
 * @param[in]  uart_handle Handle to UART.
 ******************************************************************************/
void sli_uart_rx_irq_handler(sl_uart_handle_t *uart_handle);

/***************************************************************************//**
 * Handles the TX interrupt for the given UART instance.
 *
 * @param[in]  uart_handle Handle to UART.
 ******************************************************************************/
void sli_uart_tx_irq_handler(sl_uart_handle_t *uart_handle);

 #ifdef __cplusplus
}
 #endif

 #endif // SLI_UART_H
