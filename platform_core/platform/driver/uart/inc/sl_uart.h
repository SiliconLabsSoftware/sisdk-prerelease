/***************************************************************************/ /**
 * @file
 * @brief UART Driver (polling, interrupt-driven, and async DMA APIs)
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
#include "sl_sleeptimer.h"
#include "sl_slist.h"
#endif

#if defined(SL_CATALOG_POWER_MANAGER_PRESENT)
#include "sl_power_manager.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

/***************************************************************************//**
 * @addtogroup uart UART Driver
 * @brief UART driver for USART, EUART, and EUSART peripherals.
 * @details
 *  ## Overview
 *
 *  The UART Driver is a handle-based API for Silicon Labs 32-bit MCUs and SoCs.
 *  After the peripheral is initialized and the line is configured, transfer
 *  data with one of three complementary API sets:
 *
 *  - @ref uart_polling — The CPU moves one byte at a time
 *    through the hardware FIFOs. @ref sl_uart_write_byte waits for TX FIFO
 *    room; @ref sl_uart_read_byte returns immediately with
 *    @ref SL_STATUS_EMPTY if nothing is waiting.
 *  - @ref uart_interrupt — The CPU moves data through the FIFOs, but the
 *    calls return as soon as the FIFO is drained or filled, without waiting
 *    for the rest of the transfer on the bus. UART interrupt callbacks notify
 *    the application when additional data can be read or written and when
 *    transmission is complete on the bus.
 *  - @ref uart_async — DMA moves data between the peripheral and application
 *    buffers. Transfers can be queued; a callback reports completion and
 *    related events. Provides higher throughput by removing the CPU from per-byte
 *    data handling at the cost of additional RAM and two DMA channels (RX and TX).
 *
 *  Choose the set that matches the throughput and CPU requirements of the transfer.
 *
 *  ## Choosing an API set
 *
 *  | API set | Data path | Blocking behavior | Typical use |
 *  | :------ | :-------- | :---------------- | :---------- |
 *  | @ref uart_polling | CPU copies through the FIFO | @ref sl_uart_write_byte waits for TX FIFO room; @ref sl_uart_read_byte returns immediately (@ref SL_STATUS_EMPTY if nothing to read) | Bring-up, infrequent bytes |
 *  | @ref uart_interrupt | CPU copies through the FIFO | @ref sl_uart_read / @ref sl_uart_write drain or fill the FIFO and return; ready/complete callbacks signal bus events so more operations can be performed | Multi-byte FIFO transfers without DMA |
 *  | @ref uart_async | DMA to or from application buffers | Submit is non-blocking; completion is reported in a callback | Queued buffers, higher throughput |
 *
 *  Polling and interrupt APIs can be used together on the same handle. Do not use
 *  an async DMA transfer and FIFO-based APIs simultaneously on the same direction.
 *  See "Mixing the API sets" for more details.
 *
 *  RX error notification (@ref sl_uart_set_rx_err_callback) is shared by
 *  interrupt-driven and async reception.
 *
 *  ## Integration steps
 *
 *  -# **Add a component.** Include the instantiable `uart` SLC component for a
 *     polling or interrupt-driven instance, or `uart_async` for a DMA-backed
 *     instance. `uart_async` pulls in the DMA Channel Driver and DMA Manager.
 *     The UART driver supports various peripherals. Users can enable support
 *     for a specific peripheral using the `uart_eusart` or `uart_usart` components.
 *     Failure to provide the backend corresponding to the instance's configuration
 *     will cause a compilation error.
 *  -# **Configure the instance.** Set TX, RX, and optional CTS/RTS routing
 *     along with the line settings in the instance configuration (Pin Tool).
 *     Hardware flow control also requires @c SL_UART_FLOW_CONTROL_CTS_RTS in
 *     the line configuration.
 *  -# **Automatic initialization.** Every instance declared through SLC is
 *     initialized and line-configured by the generated
 *     @c sl_uart_instances_init(), which is hooked into driver initialization
 *     and therefore runs automatically when sl_main is present. The
 *     application uses the generated `sl_uart_<instance>_handle` directly
 *     and does not call @ref sl_uart_init() or @ref sl_uart_configure_line().
 *     Without sl_main, call @c sl_uart_instances_init() once during startup.
 *  -# **Transfer data.** Use the polling, interrupt, or async APIs as
 *     described below. See "Mixing the API sets" before combining them.
 *  -# **Power management.** Call @ref sl_uart_suspend() before EM2 entry
 *     (hardware is torn down; the handle and software state remain). After
 *     EM2 exit, call @ref sl_uart_resume() to restore the hardware state.
 *  -# **Tear down.** Call @ref sl_uart_deinit() when the instance is no longer
 *     needed.
 *
 *  Applications that manage their own handles instead of declaring instances
 *  in SLC depend on `uart_core` (and `uart_async_core` for async transfers).
 *  See "Manual instance declaration" under @ref uart_driver for the full
 *  init flow, IRQ handlers, and a copy-paste example.
 *
 *  ## Mixing the API sets
 *
 *  - Polling and interrupt APIs can be combined on the same handle. For
 *    example, use @ref sl_uart_read_byte from the main loop or
 *    @ref sl_uart_read from an RX-ready callback to drain the FIFO.
 *  - An async handle can still use polling and interrupt APIs when that direction
 *    has no active DMA transfer. Check @ref sl_uart_async_is_tx_active() or
 *    @ref sl_uart_async_is_rx_active() first. Concurrent FIFO and DMA access
 *    on the same direction is undefined.
 *  - Do not call @ref sl_uart_configure_line() while RX or TX is active.
 *
 *  ## Typical usage
 *
 *  The examples below use an SLC-generated instance (`inst0`). Generated
 *  instances are already initialized and line-configured, so the application
 *  can transfer data directly through the handle. Start with polling; use the interrupt
 *  or async examples only when those API sets are required.
 *
 *  ### Polling
 *
 *  @code{.c}
 *  #include "sl_uart.h"
 *  #include "sl_uart_instances.h"
 *
 *  void app_process_action(void)
 *  {
 *    uint8_t byte = 0x55;
 *
 *    // Blocks until the byte has been accepted into the TX FIFO.
 *    sl_status_t status = sl_uart_write_byte(&sl_uart_inst0_handle, byte);
 *    if (status != SL_STATUS_OK) {
 *      // Handle error
 *    }
 *
 *    // Non-blocking: returns SL_STATUS_EMPTY when the RX FIFO has no data.
 *    if (sl_uart_read_byte(&sl_uart_inst0_handle, &byte) == SL_STATUS_OK) {
 *      // Process `byte`
 *    }
 *  }
 *  @endcode
 *
 *  ### Interrupt-driven
 *
 *  The CPU empties or fills the FIFO in @ref sl_uart_read() /
 *  @ref sl_uart_write(); those calls return immediately. Ready callbacks run
 *  when more data can be read or sent. See @ref uart_interrupt for TX-ready,
 *  TX-complete, and level-triggered interrupt behavior.
 *
 *  @code{.c}
 *  #include "sl_uart.h"
 *  #include "sl_uart_instances.h"
 *
 *  #define RX_BUFFER_SIZE 32
 *
 *  typedef struct {
 *    uint8_t data[RX_BUFFER_SIZE];
 *    size_t count;
 *  } rx_context_t;
 *
 *  static void on_rx_ready(sl_uart_handle_t *handle, void *user_arg)
 *  {
 *    rx_context_t *rx_ctx = (rx_context_t *)user_arg;
 *
 *    sl_status_t status = sl_uart_read(handle,
 *                                      rx_ctx->data,
 *                                      sizeof(rx_ctx->data),
 *                                      &rx_ctx->count);
 *    if (status == SL_STATUS_OK) {
 *      // rx_ctx->data[0 .. rx_ctx->count - 1] holds the received bytes.
 *    }
 *  }
 *
 *  void app_init(void)
 *  {
 *    static rx_context_t rx_ctx;
 *
 *    sl_uart_set_rx_ready_callback(&sl_uart_inst0_handle, on_rx_ready, &rx_ctx);
 *    sl_uart_enable_rx_ready_interrupt(&sl_uart_inst0_handle);
 *  }
 *  @endcode
 *
 *  ### Async DMA
 *
 *  Requires a `uart_async` instance. Buffers passed to
 *  @ref sl_uart_async_write() and @ref sl_uart_async_read() must remain valid
 *  until the corresponding callback indicates that the buffer has been released.
 *  See @ref uart_async for queueing, abort, RX timeout, and RX event flags.
 *
 *  @code{.c}
 *  #include "sl_uart.h"
 *  #include "sl_uart_instances.h"
 *
 *  static uint8_t tx_buf[32];
 *  static uint8_t rx_buf[32];
 *
 *  static void on_async_tx(sl_uart_handle_t *handle,
 *                          uint8_t *data,
 *                          size_t size,
 *                          void *user_data,
 *                          bool aborted)
 *  {
 *    (void)handle;
 *    (void)user_data;
 *
 *    // TX completed: `size` bytes from `data` have been sent on the bus.
 *    // The buffer can be reused or the next transfer can be submitted.
 *    if(aborted){
 *      // Handle error.
 *    }
 *    // Handle transfer completion.
 *  }
 *
 *  static void on_async_rx(sl_uart_handle_t *handle,
 *                          uint8_t *data,
 *                          size_t size,
 *                          void *user_data,
 *                          sl_uart_async_rx_event_t event)
 *  {
 *    (void)handle;
 *    (void)user_data;
 *
 *    if (event & SL_UART_ASYNC_RX_EVENT_BUF_RELEASED) {
 *      // The driver has released `data`. Consume `size` received bytes
 *      // and reuse or resubmit the buffer.
 *    }
 *  }
 *
 *  void app_init(void)
 *  {
 *    // Assign the RX and TX event callbacks.
 *    sl_uart_async_set_tx_complete_callback(&sl_uart_inst0_handle, on_async_tx, NULL);
 *    sl_uart_async_set_rx_event_callback(&sl_uart_inst0_handle, on_async_rx, NULL);
 *
 *    // Submit the RX and TX transfers.
 *    sl_uart_async_read(&sl_uart_inst0_handle, rx_buf, sizeof(rx_buf));
 *    sl_uart_async_write(&sl_uart_inst0_handle, tx_buf, sizeof(tx_buf));
 *  }
 *  @endcode
 *
 * @{
 ******************************************************************************/

