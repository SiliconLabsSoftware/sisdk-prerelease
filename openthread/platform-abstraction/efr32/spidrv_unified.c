/*
 *  Copyright (c) 2025, The OpenThread Authors.
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are met:
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *  3. Neither the name of the copyright holder nor the
 *     names of its contributors may be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 *  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * @file
 *   This file implements the OpenThread platform abstraction for SPI communication.
 *
 */

#ifdef SL_COMPONENT_CATALOG_PRESENT
#include "sl_component_catalog.h"
#endif // SL_COMPONENT_CATALOG_PRESENT

#include "sl_core.h"

// Include em_device.h early to get series defines
#include "em_device.h"

// GPIO: unified HAL for Series 2 and Series 3
#include "sl_device_gpio.h"
#include "sl_gpio.h"
#include "sl_hal_gpio.h"
#if !defined(_SILICON_LABS_32B_SERIES_2) && !defined(_SILICON_LABS_32B_SERIES_3)
#error "Series 1 devices are not supported by this component"
#endif

#include "sl_device_dma.h"
#include "sl_device_peripheral.h"
#include "sl_dma_channel.h"
#include "sl_dma_channel_device.h"

#include "spidrv.h"

#include "sl_clock_manager.h"
#if defined(SL_CATALOG_POWER_MANAGER_PRESENT)
#include "sl_power_manager.h"
#endif

// Conditional config header includes
#if defined(_SILICON_LABS_32B_SERIES_2)
#include "sl_ncp_spidrv_usart_config.h"
#elif defined(_SILICON_LABS_32B_SERIES_3)
#include "sl_ncp_spidrv_eusart_config.h"
#endif

#include "platform-efr32.h"
#include <openthread-system.h>
#include <openthread/error.h>
#include <openthread/platform/spi-slave.h>
#include "common/code_utils.hpp"
#include "common/debug.hpp"

#define SL_OT_SPIDRV_SPI_CONCAT_PASTER(first, second, third) first##second##third

// Peripheral DMA request signals (sl_dma_signal_t / SL_DMA_SIGNAL_*)
#if defined(_SILICON_LABS_32B_SERIES_2)
#define SL_OT_SPIDRV_SPI_DMA_RX_SIGNAL(periph_nbr) \
    SL_OT_SPIDRV_SPI_CONCAT_PASTER(SL_DMA_SIGNAL_USART, periph_nbr, _RXDATAV)
#define SL_OT_SPIDRV_SPI_DMA_TX_SIGNAL(periph_nbr) \
    SL_OT_SPIDRV_SPI_CONCAT_PASTER(SL_DMA_SIGNAL_USART, periph_nbr, _TXBL)
#elif defined(_SILICON_LABS_32B_SERIES_3)
#define SL_OT_SPIDRV_SPI_DMA_RX_SIGNAL(periph_nbr) \
    SL_OT_SPIDRV_SPI_CONCAT_PASTER(SL_DMA_SIGNAL_EUSART, periph_nbr, _RXFL)
#define SL_OT_SPIDRV_SPI_DMA_TX_SIGNAL(periph_nbr) \
    SL_OT_SPIDRV_SPI_CONCAT_PASTER(SL_DMA_SIGNAL_EUSART, periph_nbr, _TXFL)
#endif

// ============================================================================
// Configuration Macros (to reduce repetition)
// ============================================================================

#if defined(_SILICON_LABS_32B_SERIES_2)
#define SPI_CS_PORT SL_NCP_SPIDRV_USART_CS_PORT
#define SPI_CS_PIN SL_NCP_SPIDRV_USART_CS_PIN
#define SPI_CS_RISING_EDGE_INT SL_NCP_SPIDRV_USART_CS_RISING_EDGE_INT_NO
#define SPI_CS_FALLING_EDGE_INT SL_NCP_SPIDRV_USART_CS_FALLING_EDGE_INT_NO
#define SPI_HOST_INT_PORT SL_NCP_SPIDRV_USART_HOST_INT_PORT
#define SPI_HOST_INT_PIN SL_NCP_SPIDRV_USART_HOST_INT_PIN
#define SPI_PERIPHERAL_NO SL_NCP_SPIDRV_USART_PERIPHERAL_NO
#elif defined(_SILICON_LABS_32B_SERIES_3)
#define SPI_CS_PORT SL_NCP_SPIDRV_EUSART_CS_PORT
#define SPI_CS_PIN SL_NCP_SPIDRV_EUSART_CS_PIN
#define SPI_CS_RISING_EDGE_INT SL_NCP_SPIDRV_EUSART_CS_RISING_EDGE_INT_NO
#define SPI_CS_FALLING_EDGE_INT SL_NCP_SPIDRV_EUSART_CS_FALLING_EDGE_INT_NO
#define SPI_HOST_INT_PORT SL_NCP_SPIDRV_EUSART_HOST_INT_PORT
#define SPI_HOST_INT_PIN SL_NCP_SPIDRV_EUSART_HOST_INT_PIN
#define SPI_PERIPHERAL_NO SL_NCP_SPIDRV_EUSART_PERIPHERAL_NO
#endif

