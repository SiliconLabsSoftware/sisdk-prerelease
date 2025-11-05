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
#ifndef __PHY_H__
#define __PHY_H__
#include "mac-phy.h"

#if (defined(CORTEXM3_EFR32) && !defined(CORTEXM3_EFR32_MICRO))
// UC only seems to use CORTEXM3_EFR32 form
#define CORTEXM3_EFR32_MICRO 1
#endif

#if     (defined(PHY_DUAL) || defined(PHY_SIMULATION_DUAL))
  #include "dual/phy.h"
#endif//(defined(PHY_DUAL) || defined(PHY_SIMULATION_DUAL))

#ifndef DUAL_DCL
  #define DUAL_DCL(var)       var
  #define DUAL_CBG(var)       var
  #define DUAL_CBS(var, type) var
  #define DUAL_GET(var)       var
  #define DUAL_GET_AND(var)   var
  #define DUAL_INC(var)       ((var)++)
  #define DUAL_SET(var, type) var
#endif//DUAL_DCL

#include "phy-appended-info.h"

// IS_VALID_CHANNEL() is really meant to be used only within a channel page.
// For a full-fledged macPgChan channel-page-plus-channel-encoded value, use
// (sli_802154phy_get_phy_channel(macPgChan) != INVALID_CHANNEL) instead to vet validity.
#define IS_VALID_CHANNEL(newValue)                       \
  (((newValue) <= SL_ZIGBEE_MAX_802_15_4_CHANNEL_NUMBER) \
   && ((SL_ZIGBEE_MIN_802_15_4_CHANNEL_NUMBER == 0)      \
       || ((newValue) >= SL_ZIGBEE_MIN_802_15_4_CHANNEL_NUMBER)))

#define IS_VALID_CHANNEL_INDEX(newValue) \
  ((newValue) < SL_ZIGBEE_NUM_802_15_4_CHANNELS)

// IEEE 802.15.4-2003 6.1.6:
#define IEEE_154_PER_TEST_PHY_PAYLOAD_LENGTH 20

// where do error codes go?
#define CCA_BUSY 0
#define CCA_CLEAR 1
#define CCA_BUSY_RX_ACTIVE 2
#define CCA_BUSY_RX_ACTIVE_RSSI 3
#define CCA_BUSY_TX_ACTIVE 4
#define LQI_DONT_USE 0

// An int8_t value that represents an invalid RSSI energy level.
// Maybe someday we'll need to make it public in sl_zigbee_types.h.
#define SL_ZIGBEE_PHY_INVALID_RSSI -128

//------------------------------------------------------------------
// 15.4 MAC Frame Control and Information Element fields move to
// separate file for clients that need only those macros.
//------------------------------------------------------------------
#include "ieee802154mac.h"

//------------------------------------------------------------------------
// 15.4 Commands used by phy code
#define DATA_REQUEST_COMMAND 0x04

//------------------------------------------------------------------
// 15.4g 16-bit PHY Header (PHR) definitions -- in host order
#define PHR_154G_MODE_SWITCH_MASK        0x8000u
#define PHR_154G_MS_PARAMETER_ENTRY_MASK 0x6000u
#define PHR_154G_MS_FEC_MASK             0x1000u
#define PHR_154G_MS_NEW_MODE_MASK        0x0FE0u
#define PHR_154G_MS_CHECKSUM_MASK        0x001Eu
#define PHR_154G_MS_PARITY_MASK          0x0001u
#define PHR_154G_FCS_MASK                0x1000u
#define PHR_154G_DW_MASK                 0x0800u
#define PHR_154G_FRAME_LENGTH_MASK       0x07FFu

//------------------------------------------------------------------
// Miscellaneous PHY functions.

// This function stores the 64-bit EUI.  It relies on the value already having
// been stored in the sli_802154mac_local_eui64 variable.  For some implementations,
// this variable may be memory-mapped over the appropriate registers, in
// which case this function may evaluate to whitespace as the work is already
// done.
void sli_802154phy_set_eui64(void);

void emPhyTick(bool isrContext);

//------------------------------------------------------------------
// PHY Power Management.

// Turns the radio power completely off immediately.
void sli_802154phy_radio_sleep(void);

