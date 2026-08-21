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
#include "sl_assert.h"

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

/// UART Parity mode.
SL_ENUM(sl_uart_parity_t) {
  SL_UART_PARITY_NONE = 0,
  SL_UART_PARITY_ODD,
  SL_UART_PARITY_EVEN
};

/// Number of UART stop bits.
SL_ENUM(sl_uart_stop_bits_t) {
  SL_UART_STOP_BITS_0_5 = 0,
  SL_UART_STOP_BITS_1,
  SL_UART_STOP_BITS_1_5,
  SL_UART_STOP_BITS_2
};

/// Number of UART data bits.
SL_ENUM(sl_uart_data_bits_t) {
  SL_UART_DATA_BITS_4 = 4,
  SL_UART_DATA_BITS_5,
  SL_UART_DATA_BITS_6,
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

/// UART Flow control mode.
SL_ENUM(sl_uart_flow_control_t) {
  SL_UART_FLOW_CONTROL_NONE = 0,
  SL_UART_FLOW_CONTROL_CTS_RTS,
  SL_UART_FLOW_CONTROL_SOFT
};

/// UART Oversampling mode.
SL_ENUM(sl_uart_oversampling_t) {
  SL_UART_OVERSAMPLING_0 = 0,
  SL_UART_OVERSAMPLING_4 = 4,
  SL_UART_OVERSAMPLING_6 = 6,
  SL_UART_OVERSAMPLING_8 = 8,
  SL_UART_OVERSAMPLING_16 = 16
};

// ----------------------------------------------------------------------------
// DEFINES

/// Auto baud rate detection.
#define SL_UART_BAUDRATE_AUTO  0xFFFFFFFF

// ----------------------------------------------------------------------------
// TYPEDEFS

/// UART configuration.
typedef struct uart_config {
  uint32_t baudrate;
  sl_uart_parity_t parity;
  sl_uart_stop_bits_t stop_bits;
  sl_uart_data_bits_t data_bits;
  sl_uart_flow_control_t flow_control;
  sl_uart_oversampling_t oversampling;
} sl_uart_config_t;

// ----------------------------------------------------------------------------
// PROTOTYPES

/***************************************************************************//**
 * Converts the UART data bits to a number of bits.
 *
 * @param[in]  data_bits UART data bits.
 *
 * @return The number of bits of the data bits.
 ******************************************************************************/
static inline uint8_t sl_uart_data_bits_to_count(sl_uart_data_bits_t data_bits)
{
  if (data_bits < SL_UART_DATA_BITS_4
      || data_bits > SL_UART_DATA_BITS_16) {
    EFM_ASSERT(false);
    return 8;
  }

  return (uint8_t)data_bits;
}

/***************************************************************************//**
 * Converts the UART stop bits to a number of bits, rounding up to the closest integer.
 *
 * @param[in]  stop_bits UART stop bits.
 *
 * @return The number of stop bits.
 ******************************************************************************/
static inline uint8_t sl_uart_stop_bits_to_count(sl_uart_stop_bits_t stop_bits)
{
  switch (stop_bits) {
    case SL_UART_STOP_BITS_0_5:
    // Round up to closest integer
    case SL_UART_STOP_BITS_1:
      return 1;
    case SL_UART_STOP_BITS_1_5:
    // Round up to closest integer
    case SL_UART_STOP_BITS_2:
      return 2;
    default:
      EFM_ASSERT(false);
      return 1;
  }
}

/***************************************************************************//**
 * Converts the UART parity to a number of bits.
 *
 * @param[in]  parity UART parity.
 *
 * @return The number of parity bits.
 ******************************************************************************/
static inline uint8_t sl_uart_parity_to_count(sl_uart_parity_t parity)
{
  switch (parity) {
    case SL_UART_PARITY_NONE:
      return 0;
    case SL_UART_PARITY_ODD:
    case SL_UART_PARITY_EVEN:
      return 1;
    default:
      EFM_ASSERT(false);
      return 0;
  }
}

/***************************************************************************//**
 * Calculates the frame size for the given UART configuration.
 *
 * @param[in]  config UART configuration.
 *
 * @return The frame size.
 ******************************************************************************/
static inline uint8_t sl_uart_config_get_frame_size(sl_uart_config_t config)
{
  return 1 // Start bit
         + sl_uart_data_bits_to_count(config.data_bits)
         + sl_uart_parity_to_count(config.parity)
         + sl_uart_stop_bits_to_count(config.stop_bits);
}

/***************************************************************************//**
 * Converts the UART oversampling to a number of bits.
 *
 * @param[in]  oversampling UART oversampling.
 *
 * @return The oversampling factor.
 ******************************************************************************/
static inline uint8_t sl_uart_oversampling_to_count(sl_uart_oversampling_t oversampling)
{
  switch (oversampling) {
    case SL_UART_OVERSAMPLING_0:
      return SL_UART_OVERSAMPLING_0;
    case SL_UART_OVERSAMPLING_4:
      return SL_UART_OVERSAMPLING_4;
    case SL_UART_OVERSAMPLING_6:
      return SL_UART_OVERSAMPLING_6;
    case SL_UART_OVERSAMPLING_8:
      return SL_UART_OVERSAMPLING_8;
    case SL_UART_OVERSAMPLING_16:
      return SL_UART_OVERSAMPLING_16;
    default:
      EFM_ASSERT(false);
      return SL_UART_OVERSAMPLING_16;
  }
}

/// @cond DO_NOT_INCLUDE_WITH_DOXYGEN

/// @endcond

/** @} (end addtogroup device_uart) */

#ifdef __cplusplus
}
#endif

#endif // SL_DEVICE_UART_H
