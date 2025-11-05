/***************************************************************************//**
 * @file
 * @brief Implementation of radio functions and interface to simulated
 * transceiver.
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
#include PLATFORM_HEADER
#include "include/sl_zigbee.h"
#include "phy/dual/phy.h"
#include "core/sl_zigbee_stack.h"
#include "stack/include/sl_zigbee_types.h"
#include "stack/framework/eui64.h"    //for sli_zigbee_init_eui64
#include "tool/simulator/child/message.h"
#include "phy/phy.h"
#include "indirect-queue.h" // for sli_mac_long_id_data_pending
#include "mac-child.h"      // for sl_mac_child_*()
#include "app/test/transceiver/transceiver-simulator.h"
#include "hal/micro/random.h"
#include "routing/zigbee/child.h"
#include "core/sl_zigbee_multi_network.h"
#include "hal/hal.h"
#include "phy/plugin/duty-cycle/duty-cycle.h" // For emPhyDcTick()
#include "sl_common.h" // for SL_FALLTHROUGH

//hardware-mac-sim
uint16_t emRadioTxPowerMode = SL_ZIGBEE_TX_POWER_MODE_USE_TOKEN;
int8_t emSynthFreqOffset;
int8_t emRadioPower = MAX_RADIO_POWER;
uint8_t emRadioChannel = SL_ZIGBEE_MIN_SUBGHZ_CHANNEL_NUMBER; //todo: add support for channel page
#define DEFAULT_ENERGY_LEVEL 0
static uint8_t energyLevels[SL_ZIGBEE_NUM_802_15_4_CHANNELS];

bool ccaReceivedFlag = false;
uint8_t ccaValue = false;

#ifndef PHY_SIMULATION_DUAL
//hardware-mac-sim
bool inIsrContext = false;
//plugin/duty-cycle/duty-cycle.c
#if defined(MAC_TEST_STACK)
sl_zigbee_event_control_t emPhyDcTxEvent;
#endif
uint8_t sli_legacy_mfglib_mode = 0;
#else  //PHY_SIMULATION_DUAL
//pro2plus/phy.c
extern uint8_t sli_legacy_mfglib_mode;
#endif  //PHY_SIMULATION_DUAL

//pro2plus/phy.c
bool emRadioAlwaysUseZeroBackoff = false;
volatile bool sli_legacy_mfglib_transmit_complete = false;
//TODO: Initialize these to a time in the distant past so lbtMinOffTime
//TODO: is not applied on first transmit after bootup?
static volatile uint32_t lastTxFinishedTimeMs;
static volatile uint32_t lastCsmaTxFinishedTimeMs;
const uint8_t emPhyCcaLbtConfigArray[] = {
  //TODO: Fill this in with LBT configuration switch
  0x00
};
EmPhyBandConfig* emPhyCurrentBandConfig = NULL;

//em250class/phy-common.c
static sl_802154_short_addr_t emLocalRadioNodeId = EM_USE_LONG_ADDRESS;
static sl_802154_pan_id_t emLocalRadioPanId = EM_BROADCAST_PAN_ID;
static RadioPowerMode radioIdlePowerState = SL_ZIGBEE_RADIO_POWER_MODE_OFF;

static bool addressMatchingEnabled = true;
static bool deviceIsCoordinator = false;
static bool autoAckEnabled = true;
static int8_t edCcaThreshold = ED_CCA_THRESHOLD;

uint8_t emRadioLastOutgoingCommand = 0xff;  // To state MAC layer; not used herein

bool radioCheckCrc = true;

RadioPowerMode radioCurrentPowerStatus = SL_ZIGBEE_RADIO_POWER_MODE_OFF;

//defined in spi-protocol-trx-mcu.c
extern void incomingTransceiverCommandHandler(uint8_t *contents, uint8_t length);

static void processRxComplete(uint8_t* packet);
static inline bool emberRadioDataPendingShortIdIsrCallback(uint16_t shortId);
static inline bool emberRadioDataPendingLongIdIsrCallback(uint8_t* longId);

#define sli_mac_get_current_radio_network_index(...) 0
#define processTransceiverToMcuCommand processIncomingCommands

// Retain the least-significant bits from Page
#define macPage2Pg(macPage)      ((macPage) & (BIT(8 - CHANNEL_BITS) - 1))

// For pages other than 0, set the most-significant bits dropped in Pg encoding
// E.g. 3-bit Pgs 1-7 map to 5-bit Pages 24-31.
#define macPg2Page(macPg)        ( ((macPg) == 0) ? 0                                         \
                                   : ((macPg) | ((((1ul << (32 - MAX_CHANNELS_PER_PAGE)) - 1) \
                                                  >> (8 - CHANNEL_BITS)) << (8 - CHANNEL_BITS))) )

#define simPrint(...) //comment this to enable simPrints
#define printBuffer(...)

//em250class/phy-common.c
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

uint8_t sli_802154phy_radio_calculate_link_quality(uint16_t unpackedChipErrors, uint16_t packetLength)
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

//--------

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
  simPrint("sli_802154phy_radio_set_idle_mode() begin");
  // Put the radio into the new idle power mode.
  if (radioIdlePowerState != radioCurrentPowerStatus) {
    if (radioIdlePowerState == SL_ZIGBEE_RADIO_POWER_MODE_RX_ON) {
      emRadioPowerFem(true);
      sli_802154phy_radio_wake_up();
    } else { /* if (radioIdlePowerState == SL_ZIGBEE_RADIO_POWER_MODE_OFF) */
      sli_802154phy_radio_sleep();
      emRadioPowerFem(false);
    }
  }
  simPrint("sli_802154phy_radio_set_idle_mode() end");
  return SL_STATUS_OK;
}

