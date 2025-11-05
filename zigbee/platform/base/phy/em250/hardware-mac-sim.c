/***************************************************************************//**
 * @file
 * @brief Simulation of the EM250 baseband. For now this simulates the
 * baseband as used by em250/phy.c.  The functionality in em250class/analogue.c
 * and em250class/phy-common.c is simulated directly, rather than simulating
 * the baseband that they use.  There is less value in running those files in
 * simulation, as they are mostly just wrappers around register reads and
 * writes.
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
#ifdef PHY_SIMULATION_DUAL
#define PHY_THIS    1  // Build this for native phy for simulation_dual.
#define PHY_EM250   1
#endif  // PHY_SIMULATION_DUAL

#include "core/sl_zigbee_stack.h"
#include "hal/hal.h"
#include "hal/micro/unix/simulation/em250/regs.h"
#include "phy/phy.h"
#ifdef  PHY_TRANSCEIVER_SIM
  #include "transceiver/phy-trx-sim.h"
#else
  #include "phy/em250/phy.h"
#endif  //PHY_TRANSCEIVER_SIM
#include "phy/em250/hardware-mac-sim.h"

#include "tool/simulator/child/timer.h"

#include "phy/ieee802154mac.h"
#include "phy/plugin/duty-cycle/duty-cycle.h" // For emPhyDcGetPktMode()
#include "sl_common.h" // for SL_FALLTHROUGH

#define simPrint(string, ...) do {} while (0)
//#define simPrint(string, ...) fprintf(stderr, "%d: " string "\n", simulatorId, ##__VA_ARGS__ )

extern uint16_t sli_802154phy_radio_receive_internal_isr(uint16_t interrupt, uint16_t pcbContext);

extern RadioPowerMode radioCurrentPowerStatus;

#ifdef PHY_HAS_DUTY_CYCLE
extern EmPhyBandConfig* mapMacPgChanToPhyChanBand(uint8_t macPgChan,
                                                  uint8_t* phyChanPtr,
                                                  uint8_t* macPagePtr,
                                                  EmPhySubBandConfig** subBandConfigPtr);
extern EmPhyBandConfig* emPhyCurrentBandConfig;
#endif

// These are shared variables between phy.c and analogue.c.  They
// need to have become emRadioChannel and emRadioPower.
uint8_t emRadioChannelPageInUse = 0;  //page 0 is mapped to 2.4GHz
uint8_t emRadioChannel = 16;
int8_t emRadioPower = -1;

uint16_t emRadioTxPowerMode;

// Stubs.
uint16_t emRadioFifoThreshold;
int8_t emSynthFreqOffset;

static bool radioActivitySuspended = false;

#define DBG(x)
//#define DBG(x) x

//TODO: Initialize these to a time in the distant past so lbtMinOffTime
//TODO: is not applied on first transmit after bootup?
static volatile uint32_t lastTxFinishedTimeMs;
static volatile uint32_t lastCsmaTxFinishedTimeMs;

//----------------------------------------------------------------
// Forward declarations.
static bool messageIsForUs(uint8_t* macHeader);

//------------------------------------------------------------------------------
// Simulated EM250 registers.

kRegs_t Regs;

//----------------------------------------------------------------
// The EM250 handles the following 802.15.4 MAC functions:
//   - Filtering incoming messages using the nodes PAN ID, two-byte
//     network ID (if defined) and EUI64.
//   - Transmission of acks for messages with the ACK_REQUIRED bit set.
//     There is a 12 symbol delay between the arrival of the message and
//     the transmission of the ack.  This is taken care of by the Java
//     radio simulation.
//
// A symbol is 16 microseconds and holds a nibble.
#define MICROSECONDS_PER_SYMBOL 16
#define SYMBOLS_PER_BYTE 2

// One backoff period is twenty symbols.
#define UNIT_BACKOFF_SYMBOLS 20

static void receiveLatencyTimerHandler(void);

static uint8_t receiveLatencyTimer;

static void macBackoffTimerHandler(void);

static uint8_t macBackoffTimer;

static uint8_t ackTimeoutTimer;
static void ackTimeoutTimerHandler(void);

static bool macAckNeeded = false;

static bool amCoordinator = false;

extern bool sli_802154mac_ack_needed;

extern uint8_t sli_legacy_mfglib_mode;
extern bool sli_legacy_mfglib_transmit_complete;

#define DEFAULT_ENERGY_LEVEL 0
static uint8_t energyLevels[SL_ZIGBEE_NUM_802_15_4_CHANNELS];

static bool initialized = false;

static uint8_t unsentBytes = 0;
static bool discarding = false;

// For pages other than 0, set the most-significant bits dropped in Pg encoding
// E.g. 3-bit Pgs 1-7 map to 5-bit Pages 24-31.
#define macPg2Page(macPg)        ( ((macPg) == 0) ? 0                                         \
                                   : ((macPg) | ((((1ul << (32 - MAX_CHANNELS_PER_PAGE)) - 1) \
                                                  >> (8 - CHANNEL_BITS)) << (8 - CHANNEL_BITS))) )
//----------------------------------------------------------------
// Channel and power boilerplate.

uint8_t sli_802154phy_get_phy_radio_channel(void)
{
  return emRadioChannel;
}

int8_t sli_802154phy_get_phy_radio_power(void)
{
  return emRadioPower;
}

static void (*radioSettingHandler)(uint8_t channelpage, uint8_t channel, int8_t power) = NULL;

void setRadioSettingHandler(void (*handler)(uint8_t channelpage, uint8_t channel, int8_t power))
{
  radioSettingHandler = handler;
}

sl_status_t emRadioSetChannelAndForceCalibration(uint8_t channel)
{
  // *hand-waving* The "radio" is now magically calibrated!

  return sli_802154phy_set_phy_radio_channel(channel);
}

#if MAC_HAS_CHANNEL_PAGES
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
#endif

sl_status_t sli_802154phy_set_phy_radio_channel(uint8_t newChannel)
{
#if PHY_HAS_DUTY_CYCLE
  EmPhyBandConfig* bandConfig = mapMacPgChanToPhyChanBand(newChannel, NULL,
                                                          NULL, NULL);
  if (bandConfig == NULL) {
    return SL_STATUS_TRANSMIT_INVALID_CHANNEL;
  }

  emPhyCurrentBandConfig = bandConfig;
#endif
  emRadioChannelPageInUse = macPg2Page(sli_802154mac_pg_chan_pg(newChannel));
  emRadioChannel = newChannel;
  if (radioSettingHandler != NULL) {
    radioSettingHandler(sli_802154mac_pg_chan_pg(emRadioChannel), sli_802154mac_pg_chan_ch(emRadioChannel), emRadioPower);
  }
  return SL_STATUS_OK;
}

void emStartRadioSetChannel(void)
{
}

sl_status_t emFinishRadioSetChannel(uint8_t channel)
{
  return (sli_802154phy_set_phy_radio_channel(channel) == SL_STATUS_OK) ? SL_STATUS_OK : SL_STATUS_FAIL;
}

sl_status_t sli_802154phy_set_phy_radio_power(int8_t newPower)
{
  emRadioPower = newPower;
  if (radioSettingHandler != NULL) {
    radioSettingHandler(sli_802154mac_pg_chan_pg(emRadioChannel), sli_802154mac_pg_chan_ch(emRadioChannel), emRadioPower);
  }
  return SL_STATUS_OK;
}

uint32_t emPhyGetLastTxCompleteTimeMs(bool lastCsmaTx)
{
  return (lastCsmaTx ? lastCsmaTxFinishedTimeMs : lastTxFinishedTimeMs);
}

// This is called by sli_802154phy_radio_init(), so we use it to initialize ourselves.
sl_status_t sli_802154_stack_set_tx_power_mode(uint16_t txPowerMode)
{
  (void) txPowerMode;
  if (!initialized) {
    macBackoffTimer = defineTimer(macBackoffTimerHandler);
    ackTimeoutTimer = defineTimer(ackTimeoutTimerHandler);
    receiveLatencyTimer = defineTimer(receiveLatencyTimerHandler);

    memset(energyLevels, DEFAULT_ENERGY_LEVEL, SL_ZIGBEE_NUM_802_15_4_CHANNELS);
    initialized = true;
  }

  return SL_STATUS_OK;
}

static void (*radioTransmitHandler)(uint8_t* memptr) = NULL;

void setRadioTransmitHandler(void (*handler)(uint8_t* memptr))
{
  radioTransmitHandler = handler;
}

// Actually transmit something.

static bool inBackoff = false;
static uint8_t *pendingTransmission = NULL;
static bool ccaEnabled;
bool inIsrContext = false;
static uint8_t macSequenceNumber;
enum {
  IDLE,
  TX,
  ACK_TX,
  WAITING_FOR_FRAMEPENDING
};

static uint8_t phySimState = IDLE;

// This appends the CRC and sets up the transmit DMA.  The CSMA parameters
// have already been written to registers (it might make more sense to set
// up the DMA first, but the radio ramp-up delay makes it work either way).
//
// All we check here is the backoff time, because we currently support only
// the standard MAC transmit code.

static uint8_t ccaAttempt;  // The number of CCA attempts

static uint8_t getRandomBackoff(void)
{
  uint8_t lastBackoff;
  if (radioTransmitConfig.checkCca) {
    uint16_t exponent = radioTransmitConfig.backoffExponentMin + ccaAttempt;
    if (exponent >= radioTransmitConfig.backoffExponentMax) {
      exponent = radioTransmitConfig.backoffExponentMax;
    }
    lastBackoff = ((halCommonGetRandom()) & ((1 << exponent) - 1));
  } else {
    lastBackoff = 0;
  }
  lastBackoff += radioTransmitConfig.minimumBackoff;

  return lastBackoff;
}

sl_status_t emRadioTransmit(uint8_t* memptr)
{
  uint8_t backoff;
  EmPhyDcMode pktMode;
  assert(!inBackoff);

  if (radioActivitySuspended) {
    lastTxFinishedTimeMs = halCommonGetInt32uMillisecondTick();
    if (radioTransmitConfig.checkCca) {
      lastCsmaTxFinishedTimeMs = lastTxFinishedTimeMs;
    }
    sli_802154phy_radio_transmit_complete_callback(SL_STATUS_CCA_FAILURE,
                                                   0,
                                                   false);
    return SL_STATUS_OK;
  }

#ifdef SIMULATED_TRANSCEIVER
  // Assuming simulated transceiver is always used for subghz that uses LBT
  pktMode = EM_PHY_DC_MODE_TALK;
#else
  pktMode = emPhyDcGetPktMode();
#endif

  if ((pktMode == EM_PHY_DC_MODE_TALK)
      || (pktMode == EM_PHY_DC_MODE_DIALOG)) {
    // Transmit directly as LBT succeeded.
    backoff = 0;
  } else {
    backoff = getRandomBackoff();
  }

  simPrint("emRadioTransmit() backoff is %d, state is %d, seq 0x%x",
           backoff, phySimState,
           memptr[3]);

  if (SL_ZIGBEE_RADIO_POWER_MODE_OFF == sli_802154phy_radio_get_idle_mode()) {
    // EMIPSTACK-336 - use sli_802154phy_radio_wake_up() rather than a 'manual' subset
    sli_802154phy_radio_wake_up();

    simPrint("%s: radioCurrentPowerStatus: %s, MAC_CONFIG: %s",
             "emRadioTransmit()",
             (sli_802154phy_radio_get_power_status() == SL_ZIGBEE_RADIO_POWER_MODE_RX_ON
              ? "ON"
              : "OFF"),
             (MAC_CONFIG & MAC_CONFIG_MAC_MODE
              ? "ON"
              : "OFF"));
  }

  // At this point, the radio should be on.  Either we just
  // turned it on with the conditional above, or it was already on.
  // Also the radioCurrentPowerStatus should reflect the current
  // radio state of ON.  Either is a bug in the PHY or MAC code above
  // this simulated code.
  if ((!(MAC_CONFIG & MAC_CONFIG_MAC_MODE))
      && (sli_802154phy_radio_get_power_status() == SL_ZIGBEE_RADIO_POWER_MODE_RX_ON)) {
    fprintf(stderr, "FATAL:  MAC_CONFIG register and radio power state mismatch!\n");
    assert(0);
  }

  pendingTransmission = memptr;
  ccaEnabled = (radioTransmitConfig.checkCca
                && (pktMode != EM_PHY_DC_MODE_TALK)
                && (pktMode != EM_PHY_DC_MODE_DIALOG));

  inBackoff = true;

  simPrint("on: inBackoff = %d", inBackoff);
  ccaAttempt = 0;  // The number of CCA attempts
  if (backoff == 0) {
    macBackoffTimerHandler();
  } else {
    setTimer(macBackoffTimer,
             backoff * UNIT_BACKOFF_SYMBOLS * MICROSECONDS_PER_SYMBOL);
  }
  return SL_STATUS_OK;
}

static void macBackoffTimerHandler(void)
{
  assert(inBackoff);
  inBackoff = false;
  simPrint("off: inBackoff = %d", inBackoff);

  simPrint("phySimState = %d", phySimState);

  if (phySimState == IDLE
      && (!ccaEnabled
          || emRadioGetCca() == CCA_CLEAR)
      && !radioActivitySuspended) {
    simPrint("CCA clear, for seq 0x%x", pendingTransmission[3]);
    if (radioTransmitHandler != NULL) {
      radioTransmitHandler(pendingTransmission);
    }

    if (!sli_legacy_mfglib_mode) {
      macAckNeeded = (pendingTransmission[1] & 0x20) != 0;
      sli_802154mac_ack_needed = macAckNeeded;
      macSequenceNumber = pendingTransmission[3];
    }
    pendingTransmission = NULL;
    phySimState = TX;
  } else {
    if (++ccaAttempt < radioTransmitConfig.ccaAttemptMax
        && !radioActivitySuspended) {
      // Wait further
      uint8_t backoff = getRandomBackoff();
      inBackoff = true;
      simPrint("CCA NOT clear, ccaAttempt %d on backoff %d for seq 0x%x", ccaAttempt, backoff, pendingTransmission[3]);
      if (backoff == 0) {
        macBackoffTimerHandler();
      } else {
        setTimer(macBackoffTimer,
                 backoff * UNIT_BACKOFF_SYMBOLS * MICROSECONDS_PER_SYMBOL);
      }
    } else {
      // Give up
      lastTxFinishedTimeMs = halCommonGetInt32uMillisecondTick();
      if (radioTransmitConfig.checkCca) {
        lastCsmaTxFinishedTimeMs = lastTxFinishedTimeMs;
      }
      sli_802154phy_radio_transmit_complete_callback(SL_STATUS_CCA_FAILURE,
                                                     0,
                                                     false);
      phySimState = IDLE;
    }
  }
}

void _radioTransmitComplete(void)
{
  uint8_t oldPhySimState = phySimState;

  phySimState = IDLE;

#ifndef SIMULATED_TRANSCEIVER
  // EMIPSTACK-336 -- if INT_MACTX is off then this event would never occur
  if ((INT_CFG & INT_MACTX) == 0) {
    simPrint("_radioTransmitComplete(): transmit aborted, radioCurrentPowerStatus: %s, MAC_CONFIG: %s",
             (sli_802154phy_radio_get_power_status() == SL_ZIGBEE_RADIO_POWER_MODE_RX_ON
              ? "on"
              : "off"),
             ((MAC_CONFIG & MAC_CONFIG_MAC_MODE) ? "on" : "off"));
    // I have seen bugs in the PHY where the radio state is mismatched.
    // This is my attempt to help track them down.
    if ((!(MAC_CONFIG & MAC_CONFIG_MAC_MODE))
        && sli_802154phy_radio_get_power_status() == SL_ZIGBEE_RADIO_POWER_MODE_RX_ON) {
      fprintf(stderr, "FATAL: MAC_CONFIG register and radioCurrentPowerStatus mismatch!\n");
      assert(0);
    }
    return;
  }
#endif  //SIMULATED_TRANSCEIVER

  if (oldPhySimState == IDLE) {
    // Ignore message from simulator telling us our transmit is complete if we
    // don't think we sent anything. This can happen if we start transmitting
    // and then reboot.
    return;
  } else if (oldPhySimState == TX) {
    if (MAC_RX_CONFIG_REG & MAC_RX_CONFIG_AUTO_ACK_MASK) {
      simPrint("radio complete callback");
      if (macAckNeeded) {
        phySimState = TX;
        setTimer(ackTimeoutTimer,
                 MAC_ACK_WAIT_DURATION * MICROSECONDS_PER_SYMBOL);
      } else {
        sli_802154phy_radio_transmit_complete_callback((radioActivitySuspended
                                                        ? SL_STATUS_CCA_FAILURE
                                                        : SL_STATUS_OK),
                                                       0,
                                                       false);
      }
    }
    lastTxFinishedTimeMs = halCommonGetInt32uMillisecondTick();
    if (radioTransmitConfig.checkCca) {
      lastCsmaTxFinishedTimeMs = lastTxFinishedTimeMs;
    }
  } else if (oldPhySimState == ACK_TX) {
    lastTxFinishedTimeMs = halCommonGetInt32uMillisecondTick();
    // ACKs don't update lastCsmaTxFinishedTimeMs
    sli_802154phy_radio_tx_ack_isr_callback();
#ifndef SIMULATED_TRANSCEIVER
    // Account for this ACK in DC plugin
    emPhyDcAckSent();
#endif
  }
//  inIsrContext = false;  // we are not currently maintaining this

  // mfglibSendPacket waits after calling emRadioTransmit until
  // sli_legacy_mfglib_transmit_complete is set to true.
  if (sli_legacy_mfglib_mode) {
    sli_legacy_mfglib_transmit_complete = true;
  }
}

static void ackTimeoutTimerHandler(void)
{
  simPrint("ACK timed out");
#ifndef SIMULATED_TRANSCEIVER
  // EMIPSTACK-336 Mac INT_MACTX events must be on for this event to fire
  assert(INT_CFG & INT_MACTX);
#endif
  macAckNeeded = false;
  phySimState = IDLE;
  sli_802154phy_radio_transmit_complete_callback(SL_STATUS_MAC_NO_ACK_RECEIVED,
                                                 0,
                                                 false);
}

//----------------------------------------------------------------
// Reception and sending ACKs
//
// In the new HAL model, the determination of link quality happens in
// the HAL.  The simulator models various radios, and so the link quality
// must be computed there.
//
// If filtering is turned on this has to check that the packet is one that
// we should be seeing.
//
// 'packet' is in the over-the-air format, including having the length in
// the first byte.

//------------------------------------------------------------------------------
// Simulated Baseband-MAC interface (Interrupts and DMA System)

// Preprocessor directives in phy.c steer accesses to the hardware DMA address
// registers to these pointers.  Need to do this because the pointers are
// different sizes on the em250 and the simulator host platform.

uint8_t *simulatedMacRxStAddrA;
uint8_t *simulatedMacRxEndAddrA;
uint8_t *simulatedMacRxStAddrB;
uint8_t *simulatedMacRxEndAddrB;
uint8_t *simulatedMacTxStAddrA;
uint8_t *simulatedMacTxEndAddrA;
uint8_t *simulatedMacTxStAddrB;
uint8_t *simulatedMacTxEndAddrB;

// These are byte counts, while the hardware actually uses word counts.
// processIncomingByte() maintains the hardware word count register values.
//static int dmaAByteCount = 0;
//static int dmaBByteCount = 0;

// These two variables are used by processIncomingByte() to determine when to
// call the Isr.  1 byte time tick = 32us.
//
// The test-script-writing code must set targetLatencyByteTimeTicks to an
// appropritate value for each runScript() session.  targetLatencyByteTimeTicks
// must stay constant throughout the script writing session.
// The test-script-writing code must set currentLatencyByteTimeTicks to zero at
// the beginning of each runScript() session.  processIncomingByte() will set
// currentLatencyByteTimeTicks to zero at every latency interval thereafter.
int currentLatencyByteTimeTicks = 0;
int targetLatencyByteTimeTicks = 0;

// packetLength and packetIndex are initialized by addIncomingPacket() and used
// by processIncomingByte() to determine when the packet has completed.
static int packetIndex;
static int packetLength;

typedef struct {
  uint16_t statusRxActiveMask;
  uint16_t configRxLoadMask;
  uint16_t rxIntUnloadMask;
  volatile uint16_t *rxCountReg;
  uint16_t byteCount;
  uint8_t **buffer;
  uint8_t **bufferEnd;
} DmaRegisters;

DmaRegisters dmaRegisters[] = {
  {
    MAC_DMA_STATUS_RX_ACTIVE_A_MASK,
    MAC_DMA_CONFIG_RX_LOAD_A_MASK,
    MAC_RX_INT_SRC_RX_A_UNLOAD_SRC_MASK,
    &(RX_A_COUNT_REG),
    0,
    &simulatedMacRxStAddrA,
    &simulatedMacRxEndAddrA
  },
  {
    MAC_DMA_STATUS_RX_ACTIVE_B_MASK,
    MAC_DMA_CONFIG_RX_LOAD_B_MASK,
    MAC_RX_INT_SRC_RX_B_UNLOAD_SRC_MASK,
    &(RX_B_COUNT_REG),
    0,
    &simulatedMacRxStAddrB,
    &simulatedMacRxEndAddrB
  }
};

// Reset unloads both.  First loaded is active, with ties going to A.

void emSetMacDmaConfig(uint16_t value)
{
  if (value & MAC_DMA_CONFIG_RX_DMA_RESET) {
    MAC_DMA_CONFIG_REG &= ~(MAC_DMA_CONFIG_RX_LOAD_A_MASK
                            | MAC_DMA_CONFIG_RX_LOAD_B_MASK);

    MAC_DMA_STATUS_REG &= ~(MAC_DMA_STATUS_RX_ACTIVE_A_MASK
                            | MAC_DMA_STATUS_RX_ACTIVE_B_MASK);
  }

  if (value & MAC_DMA_CONFIG_RX_LOAD_A_MASK) {
    MAC_DMA_CONFIG_REG |= MAC_DMA_CONFIG_RX_LOAD_A_MASK;
    if (!(MAC_DMA_STATUS_REG & MAC_DMA_STATUS_RX_ACTIVE_B)) {
      MAC_DMA_STATUS_REG |= MAC_DMA_STATUS_RX_ACTIVE_A;
    }
  }

  if (value & MAC_DMA_CONFIG_RX_LOAD_B_MASK) {
    MAC_DMA_CONFIG_REG |= MAC_DMA_CONFIG_RX_LOAD_B_MASK;
    if (!(MAC_DMA_STATUS_REG & MAC_DMA_STATUS_RX_ACTIVE_A)) {
      MAC_DMA_STATUS_REG |= MAC_DMA_STATUS_RX_ACTIVE_B;
    }
  }
}

#ifndef SIMULATED_TRANSCEIVER
static void addByte(uint8_t aOrB, uint8_t byte, bool packetComplete)
{
  DmaRegisters *registers = dmaRegisters + aOrB;
  DmaRegisters *otherRegisters = dmaRegisters + (1 - aOrB);

  (*(registers->buffer))[registers->byteCount] = byte;
  registers->byteCount += 1;
  *registers->rxCountReg = registers->byteCount >> 1;

  if (packetComplete
      || (*(registers->bufferEnd) - *(registers->buffer) + 1
          == registers->byteCount)) {
    simPrint("DMA_CONFIG %04X", MAC_DMA_CONFIG);
    simPrint("%c active, done writing %d bytes to %lX",
             aOrB == 0 ? 'A' : 'B',
             registers->byteCount,
             (unsigned long) *registers->buffer);
    MAC_DMA_STATUS_REG &= ~(registers->statusRxActiveMask);
    MAC_DMA_CONFIG_REG &= ~(registers->configRxLoadMask);
    MAC_RX_INT_SRC_REG |= registers->rxIntUnloadMask;
    registers->byteCount = 0;
    if (MAC_DMA_CONFIG_REG & otherRegisters->configRxLoadMask) {
      MAC_DMA_STATUS_REG |= otherRegisters->statusRxActiveMask;
    }
  }
}

// elapsedByteTimeTicks is in byte times (32us) and will be either
// 0 or 1.

static uint8_t unsentByteLimit = 0;

static void processIncomingByte(uint8_t byte, bool isAppendedInfo)
{
  bool packetComplete = false;

  if (MAC_RX_INT_SRC_REG & MAC_RX_INT_SRC_RX_OVFLW_SRC_MASK) {
    DBG(printf("processIncomingByte:abort(Overflow)\n"));
    return;
  }

  if (discarding) {
    return;
  }

  packetIndex += 1;

  if (packetLength == packetIndex) {
    packetComplete = true;

    if (MAC_RX_INT_SRC_REG & MAC_RX_INT_SRC_RX_COMPLETE_SRC_MASK) {
      INT_MISS |= INT_MACRX;
      simPrint("setting INT_MACRX");
    }
    MAC_RX_INT_SRC_REG |= MAC_RX_INT_SRC_RX_COMPLETE_SRC_MASK;
    MAC_RX_INT_SRC_REG |= MAC_RX_INT_SRC_RX_ADDR_REC_COMP_SRC_MASK;
  }

  if (MAC_DMA_STATUS_REG & MAC_DMA_STATUS_RX_ACTIVE_A_MASK) {
    addByte(0, byte, packetComplete);
  } else if (MAC_DMA_STATUS_REG & MAC_DMA_STATUS_RX_ACTIVE_B_MASK) {
    addByte(1, byte, packetComplete);
  } else {
    DBG(printf("OVERFLOW\n"));
    MAC_RX_INT_SRC_REG = MAC_RX_INT_SRC_RX_OVFLW_SRC_MASK;
    simPrint("overflow");
    sli_802154phy_radio_receive_internal_isr(0, 0);
    discarding = true;
    return;
  }

  if (!isAppendedInfo || packetComplete) {
    unsentBytes += 1;
  }
  if (unsentByteLimit <= unsentBytes) {
    sli_802154phy_radio_receive_internal_isr(0, 0);
    unsentBytes = 0;
    // 0x007F gets lots of overflows, enough to interfere with 'make test'.
    // Setting this to one avoids any overflows at all, which is probably
    // best for 'make test'.
    // unsentByteLimit = (halCommonGetRandom() & 0x007F) + 5;
    unsentByteLimit = 1;
  } else if (packetComplete) {
    simPrint("setting timer");
    setTimer(receiveLatencyTimer,
             unsentBytes * SYMBOLS_PER_BYTE * MICROSECONDS_PER_SYMBOL);
  }
}

#endif //SIMULATED_TRANSCEIVER

static void receiveLatencyTimerHandler(void)
{
  if (unsentBytes != 0) {
    simPrint("leftover bytes timed out");
  }
  simPrint("receive ISR C");
  sli_802154phy_radio_receive_internal_isr(0, 0);
  unsentBytes = 0;
}

#ifndef SIMULATED_TRANSCEIVER
static void processIncomingWord(uint16_t theWord, bool isAppendedInfo)
{
#if BIGENDIAN_CPU
  processIncomingByte(HIGH_BYTE(theWord), isAppendedInfo);
  processIncomingByte(LOW_BYTE(theWord), isAppendedInfo);
#else
  processIncomingByte(LOW_BYTE(theWord), isAppendedInfo);
  processIncomingByte(HIGH_BYTE(theWord), isAppendedInfo);
#endif
}

#endif //SIMULATED_TRANSCEIVER

bool _radioReceive(uint8_t *message, uint32_t rxSyncTime, uint8_t linkQuality)
{
  uint16_t frameStatus;
  uint16_t frameControl = HIGH_LOW_TO_INT(message[2], message[1]);
  uint8_t sequenceNumber = message[3];

  packetIndex = 0;
  discarding = false;

  if (radioActivitySuspended
      || !(MAC_CONFIG & MAC_CONFIG_MAC_MODE)     // check if radio off
      || ( (MAC_RX_CONFIG_REG & MAC_RX_CONFIG_FILT_ADDR_ON_MASK)
           && !messageIsForUs(message + 1))) {
    simPrint("_radioReceive(): incoming message dropped, radioCurrentPowerStatus: %s, MAC_CONFIG: %s, filtering %d",
             (sli_802154phy_radio_get_power_status() == SL_ZIGBEE_RADIO_POWER_MODE_RX_ON
              ? "on"
              : "off"),
             ((MAC_CONFIG & MAC_CONFIG_MAC_MODE) ? "on" : "off"),
             MAC_RX_CONFIG_REG & MAC_RX_CONFIG_FILT_ADDR_ON_MASK);

    // I have seen bugs in the PHY where the radio state is mismatched.
    // This is my attempt to help track them down.
    if ((!(MAC_CONFIG & MAC_CONFIG_MAC_MODE))
        && sli_802154phy_radio_get_power_status() == SL_ZIGBEE_RADIO_POWER_MODE_RX_ON) {
      fprintf(stderr, "FATAL: MAC_CONFIG register and radioCurrentPowerStatus mismatch!\n");
      assert(0);
    }
    return true;
  }

  cancelTimer(receiveLatencyTimer);
  if (unsentBytes != 0) {
    simPrint("%d bytes left from earlier", unsentBytes);
  }

  if ((frameControl & (MAC_FRAME_SOURCE_MODE_MASK
                       | MAC_FRAME_DESTINATION_MODE_MASK
                       | MAC_FRAME_FLAG_INTRA_PAN))
      == (MAC_FRAME_SOURCE_MODE_SHORT
          | MAC_FRAME_DESTINATION_MODE_SHORT
          | MAC_FRAME_FLAG_INTRA_PAN)) {
    if (HIGH_LOW_TO_INT(message[7], message[6])
        != 0xFFFF) {
      simPrint("incoming %04X -> %04X",
               HIGH_LOW_TO_INT(message[9], message[8]),
               HIGH_LOW_TO_INT(message[7], message[6]));
    }
  } else {
    simPrint("incoming");
  }

  // Incoming messages do not include length byte, CRC, or the appended info.
  packetLength = 1 + message[0] + 2 + NUM_APPENDED_INFO_BYTES;

  simPrint("%d bytes arriving, %d total", message[0], packetLength);
  if ((frameControl & MAC_FRAME_TYPE_MASK)
      == MAC_FRAME_TYPE_ACK) {
    simPrint("macAckNeeded %d have 0x%x want 0x%x",
             macAckNeeded,
             sequenceNumber,
             macSequenceNumber);
    if (macAckNeeded
        && sequenceNumber == macSequenceNumber) {
      cancelTimer(ackTimeoutTimer);
      phySimState = IDLE;
      macAckNeeded = false;
      sli_802154phy_radio_transmit_complete_callback(
        SL_STATUS_TRANSMIT_ACK_RECEIVED,
        0,
        (frameControl & MAC_FRAME_FLAG_FRAME_PENDING) != 0);
    }
    return true;
  }

#ifdef SIMULATED_TRANSCEIVER
  uint8_t rxBuffer[128 + 2 + NUM_APPENDED_INFO_BYTES];   //127 + 1 (PHR), 2 for crc

  if (phySimState != IDLE || sli_802154phy_radio_get_power_status() == SL_ZIGBEE_RADIO_POWER_MODE_OFF) {
    simPrint("phySimState %x power %d, need a assert here", phySimState, sli_802154phy_radio_get_power_status());
    assert(0);
  }

  if ((packetLength >= (128 + 2 + NUM_APPENDED_INFO_BYTES))
      || (packetLength < NUM_APPENDED_INFO_BYTES)) {
    return false;
  }

  memcpy(rxBuffer, message, packetLength);

  //Append the CRC
  memset(rxBuffer + message[0] + 1, 0xEE, 2);    //message[0] + 1 as length byte doesn't include in incm messages
  rxBuffer[0] += 2;

  frameStatus = FRAME_STATUS_CRC_PASS;
  //Append info
  uint8_t *appendedInfo = rxBuffer + 2 + message[0] + 1;    // and 2 bytes for CRC
  appendedInfo[0] = (uint8_t) (frameStatus >> 8);
  appendedInfo[1] = (uint8_t) (frameStatus & 0xFF);
  appendedInfo[2] = 0;            //RSSI high
  appendedInfo[3] = linkQuality;   //RSSI low
  appendedInfo[4] = 0;            //Gain high
  appendedInfo[5] = (uint8_t) (rxSyncTime >> 16 & 0xFF);   //Gain low
  appendedInfo[6] = (uint8_t) (rxSyncTime >> 8 & 0xFF);
  appendedInfo[7] = (uint8_t) (rxSyncTime & 0xFF);
  rxBuffer[0] += NUM_APPENDED_INFO_BYTES;

  //Update the phySimState to verify, before sending phy-ack
  if ((frameControl & MAC_FRAME_FLAG_ACK_REQUIRED)
      && (MAC_RX_CONFIG_REG & MAC_RX_CONFIG_AUTO_ACK_MASK)
      && (MAC_RX_CONFIG_REG & MAC_RX_CONFIG_FILT_ADDR_ON_MASK)) {
    phySimState = WAITING_FOR_FRAMEPENDING;
  }

  sli_zigbee_packet_received_internal_isr_callback(rxBuffer, rxBuffer[0], false);
#else
  uint8_t i;
  processIncomingByte(message[0] + 2, false);         // two bytes for CRC
  for (i = 1; i <= message[0]; i++) {
    processIncomingByte(message[i], false);
  }

  // Append the CRC.
  processIncomingByte(0xEE, false);
  processIncomingByte(0xEE, false);

  // Append MAC Appended Info.

  // Pad with a zero if the total length is odd (even without length byte).
  if ((message[0] & 1) == 0) {
    packetLength += 1;
    processIncomingByte(0, false);
  }

  frameStatus = FRAME_STATUS_CRC_PASS;

  // Append MAC Frame Info.
  // The MAC hardware writes these 8 bytes in one operation, resulting in both
  // DMA buffers unloading when the appended info spans a buffer boundary.
  processIncomingByte(HIGH_BYTE(frameStatus), true);
  processIncomingByte(LOW_BYTE(frameStatus), true);
  processIncomingWord(HIGH_LOW_TO_INT(0 /* RSSI */, linkQuality), true);
  processIncomingWord(HIGH_LOW_TO_INT(0 /* gain */, rxSyncTime >> 16), true);
  processIncomingWord(rxSyncTime & 0xFFFF, true);

  if ((frameControl & MAC_FRAME_FLAG_ACK_REQUIRED)
      && (MAC_RX_CONFIG_REG & MAC_RX_CONFIG_AUTO_ACK_MASK)
      && (MAC_RX_CONFIG_REG & MAC_RX_CONFIG_FILT_ADDR_ON_MASK)
      && !discarding) {
    bool ackFramePending =
      (MAC_ACK_STROBE_REG & MAC_ACK_STROBE_FRAME_PENDING_MASK)
      != 0;
    uint16_t ackFrameControl =
      MAC_FRAME_TYPE_ACK
      | (ackFramePending ? MAC_FRAME_FLAG_FRAME_PENDING : 0);

    // For the way this simulation code is set up, if we've sent a packet and
    // are awaiting a confirmation (ACK or radio callback for transmit
    // complete), we don't respond to the currently received packet until we
    // that acknowledgement (or timeout) happens
    if (phySimState != IDLE) {
      simPrint("Dropping RX packet due to phySimState %d", phySimState);
      return true;
    }
    simPrint("sending ack %d seq 0x%x", ackFramePending, sequenceNumber);

    if (radioTransmitHandler != NULL) {
      uint8_t ackBuff[] = { 5,      // number of following bytes
                            LOW_BYTE(ackFrameControl),
                            HIGH_BYTE(ackFrameControl),
                            sequenceNumber,
                            0,    // dummy CRC
                            0     // dummy CRC
      };
      radioTransmitHandler(ackBuff);
    }
    phySimState = ACK_TX;
  }

  // Clear this for later use.
  MAC_ACK_STROBE_REG &= ~MAC_ACK_STROBE_FRAME_PENDING_MASK;
