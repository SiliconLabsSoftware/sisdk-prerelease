/***************************************************************************//**
 * @file
 * @brief 863-876 MHz radio config for PRO2+ and EFR32 (RAIL and legacy PHYs)
 *        per ZigBee 05-3474-22 (R22) and IEEE 802.15.4-2015.
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
#if     (defined(PHY_RAIL) || defined(PHY_SIMULATION_GB))
 #define RADIO_CONFIGURATION_DATA_CHANNEL_NUMBER 0
 #define RADIO_CONFIGURATION_DATA_RADIO_XO_FREQ 38400000UL
#if     PHY_RAIL
 #include "rail_ieee802154.h"
static const EmPhyRadioConfig radioConfig = {
  .radioConfig = &RAIL_IEEE802154_ConfigGB863MHzRadio,
  .irCal = &RAIL_IEEE802154_CalibrateIrSubGhz,
};
 #define myRadioConfig (&radioConfig)
#else//!PHY_RAIL
 #define myRadioConfig NULL
#endif//PHY_RAIL
#elif   ((defined(CORTEXM3_EFR32_MICRO) && !defined(PRO2_30_0_MHZ)) \
  || defined(PHY_TRANSCEIVER_SIM))        // use 30.4MHz configs
 #include "em317/radio_config_Si4468_863MHz.h"
#elif   defined(CORTEXM3_EFR32_MICRO)     // use 30.0MHz configs
 #include "em3xx/radio_config_Si4468_863MHz.h"
#else//!defined(CORTEXM3_EFR32_MICRO)
 #error "Unsupported Radio Configuration"
#endif//(defined(PHY_RAIL) || defined(PHY_SIMULATION_GB))

// From the WDS3-generated configuration file, we only really care about
// certain RF_GLOBAL, RF_PREAMBLE, RF_SYNC, RF_MODEM, RF_SYNTH and RF_FREQ
// properties.
// The rest we handle ourselves internally or are not applicable.
#undef  RADIO_CONFIGURATION_DATA_ARRAY
#define RADIO_CONFIGURATION_DATA_ARRAY {      \
    RF_GLOBAL_XO_TUNE_2,                      \
    RF_GLOBAL_CONFIG_1,                       \
    RF_PREAMBLE_TX_LENGTH_9,                  \
    RF_SYNC_CONFIG_6,                         \
    RF_MODEM_MOD_TYPE_12,                     \
    RF_MODEM_FREQ_DEV_0_1,                    \
    RF_MODEM_TX_FILTER_COEFF_8_12,            \
    RF_MODEM_IF_FREQ_2_6,                     \
    RF_MODEM_BCR_OSR_1_9,                     \
    RF_MODEM_AFC_GEAR_7,                      \
    RF_MODEM_AGC_CONTROL_1,                   \
    RF_MODEM_AGC_WINDOW_SIZE_12,              \
    RF_MODEM_RAW_CONTROL_7,                   \
    /*RF_MODEM_RSSI_JUMP_THRESH_2 redundant*/ \
    RF_MODEM_RSSI_CONTROL_3,                  \
    RF_MODEM_RAW_SEARCH2_2,                   \
    RF_MODEM_SPIKE_DET_2,                     \
    RF_MODEM_RSSI_MUTE_1,                     \
    RF_MODEM_DSA_CTRL1_5,                     \
    RF_MODEM_CHFLT_RX1_CHFLT_COE13_7_0_12,    \
    RF_MODEM_CHFLT_RX1_CHFLT_COE1_7_0_12,     \
    RF_MODEM_CHFLT_RX2_CHFLT_COE7_7_0_12,     \
    RF_PA_MODE_4,                             \
    RF_SYNTH_PFDCP_CPFF_7,                    \
    RF_FREQ_CONTROL_INTE_8,                   \
    0x00                                      \
}

#if     ((RADIO_CONFIGURATION_DATA_CHANNEL_NUMBER < 0) \
  || ((RADIO_CONFIGURATION_DATA_CHANNEL_NUMBER == 0)   \
  && !(defined(PHY_RAIL) || defined(PHY_SIMULATION_GB))))
  #error WDS3 Channel Number must be set to the PHY base channel in the band
#endif//( (RADIO_CONFIGURATION_DATA_CHANNEL_NUMBER < 0) ... )

#ifndef myRadioConfig
static const uint8_t myRadioConfig[] = RADIO_CONFIGURATION_DATA_ARRAY;
#endif//myRadioConfig