//  Member variables
static volatile bool should_process_transaction = false;
static bool          s_ot_spi_slave_ready       = false;

static uint8_t default_tx_value;

static sl_dma_channel_xfer_descriptor_t tx_dma_descriptors[2];
static sl_dma_channel_xfer_descriptor_t rx_dma_descriptor;

static sl_dma_channel_transfer_t tx_dma_transfers[2];
static sl_dma_channel_transfer_t rx_dma_transfer;

// Sizes matching legacy two-descriptor TX layout (seg0 + seg1) for transaction byte counting
static uint16_t s_tx_seg0_bytes;
static uint16_t s_tx_seg1_bytes;

// Last prepared buffers reported to the complete callback
static uint8_t *s_tx_output_buf;
static uint16_t s_tx_output_len;
static uint8_t *s_rx_buf;
static uint16_t s_rx_len;

// Transaction events callback
static volatile otPlatSpiSlaveTransactionCompleteCallback complete_callback;
static volatile otPlatSpiSlaveTransactionProcessCallback  process_callback;
static volatile void                                     *context;

// SPI Peripheral
static volatile SPIDRV_HandleData_t sl_spidrv_handle_data;

static sl_dma_channel_handle_t *rcp_spidrv_plat_tx_dma(void)
{
    return (sl_dma_channel_handle_t *)&sl_spidrv_handle_data.txDMACh;
}

static sl_dma_channel_handle_t *rcp_spidrv_plat_rx_dma(void)
{
    return (sl_dma_channel_handle_t *)&sl_spidrv_handle_data.rxDMACh;
}

// ============================================================================
// DMA helpers (DMA Channel driver)
// ============================================================================

static void rcp_spidrv_dma_setup_tx_transfer(sl_dma_channel_transfer_t        *t,
                                             void                             *src,
                                             void                             *dst_reg,
                                             size_t                            size,
                                             bool                              inc_src,
                                             sl_dma_channel_xfer_descriptor_t *desc)
{
    *t = (sl_dma_channel_transfer_t){
        .source                = src,
        .destination           = dst_reg,
        .size                  = size,
        .unit_size             = SL_DMA_CTRL_SIZE_BYTE,
        .block_size            = SL_DMA_CTRL_BLOCK_SIZE_UNIT_1,
        .increment_source      = inc_src,
        .increment_destination = false,
        .block_handshake_mode  = true,
        .callback_on_complete  = false,
        .cacheable             = false,
        .descriptor            = desc,
        .next                  = NULL,
    };
}

static void rcp_spidrv_dma_setup_rx_transfer(sl_dma_channel_transfer_t        *t,
                                             void                             *src_reg,
                                             void                             *dst,
                                             size_t                            size,
                                             sl_dma_channel_xfer_descriptor_t *desc)
{
    *t = (sl_dma_channel_transfer_t){
        .source                = src_reg,
        .destination           = dst,
        .size                  = size,
        .unit_size             = SL_DMA_CTRL_SIZE_BYTE,
        .block_size            = SL_DMA_CTRL_BLOCK_SIZE_UNIT_1,
        .increment_source      = false,
        .increment_destination = true,
        .block_handshake_mode  = true,
        .callback_on_complete  = false,
        .cacheable             = false,
        .descriptor            = desc,
        .next                  = NULL,
    };
}

// Snapshots TX progress from LDMA channel registers (must run before @ref sl_dma_channel_abort).
static uint32_t rcp_spidrv_dma_tx_compute_transaction_size(const sl_dma_channel_handle_t *handle,
                                                           uint16_t                       seg0_bytes,
                                                           uint16_t                       seg1_bytes,
                                                           uint32_t                       ctrl_reg,
                                                           uint32_t                       link_reg,
                                                           uint32_t                       fifo_tx_count)
{
    (void)handle;
    uint32_t remaining = ((ctrl_reg & _LDMA_CH_CTRL_XFERCNT_MASK) >> _LDMA_CH_CTRL_XFERCNT_SHIFT) + 1U;
    remaining += fifo_tx_count;

    uint32_t link_rel  = (link_reg & _LDMA_CH_LINK_LINK_MASK) >> _LDMA_CH_LINK_LINK_SHIFT;
    uint32_t xfer_cnt0 = (uint32_t)seg0_bytes - 1U;

    if (link_rel == 0U)
    {
        return (uint32_t)seg0_bytes + (uint32_t)seg1_bytes - remaining;
    }
    return (xfer_cnt0 - remaining) + 1U;
}

