/***************************************************************************//**
 * @file
 * @brief Sub-GHz band on PRO2+ and EFR32 (RAIL and legacy PHYs)
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

#if     (defined(PHY_RAIL) || defined (PHY_SIMULATION_GB))

//-----------------------------------------------------------------------------
// 2.4GHz, 863MHz, 915MHz PHYs on EFR32
//-----------------------------------------------------------------------------
extern const EmPhyBandConfig emPhyBandConfig2400MHz;
extern const EmPhyBandConfig emPhyBandConfig863MHz;
extern const EmPhyBandConfig emPhyBandConfig915MHz;

static const EmPhyBandConfig* const emPhyBands[] = {
  &emPhyBandConfig2400MHz,
 #if     IEEE802154_GB868_SUPPORTED
  &emPhyBandConfig863MHz,
  &emPhyBandConfig915MHz,
 #endif//IEEE802154_GB868_SUPPORTED
};

const EmPhyConfig emPhyConfigEfr = {
  /*phyName       */ (const uint8_t *)"EFR32 GHz/MHz",
  /*rssidBmOffset */ 0, // Match what RAIL PHY does -- no offset
  /*pad[]         */ { 0, },
  /*numBands      */ sizeof(emPhyBands) / sizeof(*emPhyBands),
  /*numPAs        */ 0,
  /*bandConfigs   */ emPhyBands,
  /*PAs           */ NULL,
};

#else//!(defined(PHY_RAIL) || defined(PHY_SIMULATION_GB)

//-----------------------------------------------------------------------------
// 868 MHz PHY on PRO2+
//-----------------------------------------------------------------------------

extern const EmPhyBandConfig emPhyBandConfig915MHz;
extern const EmPhyBandConfig emPhyBandConfig863MHz;

static const EmPhyBandConfig* const emPhyBands[] = {
  &emPhyBandConfig915MHz, //TODO: Put 915 band first for initial testing
  &emPhyBandConfig863MHz,
};

// From Si4468_4461_C2_868MHz_OutPwr.xlsm:PTS_PwrVsDdac_868_4468 3.3V 25C
static const EmPhyTxPowerLevel emPhyTxPa20cLevels[] = {
  //dBm, PA_PWR_LVL
  { -40, 0 },    // -40.0
  { -23, 1 },
  { -15, 2 },
  { -11, 3 },
  {  -8, 4 },
  {  -5, 5 },
  {  -4, 6 },
  {  -2, 7 },
  {  -1, 8 },
  {   0, 9 },
  {   1, 10 },
  {   2, 11 },
  {   3, 12 },
  {   4, 14 },
  {   5, 16 },
  {   6, 17 },
  {   7, 19 },
  {   8, 22 },
  {   9, 25 },
  {  10, 28 },
  {  11, 31 },
  {  12, 35 },
  {  13, 39 },
  {  14, 44 },
  {  15, 50 },
  {  16, 56 },
  {  17, 63 },
  {  18, 72 },
  {  19, 82 },
  {  20, 97 },
  {  21, 127 },
};

static const EmPhyTxPowerLevel emPhyTxPa20fLevels[] = {
  //dBm, PA_PWR_LVL
  { -43, 0 },    // -46.0
  { -29, 1 },
  { -21, 2 },
  { -17, 3 },
  { -14, 4 },
  { -12, 5 },
  { -10, 6 },
  {  -8, 7 },
  {  -7, 8 },
  {  -6, 9 },
  {  -5, 10 },
  {  -4, 11 },
  {  -3, 12 },
  {  -2, 14 },
  {  -1, 16 },
  {   0, 18 },
  {   1, 20 },
  {   2, 22 },
  {   3, 25 },
  {   4, 28 },
  {   5, 31 },
  {   6, 35 },
  {   7, 39 },
  {   8, 44 },
  {   9, 50 },
  {  10, 56 },
  {  11, 63 },
  {  12, 71 },
  {  13, 80 },
  {  14, 90 },
  {  15, 102 },
  {  16, 116 },
  {  17, 127 },
};

