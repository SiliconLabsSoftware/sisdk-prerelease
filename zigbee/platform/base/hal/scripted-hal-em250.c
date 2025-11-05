/***************************************************************************//**
 * @file
 * @brief simulated EM250-class MAC hardware.
 * This file should *NOT* be included in Ember release code to customers.
 * This is for internal testing only.
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
#include <stdlib.h>         // for malloc()
#include <string.h>         // for memcpy()
#include <ctype.h>          // for isprint()

#include "core/sl_zigbee_stack.h"
#include "hal/hal.h"
#include "hal/micro/unix/simulation/em250/regs.h"
#include "phy/phy.h"
#include "phy/symbol-timer.h"

#if (!defined(SL_ZIGBEE_STACK_IP))
// Pro Stack
  #include "framework/packet-header.h"
  #include "framework/zigbee-packet-header.h"
#else
// IP Stack
  #include "framework/ip-packet-header.h"
#endif // !SL_ZIGBEE_STACK_IP

#include "mac/mac-header.h"

#include "stack/core/parcel.h"
#include "stack/core/scripted-stub.h"
#include "hal/scripted-hal-em250.h"

// The actual ISR function.  This isn't declared in a .h because no
// one normally has to call it.
extern uint16_t sli_802154phy_radio_receive_internal_isr(uint16_t interrupt, uint16_t pcbContext);

#define DBG(x)
//#define DBG(x) x

//------------------------------------------------------------------------------
// Simulated EM250 registers.
kRegs_t Regs;

//------------------------------------------------------------------------------
// Simulated Baseband-MAC interface (Interrupts and DMA System)

// Preprocessor directives in phy.c steer accesses to the hardware DMA address
// registers to these pointers.  Need to do this because the pointers are
// different sizes on the em250 and the simulator host platform.
uint8_t * simulatedMacRxStAddrA;
uint8_t * simulatedMacRxEndAddrA;
uint8_t * simulatedMacRxStAddrB;
uint8_t * simulatedMacRxEndAddrB;
uint8_t * simulatedMacTxStAddrA;
uint8_t * simulatedMacTxEndAddrA;
uint8_t * simulatedMacTxStAddrB;
uint8_t * simulatedMacTxEndAddrB;

// These are byte counts, while the hardware actually uses word counts.
// processIncomingByte() maintains the hardware word count register values.
static int dmaAByteCount = 0;
static int dmaBByteCount = 0;

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
int targetLatencyByteTimeTicks;

// packetLength and packetIndex are initialized by addIncomingPacket() and used
// by processIncomingByte() to determine when the packet has completed.
static int packetIndex;
static int packetLength;

static void unloadDmaBufferA(void)
{
  DBG(printf("unloadDmaBufferA\n"));
  MAC_DMA_STATUS_REG &= ~MAC_DMA_STATUS_RX_ACTIVE_A_MASK;
  MAC_DMA_CONFIG_REG &= ~MAC_DMA_CONFIG_RX_LOAD_A_MASK;
  MAC_RX_INT_SRC_REG |= MAC_RX_INT_SRC_RX_A_UNLOAD_SRC_MASK;
  dmaAByteCount = 0;
  if (MAC_DMA_CONFIG_REG & MAC_DMA_CONFIG_RX_LOAD_B_MASK) {
    MAC_DMA_STATUS_REG |= MAC_DMA_STATUS_RX_ACTIVE_B_MASK;
  }
}

static void unloadDmaBufferB(void)
{
  DBG(printf("unloadDmaBufferB\n"));
  MAC_DMA_STATUS_REG &= ~MAC_DMA_STATUS_RX_ACTIVE_B_MASK;
  MAC_DMA_CONFIG_REG &= ~MAC_DMA_CONFIG_RX_LOAD_B_MASK;
  MAC_RX_INT_SRC_REG |= MAC_RX_INT_SRC_RX_B_UNLOAD_SRC_MASK;
  dmaBByteCount = 0;
  if (MAC_DMA_CONFIG_REG & MAC_DMA_CONFIG_RX_LOAD_A_MASK) {
    MAC_DMA_STATUS_REG |= MAC_DMA_STATUS_RX_ACTIVE_A_MASK;
  }
}

// The test-script-writing-code and processIncomingByte() use callIsr() to
// process interrupts that are pending since the last latency interval.
void callIsr(void)
{
//  DBG(printf("callIsr\n"));

  sli_802154phy_radio_receive_internal_isr(0, 0);
}

// processIncomingByte() is called from addIncomingPacket() for each byte in the
// packet.
// Each call to processIncomingByte() represents elapsedByteTimeTicks
// number of byte times (each one equal to 32us).  The elapsedByteTimeTicks
// parameter will typically be 1 or 0.
// At every latency interval, processIncomingByte() calls the Isr to process any
// pending interrupts.
static void processIncomingByte(int elapsedByteTimeTicks, uint8_t byte)
{
  bool packetComplete = false;

  DBG(printf("processIncomingByte:elapsedByteTimeTicks=%d,byte=0x%02x,packetIndex=%d,packetLength=%d\n",
             elapsedByteTimeTicks, byte, packetIndex, packetLength));

  if (MAC_RX_INT_SRC_REG & MAC_RX_INT_SRC_RX_OVFLW_SRC_MASK) {
    DBG(printf("processIncomingByte:abort(Overflow)\n"));
    return;
  }

  if (++packetIndex > (packetLength)) {
    DBG(printf("processIncomingByte:packetComplete\n"));
    packetComplete = true;

    if (MAC_RX_INT_SRC_REG & MAC_RX_INT_SRC_RX_COMPLETE_SRC_MASK) {
      INT_MISS |= INT_MACRX;
    }
    MAC_RX_INT_SRC_REG |= MAC_RX_INT_SRC_RX_COMPLETE_SRC_MASK;
#ifdef SL_ZIGBEE_STACK_IP
    MAC_RX_INT_SRC_REG |= MAC_RX_INT_SRC_RX_ADDR_REC_COMP_SRC_MASK;
#endif
  }

  if (!(MAC_DMA_STATUS_REG &= (MAC_DMA_STATUS_RX_ACTIVE_A_MASK
                               | MAC_DMA_STATUS_RX_ACTIVE_B_MASK))) {
    DBG(printf("processIncomingByte:neither is active\n"));
    // Neither is active, determine which is active based on which is loaded.
    if (MAC_DMA_CONFIG_REG & MAC_DMA_CONFIG_RX_LOAD_A_MASK) {
      MAC_DMA_STATUS_REG |= MAC_DMA_STATUS_RX_ACTIVE_A_MASK;
    } else if (MAC_DMA_CONFIG_REG & MAC_DMA_CONFIG_RX_LOAD_B_MASK) {
      MAC_DMA_STATUS_REG |= MAC_DMA_STATUS_RX_ACTIVE_B_MASK;
    }
    // else, nothing active. will overflow below.
  }

  if (MAC_DMA_STATUS_REG & MAC_DMA_STATUS_RX_ACTIVE_A_MASK) {
    DBG(printf("transferred byte value 0x%02hx to dma A[%i](%x)\n", byte, dmaAByteCount, &simulatedMacRxStAddrA[dmaAByteCount]));
    simulatedMacRxStAddrA[dmaAByteCount] = byte;
    dmaAByteCount += 1;
    RX_A_COUNT_REG = (dmaAByteCount >> 1);
    if (packetComplete) {
      unloadDmaBufferA();
    }
  } else if (MAC_DMA_STATUS_REG & MAC_DMA_STATUS_RX_ACTIVE_B_MASK) {
    DBG(printf("transferred byte value 0x%02hx to dma B[%i]\n", byte, dmaBByteCount));
    simulatedMacRxStAddrB[dmaBByteCount] = byte;
    dmaBByteCount += 1;
    RX_B_COUNT_REG = (dmaBByteCount >> 1);
    if (packetComplete) {
      unloadDmaBufferB();
    }
  } else {
    DBG(printf("OVERFLOW\n"));
    // We have overflowed.
    MAC_RX_INT_SRC_REG = MAC_RX_INT_SRC_RX_OVFLW_SRC_MASK;
  }

  // Simulated interrupt latency
  currentLatencyByteTimeTicks += elapsedByteTimeTicks;
  if ((elapsedByteTimeTicks)
      && (currentLatencyByteTimeTicks >= targetLatencyByteTimeTicks)) {
    DBG(printf("latency interval\n"));
    callIsr();
    currentLatencyByteTimeTicks = 0;
  }

  if (packetComplete) {
    DBG(printf("packet complete\n"));
  }
  DBG(printf("processIncomingByte:end\n"));
}

// The test-script-writing code uses addIncomingPacket() to "send" a packet.
void addIncomingPacket(sli_parcel_t *buffer,
                       bool crcPassed)
{
  int i;
  uint16_t frameStatus;

  // This is the start of a new packet.
  packetIndex = 0;

  // sli_parcel_t messages don't include the length byte, CRC, or appended info.
  packetLength = buffer->length + PHY_APPENDED_CRC_SIZE + NUM_APPENDED_INFO_BYTES;

  DBG(printf("addIncomingPacket\n"));
  // CRC is part of the PHY payload
  processIncomingByte(1, buffer->length + PHY_APPENDED_CRC_SIZE);
  DBG(printf("addIncomingPacket:length byte\n"));

  // Write bytes from sli_parcel_t to loaded DMA buffer
  for (i = 0; i < buffer->length; i++) {
    processIncomingByte(1, buffer->contents[i]);
  }
  DBG(printf("addIncomingPacket:payload\n"));

  // Append CRC
  processIncomingByte(1, 0xcc);
  processIncomingByte(0, 0xcc);
  DBG(printf("addIncomingPacket:crc\n"));

  // Append MAC Appended Info.
  if ( !(buffer->length & 1)) {
    // Align Appended Info on word boundary
    // Pad with a zero if the total length is odd (even without length byte).
    packetLength += 1;
    processIncomingByte(0, 0);
    DBG(printf("addIncomingPacket:appended info padding\n"));
  }

  // Create Frame Status Word
  frameStatus = crcPassed ? FRAME_STATUS_CRC_PASS : 0;

  // Append MAC Frame Info.
  // The MAC hardware writes these 8 bytes in one operation, resulting in both
  // DMA buffers unloading when the appended info spans a buffer boundary.
  processIncomingByte(0, HIGH_BYTE(frameStatus));
  processIncomingByte(0, LOW_BYTE(frameStatus));
  processIncomingByte(0, SIM_RSSI);
  processIncomingByte(0, SIM_LQI_IN);
  processIncomingByte(0, SIM_GAIN);
  processIncomingByte(0, LOW_BYTE(SIM_RX_TIME >> 16));
  processIncomingByte(0, LOW_BYTE(SIM_RX_TIME >> 8));
  processIncomingByte(1, LOW_BYTE(SIM_RX_TIME));
  DBG(printf("addIncomingPacket:appended info\n"));
}

//------------------------------------------------------------------------------
// No real concept of time in the Isr test, but we need to process packets added
// to emPhyToMacQueue in the ISR.
void scriptTick(void)
{
  emMacCheckIncomingQueue();
  sli_legacy_buffer_manager_reclaim_unused_buffers(NULL);
}

//------------------------------------------------------------------------------
// TODO:  at some point we need to simulate security as well.
void sli_zigbee_security_read_network_frame_counter_token(void)
{
}

uint8_t sli_zigbee_mic_length = 0;
void emSecurityEnableInLineOperation(bool enable)
{
}

uint8_t *sli_zigbee_security_eui64(uint8_t neighborIndex)
{
  return NULL;
}

void sli_zigbee_note_incoming_frame_counter(uint8_t neighborIndex, uint32_t frameCounter)
{
}

//----------------------------------------------------------------
// Stubs
void halCommonDelayMicroseconds(uint16_t us)
{
}

void emberHalRadioInit(void)
{
}

void sli_802154phy_radio_transmit_complete_callback(sl_status_t status,
                                                    uint32_t sfdSentTime,
                                                    uint8_t framePending)
{
}

void sli_802154phy_radio_tx_ack_isr_callback(void)
{
}

bool emberHalGetRadioCcaPin(void)
{
  return false;
}

uint8_t sli_zigbee_frame_counter_indexFromEui64(sl_802154_long_addr_t neighborLongAddress,
                                                sl_802154_short_addr_t neighborShortAddress)
{
  return 0;
}

bool sli_zigbee_check_incoming_frame_counter(uint8_t index, uint32_t frameCounter)
{
  return false;
}

#ifdef  MAC_TEST_STACK

void halStackCancelSymbolDelayA(void)
{
}

void halStackOrderInt16uSymbolDelayA(uint16_t symbols)
{
}

#endif//MAC_TEST_STACK

uint32_t scriptTime(void)
{
  return 0;
}

uint32_t halStackGetInt32uSymbolTick(void)
{
  return 0;
}

void halStackSeedRandom(uint32_t seed)
{
}

bool halPtaIsEnabled(void)
{
  return false;
}
