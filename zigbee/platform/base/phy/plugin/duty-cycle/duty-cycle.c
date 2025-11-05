/***************************************************************************//**
 * @file
 * @brief Duty-Cycle support
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

#ifdef  SL_ZIGBEE_TEST
#include "core/sl_zigbee_stack.h"
#include "hal/hal.h"
#else//!SL_ZIGBEE_TEST
#define LEGACY_PHY_BUILD 1
#include "phy/hal.h"
#include "core/sl_zigbee_stack.h"
#endif//SL_ZIGBEE_TEST
#include "phy/phy.h"
#include "phy/symbol-timer.h"

#if     (PHY_DUAL && (PHY_THIS == PHY_THIS_UNSPECIFIED))

// Let the phy/dual/phy.c Mux layer implementation replace this one

#else//!(PHY_DUAL && (PHY_THIS == PHY_THIS_UNSPECIFIED))

#if     PHY_HAS_DUTY_CYCLE

#include "duty-cycle.h"                // Public API
#include "duty-cycle-test.h"           // Test API

#ifndef PHY_DC_DEBUG
#define PHY_DC_DEBUG 0
#endif//PHY_DC_DEBUG

#ifndef LBT_ACKS_RESTART_TX_OFF_TIME
#define LBT_ACKS_RESTART_TX_OFF_TIME 0
#endif//LBT_ACKS_RESTART_TX_OFF_TIME

#ifndef LBT_TX_OFF_APPLIES_TO_NON_LBT
#define LBT_TX_OFF_APPLIES_TO_NON_LBT 0
#endif//LBT_TX_OFF_APPLIES_TO_NON_LBT

#ifndef LBT_POLICY_AVOID_UNTIL_NEEDED
#define LBT_POLICY_AVOID_UNTIL_NEEDED 0
#endif//LBT_POLICY_AVOID_UNTIL_NEEDED

#ifndef LBT_ACKS_IN_REQUESTORS_DC
#define LBT_ACKS_IN_REQUESTORS_DC 0
#endif//LBT_ACKS_IN_REQUESTORS_DC

#ifndef LBT_LISTEN_DURING_BACKOFF
#define LBT_LISTEN_DURING_BACKOFF 0
#endif//LBT_LISTEN_DURING_BACKOFF

#ifndef LBT_IN_SOFTWARE
#define LBT_IN_SOFTWARE 1
#endif//LBT_IN_SOFTWARE

#if     PHY_DC_DEBUG
#define DC_DEBUG(blah)  blah
#else//!PHY_DC_DEBUG
#define DC_DEBUG(blah)  /*no-op*/
#endif//PHY_DC_DEBUG

#ifdef  CORTEXM3_EFR32_MICRO
 #include "em_gpio.h"
 #define DC_GPIO_INIT() (GPIO_PinModeSet(gpioPortC, 10, gpioModePushPull, 0))
 #define DC_GPIO_OFF() (GPIO->P[2].DOUTTGL = (1u << 10)) /*PC10*/
 #define DC_GPIO_ON()  (GPIO->P[2].DOUTTGL = (1u << 10)) /*PC10*/
#ifdef  SL_ZIGBEE_TEST
 #define PTI_TRACE(value) /*no-op*/
#else//!SL_ZIGBEE_TEST
 #define PTI_TRACE(value) PTI_AuxdataOutput(value)
extern void PTI_AuxdataOutput(uint8_t value);
#endif//SL_ZIGBEE_TEST
#else//!CORTEXM3_EFR32_MICRO
 #define DC_GPIO_INIT() /*no-op*/
 #define DC_GPIO_OFF()  /*no-op*/
 #define DC_GPIO_ON()   /*no-op*/
 #define PTI_TRACE(value) /*no-op*/
#endif//CORTEXM3_EFR32_MICRO

#ifdef SL_ZIGBEE_TEST
#define IN_INTERRUPT_CONTEXT() 0
#else
#define IN_INTERRUPT_CONTEXT() (CORE_IN_IRQ_CONTEXT())
#endif

#define PTI_LBTSTARTSW  0x20
#define PTI_LBTSTOP     0x22
#define PTI_LBTFAILURE  0x27
#define PTI_LBTSUCCESS  0x28
#define PTI_LBTRETRY    0x29
#define PTI_LBTTIMEOUT  0x2A
#define PTI_LBTCCASTART 0x2B
#define PTI_LBTCCACLEAR 0x2C

#define DC_BUCKET_TICKS  (MILLISECOND_TICKS_PER_SECOND * EM_PHY_DC_BUCKET_SECS)
#define DC_BUCKET_START_TIME_TICK(msTick) /*Align bucket start time*/ \
  (((msTick) / DC_BUCKET_TICKS) * DC_BUCKET_TICKS)
#define DC_BUCKET_INDEX(bucketTicks) \
  ((bucketTicks / DC_BUCKET_TICKS) % DC_NUM_BUCKETS)
#define DC_ON_AIR_BYTES(psduBytes) ((psduBytes)        \
                                    + dcCurPktOvhBytes \
                                    + SL_ZIGBEE_PHY_MAX_PHR_BYTES)
#define ACK_PKT_LENGTH 5 // ACK payload is 5 bytes
#define DC_ON_AIR_BYTES_ACK DC_ON_AIR_BYTES(ACK_PKT_LENGTH)
#define DC_INFINITE_BYTES_SENTINAL INT32_MAX

extern uint8_t sli_legacy_mfglib_mode;
#define ackRequested(pkt) ((((pkt)[1] & MAC_FRAME_FLAG_ACK_REQUIRED) != 0U) \
                           && radioTransmitConfig.waitForAck                \
                           && (!sli_legacy_mfglib_mode))

#if     (PHY_PRO2PLUS || defined(SL_ZIGBEE_TEST))
// PTA is not supported by these PHYs or in Connect
#define issuePtaStackEvent(ptaEvent, supplement) /*no-op*/
#else//!(PHY_PRO2PLUS || ...)
#include "coexistence/protocol/ieee802154_uc/coexistence-802154.h"
#define issuePtaStackEvent(ptaStackEvent, supplement) \
  ((void) sl_rail_util_ieee802154_on_event(ptaStackEvent, supplement))
#endif//(PHY_PRO2PLUS || ...)

static EmPhyDutyCycle dcDutyCycles[EM_PHY_DC_MAX_CHANNELS];
static EmPhyDutyCycle* dcCurChanDc = NULL;
static EmPhyDcState dcCurState = EM_PHY_DC_STATE_OFF;
static EmPhyDcMode dcCurMode = EM_PHY_DC_MODE_LBT; // Default to LBT in stacks
static uint8_t dcCurPktOvhBytes = 0;
static uint8_t dcTimeAccel = 1; // Default to no time acceleration
static uint8_t dcLimitAccel = 1; // Default to no limit acceleration
static volatile uint8_t dcPktFailedOnAirBytes;
static EmPhyDcLimits dcLimitsCfg; // Inited to all EM_PHY_DC_LIMIT_USE_DEFAULT
#define EM_PHY_DC_LIMITED_DEFAULT_HECTOPCT 150; // mibDUTYCYCLELimitedThresh 1.5%
#if     !LBT_ACKS_IN_REQUESTORS_DC
static bool dcAcksSuspended = false;
static volatile uint8_t dcAcksSent;
#endif//!LBT_ACKS_IN_REQUESTORS_DC
static uint8_t* dcTxPacket;
static EmPhyDcMode dcTxMode;
static uint32_t dcTxLimitMs;