// ============================================================================
// Generic GPIO Helper Functions
// ============================================================================

static void rcp_spidrv_gpio_set_host_request(void)
{
#if defined(SPI_HOST_INT_PORT) && defined(SPI_HOST_INT_PIN)
    const sl_gpio_t host_int_gpio = {.port = SPI_HOST_INT_PORT, .pin = SPI_HOST_INT_PIN};
    sl_hal_gpio_clear_pin(&host_int_gpio);
#endif
}

static void rcp_spidrv_gpio_deassert_host_request(void)
{
#if defined(SPI_HOST_INT_PORT) && defined(SPI_HOST_INT_PIN)
    const sl_gpio_t host_int_gpio = {.port = SPI_HOST_INT_PORT, .pin = SPI_HOST_INT_PIN};
    sl_hal_gpio_set_pin(&host_int_gpio);
#endif
}

__attribute__((used)) static void rcp_spidrv_gpio_set_pin_mode(uint32_t port, uint32_t pin, uint32_t mode, uint32_t out)
{
    const sl_gpio_t gpio = {.port = port, .pin = pin};
    sl_hal_gpio_set_pin_mode(&gpio, mode, out);
}

static bool rcp_spidrv_gpio_get_pin_input(uint32_t port, uint32_t pin)
{
    const sl_gpio_t gpio = {.port = port, .pin = pin};
    return sl_hal_gpio_get_pin_input(&gpio) != 0U;
}

// Unified CS pin check helper
static bool rcp_spidrv_gpio_is_cs_high(void)
{
    return rcp_spidrv_gpio_get_pin_input(SPI_CS_PORT, SPI_CS_PIN);
}

// Forward declaration for interrupt callback
static void rcp_spidrv_spi_transaction_end_interrupt(uint8_t intNo, void *ctx);

__attribute__((used)) static void rcp_spidrv_gpio_configure_cs_interrupt(uint32_t port,
                                                                         uint32_t pin,
                                                                         uint32_t int_no,
                                                                         bool     rising_edge)
{
    const sl_gpio_t cs_gpio = {.port = port, .pin = pin};
    int32_t         intNo   = (int32_t)int_no;
    sl_status_t     status  = sl_gpio_configure_external_interrupt(&cs_gpio,
                                                              &intNo,
                                                              rising_edge ? SL_GPIO_INTERRUPT_RISING_EDGE
                                                                               : SL_GPIO_INTERRUPT_FALLING_EDGE,
                                                              rcp_spidrv_spi_transaction_end_interrupt,
                                                              NULL);
    OT_ASSERT(status == SL_STATUS_OK);
}

__attribute__((used)) static void rcp_spidrv_gpio_deconfigure_cs_interrupt(uint32_t int_no)
{
    IgnoreReturnValue(sl_gpio_deconfigure_external_interrupt((int32_t)int_no));
}

// ============================================================================
// Generic Peripheral Helper Functions
// ============================================================================

static uint32_t rcp_spidrv_peripheral_get_tx_status(void)
{
#if defined(_SILICON_LABS_32B_SERIES_2)
    return sl_spidrv_handle_data.peripheral.usartPort->STATUS & _USART_STATUS_TXBUFCNT_MASK;
#elif defined(_SILICON_LABS_32B_SERIES_3)
    return sl_spidrv_handle_data.peripheral.eusartPort->STATUS & _EUSART_STATUS_TXFCNT_MASK;
#endif
}

static uint32_t rcp_spidrv_peripheral_get_tx_status_shifted(void)
{
#if defined(_SILICON_LABS_32B_SERIES_2)
    return (sl_spidrv_handle_data.peripheral.usartPort->STATUS & _USART_STATUS_TXBUFCNT_MASK)
           >> _USART_STATUS_TXBUFCNT_SHIFT;
#elif defined(_SILICON_LABS_32B_SERIES_3)
    return (sl_spidrv_handle_data.peripheral.eusartPort->STATUS & _EUSART_STATUS_TXFCNT_MASK)
           >> _EUSART_STATUS_TXFCNT_SHIFT;
#endif
}

static uint32_t rcp_spidrv_peripheral_get_rx_status(void)
{
#if defined(_SILICON_LABS_32B_SERIES_2)
    return sl_spidrv_handle_data.peripheral.usartPort->STATUS & _USART_STATUS_RXDATAV_MASK;
#elif defined(_SILICON_LABS_32B_SERIES_3)
    return sl_spidrv_handle_data.peripheral.eusartPort->STATUS & _EUSART_STATUS_RXFL_MASK;
#endif
}

