/***************************************************************************//**
 * @file
 * @brief Dual-PHY shim multiplex layer
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
#include "core/sl_zigbee_stack.h"
#include "phy/phy.h"
#include "phy/plugin/duty-cycle/duty-cycle.h"
#include "phy/plugin/duty-cycle/duty-cycle-test.h"

#ifndef PHY_THIS_DEFAULT
#define PHY_THIS_DEFAULT PHY_THIS_BOTH
#endif//PHY_THIS_DEFAULT

#if     (defined(SL_ZIGBEE_STACK_CAN_MULTIMAC) && !defined(SL_ZIGBEE_NO_STACK))
#define DUAL_PHY_1MAC 0
#else//!(defined(SL_ZIGBEE_STACK_CAN_MULTIMAC) && !defined(SL_ZIGBEE_NO_STACK))
#define DUAL_PHY_1MAC 1
#endif//(defined(SL_ZIGBEE_STACK_CAN_MULTIMAC) && !defined(SL_ZIGBEE_NO_STACK))

#define PHY_ASSERT(expr) /* expr must not have side effects */ assert(expr)
#define PHY_ATTEST(expr) /* expr is ok to have side effects */ do { \
    if (!(expr)) {                                                  \
      PHY_ASSERT(false);                                            \
    }                                                               \
} while (0)
#define PHY_ATTEST_SUCCESS(expr) PHY_ATTEST((expr) == SL_ZIGBEE_SUCCESS)

#define DUAL_DCLF_MMAC(type, func) \
  extern type sli_phy0_##func;     \
  extern type sli_phy1_##func;     \
  type func // Wrapper exposed to Multi-MAC stack and apps

#if     DUAL_PHY_1MAC

// Full functionality "poor-man's dual-PHY" for single-MAC
#define DUAL_DCLF_TEST DUAL_DCLF_MMAC // Map test APIs to real wrapper

RadioTransmitConfig sli_phy1_radioTransmitConfig = RADIO_TRANSMIT_CONFIG_NOCCA_DEFAULTS;

uint8_t emPhyThis = PHY_THIS_DEFAULT;
uint8_t emPhyThisCallbackContext = PHY_THIS_UNSPECIFIED;

sl_status_t emPhySetThisPhy(uint8_t thisPhy)
{
  if ((thisPhy >  PHY_THIS_UNSPECIFIED)
      && (thisPhy <= PHY_THIS_BOTH)) {
    emPhyThis = thisPhy;
    return SL_STATUS_OK;
  }
  return SL_STATUS_INVALID_PARAMETER;
}

#else//!DUAL_PHY_1MAC

// Limited functionality "poor-man's dual-PHY" for multi-MAC
#define DUAL_DCLF_TEST(type, func) \
  extern type sli_phy0_##func;     \
  extern type sli_phy1_##func;     \
  type HIDDEN_##func // Hide test APIs so they'll be deadstripped

extern RadioTransmitConfig sli_phy1_radioTransmitConfig;

// Save code space by hardcoding these
#define emPhyThis PHY_THIS_DEFAULT
#define emPhyThisCallbackContext PHY_THIS_DEFAULT

sl_status_t emPhySetThisPhy(uint8_t thisPhy)
{
  return (thisPhy == emPhyThis) ? SL_STATUS_OK : SL_STATUS_INVALID_PARAMETER;
}

#endif//DUAL_PHY_1MAC

uint64_t emPhyDummy; // Dummy cell for DUAL_SET() to work

uint8_t emPhyGetThisPhy(void)
{
  return emPhyThis;
}

//-- emPhy API --

DUAL_DCLF_TEST(void, emPhyCancelTransmit(void))
{
  switch (emPhyThis) {
    case PHY_THIS_NATIVE:
      sli_phy0_emPhyCancelTransmit();
      break;
    case PHY_THIS_PRO2PLUS:
      sli_phy1_emPhyCancelTransmit();
      break;
    case PHY_THIS_BOTH:
      sli_phy0_emPhyCancelTransmit();
      sli_phy1_emPhyCancelTransmit();
      break;
    default:
      PHY_ASSERT(false);
      break;
  }
}

DUAL_DCLF_TEST(uint8_t, emPhyDcBucketIndex(uint32_t timeMsTick))
{
  uint8_t whichPhy = emPhyThisCallbackContext;
  if (whichPhy == PHY_THIS_UNSPECIFIED) {
    whichPhy = emPhyThis;
  }
  switch (whichPhy) {
    case PHY_THIS_NATIVE:
   #ifdef  CORTEXM3_EFR32_MICRO
      return sli_phy0_emPhyDcBucketIndex(timeMsTick);
   #endif//CORTEXM3_EFR32_MICRO
      break;
    case PHY_THIS_BOTH: // Get pro2plus only; no way to merge 'em
    case PHY_THIS_PRO2PLUS:
      return sli_phy1_emPhyDcBucketIndex(timeMsTick);
      break;
    default:
      PHY_ASSERT(false);
      break;
  }
  return 0;
}

DUAL_DCLF_MMAC(EmPhyDutyCycle*, emPhyDcEntry(void))
{
  uint8_t whichPhy = emPhyThisCallbackContext;
  if (whichPhy == PHY_THIS_UNSPECIFIED) {
    whichPhy = emPhyThis;
  }
  switch (whichPhy) {
    case PHY_THIS_NATIVE:
   #ifdef  CORTEXM3_EFR32_MICRO
      return sli_phy0_emPhyDcEntry();
   #endif//CORTEXM3_EFR32_MICRO
      break;
    case PHY_THIS_BOTH: // Get pro2plus only; no way to merge 'em
    case PHY_THIS_PRO2PLUS:
      return sli_phy1_emPhyDcEntry();
      break;
    default:
      PHY_ASSERT(false);
      break;
  }
  return NULL;
}

DUAL_DCLF_TEST(EmPhyDcState, emPhyDcGetActualState(void))
{
  uint8_t whichPhy = emPhyThisCallbackContext;
  if (whichPhy == PHY_THIS_UNSPECIFIED) {
    whichPhy = emPhyThis;
  }
  switch (whichPhy) {
    case PHY_THIS_NATIVE:
      return sli_phy0_emPhyDcGetActualState();
      break;
    case PHY_THIS_BOTH: // Get pro2plus only; no way to merge 'em
    case PHY_THIS_PRO2PLUS:
      return sli_phy1_emPhyDcGetActualState();
      break;
    default:
      PHY_ASSERT(false);
      break;
  }
  return EM_PHY_DC_STATE_OFF;
}

