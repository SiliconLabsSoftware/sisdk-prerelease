/***************************************************************************//**
 * @file
 * @brief Implementation of radio, baseband, and MAC functions.
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

#define LEGACY_PHY_BUILD 1
#include "phy/hal.h"
#include "core/sl_zigbee_stack.h"
#include "phy/phy.h"
#include "framework/eui64.h"
#include "phy/plugin/duty-cycle/duty-cycle.h" // For emPhyDcTick()
#include "coexistence/protocol/ieee802154_uc/coexistence-802154.h"
#include "rail_util_ant_div/sl_rail_util_ant_div.h"
// Radio related header files
#include "rail.h"
#include "rail_ieee802154.h"

#include "indirect-queue.h" // For frame-pending child table lookups
#include "mac-child.h"

#define RxFrameDetectDisable()                                        \
  (void) RAIL_ConfigRxOptions(emPhyRailHandle,                        \
                              RAIL_RX_OPTION_DISABLE_FRAME_DETECTION, \
                              RAIL_RX_OPTION_DISABLE_FRAME_DETECTION)
#define RxFrameDetectEnable()                                         \
  (void) RAIL_ConfigRxOptions(emPhyRailHandle,                        \
                              RAIL_RX_OPTION_DISABLE_FRAME_DETECTION, \
                              RAIL_RX_OPTIONS_NONE)

#ifndef RHO_USE_BLOCKTX
#define RHO_USE_BLOCKTX 0 // 0 = RHO_USE_AUTOACK mechanism instead
#endif//RHO_USE_BLOCKTX

// In the PRO stack we need to write the network index in the appended info.
// The network index is obtained by calling
// sli_mac_get_current_radio_network_index().
#include "upper-mac.h"

#ifndef PHY_USE_PREPARE_TX_API
#define PHY_USE_PREPARE_TX_API 1
#endif//PHY_USE_PREPARE_TX_API

#define PHY_ASSERT(expr) /* expr must not have side effects */ assert(expr)
#define PHY_ATTEST(expr) /* expr is ok to have side effects */ do { \
    if (!(expr)) {                                                  \
      PHY_ASSERT(false);                                            \
    }                                                               \
} while (0)

#if defined(__RELEASE__)
#define PHY_ASSERT_INTERNAL(expr) /* expr must not have side effects */ /* no-op */
#else //!defined(__RELEASE__)
#define PHY_ASSERT_INTERNAL(expr) /* expr must not have side effects */ PHY_ASSERT(expr)
#endif //defined(__RELEASE__)

#define PHY_ATTEST_SUCCESS(expr) PHY_ATTEST((expr) == SL_STATUS_OK)
#define PHY_ATTEST_RAIL(expr) PHY_ATTEST((expr) == RAIL_STATUS_NO_ERROR)

#define PA_SCALING_FACTOR RAIL_TX_POWER_DBM_SCALING_FACTOR

#ifndef MAC_SUPPORT_ENHANCED_ACKS
#define MAC_SUPPORT_ENHANCED_ACKS 0 // RAIL_FEAT_IEEE802154_E_ENH_ACK_SUPPORTED
#endif//MAC_SUPPORT_ENHANCED_ACKS

#ifndef MAC_SUPPORT_MULTIPURPOSE_FRAMES
#define MAC_SUPPORT_MULTIPURPOSE_FRAMES 0 // RAIL_FEAT_IEEE802154_MULTIPURPOSE_FRAME_SUPPORTED
#endif//MAC_SUPPORT_MULTIPURPOSE_FRAMES

#ifndef USE_ENHANCED_FRAME_PENDING
#define USE_ENHANCED_FRAME_PENDING 0
#endif//USE_ENHANCED_FRAME_PENDING

#if     (MAC_HAS_CHANNEL_PAGES && IEEE802154_GB868_SUPPORTED)
  #define PHRLen ((emRadioChannelPageInUse == 0) ? 1 : 2)
#else//!(MAC_HAS_CHANNEL_PAGES && IEEE802154_GB868_SUPPORTED)
  #define PHRLen 1
#endif//(MAC_HAS_CHANNEL_PAGES && IEEE802154_GB868_SUPPORTED)

#define RAC_STATUS_RXMASK_PROTIMER (1UL << 14)

#define RAIL_WARM_TIME_US 100u // RxWarm and TxWarm (aka Idle) time
#define MAC_TX_TIMEOUT_MS 1000u

// Defines for the scheduler to estimate packet time on air
//@TODO: These should be re-evaluated at some point, but for now this will help
// the scheduler make better choices
#define CSMA_OVERHEAD 500
#define LBT_OVERHEAD  1000
#define SL_ZIGBEE_PHY_PACKET_OVERHEAD (SL_ZIGBEE_PHY_BITS_TO_US(SL_ZIGBEE_PHY_PREAMBLE_BITS) \
                                       + SL_ZIGBEE_PHY_BITS_TO_US(SL_ZIGBEE_PHY_SFD_BITS))
#define SL_ZIGBEE_PHY_ACK_DURATION (SL_ZIGBEE_PHY_PACKET_OVERHEAD                           \
                                    + SL_ZIGBEE_PHY_BYTES_TO_US(SL_ZIGBEE_PHY_MAX_PHR_BYTES \
                                                                + 3                         \
                                                                + SL_ZIGBEE_PHY_MAX_CRC_BYTES))

#define IEEE802154_ADDR_INDEX 0

static void RAILCb_Events(RAIL_Handle_t railHandle, RAIL_Events_t events);
static void RAILCb_RxAckTimeout(RAIL_Handle_t railHandle);
static void RAILCb_TxPacketSent(RAIL_Handle_t railHandle);
static void RAILCb_IEEE802154_DataRequestCommand(RAIL_Handle_t railHandle);
static void RAILCb_RxPacketReceived(RAIL_Handle_t railHandle);
static void RAILCb_CalNeeded(RAIL_Handle_t railHandle);
static volatile int32_t lastTxStartedTimeMs = -1;

static bool radioInited = false;

#ifdef   DEBUG_BACKOFF_STATS
uint32_t emMacTotalBackoffs;
uint32_t emMacTransmits;
#endif// DEBUG_BACKOFF_STATS

RAIL_Handle_t emPhyRailHandle;
EmPhyBandConfig *emPhyCurrentBandConfig = NULL;
static const EmPhySubBandConfig *phy2SubBandConfig = NULL;
static const EmPhySubBandParams *phy2SubBandParams = NULL;
static uint8_t emRadioChannelPageInUse = 0;
static uint8_t emRadioChannel = INVALID_CHANNEL;
static int16_t emRadioPowerFl = INIT_RADIO_POWER * PA_SCALING_FACTOR;
int8_t emSynthFreqOffset = 0;

uint16_t emRadioTxPowerMode = SL_ZIGBEE_TX_POWER_MODE_USE_TOKEN;

static bool radioCheckCrc = true;
uint8_t sli_legacy_mfglib_mode = 0;
volatile bool sli_legacy_mfglib_transmit_complete = false;

static sl_802154_short_addr_t emLocalRadioNodeId = EM_USE_LONG_ADDRESS;
static sl_802154_pan_id_t emLocalRadioPanId = EM_BROADCAST_PAN_ID;

static bool ptiEnabled = true;
static bool deviceIsCoordinator = false;
static bool addressMatchingEnabled = true;
static bool autoAckEnabled = true;
static const bool enhAckSupported = (bool) (MAC_SUPPORT_ENHANCED_ACKS);

// Start with edCcaThreshold in the "uninitialized" state.
static int8_t edCcaThresholds[2] = {
  ED_CCA_THRESHOLD_UNINIT, // 2.4GHz band threshold
  ED_CCA_THRESHOLD_UNINIT, // SubGHz band threshold
};
// Pick the right threshold based on the band, using emRadioChannelPageInUse
#define edCcaThreshold (edCcaThresholds[emRadioChannelPageInUse != 0])

static bool emRadioOverflowCallbackEnabled = false;
static bool emRadioSfdSentCallbackEnabled = false;

static RadioPowerMode radioIdlePowerState = SL_ZIGBEE_RADIO_POWER_MODE_OFF;

static bool framePendingSetInOutgoingAck = false;
static bool sfdRecvNotificationEnabled = false;
static bool ptaEnabled = false;

bool emRadioAlwaysUseZeroBackoff = false;
uint8_t emRadioLastOutgoingCommand = 0xff;
RadioPowerMode radioCurrentPowerStatus = SL_ZIGBEE_RADIO_POWER_MODE_OFF;
uint32_t lastTxSfdTime;
//TODO: Initialize these to a time in the distant past so lbtMinOffTime
//TODO: is not applied on first transmit after bootup?
static volatile uint32_t lastTxFinishedTimeMs;
static volatile uint32_t lastCsmaTxFinishedTimeMs;

static RAIL_Events_t currentEventConfig = RAIL_EVENTS_NONE;

extern sl_802154_radio_priorities_t radioSchedulerPriorityTable;

static void MACSEQ_TOUT0MatchLBTEvent(void); // Forward reference
static void MACSEQ_TransmitErrorEvent(sl_status_t status); // Forward reference

// ######################################
// ####### EFR32 ########################
// ######################################

typedef enum {
  PHY_TX_STATE_CANC_W_ACK  = -4,
  PHY_TX_STATE_CANC_NOACK  = -3,
  PHY_TX_STATE_PREP_W_ACK  = -2,
  PHY_TX_STATE_PREP_NOACK  = -1,
  PHY_TX_STATE_OFF         =  0,
  PHY_TX_STATE_TRIG_NOACK  =  1,
  PHY_TX_STATE_TRIG_W_ACK  =  2,
  PHY_TX_STATE_TRIG_ACKING =  3,
} txState_t;

static volatile txState_t txState = PHY_TX_STATE_OFF;

static sl_rail_util_ieee802154_stack_status_t myPtaStackEvent(sl_rail_util_ieee802154_stack_event_t ptaStackEvent,
                                                              uint32_t supplement)
{
  sl_rail_util_ieee802154_stack_status_t status = sl_rail_util_ieee802154_on_event(ptaStackEvent, supplement);

  if (radioSchedulerPriorityTable.background_rx
      != radioSchedulerPriorityTable.active_rx) {
    switch (ptaStackEvent) {
      case SL_RAIL_UTIL_IEEE802154_STACK_EVENT_RX_STARTED:
        // Escalate priority for the duration of this Rx (and any ACK)
        RAIL_SetTaskPriority(emPhyRailHandle,
                             radioSchedulerPriorityTable.active_rx,
                             RAIL_TASK_TYPE_START_RX);
        break;
      case SL_RAIL_UTIL_IEEE802154_STACK_EVENT_RX_FILTERED:
      case SL_RAIL_UTIL_IEEE802154_STACK_EVENT_RX_CORRUPTED:
      case SL_RAIL_UTIL_IEEE802154_STACK_EVENT_RX_ACK_BLOCKED:
      case SL_RAIL_UTIL_IEEE802154_STACK_EVENT_RX_ACK_ABORTED:
      case SL_RAIL_UTIL_IEEE802154_STACK_EVENT_RX_ACK_SENT:
      case SL_RAIL_UTIL_IEEE802154_STACK_EVENT_RX_ENDED:
        // Deescalate priority now that Rx (and any ACK) has finished
        RAIL_SetTaskPriority(emPhyRailHandle,
                             radioSchedulerPriorityTable.background_rx,
                             RAIL_TASK_TYPE_START_RX);
        break;
      default:
        break;
    }
  }
  return status;
}
#define halPtaStackEvent myPtaStackEvent // Redirect below code to use wrapper

#include "routing/zigbee/child.h"

static inline bool emberRadioDataPendingShortIdIsrCallback(uint16_t shortId,
                                                           uint16_t frameType)
{
  uint8_t childIndex = sli_mac_child_index(shortId);
  if (0xFF != childIndex) {
    // This device is our child.
   #if     USE_ENHANCED_FRAME_PENDING
    if (frameType == MAC_FRAME_TYPE_DATA) {
      return emChildHasEnhancedFramePendingMessage(childIndex);
    }
   #else//!USE_ENHANCED_FRAME_PENDING
    UNUSED_VAR(frameType);
   #endif//USE_ENHANCED_FRAME_PENDING
    // TODO: sli_mac_child_has_pending_message implementation for SL-Thread.
    return sli_mac_child_has_pending_message(childIndex);
  }
  // This device is not our child so set frame pending
  // but only if it's a CMD frame (true data poll).
  return (frameType == MAC_FRAME_TYPE_CONTROL);
}

static inline bool emberRadioDataPendingLongIdIsrCallback(uint8_t *longId,
                                                          uint16_t frameType)
{
 #if     USE_ENHANCED_FRAME_PENDING
  uint8_t childIndex = sli_mac_child_long_index(longId);
  if (0xFF != childIndex) {
    // This device is our child.
    if (frameType == MAC_FRAME_TYPE_DATA) {
      return emChildHasEnhancedFramePendingMessage(childIndex);
    }
    // TODO: sli_mac_child_has_pending_message implementation for SL-Thread.
    return sli_mac_child_has_pending_message(childIndex);
  }
  // This device is not our child so set frame pending
  // but only if it's a CMD frame (true data poll).
  return (frameType == MAC_FRAME_TYPE_CONTROL);
 #else//!USE_ENHANCED_FRAME_PENDING
  UNUSED_VAR(frameType);
  return sli_mac_long_id_data_pending(longId);
 #endif//USE_ENHANCED_FRAME_PENDING
}

static void efr32WakeUp(RadioPowerMode mode); // Forward decl

//------------------------------------------------------------------------------

uint32_t sli_802154phy_get_mac_timer(void)
{
  return RAIL_GetTime();
}

bool sli_802154phy_radio_get_random_numbers(uint16_t *rn, uint8_t count)
{
  uint16_t countBytes = count * sizeof(uint16_t);

  // Collect entropy from the radio over the air. This should be good enough
  // to be used directly as a random number according to the hardware team.
  if (RAIL_GetRadioEntropy(emPhyRailHandle, (uint8_t*)rn, countBytes) != countBytes) {
    return false;
  }

  return true;
}

bool sli_802154phy_radio_check_radio(void)
{
  // EFR32 calibration events are: IRCal and tempcal
  // RAIL Cal requests update the stack via pending bits
  // (RAILCb_CalNeeded callback fires also)
  return RAIL_GetPendingCal(emPhyRailHandle);
}

// rhoActive covers both external and internal RHO users distinctly, so not bool
enum {
  RHO_INACTIVE = 0,
  RHO_EXT_ACTIVE,
  RHO_INT_ACTIVE,
  RHO_BOTH_ACTIVE,
};
static uint8_t rhoActive = RHO_INACTIVE;

bool emRadioHoldOffIsActive(void)
{
  return !!rhoActive;
}

static void setCurCcaThreshold(int8_t ccaThreshold)
{
  if (radioInited) {
    PHY_ATTEST_RAIL(RAIL_SetCcaThreshold(emPhyRailHandle, ccaThreshold));
  } else {
    // Defer to sli_802154phy_radio_init() time
  }
}

static void holdOffAutoAck(bool holdOff)
{
 #if     RHO_USE_BLOCKTX
  RAIL_EnableTxHoldOff(emPhyRailHandle, holdOff);
 #else//!RHO_USE_BLOCKTX // i.e. RHO_USE_AUTOACK
  if (holdOff) {
    RAIL_PauseRxAutoAck(emPhyRailHandle, true);
  } else {
    sli_802154mac_radio_enable_auto_ack(sli_802154phy_radio_auto_ack_enabled());
  }
 #endif//RHO_USE_BLOCKTX
}

static void emRadioHoldOffInternalIsr(uint8_t active)
{
  if (active != rhoActive) {
    rhoActive = active; // Update rhoActive early
    if (active != RHO_INACTIVE) {
      // Suspend ability of the radio to transmit
      // This is done by configuring radio such that subsequent
      // CCA attempts fail and the automatic generation of ACKs
      // is conditionally disabled.
      // We do this directly in radio registers behind the backs
      // of emRadioSetEdCcaThreshold() and sli_802154mac_radio_enable_auto_ack()
      // so the shadow state which those routines maintain in RAM
      // preseve the saved state to restore once Radio HoldOff is
      // done.
      setCurCcaThreshold(ED_CCA_THRESHOLD_MINIMUM);
      holdOffAutoAck((active >= RHO_INT_ACTIVE) // Internal always holds ACKs
                     || ((sl_rail_util_coex_get_options()
                          & SL_RAIL_UTIL_COEX_OPT_ACK_HOLDOFF)
                         != SL_RAIL_UTIL_COEX_OPT_DISABLED));
    } else {
      // Restore pre-Radio-HoldOff settings (which stack might have
      // changed while Radio HoldOff was active) using the shadow
      // copies the stack maintains in RAM.
      emRadioSetEdCcaThreshold(edCcaThreshold); // Don't use emRadioGetEdCcaThreshold() here
      holdOffAutoAck(false);
    }
  }
}

void emRadioHoldOffIsr(bool active)
{
  emRadioHoldOffInternalIsr((uint8_t) active | (rhoActive & ~RHO_EXT_ACTIVE));
}

void emRadioHoldOffInternal(bool active)
{
  DECLARE_INTERRUPT_STATE;
  DISABLE_INTERRUPTS();
  emRadioHoldOffInternalIsr((((uint8_t) active) << 1)
                            | (rhoActive & RHO_EXT_ACTIVE));
  RESTORE_INTERRUPTS();
}

bool emRadioIsReceivingFrame(void)
{
  return (RAIL_GetRadioState(emPhyRailHandle) & RAIL_RF_STATE_RX_ACTIVE) == RAIL_RF_STATE_RX_ACTIVE;
}

void emberCalibrateVref(void)
{
}

sl_status_t emApiSetTxPowerMode(uint16_t txPowerMode)
{
  // This will override certain configuration settings, so use with care!
  // First check to see if we are using defaults from the PHY_CONFIG token.
  if (txPowerMode == SL_ZIGBEE_TX_POWER_MODE_USE_TOKEN) {
    // The application has not overriden the configuration stored in the token.
    tokTypeMfgPhyConfig phyConfig = SL_ZIGBEE_TX_POWER_MODE_PER_BAND_CONFIG;
    halCommonGetMfgToken(&phyConfig, TOKEN_MFG_PHY_CONFIG);

    // Aside from SL_ZIGBEE_TX_POWER_MODE_BAND_NO_LIMITS_MASK, currently
    // no other bits in this token are defined.
    txPowerMode = (uint16_t) phyConfig;
  }

  // Store the new power mode configuration, capturing anything that was loaded
  // from the PHY_CONFIG token along the way.
  emRadioTxPowerMode = txPowerMode;
  return SL_STATUS_OK;
}

//-- Channel Mapping

// Retain the least-significant bits from Page
#define macPage2Pg(macPage)      ((macPage) & (BIT(8 - CHANNEL_BITS) - 1))
// For pages other than 0, set the most-significant bits dropped in Pg encoding
// E.g. 3-bit Pgs 1-7 map to 5-bit Pages 24-31.
#define macPg2Page(macPg)        (((macPg) == 0) ? 0                                         \
                                  : ((macPg) | ((((1UL << (32 - MAX_CHANNELS_PER_PAGE)) - 1) \
                                                 >> (8 - CHANNEL_BITS)) << (8 - CHANNEL_BITS))))

#if     MAC_HAS_CHANNEL_PAGES

