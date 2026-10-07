/***************************************************************************/ /**
 * @file
 * @brief CPC SDIO Driver implementation.
 *
 * Known limitations (CPC-2989):
 * When an error occurs during payload descriptor preparation in
 * adma_rx_start_payloads(), the driver aborts the entire transfer by clearing
 * all pending frames, flushing the full SDIO block, and discarding frames that
 * were already prepared. This is overly aggressive and can cause unnecessary
 * data loss. The desired behavior is to deliver frames already in
 * rx_complete_pending_frames to CPC, flush only the bytes for the current
 * problematic payload (including alignment padding), and continue processing the
 * next frame in rx_payload_pending_frames on ADMA complete rather than
 * aborting the whole transfer.
 *******************************************************************************
 * # License
 * <b>Copyright 2024 Silicon Laboratories Inc. www.silabs.com</b>
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

#include "sl_cpc_drv_sdio_device.h"

#include <stdalign.h>

#include "sl_bit.h"
#include "sl_clock_manager.h"
#include "sl_device_peripheral.h"
#include "sl_hal_gpio.h"
#include "sl_hal_sdio_device.h"
#include "sl_status.h"

#include "../../../src/sli_cpc_assert.h"
#include "../../../src/sli_cpc_atomic.h"
#include "../../../src/sli_cpc_bus.h"
#include "../../../src/sli_cpc_drv.h"
#include "../../../src/sli_cpc_frame_list.h"
#include "../../../src/sli_cpc_hdr.h"
#include "../../../src/sli_cpc_log.h"
#include "../../../src/sli_cpc_memory.h"
#include "sl_cpc_drv_instances.h"
#include "sl_cpc_drv_sdio_device_config.h"
#include "sl_slist.h"

/*******************************************************************************
 *********************************   DEFINES   *********************************
 ******************************************************************************/
#define CPC_DRV_SDIO_IRQ_HANDLER SL_CONCAT_PASTER_3(SDIO, SL_CPC_DRV_SDIO_PERIPHERAL_NO, _IRQHandler)
#define CPC_DRV_SDIO_DESC_COUNT SL_CPC_DRV_SDIO_DEVICE_RX_FRAME_POOL_COUNT
#define CPC_SDIO_DEFAULT_AGGREGATION SL_CPC_DRV_SDIO_DEVICE_RX_FRAME_POOL_COUNT
static_assert(CPC_DRV_SDIO_DESC_COUNT >= 2,
              "Two descriptors are required at a minimum for managing unaligned payloads");

static_assert(CPC_SDIO_DEFAULT_AGGREGATION < 256, "Aggregation too large");
static_assert(SL_CPC_BUF_MIN_ALIGNMENT >= sizeof(uint32_t), "Invalid alignment");
static_assert(SL_CPC_EP_MAX_PAYLOAD_SIZE <= SL_HAL_SDIO_DEV_DMA_DESCRIPTOR_MAX_XFER_SIZE, "Invalid payload size");
static_assert(SL_CPC_DRV_SDIO_DEVICE_RX_FRAME_POOL_COUNT >= CPC_SDIO_DEFAULT_AGGREGATION,
              "RX frame pool count is too small");

// ADMA state machine. The driver starts by sending/receiving all the pending frame
// headers, then all the payloads. Once it has completed, the corresponding CMD 53
// over callback will be serviced, ending the active transfer.
typedef enum {
  ADMA_IDLE,
  ADMA_RECEIVE_HEADER,
  ADMA_RECEIVE_PAYLOAD,
  ADMA_TRANSMIT_HEADER,
  ADMA_TRANSMIT_PAYLOAD,
  ADMA_FLUSH_SDIO,
} adma_state_t;

// Header block structure.
struct sdio_header_block {
  uint8_t frame_count;
  uint8_t reserved[3];
  sli_cpc_hdr_t headers[];
};

/*******************************************************************************
 *********************************   MACROS   **********************************
 ******************************************************************************/
// Calculate the aligned payload size.
#define ALIGNED_PAYLOAD_SIZE(x) (SL_DIV_ROUND_UP(x, SL_CPC_BUF_MIN_ALIGNMENT) * SL_CPC_BUF_MIN_ALIGNMENT)

/*******************************************************************************
 ***************************  LOCAL VARIABLES   ********************************
 ******************************************************************************/

/*
 * Singleton runtime state. Ops take `sl_cpc_bus_t *` per sli_cpc_drv.h
 * but mostly ignore `bus` and use these file-scope variables. For multi-bus
 * support, move this state into `sl_cpc_drv_sdio_device` and access it via container_of.
 */
static sl_cpc_bus_t *s_bus;

// List of frames currently pending processing by the ADMA. These frames have
// a valid header, endpoint and payload pointer, but their payload buffer are
// still to be received over the bus.
static sli_cpc_frame_list_t rx_payload_pending_frames;

// List of frames that were handled by the ADMA but that are waiting to be sent
// over the bus before being served to the core.
static sli_cpc_frame_list_t rx_complete_pending_frames;

// List of frames received successfully from the bus. These frames are completely
// populated, along with their payloads. They are ready to be handed to the core
// for processing.
static sli_cpc_frame_list_t rx_completed_frames;

static sli_cpc_frame_list_t rx_free_frame_list;

static volatile bool rx_alloc_stalled = false;

// Indicates if the driver is already busy transmitting. This is to avoid corrupting
// an active CMD53.
static volatile bool tx_busy = false;

// List of frames submitted to the driver waiting to be picked up by the driver.
// This list is used when the driver is already busy with a TX event.
static sli_cpc_frame_list_t tx_pending_frames;

// List of frames waiting for their headers to be sent. These frames were populated
// by the core and are to be processed when the host will read the frame headers.
static sli_cpc_frame_list_t tx_header_pending_frames;

// List of frames currently pending processing by the ADMA. These frames have
// a valid header, endpoint and payload pointer, but their payload buffer are
// still to be sent over the bus.
static sli_cpc_frame_list_t tx_payload_pending_frames;

// List of frames that have been sent over the bus, but pending the completion of the rest of the
// SDIO transfer (i.e.: Frame 0 has been sent over the bus, but Frame 1 is still in-flight).
static sli_cpc_frame_list_t tx_pending_xfer_complete_frames;

// Buffer used to store the frame count.
alignas(SL_CPC_BUF_MIN_ALIGNMENT) static struct sdio_header_block sdio_hdr_block;

// Descriptor used to transfer aggregated frames over the bus.
static sl_hal_sdio_dev_dma_descriptor_t frame_descriptors[CPC_DRV_SDIO_DESC_COUNT];

// Aligned buffer used to store the unaligned payloads. (4 bytes size aligned)
static uint32_t unaligned_payload_pool[CPC_DRV_SDIO_DESC_COUNT];

// CPC-1998: avoid having to waste this block, just for a flush.
// Buffer used to flush pad bytes coming from the bus.
alignas(SL_CPC_BUF_MIN_ALIGNMENT) static uint8_t flush_buff[SL_CPC_DRV_SDIO_BLOCK_SIZE];