DUAL_DCLF_MMAC(EmPhyDcHectoPct,
               emPhyDcGetDcAndLimits(EmPhyDcLimits * dcLimitsPtr))
{
  uint8_t whichPhy = emPhyThisCallbackContext;
  if (whichPhy == PHY_THIS_UNSPECIFIED) {
    whichPhy = emPhyThis;
  }
  switch (whichPhy) {
    case PHY_THIS_NATIVE:
      return sli_phy0_emPhyDcGetDcAndLimits(dcLimitsPtr);
      break;
    case PHY_THIS_BOTH: // Get pro2plus only; no way to merge 'em
    case PHY_THIS_PRO2PLUS:
      return sli_phy1_emPhyDcGetDcAndLimits(dcLimitsPtr);
      break;
    default:
      PHY_ASSERT(false);
      break;
  }
  return 0; // DC is infinite so none used
}

DUAL_DCLF_TEST(EmPhyDcExtendedStatus, emPhyDcGetExtendedStatus(void))
{
  uint8_t whichPhy = emPhyThisCallbackContext;
  if (whichPhy == PHY_THIS_UNSPECIFIED) {
    whichPhy = emPhyThis;
  }
  switch (whichPhy) {
    case PHY_THIS_NATIVE:
      return sli_phy0_emPhyDcGetExtendedStatus();
      break;
    case PHY_THIS_BOTH: // Get pro2plus only; no way to merge 'em
    case PHY_THIS_PRO2PLUS:
      return sli_phy1_emPhyDcGetExtendedStatus();
      break;
    default:
      PHY_ASSERT(false);
      break;
  }
  return EM_PHY_DC_EXT_STATUS_VALUES;
}

DUAL_DCLF_TEST(const EmPhyDcLimits*, emPhyDcGetLimitsConfig(void))
{
  // Always get pro2class' as representative of both
  return sli_phy1_emPhyDcGetLimitsConfig();
}

DUAL_DCLF_MMAC(EmPhyDcMode, emPhyDcGetMode(void))
{
  uint8_t whichPhy = emPhyThisCallbackContext;
  if (whichPhy == PHY_THIS_UNSPECIFIED) {
    whichPhy = emPhyThis;
  }
  switch (whichPhy) {
    case PHY_THIS_NATIVE:
      return sli_phy0_emPhyDcGetMode();
      break;
    case PHY_THIS_BOTH: // Get pro2plus only; no way to merge 'em
    case PHY_THIS_PRO2PLUS:
      return sli_phy1_emPhyDcGetMode();
      break;
    default:
      PHY_ASSERT(false);
      break;
  }
  return EM_PHY_DC_MODE_OFF;
}

DUAL_DCLF_MMAC(EmPhyDcState, emPhyDcGetState(void))
{
  uint8_t whichPhy = emPhyThisCallbackContext;
  if (whichPhy == PHY_THIS_UNSPECIFIED) {
    whichPhy = emPhyThis;
  }
  switch (whichPhy) {
    case PHY_THIS_NATIVE:
      return sli_phy0_emPhyDcGetState();
      break;
    case PHY_THIS_BOTH: // Get pro2plus only; no way to merge 'em
    case PHY_THIS_PRO2PLUS:
      return sli_phy1_emPhyDcGetState();
      break;
    default:
      PHY_ASSERT(false);
      break;
  }
  return EM_PHY_DC_STATE_OFF;
}

DUAL_DCLF_TEST(uint16_t, emPhyDcGetTimeAccel(void))
{
  // Always get pro2class' as representative of both
  return sli_phy1_emPhyDcGetTimeAccel();
}

static uint8_t* txPacket;
static uint32_t txLimitMs;
sl_802154_short_addr_t emPhyDstCacheAddr[PHY_DUAL_CACHE_SIZE];
uint8_t emPhyDstCachePhys[PHY_DUAL_CACHE_SIZE]; // Doesn't need to be volatile
static uint8_t* dstCacheThis;

static uint8_t dstAddrCacheLookupPhy(uint8_t* packet)
{
  dstCacheThis = NULL;
  if ((sli_phy0_radioTransmitConfig.waitForAck)
      && (packet[0] >= 9)) {   // Has at least NodeId
    uint16_t macHdr = HIGH_LOW_TO_INT(packet[2], packet[1]);
    if ((macHdr & 0x0C20) == 0x0820) { // Is ACK-requesting and short DstAddr
      uint8_t i;
      sl_802154_short_addr_t dstAddr = HIGH_LOW_TO_INT(packet[7], packet[6]);
      for (i = 0; i < PHY_DUAL_CACHE_SIZE; i++) {
        if ((dstAddr == emPhyDstCacheAddr[i])
            && (emPhyDstCachePhys[i] != PHY_THIS_UNSPECIFIED)) {
          dstCacheThis = &emPhyDstCachePhys[i];
          return *dstCacheThis;
        }
      }
      // Push a new entry, tossing oldest out
      while (--i > 0) {
        emPhyDstCacheAddr[i] = emPhyDstCacheAddr[i - 1];
        emPhyDstCachePhys[i] = emPhyDstCachePhys[i - 1];
      }
      emPhyDstCacheAddr[0] = dstAddr;
      emPhyDstCachePhys[0] = PHY_THIS_BOTH;
      dstCacheThis = &emPhyDstCachePhys[0];
    }
  }
  return PHY_THIS_BOTH;
}

DUAL_DCLF_TEST(sl_status_t,
               sli_802154phy_dc_send_pkt_asap(uint8_t * packet,
                                              uint32_t limitMs,
                                              bool callbackOnErrorStatus))
{
  uint8_t whichPhys = emPhyThis;
  if (whichPhys == PHY_THIS_BOTH) {
    // Maybe cache can limit which PHYs this packet needs to go out
    whichPhys = dstAddrCacheLookupPhy(packet);
  }
  sli_phy1_radioTransmitConfig = sli_phy0_radioTransmitConfig; // Struct copy
  switch (whichPhys) {
    case PHY_THIS_BOTH: // First send native; completion will deal with Pro2plus
      txPacket = packet;
      txLimitMs = limitMs;
      (void) txPacket; // avoid unused var compiler warning
      (void) txLimitMs; // avoid unused var compiler warning
    // Fall through
    case PHY_THIS_NATIVE:
      return sli_phy0_sli_802154phy_dc_send_pkt_asap(packet, limitMs, callbackOnErrorStatus);
    case PHY_THIS_PRO2PLUS:
      return sli_phy1_sli_802154phy_dc_send_pkt_asap(packet, limitMs, callbackOnErrorStatus);
    default:
      PHY_ASSERT(false);
      break;
  }
  return SL_STATUS_FAIL;
}

DUAL_DCLF_TEST(void, sli_802154phy_dc_send_pkt_failed(void))
{
  switch (emPhyThisCallbackContext) {
    case PHY_THIS_NATIVE:
      sli_phy0_sli_802154phy_dc_send_pkt_failed();
      break;
    case PHY_THIS_PRO2PLUS:
      sli_phy1_sli_802154phy_dc_send_pkt_failed();
      break;
    default:
      PHY_ASSERT(false);
      break;
  }
}

