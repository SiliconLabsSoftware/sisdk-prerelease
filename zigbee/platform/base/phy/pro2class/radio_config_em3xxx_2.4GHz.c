/***************************************************************************//**
 * @file
 * @brief 2.4GHz radio configuration for em3xx and EFR32 (RAIL and legacy PHYs)
 * per ZigBee 05-3474-22 (R22) and IEEE 802.15.4-2003.
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
#if     (PHY_DUALRAIL && !PHY_THIS && !PHY_INCLUDE_SOURCE)

// If SLC builds this source file without being included, make it empty

#else//!(PHY_DUALRAIL && !PHY_THIS && !PHY_INCLUDE_SOURCE)

#ifndef SL_ZIGBEE_TEST
#define LEGACY_PHY_BUILD 1
#include "phy/hal.h"
#endif//SL_ZIGBEE_TEST
#include "core/sl_zigbee_stack.h"
#include "phy/phy.h"
#include "phy-params-2.4GHz.h" // Override any prior definitions with 2.4 GHz

#if     PHY_RAIL
 #include "rail_ieee802154.h"
WEAK(RAIL_Status_t halPluginConfig2p4GHzRadio(RAIL_Handle_t railHandle))
{
  return RAIL_IEEE802154_Config2p4GHzRadio(railHandle);
}
static const EmPhyRadioConfig radioConfig = {
  .radioConfig = &halPluginConfig2p4GHzRadio,
  .irCal = &RAIL_IEEE802154_CalibrateIr2p4Ghz,
};
 #define myRadioConfig (&radioConfig)
#else//!PHY_RAIL
 #define myRadioConfig NULL
#endif//PHY_RAIL

static const EmPhySubBandParams subBandParams = {
  /*lbtBackoffSym    */ SL_ZIGBEE_PHY_TURNAROUND_SYMBOLS + SL_ZIGBEE_PHY_CCA_SYMBOLS,
  /*lbtBackoffMinExp */ RADIO_BACKOFF_EXPONENT_MIN_DEFAULT,
  /*lbtBackoffMaxExp */ RADIO_BACKOFF_EXPONENT_MAX_DEFAULT,
  /*lbtBackoffAdjust */ 0,   // No adjustment, pure rand(2^BackoffExp - 1)
  /*lbtMinBackoffs   */ RADIO_MINIMUM_BACKOFF_DEFAULT,
  /*lbtTries         */ RADIO_CCA_ATTEMPT_MAX_DEFAULT,
  /*lbtTimeoutSym    */ 0,   // No overall LBT timeout
  /*lbtCcaSym        */ SL_ZIGBEE_PHY_CCA_SYMBOLS,
  /*dutyTxOffSym     */ 0,   // No post-transmit "dead" time
  /*dutyTxMaxPktByte */ 0,   // No per-transmit limit
  /*dutyTxMaxSeqByte */ 0,   // No sequence of transmits limit
  /*pad[]            */ { 0, },
  /*ackMinSym        */ SL_ZIGBEE_PHY_TURNAROUND_SYMBOLS,
  /*ackMaxSym        */ SL_ZIGBEE_PHY_ACK_TIMEOUT_SYMBOLS,
};

// Per TRD Table 5 Regulated Maximum Power Levels
// Put exceptions ahead of default configuration
static const EmPhySubBandConfig subBandConfigs[] = {
  { // From em250class/phy.h or em3xx/phy.h
    /*minPhyChannel  */ 11,   // SL_ZIGBEE_MIN_802_15_4_CHANNEL_NUMBER,
    /*maxPhyChannel  */ 26,   // SL_ZIGBEE_MAX_802_15_4_CHANNEL_NUMBER,
    /*txMinPowerdBm  */ MIN_RADIO_POWER,    // -43 dBm
    /*txMaxPowerdBm  */ MAX_RADIO_POWER,    //   3 dBm
    /*edThresholddBm*/ ED_CCA_THRESHOLD,    // -75 dBm
    /*lbtDutyDenom   */ 1,   // 100%
    /*criDutyDenom   */ 1,   // 100%
    /*nonDutyDenom10 */ 0,   // 100%
   #if     MAC_HAS_CHANNEL_PAGES
    /*macChanPage    */ 0,   // Channel Page 0
    /*macChanOffset  */ 11,
    /*pad[]          */ { 0, },
   #endif//MAC_HAS_CHANNEL_PAGES
    /*subBandParams  */ &subBandParams,
    /*ccaConfigArray */ NULL, // Use radioTransmitConfig{} CSMA
  },
};

// These are stored in RAM so app can change 'em.
const EmPhyBandConfig emPhyBandConfig2400MHz = {
  /*minPhyChan         */ 11,         // SL_ZIGBEE_MIN_802_15_4_CHANNEL_NUMBER,
  /*maxPhyChan         */ 26,         // SL_ZIGBEE_MAX_802_15_4_CHANNEL_NUMBER,
  /*pad[]              */ {       0, },
  /*bandName           */ (const uint8_t *)"2.4 GHz",
  /*baseFreqHz         */ 2405000000UL,
  /*chanFreqHz         */ 5000000,
  /*radioXoFreq        */ 0,
  /*dataRateBps        */ SL_ZIGBEE_PHY_DATA_RATE_BPS,
  /*symbolBits         */ SL_ZIGBEE_PHY_SYMBOL_BITS,
  /*preambleBits       */ SL_ZIGBEE_PHY_PREAMBLE_BITS,
  /*sfdBits            */ SL_ZIGBEE_PHY_SFD_BITS,
  /*numSubBandConfigs  */ sizeof(subBandConfigs) / sizeof(*subBandConfigs),
  /*subBandConfigs     */ subBandConfigs,
  /*radioConfigArray   */ myRadioConfig,
};

#endif//(PHY_DUALRAIL && !PHY_THIS && !PHY_INCLUDE_SOURCE)