#endif //SIMULATED_TRANSCEIVER

  return true;
}

// Check to see if it is for us.  This isn't the full 15.4 check, just that
// part of it that the EM2420 does. Discard the packet if frame type is
// invalid/reserved. ACKs are for everyone. Beacons need to
// have the correct PAN ID (this is where the EM2420 is a bit goofy - it
// doesn't pass up beacons with the broadcast PAN ID unless our PAN ID is
// also the broadcast PAN ID).  Non-beacons need to match our PAN ID and be
// either broadcasts (sent to 0xFFFF) or match our short or long ID.
//
// The first address is the source for beacons and the destination for
// non-beacons, but either way the PAN ID has to match ours.

static bool messageIsForUs(uint8_t* macHeader)
{
  uint16_t control = HIGH_LOW_TO_INT(macHeader[1], macHeader[0]);
  uint8_t packetType = control & MAC_FRAME_TYPE_MASK;
  uint16_t panId = HIGH_LOW_TO_INT(macHeader[4], macHeader[3]);

  if (packetType >= MAC_FRAME_TYPE_RESERVED_MASK) {
    simPrint("Invalid frame type 0x%X", (packetType & MAC_FRAME_TYPE_MASK));
    return false;
  } else if (packetType == MAC_FRAME_TYPE_ACK) {
    simPrint("incoming ACK");
    return true;
  } else if (!(panId == PAN_ID_REG
               || panId == 0xFFFF
               || (PAN_ID_REG == 0xFFFF
                   && packetType == MAC_FRAME_TYPE_BEACON))) {
    simPrint("PAN_ID_REG %04X panId %04X", PAN_ID_REG, panId);
    return false;
  } else if (packetType == MAC_FRAME_TYPE_BEACON) {
    return true;        // We pass up broadcast beacons; I think that the
  }
  // EM2420 doesn't.
  else {
    switch (control & MAC_FRAME_DESTINATION_MODE_MASK) {
      case MAC_FRAME_DESTINATION_MODE_SHORT: {
        uint16_t id = HIGH_LOW_TO_INT(macHeader[6], macHeader[5]);
        //assert(! (panId == 0xFFFF && id != 0xFFFF));      // debugging check
        return id == SHORT_ADDR_REG  || id == 0xFFFF;
      }
      case MAC_FRAME_DESTINATION_MODE_LONG:
        return sli_zigbee_stack_is_local_eui64(&macHeader[5]);
      case MAC_FRAME_DESTINATION_MODE_NONE:
        return (amCoordinator);   // these are allowed if we are the coordinator
      default:
        return false;
    }
  }
}