// Wakes the radio up from sleep and puts it in RX mode.
void sli_802154phy_radio_wake_up(void);

// Returns the current power status of the radio.
RadioPowerMode sli_802154phy_radio_get_power_status(void);

//------------------------------------------------------------------
// Radio configuration (for PHYs that support this)

// APIs for dealing with MAC vs. PHY channel numbering
#define INVALID_CHANNEL 0xFF

#if     PHY_RAIL
#include "rail.h"
typedef struct EmPhyRadioConfig {
  RAIL_Status_t (*radioConfig)(RAIL_Handle_t railHandle);
  RAIL_Status_t (*irCal)(RAIL_Handle_t railHandle, uint32_t *imageRejection);
} EmPhyRadioConfig;
#else//!PHY_RAIL
typedef uint8_t EmPhyRadioConfig;
#endif//PHY_RAIL

typedef struct EmPhySubBandParams {
  uint8_t lbtBackoffSym;      // aLBTGranularity
  uint8_t lbtBackoffMinExp;   // Random Backoff        LBT_OFFSET
  uint8_t lbtBackoffMaxExp;   // = rand(2^[lbtBackoffMinExp..lbtBackoffMaxExp]
  int8_t lbtBackoffAdjust;    //        - 1 + lbtBackoffAdjust) * lbtBackoffSym
  uint8_t lbtMinBackoffs;     // Minimum backoffs required (usually 0)
  int8_t lbtTries;            // aLBTMaxTxRetries + 1 (negate if no backoff on 1st try)
  uint16_t lbtTimeoutSym;     // aLBTTimeout
  uint16_t lbtCcaSym;         // aLBTMinFree           LBT_CCA
  uint16_t dutyTxOffSym;      // aLBTTxMinOff          LBT_BACKOFF
  uint16_t dutyTxMaxPktByte;  // aLBTTxMaxPKT          LBT_MAXTX
  uint16_t dutyTxMaxSeqByte;  // aLBTMaxDlg            LBT_MAXDIALOGUE
  uint8_t pad[1];             //(alignment pad for future use)
  uint8_t ackMinSym;          // aLBTAckWindowStart    LBT_MINACK
  uint16_t ackMaxSym;         // aLBTAckWindow+ACKtime LBT_MAXACK
} EmPhySubBandParams;

typedef struct EmPhySubBandConfig {
  uint8_t minPhyChan;         // Lowest PHY channel for sub-band
  uint8_t maxPhyChan;         // Highest PHY channel for sub-band
  int8_t txMinPowerdBm;       // Minimum Tx Power allowed in sub-band
  int8_t txMaxPowerdBm;       // Maximum Tx Power allowed in sub-band
  int8_t edThresholddBm;      // Energy-Detect threshold
  uint8_t lbtDutyDenom;       // aDUTYCYCLERegulated   LBT_MAXTX1HR
  uint8_t criDutyDenom;       // aDUTYCYCLECriticalThresh
  uint8_t nonDutyDenom10;     // non-LBT duty cycle denom / 10
 #if     MAC_HAS_CHANNEL_PAGES
  uint8_t macChanPage;        // MAC Channel Page mapping for sub-band
  uint8_t macChanOffset;      // MAC Channel Page offset of 1st channel
  uint8_t pad[2];             //(alignment pad for future use)
 #endif//MAC_HAS_CHANNEL_PAGES
  const EmPhySubBandParams* subBandParams; // Pointer to sub-band parameters
  const uint8_t* ccaConfigArray; // NULL,emPhyCcaCsmaConfigArray,emPhyCcaLbtConfigArray
} EmPhySubBandConfig;

typedef struct EmPhyBandConfig {
  uint8_t minPhyChan;         // Lowest PHY channel for band
  uint8_t maxPhyChan;         // Highest PHY channel for band
  uint8_t pad[2];             //(alignment pad for future use)
  const uint8_t* bandName;    // Displayable name for the band
  uint32_t baseFreqHz;        // Base frequency of band, in Hz
  uint32_t chanFreqHz;        // Channel bandwidth (separation frequency), in Hz
  int32_t radioXoFreq;        // Radio Xtal frequency from radio configuration
  uint32_t dataRateBps;       // Data rate in bits per second
  uint8_t symbolBits;         // Number of bits in a symbol
  uint8_t preambleBits;       // Preamble bits
  uint8_t sfdBits;            // Sync word bits
  uint8_t numSubBandConfigs;  // Number of sub-band configs
  const EmPhySubBandConfig* subBandConfigs; // Pointer to sub-band configs
  const EmPhyRadioConfig* radioConfigArray; // Pointer to generated radio config
} EmPhyBandConfig;