DUAL_DCLF_MMAC(sl_status_t,
               emPhyDcSetLimitsConfig(const EmPhyDcLimits * limitsConfig))
{
  // Always set 'em both regardless of which one(s) are enabled
  (void) sli_phy0_emPhyDcSetLimitsConfig(limitsConfig);
  return sli_phy1_emPhyDcSetLimitsConfig(limitsConfig);
}

DUAL_DCLF_MMAC(sl_status_t, emPhyDcSetMode(EmPhyDcMode dcMode))
{
  sl_status_t status1 = SL_STATUS_OK;
  sl_status_t status2 = SL_STATUS_OK;
  switch (emPhyThis) {
    case PHY_THIS_NATIVE:
    case PHY_THIS_BOTH:
      status1 = sli_phy0_emPhyDcSetMode(dcMode);
      if (emPhyThis == PHY_THIS_NATIVE) {
        break;
      }
    // Fall through
    case PHY_THIS_PRO2PLUS:
      status2 = sli_phy1_emPhyDcSetMode(dcMode);
      break;
    default:
      PHY_ASSERT(false);
      break;
  }
  return (status1 != SL_STATUS_OK) ? status1 : status2;
}

DUAL_DCLF_MMAC(void, sli_802154phy_dc_set_time_accel(uint16_t timeAccel))
{
  // Always set 'em both regardless of which one(s) are enabled
  sli_phy0_sli_802154phy_dc_set_time_accel(timeAccel);
  sli_phy1_sli_802154phy_dc_set_time_accel(timeAccel);
}

DUAL_DCLF_TEST(bool, emPhyDeviceIsCoordinator(void))
{
  // Always get Native's as representative of both
  return sli_phy0_emPhyDeviceIsCoordinator();
}

DUAL_DCLF_MMAC(uint32_t, emPhyGetChannelFreqHz(uint8_t macPgChan))
{
  uint32_t freq = PHY_INVALID_FREQ_HZ;
  uint8_t whichPhy = emPhyThisCallbackContext;
  if (whichPhy == PHY_THIS_UNSPECIFIED) {
    whichPhy = emPhyThis;
  }
  switch (whichPhy) {
    case PHY_THIS_NATIVE:
    case PHY_THIS_BOTH: // Get Native only; no way to merge 'em
      freq = sli_phy0_emPhyGetChannelFreqHz(macPgChan);
      if ((whichPhy == PHY_THIS_NATIVE) || (freq != PHY_INVALID_FREQ_HZ)) {
        break;
      }
    // Fall through
    case PHY_THIS_PRO2PLUS:
      freq = sli_phy1_emPhyGetChannelFreqHz(macPgChan);
      break;
    default:
      PHY_ASSERT(false);
      break;
  }
  return freq;
}

DUAL_DCLF_MMAC(uint8_t, sli_802154phy_get_channel_page_for_channel(uint8_t macPgChan))
{
  // Pro2Plus works for Native too
  return sli_phy1_sli_802154phy_get_channel_page_for_channel(macPgChan);
}

DUAL_DCLF_MMAC(uint8_t, sli_802154phy_get_phy_channel(uint8_t macPgChan))
{
  uint8_t chan = INVALID_CHANNEL;
  uint8_t whichPhy = emPhyThisCallbackContext;
  if (whichPhy == PHY_THIS_UNSPECIFIED) {
    whichPhy = emPhyThis;
  }
  switch (whichPhy) {
    case PHY_THIS_PRO2PLUS:
    case PHY_THIS_BOTH:
      chan = sli_phy1_sli_802154phy_get_phy_channel(macPgChan);
      if ((whichPhy == PHY_THIS_PRO2PLUS) || (chan != INVALID_CHANNEL)) {
        break;
      }
    // Fall through
    case PHY_THIS_NATIVE:
      chan = sli_phy0_sli_802154phy_get_phy_channel(macPgChan);
      break;
    default:
      PHY_ASSERT(false);
      break;
  }
  return chan;
}

DUAL_DCLF_TEST(bool, emPhyIsTransmitting(void))
{
  bool isTransmitting = false;
  switch (emPhyThis) {
    case PHY_THIS_NATIVE:
    case PHY_THIS_BOTH:
      isTransmitting = sli_phy0_emPhyIsTransmitting();
      if ((emPhyThis == PHY_THIS_NATIVE) || isTransmitting) {
        break;
      }
    // Fall through
    case PHY_THIS_PRO2PLUS:
      isTransmitting = sli_phy1_emPhyIsTransmitting();
      break;
    default:
      PHY_ASSERT(false);
      break;
  }
  return isTransmitting;
}

DUAL_DCLF_MMAC(void, sli_802154phy_set_eui64(void))
{
  // Always update both regardless of which one(s) are enabled
  sli_phy0_sli_802154phy_set_eui64();
  sli_phy1_sli_802154phy_set_eui64();
}

DUAL_DCLF_MMAC(void, sli_802154phy_set_coordinator(bool coordinator))
{
  // Always update both regardless of which one(s) are enabled
  sli_phy0_sli_802154phy_set_coordinator(coordinator);
  sli_phy1_sli_802154phy_set_coordinator(coordinator);
}

DUAL_DCLF_TEST(uint32_t, emPhySymbolsToUs(uint32_t symbols))
{
  switch (emPhyThis) {
    case PHY_THIS_NATIVE:
      return sli_phy0_emPhySymbolsToUs(symbols);
      break;
    case PHY_THIS_PRO2PLUS:
    case PHY_THIS_BOTH: // Get pro2plus only; no way to merge 'em
      return sli_phy1_emPhySymbolsToUs(symbols);
      break;
    default:
      PHY_ASSERT(false);
      break;
  }
  return 0; // Avoids compiler warning
}

DUAL_DCLF_MMAC(void, emPhyTick(bool isrContext))
{
  // Always tick 'em both regardless of which one(s) are enabled
  sli_phy0_emPhyTick(isrContext);
  sli_phy1_emPhyTick(isrContext);
}

//-- emRadio API --

DUAL_DCLF_MMAC(uint8_t, emRadioAddressMatchingEnabled(void))
{
  // Always get Native's as representative of both
  return sli_phy0_emRadioAddressMatchingEnabled();
}

DUAL_DCLF_MMAC(bool, sli_802154phy_radio_auto_ack_enabled(void))
{
  // Always get Native's as representative of both
  return sli_phy0_sli_802154phy_radio_auto_ack_enabled();
}

