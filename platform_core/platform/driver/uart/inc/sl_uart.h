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

#ifndef SL_UART_H
#define SL_UART_H

#include <stdbool.h>
#include <stddef.h>

#include "sl_component_catalog.h"
#include "sl_device_gpio.h"
#include "sl_device_peripheral.h"
#include "sl_status.h"
#include "sl_device_uart.h"

#if defined(SL_CATALOG_UART_ASYNC_PRESENT)
#include "sl_dma_channel.h"
#include "sl_dma_manager.h"
#include "sl_slist.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

/***************************************************************************//**
 * @addtogroup uart UART Driver
 * @brief UART Driver
 * @li @ref uart_intro
 *
 * @n @section uart_intro Introduction
 *   This module contains functions to communicate using a UART peripheral of Silicon Labs 32-bit
 *   MCUs and SoCs.
 *
 * @{
 ******************************************************************************/

/***************************************************************************//**
 * @addtogroup uart_driver
 * @brief Core UART driver types, handle, and initialization APIs.
 * @{
 ******************************************************************************/

/*******************************************************************************
 *****************************   DATA TYPES   **********************************
 ******************************************************************************/

#define SL_UART_RX_ERR_PARITY   (1U << 0) ///< Parity error
#define SL_UART_RX_ERR_FRAMING  (1U << 1) ///< Framing error
#define SL_UART_RX_ERR_OVERFLOW (1U << 2) ///< RX overflow error
///< RX Error bitmap type.
typedef uint8_t sl_uart_rx_err_t;

// Forward declaration of the UART handle structure.
typedef struct uart_handle sl_uart_handle_t;

/***************************************************************************//**
 * Typedef for the user supplied callback function which is called when a RX
 * error is detected.
 *
 * @param uart_handle Handle to UART.
 *
 * @param error Variable that indicates the error type. This is a bitmap as
 *              multiple errors could occur on a given frame.
 *
 * @param user_arg User argument supplied when the callback was registered.
 ******************************************************************************/
typedef void (*sl_uart_rx_err_cb_t)(sl_uart_handle_t *uart_handle,
                                    sl_uart_rx_err_t error,
                                    void *user_arg);

/***************************************************************************//**
 * Typedef for the user supplied callback function which is called when there
 * is data available in the RX FIFO.
 *
 * @note The RX ready interrupt is level-based. While the interrupt remains enabled,
 *       the callback will be invoked again as long as RX data is present in the FIFO.
 *       To stop further notifications, either drain the FIFO from the callback using
 *       @ref sl_uart_read or @ref sl_uart_read_byte, or disable the interrupt with
 *       @ref sl_uart_disable_rx_ready_interrupt.
 *
 * @param uart_handle Handle to UART.
 *
 * @param user_arg User argument supplied when the callback was registered.
 *
 * @note This function will be called from interrupt context.
 ******************************************************************************/
typedef void (*sl_uart_rx_ready_cb_t)(sl_uart_handle_t *uart_handle,
                                      void *user_arg);

/***************************************************************************//**
 * Typedef for the user supplied callback function which is called when there
 * is room available in the TX FIFO.
 *
 * @note The TX ready interrupt is level-based. While the interrupt remains enabled,
 *       the callback will be invoked again as long as there is space in the TX FIFO.
 *       To stop further notifications, either fill the FIFO from the callback using
 *       @ref sl_uart_write or @ref sl_uart_write_byte, or disable the interrupt with
 *       @ref sl_uart_disable_tx_ready_interrupt.
 *
 * @param uart_handle Handle to UART.
 *
 * @param user_arg User argument supplied when the callback was registered.
 *
 * @note This function will be called from interrupt context.
 ******************************************************************************/
typedef void (*sl_uart_tx_ready_cb_t)(sl_uart_handle_t *uart_handle,
                                      void *user_arg);

/***************************************************************************//**
 * Typedef for the user supplied callback function which is called when the
 * submitted transmission has been completely sent over the bus.
 *
 * @param uart_handle Handle to UART.
 *
 * @param user_arg User argument supplied when the callback was registered.
 *
 * @note This function will be called from interrupt context.
 ******************************************************************************/