void sli_802154phy_radio_ok_to_idle(void)
{
  assert(radioCurrentPowerStatus == SL_ZIGBEE_RADIO_POWER_MODE_RX_ON);
  if (SL_ZIGBEE_RADIO_POWER_MODE_OFF == radioIdlePowerState) {
    sli_802154phy_radio_sleep();
  }
}

//simulated-TRX does not require ccathreshold
//java side of simulator calculates CCA CLEAR or BUSY
void emRadioSetEdCcaThreshold(int8_t threshold)
{
  edCcaThreshold = threshold;
}

//crc is enabled by default
void emRadioEnableReceiveCrc(bool enable)
{
  radioCheckCrc = enable;
}

bool emRadioReceiveCrcEnabled(void)
{
  return radioCheckCrc;
}

void sli_802154phy_radio_init(RadioPowerMode initialRadioPowerMode)
{
  //Update stack network config variables as well as configure transceiver
  //with initial required parameters
  sli_zigbee_init_eui64();
  sli_802154phy_set_eui64();
  emApiSetTxPowerMode(emGetTxPowerMode());
  sli_802154phy_set_phy_radio_power(emRadioPower);
  sli_802154phy_set_coordinator(emPhyDeviceIsCoordinator());
  emRadioEnableAddressMatching(addressMatchingEnabled);
  sli_802154mac_radio_enable_auto_ack(autoAckEnabled);
  sli_802154phy_set_phy_radio_channel(sli_802154phy_get_phy_radio_channel());
  sli_802154phy_radio_set_node_id(sli_802154mac_radio_get_node_id());
  sli_802154phy_radio_set_pan_id(sli_802154phy_radio_get_pan_id());
  emRadioSetEdCcaThreshold(edCcaThreshold);
  emRadioEnableReceiveCrc(radioCheckCrc);
  memset(energyLevels, DEFAULT_ENERGY_LEVEL, SL_ZIGBEE_NUM_802_15_4_CHANNELS);
  setTransceiverSerialHandler(incomingTransceiverCommandHandler);

  if (initialRadioPowerMode == SL_ZIGBEE_RADIO_POWER_MODE_OFF) {
    sli_802154phy_radio_sleep();
  } else {
    sli_802154phy_radio_wake_up();
  }
}

bool sli_802154phy_radio_check_radio(void)
{
  return false;
}

void sli_802154mac_radio_enable_auto_ack(bool enable)
{
  autoAckEnabled = enable;
}

bool sli_802154phy_radio_auto_ack_enabled(void)
{
  return autoAckEnabled;
}

uint16_t emGetTxPowerMode(void)
{
  return emRadioTxPowerMode;
}

