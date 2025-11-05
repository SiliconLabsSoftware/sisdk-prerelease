/***************************************************************************//**
 * @file
 * @brief SL_ZIGBEE_PHY SubGHz parameters on PRO2+ and EFR32 (RAIL and legacy PHYs)
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
#ifndef __PHY_PARAMS_868MHZ_H__
#define __PHY_PARAMS_868MHZ_H__

#ifndef SL_ZIGBEE_PHY_USE_PHR_154G
  #ifdef  PHY_TRANSCEIVER_SIM
    #define SL_ZIGBEE_PHY_USE_PHR_154G 1
  #else
    #define SL_ZIGBEE_PHY_USE_PHR_154G (PHY_PRO2PLUS)
  #endif
#endif//SL_ZIGBEE_PHY_USE_PHR_154G
#ifndef SL_ZIGBEE_PHY_DATA_WHITENING
#define SL_ZIGBEE_PHY_DATA_WHITENING SL_ZIGBEE_PHY_USE_PHR_154G
#endif//SL_ZIGBEE_PHY_DATA_WHITENING

// Basic timing parameters for 802.15.4 MR-FSK 868 MHz
#undef  SL_ZIGBEE_PHY_MAX_PHR_BYTES
#if     SL_ZIGBEE_PHY_USE_PHR_154G
#define SL_ZIGBEE_PHY_MAX_PHR_BYTES      2        // 802.15.4g-2012
#else//!SL_ZIGBEE_PHY_USE_PHR_154G
#define SL_ZIGBEE_PHY_MAX_PHR_BYTES      1        // 802.15.4-2003/2006/2011
#endif//SL_ZIGBEE_PHY_USE_PHR_154G
#undef  SL_ZIGBEE_PHY_MAX_PAYLOAD_BYTES
#define SL_ZIGBEE_PHY_MAX_PAYLOAD_BYTES  PHY_MAX_SIZE // What upper layers support
#undef  SL_ZIGBEE_PHY_MAX_CRC_BYTES
#define SL_ZIGBEE_PHY_MAX_CRC_BYTES      2        // 802.15.4-2003/2006/2011
#undef  SL_ZIGBEE_PHY_DATA_RATE_BPS
#define SL_ZIGBEE_PHY_DATA_RATE_BPS      (EM_PHY_CURRENT_BAND_CONFIG->dataRateBps)
#undef  SL_ZIGBEE_PHY_BIT_TIME_US
#define SL_ZIGBEE_PHY_BIT_TIME_US        (1000000ul / SL_ZIGBEE_PHY_DATA_RATE_BPS)
#undef  SL_ZIGBEE_PHY_SYMBOL_BITS
#define SL_ZIGBEE_PHY_SYMBOL_BITS        (EM_PHY_CURRENT_BAND_CONFIG->symbolBits)
#undef  SL_ZIGBEE_PHY_PREAMBLE_BITS
#define SL_ZIGBEE_PHY_PREAMBLE_BITS      (EM_PHY_CURRENT_BAND_CONFIG->preambleBits)
#undef  SL_ZIGBEE_PHY_SFD_BITS
#define SL_ZIGBEE_PHY_SFD_BITS           (EM_PHY_CURRENT_BAND_CONFIG->sfdBits)
#undef  SL_ZIGBEE_PHY_BYTE_BITS
#define SL_ZIGBEE_PHY_BYTE_BITS          8        // BYTE_TIME_US       =   80 us
#undef  SL_ZIGBEE_PHY_RAMP_TIME_BITS
#define SL_ZIGBEE_PHY_RAMP_TIME_BITS     1        // Ramp up or down time, ~10us
#undef  SL_ZIGBEE_PHY_RAMP_SETTLE_BITS
#define SL_ZIGBEE_PHY_RAMP_SETTLE_BITS   4        // Settle after ramp up, ~40us

#undef  SL_ZIGBEE_PHY_TURNAROUND_SYMBOLS
#if     SL_ZIGBEE_PHY_USE_PHR_154G
// 802.15.4g-2012 requires that aTurnaroundTime, macAckWaitDuration, and
// aCCATime be calculated using the slowest operating mode's symbol time,
// which we call SYMBOL_DURATION to distinguish it from actual SYMBOL_TIME.
// Confusing? Yes, but reading 802.15.4g-2012 will confuse you moreso!
// (equated implicitly by upper phy.h by not specifying it here).
//#define SL_ZIGBEE_PHY_SYMBOL_DURATION_US 20     // Must use slower 50 kbps rate
//#define SL_ZIGBEE_PHY_TURNAROUND_SYMBOLS (1000 / SL_ZIGBEE_PHY_SYMBOL_DURATION_US) // 100
//                                            // TURNAROUND_TIME_US = 1000 us
// ZigBee GB868 ignores 15.4G and R22 specifies turnaround time of 50 symbols.
#define SL_ZIGBEE_PHY_TURNAROUND_SYMBOLS 50       // TURNAROUND_TIME_US =  500 us
#else//!SL_ZIGBEE_PHY_USE_PHR_154G
#define SL_ZIGBEE_PHY_TURNAROUND_SYMBOLS 12       // TURNAROUND_TIME_US =  120 us
#endif//SL_ZIGBEE_PHY_USE_PHR_154G
#undef  SL_ZIGBEE_PHY_CCA_SYMBOLS
#define SL_ZIGBEE_PHY_CCA_SYMBOLS        8        // CCA_TIME_US        =   80 us
//*Override these calculated ones in outer phy.h*
#undef  SL_ZIGBEE_PHY_ACK_TIMEOUT_SYMBOLS
// ZigBee GB868 R22 specifies 100 symbols
//#define SL_ZIGBEE_PHY_ACK_TIMEOUT_SYMBOLS 100     //*ACK_TIMEOUT_US     = 1000 us
//TODO: Fix this per spec someday; for now be rather permissive and forgiving
#define SL_ZIGBEE_PHY_ACK_TIMEOUT_SYMBOLS 255     //*ACK_TIMEOUT_US     = 2550 us
#undef  SL_ZIGBEE_PHY_MAX_FRAME_RESPONSE_MULTIPLIER
#define SL_ZIGBEE_PHY_MAX_FRAME_RESPONSE_MULTIPLIER 10  //TODO: Tune this; default is 1

#endif//__PHY_PARAMS_868MHZ_H__