typedef void (*sl_uart_tx_complete_cb_t)(sl_uart_handle_t *uart_handle,
                                         void *user_arg);

#if defined(SL_CATALOG_UART_ASYNC_PRESENT)

/***************************************************************************//**
 * @addtogroup uart_async
 * @brief Asynchronous DMA transfer APIs and types.
 *
 * @note An async handle  must use only the APIs in this group for data transfer.
 *       The polling APIs (@ref uart_polling) and interrupt-driven transfer APIs
 *       in @ref uart_interrupt (@ref sl_uart_read, @ref sl_uart_write, and the
 *       TX/RX ready and TX complete interrupt APIs) must not be used on such a handle.
 *
 * @{
 ******************************************************************************/

/**
 * Indicate if transfer timed out. See @ref sl_uart_async_on_rx_cb_t for more details.
 */
#define SL_UART_ASYNC_RX_EVENT_TIMEOUT      (1 << 0)

/**
 * Indicates if buffer, pointed to by @c data, is released from the driver and can be re-used
 * by the application. See @ref sl_uart_async_on_rx_cb_t for more details.
 */
#define SL_UART_ASYNC_RX_EVENT_BUF_RELEASED (1 << 1)

/**
 * Indicates if the driver is about to run out of buffer. If true, user is encouraged to submit
 *  new RX buffer via @ref sl_uart_async_read() to avoid loosing data. See @ref sl_uart_async_on_rx_cb_t
 *  for more details.
 */
#define SL_UART_ASYNC_RX_EVENT_BUF_NEEDED   (1 << 2)

/**
 * Indicates if transfer was aborted. See @ref sl_uart_async_on_rx_cb_t for more details.
 */
#define SL_UART_ASYNC_RX_EVENT_ABORTED      (1 << 3)

typedef uint8_t sl_uart_async_rx_event_t; ///< RX Event bitmap type.

/***************************************************************************//**
 * Typedef for the user supplied callback function which is called on a tx
 * complete event.
 *
 * @param uart_handle Handle to UART.
 *
 * @param data Pointer to transmitted data buffer.
 *
 * @param size Size, in bytes, of data that were actually transmitted. Unless
 *             transfer was aborted, it should equal the buffer size that was
 *             given in @ref sl_uart_async_write().
 *
 * @param user_data Pointer to user data that is passed to the tx complete
 *                  handler function.
 *
 * @param aborted Indicate if transfer was aborted.
 *
 * @note This callback will be called for each buffer submitted via
 *       @ref sl_uart_async_write() everytime a transfer completes.
 *
 * @note In case a user calls the function @ref sl_uart_async_abort_tx(), this
 *       callback will be called for each submitted buffers that were left in
 *       the queue with the @p aborted flag set to true. If the abort function
 *       is called in the middle of a buffer transfer, this callback will be called
 *       with the aborted flag set to true and value of @p size will represent
 *       the number of bytes that were successfully transmitted before the transfer was aborted.
 *
 * @note Once this function is called, it is safe to assume that the buffer
 *       pointed  to by @p data is released from the driver and can be re-used by the caller.
 *
 * @note This function will be called from interrupt context.
 ******************************************************************************/
typedef void (*sl_uart_async_on_tx_complete_cb_t)(sl_uart_handle_t *uart_handle,
                                                  uint8_t *data,
                                                  size_t size,
                                                  void *user_data,
                                                  bool aborted);

