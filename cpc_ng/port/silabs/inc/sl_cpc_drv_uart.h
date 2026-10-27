/***************************************************************************/ /**
 * @file sl_cpc_drv_uart.h
 * @brief UART driver interface for CPC.
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

#ifndef SL_CPC_DRV_UART_H
#define SL_CPC_DRV_UART_H

#include <stddef.h>
#include <stdint.h>

#include "sl_cpc_bus.h"
#include "sl_uart.h"
#include "sli_cpc_drv.h"
#include "sli_cpc_frame_list.h"
#include "sli_cpc_hdr.h"

struct sli_cpc_drv_uart_header {
  uint8_t sof;
  sli_cpc_hdr_t hdr;
  uint8_t hdr_crc[sizeof(uint16_t)];
};

struct sli_cpc_drv_uart_tx {
  sl_cpc_frame_t *frame;
  struct sli_cpc_drv_uart_header header;
  sli_cpc_frame_list_t pending_frame_list;
};

enum sli_cpc_drv_uart_rx_state {
  SLI_CPC_DRV_UART_RX_STATE_RECEIVING_HEADER_BLOCK,
  SLI_CPC_DRV_UART_RX_STATE_RECEIVING_PAYLOAD,
  SLI_CPC_DRV_UART_RX_STATE_RECEIVING_PAYLOAD_CRC,
  SLI_CPC_DRV_UART_RX_STATE_DISCARDING_PAYLOAD,
};

struct sli_cpc_drv_uart_rx {
  enum sli_cpc_drv_uart_rx_state state;
  sl_cpc_frame_t *frame;
  struct sli_cpc_drv_uart_header header;
  uint16_t payload_bytes_remaining;
  size_t hdr_bytes_received;
  sli_cpc_frame_list_t free_frame_list;
  sli_cpc_frame_list_t pending_frame_list;
  bool waiting_for_frame;
  bool rx_started;
  bool suspended;
};

/** @brief UART CPC driver configuration. */
struct sl_cpc_drv_uart_config {
  sl_peripheral_t peripheral;
  sl_uart_pin_config_t pins;
  uint32_t baudrate;
  sl_uart_flow_control_t flow_control;
  sl_uart_oversampling_t oversampling;
};

struct sl_cpc_drv_uart {
  sl_cpc_bus_t bus;
  sl_uart_handle_t uart_handle;
  struct sli_cpc_drv_uart_tx tx;
  struct sli_cpc_drv_uart_rx rx;
};

#endif /* SL_CPC_DRV_UART_H */