// Descriptor used to discard pad bytes from the bus.
static sl_hal_sdio_dev_dma_descriptor_t flush_desc;

static volatile adma_state_t adma_state = ADMA_IDLE;

/*******************************************************************************
 **************************   LOCAL FUNCTIONS   ********************************
 ******************************************************************************/

static uint32_t sdio_drv_get_available_write_frame_slots(sl_cpc_bus_t *bus);
static sl_status_t init_hw(const sl_cpc_drv_sdio_device_t *drv);
static sl_status_t sdio_drv_init(sl_cpc_bus_t *bus);
static sl_status_t sdio_drv_read_data(sl_cpc_bus_t *bus, sli_cpc_frame_list_t *frames);
static sl_status_t sdio_drv_start_rx(sl_cpc_bus_t *bus);
static uint32_t sdio_drv_write(sl_cpc_bus_t *bus, sli_cpc_frame_list_t *frames);
static void cpc_drv_sdio_on_rx_frame_free(sl_cpc_bus_t *bus);

SLI_CPC_STATIC_ASSERT_PACKED_SIZE(struct sli_cpc_drv_sdio_device_caps, 1);

static void get_local_capabilities(sl_cpc_bus_t *bus, const void **caps_p, uint16_t *caps_size_p)
{
  const sl_cpc_drv_sdio_device_t *drv = container_of(bus, sl_cpc_drv_sdio_device_t, bus);

  *caps_p = &drv->local_caps;
  *caps_size_p = sizeof(drv->local_caps);
}

static sl_status_t set_remote_capabilities(sl_cpc_bus_t *bus, const void *caps, uint16_t caps_size)
{
  sl_cpc_drv_sdio_device_t *drv = container_of(bus, sl_cpc_drv_sdio_device_t, bus);

  if (caps_size != sizeof(drv->remote_caps)) {
    return SL_STATUS_INVALID_COUNT;
  }
  memcpy(&drv->remote_caps, caps, caps_size);
  return SL_STATUS_OK;
}

#define s_sdio_device_drv sl_cpc_drv_sdio_device_instances[0]

sl_status_t sl_cpc_drv_sdio_device_init(sl_cpc_drv_sdio_device_t *drv, const sl_cpc_drv_sdio_device_config_t *cfg,
                                        const sl_cpc_bus_config_t *bus_cfg)
{
  static const sli_cpc_drv_ops_t ops = {
    .init = &sdio_drv_init,
    .start_rx = &sdio_drv_start_rx,
    .read = &sdio_drv_read_data,
    .write = &sdio_drv_write,
    .get_available_write_frame_slots = &sdio_drv_get_available_write_frame_slots,
    .on_rx_frame_free = &cpc_drv_sdio_on_rx_frame_free,
    .get_local_capabilities = &get_local_capabilities,
    .set_remote_capabilities = &set_remote_capabilities,
  };

  sl_status_t status;

  if (cfg == NULL || bus_cfg == NULL) {
    return SL_STATUS_NULL_POINTER;
  }

  memset(drv, 0, sizeof(*drv));

  status = init_hw(drv);
  if (status != SL_STATUS_OK) {
    return status;
  }

  return sli_cpc_bus_init(&drv->bus, bus_cfg, &ops);
}

static void adma_irq_handler(void);
static void fun0_irq_handler(void);
static void fun1_irq_handler(void);
static void on_sdio_read_complete(void);
static void on_sdio_read_start(void);
static void on_sdio_write_complete(void);
static void on_sdio_write_start(void);
static void prepare_tx(void);
static void sdio_drv_pin_init(void);
static void adma_rx_start_frame_count(void);
static void adma_rx_start_headers(void);
static void adma_rx_abort_header_transfer(void);
static void adma_rx_abort_payload_transfer(sl_cpc_frame_t *frame);
static void adma_rx_start_payloads(void);
static void adma_tx_prepare_frame_count(void);
static void adma_tx_prepare_header_descriptors(void);
static void adma_tx_prepare_payload_descriptors(void);
static void clear_frame_list(sli_cpc_frame_list_t *list);
static void start_transfer(size_t idx, adma_state_t next_state);
static void start_flush_transfer(uint32_t xfer_count);
static sl_status_t alloc_rx_frame_to_free_list(sl_cpc_bus_t *bus);

/***************************************************************************/ /**
 * Checks if any of the bit in the mask is set, and clears the mask.
 ******************************************************************************/
static inline bool evaluate_and_clear_mask(uint32_t *val, uint32_t mask)
{
  uint32_t old = *val;

  SL_CLEAR_BIT(*val, mask);
  return SL_IS_BIT_SET(old, mask);
}

/***************************************************************************/ /**
 * SDIO peripheral interrupt handler.
 ******************************************************************************/
void CPC_DRV_SDIO_IRQ_HANDLER(void)
{
  uint32_t pending_irq = sl_hal_sdio_dev_get_enabled_pending_interrupts(SL_CPC_DRV_SDIO_PERIPHERAL);

  // Clear the interrupts now, as the callbacks could cause it to fire again.
  sl_hal_sdio_dev_clear_interrupts(SL_CPC_DRV_SDIO_PERIPHERAL, pending_irq);

  if (evaluate_and_clear_mask(&pending_irq, SDIO_FN0_GLB_INT_STS_REG_FN0_GLB_INT_STS_REG0)) {
    fun0_irq_handler();
  }

  if (evaluate_and_clear_mask(&pending_irq, SDIO_FN0_GLB_INT_STS_REG_FN0_GLB_INT_STS_REG1)) {
    fun1_irq_handler();
  }

  if (pending_irq) {
    // Unhandled IRQ source.
    SLI_CPC_ASSERT(false);
  }
}

/***************************************************************************/ /**
 * Function 0 interrupt handler.
 ******************************************************************************/
static void fun0_irq_handler(void)
{
  sl_status_t status;
  uint32_t pending_irq = sl_hal_sdio_dev_get_enabled_pending_function_interrupts(SL_CPC_DRV_SDIO_PERIPHERAL, 0);

  if (!pending_irq) {
    // CPC-2004: Somehow, we can get here even though no fn0 IRQ is set?!
    return;
  }

  // Soft reset
  if (evaluate_and_clear_mask(&pending_irq, SDIO_FN0_INT_TO_ARM_FN0_AXISOFT_RST)) {
    sl_hal_sdio_dev_clear_function_interrupts(SL_CPC_DRV_SDIO_PERIPHERAL, 0, SDIO_FN0_INT_TO_ARM_FN0_AXISOFT_RST);
    // CPC-2003: Handle reset from host.
  }

  // Voltage switch
  if (evaluate_and_clear_mask(&pending_irq, SDIO_FN0_INT_TO_ARM_FN0_VOLT_SWITCH_CMD)) {
    sl_hal_sdio_dev_clear_function_interrupts(SL_CPC_DRV_SDIO_PERIPHERAL, 0, SDIO_FN0_INT_TO_ARM_FN0_VOLT_SWITCH_CMD);
    status = sl_hal_sdio_dev_voltage_switch(SL_CPC_DRV_SDIO_PERIPHERAL);
    SLI_CPC_ASSERT(status == SL_STATUS_OK);
  }

  // Unhandled IRQ.
  if (pending_irq) {
    SLI_CPC_ASSERT(false);
  }
}