static void rcp_spidrv_peripheral_clear_fifos(void)
{
#if defined(_SILICON_LABS_32B_SERIES_2)
    sl_spidrv_handle_data.peripheral.usartPort->CMD = USART_CMD_CLEARTX | USART_CMD_CLEARRX;
#elif defined(_SILICON_LABS_32B_SERIES_3)
    EUSART_TypeDef *eusart = sl_spidrv_handle_data.peripheral.eusartPort;
    // EUSART_CMD_CLEARTX reportedly only affects UART, not SPI mode,
    // and there is no EUSART_CMD_CLEARRX. Only way to clear the
    // FIFOs is via the big hammer of disabling then reenabling it.
    sl_hal_eusart_disable_rx(eusart);
    sl_hal_eusart_disable_tx(eusart);
    sl_hal_eusart_disable(eusart);
    while (eusart->EN & _EUSART_EN_DISABLING_MASK);
    sl_hal_eusart_enable(eusart);
    sl_hal_eusart_enable_rx(eusart);
    sl_hal_eusart_enable_tx(eusart);
    sl_hal_eusart_wait_sync(eusart, _EUSART_SYNCBUSY_MASK);
#endif
}

static void rcp_spidrv_peripheral_clear_rx_fifo(void)
{
#if defined(_SILICON_LABS_32B_SERIES_2)
    sl_spidrv_handle_data.peripheral.usartPort->CMD = USART_CMD_CLEARRX;
#elif defined(_SILICON_LABS_32B_SERIES_3)
    // Cannot directly clear rx fifo on S3; instead, clear rx and tx
    rcp_spidrv_peripheral_clear_fifos();
#endif
}

static void rcp_spidrv_peripheral_clear_tx_fifo(void)
{
#if defined(_SILICON_LABS_32B_SERIES_2)
    sl_spidrv_handle_data.peripheral.usartPort->CMD = USART_CMD_CLEARTX;
#elif defined(_SILICON_LABS_32B_SERIES_3)
    // Cannot directly clear tx fifo on S3; instead, clear tx and rx
    rcp_spidrv_peripheral_clear_fifos();
#endif
}

static void rcp_spidrv_peripheral_wait_tx_fifo_empty(void)
{
#if defined(_SILICON_LABS_32B_SERIES_2)
    while (sl_spidrv_handle_data.peripheral.usartPort->STATUS & _USART_STATUS_TXBUFCNT_MASK);
#endif
    // Note: on series 3, it is assumed that we run rcp_spidrv_peripheral_clear_fifos() before this, which already
    // involves waiting for the fifos to empty. Thus, no action is required here for S3.
}

static void rcp_spidrv_peripheral_wait_rx_fifo_empty(void)
{
#if defined(_SILICON_LABS_32B_SERIES_2)
    while (sl_spidrv_handle_data.peripheral.usartPort->STATUS & _USART_STATUS_RXDATAV_MASK);
#endif
    // Note: on series 3, it is assumed that we run rcp_spidrv_peripheral_clear_fifos() before this, which already
    // involves waiting for the fifos to empty. Thus, no action is required here for S3.
}

// RX: clear stale RX data; on Series 2, block until STATUS shows RX drained.
static void rcp_spidrv_peripheral_clear_rx_fifo_and_drain(void)
{
    rcp_spidrv_peripheral_clear_rx_fifo();
    rcp_spidrv_peripheral_wait_rx_fifo_empty();
}

// TX: clear non-empty TX FIFO if needed; always run drain (Series 2: STATUS poll;
// Series 3: wait is a no-op after clear_fifos).
static void rcp_spidrv_peripheral_clear_tx_fifo_if_needed_and_drain(void)
{
    if (rcp_spidrv_peripheral_get_tx_status())
    {
        rcp_spidrv_peripheral_clear_tx_fifo();
    }
    rcp_spidrv_peripheral_wait_tx_fifo_empty();
}

static volatile void *rcp_spidrv_peripheral_get_rxdata_addr(void)
{
#if defined(_SILICON_LABS_32B_SERIES_2)
    return &(sl_spidrv_handle_data.peripheral.usartPort->RXDATA);
#elif defined(_SILICON_LABS_32B_SERIES_3)
    // RXDATA is const volatile (read-only), but we need to cast away const for DMA descriptor
    return (volatile void *)&(sl_spidrv_handle_data.peripheral.eusartPort->RXDATA);
#endif
}

