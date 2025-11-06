/**************************************************************************//**
 * @file
 * @brief Device Manager DMA Definition
 ******************************************************************************
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
 *****************************************************************************/

#include "em_device.h"
#include "sl_device_dma_internal.h"

/***************************************************************************//**
 * @addtogroup device_dma Device Manager DMA
 * @{
 ******************************************************************************/

#if defined(LDMAXBAR0_CH_REQSEL_SIGSEL_CAN0DBG)
const uint32_t SL_DMA_SIGNAL_CAN0_DBG_VALUE = LDMAXBAR0_CH_REQSEL_SIGSEL_CAN0DBG | LDMAXBAR0_CH_REQSEL_SOURCESEL_CAN0;
#endif
#if defined(LDMAXBAR0_CH_REQSEL_SIGSEL_CAN0RXFL_0)
const uint32_t SL_DMA_SIGNAL_CAN0_RXFL0_VALUE = LDMAXBAR0_CH_REQSEL_SIGSEL_CAN0RXFL_0 | LDMAXBAR0_CH_REQSEL_SOURCESEL_CAN0;
#endif
#if defined(LDMAXBAR0_CH_REQSEL_SIGSEL_CAN0RXFL_1)
const uint32_t SL_DMA_SIGNAL_CAN0_RXFL1_VALUE = LDMAXBAR0_CH_REQSEL_SIGSEL_CAN0RXFL_1 | LDMAXBAR0_CH_REQSEL_SOURCESEL_CAN0;
#endif
#if defined(LDMAXBAR0_CH_REQSEL_SIGSEL_CAN0TXFL_0)
const uint32_t SL_DMA_SIGNAL_CAN0_TXFL0_VALUE = LDMAXBAR0_CH_REQSEL_SIGSEL_CAN0TXFL_0 | LDMAXBAR0_CH_REQSEL_SOURCESEL_CAN0;
#endif
#if defined(LDMAXBAR0_CH_REQSEL_SIGSEL_CAN0TXFL_1)
const uint32_t SL_DMA_SIGNAL_CAN0_TXFL1_VALUE = LDMAXBAR0_CH_REQSEL_SIGSEL_CAN0TXFL_1 | LDMAXBAR0_CH_REQSEL_SOURCESEL_CAN0;
#endif

#if defined(LDMAXBAR0_CH_REQSEL_SIGSEL_CAN1DBG)
const uint32_t SL_DMA_SIGNAL_CAN1_DBG_VALUE = LDMAXBAR0_CH_REQSEL_SIGSEL_CAN1DBG | LDMAXBAR0_CH_REQSEL_SOURCESEL_CAN1;
#endif
#if defined(LDMAXBAR0_CH_REQSEL_SIGSEL_CAN1RXFL_0)
const uint32_t SL_DMA_SIGNAL_CAN1_RXFL0_VALUE = LDMAXBAR0_CH_REQSEL_SIGSEL_CAN1RXFL_0 | LDMAXBAR0_CH_REQSEL_SOURCESEL_CAN1;
#endif
#if defined(LDMAXBAR0_CH_REQSEL_SIGSEL_CAN1RXFL_1)
const uint32_t SL_DMA_SIGNAL_CAN1_RXFL1_VALUE = LDMAXBAR0_CH_REQSEL_SIGSEL_CAN1RXFL_1 | LDMAXBAR0_CH_REQSEL_SOURCESEL_CAN1;
#endif
#if defined(LDMAXBAR0_CH_REQSEL_SIGSEL_CAN1TXFL_0)
const uint32_t SL_DMA_SIGNAL_CAN1_TXFL0_VALUE = LDMAXBAR0_CH_REQSEL_SIGSEL_CAN1TXFL_0 | LDMAXBAR0_CH_REQSEL_SOURCESEL_CAN1;
#endif
#if defined(LDMAXBAR0_CH_REQSEL_SIGSEL_CAN1TXFL_1)
const uint32_t SL_DMA_SIGNAL_CAN1_TXFL1_VALUE = LDMAXBAR0_CH_REQSEL_SIGSEL_CAN1TXFL_1 | LDMAXBAR0_CH_REQSEL_SOURCESEL_CAN1;
#endif