extern sli_zigbee_event_queue_t sli_zigbee_stack_event_queue;
void emPhyDcTxEventHandler(sli_zigbee_event_t *event);
sli_zigbee_event_t emPhyDcTxEvent = { {
                                        &sli_zigbee_stack_event_queue,
                                        emPhyDcTxEventHandler,
                                        NULL,
                                        "Phy DC TX event"
                                      },
                                      NULL,
                                      0,
                                      0,
                                      NULL };

sli_zigbee_event_t emPhyDcTxIsrEvent = { {
                                           &sli_zigbee_stack_event_queue,
                                           emPhyDcTxEventHandler,
                                           sli_zigbee_isr_event_marker,
                                           "Phy DC TX ISR event"
                                         },
                                         NULL,
                                         0,
                                         0,
                                         NULL };
static uint8_t ccaAttempt = 0;

#if     LBT_IN_SOFTWARE

static uint16_t lastBackoff = 0;

typedef enum {
  EM_PHY_LBT_STATE_IDLE = 0,    // 0 Initial and final state, incl DC's TX_OFF
  EM_PHY_LBT_STATE_BACKOFF,     // 1 LBT backing off
  EM_PHY_LBT_STATE_CCA,         // 2 LBT performing CCA
  EM_PHY_LBT_STATE_BUSY,        // 3 LBT channel busy waiting for it to free
  EM_PHY_LBT_STATES             // Must be last
} EmPhyLbtState;
static EmPhyLbtState lbtState = EM_PHY_LBT_STATE_IDLE;
#define lbtTry ccaAttempt
static uint8_t lbtTries;
static uint16_t lbtBackoffTimeSym;
static uint32_t lbtCcaEndTimeSymTick;
static uint32_t lbtOverallEndTimeSymTick;

#endif//LBT_IN_SOFTWARE

//-- Internal Helper Functions

// Return "now" multiplied by the time acceleration factor
static uint32_t nowMsTick(void)
{
  return (halCommonGetInt32uMillisecondTick() * dcTimeAccel);
}

static uint64_t nowMsTick64(void)
{
  return (halCommonGetInt64uMillisecondTick() * dcTimeAccel);
}

#ifndef elapsedTimeInt64u
#define elapsedTimeInt64u(oldTime, newTime) \
  ((uint64_t) ((uint64_t)(newTime) - (uint64_t)(oldTime)))
#endif

// Find and return the EmPhyDutyCycle{} for the specified channel.
// EMZIGBEE-4299 - If not found and no entries free, reuse the entry
// with the lowest consumed bytes rather than the oldest entry --
// this better handles channel scans returning to the original
// channel without losing the original channel's DC.
static EmPhyDutyCycle* dcFindChannelDutyCycle(uint8_t macPgChan)
{
  EmPhyDutyCycle* thisEntry = &dcDutyCycles[0];
  EmPhyDutyCycle* reuseEntry = thisEntry; // A starting guess
  EmPhyDutyCycle* freeEntry = NULL;
  for (; thisEntry < &dcDutyCycles[EM_PHY_DC_MAX_CHANNELS]; thisEntry++) {
    if (thisEntry->macPgChanPlusOne - 1 == macPgChan) {
      return thisEntry;
    }
    if ((freeEntry == NULL) && (thisEntry->macPgChanPlusOne == 0)) {
      freeEntry = thisEntry;
    }
    if (thisEntry->dcPeriodConsumedBytes < reuseEntry->dcPeriodConsumedBytes) {
      reuseEntry = thisEntry;
    }
  }
  // No entry found for this channel ...
  // return a free entry or clear & reuse the indicated entry
  if (freeEntry != NULL) {
    return freeEntry;
  }
  memset(reuseEntry, 0, sizeof(EmPhyDutyCycle));
  return reuseEntry;
}

// Return the max available bytes depending on the mode
static int32_t dcMaxAvailBytesForMode(EmPhyDcMode dcMode)
{
  switch (dcMode) {
    case EM_PHY_DC_MODE_OFF:
      return DC_INFINITE_BYTES_SENTINAL;
    case EM_PHY_DC_MODE_TALK:
    case EM_PHY_DC_MODE_CSMA:
      return dcCurChanDc->dcPeriodNonRdcBytes;
    case EM_PHY_DC_MODE_DIALOG:
    case EM_PHY_DC_MODE_LBTX:
    case EM_PHY_DC_MODE_LBT:
      return dcCurChanDc->dcPeriodLbtRdcBytes;
    default:
      assert(false);
      return 0;
  }
}

static bool dcCallbackActive = false;

static EmPhyDcState dcStateForConsumed(int32_t consumed)
{
  return (EmPhyDcState)
         (((consumed > dcCurChanDc->dcPeriodNonRdcBytes)    << 0)
          + ((consumed > dcCurChanDc->dcPeriodThresholdBytes) << 1)
          + ((consumed > dcCurChanDc->dcPeriodCriticalBytes)  << 2)
          + ((consumed > dcCurChanDc->dcPeriodLbtRdcBytes)    << 3)
          + EM_PHY_DC_STATE_NORMAL
         );
}

// Check if the Duty Cycle State has changed, and issue callback if so.
// The State might have changed due to an earlier Mode change.
// The callback may select a new Duty Cycle Mode to put into effect.
static void dcCheckStateUpdateMode(void)
{
  EmPhyDcState newState;
  if (dcCurChanDc == NULL) { // No DC enforced or monitored
    newState = EM_PHY_DC_STATE_OFF;
  } else {
    if (dcCurMode == EM_PHY_DC_MODE_OFF) {
      newState = EM_PHY_DC_STATE_OFF;
    } else {
      // Signal the appropriate threshold/limit when there is no more room
      // for sending a maximum-sized packet.  This is particularly important
      // for SUSPENDED state because we never actually exceed the RDC, only
      // stop when we come too close to be able to send a frame of whatever
      // size.  Note this means entry to an increased state is conservatively
      // premature and it's still possible for smaller transmits to succeed
      // right up to the DC limit's edge.
      newState = dcStateForConsumed(dcCurChanDc->dcPeriodConsumedBytes
                                    + DC_ON_AIR_BYTES(SL_ZIGBEE_PHY_MAX_PAYLOAD_BYTES));
    }
  }
  if (dcCurState != newState) {
    EmPhyDcMode newMode;
    dcCallbackActive = true;
    newMode = sli_802154phy_dc_state_callback((dcCurChanDc
                                               ? dcCurChanDc->macPgChanPlusOne - 1
                                               : sli_802154phy_get_radio_channel()),
                                              dcCurMode,
                                              dcCurState, newState);
    dcCallbackActive = false;
    dcCurState = newState;
    // Update the mode if callback has decided to change it
    if (newMode < EM_PHY_DC_MODE_UNCHANGED) {
      dcCurMode = newMode;
    }
  }
 #if     !LBT_ACKS_IN_REQUESTORS_DC
  // Determine if duty cycle is so tight we need to suspend sending ACKs
  // (or vice-versa, we can resume sending them)
  if (sli_802154phy_radio_auto_ack_enabled()) {
    bool suspendAcks = false;
    if (dcCurState != EM_PHY_DC_STATE_OFF) {
      int32_t consumed = dcCurChanDc->dcPeriodConsumedBytes
                         + DC_ON_AIR_BYTES_ACK;
      suspendAcks = (consumed > dcMaxAvailBytesForMode(dcCurMode));
    }
    if (suspendAcks != dcAcksSuspended) {
      extern void emRadioHoldOffInternal(bool active); //FIXME: phy.h?

      emRadioHoldOffInternal(suspendAcks);
      dcAcksSuspended = suspendAcks;
    }
  }
 #endif//!LBT_ACKS_IN_REQUESTORS_DC
}

