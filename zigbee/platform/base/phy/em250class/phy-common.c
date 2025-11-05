/***************************************************************************//**
 * @file
 * @brief Implementation of radio, baseband, and MAC functions common to
 *              all em250-class devices (e.g. em2xx, em3xx).
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

#include "micro/unix/simulation/em250/pcb.h"
#include "framework/eui64.h"

#include "phy/phy.h"
#include "phy/security.h"
#include "stack/platform/micro/aes.h"
 #include "stack/mac/command.h"
  #include "micro/unix/simulation/em250/regs.h"
  #include "micro/unix/simulation/em250/regs-indirect-analog.h"
  #include "phy/plugin/duty-cycle/duty-cycle.h" // For emPhyDcTick()

#undef  halPtaStackEvent
#define halPtaStackEvent(ptaStackEvent, supplement) (0u)

// This block is required for LBT and duty cycle support in simulation
#if defined(PHY_SIMULATION_DUAL) || defined(PHY_SIMULATION_GB)
// Retain the least-significant bits from Page
#define macPage2Pg(macPage)      ((macPage) & (BIT(8 - CHANNEL_BITS) - 1))
const uint8_t emPhyCcaLbtConfigArray[]  = { 0 }; // Dummy to distinguish CSMA from LBT
EmPhyBandConfig* emPhyCurrentBandConfig = NULL;
#endif  //PHY_SIMULATION_DUAL || PHY_SIMULATION_GB

bool sli_802154mac_ack_needed;

// from phy/em250/phy.c:
extern uint8_t emRadioChannel;
extern int8_t emRadioPower;
extern uint16_t emRadioTxPowerMode;

extern int8_t emSynthFreqOffset;

bool radioCheckCrc = true;
uint8_t sli_legacy_mfglib_mode = 0;
volatile bool sli_legacy_mfglib_transmit_complete = false;

static sl_802154_short_addr_t emLocalRadioNodeId = EM_USE_LONG_ADDRESS;
static sl_802154_pan_id_t emLocalRadioPanId = EM_BROADCAST_PAN_ID;

static bool ptiEnabled = true;
static bool deviceIsCoordinator = false;
static bool addressMatchingEnabled = true;
static bool autoAckEnabled = true;

// The 2xx always starts with the default value.
static int8_t edCcaThreshold = ED_CCA_THRESHOLD;

static bool emRadioOverflowCallbackEnabled = false;
static bool emRadioSfdSentCallbackEnabled = false;

static RadioPowerMode radioIdlePowerState = SL_ZIGBEE_RADIO_POWER_MODE_OFF;

bool emRadioAlwaysUseZeroBackoff = false;
uint8_t emRadioLastOutgoingCommand = 0xff;
RadioPowerMode radioCurrentPowerStatus = SL_ZIGBEE_RADIO_POWER_MODE_OFF;
bool emRadioTransmitInProgress = false;
uint32_t lastTxSfdTime;

void printRadioState(const char* function)
{
// This is turned off by default since it is very verbose.
//#define SL_ZIGBEE_TEST_DEBUG_PRINT_POWER_STATUS

#if defined(SL_ZIGBEE_TEST_DEBUG_PRINT_POWER_STATUS)
//  if (simulatorId == 4) {
  simPrint("%s: radioCurrentPowerStatus: %s, MAC_CONFIG: %s",
           function,
           (radioCurrentPowerStatus == SL_ZIGBEE_RADIO_POWER_MODE_RX_ON
            ? "ON"
            : "OFF"),
           (MAC_CONFIG & MAC_CONFIG_MAC_MODE
            ? "ON"
            : "OFF"));
// }
#else
  UNUSED_VAR(function);
#endif
}

//------------------------------------------------------------------------------
// LQI APIs
//---------

static const uint8_t sli_802154phy_cost_divisions[] = {
  //      cost   255-LQI chipErrors/byte   chip error rate  packet error rate
  //      ----   ------- ---------------   ---------------  -----------------
  254, // 1      1       0.016             0.02%            ?
  247, // 3      8       0.125             0.20%            ~ 0.005%
  200, // 5      55      0.859             1.34%            ~ 0.02%
  1    // 7
};

uint8_t sli_802154phy_cost_division(uint8_t cost)
{
  return sli_802154phy_cost_divisions[cost >> 1];
}

// In the em250, LQI is maintained as a 10-bit quantity.  It gets stuffed into
// an 8-bit quantity according to the following rules:
// Values less than 128: the lower 7 bits of the internal 10-bit representation
// appear in the lower 7 bits of the 8-bit quantity, and the msb of the 8-bit
// quantity is clear.
// Values 128 or greater: the upper 7 bits of the internal 10-bit representation
// appear in the lower 7 bits of the 8-bit quantity, and the msb of the 8-bit
// quantity is set.
#define LQI_SCALING_BIT 0x80

// internalChipErrors is the scaled 8-bit quantity straight from the em250 MAC
// hardware.
uint16_t sli_802154phy_radio_unpack_chip_errors(uint8_t internalChipErrors)
{
  uint16_t unpackedChipErrors = (uint16_t)internalChipErrors;

  // Scale values of 128 or greater.
  if (internalChipErrors & LQI_SCALING_BIT) {
    unpackedChipErrors = (unpackedChipErrors & ~LQI_SCALING_BIT) << 3;
  }
  return unpackedChipErrors;
}

// packetLength is the PHY payload length from the PHR
uint8_t sli_802154phy_radio_calculate_link_quality(uint16_t unpackedChipErrors,
                                                   uint16_t packetLength)
{
  uint32_t errorsPerByteX64;

  // Convert total chip errors in packet (err/packet) to chip errors per byte
  // (err/byte).
  // Scale chipErrors up so that we lose less precision in division.
  // We don't care about packets that have more than 4 chip errors per byte
  // (6.25% chip error rate), so we scale up by a factor of 256/4 = 64.
  errorsPerByteX64 = (uint32_t)unpackedChipErrors << 6;
  // Note that the total chip errors in packet includes those that occur in
  // the PHY header (PHR) as well as PHY payload (packetLength), so add the
  // PHR size to the division's denominator.
  errorsPerByteX64 = errorsPerByteX64 / (packetLength + SL_ZIGBEE_PHY_MAX_PHR_BYTES);

  // 15.4 wants 0xFF to represent the best link quality, so we flip it.
  return 0xFF - (uint8_t)((errorsPerByteX64 > 0xFF)
                          ? 0xFF
                          : errorsPerByteX64);
}

#define PHY_MIN_RSSI -100
#define PHY_MIN_RSSI_ED (PHY_MIN_RSSI + 10) // 15.4: 10 dB above sensitivity
#define PHY_MAX_RSSI -36
#define PHY_MAX_ED   0xff

// No casts in this macro so it can be used for constant expressions
// Here rssi must be >= PHY_MIN_RSSI_ED and < PHY_MAX_RSSI for a valid result
#define RSSI_TO_ED(rssi) ((((rssi) - PHY_MIN_RSSI_ED) * 256) \
                          / (PHY_MAX_RSSI - PHY_MIN_RSSI_ED))

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

//------------------------------------------------------------------------------
// Transmit APIs
//--------------

uint16_t emGetTxPowerMode(void)
{
  return emRadioTxPowerMode;
}

//------------------------------------------------------------------------------
// Power State APIs
//-----------------

void emRadioEnablePta(bool enable)
{
  UNUSED_VAR(enable);
}

void sli_802154phy_radio_init(RadioPowerMode initialRadioPowerMode)
{
  emStartRadioSetChannel();

  IN_LOCK_EN_REG = 0;

  MAC_TIMER_CTRL_REG = MAC_TIMER_CTRL_MAC_TIMER_EN_MASK
                       | MAC_TIMER_CTRL_BO_TIMER_EN_MASK
                       | MAC_TIMER_CTRL_BOP_TIMER_EN_MASK;

  MIXER_GAIN_STEP = 9;
  DITHER_AMPLITUDE_REG = 31;
  CCA_MODE = CCA_MODE_ED; // Other modes don't work on em250class devices.

  emApiSetTxPowerMode(emGetTxPowerMode());
  emRadioBootPhySpecific();

  sli_zigbee_init_eui64();
  sli_802154phy_set_eui64();
  sli_802154phy_radio_set_node_id(sli_802154mac_radio_get_node_id());
  sli_802154phy_radio_set_pan_id(sli_802154phy_radio_get_pan_id());
  {
    DECLARE_INTERRUPT_STATE;
    DISABLE_INTERRUPTS(); // Protect against emRadioHoldOffIsr() while changing MAC_RX_CONFIG
    MAC_RX_CONFIG |= MAC_RX_CONFIG_APPEND_INFO;
    RESTORE_INTERRUPTS();
  }
  emRadioEnableAddressMatching(addressMatchingEnabled);
  sli_802154mac_radio_enable_auto_ack(autoAckEnabled);
  sli_802154phy_set_coordinator(deviceIsCoordinator);
  sli_802154phy_set_phy_radio_power(emRadioPower);
  emRadioEnablePacketTrace(ptiEnabled);

  emRadioSetEdCcaThreshold(edCcaThreshold);

  emRadioEnableReceiveCrc(radioCheckCrc);

  assert(SL_STATUS_OK == emFinishRadioSetChannel(emRadioChannel));

  if ((SL_ZIGBEE_RADIO_POWER_MODE_OFF == initialRadioPowerMode)
      && (MAC_CONFIG & MAC_CONFIG_MAC_MODE)) {
    // The requested initial power mode is "off" but sli_802154phy_set_phy_radio_channel() has
    // turned the radio on.  Turn it off.
    sli_802154phy_radio_sleep();
    emRadioPowerFem(false);
    (void) halPtaStackEvent(PTA_STACK_EVENT_TX_IDLED, 0U);
  }

  if ((SL_ZIGBEE_RADIO_POWER_MODE_RX_ON == initialRadioPowerMode)
      && (!(MAC_CONFIG & MAC_CONFIG_MAC_MODE))) {
    // The requested initial power mode is "on" but sli_802154phy_set_phy_radio_channel() has
    // not turned the radio on.  Turn it on.

    MAC_CONFIG |= MAC_CONFIG_MAC_MODE;
    // TODO: Optimize this delay.
    halCommonDelayMicroseconds(200);
    radioCurrentPowerStatus = SL_ZIGBEE_RADIO_POWER_MODE_RX_ON;
    printRadioState(__PRETTY_FUNCTION__);
  }

  // Enable MAC Interrupts.
  MAC_TX_INT_MASK_REG = MAC_TX_INTS;
  MAC_RX_INT_MASK_REG = MAC_RX_INTS;

  #ifndef SL_ZIGBEE_TEST
  emRadioEnablePta(halPtaIsEnabled());
  #endif

  emRadioResetAndEnableReceive();

#if     PHY_DUAL
  MAC_TIMER_INT_ON;
#endif//PHY_DUAL

  if (MAC_CONFIG & MAC_CONFIG_MAC_MODE) {
    // TODO: TX_LOCK_FAIL is initially active for some reason on the 250.
    // TODO: Consider calling sli_802154phy_radio_wake_up here.
    CLEAR_INT(MAC_TX_INT_SRC, 0xffffu);
    MAC_INTERRUPTS_ON;
  }
}

void sli_802154phy_radio_sleep(void)
{
  DECLARE_INTERRUPT_STATE;

  if (SL_ZIGBEE_RADIO_POWER_MODE_OFF == radioCurrentPowerStatus) {
    return;
  }

  // Before sleeping the radio, we need to make sure that we are not receiving
  // an SFD.  If we sleep the radio during the SFD, the sfd_found signal in the
  // baseband may get stuck high, causing subsequent CCA readings to appear
  // busy even when the channel is clear.  We have seen this behavior in bugzid:
  // 11043.

  DISABLE_INTERRUPTS();

  MAC_INTERRUPTS_OFF;

  // Power down the radio.
  MAC_CONFIG &= ~MAC_CONFIG_MAC_MODE;
  // Stop any transmit in progress.  Interrupt may still occur later.
  MAC_TX_STROBE &= ~MAC_TX_STROBE_START_TX;
  // Stop all other transmit functions.
  MAC_TX_STROBE |= MAC_TX_STROBE_MAC_TX_RST;
  // Reset RX_TOP_SM, which resets RX_BUUFER_SM, which cancels DMA writes.
  MAC_RX_CONFIG |= MAC_RX_CONFIG_MAC_RX_RST;

  radioCurrentPowerStatus = SL_ZIGBEE_RADIO_POWER_MODE_OFF;
  printRadioState(__PRETTY_FUNCTION__);

  // There is a chance that some TX interrupts will fire after ramp down.
  // Since the top level TX interrupt is off we won't service these interrupts.
  // There won't be any problem as long as:
  // 1. We don't wake before ramp down completes.
  // 2. We clear second-level TX interrupts before enabling top-level on wake.

  RESTORE_INTERRUPTS();
}

void sli_802154phy_radio_wake_up(void)
{
  if (SL_ZIGBEE_RADIO_POWER_MODE_RX_ON == radioCurrentPowerStatus) {
    return;
  }

  {
    DECLARE_INTERRUPT_STATE;
    DISABLE_INTERRUPTS();
    // Enable modulation
    MOD_EN = 1;
    // Power up the radio.
    MAC_CONFIG |= MAC_CONFIG_MAC_MODE;
    radioCurrentPowerStatus = SL_ZIGBEE_RADIO_POWER_MODE_RX_ON;
    printRadioState(__PRETTY_FUNCTION__);

    // Reset Rx.
    emRadioResetAndEnableReceive();
    // Reset Tx.
    SET_MAC_DMA_CONFIG(MAC_DMA_CONFIG_TX_DMA_RESET);
    MAC_TX_STROBE |= MAC_TX_STROBE_MAC_TX_RST;
    MAC_TIMER_CTRL |= MAC_TIMER_CTRL_BO_TIMER_RST;
    RESTORE_INTERRUPTS();
  }
  // Delay to allow spurious interrupts to fire during startup.
  halCommonDelayMicroseconds(10);
  // Clear spurious interrupts, including TX_LOCK_FAIL (only on 250).
  CLEAR_INT(MAC_TX_INT_SRC, 0xffffu);
  MAC_INTERRUPTS_ON;
}

RadioPowerMode sli_802154phy_radio_get_power_status(void)
{
  return radioCurrentPowerStatus;
}

void sli_802154phy_radio_ok_to_idle(void)
{
  printRadioState(__PRETTY_FUNCTION__);

  // this will be reenabled when Don's pull request #18 is approved
  // assert(radioCurrentPowerStatus == SL_ZIGBEE_RADIO_POWER_MODE_RX_ON);

  if (SL_ZIGBEE_RADIO_POWER_MODE_OFF == radioIdlePowerState) {
    sli_802154phy_radio_sleep();
    (void) halPtaStackEvent(PTA_STACK_EVENT_TX_IDLED, 0U);
  }
}

RadioPowerMode sli_802154phy_radio_get_idle_mode(void)
{
  return radioIdlePowerState;
}

sl_status_t sli_802154phy_radio_set_idle_mode(RadioPowerMode mode)
{
  if (mode > SL_ZIGBEE_RADIO_POWER_MODE_OFF) {
    return SL_STATUS_FAIL;
  }

  radioIdlePowerState = mode;

  printRadioState("sli_802154phy_radio_set_idle_mode() begin");

  // Put the radio into the new idle power mode.
  if (radioIdlePowerState != radioCurrentPowerStatus) {
    if (radioIdlePowerState == SL_ZIGBEE_RADIO_POWER_MODE_RX_ON) {
      emRadioPowerFem(true);
      sli_802154phy_radio_wake_up();
    } else { /* if (radioIdlePowerState == SL_ZIGBEE_RADIO_POWER_MODE_OFF) */
      sli_802154phy_radio_sleep();
      emRadioPowerFem(false);
      (void) halPtaStackEvent(PTA_STACK_EVENT_TX_IDLED, 0U);
    }
  }

  printRadioState("sli_802154phy_radio_set_idle_mode() end");

  return SL_STATUS_OK;
}