/***************************************************************************//**
 * @addtogroup uart_driver
 * @brief Core UART driver types, handle, and initialization APIs.
 * @details
 *  Allocate, initialize, configure, suspend, and tear down a UART handle.
 *
 *  SLC-generated instances are already initialized and line-configured by generated
 *  @c sl_uart_instances_init(); most applications never call these APIs for
 *  those handles. Use them for manually allocated handles, runtime line
 *  re-configuration, power management (@ref sl_uart_suspend or
 *  @ref sl_uart_resume), and tear-down. Once initialized and line-configured,
 *  transfer with @ref uart_polling, @ref uart_interrupt, or @ref uart_async.
 *
 *  ## Manual instance declaration
 *
 *  Manual instantiation is intended for advanced users who need to use the
 *  UART driver without SLC-generated instances. Prefer declaring
 *  a `uart` or `uart_async` instance through SLC whenever possible for
 *  ease of use.
 *
 *  Applications that do not declare a `uart` / `uart_async` instance through
 *  SLC must set up everything that the generated @c sl_uart_instances.c would
 *  otherwise provide. Depend on `uart_core` (and `uart_async_core` when using
 *  @ref uart_async), then:
 *
 *  -# **Declare pin and line configuration.** Fill a
 *     @ref sl_uart_pin_config_t (TX, RX, and CTS/RTS when hardware flow
 *     control is used) and a @ref sl_uart_config_t (baud rate, parity, stop
 *     bits, data bits, flow control, oversampling). These match the fields
 *     produced from the instance configuration header when using SLC.
 *  -# **Allocate a handle.** Either embed a @ref sl_uart_handle_t in static
 *     storage (as SLC does), call @ref sl_uart_handle_alloc(), or allocate
 *     dynamically with @ref sl_uart_handle_get_size() so the size stays ABI-
 *     compatible when the driver is linked as a library.
 *  -# **Provide the peripheral IRQ handlers.** Override the device
 *     @c \<PERIPH\>_RX_IRQHandler and @c \<PERIPH\>_TX_IRQHandler symbols
 *     (for example @c EUSART0_RX_IRQHandler / @c EUSART0_TX_IRQHandler) and
 *     forward each interrupt to @c sli_uart_rx_irq_handler() /
 *     @c sli_uart_tx_irq_handler() from @c sli_uart.h, passing the address
 *     of the handle. Without these wrappers, interrupt-driven and async
 *     transfers cannot complete.
 *  -# **Initialize, then configure the line.** Call @ref sl_uart_init() with
 *     the peripheral (@c SL_PERIPHERAL_\<PERIPH\>), pin config, and either
 *     @c NULL (polling / interrupt FIFO only) or a non-NULL
 *     @ref sl_uart_async_config_t such as @ref SL_UART_ASYNC_CONFIG_DEFAULT
 *     (async DMA). Then call @ref sl_uart_configure_line() before any
 *     transfer. @ref sl_uart_init() zeroes the handle, so a buffer from
 *     @c malloc() does not need a separate clear.
 *  -# **Transfer, then tear down.** Use @ref uart_polling,
 *     @ref uart_interrupt, or @ref uart_async. Call @ref sl_uart_deinit()
 *     when finished, and free the handle if it was dynamically allocated
 *     (@ref sl_uart_handle_free() or @c free()).
 *
 *  The following examples mirror the generated instance layout for EUSART0, but
 *  allocates the handle dynamically with @ref sl_uart_handle_get_size().
 *  Replace the peripheral, pins, and IRQ handler names for the hardware in
 *  use. Pass @ref SL_UART_ASYNC_CONFIG_DEFAULT instead of @c NULL to enable
 *  async DMA (requires `uart_async_core`).
 *
 *  @code{.c}
 *  #include <stdlib.h>
 *  #include "sl_uart.h"
 *  #include "sli_uart.h"
 *  #include "sl_device_peripheral.h"
 *
 *  static sl_uart_handle_t *uart_handle;
 *
 *  static const sl_uart_pin_config_t pin_config = {
 *    .tx = { SL_GPIO_PORT_A, 5 },
 *    .rx = { SL_GPIO_PORT_A, 6 },
 *    // Set .cts / .rts when using SL_UART_FLOW_CONTROL_CTS_RTS.
 *  };
 *
 *  static const sl_uart_config_t line_config = {
 *    .baudrate     = 115200,
 *    .parity       = SL_UART_PARITY_NONE,
 *    .stop_bits    = SL_UART_STOP_BITS_1,
 *    .data_bits    = SL_UART_DATA_BITS_8,
 *    .flow_control = SL_UART_FLOW_CONTROL_NONE,
 *    .oversampling = SL_UART_OVERSAMPLING_16,
 *  };
 *
 *  // Optional: enable async DMA by passing this to sl_uart_init() instead of NULL.
 *  static const sl_uart_async_config_t async_config = SL_UART_ASYNC_CONFIG_DEFAULT;
 *
 *  void EUSART0_RX_IRQHandler(void)
 *  {
 *    sli_uart_rx_irq_handler(uart_handle);
 *  }
 *
 *  void EUSART0_TX_IRQHandler(void)
 *  {
 *    sli_uart_tx_irq_handler(uart_handle);
 *  }
 *
 *  sl_status_t app_uart_init(void)
 *  {
 *    sl_status_t status;
 *
 *    uart_handle = malloc(sl_uart_handle_get_size());
 *    if (uart_handle == NULL) {
 *      return SL_STATUS_ALLOCATION_FAILED;
 *    }
 *
 *    status = sl_uart_init(uart_handle,
 *                         SL_PERIPHERAL_EUSART0,
 *                         &pin_config,
 *                         &async_config);
 *    if (status != SL_STATUS_OK) {
 *      free(uart_handle);
 *      uart_handle = NULL;
 *      return status;
 *    }
 *
 *    status = sl_uart_configure_line(uart_handle, &line_config);
 *    if (status != SL_STATUS_OK) {
 *      sl_uart_deinit(uart_handle);
 *      free(uart_handle);
 *      uart_handle = NULL;
 *      return status;
 *    }
 *
 *    return SL_STATUS_OK;
 *  }
 *
 *  void app_uart_deinit(void)
 *  {
 *    if (uart_handle != NULL) {
 *      (void)sl_uart_deinit(uart_handle);
 *      free(uart_handle);
 *      uart_handle = NULL;
 *    }
 *  }
 *  @endcode
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
 * Used by interrupt-driven and async reception. See
 * @ref sl_uart_set_rx_err_callback(). Called from interrupt context.
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
 * Typedef for the interrupt-driven RX-ready callback.
 *
 * Invoked when the RX FIFO has data. This is not the async DMA RX event
 * callback (for async DMA RX event callback, see @ref sl_uart_async_on_rx_cb_t instead).
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
 * Typedef for the interrupt-driven TX-ready callback.
 *
 * Invoked when the TX FIFO has room. This is not used by async DMA transmit.
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
 * Typedef for the interrupt-driven TX-complete callback.
 *
 * Invoked when the last byte in the TX FIFO has been sent on the wire. This
 * is not the async DMA TX-complete callback (for async DMA TX complete callback,
 * see @ref sl_uart_async_on_tx_complete_cb_t instead).
 *
 * @param uart_handle Handle to UART.
 *
 * @param user_arg User argument supplied when the callback was registered.
 *
 * @note This function will be called from interrupt context.
 ******************************************************************************/
