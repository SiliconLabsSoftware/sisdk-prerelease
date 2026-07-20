/***************************************************************************//**
 * @file
 * @brief Device Manager UART.
 *******************************************************************************
 * # License
 * <b>Copyright 2026 Silicon Laboratories, Inc. www.silabs.com</b>
 ******************************************************************************
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
 *****************************************************************************/

#ifndef SL_DEVICE_UART_H
#define SL_DEVICE_UART_H

#include <stdint.h>
#include <stdbool.h>
#include "sl_enum.h"

#ifdef __cplusplus
extern "C" {
#endif

/***************************************************************************//**
 * @addtogroup device_uart Device Manager UART
 * @details
 * ## Overview
 *
 * The Device Manager UART component defines the macros,
 * structures, and enums that are used common across UART driver and
 * peripheral.
 *
 * @{
 ******************************************************************************/

// ----------------------------------------------------------------------------
// ENUMS

SL_ENUM(sl_uart_parity_t) {
  SL_UART_PARITY_NONE,
  SL_UART_PARITY_ODD,
  SL_UART_PARITY_EVEN
};

SL_ENUM(sl_uart_stop_bits_t) {
  SL_UART_STOP_BITS_0_5,
  SL_UART_STOP_BITS_1,
  SL_UART_STOP_BITS_1_5,
  SL_UART_STOP_BITS_2
};

SL_ENUM(sl_uart_data_bits_t) {
  SL_UART_DATA_BITS_7,
  SL_UART_DATA_BITS_8,
  SL_UART_DATA_BITS_9,
  SL_UART_DATA_BITS_10,
  SL_UART_DATA_BITS_11,
  SL_UART_DATA_BITS_12,
  SL_UART_DATA_BITS_13,
  SL_UART_DATA_BITS_14,
  SL_UART_DATA_BITS_15,
  SL_UART_DATA_BITS_16
};

SL_ENUM(sl_uart_flow_control_t) {
  SL_UART_FLOW_CONTROL_NONE,
  SL_UART_FLOW_CONTROL_CTS_RTS,
  SL_UART_FLOW_CONTROL_SOFT
};

// ----------------------------------------------------------------------------
// DEFINES

#define SL_UART_BAUDRATE_AUTO  0xFFFFFFFF

// ----------------------------------------------------------------------------
// TYPEDEFS

typedef struct uart_config {
  uint32_t baudrate;
  sl_uart_parity_t parity;
  sl_uart_stop_bits_t stop_bits;
  sl_uart_data_bits_t data_bits;
  sl_uart_flow_control_t flow_control;
} sl_uart_config_t;

/// @cond DO_NOT_INCLUDE_WITH_DOXYGEN

/// @endcond

/** @} (end addtogroup device_uart) */

#ifdef __cplusplus
}
#endif

#endif // SL_DEVICE_UART_H