/***************************************************************************//**
 * Typedef for the user supplied callback function which is called on a rx
 * event.
 *
 * @param uart_handle Handle to UART.
 *
 * @param data Pointer to received data buffer.
 *
 * @param size Size, in bytes, of data received. See notes for more details.
 *
 * @param user_data Pointer to user data that is passed to the rx complete
 *                  handler function.
 *
 * @param rx_event Bitmap that indicates what event(s) happened.
 *
 * @note This callback will be called for each buffer submitted via
 *       @ref sl_uart_async_read() everytime a transfer completes, or when the driver is about
 *       to run out of buffer. It can be called several times for a given buffer.
 *
 * @note As soon as the driver starts reception on the last reception buffer in the chain and is
 *       about to run out of buffer, this callback will be called with the @ref SL_UART_ASYNC_RX_EVENT_BUF_NEEDED
 *       @p rx_event set. The user is expected to submit a new buffer via @ref sl_uart_async_read() to avoid
 *       loosing data.
 *
 * @note In case a user calls the function @ref sl_uart_async_disable_rx(), this
 *       callback will be called for each submitted buffers that were left in
 *       the queue with the @ref SL_UART_ASYNC_RX_EVENT_ABORTED and
 *       @ref SL_UART_ASYNC_RX_EVENT_BUF_RELEASED @p rx_event set. If the abort
 *       function is called in the middle of a buffer receive, @ref SL_UART_ASYNC_RX_EVENT_ABORTED
 *       and @ref SL_UART_ASYNC_RX_EVENT_BUF_RELEASED @p rx_event are set, and
 *       the value of @p size will represent the amount of bytes that were
 *       received before the transfer was aborted.
 *
 * @note In case a RX error occurs, for example a parity error is detected,
 *       transfers will be aborted with the @ref SL_UART_ASYNC_RX_EVENT_ABORTED
 *       and @ref SL_UART_ASYNC_RX_EVENT_BUF_RELEASED @p rx_event set. The callback
 *       specified with @ref sl_uart_set_rx_err_callback() will also be called and
 *       can be used to determine if/what error occurred.
 *
 * @note In case of a RX timeout, this callback will be called with the
 *       @ref SL_UART_ASYNC_RX_EVENT_TIMEOUT @p rx_event set. The @p size argument will
 *       represent how many bytes were received so far. It is important to remember
 *       that in case of a RX timeout, the current buffer remains active. So the
 *       @ref SL_UART_ASYNC_RX_EVENT_BUF_RELEASED @p rx_event will not be set.
 *
 * @note This function can be called from interrupt context.
 ******************************************************************************/
typedef void (*sl_uart_async_on_rx_cb_t)(sl_uart_handle_t *uart_handle,
                                         uint8_t *data,
                                         size_t size,
                                         void *user_data,
                                         sl_uart_async_rx_event_t rx_event);

/** @} (end addtogroup uart_async) */

#endif // SL_CATALOG_UART_ASYNC_PRESENT

/***************************************************************************//**
 * @addtogroup uart_driver
 * @{
 ******************************************************************************/

/// @brief Pin configuration
typedef struct uart_pin_config {
  sl_gpio_t tx;     ///< GPIO used for TX
  sl_gpio_t rx;     ///< GPIO used for RX
  sl_gpio_t cts;    ///< GPIO used for CTS (Only needed with @c SL_UART_FLOW_CONTROL_CTS_RTS flow control config)
  sl_gpio_t rts;    ///< GPIO used for RTS (Only needed with @c SL_UART_FLOW_CONTROL_CTS_RTS flow control config)
} sl_uart_pin_config_t;

// Forward declaration of the UART operations structure.
typedef struct sli_uart_ops sli_uart_ops_t;

#if defined(SL_CATALOG_UART_ASYNC_PRESENT)
#define SL_UART_ASYNC_DMA_CHANNEL_CONFIG_AUTO UINT8_MAX ///< Automatically allocate the DMA channel at init.

/// @brief UART pre-initialization configuration preserved across deinit.
typedef struct uart_preinit_config {
  size_t async_tx_transfer_count;
  size_t async_rx_transfer_count;
  uint8_t async_tx_dma_channel_number;
  uint8_t async_rx_dma_channel_number;
  bool async_en;
} sl_uart_preinit_config_t;
#endif

// Forward declaration of the UART transfer structure.
typedef struct sli_uart_async_transfer sli_uart_async_transfer_t;