//------------------------------------------------------------------------------
// Address and Filtering APIs
//---------------------------

void emRadioEnableAddressMatching(uint8_t enable)
{
  {
    DECLARE_INTERRUPT_STATE;
    DISABLE_INTERRUPTS(); // Protect against emRadioHoldOffIsr() while changing MAC_RX_CONFIG
    if (enable) {
      MAC_RX_CONFIG |= (MAC_RX_CONFIG_FILT_ADDR_ON
                        | MAC_RX_CONFIG_FILT_FORMAT_ON);
    } else {
      MAC_RX_CONFIG &= ~(MAC_RX_CONFIG_FILT_ADDR_ON
                         | MAC_RX_CONFIG_FILT_FORMAT_ON);
    }
    addressMatchingEnabled = !!enable;
    RESTORE_INTERRUPTS();
  }
}

uint8_t emRadioAddressMatchingEnabled(void)
{
  return addressMatchingEnabled;
}

// When beacons are implemented, the em250 class mac will have to disable
// auto-acking to avoid BugzId 3237 (non cca tx conflicts with auto-ack).
// At that time we will have to be careful that the stack does not break
// this workaround.

void sli_802154mac_radio_enable_auto_ack(bool enable)
{
  if (enable) {
    MAC_RX_CONFIG_REG |= MAC_RX_CONFIG_AUTO_ACK_MASK;
  } else {
    MAC_RX_CONFIG_REG &= ~MAC_RX_CONFIG_AUTO_ACK_MASK;
  }
  autoAckEnabled = enable;
}