DUAL_DCLF_TEST(uint8_t,
               sli_802154phy_radio_calculate_link_quality(uint16_t unpackedChipErrors,
                                                          uint16_t packetLength))
{
  uint8_t whichPhy = emPhyThisCallbackContext;
  if (whichPhy == PHY_THIS_UNSPECIFIED) {
    whichPhy = emPhyThis;
  }
  switch (whichPhy) {
    case PHY_THIS_NATIVE:
      return sli_phy0_sli_802154phy_radio_calculate_link_quality(unpackedChipErrors,
                                                                 packetLength);
      break;
    case PHY_THIS_BOTH: // Get pro2plus only; no way to merge 'em
    case PHY_THIS_PRO2PLUS:
      return sli_phy1_sli_802154phy_radio_calculate_link_quality(unpackedChipErrors,
                                                                 packetLength);
      break;
    default:
      PHY_ASSERT(false);
      break;
  }
  return unpackedChipErrors;
}

DUAL_DCLF_TEST(bool, sli_802154phy_radio_check_radio(void))
{
  // Always check both regardless of which one(s) are enabled
  return sli_phy0_sli_802154phy_radio_check_radio() || sli_phy1_sli_802154phy_radio_check_radio();
}

DUAL_DCLF_TEST(uint8_t, sli_802154phy_radio_convert_rssi_to_ed(int8_t rssiDbm))
{
  switch (emPhyThis) {
    case PHY_THIS_NATIVE:
    case PHY_THIS_BOTH: // Get Native only; no way to merge 'em
      return sli_phy0_sli_802154phy_radio_convert_rssi_to_ed(rssiDbm);
      break;
    case PHY_THIS_PRO2PLUS:
      return sli_phy1_sli_802154phy_radio_convert_rssi_to_ed(rssiDbm);
      break;
    default:
      PHY_ASSERT(false);
      break;
  }
  return 0;
}

DUAL_DCLF_TEST(uint8_t, emRadioGetCca(void))
{
  switch (emPhyThis) {
    case PHY_THIS_NATIVE:
      return sli_phy0_emRadioGetCca();
      break;
    case PHY_THIS_PRO2PLUS:
    case PHY_THIS_BOTH: // Get pro2plus only, since used primarily by LBT
      return sli_phy1_emRadioGetCca();
      break;
    default:
      PHY_ASSERT(false);
      break;
  }
  return CCA_CLEAR;
}

DUAL_DCLF_MMAC(void, emRadioEnableAddressMatching(uint8_t enable))
{
  // Always update both regardless of which one(s) are enabled
  sli_phy0_emRadioEnableAddressMatching(enable);
  sli_phy1_emRadioEnableAddressMatching(enable);
}

DUAL_DCLF_MMAC(void, sli_802154mac_radio_enable_auto_ack(bool enable))
{
  // Always update both regardless of which one(s) are enabled
  sli_phy0_sli_802154mac_radio_enable_auto_ack(enable);
  sli_phy1_sli_802154mac_radio_enable_auto_ack(enable);
}

DUAL_DCLF_TEST(void, emRadioEnableOverflowNotification(bool enable))
{
  // Always update both regardless of which one(s) are enabled
  sli_phy0_emRadioEnableOverflowNotification(enable);
  sli_phy1_emRadioEnableOverflowNotification(enable);
}

DUAL_DCLF_TEST(void, emRadioEnablePacketTrace(bool enable))
{
  // Always update both regardless of which one(s) are enabled
  sli_phy0_emRadioEnablePacketTrace(enable);
  sli_phy1_emRadioEnablePacketTrace(enable);
}

DUAL_DCLF_TEST(void, emRadioEnableReceiveCompleteInt(bool enable))
{
  // Always update both regardless of which one(s) are enabled
  sli_phy0_emRadioEnableReceiveCompleteInt(enable);
  sli_phy1_emRadioEnableReceiveCompleteInt(enable);
}

DUAL_DCLF_TEST(void, emRadioEnableReceiveCrc(bool enable))
{
  // Always update both regardless of which one(s) are enabled
  sli_phy0_emRadioEnableReceiveCrc(enable);
  sli_phy1_emRadioEnableReceiveCrc(enable);
}

DUAL_DCLF_TEST(void, emRadioEnableSfdRecvNotification(bool enable))
{
  // Always update both regardless of which one(s) are enabled
  sli_phy0_emRadioEnableSfdRecvNotification(enable);
  sli_phy1_emRadioEnableSfdRecvNotification(enable);
}

DUAL_DCLF_TEST(void, emRadioEnableSfdSentNotification(bool enable))
{
  // Always update both regardless of which one(s) are enabled
  sli_phy0_emRadioEnableSfdSentNotification(enable);
  sli_phy1_emRadioEnableSfdSentNotification(enable);
}

DUAL_DCLF_MMAC(int8_t, sli_802154phy_radio_energy_detection(void))
{
  int8_t energy1 = SL_ZIGBEE_PHY_INVALID_RSSI;
  int8_t energy2 = SL_ZIGBEE_PHY_INVALID_RSSI;
  switch (emPhyThis) {
    case PHY_THIS_NATIVE:
    case PHY_THIS_BOTH:
      energy1 = sli_phy0_sli_802154phy_radio_energy_detection();
      if (emPhyThis == PHY_THIS_NATIVE) {
        break;
      }
    // Fall through
    case PHY_THIS_PRO2PLUS:
      energy2 = sli_phy1_sli_802154phy_radio_energy_detection();
      break;
    default:
      PHY_ASSERT(false);
      break;
  }
  return (energy1 >= energy2) ? energy1 : energy2;
}

DUAL_DCLF_MMAC(int8_t, emRadioGetEdCcaThreshold(void))
{
  switch (emPhyThis) {
    case PHY_THIS_NATIVE:
    case PHY_THIS_BOTH: // Get Native only; no way to merge 'em
      return sli_phy0_emRadioGetEdCcaThreshold();
      break;
    case PHY_THIS_PRO2PLUS:
      return sli_phy1_emRadioGetEdCcaThreshold();
      break;
    default:
      PHY_ASSERT(false);
      break;
  }
  return SL_ZIGBEE_PHY_INVALID_RSSI;
}

DUAL_DCLF_TEST(RadioPowerMode, sli_802154phy_radio_get_idle_mode(void))
{
  switch (emPhyThis) {
    case PHY_THIS_NATIVE:
    case PHY_THIS_BOTH: // Get Native only; no way to merge 'em
      return sli_phy0_sli_802154phy_radio_get_idle_mode();
      break;
    case PHY_THIS_PRO2PLUS:
      return sli_phy1_sli_802154phy_radio_get_idle_mode();
      break;
    default:
      PHY_ASSERT(false);
      break;
  }
  return SL_ZIGBEE_RADIO_POWER_MODE_OFF;
}

DUAL_DCLF_MMAC(sl_802154_short_addr_t, sli_802154mac_radio_get_node_id(void))
{
  // Always get Native's as representative of both
  return sli_phy0_sli_802154mac_radio_get_node_id();
}

DUAL_DCLF_MMAC(sl_802154_pan_id_t, sli_802154phy_radio_get_pan_id(void))
{
  // Always get Native's as representative of both
  return sli_phy0_sli_802154phy_radio_get_pan_id();
}

