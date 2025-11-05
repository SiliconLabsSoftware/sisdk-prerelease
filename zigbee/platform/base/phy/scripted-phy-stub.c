/***************************************************************************//**
 * @file
 * @brief stubs for writing test scripts that simulate the PHY.
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
#include <stdarg.h>
#include <stdlib.h>                     // for malloc()
#include <string.h>                     // for memcpy()
#include <ctype.h>                      // for isprint()

#include "core/sl_zigbee_stack.h"

#include "hal/hal.h"
#include "coexistence/protocol/ieee802154_uc/coexistence-802154.h"
#include "micro/unix/simulation/em250/regs.h"
#include "phy/phy.h"
#include "phy/symbol-timer.h"

#ifdef SL_ZIGBEE_STACK_IP
// IP Stack
  #include "framework/ip-packet-header.h"
  #include "framework/event-queue.h"
  #include "mac/mac-header.h"
  #include "mac/802.15.4/mac.h"
#else
// Pro Stack
  #include "framework/packet-header.h"
  #include "lower-mac.h"
  #include "mac/upper-mac.h"
  #include "multi-mac.h"
#endif // SL_ZIGBEE_STACK_IP

#include "indirect-queue.h"
#include "mac-child.h"

#include "stack/core/parcel.h"
#include "stack/core/scripted-stub.h"
#include "scripted-phy-stub.h"

// diasbled by default
uint8_t sli_legacy_mfglib_mode = 0;
uint8_t emGrantSignal = 1;
//----------------------------------------------------------------
// Forward declarations needed because two-pass compilers were too slow
// back in the early sixties.
static void checkSymbolDelayIsr(void);

//----------------------------------------------------------------
// Radio state

RadioPowerMode radioCurrentPowerStatus = SL_ZIGBEE_RADIO_POWER_MODE_RX_ON;

RadioPowerMode sli_802154phy_radio_get_power_status(void)
{
  return radioCurrentPowerStatus;
}

static uint16_t phyShortId = 0xffff;
static uint16_t phyPanId   = 0xffff;
static uint8_t phyChannel = 0x00;
static int8_t phyPower = -1;
static RadioPowerMode radioPowerOnIdleMode = SL_ZIGBEE_RADIO_POWER_MODE_RX_ON;

bool sli_802154mac_ack_needed;

uint8_t emRadioLastOutgoingCommand = 0xff;

// For pages other than 0, set the most-significant bits dropped in Pg encoding
// E.g. 3-bit Pgs 1-7 map to 5-bit Pages 24-31.
#define macPg2Page(macPg)        ( ((macPg) == 0) ? 0                                         \
                                   : ((macPg) | ((((1ul << (32 - MAX_CHANNELS_PER_PAGE)) - 1) \
                                                  >> (8 - CHANNEL_BITS)) << (8 - CHANNEL_BITS))) )

static bool addressMatchingEnabled = false;

//----------------------------------------------------------------
// Calls from phy/phy.h

#define simPrint(string, ...) fprintf(stderr, string "\n", ##__VA_ARGS__)

void sli_802154phy_radio_init(RadioPowerMode initialRadioPowerMode)
{
  simPrint("scriptedPhyStub:sli_802154phy_radio_init");
}

void sli_802154phy_radio_set_node_id(uint16_t nodeId)
{
  phyShortId = nodeId;
}

sl_802154_short_addr_t sli_802154mac_radio_get_node_id(void)
{
  return phyShortId;
}

sl_802154_short_addr_t sli_zigbee_get_radio_node_id(void)
{
  return phyShortId;
}

void sli_802154phy_radio_set_pan_id(uint16_t panId)
{
  phyPanId = panId;
}

sl_802154_pan_id_t sli_802154phy_radio_get_pan_id(void)
{
  return phyPanId;
}

sl_802154_pan_id_t sli_zigbee_get_radio_pan_id(void)
{
  return phyPanId;
}

void sli_802154phy_radio_sleep(void)
{
}

void sli_802154phy_radio_wake_up(void)
{
  radioCurrentPowerStatus = 0;
}

uint8_t sli_802154phy_get_phy_radio_channel(void)
{
  return phyChannel;
}

int8_t sli_802154phy_get_phy_radio_power(void)
{
  return phyPower;
}

uint16_t emGetTxPowerMode(void)
{
  return 0;
}

uint8_t emRadioGetCca(void)
{
  // todo...need to write up a cca algorithm as part of the mac tests.
  // return CCA_BUSY
  return CCA_CLEAR;
}

uint8_t emReadNoiseLevel(uint8_t count)
{
  assert("emReadNoiseLevel() has no stub" == NULL);
  return 0;
}

void radioUpdateNoiseAverage(void)
{
}

bool sli_802154phy_radio_get_random_numbers(uint16_t *rngVal, uint8_t count)
{
  assert("sli_802154phy_radio_get_random_numbers() has no stub" == NULL);
  return false;
}

void emRadioEnableAddressMatching(uint8_t enable)
{
  addressMatchingEnabled = (bool) enable;
}

uint8_t emRadioAddressMatchingEnabled(void)
{
  return addressMatchingEnabled;
}

void sli_802154mac_radio_enable_auto_ack(bool enable)
{
}

bool sli_802154phy_radio_auto_ack_enabled(void)
{
  return false;
}

bool sli_802154phy_radio_check_radio(void)
{
  return false;
}

void emPhyTick(bool isrContext)
{
}

#ifndef MAC_TEST_STACK
uint8_t sli_802154phy_get_channel_page_in_use(void)
{
  return macPg2Page(sli_802154mac_pg_chan_pg(sli_802154phy_get_phy_radio_channel()));;
}
#endif

//----------------------------------------------------------------
// Calls and globals related to security and neighbor-table frame counter
// lookup.

uint32_t sli_zigbee_security_increment_outgoing_frame_counter(void)
{
  // We don't expect this to be called and will need a more intelligent
  // implementation if it is.
  assert(0);
  return 0;
}

void sli_zigbee_security_init(void)
{
  // We don't expect this to be called and will need a more intelligent
  // implementation if it is.
  assert(0);
}

void emSecuritySetNonce(uint16_t nonceIndex,
                        sl_802154_long_addr_t srcEui64,
                        uint32_t frameCounter,
                        uint8_t keySequenceNumber)
{
  // We don't expect this to be called and will need a more intelligent
  // implementation if it is.
  assert(0);
}

bool sli_zigbee_check_incoming_frame_counter(uint8_t neighborIndex, uint32_t frameCounter)
{
  // We don't expect this to be called and will need a more intelligent
  // implementation if it is.
  assert(0);
  return false;
}

void sli_zigbee_note_incoming_frame_counter(uint8_t neighborIndex, uint32_t frameCounter)
{
  // We don't expect this to be called and will need a more intelligent
  // implementation if it is.
  assert(0);
}

bool sli_zigbee_security_eui64(uint8_t neighborIndex, sl_802154_long_addr_t returnEui)
{
  // We don't expect this to be called and will need a more intelligent
  // implementation if it is.
  assert(0);
  return false;
}

//uint8_t *emGetEncryptionKey(uint8_t neighborIndex)
//{
// We don't expect this to be called and will need a more intelligent
// implementation if it is.
//  assert(0);
//  return NULL;
//}

uint8_t sli_zigbee_frame_counter_indexFromEui64(sl_802154_long_addr_t incomingEui64)
{
  // We don't expect this to be called and will need a more intelligent
  // implementation if it is.
  assert(0);
  return 0xFF;
}

uint8_t sli_zigbee_frame_counter_index(sl_802154_short_addr_t incomingNodeId)
{
  // We don't expect this to be called and will need a more intelligent
  // implementation if it is.
  assert(0);
  return 0xFF;
}

//----------------------------------------------------------------
// Radio input.

#ifndef MAC_TEST_STACK
static void radioInActionPrinter(Action *action)
{
  sli_parcel_t *message = (sli_parcel_t *) action->contents[0];
  EmberStatus status = action->contents[1];
  int i;

  fprintf(stderr, "status 0x%X ", status);

  for (i = 0; i < message->length; i++) {
    uint8_t next = message->contents[i];
    fprintf(stderr, "%s%s%X",
            i == 0 ? "[" : " ",
            next < 16 ? "0" : "",
            next);
  }
  fprintf(stderr, "]");
}
#endif

#if (!defined(MAC_TEST_STACK))
#if (!defined(SL_ZIGBEE_STACK_IP))
// Pro Stack
static void radioInActionPerformer(Action *action)
{
  sli_parcel_t *message = (sli_parcel_t *) action->contents[0];
  EmberStatus wantStatus = action->contents[1];
  EmberStatus gotStatus;
  sli_zigbee_packet_header_t rxPayload = SL_ZIGBEE_NULL_MESSAGE_BUFFER;

  // tbd:  possibly check the header length return here?
  sli_mac_flat_mac_header_length(message->contents, true);

  gotStatus = sli_mac_radio_receive_mac_header_callback(0, message->contents, message->length, &rxPayload);
  if (gotStatus != wantStatus) {
    fprintf(stderr, "[%u: sli_mac_radio_receive_mac_header_callback() returned 0x%X and not 0x%X as expected",
            scriptTime(),
            gotStatus,
            wantStatus);
    printActionCodePoint(action);
    fprintf(stderr, "]\n");
    assert(false);
  } else if (gotStatus == SL_ZIGBEE_SUCCESS) {
    sl_legacy_buffer_manager_release_message_buffer(rxPayload);
    rxPayload = SL_ZIGBEE_NULL_MESSAGE_BUFFER;
  }
}

#else
// IP Stack
static void radioInActionPerformer(Action *action)
{
  sli_parcel_t *message = (sli_parcel_t *) action->contents[0];
  EmberStatus wantStatus = action->contents[1];
  EmberStatus gotStatus;
  // The incoming buffer has the length byte at the front and the
  // CRC and appended info at the back.  The length byte includes
  // the CRC but not the appended info.
  sli_zigbee_packet_header_t rxPayload =
    parcelToBuffer(appendParcels(makeMessage("1", message->length + 2),
                                 message,
                                 makeMessage("<2<4", 0, 0),
                                 NULL));

  // tbd:  possibly check the header length return here?
  sli_mac_flat_mac_header_length(message->contents, true);

  gotStatus = emRadioReceiveMacHeaderCallback(rxPayload);
  if (gotStatus != wantStatus) {
    fprintf(stderr, "[%u: emRadioReceiveMacHeaderCallback() returned 0x%X and not 0x%X as expected",
            scriptTime(),
            gotStatus,
            wantStatus);
    printActionCodePoint(action);
    fprintf(stderr, "]\n");
    assert(false);
  }
}

#endif // !SL_ZIGBEE_STACK_IP

ActionType radioInActionType =
{
  "radio in",
  "pi",                 // packet, status
  radioInActionPrinter,
  radioInActionPerformer
};
#endif // !MAC_TEST_STACK

//----------------------------------------------------------------
// Radio output

// ----- emRadioTransmit and check.

// Checks for outgoing radio messages.  There are two fields:
//   - packet   (sli_parcel_t *)

static void radioOutCheckPrinter(Action *action)
{
  sli_parcel_t *packet = (sli_parcel_t *) action->contents[1];
  int i;

  fprintf(stderr, " checkCca:%ld ", action->contents[0]);

  for (i = 0; i < packet->length; i++) {
    uint8_t next = packet->contents[i];
    fprintf(stderr, "%s%s%X",
            i == 0 ? "[" : " ",
            next < 16 ? "0" : "",
            next);
  }
  fprintf(stderr, "]");
}

ActionType radioOutCheckType =
{
  "radio out",
  "ip",                 // packet use-cca
  radioOutCheckPrinter,
  NULL
};

extern RadioTransmitConfig radioTransmitConfig;

static EmberStatus radioTransmitReturnStatus = SL_ZIGBEE_SUCCESS;

void setRadioTransmitReturnStatus(EmberStatus status)
{
  radioTransmitReturnStatus = status;
}

//The new mac code post gb868 in Znet expects 2 more bytes here so we use this
//define to modify the call.
#ifdef SL_ZIGBEE_STACK_IP
  #define ADDED_PHY_BYTES 0
#else
  #define ADDED_PHY_BYTES 2
#endif

sl_status_t emRadioTransmit(uint8_t* memptr)
{
  if (radioTransmitReturnStatus == SL_ZIGBEE_SUCCESS) {
    // Unit tests do not test CRC functionality, so it is not included in the
    //   test packet.
    scriptCheck("emRadioTransmit",
                &radioOutCheckType,
                radioTransmitConfig.checkCca,
                makeMessage("s", memptr + 1, memptr[0] - PHY_APPENDED_CRC_SIZE + ADDED_PHY_BYTES));

    // Expect an Ack if the 15.4 Ack request bit is set.
    sli_802154mac_ack_needed = (memptr[1] & 0x20) ? true : false;

    if (!sli_802154mac_ack_needed) {
      sli_802154phy_radio_transmit_complete_callback(SL_STATUS_OK,
                                                     0,
                                                     false);
    }
  }

  return radioTransmitReturnStatus;
}

static void radioAckOutCheckPrinter(Action *action)
{
  uint8_t framePending = (uint8_t) action->contents[0];

  fprintf(stderr, "framePending %d",
          framePending);
}

ActionType radioAckOutCheckType =
{
  "radio ack out",
  "i",              // framePending
  radioAckOutCheckPrinter,
  NULL
};

//----------------------------------------------------------------
// Calls from phy/phy.h

int8_t sli_802154phy_radio_energy_detection(void)
{
  return (100 + phyChannel);
}

void emRadioSetCCAThresh(uint8_t CCAThresh)
{
}

void emRadioSetCCAMode(uint8_t CCAMode)
{
}

void sli_802154phy_set_coordinator(bool amCoord)
{
  // todo...need to allow/disallow reception of messages without a destination
  // address depending on whether we are the pan coordinator.
}
void sli_802154phy_set_eui64(void)
{
}

//----------------------------------------------------------------
// Calls from sl_zigbee_stack.h

sl_status_t sli_802154phy_set_phy_radio_power(int8_t power)
{
  if (power < MIN_RADIO_POWER
      || power > MAX_RADIO_POWER) {
    return SL_STATUS_TRANSMIT_INVALID_POWER;
  } else {
    phyPower = power;
    return SL_STATUS_OK;
  }
}

#ifndef MAC_TEST_STACK
sl_status_t sli_802154phy_set_phy_radio_channel(uint8_t radioChannel)
{
  if ((BIT32(radioChannel) & SL_ZIGBEE_ALL_802_15_4_CHANNELS_MASK) != 0) {
    phyChannel = radioChannel;
    return SL_STATUS_OK;
  } else {
    return SL_STATUS_INVALID_CHANNEL_MASK;
  }
}
#endif

//----------------------------------------------------------------
// 'Random' numbers
//
// To give the test more control over the stack's behavior we allow the test
// supply us with an array of the random numbers to be used.

static uint16_t *randomNumbers = NULL;
static uint16_t randomNumberLength = 0;
static uint16_t randomNumberCounter = 0;

void setRandomNumberScript(uint16_t *numbers, uint16_t length)
{
  randomNumberCounter = 0;
  randomNumberLength = length;
  randomNumbers = numbers;
}

// We default to always returning 1, which may not be the best thing to do.

uint16_t halCommonGetRandomTraced(char *file, int line)
{
  if (randomNumbers == NULL) {
    return 0x0001;
  } else if (randomNumberCounter < randomNumberLength) {
    return randomNumbers[randomNumberCounter++];
  } else {
    fprintf(stderr, "Ran out of random numbers at line %d of %s\n",
            line,
            file);

    return 0;
  }
}

//----------------------------------------------------------------
// HAL and other hardware stubs

// The IP stack does not use the EventControl mechanism.
#if (!defined (SL_ZIGBEE_STACK_IP) && !defined (MAC_TEST_STACK))

extern sl_zigbee_event_control_t emScanEvent;
extern void emScanEventHandler(void);

#ifdef ALT_MAC
extern sl_zigbee_event_control_t emAltMacTransmitEvent;
extern void emAltMacTransmitEventHandler(void);
#endif  // ALT_MAC

sl_zigbee_event_data_t stackEvents[] =
{
  { &sl_mac_indirect_event, sli_mac_indirect_event_handler },
  { &emScanEvent, emScanEventHandler },
#ifdef ALT_MAC
  { &emAltMacTransmitEvent, emAltMacTransmitEventHandler },
#endif
  { NULL, NULL }         // terminator
};

void sl_mac_lower_mac_mark_buffers(void);
void sli_mac_mark_upper_mac_buffers(void);
void sli_mac_mark_indirect_buffers(void);
void sli_mac_mark_scan_buffer(void);
void emMarkAltMacBuffers(void);
static BufferMarker bufferMarkers[] =
{
  sl_mac_lower_mac_mark_buffers,
  sli_mac_mark_upper_mac_buffers,
  sli_mac_mark_indirect_buffers,
  sli_mac_mark_scan_buffer,
#ifdef ALT_MAC
  emMarkAltMacBuffers,
#endif
  NULL
};

#endif // ifndef SL_ZIGBEE_STACK_IP

// Timers
//
// We track time by radio symbols.

extern sl_zigbee_event_data_t lowerMacEvents[];
#ifndef MAC_TEST_STACK
extern sl_zigbee_event_data_t upperMacEvents[];
#endif
static uint32_t symbolTimer = 0;

void scriptTick(void)
{
  symbolTimer += 1;
  checkSymbolDelayIsr();

#ifdef SL_ZIGBEE_STACK_IP
  emMacCheckIncomingQueue();
#else
  sli_mac_lower_mac_tick();
#endif

#ifndef MAC_TEST_STACK
  sli_mac_indirect_check_poll_flags();
#endif

#ifdef SL_ZIGBEE_STACK_IP
  sli_zigbee_run_event_queue(&sli_zigbee_stack_event_queue);
#else
  sli_legacy_buffer_manager_reclaim_unused_buffers(bufferMarkers);
  sl_zigbee_run_events(stackEvents);
  sl_zigbee_run_events(lowerMacEvents);
  sl_zigbee_run_events(upperMacEvents);
#endif  // ifdef SL_ZIGBEE_STACK_IP
}

uint32_t scriptTime(void)
{
  return symbolTimer;
}

#define SYMBOL_TIMER_TICK_US 16
// System tick units are 1/1024 of a second.  Symbol timer units are 16
// microseconds.  (1/1024 S) / (16 uS) = 61.

uint16_t halCommonGetInt16uMillisecondTick(void)
{
  return (uint16_t) (symbolTimer / 61);
}

uint32_t halCommonGetInt32uMillisecondTick(void)
{
  return symbolTimer / 61;
}

uint16_t halCommonGetInt16uQuarterSecondTick(void)
{
  uint16_t quarterSeconds;

  // Want (1/4) seconds.  Symbol timer units are 16 microseconds.
  // (1/4 S) / (16 uS) = 15625

  quarterSeconds = (uint16_t) (symbolTimer / 15625);

  return quarterSeconds;
}

// sTODO: halPTA test calls to check zigbee make test runs proparly
static halPtaReq_t txReq;
static halPtaCb_t txCb;
void halCommonDelayMicroseconds(uint16_t us)
{
#ifndef SL_ZIGBEE_SCRIPTED_TEST
  simulatedTimePassesUs(us);
#endif
}

void halCommonDelayMilliseconds(uint16_t msec)
{
  uint16_t cnt = msec;

  if (msec == 0) {
    return;
  }

  while (cnt-- > 0)
    halCommonDelayMicroseconds(1000);
}

void triggerGrantSignal(uint8_t signal)
{
  if (signal) {
    if ((txCb != NULL) && (txReq & PTA_REQCB_GRANTED)) {
      (*txCb)(PTA_REQCB_GRANTED);
      txReq &= ~PTA_REQCB_GRANTED;
    }
  }
}

EmberStatus halPtaSetTxRequest(halPtaReq_t ptaReq, halPtaCb_t ptaCb)
{
  EmberStatus status = SL_ZIGBEE_ERR_FATAL;
  txCb  = ptaCb;
  if (txReq == ptaReq) {
    // Save a little time if redundant request
  } else {
    txReq = ptaReq;
  }
  if (txReq & PTA_REQCB_GRANTED) {
    halCommonDelayMilliseconds(10);    //self imposed delay for simulation purposes
    triggerGrantSignal(emGrantSignal);    // if not from here, this needs to be called from CLI
  }
  status = SL_ZIGBEE_SUCCESS;
  return status;
}

void halStackRadioHoldOffPowerDown(void)
{
}

void halStackRadioHoldOffPowerUp(void)
{
}

HalPtaOptions halPtaGetOptions(void)
{
  return 0;
}

EmberStatus halPtaSetOptions(HalPtaOptions options)
{
  return SL_ZIGBEE_ERR_FATAL;
}

EmberStatus halPtaSetBool(HalPtaOptions option, bool value)
{
  return SL_ZIGBEE_ERR_FATAL;
}

EmberStatus halPtaSetEnable(bool enabled)
{
  return SL_ZIGBEE_ERR_FATAL;
}

bool halPtaIsEnabled(void)
{
  return false;
}

halPtaStackStatus_t halPtaStackEvent(halPtaStackEvent_t ptaStackEvent,
                                     uint32_t supplement)
{
  (void) ptaStackEvent;
  (void) supplement;
  return PTA_STACK_STATUS_SUCCESS;
}

//.........end of added stubs for zigbee make test

sl_status_t halCommonIdleForMilliseconds(uint32_t *duration)
{
  return SL_STATUS_OK;
}

// Symbol-timer API:

uint32_t halStackGetInt32uSymbolTick(void)
{
  return symbolTimer;
}

uint32_t simGetCurrentSymbolTime(void)
{
  return symbolTimer;
}

// The two symbol timers, A and B.
// New API:

static uint32_t nextSymbolDelayTimes[EM_HAL_SYMBOL_DELAY_CHANNELS];
static bool symbolDelayPendings[EM_HAL_SYMBOL_DELAY_CHANNELS];
static volatile
EmHalSymbolDelayCallback_t symbolDelayCallbacks[EM_HAL_SYMBOL_DELAY_CHANNELS];

static void halStackSymbolDelayIsr(uint8_t delayChan)
{
  EmHalSymbolDelayCallback_t cb = symbolDelayCallbacks[delayChan];
  if (cb != NULL) {
    symbolDelayCallbacks[delayChan] = NULL;
    (*cb)((EmHalSymbolDelayChannel_t)delayChan);
  }
}

static void checkSymbolDelayIsr(void)
{
  uint8_t delayChan;
  for (delayChan = 0; delayChan < EM_HAL_SYMBOL_DELAY_CHANNELS; delayChan++) {
    if (symbolDelayPendings[delayChan]
        && symbolTimer >= nextSymbolDelayTimes[delayChan]) {
      symbolDelayPendings[delayChan] = false;
      halStackSymbolDelayIsr(delayChan);
    }
  }
}

// Set this to true to trace symbol delay events.
bool simulatedSymbolDelayDebug = false;
static int delayCount = 0;

uint32_t halStackOrderSymbolDelay(EmHalSymbolDelayChannel_t delayChan,
                                  EmHalSymbolDelayCallback_t callback,
                                  uint32_t microseconds)
{
  uint32_t timerTicks = (microseconds + (SYMBOL_TIMER_TICK_US / 2))
                        / SYMBOL_TIMER_TICK_US; // Round
  uint32_t nextDelayTime = symbolTimer + timerTicks;

  if (callback == NULL) {
    // Caller just wants to know the absolute tick at expiry for polling
  } else {
    assert(delayChan < EM_HAL_SYMBOL_DELAY_CHANNELS);
    assert(symbolDelayCallbacks[delayChan] == NULL);
    assert(symbolDelayPendings[delayChan] == false);
    if (simulatedSymbolDelayDebug) {
      fprintf(stderr, "%d order symbol delay %c %d\n", delayCount++,
              'A' + delayChan, timerTicks);
      fprintf(stderr, "Next delay %c ends at %d\n", 'A' + delayChan,
              nextDelayTime);
    }
    if (timerTicks == 0) {
      // Timer will expire too quickly so simply call the completion routine now.
      (*callback)(delayChan);
    } else {
      symbolDelayCallbacks[delayChan] = callback;
      nextSymbolDelayTimes[delayChan] = nextDelayTime;
      symbolDelayPendings[delayChan] = true;
    }
  }
  return nextDelayTime;
}

void halStackCancelSymbolDelay(EmHalSymbolDelayChannel_t delayChan,
                               EmHalSymbolDelayCallback_t callback)
{
  assert(delayChan < EM_HAL_SYMBOL_DELAY_CHANNELS);
  if (symbolDelayCallbacks[delayChan] == callback) {
    if (simulatedSymbolDelayDebug) {
      fprintf(stderr, "cancel symbol delay %c\n", 'A' + delayChan);
    }
    symbolDelayCallbacks[delayChan] = NULL;
    symbolDelayPendings[delayChan] = false;
  }
}

#ifdef MAC_TEST_STACK

// Old API -- deprecated:

static void internalDelayAIsr(EmHalSymbolDelayChannel_t delayChan)
{
  UNUSED_VAR(delayChan);
  halStackSymbolDelayAIsr();
}

void halStackOrderInt16uSymbolDelayA(uint16_t symbols)
{
  // This API enforces a minimum one-symbol delay
  if (symbols == 0) {
    symbols = 1;
  }
  (void) halStackOrderSymbolDelay(EM_HAL_SYMBOL_DELAY_CHANNEL_A,
                                  &internalDelayAIsr,
                                  symbols * SYMBOL_TIMER_TICK_US);
}

void halStackCancelSymbolDelayA(void)
{
  halStackCancelSymbolDelay(EM_HAL_SYMBOL_DELAY_CHANNEL_A,
                            &internalDelayAIsr);
}

#endif//MAC_TEST_STACK

// End symbol-timer API

void phyVerifyNoActiveTimers(void)
{
  if ( symbolDelayPendings[EM_HAL_SYMBOL_DELAY_CHANNEL_A]
       || symbolDelayPendings[EM_HAL_SYMBOL_DELAY_CHANNEL_B]) {
    fprintf(stderr, "[ timing error ]\n");
  } else {
    if (simulatedSymbolDelayDebug) {
      fprintf(stderr, "[ symbol timer is idle as expected ]\n");
    }
  }

  assert(symbolDelayPendings[EM_HAL_SYMBOL_DELAY_CHANNEL_A] == false);
  assert(symbolDelayPendings[EM_HAL_SYMBOL_DELAY_CHANNEL_B] == false);
}

bool traceOkToIdleCalls = false;
// Idle mode stubs
void sli_802154phy_radio_ok_to_idle(void)
{
  if (traceOkToIdleCalls) {
    simpleScriptCheck("sli_802154phy_radio_ok_to_idle", "sli_802154phy_radio_ok_to_idle", "");
  }
  assert(radioCurrentPowerStatus == SL_ZIGBEE_RADIO_POWER_MODE_RX_ON);

  if (SL_ZIGBEE_RADIO_POWER_MODE_RX_ON == radioPowerOnIdleMode) {
    return;
  } else {
    sli_802154phy_radio_sleep();
  }

  radioCurrentPowerStatus = radioPowerOnIdleMode;
}

RadioPowerMode sli_802154phy_radio_get_idle_mode(void)
{
  return radioPowerOnIdleMode;
}

sl_status_t sli_802154phy_radio_set_idle_mode(RadioPowerMode mode)
{
  if (mode > SL_ZIGBEE_RADIO_POWER_MODE_OFF) {
    return SL_STATUS_FAIL;
  }

  radioPowerOnIdleMode = mode;

  // Put the radio into the new idle power mode.
  if ( SL_ZIGBEE_RADIO_POWER_MODE_OFF == radioCurrentPowerStatus) {
    // The radio is asleep.
    if (SL_ZIGBEE_RADIO_POWER_MODE_RX_ON == radioPowerOnIdleMode) {
      // The radio is supposed to be awake.  Wake it up.
      sli_802154phy_radio_wake_up();
    }
  } else {
    // The radio is awake.  Idle it (possibly sleep, depending on mode).
    sli_802154phy_radio_ok_to_idle();
  }

  return SL_STATUS_OK;
}

// Random stubs
EmberStatus halToggleAntenna(void)
{
  return SL_ZIGBEE_INVALID_CALL;
}

void halStackIndicateActivity(bool turnOn)
{
}

void sli_802154phy_start_transmit_tone(void)
{
}

void sli_802154phy_stop_transmit_tone(void)
{
}

void sli_802154phy_start_transmit_stream(void)
{
}

void sli_802154phy_stop_transmit_stream(void)
{
}

uint16_t sli_802154phy_radio_unpack_chip_errors(uint8_t internalChipErrors)
{
  return 0;
}

uint8_t sli_802154phy_radio_calculate_link_quality(uint16_t unpackedChipErrors, uint16_t packetLength)
{
  return 0xFF;
}

#ifdef SL_ZIGBEE_STACK_IP
uint8_t *emGetLongIdFromAuxFrame(uint8_t *packet)
{
  return sli_802154mac_local_eui64.bytes;
}

uint8_t sli_zigbee_add_neighbor_guaranteed(sl_802154_short_addr_t shortId,
                                           sl_802154_long_addr_t longId)
{
  return 0;
}

bool emCcmDecryptUsingAuxFrame(uint8_t *packet,
                               uint8_t *lengthLoc,
                               uint8_t *sourceEui64,
                               bool isMacPacket)
{
  return false;
}

uint8_t emMacEncrypt(uint8_t *packet, uint8_t packetLength, uint32_t frameCounter)
{
  return 0;
}

//******************************************************************************
#endif // SL_ZIGBEE_STACK_IP

uint32_t emPhyGetChannelFreqHz(uint8_t macPgChan)
{
  return 0;
}

uint8_t sli_802154phy_get_phy_channel(uint8_t c)
{
  return c;
}

void emRadioSetEdCcaThreshold(int8_t thresholddBm)
{
}

uint32_t sli_802154phy_get_mac_timer(void)
{
  return 0;
}
