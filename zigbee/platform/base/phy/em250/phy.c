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
#include "core/sl_zigbee_stack.h"
#include "hal/hal.h"
#include "phy/phy.h"
#include "indirect-queue.h" // unified-mac
#include "mac-child.h" // unified-mac
#include "framework/eui64.h"
#include "phy/security.h"
#include "platform/micro/aes.h"
#include "phy/ieee802154mac.h"

// TODO: sli_mac_child_has_pending_message implementation for SL-Thread.
  #include "routing/zigbee/child.h"

// In the PRO stack we need to write the network index in the appended info.
// The network index is obtained by calling
// sli_mac_get_current_radio_network_index().
// In the IP stack we always write 0.
  #include "upper-mac.h"

#define SETUP_MAC_RX_DMA_A()                             \
  MAC_RX_ST_ADDR_A_REG = MAC_RX_END_ADDR_A_REG           \
                           = (DmaAddressType) rxBufferA; \
  MAC_RX_END_ADDR_A_REG += (RX_BUFFER_BYTES - 1);

#define SETUP_MAC_RX_DMA_B()                             \
  MAC_RX_ST_ADDR_B_REG = MAC_RX_END_ADDR_B_REG           \
                           = (DmaAddressType) rxBufferB; \
  MAC_RX_END_ADDR_B_REG += (RX_BUFFER_BYTES - 1);

// Forward declarations
static void unload(bool unloadA);

static uint8_t currentReceiveChannel;

extern uint8_t emRadioChannel;
// from phy-common.c:
extern bool sli_802154mac_ack_needed;
extern uint32_t lastTxSfdTime;
extern RadioPowerMode radioCurrentPowerStatus;

// for /app/test/link.c:
uint16_t eventCounts[8];

// Maximum incoming is the length byte, the MAC message, and the appended info.
// It is important that this be even.
#define RX_BUFFER_BYTES (1 + 127 + NUM_APPENDED_INFO_BYTES)
#define RX_BUFFER_WORDS (RX_BUFFER_BYTES / 2)
#define NUM_RX_DMA_BUFFERS 2

uint8_t rxBufferA[RX_BUFFER_BYTES];
uint8_t rxBufferB[RX_BUFFER_BYTES];

static bool aNext;
static bool processedAddressesA;
static bool processedAddressesB;
static bool framePendingSetInOutgoingAck[NUM_RX_DMA_BUFFERS];

static inline bool emberRadioDataPendingShortIdIsrCallback(uint16_t shortId,
                                                           uint16_t frameType);
static inline bool emberRadioDataPendingLongIdIsrCallback(uint8_t* longId,
                                                          uint16_t frameType);
// xap2b Transmit Interrupt Handler Wrapper
extern void emRadioTransmitIsr(void);

uint16_t emRadioTransmitIsrPcbContext;

uint16_t emRadioTransmitInternalIsr(uint16_t interrupt, uint16_t pcbContext)
{
  UNUSED_VAR(interrupt);
  emRadioTransmitIsrPcbContext = pcbContext;
  emRadioTransmitIsr();
  return 0;
}

static void reloadA(void)
{
  SETUP_MAC_RX_DMA_A();
  aNext = false;
  processedAddressesA = false;

  SET_MAC_DMA_CONFIG(MAC_DMA_CONFIG_RX_LOAD_A_MASK);
}

static void reloadB(void)
{
  SETUP_MAC_RX_DMA_B();
  aNext = true;
  processedAddressesB = false;

  SET_MAC_DMA_CONFIG(MAC_DMA_CONFIG_RX_LOAD_B_MASK);
}

void emRadioResetAndEnableReceive(void)
{
  // RX DMA Config
  SET_MAC_DMA_CONFIG(MAC_DMA_CONFIG_RX_DMA_RESET);
  // Reset MAC Rx.  Preserve existing Rx configuration.
  MAC_RX_CONFIG |= MAC_RX_CONFIG_MAC_RX_RST;
  reloadA();
  reloadB();
}