typedef void (*sl_uart_tx_complete_cb_t)(sl_uart_handle_t *uart_handle,
                                         void *user_arg);

/***************************************************************************//**
 * @addtogroup uart_async Asynchronous Transfer APIs
 * @brief DMA-backed queued transmit and receive.
 * @details
 *  Async APIs submit application buffers to a software queue. The driver
 *  programs DMA for each queued buffer and invokes
 *  @ref sl_uart_async_on_tx_complete_cb_t or @ref sl_uart_async_on_rx_cb_t
 *  when a buffer completes, a receive timeout fires, the driver needs another
 *  RX buffer, or a transfer is aborted.
 *
 *  Requires the `uart_async` component (or `uart_async_core` for manually
 *  allocated handles). SLC-generated `uart_async` instances are already
 *  initialized with async resources by @c sl_uart_instances_init();
 *  For manually allocated handles, pass a non-NULL @ref sl_uart_async_config_t
 *  to @ref sl_uart_init() — for example @ref SL_UART_ASYNC_CONFIG_DEFAULT.
 *  Calling async functions without async resources is not supported.
 *
 *  Use this set for queued DMA transfers, higher throughput, or inter-frame
 *  RX timeout (@ref sl_uart_async_read_set_timeout). For CPU FIFO access
 *  without DMA, use @ref uart_polling or @ref uart_interrupt instead.
 *
 *  @note An async handle can also use the polling and interrupt FIFO APIs on a
 *        direction that has no active DMA transfer. Check
 *        @ref sl_uart_async_is_tx_active() / @ref sl_uart_async_is_rx_active()
 *        first; concurrent FIFO and DMA access on the same direction is
 *        undefined.
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
 *  new RX buffer via @ref sl_uart_async_read() to avoid losing data. See @ref sl_uart_async_on_rx_cb_t
 *  for more details.
 */