//----------------------------------------------------------------
// CCA checks go up to the radio simulator unless we are about to transmit
// something, in which case we know the answer ourselves.

static uint8_t (*radioGetCCAHandler)(void) = NULL;

void setRadioGetCCAHandler(uint8_t (*handler)(void))
{
  radioGetCCAHandler = handler;
}

void simulatorSuspendRadioActivity(void)
{
  radioActivitySuspended = true;
}

void simulatorResumeRadioActivity(void)
{
  radioActivitySuspended = false;
}

uint8_t emRadioGetCca(void)
{
  if (phySimState != IDLE || radioActivitySuspended) {
    return false;
  } else if (radioGetCCAHandler != NULL) {
    return radioGetCCAHandler();
  } else {
    return true;
  }
}

#ifdef SIMULATED_TRANSCEIVER
//To send phy-ack once framePending status receive from mcu
void sendPhyAck(bool ackFramePending, uint8_t sequenceNumber)
{
  uint16_t ackFrameControl =
    MAC_FRAME_TYPE_ACK
    | (ackFramePending ? MAC_FRAME_FLAG_FRAME_PENDING : 0);

  assert(phySimState == WAITING_FOR_FRAMEPENDING);

  if (radioTransmitHandler != NULL) {
    uint8_t ackBuff[] = { 5,         // number of following bytes
                          LOW_BYTE(ackFrameControl),
                          HIGH_BYTE(ackFrameControl),
                          sequenceNumber,
                          0,       // dummy CRC
                          0        // dummy CRC
    };
    radioTransmitHandler(ackBuff);
  }
  phySimState = ACK_TX;
}