// Update the Duty Cycle parameters for the bytes consumed.
// Returns the remaining RDC transmit budget for non-LBT or LBT as requested
static int32_t dcUpdateBytes(int32_t pktBytes, EmPhyDcMode pktMode)
{
  // Make local copy of volatile dcPktFailedOnAirBytes to avoid needing to disable
  // interrupts
  uint8_t pktFailedOnAirBytes = dcPktFailedOnAirBytes;
  if (pktFailedOnAirBytes > 0) {
    // It's safe to clear dcPktFailedBytes after it's been found set
    // because only one transmit can be active at a time.
    dcPktFailedOnAirBytes = 0;
  }
  if (dcCurChanDc == NULL) { // No duty cycle currently applies
    return DC_INFINITE_BYTES_SENTINAL;
  }
  uint64_t nowBucketStartMsTick = DC_BUCKET_START_TIME_TICK(nowMsTick64());
  uint32_t elapsedBuckets = elapsedTimeInt64u(dcCurChanDc->dcLastBucketStartMsTick,
                                              nowBucketStartMsTick)
                            / DC_BUCKET_TICKS;
  uint8_t bucketIndex = DC_BUCKET_INDEX(nowBucketStartMsTick);

  if (elapsedBuckets >= DC_NUM_BUCKETS) {
    // So much time has transpired that we can clear the entire bucket set
    dcCurChanDc->dcPeriodConsumedBytes = 0;
    memset(dcCurChanDc->dcBucketBytes, 0, sizeof(dcCurChanDc->dcBucketBytes));
  } else
  if (elapsedBuckets > 0) {
    bucketIndex = DC_BUCKET_INDEX(dcCurChanDc->dcLastBucketStartMsTick);
    do {
      if (++bucketIndex == DC_NUM_BUCKETS) {
        bucketIndex = 0;
      }
      dcCurChanDc->dcPeriodConsumedBytes -= dcCurChanDc->dcBucketBytes[bucketIndex];
      dcCurChanDc->dcBucketBytes[bucketIndex] = 0;
    } while (--elapsedBuckets > 0);
    assert(bucketIndex == DC_BUCKET_INDEX(nowBucketStartMsTick));
  }

  // Adjust pktBytes for PHR and on-air overhead
  if (pktBytes > 0) {
    pktBytes += DC_ON_AIR_BYTES(0);
  } else
  if (pktBytes < 0) {
    pktBytes -= DC_ON_AIR_BYTES(0);
  }
 #if     !LBT_ACKS_IN_REQUESTORS_DC
  // Account for any ACKs sent
  // This unfortunatly needs to have interupts disabled because ACKs could be sent
  // at any time.
  uint8_t acksSent; // Local copy so disabled interrupts region is very tight
  {
    DECLARE_INTERRUPT_STATE;
    DISABLE_INTERRUPTS();
    acksSent = dcAcksSent;
    dcAcksSent = 0;
    RESTORE_INTERRUPTS();
  }
  pktBytes += (acksSent * DC_ON_AIR_BYTES_ACK);
 #endif//!LBT_ACKS_IN_REQUESTORS_DC
  // Account for any failed packets
  pktBytes -= pktFailedOnAirBytes;
  dcCurChanDc->dcPeriodConsumedBytes += pktBytes;
  dcCurChanDc->dcBucketBytes[bucketIndex] += pktBytes;
  dcCurChanDc->dcLastBucketStartMsTick = nowBucketStartMsTick;

  dcCheckStateUpdateMode();

  if (pktMode == EM_PHY_DC_MODE_UNCHANGED) {
    pktMode = dcCurMode;
  }
  return dcMaxAvailBytesForMode(pktMode) - dcCurChanDc->dcPeriodConsumedBytes;
}

// Get the EmPhyDutyCycle{} entry for the current channel and if it's new
// (or force parameter is true), initialize it according to the PHY Duty
// Cycle parameters and the limits as specified/updated by higher layers.
static void dcUpdateEntry(bool force)
{
  uint8_t macPgChan = sli_802154phy_get_radio_channel();
  EmPhyDutyCycle* dcEntry = dcFindChannelDutyCycle(macPgChan);
  if (force || dcEntry->macPgChanPlusOne == 0) {
    // Need to initialize it afresh
    if (emPhyGetDutyCycleParams(macPgChan, &dcEntry->dcParams)
        == SL_STATUS_OK) {
      dcEntry->macPgChanPlusOne = macPgChan + 1;
      // Always ensure dcLimitsCfg.limiThresh is set to a real value
      if (dcLimitsCfg.limiThresh == EM_PHY_DC_LIMIT_USE_DEFAULT) {
        dcLimitsCfg.limiThresh = EM_PHY_DC_LIMITED_DEFAULT_HECTOPCT;
      }

      dcEntry->dcPeriodMaximumBytes
        = EM_PHY_DC_PERIOD_SECS            // sec
          * 1000000ul                      // us/sec
          / (dcEntry->dcParams.byteTimeUs * dcLimitAccel);  // us/byte

      if (dcLimitsCfg.suspLimit  == EM_PHY_DC_LIMIT_USE_DEFAULT) {
        if (dcEntry->dcParams.lbtRdcDenom == 0) {
          dcEntry->dcParams.lbtRdcDenom = 1; // 100%
        }
        dcEntry->dcPeriodLbtRdcBytes
          = (dcEntry->dcPeriodMaximumBytes / dcEntry->dcParams.lbtRdcDenom);
      } else {
        dcEntry->dcPeriodLbtRdcBytes
          = (dcEntry->dcPeriodMaximumBytes / EM_PHY_DC_LIMIT_UNLIMITED)
            * dcLimitsCfg.suspLimit;
      }

      if (dcLimitsCfg.critThresh == EM_PHY_DC_LIMIT_USE_DEFAULT) {
        if (dcEntry->dcParams.criRdcDenom == 0) {
          dcEntry->dcParams.criRdcDenom = 1; // 100%
        }
        dcEntry->dcPeriodCriticalBytes
          = (dcEntry->dcPeriodMaximumBytes / dcEntry->dcParams.criRdcDenom);
      } else {
        dcEntry->dcPeriodCriticalBytes
          = (dcEntry->dcPeriodMaximumBytes / EM_PHY_DC_LIMIT_UNLIMITED)
            * dcLimitsCfg.critThresh;
      }
      if (dcEntry->dcPeriodCriticalBytes > dcEntry->dcPeriodLbtRdcBytes) {
        dcEntry->dcPeriodCriticalBytes = dcEntry->dcPeriodLbtRdcBytes;
      }

      if (dcLimitsCfg.talkLimit  == EM_PHY_DC_LIMIT_USE_DEFAULT) {
        uint16_t nonRdcDenom = (uint16_t)dcEntry->dcParams.nonRdcDenom10 * 10;
        if (nonRdcDenom == 0) {
          nonRdcDenom = 1; // 100%
        }
        dcEntry->dcPeriodNonRdcBytes
          = (dcEntry->dcPeriodMaximumBytes / nonRdcDenom);
      } else {
        dcEntry->dcPeriodNonRdcBytes
          = (dcEntry->dcPeriodMaximumBytes / EM_PHY_DC_LIMIT_UNLIMITED)
            * dcLimitsCfg.talkLimit;
      }

      dcEntry->dcPeriodThresholdBytes
        = (dcEntry->dcPeriodMaximumBytes / EM_PHY_DC_LIMIT_UNLIMITED)
          * dcLimitsCfg.limiThresh;
      if (dcEntry->dcPeriodThresholdBytes > dcEntry->dcPeriodLbtRdcBytes) {
        dcEntry->dcPeriodThresholdBytes = dcEntry->dcPeriodLbtRdcBytes;
      }

      dcCurPktOvhBytes = dcEntry->dcParams.pktOvhBytes;
      dcCurChanDc = dcEntry;
      (void) dcUpdateBytes(0, EM_PHY_DC_MODE_OFF);  // Mode doesn't matter
    } else {
      // No DC to enforce; leave free entry untouched and free for next use
      dcCurPktOvhBytes = 0;
      dcCurChanDc = NULL;
      dcCheckStateUpdateMode();
    }
  } else
  if (dcCurChanDc != dcEntry) { // Channel changed to one we know about
    dcCurPktOvhBytes = dcEntry->dcParams.pktOvhBytes;
    dcCurChanDc = dcEntry;
    (void) dcUpdateBytes(0, EM_PHY_DC_MODE_OFF);  // Mode doesn't matter
  }
}