#define SL_UART_ASYNC_RX_EVENT_BUF_NEEDED   (1 << 2)

/**
 * Indicates if transfer was aborted. See @ref sl_uart_async_on_rx_cb_t for more details.
 */
#define SL_UART_ASYNC_RX_EVENT_ABORTED      (1 << 3)

typedef uint8_t sl_uart_async_rx_event_t; ///< RX Event bitmap type.

/***************************************************************************//**
 * Typedef for the async DMA TX-complete callback.
 *
 * Called for each buffer submitted with @ref sl_uart_async_write(). This is
 * not the interrupt-driven FIFO TX-complete callback (for interrupt-driven TX complete
 * callback, see @ref sl_uart_tx_complete_cb_t instead).
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
 * @note In case a user submits a new transfer from this callback, the new transfer will be added to
 *       the queue. If the transfer was queued when the @p aborted flag was set to true, the transfer
 *       is conserved, allowing users to implement abortion recovery paths.
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
 * Typedef for the async DMA RX event callback.
 *
 * Called for buffers submitted with @ref sl_uart_async_read() on completion,
 * timeout, buffer-needed, or abort. This is not the interrupt-driven RX-ready
 * callback (for async DMA RX event callback, see @ref sl_uart_async_on_rx_cb_t
 * instead).
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
 *       losing data.
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

#define SL_UART_ASYNC_DMA_CHANNEL_CONFIG_AUTO UINT8_MAX ///< Automatically allocate the DMA channel at init.

/// @brief UART asynchronous DMA transfer configuration.
typedef struct {
  size_t async_tx_transfer_count;       ///< Number of TX transfers that can be queued.
  size_t async_rx_transfer_count;       ///< Number of RX transfers that can be queued.
  uint8_t async_tx_dma_channel_number;  ///< TX DMA channel, or @ref SL_UART_ASYNC_DMA_CHANNEL_CONFIG_AUTO.
  uint8_t async_rx_dma_channel_number;  ///< RX DMA channel, or @ref SL_UART_ASYNC_DMA_CHANNEL_CONFIG_AUTO.
} sl_uart_async_config_t;

/// Default async configuration with auto-allocated DMA channels and a transfer pool of 5.
#define SL_UART_ASYNC_CONFIG_DEFAULT (sl_uart_async_config_t) {                                      \
          .async_tx_transfer_count = 5,                                           \
          .async_rx_transfer_count = 5,                                           \
          .async_tx_dma_channel_number = SL_UART_ASYNC_DMA_CHANNEL_CONFIG_AUTO,   \
          .async_rx_dma_channel_number = SL_UART_ASYNC_DMA_CHANNEL_CONFIG_AUTO,   \
}

/// UART handle state
typedef enum {
  SL_UART_HANDLE_STATE_SUSPENDED = 0,
  SL_UART_HANDLE_STATE_IDLE,
  SL_UART_HANDLE_STATE_ACTIVE,
} sl_uart_handle_state_t;

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
  uint32_t enabled_irq;
#if defined(SL_CATALOG_POWER_MANAGER_PRESENT)
  sl_power_manager_em_t em_requirement;
#endif
#if defined(SL_CATALOG_UART_ASYNC_PRESENT)
  sl_uart_async_config_t async_config;
  bool async_en;
  sl_uart_handle_state_t async_tx_state;
  sl_uart_handle_state_t async_rx_state;
  sl_slist_node_t *async_tx_transfer_submitted_list_head;
  sl_slist_node_t *async_rx_transfer_pending_list_head;
  sl_slist_node_t *async_rx_transfer_active_list_head;
  size_t async_rx_aborted_bytes_completed;
  sl_slist_node_t *async_rx_transfer_aborted_list_head;
  sl_slist_node_t *async_tx_free_list_head;
  sl_slist_node_t *async_rx_free_list_head;
  sl_uart_async_on_tx_complete_cb_t async_tx_complete_cb;
  void* async_tx_complete_cb_user_data;
  sl_uart_async_on_rx_cb_t async_rx_cb;
  void* async_rx_cb_user_data;
  sl_dma_channel_handle_t async_tx_dma_channel;
  sl_dma_channel_handle_t async_rx_dma_channel;
  sl_sleeptimer_timer_handle_t async_rx_timeout_timer;
  uint32_t async_rx_timeout_us;
  uint32_t async_rx_timeout_sw_us;
#endif
  sl_uart_handle_state_t state;
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
 * The handle is completely zeroed before initialization. Dynamically allocated
 * handles (for example via malloc) do not need to be cleared by the caller.
 *
 * Pass @p async_config as NULL to use the polling (@ref uart_polling) and
 * interrupt-driven (@ref uart_interrupt) FIFO APIs only. Pass a non-NULL
 * configuration (for example @ref SL_UART_ASYNC_CONFIG_DEFAULT) to enable
 * async DMA transfers (@ref uart_async). Call @ref sl_uart_configure_line()
 * after init and before any transfer.
 *
 * @note Instances declared through SLC are already initialized by the
 *       generated @c sl_uart_instances_init(). Only call this function for
 *       handles the application owns itself.
 *
 * @param[in]  uart_handle Handle to UART.
 *
 * @param[in]  uart UART peripheral to use with this handle.
 *
 * @param[in]  pin_config Pointer to the pin configuration.
 *
 * @param[in]  async_config Pointer to the asynchronous configuration, or NULL
 *                          for a polling/interrupt-driven instance. When
 *                          non-NULL, async resources (DMA channels and
 *                          transfer pools) are initialized. Use
 *                          @ref SL_UART_ASYNC_CONFIG_DEFAULT for typical
 *                          async setups. After @ref sl_uart_deinit, pass the
 *                          async configuration again to re-initialize as
 *                          async.
 *
 * @note Calling this function on an already-initialized handle (without a
 *       prior @ref sl_uart_deinit) is undefined behavior.
 *
 * @return @ref SL_STATUS_OK if successful.
 *         @ref SL_STATUS_NOT_SUPPORTED if the selected peripheral's UART
 *         backend is not present in the project.
 *         Error code otherwise.
 ******************************************************************************/