typedef struct EmPhyTxPowerLevel {
  int8_t dBm;
  uint8_t regValue;
} EmPhyTxPowerLevel;

typedef struct EmPhyTxPA {
  const uint8_t* paName;      // Printable name for the PA
  uint8_t paSelEnum;          // PROP_PA_MODE_PA_SEL_ENUM_*
  uint8_t numPowerLevels;     // Number of powerLevels
  const EmPhyTxPowerLevel* powerLevels;
} EmPhyTxPA;

typedef struct EmPhyConfig {
  const uint8_t* phyName;     // Printable name for the PHY
  uint8_t rssidBmOffset;      // RSSI to dBm arithmetic conversion factor (subtracted)
  uint8_t pad[1];             //(alignment pad for future use)
  uint8_t numBands;           // Number of bands in bandConfigs[] for this PHY
  uint8_t numPAs;             // Number of Tx PAs for this PHY
  const EmPhyBandConfig* const* bandConfigs; // Pointer to band configs
  const EmPhyTxPA* PAs;     // Pointer to PA descriptors
} EmPhyConfig;

// The "public" structure for Duty Cycle information
typedef struct EmPhyDutyCycleParams {
  uint8_t lbtRdcDenom;        // Regulatory duty cycle denominator w/  LBT
  uint8_t criRdcDenom;        // Critical threshold level of lbtRdcDenom
  uint8_t nonRdcDenom10;      // Regulatory duty cycle denominator w/o LBT / 10
  uint8_t lbtFlags;           // Flags, e.g. lbtParams are LBT vs. CSMA
  uint32_t lbtMinOffTimeUs;   // Min post-LBT-Tx time the channel must be quiet
  uint16_t lbtCcaTimeUs;      // LBT CCA time
 #define   maxSeqDeadTimeUs lbtCcaTimeUs // Max inter-packet deadtime to stay in sequence
  uint16_t maxSeqBytes;       // Max # bytes allowed in a dialog sequence
  uint16_t maxPktBytes;       // Max # bytes allowed per transmission
  uint8_t byteTimeUs;         // # microseconds per byte
  uint8_t pktOvhBytes;        // Per-packet transmit overhead in bytes
  const EmPhySubBandParams* lbtParams; // LBT parameters
} EmPhyDutyCycleParams;

//------------------------------------------------------------------
// Transmit APIs.

#define RADIO_CCA_ATTEMPT_MAX_DEFAULT      5 // 1 + macMaxCSMAAttempts' retries
#define RADIO_BACKOFF_EXPONENT_MIN_DEFAULT 3
#define RADIO_BACKOFF_EXPONENT_MAX_DEFAULT 5
#define RADIO_MINIMUM_BACKOFF_DEFAULT      0

#define RADIO_TRANSMIT_CONFIG_NOCCA_DEFAULTS                     \
  {                                                              \
    true,  /* waitForAck */                                      \
    false, /* checkCca */                                        \
    RADIO_CCA_ATTEMPT_MAX_DEFAULT,      /* ccaAttemptMax */      \
    RADIO_BACKOFF_EXPONENT_MIN_DEFAULT, /* backoffExponentMin */ \
    RADIO_BACKOFF_EXPONENT_MAX_DEFAULT, /* backoffExponentMax */ \
    RADIO_MINIMUM_BACKOFF_DEFAULT,      /* minimumBackoff */     \
    true  /* appendCrc */                                        \
  }

extern RadioTransmitConfig radioTransmitConfig;

// the mac will call this function
// -memptr must point to a phy packet.
// -memptr[0] (length byte) must be set to the value that is to be transmitted
//  over the air.
//  This function does not modify the length byte to reflect the two CRC bytes
//  that the phy appends.
sl_status_t emRadioTransmit(uint8_t* memptr);

sl_status_t emRadioPrepareTransmit(uint8_t* memptr);