/***************************************************************************/ /**
 * Function 1 interrupt handler.
 ******************************************************************************/
static void fun1_irq_handler(void)
{
  uint32_t pending_irq = sl_hal_sdio_dev_get_enabled_pending_function_interrupts(SL_CPC_DRV_SDIO_PERIPHERAL, 1);

  if (!pending_irq) {
    // CPC-2004: Somehow, we can get here even though no fn0 IRQ is set?!
    return;
  }

  if (evaluate_and_clear_mask(&pending_irq, SDIO_FN1_AXI_FN1_INT_SIG_FUN1_EN)) {
    sl_hal_sdio_dev_clear_function_interrupts(SL_CPC_DRV_SDIO_PERIPHERAL, SL_CPC_DRV_SDIO_FUNCTION_NUM,
                                              SDIO_FN1_AXI_FN1_INT_SIG_FUN1_EN);
  }

  // ADMA Transfer
  if (evaluate_and_clear_mask(&pending_irq, SDIO_FN1_AXI_FN1_INT_FN1_SIG_ADMA_END_INT)) {
    sl_hal_sdio_dev_clear_function_interrupts(SL_CPC_DRV_SDIO_PERIPHERAL, SL_CPC_DRV_SDIO_FUNCTION_NUM,
                                              SDIO_FN1_AXI_FN1_INT_FN1_SIG_ADMA_END_INT);

    adma_irq_handler();
  }

  // CMD53 Write end.
  if (evaluate_and_clear_mask(&pending_irq, SDIO_FN1_AXI_FN1_INT_SIG_FN1_WR_OVER)) {
    sl_hal_sdio_dev_clear_function_interrupts(SL_CPC_DRV_SDIO_PERIPHERAL, SL_CPC_DRV_SDIO_FUNCTION_NUM,
                                              SDIO_FN1_AXI_FN1_INT_SIG_FN1_WR_OVER);

    // Reception completed.
    on_sdio_write_complete();
  }

  // CMD53 Read end
  if (evaluate_and_clear_mask(&pending_irq, SDIO_FN1_AXI_FN1_INT_SIG_FN1_RD_OVER)) {
    sl_hal_sdio_dev_clear_function_interrupts(SL_CPC_DRV_SDIO_PERIPHERAL, SL_CPC_DRV_SDIO_FUNCTION_NUM,
                                              SDIO_FN1_AXI_FN1_INT_SIG_FN1_RD_OVER);

    // Transmission completed.
    on_sdio_read_complete();
  }

  // CMD53 Write Start
  if (evaluate_and_clear_mask(&pending_irq, SDIO_FN1_AXI_FN1_INT_SIG_FN1_SDIO_WR_START)) {
    sl_hal_sdio_dev_clear_function_interrupts(SL_CPC_DRV_SDIO_PERIPHERAL, SL_CPC_DRV_SDIO_FUNCTION_NUM,
                                              SDIO_FN1_AXI_FN1_INT_SIG_FN1_SDIO_WR_START);

    // Host initiated reception operation.
    on_sdio_write_start();
  }

  // CMD53 Read Start
  if (evaluate_and_clear_mask(&pending_irq, SDIO_FN1_AXI_FN1_INT_SIG_FN1_SDIO_RD_START)) {
    sl_hal_sdio_dev_clear_function_interrupts(SL_CPC_DRV_SDIO_PERIPHERAL, SL_CPC_DRV_SDIO_FUNCTION_NUM,
                                              SDIO_FN1_AXI_FN1_INT_SIG_FN1_SDIO_RD_START);

    // Host requested transmission.
    on_sdio_read_start();
  }

  if (evaluate_and_clear_mask(&pending_irq, SDIO_FN1_AXI_FN1_INT_SIG_FUN1_RST)) {
    sl_hal_sdio_dev_clear_function_interrupts(SL_CPC_DRV_SDIO_PERIPHERAL, SL_CPC_DRV_SDIO_FUNCTION_NUM,
                                              SDIO_FN1_AXI_FN1_INT_SIG_FUN1_RST);

    // Function reset. This most likely indicates that the CPC kernel subsystem
    // has disappeared.
    // CPC-2003: Handle function reset
  }

  // CMD53 Read Error (CRC, timeout, etc.)
  if (evaluate_and_clear_mask(&pending_irq, SDIO_FN1_AXI_FN1_INT_SIG_FN1_RD_ERROR)) {
    sl_hal_sdio_dev_clear_function_interrupts(SL_CPC_DRV_SDIO_PERIPHERAL, SL_CPC_DRV_SDIO_FUNCTION_NUM,
                                              SDIO_FN1_AXI_FN1_INT_SIG_FN1_RD_ERROR);
    SLI_CPC_LOG_ERROR("[SDIO] CMD53 Read error (CRC/timeout)");
  }

  // ADMA Transfer Error
  if (evaluate_and_clear_mask(&pending_irq, SDIO_FN1_AXI_FN1_INT_FN1_SIG_ADMA_ERR)) {
    sl_hal_sdio_dev_clear_function_interrupts(SL_CPC_DRV_SDIO_PERIPHERAL, SL_CPC_DRV_SDIO_FUNCTION_NUM,
                                              SDIO_FN1_AXI_FN1_INT_FN1_SIG_ADMA_ERR);

    SLI_CPC_LOG_ERROR("[SDIO] ADMA transfer error");
  }
  // Unhandled IRQ.
  if (pending_irq) {
    SLI_CPC_LOG_ERROR("[SDIO] Unhandled FN1 interrupt: %lu", pending_irq);
    SLI_CPC_ASSERT(false);
  }
}

/***************************************************************************/ /**
 * Initialize the driver hardware interface(s).
 ******************************************************************************/