#define DC_EXCEEDED_LIMIT_SENTINAL INT32_MAX

// Check if packet can be sent, and if so, when.
// pktBytes is the PHY payload length (not including PHR length byte).
// pktMode is the mode (OFF/TALK/CSMA/LBT/UNCHANGED) to use for this packet.
// Returns a millisecond time delay for when the packet can go out --
// if negative or 0 it can go out immediately, otherwise must wait.
// Returns DC_EXCEEDED_LIMIT_SENTINAL for 'try again an indeterminate time
// later' if duty cycle limit has been exceeded.  (Routine doesn't
// calculate when the duty cycle budget will become available to send it.)
static int32_t dcCanSendPkt(uint8_t* packet, EmPhyDcMode pktMode)
{
  dcUpdateEntry(false); // Catch a potential channel change (mode could change)
  EmPhyDcMode modeToUse = pktMode;
  if (modeToUse == EM_PHY_DC_MODE_UNCHANGED) {
    modeToUse = dcCurMode;
  }
  if ((dcCurChanDc == NULL) || (modeToUse == EM_PHY_DC_MODE_OFF)) {
    return 0; // No enforcement - can send immediately
  }
  int32_t pktBytes = DC_ON_AIR_BYTES(packet[0]);
  // Enforce per-packet limit:
  if ((modeToUse > EM_PHY_DC_MODE_OFF)
      && (dcCurChanDc->dcParams.maxPktBytes > 0)
      && (pktBytes > dcCurChanDc->dcParams.maxPktBytes)) {
    return DC_EXCEEDED_LIMIT_SENTINAL; // try again later
  }
  //TODO: Enforce per-dialog limit:
  // Enforce duty cycle budget limit:
 #if     LBT_ACKS_IN_REQUESTORS_DC
  // ACKs are being accounted for in the requestor's duty-cycle
  // rather than the responder's.
  if (ackRequested(packet)) {
    pktBytes += DC_ON_AIR_BYTES_ACK;
  }
 #endif//LBT_ACKS_IN_REQUESTORS_DC
 #if 0 //TODO: NEEDED?
  // Assume packet will be sent with CSMA/LBT and PHY might want to ACK
  // a few packets that might come in during this period, so reserve some
  // budget for 2 ACKs.  This prevents us from having to check during ACK
  // processing whether the outgoing packet in CSMA/LBT needs to be aborted.
  pktBytes += 2 * DC_ON_AIR_BYTES_ACK;
 #endif
  // Here we pass original pktMode as passed in, not modeToUse
  if (dcUpdateBytes(0, pktMode) < pktBytes) {  // Budget would be exceeded!
    assert(pktMode > EM_PHY_DC_MODE_OFF); // Should only get here if enforcing!
    return DC_EXCEEDED_LIMIT_SENTINAL; // try again later
  }
  // If got here, we have budget to send; calculate when we could send it
  // due to the minimum post-previous-transmission off time, and pktMode.
  // Recalc modeToUse since dcCurMode might have changed
  if (pktMode == EM_PHY_DC_MODE_UNCHANGED) {
    modeToUse = dcCurMode;
  }
  if ((dcCurChanDc->dcParams.lbtMinOffTimeUs == 0)   // No limit
    #if     LBT_TX_OFF_APPLIES_TO_NON_LBT
      || (modeToUse == EM_PHY_DC_MODE_OFF)
    #else//!LBT_TX_OFF_APPLIES_TO_NON_LBT
      || (modeToUse < EM_PHY_DC_MODE_LBT)
    #endif//LBT_TX_OFF_APPLIES_TO_NON_LBT
      ) {
    return 0; // Can send immediately
  }
  // Begin delay calculation with the LBT minimum Tx off time, which we can
  // reduce by the LBT CCA time:
  int32_t delay = dcCurChanDc->dcParams.lbtMinOffTimeUs;
  if (modeToUse >= EM_PHY_DC_MODE_LBTX) {
    delay -= dcCurChanDc->dcParams.lbtCcaTimeUs;
  }
  // Convert that from microseconds to millisecond ticks, with rounding:
  delay = ((delay * MILLISECOND_TICKS_PER_SECOND)
           + (1000000 / 2))
          / 1000000; // 1000 us/ms * 1000 ms/sec
  // Now compare it to the time the last transmit finished.
  // This code assumes we successfully transmit at least once a year
  // (half 32-bit ticks) for the delta to be valid.
  // Due to millisecond granularity, the delta error could be (-1..+1)
  // so we add one such that the error is (0..2) ensuring we won't put
  // something on the air prematurely.
  delay = elapsedTimeInt32u(nowMsTick(),
                            emPhyGetLastTxCompleteTimeMs(!LBT_ACKS_RESTART_TX_OFF_TIME)
                            + delay + 1);
  return delay;
}

static void transmitComplete(sl_status_t status)
{
  extern volatile bool sli_legacy_mfglib_transmit_complete;
  // EMZIGBEE-9230?
  //   This function is only called on errors that thwart a transmit
  //   which likely got prepared.
  //   Call sli_802154phy_radio_ok_to_idle() to clean up PHY transmit state machine
  //   (and idle the radio if allowed).
  //   We used to be able to count on the MAC layer to do this but
  //   something changed there such that it no longer makes this
  //   call on such errors.
  sli_802154phy_radio_ok_to_idle();
  if (sli_legacy_mfglib_mode) {
    sli_legacy_mfglib_transmit_complete = true;
  } else {
    sli_802154phy_radio_transmit_complete_callback(status, 0, 0);
  }
}

#if     LBT_IN_SOFTWARE

// Some PHYs may not have implemented this
WEAK(sl_status_t emRadioPrepareTransmit(uint8_t* memptr))
{
  UNUSED_VAR(memptr);
  return SL_STATUS_NOT_SUPPORTED;
}