static volatile void *rcp_spidrv_peripheral_get_txdata_addr(void)
{
#if defined(_SILICON_LABS_32B_SERIES_2)
    return &(sl_spidrv_handle_data.peripheral.usartPort->TXDATA);
#elif defined(_SILICON_LABS_32B_SERIES_3)
    return &(sl_spidrv_handle_data.peripheral.eusartPort->TXDATA);
#endif
}

// ============================================================================
// Legacy wrapper functions for backward compatibility
// ============================================================================

static void rcp_spidrv_set_host_request(void)
{
    rcp_spidrv_gpio_set_host_request();
}

static void rcp_spidrv_deassert_host_request(void)
{
    rcp_spidrv_gpio_deassert_host_request();
}

// ============================================================================
// Main Implementation Functions
// ============================================================================

static void rcp_spidrv_spi_transaction_end_interrupt(uint8_t intNo, void *ctx)
{
    OT_UNUSED_VARIABLE(ctx);

    // Handle falling edge (transaction start) - no action needed
    if (intNo == SPI_CS_FALLING_EDGE_INT)
    {
        return;
    }

    // Handle rising edge (transaction end)
    if (intNo == SPI_CS_RISING_EDGE_INT)
    {
        // Must be done before calling the "complete_callback" since
        // this callback will use otPlatSpiSlavePrepareTransaction who
        // would not setup the buffers if a transaction is ongoing.
        rcp_spidrv_deassert_host_request();
    }

    uint32_t tx_transaction_size = 0U;

    sl_dma_channel_handle_t *tx_dma  = rcp_spidrv_plat_tx_dma();
    LDMA_TypeDef            *tx_ldma = sl_device_peripheral_ldma_get_base_addr((sl_peripheral_t)tx_dma->dma_peripheral);
    uint8_t                  tx_ch   = tx_dma->channel_number;
    uint32_t                 tx_ctrl = tx_ldma->CH[tx_ch].CTRL;
    uint32_t                 tx_link = tx_ldma->CH[tx_ch].LINK;
    uint32_t                 fifo_tx = rcp_spidrv_peripheral_get_tx_status_shifted();

    uint8_t *old_tx_buffer      = (s_tx_output_len != 0U) ? s_tx_output_buf : (uint8_t *)&default_tx_value;
    uint16_t old_tx_buffer_size = (s_tx_output_len != 0U) ? s_tx_output_len : 1U;

    uint8_t *old_rx_buffer      = s_rx_buf;
    uint16_t old_rx_buffer_size = (s_rx_len != 0U) ? s_rx_len : 1U;

    tx_transaction_size =
        rcp_spidrv_dma_tx_compute_transaction_size(tx_dma, s_tx_seg0_bytes, s_tx_seg1_bytes, tx_ctrl, tx_link, fifo_tx);

    IgnoreReturnValue(sl_dma_channel_abort(rcp_spidrv_plat_tx_dma()));
    IgnoreReturnValue(sl_dma_channel_abort(rcp_spidrv_plat_rx_dma()));

    // Clear the FIFOs if there are more bytes to transmit than expected DMA tx xferCnt.
    if (rcp_spidrv_peripheral_get_tx_status())
    {
        rcp_spidrv_peripheral_clear_rx_fifo();
    }

    // call's otPlatSpiSlavePrepareTransaction in the background, the DMA buffer's will be ready after this call.
    if (complete_callback((void *)context,
                          (uint8_t *)old_tx_buffer,
                          old_tx_buffer_size,
                          (uint8_t *)old_rx_buffer,
                          old_rx_buffer_size,
                          tx_transaction_size))
    {
        otSysEventSignalPending();
        should_process_transaction = true;
    }
}