static EmPhyBandConfig *mapMacPgChanToPhyChanBand(uint8_t macPgChan,
                                                  uint8_t *phyChanPtr,
                                                  uint8_t *macPagePtr,
                                                  EmPhySubBandConfig **subBandConfigPtr)
{
  // Here we assume macPgChan has already been mapped
  // Loop thru the bands and their subBandConfigs looking for a channel mapping
  uint8_t band, txpl;
  uint8_t macPg = sli_802154mac_pg_chan_pg(macPgChan);
  uint8_t macCh = sli_802154mac_pg_chan_ch(macPgChan);
  for (band = 0; band < emPhyConfigEfr.numBands; band++) {
    EmPhyBandConfig *bandConfig = (EmPhyBandConfig*) emPhyConfigEfr.bandConfigs[band];
    for (txpl = 0; txpl < bandConfig->numSubBandConfigs; txpl++) {
      EmPhySubBandConfig *macMap = (EmPhySubBandConfig*) &bandConfig->subBandConfigs[txpl];
      if ((macPage2Pg(macMap->macChanPage) == macPg)
          && (macMap->macChanOffset <= macCh)
          && (macCh <= (macMap->macChanOffset
                        + (macMap->maxPhyChan - macMap->minPhyChan)))) {
        // Found it!
        // Return success after filling in the phyChan mapping
        if (phyChanPtr != NULL) {
          *phyChanPtr = (macCh - macMap->macChanOffset) + macMap->minPhyChan;
        }
        if (macPagePtr != NULL) {
          *macPagePtr = macMap->macChanPage; // the full 5-bit page#
        }
        if (subBandConfigPtr != NULL) {
          *subBandConfigPtr = macMap;
        }
        return bandConfig;
      }
    }
  }
  return NULL; // No band found for this macPgChan
}

uint8_t sli_802154phy_get_channel_page_in_use(void)
{
  return emRadioChannelPageInUse;
}

uint8_t sli_802154phy_get_channel_page_for_channel(uint8_t macPgChan)
{
  // Convert internal 3-bit abbreviated Pg to full 5-bit Page
  // Here treat macPgChan verbatim -- needs no remapping
  return macPg2Page(sli_802154mac_pg_chan_pg(macPgChan));
}

// Default to first MAC channel on first band's macMap in subBandConfigs{}
// Note: If this channel is NOT valid due to channel mask enforcement,
// sli_802154phy_radio_init() will likely assert.  Fix the radio configs so this
// choice *is* valid, or app must explicitly set channel to a valid
// one to avoid this poor choice of default channel.
#define selectDefaultChannel()                                                         \
  (sli_802154mac_pg_chan(emPhyConfigEfr.bandConfigs[0]->subBandConfigs[0].macChanPage, \
                         emPhyConfigEfr.bandConfigs[0]->subBandConfigs[0].macChanOffset))

#else//!MAC_HAS_CHANNEL_PAGES

// Default to first channel on first band
#define selectDefaultChannel() (emPhyConfigEfr.bandConfigs[0]->minPhyChan)

static EmPhyBandConfig *mapMacPgChanToPhyChanBand(uint8_t macPgChan,
                                                  uint8_t *phyChanPtr,
                                                  uint8_t *macPagePtr,
                                                  EmPhySubBandConfig **subBandConfigPtr)
{
  // Here we assume macPgChan has already been mapped
  if (phyChanPtr != NULL) {
    *phyChanPtr = macPgChan;
  }
  if (macPagePtr != NULL) {
    *macPagePtr = 0;
  }
  EmPhyBandConfig *bandConfig = (EmPhyBandConfig*) emPhyConfigEfr.bandConfigs[0];
  if (subBandConfigPtr != NULL) {
    *subBandConfigPtr = (EmPhySubBandConfig*) &emPhyConfigEfr.bandConfigs[0]->subBandConfigs[0];
  }
  return bandConfig;
}

#endif//MAC_HAS_CHANNEL_PAGES

sl_status_t emRadioSetChannelAndForceCalibration(uint8_t macPgChan)
{
  // Attemp to change the radio channel
  sl_status_t result = sli_802154phy_set_phy_radio_channel(macPgChan);
  if (result == SL_STATUS_OK) {
    sl_802154_calibrate_current_channel();
  }
  return result;
}

void sl_802154_calibrate_current_channel(void)
{
  RAIL_CalMask_t pending = RAIL_GetPendingCal(emPhyRailHandle);
  if (pending & RAIL_CAL_TEMP_VCO) {
    RAIL_CalibrateTemp(emPhyRailHandle);
  }
  if (pending & RAIL_CAL_ONETIME_IRCAL) {
    // Run the appropriate IRCAL function based off of our radio config
    PHY_ASSERT(emPhyCurrentBandConfig->radioConfigArray != NULL);
    #define emPhyIrCal (emPhyCurrentBandConfig->radioConfigArray->irCal)
    PHY_ASSERT(emPhyIrCal != NULL);
    PHY_ATTEST_RAIL((*emPhyIrCal)(emPhyRailHandle, NULL));
    #undef emPhyIrCal
  }
}

extern const RAIL_TxPowerConfig_t *halInternalPa2p4GHzInit;
extern const RAIL_TxPowerConfig_t *halInternalPaSubGHzInit;

static const RAIL_TxPowerConfig_t *getPaConfig(uint8_t macPgChan)
{
  // Here we assume macPgChan has already been mapped
  if ((macPgChan >= 11) && (macPgChan <= 26)) {
    return halInternalPa2p4GHzInit;
  }
  return halInternalPaSubGHzInit;
}

static bool configPa(uint8_t macPgChan)
{
  static const RAIL_TxPowerConfig_t *curPaConfig = NULL;
  const RAIL_TxPowerConfig_t *newPaConfig = getPaConfig(macPgChan);

  if (newPaConfig == NULL || newPaConfig->mode == RAIL_TX_POWER_MODE_NONE) {
    return false;
  }
  if (newPaConfig != curPaConfig) {
    RAIL_TxPowerConfig_t adjPaConfig = *newPaConfig; // Structure copy
    curPaConfig = newPaConfig;
    RAIL_Status_t status = RAIL_ConfigTxPower(emPhyRailHandle, &adjPaConfig);
    if (status != RAIL_STATUS_NO_ERROR) {
      return false;
    }
    RAIL_TxPowerConfig_t txPowerConfig;
    RAIL_GetTxPowerConfig(emPhyRailHandle, &txPowerConfig);
    return (RAIL_SetTxPowerDbm(emPhyRailHandle, emRadioPowerFl)
            == RAIL_STATUS_NO_ERROR);
  }
  return true;
}

static void restartListening(void)
{
  RAIL_SchedulerInfo_t rxSchedulerInfo = {
    .priority = radioSchedulerPriorityTable.background_rx,
  };

  if ((radioCurrentPowerStatus == SL_ZIGBEE_RADIO_POWER_MODE_RX_ON)
      || (radioCurrentPowerStatus == SL_ZIGBEE_RADIO_POWER_MODE_ED_ON)) {
    //@TODO: Remove retry after RAIL_LIB-825 is fixed
    if (RAIL_StartRx(emPhyRailHandle, emRadioChannel, &rxSchedulerInfo)
        != RAIL_STATUS_NO_ERROR) {
      // Oops -- try idling first
      RAIL_Idle(emPhyRailHandle, RAIL_IDLE, true);
      PHY_ATTEST_RAIL(RAIL_StartRx(emPhyRailHandle, emRadioChannel, &rxSchedulerInfo));
    }
  }
}

sl_status_t emPhySetRadioChannel(uint8_t macPgChan) // AKA sli_802154phy_set_phy_radio_channel
{
  if (macPgChan == INVALID_CHANNEL) {
    macPgChan = selectDefaultChannel();
  }

  // Quickly reject an illegal channel#
  if (sli_802154mac_pg_chan_ch(macPgChan) >= MAX_CHANNELS_PER_PAGE) {
    return SL_STATUS_TRANSMIT_INVALID_CHANNEL;
  }

  // Try to map macPgChan to valid macPage
  uint8_t macPage;
  EmPhySubBandConfig *subBandConfig = NULL;
  EmPhyBandConfig *bandConfig = mapMacPgChanToPhyChanBand(macPgChan, NULL,
                                                          &macPage, &subBandConfig);
  if ((bandConfig == NULL) || (subBandConfig == NULL)) {
    return SL_STATUS_TRANSMIT_INVALID_CHANNEL;
  }

  // Only do real channel-changing work if the channel is actually changing
  if (macPgChan != emRadioChannel) {
    // First try to establish proper PA. We do not need to sleep radio
    // or change Radio config yet for this.  It might fail if the chip
    // does not support the PA configured, nicely leaving the radio as
    // it was.
    if (!configPa(macPgChan)) {  // No valid PA config for this channel
      return SL_STATUS_TRANSMIT_INVALID_CHANNEL;
    }

    emPhyCurrentBandConfig = bandConfig;

    // Deal with a SubBand Config change
    if (phy2SubBandConfig == subBandConfig) {
      // In-SubBand change is quick:
      emRadioChannelPageInUse = macPage;
      emRadioChannel = macPgChan;
      restartListening();
    } else {
      // Cross-SubBand change is slower:
      // Best to fully sleep radio during this; remember its original state.
      RadioPowerMode origPowerStatus = radioCurrentPowerStatus;
      sli_802154phy_radio_sleep();

      // Deinit 15.4 so we can re-init it with new SubBand ACK and
      // turnaround timing, CCA threshold, etc.
      // Only do this if it was inited previously.
      if (phy2SubBandConfig != NULL) {
        PHY_ATTEST_RAIL(RAIL_IEEE802154_Deinit(emPhyRailHandle));
      }

      phy2SubBandConfig = subBandConfig;
      phy2SubBandParams = subBandConfig->subBandParams;
      PHY_ASSERT(phy2SubBandParams != NULL);

      // Deal with a Radio Config change (typically a Band Config change).
      // Note that SubBand Configs cannot be shared across different
      // Band Configs or Radio Configs.
      static const EmPhyRadioConfig *curRadioConfig = NULL;
      if (curRadioConfig != bandConfig->radioConfigArray) {
        PHY_ASSERT(bandConfig->radioConfigArray != NULL);
        #define emPhyConfigRadio (bandConfig->radioConfigArray->radioConfig)
        PHY_ASSERT(emPhyConfigRadio != NULL);
        PHY_ATTEST_RAIL((*emPhyConfigRadio)(emPhyRailHandle)); // Triggers IR CAL
        #undef emPhyConfigRadio
        curRadioConfig = bandConfig->radioConfigArray;
      }

      emRadioChannelPageInUse = macPage; // Update here for PHRLen
      emRadioChannel = macPgChan;

      RAIL_IEEE802154_Config_t rail154Config = {
        .addresses = NULL, // Will fix addresses later; this saves RAM
        .ackConfig = {
          .enable = true,
          // RAIL's ackTimeout doesn't include PHR or PSDU of the 5-byte ACK
          .ackTimeout = SL_ZIGBEE_PHY_SYMBOLS_TO_US(phy2SubBandParams->ackMaxSym)
                        - SL_ZIGBEE_PHY_BYTES_TO_US(PHRLen + 5),
          .rxTransitions = { // Will get fixed when re-awakening radio later
            .success = RAIL_RF_STATE_IDLE,
            .error = RAIL_RF_STATE_IDLE,
          },
          .txTransitions = { // Will get fixed when re-awakening radio later
            .success = RAIL_RF_STATE_IDLE,
            .error = RAIL_RF_STATE_IDLE,
          },
        },
        .timings = {
          .idleToRx = RAIL_WARM_TIME_US,
          .idleToTx = RAIL_WARM_TIME_US,
          .rxToTx = SL_ZIGBEE_PHY_SYMBOLS_TO_US(phy2SubBandParams->ackMinSym),
          .txToRx = SL_ZIGBEE_PHY_SYMBOLS_TO_US(phy2SubBandParams->ackMinSym)
                    - 20, // Used to be 10; increased to be more conservative
          .rxSearchTimeout = 0, // not used
          .txToRxSearchTimeout = 0, // not used
        },
        .framesMask = (0U
                       | RAIL_IEEE802154_ACCEPT_STANDARD_FRAMES
       #if     MAC_SUPPORT_MULTIPURPOSE_FRAMES
                       | RAIL_IEEE802154_ACCEPT_MULTIPURPOSE_FRAMES
       #endif//MAC_SUPPORT_MULTIPURPOSE_FRAMES
                       ),
        .promiscuousMode = !addressMatchingEnabled,
        .isPanCoordinator = deviceIsCoordinator,
      };

      PHY_ATTEST_RAIL(RAIL_IEEE802154_Init(emPhyRailHandle, &rail154Config));

     #if     MAC_SUPPORT_ENHANCED_ACKS
      // ZNet or Thread with CSL:  needs Enh-ACK support.
      // Enable 802.15.4E mode for GB868 and CSL.
      // Platforms that support 15.4E but not Enh-ACKs will assert here.
      PHY_ATTEST_RAIL(RAIL_IEEE802154_ConfigEOptions(emPhyRailHandle,
                                                     (RAIL_IEEE802154_E_OPTION_GB868
                                                      | RAIL_IEEE802154_E_OPTION_ENH_ACK),
                                                     (RAIL_IEEE802154_E_OPTION_GB868
                                                      | RAIL_IEEE802154_E_OPTION_ENH_ACK)));
      // For Enh-ACKs and Thread's Enhanced Frame Pending, also
      // need both Early and Data Frame Pending to be enabled
      // so RAIL_EVENT_IEEE802154_DATA_REQUEST_COMMAND triggers
      // on ACK-requesting packets as soon as their addressing
      // information is available.  That event is used to detect
      // the need for an Enh-ACK and construct it.
      PHY_ATTEST_RAIL(RAIL_IEEE802154_EnableEarlyFramePending(emPhyRailHandle, true));
      PHY_ATTEST_RAIL(RAIL_IEEE802154_EnableDataFramePending(emPhyRailHandle, true));
     #elif   USE_ENHANCED_FRAME_PENDING
      // Thread without CSL:  does not need 15.4E support.
      // But it does need Data Frame Pending enabled for
      // its Enhanced Frame Pending feature.  While not
      // strictly needed, Early Frame Pending is enabled
      // to give Thread more Frame Pending lookup time.
      PHY_ATTEST_RAIL(RAIL_IEEE802154_EnableEarlyFramePending(emPhyRailHandle, true));
      PHY_ATTEST_RAIL(RAIL_IEEE802154_EnableDataFramePending(emPhyRailHandle, true));
     #else
      // ZNet without CSL:  still needs 15.4E for GB868 features
      // like Enhanced Beacons, even in 2.4GHz band.
      // It does not need Early Frame Pending, and definitely
      // doesn't want Data Frame Pending.
      // On platforms that support Enh-ACK, enabling 15.4E also
      // implicitly enables RAIL Enh-ACK support, but this PHY
      // simply won't generate an Enh-ACK.
      PHY_ATTEST_RAIL(RAIL_IEEE802154_ConfigEOptions(emPhyRailHandle,
                                                     RAIL_IEEE802154_E_OPTION_GB868,
                                                     RAIL_IEEE802154_E_OPTION_GB868));
     #endif//MAC_SUPPORT_ENHANCED_ACKS

      // Init effectively did:
      //   emRadioEnableAddressMatching(emRadioAddressMatchingEnabled());
      //   sli_802154phy_set_coordinator(emPhyDeviceIsCoordinator());
      // but it destroyed our addresses, restore 'em:
      sli_802154phy_set_eui64();
      sli_802154phy_radio_set_node_id(sli_802154mac_radio_get_node_id());
      sli_802154phy_radio_set_pan_id(sli_802154phy_radio_get_pan_id());
      // and it probably changed our AutoAck pause config, restore it:
      sli_802154mac_radio_enable_auto_ack(sli_802154phy_radio_auto_ack_enabled());

      // and we need to apply new subband's CCA threshold,
      // *after* emRadioChannelPageInUse has been updated
      // becuase that selects the proper slot in edCcaThresholds[].
      emRadioSetEdCcaThreshold(emRadioGetEdCcaThreshold());

      // Finally, reawaken the radio if it was on before to restart listening
      efr32WakeUp(origPowerStatus);
      // efr32WakeUp() effectively did:
      //   emRadioEnableReceiveCrc(emRadioReceiveCrcEnabled());
    }
  }

  // Apply emSynthFreqOffset -- very chip-specific!  On EFR32,
  // the CALOFFSET field is 15-bit signed while emSynthFreqOffset is
  // 8-bit signed, which is mapped into the upper 8 bits of CALOFFSET.
  RAIL_SetFreqOffset(emPhyRailHandle, ((int16_t)emSynthFreqOffset) << 7);
  return SL_STATUS_OK;
}

// Returns the channel the physical layer is currently tuned on
uint8_t sli_802154phy_get_phy_radio_channel(void)
{
  return emRadioChannel;
}

int8_t sli_802154phy_get_phy_radio_power(void)
{
#define PHY_UTILS_SIGNED_DIV_AND_ROUND(a, b) \
  (((a) + ((b) / ((((a) < 0) == ((b) < 0)) ? 2 : -2))) / (b))
  // This can never result in a division by 0. The result of the logic in parentheses
  // will be a division by 2 or -2, and PA_SCALING_FACTOR is defined to be 10.
  return PHY_UTILS_SIGNED_DIV_AND_ROUND(sli_802154phy_get_phy_radio_powerFl(), PA_SCALING_FACTOR);
}

sl_status_t sli_802154phy_set_phy_radio_power(int8_t power)
{
  return sli_802154phy_set_phy_radio_powerFl((int16_t)power * PA_SCALING_FACTOR);
}

int16_t sli_802154phy_get_phy_radio_powerFl(void)
{
  if (radioInited) {
    return RAIL_GetTxPowerDbm(emPhyRailHandle);
  }
  return emRadioPowerFl;
}

sl_status_t sli_802154phy_set_phy_radio_powerFl(int16_t power)
{
  if ((power < (MIN_RADIO_POWER * PA_SCALING_FACTOR))
      || (power > (MAX_RADIO_POWER * PA_SCALING_FACTOR))) {
    return SL_STATUS_TRANSMIT_INVALID_POWER;
  }
  // Next enforce channel's power limits per band configurations.
  // Here we don't return an error, but peg setting to the limit.
  const EmPhySubBandConfig *powerLimit = phy2SubBandConfig;
  if ((powerLimit == NULL)
      || ((emRadioTxPowerMode & SL_ZIGBEE_TX_POWER_MODE_BAND_NO_LIMITS_MASK)
          == SL_ZIGBEE_TX_POWER_MODE_BAND_NO_LIMITS_VALUE)
      || (sli_legacy_mfglib_mode)) {
    // No further power limits apply to this channel
  } else
  if (power < (powerLimit->txMinPowerdBm * PA_SCALING_FACTOR)) {
    power = powerLimit->txMinPowerdBm * PA_SCALING_FACTOR;
  } else
  if (power > (powerLimit->txMaxPowerdBm * PA_SCALING_FACTOR)) {
    power = powerLimit->txMaxPowerdBm * PA_SCALING_FACTOR;
  }

  if (radioInited) {
    // Can actually change power
    PHY_ATTEST_RAIL(RAIL_SetTxPowerDbm(emPhyRailHandle, power));
  } else {
    // Defer to sli_802154phy_radio_init() time
  }
  emRadioPowerFl = power; // Record desired power setting
  return SL_STATUS_OK;
}