// Per ZigBee docs-14-0382-23-pnrg-gb-868-phy-mac-tech-spec-draft.docx
// for GB868 UK market.
static const EmPhySubBandParams gb868Sb1Params = { // Channel Page 28,29,30(UK)
  /*lbtBackoffSym    */ 50,    // aLBTGranularity      =  500us
  /*lbtBackoffMinExp */ 3,     // aLBTMin/MaxRandom    =    7   (2^3 - 1)
  /*lbtBackoffMaxExp */ 3,     // aLBTMin/MaxRandom    =    7   (2^3 - 1)
  /*lbtBackoffAdjust */ 3,     // aLBTMin/MaxRandom    +3= 10   (rand(0..10))
  /*lbtMinBackoffs   */ 0,     // aLBTMinRandom        +0= 10   (rand(0..10))
  /*lbtTries         */ 4,     // aLBTMaxTxRetries+1   = 3(+1), 1st w/ backoff
  /*lbtTimeoutSym    */ 6000,  // aLBTTimeout          =   60ms
  /*lbtCcaSym        */ 500,   // aLBTMinFree          =    5ms
  /*dutyTxOffSym     */ 10000, // aLBTTxMinOff         =  100ms
  /*dutyTxMaxPktByte */ 12500, // aLBTTxMaxPKT         =    1s
  /*dutyTxMaxSeqByte */ 50000, // aLBTMaxDlg           =    4s
  /*pad[]            */ {   0, },
  /*ackMinSym        */ 45,    // aLBTAckWindowStart   =  450us >= aLBTRxTxTurnAround
  /*ackMaxSym        */ 100 + 136,// aLBTAckWindow+ACKtime= 1000us+(8+2+2+5)bytes
};

#if     USE_ETSI_EN303_204_IN_CP30_31
// Per ZigBee docs-14-0382-23-pnrg-gb-868-phy-mac-tech-spec-draft.docx
// for EU (non-UK) market, adapting ETSI 303 204.1 Polite Spectrum Access
static const EmPhySubBandParams gb868Sb2Params = { // Channel Page 30(EU)
  /*lbtBackoffSym    */ 1,     // aLBTGranularity      =   10us
  /*lbtBackoffMinExp */ 9,     // aLBTMin/MaxRandom    =  511   (2^9 - 1)
  /*lbtBackoffMaxExp */ 9,     // aLBTMin/MaxRandom    =  511   (2^9 - 1)
  /*lbtBackoffAdjust */ -27,   // aLBTMin/MaxRandom   -27=484   (rand(0..484))
  /*lbtMinBackoffs   */ 16,    // aLBTMinRandom       +16=500   (rand(16..500))
  /*lbtTries         */ 4,     // aLBTMaxTxRetries+1   = 3(+1), 1st w/ backoff
  /*lbtTimeoutSym    */ 6000,  // aLBTTimeout          =   60ms
  /*lbtCcaSym        */ 16,    // aLBTMinFree          =  160us
  /*dutyTxOffSym     */ 10000, // aLBTTxMinOff         =  100ms 14-0382-23
  /*dutyTxMaxPktByte */ 12500, // aLBTTxMaxPKT         =    1s  14-0382-23
  /*dutyTxMaxSeqByte */ 50000, // aLBTMaxDlg           =    4s  14-0382-23
  /*pad[]            */ {   0, },
  /*ackMinSym        */ 45,    // aLBTAckWindowStart   =  450us >= aLBTRxTxTurnAround
  /*ackMaxSym        */ 100 + 136,// aLBTAckWindow+ACKtime= 1000us+(8+2+2+5)bytes
};
#else//!USE_ETSI_EN303_204_IN_CP30_31
 #define gb868Sb2Params gb868Sb1Params // Apply Sb1 Params to all Channel Pages
#endif//USE_ETSI_EN303_204_IN_CP30_31