otError otPlatSpiSlaveEnable(otPlatSpiSlaveTransactionCompleteCallback aCompleteCallback,
                             otPlatSpiSlaveTransactionProcessCallback  aProcessCallback,
                             void                                     *aContext)
{
    CORE_DECLARE_IRQ_STATE;
    otError error         = OT_ERROR_NONE;
    bool    spidrv_inited = false;

    // If driver was already configured, an error is returned.
    VerifyOrExit(complete_callback == NULL, error = OT_ERROR_ALREADY);
    VerifyOrExit(process_callback == NULL, error = OT_ERROR_ALREADY);
    VerifyOrExit(context == NULL, error = OT_ERROR_ALREADY);

    sl_clock_manager_enable_bus_clock(SL_BUS_CLOCK_GPIO);

#if defined(_SILICON_LABS_32B_SERIES_2)
    SPIDRV_Init_t init_data = (SPIDRV_Init_t){
        SL_NCP_SPIDRV_USART_PERIPHERAL, // The USART used for SPI.
#if defined(_USART_ROUTELOC0_MASK)
        SL_NCP_SPIDRV_USART_TX_LOC,  // A location number for the SPI Tx pin.
        SL_NCP_SPIDRV_USART_RX_LOC,  // A location number for the SPI Rx pin.
        SL_NCP_SPIDRV_USART_CLK_LOC, // A location number for the SPI Clk pin.
        SL_NCP_SPIDRV_USART_CS_LOC,  // A location number for the SPI Cs pin.
#elif defined(_GPIO_USART_ROUTEEN_MASK)
        SL_NCP_SPIDRV_USART_TX_PORT,  // Tx port.
        SL_NCP_SPIDRV_USART_RX_PORT,  // Rx port.
        SL_NCP_SPIDRV_USART_CLK_PORT, // Clock port.
        SL_NCP_SPIDRV_USART_CS_PORT,  // Chip select port.
        SL_NCP_SPIDRV_USART_TX_PIN,   // Tx pin.
        SL_NCP_SPIDRV_USART_RX_PIN,   // Rx pin.
        SL_NCP_SPIDRV_USART_CLK_PIN,  // Clock pin.
        SL_NCP_SPIDRV_USART_CS_PIN,   // Chip select pin.
#endif
        0U,                             // An SPI bitrate.
        8,                              // An SPI framelength, valid numbers are 4..16
        0,                              // The value to transmit when using SPI receive API functions.
        spidrvSlave,                    // An SPI type, slave.
        SL_NCP_SPIDRV_USART_BIT_ORDER,  // A bit order on the SPI bus, MSB or LSB first.
        SL_NCP_SPIDRV_USART_CLOCK_MODE, // SPI mode, CLKPOL/CLKPHASE setting.
        spidrvCsControlAuto,            // A select master mode chip select (CS) control scheme.
        spidrvSlaveStartImmediate,      // A slave mode transfer start scheme.
    };
#elif defined(_SILICON_LABS_32B_SERIES_3)
    SPIDRV_Init_t init_data = (SPIDRV_Init_t){
        SL_NCP_SPIDRV_EUSART_PERIPHERAL, // The EUSART used for SPI.
        SL_NCP_SPIDRV_EUSART_TX_PORT,    // Tx port.
        SL_NCP_SPIDRV_EUSART_RX_PORT,    // Rx port.
        SL_NCP_SPIDRV_EUSART_SCLK_PORT,  // Clock port.
        SL_NCP_SPIDRV_EUSART_CS_PORT,    // Chip select port.
        SL_NCP_SPIDRV_EUSART_TX_PIN,     // Tx pin.
        SL_NCP_SPIDRV_EUSART_RX_PIN,     // Rx pin.
        SL_NCP_SPIDRV_EUSART_SCLK_PIN,   // Clock pin.
        SL_NCP_SPIDRV_EUSART_CS_PIN,     // Chip select pin.
        0U,                              // An SPI bitrate.
        8,                               // An SPI framelength, valid numbers are 4..16
        0,                               // The value to transmit when using SPI receive API functions.
        spidrvSlave,                     // An SPI type, slave.
        SL_NCP_SPIDRV_EUSART_BIT_ORDER,  // A bit order on the SPI bus, MSB or LSB first.
        SL_NCP_SPIDRV_EUSART_CLOCK_MODE, // SPI mode, CLKPOL/CLKPHASE setting.
        spidrvCsControlAuto,             // A select master mode chip select (CS) control scheme.
        spidrvSlaveStartImmediate,       // A slave mode transfer start scheme.
    };
#endif

    VerifyOrExit(SPIDRV_Init((SPIDRV_HandleData_t *)&sl_spidrv_handle_data, &init_data) == ECODE_EMDRV_SPIDRV_OK,
                 error = OT_ERROR_FAILED);
    spidrv_inited = true; // SPIDRV owns DMA channel init/allocation in txDMACh / rxDMACh.

    // TX default value.
    default_tx_value = 0xFFU;

    s_tx_output_buf = NULL;
    s_tx_output_len = 0U;
    s_rx_buf        = NULL;
    s_rx_len        = 0U;
    s_tx_seg0_bytes = 1U;
    s_tx_seg1_bytes = (uint16_t)SL_DMA_CHANNEL_MAX_XFER_UNIT_COUNT;

    // Configuring Host INT line. Active low
#if defined(SPI_HOST_INT_PORT) && defined(SPI_HOST_INT_PIN)
    rcp_spidrv_gpio_set_pin_mode(SPI_HOST_INT_PORT, SPI_HOST_INT_PIN, SL_GPIO_MODE_PUSH_PULL, 1U);
#endif

    // Configure CS pin
    rcp_spidrv_gpio_set_pin_mode(SPI_CS_PORT, SPI_CS_PIN, SL_GPIO_MODE_INPUT_PULL_FILTER, 1);

    CORE_ENTER_ATOMIC();

    sl_status_t st;

    // Initialization during transaction is not supported.
    if (!rcp_spidrv_gpio_is_cs_high())
    {
        error = OT_ERROR_FAILED;
        goto enable_fail;
    }

    // Configure CS interrupts
    rcp_spidrv_gpio_configure_cs_interrupt(SPI_CS_PORT, SPI_CS_PIN, SPI_CS_RISING_EDGE_INT, true);
    rcp_spidrv_gpio_configure_cs_interrupt(SPI_CS_PORT, SPI_CS_PIN, SPI_CS_FALLING_EDGE_INT, false);

    // Clear the peripheral RX/TX FIFO before configuring the dma transfers.
    rcp_spidrv_peripheral_clear_fifos();

    st = sl_dma_channel_set_peripheral_signal(rcp_spidrv_plat_tx_dma(),
                                              SL_OT_SPIDRV_SPI_DMA_TX_SIGNAL(SPI_PERIPHERAL_NO));
    if (st != SL_STATUS_OK)
    {
        error = OT_ERROR_FAILED;
        goto enable_fail;
    }

    rcp_spidrv_dma_setup_tx_transfer(&tx_dma_transfers[1],
                                     &default_tx_value,
                                     (void *)rcp_spidrv_peripheral_get_txdata_addr(),
                                     SL_DMA_CHANNEL_MAX_XFER_UNIT_COUNT,
                                     false,
                                     &tx_dma_descriptors[1]);

    st = sl_dma_channel_submit_transfer_list(rcp_spidrv_plat_tx_dma(), &tx_dma_transfers[1]);
    if (st != SL_STATUS_OK)
    {
        error = OT_ERROR_FAILED;
        goto enable_fail;
    }

    complete_callback          = aCompleteCallback;
    process_callback           = aProcessCallback;
    context                    = aContext;
    should_process_transaction = false;
    s_ot_spi_slave_ready       = true;

#if defined(SL_CATALOG_POWER_MANAGER_PRESENT)
    sl_power_manager_add_em_requirement(SL_POWER_MANAGER_EM1);
#endif

    CORE_EXIT_ATOMIC();

exit:
    return error;

enable_fail:
    rcp_spidrv_gpio_deconfigure_cs_interrupt(SPI_CS_RISING_EDGE_INT);
    rcp_spidrv_gpio_deconfigure_cs_interrupt(SPI_CS_FALLING_EDGE_INT);
    CORE_EXIT_ATOMIC();
    if (spidrv_inited)
    {
        IgnoreReturnValue(SPIDRV_DeInit((SPIDRV_Handle_t)&sl_spidrv_handle_data));
    }
    complete_callback          = NULL;
    process_callback           = NULL;
    context                    = NULL;
    should_process_transaction = false;
    return error;
}

