/* Copyright 2026 Silicon Laboratories Inc. */

#ifndef SL_CPC_DRV_SPI_CONTROLLER_H
#define SL_CPC_DRV_SPI_CONTROLLER_H

#include <stddef.h>
#include <stdint.h>

#include "sl_cpc_bus.h"
#include "sl_cpc_drv_spi.h"
#include "sl_device_gpio.h"
#include "sl_device_peripheral.h"
#include "sl_status.h"
#include "spidrv.h"

#include "sli_cpc_drv.h"

enum sl_cpc_drv_spi_controller_state {
  SL_CPC_DRV_SPI_CONTROLLER_STATE_IDLE,
  SL_CPC_DRV_SPI_CONTROLLER_STATE_TFER_HDR,
  SL_CPC_DRV_SPI_CONTROLLER_STATE_TFER_PAYLOAD,
};

struct sl_cpc_drv_spi_controller_config {
  sl_peripheral_t peripheral;
  sl_gpio_t copi;
  sl_gpio_t cipo;
  sl_gpio_t clk;
  sl_gpio_t cs;
  sl_gpio_t irq;
  uint32_t bitrate;
};

struct sl_cpc_drv_spi_controller_rx {
  // Frame currently being received and its payload slice.
  struct {
    sl_cpc_frame_t *frame;
    // Bucket for receiving SPI header.
    sli_cpc_drv_spi_header_t hdr;
    // Current payload buffer; NULL once past the chain (CRC) or if no payload.
    sl_cpc_buf_t *buf;
    // Byte offset into buf, or into the payload CRC once buf is NULL.
    size_t offset;
  } active;

  // Pre-allocated frames available for receiving.
  sli_cpc_frame_list_t free_frame_list;
  // Frames that have been received and are waiting for the core.
  sli_cpc_frame_list_t pending_frame_list;

  // Driver ran out of RX frame.
  bool need_frame;
};

struct sl_cpc_drv_spi_controller_tx {
  // Frame currently being sent and its payload slice.
  struct {
    sl_cpc_frame_t *frame;
    // Bucket for sending SPI header.
    sli_cpc_drv_spi_header_t hdr;
    // Current payload buffer; NULL once past the chain (CRC) or if no payload.
    sl_cpc_buf_t *buf;
    // Byte offset into buf, or into the payload CRC once buf is NULL.
    size_t offset;
  } active;

  // Frames ready for transmission waiting to be sent on the wire.
  sli_cpc_frame_list_t submitted_frame_list;
  // Frames that have been completely sent over the bus.
  sli_cpc_frame_list_t completed_frame_list;
};

struct sl_cpc_drv_spi_controller {
  sl_peripheral_t peripheral;
  SPIDRV_HandleData_t spidrv;
  sl_cpc_bus_t bus;
  struct {
    sl_gpio_t gpio;
    int32_t int_no;
  } irq;
  sl_gpio_t cs;
  enum sl_cpc_drv_spi_controller_state state;
  size_t tfer_count;
  struct sl_cpc_drv_spi_controller_rx rx;
  struct sl_cpc_drv_spi_controller_tx tx;
};

#endif /* SL_CPC_DRV_SPI_CONTROLLER_H */
