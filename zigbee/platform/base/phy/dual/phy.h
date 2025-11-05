/***************************************************************************//**
 * @file
 * @brief DUAL-PHY machinations
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
#ifndef __PHY_DUAL_PHY_H__

#define PHY_THIS_UNSPECIFIED 0
#define PHY_THIS_NATIVE      1
#define PHY_THIS_PRO2PLUS    2
#define PHY_THIS_BOTH        3

#define PHY_DUAL_CACHE_SIZE  4 // Number of entries in dest addr cache

#if      (PHY_THIS == PHY_THIS_UNSPECIFIED)

// Choose the correct host PHY definitions based on MICRO
 #if defined(CORTEXM3_EFR32_MICRO)
   #if     PHY_DUALRAIL
    #define  PHY_RAIL 1
   #else//!PHY_DUALRAIL
    #define  PHY_EFR32 1
   #endif//PHY_DUALRAIL
 #elif defined(SL_ZIGBEE_TEST)
  #define  PHY_EM250 1
 #else
  #error "DUAL PHY unsupported on this platform"
 #endif

#define DUAL_PASTE(a, b)      a##b
#define DUAL_EVAL2(a, b)      DUAL_PASTE(a, b)

// DUAL_DCL(var) declares both PHYs' vars
#define DUAL_DCL(var)   DUAL_EVAL2(sli_phy0_, var), DUAL_EVAL2(sli_phy1_, var)
// CBG(var) gets the Callback context PHY's value
#define DUAL_CBG(var)   ((emPhyThisCallbackContext == PHY_THIS_NATIVE) \
                         ? DUAL_EVAL2(sli_phy0_, var)                  \
                         : DUAL_EVAL2(sli_phy1_, var))
// DUAL_CBS(var,type) sets the Callback context PHY's value if in CB context,
// otherwise act like DUAL_SET() in normal context
#define DUAL_CBS(var, type) (*(((emPhyThisCallbackContext & PHY_THIS_NATIVE)           \
                                || ((emPhyThisCallbackContext == PHY_THIS_UNSPECIFIED) \
                                    && (emPhyThis & PHY_THIS_NATIVE)))                 \
                               ? &DUAL_EVAL2(sli_phy0_, var)                           \
                               : (type*)&emPhyDummy)) =                                \
  (*(((emPhyThisCallbackContext & PHY_THIS_PRO2PLUS)                                   \
      || ((emPhyThisCallbackContext == PHY_THIS_UNSPECIFIED)                           \
          && (emPhyThis & PHY_THIS_PRO2PLUS)))                                         \
     ? &DUAL_EVAL2(sli_phy1_, var)                                                     \
     : (type*)&emPhyDummy))
// GET(var) gets the active PHYs' value(s), summed
#define DUAL_GET(var)   (((emPhyThis & PHY_THIS_NATIVE)     \
                          ? DUAL_EVAL2(sli_phy0_, var)      \
                          : 0)                              \
                         + ((emPhyThis & PHY_THIS_PRO2PLUS) \
                            ? DUAL_EVAL2(sli_phy1_, var)    \
                            : 0))
// GET_AND(var) gets the active PHYs' value(s), ANDed
#define DUAL_GET_AND(var)   (((emPhyThis & PHY_THIS_NATIVE)      \
                              ? DUAL_EVAL2(sli_phy0_, var)       \
                              : true)                            \
                             && ((emPhyThis & PHY_THIS_PRO2PLUS) \
                                 ? DUAL_EVAL2(sli_phy1_, var)    \
                                 : true))
// INC(var) increments the active PHYs' value(s)
#define DUAL_INC(var)   (((emPhyThis & PHY_THIS_NATIVE)   \
                          ? DUAL_EVAL2(sli_phy0_, var)++  \
                          : 0),                           \
                         ((emPhyThis & PHY_THIS_PRO2PLUS) \
                          ? DUAL_EVAL2(sli_phy1_, var)++  \
                          : 0))
// DUAL_SET(var,type) sets the active PHYs' var(s) of type
// Unfortuantely not all compilers support typeof(var) requiring type arg
#define DUAL_SET(var, type) (*((emPhyThis & PHY_THIS_NATIVE) \
                               ? &DUAL_EVAL2(sli_phy0_, var) \
                               : (type*)&emPhyDummy)) =      \
  (*((emPhyThis & PHY_THIS_PRO2PLUS)                         \
     ? &DUAL_EVAL2(sli_phy1_, var)                           \
     : (type*)&emPhyDummy))

extern uint64_t emPhyDummy; // Dummy cell for DUAL_SET() to work
extern uint8_t emPhyThis;
extern uint8_t emPhyThisCallbackContext;
extern sl_status_t emPhySetThisPhy(uint8_t thisPhy);
extern uint8_t emPhyGetThisPhy(void);

#define radioTransmitConfig sli_phy0_radioTransmitConfig // Map to Native
#define emPhyCurrentBandConfig sli_phy1_emPhyCurrentBandConfig // Map to Pro2plus

#elif    (PHY_THIS == PHY_THIS_NATIVE)

//----------------
// phy/em3xx/phy.o [em3xx only]
//----------------
// Stet _disableBasePri // [ref]
// Stet _writeBasePri // [ref]
// Stet abs // [ref]
// Stet calAdcTempOffset // [static]
// Stet calAdcTempOffsetValid // [static]
// Stet calDisableAdc // [ref]
// Stet calReadAdcBlocking // [ref]
// Stet calStartAdcConversion // [ref]
// Stet cancelPtaRxRequest // [static]
// Stet convertAdcValueToTemp // [static]
// Stet currentReceiveChannel // [static]
// Stet sli_zigbee_aes_encrypt // [ref]
// Stet sli_mac_child_index // [ref] MAC layer
// Stet sli_zigbee_child_status // [ref] MAC layer
#define emDroppedAckStallWorkaround sli_phy0_emDroppedAckStallWorkaround // [ref]
// Stet emFinishAdcTempReading // [code] shared with other PHYs
// Stet sli_mac_get_current_radio_network_index // [ref] MAC layer
// Stet sli_802154phy_get_mac_timer // [code] shared with other PHYs
// Stet emGetTempFromAdc // [code] shared with other PHYs
// Stet emipstack232count // [static]
// Stet emLongIdDataPendingForDevice // [ref] MAC layer
#define sli_802154mac_ack_needed sli_phy0_sli_802154mac_ack_needed // [ref]
#define sli_zigbee_packet_received_internal_isr_callback sli_phy0_sli_zigbee_packet_received_internal_isr_callback // [ref]
#define sli_802154phy_radio_auto_ack_enabled sli_phy0_sli_802154phy_radio_auto_ack_enabled // [ref]
#define emRadioBootPhySpecific sli_phy0_emRadioBootPhySpecific // [code]
#define emRadioChannel sli_phy0_emRadioChannel // [ref]
#define sli_802154phy_radio_check_radio sli_phy0_sli_802154phy_radio_check_radio // [code]
// Stet emRadioConfigScheduledCallbackEnabled // [code] shared with other PHYs
#define sli_802154mac_radio_enable_auto_ack sli_phy0_sli_802154mac_radio_enable_auto_ack // [ref]
// Stet emRadioEnableConfigScheduledCallback // [code] shared with other PHYs
#define emRadioEnableReceiveCompleteInt sli_phy0_emRadioEnableReceiveCompleteInt // [code]
#define emRadioEnableSfdRecvNotification sli_phy0_emRadioEnableSfdRecvNotification // [code]
#define emRadioGetEdCcaThreshold sli_phy0_emRadioGetEdCcaThreshold // [ref]
#define sli_802154phy_radio_get_idle_mode sli_phy0_sli_802154phy_radio_get_idle_mode // [ref]
// Stet sli_802154phy_radio_get_random_numbers // [code] shared with other PHYs
#define emRadioHoldOffIsActive sli_phy0_emRadioHoldOffIsActive // [code]
#define emRadioHoldOffIsr sli_phy0_emRadioHoldOffIsr // [code]
#define emRadioIsReceivingFrame sli_phy0_emRadioIsReceivingFrame // [code]
#define emRadioLastOutgoingCommand sli_phy0_emRadioLastOutgoingCommand // [ref] MAC layer
#define emRadioReceiveCrcEnabled sli_phy0_emRadioReceiveCrcEnabled // [ref]
#define emRadioReceiveCompleteIntEnabled sli_phy0_emRadioReceiveCompleteIntEnabled // [code]
#define emRadioReceiveCompleteIntPending sli_phy0_emRadioReceiveCompleteIntPending // [code]
// Stet emRadioReceiveIsr // [code] NVIC ISR
#define emRadioResetAndEnableReceive sli_phy0_emRadioResetAndEnableReceive // [code]
#define emRadioSetEdCcaThreshold sli_phy0_emRadioSetEdCcaThreshold // [ref]
#define emRadioSfdRecvIsrCallback sli_phy0_emRadioSfdRecvIsrCallback // [weak code]
#define emRadioSfdRecvNotificationEnabled sli_phy0_emRadioSfdRecvNotificationEnabled // [code]
#define sli_802154phy_radio_transmit_complete_callback sli_phy0_sli_802154phy_radio_transmit_complete_callback // [ref] MAC layer
// Stet emRadioTruncateIncomingAckToPoll // [var] test code only
#define sli_802154phy_radio_tx_ack_isr_callback sli_phy0_sli_802154phy_radio_tx_ack_isr_callback // [ref] MAC layer
#define emRxCRCFail sli_phy0_emRxCRCFail // [var]
#define emRxOverflow sli_phy0_emRxOverflow // [var]
#define emRxPacket sli_phy0_emRxPacket // [var]
// Stet emStartAdcTempReading // [code] shared with other PHYs
// Stet framePendingSetInOutgoingAck // [static var]
// Stet halInternalGetMfgTokenData // [ref]
// Stet halPtaIsEnabled // [ref]
// Stet halPtaSetRxRequest // [ref]
// Stet halStackCancelSymbolDelay // [ref]
// Stet halStackOrderSymbolDelay // [ref]
#define lastTxSfdTime sli_phy0_lastTxSfdTime // [ref]
// Stet previousTemperature // [static var]
// Stet previousTemperatureAvailable // [static var]
// Stet processRxAddressReceiveCompleteIsr // [static code]
// Stet processRxPacketCompleteIsr // [static code]
#define radioCurrentPowerStatus sli_phy0_radioCurrentPowerStatus // [ref]
// Stet rhoActive // [static var]
// Stet rxBuffer // [static var]
// Stet sfdRecvNotificationEnabled // [static var]
// Stet timeoutPtaRxRequest // [static code]
// Stet TOKEN_MFG_TEMP_CAL // [ref]

//--------------------------
// phy/em250class/analogue.o [em3xx only]
//--------------------------
// Stet boostTxPowerMax // [static const]
// Stet calDisableAdc // [ref]
#define sl_802154_calibrate_current_channel sli_phy0_sl_802154_calibrate_current_channel // [code]
void    sli_phy0_sl_802154_calibrate_current_channel(void); // Shadow stack-info.h
// Stet emberCalibrateVref // [code] shared with HAL and other PHYs
#define sli_802154_stack_set_tx_power_mode sli_phy0_sli_802154_stack_set_tx_power_mode // [code]
sl_status_t sli_phy0_sli_802154_stack_set_tx_power_mode(uint16_t txPowerMode); // Shadow stack-info.h
#define emChannelSettings sli_phy0_emChannelSettings // [code]
// Stet emFinishAdcTempReading // [ref] shared with other PHYs
#define emFinishCalibrateCurrentChannel sli_phy0_emFinishCalibrateCurrentChannel // [code]
#define emFinishRadioSetChannel sli_phy0_emFinishRadioSetChannel // [code]
#define sli_802154phy_get_phy_radio_channel sli_phy0_sli_802154phy_get_phy_radio_channel // [code]
#define sli_802154phy_get_phy_radio_power sli_phy0_sli_802154phy_get_phy_radio_power // [code]
#define emPhyGetChannelFreqHz sli_phy0_emPhyGetChannelFreqHz // [code]
#define sli_802154phy_get_channel_page_for_channel sli_phy0_sli_802154phy_get_channel_page_for_channel // [code]
#define sli_802154phy_get_channel_page_in_use sli_phy0_sli_802154phy_get_channel_page_in_use // [code]
#define sli_802154phy_get_phy_channel sli_phy0_sli_802154phy_get_phy_channel // [code]
#define emRadioChannel sli_phy0_emRadioChannel // [var]
#define emRadioEnablePacketTrace sli_phy0_emRadioEnablePacketTrace // [ref]
#define sli_802154phy_radio_get_power_status sli_phy0_sli_802154phy_radio_get_power_status // [ref]
#define emRadioPower sli_phy0_emRadioPower // [var]
// Stet emRadioReadSerialReg // [code] unique to em3xx, ref by mfglib & tests
#define emRadioResetAndEnableReceive sli_phy0_emRadioResetAndEnableReceive // [ref]
#define emRadioSetChannelAndForceCalibration sli_phy0_emRadioSetChannelAndForceCalibration // [code]
#define sli_802154phy_radio_sleep sli_phy0_sli_802154phy_radio_sleep // [ref]
#define emRadioTxPowerMode sli_phy0_emRadioTxPowerMode // [var]
// Stet emRadioWriteSerialReg // [code] unique to em3xx, ref by mfglib & tests
// Stet emSerialRegAddress // [code] unique to em3xx, ref by mfglib & tests
#define sli_802154phy_set_phy_radio_channel sli_phy0_sli_802154phy_set_phy_radio_channel // [code]
#define sli_802154phy_set_phy_radio_power sli_phy0_sli_802154phy_set_phy_radio_power // [code]
// Stet emStartAdcTempReading // [ref] shared with other PHYs
#define emStartCalibrateCurrentChannel sli_phy0_emStartCalibrateCurrentChannel // [code]
#define emStartRadioSetChannel sli_phy0_emStartRadioSetChannel // [code]
// Stet emSynthFreqOffset // [var] supported only on native, ref by tests
// Stet emTxStepTimeAltRfPath // [static var]
// Stet filterForceReCal // [var] unique to em3xx, documented for app/stack ref
// Stet halCommonBlockUntilXtal // [ref]
// Stet halCommonDelayMicroseconds // [ref]
// Stet halInternalGetMfgTokenData // [ref]
// Stet halInternalGetTokenData // [ref]
// Stet halInternalSetTokenData // [ref]
// Stet paPresent // [static var]
// Stet powerLevelSettings // [static const]
#define radioCurrentPowerStatus sli_phy0_radioCurrentPowerStatus // [ref]
#define reCalibrateVref sli_phy0_reCalibrateVref // [var] //@TODO: meke static
// Stet serialRegData // [static var]
// Stet setTxStepTime // [static code]
// Stet TOKEN_MFG_ANALOG_TRIM_BOOST // [ref]
// Stet TOKEN_MFG_ANALOG_TRIM_BOTH // [ref]
// Stet TOKEN_MFG_ANALOG_TRIM_NORMAL // [ref]
// Stet TOKEN_MFG_PHY_CONFIG // [ref] //@TODO: Need separate PHY_CONFIG?!
// Stet txPower // [static const]

//----------------------------
// phy/em250class/phy-common.o [em3xx only]
//----------------------------
// Stet basePriIsDisabled // [ref]
// Stet disableBasePri // [ref]
// Stet enableBasePri // [ref]
// Stet writeBasePri // [ref]
// Stet addressMatchingEnabled // [static var]
// Stet asFoundAnEnTest // [static var]
// Stet asFoundFixedCodeEn // [static var]
// Stet asFoundModEn // [static var]
// Stet autoAckEnabled // [static var]
// Stet deviceIsCoordinator // [static var]
// Stet edCcaThreshold // [static var]
#define sli_802154_stack_radio_get_scheduler_priorities sli_phy0_sli_802154_stack_radio_get_scheduler_priorities // [code]
#define sli_802154_stack_radio_set_scheduler_priorities sli_phy0_sli_802154_stack_radio_set_scheduler_priorities // [code]
#define sli_802154_stack_radio_set_scheduler_sliptime sli_phy0_sli_802154_stack_radio_set_scheduler_sliptime // [code]
#define sli_802154_stack_radio_get_scheduler_sliptime sli_phy0_sli_802154_stack_radio_get_scheduler_sliptime // [code]

#define sli_802154_stack_set_tx_power_mode sli_phy0_sli_802154_stack_set_tx_power_mode // [ref]
#define sli_802154phy_cost_division sli_phy0_sli_802154phy_cost_division // [code]
// Stet sli_802154phy_cost_divisions // [static code]
#define emDroppedAckStallWorkaround sli_phy0_emDroppedAckStallWorkaround // [code]
#define emFinishRadioSetChannel sli_phy0_emFinishRadioSetChannel // [ref]
// Stet sli_802154phy_get_mac_timer // [ref] shared with other PHYs
#define emGetTxPowerMode sli_phy0_emGetTxPowerMode // [code]
// Stet sli_zigbee_init_eui64 // [ref] common util
// Stet sli_802154mac_local_eui64 // [ref]
// Stet emLocalRadioNodeId // [static var]
// Stet emLocalRadioPanId // [static var]
#define sli_802154mac_ack_needed sli_phy0_sli_802154mac_ack_needed // [var]
// Stet sli_legacy_mfglib_mode // [var] shared with other PHYs
#define sli_legacy_mfglib_transmit_complete sli_phy0_sli_legacy_mfglib_transmit_complete // [var]
#define emPhyCancelTransmit sli_phy0_emPhyCancelTransmit // [code]
#define emPhyDeviceIsCoordinator sli_phy0_emPhyDeviceIsCoordinator // [code]
#define sli_802154phy_get_phy_channel sli_phy0_sli_802154phy_get_phy_channel // [code]
#define emPhyIsTransmitting sli_phy0_emPhyIsTransmitting // [code]
#define sli_802154phy_set_coordinator sli_phy0_sli_802154phy_set_coordinator // [code]
#define sli_802154phy_set_eui64 sli_phy0_sli_802154phy_set_eui64 // [code]
#define emPhySymbolsToUs sli_phy0_emPhySymbolsToUs // [code]
#define emPhyTick sli_phy0_emPhyTick // [code]
#define emRadioAddressMatchingEnabled sli_phy0_emRadioAddressMatchingEnabled // [code]
#define emRadioAlwaysUseZeroBackoff sli_phy0_emRadioAlwaysUseZeroBackoff // [var]
#define sli_802154phy_radio_auto_ack_enabled sli_phy0_sli_802154phy_radio_auto_ack_enabled // [code]
#define emRadioBootPhySpecific sli_phy0_emRadioBootPhySpecific // [ref]
#define sli_802154phy_radio_calculate_link_quality sli_phy0_sli_802154phy_radio_calculate_link_quality // [code]
#define emRadioChannel sli_phy0_emRadioChannel // [ref]
#define sli_802154phy_radio_convert_rssi_to_ed sli_phy0_sli_802154phy_radio_convert_rssi_to_ed // [code]
#define emRadioEnableAddressMatching sli_phy0_emRadioEnableAddressMatching // [code]
#define sli_802154mac_radio_enable_auto_ack sli_phy0_sli_802154mac_radio_enable_auto_ack // [code]
#define emRadioEnableOverflowNotification sli_phy0_emRadioEnableOverflowNotification // [code]
#define emRadioEnablePacketTrace sli_phy0_emRadioEnablePacketTrace // [code]
#define emRadioEnablePta sli_phy0_emRadioEnablePta // [code]
#define emRadioEnableReceiveCrc sli_phy0_emRadioEnableReceiveCrc // [code]
#define emRadioEnableSfdSentNotification sli_phy0_emRadioEnableSfdSentNotification // [code]
#define sli_802154phy_radio_energy_detection sli_phy0_sli_802154phy_radio_energy_detection // [code]
#define emRadioGetCca sli_phy0_emRadioGetCca // [code]
#define emRadioGetEdCcaThreshold sli_phy0_emRadioGetEdCcaThreshold // [code]
#define sli_802154phy_radio_get_idle_mode sli_phy0_sli_802154phy_radio_get_idle_mode // [code]
#define sli_802154mac_radio_get_node_id sli_phy0_sli_802154mac_radio_get_node_id // [code]
#define sli_802154phy_radio_get_pan_id sli_phy0_sli_802154phy_radio_get_pan_id // [code]
#define sli_802154phy_radio_get_power_status sli_phy0_sli_802154phy_radio_get_power_status // [code]
#define emRadioHoldOffIsActive sli_phy0_emRadioHoldOffIsActive // [ref]
#define sli_802154phy_radio_init sli_phy0_sli_802154phy_radio_init // [code]
#define emRadioLastOutgoingCommand sli_phy0_emRadioLastOutgoingCommand // [var] MAC layer
// Stet emRadioMacTimerDelayExpired // [static var]
#define emRadioManagePollTxRxTurnaround sli_phy0_emRadioManagePollTxRxTurnaround // [var]
#define sli_802154phy_radio_ok_to_idle sli_phy0_sli_802154phy_radio_ok_to_idle // [code]
// Stet emRadioOverflowCallbackEnabled // [static var]
#define emRadioOverflowNotificationEnabled sli_phy0_emRadioOverflowNotificationEnabled // [code]
#define sli_802154phy_radio_packet_trace_enabled sli_phy0_sli_802154phy_radio_packet_trace_enabled // [code]
#define emRadioPower sli_phy0_emRadioPower // [ref]
#define emRadioReceiveCrcEnabled sli_phy0_emRadioReceiveCrcEnabled // [code]
#define emRadioResetAndEnableReceive sli_phy0_emRadioResetAndEnableReceive // [ref]
#define emRadioSetEdCcaThreshold sli_phy0_emRadioSetEdCcaThreshold // [code]
#define sli_802154phy_radio_set_idle_mode sli_phy0_sli_802154phy_radio_set_idle_mode // [code]
#define sli_802154phy_radio_set_node_id sli_phy0_sli_802154phy_radio_set_node_id // [code]
#define sli_802154phy_radio_set_pan_id sli_phy0_sli_802154phy_radio_set_pan_id // [code]
#define emRadioSfdSentCallbackEnabled sli_phy0_emRadioSfdSentCallbackEnabled // [static var]
#define emRadioSfdSentNotificationEnabled sli_phy0_emRadioSfdSentNotificationEnabled // [code]
#define sli_802154phy_radio_sleep sli_phy0_sli_802154phy_radio_sleep // [code]
#define emRadioTransmit sli_phy0_emRadioTransmit // [code]
#define sli_802154phy_radio_transmit_complete_callback sli_phy0_sli_802154phy_radio_transmit_complete_callback // [ref]
#define emRadioTransmitInProgress sli_phy0_emRadioTransmitInProgress // [var]
// Stet emRadioTransmitIsr // [code] NVIC ISR
#define emRadioTxPowerMode sli_phy0_emRadioTxPowerMode // [ref]
#define sli_802154phy_radio_unpack_chip_errors sli_phy0_sli_802154phy_radio_unpack_chip_errors // [code]
#define sli_802154phy_radio_wake_up sli_phy0_sli_802154phy_radio_wake_up // [code]
#define sli_802154phy_set_phy_radio_power sli_phy0_sli_802154phy_set_phy_radio_power // [ref]
#define emStartRadioSetChannel sli_phy0_emStartRadioSetChannel // [ref]
#define sli_802154phy_start_transmit_stream sli_phy0_sli_802154phy_start_transmit_stream // [code]
#define sli_802154phy_start_transmit_tone sli_phy0_sli_802154phy_start_transmit_tone // [code]
#define sli_802154phy_stop_transmit_stream sli_phy0_sli_802154phy_stop_transmit_stream // [code]
#define sli_802154phy_stop_transmit_tone sli_phy0_sli_802154phy_stop_transmit_tone // [code]
// Stet emSynthFreqOffset // [ref] supported only on native, ref by tests
// Stet getRandomBackoff // [static code]
// Stet halCommonBlockUntilXtal // [ref]
// Stet halCommonDelayMicroseconds // [ref]
// Stet halCommonGetRandom // [ref]
// Stet halInternalAssertFailed // [ref]
// Stet halInternalGetMfgTokenData // [ref]
// Stet halInternalScheduleSysTickInMicroseconds // [ref]
// Stet halInternalSleep // [ref]
// Stet halPtaIsEnabled // [ref]
// Stet halSysTickIsr // [code]
#define lastTxSfdTime sli_phy0_lastTxSfdTime // [var]
// Stet managePollTxRxTurnaround [static code]
// Stet ptiEnabled // [static var]
#define radioCheckCrc sli_phy0_radioCheckCrc // [var]
#define radioCurrentPowerStatus sli_phy0_radioCurrentPowerStatus // [var] //@TODO: Remove QA test app refs to this!
// Stet radioIdlePowerState // [static var]
#define radioTransmitConfig sli_phy0_radioTransmitConfig // [ref]
// Stet TOKEN_MFG_CCA_THRESHOLD // [ref] //@TODO: Need separate PHY_CONFIG?!
// Stet TOKEN_MFG_SYNTH_FREQ_OFFSET // [ref]
// Stet txStepTimeSnapshot // [static var]

//------------------------------------------
// phy/plugin/duty-cycle/duty-cycle[-stub].o
//------------------------------------------
// Stet ??dcEvents // [static code]
// Stet _disableBasePri // [ref]
// Stet _writeBasePri // [ref]
// Stet dcAcksSent // [static var]
// Stet dcAcksSuspended // [static var]
// Stet dcCallbackActive // [static var]
// Stet dcCanSendPkt // [static code]
// Stet dcCheckStateUpdateMode // [static code]
// Stet dcCurChanDc // [static var]
// Stet dcCurMode // [static var]
// Stet dcCurPktOvhBytes // [static var]
// Stet dcCurState // [static var]
// Stet dcDutyCycles // [static var]
// Stet dcLimitAccel // [static var]
// Stet dcMaxAvailBytesForMode // [static code]
// Stet dcNodcHectoPct // [static var]
// Stet dcPktFailedOnAirBytes // [static var]
// Stet dcStateForConsumed // [static code]
// Stet dcTimeAccel // [static var]
// Stet dcTransmit // [static code]
// Stet dcTryPktNow // [static code]
#define dcTxDelaysMs sli_phy0_dcTxDelaysMs // [var]
// Stet dcTxLimitMs // [static var]
// Stet dcTxMode // [static var]
// Stet dcTxPacket // [static var]
// Stet dcUpdateBytes // [static code]
// Stet dcUpdateEntry // [static code]
// Stet sl_zigbee_run_events // [ref]
// Stet sli_event_control_set_active // [ref]
// Stet emEventControlSetDelayMS // [ref]
#define sli_802154phy_get_phy_radio_channel sli_phy0_sli_802154phy_get_phy_radio_channel // [ref]
// Stet sli_legacy_mfglib_mode // [ref] use native PHY's
#define sli_legacy_mfglib_transmit_complete sli_phy0_sli_legacy_mfglib_transmit_complete // [ref]
#define emPhyConfigEfr sli_phy0_emPhyConfigEfr // [ref] [efr32-only]
#define emPhyCurrentBandConfig sli_phy0_emPhyCurrentBandConfig // [ref]
#define emPhyDcAckSent sli_phy0_emPhyDcAckSent // [code]
#define emPhyDcBucketIndex sli_phy0_emPhyDcBucketIndex // [code]
#define emPhyDcEntry sli_phy0_emPhyDcEntry // [code]
// Stet emPhyDcExtendedStatus // [static var]
#define emPhyDcGetActualState sli_phy0_emPhyDcGetActualState // [code]
#define emPhyDcGetDcAndLimits sli_phy0_emPhyDcGetDcAndLimits // [code]
#define emPhyDcGetExtendedStatus sli_phy0_emPhyDcGetExtendedStatus // [code]
#define emPhyDcGetLimitsConfig sli_phy0_emPhyDcGetLimitsConfig // [code]
#define emPhyDcGetMode sli_phy0_emPhyDcGetMode // [code]
#define emPhyDcGetPktMode sli_phy0_emPhyDcGetPktMode // [code]
#define emPhyDcGetState sli_phy0_emPhyDcGetState // [code]
#define emPhyDcGetTimeAccel sli_phy0_emPhyDcGetTimeAccel // [code]
#define sli_802154phy_dc_send_pkt_asap sli_phy0_sli_802154phy_dc_send_pkt_asap // [code]
#define sli_802154phy_dc_send_pkt_failed sli_phy0_sli_802154phy_dc_send_pkt_failed // [code]
#define emPhyDcSetExtendedStatus sli_phy0_emPhyDcSetExtendedStatus // [code]
#define emPhyDcSetLimitsConfig sli_phy0_emPhyDcSetLimitsConfig // [code]
#define emPhyDcSetMode sli_phy0_emPhyDcSetMode // [code]
#define sli_802154phy_dc_set_time_accel sli_phy0_sli_802154phy_dc_set_time_accel // [code]
#define sli_802154phy_dc_state_callback sli_phy0_sli_802154phy_dc_state_callback // [weak code]
#define emPhyDcTick sli_phy0_emPhyDcTick // [code]
#define emPhyDcTxEvent sli_phy0_emPhyDcTxEvent // [var]
#define emPhyDcTxEventHandler sli_phy0_emPhyDcTxEventHandler // [code]
#define emPhyGetDutyCycleParams sli_phy0_emPhyGetDutyCycleParams // [ref]
#define emPhyGetLastTxCompleteTimeMs sli_phy0_emPhyGetLastTxCompleteTimeMs // [ref]
#define emRadioAlwaysUseZeroBackoff sli_phy0_emRadioAlwaysUseZeroBackoff // [ref]
#define sli_802154phy_radio_auto_ack_enabled sli_phy0_sli_802154phy_radio_auto_ack_enabled // [ref]
#define emRadioGetCca sli_phy0_emRadioGetCca // [ref]
#define emRadioHoldOffInternal sli_phy0_emRadioHoldOffInternal // [ref]
#define emRadioPrepareTransmit sli_phy0_emRadioPrepareTransmit // [weak code]
#define emRadioTransmit sli_phy0_emRadioTransmit // [ref]
#define sli_802154phy_radio_transmit_complete_callback sli_phy0_sli_802154phy_radio_transmit_complete_callback // [ref] MAC layer
#define sli_802154phy_radio_wake_up sli_phy0_sli_802154phy_radio_wake_up // [ref]
// Stet halCommonGetInt32uMillisecondTick // [ref]
// Stet halCommonGetRandom // [ref]
// Stet halInternalAssertFailed // [ref]
// Stet halStackGetInt32uSymbolTick // [ref]
// Stet halStackInt32uSymbolTickGTorEqual // [ref]
// Stet halStackOrderSymbolDelay // [ref]
// Stet lbtBackoffTimeSym // [static var]
// Stet lbtCcaEndTimeSymTick // [static var]
// Stet lbtFail // [static code]
// Stet lbtGetRandomBackoffSym // [static code]
// Stet lbtOverallEndTimeSymTick // [static var]
// Stet lbtScheduleBackoffEnd // [static code]
// Stet lbtState // [static var]
// Stet lbtTransmit // [static code]
// Stet lbtTries // [static var]
#define radioTransmitConfig sli_phy0_radioTransmitConfig // [ref]
// Stet symbolDelayCb // [static code]
// Stet transmitComplete // [static code]

//----------------
// phy/efr32/phy.o [efr32-only]
//----------------
// Stet ??curPaConfig // [static var]
// Stet _disableBasePri // [ref]
// Stet _writeBasePri // [ref]
// Stet ackTimeoutCb // [static code]
// Stet addressMatchingEnabled // [static var]
// Stet autoAckEnabled // [static var]
// Stet BUS_RegBitWrite // [static code]
// Stet cancelPtaRxRequest // [static code]
// Stet deviceIsCoordinator // [static var]
// Stet edCcaThreshold // [static var]
// Stet efr32WakeUp // [static code]
#define sl_802154_calibrate_current_channel sli_phy0_sl_802154_calibrate_current_channel // [code]
// Stet emberCalibrateVref // [code] shared with HAL and other PHYs
// Stet sli_mac_child_index // [ref] MAC layer
#define sli_802154_stack_radio_get_scheduler_priorities sli_phy0_sli_802154_stack_radio_get_scheduler_priorities // [code]
// Stet sli_zigbee_stack_radio_needs_calibrating_handler // [ref] MAC/APP layer
#define sli_802154_stack_radio_set_scheduler_priorities sli_phy0_sli_802154_stack_radio_set_scheduler_priorities // [code]
#define sli_802154_stack_radio_set_scheduler_sliptime sli_phy0_sli_802154_stack_radio_set_scheduler_sliptime // [code]
#define sli_802154_stack_radio_get_scheduler_sliptime sli_phy0_sli_802154_stack_radio_get_scheduler_sliptime // [code]
#define sli_802154_stack_set_tx_power_mode sli_phy0_sli_802154_stack_set_tx_power_mode // [code]
sl_status_t sli_phy0_sli_802154_stack_set_tx_power_mode(uint16_t txPowerMode); // Shadow stack-info.h
// Stet sli_zigbee_child_status // [ref] MAC layer
#define sli_802154phy_cost_division sli_phy0_sli_802154phy_cost_division // [code]
// Stet sli_802154phy_cost_divisions // [static var]
// Stet sli_mac_get_current_radio_network_index // [ref] MAC layer
// Stet sli_802154phy_get_mac_timer // [code] shared with other PHYs
#define sli_802154phy_get_phy_radio_channel sli_phy0_sli_802154phy_get_phy_radio_channel // [code]
#define sli_802154phy_get_phy_radio_power sli_phy0_sli_802154phy_get_phy_radio_power // [code]
// Stat sli_802154phy_get_phy_radio_powerFl // [code] unique to efr32, ref by tests
#define emGetTxPowerMode sli_phy0_emGetTxPowerMode // [code]
// Stet sli_zigbee_init_eui64 // [ref] common util
// Stet sli_802154mac_local_eui64 // [ref]
// Stet emLocalRadioNodeId // [static var]
// Stet emLocalRadioPanId // [static var]
// Stet emLongIdDataPendingForDevice // [ref] MAC layer
// Stet sli_legacy_mfglib_mode // [var] shared with other PHYs
#define sli_legacy_mfglib_transmit_complete sli_phy0_sli_legacy_mfglib_transmit_complete // [var]
#define sli_zigbee_packet_received_internal_isr_callback sli_phy0_sli_zigbee_packet_received_internal_isr_callback // [ref]
#define emPhyCancelTransmit sli_phy0_emPhyCancelTransmit // [code]
#define emPhyCcaCsmaConfigArray sli_phy0_emPhyCcaCsmaConfigArray // [const]
#define emPhyCcaLbtConfigArray sli_phy0_emPhyCcaLbtConfigArray // [code/const]
#define emPhyCurrentBandConfig sli_phy0_emPhyCurrentBandConfig // [var]
#define emPhyDcAckSent sli_phy0_emPhyDcAckSent // [ref]
#define emPhyDcGetPktMode sli_phy0_emPhyDcGetPktMode // [ref]
#define emPhyDcSetExtendedStatus sli_phy0_emPhyDcSetExtendedStatus // [ref]
#define emPhyDcTick sli_phy0_emPhyDcTick // [ref]
#define emPhyDeviceIsCoordinator sli_phy0_emPhyDeviceIsCoordinator // [code]
#define emPhyConfigEfr sli_phy0_emPhyConfigEfr // [ref] [efr32-only]
#define emPhyGetChannelFreqHz sli_phy0_emPhyGetChannelFreqHz // [code]
#define sli_802154phy_get_channel_page_for_channel sli_phy0_sli_802154phy_get_channel_page_for_channel // [code]
#define sli_802154phy_get_channel_page_in_use sli_phy0_sli_802154phy_get_channel_page_in_use // [code]
#define emPhyGetDutyCycleParams sli_phy0_emPhyGetDutyCycleParams // [code]
#define emPhyGetLastTxCompleteTimeMs sli_phy0_emPhyGetLastTxCompleteTimeMs // [code]
#define sli_802154phy_get_phy_channel sli_phy0_sli_802154phy_get_phy_channel // [code]
#define emPhyIsTransmitting sli_phy0_emPhyIsTransmitting // [code]
#define sli_802154phy_set_coordinator sli_phy0_sli_802154phy_set_coordinator // [code]
#define sli_802154phy_set_eui64 sli_phy0_sli_802154phy_set_eui64 // [code]
#define emPhySymbolsToUs sli_phy0_emPhySymbolsToUs // [code]
#define emPhyTick sli_phy0_emPhyTick // [code]
#define emRadioAddressMatchingEnabled sli_phy0_emRadioAddressMatchingEnabled // [code]
#define emRadioAlwaysUseZeroBackoff sli_phy0_emRadioAlwaysUseZeroBackoff // [var]
#define sli_802154phy_radio_auto_ack_enabled sli_phy0_sli_802154phy_radio_auto_ack_enabled // [code]
#define sli_802154phy_radio_calculate_link_quality sli_phy0_sli_802154phy_radio_calculate_link_quality // [code]
#define emRadioChannel sli_phy0_emRadioChannel // [static var] only static in efr32 but not em3xx
// Stet emRadioChannelPageInUse // [static var]
#define sli_802154phy_radio_check_radio sli_phy0_sli_802154phy_radio_check_radio // [code]
// Stet emRadioConfigScheduledCallbackEnabled // [code] shared with other PHYs
#define sli_802154phy_radio_convert_rssi_to_ed sli_phy0_sli_802154phy_radio_convert_rssi_to_ed // [code]
#define emRadioEnableAddressMatching sli_phy0_emRadioEnableAddressMatching // [code]
#define sli_802154mac_radio_enable_auto_ack sli_phy0_sli_802154mac_radio_enable_auto_ack // [code]
// Stet emRadioEnableConfigScheduledCallback // [code] shared with other PHYs
#define emRadioEnableOverflowNotification sli_phy0_emRadioEnableOverflowNotification // [code]
#define emRadioEnablePacketTrace sli_phy0_emRadioEnablePacketTrace // [code]
#define emRadioEnablePta sli_phy0_emRadioEnablePta // [code]
#define emRadioEnableReceiveCompleteInt sli_phy0_emRadioEnableReceiveCompleteInt // [code]
#define emRadioEnableReceiveCrc sli_phy0_emRadioEnableReceiveCrc // [code]
#define emRadioEnableSfdRecvNotification sli_phy0_emRadioEnableSfdRecvNotification // [code]
#define emRadioEnableSfdSentNotification sli_phy0_emRadioEnableSfdSentNotification // [code]
#define sli_802154phy_radio_energy_detection sli_phy0_sli_802154phy_radio_energy_detection // [code]
#define emRadioGetCca sli_phy0_emRadioGetCca // [code]
#define emRadioGetEdCcaThreshold sli_phy0_emRadioGetEdCcaThreshold // [code]
#define sli_802154phy_radio_get_idle_mode sli_phy0_sli_802154phy_radio_get_idle_mode // [code]
#define sli_802154mac_radio_get_node_id sli_phy0_sli_802154mac_radio_get_node_id // [code]
#define sli_802154phy_radio_get_pan_id sli_phy0_sli_802154phy_radio_get_pan_id // [code]
#define sli_802154phy_radio_get_power_status sli_phy0_sli_802154phy_radio_get_power_status // [code]
// Stet sli_802154phy_radio_get_random_numbers // [code] shared with other PHYs
#define emRadioHoldOffInternal sli_phy0_emRadioHoldOffInternal // [code]
// Stet emRadioHoldOffInternalIsr // [static code]
#define emRadioHoldOffIsActive sli_phy0_emRadioHoldOffIsActive // [code]
#define emRadioHoldOffIsr sli_phy0_emRadioHoldOffIsr // [code]
#define sli_802154phy_radio_init sli_phy0_sli_802154phy_radio_init // [code]
#define emRadioIsReceivingFrame sli_phy0_emRadioIsReceivingFrame // [code]
#define emRadioLastOutgoingCommand sli_phy0_emRadioLastOutgoingCommand // [var] MAC layer
// Stet emRadioMacTimerDelayExpired // [static var]
#define sli_802154phy_radio_ok_to_idle sli_phy0_sli_802154phy_radio_ok_to_idle // [code]
// Stet emRadioOverflowCallbackEnabled // [static var]
#define emRadioOverflowNotificationEnabled sli_phy0_emRadioOverflowNotificationEnabled // [code]
#define sli_802154phy_radio_packet_trace_enabled sli_phy0_sli_802154phy_radio_packet_trace_enabled // [code]
// Stet emRadioPowerFl // [static var]
#define emRadioPrepareTransmit sli_phy0_emRadioPrepareTransmit // [code]
#define emRadioReceiveCompleteIntEnabled sli_phy0_emRadioReceiveCompleteIntEnabled // [code]
#define emRadioReceiveCompleteIntPending sli_phy0_emRadioReceiveCompleteIntPending // [code]
#define emRadioReceiveCrcEnabled sli_phy0_emRadioReceiveCrcEnabled // [code]
#define emRadioSetChannelAndForceCalibration sli_phy0_emRadioSetChannelAndForceCalibration // [code]
#define emRadioSetEdCcaThreshold sli_phy0_emRadioSetEdCcaThreshold // [code]
#define sli_802154phy_radio_set_idle_mode sli_phy0_sli_802154phy_radio_set_idle_mode // [code]
#define sli_802154phy_radio_set_node_id sli_phy0_sli_802154phy_radio_set_node_id // [code]
#define sli_802154phy_radio_set_pan_id sli_phy0_sli_802154phy_radio_set_pan_id // [code]
// Stet emRadioSetPowerStatus // [static code]
#define emRadioSfdRecvIsrCallback sli_phy0_emRadioSfdRecvIsrCallback // [weak code]
#define emRadioSfdRecvNotificationEnabled sli_phy0_emRadioSfdRecvNotificationEnabled // [code]
#define emRadioSfdSentCallbackEnabled sli_phy0_emRadioSfdSentCallbackEnabled // [static var] only static in efr32 but not em3xx
#define emRadioSfdSentNotificationEnabled sli_phy0_emRadioSfdSentNotificationEnabled // [code]
#define sli_802154phy_radio_sleep sli_phy0_sli_802154phy_radio_sleep // [code]
#define emRadioTransmit sli_phy0_emRadioTransmit // [code]
#define sli_802154phy_radio_transmit_complete_callback sli_phy0_sli_802154phy_radio_transmit_complete_callback // [ref]
#define sli_802154phy_radio_tx_ack_isr_callback sli_phy0_sli_802154phy_radio_tx_ack_isr_callback // [code] MAC layer
#define emRadioTxPowerMode sli_phy0_emRadioTxPowerMode // [var]
#define sli_802154phy_radio_unpack_chip_errors sli_phy0_sli_802154phy_radio_unpack_chip_errors // [code]
#define sli_802154phy_radio_wake_up sli_phy0_sli_802154phy_radio_wake_up // [code]
#define emRxCRCFail sli_phy0_emRxCRCFail // [var]
#define emRxOverflow sli_phy0_emRxOverflow // [var]
#define emRxPacket sli_phy0_emRxPacket // [var]
// Stet emSetCostDivisions // [code] unique to efr32, ref by LQI plugin
#define sli_802154phy_set_phy_radio_channel sli_phy0_sli_802154phy_set_phy_radio_channel // [code]
#define sli_802154phy_set_phy_radio_power sli_phy0_sli_802154phy_set_phy_radio_power // [code]
// Stet sli_802154phy_set_phy_radio_powerFl // [code] unique to efr32, ref by tests
#define sli_802154phy_start_transmit_stream sli_phy0_sli_802154phy_start_transmit_stream // [code]
#define sli_802154phy_start_transmit_tone sli_phy0_sli_802154phy_start_transmit_tone // [code]
#define sli_802154phy_stop_transmit_stream sli_phy0_sli_802154phy_stop_transmit_stream // [code]
#define sli_802154phy_stop_transmit_tone sli_phy0_sli_802154phy_stop_transmit_tone // [code]
// Stet emSynthFreqOffset // [var] supported only on native, ref by tests
// Stet framePendingSetInOutgoingAck // [static var]
// Stet getGainValues // [ref] unique to efr32 in efrseq
// Stat getPaConfig // [static code]
// Stet halCommonGetInt32uMillisecondTick // [ref]
// Stet halInternalAssertFailed // [ref]
// Stet halInternalGetMfgTokenData // [ref]
// Stet halInternalPa2p4GHzInit // [ref]
// Stet halInternalPaSubGHzInit // [ref]
// Stet halInternalSleep // [ref]
// Stet halInternalStartSymbolTimer // [code] shared with other PHYs
// Stet halPtaIsEnabled // [ref]
// Stet halPtaSetRxRequest // [ref]
// Stet halPtaSetTxRequest // [ref]
// Stet halStackCancelSymbolDelay // [ref]
// Stet halStackOrderSymbolDelay // [ref]
// Stet halStackSymbolDelayIsr // [ref]
// Stet IEEE802154PHY_AGCCCAThresholdSet // [ref]
// Stet IEEE802154PHY_ChannelSet // [ref]
// Stet IEEE802154PHY_DataIndicationGet // [ref]
// Stet IEEE802154PHY_DataRequest // [ref]
// Stet IEEE802154PHY_EnableAutoAckSet // [ref]
// Stet IEEE802154PHY_FramePendingSet // [ref]
// Stet IEEE802154PHY_Init // [ref]
// Stet IEEE802154PHY_IsRACProcessingRxFrame // [ref]
// Stet IEEE802154PHY_LBTCfgSet // [ref]
// Stet IEEE802154PHY_LBTStop // [ref]
// Stet IEEE802154PHY_LocalIeeeAddrCfgSet // [ref]
// Stet IEEE802154PHY_LocalPanIdCfgSet // [ref]
// Stet IEEE802154PHY_LocalShortAddrCfgSet // [ref]
// Stet IEEE802154PHY_PanCoordinatorModeSet // [ref]
// Stet IEEE802154PHY_PHRLengthGet // [ref]
// Stet IEEE802154PHY_PROTIMERClockGet // [ref]
// Stet IEEE802154PHY_PROTIMERTimerStart // [ref]
// Stet IEEE802154PHY_PROTIMERTimerStop // [ref]
// Stet IEEE802154PHY_RACHandleFramePendingSEQIRQ // [code]
// Stet IEEE802154PHY_RACHandleTempcalSEQIRQ // [code]
// Stet IEEE802154PHY_RxSrcEnable // [ref]
// Stet lastCsmaTxFinishedTimeMs // [static var]
// Stet lastTxFinishedTimeMs // [static var]
#define lastTxSfdTime sli_phy0_lastTxSfdTime // [var]
// Stet MACSEQ_AckSentEvent // [code]
// Stet MACSEQ_DataIndicationEvent // [code]
// Stet MACSEQ_FrameErrorEvent // [code]
// Stet MACSEQ_GetNewBufferEvent // [code]
// Stet MACSEQ_LBTFailureEvent // [code]
// Stet MACSEQ_LBTPausedEvent // [code]
// Stet MACSEQ_LBTRetryEvent // [code]
// Stet MACSEQ_LBTSuccessEvent // [code]
// Stet MACSEQ_ProtocolTimerEvent // [code]
// Stet MACSEQ_RSSIDoneEvent // [code]
// Stet MACSEQ_RxAbortedEvent // [code]
// Stet MACSEQ_RxFrameDetected // [code]
// Stet MACSEQ_TOUT0MatchLBTEvent // [code]
// Stet MACSEQ_TransmitDoneEvent // [code]
// Stet MACSEQ_TransmitErrorEvent // [code]
// Stet mapMacPgChanToPhyChanBand // [static code]
// Stet NVIC_ClearPendingIRQ // [static code]
// Stet PA_OutputPowerSet // [ref]
// Stet phy2SubBandConfig // [static var]
// Stet phy2SubBandParams // [static var]
// Stet PhyConfig // [ref] unique to efr32 in efrseq
// Stet ptiEnabled // [static var]
// Stet RADIO_ClearDisableIRQ // [static code]
// Stet RADIO_ClearEnableIRQ // [stztic code]
// Stet RADIO_CLKEnable // [ref]
// Stet RADIO_MODEMRxFrameDetectDisable // [static code]
// Stet RADIO_PA_Init // [ref]
// Stet RADIO_PTI_Disable // [ref]
// Stet RADIO_PTI_Enable // [ref]
// Stet RADIO_RxBufferReset // [ref]
// Stet RADIO_RxTrailDataLength // [ref]
#define radioCheckCrc sli_phy0_radioCheckCrc // [static var] only static in efr32 but not em3xx
#define radioCurrentPowerStatus sli_phy0_radioCurrentPowerStatus // [var]
// Stet radioIdlePowerState // [static var]
// Stet radioInited // [static var]
#define radioTransmitConfig sli_phy0_radioTransmitConfig // [ref]
// Stet RFRAND_GetRadioEntropy // [ref]
// Stet RFTEST_RestoreRadioConfiguration // [ref]
// Stet RFTEST_SaveRadioConfiguration // [ref]
// Stet RFTEST_StartCwTx // [ref]
// Stet RFTEST_StartStreamTx // [ref]
// Stet RFTEST_StopTx // [ref]
// Stet rhoActive // [static var]
// Stet rxBuffer // [static var]
// Stet seqNo // [static var]
// Stet sfdRecvNotificationEnabled // [static var]
// Stet TEMPCAL_Perform // [ref]
// Stet thisTxConfig // [static var]
// Stet TOKEN_MFG_CCA_THRESHOLD // [ref] //@TODO: Need separate PHY_CONFIG?!
// Stet TOKEN_MFG_PHY_CONFIG // [ref] //@TODO: Need separate PHY_CONFIG?!
// Stet TOKEN_MFG_SYNTH_FREQ_OFFSET // [ref]
// Stet TxMsg // [static var]
// Stet txState // [static var]

//-------------------------------------------
// phy/pro2class/radio_config_Si4468_915MHz.o [efr32-only]
//-------------------------------------------
#define emPhyBandConfig915MHz sli_phy0_emPhyBandConfig915MHz // [const]
#define emPhyCcaLbtConfigArray sli_phy0_emPhyCcaLbtConfigArray // [ref]
// Stet gb915SbParams // [static const]
// Stet subBandConfigs // [static const]

//-------------------------------------------
// phy/pro2class/radio_config_Si4468_863MHz.o [efr32-only]
//-------------------------------------------
#define emPhyBandConfig863MHz sli_phy0_emPhyBandConfig863MHz // [const]
#define emPhyCcaLbtConfigArray sli_phy0_emPhyCcaLbtConfigArray // [ref]
// Stet gb868Sb1Params // [static const]
// Stet subBandConfigs // [static const]

//-------------------------------------------
// phy/pro2class/radio_config_em3xxx_2.4GHz.o [efr32-only]
//-------------------------------------------
#define emPhyBandConfig2400MHz sli_phy0_emPhyBandConfig2400MHz // [const]
// Stet subBandConfigs // [static const]
// Stet subBandParams // [static const]

//-----------------------
// phy/efr32/phy-config.o [efr32-only]
//-----------------------
#define emPhyBandConfig2400MHz sli_phy0_emPhyBandConfig2400MHz // [ref]
#define emPhyBandConfig863MHz sli_phy0_emPhyBandConfig863MHz // [ref]
#define emPhyBandConfig915MHz sli_phy0_emPhyBandConfig915MHz // [ref]
// Stet emPhyBands // [static var]
#define emPhyConfigEfr sli_phy0_emPhyConfigEfr // [var]

//-----------------------------------------
// EFR32-support:
// em_crypto.o
// ieee802154_annotated.o
// ieee802154_filtering.o
// ieee802154_legacyphyconfig.o
// ieee802154_gb868phyconfig.o
// ieee802154_MRFSKFECWhiteningModeSwitch.o
// ieee802154_phy.o
// ieee802154_SUNphyconfig.o
// ieee802154_configurator_out.o
// ieee802154_gb868_configurator_out.o
// pa.o
// pti.o
// radio.o
// if_adc.o
// phy_utils.o
// rf_test.o
// synth.o
// rf_rand.o
// protimer.o
// radio_cmu.o
// ir_cal.o
// aux_pll.o
// tempcal.o
//-----------------------------------------
// Stet all symbols
#ifdef PHY_SIMULATION_DUAL
#define simulatorResumeRadioActivity  sli_phy0_simulatorResumeRadioActivity
#define simulatorSuspendRadioActivity sli_phy0_simulatorSuspendRadioActivity
#endif  //PHY_SIMULATION_DUAL

#elif    (PHY_THIS == PHY_THIS_PRO2PLUS)

//--------------------
// phy/pro2plus/phy.o
//--------------------
// Stet ??paSels // [static code/const]
// Stet ??previousTemperature // [static var] [em3xx-only]
// Stet ??previousTemperatureAvailable // [static var] [em3xx-only]
// Stet ??prevModTypeMode // [static var]
// Stet _disableBasePri // [ref]
// Stet _writeBasePri // [ref]
// Stet abs // [ref] [em3xx-only]
// Stet bytesToWrite // [static var]
// Stet edCcaThreshold // [static var]
#define sl_802154_calibrate_current_channel sli_phy1_sl_802154_calibrate_current_channel // [code]
#define sli_802154_stack_radio_get_scheduler_priorities sli_phy1_sli_802154_stack_radio_get_scheduler_priorities // [code]
#define sli_802154_stack_radio_set_scheduler_priorities sli_phy1_sli_802154_stack_radio_set_scheduler_priorities // [code]
#define sli_802154_stack_radio_set_scheduler_sliptime sli_phy1_sli_802154_stack_radio_set_scheduler_sliptime // [code]
#define sli_802154_stack_radio_get_scheduler_sliptime sli_phy1_sli_802154_stack_radio_get_scheduler_sliptime // [code]
void    sli_phy1_sl_802154_calibrate_current_channel(void); // Shadow stack-info.h
// Stet sli_mac_child_index // [ref] MAC layer
// Stet sli_zigbee_child_status // [ref] MAC layer
#define sli_802154_stack_set_tx_power_mode sli_phy1_sli_802154_stack_set_tx_power_mode // [code]
sl_status_t sli_phy1_sli_802154_stack_set_tx_power_mode(uint16_t txPowerMode); // Shadow stack-info.h
#define sli_802154phy_cost_division sli_phy1_sli_802154phy_cost_division // [code]
// Stet sli_802154phy_cost_divisions // [static code]
// Stet sli_mac_get_current_radio_network_index // [ref] MAC layer
// Stet sli_802154phy_get_mac_timer // [ref] use native PHY's
#define sli_802154phy_get_phy_radio_channel sli_phy1_sli_802154phy_get_phy_radio_channel // [code]
#define sli_802154phy_get_phy_radio_power sli_phy1_sli_802154phy_get_phy_radio_power // [code]
// Stet emGetTempFromAdc // [ref] [em3xx-only] use native PHY's
#define emGetTxPowerMode sli_phy1_emGetTxPowerMode // [code]
// Stet sli_zigbee_init_eui64 // [ref] common util
// Stet sli_802154mac_local_eui64 // [ref]
// Stet emLongIdDataPendingForDevice // [ref] MAC layer
// Stet sli_legacy_mfglib_mode // [ref] use native PHY's
#define sli_legacy_mfglib_transmit_complete sli_phy1_sli_legacy_mfglib_transmit_complete // [var]
#define sli_zigbee_packet_received_internal_isr_callback sli_phy1_sli_zigbee_packet_received_internal_isr_callback // [ref]
#define emPhyCancelTransmit sli_phy1_emPhyCancelTransmit // [code]
#define emPhyCcaCsmaConfigArray sli_phy1_emPhyCcaCsmaConfigArray // [const]
#define emPhyCcaLbtConfigArray sli_phy1_emPhyCcaLbtConfigArray // [code/const]
// Stet emPhyConfigMHz // [ref] unique to pro2plus, used by tests
#define emPhyCurrentBandConfig sli_phy1_emPhyCurrentBandConfig // [var]
// Stet emPhyCurrentSubBandParams // [static var]
#define emPhyDcAckSent sli_phy1_emPhyDcAckSent // [ref]
#define emPhyDcGetPktMode sli_phy1_emPhyDcGetPktMode // [ref]
#define emPhyDcSetExtendedStatus sli_phy1_emPhyDcSetExtendedStatus // [ref]
#define emPhyDcTick sli_phy1_emPhyDcTick // [ref]
#define emPhyDeviceIsCoordinator sli_phy1_emPhyDeviceIsCoordinator // [code]
#define emPhyGetChannelFreqHz sli_phy1_emPhyGetChannelFreqHz // [code]
#define sli_802154phy_get_channel_page_for_channel sli_phy1_sli_802154phy_get_channel_page_for_channel // [code]
#define sli_802154phy_get_channel_page_in_use sli_phy1_sli_802154phy_get_channel_page_in_use // [code]
#define emPhyGetDutyCycleParams sli_phy1_emPhyGetDutyCycleParams // [code]
#define emPhyGetLastTxCompleteTimeMs sli_phy1_emPhyGetLastTxCompleteTimeMs // [code]
#define sli_802154phy_get_phy_channel sli_phy1_sli_802154phy_get_phy_channel // [code]
#define emPhyIsTransmitting sli_phy1_emPhyIsTransmitting // [code]
// Stet emPhyPhr154gFeatures // [var] unique to pro2plus, ref by tests
#define sli_802154phy_set_coordinator sli_phy1_sli_802154phy_set_coordinator // [code]
#define sli_802154phy_set_coordinatorOnNetwork sli_phy1_sli_802154phy_set_coordinatorOnNetwork // [code]
#define sli_802154phy_set_eui64 sli_phy1_sli_802154phy_set_eui64 // [code]
// Stet emPhySetPhyMode // [code] unique to pro2plus, ref by tests
#define emPhySetRadioChannelOnNetwork sli_phy1_emPhySetRadioChannelOnNetwork // [code]
#define emPhySetRadioPowerOnNetwork sli_phy1_emPhySetRadioPowerOnNetwork // [code]
#define emPhySymbolsToUs sli_phy1_emPhySymbolsToUs // [code]
#define emPhyTick sli_phy1_emPhyTick // [code]
// Stet emPhyTxState // [static var]
#define emRadioAddressMatchingEnabled sli_phy1_emRadioAddressMatchingEnabled // [code]
#define emRadioAlwaysUseZeroBackoff sli_phy1_emRadioAlwaysUseZeroBackoff // [var]
#define sli_802154phy_radio_auto_ack_enabled sli_phy1_sli_802154phy_radio_auto_ack_enabled // [code]
#define sli_802154phy_radio_calculate_link_quality sli_phy1_sli_802154phy_radio_calculate_link_quality // [code]
#define sli_802154phy_radio_check_radio sli_phy1_sli_802154phy_radio_check_radio // [code]
#define sli_802154phy_radio_convert_rssi_to_ed sli_phy1_sli_802154phy_radio_convert_rssi_to_ed // [code]
#define emRadioEnableAddressMatching sli_phy1_emRadioEnableAddressMatching // [code]
#define sli_802154mac_radio_enable_auto_ack sli_phy1_sli_802154mac_radio_enable_auto_ack // [code]
#define emRadioEnableOverflowNotification sli_phy1_emRadioEnableOverflowNotification // [code]
#define emRadioEnablePacketTrace sli_phy1_emRadioEnablePacketTrace // [code]
#define emRadioEnableReceiveCompleteInt sli_phy1_emRadioEnableReceiveCompleteInt // [code]
#define emRadioEnableReceiveCrc sli_phy1_emRadioEnableReceiveCrc // [code]
#define emRadioEnableSfdRecvNotification sli_phy1_emRadioEnableSfdRecvNotification // [code]
#define emRadioEnableSfdSentNotification sli_phy1_emRadioEnableSfdSentNotification // [code]
#define sli_802154phy_radio_energy_detection sli_phy1_sli_802154phy_radio_energy_detection // [code]
#define emRadioGetCca sli_phy1_emRadioGetCca // [code]
#define emRadioGetEdCcaThreshold sli_phy1_emRadioGetEdCcaThreshold // [code]
#define sli_802154phy_radio_get_idle_mode sli_phy1_sli_802154phy_radio_get_idle_mode // [code]
#define sli_802154mac_radio_get_node_id sli_phy1_sli_802154mac_radio_get_node_id // [code]
#define sli_802154phy_radio_get_pan_id sli_phy1_sli_802154phy_radio_get_pan_id // [code]
#define sli_802154phy_radio_get_power_status sli_phy1_sli_802154phy_radio_get_power_status // [code] use native PHY's
// Stet sli_802154phy_radio_get_random_numbers // [ref]
#define emRadioHoldOffInternal sli_phy1_emRadioHoldOffInternal // [code]
// Stet emRadioHoldOffInternalIsr // [static code]
#define emRadioHoldOffIsActive sli_phy1_emRadioHoldOffIsActive // [code]
#define emRadioHoldOffIsr sli_phy1_emRadioHoldOffIsr // [code]
#define sli_802154phy_radio_init sli_phy1_sli_802154phy_radio_init // [code]
#define emRadioIsReceivingFrame sli_phy1_emRadioIsReceivingFrame // [code]
#define emRadioLastOutgoingCommand sli_phy1_emRadioLastOutgoingCommand // [var]
#define sli_802154phy_radio_ok_to_idle sli_phy1_sli_802154phy_radio_ok_to_idle // [code]
#define emRadioOverflowNotificationEnabled sli_phy1_emRadioOverflowNotificationEnabled // [code]
#define sli_802154phy_radio_packet_trace_enabled sli_phy1_sli_802154phy_radio_packet_trace_enabled // [code]
// Stet emRadioPowerDown // [code] unique to pro2plus, ref by board header
// Stet emRadioPowerUp // [code] unique to pro2plus, ref by board header
#define emRadioPrepareTransmit sli_phy1_emRadioPrepareTransmit // [code]
#define emRadioReceiveCompleteIntEnabled sli_phy1_emRadioReceiveCompleteIntEnabled // [code]
#define emRadioReceiveCompleteIntPending sli_phy1_emRadioReceiveCompleteIntPending // [code]
#define emRadioReceiveCrcEnabled sli_phy1_emRadioReceiveCrcEnabled // [code]
#define emRadioSetChannelAndForceCalibration sli_phy1_emRadioSetChannelAndForceCalibration // [code]
#define emRadioSetEdCcaThreshold sli_phy1_emRadioSetEdCcaThreshold // [code]
#define sli_802154phy_radio_set_idle_mode sli_phy1_sli_802154phy_radio_set_idle_mode // [code]
#define sli_802154phy_radio_set_node_id sli_phy1_sli_802154phy_radio_set_node_id // [code]
#define sli_802154phy_radio_set_node_idOnNetwork sli_phy1_sli_802154phy_radio_set_node_idOnNetwork // [code]
#define sli_802154phy_radio_set_pan_id sli_phy1_sli_802154phy_radio_set_pan_id // [code]
#define sli_802154phy_radio_set_pan_idOnNetwork sli_phy1_sli_802154phy_radio_set_pan_idOnNetwork // [code]
#define emRadioSfdRecvIsrCallback sli_phy1_emRadioSfdRecvIsrCallback // [weak code]
#define emRadioSfdRecvNotificationEnabled sli_phy1_emRadioSfdRecvNotificationEnabled // [code]
#define emRadioSfdSentNotificationEnabled sli_phy1_emRadioSfdSentNotificationEnabled // [code]
#define sli_802154phy_radio_sleep sli_phy1_sli_802154phy_radio_sleep // [code]
#define emRadioTransmit sli_phy1_emRadioTransmit // [code]
#define sli_802154phy_radio_transmit_complete_callback sli_phy1_sli_802154phy_radio_transmit_complete_callback // [ref] MAC layer
#define sli_802154phy_radio_tx_ack_isr_callback sli_phy1_sli_802154phy_radio_tx_ack_isr_callback // [ref] MAC layer
#define sli_802154phy_radio_unpack_chip_errors sli_phy1_sli_802154phy_radio_unpack_chip_errors // [code]
// Stet emRadioTxPowerMode // [static var]
#define sli_802154phy_radio_wake_up sli_phy1_sli_802154phy_radio_wake_up // [code]
// Stet emRxAborted // [var] unique to pro2plus, ref by tests
#define emRxCRCFail sli_phy1_emRxCRCFail // [var]
// Stet emRxMissedFramePending // [var] unique to pro2plus, ref by tests
#define emRxOverflow sli_phy1_emRxOverflow // [var]
#define emRxPacket sli_phy1_emRxPacket // [var]
#define sli_802154phy_set_phy_radio_channel sli_phy1_sli_802154phy_set_phy_radio_channel // [code]
#define sli_802154phy_set_phy_radio_power sli_phy1_sli_802154phy_set_phy_radio_power // [code]
#define sli_802154phy_start_transmit_stream sli_phy1_sli_802154phy_start_transmit_stream // [code]
#define sli_802154phy_start_transmit_tone sli_phy1_sli_802154phy_start_transmit_tone // [code]
#define sli_802154phy_stop_transmit_stream sli_phy1_sli_802154phy_stop_transmit_stream // [code]
#define sli_802154phy_stop_transmit_tone sli_phy1_sli_802154phy_stop_transmit_tone // [code]
#define emSynthFreqOffset sli_phy1_emSynthFreqOffset // [var]
// Stet emTxAborted // [var] unique to pro2plus, ref by tests
// Stet framePendingSetInOutgoingAck // [static var]
// Stet halCommonDelayMicroseconds // [ref]
// Stet halCommonGetInt16uMillisecondTick // [ref]
// Stet halCommonGetInt32uMillisecondTick // [ref]
// Stet halInternalAssertFailed // [ref]
// Stet halInternalGetMfgTokenData // [ref]
// Stet halInternalUartGetBaudRate // [ref] [em3xx-only]
// Stet halInternalUartSetBaudRate // [ref] [em3xx-only]
// Stet halSystemClockEnableCallback // [code] [em3xx-only]
// Stet halSystemXtalHz // [ref]
// Stet lastCsmaTxFinishedTimeMs // [static var]
// Stet lastTransmitPhyChan // [static var]
// Stet lastTxFinishedTimeMs // [static var]
#define lastTxSfdTime sli_phy1_lastTxSfdTime // [var]
// Stet lastTxStartedTimeMs // [static var]
// Stet listeningPhyChan // [static var]
// Stet mapChannelToBand // [static code]
// Stet mapMacPgChanToPhyChanBand // [static code]
// Stet parsePhr // [static code]
// Stet phy2Channel // [static var]
// Stet phy2ChEvents // [static var]
// Stet phy2CoFlags // [static var]
// Stet phy2CoPhCfg // [static var]
// Stet phy2IsCoord // [static var]
// Stet phy2L1Events // [static var]
// Stet phy2Listening // [static var]
// Stet phy2MacPageChan // [static var]
// Stet phy2NetworkParams // [static var]
// Stet phy2NetworkParamsIndexForSlot // [static var]
// Stet phy2NetworkParamsSlotForIndex // [static var]
// Stet phy2PtiGpio // [weak var] ref by board headers
// Stet phy2PtiLog // [static var]
// Stet phy2RxEvents // [static var]
// Stet phy2RxFlags // [static var]
// Stet phy2SubBandConfig // [static var]
// Stet phy2TxEvents // [static var]
// Stet phy2TxFlags // [static var]
// Stet phy2TxPA // [static var]
// Stet phy2TxPowerGranted // [static var]
// Stet phy2TxPowerRequested // [static var]
// Stet pro2CcaThreshold // [static var]
// Stet pro2ChEvents // [static var]
// Stet pro2CleanupTxState // [static code]
// Stet pro2CoFlags // [static var]
// Stet pro2CoPhCfg // [static var]
// Stet pro2DieInfo // [static var]
// Stet pro2ForceCalibration // [static var]
// Stet pro2FuncInfo // [static var]
// Stet pro2GetAckEvents // [static code]
// Stet pro2GetChipRev // [code] unique to pro2plus, ref by tests
// Stet pro2GetDieName // [code] unique to pro2plus, ref by tests
// Stet pro2GetFunc // [code] unique to pro2plus, ref by tests
// Stet pro2GetFwRev // [code] unique to pro2plus, ref by tests
// Stet pro2GetFwSvn // [code] unique to pro2plus, ref by tests
// Stet pro2GetInfo // [static code]
// Stet pro2GetOtpId // [code] unique to pro2plus, ref by tests
// Stet pro2GetPartNum // [code] unique to pro2plus, ref by tests
// Stet pro2GetPatchRev // [code] unique to pro2plus, ref by tests
// Stet pro2GetPtiLogging // [code] unique to pro2plus, ref by tests
// Stet pro2GetRadioBootMode // [code] unique to pro2plus, ref by tests
// Stet pro2GetRadioTxPowerMode // [code] unique to pro2plus, ref by tests
// Stet pro2GetRomId // [code] unique to pro2plus, ref by tests
// Stet pro2GpioCfgPowerUp // [weak var] unique to pro2plus, ref by tests
// Stet pro2GpioConfig // [code] unique to pro2plus, ref by tests
// Stet pro2IdleRadio // [static code]
// Stet pro2IsCoord // [static var]
// Stet pro2Isr // [static code]
// Stet pro2L1Events // [static var]
// Stet pro2Listening // [static var]
// Stet pro2OverridePaSelection // [static code]
// Stet pro2PartInfo // [static var]
// Stet pro2Peek // [code] unique to pro2plus, ref by tests
// Stet pro2Poke // [code] unique to pro2plus, ref by tests
// Stet pro2PoweredUp // [static var]
// Stet pro2PowerUp // [static code]
// Stet pro2PtiGpio // [static var]
// Stet pro2PtiLog // [static var]
// Stet pro2RadioBootMode // [static var]
// Stet pro2ReadOneProperty // [code] unique to pro2plus, ref by tests
// Stet pro2ResetEvents // [static code]
// Stet pro2RestartRadio // [static code]
// Stet pro2RxEvents // [static var]
// Stet pro2RxFlags // [static var]
// Stet pro2RxTxFifoInfo // [static code]
// Stet pro2SetCcaThreshold // [static code]
// Stet pro2SetPtiLogging // [code] unique to pro2plus, ref by tests
// Stet pro2SetRadioBootMode // [code] unique to pro2plus, ref by tests
// Stet pro2SetRadioProperties // [static code]
// Stet pro2SetRadioRamPatch // [static code]
// Stet pro2StartEd // [static code]
// Stet pro2StartRx // [static code]
// Stet pro2ToneOrStream // [static code]
// Stet pro2TxCsmaBackoffAdjust // [static var]
// Stet pro2TxCsmaConfig // [static var]
// Stet pro2TxEvents // [static var]
// Stet pro2TxFlags // [static var]
// Stet pro2UpdateCoFlags // [static code]
// Stet pro2UpdateCoPhCfg // [static code]
// Stet pro2UpdateEvents // [static code]
// Stet pro2UpdateMaxPayloadSize // [static code]
// Stet pro2UpdateRxFlags // [static code]
// Stet pro2UpdateSymbolTimingProperties // [static code]
// Stet pro2UpdateTxCsmaProperties // [static code]
// Stet pro2WakeUp // [static code]
// Stet pro2WriteNProperties // [static code]
// Stet pro2WriteOneProperty // [code] unique to pro2plus, ref by tests
// Stet processRxComplete // [static code]
// Stet processRxEvents // [static code]
// Stet processRxSrcAddr // [static code]
// Stet processTxEvents // [static code]
#define radioCurrentPowerStatus sli_phy1_radioCurrentPowerStatus // [var]
// Stet radioIdlePowerState // [static var]
// Stet radioInternalConfigArray // [static code]
// Stet radioPostTxListening // [static var]
// Stet radioRamPatchImage446xRom6Ie154 // [static code]
// Stet radioRamPatchImage446xRom6Pro2 // [static code]
#define radioTransmitConfig sli_phy1_radioTransmitConfig // [ref]
// Stet resetForNextFrame // [static code]
// Stet rhoActive // [static var]
// Stet rxBuffer // [static var]
// Stet rxBufLen // [static var]
// Stet siPro2GetFRR // [ref]
// Stet siPro2Init // [ref]
// Stet siPro2IntClearPend // [ref]
// Stet siPro2IntDisable // [ref]
// Stet siPro2IntEnable // [ref]
// Stet siPro2PowerDown // [ref]
// Stet siPro2ReadRxFifo // [ref]
// Stet siPro2SendCommand // [ref]
// Stet siPro2SendCommandGetResponse // [ref]
// Stet siPro2WaitForReady // [ref]
// Stet siPro2WriteTxFifo // [ref]
// Stet switchBand // [static code]
// Stet TOKEN_MFG_CCA_THRESHOLD // [ref] //@TODO: Need separate PHY_CONFIG?!
// Stet TOKEN_MFG_PHY_CONFIG // [ref] //@TODO: Need separate PHY_CONFIG?!
// Stet txBufPending // [static var]
// Stet txCmdBuf // [static var]
// Stet updateRadioCurrentPowerStatusPostTx // [static code]

//-----------------------------------
// phy/plugin/duty-cycle/duty-cycle.o
//-----------------------------------
// Stet ??dcEvents // [static code]
// Stet _disableBasePri // [ref]
// Stet _writeBasePri // [ref]
// Stet dcAcksSent // [static var]
// Stet dcAcksSuspended // [static var]
// Stet dcCallbackActive // [static var]
// Stet dcCanSendPkt // [static code]
// Stet dcCheckStateUpdateMode // [static code]
// Stet dcCurChanDc // [static var]
// Stet dcCurMode // [static var]
// Stet dcCurPktOvhBytes // [static var]
// Stet dcCurState // [static var]
// Stet dcDutyCycles // [static var]
// Stet dcLimitAccel // [static var]
// Stet dcMaxAvailBytesForMode // [static code]
// Stet dcNodcHectoPct // [static var]
// Stet dcPktFailedOnAirBytes // [static var]
// Stet dcStateForConsumed // [static code]
// Stet dcTimeAccel // [static var]
// Stet dcTransmit // [static code]
// Stet dcTryPktNow // [static code]
#define dcTxDelaysMs sli_phy1_dcTxDelaysMs // [var]
// Stet dcTxLimitMs // [static var]
// Stet dcTxMode // [static var]
// Stet dcTxPacket // [static var]
// Stet dcUpdateBytes // [static code]
// Stet dcUpdateEntry // [static code]
// Stet sl_zigbee_run_events // [ref]
// Stet sli_event_control_set_active // [ref]
// Stet emEventControlSetDelayMS // [ref]
#define sli_802154phy_get_phy_radio_channel sli_phy1_sli_802154phy_get_phy_radio_channel // [ref]
// Stet sli_legacy_mfglib_mode // [ref] use native PHY's
#define sli_legacy_mfglib_transmit_complete sli_phy1_sli_legacy_mfglib_transmit_complete // [ref]
// Stet emPhyConfigMHz // [ref] unique to pro2plus, used by tests
#define emPhyCurrentBandConfig sli_phy1_emPhyCurrentBandConfig // [ref]
#define emPhyDcAckSent sli_phy1_emPhyDcAckSent // [code]
#define emPhyDcBucketIndex sli_phy1_emPhyDcBucketIndex // [code]
#define emPhyDcEntry sli_phy1_emPhyDcEntry // [code]
// Stet emPhyDcExtendedStatus // [static var]
#define emPhyDcGetActualState sli_phy1_emPhyDcGetActualState // [code]
#define emPhyDcGetDcAndLimits sli_phy1_emPhyDcGetDcAndLimits // [code]
#define emPhyDcGetExtendedStatus sli_phy1_emPhyDcGetExtendedStatus // [code]
#define emPhyDcGetLimitsConfig sli_phy1_emPhyDcGetLimitsConfig // [code]
#define emPhyDcGetMode sli_phy1_emPhyDcGetMode // [code]
#define emPhyDcGetPktMode sli_phy1_emPhyDcGetPktMode // [code]
#define emPhyDcGetState sli_phy1_emPhyDcGetState // [code]
#define emPhyDcGetTimeAccel sli_phy1_emPhyDcGetTimeAccel // [code]
#define sli_802154phy_dc_send_pkt_asap sli_phy1_sli_802154phy_dc_send_pkt_asap // [code]
#define sli_802154phy_dc_send_pkt_failed sli_phy1_sli_802154phy_dc_send_pkt_failed // [code]
#define emPhyDcSetExtendedStatus sli_phy1_emPhyDcSetExtendedStatus // [code]
#define emPhyDcSetLimitsConfig sli_phy1_emPhyDcSetLimitsConfig // [code]
#define emPhyDcSetMode sli_phy1_emPhyDcSetMode // [code]
#define sli_802154phy_dc_set_time_accel sli_phy1_sli_802154phy_dc_set_time_accel // [code]
#define sli_802154phy_dc_state_callback sli_phy1_sli_802154phy_dc_state_callback // [weak code]
#define emPhyDcTick sli_phy1_emPhyDcTick // [code]
// Stet emPhyDcTxEvent // [var] expose pro2plus version by default name
// Stet emPhyDcTxEventHandler // [code] expose pro2plus version by default name
#define emPhyGetDutyCycleParams sli_phy1_emPhyGetDutyCycleParams // [ref]
#define emPhyGetLastTxCompleteTimeMs sli_phy1_emPhyGetLastTxCompleteTimeMs // [ref]
#define emRadioAlwaysUseZeroBackoff sli_phy1_emRadioAlwaysUseZeroBackoff // [ref]
#define sli_802154phy_radio_auto_ack_enabled sli_phy1_sli_802154phy_radio_auto_ack_enabled // [ref]
#define emRadioGetCca sli_phy1_emRadioGetCca // [ref]
#define emRadioHoldOffInternal sli_phy1_emRadioHoldOffInternal // [ref]
#define emRadioPrepareTransmit sli_phy1_emRadioPrepareTransmit // [weak code]
#define emRadioTransmit sli_phy1_emRadioTransmit // [ref]
#define sli_802154phy_radio_transmit_complete_callback sli_phy1_sli_802154phy_radio_transmit_complete_callback // [ref] MAC layer
#define sli_802154phy_radio_wake_up sli_phy1_sli_802154phy_radio_wake_up // [ref]
// Stet halCommonGetInt32uMillisecondTick // [ref]
// Stet halCommonGetRandom // [ref]
// Stet halInternalAssertFailed // [ref]
// Stet halStackGetInt32uSymbolTick // [ref]
// Stet halStackInt32uSymbolTickGTorEqual // [ref]
// Stet halStackOrderSymbolDelay // [ref]
// Stet lbtBackoffTimeSym // [static var]
// Stet lbtCcaEndTimeSymTick // [static var]
// Stet lbtFail // [static code]
// Stet lbtGetRandomBackoffSym // [static code]
// Stet lbtOverallEndTimeSymTick // [static var]
// Stet lbtScheduleBackoffEnd // [static code]
// Stet lbtState // [static var]
// Stet lbtTransmit // [static code]
// Stet lbtTries // [static var]
#define radioTransmitConfig sli_phy1_radioTransmitConfig // [ref]
// Stet symbolDelayCb // [static code]
// Stet transmitComplete // [static code]

//-------------------------------------------
// phy/pro2class/radio_config_Si4468_915MHz.o
//-------------------------------------------
#define emPhyBandConfig915MHz sli_phy1_emPhyBandConfig915MHz // [const]
#define emPhyCcaLbtConfigArray sli_phy1_emPhyCcaLbtConfigArray // [ref]
// Stet gb915SbParams // [static const]
// Stet radioConfigArray // [static const]
// Stet subBandConfigs // [static const]

//-------------------------------------------
// phy/pro2class/radio_config_Si4468_863MHz.o
//-------------------------------------------
#define emPhyBandConfig863MHz sli_phy1_emPhyBandConfig863MHz // [const]
#define emPhyCcaLbtConfigArray sli_phy1_emPhyCcaLbtConfigArray // [ref]
// Stet gb868Sb1Params // [static const]
// Stet radioConfigArray // [static const]
// Stet subBandConfigs // [static const]

//-------------------------------------------
// phy/pro2class/radio_config_em3xxx_2.4GHz.o
//-------------------------------------------
#define emPhyBandConfig2400MHz sli_phy1_emPhyBandConfig2400MHz // [const]
// Stet subBandConfigs // [static const]
// Stet subBandParams // [static const]

//-------------------------------
// phy/pro2class/phy-config-MHz.o
//-------------------------------
#define emPhyBandConfig863MHz sli_phy1_emPhyBandConfig863MHz // [ref]
#define emPhyBandConfig915MHz sli_phy1_emPhyBandConfig915MHz // [ref]
// Stet emPhyConfigMHz // [const] unique to pro2plus, used by tests
// Stet emPhyBands // [static const]
// Stet emPhyTxPa13Levels // [static const]
// Stet emPhyTxPa16Levels // [static const]
// Stet emPhyTxPa20cLevels // [static const]
// Stet emPhyTxPa20fLevels // [static const]
// Stet emPhyTxPAs // [static const]

//-----------------------
// phy/pro2class/sipro2.o
//-----------------------
// Stet ??spiHandleInited // [static var] [efr32-only]
// Stet devConfig // [static var]
// Stet halCommonDelayMicroseconds // [ref]
// Stet halCommonGetInt32uMillisecondTick // [ref]
// Stet halExtDeviceDeselect // [ref] [efr32-only]
// Stet halExtDeviceInit // [ref]
// Stet halExtDeviceIntDisable // [ref]
// Stet halExtDeviceIntEnable // [ref]
// Stet halExtDeviceIsReady // [ref]
// Stet halExtDevicePowerDown // [ref]
// Stet halExtDevicePowerUp // [ref]
// Stet halExtDeviceSelect // [ref] [efr32-only]
// Stet halInternalAssertFailed // [ref]
// Stet halInternalResetWatchDog // [ref]
// Stet halSpiMasterInit // [ref] [em3xx-only]
// Stet halSpiMasterTransferBuf // [ref] [em3xx-only]
// Stet isReadyProbeFunction // [static var]
// Stet pro2SpiClockMHz // [ref] unique to pro2plus, ref by tests
// Stet pro2SpiPort // [ref] unique to pro2plus, ref by tests
// Stet ready // [static var]
// Stet siPro2CmdRead // [static code]
// Stet siPro2GetFRR // [code]
// Stet siPro2GetFRRs // [code]
// Stet siPro2GetResponse // [code]
// Stet siPro2Init // [code]
// Stet siPro2IntClearPend // [code]
// Stet siPro2IntDisable // [code]
// Stet siPro2IntEnable // [code]
// Stet siPro2IsReady // [static code]
// Stet siPro2PowerDown // [code]
// Stet siPro2PowerUp // [code]
// Stet siPro2ReadRxFifo // [code]
// Stet siPro2SendCommand // [code]
// Stet siPro2SendCommandGetResponse // [code]
// Stet siPro2WaitForReady // [code]
// Stet siPro2WriteTxFifo // [code]
// Stet SPIDRV_Init // [ref] [efr32-only]
// Stet SPIDRV_MTransferB // [ref] [efr32-only]
// Stet SPIDRV_MTransmitB // [ref] [efr32-only]
// Stet spiHandleData // [static var] [efr32-only]
// Stet spiXfer // [static code] [efr32-only]
// Stet splitTrxBuf // [static var] [efr32-only]
#ifdef PHY_SIMULATION_DUAL
#define simulatorResumeRadioActivity  sli_phy1_simulatorResumeRadioActivity
#define simulatorSuspendRadioActivity sli_phy1_simulatorSuspendRadioActivity
#endif  //PHY_SIMULATION_DUAL

#else//!PHY_THIS

#error "Unsupported PHY_THIS!"

#endif//PHY_THIS

#endif//__PHY_DUAL_PHY_H__