uint32_t emPhyGetChannelFreqHz(uint8_t macPgChan)
{
 #if     MAC_HAS_CHANNEL_PAGES
  uint8_t phyChan = sli_802154phy_get_phy_channel(macPgChan); // Pass unmapped form
  if (phyChan == INVALID_CHANNEL) {
    return PHY_INVALID_FREQ_HZ;
  }
 #endif//MAC_HAS_CHANNEL_PAGES
  uint8_t macPage = sli_802154phy_get_channel_page_for_channel(macPgChan);
  switch (macPage) {
    case 0:
   #if     (SL_ZIGBEE_MIN_802_15_4_CHANNEL_NUMBER < 11)
      if (macPgChan == 0) {
        return (SYNTH_RF_FREQUENCY_868MHZ
                + (SYNTH_CHANNEL_SPACING_868MHZ * (macPgChan - 0)));
      } else
      if (macPgChan <= 10) {
        return (SYNTH_RF_FREQUENCY_915MHZ
                + (SYNTH_CHANNEL_SPACING_915MHZ * (macPgChan - 1)));
      } else
   #endif//(SL_ZIGBEE_MIN_802_15_4_CHANNEL_NUMBER < 11)
      if ((macPgChan >= 11) && (macPgChan <= 26)) {
        return (SYNTH_RF_FREQUENCY_24GHZ
                + (SYNTH_CHANNEL_SPACING_24GHZ * (macPgChan - 11)));
      }
      break;
 #if     (MAC_HAS_CHANNEL_PAGES && IEEE802154_GB868_SUPPORTED)
    case 28: //  0..26 in 863 MHz band
    case 29: // 27..34,62
    case 30: // 35..61
      return (SYNTH_RF_FREQUENCY_GB868_868MHZ
              + SYNTH_CHANNEL_SPACING_GB868 * (phyChan));
      break;
    case 31: //  0..26 in 915 MHz band
      return (SYNTH_RF_FREQUENCY_GB868_915MHZ
              + SYNTH_CHANNEL_SPACING_GB868 * (phyChan));
      break;
 #endif//(MAC_HAS_CHANNEL_PAGES && IEEE802154_GB868_SUPPORTED)
    default:
      break;
  }
  return PHY_INVALID_FREQ_HZ; // Sentinal for illegal channel
}

uint8_t sli_802154phy_get_phy_channel(uint8_t macPgChan)
{
  // Must have a PA suitable for this channel, and we use
  // mapMacPgChanToPhyChanBand() to validate the channel,
  // but can't use its phyChan which is for PRO2+
  // so must calculate efr32's on our own:
  if ((getPaConfig(macPgChan) != NULL)
      && (mapMacPgChanToPhyChanBand(macPgChan, NULL, NULL, NULL) != NULL)) {
    uint8_t phyChan = sli_802154mac_pg_chan_ch(macPgChan);
    if (phyChan < MAX_CHANNELS_PER_PAGE) {
      uint8_t macPage = sli_802154phy_get_channel_page_for_channel(macPgChan);
      switch (macPage) {
        case 0:
          if ((SL_ZIGBEE_MIN_802_15_4_CHANNEL_NUMBER == 0)  // Avoids IAR pointless warning
              || (phyChan >= SL_ZIGBEE_MIN_802_15_4_CHANNEL_NUMBER)) {
            return phyChan;
          }
          break;
     #if     (MAC_HAS_CHANNEL_PAGES && IEEE802154_GB868_SUPPORTED)
        case 29: // 27..34,62
          phyChan += 27;
          if (phyChan == 35) {
            phyChan = 62;
          }
        // Fall through
        case 28: //  0..26 in 863 MHz band
        case 31: //  0..26 in 915 MHz band
          return phyChan;
          break;
        case 30: // 35..61
          return (phyChan + 35);
          break;
     #endif//(MAC_HAS_CHANNEL_PAGES && IEEE802154_GB868_SUPPORTED)
        default:
          break;
      }
    }
  }
  return INVALID_CHANNEL;
}

//------------------------------------------------------------------------------
// LQI APIs
//---------

#define PHY_MIN_RSSI -100
#define PHY_MIN_RSSI_ED (PHY_MIN_RSSI + 10) // 15.4: 10 dB above sensitivity
#define PHY_MAX_RSSI -36
#define PHY_MAX_ED   0xff
#define PHY_MAX_LQI  0xff

// No casts in this macro so it can be used for constant expressions
// Here rssi must be >= PHY_MIN_RSSI and < PHY_MAX_RSSI for a valid result
#define RSSI_TO_LQI(rssi) ((((rssi) - PHY_MIN_RSSI) * 256) \
                           / (PHY_MAX_RSSI - PHY_MIN_RSSI))
#define RSSI_TO_ED(rssi) ((((rssi) - PHY_MIN_RSSI_ED) * 256) \
                          / (PHY_MAX_RSSI - PHY_MIN_RSSI_ED))

// For EFR32, the RSSI range of [-100..-36] is mapped to an LQI range [0..255].
// These values are chosen in part to be compatible with EM35x divisions.
// Those were chosen too close together because of the very narrow dBm range of
// LQI values on that chip. These are spread apart more because that is
// better for differentiating link qualities, but not so much as to create
// a dramatic interoperability difference.
static uint8_t sli_802154phy_cost_divisions[] = {
//          >=RSSI                LQI  index  cost
//            ----                ---  -----  ----
  RSSI_TO_LQI(-80),          // =  80    0      1
  RSSI_TO_LQI(-84),          // =  64    1      3
  RSSI_TO_LQI(-88),          // =  48    2      5
  RSSI_TO_LQI(PHY_MIN_RSSI), // =   0    3      7
};

void emSetCostDivisions(const uint8_t *costDivisions)
{
  memcpy(sli_802154phy_cost_divisions, costDivisions, sizeof(uint8_t) * 4);
}

uint8_t sli_802154phy_cost_division(uint8_t cost)
{
  return sli_802154phy_cost_divisions[(cost >> 1)];
}

// For EFR32, LQI is done in PHY so the value in the appended info
// is the one we want to use.  So just let that value pass that straight
// through both sli_802154phy_radio_unpack_chip_errors() and sli_802154phy_radio_calculate_link_quality():
uint16_t sli_802154phy_radio_unpack_chip_errors(uint8_t internalChipErrors)
{
  return internalChipErrors;
}

uint8_t sli_802154phy_radio_calculate_link_quality(uint16_t unpackedChipErrors,
                                                   uint16_t packetLength)
{
  UNUSED_VAR(packetLength);
  return unpackedChipErrors;
}

uint8_t sli_802154phy_radio_convert_rssi_to_ed(int8_t rssiDbm)
{
  // Enforce Limits
  if (rssiDbm <= PHY_MIN_RSSI_ED) {
    return 0;
  }
  if (rssiDbm >= PHY_MAX_RSSI) {
    return PHY_MAX_ED;
  }
  return (uint8_t) RSSI_TO_ED((int16_t)rssiDbm);
}

static uint8_t emRadioConvertRssiToLqi(uint8_t lqi, int8_t rssiDbm)
{
  UNUSED_VAR(lqi);
  // Enforce Limits
  if (rssiDbm <= PHY_MIN_RSSI) {
    return 0;
  }
  if (rssiDbm >= PHY_MAX_RSSI) {
    return PHY_MAX_LQI;
  }
  return (uint8_t) RSSI_TO_LQI((int16_t)rssiDbm);
}

//------------------------------------------------------------------------------
// Transmit APIs
//--------------
static union {
  RAIL_CsmaConfig_t csmaConfig;
  RAIL_LbtConfig_t lbtConfig;
} txParams;

typedef enum {
  PRETXOP_USE_TX,
  PRETXOP_USE_CCA_CSMA,
  PRETXOP_USE_CCA_LBT,
} PreTxOp_t;
static PreTxOp_t preTxOp;
static RAIL_TxOptions_t txOptions;
static uint8_t txCcaTries;
static RAIL_SchedulerInfo_t txSchedulerInfo = {
  .priority = 0, //filled in later
  .slipTime = 500000,
  .transactionTime = 0
};

// Updates txParams{} and returns the number of CCA/LBT checks to perform
static uint8_t txCsmaTries(void)
{
  preTxOp = PRETXOP_USE_TX;
  if (!radioTransmitConfig.checkCca) {
    return 0;
  }
  // Even if we've requested CSMA, the Duty Cycle Pkt Mode may override that:
  EmPhyDcMode pktMode = emPhyDcGetPktMode();
  if ((pktMode == EM_PHY_DC_MODE_TALK)
      || (pktMode == EM_PHY_DC_MODE_DIALOG)) {
    return 0;
  }
  // Determine the CSMA/LBT parameters for this transmit
  if ((phy2SubBandParams == NULL)
      || (phy2SubBandConfig->ccaConfigArray == NULL)
      || (pktMode == EM_PHY_DC_MODE_CSMA)) {
    // If no EmPhySubBandParams{}, or Duty Cycle Mode is CSMA,
    // then the radioTransmitConfig{} CSMA applies:
    preTxOp = PRETXOP_USE_CCA_CSMA;
    txSchedulerInfo.transactionTime += CSMA_OVERHEAD;
    if (emRadioAlwaysUseZeroBackoff) {
      // This flag says to perform CCA checks, but with no preceding backoff.
      // The EFR32 hardware enforces a minimum of one backoff period regardless,
      // so that's the best we can do.  Even a fixed backoff of 0 likely results
      // in one backoff period, at least for the 1st CCA try.
      txParams.csmaConfig.csmaMinBoExp   = 0;
      txParams.csmaConfig.csmaMaxBoExp   = 0;
      txParams.csmaConfig.ccaBackoff     = RAIL_WARM_TIME_US;
    } else {
      txParams.csmaConfig.csmaMinBoExp   = radioTransmitConfig.backoffExponentMin;
      txParams.csmaConfig.csmaMaxBoExp   = radioTransmitConfig.backoffExponentMax;
      txParams.csmaConfig.ccaBackoff     = SL_ZIGBEE_PHY_SYMBOLS_TO_US(20);
      // EFR32 CSMA has no way to enforce radioTransmitConfig.minimumBackoff,
      // but it effectively already enforces a minimum of 1 backoff period
      // (though not initially if radio is already ON -- see RAIL_LIB-144).
    }
    txParams.csmaConfig.csmaTries        = radioTransmitConfig.ccaAttemptMax;
    txParams.csmaConfig.ccaDuration      = SL_ZIGBEE_PHY_SYMBOLS_TO_US(8);
    txParams.csmaConfig.csmaTimeout      = 0; // No overall CSMA timeout
    txParams.csmaConfig.ccaThreshold     = emRadioGetEdCcaThreshold();
  } else {
    // Otherwise the EmPhySubBandParams{} LBT/CSMA applies:
    if (phy2SubBandConfig->ccaConfigArray == emPhyCcaLbtConfigArray) {
      // SubBand specifies ETSI LBT
      preTxOp = PRETXOP_USE_CCA_LBT;
      txSchedulerInfo.transactionTime += LBT_OVERHEAD;
      if (emRadioAlwaysUseZeroBackoff) {
        // This flag says to perform CCA checks, but with no preceding backoff.
        // The EFR32 hardware enforces a minimum of one backoff period regardless,
        // so that's the best we can do.  Even a fixed backoff of 0 likely results
        // in one backoff period, at least for the 1st CCA try.
        txParams.lbtConfig.lbtMinBoRand = 0;
        txParams.lbtConfig.lbtMaxBoRand = 0;
        txParams.lbtConfig.lbtBackoff   = RAIL_WARM_TIME_US;
      } else {
        txParams.lbtConfig.lbtMinBoRand = phy2SubBandParams->lbtMinBackoffs;
        txParams.lbtConfig.lbtMaxBoRand = phy2SubBandParams->lbtMinBackoffs
                                          + (1u << phy2SubBandParams->lbtBackoffMaxExp)
                                          - 1
                                          + phy2SubBandParams->lbtBackoffAdjust;
        txParams.lbtConfig.lbtBackoff   = SL_ZIGBEE_PHY_SYMBOLS_TO_US(phy2SubBandParams->lbtBackoffSym);
      }
      txParams.lbtConfig.lbtTries       = phy2SubBandParams->lbtTries;
      txParams.lbtConfig.lbtDuration    = SL_ZIGBEE_PHY_SYMBOLS_TO_US(phy2SubBandParams->lbtCcaSym);
      txParams.lbtConfig.lbtTimeout     = SL_ZIGBEE_PHY_SYMBOLS_TO_US(phy2SubBandParams->lbtTimeoutSym);
      txParams.lbtConfig.lbtThreshold   = emRadioGetEdCcaThreshold();
    } else {
      // SubBand specifies CSMA
      preTxOp = PRETXOP_USE_CCA_CSMA;
      txSchedulerInfo.transactionTime += CSMA_OVERHEAD;
      if (emRadioAlwaysUseZeroBackoff) {
        // This flag says to perform CCA checks, but with no preceding backoff.
        // The EFR32 hardware enforces a minimum of one backoff period regardless,
        // so that's the best we can do.  Even a fixed backoff of 0 likely results
        // in one backoff period, at least for the 1st CCA try.
        txParams.csmaConfig.csmaMinBoExp = 0;
        txParams.csmaConfig.csmaMaxBoExp = 0;
        txParams.csmaConfig.ccaBackoff   = RAIL_WARM_TIME_US;
      } else {
        txParams.csmaConfig.csmaMinBoExp = phy2SubBandParams->lbtBackoffMinExp;
        txParams.csmaConfig.csmaMaxBoExp = phy2SubBandParams->lbtBackoffMaxExp;
        txParams.csmaConfig.ccaBackoff   = SL_ZIGBEE_PHY_SYMBOLS_TO_US(phy2SubBandParams->lbtBackoffSym);
        // EFR32 CSMA has no way to enforce phy2SubBandParams->lbtMinBackoffs
        // but it effectively already enforces a minimum of 1 backoff period
        // (though not initially if radio is already ON -- see RAIL_LIB-144).
      }
      txParams.csmaConfig.csmaTries      = phy2SubBandParams->lbtTries;
      txParams.csmaConfig.ccaDuration    = SL_ZIGBEE_PHY_SYMBOLS_TO_US(phy2SubBandParams->lbtCcaSym);
      txParams.csmaConfig.csmaTimeout    = SL_ZIGBEE_PHY_SYMBOLS_TO_US(phy2SubBandParams->lbtTimeoutSym);
      txParams.csmaConfig.ccaThreshold   = emRadioGetEdCcaThreshold();
    }
  }
  // If ccaAttemptMax is negative, that is a sentinal indicating
  // the first try should *not* be preceded by a random backoff period.
  // ccaTries is abs(ccaAttemptMax).
  int8_t ccaTries = txParams.csmaConfig.csmaTries; // aka .lbtConfig.lbtTries
  if (ccaTries < 0) {
    ccaTries = -ccaTries;
  }
  return (uint8_t) ccaTries;
}

#if     (!PHY_USE_PREPARE_TX_API)
static // Let the WEAK one be used by outside callers
#endif//(!PHY_USE_PREPARE_TX_API)
sl_status_t emRadioPrepareTransmit(uint8_t *memptr)
{
  uint8_t pktLen = memptr[0];

  if (txState != PHY_TX_STATE_OFF) {
    return SL_STATUS_TRANSMIT_BUSY;
  }

  txOptions = RAIL_TX_OPTIONS_NONE; // Assume no options to begin with

  // configure LBT
  txCcaTries = txCsmaTries();
  if (txCcaTries > 0) {
  }

  txSchedulerInfo.priority = radioSchedulerPriorityTable.min_tx_priority;
  // calculate scheduler transactionTime
  txSchedulerInfo.transactionTime
    = SL_ZIGBEE_PHY_PACKET_OVERHEAD
      + SL_ZIGBEE_PHY_BYTES_TO_US(SL_ZIGBEE_PHY_MAX_CRC_BYTES)
      + SL_ZIGBEE_PHY_BYTES_TO_US(pktLen + PHRLen);

  if (radioTransmitConfig.appendCrc) {
    pktLen -= 2; // Compensate for not loading CRC bytes into BUFC
  } else {
    txOptions |= RAIL_TX_OPTION_REMOVE_CRC;
  }

  // check if we need to wait for an ack that was requested
  if (!sli_legacy_mfglib_mode && radioTransmitConfig.waitForAck && (memptr[1] & 0x20)) {
    txOptions |= RAIL_TX_OPTION_WAIT_FOR_ACK;
    txState = PHY_TX_STATE_PREP_W_ACK;
    //@FIXME: Not all vendors start their ACK at the turnaround time boundary!
    txSchedulerInfo.transactionTime
      += SL_ZIGBEE_PHY_SYMBOLS_TO_US(SL_ZIGBEE_PHY_TURNAROUND_SYMBOLS)
         + SL_ZIGBEE_PHY_ACK_DURATION;
  } else {
    txState = PHY_TX_STATE_PREP_NOACK;
  }

  // Translate Tx antenna diversity mode into RAIL Tx Antenna options:
  // If enabled, use the currently-selected antenna, otherwise leave
  // both options 0 so Tx antenna tracks Rx antenna.
  if (sl_rail_util_ant_div_get_tx_antenna_mode() != SL_RAIL_UTIL_ANTENNA_MODE_DISABLED) {
    txOptions |= ((sl_rail_util_ant_div_get_tx_antenna_selected() == SL_RAIL_UTIL_ANTENNA_SELECT_ANTENNA1)
                  ? RAIL_TX_OPTION_ANTENNA0 : RAIL_TX_OPTION_ANTENNA1);
  }

  efr32WakeUp(SL_ZIGBEE_RADIO_POWER_MODE_TX_ON); // Ensure radio is on for Tx

#if      (MAC_HAS_CHANNEL_PAGES && IEEE802154_GB868_SUPPORTED)
  if (PHRLen == 2) {
    // Unfortunately we can't muck with the caller's memptr[] packet
    // to replace its 1-byte PHR with 2-byte 15.4G PHR, so we have to
    // resort to using a separate buffer and copy the packet into it.
    static uint8_t txBuffer[256];
    PHY_ASSERT((size_t)(pktLen + 2U) <= sizeof(txBuffer));
    txBuffer[0] = (0x08 /*FCS=2byte*/ | 0x10 /*Whiten=enabled*/);
    txBuffer[1] = (uint8_t)(__RBIT(memptr[0]) >> 24);
    memcpy(&txBuffer[2], &memptr[1], pktLen);
    memptr = txBuffer;
  }
#endif//(MAC_HAS_CHANNEL_PAGES && IEEE802154_GB868_SUPPORTED)

  // get the packet into the radio the quick way
  // We 'lie' about the buffer size to guarantee RAIL will send the
  // full pktLen + PHRLen and not think the buffer is smaller than that.
  // RAIL won't actually access any data beyond pktLen + PHRLen.
  PHY_ATTEST(RAIL_SetTxFifo(emPhyRailHandle, memptr, pktLen + PHRLen, 256)
             >= pktLen + PHRLen);
  return SL_STATUS_OK;
}