void otPlatSpiSlaveDisable(void)
{
    CORE_DECLARE_IRQ_STATE;
    CORE_ENTER_ATOMIC();

    // Disable CS GPIO IRQ.
    rcp_spidrv_gpio_deconfigure_cs_interrupt(SPI_CS_RISING_EDGE_INT);
    rcp_spidrv_gpio_deconfigure_cs_interrupt(SPI_CS_FALLING_EDGE_INT);

    CORE_EXIT_ATOMIC();

    if (s_ot_spi_slave_ready)
    {
        IgnoreReturnValue(SPIDRV_DeInit((SPIDRV_Handle_t)&sl_spidrv_handle_data));
        s_ot_spi_slave_ready = false;
    }

    // Host INT line.
    rcp_spidrv_deassert_host_request();
#if defined(SPI_HOST_INT_PORT) && defined(SPI_HOST_INT_PIN)
    rcp_spidrv_gpio_set_pin_mode(SPI_HOST_INT_PORT, SPI_HOST_INT_PIN, SL_GPIO_MODE_INPUT, 0U);
#endif

#if defined(SL_CATALOG_POWER_MANAGER_PRESENT)
    sl_power_manager_remove_em_requirement(SL_POWER_MANAGER_EM1);
#endif

    should_process_transaction = false;

    complete_callback = NULL;
    process_callback  = NULL;
    context           = NULL;
}