sl_status_t sl_uart_init(sl_uart_handle_t *uart_handle,
                         sl_peripheral_t uart,
                         const sl_uart_pin_config_t *pin_config,
                         const sl_uart_async_config_t *async_config);

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
 * Suspends the UART peripheral hardware.
 *
 * De-initializes the underlying UART hardware (peripheral, clocks, pins, and
 * interrupts as required) while leaving the driver handle and its software
 * state unchanged. Intended to be called before EM2 entry.
 *
 * @note The only valid API call after this function is @ref sl_uart_resume() or
 *       @ref sl_uart_deinit(). Calling any other function is undefined behavior.
 *
 * @param[in] uart_handle Handle to UART.
 *
 * @return @ref SL_STATUS_OK if successful.
 *         @ref SL_STATUS_NOT_INITIALIZED if the UART is not initialized.
 *         @ref SL_STATUS_BUSY if the UART peripheral is busy processing a transfer.
 *         Error code otherwise.
 *
 * @note This is not equivalent to @ref sl_uart_deinit(). The handle remains
 *       initialized from the driver's perspective; only the hardware is torn
 *       down so it can be restored with @ref sl_uart_resume().
 ******************************************************************************/
sl_status_t sl_uart_suspend(sl_uart_handle_t *uart_handle);

/***************************************************************************//**
 * Resumes the UART peripheral hardware after @ref sl_uart_suspend().
 *
 * Re-initializes the underlying UART hardware using the configuration stored
 * in the handle (line settings, pin configuration, and async resources as
 * applicable). Does not modify the driver handle or its software state beyond
 * what is required to bring the hardware back up. Intended to be called after EM2 exit.
 *
 * @param[in] uart_handle Handle to UART.
 *
 * @return @ref SL_STATUS_OK if successful.
 *         @ref SL_STATUS_NOT_INITIALIZED if the UART is not initialized.
 *         Error code otherwise.
 ******************************************************************************/
sl_status_t sl_uart_resume(sl_uart_handle_t *uart_handle);

/***************************************************************************//**
 * Applies line configuration.
 *
 * @param[in]  uart_handle Handle to UART.
 *
 * @param[in]  config Pointer to the line configuration.
 *
 * @return @ref SL_STATUS_OK if successful. Error code otherwise.
 *
 * @note This function shall be called after initialization and before any transfer
 *       is initiated. It can be called later to re-configure the line, but it
 *       cannot be called while there is any active RX/TX operations. Applying a
 *       new configuration will reset the UART peripheral and any data left in the
 *       RX or TX FIFO will be lost.
 *
 * @note Instances declared through SLC are already line-configured by the
 *       generated @c sl_uart_instances_init() using the settings from the
 *       instance configuration. Calling this function on such an instance
 *       re-configures the line at runtime.
 *
 * @note Instances using the EUSART peripheral are limited to 7 or 8 data bits.
 *       Instances using the USART peripheral are limited to 4 to 8 data bits.
 ******************************************************************************/
sl_status_t sl_uart_configure_line(sl_uart_handle_t *uart_handle,
                                   const sl_uart_config_t *config);

/***************************************************************************//**
 * Gets active line configuration for the specified UART handle.
 *
 * @param[in]  uart_handle Handle to UART.
 *
 * @param[out]  config Pointer to variable that will receive the line configuration.
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
 * @addtogroup uart_interrupt Interrupt-Driven Transfer APIs
 * @brief CPU FIFO copies that return immediately; callbacks report further progress.
 * @details
 *  Like @ref uart_polling, the interrupt APIs have the CPU copy bytes to and
 *  from the peripheral FIFOs. Unlike polling, @ref sl_uart_read() and
 *  @ref sl_uart_write() do not wait for on-bus completion: they empty or fill
 *  the FIFO as far as it currently allows and return. Callbacks indicate when
 *  more data can be read or sent (RX-ready / TX-ready) or when the last TX
 *  byte has left the wire (TX-complete).
 *
 *  These APIs do not queue application buffers. Call @ref sl_uart_read() or
 *  @ref sl_uart_write() from the ready callbacks (or elsewhere) to continue.
 *
 *  RX-ready and TX-ready interrupts are level-triggered. While enabled, the
 *  callback re-enters as long as the FIFO condition remains true — drain or
 *  fill the FIFO in the callback, or disable the interrupt.
 *
 *  @note Do not call @ref sl_uart_read() / @ref sl_uart_write() on a direction
 *        that has an active async DMA transfer.
 *
 * @{
 ******************************************************************************/