bool sli_802154phy_radio_auto_ack_enabled(void)
{
  return autoAckEnabled;
}

void sli_802154phy_set_eui64(void)
{
  uint8_t * eui = sli_zigbee_stack_get_eui64();

  // eui64 is stored as a little-endian 8-byte array,
  // the same byte order as over the air transmission.

  EXT_ADDR_0_REG = HIGH_LOW_TO_INT(eui[1], eui[0]);
  EXT_ADDR_1_REG = HIGH_LOW_TO_INT(eui[3], eui[2]);
  EXT_ADDR_2_REG = HIGH_LOW_TO_INT(eui[5], eui[4]);
  EXT_ADDR_3_REG = HIGH_LOW_TO_INT(eui[7], eui[6]);
}

void sli_802154phy_set_coordinator(bool coordinator)
{
  {
    DECLARE_INTERRUPT_STATE;
    DISABLE_INTERRUPTS(); // Protect against emRadioHoldOffIsr() while changing MAC_RX_CONFIG
    if (coordinator) {
      MAC_RX_CONFIG_REG |= MAC_RX_CONFIG_COORDINATOR_MASK;
    } else {
      MAC_RX_CONFIG_REG &= ~MAC_RX_CONFIG_COORDINATOR_MASK;
    }
    deviceIsCoordinator = coordinator;
    RESTORE_INTERRUPTS();
  }
}