/// @brief UART handle structure.
typedef struct uart_handle {
  const sli_uart_ops_t *ops;
  sl_peripheral_t uart;
  sl_uart_pin_config_t pin_config;
  sl_uart_config_t config;
  sl_uart_rx_err_cb_t rx_err_cb;
  void *rx_err_cb_arg;
  sl_uart_rx_ready_cb_t rx_ready_cb;
  void *rx_ready_cb_arg;
  sl_uart_tx_ready_cb_t tx_ready_cb;
  void *tx_ready_cb_arg;
  sl_uart_tx_complete_cb_t tx_complete_cb;
  void *tx_complete_cb_arg;
#if defined(SL_CATALOG_UART_ASYNC_PRESENT)
  sl_uart_preinit_config_t preinit_config;
  sl_slist_node_t *async_tx_submitted_list_head;
  sl_slist_node_t *async_rx_submitted_list_head;
  sl_slist_node_t *async_tx_pool;
  sl_slist_node_t *async_rx_pool;
  sl_uart_async_on_tx_complete_cb_t async_tx_complete_cb;
  void* async_tx_complete_cb_user_data;
  sl_uart_async_on_rx_cb_t async_rx_cb;
  void* async_rx_cb_user_data;
  sl_dma_channel_handle_t async_tx_dma_channel;
  sl_dma_channel_handle_t async_rx_dma_channel;
#endif
} sl_uart_handle_t;

/*******************************************************************************
 *****************************   PROTOTYPES   **********************************
 ******************************************************************************/

/***************************************************************************//**
 * Allocates and return a UART handle.
 *
 * @param[out]  uart_handle Pointer to variable that will receive the allocated
 *                          handle.
 *
 * @return @ref SL_STATUS_OK if successful. Error code otherwise.
 ******************************************************************************/
sl_status_t sl_uart_handle_alloc(sl_uart_handle_t **uart_handle);

/***************************************************************************//**
 * Frees a UART handle.
 *
 * @param[in]  uart_handle UART handle to free.
 *
 * @return @ref SL_STATUS_OK if successful. Error code otherwise.
 ******************************************************************************/
sl_status_t sl_uart_handle_free(sl_uart_handle_t *uart_handle);

/***************************************************************************//**
 * Gets the size of the UART handle structure.
 *
 * @note This function is used to ensure ABI compatibility with applications
 *       that want to allocate the UART handle manually when the driver is
 *       compiled as a library.
 *
 * @return  UART handle structure's size.
 ******************************************************************************/
__STATIC_INLINE size_t sl_uart_handle_get_size(void)
{
  return sizeof(sl_uart_handle_t);
}

/***************************************************************************//**
 * Initializes given UART instance.
 *
 * @param[in]  uart_handle Handle to UART.
 *
 * @param[in]  uart UART peripheral to use with this handle.
 *
 * @param[in]  pin_config For the specified UART.
 *
 * @return @ref SL_STATUS_OK if successful.
 *         @ref SL_STATUS_ALREADY_INITIALIZED if the UART is already initialized.
 *         Error code otherwise.
 ******************************************************************************/
sl_status_t sl_uart_init(sl_uart_handle_t *uart_handle,
                         sl_peripheral_t uart,
                         sl_uart_pin_config_t pin_config);

/***************************************************************************//**
 * De-initializes given UART instance.
 *
 * @param[in]  uart_handle Handle to UART.
 *
 * @return @ref SL_STATUS_OK if successful.
 *         @ref SL_STATUS_NOT_INITIALIZED if the UART is not initialized.
 *         Error code otherwise.
 ******************************************************************************/
sl_status_t sl_uart_deinit(sl_uart_handle_t *uart_handle);

/***************************************************************************//**
 * Applies line configuration.
 *
 * @param[in]  uart_handle Handle to UART.
 *
 * @param[in]  config Line configuration.
 *
 * @return @ref SL_STATUS_OK if successful. Error code otherwise.
 *
 * @note This function shall be called after initialization and before any transfer
 *       is initiated. It can be called later to re-configure the line, but it
 *       cannot be called while there is any active RX/TX operations. Applying a
 *       new configuration will reset the UART peripheral and any data left in the
 *       RX or TX FIFO will be lost.
 ******************************************************************************/
