/***************************************************************************//**
 * @file
 * @brief Device Manager DMA (device independent enumerations)
 *******************************************************************************
 * # License
 * <b>Copyright 2025 Silicon Laboratories, Inc. www.silabs.com</b>
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
 ******************************************************************************/

#ifndef SL_DEVICE_DMA_INTERNAL_H
#define SL_DEVICE_DMA_INTERNAL_H

#include <stdint.h>
#include <stdbool.h>

#include "sl_enum.h"
#include "sl_device_dma.h"

#if defined(SL_COMPONENT_CATALOG_PRESENT)
#include "sl_component_catalog.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

/***************************************************************************//**
 * @addtogroup device_dma Device Manager DMA
 * @brief Portable (device independent) DMA configuration enums.
 * @details
 * ## Overview
 * This header provides abstract, device-agnostic enumerations for common
 * DMA configuration parameters. They intentionally avoid binding their
 * enumerator values to any specific device register encodings. A mapping
 * layer (typically within the HAL or driver implementation) is expected to
 * translate these portable values to device-specific register fields.
 *
 * The goal is to allow higher-level components, code generators and
 * configuration tooling to reference DMA attributes without pulling in
 * device headers or conditional compilation based on device series.
 *
 * The enums defined here SHOULD NOT be assumed to match any hardware
 * bit patterns. Only their relative meaning is guaranteed.
 *
 * @{
 ******************************************************************************/

/***************************************************************************//**
 * @name DMA Signal Defines
 * Those defines can be used as constant of type sl_dma_signal_t
 * The values of those defines are device specific.
 * @{
 ******************************************************************************/

/// Define for CAN0 DBG DMA Signal.
#define SL_DMA_SIGNAL_CAN0_DBG (&SL_DMA_SIGNAL_CAN0_DBG_VALUE)
/// Define for CAN0 RXFL0 DMA Signal.
#define SL_DMA_SIGNAL_CAN0_RXFL0 (&SL_DMA_SIGNAL_CAN0_RXFL0_VALUE)
/// Define for CAN0 RXFL1 DMA Signal.
#define SL_DMA_SIGNAL_CAN0_RXFL1 (&SL_DMA_SIGNAL_CAN0_RXFL1_VALUE)
/// Define for CAN0 TXFL0 DMA Signal.
#define SL_DMA_SIGNAL_CAN0_TXFL0 (&SL_DMA_SIGNAL_CAN0_TXFL0_VALUE)
/// Define for CAN0 TXFL1 DMA Signal.
#define SL_DMA_SIGNAL_CAN0_TXFL1 (&SL_DMA_SIGNAL_CAN0_TXFL1_VALUE)

/// Define for CAN1 DBG DMA Signal.
#define SL_DMA_SIGNAL_CAN1_DBG (&SL_DMA_SIGNAL_CAN1_DBG_VALUE)
/// Define for CAN1 RXFL0 DMA Signal.
#define SL_DMA_SIGNAL_CAN1_RXFL0 (&SL_DMA_SIGNAL_CAN1_RXFL0_VALUE)
/// Define for CAN1 RXFL1 DMA Signal.
#define SL_DMA_SIGNAL_CAN1_RXFL1 (&SL_DMA_SIGNAL_CAN1_RXFL1_VALUE)
/// Define for CAN1 TXFL0 DMA Signal.
#define SL_DMA_SIGNAL_CAN1_TXFL0 (&SL_DMA_SIGNAL_CAN1_TXFL0_VALUE)
/// Define for CAN1 TXFL1 DMA Signal.
#define SL_DMA_SIGNAL_CAN1_TXFL1 (&SL_DMA_SIGNAL_CAN1_TXFL1_VALUE)

/// Define for I3C0 RXFL DMA Signal.
#define SL_DMA_SIGNAL_I3C0_RXFL (&SL_DMA_SIGNAL_I3C0_RXFL_VALUE)
/// Define for I3C0 TXFL DMA Signal.
#define SL_DMA_SIGNAL_I3C0_TXFL (&SL_DMA_SIGNAL_I3C0_TXFL_VALUE)
/// Define for I3C0 TXFL0 DMA Signal.
#define SL_DMA_SIGNAL_I3C0_TXFL0 (&SL_DMA_SIGNAL_I3C0_TXFL0_VALUE)
/// Define for I3C0 TXFL DMA Signal.
#define SL_DMA_SIGNAL_I3C0_TXFL1 (&SL_DMA_SIGNAL_I3C0_TXFL1_VALUE)

/// Define for I2ST0 REQRX DMA Signal.
#define SL_DMA_SIGNAL_I2ST0_REQRX (&SL_DMA_SIGNAL_I2ST0_REQRX_VALUE)
/// Define for I2ST0 REQTX DMA Signal.
#define SL_DMA_SIGNAL_I2ST0_REQTX (&SL_DMA_SIGNAL_I2ST0_REQTX_VALUE)

/// Define for I2S0 REQRX DMA Signal.
#define SL_DMA_SIGNAL_I2S0_REQRX (&SL_DMA_SIGNAL_I2S0_REQRX_VALUE)
/// Define for I2S0 REQTX DMA Signal.
#define SL_DMA_SIGNAL_I2S0_REQTX (&SL_DMA_SIGNAL_I2S0_REQTX_VALUE)