#ifdef  PHY_PRO2PLUS
#define EM_DC_SYMBOL_DELAY_CHANNEL EM_HAL_SYMBOL_DELAY_CHANNEL_B
#else//!PHY_PRO2PLUS
#define EM_DC_SYMBOL_DELAY_CHANNEL EM_HAL_SYMBOL_DELAY_CHANNEL_A
#endif//PHY_PRO2PLUS

#if     LBT_LISTEN_DURING_BACKOFF

static void lbtScheduleBackoffEnd(uint16_t symbols)
{
  // Skip BACKOFF state and go right into CCA state for the backoff+CCA time
  symbols += dcCurChanDc->dcParams.lbtParams->lbtCcaSym;
  lbtCcaEndTimeSymTick =
    halStackOrderSymbolDelay(EM_DC_SYMBOL_DELAY_CHANNEL, NULL,
                             SL_ZIGBEE_PHY_SYMBOLS_TO_US(symbols));
  lbtState = EM_PHY_LBT_STATE_CCA;

  sli_zigbee_event_set_active(&emPhyDcTxEvent);
}

#else//!LBT_LISTEN_DURING_BACKOFF

static void symbolDelayCb(EmHalSymbolDelayChannel_t delayChan)
{
  UNUSED_VAR(delayChan);
  sli_zigbee_event_set_active(&emPhyDcTxIsrEvent);
}

static void lbtScheduleBackoffEnd(uint16_t symbols)
{
  lbtState = EM_PHY_LBT_STATE_BACKOFF;
  (void) halStackOrderSymbolDelay(EM_DC_SYMBOL_DELAY_CHANNEL, &symbolDelayCb,
                                  SL_ZIGBEE_PHY_SYMBOLS_TO_US(symbols));
}

#endif//LBT_LISTEN_DURING_BACKOFF

static uint16_t lbtGetRandomBackoffSym(const EmPhySubBandParams* lbtParams)
{
  extern bool emRadioAlwaysUseZeroBackoff;
  if (emRadioAlwaysUseZeroBackoff) {
    return 0;
  }
  uint16_t exp = lbtParams->lbtBackoffMinExp + lbtTry;
  if (exp >= lbtParams->lbtBackoffMaxExp) {
    exp = lbtParams->lbtBackoffMaxExp;
  }
  lastBackoff = halCommonGetRandom()
                % ((1u << exp) + lbtParams->lbtBackoffAdjust);
  lastBackoff += lbtParams->lbtMinBackoffs;

  return lastBackoff * lbtParams->lbtBackoffSym;
}

static void lbtFail(EmPhyDcExtendedStatus dcStatus)
{
  DC_DEBUG(DC_GPIO_OFF(); DC_GPIO_ON(); DC_GPIO_OFF()); // CCA_FAIL
  lbtState = EM_PHY_LBT_STATE_IDLE;
  emPhyDcSetExtendedStatus(dcStatus);
  issuePtaStackEvent(SL_RAIL_UTIL_IEEE802154_STACK_EVENT_TX_BLOCKED,
                     (uint32_t) ackRequested(dcTxPacket));
  transmitComplete(SL_STATUS_CCA_FAILURE);
}

static sl_status_t lbtTransmit(bool callbackOnError)
{
  assert(lbtState == EM_PHY_LBT_STATE_IDLE);
  assert(dcCurChanDc != NULL);
  const EmPhySubBandParams* lbtParams = dcCurChanDc->dcParams.lbtParams;
  assert(lbtParams != NULL);

  // Prepare for the transmit prior to starting LBT so when LBT succeeds,
  // minimal time is needed to actually get the packet on the air.
  // This function can return SL_STATUS_NOT_SUPPORTED if it's not implemented.
  sl_status_t status = emRadioPrepareTransmit(dcTxPacket);
  if ((status != SL_STATUS_OK) && (status != SL_STATUS_NOT_SUPPORTED)) {
    DC_DEBUG(DC_GPIO_ON(); DC_GPIO_OFF()); // TX_BUSY
    emPhyDcSetExtendedStatus(EM_PHY_DC_EXT_STATUS_PHY_BUSY);
    if (callbackOnError) {
      transmitComplete(status);
    }
    return status;
  }

  // Ensure radio is on suitably for doing CCA checks.
  // We do this here before LBT backoff because on some radios it
  // can take too long to wake the radio in between backoff and CCA.
  // sli_802154phy_radio_transmit_complete_callback() ensures radio returns to its
  // proper Idle mode.
  DC_DEBUG(DC_GPIO_OFF());
  // Normally this would be where SL_RAIL_UTIL_IEEE802154_EVENT_CCA_SOON would be posted,
  // but for LBT, since radio is on for long time, defer posting this
  // until the CCA actually starts.
  if (status == SL_STATUS_OK) { // Prepare above worked, radio already on:
    (void) emRadioGetCca();      // Prime it for doing CCA.
  } else {                       // Prepare above failed - not supported:
    sli_802154phy_radio_wake_up();             // Turn the radio on.
  }
  DC_DEBUG(DC_GPIO_ON());

  lbtOverallEndTimeSymTick =
    halStackOrderSymbolDelay(EM_DC_SYMBOL_DELAY_CHANNEL, NULL,
                             SL_ZIGBEE_PHY_SYMBOLS_TO_US(lbtParams->lbtTimeoutSym));
  lbtTry = 0;
  // Don't init extended status; only update it on errors
  //emPhyDcSetExtendedStatus(EM_PHY_DC_EXT_STATUS_SUCCESS);
  assert(lbtParams->lbtTries != 0);
  if (lbtParams->lbtTries < 0) { // Sentinal for no backoff on 1st try
    lbtTries = -lbtParams->lbtTries;
    lbtBackoffTimeSym = 0;
  } else {
    lbtTries = lbtParams->lbtTries;
    lbtBackoffTimeSym = lbtGetRandomBackoffSym(lbtParams);
  }
  PTI_TRACE(PTI_LBTSTARTSW);
  lbtScheduleBackoffEnd(lbtBackoffTimeSym); // updates lbtState
  return SL_STATUS_OK;
}

#endif//LBT_IN_SOFTWARE

static sl_status_t dcTransmit(bool callbackOnError)
{
  DC_DEBUG(DC_GPIO_OFF()); // LBT SUCCESS
  sl_status_t status = emRadioTransmit(dcTxPacket);
  if (status == SL_STATUS_OK) {
    emPhyDcSetExtendedStatus(EM_PHY_DC_EXT_STATUS_SUCCESS);
    // Assume the in-progress packet will go out
    (void) dcUpdateBytes(dcTxPacket[0], EM_PHY_DC_MODE_OFF);  // Mode doesn't matter
   #if     LBT_ACKS_IN_REQUESTORS_DC
    // Account for the requested ACK too
    if (ackRequested(dcTxPacket)) {
      (void) dcUpdateBytes(ACK_PKT_LENGTH, EM_PHY_DC_MODE_OFF);  // Mode doesn't matter
    }
   #endif//LBT_ACKS_IN_REQUESTORS_DC
  } else {
    DC_DEBUG(DC_GPIO_ON(); DC_GPIO_OFF()); // TX_BUSY
    emPhyDcSetExtendedStatus(EM_PHY_DC_EXT_STATUS_PHY_BUSY);
    if (callbackOnError) {
      // emRadioTransmit() already issued SL_RAIL_UTIL_IEEE802154_EVENT_TX_BLOCKED
      transmitComplete(status);
    }
  }
  return status;
}

//-- Public API routines

// Extended Status
static EmPhyDcExtendedStatus emPhyDcExtendedStatus;