sl_status_t sl_uart_configure_line(sl_uart_handle_t *uart_handle,
                                   sl_uart_config_t config);

/***************************************************************************//**
 * Gets active line configuration for the specified UART handle.
 *
 * @param[in]  uart_handle Handle to UART.
 *
 * @param[in]  config Pointer to variable that will receive the line configuration.
 *
 * @return @ref SL_STATUS_OK if successful. Error code otherwise.
 *
 * @note This function can only be called after sl_uart_configure_line() was
 *       called.
 ******************************************************************************/
sl_status_t sl_uart_get_line_configuration(sl_uart_handle_t *uart_handle,
                                           sl_uart_config_t *config);

/** @} (end addtogroup uart_driver) */

/***************************************************************************//**
 * @addtogroup uart_interrupt
 * @{
 ******************************************************************************/

/***************************************************************************//**
 * Registers a callback that will be called when a RX error happens.
 *
 * @note Calling this function will enable the PARITY, FRAMING, and OVERFLOW error IRQ signals on
 *       the UART peripheral. The callback will be called when any of these errors occur.
 *
 * @param[in]  uart_handle Handle to UART.
 *
 * @param[in]  rx_err_cb Function to be called on error.
 *
 * @param[in]  user_arg User argument passed to @p rx_err_cb.
 ******************************************************************************/
void sl_uart_set_rx_err_callback(sl_uart_handle_t *uart_handle,
                                 sl_uart_rx_err_cb_t rx_err_cb,
                                 void *user_arg);

/***************************************************************************//**
 * Registers a RX ready callback.
 *
 * @param[in]  uart_handle Handle to UART.
 *
 * @param[in]  rx_ready_cb Function handler of the callback.
 *
 * @param[in]  user_arg User argument passed to @p rx_ready_cb.
 ******************************************************************************/
void sl_uart_set_rx_ready_callback(sl_uart_handle_t *uart_handle,
                                   sl_uart_rx_ready_cb_t rx_ready_cb,
                                   void *user_arg);

/***************************************************************************//**
 * Registers a TX ready callback.
 *
 * @param[in]  uart_handle Handle to UART.
 *
 * @param[in]  tx_ready_cb Function handler of the callback.
 *
 * @param[in]  user_arg User argument passed to @p tx_ready_cb.
 ******************************************************************************/
void sl_uart_set_tx_ready_callback(sl_uart_handle_t *uart_handle,
                                   sl_uart_tx_ready_cb_t tx_ready_cb,
                                   void *user_arg);

/***************************************************************************//**
 * Registers a TX complete callback.
 *
 * @param[in]  uart_handle Handle to UART.
 *
 * @param[in]  tx_complete_cb Function handler of the callback.
 *
 * @param[in]  user_arg User argument passed to @p tx_complete_cb.
 ******************************************************************************/
void sl_uart_set_tx_complete_callback(sl_uart_handle_t *uart_handle,
                                      sl_uart_tx_complete_cb_t tx_complete_cb,
                                      void *user_arg);

/***************************************************************************//**
 * Enables RX ready interrupt.
 *
 * @note The RX ready interrupt is level-based. It fires while RX data is present
 *       in the FIFO. If the FIFO already contains data when this function is called,
 *       the interrupt will fire immediately.
 *
 * @param[in]  uart_handle Handle to UART.
 ******************************************************************************/
void sl_uart_enable_rx_ready_interrupt(sl_uart_handle_t *uart_handle);

/***************************************************************************//**
 * Enables TX ready interrupt.
 *
 * @note The TX ready interrupt is level-based. It fires while there is room in
 *       the TX FIFO. If the FIFO already has space when this function is called,
 *       the interrupt will fire immediately.
 *
 * @param[in]  uart_handle Handle to UART.
 ******************************************************************************/