// emRadioTransmit
// This function appends CRC; caller must set length byte to accommodate CRC
// bytes.
sl_status_t emRadioTransmit(uint8_t *memptr)
{
  sl_status_t status = SL_STATUS_OK;
  switch (txState) {
    case PHY_TX_STATE_OFF:       // No Tx has been prepared, so prepare it
      status = emRadioPrepareTransmit(memptr);
      if (status != SL_STATUS_OK) {
        (void) halPtaStackEvent(SL_RAIL_UTIL_IEEE802154_STACK_EVENT_TX_BLOCKED, 0U);
        return status;
      }
      break;
    case PHY_TX_STATE_PREP_W_ACK: // Already prepared, so proceed
    case PHY_TX_STATE_PREP_NOACK:

      break;
    case PHY_TX_STATE_CANC_W_ACK: // Cancelled in preparation, so abort it
    case PHY_TX_STATE_CANC_NOACK: // Cancelled in preparation, so abort it
      (void) halPtaStackEvent(SL_RAIL_UTIL_IEEE802154_STACK_EVENT_TX_BLOCKED,
                              (uint32_t) (txState == PHY_TX_STATE_CANC_W_ACK));
      txState = PHY_TX_STATE_OFF;
      RAIL_YieldRadio(emPhyRailHandle);
      return SL_STATUS_TRANSMIT_BLOCKED;
      break;
    default:                     // Tx already in progress -- oops!!
      (void) halPtaStackEvent(SL_RAIL_UTIL_IEEE802154_STACK_EVENT_TX_BLOCKED, 0U);
      return SL_STATUS_TRANSMIT_BUSY;
      break;
  }

  // Tx has been prepared, try to send it out!
  // We must go ATOMIC to re-check txState which should be one of the
  // PREPared states but actually could have been cancelled from an ISR,
  // and to initiate the transmit without interruption.
  DECLARE_INTERRUPT_STATE;
  DISABLE_INTERRUPTS();
  if ((txState == PHY_TX_STATE_PREP_NOACK)
      || (txState == PHY_TX_STATE_PREP_W_ACK)) {
    RAIL_Status_t result;

    // Set this up before starting Tx since those can asynchronously error in
    // multiprotocol versions
    txState = (txState_t) -txState;   // PREP to TRIG equivalent
    lastTxStartedTimeMs = (int32_t) halCommonGetInt16uMillisecondTick();

    // Ensure frame detection is enabled to catch ACKs and polled data!
    // In theory this is needed only for sleepy ACK-requesting transmits
    // but it's harmless and quick to do unconditionally.
    RxFrameDetectEnable();

    if (preTxOp == PRETXOP_USE_TX) {
      result = RAIL_StartTx(emPhyRailHandle,
                            sli_802154phy_get_phy_radio_channel(),
                            txOptions,
                            &txSchedulerInfo);
      if (result == RAIL_STATUS_NO_ERROR) {
        (void) halPtaStackEvent(SL_RAIL_UTIL_IEEE802154_STACK_EVENT_TX_STARTED, 0U);
      }
    } else if (preTxOp == PRETXOP_USE_CCA_CSMA) {
      result = RAIL_StartCcaCsmaTx(emPhyRailHandle,
                                   sli_802154phy_get_phy_radio_channel(),
                                   txOptions,
                                   &txParams.csmaConfig,
                                   &txSchedulerInfo);
    } else { // must be PRETXOP_USE_CCA_LBT
      result = RAIL_StartCcaLbtTx(emPhyRailHandle,
                                  sli_802154phy_get_phy_radio_channel(),
                                  txOptions,
                                  &txParams.lbtConfig,
                                  &txSchedulerInfo);
    }
    if (result != RAIL_STATUS_NO_ERROR) {
      PHY_ASSERT_INTERNAL(false);
      // We don't expect a transmit to be ongoing here
      txState = PHY_TX_STATE_OFF; // Attempt to recover by resetting state
      lastTxStartedTimeMs = -1;
      status = SL_STATUS_TRANSMIT_BUSY;
      RAIL_YieldRadio(emPhyRailHandle);
      (void) halPtaStackEvent(SL_RAIL_UTIL_IEEE802154_STACK_EVENT_TX_BLOCKED, 0U);
    }
  } else {
    (void) halPtaStackEvent(SL_RAIL_UTIL_IEEE802154_STACK_EVENT_TX_BLOCKED,
                            (uint32_t) (txState == PHY_TX_STATE_CANC_W_ACK));
    txState = PHY_TX_STATE_OFF;   // In case it was a CANC state
    status = SL_STATUS_TRANSMIT_BLOCKED;
    RAIL_YieldRadio(emPhyRailHandle);
  }
  RESTORE_INTERRUPTS();
  return status;
}

bool emPhyIsTransmitting(void)
{
  // Radio has a transmit in progress (including during CSMA phase),
  // or is waiting for that transmit's ACK.  Does not include sending
  // an ACK in resposne to an incoming frame.
  return (txState != PHY_TX_STATE_OFF);
}

void emPhyCancelTransmit(void)
{
  DECLARE_INTERRUPT_STATE;
  DISABLE_INTERRUPTS();
  if (txState < PHY_TX_STATE_OFF) {
    // Cancel prepared Tx, and don't allow it to go out
    if (txState == PHY_TX_STATE_PREP_W_ACK) {
      txState = PHY_TX_STATE_CANC_W_ACK;
    } else if (txState == PHY_TX_STATE_PREP_NOACK) {
      txState = PHY_TX_STATE_CANC_NOACK;
    } else {
      // Tx was redundantly cancelled, already in a CANCelled state
    }
    RESTORE_INTERRUPTS();
  } else
  if ((txState > PHY_TX_STATE_OFF) && (txState < PHY_TX_STATE_TRIG_ACKING)) {
    // Do what emRadioSetPowerStatus(OFF) followed by
    // emRadioSetPowerStatus(idleState) would do, but
    // without messing with emRadioPowerFem().
    sli_802154phy_radio_sleep(); // Will transiton txState to OFF
    RESTORE_INTERRUPTS();
    efr32WakeUp(radioIdlePowerState); // no-op if idleState is OFF
  } else {
    RESTORE_INTERRUPTS();
  }
}

uint16_t emGetTxPowerMode(void)
{
  return emRadioTxPowerMode;
}

//------------------------------------------------------------------------------
// Power State APIs
//-----------------

void sli_802154phy_radio_sleep(void)
{
  // sli_802154phy_radio_sleep() can often be called from interrupt context.
  // Regardless, shut down event processing at this point.
  DECLARE_INTERRUPT_STATE;
  DISABLE_INTERRUPTS();
  if (radioCurrentPowerStatus == SL_ZIGBEE_RADIO_POWER_MODE_OFF) {
    // Nothing to do!
    RESTORE_INTERRUPTS();
    return;
  }
  // Rx: Shut down future Rx then cancel any Rx in progress.
  // Tx: Cancel any Tx in progress, including any LBT in progress
  RAIL_Idle(emPhyRailHandle, RAIL_IDLE_ABORT, true);
  RxFrameDetectDisable();
  // This does what RAIL_ConfigAutoAck() would do when just changing
  // its success states to RAIL_RF_STATE_IDLE:
  RAIL_StateTransitions_t stateTransitions = {
    .success = RAIL_RF_STATE_TX,
    .error = RAIL_RF_STATE_IDLE,
  };
  PHY_ATTEST_RAIL(RAIL_SetRxTransitions(emPhyRailHandle, &stateTransitions));
  stateTransitions.success = RAIL_RF_STATE_RX;
  PHY_ATTEST_RAIL(RAIL_SetTxTransitions(emPhyRailHandle, &stateTransitions));
  RAIL_CancelTimer(emPhyRailHandle);
  // Cancel any PTA requests
  (void) halPtaStackEvent(SL_RAIL_UTIL_IEEE802154_STACK_EVENT_TX_IDLED, 0U);
  (void) halPtaStackEvent(SL_RAIL_UTIL_IEEE802154_STACK_EVENT_RX_IDLED, 0U);
  // Set radioCurrentPowerStatus OFF *before* potentially issuing
  // transmit-complete callback to upper layers, lest they call
  // sli_802154phy_radio_ok_to_idle() and end up back here recursively.
  radioCurrentPowerStatus = SL_ZIGBEE_RADIO_POWER_MODE_OFF;
  // Ensure our Tx State machine (and the MAC's) isn't left hung.
  // This is done with interrupts disabled because the callbacks it might
  // trigger are usually made from interrupt context.
  switch (txState) {
    case PHY_TX_STATE_OFF:
      sli_802154phy_radio_tx_ack_isr_callback(); // Terminate data poll (no-op if none active)
      break;
    case PHY_TX_STATE_CANC_W_ACK:
    case PHY_TX_STATE_CANC_NOACK:
      break;
    case PHY_TX_STATE_PREP_W_ACK:
      txState = PHY_TX_STATE_CANC_W_ACK;   // Cancel prepared transmit
      break;
    case PHY_TX_STATE_PREP_NOACK:
      txState = PHY_TX_STATE_CANC_NOACK;   // Cancel prepared transmit
      break;
    case PHY_TX_STATE_TRIG_NOACK:
    case PHY_TX_STATE_TRIG_W_ACK:
    case PHY_TX_STATE_TRIG_ACKING:
      // N.B. Even in ACKING state we use TX_INCOMPLETE rather than
      //      NO_ACK_RECEIVED because the latter could trigger a
      //      MAC retry from within the callback -- we're assuming
      //      someone calling sli_802154phy_radio_sleep() during a Tx-in-progress
      //      doesn't want a Tx immediately restarted (and the radio
      //      awoken) before returning from sli_802154phy_radio_sleep()!
      MACSEQ_TransmitErrorEvent(SL_STATUS_TRANSMIT_INCOMPLETE); // Will advance txState to OFF
      break;
    default:
      PHY_ASSERT(false);
      break;
  }
  RESTORE_INTERRUPTS();
}

static void efr32WakeUp(RadioPowerMode mode)
{
  if (radioCurrentPowerStatus != mode) {
    // The following mode transitions are possible here:
    //   OFF   => TX_ON or ED_ON or RX_ON
    //   TX_ON => ED_ON or RX_ON
    //   ED_ON => RX_ON (TX_ON promoted to RX_ON)
    //   RX_ON => ED_ON (TX_ON ignored)
    RAIL_StateTransitions_t stateTransitions = {
      .success = RAIL_RF_STATE_TX,
      .error = RAIL_RF_STATE_RX,
    };
    switch (radioCurrentPowerStatus) {
      case SL_ZIGBEE_RADIO_POWER_MODE_OFF: // => TX_ON or ED_ON or RX_ON
        // Tell RAIL its post-Tx state is RX rather than IDLE in case
        // the TX we're waking to send is a Data Poll.  If it's not,
        // then sli_802154phy_radio_ok_to_idle() will IDLE us eventually.
        // This does what RAIL_ConfigAutoAck() would do when just changing
        // its defaultState to RAIL_RF_STATE_RX, but with the added ability
        // to disable CRC checking if so configured:
        PHY_ATTEST_RAIL(RAIL_SetRxTransitions(emPhyRailHandle, &stateTransitions));
        stateTransitions.success = RAIL_RF_STATE_RX;
        PHY_ATTEST_RAIL(RAIL_SetTxTransitions(emPhyRailHandle, &stateTransitions));
        PHY_ATTEST_RAIL(RAIL_ConfigRxOptions(emPhyRailHandle,
                                             RAIL_RX_OPTION_IGNORE_CRC_ERRORS,
                                             (radioCheckCrc
                                              ? 0
                                              : RAIL_RX_OPTION_IGNORE_CRC_ERRORS)));
        radioCurrentPowerStatus = SL_ZIGBEE_RADIO_POWER_MODE_TX_ON;
        if (mode == SL_ZIGBEE_RADIO_POWER_MODE_TX_ON) {
          break;
        }
      // Fall through

      case SL_ZIGBEE_RADIO_POWER_MODE_TX_ON: // => ED_ON or RX_ON
        // Enable receiver for CCAs but don't enable packet reception
        RxFrameDetectDisable();
        radioCurrentPowerStatus = SL_ZIGBEE_RADIO_POWER_MODE_ED_ON;
        PHY_ATTEST_RAIL(RAIL_StartRx(emPhyRailHandle,
                                     emRadioChannel,
                                     NULL));
        if (mode == SL_ZIGBEE_RADIO_POWER_MODE_ED_ON) {
          break;
        }
      // Fall through

      case SL_ZIGBEE_RADIO_POWER_MODE_ED_ON: // => RX_ON (TX_ON promotes to RX_ON)
        if ((mode == SL_ZIGBEE_RADIO_POWER_MODE_RX_ON)
            || (mode == SL_ZIGBEE_RADIO_POWER_MODE_TX_ON)) {
          radioCurrentPowerStatus = SL_ZIGBEE_RADIO_POWER_MODE_RX_ON;
          RxFrameDetectEnable();
        }
        break;

      case SL_ZIGBEE_RADIO_POWER_MODE_RX_ON: // => ED_ON (TX_ON ignored)
        if (mode == SL_ZIGBEE_RADIO_POWER_MODE_ED_ON) {
          RxFrameDetectDisable();
          radioCurrentPowerStatus = SL_ZIGBEE_RADIO_POWER_MODE_ED_ON;
        }
        break;

      default:
        PHY_ASSERT(false);
        break;
    }
  }
}

void sli_802154phy_radio_wake_up(void)
{
  efr32WakeUp(SL_ZIGBEE_RADIO_POWER_MODE_RX_ON);
}

RadioPowerMode sli_802154phy_radio_get_power_status(void)
{
  return radioCurrentPowerStatus;
}

static void emRadioSetPowerStatus(RadioPowerMode newState)
{
  switch (newState) {
    case SL_ZIGBEE_RADIO_POWER_MODE_OFF:
      sli_802154phy_radio_sleep();
      emRadioPowerFem(false);
      break;
    case SL_ZIGBEE_RADIO_POWER_MODE_TX_ON:
    case SL_ZIGBEE_RADIO_POWER_MODE_ED_ON:
    case SL_ZIGBEE_RADIO_POWER_MODE_RX_ON:
      emRadioPowerFem(true);
      efr32WakeUp(newState);
      break;
    default:
      PHY_ASSERT(false);
      break;
  }
}

void sli_802154phy_radio_ok_to_idle(void)
{
  RAIL_YieldRadio(emPhyRailHandle);
  // Cancel any prepared Tx that never went out
  if (txState < PHY_TX_STATE_OFF) {
    // Clean up global state modified from Prepare phase that is
    // no longer relevant because we can't complete this transmit.
    txState = PHY_TX_STATE_OFF;
  }
  if (radioIdlePowerState == SL_ZIGBEE_RADIO_POWER_MODE_OFF) {
    sli_802154phy_radio_sleep();
  }
}

RadioPowerMode sli_802154phy_radio_get_idle_mode(void)
{
  return radioIdlePowerState;
}

sl_status_t sli_802154phy_radio_set_idle_mode(RadioPowerMode mode)
{
  if (mode > SL_ZIGBEE_RADIO_POWER_MODE_ED_ON) {
    return SL_STATUS_FAIL;
  }
  radioIdlePowerState = mode;
  if (radioIdlePowerState != radioCurrentPowerStatus) {
    emRadioSetPowerStatus(radioIdlePowerState);
  }
  return SL_STATUS_OK;
}

static void updateEvents(RAIL_Events_t mask, RAIL_Events_t values)
{
  RAIL_Events_t newEventConfig = (currentEventConfig & ~mask) | (values & mask);
  if (newEventConfig != currentEventConfig) {
    PHY_ATTEST_RAIL(RAIL_ConfigEvents(emPhyRailHandle, mask, values));
    currentEventConfig = newEventConfig;
  }
}

static void changeDynamicEvents(void)
{
  if (!radioInited) {
    return; // Defer to sli_802154phy_radio_init() time
  }
  // sfdRecvNotification needs the following events:
  // - RX_SYNC1_DETECT     (not normally enabled; enable/disable here)
  //
  // ActiveRx priority needs the following events:
  // - RX_SYNC1_DETECT     (not normally enabled; enable/disable here)
  // - RX_FRAME_ERROR      (not normally enabled; enable/disable here)
  // - RX_FIFO_OVERFLOW    (not normally enabled; enable/disable here)
  // - RX_PACKET_ABORTED   (not normally enabled; enable/disable here)
  // - RX_ADDRESS_FILTERED (not normally enabled; enable/disable here)
  // - RX_PACKET_RECEIVED  (always enabled)
  //
  // Rx PTA needs the following events:
  // - RX_SYNC1_DETECT     (not normally enabled; enable/disable here)
  // - RX_FRAME_ERROR      (not normally enabled; enable/disable here)
  // - RX_FIFO_OVERFLOW    (not normally enabled; enable/disable here)
  // - RX_PACKET_ABORTED   (not normally enabled; enable/disable here)
  // - RX_ADDRESS_FILTERED (not normally enabled; enable/disable here)
  // - RX_FILTER_PASSED    (not normally enabled; enable/disable here)
  // - RX_PACKET_RECEIVED  (always enabled)
  //
  // Tx PTA needs the following events:
  // - TX_START_CCA        (not normally enabled; enable/disable here)
  // - TX_CCA_RETRY        (not normally enabled; enable/disable here)
  // - TX_CHANNEL_CLEAR    (not normally enabled; enable/disable here)
  // - TX_CHANNEL_BUSY     (always enabled)
  // - TX_UNDERFLOW        (always enabled)
  // - TX_ABORTED          (always enabled)
  // - TX_BLOCKED          (always enabled)
  // - TX_PACKET_SENT      (always enabled)
  // - TXACK_UNDERFLOW     (always enabled)
  // - TXACK_ABORTED       (always enabled)
  // - TXACK_BLOCKED       (always enabled)
  // - TXACK_PACKET_SENT   (always enabled)
  //
  // Notice the RX and TX PTA events enabled/disabled here as a group are
  // a proper superset of all the other use cases; the code below assumes
  // this. Additionally, note that the events here are strictly mutually
  // exclusive with the events enabled in the call to updateEvents within
  // sli_802154phy_radio_init. This exclusivity must be maintained in order to not disable
  // events the the app wants on regardless of which features are enabled.
  const RAIL_Events_t eventMask = RAIL_EVENTS_NONE
                                  | RAIL_EVENT_RX_SYNC1_DETECT
                                  | RAIL_EVENT_RX_FRAME_ERROR
                                  | RAIL_EVENT_RX_FIFO_OVERFLOW
                                  | RAIL_EVENT_RX_ADDRESS_FILTERED
                                  | RAIL_EVENT_RX_PACKET_ABORTED
                                  | RAIL_EVENT_RX_FILTER_PASSED
                                  | RAIL_EVENT_TX_CHANNEL_CLEAR
                                  | RAIL_EVENT_TX_CCA_RETRY
                                  | RAIL_EVENT_TX_START_CCA
  ;
  RAIL_Events_t eventValues = RAIL_EVENTS_NONE;

  if (ptaEnabled) {
    eventValues |= eventMask;
  } else {
    if (sfdRecvNotificationEnabled) {
      eventValues |= RAIL_EVENT_RX_SYNC1_DETECT;
    }
    if (radioSchedulerPriorityTable.background_rx
        != radioSchedulerPriorityTable.active_rx) {
      eventValues |= (RAIL_EVENT_RX_SYNC1_DETECT
                      | RAIL_EVENTS_RX_COMPLETION);
    }
  }

  updateEvents(eventMask, eventValues);
}

// This API should only be called prior to sli_802154phy_radio_init() or when
// the radio is OFF -- otherwise its new priorities won't be put
// into effect until the next time the radio gets turned on.
sl_status_t sli_802154_stack_radio_set_scheduler_priorities(const sl_802154_radio_priorities_t *priorities)
{
  radioSchedulerPriorityTable = *priorities;
  changeDynamicEvents();
  return SL_STATUS_OK;
}

sl_status_t sli_802154_stack_radio_get_scheduler_priorities(sl_802154_radio_priorities_t *priorities)
{
  *priorities = radioSchedulerPriorityTable;
  return SL_STATUS_OK;
}

sl_status_t sli_802154_stack_radio_set_scheduler_sliptime(uint32_t slipTime)
{
  txSchedulerInfo.slipTime = slipTime;
  return SL_STATUS_OK;
}

sl_status_t sli_802154_stack_radio_get_scheduler_sliptime(uint32_t *slipTime)
{
  if (slipTime != NULL) {
    *slipTime = txSchedulerInfo.slipTime;
  }
  return SL_STATUS_OK;
}

void emRadioEnablePta(bool enable)
{
  ptaEnabled = enable;
  if (!radioInited) {
    return; // Defer to sli_802154phy_radio_init() time
  }

  changeDynamicEvents();

  // When PTA is enabled, we want to negate PTA_REQ as soon as an incoming
  // frame is aborted, e.g. due to filtering.  To do that we must turn off
  // the TRACKABFRAME feature that's normally on to benefit sniffing on PTI.
  // That feature has the unfortunate side effect of delaying RXABORT
  // notification until the full packet has been received, even tho it
  // won't be processed.
  PHY_ATTEST_RAIL(RAIL_ConfigRxOptions(emPhyRailHandle,
                                       RAIL_RX_OPTION_TRACK_ABORTED_FRAMES,
                                       (enable)
                                       ? 0
                                       : RAIL_RX_OPTION_TRACK_ABORTED_FRAMES));
}