static sl_status_t init_hw(const sl_cpc_drv_sdio_device_t *drv)
{
  sl_status_t status;
  sl_hal_sdio_dev_init_params_t sdio_init
    = {.host_1v8_only = SL_CPC_DRV_SDIO_HOST_1V8_ONLY, .speed_mode = SL_CPC_DRV_SDIO_SPEED_MODE};
  sl_hal_sdio_dev_cis_common_t common_cis = SL_HAL_SDIO_DEV_CIS_COMMON_DEFAULT;
  common_cis.vendor_id = 0x296;
  common_cis.product_id = 0x5347;
  sl_hal_sdio_dev_cis_function_t func1_cis = SL_HAL_SDIO_DEV_CIS_FUNCTION_DEFAULT;
  func1_cis.max_block_size = SL_CPC_DRV_SDIO_BLOCK_SIZE;

#if defined(SL_CLOCK_BRANCH_SDIO)
#warning Remove if def and else once SL_CLOCK_BRANCH_SDIO is defined.
  sdio_clock_branch = sl_device_peripheral_get_clock_branch(&sl_peripheral_val_sdio0);

  status = sl_clock_manager_get_clock_branch_frequency(sdio_clock_branch, &sdio_init.bus_clock);
  if (status != SL_STATUS_OK) {
    return status;
  }
#else
  sdio_init.bus_clock = 5000000UL;
#endif

  (void)drv;

  // Configure SDIO peripheral pins
  sdio_drv_pin_init();

  // Initialize SDIO peripheral
  status = sl_hal_sdio_dev_reset(SL_CPC_DRV_SDIO_PERIPHERAL);
  if (status != SL_STATUS_OK) {
    SLI_CPC_LOG_ERROR("[SDIO] Failed to reset SDIO peripheral: 0x%lx", (unsigned long)status);
    SLI_CPC_ASSERT(false);
    return status;
  }

  status = sl_hal_sdio_dev_init(SL_CPC_DRV_SDIO_PERIPHERAL, &sdio_init, &common_cis, &func1_cis, NULL);
  if (status != SL_STATUS_OK) {
    SLI_CPC_LOG_ERROR("[SDIO] Failed to initialize SDIO peripheral: 0x%lx", (unsigned long)status);
    SLI_CPC_ASSERT(false);
    return status;
  }

  // Enable interrupts for FN0 & FN1
  sl_hal_sdio_dev_enable_interrupts(SL_CPC_DRV_SDIO_PERIPHERAL, (SDIO_FN0_GLB_INT_EN_REG_FN0_INT_TO_ARM_EN
                                                                 | SDIO_FN0_GLB_INT_EN_REG_FN0_FN1_INT_TO_ARM_EN));

  // Enable reset command interrupt to signal the core
  sl_hal_sdio_dev_enable_function_interrupts(SL_CPC_DRV_SDIO_PERIPHERAL, 0,
                                             (SDIO_FN0_INT_EN_FN0_AXISOFT_RST_EN          // Card reset
                                              | SDIO_FN0_INT_EN_FN0_VOLT_SWITCH_CMD_EN)); // Voltage switch

  // Enable FN1 interrupts
  sl_hal_sdio_dev_enable_function_interrupts(SL_CPC_DRV_SDIO_PERIPHERAL, SL_CPC_DRV_SDIO_FUNCTION_NUM,
                                             (SDIO_FN1_INT_EN_FN1_SIG_FUN_EN_INT_EN       // Function enabled
                                              | SDIO_FN1_INT_EN_SIG_FN1_SDIO_RD_START_EN  // CMD53 Read start
                                              | SDIO_FN1_AXI_FN1_INT_FN1_SIG_ADMA_END_INT // ADMA transfer done
                                              | SDIO_FN1_INT_EN_SIG_FN1_WR_OVER_EN        // CMD53 Write done
                                              | SDIO_FN1_INT_EN_SIG_FN1_RD_OVER_EN        // CMD53 Read done
                                              | SDIO_FN1_INT_EN_SIG_FN1_RD_ERROR_EN       // Read error
                                              | SDIO_FN1_INT_EN_FN1_SIG_ADMA_ERR_EN       // ADMA transfer error
                                              | SDIO_FN1_INT_EN_SIG_FUN1_RST_EN));        // FN1 reset

  // Enable interrupts for SDIO peripheral
  sl_interrupt_manager_clear_irq_pending(SDIO0_IRQn);
  sl_interrupt_manager_enable_irq(SDIO0_IRQn);

  return SL_STATUS_OK;
}

/***************************************************************************/ /**
 * Initialize the SDIO peripheral pins.
 ******************************************************************************/
static void sdio_drv_pin_init(void)
{
  sl_gpio_t gpio;
  // CMD and DAT are driven by the device; SCLK is host-driven and stays an input.
  sl_gpio_t driven_pins[] = {
    {.port = SL_CPC_DRV_SDIO_CMD_PORT, .pin = SL_CPC_DRV_SDIO_CMD_PIN},
    {.port = SL_CPC_DRV_SDIO_DAT0_PORT, .pin = SL_CPC_DRV_SDIO_DAT0_PIN},
    {.port = SL_CPC_DRV_SDIO_DAT1_PORT, .pin = SL_CPC_DRV_SDIO_DAT1_PIN},
    {.port = SL_CPC_DRV_SDIO_DAT2_PORT, .pin = SL_CPC_DRV_SDIO_DAT2_PIN},
    {.port = SL_CPC_DRV_SDIO_DAT3_PORT, .pin = SL_CPC_DRV_SDIO_DAT3_PIN},
  };

  // Enable bus clock for GPIO.
  sl_clock_manager_enable_bus_clock(SL_BUS_CLOCK_GPIO);

  // Enable bus clock for SDIO.
  sl_clock_manager_enable_bus_clock(SL_BUS_CLOCK_SDIO0);

  // Configure SDIO pins routing.
  GPIO->SDIO0PRI2ROUTEPEN
    = (GPIO_SDIO0PRI2ROUTEPEN_SDIO0PRI2DATA0ROUTEPEN | GPIO_SDIO0PRI2ROUTEPEN_SDIO0PRI2DATA1ROUTEPEN
       | GPIO_SDIO0PRI2ROUTEPEN_SDIO0PRI2DATA2ROUTEPEN | GPIO_SDIO0PRI2ROUTEPEN_SDIO0PRI2DATA3ROUTEPEN
       | GPIO_SDIO0PRI2ROUTEPEN_SDIO0PRI2CMDROUTEPEN | GPIO_SDIO0PRI2ROUTEPEN_SDIO0PRI2CLKROUTEPEN);

  // If CD GPIO pins are configured, use GPIO card detect; otherwise use the SDIO CD pin.
#if defined(SL_CPC_DRV_SDIO_CD_PORT) && defined(SL_CPC_DRV_SDIO_CD_PIN)
  gpio = (sl_gpio_t){.port = SL_CPC_DRV_SDIO_CD_PORT, .pin = SL_CPC_DRV_SDIO_CD_PIN};
  sl_hal_gpio_set_pin_mode(&gpio, SL_GPIO_MODE_PUSH_PULL, 1);
#endif

  // Configure SDIO pins.
  gpio = (sl_gpio_t){.port = SL_CPC_DRV_SDIO_SCLK_PORT, .pin = SL_CPC_DRV_SDIO_SCLK_PIN};
  sl_hal_gpio_set_pin_mode(&gpio, SL_GPIO_MODE_INPUT, 1);

  for (size_t i = 0; i < SL_ARRAY_SIZE(driven_pins); i++) {
    sl_hal_gpio_set_pin_mode(&driven_pins[i], SL_GPIO_MODE_PUSH_PULL, 1);
  }
}

/***************************************************************************/ /**
 * Initialize software structures required by the driver.
 ******************************************************************************/