// the mac will implement this function
extern void sli_802154phy_radio_transmit_complete_callback(sl_status_t status,
                                                           uint32_t sfdSentTime,
                                                           uint8_t framePending);

// Returns true if radio needs calibrating; false otherwise.
// This function must not be called while a transmit is in progress.
bool sli_802154phy_radio_check_radio(void);
void sl_802154_calibrate_current_channel(void);

// Setters and getters for radio node id and pan id.
sl_802154_short_addr_t sli_802154mac_radio_get_node_id(void);
void sli_802154phy_radio_set_node_id(uint16_t nodeId);
sl_802154_pan_id_t sli_802154phy_radio_get_pan_id(void);
void sli_802154phy_radio_set_pan_id(uint16_t panId);

// This API returns INVALID_CHANNEL if the passed channel is invalid.
uint8_t sli_802154phy_get_phy_channel(uint8_t macPgChan);

#define PHY_INVALID_FREQ_HZ 0xFFFFFFFFul
uint32_t emPhyGetChannelFreqHz(uint8_t macPgChan);

// Setters and getters for radio channel and power.
sl_status_t sli_802154phy_set_phy_radio_channel(uint8_t radioChannel);
uint8_t sli_802154phy_get_phy_radio_channel(void);
sl_status_t sli_802154phy_set_phy_radio_power(int8_t power);
int8_t sli_802154phy_get_phy_radio_power(void);
// Set floating point radio power
sl_status_t sli_802154phy_set_phy_radio_powerFl(int16_t power);
int16_t sli_802154phy_get_phy_radio_powerFl(void);

uint16_t emGetTxPowerMode(void);

uint8_t emRadioGetCca(void);

// Gets a random number out of chips that support this. The method of obtaining
// and quality of the random varies by chip family. For high security
// applications, please verify that that the generator's properties match your
// needs.
//
// Calling this function  will typically take a while, so some applications may
// choose to use its output only to seed a separate PRNG rather than as a source
// of random numbers for regular use.
//
// Note that on an EM3xx, the first call to this function after reset will take
// much longer than subsequent calls if emPrngReseed was not called first. The
// watchdog, if enabled, may need to be reset before and after. Similarly,
// passing a large count can cause the call to take a long time to complete.
// Other that the first all, EM3xx chips forward this call to a deterministic
// (by default) CSPRNG. To instead add fresh entropy each time, please use
// emPrngGetRandomNumbers and pass true as the last parameter.
//
// Returns true if the call succeeds, and false otherwise.
bool sli_802154phy_radio_get_random_numbers(uint16_t *rn, uint8_t count);

// Old interface which only gets a single uint16_t.
#define emRadioGetRandomNumber(rn) \
  (sli_802154phy_radio_get_random_numbers((rn), 1))

void emRadioEnableAddressMatching(uint8_t enable);
uint8_t emRadioAddressMatchingEnabled(void);
void sli_802154mac_radio_enable_auto_ack(bool enable);
bool sli_802154phy_radio_auto_ack_enabled(void);
bool emRadioHoldOffIsActive(void);

void emRadioEnablePacketTrace(bool enable);
bool sli_802154phy_radio_packet_trace_enabled(void);

void emRadioEnableReceiveCrc(bool enable);
bool emRadioReceiveCrcEnabled(void);

void sli_802154phy_start_transmit_stream(void);

void sli_802154phy_stop_transmit_stream(void);

void sli_802154phy_start_transmit_tone(void);

void sli_802154phy_stop_transmit_tone(void);

void emMacCheckIncomingQueue(void);

// Exposed for tests
void emRadioEnableSfdRecvNotification(bool enable);
bool emRadioSfdRecvNotificationEnabled(void);
void emRadioSfdRecvIsrCallback(void);
void emRadioEnableReceiveCompleteInt(bool enable);
bool emRadioReceiveCompleteIntEnabled(void);
bool emRadioReceiveCompleteIntPending(bool acknowledgePending);
void emRadioEnableConfigScheduledCallback(bool enable);
bool emRadioConfigScheduledCallbackEnabled(void);
void emRadioConfigScheduledCallback(bool scheduled);

  #define emRadioPowerFem(powerUp) /* no-op */