otError otPlatSpiSlavePrepareTransaction(uint8_t *aOutputBuf,
                                         uint16_t aOutputBufLen,
                                         uint8_t *aInputBuf,
                                         uint16_t aInputBufLen,
                                         bool     aRequestTransactionFlag)
{
    CORE_DECLARE_IRQ_STATE;
    CORE_ENTER_ATOMIC();

    otError error = OT_ERROR_NONE;

    VerifyOrExit(aOutputBufLen <= SL_DMA_CHANNEL_MAX_XFER_UNIT_COUNT, error = OT_ERROR_FAILED);
    VerifyOrExit(aInputBufLen <= SL_DMA_CHANNEL_MAX_XFER_UNIT_COUNT, error = OT_ERROR_FAILED);

    // Check the CS pin if SPI transactions are in progress (must be high/idle).
    VerifyOrExit(rcp_spidrv_gpio_is_cs_high(), error = OT_ERROR_BUSY);

    if (aOutputBuf)
    {
        IgnoreReturnValue(sl_dma_channel_abort(rcp_spidrv_plat_tx_dma()));
    }

    if (aInputBuf)
    {
        // Clear the rxFifo only if it is not empty.
        if (rcp_spidrv_peripheral_get_rx_status())
        {
            rcp_spidrv_peripheral_clear_rx_fifo_and_drain();
        }

        IgnoreReturnValue(sl_dma_channel_abort(rcp_spidrv_plat_rx_dma()));
    }

    // Verify CS is still high after stopping transfers.
    VerifyOrExit(rcp_spidrv_gpio_is_cs_high(), error = OT_ERROR_BUSY);

    if (aOutputBuf != NULL)
    {
        rcp_spidrv_peripheral_clear_tx_fifo_if_needed_and_drain();

        s_tx_output_buf = aOutputBuf;
        s_tx_output_len = aOutputBufLen;
        s_tx_seg0_bytes = aOutputBufLen;
        s_tx_seg1_bytes = (uint16_t)SL_DMA_CHANNEL_MAX_XFER_UNIT_COUNT;
    }

    if (aInputBuf != NULL)
    {
        s_rx_buf = aInputBuf;
        s_rx_len = aInputBufLen;
    }

    // Final CS check before starting transfers.
    VerifyOrExit(rcp_spidrv_gpio_is_cs_high(), error = OT_ERROR_BUSY);

    if (aOutputBuf != NULL)
    {
        sl_status_t st = sl_dma_channel_set_peripheral_signal(rcp_spidrv_plat_tx_dma(),
                                                              SL_OT_SPIDRV_SPI_DMA_TX_SIGNAL(SPI_PERIPHERAL_NO));
        VerifyOrExit(st == SL_STATUS_OK, error = OT_ERROR_FAILED);

        rcp_spidrv_dma_setup_tx_transfer(&tx_dma_transfers[0],
                                         aOutputBuf,
                                         (void *)rcp_spidrv_peripheral_get_txdata_addr(),
                                         aOutputBufLen,
                                         true,
                                         &tx_dma_descriptors[0]);
        rcp_spidrv_dma_setup_tx_transfer(&tx_dma_transfers[1],
                                         &default_tx_value,
                                         (void *)rcp_spidrv_peripheral_get_txdata_addr(),
                                         SL_DMA_CHANNEL_MAX_XFER_UNIT_COUNT,
                                         false,
                                         &tx_dma_descriptors[1]);
        tx_dma_transfers[0].next = &tx_dma_transfers[1];

        st = sl_dma_channel_submit_transfer_list(rcp_spidrv_plat_tx_dma(), &tx_dma_transfers[0]);
        VerifyOrExit(st == SL_STATUS_OK, error = OT_ERROR_FAILED);
    }

    if (aInputBuf != NULL)
    {
        sl_status_t st = sl_dma_channel_set_peripheral_signal(rcp_spidrv_plat_rx_dma(),
                                                              SL_OT_SPIDRV_SPI_DMA_RX_SIGNAL(SPI_PERIPHERAL_NO));
        VerifyOrExit(st == SL_STATUS_OK, error = OT_ERROR_FAILED);

        rcp_spidrv_dma_setup_rx_transfer(&rx_dma_transfer,
                                         (void *)rcp_spidrv_peripheral_get_rxdata_addr(),
                                         aInputBuf,
                                         aInputBufLen,
                                         &rx_dma_descriptor);

        st = sl_dma_channel_submit_transfer_list(rcp_spidrv_plat_rx_dma(), &rx_dma_transfer);
        VerifyOrExit(st == SL_STATUS_OK, error = OT_ERROR_FAILED);
    }

    if (aRequestTransactionFlag)
    {
        rcp_spidrv_set_host_request();
    }
    else
    {
        rcp_spidrv_deassert_host_request();
    }

exit:

    CORE_EXIT_ATOMIC();
    return error;
}

void efr32SpiProcess(void)
{
    if (should_process_transaction)
    {
        if (context)
        {
            process_callback((void *)context);
        }

        should_process_transaction = false;
    }
}