static sl_status_t sdio_drv_init(sl_cpc_bus_t *bus)
{
  sl_cpc_drv_sdio_device_t *drv = container_of(bus, sl_cpc_drv_sdio_device_t, bus);

  s_bus = bus;

  // Initialize driver frame lists.
  sli_cpc_frame_list_init(&rx_payload_pending_frames);
  sli_cpc_frame_list_init(&rx_complete_pending_frames);
  sli_cpc_frame_list_init(&rx_completed_frames);
  sli_cpc_frame_list_init(&rx_free_frame_list);
  sli_cpc_frame_list_init(&tx_pending_frames);
  sli_cpc_frame_list_init(&tx_header_pending_frames);
  sli_cpc_frame_list_init(&tx_payload_pending_frames);
  sli_cpc_frame_list_init(&tx_pending_xfer_complete_frames);

  for (uint32_t i = 0; i < CPC_SDIO_DEFAULT_AGGREGATION; i++) {
    sl_status_t status = alloc_rx_frame_to_free_list(bus);
    if (status != SL_STATUS_OK) {
      SLI_CPC_ASSERT(false);
      return status;
    }
  }

  flush_desc = SL_HAL_SDIO_DEV_DMA_DESCRIPTOR_XFER_END_INTERRUPT(flush_buff, 0);

  // Initialize local capabilities
  drv->remote_caps.max_aggregation = 1;
  drv->local_caps.max_aggregation = CPC_SDIO_DEFAULT_AGGREGATION;

  return SL_STATUS_OK;
}

/***************************************************************************/ /**
 * Read data received from the bus.
 ******************************************************************************/
static sl_status_t sdio_drv_read_data(sl_cpc_bus_t *bus, sli_cpc_frame_list_t *frames)
{
  MCU_DECLARE_IRQ_STATE;
  (void)bus;

  MCU_ENTER_ATOMIC();

  if (sli_cpc_frame_list_empty(&rx_completed_frames)) {
    MCU_EXIT_ATOMIC();
    return SL_STATUS_EMPTY;
  }

  sli_cpc_frame_list_extend(frames, &rx_completed_frames);

  MCU_EXIT_ATOMIC();

  return SL_STATUS_OK;
}

/***************************************************************************/ /**
 * Called when new data is ready to be sent by the core.
 *
 * Signal the host that new data is available to be read via the
 ******************************************************************************/
static uint32_t sdio_drv_write(sl_cpc_bus_t *bus, sli_cpc_frame_list_t *frames)
{
  uint32_t num_frames = sli_cpc_frame_list_get_len(frames);
  (void)bus;

  if (sdio_drv_get_available_write_frame_slots(bus) < num_frames) {
    // Core attempted to send more frames than available.
    SLI_CPC_ASSERT(false);

    return 0;
  }

  if (num_frames == 0) {
    goto exit;
  }

  MCU_ATOMIC_SECTION(
    // tx_pending_frames also accessed from IRQ to queue another CMD53 Read operation.
    sli_cpc_frame_list_extend(&tx_pending_frames, frames);)

  prepare_tx();

exit:
  return num_frames;
}

/***************************************************************************/ /**
 * Attempt to arm a CMD53 Read operation if frames are pending.
 ******************************************************************************/
static void prepare_tx(void)
{
  sli_cpc_frame_list_t *frames = &tx_pending_frames;
  uint32_t num_frames = sli_cpc_frame_list_get_len(frames);
  bool should_start_tx = false;
  sl_cpc_frame_t *frame;
  size_t tfer_len = 0;
  sl_status_t status;

  MCU_DECLARE_IRQ_STATE;

  MCU_ENTER_ATOMIC();
  if (!tx_busy && num_frames > 0) {
    tx_busy = true;
    should_start_tx = true;
  }
  MCU_EXIT_ATOMIC();

  if (!should_start_tx) {
    // CMD53 Read is already active or no more data to be processed, nothing to do.
    return;
  }

  // Starting a transmission.

  SLI_CPC_ASSERT(sli_cpc_frame_list_empty(&tx_header_pending_frames));
  SLI_CPC_ASSERT(sli_cpc_frame_list_empty(&tx_payload_pending_frames));

  // Start with the frame count size (4 bytes)
  tfer_len = sizeof(struct sdio_header_block);

  SLI_CPC_FRAME_LIST_FOR_EACH(frames, frame)
  {
    // Compute the size of the CMD53.
    tfer_len += SLI_CPC_HEADER_SIZE;

    if (frame && frame->payload) {
      tfer_len += ALIGNED_PAYLOAD_SIZE(frame->payload->len);
    }
  }

  // To reduce the number of individual CMD53 needed from the host, advertise
  // only a multiple of the block size to the host. If less data is sent, we will
  // pad the last block with zeroes. The host will be responsible for ignoring the
  // padding, based on the header(s).
  tfer_len = SL_DIV_ROUND_UP(tfer_len, SL_CPC_DRV_SDIO_BLOCK_SIZE) * SL_CPC_DRV_SDIO_BLOCK_SIZE;

  // Transaction starts by sending the headers.
  sli_cpc_frame_list_extend(&tx_header_pending_frames, frames);

  // Signal the SDIO host that new frames are ready to be sent.
  status = sl_hal_sdio_dev_setup_function_transfer(SL_CPC_DRV_SDIO_PERIPHERAL, SL_CPC_DRV_SDIO_FUNCTION_NUM, tfer_len);
  SLI_CPC_ASSERT(status == SL_STATUS_OK);
}

/**************************************************************************/ /**
 * Return the number of frames that can be queued in the driver.
 ******************************************************************************/
static uint32_t sdio_drv_get_available_write_frame_slots(sl_cpc_bus_t *bus)
{
  const sl_cpc_drv_sdio_device_t *drv = container_of(bus, sl_cpc_drv_sdio_device_t, bus);
  uint32_t num_available_slots;

  MCU_ATOMIC_SECTION(num_available_slots
                     = drv->remote_caps.max_aggregation - sli_cpc_frame_list_get_len(&tx_pending_frames);)

  // Sanity check for underflow
  if (num_available_slots > drv->remote_caps.max_aggregation) {
    SLI_CPC_ASSERT(false);
    return 0;
  }

  return num_available_slots;
}

/***************************************************************************/ /**
 * Start reception
 ******************************************************************************/
static sl_status_t sdio_drv_start_rx(sl_cpc_bus_t *bus)
{
  (void)bus;

  // Enable function 1 CMD53 write start to start reception.
  sl_hal_sdio_dev_enable_function_interrupts(SL_CPC_DRV_SDIO_PERIPHERAL, SL_CPC_DRV_SDIO_FUNCTION_NUM,
                                             SDIO_FN1_INT_EN_SIG_FN1_SDIO_WR_START_EN);

  return SL_STATUS_OK;
}