#if defined(PHY_EM250)
  #include "em250/phy.h"
#elif defined(PHY_PRO2PLUS)
  #include "pro2class/phy.h"
#elif defined(PHY_TRANSCEIVER_SIM) || defined(PHY_SIMULATION_DUAL)
  #include "transceiver/phy-trx-sim.h"
#elif defined (PHY_RAIL)
  #include "rail/phy.h"
#elif defined (PHY_SIMULATION)
#elif defined (PHY_SIMULATION_LEGACY)
#elif !defined(PHY_NULL)
  #error No radio defined - include ember-config.h
#endif

#if     (PHY_DUAL && !PHY_THIS)
  #include "pro2class/phy.h"
#endif//(PHY_DUAL && !PHY_THIS)

#ifndef SL_ZIGBEE_RADIO_POWER_MODE_ED_ON
  #define SL_ZIGBEE_RADIO_POWER_MODE_ED_ON SL_ZIGBEE_RADIO_POWER_MODE_RX_ON
#endif

// The above per-PHY include file should define the following parameters
// (with example values shown for two representative PHY possibilities):
//                               QPSK       802.15.4            MR-FSK
// Per-PHY Parameter Name        2011       Parameter           2011-g
// ----------------------------  ---------  ------------------  ------
// SL_ZIGBEE_PHY_MAX_PHR_BYTES       1          aMRFSKPHRLength---> 2
// SL_ZIGBEE_PHY_MAX_PAYLOAD_BYTES   127        aMaxPHYPacketSize   2047
// SL_ZIGBEE_PHY_MAX_CRC_BYTES       2          --                  4
// SL_ZIGBEE_PHY_DATA_RATE_BPS       250        kb/s                100
// SL_ZIGBEE_PHY_BIT_TIME_US         4          (1000/data rate)    10
// SL_ZIGBEE_PHY_SYMBOL_BITS         4        8/phySymbolsPerOctet  1
// SL_ZIGBEE_PHY_PREAMBLE_BITS       32 Sym2Bit(phyPreambleSymbolLength)
// SL_ZIGBEE_PHY_SFD_BITS            8  Sym2Bit(phySHRDuration-phyPreambleSymbolLength)
//                          -or-          8*aMRFSKSFDLength---> 16
// SL_ZIGBEE_PHY_BYTE_BITS           8  Sym2Bit(phySymbolsPerOctet) 8
// SL_ZIGBEE_PHY_TURNAROUND_SYMBOLS  12         aTurnaroundTime     1ms / n us/sym
// SL_ZIGBEE_PHY_CCA_SYMBOLS         8          aCCATime            8
//                          -or- 0..1000    phyCCADuration*     0..1000
//                              *For PHYs operating in Japan 920,950MHz band
//
// The per-PHY include file can also optionally override:
// PHY_MAX_SIZE
// SL_ZIGBEE_PHY_SYMBOL_DURATION_US
// SL_ZIGBEE_PHY_ACK_TIMEOUT_SYMBOLS
// SL_ZIGBEE_PHY_BACKOFF_SYMBOLS

#ifndef SL_ZIGBEE_PHY_MAX_PAYLOAD_BYTES
#define SL_ZIGBEE_PHY_MAX_PAYLOAD_BYTES PHY_MAX_SIZE
#endif//SL_ZIGBEE_PHY_MAX_PAYLOAD_BYTES
#if     (SL_ZIGBEE_PHY_MAX_PAYLOAD_BYTES > PHY_MAX_SIZE)
 #warning Limiting SL_ZIGBEE_PHY_MAX_PAYLOAD_BYTES to PHY_MAX_SIZE
 #undef  SL_ZIGBEE_PHY_MAX_PAYLOAD_BYTES
 #define SL_ZIGBEE_PHY_MAX_PAYLOAD_BYTES PHY_MAX_SIZE
#endif//(SL_ZIGBEE_PHY_MAX_PAYLOAD_BYTES > PHY_MAX_SIZE)