/// Define for HSPI0 RXFL DMA Signal.
#define SL_DMA_SIGNAL_HSPI0_RXFL (&SL_DMA_SIGNAL_HSPI0_RXFL_VALUE)
/// Define for HSPI0 TXFL DMA Signal.
#define SL_DMA_SIGNAL_HSPI0_TXFL (&SL_DMA_SIGNAL_HSPI0_TXFL_VALUE)

/// Define for NPUCFG0 MVP DMA Signal.
#define SL_DMA_SIGNAL_NPUCFG0_MVP (&SL_DMA_SIGNAL_NPUCFG0_MVP_VALUE)

/// Define for CSEN0 CSENSCAN DMA Signal.
#define SL_DMA_SIGNAL_CSEN0_CSENSCAN (&SL_DMA_SIGNAL_CSEN0_CSENSCAN_VALUE)

/// Define for AGC RSSI DMA Signal.
#define SL_DMA_SIGNAL_AGC_RSSI (&SL_DMA_SIGNAL_AGC_RSSI_VALUE)

/// Define for MODEM DEBUG DMA Signal.
#define SL_DMA_SIGNAL_MODEM_DEBUG (&SL_DMA_SIGNAL_MODEM_DEBUG_VALUE)

/// @} (end dma_signal_defines)

// ----------------------------------------------------------------------------
// EXTERNS

/// @cond DO_NOT_INCLUDE_WITH_DOXYGEN

// External declaration for AGC RSSI DMA Signal
extern const uint32_t SL_DMA_SIGNAL_AGC_RSSI_VALUE;

// External declaration for MODEM DEBUG DMA Signal
extern const uint32_t SL_DMA_SIGNAL_MODEM_DEBUG_VALUE;

// External declaration for CAN0 DBG DMA Signal
extern const uint32_t SL_DMA_SIGNAL_CAN0_DBG_VALUE;
// External declaration for CAN0 RXFL0 DMA Signal
extern const uint32_t SL_DMA_SIGNAL_CAN0_RXFL0_VALUE;
// External declaration for CAN0 RXFL1 DMA Signal
extern const uint32_t SL_DMA_SIGNAL_CAN0_RXFL1_VALUE;
// External declaration for CAN0 TXFL0 DMA Signal
extern const uint32_t SL_DMA_SIGNAL_CAN0_TXFL0_VALUE;
// External declaration for CAN0 TXFL1 DMA Signal
extern const uint32_t SL_DMA_SIGNAL_CAN0_TXFL1_VALUE;

// External declaration for CAN1 DBG DMA Signal
extern const uint32_t SL_DMA_SIGNAL_CAN1_DBG_VALUE;
// External declaration for CAN1 RXFL0 DMA Signal
extern const uint32_t SL_DMA_SIGNAL_CAN1_RXFL0_VALUE;
// External declaration for CAN1 RXFL1 DMA Signal
extern const uint32_t SL_DMA_SIGNAL_CAN1_RXFL1_VALUE;
// External declaration for CAN1 TXFL0 DMA Signal
extern const uint32_t SL_DMA_SIGNAL_CAN1_TXFL0_VALUE;
// External declaration for CAN1 TXFL1 DMA Signal
extern const uint32_t SL_DMA_SIGNAL_CAN1_TXFL1_VALUE;

// External declaration for I3C0 RXFL DMA Signal
extern const uint32_t SL_DMA_SIGNAL_I3C0_RXFL_VALUE;
// External declaration for I3C0 TXFL DMA Signal
extern const uint32_t SL_DMA_SIGNAL_I3C0_TXFL_VALUE;
// External declaration for I3C0 TXFL0 DMA Signal
extern const uint32_t SL_DMA_SIGNAL_I3C0_TXFL0_VALUE;
// External declaration for I3C0 TXFL1 DMA Signal
extern const uint32_t SL_DMA_SIGNAL_I3C0_TXFL1_VALUE;

// External declaration for I2ST0 REQRX DMA Signal
extern const uint32_t SL_DMA_SIGNAL_I2ST0_REQRX_VALUE;
// External declaration for I2ST0 REQTX DMA Signal
extern const uint32_t SL_DMA_SIGNAL_I2ST0_REQTX_VALUE;

// External declaration for I2S0 REQRX DMA Signal
extern const uint32_t SL_DMA_SIGNAL_I2S0_REQRX_VALUE;
// External declaration for I2S0 REQTX DMA Signal
extern const uint32_t SL_DMA_SIGNAL_I2S0_REQTX_VALUE;

// External declaration for HSPI0 RXFL DMA Signal
extern const uint32_t SL_DMA_SIGNAL_HSPI0_RXFL_VALUE;
// External declaration for HSPI0 TXFL DMA Signal
extern const uint32_t SL_DMA_SIGNAL_HSPI0_TXFL_VALUE;

// External declaration for NPUCFG0 MVP DMA Signal
extern const uint32_t SL_DMA_SIGNAL_NPUCFG0_MVP_VALUE;

// External declaration for CSEN0 CSENSCAN DMA Signal
extern const uint32_t SL_DMA_SIGNAL_CSEN0_CSENSCAN_VALUE;

/// @endcond

/** @} (end addtogroup device_dma) */

#ifdef __cplusplus
}
#endif

#endif // SL_DEVICE_DMA_INTERNAL_H