/***************************************************************************//**
 * Registers a callback that is invoked when a parity, framing, or overflow
 * RX error is detected.
 *
 * Shared by interrupt-driven and async reception. Calling this function
 * enables the PARITY, FRAMING, and OVERFLOW error IRQ signals. The callback
 * runs from interrupt context.
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
 * Registers the interrupt-driven RX-ready callback.
 *
 * The callback is invoked from interrupt context while RX data is present in
 * the FIFO (level-triggered). Register it before
 * @ref sl_uart_enable_rx_ready_interrupt(). Drain the FIFO with
 * @ref sl_uart_read() or @ref sl_uart_read_byte(), or disable the interrupt,
 * to stop further notifications. This is not the async RX event callback
 * (for async DMA RX event callback, see @ref sl_uart_async_on_rx_cb_t instead).
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
 * Registers the interrupt-driven TX-ready callback.
 *
 * The callback is invoked from interrupt context while the TX FIFO has room
 * (level-triggered). Register it before
 * @ref sl_uart_enable_tx_ready_interrupt(). Fill the FIFO with
 * @ref sl_uart_write() or @ref sl_uart_write_byte(), or disable the interrupt,
 * to stop further notifications.
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
 * Registers the interrupt-driven TX-complete callback.
 *
 * The callback is invoked from interrupt context after the last byte in the
 * TX FIFO has been sent. This is not the async DMA TX-complete callback
 * (for async DMA TX complete callback, see @ref sl_uart_async_on_tx_complete_cb_t instead).
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
 * Enables the interrupt-driven RX-ready interrupt.
 *
 * @note The RX ready interrupt is level-based. It fires while RX data is present
 *       in the FIFO. If the FIFO already contains data when this function is called,
 *       the interrupt will fire immediately. A callback must already be
 *       registered with @ref sl_uart_set_rx_ready_callback().
 *
 * @param[in]  uart_handle Handle to UART.
 ******************************************************************************/
void sl_uart_enable_rx_ready_interrupt(sl_uart_handle_t *uart_handle);

/***************************************************************************//**
 * Enables the interrupt-driven TX-ready interrupt.
 *
 * @note The TX ready interrupt is level-based. It fires while there is room in
 *       the TX FIFO. If the FIFO already has space when this function is called,
 *       the interrupt will fire immediately. A callback must already be
 *       registered with @ref sl_uart_set_tx_ready_callback().
 *
 * @param[in]  uart_handle Handle to UART.
 ******************************************************************************/
void sl_uart_enable_tx_ready_interrupt(sl_uart_handle_t *uart_handle);

/***************************************************************************//**
 * Enables the interrupt-driven TX-complete interrupt.
 *
 * @note The TX complete interrupt will fire as soon as all bytes in the TX FIFO
 *       have been sent. If the FIFO was already empty, the interrupt will not fire.
 *       A callback must already be registered with
 *       @ref sl_uart_set_tx_complete_callback().
 *
 * @param[in]  uart_handle Handle to UART.
 ******************************************************************************/
void sl_uart_enable_tx_complete_interrupt(sl_uart_handle_t *uart_handle);

/***************************************************************************//**
 * Disables the interrupt-driven RX-ready interrupt.
 *
 * Stops further RX-ready callbacks. Does not discard data already in the FIFO.
 *
 * @param[in]  uart_handle Handle to UART.
 ******************************************************************************/
void sl_uart_disable_rx_ready_interrupt(sl_uart_handle_t *uart_handle);

/***************************************************************************//**
 * Disables the interrupt-driven TX-ready interrupt.
 *
 * Stops further TX-ready callbacks. Does not flush the TX FIFO.
 *
 * @param[in]  uart_handle Handle to UART.
 ******************************************************************************/
void sl_uart_disable_tx_ready_interrupt(sl_uart_handle_t *uart_handle);

/***************************************************************************//**
 * Disables the interrupt-driven TX-complete interrupt.
 *
 * Stops further TX-complete callbacks for the interrupt-driven API.
 *
 * @param[in]  uart_handle Handle to UART.
 ******************************************************************************/
void sl_uart_disable_tx_complete_interrupt(sl_uart_handle_t *uart_handle);

/** @} (end addtogroup uart_interrupt) */

/***************************************************************************//**
 * @addtogroup uart_polling Polling Transfer APIs
 * @brief CPU transfers one byte through the FIFO and waits in the call for TX.
 * @details
 *  @ref sl_uart_write_byte() blocks until the byte is accepted into the TX
 *  FIFO. @ref sl_uart_read_byte() returns one byte if the RX FIFO has data,
 *  otherwise @ref SL_STATUS_EMPTY (call again to poll). No UART ready
 *  interrupts and no DMA.
 *
 *  Use for infrequent bytes or bring-up. For multi-byte FIFO copies that
 *  return immediately, see @ref uart_interrupt. For queued DMA transfers,
 *  see @ref uart_async.
 *
 * @{
 ******************************************************************************/

/***************************************************************************//**
 * Reads one byte from the RX FIFO (polling).
 *
 * The CPU reads the peripheral FIFO directly. Unlike @ref sl_uart_read(),
 * there is no RX-ready callback to notify when a byte arrives. Returns
 * @ref SL_STATUS_EMPTY immediately if the FIFO is empty — this call does not
 * block waiting for data; poll again later or use the interrupt-driven APIs.
 * For DMA reception, use @ref sl_uart_async_read().
 *
 * @param[in]  uart_handle Handle to UART.
 *
 * @param[out] byte Pointer to location where received byte will be stored.
 *
 * @return @ref SL_STATUS_OK if successful. @ref SL_STATUS_EMPTY if no byte received.
 *         Error code otherwise.
 *
 * @note This function can be called from IRQ context.
 *
 * @note The line must be configured before any transfer, either by the
 *       generated init for SLC instances or by @ref sl_uart_configure_line().
 *       Failure to do so is undefined behavior.
 *
 * @note Do not call this function while an async RX transfer is active.
 *       Ensure @ref sl_uart_async_is_rx_active() returns false. Concurrent
 *       FIFO and DMA access on RX is undefined.
 ******************************************************************************/
