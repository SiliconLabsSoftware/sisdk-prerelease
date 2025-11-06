/**************************************************************************//**
 * @file
 * @brief Device Manager DMA API Definition
 ******************************************************************************
 * # License
 * <b>Copyright 2024 Silicon Laboratories, Inc. www.silabs.com</b>
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

#include "sl_device_dma.h"
#include "cmsis_compiler.h"

/***************************************************************************//**
 * @addtogroup device_dma Device Manager DMA
 * @{
 ******************************************************************************/

// Weak declaration for AGC RSSI DMA Signal
__WEAK const uint32_t SL_DMA_SIGNAL_AGC_RSSI_VALUE = 0xFFFFFFFF;

// Weak declaration for MODEM DEBUG DMA Signal
__WEAK const uint32_t SL_DMA_SIGNAL_MODEM_DEBUG_VALUE = 0xFFFFFFFF;

// Weak declaration for CAN0 DBG DMA Signal
__WEAK const uint32_t SL_DMA_SIGNAL_CAN0_DBG_VALUE = 0xFFFFFFFF;
// Weak declaration for CAN0 RXFL0 DMA Signal
__WEAK const uint32_t SL_DMA_SIGNAL_CAN0_RXFL0_VALUE = 0xFFFFFFFF;
// Weak declaration for CAN0 RXFL1 DMA Signal
__WEAK const uint32_t SL_DMA_SIGNAL_CAN0_RXFL1_VALUE = 0xFFFFFFFF;
// Weak declaration for CAN0 TXFL0 DMA Signal
__WEAK const uint32_t SL_DMA_SIGNAL_CAN0_TXFL0_VALUE = 0xFFFFFFFF;
// Weak declaration for CAN0 TXFL1 DMA Signal
__WEAK const uint32_t SL_DMA_SIGNAL_CAN0_TXFL1_VALUE = 0xFFFFFFFF;

// Weak declaration for CAN1 DBG DMA Signal
__WEAK const uint32_t SL_DMA_SIGNAL_CAN1_DBG_VALUE = 0xFFFFFFFF;
// Weak declaration for CAN1 RXFL0 DMA Signal
__WEAK const uint32_t SL_DMA_SIGNAL_CAN1_RXFL0_VALUE = 0xFFFFFFFF;
// Weak declaration for CAN1 RXFL1 DMA Signal
__WEAK const uint32_t SL_DMA_SIGNAL_CAN1_RXFL1_VALUE = 0xFFFFFFFF;
// Weak declaration for CAN1 TXFL0 DMA Signal
__WEAK const uint32_t SL_DMA_SIGNAL_CAN1_TXFL0_VALUE = 0xFFFFFFFF;
// Weak declaration for CAN1 TXFL1 DMA Signal
__WEAK const uint32_t SL_DMA_SIGNAL_CAN1_TXFL1_VALUE = 0xFFFFFFFF;

// Weak declaration for I3C0 RXFL DMA Signal
__WEAK const uint32_t SL_DMA_SIGNAL_I3C0_RXFL_VALUE = 0xFFFFFFFF;
// Weak declaration for I3C0 TXFL DMA Signal
__WEAK const uint32_t SL_DMA_SIGNAL_I3C0_TXFL_VALUE = 0xFFFFFFFF;
// Weak declaration for I3C0 TXFL0 DMA Signal
__WEAK const uint32_t SL_DMA_SIGNAL_I3C0_TXFL0_VALUE = 0xFFFFFFFF;
// Weak declaration for I3C0 TXFL1 DMA Signal
__WEAK const uint32_t SL_DMA_SIGNAL_I3C0_TXFL1_VALUE = 0xFFFFFFFF;

// Weak declaration for I2ST0 REQRX DMA Signal
__WEAK const uint32_t SL_DMA_SIGNAL_I2ST0_REQRX_VALUE = 0xFFFFFFFF;
// Weak declaration for I2ST0 REQTX DMA Signal
__WEAK const uint32_t SL_DMA_SIGNAL_I2ST0_REQTX_VALUE = 0xFFFFFFFF;

// Weak declaration for I2S0 REQRX DMA Signal
__WEAK const uint32_t SL_DMA_SIGNAL_I2S0_REQRX_VALUE = 0xFFFFFFFF;
// Weak declaration for I2S0 REQTX DMA Signal
__WEAK const uint32_t SL_DMA_SIGNAL_I2S0_REQTX_VALUE = 0xFFFFFFFF;

// Weak declaration for HSPI0 RXFL DMA Signal
__WEAK const uint32_t SL_DMA_SIGNAL_HSPI0_RXFL_VALUE = 0xFFFFFFFF;
// Weak declaration for HSPI0 TXFL DMA Signal
__WEAK const uint32_t SL_DMA_SIGNAL_HSPI0_TXFL_VALUE = 0xFFFFFFFF;

// Weak declaration for NPUCFG0 MVP DMA Signal
__WEAK const uint32_t SL_DMA_SIGNAL_NPUCFG0_MVP_VALUE = 0xFFFFFFFF;

// Weak declaration for CSEN0 CSENSCAN DMA Signal
__WEAK const uint32_t SL_DMA_SIGNAL_CSEN0_CSENSCAN_VALUE = 0xFFFFFFFF;

/** @} (end addtogroup device_dma) */
