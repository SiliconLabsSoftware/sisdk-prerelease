/***************************************************************************//**
 * @file
 * @brief SL_ZIGBEE_PHY 2.4GHz parameters on em3xx and EFR32 (RAIL and legacy PHYs)
 *******************************************************************************
 * # License
 * <b>Copyright 2018 Silicon Laboratories Inc. www.silabs.com</b>
 *******************************************************************************
 *
 * The licensor of this software is Silicon Laboratories Inc. Your use of this
 * software is governed by the terms of Silicon Labs Master Software License
 * Agreement (MSLA) available at
 * www.silabs.com/about-us/legal/master-software-license-agreement. This
 * software is distributed to you in Source Code format and is governed by the
 * sections of the MSLA applicable to Source Code.
 *
 ******************************************************************************/
#ifndef __PHY_PARAMS_2400MHZ_H__
#define __PHY_PARAMS_2400MHZ_H__

// Basic timing parameters for 802.15.4 OQPSK 2400 MHz
#undef  SL_ZIGBEE_PHY_MAX_PHR_BYTES
#define SL_ZIGBEE_PHY_MAX_PHR_BYTES      1        // 802.15.4-2003/2006/2011
#undef  SL_ZIGBEE_PHY_MAX_PAYLOAD_BYTES
#define SL_ZIGBEE_PHY_MAX_PAYLOAD_BYTES  127      // 802.15.4-2003/2006/2011
#undef  SL_ZIGBEE_PHY_MAX_CRC_BYTES
#define SL_ZIGBEE_PHY_MAX_CRC_BYTES      2        // 802.15.4-2003/2006/2011
#undef  SL_ZIGBEE_PHY_DATA_RATE_BPS
#define SL_ZIGBEE_PHY_DATA_RATE_BPS      250000UL // 250 kbps
#undef  SL_ZIGBEE_PHY_BIT_TIME_US
#define SL_ZIGBEE_PHY_BIT_TIME_US        4        // 1000000 us/sec / DATA_RATE_BPS
#undef  SL_ZIGBEE_PHY_SYMBOL_BITS
#define SL_ZIGBEE_PHY_SYMBOL_BITS        4        // SYMBOL_TIME_US     =   16 us
#undef  SL_ZIGBEE_PHY_PREAMBLE_BITS
#define SL_ZIGBEE_PHY_PREAMBLE_BITS      32       // PREAMBLE_TIME_US   =  320 us
#undef  SL_ZIGBEE_PHY_SFD_BITS
#define SL_ZIGBEE_PHY_SFD_BITS           8        // SFD_TIME_US        =   32 us
#undef  SL_ZIGBEE_PHY_BYTE_BITS
#define SL_ZIGBEE_PHY_BYTE_BITS          8        // BYTE_TIME_US       =   32 us
#undef  SL_ZIGBEE_PHY_TURNAROUND_SYMBOLS
#define SL_ZIGBEE_PHY_TURNAROUND_SYMBOLS 12       // TURNAROUND_TIME_US =  192 us
#undef  SL_ZIGBEE_PHY_CCA_SYMBOLS
#define SL_ZIGBEE_PHY_CCA_SYMBOLS        8        // CCA_TIME_US        =  128 us
//*Override these calculated ones in outer phy.h*
#undef  SL_ZIGBEE_PHY_ACK_TIMEOUT_SYMBOLS
#ifdef  MAC_ACK_TO_RESET
#define SL_ZIGBEE_PHY_ACK_TIMEOUT_SYMBOLS (MAC_ACK_TO_RESET / SL_ZIGBEE_PHY_SYMBOL_TIME_US)
// 802.15.4 says ACK timeout should be 54 symbols, but we override that
// with this which evaluates to 48 symbols (we currently don't override
// MAC_ACK_TO_RESET) -- see bug EMSTACK-867.
#else//!MAC_ACK_TO_RESET
#define SL_ZIGBEE_PHY_ACK_TIMEOUT_SYMBOLS 54  // Per 15.4-2003 macAckWaitDuration
#endif//MAC_ACK_TO_RESET
#undef  SL_ZIGBEE_PHY_MAX_FRAME_RESPONSE_MULTIPLIER
#define SL_ZIGBEE_PHY_MAX_FRAME_RESPONSE_MULTIPLIER 1

#undef  ED_CCA_THRESHOLD
#define ED_CCA_THRESHOLD  -75 // dBm

#endif//__PHY_PARAMS_2400MHZ_H__