// Convenience macros and definitions for converting per-PHY parameters.
// These should work fine for known PHYs yielding values that will fit
// within 16-bit (or larger) signed quantities.
#define SL_ZIGBEE_PHY_BITS_TO_BYTES(bits)     (((bits) + SL_ZIGBEE_PHY_BYTE_BITS - 1) / SL_ZIGBEE_PHY_BYTE_BITS)
#define SL_ZIGBEE_PHY_BITS_TO_SYMBOLS(bits)   (((bits) + SL_ZIGBEE_PHY_SYMBOL_BITS - 1) / SL_ZIGBEE_PHY_SYMBOL_BITS)
#define SL_ZIGBEE_PHY_BYTES_TO_SYMBOLS(bytes) (((bytes) * SL_ZIGBEE_PHY_BYTE_BITS) / SL_ZIGBEE_PHY_SYMBOL_BITS)
#define SL_ZIGBEE_PHY_BITS_TO_US(bits)        ((bits) * SL_ZIGBEE_PHY_BIT_TIME_US)
#define SL_ZIGBEE_PHY_BYTES_TO_US(bytes)      ((bytes) * SL_ZIGBEE_PHY_BITS_TO_US(SL_ZIGBEE_PHY_BYTE_BITS))
#define SL_ZIGBEE_PHY_SYMBOL_TIME_US          SL_ZIGBEE_PHY_BITS_TO_US(SL_ZIGBEE_PHY_SYMBOL_BITS)
#define SL_ZIGBEE_PHY_SYMBOLS_TO_US(symbols)  ((symbols) * SL_ZIGBEE_PHY_SYMBOL_TIME_US)
// Some PHYs separate actual Symbol 'time' from 'duration' where the former
// is actual time on the air, and the latter is a worst-case duration applied
// to CSMA, CCA, and MAC timing/timeouts.  Otherwise treat 'em the same.
#ifndef SL_ZIGBEE_PHY_SYMBOL_DURATION_US
#define SL_ZIGBEE_PHY_SYMBOL_DURATION_US      SL_ZIGBEE_PHY_SYMBOL_TIME_US
#endif//SL_ZIGBEE_PHY_SYMBOL_DURATION_US
#define SL_ZIGBEE_PHY_SYMBOLS_TO_DURATION_US(symbols) \
  ((symbols) * SL_ZIGBEE_PHY_SYMBOL_DURATION_US)
#define SL_ZIGBEE_PHY_CCA_DURATION_US         SL_ZIGBEE_PHY_SYMBOLS_TO_DURATION_US(SL_ZIGBEE_PHY_CCA_SYMBOLS)
#define SL_ZIGBEE_PHY_TURNAROUND_DURATION_US  SL_ZIGBEE_PHY_SYMBOLS_TO_DURATION_US(SL_ZIGBEE_PHY_TURNAROUND_SYMBOLS)
#define SL_ZIGBEE_PHY_ACK_TIMEOUT_DURATION_US SL_ZIGBEE_PHY_SYMBOLS_TO_DURATION_US(SL_ZIGBEE_PHY_ACK_TIMEOUT_SYMBOLS)
// 802.15.4-2011g Section 6.4.3:
// macAckWaitDuration = aUnitBackoffPeriod + aTurnaroundTime
//   + phySHRDuration + ceil((PHR+FCF+Seq#+CRC) * phySymbolsPerOctet)
// where:
// aUnitBackoffPeriod = aTurnaroundTime + aCCATime (or phyCCADuration)
// phySHRDuration     = PreambleDuration + SFDDuration
#ifndef SL_ZIGBEE_PHY_BACKOFF_SYMBOLS
#define SL_ZIGBEE_PHY_BACKOFF_SYMBOLS (0                                  \
                                       + SL_ZIGBEE_PHY_TURNAROUND_SYMBOLS \
                                       + SL_ZIGBEE_PHY_CCA_SYMBOLS        \
                                       )
#endif//SL_ZIGBEE_PHY_BACKOFF_SYMBOLS
#ifndef SL_ZIGBEE_PHY_ACK_TIMEOUT_SYMBOLS
#define SL_ZIGBEE_PHY_ACK_TIMEOUT_SYMBOLS (0                                                                  \
                                           + SL_ZIGBEE_PHY_BACKOFF_SYMBOLS                                    \
                                           + SL_ZIGBEE_PHY_TURNAROUND_SYMBOLS                                 \
                                           + SL_ZIGBEE_PHY_BITS_TO_SYMBOLS(SL_ZIGBEE_PHY_PREAMBLE_BITS)       \
                                           + SL_ZIGBEE_PHY_BITS_TO_SYMBOLS(SL_ZIGBEE_PHY_SFD_BITS)            \
                                           + ((SL_ZIGBEE_PHY_MAX_PHR_BYTES + 3 + SL_ZIGBEE_PHY_MAX_CRC_BYTES) \
                                              * SL_ZIGBEE_PHY_BITS_TO_SYMBOLS(SL_ZIGBEE_PHY_BYTE_BITS))       \
                                           )