void emRadioEnableAddressMatching(uint8_t enable)
{
  addressMatchingEnabled = enable;
}

uint8_t emRadioAddressMatchingEnabled(void)
{
  return addressMatchingEnabled;
}

void emPhyTick(bool isrContext)
{
  if (!isrContext) {
    emPhyDcTick();
  }
}

//em250class/analogue.c
void sl_802154_calibrate_current_channel(void)
{
  //sli_802154phy_radio_check_radio() always return false in simulation, so this
  //function will never get called.
}

//hardware-mac-sim.c
int8_t sli_802154phy_radio_energy_detection(void)
{
  return energyLevels[emRadioChannel - SL_ZIGBEE_MIN_802_15_4_CHANNEL_NUMBER];
}

sl_status_t sli_802154phy_set_phy_radio_channel(uint8_t newChannel)
{
  emRadioChannel = newChannel;
  trxSendCommandRadioSetting(SET_CHANNEL, sizeof(emRadioChannel), &emRadioChannel);
  return SL_STATUS_OK;
}

#if !defined(PHY_SIMULATION_DUAL)
bool sli_simulator_set_energy_level(uint8_t channelpage, uint8_t channel, uint8_t level)
{
  (void)channelpage; //Todo:will be using this later
  if ( channel < SL_ZIGBEE_MIN_802_15_4_CHANNEL_NUMBER
       || channel > SL_ZIGBEE_MAX_802_15_4_CHANNEL_NUMBER) {
    return false;
  }

  energyLevels[channel - SL_ZIGBEE_MIN_802_15_4_CHANNEL_NUMBER] = level;
  return true;
}

void setRadioGetCCAHandler(uint8_t(*handler)(void))
{
}

void setRadioTransmitHandler(void (*handler)(uint8_t phyId, uint8_t* memptr))
{
}

void setRadioSettingHandler(void (*handler)(uint8_t phyId, uint8_t channelpage, uint8_t channel, int8_t power))
{
}

bool _radioReceive(uint8_t *memptr, uint32_t rxSyncTime, uint8_t linkQuality)
{
  return 0;
}

void _radioTransmitComplete(void)
{
}

//phy-utiil.c
void sli_802154phy_radio_seed_random(void)
{
  uint32_t seed;
  sli_802154phy_radio_get_random_numbers((uint16_t *)(&seed), 2);
  halStackSeedRandom(seed);
}

//em250/phy.c
bool sli_802154phy_radio_get_random_numbers(uint16_t *rn, uint8_t count)
{
  for (; 0 < count; count--, rn++) {
    *rn = halCommonGetRandomTraced(__FILE__, __LINE__);
  }
  return true;
}
#endif  //!PHY_SIMULATION_DUAL

//hardware-mac-sim.c
uint8_t sli_802154phy_get_phy_radio_channel(void)
{
  return emRadioChannel;
}

int8_t sli_802154phy_get_phy_radio_power(void)
{
  return emRadioPower;
}

sl_status_t sli_802154phy_set_phy_radio_power(int8_t newPower)
{
  emRadioPower = newPower;
  trxSendCommandRadioSetting(SET_TX_POWER, sizeof(newPower), (uint8_t *) &newPower);
  return SL_STATUS_OK;
}

void sli_802154phy_radio_set_node_id(sl_802154_short_addr_t nodeId)
{
  emLocalRadioNodeId = nodeId;
  trxSendCommandRadioSetting(SET_SHORTID, sizeof(emLocalRadioNodeId), &emLocalRadioNodeId);
}

void sli_802154phy_set_eui64(void)
{
  trxSendCommandRadioSetting(SET_EUI64, EUI64_SIZE, sli_zigbee_stack_get_eui64());
}

RadioPowerMode sli_802154phy_radio_get_power_status(void)
{
  return radioCurrentPowerStatus;
}

void sli_802154phy_radio_sleep(void)
{
  if (radioCurrentPowerStatus == SL_ZIGBEE_RADIO_POWER_MODE_OFF) {
    return;
  }
  radioCurrentPowerStatus = SL_ZIGBEE_RADIO_POWER_MODE_OFF;
  trxSendCommandRadioSetting(SET_RADIO_POWER, sizeof(radioCurrentPowerStatus), &radioCurrentPowerStatus);
}