bool emPhyDeviceIsCoordinator(void)
{
  return deviceIsCoordinator;
}

sl_802154_short_addr_t sli_802154mac_radio_get_node_id(void)
{
  return emLocalRadioNodeId;
}

void sli_802154phy_radio_set_node_id(sl_802154_short_addr_t nodeId)
{
  emLocalRadioNodeId = nodeId;
  SHORT_ADDR_REG = nodeId;
}

sl_802154_pan_id_t sli_802154phy_radio_get_pan_id(void)
{
  return emLocalRadioPanId;
}

void sli_802154phy_radio_set_pan_id(sl_802154_pan_id_t panId)
{
  emLocalRadioPanId = panId;
  PAN_ID_REG = panId;
}

void emRadioEnableReceiveCrc(bool enable)
{
  radioCheckCrc = enable;
}

bool emRadioReceiveCrcEnabled(void)
{
  return radioCheckCrc;
}

//------------------------------------------------------------------------------
// Miscellaneous APIs
//-------------------

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
  if (enable) {
    MAC_TX_INT_SRC = MAC_TX_INT_SRC_SFD_SENT_SRC;
    MAC_TX_INT_MASK |= MAC_TX_INT_MASK_SFD_SENT_MSK;
  } else {
    MAC_TX_INT_MASK &= ~MAC_TX_INT_MASK_SFD_SENT_MSK;
  }
  emRadioSfdSentCallbackEnabled = enable;
}