static RAILSched_Config_t railSchedState;

static RAIL_Config_t railCfg = {
  .eventsCallback = &RAILCb_Events,
  .scheduler = &railSchedState,
};

void sli_802154phy_radio_init(RadioPowerMode initialRadioPowerMode)
{
  PHY_ASSERT(initialRadioPowerMode <= SL_ZIGBEE_RADIO_POWER_MODE_OFF);

  // sli_802154phy_radio_init() is generally called upon bootup and deep sleep wakeup.
  // On EFR32, unlike EM3xx, the radio retains all its registers thru
  // deep sleep, so we only need to really init everything once.
  if (!radioInited) {
    PHY_ASSERT(radioCurrentPowerStatus == SL_ZIGBEE_RADIO_POWER_MODE_OFF);

    emPhyRailHandle = RAIL_Init(&railCfg, NULL);
    PHY_ASSERT(emPhyRailHandle != NULL);

   #if     PHY_DUAL
    PHY_ATTEST(RAIL_ConfigMultiTimer(true));
   #endif//PHY_DUAL
    PHY_ATTEST_RAIL(RAIL_SetPtiProtocol(emPhyRailHandle, RAIL_PTI_PROTOCOL_ZIGBEE));
    // If this event mask is ever changed, the changer must confirm that these
    // remain mutually exclusive with the events handled in
    // changeDynamicEvents.
    updateEvents(RAIL_EVENTS_ALL,
                 0
                 | RAIL_EVENT_CAL_NEEDED
                 | RAIL_EVENT_RX_PACKET_RECEIVED
                 | RAIL_EVENT_RX_ACK_TIMEOUT
                 | RAIL_EVENT_IEEE802154_DATA_REQUEST_COMMAND
                 | RAIL_EVENT_TXACK_PACKET_SENT
                 | RAIL_EVENT_TXACK_UNDERFLOW
                 | RAIL_EVENT_TXACK_ABORTED
                 | RAIL_EVENT_TXACK_BLOCKED
                 | RAIL_EVENT_TX_PACKET_SENT
                 | RAIL_EVENT_TX_UNDERFLOW
                 | RAIL_EVENT_TX_ABORTED
                 | RAIL_EVENT_TX_BLOCKED
                 | RAIL_EVENT_TX_CHANNEL_BUSY
                 | RAIL_EVENT_CONFIG_UNSCHEDULED
                 | RAIL_EVENT_SCHEDULER_STATUS
                 );
    PHY_ATTEST_RAIL(RAIL_ConfigCal(emPhyRailHandle, RAIL_CAL_ALL));
    PHY_ATTEST_RAIL(RAIL_ConfigRxOptions(emPhyRailHandle,
                                         RAIL_RX_OPTIONS_ALL,
                                         0
                                         | RAIL_RX_OPTION_STORE_CRC
                                         | RAIL_RX_OPTION_TRACK_ABORTED_FRAMES
                                         ));
    // Register the RSSI -> LQI conversion callback to fix the LQI values
    RAIL_ConvertLqi(emPhyRailHandle, emRadioConvertRssiToLqi);
    radioInited = true; // Do this here so below emRadio calls can call RAIL

    tokTypeMfgSynthFreqOffset synthFreqOffset;
    halCommonGetMfgToken(&synthFreqOffset, TOKEN_MFG_SYNTH_FREQ_OFFSET);
    if (BIT(8) & synthFreqOffset) {
      // The lsb of the high byte is in the default state.
      // Ignore the low byte.
      emSynthFreqOffset = 0;
    } else {
      // The lsb of the high byte has been cleared.
      // Interpret the low byte as the synth frequency offset.
      emSynthFreqOffset = (int8_t)LOW_BYTE(synthFreqOffset);
    }

    emRadioEnablePta(sl_rail_util_coex_is_enabled()); // Restores settings destroyed by reinit
    emApiSetTxPowerMode(emGetTxPowerMode());
    sli_zigbee_init_eui64();
    PHY_ATTEST_SUCCESS(sli_802154phy_set_phy_radio_channel(sli_802154phy_get_phy_radio_channel()));
    // sli_802154phy_set_phy_radio_channel() also explcitly inits:
    //   emRadioEnableAddressMatching(emRadioAddressMatchingEnabled());
    //   sli_802154mac_radio_enable_auto_ack(sli_802154phy_radio_auto_ack_enabled());
    //   sli_802154phy_set_coordinator(emPhyDeviceIsCoordinator());
    //   sli_802154phy_set_eui64();
    //   sli_802154phy_radio_set_node_id(sli_802154mac_radio_get_node_id());
    //   sli_802154phy_radio_set_pan_id(sli_802154phy_radio_get_pan_id());
    //   emRadioEnableReceiveCrc(emRadioReceiveCrcEnabled());
    //   emRadioSetEdCcaThreshold(emRadioGetEdCcaThreshold());
    // So we don't need to do these again here.

    emRadioEnableSfdRecvNotification(emRadioSfdRecvNotificationEnabled());
    sli_802154phy_set_phy_radio_powerFl(emRadioPowerFl);
    emRadioEnablePacketTrace(sli_802154phy_radio_packet_trace_enabled());
  }

  emRadioSetPowerStatus(initialRadioPowerMode);
}

void halInternalStartSymbolTimer(void)
{
  if (!radioInited) {
    // Only need to init enough to use PROTIMER but not the rest of the RADIO,
    // but the RF PHY makes that tricky, so just init normally with radio off.
    sli_802154phy_radio_init(SL_ZIGBEE_RADIO_POWER_MODE_OFF);
  }
}

//------------------------------------------------------------------------------
// Energy Detection APIs
//----------------------

int8_t sli_802154phy_radio_energy_detection(void)
{
  // This API is expected to return the current RSSI rolling average over
  // the last 8 symbols.  That's what we think RAIL_GetRssi() returns
  // on efr32 per the radio configuration's AGC config.
  int16_t rssiQdBm = RAIL_GetRssi(emPhyRailHandle, true); // quad dBm (dBm*4, i.e. 2 fraction bits)
  if (rssiQdBm == RAIL_RSSI_INVALID) {
    return SL_ZIGBEE_PHY_INVALID_RSSI;
  }
  // Toss the fractional portion (truncate toward next-smaller signed integer)
  int8_t energyLevel = (int8_t)(rssiQdBm >> 2);
  return energyLevel + RSSI_DBM_OFFSET_RAIL;
}

//------------------------------------------------------------------------------
// Address and Filtering APIs
//---------------------------

void emRadioEnableAddressMatching(uint8_t enable)
{
  if (radioInited) {
    PHY_ATTEST_RAIL(RAIL_IEEE802154_SetPromiscuousMode(emPhyRailHandle,
                                                       !enable));
  } else {
    // Defer to sli_802154phy_radio_init() time
  }
  addressMatchingEnabled = !!enable;
}

uint8_t emRadioAddressMatchingEnabled(void)
{
  return addressMatchingEnabled;
}

void sli_802154mac_radio_enable_auto_ack(bool enable)
{
  DECLARE_INTERRUPT_STATE;
  DISABLE_INTERRUPTS(); // Protect against emRadioHoldOffIsr() while changing Radio config
  autoAckEnabled = enable;
 #if     RHO_USE_BLOCKTX
  bool updateAutoAck = true;
 #else//!RHO_USE_BLOCKTX
  // Only allow the shadow state to be updated when Radio HoldOff is active
  // but not the real state of the radio
  bool updateAutoAck = !emRadioHoldOffIsActive();
 #endif//RHO_USE_BLOCKTX
  if (radioInited && updateAutoAck) {
    RAIL_PauseRxAutoAck(emPhyRailHandle, !enable);
  }
  RESTORE_INTERRUPTS();
}

bool sli_802154phy_radio_auto_ack_enabled(void)
{
  return autoAckEnabled;
}

void sli_802154phy_set_eui64(void)
{
  // eui64 is stored as a little-endian 8-byte array,
  // the same byte order as over the air transmission,
  // which matches the order sli_zigbee_stack_get_eui64() returns.
  uint8_t *eui = (uint8_t*) sli_zigbee_stack_get_eui64();
  if (radioInited) {
    PHY_ATTEST_RAIL(RAIL_IEEE802154_SetLongAddress(emPhyRailHandle,
                                                   eui,
                                                   IEEE802154_ADDR_INDEX));
  } else {
    // Defer to sli_802154phy_radio_init() time
  }
}

void sli_802154phy_set_coordinator(bool coordinator)
{
  deviceIsCoordinator = coordinator;
  if (radioInited) {
    PHY_ATTEST_RAIL(RAIL_IEEE802154_SetPanCoordinator(emPhyRailHandle,
                                                      coordinator));
  } else {
    // Defer to sli_802154phy_radio_init() time
  }
}

bool emPhyDeviceIsCoordinator(void)
{
  return deviceIsCoordinator;
}

void sli_802154phy_radio_set_node_id(sl_802154_short_addr_t nodeId)
{
  emLocalRadioNodeId = nodeId;
  if (radioInited) {
    PHY_ATTEST_RAIL(RAIL_IEEE802154_SetShortAddress(emPhyRailHandle,
                                                    (uint16_t) nodeId,
                                                    IEEE802154_ADDR_INDEX));
  } else {
    // Defer to sli_802154phy_radio_init() time
  }
}

sl_802154_short_addr_t sli_802154mac_radio_get_node_id(void)
{
  return emLocalRadioNodeId;
}

sl_802154_pan_id_t sli_802154phy_radio_get_pan_id(void)
{
  return emLocalRadioPanId;
}

void sli_802154phy_radio_set_pan_id(sl_802154_pan_id_t panId)
{
  emLocalRadioPanId = panId;
  if (radioInited) {
    PHY_ATTEST_RAIL(RAIL_IEEE802154_SetPanId(emPhyRailHandle,
                                             (uint16_t) panId,
                                             IEEE802154_ADDR_INDEX));
  } else {
    // Defer to sli_802154phy_radio_init() time
  }
}

void emRadioEnableReceiveCrc(bool enable)
{
  if (radioCheckCrc != enable) {
    radioCheckCrc = enable;
    // Change is applied at efr32WakeUp() time, force that if already awake
    RadioPowerMode origPowerMode = radioCurrentPowerStatus;
    if (origPowerMode != SL_ZIGBEE_RADIO_POWER_MODE_OFF) {
      sli_802154phy_radio_sleep();
      efr32WakeUp(origPowerMode);
    }
  }
}

bool emRadioReceiveCrcEnabled(void)
{
  return radioCheckCrc;
}

//------------------------------------------------------------------------------
// Diagnostic Transmit APIs
//-------------------------

void sli_802154phy_start_transmit_stream(void)
{
  //@TODO: RAIL_LIB-560 workaround: Idle radio before starting tone/stream
  RAIL_Idle(emPhyRailHandle, RAIL_IDLE_ABORT, true);
  PHY_ATTEST_RAIL(RAIL_StartTxStream(emPhyRailHandle,
                                     emRadioChannel,
                                     RAIL_STREAM_PN9_STREAM));
}

void sli_802154phy_stop_transmit_stream(void)
{
  PHY_ATTEST_RAIL(RAIL_StopTxStream(emPhyRailHandle));
  //@TODO: RAIL_LIB-560 workaround: Restore radio state
  restartListening();
}

void sli_802154phy_start_transmit_tone(void)
{
  //@TODO: RAIL_LIB-560 workaround: Idle radio before starting tone/stream
  RAIL_Idle(emPhyRailHandle, RAIL_IDLE_ABORT, true);
  PHY_ATTEST_RAIL(RAIL_StartTxStream(emPhyRailHandle,
                                     emRadioChannel,
                                     RAIL_STREAM_CARRIER_WAVE));
}

void sli_802154phy_stop_transmit_tone(void)
{
  sli_802154phy_stop_transmit_stream();
}

//------------------------------------------------------------------------------
// Miscellaneous APIs
//-------------------

// Assess Clear Channel
uint8_t emRadioGetCca(void)
{
  // If we have a transmit in progress, consider channel busy
  if ((txState > PHY_TX_STATE_OFF)              // PHY_TX_STATE_TRIG_NOACK or
      && (txState < PHY_TX_STATE_TRIG_ACKING)) {   // PHY_TX_STATE_TRIG_W_ACK
    return CCA_BUSY;
  }
  // If radio isn't on, force it on for the duration of the CCA check
  // N.B. This could take a lot longer than the 8 symbols to make a CCA
  //      assessment, so callers should be aware of this limitation.
  RadioPowerMode origPowerStatus = radioCurrentPowerStatus;
  if ((origPowerStatus != SL_ZIGBEE_RADIO_POWER_MODE_ED_ON)
      && (origPowerStatus != SL_ZIGBEE_RADIO_POWER_MODE_RX_ON)) {
    efr32WakeUp(SL_ZIGBEE_RADIO_POWER_MODE_ED_ON);
    // Wait for radio to get to RXSEARCH state for RSSI readings to be useful
    //@TODO Might be able to remove delay depending on how RAIL_LIB-831 is fixed
    halCommonDelayMicroseconds(RAIL_WARM_TIME_US);
  }

  // This API does an 8-symbol CCA check irrespective of
  // phy2SubBandParams->lbtCcaSym, to support software LBT
  // implementations that need to continuously monitor CCA
  // during the quite lengthy lbtCcaSym period.
  //@TODO: Need RAIL_LIB-831 fixed before trying to use RAIL_StartAverageRssi()
  halCommonDelayMicroseconds(SL_ZIGBEE_PHY_SYMBOLS_TO_US(8));
  int16_t rssiQdBm = RAIL_GetRssi(emPhyRailHandle, true);
  // quad dBm (dBm*4, i.e. 2 fraction bits)
  uint8_t cca = ((rssiQdBm == RAIL_RSSI_INVALID)
                 || (rssiQdBm > (emRadioGetEdCcaThreshold() * 4)))
                ? CCA_BUSY
                : CCA_CLEAR;

  // Restore original radio power state
  if (origPowerStatus == SL_ZIGBEE_RADIO_POWER_MODE_OFF) {
    sli_802154phy_radio_sleep();
  } else {
    // If was in TX_ON mode, stay in ED_ON mode for further LBT checks;
    // otherwise stay in original ED_ON or RX_ON mode.
  }
  return cca;
}

void emRadioEnableOverflowNotification(bool enable)
{
  emRadioOverflowCallbackEnabled = enable;
}

bool emRadioOverflowNotificationEnabled(void)
{
  return emRadioOverflowCallbackEnabled;
}

void emRadioEnableSfdSentNotification(bool enable)
{
  emRadioSfdSentCallbackEnabled = enable;
}

bool emRadioSfdSentNotificationEnabled(void)
{
  return emRadioSfdSentCallbackEnabled;
}

void emRadioEnablePacketTrace(bool enable)
{
  RAIL_EnablePti(emPhyRailHandle, enable);
  ptiEnabled = enable;
}

bool sli_802154phy_radio_packet_trace_enabled(void)
{
  return ptiEnabled;
}

void emRadioSetEdCcaThreshold(int8_t thresholddBm)
{
  if (thresholddBm == ED_CCA_THRESHOLD_UNINIT) {
    // The first time emRadioSetEdCcaThreshold() is called we use the
    // CCA_THRESHOLD mfg token, if present, else we use the default.
    // Subsequent calls to emRadioSetEdCcaThreshold will ignore the
    // token/default value and use whatever value the user passes in.
    tokTypeMfgCcaThreshold ccaThresholdToken = 0xFFFF; // Assume unset
    // Default is to use per-sub-band edThresholddBm which
    // is emplaced during switchBand(), so just ensure
    // edCcaThreshold get set to _SUBBAND sentinal.
    thresholddBm = ED_CCA_THRESHOLD_SUBBAND;
    halCommonGetMfgToken(&ccaThresholdToken, TOKEN_MFG_CCA_THRESHOLD);
    // The format of this token is now (per RAIL_LIB-1373):
    // uint8_t ccaThresholdSubNeg     : 7, // bits <15:9> unsigned but negated
    // bool    ccaThreshold2p4Invalid : 1, // bit  <8> 1=invalid 0=valid
    // int8_t  ccaThreshold2p4        : 8, // bits <7:0> signed 2's complement
    // where:
    // ccaThresholdSubNeg is 7-bit unsigned value where values 0 and 127
    //   both mean "use default" (which is what allows us to jimmy SubGHz
    //   CCA threshold oonfiguration into this pre-existing token), and
    //   values 1..126 are negated to map to -1..-126 dBm, respectively.
    // ccaThreshold2p4Invalid indicates whether ccaThreshold2p4 is valid;
    // ccaThreshold2p4 is the actual signed 2's complement CCA threshold;
    // This PHY can support both SubGHz and 2.4GHz thresholds:
   #if     (MAC_HAS_CHANNEL_PAGES && IEEE802154_GB868_SUPPORTED)
    if (emRadioChannelPageInUse != 0) {
      uint8_t ccaThresholdSubNeg = (ccaThresholdToken >> 9);
      if ((ccaThresholdSubNeg > 0) && (ccaThresholdSubNeg < 127)) {
        thresholddBm = -(int8_t)ccaThresholdSubNeg;
      }
    } else
   #endif//(MAC_HAS_CHANNEL_PAGES && IEEE802154_GB868_SUPPORTED)
    if ((BIT(8) & ccaThresholdToken) == 0) {
      // The lsb of the high byte has been cleared.
      // Interpret the low byte as the CCA threshold.
      thresholddBm = (int8_t)LOW_BYTE(ccaThresholdToken);
    }
  }
  {
    DECLARE_INTERRUPT_STATE;
    DISABLE_INTERRUPTS(); // Protect against emRadioHoldOffIsr() while changing radio state
    // Only allow the shadow state to be updated when Radio HoldOff is active
    // but not the real state of the radio
    if (!emRadioHoldOffIsActive()) {
      int8_t thresholdToApply = thresholddBm;
      if ((thresholdToApply == ED_CCA_THRESHOLD_SUBBAND)
          && (phy2SubBandConfig != NULL)) {
        thresholdToApply = phy2SubBandConfig->edThresholddBm;
      }
      if (thresholdToApply < ED_CCA_THRESHOLD_SUBBAND) {
        thresholdToApply -= RSSI_DBM_OFFSET_RAIL;
        setCurCcaThreshold(thresholdToApply);
      }
    }
    edCcaThreshold = thresholddBm;
    RESTORE_INTERRUPTS();
  }
}

int8_t emRadioGetEdCcaThreshold(void)
{
  if (edCcaThreshold == ED_CCA_THRESHOLD_SUBBAND) {
    // Return the current sub-band's threshold
    return ((phy2SubBandConfig == NULL)
            ? ED_CCA_THRESHOLD_UNINIT
            : phy2SubBandConfig->edThresholddBm);
  }
  return edCcaThreshold;
}

// ################################################
// #### EFR32 #####################################
// ################################################

// ## Transmit Events ##

/***************************************************************************//**
 * @brief
 *   MACSEQ_TOUT0MatchLBTEvent() is used by the PHY to signal than the LBT
 *   timeout counter has reached 0.
 *
 * @details
 *
 ******************************************************************************/
static void MACSEQ_TOUT0MatchLBTEvent(void)
{
  //assert PTA_REQ prior to CCA check
  (void) halPtaStackEvent(SL_RAIL_UTIL_IEEE802154_STACK_EVENT_TX_CCA_SOON, 0U);
}

