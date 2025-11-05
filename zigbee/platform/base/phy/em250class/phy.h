/***************************************************************************//**
 * @file
 * @brief
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
#ifndef __EM250CLASS_PHY_H__
#define __EM250CLASS_PHY_H__

#ifndef __PHY_H__
#error do not include this file directly - include phy/phy.h
#endif//__PHY_H__

// Include the types file to get the EM358X define
#include "hal/micro/micro-types.h"

#if (defined(IEEE802154_GB868_SUPPORTED))
// The pro2class parameters are pretty close to what we want on EFR32 too
  #define SL_ZIGBEE_PHY_USE_PHR_154G   1
  #define SL_ZIGBEE_PHY_DATA_WHITENING 1

  #include "phy/pro2class/phy-params-subGHz.h"

#else//!IEEE802154_GB868_SUPPORTED
// Basic timing parameters for 802.15.4-2011 QPSK 2.4 GHz PHY
#define SL_ZIGBEE_PHY_MAX_PHR_BYTES      1        // 802.15.4-2003/2006/2011
#define SL_ZIGBEE_PHY_MAX_PAYLOAD_BYTES  127      // 802.15.4-2003/2006/2011
#define SL_ZIGBEE_PHY_MAX_CRC_BYTES      2        // 802.15.4-2003/2006/2011
#define SL_ZIGBEE_PHY_DATA_RATE_BPS      250000UL // 250 kbps
#define SL_ZIGBEE_PHY_BIT_TIME_US        4        // 1000000 us/sec / DATA_RATE_BPS
#define SL_ZIGBEE_PHY_SYMBOL_BITS        4        // SYMBOL_TIME_US     =   16 us
#define SL_ZIGBEE_PHY_PREAMBLE_BITS      32       // PREAMBLE_TIME_US   =  128 us
#define SL_ZIGBEE_PHY_SFD_BITS           8        // SFD_TIME_US        =   32 us
#define SL_ZIGBEE_PHY_BYTE_BITS          8        // BYTE_TIME_US       =   32 us
#define SL_ZIGBEE_PHY_TURNAROUND_SYMBOLS 12       // TURNAROUND_TIME_US =  192 us
#define SL_ZIGBEE_PHY_CCA_SYMBOLS        8        // CCA_TIME_US        =  128 us

#define SL_ZIGBEE_PHY_MAX_FRAME_RESPONSE_MULTIPLIER 1
#define SL_ZIGBEE_PHY_ACK_TIMEOUT_SYMBOLS           (MAC_ACK_TO_RESET / SL_ZIGBEE_PHY_SYMBOL_TIME_US)
#endif//IEEE802154_GB868_SUPPORTED

// 802.15.4 says ACK timeout should be 54 symbols, but we override that
// with this which evalutes to 48 symbols (we currently don't override
// MAC_ACK_TO_RESET) -- see bug EMSTACK-867.

// The register definitions need to be included for the simulator.
// "Some of these definitions will be redefined below.
  #include "micro/unix/simulation/em250/regs.h"
  #include "micro/unix/simulation/em250/regs-indirect-analog.h"

// Parameters for DMA buffers
#define NUM_RX_DMA_BUFFER_BYTES 138 // 128 (phy) + 8 (appended) + 2 (MAC HW bug)
#define NUM_RX_DMA_BUFFER_WORDS    NUM_RX_DMA_BUFFER_BYTES / 2
#define NUM_TX_DMA_BUFFER_BYTES 128

    #define ALL_MAC_INTS (INT_MACRX | INT_MACTX | INT_MACTMR)
 #if     PHY_DUAL
    #define SOME_MAC_INTS (INT_MACRX | INT_MACTX) // INT_MACTMR stays on for other PHY
 #else//!PHY_DUAL
  #define SOME_MAC_INTS ALL_MAC_INTS
 #endif//PHY_DUAL

  #define MAC_RX_COMPLETE_INTS (MAC_RX_INT_MASK_RX_A_UNLOAD_MSK_MASK \
                                | MAC_RX_INT_MASK_RX_B_UNLOAD_MSK_MASK)
  #define MAC_TX_INT_ON      INT_CFG |=  INT_MACTX;
  #define MAC_TX_INT_OFF     INT_CFG &= ~INT_MACTX;
  #define MAC_TIMER_INT_ON   INT_CFG  |=  INT_MACTMR;
  #define MAC_TIMER_INT_OFF  INT_CFG &= ~INT_MACTMR;
  #define MAC_INTERRUPTS_ON  INT_CFG |=  ALL_MAC_INTS;
  #define MAC_INTERRUPTS_OFF INT_CFG &= ~ALL_MAC_INTS;
  #define DECLARE_MAC_INTERRUPT_STATE uint16_t _macInterruptState
  #define DISABLE_MAC_INTERRUPTS               \
  _macInterruptState = INT_CFG & ALL_MAC_INTS; \
  INT_CFG &= ~ALL_MAC_INTS;
  #define RESTORE_MAC_INTERRUPTS \
  INT_CFG |= _macInterruptState;

// Drop if frame status indicates baseband has found a length byte error.
// Drop if frame status indicates baseband has dropped packet.
// Drop if frame status indicates Tx has collided with Rx packet.
// Drop if length byte greater than max allowed.
  #define FRAME_STATUS_DROP_CONDITIONS \
  (0                                   \
   | FRAME_STATUS_ERROR_RX_CORR        \
   | FRAME_STATUS_BB_LENGTH_ERROR      \
   | FRAME_STATUS_TX_COLL_RX           \
   | FRAME_STATUS_RX_MIN_LENGTH        \
  )

// We need to use a more conventional approach on the simulator.
  #define CLEAR_INT(intReg, intMask) \
  intReg &= ~intMask;

// On the simulator we need to actively respond to sets.
void emSetMacDmaConfig(uint16_t value);
#define SET_MAC_DMA_CONFIG(value) (emSetMacDmaConfig(value))

// The hardware DMA address registers store 16-bit pointers.
// Need native pointers for simulation.
// Rx
extern uint8_t * simulatedMacRxStAddrA;
  #undef  MAC_RX_ST_ADDR_A_REG
  #define MAC_RX_ST_ADDR_A_REG simulatedMacRxStAddrA
extern uint8_t * simulatedMacRxEndAddrA;
  #undef  MAC_RX_END_ADDR_A_REG
  #define MAC_RX_END_ADDR_A_REG simulatedMacRxEndAddrA
extern uint8_t * simulatedMacRxStAddrB;
  #undef  MAC_RX_ST_ADDR_B_REG
  #define MAC_RX_ST_ADDR_B_REG simulatedMacRxStAddrB
extern uint8_t * simulatedMacRxEndAddrB;
  #undef  MAC_RX_END_ADDR_B_REG
  #define MAC_RX_END_ADDR_B_REG simulatedMacRxEndAddrB
// Tx
extern uint8_t * simulatedMacTxStAddrA;
  #undef  MAC_TX_ST_ADDR_A_REG
  #define MAC_TX_ST_ADDR_A_REG simulatedMacTxStAddrA
extern uint8_t * simulatedMacTxEndAddrA;
  #undef  MAC_TX_END_ADDR_A_REG
  #define MAC_TX_END_ADDR_A_REG simulatedMacTxEndAddrA
extern uint8_t * simulatedMacTxStAddrB;
  #undef  MAC_TX_ST_ADDR_B_REG
  #define MAC_TX_ST_ADDR_B_REG simulatedMacTxStAddrB
extern uint8_t * simulatedMacTxEndAddrB;
  #undef  MAC_TX_END_ADDR_B_REG
  #define MAC_TX_END_ADDR_B_REG simulatedMacTxEndAddrB
// Define the type used in casting (no cast strictly needed for simulator)
typedef uint8_t* DmaAddressType;

#ifdef  STACK
// In a stack build, the board header is not included.
// Define led pins here so that leds work in stack builds.
// This enum is lifted from test0455.h.
enum HalBoardLedPins {
  BOARDLED0 = 14,
  BOARDLED1 = 13,
  BOARDLED2 = 12,
  BOARDLED3 = 11,
  BOARD_ACTIVITY_LED  = BOARDLED0,
  BOARD_HEARTBEAT_LED = BOARDLED1
};
#endif//STACK

// TODO: Use calibrated RSSI scaling.
// The offset of -154 converts approximately to dBm (per N. Horne),
// but the exact value may vary from chip to chip and with temperature.
#define RSSI_DBM_OFFSET_EM250CLASS -154

// CCA Modes
// Note: The only valid mode for EM250-class devices is CCA_MODE_ED.
// The Carrier Sense feature is susceptible to interence in the presence of a
// pure sine wave signal.
//
// CCA Mode 0x00 - Busy when carrier sensed OR energy detected above threshold.
#define CCA_MODE_CS_OR_ED     0
// 802.15.42-003 6.7.9 defines:
// CCA Mode 0x01 - Busy when energy detected above threshold.
#define CCA_MODE_ED           1
// CCA Mode 0x10 - Busy when carrier sensed.
#define CCA_MODE_CS           2
// CCA Mode 0x11 - Busy when carrier sensed AND energy detected above thresh.
#define CCA_MODE_CS_AND_ED    3

// Internal MAC HW States
#define RX_TOP_STATE_OFF            0
#define RX_TOP_STATE_WAIT_RX_FRAME  1
#define RX_TOP_STATE_CLEAR          2
#define RX_TOP_STATE_LENGTH         3
#define RX_TOP_STATE_ILLEGAL_LENGTH 4
#define RX_TOP_STATE_FC_LSB         5
#define RX_TOP_STATE_FC_MSB         6
#define RX_TOP_STATE_ILLEGAL_FORMAT 7
#define RX_TOP_STATE_SEQ_NUM        8
#define RX_TOP_STATE_DEST_PAN_ID    9
#define RX_TOP_STATE_DEST_ADDR      10
#define RX_TOP_STATE_SRC_PAN_ID     11
#define RX_TOP_STATE_SRC_ADDR       12
#define RX_TOP_STATE_PAYLOAD        13
#define RX_TOP_STATE_CHECK_CRC_PASS 14
#define RX_TOP_STATE_APPEND         15
#define RX_TOP_STATE_NOTIFY_CPU     16
#define RX_TOP_STATE_WAIT_END       17

#define BO_STATE_OFF          0 // no back-off/transmit/rx ack wait
#define BO_STATE_BO           1 // run back-off time
#define BO_STATE_CCA          2 // run cca time
#define BO_STATE_BO_SINGLE    3 // slotted wait another back-off period, before 2nd cca check
#define BO_STATE_CCA_SLOT     4 // 2nd cca check for slotted
#define BO_STATE_TX_PACKET    5 // transmit packet (non ack)
#define BO_STATE_BO_FAIL      6 // back-off has failed, cca fail
#define BO_STATE_RX_ACK_TIMER 7 // wait for rx ack
#define BO_STATE_ACK_ERR      8 // rx ack has failed

#define MAC_TOP_STATE_OFF       0
#define MAC_TOP_STATE_RX_IDLE   1
#define MAC_TOP_STATE_PKT_START 2
#define MAC_TOP_STATE_TRANSMIT  3

void emRadioBootPhySpecific(void);

void sli_802154phy_set_coordinator(bool coordinator);
bool emPhyDeviceIsCoordinator(void);

void emRadioSetEdCcaThreshold(int8_t threshold);
int8_t emRadioGetEdCcaThreshold(void);

void emRadioResetAndEnableReceive(void);

uint16_t sli_802154phy_radio_unpack_chip_errors(uint8_t internalChipErrors);
uint8_t sli_802154phy_radio_calculate_link_quality(uint16_t unpackedChipErrors, uint16_t packetLength);
uint8_t sli_802154phy_radio_convert_rssi_to_ed(int8_t rssiDbm);
int8_t sli_802154phy_radio_energy_detection(void);

uint32_t sli_802154phy_get_mac_timer(void);

bool emPhyIsTransmitting(void);
void emPhyCancelTransmit(void);

void sli_zigbee_packet_received_internal_isr_callback(uint8_t *packet,
                                                      uint8_t length,
                                                      bool framePendingSetInTxAck);

void emRadioEnableOverflowNotification(bool enable);
bool emRadioOverflowNotificationEnabled(void);

void emRadioEnableSfdSentNotification(bool enable);
bool emRadioSfdSentNotificationEnabled(void);

void sli_802154phy_radio_tx_ack_isr_callback(void);

// Called by the PHY to implement the radio off when idle function.
// Note:  doesn't check to see if we are in the middle of a receive.
void sli_802154phy_radio_ok_to_idle(void);

// Returns the state that the radio will be in when idle.
RadioPowerMode sli_802154phy_radio_get_idle_mode(void);

// Sets the state that the radio will be in when idle.
// Note:  Applies state immediately (will wake up or sleep the radio).
sl_status_t sli_802154phy_radio_set_idle_mode(RadioPowerMode mode);

// Enables or disables mode where energy is spread during Mod DAC calibration.
void emRadioCalModDacWithSpreading(bool enable);

#endif //__EM250CLASS_PHY_H__