DUAL_DCLF_MMAC(RadioPowerMode, sli_802154phy_radio_get_power_status(void))
{
  switch (emPhyThis) {
    case PHY_THIS_NATIVE:
    case PHY_THIS_BOTH: // Get Native only; no way to merge 'em
      return sli_phy0_sli_802154phy_radio_get_power_status();
      break;
    case PHY_THIS_PRO2PLUS:
      return sli_phy1_sli_802154phy_radio_get_power_status();
      break;
    default:
      PHY_ASSERT(false);
      break;
  }
  return SL_ZIGBEE_RADIO_POWER_MODE_OFF;
}

DUAL_DCLF_TEST(bool, emRadioHoldOffIsActive(void))
{
  return sli_phy0_emRadioHoldOffIsActive();
  // Only need to hit Native PHY for holdoff; SubGHz PHY doesn't support it
}

DUAL_DCLF_MMAC(void, emRadioHoldOffIsr(bool active))
{
  sli_phy0_emRadioHoldOffIsr(active);
  // Only need to hit Native PHY for holdoff; SubGHz PHY doesn't support it
  // sli_phy1_emRadioHoldOffIsr(active);
}

DUAL_DCLF_MMAC(void, sli_802154phy_radio_init(RadioPowerMode initialRadioPowerMode))
{
  switch (emPhyThis) {
    case PHY_THIS_NATIVE:
      sli_phy0_sli_802154phy_radio_init(initialRadioPowerMode);
      sli_phy1_sli_802154phy_radio_init(SL_ZIGBEE_RADIO_POWER_MODE_OFF);
      break;
    case PHY_THIS_PRO2PLUS:
      sli_phy0_sli_802154phy_radio_init(SL_ZIGBEE_RADIO_POWER_MODE_OFF);
      sli_phy1_sli_802154phy_radio_init(initialRadioPowerMode);
      break;
    case PHY_THIS_BOTH:
      sli_phy0_sli_802154phy_radio_init(initialRadioPowerMode);
      sli_phy1_sli_802154phy_radio_init(initialRadioPowerMode);
      break;
    default:
      PHY_ASSERT(false);
      break;
  }
}

DUAL_DCLF_TEST(void, sli_802154phy_radio_ok_to_idle(void))
{
  switch (emPhyThisCallbackContext) {
    case PHY_THIS_NATIVE:
      sli_phy0_sli_802154phy_radio_ok_to_idle();
      break;
    case PHY_THIS_PRO2PLUS:
      sli_phy1_sli_802154phy_radio_ok_to_idle();
      break;
    default:
      PHY_ASSERT(false);
      break;
  }
}

DUAL_DCLF_TEST(bool, emRadioOverflowNotificationEnabled(void))
{
  // Always get Native's as representative of both
  return sli_phy0_emRadioOverflowNotificationEnabled();
}

DUAL_DCLF_TEST(bool, sli_802154phy_radio_packet_trace_enabled(void))
{
  // Always get Native's as representative of both
  return sli_phy0_sli_802154phy_radio_packet_trace_enabled();
}

DUAL_DCLF_TEST(bool, emRadioReceiveCompleteIntEnabled(void))
{
  // Always get Pro2Plus's as representative of both
  return sli_phy1_emRadioReceiveCompleteIntEnabled();
}

DUAL_DCLF_TEST(bool,
               emRadioReceiveCompleteIntPending(bool acknowledgePending))
{
  // Always check both regardless of which one(s) are enabled
  return sli_phy0_emRadioReceiveCompleteIntPending(acknowledgePending)
         || sli_phy1_emRadioReceiveCompleteIntPending(acknowledgePending);
}

DUAL_DCLF_TEST(bool, emRadioReceiveCrcEnabled(void))
{
  // Always get pro2class' as representative of both
  return sli_phy1_emRadioReceiveCrcEnabled();
}

DUAL_DCLF_TEST(sl_status_t,
               emRadioSetChannelAndForceCalibration(uint8_t channel))
{
  sl_status_t status = SL_STATUS_FAIL;
  switch (emPhyThis) {
    case PHY_THIS_NATIVE:
    case PHY_THIS_BOTH:
      status = sli_phy0_emRadioSetChannelAndForceCalibration(channel);
      if ((emPhyThis == PHY_THIS_NATIVE) || (status == SL_STATUS_OK)) {
        break;
      }
    // Fall through
    case PHY_THIS_PRO2PLUS:
      status = sli_phy1_emRadioSetChannelAndForceCalibration(channel);
      break;
    default:
      PHY_ASSERT(false);
      break;
  }
  return status;
}

DUAL_DCLF_MMAC(void, emRadioSetEdCcaThreshold(int8_t threshold))
{
  switch (emPhyThis) {
    case PHY_THIS_NATIVE:
      sli_phy0_emRadioSetEdCcaThreshold(threshold);
      break;
    case PHY_THIS_PRO2PLUS:
      sli_phy1_emRadioSetEdCcaThreshold(threshold);
      break;
    case PHY_THIS_BOTH:
      sli_phy0_emRadioSetEdCcaThreshold(threshold);
      sli_phy1_emRadioSetEdCcaThreshold(threshold);
      break;
    default:
      PHY_ASSERT(false);
      break;
  }
}

DUAL_DCLF_TEST(sl_status_t, sli_802154phy_radio_set_idle_mode(RadioPowerMode mode))
{
  sl_status_t status1 = SL_STATUS_OK;
  sl_status_t status2 = SL_STATUS_OK;
  switch (emPhyThis) {
    case PHY_THIS_NATIVE:
    case PHY_THIS_BOTH:
      status1 = sli_phy0_sli_802154phy_radio_set_idle_mode(mode);
      if (emPhyThis == PHY_THIS_NATIVE) {
        break;
      }
    // Fall through
    case PHY_THIS_PRO2PLUS:
      status2 = sli_phy1_sli_802154phy_radio_set_idle_mode(mode);
      break;
    default:
      PHY_ASSERT(false);
      break;
  }
  return (status1 != SL_STATUS_OK) ? status1 : status2;
}

DUAL_DCLF_MMAC(void, sli_802154phy_radio_set_node_id(sl_802154_short_addr_t nodeId))
{
  // Always update both regardless of which one(s) are enabled
  sli_phy0_sli_802154phy_radio_set_node_id(nodeId);
  sli_phy1_sli_802154phy_radio_set_node_id(nodeId);
}

DUAL_DCLF_MMAC(void, sli_802154phy_radio_set_pan_id(uint16_t panId))
{
  // Always update both regardless of which one(s) are enabled
  sli_phy0_sli_802154phy_radio_set_pan_id(panId);
  sli_phy1_sli_802154phy_radio_set_pan_id(panId);
}

DUAL_DCLF_TEST(bool, emRadioSfdRecvNotificationEnabled(void))
{
  // Always get Native's as representative of both
  return sli_phy0_emRadioSfdRecvNotificationEnabled();
}