bool emRadioSfdSentNotificationEnabled(void)
{
  return emRadioSfdSentCallbackEnabled;
}

void emRadioEnablePacketTrace(bool enable)
{
  if (enable) {
    MAC_CONFIG |= MAC_CONFIG_SPI_SPY_EN;
  } else {
    MAC_CONFIG &= ~MAC_CONFIG_SPI_SPY_EN;
  }
  ptiEnabled = enable;
}

bool sli_802154phy_radio_packet_trace_enabled(void)
{
  return ptiEnabled;
}

void emRadioSetEdCcaThreshold(int8_t threshold)
{
  // The threshold parameter is expected to be a negative dBm value.
  RSSI_THRESH = (uint16_t) (((uint16_t)(threshold - RSSI_DBM_OFFSET_EM250CLASS)) << 6);
  edCcaThreshold = threshold;
}

int8_t emRadioGetEdCcaThreshold(void)
{
  return edCcaThreshold;
}

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

#if     PHY_DUAL
uint32_t emPhySymbolsToUs(uint32_t symbols)
{
  return SL_ZIGBEE_PHY_SYMBOLS_TO_US(symbols);
}
#endif//PHY_DUAL

void emPhyTick(bool isrContext)
{
  if (!isrContext) {
    emPhyDcTick();
  }
}