// Retrieve the extended status and grab LBT/CCA attempts
EmPhyDcExtendedStatus emPhyDcGetExtendedStatus(void)
{
  return emPhyDcExtendedStatus;
}

// Update the extended status and grab LBT/CCA attempts
void emPhyDcSetExtendedStatus(EmPhyDcExtendedStatus dcStatus)
{
  emPhyDcExtendedStatus = (dcStatus & EM_PHY_DC_EXT_STATUS_MASK)
                          | (ccaAttempt << EM_PHY_DC_EXT_LBT_TRIES_SHIFT);
}

// Set a new Duty Cycle Mode
sl_status_t emPhyDcSetMode(EmPhyDcMode dcMode)
{
  if (dcMode >= EM_PHY_DC_MODE_ITEMS) { // Includes "UNCHANGED"
    return SL_STATUS_INVALID_PARAMETER;
  }
  DC_DEBUG(DC_GPIO_INIT());
  dcCurMode = dcMode;
  dcUpdateEntry(false);
  return SL_STATUS_OK;
}

// Get current Duty Cycle Mode
EmPhyDcMode emPhyDcGetMode(void)
{
  return dcCurMode;
}

// Get current in-transit packet's Duty Cycle Mode
EmPhyDcMode emPhyDcGetPktMode(void)
{
  return dcTxMode;
}

// Get current Duty Cycle State
EmPhyDcState emPhyDcGetState(void)
{
  return dcCurState;
}

// Get current Duty Cycle State
EmPhyDcState emPhyDcGetActualState(void)
{
  // dcCurState is the conservative anticipatory state reflecting
  // the ability to send a full-size packet.  This routine returns
  // the current *actual* state.  As such, it should never return
  // SUSPENDED unless that was entered while DC enforcement was
  // off and it's since been turned on.
  if ((dcCurState == EM_PHY_DC_STATE_OFF)
      || (dcCurChanDc == NULL)) {   // No DC enforced or monitored
    return EM_PHY_DC_STATE_OFF;
  }
  return dcStateForConsumed(dcCurChanDc->dcPeriodConsumedBytes);
}

// Obtain the current duty cycle, and optionally the current set of limits.
// All units returned are "HectoPct" or Percent*100.
EmPhyDcHectoPct emPhyDcGetDcAndLimits(EmPhyDcLimits* limitsPtr)
{
  dcUpdateEntry(false); // Catch a potential channel change (mode could change)
  EmPhyDutyCycle* dcEntry = dcCurChanDc;
  if (dcEntry == NULL) {
    if (limitsPtr != NULL) {
      limitsPtr->talkLimit  = EM_PHY_DC_LIMIT_UNLIMITED;
      limitsPtr->limiThresh = EM_PHY_DC_LIMIT_UNLIMITED;
      limitsPtr->critThresh = EM_PHY_DC_LIMIT_UNLIMITED;
      limitsPtr->suspLimit  = EM_PHY_DC_LIMIT_UNLIMITED;
    }
    return 0; // 0% used of infinite limit
  }

  // Ensure we're up-to-date -- but prevent recursion if called from callback!
  if (!dcCallbackActive) {
    (void) dcUpdateBytes(0, EM_PHY_DC_MODE_OFF);  // Mode doesn't matter
  }

  if (limitsPtr != NULL) {
    limitsPtr->talkLimit  = ((((int64_t)dcEntry->dcPeriodNonRdcBytes
                               * EM_PHY_DC_LIMIT_UNLIMITED)
                              + (dcEntry->dcPeriodMaximumBytes / 2))    // Round up
                             / dcEntry->dcPeriodMaximumBytes);
    limitsPtr->limiThresh = ((((int64_t)dcEntry->dcPeriodThresholdBytes
                               * EM_PHY_DC_LIMIT_UNLIMITED)
                              + (dcEntry->dcPeriodMaximumBytes / 2))    // Round up
                             / dcEntry->dcPeriodMaximumBytes);
    limitsPtr->critThresh = ((((int64_t)dcEntry->dcPeriodCriticalBytes
                               * EM_PHY_DC_LIMIT_UNLIMITED)
                              + (dcEntry->dcPeriodMaximumBytes / 2))    // Round up
                             / dcEntry->dcPeriodMaximumBytes);
    limitsPtr->suspLimit  = ((((int64_t)dcEntry->dcPeriodLbtRdcBytes
                               * EM_PHY_DC_LIMIT_UNLIMITED)
                              + (dcEntry->dcPeriodMaximumBytes / 2))    // Round up
                             / dcEntry->dcPeriodMaximumBytes);
  }
  return ((((int64_t)dcEntry->dcPeriodConsumedBytes
            * EM_PHY_DC_LIMIT_UNLIMITED)
           + (dcEntry->dcPeriodMaximumBytes / 2))    // Round up
          / dcEntry->dcPeriodMaximumBytes);
}

// Return pointer to our copy of the current DC Limit configuration
const EmPhyDcLimits* emPhyDcGetLimitsConfig(void)
{
  return &dcLimitsCfg;
}

sl_status_t emPhyDcSetLimitsConfig(const EmPhyDcLimits* limitsConfig)
{
  if (limitsConfig == NULL) {
    return SL_STATUS_INVALID_PARAMETER;
  }
  EmPhyDcLimits checkCfg = *limitsConfig; // Struct copy
  // Fill in limiThresh with real value right here; it's not PHY-dependent
  if (checkCfg.limiThresh == EM_PHY_DC_LIMIT_USE_DEFAULT) {
    checkCfg.limiThresh = EM_PHY_DC_LIMITED_DEFAULT_HECTOPCT;
  }
  // Ensure non-USE_DEFAULT entries are in sorted order low-to-high
  // This code knows EmPhyDcLimits{} is essentially an array of
  // EmPhyDcHectoPct entries.
  EmPhyDcHectoPct prevLimit = 0;
  uint8_t i;
  for (i = 0; i < (sizeof(EmPhyDcLimits) / sizeof(EmPhyDcHectoPct)); i++) {
    EmPhyDcHectoPct thisLimit = ((EmPhyDcHectoPct*) &checkCfg)[i];
    if (thisLimit != EM_PHY_DC_LIMIT_USE_DEFAULT) {
      if ((thisLimit > EM_PHY_DC_LIMIT_UNLIMITED)
          || (thisLimit < prevLimit)) {
        return SL_STATUS_INVALID_PARAMETER;
      }
      prevLimit = thisLimit;
    }
  }
  dcLimitsCfg = checkCfg; // Struct copy of validated config
  dcUpdateEntry(true);
  return SL_STATUS_OK;
}