void sl_uart_enable_tx_ready_interrupt(sl_uart_handle_t *uart_handle);

/***************************************************************************//**
 * Enables TX complete interrupt.
 *
 * @note The TX complete interrupt will fire as soon as all bytes in the TX FIFO
 *       have been sent. If the FIFO was already empty, the interrupt will not fire.
 *
 * @param[in]  uart_handle Handle to UART.
 ******************************************************************************/
void sl_uart_enable_tx_complete_interrupt(sl_uart_handle_t *uart_handle);

/***************************************************************************//**
 * Disables RX ready interrupt.
 *
 * @param[in]  uart_handle Handle to UART.
 ******************************************************************************/
void sl_uart_disable_rx_ready_interrupt(sl_uart_handle_t *uart_handle);

/***************************************************************************//**
 * Disables TX ready interrupt.
 *
 * @param[in]  uart_handle Handle to UART.
 ******************************************************************************/
void sl_uart_disable_tx_ready_interrupt(sl_uart_handle_t *uart_handle);

/***************************************************************************//**
 * Disables TX complete interrupt.
 *
 * @param[in]  uart_handle Handle to UART.
 ******************************************************************************/
void sl_uart_disable_tx_complete_interrupt(sl_uart_handle_t *uart_handle);

/** @} (end addtogroup uart_interrupt) */

/***************************************************************************//**
 * @addtogroup uart_polling
 * @brief Polling transfer APIs.
 * @{
 ******************************************************************************/

/***************************************************************************//**
 * Reads a single character on UART.
 *
 * @param[in]  uart_handle Handle to UART.
 *
 * @param[out] byte Pointer to location where received byte will be stored.
 *
 * @return @ref SL_STATUS_OK if successful. @ref SL_STATUS_EMPTY if no byte received.
 *         Error code otherwise.
 *
 * @note This function can be called from IRQ context.
 ******************************************************************************/
sl_status_t sl_uart_read_byte(sl_uart_handle_t *uart_handle, uint8_t *byte);

/***************************************************************************//**
 * Writes a single character on UART.
 *
 * @param[in]  uart_handle Handle to UART.
 *
 * @param[in]  byte Byte to write.
 *
 * @return @ref SL_STATUS_OK if successful. Error code otherwise.
 *
 * @note This function is blocking. If FIFO is full, the function will block
 *       until some room is available.
 *
 * @note This function can be called from IRQ context.
 ******************************************************************************/
sl_status_t sl_uart_write_byte(sl_uart_handle_t *uart_handle, uint8_t byte);

/** @} (end addtogroup uart_polling) */

/***************************************************************************//**
 * @addtogroup uart_interrupt
 * @{
 ******************************************************************************/

/***************************************************************************//**
 * Reads data from RX FIFO.
 *
 * @param[in]  uart_handle Handle to UART.
 *
 * @param[in]  data Pointer to buffer that will receive data.
 *
 * @param[in]  size Size of the buffer.
 *
 * @param[out]  read_size Pointer to variable that will receive the size of data
 *                        that was actually copied from the RX FIFO.
 *
 * @return @ref SL_STATUS_OK if one or more bytes were read (see @p read_size),
 *         @ref SL_STATUS_EMPTY if no data was available to read.
 *         Error code otherwise.
 *
 * @note This function will copy data from RX FIFO to the supplied buffer until the
 *       RX FIFO is empty OR the buffer size is reached.
 *
 * @note This function can be called from IRQ context.
 ******************************************************************************/
sl_status_t sl_uart_read(sl_uart_handle_t *uart_handle,
                         void *data,
                         size_t size,
                         size_t *read_size);