// Per TRD Table 5 Regulated Maximum Power Levels
// Put exceptions ahead of default configuration
static const EmPhySubBandConfig subBandConfigs[] = {
  { // Annex 1 subband g1, Channels 0-9 (MAC Channel Page 28 Channels 0-9)
    /*minPhyChan     */ 0 + RADIO_CONFIGURATION_DATA_CHANNEL_NUMBER,
    /*maxPhyChan     */ 9 + RADIO_CONFIGURATION_DATA_CHANNEL_NUMBER,
    /*txMinPowerdBm  */ -43,
    /*txMaxPowerdBm  */ 14,
    /*edThresholddBm */ -87,  // aLBTThresholdLevelLp
    /*lbtDutyDenom   */ 36,   // mibDUTYCYCLERegulated      = 100s(/3600s)
    /*criDutyDenom   */ 48,   // mibDUTYCYCLECriticalThresh = 75s(/3600s)
    /*nonDutyDenom10 */ 100,  // 0.1% per docs-14-0017-00 (1000/10)
   #if     MAC_HAS_CHANNEL_PAGES
    /*macChanPage    */ 28,   // Channel Page 28 - low power
    /*macChanOffset  */ 0,
    /*pad[]          */ {  0, },
   #endif//MAC_HAS_CHANNEL_PAGES
    /*subBandParams  */ &gb868Sb1Params, // Sb1 parameters
    /*ccaConfigArray */ emPhyCcaLbtConfigArray,
  },
  { // Annex 1 subband g1, Channels 10-26 (MAC Channel Page 28 Channels 10-26)
    /*minPhyChan     */ 10 + RADIO_CONFIGURATION_DATA_CHANNEL_NUMBER,
    /*maxPhyChan     */ 26 + RADIO_CONFIGURATION_DATA_CHANNEL_NUMBER,
    /*txMinPowerdBm  */ -43,
    /*txMaxPowerdBm  */ 14,
    /*edThresholddBm */ -87,  // aLBTThresholdLevelLp
    /*lbtDutyDenom   */ 36,   // mibDUTYCYCLERegulated      = 100s(/3600s)
    /*criDutyDenom   */ 48,   // mibDUTYCYCLECriticalThresh = 75s(/3600s)
    /*nonDutyDenom10 */ 10,   // 1% per docs-14-0017-00 (100/10)
   #if     MAC_HAS_CHANNEL_PAGES
    /*macChanPage    */ 28,   // Channel Page 28 - low power
    /*macChanOffset  */ 10,
    /*pad[]          */ {  0, },
   #endif//MAC_HAS_CHANNEL_PAGES
    /*subBandParams  */ &gb868Sb1Params, // Sb1 parameters
    /*ccaConfigArray */ emPhyCcaLbtConfigArray,
  },
  { // Annex 1 subband g1, Channels 27-30 (MAC Channel Page 29 Channels 0-3)
    /*minPhyChan     */ 27 + RADIO_CONFIGURATION_DATA_CHANNEL_NUMBER,
    /*maxPhyChan     */ 30 + RADIO_CONFIGURATION_DATA_CHANNEL_NUMBER,
    /*txMinPowerdBm  */ -43,
    /*txMaxPowerdBm  */ 14,
    /*edThresholddBm */ -87,  // aLBTThresholdLevelLp
    /*lbtDutyDenom   */ 36,   // mibDUTYCYCLERegulated      = 100s(/3600s)
    /*criDutyDenom   */ 48,   // mibDUTYCYCLECriticalThresh = 75s(/3600s)
    /*nonDutyDenom10 */ 10,   // 1% per docs-14-0017-00 (100/10)
   #if     MAC_HAS_CHANNEL_PAGES
    /*macChanPage    */ 29,   // Channel Page 29 - low power
    /*macChanOffset  */ 0,
    /*pad[]          */ {  0, },
   #endif//MAC_HAS_CHANNEL_PAGES
    /*subBandParams  */ &gb868Sb1Params, // Sb1 parameters
    /*ccaConfigArray */ emPhyCcaLbtConfigArray,
  },
  { // Annex 1 subband g1, Channels 31-33 (MAC Channel Page 29 Channels 4-6)
    /*minPhyChan     */ 31 + RADIO_CONFIGURATION_DATA_CHANNEL_NUMBER,
    /*maxPhyChan     */ 33 + RADIO_CONFIGURATION_DATA_CHANNEL_NUMBER,
    /*txMinPowerdBm  */ -43,
    /*txMaxPowerdBm  */ 14,
    /*edThresholddBm */ -87,  // aLBTThresholdLevelLp
    /*lbtDutyDenom   */ 36,   // mibDUTYCYCLERegulated      = 100s(/3600s)
    /*criDutyDenom   */ 48,   // mibDUTYCYCLECriticalThresh = 75s(/3600s)
    /*nonDutyDenom10 */ 100,  // 0.1% per docs-14-0017-00 (1000/10)
   #if     MAC_HAS_CHANNEL_PAGES
    /*macChanPage    */ 29,   // Channel Page 29 - low power
    /*macChanOffset  */ 4,
    /*pad[]          */ {  0, },
   #endif//MAC_HAS_CHANNEL_PAGES
    /*subBandParams  */ &gb868Sb1Params, // Sb1 parameters
    /*ccaConfigArray */ emPhyCcaLbtConfigArray,
  },
  { // Annex 2 subband c, Channels 35-61 (MAC Channel Page 30 Channels 0-26)
    /*minPhyChan     */ 35 + RADIO_CONFIGURATION_DATA_CHANNEL_NUMBER,
    /*maxPhyChan     */ 61 + RADIO_CONFIGURATION_DATA_CHANNEL_NUMBER,
    /*txMinPowerdBm  */ -43,
    /*txMaxPowerdBm  */ 27,   // EU=+27 GB=+14; PRO2+ limits us to +20 dBm
    /*edThresholddBm */ -87,  // aLBTThresholdLevelLp (-91 Hp deprecated)
    /*lbtDutyDenom   */ 40,   // mibDUTYCYCLERegulated      = 90s(/3600s)
    /*criDutyDenom   */ 48,   // mibDUTYCYCLECriticalThresh = 75s(/3600s)
    /*nonDutyDenom10 */ 4,    // 2.5% per docs-14-0017-00 (40/10)
   #if     MAC_HAS_CHANNEL_PAGES
    /*macChanPage    */ 30,   // Channel Page 30 - high power
    /*macChanOffset  */ 0,
    /*pad[]          */ {  0, },
   #endif//MAC_HAS_CHANNEL_PAGES
    /*subBandParams  */ &gb868Sb2Params, // Sb2 parameters
    /*ccaConfigArray */ emPhyCcaLbtConfigArray,
  },
  { // Annex 1 subband g1, Channel 34 (MAC Channel Page 29 Channel 7)
    /*minPhyChan     */ 34 + RADIO_CONFIGURATION_DATA_CHANNEL_NUMBER,
    /*maxPhyChan     */ 34 + RADIO_CONFIGURATION_DATA_CHANNEL_NUMBER,
    /*txMinPowerdBm  */ -43,
    /*txMaxPowerdBm  */ 14,
    /*edThresholddBm */ -87,  // aLBTThresholdLevelLp
    /*lbtDutyDenom   */ 36,   // mibDUTYCYCLERegulated      = 100s(/3600s) (not 90s)
    /*criDutyDenom   */ 48,   // mibDUTYCYCLECriticalThresh = 75s(/3600s)
    /*nonDutyDenom10 */ 10,   // 1% per docs-14-0017-00 (100/10)
   #if     MAC_HAS_CHANNEL_PAGES
    /*macChanPage    */ 29,   // Channel Page 29 - low power
    /*macChanOffset  */ 7,
    /*pad[]          */ {  0, },
   #endif//MAC_HAS_CHANNEL_PAGES
    /*subBandParams  */ &gb868Sb1Params, // Sb1 parameters
    /*ccaConfigArray */ emPhyCcaLbtConfigArray,
  },
  { // Annex 1 subband g1, Channel 62 (MAC Channel Page 29 Channel 8)
    /*minPhyChan     */ 62 + RADIO_CONFIGURATION_DATA_CHANNEL_NUMBER,
    /*maxPhyChan     */ 62 + RADIO_CONFIGURATION_DATA_CHANNEL_NUMBER,
    /*txMinPowerdBm  */ -43,
    /*txMaxPowerdBm  */ 14,
    /*edThresholddBm */ -87,  // aLBTThresholdLevelLp
    /*lbtDutyDenom   */ 36,   // mibDUTYCYCLERegulated      = 100s(/3600s) (not 1%)
    /*criDutyDenom   */ 48,   // mibDUTYCYCLECriticalThresh = 75s(/3600s)
    /*nonDutyDenom10 */ 10,   // 1% per docs-14-0017-00 (100/10)
   #if     MAC_HAS_CHANNEL_PAGES
    /*macChanPage    */ 29,   // Channel Page 29 - low power
    /*macChanOffset  */ 8,
    /*pad[]          */ {  0, },
   #endif//MAC_HAS_CHANNEL_PAGES
    /*subBandParams  */ &gb868Sb1Params, // Sb1 parameters
    /*ccaConfigArray */ emPhyCcaLbtConfigArray,
  },
};

const EmPhyBandConfig emPhyBandConfig863MHz = {
  /*minPhyChan         */ 0 + RADIO_CONFIGURATION_DATA_CHANNEL_NUMBER,
  /*maxPhyChan         */ 62 + RADIO_CONFIGURATION_DATA_CHANNEL_NUMBER,
  /*pad[]              */ {       0, },
  /*bandName           */ (const uint8_t *)"863-876 MHz",
  /*baseFreqHz         */ 857250000,
  /*chanFreqHz         */ 200000,
  /*radioXoFreq        */ RADIO_CONFIGURATION_DATA_RADIO_XO_FREQ,
  /*dataRateBps        */ 100000,
  /*symbolBits         */ 1,
  /*preambleBits       */ 64,
  /*sfdBits            */ 16,
  /*numSubBandConfigs  */ sizeof(subBandConfigs) / sizeof(*subBandConfigs),
  /*subBandConfigs     */ subBandConfigs,
  /*radioConfigArray   */ myRadioConfig,
};

#endif//(PHY_DUALRAIL && !PHY_THIS && !PHY_INCLUDE_SOURCE)