DUAL_DCLF_TEST(bool, emRadioSfdSentNotificationEnabled(void))
{
  // Always get Native's as representative of both
  return sli_phy0_emRadioSfdSentNotificationEnabled();
}

DUAL_DCLF_MMAC(void, sli_802154phy_radio_sleep(void))
{
  switch (emPhyThis) {
    case PHY_THIS_NATIVE:
      sli_phy0_sli_802154phy_radio_sleep();
      break;
    case PHY_THIS_PRO2PLUS:
      sli_phy1_sli_802154phy_radio_sleep();
      break;
    case PHY_THIS_BOTH:
      sli_phy0_sli_802154phy_radio_sleep();
      sli_phy1_sli_802154phy_radio_sleep();
      break;
    default:
      PHY_ASSERT(false);
      break;
  }
}

DUAL_DCLF_TEST(sl_status_t, emRadioTransmit(uint8_t * packet))
{
  switch (emPhyThis) {
    case PHY_THIS_NATIVE:
      return sli_phy0_emRadioTransmit(packet);
    case PHY_THIS_PRO2PLUS:
    case PHY_THIS_BOTH: // Only Pro2plus since it's messy if 2nd one fails
      sli_phy1_radioTransmitConfig = sli_phy0_radioTransmitConfig; // Struct copy
      return sli_phy1_emRadioTransmit(packet);
    default:
      PHY_ASSERT(false);
      break;
  }
  return SL_STATUS_FAIL;
}

DUAL_DCLF_TEST(uint16_t, sli_802154phy_radio_unpack_chip_errors(uint8_t internalChipErrors))
{
  uint8_t whichPhy = emPhyThisCallbackContext;
  if (whichPhy == PHY_THIS_UNSPECIFIED) {
    whichPhy = emPhyThis;
  }
  switch (whichPhy) {
    case PHY_THIS_NATIVE:
      return sli_phy0_sli_802154phy_radio_unpack_chip_errors(internalChipErrors);
      break;
    case PHY_THIS_BOTH: // Get pro2plus only; no way to merge 'em
    case PHY_THIS_PRO2PLUS:
      return sli_phy1_sli_802154phy_radio_unpack_chip_errors(internalChipErrors);
      break;
    default:
      PHY_ASSERT(false);
      break;
  }
  return internalChipErrors;
}

DUAL_DCLF_MMAC(void, sli_802154phy_radio_wake_up(void))
{
  switch (emPhyThis) {
    case PHY_THIS_NATIVE:
      sli_phy0_sli_802154phy_radio_wake_up();
      break;
    case PHY_THIS_PRO2PLUS:
      sli_phy1_sli_802154phy_radio_wake_up();
      break;
    case PHY_THIS_BOTH:
      sli_phy0_sli_802154phy_radio_wake_up();
      sli_phy1_sli_802154phy_radio_wake_up();
      break;
    default:
      PHY_ASSERT(false);
      break;
  }
}

//-- ember API --

DUAL_DCLF_MMAC(void, sl_802154_calibrate_current_channel(void))
{
  // Always check both regardless of which one(s) are enabled
  sli_phy0_sl_802154_calibrate_current_channel();
  sli_phy1_sl_802154_calibrate_current_channel();
}

DUAL_DCLF_MMAC(sl_status_t, sli_802154_stack_set_tx_power_mode(uint16_t txPowerMode))
{
  switch (emPhyThis) {
    case PHY_THIS_NATIVE:
    case PHY_THIS_BOTH:
      (void) sli_phy0_sli_802154_stack_set_tx_power_mode(txPowerMode);
    // Fall through
    case PHY_THIS_PRO2PLUS:
      return sli_phy1_sli_802154_stack_set_tx_power_mode(txPowerMode);
      break;
    default:
      PHY_ASSERT(false);
      break;
  }
  return SL_STATUS_FAIL;
}

DUAL_DCLF_MMAC(sl_status_t, sli_802154_stack_radio_get_scheduler_priorities(sl_802154_radio_priorities_t * priorities))
{
  // Always get Native's as representative of both
  return sli_phy0_sli_802154_stack_radio_get_scheduler_priorities(priorities);
}

DUAL_DCLF_MMAC(sl_status_t, sli_802154_stack_radio_set_scheduler_priorities(const sl_802154_radio_priorities_t * priorities))
{
  // Always set Native's as representative of both
  return sli_phy0_sli_802154_stack_radio_set_scheduler_priorities(priorities);
}

DUAL_DCLF_MMAC(sl_status_t, sli_802154_stack_radio_set_scheduler_sliptime(uint32_t slipTime))
{
  // Always set Native's as representative of both
  return sli_phy0_sli_802154_stack_radio_set_scheduler_sliptime(slipTime);
}

DUAL_DCLF_MMAC(sl_status_t, sli_802154_stack_radio_get_scheduler_sliptime(uint32_t * slipTime))
{
  // Always get Native's as representative of both
  return sli_phy0_sli_802154_stack_radio_get_scheduler_sliptime(slipTime);
}

//-- Non-conformant API --

DUAL_DCLF_MMAC(uint8_t, sli_802154phy_cost_division(uint8_t cost))
{
  uint8_t cost1 = 255;
  uint8_t cost2 = 255;
  switch (emPhyThis) {
    case PHY_THIS_NATIVE:
    case PHY_THIS_BOTH: // Take smallest cost from both
      cost1 = sli_phy0_sli_802154phy_cost_division(cost);
      if (emPhyThis == PHY_THIS_NATIVE) {
        break;
      }
    // Fall through
    case PHY_THIS_PRO2PLUS:
      cost2 = sli_phy1_sli_802154phy_cost_division(cost);
      break;
    default:
      PHY_ASSERT(false);
      break;
  }
  return ((cost1 < cost2) ? cost1 : cost2);
}

DUAL_DCLF_MMAC(uint8_t, sli_802154phy_get_phy_radio_channel(void))
{
  switch (emPhyThis) {
    case PHY_THIS_NATIVE:
    case PHY_THIS_BOTH: // Get Native only; no way to merge 'em
      return sli_phy0_sli_802154phy_get_phy_radio_channel();
      break;
    case PHY_THIS_PRO2PLUS:
      return sli_phy1_sli_802154phy_get_phy_radio_channel();
      break;
    default:
      PHY_ASSERT(false);
      break;
  }
  return INVALID_CHANNEL;
}

DUAL_DCLF_MMAC(int8_t, sli_802154phy_get_phy_radio_power(void))
{
  switch (emPhyThis) {
    case PHY_THIS_NATIVE:
    case PHY_THIS_BOTH: // Get Native only; no way to merge 'em
      return sli_phy0_sli_802154phy_get_phy_radio_power();
      break;
    case PHY_THIS_PRO2PLUS:
      return sli_phy1_sli_802154phy_get_phy_radio_power();
      break;
    default:
      PHY_ASSERT(false);
      break;
  }
  return INVALID_RADIO_POWER;
}