/***************************************************************************//**
 * Writes data to TX FIFO.
 *
 * @param[in]  uart_handle Handle to UART.
 *
 * @param[in]  data Pointer to buffer that contains data to transmit.
 *
 * @param[in]  size Size of the buffer.
 *
 * @param[out]  write_size Pointer to variable that will receive the size of
 *                         data that was actually copied to the TX FIFO.
 *
 * @return @ref SL_STATUS_OK if successful and buffer has been completely written to the TX FIFO,
 *         @ref SL_STATUS_FULL if TX FIFO is full and more data was left in the buffer.
 *         Error code otherwise.
 *
 * @note This function will copy data from the supplied buffer to the TX FIFO
 *       until the TX FIFO is full OR the buffer size is reached.
 *
 * @note This function can be called from IRQ context.
 ******************************************************************************/
sl_status_t sl_uart_write(sl_uart_handle_t *uart_handle,
                          const void *data,
                          const size_t size,
                          size_t *write_size);

/** @} (end addtogroup uart_interrupt) */

#if defined(SL_CATALOG_UART_ASYNC_PRESENT)

/***************************************************************************//**
 * @addtogroup uart_async
 * @{
 ******************************************************************************/

/***************************************************************************//**
 * Registers a TX complete callback.
 *
 * @param[in]  uart_handle Handle to UART.
 *
 * @param[in]  on_tx_complete Function handler of the callback.
 *
 * @param user_data Pointer to user data that is passed to the tx complete
 *                  handler function.
 ******************************************************************************/
void sl_uart_async_set_tx_complete_callback(sl_uart_handle_t *uart_handle,
                                            sl_uart_async_on_tx_complete_cb_t on_tx_complete,
                                            void *user_data);

/***************************************************************************//**
 * Registers a RX event callback.
 *
 * @param[in]  uart_handle Handle to UART.
 *
 * @param[in]  on_rx_event Function handler of the callback.
 *
 * @param user_data Pointer to user data that is passed to the rx complete
 *                  handler function.
 ******************************************************************************/
void sl_uart_async_set_rx_event_callback(sl_uart_handle_t *uart_handle,
                                         sl_uart_async_on_rx_cb_t on_rx_event,
                                         void *user_data);

/***************************************************************************//**
 * Submits buffer to write.
 *
 * @param[in] uart_handle Handle to UART.
 *
 * @param[in] size Size of the buffer.
 *
 * @param[in] data Pointer to data buffer to transmit.
 *
 * @return @ref SL_STATUS_OK if successful.
 *         @ref SL_STATUS_EMPTY if @p size is 0.
 *         @ref SL_STATUS_BUSY if the maximum number of queued TX transfers has been reached.
 *         @ref SL_STATUS_INVALID_PARAMETER if @p size exceeds the maximum transfer size
 *         supported by a single DMA descriptor (@ref SL_DMA_CHANNEL_MAX_XFER_UNIT_COUNT).
 *         Error code otherwise.
 *
 * @note This function is non blocking. Once transfer is completed, the callback
 *       specified with sl_uart_async_set_tx_complete_callback() will be called.
 *
 * @note When submitting a transfer before the previous completes, it will be
 *       queued and scheduled for transmission once the previous one has been
 *       transmitted entirely over the bus.
 *
 * @note This function can be called from an ISR.
 ******************************************************************************/
sl_status_t sl_uart_async_write(sl_uart_handle_t *uart_handle,
                                const void *data,
                                const size_t size);

/***************************************************************************//**
 * Aborts TX.
 *
 * @param[in] uart_handle Handle to UART.
 *
 * @return @ref SL_STATUS_OK if successful.
 *         Error code otherwise.
 *
 * @note When calling this function, transmission will stop and all submitted
 *       transmit buffer via sl_uart_async_write() will be released.
 ******************************************************************************/
sl_status_t sl_uart_async_abort_tx(sl_uart_handle_t *uart_handle);

/***************************************************************************//**
 * Retrieves TX status.
 *
 * @param[in]  uart_handle Handle to UART.
 *
 * @return true if there is an active TX. False, otherwise.
 ******************************************************************************/