sl_status_t sl_uart_read_byte(sl_uart_handle_t *uart_handle, uint8_t *byte);

/***************************************************************************//**
 * Writes one byte to the TX FIFO (polling).
 *
 * Blocks the CPU until the byte is accepted into the TX FIFO. Unlike
 * @ref sl_uart_write(), this call waits in the function instead of returning
 * and using a TX-ready interrupt to continue. It does not wait for the byte
 * to finish on the bus; use the interrupt-driven TX-complete callback or
 * async TX complete for that. For DMA transmission, use
 * @ref sl_uart_async_write().
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
 *
 * @note The line must be configured before any transfer, either by the
 *       generated init for SLC instances or by @ref sl_uart_configure_line().
 *       Failure to do so is undefined behavior.
 *
 * @note Do not call this function while an async TX transfer is active.
 *       Ensure @ref sl_uart_async_is_tx_active() returns false. Concurrent
 *       FIFO and DMA access on TX is undefined.
 *
 * @note This function can block for an indefinite period of time if the TX FIFO is full.
 *       and the remote device enforces its hardware flow control.
 ******************************************************************************/
sl_status_t sl_uart_write_byte(sl_uart_handle_t *uart_handle, uint8_t byte);

/** @} (end addtogroup uart_polling) */

/***************************************************************************//**
 * @addtogroup uart_interrupt
 * @{
 ******************************************************************************/

/***************************************************************************//**
 * Copies bytes from the RX FIFO into a buffer (interrupt-driven).
 *
 * The CPU still reads the peripheral FIFO. Empties the FIFO into @p data
 * until the FIFO is empty or @p size is reached, then returns. Does not block
 * until a full buffer has arrived on the bus. Enable the RX-ready interrupt
 * so a callback can call this again when more data can be read. For a
 * single-byte poll with no interrupt, use @ref sl_uart_read_byte(). For DMA
 * reception, use @ref sl_uart_async_read().
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
 * @note This function can be called from IRQ context.
 *
 * @note The line must be configured before any transfer, either by the
 *       generated init for SLC instances or by @ref sl_uart_configure_line().
 *       Failure to do so is undefined behavior.
 *
 * @note Do not call this function while an async RX transfer is active.
 *       Ensure @ref sl_uart_async_is_rx_active() returns false. Concurrent
 *       FIFO and DMA access on RX is undefined.
 ******************************************************************************/
sl_status_t sl_uart_read(sl_uart_handle_t *uart_handle,
                         void *data,
                         size_t size,
                         size_t *read_size);

/***************************************************************************//**
 * Copies bytes from a buffer into the TX FIFO (interrupt-driven).
 *
 * The CPU still writes the peripheral FIFO. Fills the FIFO from @p data until
 * the FIFO is full or @p size is reached, then returns. Does not block until
 * the bytes have been sent on the bus. Enable the TX-ready interrupt so a
 * callback can call this again when more data can be sent, and the TX-complete
 * interrupt to learn when the last byte has left the wire. For a call that
 * waits until one byte is accepted into the FIFO, use @ref sl_uart_write_byte().
 * For DMA transmission, use @ref sl_uart_async_write().
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
 * @note This function can be called from IRQ context.
 *
 * @note The line must be configured before any transfer, either by the
 *       generated init for SLC instances or by @ref sl_uart_configure_line().
 *       Failure to do so is undefined behavior.
 *
 * @note Do not call this function while an async TX transfer is active.
 *       Ensure @ref sl_uart_async_is_tx_active() returns false. Concurrent
 *       FIFO and DMA access on TX is undefined.
 ******************************************************************************/
sl_status_t sl_uart_write(sl_uart_handle_t *uart_handle,
                          const void *data,
                          const size_t size,
                          size_t *write_size);

/** @} (end addtogroup uart_interrupt) */

/***************************************************************************//**
 * @addtogroup uart_async
 * @{
 ******************************************************************************/

/***************************************************************************//**
 * Registers the async DMA TX-complete callback.
 *
 * Invoked once per buffer submitted to @ref sl_uart_async_write(), including
 * buffers released by @ref sl_uart_async_abort_tx(). This is not the
 * interrupt-driven FIFO TX-complete callback (for interrupt-driven TX complete callback,
 * see @ref sl_uart_tx_complete_cb_t instead).
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
 * Registers the async DMA RX event callback.
 *
 * Invoked when a buffer submitted to @ref sl_uart_async_read() completes,
 * times out, needs a follow-on buffer, or is aborted. This is not the
 * interrupt-driven RX-ready callback (for interrupt-driven RX ready callback,
 * see @ref sl_uart_rx_ready_cb_t instead).
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
 * Queues a buffer for DMA transmit (async).
 *
 * Returns immediately after the buffer is queued. The buffer must remain
 * valid until @ref sl_uart_async_on_tx_complete_cb_t reports it is released.
 * Additional calls are queued up to @ref sl_uart_async_config_t
 * async_tx_transfer_count. For CPU FIFO writes, use @ref sl_uart_write() or
 * @ref sl_uart_write_byte() instead.
 *
 * @param[in] uart_handle Handle to UART.
 *
 * @param[in] data Pointer to data buffer to transmit.
 *
 * @param[in] size Size of the buffer.
 *
 * @return @ref SL_STATUS_OK if successful.
 *         @ref SL_STATUS_EMPTY if @p size is 0.
 *         @ref SL_STATUS_BUSY if the maximum number of queued TX transfers has been reached.
 *         Error code otherwise.
 *
 * @note This function is non blocking. Once transfer is completed, the callback
 *       specified with @ref sl_uart_async_set_tx_complete_callback() will be called.
 *
 * @note When submitting a transfer before the previous completes, it will be
 *       queued and scheduled for transmission once the previous one has been
 *       transmitted entirely over the bus.
 *
 * @note This function can be called from an ISR.
 *
 * @note The line must be configured before any transfer, either by the
 *       generated init for SLC instances or by @ref sl_uart_configure_line().
 *       Failure to do so is undefined behavior.
 ******************************************************************************/