static sl_status_t dcTryPktNow(bool callbackOnError)
{
  int32_t txDelayMs = dcCanSendPkt(dcTxPacket, dcTxMode);
  if (txDelayMs > 0) {
    if ((txDelayMs == DC_EXCEEDED_LIMIT_SENTINAL)
        || timeGTorEqualInt32u(nowMsTick() + txDelayMs, dcTxLimitMs)) {
      //TODO: TX_BUSY is the closest thing we have to duty-cycle-exceeded
      DC_DEBUG(DC_GPIO_OFF(); DC_GPIO_ON(); DC_GPIO_OFF()); // TX_BUSY
      emPhyDcSetExtendedStatus((txDelayMs == DC_EXCEEDED_LIMIT_SENTINAL)
                               ? EM_PHY_DC_EXT_STATUS_DC_EXCEEDED
                               : EM_PHY_DC_EXT_STATUS_TX_TIME_LIMIT);
      if (callbackOnError) {
        // For duty-cycle exceeded, tell PTA packet was not ACK-requesting
        issuePtaStackEvent(SL_RAIL_UTIL_IEEE802154_STACK_EVENT_TX_BLOCKED, 0U);
        transmitComplete(SL_STATUS_TRANSMIT_BUSY);
      }
      return SL_STATUS_TRANSMIT_BUSY;
    }
    // Need to wait minimum post-previous-transmission off time.
    // Schedule an event then and we'll re-check at that time.
   #if     LBT_IN_SOFTWARE
    assert(lbtState == EM_PHY_LBT_STATE_IDLE);
   #endif//LBT_IN_SOFTWARE
    DC_DEBUG(DC_GPIO_OFF()); // Off for IDLE begin
    sli_zigbee_event_set_delay_ms(&emPhyDcTxEvent, txDelayMs);

    return SL_STATUS_OK;
  }
  // Solidify the Tx Mode now so the PHY driver can reference it
  if (dcTxMode == EM_PHY_DC_MODE_UNCHANGED) {
    dcTxMode = dcCurMode;
    // If DC Mode is OFF, we've not enforced any DC limit to get here,
    // but if we're supposed to checkCca, then respect that by using
    // LBTX mode or its CSMA fallback for the transmit.
    if ((dcTxMode == EM_PHY_DC_MODE_OFF)
        && (radioTransmitConfig.checkCca)) {
      dcTxMode = EM_PHY_DC_MODE_LBTX;
    }
  }
 #if     LBT_IN_SOFTWARE
  assert(lbtState == EM_PHY_LBT_STATE_IDLE);
  if (dcTxMode >= EM_PHY_DC_MODE_LBTX) {
    if ((dcCurChanDc != NULL) && (dcCurChanDc->dcParams.lbtParams != NULL)) {
      // We have LBT parameters -- but they also need to be for LBT operation
      if ((dcCurChanDc->dcParams.lbtFlags & EM_PHY_LBT_FLAG_MODE_MASK)
          == EM_PHY_LBT_FLAG_MODE_LBT) {
        dcTxMode = EM_PHY_DC_MODE_TALK; // Transmit directly when LBT succeeds!
        return lbtTransmit(callbackOnError);
      } else {
        // Leave dcTxMode as-is so PHY layer uses proper sub-band CSMA-mode
        // lbtParams.  Fall into calling dcTransmit() below.
      }
    } else {
      // Told to do LBT but the channel doesn't support it: do CSMA instead
      dcTxMode = EM_PHY_DC_MODE_CSMA;
    }
  }
 #endif//LBT_IN_SOFTWARE
  return dcTransmit(callbackOnError);
}

// Schedule packet to be sent as soon as practical.
sl_status_t sli_802154phy_dc_send_pkt_asap(uint8_t* packet,
                                           uint32_t limitMs,
                                           bool callbackOnErrorStatus)
{
  issuePtaStackEvent(SL_RAIL_UTIL_IEEE802154_STACK_EVENT_TX_PENDED_PHY,
                     (uint32_t)radioTransmitConfig.checkCca);
  // Only one Tx packet in flight at a time, so use statics for event handling
  dcTxLimitMs = nowMsTick() + limitMs; // convert to absolute time
  dcTxPacket  = packet;
  dcTxMode    = (radioTransmitConfig.checkCca
                 || (dcCurMode == EM_PHY_DC_MODE_OFF))
                ? EM_PHY_DC_MODE_UNCHANGED
                : EM_PHY_DC_MODE_TALK; // Force TALK mode when not CCA checking
  DC_DEBUG(DC_GPIO_ON()); // LBT START
  // If we were called within ISR context, defer calling dcTryPktNow()
  // to main context via our event, so we don't risk dcUpdateBytes()
  // being called re-entrantly.  This can happen on MAC retransmits.
  if (IN_INTERRUPT_CONTEXT()) { // We're within an interrupt!
    sli_zigbee_event_set_active(&emPhyDcTxIsrEvent);
    return SL_STATUS_OK;
  }
  return dcTryPktNow(callbackOnErrorStatus);
}