DUAL_DCLF_MMAC(uint16_t, emGetTxPowerMode(void))
{
  switch (emPhyThis) {
    case PHY_THIS_NATIVE:
    case PHY_THIS_BOTH: // Only get native in this case
      return sli_phy0_emGetTxPowerMode();
      break;
    case PHY_THIS_PRO2PLUS:
      return sli_phy1_emGetTxPowerMode();
      break;
    default:
      PHY_ASSERT(false);
      break;
  }
  return 0;
}

DUAL_DCLF_MMAC(sl_status_t, sli_802154phy_set_phy_radio_channel(uint8_t channel))
{
  sl_status_t status1 = SL_STATUS_INVALID_CHANNEL_MASK;
  sl_status_t status2 = SL_STATUS_INVALID_CHANNEL_MASK;
  switch (emPhyThis) {
    case PHY_THIS_NATIVE:
    case PHY_THIS_BOTH:
      status1 = sli_phy0_sli_802154phy_set_phy_radio_channel(channel);
      if (emPhyThis == PHY_THIS_NATIVE) {
        break;
      }
    // Fall through
    case PHY_THIS_PRO2PLUS:
      status2 = sli_phy1_sli_802154phy_set_phy_radio_channel(channel);
      break;
    default:
      PHY_ASSERT(false);
      break;
  }
  return (status1 == SL_STATUS_OK) ? SL_STATUS_OK : status2;
}

DUAL_DCLF_MMAC(sl_status_t, sli_802154phy_set_phy_radio_power(int8_t power))
{
  sl_status_t status1 = SL_STATUS_TRANSMIT_INVALID_POWER;
  sl_status_t status2 = SL_STATUS_TRANSMIT_INVALID_POWER;
  switch (emPhyThis) {
    case PHY_THIS_NATIVE:
    case PHY_THIS_BOTH:
      status1 = sli_phy0_sli_802154phy_set_phy_radio_power(power);
      if (emPhyThis == PHY_THIS_NATIVE) {
        break;
      }
    // Fall through
    case PHY_THIS_PRO2PLUS:
      status2 = sli_phy1_sli_802154phy_set_phy_radio_power(power);
      break;
    default:
      PHY_ASSERT(false);
      break;
  }
  return (status1 == SL_STATUS_OK) ? SL_STATUS_OK : status2;
}

DUAL_DCLF_TEST(void, sli_802154phy_start_transmit_stream(void))
{
  switch (emPhyThis) {
    case PHY_THIS_NATIVE:
    case PHY_THIS_BOTH:
      sli_phy0_sli_802154phy_start_transmit_stream();
      if (emPhyThis == PHY_THIS_NATIVE) {
        break;
      }
    // Fall through
    case PHY_THIS_PRO2PLUS:
      sli_phy1_sli_802154phy_start_transmit_stream();
      break;
    default:
      PHY_ASSERT(false);
      break;
  }
}

DUAL_DCLF_TEST(void, sli_802154phy_stop_transmit_stream(void))
{
  switch (emPhyThis) {
    case PHY_THIS_NATIVE:
    case PHY_THIS_BOTH:
      sli_phy0_sli_802154phy_stop_transmit_stream();
      if (emPhyThis == PHY_THIS_NATIVE) {
        break;
      }
    // Fall through
    case PHY_THIS_PRO2PLUS:
      sli_phy1_sli_802154phy_stop_transmit_stream();
      break;
    default:
      PHY_ASSERT(false);
      break;
  }
}

DUAL_DCLF_TEST(void, sli_802154phy_start_transmit_tone(void))
{
  switch (emPhyThis) {
    case PHY_THIS_NATIVE:
    case PHY_THIS_BOTH:
      sli_phy0_sli_802154phy_start_transmit_tone();
      if (emPhyThis == PHY_THIS_NATIVE) {
        break;
      }
    // Fall through
    case PHY_THIS_PRO2PLUS:
      sli_phy1_sli_802154phy_start_transmit_tone();
      break;
    default:
      PHY_ASSERT(false);
      break;
  }
}

DUAL_DCLF_TEST(void, sli_802154phy_stop_transmit_tone(void))
{
  switch (emPhyThis) {
    case PHY_THIS_NATIVE:
    case PHY_THIS_BOTH:
      sli_phy0_sli_802154phy_stop_transmit_tone();
      if (emPhyThis == PHY_THIS_NATIVE) {
        break;
      }
    // Fall through
    case PHY_THIS_PRO2PLUS:
      sli_phy1_sli_802154phy_stop_transmit_tone();
      break;
    default:
      PHY_ASSERT(false);
      break;
  }
}

//-- Callbacks --

#if     DUAL_PHY_1MAC

static void muxTransmitCompleteCallback(sl_status_t status,
                                        uint32_t sfdSentTime,
                                        uint8_t framePending)
{
  if (emPhyThis == PHY_THIS_BOTH) {
    if (emPhyThisCallbackContext == PHY_THIS_NATIVE) { // Native completes 1st
      switch (status) {
        case SL_STATUS_TRANSMIT_ACK_RECEIVED:  // ACK received, found right PHY!
          if (dstCacheThis != NULL) {
            *dstCacheThis = PHY_THIS_NATIVE;
          }
        // Fall through
        case SL_STATUS_TRANSMIT_INCOMPLETE: // Packet failed to go out unexpectedly
        case SL_STATUS_TRANSMIT_BLOCKED:    // Packet failed to go out unexpectedly
        case SL_STATUS_TRANSMIT_UNDERFLOW:  // Packet failed to go out unexpectedly
        case SL_STATUS_CCA_FAILURE:         // Packet failed to go out
        case SL_STATUS_TRANSMIT_SCHEDULER_FAIL: // Radio scheduler couldn't send packet
        case SL_STATUS_TRANSMIT_BUSY:       // Radio already BUSY
          break;                            // We're done!
        case SL_STATUS_MAC_NO_ACK_RECEIVED: // No ACK received, maybe wrong PHY?
          if (dstCacheThis != NULL) {
            *dstCacheThis = PHY_THIS_BOTH;
          }
        // Fall through
        case SL_STATUS_OK:           // Non-ACK pkt, also send Pro2plus PHY
          status = sli_phy1_sli_802154phy_dc_send_pkt_asap(txPacket, txLimitMs, false);
          if (status == SL_STATUS_OK) {
            sli_phy0_sli_802154phy_radio_ok_to_idle(); // Idle native PHY
            return;                   // Wait for pro2plus PHY
          }
          break;
        default:
          PHY_ASSERT(false);
          break;
      }
    } else                            // Pro2plus completes 2nd
    if (dstCacheThis != NULL) {
      switch (status) {
        case SL_ZIGBEE_PHY_ACK_RECEIVED:  // ACK received, found right PHY!
          *dstCacheThis = PHY_THIS_PRO2PLUS;
          break;
        case SL_802154_NO_ACK_RECEIVED: // No ACK received, maybe wrong PHY?
          *dstCacheThis = PHY_THIS_BOTH;
          break;
      }
    }
    // status reflects what we should report up
  }
  sli_802154phy_radio_transmit_complete_callback(status, sfdSentTime, framePending);
}