// From Si4468_4461_C2_868MHz_OutPwr.xlsm:PTS_PwrVsDdac_868_4461 3.3V 25C
static const EmPhyTxPowerLevel emPhyTxPa16Levels[] = {
  //dBm, PA_PWR_LVL
  { -43, 0 },    // -43.7
  { -24, 1 },
  { -16, 2 },
  { -11, 3 },
  {  -8, 4 },
  {  -6, 5 },
  {  -4, 6 },
  {  -3, 7 },
  {  -2, 8 },
  {  -1, 9 },
  {   0, 10 },
  {   1, 11 },
  {   2, 12 },
  {   3, 13 },
  {   4, 15 },
  {   5, 16 },
  {   6, 18 },
  {   7, 21 },
  {   8, 23 },
  {   9, 26 },
  {  10, 30 },
  {  11, 34 },
  {  12, 38 },
  {  13, 43 },
  {  14, 48 },
  {  15, 56 },
  {  16, 65 },
  {  17, 79 },
  {  18, 107 },
  {  19, 127 },
};

// From Si4467_C2_868MHz_OutPwr.xlsm:PTS_PwrVsDdac_868_4467 3.3V 25C
static const EmPhyTxPowerLevel emPhyTxPa13Levels[] = {
  //dBm, PA_PWR_LVL
  { -43, 0 },    // -62.4
  { -18, 1 },
  { -12, 2 },
  {  -8, 3 },
  {  -6, 4 },
  {  -4, 5 },
  {  -2, 6 },
  {  -1, 7 },
  {   0, 8 },
  {   1, 9 },
  {   2, 10 },
  {   3, 11 },
  {   4, 13 },
  {   5, 16 },
  {   6, 18 },
  {   7, 20 },
  {   8, 22 },
  {   9, 26 },
  {  10, 30 },
  {  11, 36 },
  {  12, 45 },
  {  13, 61 },
  {  14, 79 },
  // PA_PWR_LVL values above 79 wrap in some weird way, so avoid them!
  // The default PA_PWR_LVL of 127 should end up being same as 79.
};

static const EmPhyTxPA emPhyTxPAs[] = {
  {
    /*paName        */ (const uint8_t *)"+13dBm LP",
    /*paSelEnum     */ 6, // PROP_PA_MODE_PA_SEL_ENUM_LP,
    /*numPowerLevels*/ sizeof(emPhyTxPa13Levels) / sizeof(*emPhyTxPa13Levels),
    /*powerLevels   */ emPhyTxPa13Levels,
  },
  {
    /*paName        */ (const uint8_t *)"+16dBm MP",
    /*paSelEnum     */ 8, // PROP_PA_MODE_PA_SEL_ENUM_MP,
    /*numPowerLevels*/ sizeof(emPhyTxPa16Levels) / sizeof(*emPhyTxPa16Levels),
    /*powerLevels   */ emPhyTxPa16Levels,
  },
  {
    /*paName        */ (const uint8_t *)"+20dBm Fine HP",
    /*paSelEnum     */ 1, // PROP_PA_MODE_PA_SEL_ENUM_HP_FINE,
    /*numPowerLevels*/ sizeof(emPhyTxPa20fLevels) / sizeof(*emPhyTxPa20fLevels),
    /*powerLevels   */ emPhyTxPa20fLevels,
  },
  {
    /*paName        */ (const uint8_t *)"+20dBm Coarse HP",
    /*paSelEnum     */ 2, // PROP_PA_MODE_PA_SEL_ENUM_HP_COARSE,
    /*numPowerLevels*/ sizeof(emPhyTxPa20cLevels) / sizeof(*emPhyTxPa20cLevels),
    /*powerLevels   */ emPhyTxPa20cLevels,
  },
};

const EmPhyConfig emPhyConfigMHz = {
  /*phyName       */ (const uint8_t *)"PRO2+ 868 MHz",
  /*rssidBmOffset */ 130, // From Si446x data sheet and used here too -----vvv
  /*pad[]         */ { 0, },
  /*numBands      */ sizeof(emPhyBands) / sizeof(*emPhyBands),
  /*numPAs        */ sizeof(emPhyTxPAs) / sizeof(*emPhyTxPAs),
  /*bandConfigs   */ emPhyBands,
  /*PAs           */ emPhyTxPAs,
};

#endif//(defined(PHY_RAIL) || defined(PHY_SIMULATION_GB)

#endif//(PHY_DUALRAIL && !PHY_THIS && !PHY_INCLUDE_SOURCE)