#if PHY_HAS_DUTY_CYCLE
EmPhyBandConfig* mapMacPgChanToPhyChanBand(uint8_t macPgChan,
                                           uint8_t* phyChanPtr,
                                           uint8_t* macPagePtr,
                                           EmPhySubBandConfig** subBandConfigPtr)
{
  // Here we assume macPgChan has already been mapped
  // Loop thru the bands and their subBandConfigs looking for a channel mapping
  uint8_t band, txpl;
  uint8_t macPg = sli_802154mac_pg_chan_pg(macPgChan);
  uint8_t macCh = sli_802154mac_pg_chan_ch(macPgChan);
  for (band = 0; band < emPhyConfigEfr.numBands; band++) {
    EmPhyBandConfig* bandConfig = (EmPhyBandConfig*) emPhyConfigEfr.bandConfigs[band];
    for (txpl = 0; txpl < bandConfig->numSubBandConfigs; txpl++) {
      EmPhySubBandConfig* macMap = (EmPhySubBandConfig*) &bandConfig->subBandConfigs[txpl];
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

sl_status_t emPhyGetDutyCycleParams(uint8_t macPgChan,
                                    EmPhyDutyCycleParams* dutyCycleParams)
{
  // Quickly reject an illegal channel#
  if (sli_802154mac_pg_chan_ch(macPgChan) >= MAX_CHANNELS_PER_PAGE) {
    return SL_STATUS_TRANSMIT_INVALID_CHANNEL;
  }
  EmPhySubBandConfig* subBandConfig = NULL;
  EmPhyBandConfig* bandConfig = mapMacPgChanToPhyChanBand(macPgChan, NULL,
                                                          NULL, &subBandConfig);
  if ((bandConfig == NULL) || (subBandConfig == NULL)) {
    return SL_STATUS_TRANSMIT_INVALID_CHANNEL;
  }
  // Purposefully shadow global EM_PHY_CURRENT_BAND_CONFIG for below
  // calculations using SL_ZIGBEE_PHY_SYMBOLS_TO_US() so they apply to
  // bandConfig and not the global currently-active bandConfig.
  #undef  EM_PHY_CURRENT_BAND_CONFIG
  #define EM_PHY_CURRENT_BAND_CONFIG bandConfig
  //PHY_ASSERT(dutyCycleParams != NULL);
  dutyCycleParams->lbtRdcDenom = subBandConfig->lbtDutyDenom;
  dutyCycleParams->criRdcDenom = subBandConfig->criDutyDenom;
  dutyCycleParams->nonRdcDenom10 = subBandConfig->nonDutyDenom10;
  dutyCycleParams->byteTimeUs = (
    ((1000000ul /*us/sec*/ * SL_ZIGBEE_PHY_BYTE_BITS)
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

static void emRadioHoldOffInternalIsr(uint8_t active)
{
  if (active) {
    // Suspend ability of the radio to transmit
    // This is done by configuring radio such that subsequent
    // CCA attempts fail and the automatic generation of ACKs
    // is disabled.
    // We do this directly in radio registers behind the backs
    // of emRadioSetEdCcaThreshold() and sli_802154mac_radio_enable_auto_ack()
    // so the shadow state which those routines maintain in RAM
    // preseve the saved state to restore once Radio HoldOff is
    // done.
    emRadioSetEdCcaThreshold(-128);
    sli_802154mac_radio_enable_auto_ack(false);
    simulatorSuspendRadioActivity();
  } else {
    // Restore pre-Radio-HoldOff settings (which stack might have
    // changed while Radio HoldOff was active) using the shadow
    // copies the stack maintains in RAM.
    emRadioSetEdCcaThreshold(edCcaThreshold);
    sli_802154mac_radio_enable_auto_ack(true);
    simulatorResumeRadioActivity();
  }
}

void emRadioHoldOffInternal(bool active)
{
  emRadioHoldOffInternalIsr((uint8_t)active);
}

#endif  //PHY_HAS_DUTY_CYCLE

// Unsupported on this platform
sl_status_t sli_802154_stack_radio_get_scheduler_priorities(sl_802154_radio_priorities_t *priorities)
{
  (void) priorities;
  return SL_STATUS_INVALID_STATE;
}

sl_status_t sli_802154_stack_radio_set_scheduler_priorities(const sl_802154_radio_priorities_t *priorities)
{
  (void) priorities;
  return SL_STATUS_INVALID_STATE;
}

sl_status_t sli_802154_stack_radio_set_scheduler_sliptime(uint32_t slipTime)
{
  (void) slipTime;
  return SL_STATUS_INVALID_STATE;
}

sl_status_t sli_802154_stack_radio_get_scheduler_sliptime(uint32_t *slipTime)
{
  (void) slipTime;
  return SL_STATUS_INVALID_STATE;
}