void sli_phy0_sli_zigbee_packet_received_internal_isr_callback(uint8_t *packet,
                                                               uint8_t length,
                                                               bool framePendingSetInTxAck)
{
  PHY_ASSERT(emPhyThisCallbackContext == PHY_THIS_UNSPECIFIED);
  emPhyThisCallbackContext = PHY_THIS_NATIVE;
  sli_zigbee_packet_received_internal_isr_callback(packet, length, framePendingSetInTxAck);
  emPhyThisCallbackContext = PHY_THIS_UNSPECIFIED;
}

void sli_phy1_sli_zigbee_packet_received_internal_isr_callback(uint8_t *packet,
                                                               uint8_t length,
                                                               bool framePendingSetInTxAck)
{
  PHY_ASSERT(emPhyThisCallbackContext == PHY_THIS_UNSPECIFIED);
  emPhyThisCallbackContext = PHY_THIS_PRO2PLUS;
  sli_zigbee_packet_received_internal_isr_callback(packet, length, framePendingSetInTxAck);
  emPhyThisCallbackContext = PHY_THIS_UNSPECIFIED;
}

WEAK(EmPhyDcMode sli_802154phy_dc_state_callback(uint8_t macPgChan,
                                                 EmPhyDcMode curMode,
                                                 EmPhyDcState oldState,
                                                 EmPhyDcState newState))
{
  return EM_PHY_DC_MODE_UNCHANGED;
}

EmPhyDcMode sli_phy0_sli_802154phy_dc_state_callback(uint8_t macPgChan,
                                                     EmPhyDcMode curMode,
                                                     EmPhyDcState oldState,
                                                     EmPhyDcState newState)
{
  EmPhyDcMode dcMode;
  // Tolerate possibility this is called from within an active callback
  if (emPhyThisCallbackContext == PHY_THIS_UNSPECIFIED) {
    emPhyThisCallbackContext = PHY_THIS_NATIVE;
    dcMode = sli_802154phy_dc_state_callback(macPgChan, curMode, oldState, newState);
    emPhyThisCallbackContext = PHY_THIS_UNSPECIFIED;
  } else {
    dcMode = sli_802154phy_dc_state_callback(macPgChan, curMode, oldState, newState);
  }
  return dcMode;
}

EmPhyDcMode sli_phy1_sli_802154phy_dc_state_callback(uint8_t macPgChan,
                                                     EmPhyDcMode curMode,
                                                     EmPhyDcState oldState,
                                                     EmPhyDcState newState)
{
  EmPhyDcMode dcMode;
  // Tolerate possibility this is called from within an active callback
  if (emPhyThisCallbackContext == PHY_THIS_UNSPECIFIED) {
    emPhyThisCallbackContext = PHY_THIS_PRO2PLUS;
    dcMode = sli_802154phy_dc_state_callback(macPgChan, curMode, oldState, newState);
    emPhyThisCallbackContext = PHY_THIS_UNSPECIFIED;
  } else {
    dcMode = sli_802154phy_dc_state_callback(macPgChan, curMode, oldState, newState);
  }
  return dcMode;
}

WEAK(void emRadioSfdRecvIsrCallback(void)) {
}

void sli_phy0_emRadioSfdRecvIsrCallback(void)
{
  PHY_ASSERT(emPhyThisCallbackContext == PHY_THIS_UNSPECIFIED);
  emPhyThisCallbackContext = PHY_THIS_NATIVE;
  emRadioSfdRecvIsrCallback();
  emPhyThisCallbackContext = PHY_THIS_UNSPECIFIED;
}

void sli_phy1_emRadioSfdRecvIsrCallback(void)
{
  PHY_ASSERT(emPhyThisCallbackContext == PHY_THIS_UNSPECIFIED);
  emPhyThisCallbackContext = PHY_THIS_PRO2PLUS;
  emRadioSfdRecvIsrCallback();
  emPhyThisCallbackContext = PHY_THIS_UNSPECIFIED;
}

void sli_phy0_sli_802154phy_radio_transmit_complete_callback(sl_status_t status,
                                                             uint32_t sfdSentTime,
                                                             uint8_t framePending)
{
  if (emPhyThisCallbackContext != PHY_THIS_UNSPECIFIED) {
    return; // Ignore re-entrant callback
  }
  emPhyThisCallbackContext = PHY_THIS_NATIVE;
  muxTransmitCompleteCallback(status, sfdSentTime, framePending);
  emPhyThisCallbackContext = PHY_THIS_UNSPECIFIED;
}

void sli_phy1_sli_802154phy_radio_transmit_complete_callback(sl_status_t status,
                                                             uint32_t sfdSentTime,
                                                             uint8_t framePending)
{
  if (emPhyThisCallbackContext != PHY_THIS_UNSPECIFIED) {
    return; // Ignore re-entrant callback
  }
  emPhyThisCallbackContext = PHY_THIS_PRO2PLUS;
  muxTransmitCompleteCallback(status, sfdSentTime, framePending);
  emPhyThisCallbackContext = PHY_THIS_UNSPECIFIED;
}

void sli_phy0_sli_802154phy_radio_tx_ack_isr_callback(void)
{
  if (emPhyThisCallbackContext == PHY_THIS_UNSPECIFIED) {
    emPhyThisCallbackContext = PHY_THIS_NATIVE;
    sli_802154phy_radio_tx_ack_isr_callback();
    emPhyThisCallbackContext = PHY_THIS_UNSPECIFIED;
  } else if (emPhyThisCallbackContext == PHY_THIS_NATIVE) {
    // No-op - phytest cleanup from Tx complete's sli_802154phy_radio_ok_to_idle()
  } else {
    PHY_ASSERT(false);
  }
}

void sli_phy1_sli_802154phy_radio_tx_ack_isr_callback(void)
{
  if (emPhyThisCallbackContext == PHY_THIS_UNSPECIFIED) {
    emPhyThisCallbackContext = PHY_THIS_PRO2PLUS;
    sli_802154phy_radio_tx_ack_isr_callback();
    emPhyThisCallbackContext = PHY_THIS_UNSPECIFIED;
  } else if (emPhyThisCallbackContext == PHY_THIS_NATIVE) {
    // No-op - phytest cleanup from Tx complete's sli_802154phy_radio_ok_to_idle()
  } else {
    PHY_ASSERT(false);
  }
}

#endif//DUAL_PHY_1MAC