void emRadioBootPhySpecific(void)
{
  SYNTH_START_REG = 0x3264;

  GAIN_THRESH_MIN_REG = 40;
  GAIN_THRESH_MID_REG = 85;
  GAIN_THRESH_MAX_REG = 190;
  GAIN_SETTING = 0x7640;

  // Preamble correlator thresholds are set to be as low as possible for max
  // sensitivity, but just above noise so we don't get too many false detects.
  // The effect of false detects is to potentially mask a real packet if the
  // false detect lines up with the real message preamble symbols.
  // The CW tone transmitted during ramp up effectively raises the noise that
  // the correlator sees, which means we get quite a lot of false detects during
  // the ramp.  They are randomly distributed, and some occur in exact alignment
  // with the subsequent message preamble causing the message to be lost.
  // At PREAMBLE_EVENT=0x6066 we saw PER as high as 2%.
  PREAMBLE_EVENT_REG = 0x606e;
  PREAMBLE_ABORT_THRESH_REG = 0x0058;
  PREAMBLE_ACCEPT_WINDOW_REG = 0x0003; // any lower gives a residual PER

  RX_ERR_THRESH_REG = 0x2608;
}

// The radio receive interrupt interrupt handler called from cstartup.xap.
uint16_t sli_802154phy_radio_receive_internal_isr(uint16_t interrupt, uint16_t pcbContext)
{
  UNUSED_VAR(pcbContext);
  int8_t numMissed;

  // Capture any new events that occurred since intHandler peeked
  interrupt |= (MAC_RX_INT_SRC_REG & MAC_RX_INT_MASK_REG);
  // Ack interrupts early to ensure better capture of new and missed events
  CLEAR_INT(MAC_RX_INT_SRC_REG, interrupt);
  // With all interrupts ack'd, INT_MISS should accurately reflect any
  // events missed since last time interrupts were acknowledged
  numMissed = !!(INT_MISS & INT_MACRX);
  if (numMissed) {
    // Don't clear int missed unnecessarily.  Might lose missed info.
    CLEAR_INT(INT_MISS, INT_MACRX);
  }

  // MAC_RX_INT_MASK_TX_ACK_MSK_MASK occurs after Ack has been sent.
  // This will be approximately 22symbols after RX_COMPLETE.

  // MAC_RX_INT_MASK_RX_ERROR_MSK_MASK
  // don't need to be interrupted by this, but the Dropped Ack workaround will
  // turn it on as needed.

  // MAC_RX_INT_MASK_BB_RX_LEN_ERR_MSK_MASK happens in first word.
  // don't need to be interrupted by this.

  // MAC_RX_INT_MASK_TX_ACK_ERR_MSK_MASK will be because of:
  //  1. TX_LOCK_FAIL
  //  2. non-cca, backoff (2 or less backoffs remaining) tx inhibits ack.

  // MAC_RX_INT_SRC_RX_ROLLBACK_SRC_MASK
  // will not see if filtering.
  // if not filtering, and after the first buffer we will see ROLLBACK.
  // in this case we will not see RX_COMPLETE.
  // ROLLBACK is telling us that the packet has been dropped,
  // the first buffer(s) are invalid, and the current DMA buffer is still
  // loaded.

  // TODO: handle TX_ACK and TX_ACK_ERR.  We currently pass the packet up
  // on RX_COMPLETE, but perhaps we shouldn't do anything with it intil we
  // know it is acked.

  if ( interrupt & (0
                    | MAC_RX_INT_SRC_RX_ROLLBACK_SRC_MASK
                    | MAC_RX_INT_SRC_TX_COLL_RX_SRC_MASK
                    | MAC_RX_INT_SRC_RX_OVFLW_SRC_MASK
                    )) {
    // TODO: We don't need to be so drastic with ROLLBACK.  There is a bufer
    // loaded and ready to receive or already receiving.  This would only be
    // optimization for promiscuous mode.  Not high priority.

    // TODO: We don't need to be so drastic with TX_COLL, but we probably have
    // alot of time.  Also, TX_COLL is included in appended frame status word.
    // Drop decision can be made at that time.

    SETUP_MAC_RX_DMA_A();
    SETUP_MAC_RX_DMA_B();
    aNext = true;
    processedAddressesA = false;
    processedAddressesB = false;

    // Prepare for immediate overflow check
    CLEAR_INT(MAC_RX_INT_SRC_REG, MAC_RX_INT_SRC_RX_OVFLW_SRC_MASK);

    // EM250 bug 3081
    // If we reset rx during the length portion of the RX_TOP_SM, we run
    // the risk of having bad data transferred to memory.
    // So we wait it out.  Will not take more than 32us.
    if ( RX_TOP_STATE_LENGTH == (RX_STATE_REG & RX_STATE_RX_TOP_STATE_MASK)) {
      do {
      } while ( RX_TOP_STATE_LENGTH == (RX_STATE_REG & RX_STATE_RX_TOP_STATE_MASK));
    }

    // Operation 1 below must occur within 16us of the length state check
    // above.  Any more than that and is possible that the first nibble of the
    // length byte will have been received when we reset.  This condition
    // results in corruption of data in memory with a valid CRC.
    // Operations 1, 2, 3, and 4 must occur in direct sequence.
    // A DMA buffer must be loaded within 16us of the Rx reset
    // to avoid possible immediate overflow.
    // 1. Reset RX
    MAC_RX_CONFIG_REG |= MAC_RX_CONFIG_MAC_RX_RST_MASK;
    // 2. Reset DMA
    SET_MAC_DMA_CONFIG(MAC_DMA_CONFIG_RX_DMA_RESET_MASK);
    // 3. Clear interrupts (except overflow, still need to check it)
    // DMA buffers unload on reset.  Don't service them.
    // RX_COMPLETE might set again quickly if the reset occured right at the
    // end of the packet.  If so we'll service it next time through the Isr.
    CLEAR_INT(MAC_RX_INT_SRC_REG, (0xffff & ~MAC_RX_INT_SRC_RX_OVFLW_SRC_MASK));
    // 4. Load DMA
    SET_MAC_DMA_CONFIG(MAC_DMA_CONFIG_RX_LOAD_A_MASK
                       | MAC_DMA_CONFIG_RX_LOAD_B_MASK);
    // Check for immediate overflow
    if (MAC_RX_INT_SRC_REG & MAC_RX_INT_SRC_RX_OVFLW_SRC_MASK) {
      // The only valid reason for this to have occured is that we were right
      // at the end of the packet and the appended info was written with
      // no delay.  Detect this by resetting everything again (which we need
      // to do anyway).
      // Operations 2, 3, 4 and 5 must occur in direct sequence.
      // A DMA buffer must be loaded within 16us of the Rx reset
      // to avoid possible immediate overflow.

      // 1. Prepare for immediate overflow check
      CLEAR_INT(MAC_RX_INT_SRC_REG, MAC_RX_INT_SRC_RX_OVFLW_SRC_MASK);
      // 2. Reset RX
      MAC_RX_CONFIG_REG |= MAC_RX_CONFIG_MAC_RX_RST_MASK;
      // 3. Reset DMA
      SET_MAC_DMA_CONFIG(MAC_DMA_CONFIG_RX_DMA_RESET_MASK);
      // 4. Clear interrupts (except overflow, still need to check it)
      // DMA buffers unload on reset.  Don't service them.
      // RX_COMPLETE might set again quickly if the reset occured right at the
      // end of the packet.  If so we'll service it next time through the Isr.
      CLEAR_INT(MAC_RX_INT_SRC_REG, 0xffff & ~MAC_RX_INT_SRC_RX_OVFLW_SRC_MASK);
      // 5. Load DMA
      SET_MAC_DMA_CONFIG(MAC_DMA_CONFIG_RX_LOAD_A_MASK
                         | MAC_DMA_CONFIG_RX_LOAD_B_MASK);
      // 6. Check for immediate overflow (serious problem, possibly hardware)
      if (MAC_RX_INT_SRC_REG & MAC_RX_INT_SRC_RX_OVFLW_SRC_MASK) {
        // This means that it was not an appended info write that caused the
        // previous error.  Possibly another packet has started to be received,
        // but more likely something bad has happened and continues. Consider:
        // assert(0);
      }
    }

    // We have already reset, no need to service anything else.
    return 0;
  }

  if (interrupt & MAC_RX_INT_SRC_RX_ERROR_SRC) {
  }

  // If we are late in servicing the ISR we may receive an ADDR_REC_COMP
  // interrupt for a packet before we have finished processing the previous
  // one.  The haveAddresses flag prevents us from setting the frame pending
  // bit for the second packet based on the source address of the first.

  if (interrupt & MAC_RX_INT_SRC_RX_ADDR_REC_COMP_SRC_MASK) {
    uint8_t* packet = aNext ? rxBufferA : rxBufferB;
    uint16_t control = HIGH_LOW_TO_INT(packet[2], packet[1]);
    uint16_t srcAddressMode = control & MAC_FRAME_SOURCE_MODE_MASK;
    bool setFramePending = false;
    uint8_t dmaBuffer = aNext ? 0 : 1;

    uint16_t dstAddressMode = control & MAC_FRAME_DESTINATION_MODE_MASK;
    uint8_t srcAddressOffset = 4; //packet length(1) + frame control(2) + seqn(1)
    if (dstAddressMode == MAC_FRAME_DESTINATION_MODE_SHORT) {
      srcAddressOffset += 4; // short address + panID
    } else if (dstAddressMode == MAC_FRAME_DESTINATION_MODE_LONG) {
      srcAddressOffset += 10; // long address + panID
    }
    //else the dest. address and dest. panID is not present

    // If src address mode is non-zero and intra-pan bit is not set, the source
    // panID will be present.
    if (srcAddressMode !=  MAC_FRAME_DESTINATION_MODE_NONE
        && !(control & MAC_FRAME_FLAG_INTRA_PAN)) {
      srcAddressOffset += 2;
    }

    // Record the channel that this message arrived on.
    // This info will be passed up the stack along with the packet.
    currentReceiveChannel = emRadioChannel;

    // TODO: Handle case where two of these ints have occured.
    // This can happen when interrupt latency > 384us (3 bytes of preamble +
    // 1 byte SFD + 8 byte header).  Could be even less if shorter header than
    // just length, fcf, seq, short_addr and pan_id.

    // Prepare the frame pending bit in the ack.  Don't do this if:
    // - the ack flag is not set
    // - the hardware has been told not to send acks
    // - we have already processed the addresses in current buffer
    //   (this happens if a second packet's addresses arrive after
    //    we have processed first one's addresses but before we
    //    process the first's unload)
    framePendingSetInOutgoingAck[dmaBuffer] = false;
    uint16_t frameType = (control & MAC_FRAME_TYPE_MASK);
    if ((control & MAC_FRAME_FLAG_ACK_REQUIRED)
        && (frameType == MAC_FRAME_TYPE_CONTROL
            )
        && (MAC_RX_CONFIG_REG & MAC_RX_CONFIG_AUTO_ACK_MASK)
        && !(aNext
             ? processedAddressesA
             : processedAddressesB)) {
      if (MAC_FRAME_SOURCE_MODE_LONG == srcAddressMode) {
        setFramePending =
          emberRadioDataPendingLongIdIsrCallback(&packet[srcAddressOffset],
                                                 frameType);
      } else if (MAC_FRAME_SOURCE_MODE_SHORT == srcAddressMode) {
        uint16_t shortAddress =
          HIGH_LOW_TO_INT(packet[srcAddressOffset + 1],
                          packet[srcAddressOffset]);
        setFramePending = emberRadioDataPendingShortIdIsrCallback(shortAddress,
                                                                  frameType);
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
        uint8_t wordsToRead = (srcAddressOffset >> 1);
        if (srcAddressOffset & 0x1) {
          wordsToRead++;
        }

        if (srcAddressMode == MAC_FRAME_SOURCE_MODE_SHORT) {
          payloadOffset += 2;
        } else if (srcAddressMode == MAC_FRAME_SOURCE_MODE_LONG) {
          payloadOffset += 8;
        }

        // Make sure this is a MAC command message and that we've read at least
        // as many bytes as this packet's source and destination address would
        // indicate. If we have read fewer bytes then something is very wrong.
        if (wordsToRead <= (aNext
                            ? RX_A_COUNT
                            : RX_B_COUNT)) {
          // This is a MAC command frame with the appropriate header length.
          // Wait for the command byte.  RX_*_COUNT will always get to 6 even
          // if the baseband drops the frame.  Note:  If Tx stomps on this Rx
          // RX_*_COUNT will not get to 6 and watchdog will eventually fire.
          while ((aNext
                  ? RX_A_COUNT
                  : RX_B_COUNT)
                 < wordsToRead + 1) ;
          // This is not a data poll so do not set frame pending
          if (DATA_REQUEST_COMMAND != packet[payloadOffset]) {
            // This packet is not a data poll; Do not set Frame Pending.
            setFramePending = false;
          }
        } else {
          // The header length is wrong, do not set Frame Pending.
          setFramePending = false;
        }
      }

      // Set or clear the frame pending bit as necessary.
      if (setFramePending) {
        MAC_ACK_STROBE_REG |= MAC_ACK_STROBE_FRAME_PENDING_MASK;
        framePendingSetInOutgoingAck[dmaBuffer] = true;
      }
    }
    if (aNext) {
      processedAddressesA = true;
    } else {
      processedAddressesB = true;
    }
  }

  if (aNext
      && (interrupt & MAC_RX_INT_SRC_RX_A_UNLOAD_SRC_MASK)) {
    unload(true);
    interrupt &= ~MAC_RX_INT_SRC_RX_A_UNLOAD_SRC_MASK;
  }

  if (interrupt & MAC_RX_INT_SRC_RX_B_UNLOAD_SRC_MASK) {
    unload(false);
  }

  if (interrupt & MAC_RX_INT_SRC_RX_A_UNLOAD_SRC_MASK) {
    unload(true);
  }

  if (interrupt & MAC_RX_INT_SRC_TX_ACK_SRC_MASK) {
    sli_802154phy_radio_tx_ack_isr_callback();
  }

  return 0;
}