/***************************************************************************//**
 * @brief
 *   MACSEQ_LBTRetryEvent() is used by the PHY to signal that an LBT sequence
 *   is being retried.
 *
 * @details
 *  LBT sucess is not captured as TX DONE is used instead.
 ******************************************************************************/
static void MACSEQ_LBTRetryEvent(void)
{
  // Deassert PTA_REQ when CCA check failed
  // but only if radio wasn't left on or already is turned on for next CCA
  (void) halPtaStackEvent(SL_RAIL_UTIL_IEEE802154_STACK_EVENT_TX_CCA_BUSY,
                          (uint32_t)RAIL_IsNextCcaNow(emPhyRailHandle));
}

/***************************************************************************//**
 * @brief
 *   MACSEQ_LBTFailureEvent() is used by the PHY to signal that an LBT sequence
 *   has failed.
 ******************************************************************************/
static void MACSEQ_LBTFailureEvent(void)
{
  bool pktRequestedAck = (txState == PHY_TX_STATE_TRIG_W_ACK);
  txState = PHY_TX_STATE_OFF;
  emPhyDcSetExtendedStatus(EM_PHY_DC_EXT_STATUS_LBT_TRIES_EXCEEDED);
  lastTxStartedTimeMs = -1; // Deem Tx completed
  (void) halPtaStackEvent(SL_RAIL_UTIL_IEEE802154_STACK_EVENT_TX_BLOCKED,
                          (uint32_t) pktRequestedAck);
  if (sli_legacy_mfglib_mode) {
    RAIL_YieldRadio(emPhyRailHandle);
    sli_legacy_mfglib_transmit_complete = true;
  } else {
    // If we're doing coex change this to SL_STATUS_TRANSMIT_BLOCKED to be more
    // clear to the upper layer
    sl_status_t status = emRadioHoldOffIsActive() ? SL_STATUS_TRANSMIT_BLOCKED
                         : SL_STATUS_CCA_FAILURE;
    sli_802154phy_radio_transmit_complete_callback(status, 0, 0);
  }
}

static void RAILCb_RxAckTimeout(RAIL_Handle_t railHandle)
{
  UNUSED_VAR(railHandle);
  // we are waiting for an ack, but the timer expired before we received it
  if (txState == PHY_TX_STATE_TRIG_ACKING) {
    txState = PHY_TX_STATE_OFF;
    lastTxStartedTimeMs = -1; // Deem Tx completed
    // SL_RAIL_UTIL_IEEE802154_STACK_EVENT_TX_ACK_TIMEDOUT signalled by callback
    sli_802154phy_radio_transmit_complete_callback(SL_STATUS_MAC_NO_ACK_RECEIVED,
                                                   lastTxSfdTime, 0);
  }
}

/***************************************************************************//**
 * @brief
 *   MACSEQ_TransmitDoneEvent() is used by the PHY to signal that a transmission
 *   is complete.
 *
 * @details
 *
 ******************************************************************************/
static void MACSEQ_TransmitDoneEvent(void)
{
  lastTxFinishedTimeMs = halCommonGetInt32uMillisecondTick();
  if (radioTransmitConfig.checkCca) {
    lastCsmaTxFinishedTimeMs = lastTxFinishedTimeMs;
  }

  switch (txState) {
    case PHY_TX_STATE_TRIG_NOACK:
      txState = PHY_TX_STATE_OFF;
      lastTxStartedTimeMs = -1; // Deem Tx completed
      (void) halPtaStackEvent(SL_RAIL_UTIL_IEEE802154_STACK_EVENT_TX_ENDED, 0U);
      if (sli_legacy_mfglib_mode) {
        RAIL_YieldRadio(emPhyRailHandle);
        sli_legacy_mfglib_transmit_complete = true;
      } else {
        sli_802154phy_radio_transmit_complete_callback(SL_STATUS_OK, lastTxSfdTime, 0);
      }
      break;
    case PHY_TX_STATE_TRIG_W_ACK:
      // If ACK was requested, just wait for the RX or the ACK Timeout.
      // RAIL should have started ACK timeout timer for us.
      txState = PHY_TX_STATE_TRIG_ACKING;
      (void) halPtaStackEvent(SL_RAIL_UTIL_IEEE802154_STACK_EVENT_TX_ACK_WAITING, 0U);
      break;
    case PHY_TX_STATE_OFF:
    default:
      PHY_ASSERT(false);
      break;
  }
}

/***************************************************************************//**
 * @brief
 *   MACSEQ_AckDoneEvent() is used by the PHY to signal that an ACK transmission
 *   is complete.
 *
 * @details
 *
 ******************************************************************************/
static void MACSEQ_AckDoneEvent(sl_rail_util_ieee802154_stack_event_t ptaStackEvent)
{
  lastTxFinishedTimeMs = halCommonGetInt32uMillisecondTick();
  (void) halPtaStackEvent(ptaStackEvent, (uint32_t)emRadioIsReceivingFrame());
  // ACKs don't update lastCsmaTxFinishedTimeMs
  if (ptaStackEvent != SL_RAIL_UTIL_IEEE802154_STACK_EVENT_RX_ACK_BLOCKED) { // ACK made it on-air
    // Account for this ACK in DC plugin
    emPhyDcAckSent();
  } else {
    // If it's not too late, inform upper layer it should *not* send
    // indirect data to a poller that might have waited for it if an
    // ACK with FP actually made it on the air.
    framePendingSetInOutgoingAck = false;
  }
  sli_802154phy_radio_tx_ack_isr_callback(); // Notify MAC layer we sent an ACK
}

// This callback fires if transmission was successful.
// Notice that we configure RAIL so that the radio goes back to RX after any
// TX operation.
static void RAILCb_TxPacketSent(RAIL_Handle_t railHandle)
{
  UNUSED_VAR(railHandle);
  if ((txState > PHY_TX_STATE_OFF)          // PHY_TX_STATE_TRIG_NOACK or
      && (txState < PHY_TX_STATE_TRIG_ACKING)) { // PHY_TX_STATE_TRIG_W_ACK
    //@TODO: Use RAIL_GetTxTimeSyncWordEnd if really care about timestamp
    RAIL_GetTxPacketDetailsAlt(emPhyRailHandle, false, &lastTxSfdTime);
    MACSEQ_TransmitDoneEvent();
  }
}

/***************************************************************************//**
 * @brief
 *   MACSEQ_TransmitErrorEvent() is used by the PHY to signal that an error
 *   occured during normal (not ACK) transmit.
 *
 * @details
 *
 ******************************************************************************/
static void MACSEQ_TransmitErrorEvent(sl_status_t status)
{
  lastTxStartedTimeMs = -1; // Deem Tx completed
  // Since some portion of packet may have gone out, update lastTxFinishedTimeMs
  lastTxFinishedTimeMs = halCommonGetInt32uMillisecondTick();
  // Only a non-ACK would want to change txState.
  // Since we're called in some multiprotocol scheduler situations that
  // don't check txState, do that here.
  if (txState <= PHY_TX_STATE_OFF) { // Hypothetical error - ignore it
    // From a PTA standpoint, this might be a RAIL_EVENT_SCHEDULER_STATUS
    // event with status RAIL_SCHEDULER_STATUS_EVENT_INTERRUPTED -- that
    // only happens after RAIL_EVENT_CONFIG_UNSCHEDULED has been signaled,
    // for which we've already signaled SL_RAIL_UTIL_IEEE802154_STACK_EVENT_RX_IDLED, so there's
    // nothing further we need to do here for PTA.
    return;
  }
  // Only update lastCsmaTxFinishedTimeMs if this was a CSMA transmit failure
  if (radioTransmitConfig.checkCca) {
    lastCsmaTxFinishedTimeMs = lastTxFinishedTimeMs;
  }
  bool pktRequestedAck = (txState == PHY_TX_STATE_TRIG_W_ACK);
  txState = PHY_TX_STATE_OFF;
  (void) halPtaStackEvent((((status == SL_STATUS_CCA_FAILURE)
                            || (status == SL_STATUS_TRANSMIT_SCHEDULER_FAIL))
                           ? SL_RAIL_UTIL_IEEE802154_STACK_EVENT_TX_BLOCKED
                           : SL_RAIL_UTIL_IEEE802154_STACK_EVENT_TX_ABORTED),
                          (uint32_t) pktRequestedAck);
  if (sli_legacy_mfglib_mode) {
    RAIL_YieldRadio(emPhyRailHandle);
    sli_legacy_mfglib_transmit_complete = true;
  } else {
    sli_802154phy_radio_transmit_complete_callback(status, 0, 0);
  }
}

static void schedulerEventHandler(RAIL_Handle_t railHandle)
{
  switch (RAIL_GetSchedulerStatus(railHandle)) {
    case RAIL_SCHEDULER_STATUS_SCHEDULE_FAIL:
      // Our transmit couldn't occur because the radio isn't available.
      MACSEQ_TransmitErrorEvent(SL_STATUS_TRANSMIT_SCHEDULER_FAIL);
      break;
    case RAIL_SCHEDULER_STATUS_EVENT_INTERRUPTED:
      // Our transmit got pre-empted by a higher-priority protocol.
      // Treat this as a TX_INCOMPLETE assuming some portion of it
      // might have gotten on-air.
      MACSEQ_TransmitErrorEvent(SL_STATUS_TRANSMIT_INCOMPLETE);
      break;
    case RAIL_SCHEDULER_STATUS_NO_ERROR:
      break;
    case RAIL_SCHEDULER_STATUS_UNSUPPORTED:
    case RAIL_SCHEDULER_STATUS_CCA_CSMA_TX_FAIL:
    case RAIL_SCHEDULER_STATUS_CCA_LBT_TX_FAIL:
    case RAIL_SCHEDULER_STATUS_SINGLE_TX_FAIL:
    case RAIL_SCHEDULER_STATUS_SCHEDULED_TX_FAIL:
    case RAIL_SCHEDULER_STATUS_SCHEDULED_RX_FAIL:
    case RAIL_SCHEDULER_STATUS_INTERNAL_ERROR:
    default:
      // Unexpected statuses for the scheduler event in our protocol
      PHY_ASSERT(false);
      break;
  }
}

// ## Receive Events ##

static void MACSEQ_RxFrameDetected(uint8_t sfdId)
{
  UNUSED_VAR(sfdId);
  // We've just started to receive a frame -- it might or might not
  // be for us, but we must play it safe and issue a PTA REQUEST in
  // case it really is for us, because if we delay until we know it
  // is ours, timing is too tight to receive the GRANT to ACK it.
  (void) halPtaStackEvent(SL_RAIL_UTIL_IEEE802154_STACK_EVENT_RX_STARTED,
                          (uint32_t)emRadioIsReceivingFrame());
  if (sfdRecvNotificationEnabled) {
    emRadioSfdRecvIsrCallback();
  }
}

/***************************************************************************//**
 * @brief
 *   MACSEQ_FrameErrorEvent() is used by the PHY to signal that a frame error
 *   occured during reception.
 *
 * @details
 *
 ******************************************************************************/
static void MACSEQ_FrameErrorEvent(void)
{
  (void) halPtaStackEvent(SL_RAIL_UTIL_IEEE802154_STACK_EVENT_RX_CORRUPTED,
                          (uint32_t)emRadioIsReceivingFrame());
}

/***************************************************************************//**
 * @brief
 *   MACSEQ_RxAbortedEvent() is used by the PHY to signal that an Rx was
 *   aborted.
 *
 * @details
 *
 ******************************************************************************/
static void MACSEQ_RxAbortedEvent(void)
{
  (void) halPtaStackEvent(SL_RAIL_UTIL_IEEE802154_STACK_EVENT_RX_FILTERED,
                          (uint32_t)emRadioIsReceivingFrame());
}

/***************************************************************************//**
 * @brief
 *   MACSEQ_FilterPassedEvent() is used by the PHY to signal that the frame
 *   currently being received has passed address filtering.
 ******************************************************************************/
static void MACSEQ_FilterPassedEvent(void)
{
  (void) halPtaStackEvent(SL_RAIL_UTIL_IEEE802154_STACK_EVENT_RX_ACCEPTED,
                          (uint32_t)emRadioIsReceivingFrame());
}