static void adma_irq_handler(void)
{
  switch (adma_state) {
    case ADMA_RECEIVE_HEADER:
      // Process the aggregated headers received from the host
      adma_rx_start_headers();
      break;

    case ADMA_RECEIVE_PAYLOAD:
      // Process the aggregated payloads received from the host
      adma_rx_start_payloads();
      break;

    case ADMA_FLUSH_SDIO:
      // Continue with flushing data until SDIO transfer is complete.
      start_flush_transfer(SL_CPC_DRV_SDIO_BLOCK_SIZE);
      break;

    case ADMA_TRANSMIT_HEADER:
      adma_tx_prepare_header_descriptors();
      break;

    case ADMA_TRANSMIT_PAYLOAD:
      adma_tx_prepare_payload_descriptors();
      break;

    default:
      SLI_CPC_ASSERT(false);
      break;
  }
}

/***************************************************************************/ /**
 * Handle SDIO CMD53 write start. Start a reception operation.
 *
 * The host will start by sending a complete block with the following structure:
 * [Number of frames (n) | Frame header 1 | ... | Frame header n]
 *
 * The secondary must receive these frames headers in order to know how many frames
 * are coming, and where to store the payload data.
 ******************************************************************************/
static void on_sdio_write_start(void)
{
  // Any prior transaction should be complete prior to starting a new one.
  SLI_CPC_ASSERT(adma_state == ADMA_IDLE);

  adma_rx_start_frame_count();
}

/***************************************************************************/ /**
 * CMD53 write completed, finished reception. Notify the core that new data is
 * available to be read from the driver.
 ******************************************************************************/
static void on_sdio_write_complete(void)
{
  sli_cpc_frame_list_extend(&rx_completed_frames, &rx_complete_pending_frames);

  // Notify the core that new data is available to be read.
  sli_cpc_bus_notify_rx_data_from_drv(s_bus);

  // ADMA transfer is complete.
  adma_state = ADMA_IDLE;
}

/***************************************************************************/ /**
 * Handle SDIO CMD53 read start, begin transmission.
 *
 * Start the ADMA transfer that was previously armed prior to signaling the host
 * that new data was available to be read.
 ******************************************************************************/
static void on_sdio_read_start(void)
{
  if (!tx_busy) {
    // Host attempted a read when we had no frames to send. There is nothing we
    // can do here, the CMD53 will timeout and we will have to handle the error
    // later on.
    SLI_CPC_ASSERT(false);
    return;
  }

  // Any prior transaction should be complete prior to starting a new one.
  SLI_CPC_ASSERT(adma_state == ADMA_IDLE);

  // Aggregation enabled - multiple frames can be pending.
  adma_tx_prepare_frame_count();
}

/***************************************************************************/ /**
 * Handle SDIO CMD53 read over. Transmission completed.
 ******************************************************************************/
static void on_sdio_read_complete(void)
{
  sli_cpc_bus_notify_tx_data_by_drv(s_bus, &tx_pending_xfer_complete_frames);

  // Transmission done.
  tx_busy = false;

  // ADMA transfer is complete.
  adma_state = ADMA_IDLE;

  // Check if any new frames were submitted while we were busy.
  prepare_tx();
}

/***************************************************************************/ /**
 * Start the ADMA transfer for the incoming aggregated frames.
 ******************************************************************************/
static void adma_rx_start_frame_count(void)
{
  size_t idx = 0;

  memset(&sdio_hdr_block, 0, sizeof(struct sdio_header_block));

  // Arm the ADMA for the next raw header.
  frame_descriptors[idx++]
    = SL_HAL_SDIO_DEV_DMA_DESCRIPTOR_XFER_END_INTERRUPT(&sdio_hdr_block, sizeof(struct sdio_header_block));

  start_transfer(idx, ADMA_RECEIVE_HEADER);
}

/***************************************************************************/ /**
 * This function reads the receive frame count and chain the adma descriptors for
 * the incoming headers.
 ******************************************************************************/
static void adma_rx_start_headers(void)
{
  uint8_t frame_count = sdio_hdr_block.frame_count;
  sl_cpc_frame_t *frame = NULL;
  uint8_t idx = 0;
  MCU_DECLARE_IRQ_STATE;

  if (frame_count == 0) {
    SLI_CPC_LOG_WARN("[SDIO] RX - Received frame count is 0, flush the whole transfer");
    adma_rx_abort_header_transfer();
    return;
  }

  // validate the frame count
  if (frame_count > s_sdio_device_drv.local_caps.max_aggregation) {
    SLI_CPC_LOG_ERROR(
      "[SDIO] RX - Received frame count (%u) exceeds local driver capabilities, flush the whole transfer", frame_count);
    adma_rx_abort_header_transfer();
    return;
  }

  MCU_ENTER_ATOMIC();
  if (rx_free_frame_list.len < frame_count) {
    rx_alloc_stalled = true;
    MCU_EXIT_ATOMIC();
    return;
  }
  MCU_EXIT_ATOMIC();

  for (uint32_t i = 0; i < frame_count; i++) {
    // Check that maximum descriptor count is not exceeded.
    if (idx >= CPC_DRV_SDIO_DESC_COUNT) {
      // No more descriptors available, start a transfer and continue with headers preparation.

      // Update frame count buffer with remaining frames for the next iteration.
      sdio_hdr_block.frame_count = frame_count - idx;
      start_transfer(idx, ADMA_RECEIVE_HEADER);
      return;
    }

    MCU_ENTER_ATOMIC();
    frame = sli_cpc_frame_list_pop(&rx_free_frame_list);
    MCU_EXIT_ATOMIC();
    SLI_CPC_ASSERT(frame != NULL);

    frame_descriptors[idx++]
      = SL_HAL_SDIO_DEV_DMA_DESCRIPTOR_XFER(sli_cpc_frame_get_header(frame), SLI_CPC_HEADER_SIZE);

    // Push the frame to the payload pending list.
    sli_cpc_frame_list_push_back(&rx_payload_pending_frames, frame);
    frame = NULL;
  }

  // Done preparing descriptors for headers, start ADMA transfer and continue with payloads preparation.
  start_transfer(idx, ADMA_RECEIVE_PAYLOAD);
}

/***************************************************************************/ /**
 * Abort an RX header transfer by flushing the SDIO block and clearing pending lists.
 ******************************************************************************/
static void adma_rx_abort_header_transfer(void)
{
  // CPC-2989: Harden SDIO driver abort transfer behavior.
  clear_frame_list(&rx_payload_pending_frames);
  start_flush_transfer(SL_CPC_DRV_SDIO_BLOCK_SIZE);
}

/***************************************************************************/ /**
 * Abort an RX payload transfer by flushing the SDIO block and clearing pending lists.
 ******************************************************************************/
static void adma_rx_abort_payload_transfer(sl_cpc_frame_t *frame)
{
  // CPC-2989: Harden SDIO driver abort transfer behavior.
  sli_cpc_frame_put_ref(&frame);
  clear_frame_list(&rx_payload_pending_frames);
  clear_frame_list(&rx_complete_pending_frames);
  start_flush_transfer(SL_CPC_DRV_SDIO_BLOCK_SIZE);
}