// If can send the packet, do so, otherwise reschedule to when it can be sent.
void emPhyDcTxEventHandler(sli_zigbee_event_t *event)
{
  UNUSED_VAR(event);
  sli_zigbee_event_set_inactive(&emPhyDcTxEvent);
  sli_zigbee_event_set_inactive(&emPhyDcTxIsrEvent);

 #if     LBT_IN_SOFTWARE
  switch (lbtState) {
    case EM_PHY_LBT_STATE_IDLE:
      DC_DEBUG(DC_GPIO_ON()); // On for IDLE end
      (void) dcTryPktNow(true); // Always issue callback from within handler
      break;
    case EM_PHY_LBT_STATE_BACKOFF:
      DC_DEBUG(DC_GPIO_OFF(); DC_GPIO_ON()); // Toggle on BACKOFF end
#ifdef SL_ZIGBEE_TEST
      // Simulation does not have support for halStackGetInt32uSymbolTick hence we need to
      // to use halCommonGetInt32uMillisecondTick instead and change calculation accordingly
      lbtCcaEndTimeSymTick = halCommonGetInt32uMillisecondTick()
                             + ((SL_ZIGBEE_PHY_SYMBOLS_TO_US(dcCurChanDc->dcParams.lbtParams->lbtCcaSym)) / 1000);
#else
      lbtCcaEndTimeSymTick =
        halStackOrderSymbolDelay(EM_DC_SYMBOL_DELAY_CHANNEL, NULL,
                                 SL_ZIGBEE_PHY_SYMBOLS_TO_US(dcCurChanDc->dcParams.lbtParams->lbtCcaSym));
#endif
      lbtState = EM_PHY_LBT_STATE_CCA;
      PTI_TRACE(PTI_LBTCCASTART);
      issuePtaStackEvent(SL_RAIL_UTIL_IEEE802154_STACK_EVENT_TX_CCA_SOON, 0U);
    // Fall into new state rather than waiting for next tick...
    // Fall through
    case EM_PHY_LBT_STATE_CCA:
      if (emRadioGetCca() != CCA_CLEAR) {
        if (++lbtTry < lbtTries) {
          DC_DEBUG(DC_GPIO_OFF(); DC_GPIO_ON()); // Toggle on BUSY retry
          DC_DEBUG(DC_GPIO_OFF(); DC_GPIO_ON()); // twice
          lbtState = EM_PHY_LBT_STATE_BUSY;
          PTI_TRACE(PTI_LBTRETRY);
          issuePtaStackEvent(SL_RAIL_UTIL_IEEE802154_STACK_EVENT_TX_CCA_BUSY,
                             (uint32_t)LBT_LISTEN_DURING_BACKOFF);
          sli_zigbee_event_set_active(&emPhyDcTxEvent);
        } else { // No more tries -- fail the Tx
          DC_DEBUG(DC_GPIO_OFF(); DC_GPIO_ON()); // Toggle on BUSY retry
          PTI_TRACE(PTI_LBTFAILURE);
          lbtFail(EM_PHY_DC_EXT_STATUS_LBT_TRIES_EXCEEDED);
        }
      } else { // CCA_CLEAR!
#ifdef SL_ZIGBEE_TEST
        if (halStackInt32uSymbolTickGTorEqual(halCommonGetInt32uMillisecondTick(),
                                              lbtCcaEndTimeSymTick)) {
#else
        if (halStackInt32uSymbolTickGTorEqual(halStackGetInt32uSymbolTick(),
                                              lbtCcaEndTimeSymTick)) {
#endif
          // We can claim the channel and transmit!
          lbtState = EM_PHY_LBT_STATE_IDLE;
          PTI_TRACE(PTI_LBTSUCCESS);
          (void) dcTransmit(true); // Will issue SL_RAIL_UTIL_IEEE802154_EVENT_TX_STARTED
        } else {
          // stay in EM_PHY_LBT_STATE_CCA
          sli_zigbee_event_set_active(&emPhyDcTxEvent);
        }
      }
      break;
    case EM_PHY_LBT_STATE_BUSY:
      if (emRadioGetCca() != CCA_CLEAR) {
        if (halStackInt32uSymbolTickGTorEqual(halStackGetInt32uSymbolTick(),
                                              lbtOverallEndTimeSymTick)) {
          PTI_TRACE(PTI_LBTTIMEOUT);
          lbtFail(EM_PHY_DC_EXT_STATUS_LBT_TIMEDOUT);
        } else {
          // lbtState = EM_PHY_LBT_STATE_BUSY; // Stay in this state
          sli_zigbee_event_set_active(&emPhyDcTxEvent);
        }
      } else { // CCA_CLEAR!
        DC_DEBUG(DC_GPIO_OFF(); DC_GPIO_ON()); // Toggle on CLEAR retry
        lbtBackoffTimeSym = lbtGetRandomBackoffSym(dcCurChanDc->dcParams.lbtParams);
        uint32_t lbtNeededEndTimeSymTick =
          halStackOrderSymbolDelay(EM_DC_SYMBOL_DELAY_CHANNEL, NULL,
                                   SL_ZIGBEE_PHY_SYMBOLS_TO_US(lbtBackoffTimeSym
                                                               + dcCurChanDc->dcParams.lbtParams->lbtCcaSym));
        if (halStackInt32uSymbolTickGTorEqual(lbtNeededEndTimeSymTick,
                                              lbtOverallEndTimeSymTick)) {
          // Cannot possibly succeed given the time left, so fail
          PTI_TRACE(PTI_LBTTIMEOUT);
          lbtFail(EM_PHY_DC_EXT_STATUS_LBT_TIMEOUT);
        } else {
          PTI_TRACE(PTI_LBTCCACLEAR);
          // There is no PTA event for this LBT notion
          lbtScheduleBackoffEnd(lbtBackoffTimeSym); // updates lbtState
        }
      }
      break;
    default:
      assert(0);
      break;
  }
 #else//!LBT_IN_SOFTWARE
  (void) dcTryPktNow(true); // Always issue callback from within handler
 #endif//LBT_IN_SOFTWARE
}

// Credit duty cycle for packet that failed to send -- called from ISR context
void sli_802154phy_dc_send_pkt_failed(void)
{
  // Assume called from ISR context -- defer to mainline code any such updates
  uint8_t pktFailedOnAirBytes = DC_ON_AIR_BYTES(dcTxPacket[0]);
 #if     LBT_ACKS_IN_REQUESTORS_DC
  // Account for the requested ACK that's no longer gonna be seen because
  // the requesting packet never went out
  if (ackRequested(dcTxPacket)) {
    pktFailedOnAirBytes += DC_ON_AIR_BYTES_ACK;
  }
 #endif//LBT_ACKS_IN_REQUESTORS_DC
  dcPktFailedOnAirBytes = pktFailedOnAirBytes;
}

// Debit duty cycle for an ACK that went out -- called from ISR context
void emPhyDcAckSent(void)
{
 #if     LBT_ACKS_IN_REQUESTORS_DC
  // ACK already accounted for by requestor
 #else//!LBT_ACKS_IN_REQUESTORS_DC
  dcAcksSent++;
 #endif//LBT_ACKS_IN_REQUESTORS_DC
}

// Tick routine ensures DC is updated as time passes -- even a looong time.
void emPhyDcTick(void)
{
  // Expire buckets and notify DC state changes
  (void) dcUpdateBytes(0, EM_PHY_DC_MODE_OFF);  // Mode doesn't matter
}

//-- Private Test APIs

// Return the current duty cycle entry
EmPhyDutyCycle* emPhyDcEntry(void)
{
  return dcCurChanDc;
}

uint8_t emPhyDcBucketIndex(uint32_t timeMsTick)
{
  return DC_BUCKET_INDEX(timeMsTick);
}

// Set time acceleration factor
void sli_802154phy_dc_set_time_accel(uint16_t timeAccel)
{
  dcTimeAccel = (timeAccel & 0x00FF);
  dcLimitAccel = (timeAccel >> 8);
  if (dcLimitAccel == 0) { // Unspecified means LimitAccel same as TimeAccel
    dcLimitAccel = dcTimeAccel;
  }
  if (dcLimitAccel == 0) { // Minimum LimitAccel is 1.
    dcLimitAccel = 1;
  }
  // N.B. if dcTimeAccel == 0, time will not advance at all!

  // This triggers total reinit of internal data structures
  memset(dcDutyCycles, 0, sizeof(dcDutyCycles));
  dcCurPktOvhBytes = 0;
  dcCurChanDc = NULL;
  dcUpdateEntry(true);
}

// Get time acceleration factor
uint16_t emPhyDcGetTimeAccel(void)
{
  uint32_t accelFactors = dcTimeAccel;
  if (dcLimitAccel != dcTimeAccel) {
    accelFactors |= ((uint16_t)dcLimitAccel << 8);
  }
  return accelFactors;
}

//-- Public Callbacks

WEAK(EmPhyDcMode sli_802154phy_dc_state_callback(uint8_t macPgChan,
                                                 EmPhyDcMode curMode,
                                                 EmPhyDcState oldState,
                                                 EmPhyDcState newState))
{
  UNUSED_VAR(macPgChan);
  UNUSED_VAR(curMode);
  UNUSED_VAR(oldState);
#if LBT_POLICY_AVOID_UNTIL_NEEDED
  switch (newState) {
    case EM_PHY_DC_STATE_OFF:
    case EM_PHY_DC_STATE_SUSPENDED:
      return EM_PHY_DC_MODE_UNCHANGED;
    case EM_PHY_DC_STATE_NORMAL:
    case EM_PHY_DC_STATE_LIMITED_TALK:
      return EM_PHY_DC_MODE_TALK; // Or CSMA? Policy to NOT use LBT until needed!
    case EM_PHY_DC_STATE_REQUIRE_LBT:
    case EM_PHY_DC_STATE_LIMITED_LBT:
    case EM_PHY_DC_STATE_CRITICAL_NODC:
    case EM_PHY_DC_STATE_CRITICAL_LBT:
      return EM_PHY_DC_MODE_LBT; // ...and now we need to :-)
    default:
      assert(false);
      break;
  }
#else
  UNUSED_VAR(newState);
#endif//LBT_POLICY_AVOID_UNTIL_NEEDED
  return EM_PHY_DC_MODE_UNCHANGED;
}

#else//!PHY_HAS_DUTY_CYCLE

#include "duty-cycle-stub.c"

#endif//PHY_HAS_DUTY_CYCLE

#endif//(PHY_DUAL && (PHY_THIS != PHY_THIS_UNSPECIFIED))

#endif//(PHY_DUALRAIL && !PHY_THIS && !PHY_INCLUDE_SOURCE)