static bool emberRadioDataPendingShortIdIsrCallback(uint16_t shortId,
                                                    uint16_t frameType)
{
  uint8_t childIndex = sli_mac_child_index(shortId);
  if (0xFF != childIndex) {
    // This device is our child.
    UNUSED_VAR(frameType);
    // TODO: sli_mac_child_has_pending_message implementation for SL-Thread.
    return sli_mac_child_has_pending_message(childIndex);
  }
  // This device is not our child so set frame pending
  // but only if it's a CMD frame (true data poll).
  return (frameType == MAC_FRAME_TYPE_CONTROL);
}

static inline bool emberRadioDataPendingLongIdIsrCallback(uint8_t* longId,
                                                          uint16_t frameType)
{
  UNUSED_VAR(frameType);
  return sli_mac_long_id_data_pending(longId);
}

static void unload(bool unloadA)
{
  uint8_t *buffer;
  uint16_t dmaWords;
  uint8_t dmaBuffer;

  if (unloadA) {
    buffer = rxBufferA;
    dmaWords = RX_A_COUNT_REG;
    dmaBuffer = 0;
  } else {
    buffer = rxBufferB;
    dmaWords = RX_B_COUNT_REG;
    dmaBuffer = 1;
  }

  // Turn off the baseband drop interrupt for BugzId: 10598 workaround.
  MAC_RX_INT_MASK &= ~MAC_RX_INT_MASK_RX_ERROR_MSK;

  if (dmaWords < NUM_APPENDED_INFO_WORDS) {
    // EM250 bug 3081
    // It is possible for the CRC to appear to pass for short error packet
    // remainders that get DMA'd to memory after rx reset.  This occurs
    // if rx reset occurs in the first byte of a single byte packet remainder.
    // The problem is that CRC has been cleared (zero indicates pass), but
    // the calculation never starts because the packet has no payload.
    // Trap this condition by dropping packets that are only a single byte.
    // Since we only have an indication of the number of words written,
    // refuse everything with zero or one word.
  } else {
    uint8_t *appendedInfo;
    uint16_t dmaBytes = dmaWords << 1;
    uint16_t frameStatus;
    uint16_t phyPacketLength;

    appendedInfo = &buffer[dmaBytes - NUM_APPENDED_INFO_BYTES];
    frameStatus
      = HIGH_LOW_TO_INT(appendedInfo[APPENDED_INFO_FRAME_STATUS_BYTE_1_INDEX],
                        appendedInfo[APPENDED_INFO_FRAME_STATUS_BYTE_0_INDEX]);
    // Upper layers now get true dBm so convert raw to dBm here
    appendedInfo[APPENDED_INFO_RSSI_BYTE_INDEX] += RSSI_DBM_OFFSET_EM250CLASS;
    // Appended frame status no longer needed. Store channel in 1st appended byte.
    appendedInfo[APPENDED_INFO_CHANNEL_OVERWRITE_BYTE_INDEX] = currentReceiveChannel;
    // Append the network index in the 2nd appended byte.
    appendedInfo[APPENDED_INFO_NETWORK_OVERWRITE_BYTE_INDEX] = sli_mac_get_current_radio_network_index(0);

    phyPacketLength = buffer[0];

    if (MAC_TX_INT_SRC_REG & MAC_TX_INT_SRC_RX_ACK_SRC_MASK ) {
      // This packet is an Ack, potentially valid.
      if (sli_802154mac_ack_needed) {
        // This Ack is expected.
        sli_802154mac_ack_needed = false;
        // There are a few HW peculiarities we need to watch out for:
        // 1. If CRC fails, HW will still call this a good Ack (All 35x).
        // 2. If the packet is dropped during the CRC, no interrupt will fire
        // (Lipari silicon, fixed in metal spin).
        // So we consider any packet error to be a failed Ack.
        if (frameStatus & FRAME_STATUS_DROP_CONDITIONS) {
          // Since no completion interrupt will fire, complete this transmit attempt:
          sli_802154phy_radio_transmit_complete_callback(SL_STATUS_MAC_NO_ACK_RECEIVED,
                                                         lastTxSfdTime,
                                                         false);
        } else {
          // Process Ack, indicating whether frame pending was set in FCF or not.
          sli_802154phy_radio_transmit_complete_callback(SL_STATUS_TRANSMIT_ACK_RECEIVED,
                                                         lastTxSfdTime,
                                                         (buffer[1] & 0x10) ? true : false);
        }
      }
      // Clear the corresponding Tx interrupt.
      MAC_TX_INT_SRC = MAC_TX_INT_SRC_RX_ACK_SRC_MASK;
    } else if ( !(frameStatus & FRAME_STATUS_DROP_CONDITIONS)
                && ((!emRadioReceiveCrcEnabled())
                    || (frameStatus & FRAME_STATUS_CRC_PASS))
                && (phyPacketLength <= PHY_MAX_SIZE)
                && ((phyPacketLength             // PHY Payload
                     + ((phyPacketLength + 1) & 1)  // MAC HW Appending Padding
                     + NUM_APPENDED_INFO_BYTES   // MAC HW Appended Info
                     + 1)                        // PHY Header
                    == dmaBytes)) {
      // This is a valid packet:
      // 1. It does not have any drop conditions that we care about.
      // 2. If we care, CRC has passed.
      // 3. It is not too long.
      // 4. It is exactly as long as it should be in memory.
      sli_zigbee_packet_received_internal_isr_callback(buffer,
                                                       dmaBytes,
                                                       framePendingSetInOutgoingAck[dmaBuffer]);
    }
  }

  if (unloadA) {
    reloadA();
  } else {
    reloadB();
  }
}

uint32_t sli_802154phy_get_mac_timer(void)
{
  union {
    uint32_t full;
    struct {
      uint16_t h;
      uint16_t l;
    } w;
  } mac_timer;

  mac_timer.w.l = MAC_TIMER_L_REG; // Read low word first
  mac_timer.w.h = MAC_TIMER_H_REG;
  return mac_timer.full;
}

bool sli_802154phy_radio_get_random_numbers(uint16_t *rn, uint8_t count)
{
  for (; 0 < count; count--, rn++) {
    *rn = halCommonGetRandomTraced(__FILE__, __LINE__);
  }
  return true;
}

bool sli_802154phy_radio_check_radio(void)
{
  bool calibrationNeeded = false;

  return calibrationNeeded;
}