static inline bool sl_uart_async_is_tx_active(sl_uart_handle_t *uart_handle)
{
  EFM_ASSERT((uart_handle) != NULL);
  EFM_ASSERT((uart_handle)->uart != NULL);
  EFM_ASSERT((uart_handle)->preinit_config.async_tx_transfer_count != 0
             || (uart_handle)->preinit_config.async_rx_transfer_count != 0);

  return !sl_slist_is_empty(uart_handle->async_tx_submitted_list_head);
}

/***************************************************************************//**
 * Submits buffer to read.
 *
 * @param[in]  uart_handle Handle to UART.
 *
 * @param[in]  new_len New length.
 *
 * @return @ref SL_STATUS_OK if successful.
 *         @ref SL_STATUS_EMPTY if @p size is 0.
 *         @ref SL_STATUS_BUSY if the maximum number of queued RX transfers has been reached.
 *         @ref SL_STATUS_INVALID_PARAMETER if @p size exceeds the maximum transfer size
 *         supported by a single DMA descriptor (@ref SL_DMA_CHANNEL_MAX_XFER_UNIT_COUNT).
 *         Error code otherwise.
 *
 * @note This function is non blocking. Once transfer is completed, the callback
 *       specified with @ref sl_uart_async_set_rx_event_callback() will be called.
 *
 * @note This function can be called from an ISR.
 ******************************************************************************/
sl_status_t sl_uart_async_read(sl_uart_handle_t *uart_handle,
                               void *data,
                               const size_t size);

/***************************************************************************//**
 * Disables RX.
 *
 * @param[in]  uart_handle Handle to UART.
 *
 * @return @ref SL_STATUS_OK if successful. Error code otherwise.
 *
 * @note This function will disable RX. All submitted buffers via
 *       @ref sl_uart_async_read() will be released.
 ******************************************************************************/
sl_status_t sl_uart_async_disable_rx(sl_uart_handle_t *uart_handle);

/***************************************************************************//**
 * Retrieves RX status.
 *
 * @param[in]  uart_handle Handle to UART.
 *
 * @return true if there is an active RX. False, otherwise.
 ******************************************************************************/
static inline bool sl_uart_async_is_rx_active(sl_uart_handle_t *uart_handle)
{
  EFM_ASSERT((uart_handle) != NULL);
  EFM_ASSERT((uart_handle)->uart != NULL);
  EFM_ASSERT((uart_handle)->preinit_config.async_tx_transfer_count != 0
             || (uart_handle)->preinit_config.async_rx_transfer_count != 0);

  return !sl_slist_is_empty(uart_handle->async_rx_submitted_list_head);
}

/***************************************************************************//**
 * Update size of the current read.
 *
 * @param[in]  uart_handle Handle to UART.
 *
 * @param[in]  new_size New size.
 *
 * @return @ref SL_STATUS_OK if successful.
 *         @ref SL_STATUS_INVALID_PARAMETER if @p new_size is 0, if more bytes have already
 *         been received than @p new_size, or if the remaining transfer size exceeds the
 *         maximum supported by a single DMA descriptor
 *         (@ref SL_DMA_CHANNEL_MAX_XFER_UNIT_COUNT).
 *         Error code otherwise.
 *
 * @note This function allows to update the expected size of RX data for the active
 *       buffer. This function is useful in cases of, for example, the expected
 *       data follows a pattern of a header that specifies a payload size followed by
 *       the actual payload. When the header and the payload have to be in separate
 *       buffers and 0-copy is desired, the user can parse the header from the rx_event
 *       callback and call this function to update the size matching the information
 *       read from the header.
 *
 * @note Special care must be taken by the caller to ensure that the active
 *       buffer is really the one it aims at modifying.
 *
 * @note It is up to the caller to ensure that @p new_size is not larger than the
 *       buffer that was submitted earlier.
 ******************************************************************************/
sl_status_t sl_uart_async_update_current_read_size(sl_uart_handle_t *uart_handle,
                                                   const size_t new_size);

#endif // SL_CATALOG_UART_ASYNC_PRESENT

/** @} (end addtogroup uart_async) */

/** @} (end addtogroup uart) */

#ifdef __cplusplus
}
#endif

#endif // SL_UART_H