#endif//SL_ZIGBEE_PHY_ACK_TIMEOUT_SYMBOLS

#define emRadioReseed() (true)
#define emRadioPartialReseed() (true)

#if     defined(PHY_DUAL) || defined(PHY_SIMULATION_DUAL)
// This API is the safe one to use
extern uint32_t emPhySymbolsToUs(uint32_t symbols);
#else//!PHY_DUAL || !PHY_SIMULATION_DUAL
// Can safely map this to the macro in single-PHY case
  #define emPhySymbolsToUs(symbols) SL_ZIGBEE_PHY_SYMBOLS_TO_US(symbols)
#endif//PHY_DUAL || PHY_SIMULATION_DUAL

// On the ZIP stack all ember... / mfglib... APIs are renamed to
// emApi... / mfglibApi... due to RTOS namespace conflicts.
// This change doesn't apply to ZNET.
  #define emApiSetTxPowerMode sli_802154_stack_set_tx_power_mode
  #define mfglibApiStart mfglibStart
  #define mfglibApiEnd mfglibEnd
  #define mfglibApiStartTone mfglibStartTone
  #define mfglibApiStopTone mfglibStopTone
  #define mfglibApiStartStream mfglibStartStream
  #define mfglibApiStopStream mfglibStopStream
  #define mfglibApiSendPacket mfglibSendPacket
  #define mfglibApiSetChannel mfglibSetChannel
  #define mfglibApiGetChannel mfglibGetChannel
  #define mfglibApiSetPower mfglibSetPower
  #define mfglibApiGetPower mfglibGetPower
  #define mfglibApiGetPowerMode mfglibGetPowerMode
  #define mfglibApiSetSynOffset mfglibSetSynOffset
  #define mfglibApiGetSynOffset mfglibGetSynOffset
  #define mfglibApiSetOptions mfglibSetOptions
  #define mfglibApiGetOptions mfglibGetOptions

// Map some internally-consistent names with less consistent API naming
#define sli_802154phy_get_radio_channel     sli_802154phy_get_phy_radio_channel
#define emPhyGetRadioPower       sli_802154phy_get_phy_radio_power
#define emPhySetRadioChannel     sli_802154phy_set_phy_radio_channel
#define emPhySetRadioPower       sli_802154phy_set_phy_radio_power
#define emPhyStartTransmitStream sli_802154phy_start_transmit_stream
#define emPhyStartTransmitTone   sli_802154phy_start_transmit_tone
#define emPhyStopTransmitStream  sli_802154phy_stop_transmit_stream
#define emPhyStopTransmitTone    sli_802154phy_stop_transmit_tone

#ifdef  CORTEXM3_EFR32_MICRO
// Provide some em3xx-like compatibility macros for certain test code
// for converting microseconds to sli_802154phy_get_mac_timer()'s ticks.
  #define MAC_TIMER (sli_802154phy_get_mac_timer())
 #ifdef  PHY_RAIL
// The RAIL timer ticks every 1us.
  #define MAC_TIMER_US(ticks)  (ticks)
  #define MAC_TIMER_TICKS(microseconds) (microseconds)
  #define MAC_TIMER_MAC_TIMER_MASK 0xFFFFFFFFul
 #else//!PHY_RAIL
// The PROTIMER ticks every 2us.
  #define MAC_TIMER_US(ticks)  ((ticks) * 2)
  #define MAC_TIMER_TICKS(microseconds) (((microseconds) + 1) / 2)
  #define MAC_TIMER_MAC_TIMER_MASK (PROTIMER->WRAPCNTTOP)
 #endif//PHY_RAIL
#endif//CORTEXM3_EFR32_MICRO

#endif //__PHY_H__