#endif  //SIMULATED_TRANSCEIVER

// Used for energy scans.  We don't actually model multiple channels so
// there would be little point in providing different noise levels.

uint8_t emReadNoiseLevel(uint8_t count)
{
  (void) count;
  return 17;
}

void radioUpdateNoiseAverage(void)
{
}

int8_t sli_802154phy_radio_energy_detection(void)
{
  return energyLevels[emRadioChannel - SL_ZIGBEE_MIN_802_15_4_CHANNEL_NUMBER];
}

bool sli_simulator_set_energy_level(uint8_t channelpage, uint8_t channel, uint8_t level)
{
  (void)channelpage;   //Todo:will be using this later
  if ( channel < SL_ZIGBEE_MIN_802_15_4_CHANNEL_NUMBER
       || channel > SL_ZIGBEE_MAX_802_15_4_CHANNEL_NUMBER) {
    return false;
  }

  energyLevels[channel - SL_ZIGBEE_MIN_802_15_4_CHANNEL_NUMBER] = level;
  return true;
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

sl_zigbee_event_control_t emCalPadsEvent;
void emCalPadsEventHandler(void)
{
}

uint32_t emPhyGetChannelFreqHz(uint8_t macPgChan)
{
  (void) macPgChan;
  return 0;
}

uint8_t sli_802154phy_get_phy_channel(uint8_t c)
{
#if PHY_HAS_DUTY_CYCLE
  if (mapMacPgChanToPhyChanBand(c, NULL, NULL, NULL) != NULL) {
    uint8_t phyChan = sli_802154mac_pg_chan_ch(c);
    if (phyChan < MAX_CHANNELS_PER_PAGE) {
      uint8_t macPage = sli_802154phy_get_channel_page_for_channel(c);
      switch (macPage) {
        case 0:
          if (phyChan >= SL_ZIGBEE_MIN_802_15_4_CHANNEL_NUMBER) {
            return phyChan;
          }
          break;
       #if     (MAC_HAS_CHANNEL_PAGES && IEEE802154_GB868_SUPPORTED)
        case 29:   // 27..34,62
          phyChan += 27;
          if (phyChan == 35) {
            phyChan = 62;
          }
          SL_FALLTHROUGH
        case 28:   //  0..26 in 863 MHz band
        case 31:   //  0..26 in 915 MHz band
          return phyChan;
          break;
        case 30:   // 35..61
          return (phyChan + 35);
          break;
       #endif//(MAC_HAS_CHANNEL_PAGES && IEEE802154_GB868_SUPPORTED)
        default:
          break;
      }
    }
  }
  return INVALID_CHANNEL;
#else // !PHY_HAS_DUTY_CYCLE
  uint8_t phyChan = sli_802154mac_pg_chan_ch(c);
  if (phyChan >= SL_ZIGBEE_MIN_802_15_4_CHANNEL_NUMBER) {
    return phyChan;
  } else {
    return INVALID_CHANNEL;
  }
#endif  // PHY_HAS_DUTY_CYCLE
}

// EMPIPSTACK-336 - Neuter events that couldn't have fired on silicon
bool halSimulatorCheckMacTimerIntEnabled(void)
{
  // EMIPSTACK-336 -- if INT_MACTMR is off then timer event would never occur
  // This is only called when a simulated timer expiry occurs.
  bool macTimerIntEnabled = !!(INT_CFG & INT_MACTMR);
  if (!macTimerIntEnabled) {
    simPrint("Symbol Delay event cannot fire with INT_MACTMR off");
  }
  return macTimerIntEnabled;
}