void sli_802154phy_radio_wake_up(void)
{
  if (radioCurrentPowerStatus == SL_ZIGBEE_RADIO_POWER_MODE_RX_ON) {
    return;
  }
  radioCurrentPowerStatus = SL_ZIGBEE_RADIO_POWER_MODE_RX_ON;
  trxSendCommandRadioSetting(SET_RADIO_POWER, sizeof(radioCurrentPowerStatus), &radioCurrentPowerStatus);
}

void sli_802154phy_radio_set_pan_id(sl_802154_pan_id_t panId)
{
  emLocalRadioPanId = panId;
  trxSendCommandRadioSetting(SET_PANID, sizeof(emLocalRadioPanId), &emLocalRadioPanId);
}

void sli_802154phy_set_coordinator(bool coordinator)
{
  deviceIsCoordinator = coordinator;
}

bool emPhyDeviceIsCoordinator(void)
{
  return deviceIsCoordinator;
}

sl_802154_short_addr_t sli_802154mac_radio_get_node_id(void)
{
  return emLocalRadioNodeId;
}

sl_802154_short_addr_t sli_802154phy_radio_get_pan_id(void)
{
  return emLocalRadioPanId;
}

sl_status_t sli_802154_stack_set_tx_power_mode(uint16_t txPowerMode)
{
  emRadioTxPowerMode = txPowerMode;
  return SL_STATUS_OK;
}

sl_status_t emRadioTransmit(uint8_t* memptr)
{
  if (memptr == NULL) {
    assert(false);
  }

  if (SL_ZIGBEE_RADIO_POWER_MODE_OFF == sli_802154phy_radio_get_idle_mode()) {
    sli_802154phy_radio_wake_up();
  }
  trxSendCommandRadioOut(memptr);
  return SL_STATUS_OK;
}