static void RAILCb_IEEE802154_DataRequestCommand(RAIL_Handle_t railHandle)
{
  // This callback occurs after the address fields of an incoming
  // ACK-requesting CMD or DATA frame have been received and we
  // can do a frame pending check.  We must also figure out what
  // kind of ACK is being requesting -- Immediate or Enhanced
  // when Enh-ACKs are supported and enabled.

  framePendingSetInOutgoingAck = false; // Assume we're too late

  // It only makes sense to bother doing FP lookup or
  // generating an Enh-ACK if Auto-ACKing is fully active.
  if (!autoAckEnabled || RAIL_IsRxAutoAckPaused(railHandle)) {
    return;
  }

  bool setFramePending = false;

 #if     MAC_SUPPORT_ENHANCED_ACKS

  // This table is derived from 802.15.4-2015 Section 7.2.1.5 PAN ID
  // Compression field and Table 7-2 for both 2003/2006 and 2015
  // frame versions.  It is indexed by 6 bits of the MacFCF:
  //   SrcAdrMode FrameVer<msbit> DstAdrMode PanIdCompression
  // and each address' length is encoded in a nibble:
  //    15:12  11:8     7:4     3:0
  //   SrcAdr  SrcPan  DstAdr  DstPan
  // Illegal combinations are indicated by 0xFFFFU.
  #define ADDRSIZE_DST_PAN_SHIFT 0
  #define ADDRSIZE_DST_PAN_MASK  (0x0FU << ADDRSIZE_DST_PAN_SHIFT)
  #define ADDRSIZE_DST_ADR_SHIFT 4
  #define ADDRSIZE_DST_ADR_MASK  (0x0FU << ADDRSIZE_DST_ADR_SHIFT)
  #define ADDRSIZE_SRC_PAN_SHIFT 8
  #define ADDRSIZE_SRC_PAN_MASK  (0x0FU << ADDRSIZE_SRC_PAN_SHIFT)
  #define ADDRSIZE_SRC_ADR_SHIFT 12
  #define ADDRSIZE_SRC_ADR_MASK  (0x0FU << ADDRSIZE_SRC_ADR_SHIFT)

  static const uint16_t ieee802154Table7p2[64] = {
    0x0000U, 0x0000U, 0xFFFFU, 0xFFFFU, 0x0022U, 0x0022U, 0x0082U, 0x0082U,
    0x0000U, 0x0002U, 0xFFFFU, 0xFFFFU, 0x0022U, 0x0020U, 0x0082U, 0x0080U,
    0xFFFFU, 0xFFFFU, 0xFFFFU, 0xFFFFU, 0xFFFFU, 0xFFFFU, 0xFFFFU, 0xFFFFU,
    0xFFFFU, 0xFFFFU, 0xFFFFU, 0xFFFFU, 0xFFFFU, 0xFFFFU, 0xFFFFU, 0xFFFFU,
    0x2200U, 0x2200U, 0xFFFFU, 0xFFFFU, 0x2222U, 0x2022U, 0x2282U, 0x2082U,
    0x2200U, 0x2000U, 0xFFFFU, 0xFFFFU, 0x2222U, 0x2022U, 0x2282U, 0x2082U,
    0x8200U, 0x8200U, 0xFFFFU, 0xFFFFU, 0x8222U, 0x8022U, 0x8282U, 0x8082U,
    0x8200U, 0x8000U, 0xFFFFU, 0xFFFFU, 0x8222U, 0x8022U, 0x8082U, 0x8080U,
  };

  // For an Enhanced ACK, we need to generate that ourselves;
  // RAIL will generate an Immediate ACK for us, though we can
  // tell it to go out with its FramePending bit set.
  // An 802.15.4 packet from RAIL should look like:
  // 1/2 |   1/2  | 0/1  |  0/2   | 0/2/8  |  0/2   | 0/2/8  | ...
  // PHR | MacFCF | Seq# | DstPan | DstAdr | SrcPan | SrcAdr | ...
  #define MaxExpectedBytes (2U + 2U + 1U + 2U + 8U + 2U + 8U)
  RAIL_RxPacketInfo_t packetInfo;
  uint8_t pkt[MaxExpectedBytes];
  uint8_t pktOffset = PHRLen; // No need to parse the PHR byte(s)
  RAIL_GetRxIncomingPacketInfo(emPhyRailHandle, &packetInfo);
  if (packetInfo.packetBytes < (pktOffset + 2U)) {
    return;
  }
  // Only extract what we care about
  if (packetInfo.packetBytes > MaxExpectedBytes) {
    packetInfo.packetBytes = MaxExpectedBytes;
    if (packetInfo.firstPortionBytes >= MaxExpectedBytes) {
      packetInfo.firstPortionBytes = MaxExpectedBytes;
      packetInfo.lastPortionData = NULL;
    }
  }
  RAIL_CopyRxPacket(pkt, &packetInfo);
  uint16_t macFcf = pkt[pktOffset++];
 #if     MAC_SUPPORT_MULTIPURPOSE_FRAMES
  if ((macFcf & MAC_FRAME_TYPE_MASK) == MAC_FRAME_TYPE_MULTIPURPOSE) {
    // Multipurpose frames have an arcane FCF structure
    if ((macFcf & MAC_MP_FRAME_FLAG_LONG_FCF) != 0U) {
      macFcf |= (pkt[pktOffset++] << 8);
    }
    // Map Multipurpose FCF to a 'normal' Version 2012 FCF
    // as best we can.
    macFcf = (MAC_FRAME_TYPE_MULTIPURPOSE
              | ((macFcf & (MAC_MP_FRAME_FLAG_SECURITY_ENABLED
                            | MAC_MP_FRAME_FLAG_IE_LIST_PRESENT)) >> 6)
              | ((macFcf & MAC_MP_FRAME_FLAG_FRAME_PENDING) >> 7)
              | ((macFcf & MAC_MP_FRAME_FLAG_ACK_REQUIRED) >> 9)
              | ((macFcf & (MAC_MP_FRAME_FLAG_PANID_PRESENT
                            | MAC_MP_FRAME_FLAG_SEQ_SUPPRESSION)) >> 2)
              | ((macFcf & MAC_MP_FRAME_DESTINATION_MODE_MASK) << 6)
              | MAC_FRAME_VERSION_2012
              | ((macFcf & MAC_MP_FRAME_SOURCE_MODE_MASK) << 8)
              );
    // MultiPurpose's PANID_PRESENT is not equivalent to 2012/5's
    // PANID_COMPRESSION so we map it best we can by flipping it
    // in the following address-combination situations:
    uint16_t addrCombo = (macFcf & (MAC_FRAME_SOURCE_MODE_MASK | MAC_FRAME_DESTINATION_MODE_MASK));
    if ((addrCombo == (MAC_FRAME_SOURCE_MODE_NONE | MAC_FRAME_DESTINATION_MODE_NONE))
        || (addrCombo == (MAC_FRAME_SOURCE_MODE_SHORT | MAC_FRAME_DESTINATION_MODE_SHORT))
        || (addrCombo == (MAC_FRAME_SOURCE_MODE_SHORT | MAC_FRAME_DESTINATION_MODE_LONG))
        || (addrCombo == (MAC_FRAME_SOURCE_MODE_LONG | MAC_FRAME_DESTINATION_MODE_SHORT))
        ) {
      // 802.15.4-2015 PANID_COMPRESSION = MP PANID_PRESENT
    } else {
      // 802.15.4-2015 PANID_COMPRESSION = !MP PANID_PRESENT
      macFcf ^= MAC_FRAME_FLAG_PANID_COMPRESSION; // Flip it
    }
  } else
 #endif//MAC_SUPPORT_MULTIPURPOSE_FRAMES
  {
    macFcf |= (pkt[pktOffset++] << 8);
  }

 #if     USE_ENHANCED_FRAME_PENDING
  bool checkFramePending = true;
 #else//!USE_ENHANCED_FRAME_PENDING
  bool checkFramePending = ((macFcf & MAC_FRAME_TYPE_MASK) == MAC_FRAME_TYPE_CONTROL);
 #endif//USE_ENHANCED_FRAME_PENDING
  bool enhAck = ((macFcf & MAC_FRAME_VERSION_MASK) == MAC_FRAME_VERSION_2012);
  // Compress MAC FCF to index into 64-entry address-length table:
  // SrcAdrMode FrameVer<msbit> DstAdrMode PanIdCompression
  uint16_t index = (((macFcf & (MAC_FRAME_SOURCE_MODE_MASK | MAC_FRAME_VERSION_2012)) >> 10)
                    | ((macFcf & MAC_FRAME_DESTINATION_MODE_MASK) >> 9)
                    | ((macFcf & MAC_FRAME_FLAG_PANID_COMPRESSION) >> 6));
  uint16_t addrSizes = ieee802154Table7p2[index];
  // Illegal combinations mean illegal packets which we ignore
  if (addrSizes == 0xFFFFU) {
    return;
  }
  uint8_t seqNo = ((enhAck && ((macFcf & MAC_FRAME_FLAG_SEQ_SUPPRESSION) != 0U))
                   ? 0U : pkt[pktOffset++]); // Seq#
  if (enhAck) {
    // Enhanced ACK -- need to construct it since RAIL cannot.
    if (!enhAckSupported) {
      // RAIL passed up a packet needing Enh-ACK but we don't support
      // generating an Enh-ACK.  Return to not ACK it and save code space.
      return;
    }
    // First extract addresses from incoming packet since we may
    // need to reflect them in a different order in the outgoing ACK.
    // Use byte[0] to hold each one's length.
    uint8_t dstPan[3] = { 0, }; // Initialized only to eliminate false gcc warning
    dstPan[0] = ((addrSizes & ADDRSIZE_DST_PAN_MASK) >> ADDRSIZE_DST_PAN_SHIFT);
    if ((dstPan[0] + pktOffset) > packetInfo.packetBytes) {
      return;
    }
    if (dstPan[0] > 0U) {
      dstPan[1] = pkt[pktOffset++];
      dstPan[2] = pkt[pktOffset++];
    }
    uint8_t dstAdr[9];
    dstAdr[0] = ((addrSizes & ADDRSIZE_DST_ADR_MASK) >> ADDRSIZE_DST_ADR_SHIFT);
    if ((dstAdr[0] + pktOffset) > packetInfo.packetBytes) {
      return;
    }
    for (uint8_t i = 1U; i <= dstAdr[0]; i++) {
      dstAdr[i] = pkt[pktOffset++];
    }
    uint8_t srcPan[3];
    srcPan[0] = ((addrSizes & ADDRSIZE_SRC_PAN_MASK) >> ADDRSIZE_SRC_PAN_SHIFT);
    if ((srcPan[0] + pktOffset) > packetInfo.packetBytes) {
      return;
    }
    if (srcPan[0] > 0U) {
      srcPan[1] = pkt[pktOffset++];
      srcPan[2] = pkt[pktOffset++];
    }
    uint8_t srcAdr[9];
    srcAdr[0] = ((addrSizes & ADDRSIZE_SRC_ADR_MASK) >> ADDRSIZE_SRC_ADR_SHIFT);
    if ((srcAdr[0] + pktOffset) > packetInfo.packetBytes) {
      return;
    }
    for (uint8_t i = 1U; i <= srcAdr[0]; i++) {
      srcAdr[i] = pkt[pktOffset++];
    }

    // Reuse packet[] buffer for outgoing Enhanced ACK.
    // Phr1 Phr2 FcfL FcfH [Seq#] [DstPan] [DstAdr] [SrcPan] [SrcAdr]
    // Will fill in PHR later.
    // MAC Fcf:
    // - Frame Type = ACK
    // - Security Enabled = 0 in this implementation
    // - Frame Pending = 0 or as appropriate
    // - ACK Request = 0
    // - PanId compression = incoming packet's
    // - Seq# suppression = incoming packet's
    // - IE Present = 0 in this implementation
    // - DstAdrMode = SrcAdrMode of incoming packet's
    // - Frame Version = 2 (154E)
    // - SrcAdrMode = DstAdrMode of incoming packet's (for convenience)
    uint16_t ackFcf = (MAC_FRAME_TYPE_ACK
                       | (macFcf & MAC_FRAME_FLAG_PANID_COMPRESSION)
                       | (macFcf & MAC_FRAME_FLAG_SEQ_SUPPRESSION)
                       | MAC_FRAME_VERSION_2012
                       | ((macFcf & MAC_FRAME_SOURCE_MODE_MASK) >> 4)
                       | ((macFcf & MAC_FRAME_DESTINATION_MODE_MASK) << 4)
                       );
    // Do frame-pending check now
    if (checkFramePending && (srcAdr[0] > 0U)) {
      if (srcAdr[0] == 8U) {
        setFramePending = emberRadioDataPendingLongIdIsrCallback(
          &srcAdr[1], (macFcf & MAC_FRAME_TYPE_MASK));
      } else {
        uint16_t srcAdrShort = srcAdr[1] | (srcAdr[2] << 8);
        setFramePending = emberRadioDataPendingShortIdIsrCallback(
          srcAdrShort, (macFcf & MAC_FRAME_TYPE_MASK));
      }
      if (setFramePending) {
        ackFcf |= MAC_FRAME_FLAG_FRAME_PENDING;
      }
    }
    pktOffset = PHRLen;
    pkt[pktOffset++] = (uint8_t)ackFcf;
    pkt[pktOffset++] = (uint8_t)(ackFcf >> 8);

    if ((macFcf & MAC_FRAME_FLAG_SEQ_SUPPRESSION) == 0U) {
      pkt[pktOffset++] = seqNo;
    }
    // Determine outgoing ACK's address field sizes
    index = (((ackFcf & (MAC_FRAME_SOURCE_MODE_MASK | MAC_FRAME_VERSION_2012)) >> 10)
             | ((ackFcf & MAC_FRAME_DESTINATION_MODE_MASK) >> 9)
             | ((ackFcf & MAC_FRAME_FLAG_PANID_COMPRESSION) >> 6));
    addrSizes = ieee802154Table7p2[index];
    if (addrSizes == 0xFFFFU) {
      // Uh-oh! Enh-ACK would be malformed?!  Something funky happened!
      // Possibly a latency-induced issue.
      return;
    }
    // DstPan = SrcPan of incoming if avail otherwise DstPan of incoming
    if ((addrSizes & ADDRSIZE_DST_PAN_MASK) != 0U) {
      if (srcPan[0] > 0U) {
        pkt[pktOffset++] = srcPan[1];
        pkt[pktOffset++] = srcPan[2];
      } else if (dstPan[0] > 0U) {
        pkt[pktOffset++] = dstPan[1];
        pkt[pktOffset++] = dstPan[2];
      } else {
        // Uh-oh! Outgoing packet needs a DstPanId but incoming had neither!
        // Possibly a latency-induced issue.
        return;
      }
    }
    // DstAdr = SrcAdr of incoming packet -- their sizes should match
    if ((addrSizes & ADDRSIZE_DST_ADR_MASK) != 0U) {
      for (uint8_t i = 1U; i <= srcAdr[0]; i++) {
        pkt[pktOffset++] = srcAdr[i];
      }
    }
    // SrcPan = DstPan of incoming if avail otherwise SrcPan of incoming
    if ((addrSizes & ADDRSIZE_SRC_PAN_MASK) != 0U) {
      if (dstPan[0] > 0U) {
        pkt[pktOffset++] = dstPan[1];
        pkt[pktOffset++] = dstPan[2];
      } else if (srcPan[0] > 0U) {
        pkt[pktOffset++] = srcPan[1];
        pkt[pktOffset++] = srcPan[2];
      } else {
        // Uh-oh! Outgoing packet needs a SrcPanId but incoming had neither!
        // Possibly a latency-induced issue.
        return;
      }
    }
    // SrcAdr = DstAdr of incoming packet -- their sizes should match
    if ((addrSizes & ADDRSIZE_SRC_ADR_MASK) != 0U) {
      for (uint8_t i = 1U; i <= dstAdr[0]; i++) {
        pkt[pktOffset++] = dstAdr[i];
      }
    }

    // Fill in PHR now that we know Enh-ACK's length
    if (PHRLen == 2U) {
      pkt[0] = (0x08U /*FCS=2byte*/ | 0x10U /*Whiten=enabled*/);
      pkt[1] = (uint8_t)(__RBIT(pktOffset - 2U /*PHRLen*/ + 2U /*FCS*/) >> 24);
    } else {
      pkt[0] = (pktOffset - 1U /*PHRLen*/ + 2U /*FCS*/);
    }
    if ((RAIL_IEEE802154_WriteEnhAck(railHandle, pkt, pktOffset)
         == RAIL_STATUS_NO_ERROR)
        && setFramePending) {
      framePendingSetInOutgoingAck = true;
    }
    return;
  }

  // Immediate ACK -- RAIL will send, but may need to do FP lookup.
  if (!checkFramePending) {
    return;
  }
  uint16_t srcAdrLen = ((addrSizes & ADDRSIZE_SRC_ADR_MASK) >> ADDRSIZE_SRC_ADR_SHIFT);
  if (srcAdrLen == 0U) {
    // No source address info to do FP lookup
    return;
  }
  // Skip over DstPan, DstAdr, SrcPan to get to SrcAdr
  pktOffset += (((addrSizes & ADDRSIZE_DST_PAN_MASK) >> ADDRSIZE_DST_PAN_SHIFT)
                + ((addrSizes & ADDRSIZE_DST_ADR_MASK) >> ADDRSIZE_DST_ADR_SHIFT)
                + ((addrSizes & ADDRSIZE_SRC_PAN_MASK) >> ADDRSIZE_SRC_PAN_SHIFT));
  // At this point we expect the addressing info to be there
  if ((srcAdrLen + pktOffset) > packetInfo.packetBytes) {
    return; // Oops, maybe latency caused us to see a later incoming frame
  }
  if (srcAdrLen == 8U) {
    setFramePending = emberRadioDataPendingLongIdIsrCallback(
      &pkt[pktOffset], (macFcf & MAC_FRAME_TYPE_MASK));
  } else {
    uint16_t srcAdrShort = (pkt[pktOffset] | (pkt[pktOffset + 1U] << 8));
    setFramePending = emberRadioDataPendingShortIdIsrCallback(
      srcAdrShort, (macFcf & MAC_FRAME_TYPE_MASK));
  }

 #else//!MAC_SUPPORT_ENHANCED_ACKS

  // When Enh-ACK support is not needed, life is simplified by this
  // event only signalling frame-pending lookup.
 #if     USE_ENHANCED_FRAME_PENDING
  // Grab the MAC FCF low byte (for the Frame Type)
  uint8_t macFcfLOffset = PHRLen;
  uint16_t frameType;
  RAIL_RxPacketInfo_t packetInfo;
  RAIL_GetRxIncomingPacketInfo(emPhyRailHandle, &packetInfo);
  if (macFcfLOffset >= packetInfo.packetBytes) {
    return;
  }
  if (macFcfLOffset < packetInfo.firstPortionBytes) {
    frameType = packetInfo.firstPortionData[macFcfLOffset];
  } else {
    frameType = packetInfo.lastPortionData[macFcfLOffset
                                           - packetInfo.firstPortionBytes];
  }
  frameType &= MAC_FRAME_TYPE_MASK;
 #else//!USE_ENHANCED_FRAME_PENDING
  // RAIL has been configured to only signal this event on a true
  // MAC Data Poll command, so no need to retrieve frame type.
  uint16_t frameType = MAC_FRAME_TYPE_CONTROL;
 #endif//USE_ENHANCED_FRAME_PENDING

  RAIL_IEEE802154_Address_t address;
  if (RAIL_IEEE802154_GetAddress(railHandle, &address) != RAIL_STATUS_NO_ERROR) {
    return;
  }
  // Ask the network layer if it has any pending data for this device.
  if (address.length == RAIL_IEEE802154_ShortAddress) {
    setFramePending = emberRadioDataPendingShortIdIsrCallback(address.shortAddress,
                                                              frameType);
  } else if (address.length == RAIL_IEEE802154_LongAddress) {
    setFramePending = emberRadioDataPendingLongIdIsrCallback(address.longAddress,
                                                             frameType);
  } else {
    // In case someday we want this event signalled on all packet filter-pass
    // don't assert here.
  }

 #endif//MAC_SUPPORT_ENHANCED_ACKS

  //@TODO: Fix this when RAIL_LIB-323 inverted FP bit mode is supported in RAIL
  if (setFramePending) {
    if (RAIL_IEEE802154_SetFramePending(emPhyRailHandle) == RAIL_STATUS_NO_ERROR) {
      framePendingSetInOutgoingAck = true;
    }
  }
}

#define RECEIVE_BUF_LEN                ((SL_ZIGBEE_PHY_MAX_PHR_BYTES       \
                                         + SL_ZIGBEE_PHY_MAX_PAYLOAD_BYTES \
                                         + NUM_APPENDED_INFO_BYTES         \
                                         + 2 /*Safety Pad*/)               \
                                        & ~1)  // Make it even # bytes
static uint8_t rxBuffer[RECEIVE_BUF_LEN];

static void RAILCb_RxPacketReceived(RAIL_Handle_t railHandle)
{
  UNUSED_VAR(railHandle);
  RAIL_RxPacketInfo_t packetInfo;
  RAIL_RxPacketHandle_t packetHandle
    = RAIL_GetRxPacketInfo(emPhyRailHandle, RAIL_RX_PACKET_HANDLE_NEWEST,
                           &packetInfo);
  PHY_ASSERT(packetHandle != NULL);
  // Assume RAIL always includes CRC in packet length
  uint16_t packetLength = packetInfo.packetBytes;

  if (((packetInfo.packetStatus != RAIL_RX_PACKET_READY_SUCCESS) // CRC bad
       && (radioCheckCrc)) // am checking CRC
      // These additional PHR checks are done in case radio config or seq doesn't:
      // Enforce packet minimum size:
      || (packetLength < (PHRLen + 5))        // Pkt too small
      // Enforce packet maximum size:
      || ((size_t)(packetLength + NUM_APPENDED_INFO_BYTES) > sizeof(rxBuffer))
      ) {
    // Packet CRC failed -- ignore and filter it
    (void) halPtaStackEvent(SL_RAIL_UTIL_IEEE802154_STACK_EVENT_RX_CORRUPTED,
                            (uint32_t)emRadioIsReceivingFrame());
    return;
  }

  // Read the packet payload into rxBuffer[]
  PHY_ATTEST(RAIL_PeekRxPacket(emPhyRailHandle, packetHandle, rxBuffer,
                               packetLength, 0) == packetLength);
  uint8_t *pktToSendUp = rxBuffer;
  bool filterPacket = false;

  // Enforce packet maximum size (also works around RAIL_LIB-520):
  if (!(((PHRLen == 1)                            // 2.4GHz 1-byte PHR:
         && ((pktToSendUp[0] & 0x80) == 0x00))    // PktLen<7> == 0 (PktLen<128)
        || ((PHRLen != 1)                         // SubGHz 2-byte bit-rev PHR:
            && ((pktToSendUp[0] & 0xE8) == 0x08)  // PktLen<8..10> == 0, FCS=2byte
            && ((pktToSendUp[1] & 0x01) == 0x00)) // PktLen<7> == 0 (PktLen<128)
        )) { // Whitening and ModeSwitch fields are ignored in this check
    (void) halPtaStackEvent(SL_RAIL_UTIL_IEEE802154_STACK_EVENT_RX_CORRUPTED,
                            (uint32_t)emRadioIsReceivingFrame());
    return;
  }

  // Grab packet details
  RAIL_RxPacketDetails_t packetDetails;
  PHY_ATTEST_SUCCESS(RAIL_GetRxPacketDetailsAlt(emPhyRailHandle, packetHandle,
                                                &packetDetails));

  //@TODO: Hopefully remove/redo ACK processing when RAIL_LIB-518/534 fixed
  // check if received packet is an ACK
  if ((pktToSendUp[PHRLen] & MAC_FRAME_TYPE_MASK) == MAC_FRAME_TYPE_ACK) {
    (void) halPtaStackEvent(SL_RAIL_UTIL_IEEE802154_STACK_EVENT_RX_ENDED,
                            (uint32_t)emRadioIsReceivingFrame());
    // Generally pass ACKs up only when promiscuous
    filterPacket = addressMatchingEnabled;
    // check if we are waiting for an ACK
    // and packet was blessed by RAIL as the expected ACK
    if ((txState == PHY_TX_STATE_TRIG_ACKING)
        && (packetDetails.isAck)) { // This confirms RAIL matched seq#
      txState = PHY_TX_STATE_OFF;
      lastTxStartedTimeMs = -1; // Deem Tx completed
      if (enhAckSupported
          || (((pktToSendUp[PHRLen + 1] & (MAC_FRAME_VERSION_MASK >> 8))
               < (MAC_FRAME_VERSION_2012 >> 8)) // Version 2003/2006
              && (packetLength == (PHRLen + 5)))) {
        uint8_t framePendingInAck = !!(pktToSendUp[PHRLen]
                                       & MAC_FRAME_FLAG_FRAME_PENDING);
        (void) halPtaStackEvent(SL_RAIL_UTIL_IEEE802154_STACK_EVENT_TX_ACK_RECEIVED,
                                (uint32_t)framePendingInAck);
        sli_802154phy_radio_transmit_complete_callback(SL_STATUS_TRANSMIT_ACK_RECEIVED,
                                                       lastTxSfdTime,
                                                       framePendingInAck);
        // Always filter ACKs that we were expecting except for
        // (Enhanced) ACKs of size > 5 which may have IEs or payload.
        filterPacket = (packetLength <= (PHRLen + 5));
      } else {
        // Received an Enhanced ACK that RAIL blessed by mistake.
        // SL_RAIL_UTIL_IEEE802154_STACK_EVENT_TX_ACK_TIMEDOUT signalled by callback
        sli_802154phy_radio_transmit_complete_callback(SL_STATUS_MAC_NO_ACK_RECEIVED,
                                                       lastTxSfdTime, 0);
      }
    }
  } else if (packetDetails.isAck) {
    // RAIL thought this packet was an ACK even though the frame type didn't
    // match. This can only happen in promiscuous mode. RAILCb_AckTimeout()
    // will check that (txState == PHY_TX_STATE_TRIG_ACKING) to make sure
    // that we don't get into an invalid state.
    // @TODO: Remove when RAIL_LIB-778 is fixed
    RAILCb_RxAckTimeout(NULL);
  }
  if (!filterPacket) {
    // We have a 'good' packet to send up to the MAC:
    // Can cancel PTA request now if not planning to ACK this frame:
    if ( addressMatchingEnabled
         && (pktToSendUp[PHRLen] & MAC_FRAME_FLAG_ACK_REQUIRED)
         && autoAckEnabled ) {
      // ACK will, is, or did, go out, or is blocked by RHO.
     #if      (!RHO_USE_BLOCKTX)
      if (RAIL_IsRxAutoAckPaused(emPhyRailHandle)) {
        (void) halPtaStackEvent(SL_RAIL_UTIL_IEEE802154_STACK_EVENT_RX_ACK_BLOCKED,
                                (uint32_t)emRadioIsReceivingFrame());
      } else
     #endif //(!RHO_USE_BLOCKTX)
      {
        (void) halPtaStackEvent(SL_RAIL_UTIL_IEEE802154_STACK_EVENT_RX_ACKING,
                                (uint32_t)emRadioIsReceivingFrame());
      }
    } else {
      // No ACK needed, or AUTO_ACK is off.
      // Can cancel PTA now (already did this for ACKs above).
      if ((pktToSendUp[PHRLen] & MAC_FRAME_TYPE_MASK) != MAC_FRAME_TYPE_ACK) {
        (void) halPtaStackEvent(SL_RAIL_UTIL_IEEE802154_STACK_EVENT_RX_ENDED,
                                (uint32_t)emRadioIsReceivingFrame());
      }
      // Make sure to inform upper layer it should *not* send indirect
      // data to a poller that might have waited for it if an ACK with
      // FP actually made it on the air.
      framePendingSetInOutgoingAck = false;
    }
    // We want Rx timestamps at sync-detect, so convert.
    // packetLength includes PHR and CRC bytes.
    packetDetails.timeReceived.totalPacketBytes = packetLength;
    RAIL_GetRxTimeSyncWordEndAlt(emPhyRailHandle, &packetDetails);
    if (PHRLen == 2) {
      // Convert 2-byte 15.4g PHR into 1-byte 15.4 PHR for upper layers
      // by skipping over the 1st PHR byte and putting the length bits
      // in the 2nd PHR byte in the proper order for software.  We do
      // not support PHY payloads > 255 bytes here.
      pktToSendUp++;
      packetLength--;
      pktToSendUp[0] = (uint8_t)(__RBIT(pktToSendUp[0]) >> 24);
    }
    PHY_ASSERT((pktToSendUp[0] + 1) == packetLength);

    // add appended info
    uint8_t *appendedInfo = &pktToSendUp[packetLength];
    appendedInfo[APPENDED_INFO_CHANNEL_OVERWRITE_BYTE_INDEX] = emRadioChannel;
    appendedInfo[APPENDED_INFO_NETWORK_OVERWRITE_BYTE_INDEX]
      = sli_mac_get_current_radio_network_index(PHY_THIS_ID)
        | (PHY_THIS_ID << APPENDED_INFO_NETWORK_OVERWRITE_PHY_ID_SHIFT);
    // We now apply RSSI_DBM_OFFSET_RAIL so upper layers need not be PHY-specific
    appendedInfo[APPENDED_INFO_RSSI_BYTE_INDEX] = packetDetails.rssi
                                                  + RSSI_DBM_OFFSET_RAIL;
    appendedInfo[APPENDED_INFO_LQI_BYTE_INDEX]  = packetDetails.lqi;

    // Packet received timestamp
    appendedInfo[APPENDED_INFO_MAC_TIMER_BYTE_3_INDEX] = BYTE_3(packetDetails.timeReceived.packetTime);
    appendedInfo[APPENDED_INFO_MAC_TIMER_BYTE_2_INDEX] = BYTE_2(packetDetails.timeReceived.packetTime);
    appendedInfo[APPENDED_INFO_MAC_TIMER_BYTE_1_INDEX] = BYTE_1(packetDetails.timeReceived.packetTime);
    appendedInfo[APPENDED_INFO_MAC_TIMER_BYTE_0_INDEX] = BYTE_0(packetDetails.timeReceived.packetTime);

    packetLength += NUM_APPENDED_INFO_BYTES;
    sli_zigbee_packet_received_internal_isr_callback(pktToSendUp, packetLength,
                                                     framePendingSetInOutgoingAck);
    framePendingSetInOutgoingAck = false;
  }
}