sl_status_t sl_uart_async_write(sl_uart_handle_t *uart_handle,
                                const void *data,
                                const size_t size);

/***************************************************************************//**
 * Aborts all queued and in-progress async DMA transmits.
 *
 * Stops DMA transmission and releases every buffer submitted with
 * @ref sl_uart_async_write(). Each released buffer is reported through
 * @ref sl_uart_async_on_tx_complete_cb_t with @p aborted set to true.
 *
 * @param[in] uart_handle Handle to UART.
 *
 * @return @ref SL_STATUS_OK if successful.
 *         Error code otherwise.
 ******************************************************************************/
sl_status_t sl_uart_async_abort_tx(sl_uart_handle_t *uart_handle);

/***************************************************************************//**
 * Returns whether an async DMA transmit is queued or in progress.
 *
 * Use this before calling polling or interrupt-driven TX APIs on an async
 * handle.
 *
 * @param[in]  uart_handle Handle to UART.
 *
 * @return true if there is an active TX. False, otherwise.
 ******************************************************************************/
bool sl_uart_async_is_tx_active(sl_uart_handle_t *uart_handle);

/***************************************************************************//**
 * Queues a buffer for DMA receive (async).
 *
 * Returns immediately after the buffer is queued. The buffer must remain
 * valid until the RX event callback reports
 * @ref SL_UART_ASYNC_RX_EVENT_BUF_RELEASED. Submit another buffer when
 * @ref SL_UART_ASYNC_RX_EVENT_BUF_NEEDED is set to avoid dropping data. For
 * CPU FIFO reads, use @ref sl_uart_read() or @ref sl_uart_read_byte() instead.
 *
 * @param[in]  uart_handle Handle to UART.
 *
 * @param[in]  data Pointer to buffer that will receive data.
 *
 * @param[in]  size Size of the buffer, in bytes.
 *
 * @return @ref SL_STATUS_OK if successful.
 *         @ref SL_STATUS_EMPTY if @p size is 0.
 *         @ref SL_STATUS_BUSY if the maximum number of queued RX transfers has been reached.
 *         Error code otherwise.
 *
 * @note This function is non blocking. Once transfer is completed, the callback
 *       specified with @ref sl_uart_async_set_rx_event_callback() will be called.
 *
 * @note This function can be called from an ISR.
 *
 * @note The line must be configured before any transfer, either by the
 *       generated init for SLC instances or by @ref sl_uart_configure_line().
 *       Failure to do so is undefined behavior.
 ******************************************************************************/
sl_status_t sl_uart_async_read(sl_uart_handle_t *uart_handle,
                               void *data,
                               const size_t size);

/***************************************************************************//**
 * Aborts all queued and in-progress async DMA receives and disables RX.
 *
 * Releases every buffer submitted with @ref sl_uart_async_read(). Each
 * released buffer is reported through @ref sl_uart_async_on_rx_cb_t with
 * @ref SL_UART_ASYNC_RX_EVENT_ABORTED and
 * @ref SL_UART_ASYNC_RX_EVENT_BUF_RELEASED set.
 *
 * @param[in]  uart_handle Handle to UART.
 *
 * @return @ref SL_STATUS_OK if successful. Error code otherwise.
 ******************************************************************************/
sl_status_t sl_uart_async_disable_rx(sl_uart_handle_t *uart_handle);

/***************************************************************************//**
 * Returns whether an async DMA receive is queued or in progress.
 *
 * Use this before calling polling or interrupt-driven RX APIs on an async
 * handle.
 *
 * @param[in]  uart_handle Handle to UART.
 *
 * @return true if there is an active RX. False, otherwise.
 ******************************************************************************/
bool sl_uart_async_is_rx_active(sl_uart_handle_t *uart_handle);

/***************************************************************************//**
 * Sets the async inter-frame RX timeout.
 *
 * Async-only. The timeout is the maximum idle time between two received UART
 * frames; it is not a timeout to fill an entire buffer. It does not apply to
 * @ref sl_uart_read() or @ref sl_uart_read_byte().
 *
 * @param[in]  uart_handle Handle to UART.
 *
 * @param[in]  timeout_us Timeout, in microseconds. 0 means wait forever.
 *
 * @return SL_STATUS_OK if successful.
 *         Error code otherwise.
 *
 * @note The timeout specified by argument @p timeout_us is the maximum time to wait
 *       between 2 received UART frames. It does NOT represent a timeout to fill an
 *       entire buffer. If the timeout is reached, the RX event callback will be called
 *       with the @ref SL_UART_ASYNC_RX_EVENT_TIMEOUT event set, without aborting the
 *       current transfer, with the appropriate number of bytes received so far.
 *       Upon receiving the next frame, the timeout will be reset, meaning that the timeout
 *       can trigger multiple times for a single transfer. Once all transfers have been completed,
 *       the timeout will be disabled, and start again upon starting the next transfer.
 *
 * @note If a timeout was already set and this API is called, any pending timeout will be cancelled
 *       and the new timeout will be armed only upon receiving a new UART frame.
 *
 * @note This API must be called once the line is configured, since the timeout
 *       value is computed from the configured baud rate. Failure to do so is
 *       undefined behavior.
 *
 * @note If this API is never called, the default value is 0, meaning no timeout.
 *
 * @note This API is not supported on EUART-based instances due to the lack of
 *       hardware RX timeout support (i.e.: xG22 devices). Calling this API on such
 *       instances will return SL_STATUS_NOT_SUPPORTED.
 ******************************************************************************/
sl_status_t sl_uart_async_read_set_timeout(sl_uart_handle_t *uart_handle,
                                           uint32_t timeout_us);

/** @} (end addtogroup uart_async) */

/** @} (end addtogroup uart) */

#ifdef __cplusplus
}
#endif

#endif // SL_UART_H