uint8_t sli_802154phy_get_phy_channel(uint8_t macPgChan)
{
  // This phy is only used for subghz interface hence
  // validating subghz channels only.
  uint8_t phyChan = sli_802154mac_pg_chan_ch(macPgChan);
  if (phyChan < MAX_CHANNELS_PER_PAGE) {
    uint8_t macPage = sli_802154phy_get_channel_page_for_channel(macPgChan);
    switch (macPage) {
      case 29: // 27..34,62
        phyChan += 27;
        if (phyChan == 35) {
          phyChan = 62;
        }
        SL_FALLTHROUGH
      case 28: //  0..26 in 863 MHz band
      case 31: //  0..26 in 915 MHz band
        return phyChan;
        break;
      case 30: // 35..61
        return (phyChan + 35);
        break;
      default:
        break;
    }
  }
  return INVALID_CHANNEL;
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

void processTransceiverToMcuCommand(uint8_t* frame)
{
  uint8_t messageType = frame[0];

  switch (messageType) {
    case RADIO_IN:
      processRxComplete((uint8_t*) &frame[1]); //pass the remaining packet
      break;
    case TX_COMPLETE:
    { sl_status_t status;
      status = (frame[5] << 8) | frame[4];
      status <<= 16;
      status |= ((frame[3] << 8) | frame[2]);
      uint32_t sfdSentTime;
      uint8_t framePending;
      sfdSentTime = (frame[9] << 8) | frame[8];
      sfdSentTime <<= 16;
      sfdSentTime |= ((frame[7] << 8) | frame[6]);
      framePending = frame[10];
      lastTxFinishedTimeMs = halCommonGetInt32uMillisecondTick();
      if (radioTransmitConfig.checkCca) {
        lastCsmaTxFinishedTimeMs = lastTxFinishedTimeMs;
      }
      sli_802154phy_radio_transmit_complete_callback(status, sfdSentTime, framePending); }
                                                                                         break;
    case TX_ACK_COMPLETE:
      simPrint("ack transmit complete");
      lastTxFinishedTimeMs = halCommonGetInt32uMillisecondTick();
      // ACKs don't update lastCsmaTxFinishedTimeMs
      sli_802154phy_radio_tx_ack_isr_callback();
      // Account for this ACK in DC plugin
      emPhyDcAckSent();
      break;
    case CCA:
      ccaReceivedFlag = true;
      ccaValue = frame[2];
      simPrint("received ccaValue %d", ccaValue);
      break;
    default:
      break;
  }
}

static void processRxComplete(uint8_t* packet)
{
  //first check for frame pending
  uint16_t control = HIGH_LOW_TO_INT(packet[2], packet[1]);
  bool setFramePending = false;
  uint16_t srcAddressMode = control & MAC_FRAME_SOURCE_MODE_MASK;
  uint8_t *appendedInfo;
  uint16_t frameStatus;
  uint16_t phyPacketLength;
  simPrint("%s: control = 0x%x len = %d", __func__, control, packet[0]);

  uint16_t dstAddressMode = control & MAC_FRAME_DESTINATION_MODE_MASK;
  uint8_t srcAddressOffset = 4;   //packet length(1) + frame control(2) + seqn(1)
  if (dstAddressMode == MAC_FRAME_DESTINATION_MODE_SHORT) {
    srcAddressOffset += 4;   // short address + panID
  } else if (dstAddressMode == MAC_FRAME_DESTINATION_MODE_LONG) {
    srcAddressOffset += 10;   // long address + panID
  }
  //else the dest. address and dest. panID is not present

  // If src address mode is non-zero and intra-pan bit is not set, the source
  // panID will be present.
  if (srcAddressMode !=  MAC_FRAME_DESTINATION_MODE_NONE
      && !(control & MAC_FRAME_FLAG_INTRA_PAN)) {
    srcAddressOffset += 2;
  }

  if ((control & MAC_FRAME_FLAG_ACK_REQUIRED)
      && MAC_FRAME_TYPE_CONTROL == (control & MAC_FRAME_TYPE_MASK)
      && autoAckEnabled) {
    if (MAC_FRAME_SOURCE_MODE_LONG == srcAddressMode) {
      setFramePending = emberRadioDataPendingLongIdIsrCallback(&packet[srcAddressOffset]);
      simPrint("srcAddressMode: MAC_FRAME_SOURCE_MODE_LONG setFramePending %d", setFramePending);
    } else if (MAC_FRAME_SOURCE_MODE_SHORT == srcAddressMode) {
      uint16_t shortAddress =
        HIGH_LOW_TO_INT(packet[srcAddressOffset + 1],
                        packet[srcAddressOffset]);
      setFramePending = emberRadioDataPendingShortIdIsrCallback(shortAddress);
      simPrint("srcAddressMode: MAC_FRAME_SOURCE_MODE_SHORT setFramePending %d", setFramePending);
    }
  }
  // Wait for MAC command byte to determine if this is a data poll.
  // We only want to set the Frame pending bit if this is a data poll.
  //
  // If it is secured then we assume that it is a poll.  Polls are the
  // only encryped MAC commands we should receive, and spinning past
  // the security AUX header until the actual byte appears would add
  // a lot of latency to the ISR.
  if (setFramePending
      && !(control & MAC_FRAME_FLAG_SECURITY_ENABLED)) {
    uint8_t payloadOffset = srcAddressOffset;

    if (srcAddressMode == MAC_FRAME_SOURCE_MODE_SHORT) {
      payloadOffset += 2;
    } else if (srcAddressMode == MAC_FRAME_SOURCE_MODE_LONG) {
      payloadOffset += 8;
    }

    // This is not a data poll so do not set frame pending
    if (DATA_REQUEST_COMMAND != packet[payloadOffset]) {
      // This packet is not a data poll; Do not set Frame Pending.
      setFramePending = false;
    }
  }

  //valid the packet and extract required infor and pass on to higher layer
  appendedInfo = &packet[packet[0] - NUM_APPENDED_INFO_BYTES];
  frameStatus
    = HIGH_LOW_TO_INT(appendedInfo[APPENDED_INFO_FRAME_STATUS_BYTE_1_INDEX],
                      appendedInfo[APPENDED_INFO_FRAME_STATUS_BYTE_0_INDEX]);
  // Appended frame status no longer needed. Store channel in 1st appended byte.
  appendedInfo[APPENDED_INFO_CHANNEL_OVERWRITE_BYTE_INDEX] = emRadioChannel;
  // Append the network index in the 2nd appended byte.
  appendedInfo[APPENDED_INFO_NETWORK_OVERWRITE_BYTE_INDEX] = sli_mac_get_current_radio_network_index(0); //mac_index

  //remove length of info-bytes from packet length as it is not part of actual received radio packet.
  //In simulation, it is just added to transfer complete packet (data + crc + appendinfo)
  //from transceiver.
  packet[0] -= NUM_APPENDED_INFO_BYTES;
  phyPacketLength = packet[0];

  if ((!emRadioReceiveCrcEnabled() || (frameStatus & FRAME_STATUS_CRC_PASS))
      && (phyPacketLength <= (PHY_MAX_SIZE + NUM_APPENDED_INFO_BYTES))) {
    // This is a valid packet:
    // 1. If we care, CRC has passed.
    // 2. It is not too long.
    printBuffer(__func__, packet, phyPacketLength + 1);     //+1 for PHR
    sli_zigbee_packet_received_internal_isr_callback(packet,
                                                     phyPacketLength   // PHY Payload
                                                     + NUM_APPENDED_INFO_BYTES, // MAC HW Appended Info
                                                     setFramePending);
  }

  //send framePending bit and seqNum only if phy ack required.
  if ((control & MAC_FRAME_FLAG_ACK_REQUIRED) && sli_802154phy_radio_auto_ack_enabled()) {
    trxSendCommandTxAck(setFramePending, packet[3]);    //packet[3]: seqNum
  }
}

static bool emberRadioDataPendingShortIdIsrCallback(uint16_t shortId)
{
  uint8_t childIndex;
  bool setFramePending = false;

  childIndex = sli_mac_child_index(shortId);
  if (0xFF != childIndex) {
    // This device is our child.
    setFramePending = sli_mac_child_has_pending_message(childIndex);
  } else {
    // This device is not our child so set frame pending
    setFramePending = true;
  }
  return setFramePending;
}

static inline bool emberRadioDataPendingLongIdIsrCallback(uint8_t* longId)
{
  return sli_mac_long_id_data_pending(longId);
}

uint8_t sli_802154phy_get_channel_page_in_use(void)
{
  return macPg2Page(sli_802154mac_pg_chan_pg(emRadioChannel));;
}

uint8_t sli_802154phy_get_channel_page_for_channel(uint8_t macPgChan)
{
  // Convert internal 3-bit abbreviated Pg to full 5-bit Page
  // Here treat macPgChan verbatim -- needs no remapping
  return macPg2Page(sli_802154mac_pg_chan_pg(macPgChan));
}

uint32_t emPhyGetLastTxCompleteTimeMs(bool lastCsmaTx)
{
  return (lastCsmaTx ? lastCsmaTxFinishedTimeMs : lastTxFinishedTimeMs);
}

static EmPhyBandConfig* mapMacPgChanToPhyChanBand(uint8_t macPgChan,
                                                  uint8_t* phyChanPtr,
                                                  uint8_t* macPagePtr,
                                                  EmPhySubBandConfig** subBandConfigPtr)
{
  // Here we assume macPgChan has already been mapped
  // Loop thru the bands and their subBandConfigs looking for a channel mapping
  uint8_t band, txpl;
  uint8_t macPg = sli_802154mac_pg_chan_pg(macPgChan);
  uint8_t macCh = sli_802154mac_pg_chan_ch(macPgChan);
  for (band = 0; band < emPhyConfigMHz.numBands; band++) {
    EmPhyBandConfig* bandConfig = (EmPhyBandConfig*) emPhyConfigMHz.bandConfigs[band];
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

void simulatedTimePassesUs(uint32_t delayUs);

uint8_t emRadioGetCca(void)
{
  trxSendCcaRequest();
  while (ccaReceivedFlag == false) {
    simulatedTimePassesUs(1);
  }
  ccaReceivedFlag = false;
  return ccaValue;
}

void simulatorSuspendRadioActivity(void)
{
  trxSendCommandRadioSetting(SUSPEND_RADIO, 0, NULL);
}

void simulatorResumeRadioActivity(void)
{
  trxSendCommandRadioSetting(RESUME_RADIO, 0, NULL);
}