/***************************************************************************/ /**
 * This function prepares the adma descriptors for the incoming payloads.
 ******************************************************************************/
static void adma_rx_start_payloads(void)
{
  sl_cpc_frame_t *frame = NULL;
  bool has_payload = false;
  size_t transfer_len = 0;
  size_t payload_len = 0;
  uint32_t list_len = 0;
  sl_status_t status;
  size_t idx = 0;

  list_len = sli_cpc_frame_list_get_len(&rx_payload_pending_frames);

  // validate the frame count
  if (list_len == 0) {
    SLI_CPC_LOG_ERROR("[SDIO] RX - Pending payload list length is 0, flush the whole transfer");
    adma_rx_abort_payload_transfer(NULL);
    return;
  }

  for (uint32_t i = 0; i < list_len; i++) {
    // Check that maximum descriptor count is not exceeded.
    if (idx >= CPC_DRV_SDIO_DESC_COUNT) {
      // No more descriptors available, start a transfer and continue with payloads preparation.
      start_transfer(idx, ADMA_RECEIVE_PAYLOAD);
      return;
    }

    // Get a frame for the current payload.
    frame = sli_cpc_frame_list_pop(&rx_payload_pending_frames);
    if (frame == NULL) {
      SLI_CPC_LOG_WARN("[SDIO] RX - Frame is NULL, flush the whole transfer");
      adma_rx_abort_payload_transfer(NULL);
      return;
    }

    if (!sli_cpc_header_is_size_valid(sli_cpc_frame_get_header(frame))) {
      // Invalid header size - discard the frame
      SLI_CPC_LOG_WARN("[SDIO] RX - Invalid header size, flush the whole transfer");
      adma_rx_abort_payload_transfer(frame);
      return;
    }

    /*
     * SDIO hardware performs CRC validation on all block transfers.
     * Therefore, payload checksum validation is not required for this driver.
     * Set to true to satisfy datalink layer requirements until
     * that part is refactored.
     */
    frame->payload_csum_is_valid = true;

    status = sli_cpc_alloc_rx_payload(s_bus, frame);

    // Check if the frame has a payload
    payload_len = sli_cpc_header_get_payload_size(sli_cpc_frame_get_header(frame));
    if (payload_len > 0) {
      if (status == SL_STATUS_OK) {
        // Calculate the transfer length to reach the next 4-byte boundary.
        transfer_len = ALIGNED_PAYLOAD_SIZE(payload_len);

        // Prepare the ADMA descriptor for the payload including the trailing padding.
        frame_descriptors[idx++] = SL_HAL_SDIO_DEV_DMA_DESCRIPTOR_XFER(frame->payload->ptr, transfer_len);
        has_payload = true;
      } else {
        // CPC-2989: The frame has a payload but no RX buffer could be allocated. It should be
        // discarded while processing continues for remaining frames; for now, abort.
        adma_rx_abort_payload_transfer(frame);
        return;
      }
    }
    // Push the frame to the complete pending frames list.
    sli_cpc_frame_list_push_back(&rx_complete_pending_frames, frame);
    frame = NULL;
  }

  // Done preparing descriptors for payloads, start ADMA transfer and continue with flushing the remaining padding bytes.
  if (has_payload) {
    start_transfer(idx, ADMA_FLUSH_SDIO);
  } else {
    start_flush_transfer(0);
  }
}

/***************************************************************************/ /**
 * Prepare the ADMA descriptor for the frame count.
 ******************************************************************************/
static void adma_tx_prepare_frame_count(void)
{
  uint32_t list_len = 0;
  uint8_t frame_count = 0;
  size_t idx = 0;

  // Prepare the aggregate header block with the frame count.
  list_len = sli_cpc_frame_list_get_len(&tx_header_pending_frames);
  if (!list_len) {
    // No frames to send, nothing to do.
    SLI_CPC_LOG_ERROR("[SDIO] No frames to send - aborting transfer");
    start_flush_transfer(SL_CPC_DRV_SDIO_BLOCK_SIZE);
    return;
  }

  memset(&sdio_hdr_block, 0, sizeof(struct sdio_header_block));

  frame_count = (uint8_t)SL_MIN(list_len, s_sdio_device_drv.remote_caps.max_aggregation);
  sdio_hdr_block.frame_count = frame_count;

  // Add frame count as first descriptor
  frame_descriptors[idx++] = SL_HAL_SDIO_DEV_DMA_DESCRIPTOR_XFER(&sdio_hdr_block, sizeof(struct sdio_header_block));

  start_transfer(idx, ADMA_TRANSMIT_HEADER);
}

/***************************************************************************/ /**
 * Prepare the ADMA descriptors for the headers.
 ******************************************************************************/
static void adma_tx_prepare_header_descriptors(void)
{
  sl_cpc_frame_t *frame = NULL;
  uint32_t tx_frame_count = 0;
  uint32_t list_len = 0;
  size_t idx = 0;

  // Prepare the aggregate header block.
  list_len = sli_cpc_frame_list_get_len(&tx_header_pending_frames);
  if (!list_len) {
    // No frames to send, nothing to do.
    adma_state = ADMA_IDLE;
    return;
  }

  // Validate the frame count.
  tx_frame_count = SL_MIN(list_len, s_sdio_device_drv.remote_caps.max_aggregation);
  SLI_CPC_ASSERT(tx_frame_count > 0);

  for (uint32_t i = 0; i < tx_frame_count; i++) {
    // Check that maximum descriptor count is not exceeded.
    if (idx >= CPC_DRV_SDIO_DESC_COUNT) {
      // No more descriptors available, start a transfer and continue with headers preparation.
      start_transfer(idx, ADMA_TRANSMIT_HEADER);
      return;
    }

    // Pop the frame from the header pending list and push it to the payload pending list.
    frame = sli_cpc_frame_list_pop(&tx_header_pending_frames);
    if (frame == NULL) {
      SLI_CPC_LOG_WARN("[SDIO] TX - Frame count exceeds list length");
      // This should never happen, as we should pop less or equal to the frame count.
      SLI_CPC_ASSERT(false);
      return;
    }

    frame_descriptors[idx++]
      = SL_HAL_SDIO_DEV_DMA_DESCRIPTOR_XFER(sli_cpc_frame_get_header(frame), SLI_CPC_HEADER_SIZE);

    // Move frame to pending list to notify completion.
    sli_cpc_frame_list_push_back(&tx_payload_pending_frames, frame);
  }

  // Done preparing descriptors for headers, start ADMA transfer and continue with payloads preparation.
  start_transfer(idx, ADMA_TRANSMIT_PAYLOAD);
}

/***************************************************************************/ /**
 * Prepare the ADMA descriptors for the payloads.
 ******************************************************************************/
