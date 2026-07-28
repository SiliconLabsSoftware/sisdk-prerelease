/***************************************************************************//**
 * @file
 * @brief Device Manager I2S / I2ST shared contract.
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

#ifndef SL_DEVICE_I2S_H
#define SL_DEVICE_I2S_H

#include "sl_enum.h"

#if defined(SL_COMPONENT_CATALOG_PRESENT)
#include "sl_component_catalog.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

/***************************************************************************//**
 * @addtogroup device_i2s Device Manager I2S
 * @details
 * ## Overview
 *
 * Shared enums, capability limits, and validation macros for I2S and I2ST used
 * by the HAL and high-level driver public APIs.
 *
 * HAL configuration structures live in @ref sl_hal_i2s.h.
 *
 * Channel IRQ composites and ISR decode live in @ref sl_hal_i2s.h (HAL only).
 *
 * Instance identity (base address, bus clock, DMA signals) lives in
 * @ref sl_device_peripheral.h.
 *
 * @{
 ******************************************************************************/

// ----------------------------------------------------------------------------
// ENUMS

/// I2S operating mode (controller or target).
SL_ENUM(sl_i2s_operating_mode_t) {
  SL_I2S_OPERATING_MODE_TARGET = 0x0,
  SL_I2S_OPERATING_MODE_CONTROLLER,
  SL_I2S_OPERATING_MODE_MAX
};

/// I2S data format (standard, TDM, left-justified, right-justified).
SL_ENUM(sl_i2s_data_format_t) {
  SL_I2S_DATA_FORMAT_STANDARD = 0x0,
  SL_I2S_DATA_FORMAT_TDM,
  SL_I2S_DATA_FORMAT_LJ,
  SL_I2S_DATA_FORMAT_RJ,
  SL_I2S_DATA_FORMAT_MAX
};

/// Serial data bit order.
SL_ENUM(sl_i2s_endianness_t) {
  SL_I2S_ENDIANNESS_MSB_FIRST = 0x0,
  SL_I2S_ENDIANNESS_LSB_FIRST,
  SL_I2S_ENDIANNESS_MAX
};

/// Channel index.
SL_ENUM(sl_i2s_channel_id_t) {
  SL_I2S_CHANNEL_ID_0 = 0x0,
  SL_I2S_CHANNEL_ID_1,
  SL_I2S_CHANNEL_ID_2,
  SL_I2S_CHANNEL_ID_MAX
};

/// Channel / block direction (TX, RX, or both). Driver-only BOTH is not listed here.
SL_ENUM(sl_i2s_direction_t) {
  SL_I2S_DIR_TX = 0x0,
  SL_I2S_DIR_RX,
  SL_I2S_DIR_TX_RX,
  SL_I2S_DIR_MAX
};

/// Word length per path.
SL_ENUM(sl_i2s_word_length_t) {
  SL_I2S_WORD_LENGTH_NONE = 0x0,
  SL_I2S_WORD_LENGTH_12_BIT,
  SL_I2S_WORD_LENGTH_16_BIT,
  SL_I2S_WORD_LENGTH_20_BIT,
  SL_I2S_WORD_LENGTH_24_BIT,
  SL_I2S_WORD_LENGTH_MAX
};

/// FIFO trigger level.
SL_ENUM(sl_i2s_fifo_trigger_level_t) {
  SL_I2S_FIFO_TRIGGER_LEVEL_1 = 0x0,
  SL_I2S_FIFO_TRIGGER_LEVEL_2,
  SL_I2S_FIFO_TRIGGER_LEVEL_3,
  SL_I2S_FIFO_TRIGGER_LEVEL_4,
  SL_I2S_FIFO_TRIGGER_LEVEL_5,
  SL_I2S_FIFO_TRIGGER_LEVEL_6,
  SL_I2S_FIFO_TRIGGER_LEVEL_7,
  SL_I2S_FIFO_TRIGGER_LEVEL_8,
  SL_I2S_FIFO_TRIGGER_LEVEL_9,
  SL_I2S_FIFO_TRIGGER_LEVEL_10,
  SL_I2S_FIFO_TRIGGER_LEVEL_11,
  SL_I2S_FIFO_TRIGGER_LEVEL_12,
  SL_I2S_FIFO_TRIGGER_LEVEL_13,
  SL_I2S_FIFO_TRIGGER_LEVEL_14,
  SL_I2S_FIFO_TRIGGER_LEVEL_15,
  SL_I2S_FIFO_TRIGGER_LEVEL_16,
  SL_I2S_FIFO_TRIGGER_LEVEL_MAX
};

/// TDM slot length in SCLK cycles.
SL_ENUM(sl_i2s_tdm_slot_length_t) {
  SL_I2S_TDM_SLOT_LENGTH_12 = 0x0,
  SL_I2S_TDM_SLOT_LENGTH_16,
  SL_I2S_TDM_SLOT_LENGTH_20,
  SL_I2S_TDM_SLOT_LENGTH_24,
  SL_I2S_TDM_SLOT_LENGTH_32,
  SL_I2S_TDM_SLOT_LENGTH_MAX
};

// ----------------------------------------------------------------------------
// CAPABILITY LIMITS

#define SL_I2S_MAX_TDM_SLOTS              (16U)
#define SL_I2S_MAX_CHANNELS               ((uint32_t)SL_I2S_CHANNEL_ID_MAX)
#if defined(I2ST_PRESENT)
#define SL_I2ST_MAX_TDM_SLOTS             (8U)
#define SL_I2ST_MAX_CHANNELS              ((uint32_t)SL_I2S_CHANNEL_ID_MAX - 2U)
#endif

// ----------------------------------------------------------------------------
// VALIDATION MACROS

#define SL_I2S_OPERATING_MODE_IS_VALID(mode) \
  (((mode) == SL_I2S_OPERATING_MODE_TARGET) || ((mode) == SL_I2S_OPERATING_MODE_CONTROLLER))

#define SL_I2S_DATA_FORMAT_IS_VALID(format) \
  (((format) == SL_I2S_DATA_FORMAT_STANDARD) \
   || ((format) == SL_I2S_DATA_FORMAT_TDM) \
   || ((format) == SL_I2S_DATA_FORMAT_LJ) \
   || ((format) == SL_I2S_DATA_FORMAT_RJ))

#define SL_I2S_ENDIANNESS_IS_VALID(endianness) \
  (((endianness) == SL_I2S_ENDIANNESS_MSB_FIRST) \
   || ((endianness) == SL_I2S_ENDIANNESS_LSB_FIRST))

#define SL_I2S_DIRECTION_IS_VALID(dir) \
  (((dir) == SL_I2S_DIR_TX) || ((dir) == SL_I2S_DIR_RX) || ((dir) == SL_I2S_DIR_TX_RX))

/** @} (end addtogroup device_i2s) */

#ifdef __cplusplus
}
#endif

#endif /* SL_DEVICE_I2S_H */