// ## Miscellaneous Events ##

static void RAILCb_CalNeeded(RAIL_Handle_t railHandle)
{
  UNUSED_VAR(railHandle);
  sli_zigbee_stack_radio_needs_calibrating_handler();
}

// Map RAIL asserts to Ember asserts
void RAILCb_AssertFailed(RAIL_Handle_t railHandle,
                         uint32_t errorCode)
{
  // Call the halInternalAssertFailed() API directly with the file name set to
  // librail.a and the line number set to the errorCode for debugging.
  UNUSED_VAR(railHandle);
  halInternalAssertFailed("librail.a", errorCode);
}

// ## Recent API Additions ##

#ifndef emRadioPowerFem // Not stubbed so need to provide a real implementation
// Called in main-line context (never ISR context) before the PHY actually
// powers up the radio for Rx or Tx and after the PHY has powered it down.
void emRadioPowerFem(bool powerUp)
{
  // Only notify the HAL when preparing to power up from OFF
  // or after having really powered down, and such notification
  // is not redundant.
  static bool femPowered = false;
  if ((radioCurrentPowerStatus == SL_ZIGBEE_RADIO_POWER_MODE_OFF)
      && (powerUp != femPowered)) {
    femPowered = powerUp;
  }
}
#endif//emRadioPowerFem

uint32_t emPhyGetLastTxCompleteTimeMs(bool lastCsmaTx)
{
  return (lastCsmaTx ? lastCsmaTxFinishedTimeMs : lastTxFinishedTimeMs);
}

#if     PHY_DUAL
uint32_t emPhySymbolsToUs(uint32_t symbols)
{
  return SL_ZIGBEE_PHY_SYMBOLS_TO_US(symbols);
}
#endif//PHY_DUAL

static bool isPhyTimerExpired(int32_t timerMs, uint16_t timeoutMs)
{
  return (timerMs >= 0)
         && (elapsedTimeInt16u((uint16_t)timerMs,
                               halCommonGetInt16uMillisecondTick()) > timeoutMs);
}

static void phyTimerTick(void)
{
  if ( isPhyTimerExpired(lastTxStartedTimeMs, MAC_TX_TIMEOUT_MS)) {
    PHY_ASSERT_INTERNAL(txState > PHY_TX_STATE_OFF);
    PHY_ASSERT_INTERNAL(false);
    // Attempt to recover if asserts are off
    if (txState == PHY_TX_STATE_TRIG_ACKING) {
      RAILCb_RxAckTimeout(NULL);
    } else {
      MACSEQ_TransmitErrorEvent(SL_STATUS_TRANSMIT_INCOMPLETE);
    }
  }
}

void emPhyTick(bool isrContext)
{
  // Called by sl_zigbee_tick()
  if (!isrContext) {
    emPhyDcTick();
    // Protect against re-entrancy, to read a consistent state of
    // volatile globals, and for issuing callbacks
    DECLARE_INTERRUPT_STATE;
    DISABLE_INTERRUPTS();
    (void) halPtaStackEvent(SL_RAIL_UTIL_IEEE802154_STACK_EVENT_TICK, 0U);
    phyTimerTick();
    RESTORE_INTERRUPTS();
  } else {
    phyTimerTick();
  }
}

const uint8_t emPhyCcaCsmaConfigArray[] = { 0 }; // Dummy to distinguish CSMA from LBT
const uint8_t emPhyCcaLbtConfigArray[]  = { 0 }; // Dummy to distinguish CSMA from LBT

sl_status_t emPhyGetDutyCycleParams(uint8_t macPgChan,
                                    EmPhyDutyCycleParams *dutyCycleParams)
{
  // Quickly reject an illegal channel#
  if (sli_802154mac_pg_chan_ch(macPgChan) >= MAX_CHANNELS_PER_PAGE) {
    return SL_STATUS_TRANSMIT_INVALID_CHANNEL;
  }
  EmPhySubBandConfig *subBandConfig = NULL;
  EmPhyBandConfig *bandConfig = mapMacPgChanToPhyChanBand(macPgChan, NULL,
                                                          NULL, &subBandConfig);
  if ((bandConfig == NULL) || (subBandConfig == NULL)) {
    return SL_STATUS_TRANSMIT_INVALID_CHANNEL;
  }
  // Purposefully shadow global EM_PHY_CURRENT_BAND_CONFIG for below
  // calculations using SL_ZIGBEE_PHY_SYMBOLS_TO_US() so they apply to
  // bandConfig and not the global currently-active bandConfig.
  #undef  EM_PHY_CURRENT_BAND_CONFIG
  #define EM_PHY_CURRENT_BAND_CONFIG bandConfig
  if (dutyCycleParams == NULL) {
    PHY_ASSERT(false);
    return SL_STATUS_FAIL;
  }
  dutyCycleParams->lbtRdcDenom = subBandConfig->lbtDutyDenom;
  dutyCycleParams->criRdcDenom = subBandConfig->criDutyDenom;
  dutyCycleParams->nonRdcDenom10 = subBandConfig->nonDutyDenom10;
  dutyCycleParams->byteTimeUs = (
    ((1000000UL /*us/sec*/ * SL_ZIGBEE_PHY_BYTE_BITS)
     + (bandConfig->dataRateBps - 1)                 // Ceiling
    ) / bandConfig->dataRateBps
    );
  dutyCycleParams->pktOvhBytes = (
    ((SL_ZIGBEE_PHY_RAMP_TIME_BITS                       // Ramp up PA
      + SL_ZIGBEE_PHY_RAMP_SETTLE_BITS                   // Settle PA
      + bandConfig->preambleBits                     // Preamble
      + bandConfig->sfdBits                          // SFD (SHR)
      + SL_ZIGBEE_PHY_RAMP_TIME_BITS                     // Ramp down PA
      + 7                                            // Ceiling
      ) / 8)                                         // Bits-to-Bytes
    );
  dutyCycleParams->lbtParams = subBandConfig->subBandParams;
  if (subBandConfig->subBandParams == NULL) {
    dutyCycleParams->lbtFlags        = EM_PHY_LBT_FLAG_MODE_CSMA;
    dutyCycleParams->lbtMinOffTimeUs = 0;       // No limit
    dutyCycleParams->lbtCcaTimeUs    = 0;       // No limit
    dutyCycleParams->maxSeqBytes     = 0;       // No limit
    dutyCycleParams->maxPktBytes     = 0;       // No limit
  } else {
    dutyCycleParams->lbtFlags        = (subBandConfig->ccaConfigArray
                                        == emPhyCcaLbtConfigArray)
                                       ? EM_PHY_LBT_FLAG_MODE_LBT
                                       : EM_PHY_LBT_FLAG_MODE_CSMA;
    dutyCycleParams->lbtMinOffTimeUs = SL_ZIGBEE_PHY_SYMBOLS_TO_US(
      subBandConfig->subBandParams->dutyTxOffSym
      );
    dutyCycleParams->lbtCcaTimeUs    = SL_ZIGBEE_PHY_SYMBOLS_TO_US(
      subBandConfig->subBandParams->lbtCcaSym
      );
    dutyCycleParams->maxSeqBytes     = (
      subBandConfig->subBandParams->dutyTxMaxSeqByte
      );
    dutyCycleParams->maxPktBytes     = (
      subBandConfig->subBandParams->dutyTxMaxPktByte
      );
  }
  return SL_STATUS_OK;
}

void emRadioEnableReceiveCompleteInt(bool enable)
{
  UNUSED_VAR(enable);
}

bool emRadioReceiveCompleteIntEnabled(void)
{
  return false;
}

bool emRadioReceiveCompleteIntPending(bool acknowledgePending)
{
  UNUSED_VAR(acknowledgePending);
  return false;
}

void emRadioEnableSfdRecvNotification(bool enable)
{
  sfdRecvNotificationEnabled = enable;
  changeDynamicEvents();
}

bool emRadioSfdRecvNotificationEnabled(void)
{
  return sfdRecvNotificationEnabled;
}

WEAK(void emRadioSfdRecvIsrCallback(void))
{
  // no-op
}

void emRadioEnableConfigScheduledCallback(bool enable)
{
  updateEvents(RAIL_EVENT_CONFIG_SCHEDULED, enable ? RAIL_EVENT_CONFIG_SCHEDULED : RAIL_EVENTS_NONE);
}

bool emRadioConfigScheduledCallbackEnabled(void)
{
  return (currentEventConfig & RAIL_EVENT_CONFIG_SCHEDULED);
}

WEAK(void emRadioConfigScheduledCallback(bool scheduled))
{
  // no-op
  UNUSED_VAR(scheduled);
}

static void RAILCb_Events(RAIL_Handle_t railHandle, RAIL_Events_t events)
{
  // Deal with time-sensitive pre-completion Rx-related events first:
  //@TODO: Figure out a reasonable ordering between pre-completion and
  //       a completion event when they're presented simultaneously:
  //       unfortunately unlike Tx, the pre-completion events might be
  //       for the completed packet or for a new one that started after...
  if (events & RAIL_EVENT_RX_SYNC1_DETECT) {
    MACSEQ_RxFrameDetected(0);
  } else if (events & RAIL_EVENT_RX_SYNC2_DETECT) {
    MACSEQ_RxFrameDetected(1);
  }
  if (events & RAIL_EVENT_IEEE802154_DATA_REQUEST_COMMAND) {
    RAILCb_IEEE802154_DataRequestCommand(railHandle);
  }
  if (events & RAIL_EVENT_RX_FILTER_PASSED) {
    MACSEQ_FilterPassedEvent();
  }

  // Deal with normal Tx-related events next, because some Rx completion
  // events like ACK reception depend on the Tx state being up-to-date:
  if (txState <= PHY_TX_STATE_OFF) {
    // No normal transmit is pending
    // This is a spurrious event (e.g. after sleeping radio)
    // that we can ignore.
  } else {
    // An event has occurred while a normal transmit was in-progress.
    // When a completion event is presented, only one would occur and
    // any pre-completion events also presented simultaneously can be
    // ignored.
    if (events & RAIL_EVENT_TX_PACKET_SENT) {
      RAILCb_TxPacketSent(railHandle);
    } else
    if (events & (RAIL_EVENT_TX_CHANNEL_BUSY
                  | RAIL_EVENT_TX_BLOCKED)) {
      // These events imply the PA never ramped, so nothing went on-air.
      // TX_BLOCKED is triggered by PTA/RHO blocking a transmit of a normal
      // packet, which is treated the same as a CSMA failure.
      MACSEQ_LBTFailureEvent();
    } else
    if (events & RAIL_EVENT_TX_ABORTED) {
      // This error implies the PA ramped and some on-air time occurred.
      MACSEQ_TransmitErrorEvent(SL_STATUS_TRANSMIT_INCOMPLETE);
    } else
    if (events & RAIL_EVENT_TX_UNDERFLOW) {
      // This error implies the PA ramped and some on-air time occurred.
      // Technically, TX_UNDERFLOW should indicate a bug and assert()
      // but looking forward to the day we support extra long frames, it
      // will mean we couldn't keep pace, and will need to clean up and
      // try again.
      MACSEQ_TransmitErrorEvent(SL_STATUS_TRANSMIT_UNDERFLOW);
    } else {
      // Pre-completion events occur are processed in their logical order:
      if (events & RAIL_EVENT_TX_START_CCA) {
        // We are starting RXWARM for a CCA check
        MACSEQ_TOUT0MatchLBTEvent();
      }
      if (events & RAIL_EVENT_TX_CCA_RETRY) {
        // We failed a CCA check and need to retry
        MACSEQ_LBTRetryEvent();
      }
      if (events & RAIL_EVENT_TX_CHANNEL_CLEAR) {
        // We're going on-air
        (void) halPtaStackEvent(SL_RAIL_UTIL_IEEE802154_STACK_EVENT_TX_STARTED, 0U);
      }
    }
  }

  // Deal with Rx completion events next - this includes ACKs sent after an Rx:
  if (events & RAIL_EVENT_RX_PACKET_RECEIVED) {
    RAILCb_RxPacketReceived(railHandle);
  }
  if (events & RAIL_EVENT_RX_FRAME_ERROR) {
    MACSEQ_FrameErrorEvent();
  }
  // The following 3 events cause us to not receive a packet
  if (events & (RAIL_EVENT_RX_PACKET_ABORTED
                | RAIL_EVENT_RX_ADDRESS_FILTERED
                | RAIL_EVENT_RX_FIFO_OVERFLOW)) {
    MACSEQ_RxAbortedEvent();
  }
  if (events & RAIL_EVENT_TXACK_PACKET_SENT) {
    MACSEQ_AckDoneEvent(SL_RAIL_UTIL_IEEE802154_STACK_EVENT_RX_ACK_SENT);
  }
  if (events & (RAIL_EVENT_TXACK_ABORTED | RAIL_EVENT_TXACK_UNDERFLOW)) {
    MACSEQ_AckDoneEvent(SL_RAIL_UTIL_IEEE802154_STACK_EVENT_RX_ACK_ABORTED);
  }
  if (events & RAIL_EVENT_TXACK_BLOCKED) {
    MACSEQ_AckDoneEvent(SL_RAIL_UTIL_IEEE802154_STACK_EVENT_RX_ACK_BLOCKED);
  }

  // Deal with ACK timeout after possible RX completion in case RAIL
  // notifies us of the ACK and the timeout simultaneously -- we want
  // the ACK to win over the timeout.
  if (events & RAIL_EVENT_RX_ACK_TIMEOUT) {
    RAILCb_RxAckTimeout(railHandle);
  }

  // Deal with other administrative events last:
  if (events & RAIL_EVENT_CONFIG_UNSCHEDULED) {
    // An Rx possibly got aborted so must cancel PTA Rx
    (void) halPtaStackEvent(SL_RAIL_UTIL_IEEE802154_STACK_EVENT_RX_IDLED, 0U);
    // The only Tx situation we care about being unscheduled is during ACK-wait
    // where this event means we will never get the ACK we were waiting for.
    RAILCb_RxAckTimeout(railHandle);

    // Notify the user of scheduled/unscheduled events if the feature is enabled
    if (emRadioConfigScheduledCallbackEnabled()) {
      // Application callback that user can sign up for
      emRadioConfigScheduledCallback(false);
    }
  }
  if (events & RAIL_EVENT_CONFIG_SCHEDULED) {
    // Application callback that user can sign up for
    emRadioConfigScheduledCallback(true);
  }
  if (events & RAIL_EVENT_SCHEDULER_STATUS) {
    schedulerEventHandler(railHandle);
  }
  if (events & RAIL_EVENT_CAL_NEEDED) {
    RAILCb_CalNeeded(railHandle);
  }
}

// Use antenna-diversity radio configuration

#ifndef RAIL_RX_OPTIONS_ANTENNA
#define RAIL_RX_OPTIONS_ANTENNA \
  (RAIL_RX_OPTION_ANTENNA0 | RAIL_RX_OPTION_ANTENNA1)
#endif//RAIL_RX_OPTIONS_ANTENNA

static RAIL_Events_t getRxAntOptions(sl_rail_util_antenna_mode_t mode)
{
  switch (mode) {
    default:
    case SL_RAIL_UTIL_ANTENNA_MODE_DISABLED: // Leave antenna untouched (e.g. from Tx)
      return RAIL_RX_OPTIONS_NONE;
      break;
    case SL_RAIL_UTIL_ANTENNA_MODE_ENABLE1:
      return RAIL_RX_OPTION_ANTENNA0;
      break;
    case SL_RAIL_UTIL_ANTENNA_MODE_ENABLE2:
      return RAIL_RX_OPTION_ANTENNA1;
      break;
    case SL_RAIL_UTIL_ANTENNA_MODE_DIVERSITY: // Use radio config's diversity scheme
      return RAIL_RX_OPTIONS_ANTENNA;
      break;
  }
}

sl_status_t emRadioConfigRxAntenna(sl_rail_util_antenna_mode_t mode)
{
  if (emPhyRailHandle == NULL) {
    // This call is premature, before radio is initialized.
    // Defer to when we're re-called as part of halPluginConfig2p4GHzRadio().
    return SL_STATUS_OK;
  }
  // Tell RAIL what Rx antenna mode to use
  return (RAIL_ConfigRxOptions(emPhyRailHandle, RAIL_RX_OPTIONS_ANTENNA,
                               getRxAntOptions(mode))
          == RAIL_STATUS_NO_ERROR) ? SL_STATUS_OK : SL_STATUS_FAIL;
}

#endif//(PHY_DUALRAIL && !PHY_THIS && !PHY_INCLUDE_SOURCE)