static void adma_tx_prepare_payload_descriptors(void)
{
  sl_cpc_frame_t *frame = NULL;
  size_t unaligned_len = 0;
  bool has_payload = false;
  size_t aligned_len = 0;
  size_t payload_len = 0;
  uint32_t dword_idx = 0;
  uint32_t list_len = 0;
  size_t idx = 0;

  list_len = sli_cpc_frame_list_get_len(&tx_payload_pending_frames);

  if (!list_len) {
    // No frames to send, nothing to do.
    SLI_CPC_LOG_ERROR("[SDIO] No frames to send - aborting transfer");
    start_flush_transfer(SL_CPC_DRV_SDIO_BLOCK_SIZE);
    return;
  }

  // link the frame payload descriptors
  for (uint32_t i = 0; i < list_len; i++) {
    // Check that maximum descriptor count is not exceeded, accomodate extra descriptor for unaligned payload.
    if (idx >= (CPC_DRV_SDIO_DESC_COUNT - 1)) {
      // No more descriptors available, start a transfer and continue with payloads preparation.
      start_transfer(idx, ADMA_TRANSMIT_PAYLOAD);
      return;
    }

    frame = sli_cpc_frame_list_pop(&tx_payload_pending_frames);
    if (frame == NULL) {
      SLI_CPC_LOG_WARN("[SDIO] TX - Frame count exceeds list length");
      // This should never happen, as we should pop less or equal to the list length.
      SLI_CPC_ASSERT(false);
      return;
    }

    if (frame->payload) {
      uint8_t *payload = frame->payload->ptr;
      payload_len = sli_cpc_header_get_payload_size(sli_cpc_frame_get_header(frame));

      // Calculate the unaligned and aligned lengths.
      unaligned_len = payload_len % SL_CPC_BUF_MIN_ALIGNMENT;
      aligned_len = payload_len - unaligned_len;

      if (aligned_len > 0) {
        frame_descriptors[idx++] = SL_HAL_SDIO_DEV_DMA_DESCRIPTOR_XFER(payload, aligned_len);
      }

      // Add the unaligned part to the realignment buffer.
      if (unaligned_len > 0) {
        // Clear the unaligned payload pool to avoid garbage data.
        memset(&unaligned_payload_pool[dword_idx], 0, SL_CPC_BUF_MIN_ALIGNMENT);
        memcpy(&unaligned_payload_pool[dword_idx], &payload[aligned_len], unaligned_len);

        frame_descriptors[idx++]
          = SL_HAL_SDIO_DEV_DMA_DESCRIPTOR_XFER(&unaligned_payload_pool[dword_idx], SL_CPC_BUF_MIN_ALIGNMENT);
        dword_idx++;
      }
      has_payload = true;
    }
    // Move frame to pending list to notify completion.
    sli_cpc_frame_list_push_back(&tx_pending_xfer_complete_frames, frame);
  }

  // Done preparing descriptors for payloads, start ADMA transfer and continue with flushing the remaining padding bytes.
  if (has_payload) {
    start_transfer(idx, ADMA_FLUSH_SDIO);
  } else {
    start_flush_transfer(0);
  }
}

/***************************************************************************/ /**
 * This function clears the frame list.
 ******************************************************************************/
static void clear_frame_list(sli_cpc_frame_list_t *list)
{
  sl_cpc_frame_t *frame;

  while (!sli_cpc_frame_list_empty(list)) {
    frame = sli_cpc_frame_list_pop(list);
    sli_cpc_frame_put_ref(&frame);
  }
}

/***************************************************************************/ /**
 * This function starts the ADMA transfer.
 ******************************************************************************/
static void start_transfer(size_t idx, adma_state_t next_state)
{
  sl_status_t status;

  if (idx == 0 || idx > CPC_DRV_SDIO_DESC_COUNT) {
    SLI_CPC_LOG_ERROR("[SDIO] Invalid descriptor count for ADMA transfer: %u", (unsigned)idx);
    SLI_CPC_ASSERT(false);
    start_flush_transfer(SL_CPC_DRV_SDIO_BLOCK_SIZE);
    return;
  }

  adma_state = next_state;
  frame_descriptors[idx - 1].xfer.end_ifs = true;
  status = sl_hal_sdio_dev_start_dma_transfer(SL_CPC_DRV_SDIO_PERIPHERAL, frame_descriptors);
  if (status != SL_STATUS_OK) {
    SLI_CPC_LOG_ERROR("[SDIO] Failed to start ADMA transfer, state: %u", next_state);
    SLI_CPC_ASSERT(false);
    return;
  }
}

/***************************************************************************/ /**
 * This function starts the ADMA flush transfer.
 ******************************************************************************/
static void start_flush_transfer(uint32_t xfer_count)
{
  sl_status_t status;

  adma_state = ADMA_FLUSH_SDIO;
  flush_desc.xfer.xfer_count = xfer_count;
  status = sl_hal_sdio_dev_start_dma_transfer(SL_CPC_DRV_SDIO_PERIPHERAL, &flush_desc);
  if (status != SL_STATUS_OK) {
    SLI_CPC_LOG_ERROR("[SDIO] Failed to start ADMA flush transfer");
    SLI_CPC_ASSERT(false);
    return;
  }
}

/***************************************************************************/ /**
 * Driver hook invoked by sli_cpc_frame_free() after an RX frame has been
 * returned to the per-bus pool (so the pool's free count is already
 * up-to-date).
 *
 * Refills @ref rx_free_frame_list with a freshly allocated frame so the
 * SDIO IRQ has a buffer ready for the next CMD53 RX, and -- if a previous IRQ
 * had stalled because the free list was empty -- re-arms ADMA via
 * adma_rx_start_headers() to unblock the host on the bus.
 ******************************************************************************/
static void cpc_drv_sdio_on_rx_frame_free(sl_cpc_bus_t *bus)
{
  bool resume = false;
  MCU_DECLARE_IRQ_STATE;

  (void)bus;

  alloc_rx_frame_to_free_list(s_bus);

  MCU_ENTER_ATOMIC();
  if (rx_alloc_stalled) {
    rx_alloc_stalled = false;
    resume = true;
  }
  MCU_EXIT_ATOMIC();

  if (resume) {
    adma_rx_start_headers();
  }
}

/***************************************************************************/ /**
 * Allocate a single RX frame and push it onto rx_free_frame_list, ready
 * to be popped by the SDIO IRQ. The free list is replenished as RX-pool slots
 * become available via the @ref cpc_drv_sdio_on_rx_frame_free hook.
 ******************************************************************************/
static sl_status_t alloc_rx_frame_to_free_list(sl_cpc_bus_t *bus)
{
  sl_cpc_frame_t *frame;

  frame = sli_cpc_frame_new(bus, true);
  if (frame == NULL) {
    return SL_STATUS_ALLOCATION_FAILED;
  }

  MCU_ATOMIC_SECTION(sli_cpc_frame_list_push_back(&rx_free_frame_list, frame);)

  return SL_STATUS_OK;
}

sl_cpc_drv_sdio_device_t sl_cpc_drv_sdio_device_instances[SL_CPC_DRV_SDIO_DEVICE_INSTANCES_COUNT];