#if defined(LDMAXBAR0_CH_REQSEL_SIGSEL_I3C0REQ_RXFL)
const uint32_t SL_DMA_SIGNAL_I3C0_RXFL_VALUE = LDMAXBAR0_CH_REQSEL_SIGSEL_I3C0REQ_RXFL | LDMAXBAR0_CH_REQSEL_SOURCESEL_I3C0;
#endif
#if defined(LDMAXBAR0_CH_REQSEL_SIGSEL_I3C0REQ_TXFL)
const uint32_t SL_DMA_SIGNAL_I3C0_TXFL_VALUE = LDMAXBAR0_CH_REQSEL_SIGSEL_I3C0REQ_TXFL | LDMAXBAR0_CH_REQSEL_SOURCESEL_I3C0;
#endif
#if defined(LDMAXBAR0_CH_REQSEL_SIGSEL_I3C0REQ_TXFL_0)
const uint32_t SL_DMA_SIGNAL_I3C0_TXFL0_VALUE = LDMAXBAR0_CH_REQSEL_SIGSEL_I3C0REQ_TXFL_0 | LDMAXBAR0_CH_REQSEL_SOURCESEL_I3C0;
#endif
#if defined(LDMAXBAR0_CH_REQSEL_SIGSEL_I3C0REQ_TXFL_1)
const uint32_t SL_DMA_SIGNAL_I3C0_TXFL1_VALUE = LDMAXBAR0_CH_REQSEL_SIGSEL_I3C0REQ_TXFL_1 | LDMAXBAR0_CH_REQSEL_SOURCESEL_I3C0;
#endif

#if defined(LDMAXBAR0_CH_REQSEL_SIGSEL_I2ST0REQ_RX)
const uint32_t SL_DMA_SIGNAL_I2ST0_REQRX_VALUE = LDMAXBAR0_CH_REQSEL_SIGSEL_I2ST0REQ_RX | LDMAXBAR0_CH_REQSEL_SOURCESEL_I2ST0;
#endif
#if defined(LDMAXBAR0_CH_REQSEL_SIGSEL_I2ST0REQ_TX)
const uint32_t SL_DMA_SIGNAL_I2ST0_REQTX_VALUE = LDMAXBAR0_CH_REQSEL_SIGSEL_I2ST0REQ_TX | LDMAXBAR0_CH_REQSEL_SOURCESEL_I2ST0;
#endif

#if defined(LDMAXBAR0_CH_REQSEL_SIGSEL_I2S0REQ_RX)
const uint32_t SL_DMA_SIGNAL_I2S0_REQRX_VALUE = LDMAXBAR0_CH_REQSEL_SIGSEL_I2S0REQ_RX | LDMAXBAR0_CH_REQSEL_SOURCESEL_I2S0;
#endif
#if defined(LDMAXBAR0_CH_REQSEL_SIGSEL_I2S0REQ_TX)
const uint32_t SL_DMA_SIGNAL_I2S0_REQTX_VALUE = LDMAXBAR0_CH_REQSEL_SIGSEL_I2S0REQ_TX | LDMAXBAR0_CH_REQSEL_SOURCESEL_I2S0;
#endif

#if defined(LDMAXBAR0_CH_REQSEL_SIGSEL_HSPI0RXFL)
const uint32_t SL_DMA_SIGNAL_HSPI0_RXFL_VALUE = LDMAXBAR0_CH_REQSEL_SIGSEL_HSPI0RXFL | LDMAXBAR0_CH_REQSEL_SOURCESEL_HSPI0;
#endif
#if defined(LDMAXBAR0_CH_REQSEL_SIGSEL_HSPI0TXFL)
const uint32_t SL_DMA_SIGNAL_HSPI0_TXFL_VALUE = LDMAXBAR0_CH_REQSEL_SIGSEL_HSPI0TXFL | LDMAXBAR0_CH_REQSEL_SOURCESEL_HSPI0;
#endif

#if defined(LDMAXBAR0_CH_REQSEL_SIGSEL_AGCRSSI)
const uint32_t SL_DMA_SIGNAL_AGC_RSSI_VALUE = LDMAXBAR0_CH_REQSEL_SIGSEL_AGCRSSI | LDMAXBAR0_CH_REQSEL_SOURCESEL_AGC;
#endif

#if defined(LDMAXBAR0_CH_REQSEL_SIGSEL_MODEMDEBUG)
const uint32_t SL_DMA_SIGNAL_MODEM_DEBUG_VALUE = LDMAXBAR0_CH_REQSEL_SIGSEL_MODEMDEBUG | LDMAXBAR0_CH_REQSEL_SOURCESEL_MODEM;
#endif

#if defined(LDMAXBAR0_CH_REQSEL_SIGSEL_NPUCFG0MVP)
const uint32_t SL_DMA_SIGNAL_NPUCFG0_MVP_VALUE = LDMAXBAR0_CH_REQSEL_SIGSEL_NPUCFG0MVP | LDMAXBAR0_CH_REQSEL_SOURCESEL_NPUCFG0;
#endif

#if defined(LDMAXBAR0_CH_REQSEL_SIGSEL_CSEN0CSEN_SCAN)
const uint32_t SL_DMA_SIGNAL_CSEN0_CSENSCAN_VALUE = LDMAXBAR0_CH_REQSEL_SIGSEL_CSEN0CSEN_SCAN | LDMAXBAR0_CH_REQSEL_SOURCESEL_CSEN0;
#endif

/** @} (end addtogroup device_dma) */
