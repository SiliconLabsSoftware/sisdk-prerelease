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

#ifndef PHY_USE_PREPARE_TX_API
#define PHY_USE_PREPARE_TX_API 1
#endif//PHY_USE_PREPARE_TX_API

#ifndef PHY_ACK_AT_MAX_POWER
#define PHY_ACK_AT_MAX_POWER 1  // Default to enable ACK at max power
#endif//PHY_ACK_AT_MAX_POWER

#ifndef FP_DATA_POLL_COMPLY
#define FP_DATA_POLL_COMPLY 0   // Default to not limit frame-pending to only data polls
#endif//FP_DATA_POLL_COMPLY

#ifndef TX_WAIT_CCA_MIN_1
#define TX_WAIT_CCA_MIN_1 0     // Default to not wait this way
#endif//TX_WAIT_CCA_MIN_1

#ifndef TX_WAIT_RX_ACK_SENT
#define TX_WAIT_RX_ACK_SENT 0   // Default to not wait this way
#endif//TX_WAIT_RX_ACK_SENT

#ifndef SI4440_1087_TOLERATE
#define SI4440_1087_TOLERATE 1 // Default to workaround
#endif//SI4440_1087_TOLERATE

#ifndef SI4440_1442_TOLERATE
#define SI4440_1442_TOLERATE 1 // Default to workaround
#endif//SI4440_1442_TOLERATE

#ifndef SI4440_1461_TOLERATE
#define SI4440_1461_TOLERATE (PRO2_FW_API <  0xFFFFFFFFul)
#endif//SI4440_1461_TOLERATE

#ifndef SI4440_1462_TOLERATE
#define SI4440_1462_TOLERATE 1 // Default to set GPIO high
#endif//SI4440_1462_TOLERATE

#ifndef SI4440_1468_TOLERATE
#define SI4440_1468_TOLERATE (PRO2_FW_API <  0xFFFFFFFFul)
#endif//SI4440_1468_TOLERATE

#ifndef SI4440_1672_TOLERATE
// Default to tolerate this in ROM5 only when LP < MP per ROM6 fix
//TODO: Remove this when/if this bug is fixed in ROM5/patches
#define SI4440_1672_TOLERATE (PROP_PA_MODE_PA_SEL_ENUM_LP < PROP_PA_MODE_PA_SEL_ENUM_MP)
#endif//SI4440_1672_TOLERATE
#define SI4440_1672_FIX_FWREV 0x06000800ul // Fixed at 6.0.8

#ifndef SI4440_1734_TOLERATE
#define SI4440_1734_TOLERATE (PRO2_FW_API <  0xFFFFFFFFul)
#endif//SI4440_1734_TOLERATE

#ifndef RAIL_LIB_4848_TALK_CCA
#define RAIL_LIB_4848_TALK_CCA 0 // Disable 'til integrated as LBT retry in duty-cycle.c
#endif//RAIL_LIB_4848_TALK_CCA

#ifndef RAIL_LIB_4848_CCA_RX
#define RAIL_LIB_4848_CCA_RX 0 // Disable 'til exhaustively tested
#endif//RAIL_LIB_4848_CCA_RX

#ifndef NO_RADIO_CONFIG
#define NO_RADIO_CONFIG 0 // Default to enable radio configurations
#endif//NO_RADIO_CONFIG

#ifdef  PTI_LOG // Override PHY_PTI_LOG_DEFAULT
#define PHY_PTI_LOG_DEFAULT PTI_LOG
#endif//PTI_LOG // Override PHY_PTI_LOG_DEFAULT

#define LEGACY_PHY_BUILD 1
#undef PHY_RAIL
#include "phy/hal.h"
#include "core/sl_zigbee_stack.h"
#include "mac-child.h"
#include "indirect-queue.h"

#include "stack/framework/eui64.h"      // For sli_zigbee_init_eui64() and sli_802154mac_local_eui64{}
#include "phy/phy.h"
#include "phy/pro2class/sipro2.h"                    // Utilize this API
#include "phy/plugin/duty-cycle/duty-cycle.h" // For emPhyDcTick()

#include "phy/pro2class/typedef.h" // To map PRO2+ auto-gen types according to our environment
#include "phy/pro2class/prop_map.h"
#include "phy/pro2class/sixxxx_cmd.h"   // DO NOT RELEASE OUTSIDE
#include "phy/pro2class/sixxxx_prop.h"  // DO NOT RELEASE OUTSIDE

#undef   ADC_CFG  // conflict
#undef   GPIO     // conflict
#undef   RAM_TEST // conflict
#undef   CRC      // conflict on EFR32
#pragma pack (1)
#include "phy/pro2class/cmd_map.h"
#pragma pack ()

#define OTPID_ROM6_ie154 0x8602 // RAM Patch applies when <= this OTPID

static const uint8_t radioRamPatchImage446xRom6Ie154[] = {
  #include "phy/pro2class/rom6/ie154_0x8602.h"
  0
};

#ifndef PRO2_RADIO_BOOTMODE // Allow overriding this during build
#define PRO2_RADIO_BOOTMODE (CMD_POWER_UP_ARG_BOOT_OPTIONS_FUNC_ENUM_IE154 \
                             | CMD_POWER_UP_ARG_BOOT_OPTIONS_PATCH_MASK)
#endif//PRO2_RADIO_BOOTMODE
static uint8_t pro2RadioBootMode = PRO2_RADIO_BOOTMODE;

#include "radio-internal-config.h"
static const uint8_t radioInternalConfigArray[] =
  RADIO_INTERNAL_CONFIGURATION_DATA_ARRAY;

const uint8_t emPhyCcaCsmaConfigArray[] = {
  //TODO: Fill this in with CSMA configuration switch
  0x00
};
const uint8_t emPhyCcaLbtConfigArray[] = {
  //TODO: Fill this in with LBT configuration switch
  0x00
};

// In the PRO stack we need to write the network index in the appended info.
// The network index is obtained by calling
// sli_mac_get_current_radio_network_index().
#include "upper-mac.h"
#if     PHY_ACK_AT_MAX_POWER // Subvert MAC multi-network for PHY multi-power
  #define PRO2_MAX_NETWORKS 2 // just need primary and a max-power slot same channel
  #define emPhyCurrentRadioNetworkIndex() (1)
  #define emPhyMaxPowerNetworkIndex()     (0)
#else//!PHY_ACK_AT_MAX_POWER
  #define PRO2_MAX_NETWORKS 4 // #slots in PROP_IE154_RX_PAN_NODE_CHANNEL_ID_SET
  #define emPhyCurrentRadioNetworkIndex() sli_mac_get_current_radio_network_index(0) //mac_index
#endif//PHY_ACK_AT_MAX_POWER

sl_status_t sli_802154_stack_set_tx_power_mode(uint16_t txPowerMode);

#ifndef offsetof
#define offsetof(type, field)  ((int) &((type *) 0)->field)
#endif//offsetof

#define PRO2PLUS_SYSCLK_STABLE_US   0 //TODO: Adjust accordingly
// Per SI4440-1687, IDLE to ED starting to sample: 110 us + 4Tb
#define PRO2PLUS_START_ED_DELAY_US  (110 + SL_ZIGBEE_PHY_BITS_TO_US(4))

#define PRO2_APPENDED_INFO_BYTES 8

#define MAC_INTERRUPTS_OFF()  do { \
    siPro2IntDisable();            \
} while (0)
#define MAC_INTERRUPTS_ON()   do { \
    siPro2IntEnable();             \
} while (0)
#define MAC_ATOMIC(blah)      do { \
    MAC_INTERRUPTS_OFF();          \
    { blah; }                      \
    MAC_INTERRUPTS_ON();           \
} while (0)
#define ACK_ALL 0xFF // Acknowledge all pended interrupts
#define ALL_FLAGS 0xFF // Set all flags

#define elapsedTimeMacTimer(oldTime, newTime) \
  ((uint32_t) ((uint32_t)(newTime) - (uint32_t)(oldTime)) & MAC_TIMER_MAC_TIMER_MASK)

#define NO_TX_ERRORS 0
#define ANY_TX_ERROR CMD_IE154_GET_INT_STATUS_REP_IE154_TX_ERR_STATUS_TX_ABORTED_MASK

typedef struct reply_IE154_GET_INT_STATUS_map Events;
#define intPend   IE154_INT_PEND
#define intStatus IE154_INT_STATUS
#define txPend    IE154_TX_PEND
#define txStatus  IE154_TX_STATUS
#define txAck     txStatus // Overlay for acking
#define txError   IE154_TX_ERR_STATUS
#define rxPend    IE154_RX_PEND
#define rxStatus  IE154_RX_STATUS
#define rxAck     rxStatus // Overlay for acking
#define rxError   IE154_RX_ERR_STATUS
#define chPend    IE154_CHIP_PEND
#define chStatus  IE154_CHIP_STATUS
#define chAck     chStatus // Overlay for acking
#define txAvail   TX_FIFO_SPACE
#define rxAvail   RX_FIFO_COUNT

static uint16_t emRadioTxPowerMode = SL_ZIGBEE_TX_POWER_MODE_USE_TOKEN;
static bool pro2Listening = false;
static bool phy2Listening = false;
static bool pro2ForceCalibration = false;
typedef enum {
  PHY_TX_STATE_PREP_W_ACK  = -2,
  PHY_TX_STATE_PREP_NOACK  = -1,
  PHY_TX_STATE_OFF         =  0,
  PHY_TX_STATE_TRIG_NOACK  =  1,
  PHY_TX_STATE_TRIG_W_ACK  =  2,
  PHY_TX_STATE_TRIG_ACKING =  3,
} EmPhyTxState;
static volatile EmPhyTxState emPhyTxState = PHY_TX_STATE_OFF;
static bool radioPostTxListening;
RadioPowerMode radioCurrentPowerStatus = SL_ZIGBEE_RADIO_POWER_MODE_OFF;
static RadioPowerMode radioIdlePowerState = SL_ZIGBEE_RADIO_POWER_MODE_OFF;
static bool pro2PoweredUp = false;

static int8_t pro2RssiOffsetdB = 0;
int8_t emSynthFreqOffset; //TODO: Not implemented but referenced by mfglib
bool emRadioAlwaysUseZeroBackoff = false;
uint8_t emRadioLastOutgoingCommand = 0xff; // To sate MAC layer; not used herein
static RadioTransmitConfig pro2TxCsmaConfig;
static int8_t pro2TxCsmaBackoffAdjust = 0;
// Replace pro2Transmitconfig.checkCca field with emRadioAlwaysUseZeroBackoff
#define useZeroBackoff checkCca

static uint8_t txCmdBuf[sizeof(struct cmd_IE154_START_TX_map)];

static uint8_t rxBuffer[NUM_RX_BUFFER_BYTES + 20 /*safety and back-to-back frames*/];
// RX_BUFFER_FIFO_SPACE says how much of rxBuffer[] can be used
// for retrieving PRO2 RX FIFO data -- we need to leave space for
// possibly faking the receipt of CRC bytes which the PRO2 doesn't
// provide, the pad byte to align appended info, and appended info
// itself.
#define RX_BUFFER_FIFO_SPACE (sizeof(rxBuffer)                                       \
                              - 4/*MaxCRC*/                                          \
                              - 1/*Pad*/                                             \
                              - (NUM_APPENDED_INFO_BYTES - PRO2_APPENDED_INFO_BYTES) \
                              )
static uint16_t rxBufLen = 0; // Number of valid bytes in rxBuffer[]
static int16_t bytesToWrite; // Tracking Tx progress across limited PRO2 TX FIFO
static uint8_t* txBufPending;
static bool framePendingSetInOutgoingAck;

uint32_t lastTxSfdTime = 0;
// Avoid lockup due to PRO2+ misbehavior -- give each transmit
// 1 second to complete and if we don't hear from PRO2+ that it
// has, abort it so we don't lock up our or MAC-layer's Tx state
// machine.
#define MAC_TX_TIMEOUT_MS 1000
static volatile int32_t lastTxStartedTimeMs = -1;
//TODO: Initialize these to a time in the distant past so lbtMinOffTime
//TODO: is not applied on first transmit after bootup?
static volatile uint32_t lastTxFinishedTimeMs;
static volatile uint32_t lastCsmaTxFinishedTimeMs;

extern uint8_t sli_legacy_mfglib_mode;
volatile bool sli_legacy_mfglib_transmit_complete = false;

#define CHANNEL_IN_BAND(phyChan, band)  \
  (((band) != NULL)                     \
   && ((phyChan) >= (band)->minPhyChan) \
   && ((phyChan) <= (band)->maxPhyChan))

#define PRO2_NETWORK_PARAM_DEFAULTS /* PanId  */ { 0xFF, 0xFF }, \
  /* NodeId */ { 0xFF, 0xFE },                                   \
  /* Channel*/ INVALID_CHANNEL,                                  \
  /* Power  */ 0x00,                                   // The default is 0x0F
// but we purposefully choose 0x00 to avoid
// sorting unused slots ahead of used ones.
// This works because we only write full
// entries and not just changed fields.
typedef struct {
  uint8_t panId[2];       // panId
  uint8_t nodeId[2];      // nodeId
  uint8_t phyChan;        // PHY and PRO2+ channel
  uint8_t power;          // in PRO2+ units
} Pro2NetworkParams;
static Pro2NetworkParams phy2NetworkParams[PRO2_MAX_NETWORKS] = {
  { PRO2_NETWORK_PARAM_DEFAULTS },
 #if     (PRO2_MAX_NETWORKS > 1)
  { PRO2_NETWORK_PARAM_DEFAULTS },
 #if     (PRO2_MAX_NETWORKS > 2)
  { PRO2_NETWORK_PARAM_DEFAULTS },
  { PRO2_NETWORK_PARAM_DEFAULTS },
 #endif//(PRO2_MAX_NETWORKS > 2)
 #endif//(PRO2_MAX_NETWORKS > 1)
};
static int8_t phy2TxPowerRequested[PRO2_MAX_NETWORKS] = {
  INVALID_RADIO_POWER,
 #if     (PRO2_MAX_NETWORKS > 1)
  INVALID_RADIO_POWER,
 #if     (PRO2_MAX_NETWORKS > 2)
  INVALID_RADIO_POWER,
  INVALID_RADIO_POWER,
 #endif//(PRO2_MAX_NETWORKS > 2)
 #endif//(PRO2_MAX_NETWORKS > 1)
};
static int8_t phy2TxPowerGranted[PRO2_MAX_NETWORKS] = {
  INVALID_RADIO_POWER,
 #if     (PRO2_MAX_NETWORKS > 1)
  INVALID_RADIO_POWER,
 #if     (PRO2_MAX_NETWORKS > 2)
  INVALID_RADIO_POWER,
  INVALID_RADIO_POWER,
 #endif//(PRO2_MAX_NETWORKS > 2)
 #endif//(PRO2_MAX_NETWORKS > 1)
};
// phy2MacPageChan[] encodes the full macPage value, not just its abbreviation
static uint16_t phy2MacPageChan[PRO2_MAX_NETWORKS] = {
  INVALID_CHANNEL,
 #if     (PRO2_MAX_NETWORKS > 1)
  INVALID_CHANNEL,
 #if     (PRO2_MAX_NETWORKS > 2)
  INVALID_CHANNEL,
  INVALID_CHANNEL,
 #endif//(PRO2_MAX_NETWORKS > 2)
 #endif//(PRO2_MAX_NETWORKS > 1)
};

// Mappings between PRO2+ Network Parameters slots and Multi-Network indexes
static int8_t phy2NetworkParamsSlotForIndex[PRO2_MAX_NETWORKS] = {
  0,
 #if     (PRO2_MAX_NETWORKS > 1)
  1,
 #if     (PRO2_MAX_NETWORKS > 2)
  2,
  3,
 #endif//(PRO2_MAX_NETWORKS > 2)
 #endif//(PRO2_MAX_NETWORKS > 1)
};
static int8_t phy2NetworkParamsIndexForSlot[PRO2_MAX_NETWORKS] = {
  0,
 #if     (PRO2_MAX_NETWORKS > 1)
  1,
 #if     (PRO2_MAX_NETWORKS > 2)
  2,
  3,
 #endif//(PRO2_MAX_NETWORKS > 2)
 #endif//(PRO2_MAX_NETWORKS > 1)
};

#if     (PRO2_MAX_NETWORKS > 2)
static uint8_t listeningPhyChan = INVALID_CHANNEL;
static uint8_t lastTransmitPhyChan = INVALID_CHANNEL;
#endif//(PRO2_MAX_NETWORKS > 2)

#include "routing/zigbee/child.h"

static inline bool emberRadioDataPendingShortIdIsrCallback(uint16_t shortId)
{
  uint8_t childIndex = sli_mac_child_index(shortId);
  bool setFramePending;

  if (0xFF != childIndex) {
    // This device is our child.
    // TODO: sli_mac_child_has_pending_message implementation for SL-Thread.
    setFramePending = sli_mac_child_has_pending_message(childIndex);
  } else {
    // This device is not our child so set frame pending
    setFramePending = true;
  }
  return setFramePending;
}

static inline bool emberRadioDataPendingLongIdIsrCallback(uint8_t *longId)
{
  // TODO: sli_mac_child_has_pending_message implementation for SL-Thread.
  return sli_mac_long_id_data_pending(longId);
}

// Driver's defaults
#define PHY_NETWORK_COORDINATOR_MASK_DEFAULT 0x00

#ifndef PHY_PTI_LOG_DEFAULT // Can override on command line
#define PHY_PTI_LOG_DEFAULT (0                                                      \
                             | (false << PROP_PTI_LOG_EN_PWR_STATE_EN_LSB)          \
                             | (true  << PROP_PTI_LOG_EN_RX_EN_LSB)                 \
                             | (true  << PROP_PTI_LOG_EN_TX_EN_LSB)                 \
                             | (false << PROP_PTI_LOG_EN_CMD_EN_LSB)                \
                             | (false << PROP_PTI_LOG_EN_CAL_MAN_EN_LSB)            \
                             | (false << (8 + PROP_PTI_LOG_EN_2_CCA_EN_LSB))        \
                             | (false << (8 + PROP_PTI_LOG_EN_2_MODE_PHASE_EN_LSB)) \
                             )
#endif//PHY_PTI_LOG_DEFAULT

#define PHY_RX_FLAGS_DEFAULT (0                                                                        \
                              | (true  << PROP_IE154_RX_FLAGS_RX_FILTERED_LSB)                         \
                              | (false << PROP_IE154_RX_FLAGS_FRAME_PENDING_DEFAULT_VALUE_LSB)         \
                              | (true  << PROP_IE154_RX_FLAGS_SRC_ADDR_EVENT_ONLY_ON_ACK_REQUESTS_LSB) \
                              | (false << PROP_IE154_RX_FLAGS_SRC_ADDR_EVENT_DELAY_LSB)                \
                              | (true  << PROP_IE154_RX_FLAGS_AUTO_ACK_DEFAULT_VALUE_LSB)              \
                              | (true  << PROP_IE154_RX_FLAGS_FILTERING_LSB)                           \
                              )

// Take advantage of not having to retrieve expected ACKs from the RX FIFO
#define PHY_TX_FLAGS_DEFAULT (0                                                \
                              | (false << PROP_IE154_TX_FLAGS_ACK_IN_FIFO_LSB) \
                              )

#define PHY_CO_FLAGS_DEFAULT (0                                                         \
                              | (PROP_IE154_COMMON_FLAGS_FRAME_VERSION_FILTER_ENUM_2012 \
                                 << PROP_IE154_COMMON_FLAGS_FRAME_VERSION_FILTER_LSB)   \
                              )

#define PHY_CO_PHCFG_DEFAULT (0                                                     \
                              | (((SL_ZIGBEE_PHY_MAX_PHR_BYTES == 1)                \
                                  ? PROP_IE154_COMMON_PHY_CFG_PHY_VER_ENUM_STD      \
                                  : PROP_IE154_COMMON_PHY_CFG_PHY_VER_ENUM_G)       \
                                 << PROP_IE154_COMMON_PHY_CFG_PHY_VER_LSB)          \
                              | (false << PROP_IE154_COMMON_PHY_CFG_CODING_LSB)     \
                              | (true  << PROP_IE154_COMMON_PHY_CFG_APPEND_CRC_LSB) \
                              | (true  << PROP_IE154_COMMON_PHY_CFG_CHECK_CRC_LSB)  \
                              )

#define IE154G_ACTIVE() ((phy2CoPhCfg & PROP_IE154_COMMON_PHY_CFG_PHY_VER_MASK) \
                         == (PROP_IE154_COMMON_PHY_CFG_PHY_VER_ENUM_G           \
                             << PROP_IE154_COMMON_PHY_CFG_PHY_VER_LSB))

#ifndef DEFAULT_PHR_154G_FEATURES
#if     SL_ZIGBEE_PHY_DATA_WHITENING
  #define DEFAULT_PHR_154G_FEATURES (PHR_154G_FCS_MASK | PHR_154G_DW_MASK)
#else//!SL_ZIGBEE_PHY_DATA_WHITENING
  #define DEFAULT_PHR_154G_FEATURES (PHR_154G_FCS_MASK)
#endif//SL_ZIGBEE_PHY_DATA_WHITENING
#endif//DEFAULT_PHR_154G_FEATURES

uint8_t emPhyPhr154gFeatures = (DEFAULT_PHR_154G_FEATURES >> 8);

#define PHY_L1_EVENTS_CONFIG_DEFAULT (0                                                             \
                                      | (true  << PROP_IE154_INT_CTL_ENABLE_CHIP_INT_STATUS_EN_LSB) \
                                      | (true  << PROP_IE154_INT_CTL_ENABLE_RX_INT_STATUS_EN_LSB)   \
                                      | (true  << PROP_IE154_INT_CTL_ENABLE_TX_INT_STATUS_EN_LSB)   \
                                      )

#define PHY_RX_EVENTS_CONFIG_DEFAULT (0                                                                \
                                      | (true  << PROP_IE154_INT_CTL_RX_ENABLE_RX_ERROR_LSB)           \
                                      | (((SL_ZIGBEE_PHY_MAX_PHR_BYTES                                 \
                                           + SL_ZIGBEE_PHY_MAX_PAYLOAD_BYTES                           \
                                           + PRO2_APPENDED_INFO_BYTES) > PRO2_RX_FIFO_SIZE)            \
                                         << PROP_IE154_INT_CTL_RX_ENABLE_RX_FIFO_ALMOST_FULL_LSB)      \
                                      | (true  << PROP_IE154_INT_CTL_RX_ENABLE_RX_ACK_TX_COMPLETE_LSB) \
                                      | (true  << PROP_IE154_INT_CTL_RX_ENABLE_RX_COMPLETE_LSB)        \
                                      | (true  << PROP_IE154_INT_CTL_RX_ENABLE_RX_SRC_ADDR_LSB)        \
                                      | (false << PROP_IE154_INT_CTL_RX_ENABLE_RX_SFD_LSB)             \
                                      )

#define PHY_TX_EVENTS_CONFIG_DEFAULT (0                                                                   \
                                      | (true  << PROP_IE154_INT_CTL_TX_ENABLE_TX_ERROR_LSB)              \
                                      | (false << PROP_IE154_INT_CTL_TX_ENABLE_TX_FIFO_ALMOST_EMPTY_LSB)  \
                                      | (true  << PROP_IE154_INT_CTL_TX_ENABLE_TX_ACK_RECEIVED_W_FP_LSB)  \
                                      | (true  << PROP_IE154_INT_CTL_TX_ENABLE_TX_ACK_RECEIVED_NO_FP_LSB) \
                                      | (true  << PROP_IE154_INT_CTL_TX_ENABLE_TX_COMPLETE_LSB)           \
                                      | (true  << PROP_IE154_INT_CTL_TX_ENABLE_TX_SFD_SENT_LSB)           \
                                      )

#define PHY_CH_EVENTS_CONFIG_DEFAULT (0                                                                                      \
                                      | (false << PROP_IE154_INT_CTL_IE154_CHIP_ENABLE_CAL_EN_LSB)                           \
                                      | (true  << PROP_IE154_INT_CTL_IE154_CHIP_ENABLE_FIFO_UNDERFLOW_OVERFLOW_ERROR_EN_LSB) \
                                      | (false << PROP_IE154_INT_CTL_IE154_CHIP_ENABLE_STATE_CHANGE_EN_LSB)                  \
                                      | (true  << PROP_IE154_INT_CTL_IE154_CHIP_ENABLE_CMD_ERROR_EN_LSB)                     \
                                      | (false << PROP_IE154_INT_CTL_IE154_CHIP_ENABLE_CHIP_READY_EN_LSB)                    \
                                      | (false << PROP_IE154_INT_CTL_IE154_CHIP_ENABLE_LOW_BATT_EN_LSB)                      \
                                      | (false << PROP_IE154_INT_CTL_IE154_CHIP_ENABLE_WUT_EN_LSB)                           \
                                      )

// phy2 = Driver's defaults
// pro2 = PRO2+ Radio's defaults -- which will be sync'd to driver's
static uint8_t phy2IsCoord  = PHY_NETWORK_COORDINATOR_MASK_DEFAULT;
static uint8_t pro2IsCoord  = PROP_IE154_NETWORK_COORDINATOR_MASK_DEFAULT;

static uint8_t phy2RxFlags  = PHY_RX_FLAGS_DEFAULT;
static uint8_t pro2RxFlags  = PROP_IE154_RX_FLAGS_DEFAULT;

static uint8_t phy2TxFlags  = PHY_TX_FLAGS_DEFAULT;
static uint8_t pro2TxFlags  = PROP_IE154_TX_FLAGS_DEFAULT;

static uint8_t phy2CoFlags  = PHY_CO_FLAGS_DEFAULT;
static uint8_t pro2CoFlags  = PROP_IE154_COMMON_FLAGS_DEFAULT;

static uint8_t phy2CoPhCfg  = PHY_CO_PHCFG_DEFAULT;
static uint8_t pro2CoPhCfg  = PROP_IE154_COMMON_PHY_CFG_DEFAULT;

static uint8_t phy2L1Events = PHY_L1_EVENTS_CONFIG_DEFAULT;
static uint8_t pro2L1Events = PROP_IE154_INT_CTL_ENABLE_DEFAULT;

static uint8_t phy2RxEvents = PHY_RX_EVENTS_CONFIG_DEFAULT;
static uint8_t pro2RxEvents = PROP_IE154_INT_CTL_RX_ENABLE_DEFAULT;

static uint8_t phy2TxEvents = PHY_TX_EVENTS_CONFIG_DEFAULT;
static uint8_t pro2TxEvents = PROP_IE154_INT_CTL_TX_ENABLE_DEFAULT;

static uint8_t phy2ChEvents = PHY_CH_EVENTS_CONFIG_DEFAULT;
static uint8_t pro2ChEvents = PROP_IE154_INT_CTL_IE154_CHIP_ENABLE_DEFAULT;

static uint16_t phy2PtiLog  = PHY_PTI_LOG_DEFAULT;
static uint16_t pro2PtiLog  = (PROP_PTI_LOG_EN_DEFAULT
                               | (PROP_PTI_LOG_EN_2_DEFAULT << 8)
                               );

static uint8_t phy2Channel = INVALID_CHANNEL;
EmPhyBandConfig* emPhyCurrentBandConfig = NULL;
#define phy2BandConfig emPhyCurrentBandConfig
static EmPhySubBandConfig* phy2SubBandConfig = NULL;
static const EmPhySubBandParams* emPhyCurrentSubBandParams = NULL;
#define phy2SubBandParams emPhyCurrentSubBandParams
static const EmPhyTxPA* phy2TxPA = NULL;

// efr32 boards put PTI on GPIO3, RF switch on GPIO2/0, LOW drv strength
WEAK(uint8_t pro2GpioCfgPowerUp[]) = { // Permit an app to override
  8,                  /*Number of bytes in following PRO2+ command*/
  CMD_GPIO_PIN_CFG,
  /*GPIO0*/ CMD_GPIO_PIN_CFG_ARG_GPIO_GPIO_MODE_ENUM_RX_STATE,
  /*GPIO1*/ CMD_GPIO_PIN_CFG_ARG_GPIO_GPIO_MODE_ENUM_CTS,
  /*GPIO2*/ CMD_GPIO_PIN_CFG_ARG_GPIO_GPIO_MODE_ENUM_TX_STATE,
  /*GPIO3*/ CMD_GPIO_PIN_CFG_ARG_GPIO_GPIO_MODE_ENUM_PKT_TRACE,
  /*NIRQ */ CMD_GPIO_PIN_CFG_ARG_GPIO_GPIO_MODE_ENUM_DONOTHING,
  /*SDO  */ CMD_GPIO_PIN_CFG_ARG_GPIO_GPIO_MODE_ENUM_DONOTHING,
  /*GEN  */ (CMD_GPIO_PIN_CFG_ARG_GEN_CONFIG_DRV_STRENGTH_ENUM_LOW
             << CMD_GPIO_PIN_CFG_ARG_GEN_CONFIG_DRV_STRENGTH_LSB),
  0,                  /*Number of bytes in following PRO2+ command*/
};

WEAK(Pro2Gpio phy2PtiGpio) = PRO2_GPIO_3; // Let BOARD_HEADER override
static Pro2Gpio pro2PtiGpio = PRO2_GPIO_NONE;

static int8_t edCcaThreshold = ED_CCA_THRESHOLD_UNINIT;
static uint8_t pro2CcaThreshold = PROP_IE154_TX_CCA_THRESHOLD_DEFAULT;

static struct reply_PART_INFO_map pro2PartInfo;
static struct reply_FUNC_INFO_map pro2FuncInfo;
static struct reply_DIE_INFO_map pro2DieInfo;

//== TRACING DEBUG ==

#ifdef  PHY_TRACE

#define PHY_DEBUG_TRACE_SIZE 1024 // MUST BE POWER-OF-2
static uint8_t phyTraceData[PHY_DEBUG_TRACE_SIZE];
static volatile uint16_t phyTraceHead = 0;
static uint16_t phyTraceTail = 0;
static uint16_t phyTraceLostEvents = 0;
bool emPhyTrace = PHY_TRACE;
static uint8_t phyTracePort = 0;

// Insert a trace buffer entry for an arbitrary datum
void emPhyTraceRaw(uint8_t tag, uint8_t* data, uint8_t len)
{
  if (emPhyTrace) {
    DECLARE_INTERRUPT_STATE;
    DISABLE_INTERRUPTS();
    uint16_t spaceAvail = sizeof(phyTraceData) - 1
                          - ((phyTraceHead - phyTraceTail)
                             & (sizeof(phyTraceData) - 1));
    if ((len + 2 + (phyTraceLostEvents > 0 ? 4 : 0)) <= spaceAvail) {
      // Have space for this event (and lost event report)
      if (phyTraceLostEvents > 0) { // Report # lost/compressed events
        phyTraceData[phyTraceHead++] = '!';
        if (phyTraceHead >= sizeof(phyTraceData)) {
          phyTraceHead = 0;
        }
        phyTraceData[phyTraceHead++] = 2;
        if (phyTraceHead >= sizeof(phyTraceData)) {
          phyTraceHead = 0;
        }
        phyTraceData[phyTraceHead++] = HIGH_BYTE(phyTraceLostEvents);
        if (phyTraceHead >= sizeof(phyTraceData)) {
          phyTraceHead = 0;
        }
        phyTraceData[phyTraceHead++] = LOW_BYTE(phyTraceLostEvents);
        if (phyTraceHead >= sizeof(phyTraceData)) {
          phyTraceHead = 0;
        }
        phyTraceLostEvents = 0;
      }
      phyTraceData[phyTraceHead++] = tag;
      if (phyTraceHead >= sizeof(phyTraceData)) {
        phyTraceHead = 0;
      }
      phyTraceData[phyTraceHead++] = len;
      if (phyTraceHead >= sizeof(phyTraceData)) {
        phyTraceHead = 0;
      }
      while (len-- != 0) {
        phyTraceData[phyTraceHead++] = *data++;
        if (phyTraceHead >= sizeof(phyTraceData)) {
          phyTraceHead = 0;
        }
      }
    } else {
      // Not enough space for this event
      phyTraceLostEvents++;
      if (2 <= spaceAvail) { // Just record the tag dropping content
        phyTraceData[phyTraceHead++] = tag;
        if (phyTraceHead >= sizeof(phyTraceData)) {
          phyTraceHead = 0;
        }
        phyTraceData[phyTraceHead++] = 0;
        if (phyTraceHead >= sizeof(phyTraceData)) {
          phyTraceHead = 0;
        }
      }
    }
    RESTORE_INTERRUPTS();
  }
}

// Insert a trace buffer entry for a Zigbee Packet
void emPhyTracePkt(uint8_t tag, uint8_t* zigPkt)
{
  uint8_t len  = zigPkt ?  zigPkt[0] : 0;
  uint8_t* data = zigPkt ? &zigPkt[1] : NULL;
  emPhyTraceRaw(tag, data, len);
}

// Print contents of trace buffer (from tail to head)
void emPhyTracePrint(uint8_t port)
{
  extern sl_status_t sli_legacy_serial_write_byte(uint8_t port, uint8_t dataByte);
  extern sl_status_t sli_legacy_serial_write_hex(uint8_t port, uint8_t dataByte);
  extern sl_status_t sli_legacy_serial_print_carriage_return(uint8_t port);
  extern sl_status_t sli_legacy_serial_wait_send(uint8_t port);
  static uint8_t syncCount = 0;

  phyTracePort = port;
  if (emPhyTrace) {
    while (phyTraceTail != phyTraceHead) {
      uint8_t byte = phyTraceData[phyTraceTail++];

      if (phyTraceTail >= sizeof(phyTraceData)) {
        phyTraceTail = 0;
      }
      if (syncCount == 0) {
        switch (byte) {
          case ')': // emRadioTransmit() ACK requested, received
          case '>': // emRadioTransmit() no ACK requested
          case 'a': // pro2Isr() ints acknowledged
          case 'I': // pro2Isr() ints pending and status
          case 'H': // parsePhr() PHR received
          case 'g': // parsePhr() failed(3) (rxBuffer[PHR])
          case 'h': // parsePhr() failed(1) (rxBufLen{2})
          case 'i': // parsePhr() failed(2) (payLen{2})
          case 'B': // processRx_() error from readPhr() or parsePay()
          case 'F': // parsePay() data flushed
          case 'P': // parsePay() packet received
          case 'l': // processRxComplete() error (payLen{2})
          case 'm': // processRxComplete() error (ignorePartial{1})
          case 'n': // processRxComplete() error (processedExtraRxComplete{1})
          case 'E': // processChEvents() CMD_ERROR GET_CHIP_STATUS
          case 'e': // processTxEvents() TX_ERR
          case 'x': // processTxEvents() TX_COMPLETE
          case '-': // processTxEvents() CCA fail
          case '?': // processTxEvents() requested ACK timed out
          case '!': // Lost event count                            in Trace
            halResetWatchdog(); // Give ourselves a new watchdog slice
            syncCount = phyTraceData[phyTraceTail++];
            if (phyTraceTail >= sizeof(phyTraceData)) {
              phyTraceTail = 0;
            }
            sli_legacy_serial_guaranteed_printf(port, "\r\n");
            sli_legacy_serial_guaranteed_printf(port, "%c", byte);
            sli_legacy_serial_guaranteed_printf(port, "%x", syncCount);
            break;
          case '\0': // Oops, have lost sync! Don't print \0 bytes
            break;
          default: // Oops, have lost sync!  Try to re-sync.
            sli_legacy_serial_guaranteed_printf(port, "%x", byte);
            break;
        }
      } else {
        sli_legacy_serial_guaranteed_printf(port, "%x", byte);
        syncCount--;
      }
    }
    // sli_legacy_serial_wait_send(port);
  }
}

#define PHY_TRACE_PKT(tag, pkt)      emPhyTracePkt((tag), (uint8_t*)(pkt))
#define PHY_TRACE_RAW(tag, buf, len) emPhyTraceRaw((tag), (uint8_t*)(buf), (len))
#define PHY_TRACE_OP(blah)           do { blah } while (0)

#define PHY_DEBUG(blah)  blah
#define PHY_ASSERT(expr) /* expr must not have side effects */ do { \
    if (!(expr)) {                                                  \
      emPhyTracePrint(phyTracePort);                                \
      assert(false);                                                \
    }                                                               \
} while (0)

#else//!PHY_TRACE

#define PHY_TRACE_PKT(tag, pkt)       /*no-op*/
#define PHY_TRACE_RAW(tag, buf, len)   /*no-op*/
#define PHY_TRACE_OP(blah)           /*no-op*/

#define PHY_DEBUG(blah)  blah
#define PHY_ASSERT(expr) /* expr must not have side effects */ assert(expr)

#endif//PHY_TRACE

#define PHY_ATTEST(expr) /* expr is ok to have side effects */ do { \
    if (!(expr)) {                                                  \
      PHY_ASSERT(false);                                            \
    }                                                               \
} while (0)
#define PHY_ATTEST_SUCCESS(expr) PHY_ATTEST((expr) == SL_STATUS_OK)

//== INTERNAL FUNCTIONS ==

static void pro2PowerUp(void); // Forward reference
static void pro2WakeUp(RadioPowerMode mode); // Forward reference

uint8_t pro2Peek(uint16_t addr)
{
  uint8_t cmdBuf[] = {
    /*0-Command   */ CMD_PEEK,
    /*1-Addr[15:8]*/ HIGH_BYTE(addr),
    /*2-Addr[7:0] */ LOW_BYTE(addr),
  };
  uint8_t value;
  PHY_ATTEST_SUCCESS(siPro2SendCommandGetResponse(sizeof(cmdBuf), cmdBuf,
                                                  sizeof(value), &value));
  return value;
}

void pro2Poke(uint16_t addr, uint8_t value)
{
  uint8_t cmdBuf[] = {
    /*0-Command   */ CMD_POKE,
    /*1-Addr[15:8]*/ HIGH_BYTE(addr),
    /*2-Addr[7:0] */ LOW_BYTE(addr),
    /*3-Value     */ value,
  };
  PHY_ATTEST_SUCCESS(siPro2SendCommandNoResponse(sizeof(cmdBuf), cmdBuf));
}

uint8_t pro2ReadOneProperty(uint8_t propGrp, uint8_t prop)
{
  uint8_t cmdBuf[] = {
    /*0-Command */ CMD_GET_PROPERTY,
    /*1-Group   */ propGrp,
    /*2-Num     */ 1,
    /*3-Property*/ prop,
  };
  uint8_t value;
  PHY_ATTEST_SUCCESS(siPro2SendCommandGetResponse(sizeof(cmdBuf), cmdBuf,
                                                  sizeof(value), &value));
  return value;
}

static void pro2WriteNProperties(uint8_t propGrp, uint8_t prop,
                                 uint8_t len, uint8_t* values)
{
  struct cmd_SET_PROPERTY_map cmdBuf = {
    /*0-Command */ CMD_SET_PROPERTY,
    /*1-Group   */ propGrp,
    /*2-Num     */ len,
    /*3-Property*/ prop,
    /*4-16 Vals */ { 0 }  // ... will copy in below based on len
  };
  PHY_ASSERT((len > 0) && (len < sizeof(cmdBuf.DATA)));
  memcpy(&cmdBuf.DATA, values, len);
  PHY_ATTEST_SUCCESS(siPro2SendCommandNoResponse(sizeof(cmdBuf)
                                                 - sizeof(cmdBuf.DATA) + len,
                                                 (uint8_t*) &cmdBuf));
}

void pro2WriteOneProperty(uint8_t propGrp, uint8_t prop, uint8_t value)
{
  uint8_t cmdBuf[] = {
    /*0-Command */ CMD_SET_PROPERTY,
    /*1-Group   */ propGrp,
    /*2-Num     */ 1,
    /*3-Property*/ prop,
    /*4-Value   */ value,
  };
  PHY_ATTEST_SUCCESS(siPro2SendCommandNoResponse(sizeof(cmdBuf), cmdBuf));
}

static void pro2GetAckEvents(Events* events)
{
  uint8_t cmdBuf[] = {
    /*0-Command */ CMD_IE154_GET_INT_STATUS,
    /*1-TxAck   */ ~(events->txAck),  // 0=ack, 1=leave pended
    /*2-RxAck   */ ~(events->rxAck),  // 0=ack, 1=leave pended
    /*3-ChipACK */ ~(events->chAck),  // 0=ack, 1=leave pended
  };
//PHY_TRACE_RAW('a', (uint8_t*)events, sizeof(Events));
  PHY_ATTEST_SUCCESS(siPro2SendCommandGetResponse(sizeof(cmdBuf), cmdBuf,
                                                  sizeof(Events), (uint8_t*) events));
}

static void pro2GetInfo(bool afterPowerUp)
{
  uint8_t cmd = CMD_PART_INFO;
  memset(&pro2PartInfo, 0, sizeof(pro2PartInfo));
  PHY_ATTEST_SUCCESS(siPro2SendCommandGetResponse(sizeof(cmd), &cmd,
                                                  sizeof(pro2PartInfo),
                                                  (uint8_t*) &pro2PartInfo));
  if (afterPowerUp) {
    // This command is only really useful after POWER_UP
    memset(&pro2FuncInfo, 0, sizeof(pro2FuncInfo));
    cmd = CMD_FUNC_INFO;
    PHY_ATTEST_SUCCESS(siPro2SendCommandGetResponse(sizeof(cmd), &cmd,
                                                    sizeof(pro2FuncInfo),
                                                    (uint8_t*) &pro2FuncInfo));
  } else {
    // This command is only really useful before POWER_UP, while in bootloader
    memset(&pro2DieInfo, 0, sizeof(pro2DieInfo));
    cmd = CMD_DIE_INFO;
    PHY_ATTEST_SUCCESS(siPro2SendCommandGetResponse(sizeof(cmd), &cmd,
                                                    sizeof(pro2DieInfo),
                                                    (uint8_t*) &pro2DieInfo));
  }
}

uint8_t pro2GetChipRev(void)
{
  return pro2DieInfo.CHIP_REV;
}

const uint8_t* pro2GetDieName(void)
{
  return (const uint8_t*)pro2DieInfo.DIE_NAME;
}

uint8_t pro2GetFunc(void)
{
  return pro2FuncInfo.FUNC;
}

uint32_t pro2GetFwRev(void)
{
  return (((uint32_t)pro2FuncInfo.REVEXT    << 24)
          | ((uint32_t)pro2FuncInfo.REVBRANCH << 16)
          | ((uint32_t)pro2FuncInfo.REVINT    <<  8)
          );
}

uint32_t pro2GetFwSvn(void)
{
  return NTOHL(pro2FuncInfo.SVNREV);
}

uint16_t pro2GetOtpId(void)
{
  return NTOHS(pro2PartInfo.ID);
}

uint16_t pro2GetPartNum(void)
{
  return NTOHS(pro2PartInfo.PART);
}

uint16_t pro2GetPatchRev(void)
{
  return NTOHS(pro2FuncInfo.PATCH);
}

uint8_t pro2GetRomId(void)
{
  PHY_ASSERT(pro2PartInfo.ROMID == pro2DieInfo.ROM_ID);
  return pro2PartInfo.ROMID;
}

static uint32_t pro2GetTxSfdTime(void)
{
  //TODO: Retrieve this from PRO2+
  // For now return SFD time set by pro2SetTxSfdTime
  return lastTxSfdTime;
}

sl_status_t pro2GpioConfig(Pro2Gpio gpio, uint8_t config)
{
  // Configure GPIO for PTI input pulled-up
  uint8_t cmdBuf[] = {
    /*0-Command */ CMD_GPIO_PIN_CFG,
    /*1-GPIO[0] */ (CMD_GPIO_PIN_CFG_ARG_GPIO_GPIO_MODE_ENUM_DONOTHING
                    << CMD_GPIO_PIN_CFG_ARG_GPIO_GPIO_MODE_LSB),
    /*2-GPIO[1] */ (CMD_GPIO_PIN_CFG_ARG_GPIO_GPIO_MODE_ENUM_DONOTHING
                    << CMD_GPIO_PIN_CFG_ARG_GPIO_GPIO_MODE_LSB),
    /*3-GPIO[2] */ (CMD_GPIO_PIN_CFG_ARG_GPIO_GPIO_MODE_ENUM_DONOTHING
                    << CMD_GPIO_PIN_CFG_ARG_GPIO_GPIO_MODE_LSB),
    /*4-GPIO[3] */ (CMD_GPIO_PIN_CFG_ARG_GPIO_GPIO_MODE_ENUM_DONOTHING
                    << CMD_GPIO_PIN_CFG_ARG_GPIO_GPIO_MODE_LSB),
    /*5-NIRQ    */
    /*6-SDO     */
    /*7-GEN_CONF*/
  };
  PHY_ASSERT(gpio < PRO2_GPIO_MAX);
  cmdBuf[gpio] = config;
  return siPro2SendCommandNoResponse(1 + gpio /*only send needed bytes*/, cmdBuf);
}

static void pro2PowerUpCommand(uint8_t bootOptions)
{
  uint8_t cmdBuf[] = {
    /*0-Command     */ CMD_POWER_UP,
    /*1-BOOT_OPTIONS*/ bootOptions,
    /*2-XTAL_OPTIONS*/ 0x00,
    /*3-XO_FREQ     */ BYTE_3(EM_PHY_DEFAULT_BAND_CONFIG->radioXoFreq),
    /*4-XO_FREQ     */ BYTE_2(EM_PHY_DEFAULT_BAND_CONFIG->radioXoFreq),
    /*5-XO_FREQ     */ BYTE_1(EM_PHY_DEFAULT_BAND_CONFIG->radioXoFreq),
    /*6-XO_FREQ     */ BYTE_0(EM_PHY_DEFAULT_BAND_CONFIG->radioXoFreq),
  };
  PHY_ATTEST_SUCCESS(siPro2SendCommandNoResponse(sizeof(cmdBuf), cmdBuf));
}

static void pro2RxTxFifoInfo(bool rxReset, bool txReset,
                             uint8_t* rxAvail, uint8_t* txAvail)
{
  uint8_t cmdRsp[2] = {
    /*0-Command */ CMD_FIFO_INFO,
    /*1-Reset   */ (0
                    | ((!!rxReset) << CMD_FIFO_INFO_ARG_FIFO_RX_LSB)
                    | ((!!txReset) << CMD_FIFO_INFO_ARG_FIFO_TX_LSB)
                    ),
  };
  if (txReset) {
    // If we reset the TX FIFO, then we must abort any pending FIFO writes too
    bytesToWrite = 0;
    txBufPending = NULL;
  }
  if (rxReset) {
    // If we reset the RX FIFO, then we must throw away any Rx data pending
    rxBufLen = 0; // Reset for next Rx
  }
  PHY_ATTEST_SUCCESS(siPro2SendCommandGetResponse(sizeof(cmdRsp), cmdRsp,
                                                  2, cmdRsp));
  if (rxAvail != NULL) {
    *rxAvail = cmdRsp[CMD_FIFO_INFO_REP_RX_FIFO_COUNT_INDEX - 1];
  }
  if (txAvail != NULL) {
    *txAvail = cmdRsp[CMD_FIFO_INFO_REP_TX_FIFO_SPACE_INDEX - 1];
  }
}

static void pro2ResetEvents(Events* events)
{
  // Reset Rx and/or Tx FIFO depending on whether Rx and/or Tx is being reset
  if (events->rxAck || events->txAck) {
    pro2RxTxFifoInfo(!!events->rxAck, !!events->txAck, NULL, NULL);
  }
  pro2GetAckEvents(events);
}

static void pro2SendRadioCommands(const uint8_t* radioConfig)
{
  // loop until we reach the end marker 0x00
  while (*radioConfig != 0x00) {
    uint8_t commandLength = *radioConfig++;
    PHY_ATTEST_SUCCESS(siPro2SendCommandNoResponse(commandLength,
                                                   (uint8_t*) radioConfig));
    radioConfig += commandLength;
  }
}

static void pro2SetRadioProperties(const uint8_t* radioConfig)
{
  // loop until we reach the end marker 0x00
  while (*radioConfig != 0x00) {
    PHY_ASSERT(*radioConfig == CMD_SET_PROPERTY); // Only handle SET_PROP here
    uint8_t commandLength = radioConfig[2] + 4;
    PHY_ATTEST_SUCCESS(siPro2SendCommandNoResponse(commandLength,
                                                   (uint8_t*) radioConfig));
    radioConfig += commandLength;
  }
}

static void pro2SetRadioRamPatch(const uint8_t* radioRamPatch)
{
  // loop until we reach the end marker 0x00
  while (*radioRamPatch != 0x00) {
    uint8_t commandLength = 8; // hardcoded 8-byte chunks
    uint8_t ramBuf[8]; // Use RAM copy which can be DMA'd, which is tad faster
    memcpy(ramBuf, radioRamPatch, commandLength);
    PHY_ATTEST_SUCCESS(siPro2SendCommandNoResponse(commandLength, ramBuf));
    radioRamPatch += commandLength;
  }
}

static void pro2SetRadioRngSeed(void)
{
 #if     (defined(PRO2_RNG_SEED) || SI4440_1442_TOLERATE)
  uint32_t seed;
 #ifdef  PRO2_RNG_SEED
  seed = PRO2_RNG_SEED;
 #elif   SI4440_1442_TOLERATE
  sli_802154phy_radio_get_random_numbers((uint16_t*) &seed, sizeof(seed) / sizeof(uint16_t));
 #endif//PRO2_RNG_SEED||SI4440_1442_TOLERATE
  // Seed the PRO2+'s random number generator
  // The PRO2+ supposedly seeds itself with a good random value by default
  // and this API is only intended to be used to force it to be deterministic.
  uint8_t cmdBuf[] = {
    /*0-Command */ CMD_SRAND,
    /*1-Seed[3] */ (uint8_t) (seed >> 24),
    /*2-Seed[2] */ (uint8_t) (seed >> 16),
    /*3-Seed[1] */ (uint8_t) (seed >>  8),
    /*4-Seed[0] */ (uint8_t) (seed >>  0),
  };
  PHY_ATTEST_SUCCESS(siPro2SendCommandNoResponse(sizeof(cmdBuf), cmdBuf));
 #endif//(defined(PRO2_RNG_SEED) || SI4440_1442_TOLERATE)
}

static void pro2UpdateEvents(uint8_t l1Events, uint8_t rxEvents,
                             uint8_t txEvents, uint8_t chEvents)
{
  if ((l1Events != pro2L1Events)
      || (rxEvents != pro2RxEvents)
      || (txEvents != pro2TxEvents)
      || (chEvents != pro2ChEvents)) {
    uint8_t cmdBuf[] = {
      /*0-Command */ CMD_SET_PROPERTY,
      /*1-Group   */ PROP_GRP_ID_IE154_INT_CTL,
      /*2-Num     */ 4,
      /*3-Property*/ PROP_IE154_INT_CTL_ENABLE_OFFSET,
      /*4-Enable  */ l1Events,
      /*5-TxEnable*/ txEvents,
      /*6-RxEnable*/ rxEvents,
      /*7-ChEnable*/ chEvents,
    };
    // First clear out any stale events being newly enabled
    Events events = {
      0, 0,  // No L1 acknowledgements
      0, txEvents & (txEvents ^ pro2TxEvents),
      0, rxEvents & (rxEvents ^ pro2RxEvents),
      0, chEvents & (chEvents ^ pro2ChEvents),
      0, 0,
      0, 0   // TX_FIFO_SPACE, RX_FIFO_COUNT
    };
    pro2GetAckEvents(&events);
    // Now update event configuration
    PHY_ATTEST_SUCCESS(siPro2SendCommandNoResponse(sizeof(cmdBuf), cmdBuf));
    phy2L1Events = pro2L1Events = l1Events;
    phy2RxEvents = pro2RxEvents = rxEvents;
    phy2TxEvents = pro2TxEvents = txEvents;
    phy2ChEvents = pro2ChEvents = chEvents;
  }
}

static void pro2IdleRadio(void)
{
  uint8_t cmdBuf[] = {
    /*0-Command */ CMD_IE154_START_IDLE,
  };

  PHY_ATTEST_SUCCESS(siPro2SendCommandNoResponse(sizeof(cmdBuf), cmdBuf));
  PHY_DEBUG(pro2Listening = false); // Mark only radio (but not PHY) as no longer listening
}

static EmPhyBandConfig* mapChannelToBand(uint8_t phyChan)
{
  // First check if channel is within the current band --
  // the easiest and most likely case
  if (CHANNEL_IN_BAND(phyChan, phy2BandConfig)) {
    return phy2BandConfig;
  }
  // Now loop thru the other bands looking for a match
  uint8_t band;
  for (band = emPhyConfigMHz.numBands; band > 0; ) {
    EmPhyBandConfig* bandConfig = (EmPhyBandConfig*) emPhyConfigMHz.bandConfigs[--band];
    if ((bandConfig != phy2BandConfig)  // already checked this one
        && (CHANNEL_IN_BAND(phyChan, bandConfig))) {
      return bandConfig;
    }
  }
  return NULL; // No band found for this channel
}

static EmPhySubBandConfig* subBandConfig(uint8_t phyChan,
                                         EmPhyBandConfig *bandConfig)
{
  if (bandConfig == NULL) {
    bandConfig = mapChannelToBand(phyChan);
    if (bandConfig == NULL) {
      return NULL;
    }
  }
  uint8_t index;
  for (index = 0; index < bandConfig->numSubBandConfigs; index++) {
    if (CHANNEL_IN_BAND(phyChan, &bandConfig->subBandConfigs[index])) {
      return ((EmPhySubBandConfig*) &bandConfig->subBandConfigs[index]);
    }
  }
  return NULL;
}

uint16_t pro2GetRadioTxPowerMode(void)
{
  uint8_t curPaMode = pro2ReadOneProperty(PROP_GRP_ID_PA, PROP_PA_MODE_OFFSET);
  uint8_t curPaSel = (curPaMode & PROP_PA_MODE_PA_SEL_MASK) >> PROP_PA_MODE_PA_SEL_LSB;
  uint16_t txPowerMode;
 #if     SI4440_1672_TOLERATE
  // LP and MP are reversed prior to fix rev, which PHY now uses :-(
  if (pro2GetFwRev() < SI4440_1672_FIX_FWREV) {
    if (curPaSel == PROP_PA_MODE_PA_SEL_ENUM_LP) { // Really MP on ROM5
      curPaSel = PROP_PA_MODE_PA_SEL_ENUM_MP;
    } else
    if (curPaSel == PROP_PA_MODE_PA_SEL_ENUM_MP) { // Really LP on ROM5
      curPaSel = PROP_PA_MODE_PA_SEL_ENUM_LP;
    }
  }
 #endif//SI4440_1672_TOLERATE
  switch (curPaSel) {
    case PROP_PA_MODE_PA_SEL_ENUM_LP:
      txPowerMode = SL_ZIGBEE_TX_POWER_MODE_DEFAULT;
      break;
    case PROP_PA_MODE_PA_SEL_ENUM_MP:
      txPowerMode = SL_ZIGBEE_TX_POWER_MODE_BOOST;
      break;
    case PROP_PA_MODE_PA_SEL_ENUM_HP_FINE:
      txPowerMode = SL_ZIGBEE_TX_POWER_MODE_ALTERNATE;
      break;
    case PROP_PA_MODE_PA_SEL_ENUM_HP_COARSE:
      txPowerMode = SL_ZIGBEE_TX_POWER_MODE_BOOST_AND_ALTERNATE;
      break;
    default:
      txPowerMode = SL_ZIGBEE_TX_POWER_MODE_DEFAULT; // Really unknown!
      break;
  }
  return (txPowerMode
          | (emRadioTxPowerMode & ((uint16_t) ~SL_ZIGBEE_TX_POWER_MODE_PA_SELECTION_MASK)));
}

static void pro2SetTxPA(uint8_t newPaSel)
{
  uint8_t paIndex;
  for (paIndex = 0; paIndex < emPhyConfigMHz.numPAs; paIndex++) {
    if (emPhyConfigMHz.PAs[paIndex].paSelEnum == newPaSel) {
      if (phy2TxPA != &emPhyConfigMHz.PAs[paIndex]) {
        phy2TxPA = &emPhyConfigMHz.PAs[paIndex];
        // We've just switched PA's -- need to adjust the power levels
        // per the new PA equation across all our networks
        int i;
        for (i = 0; i < PRO2_MAX_NETWORKS; i++) {
          emPhySetRadioPowerOnNetwork(phy2TxPowerRequested[i], i);
        }
      }
      return;
    }
  }
  // No match?  That's bad!
  PHY_ASSERT(false);
}

static uint16_t pro2OverridePaSelection(uint16_t txPowerMode)
{
  PHY_ASSERT(txPowerMode != SL_ZIGBEE_TX_POWER_MODE_USE_TOKEN);
  pro2PowerUp(); // Ensure PRO2+ is up
  uint8_t curPaMode = pro2ReadOneProperty(PROP_GRP_ID_PA, PROP_PA_MODE_OFFSET);
  uint8_t newPaSel;

  if (txPowerMode == SL_ZIGBEE_TX_POWER_MODE_PER_BAND_CONFIG) {
    // Simply extract the radio config's PA selection
    newPaSel = ((curPaMode & PROP_PA_MODE_PA_SEL_MASK)
                >> PROP_PA_MODE_PA_SEL_LSB);
   #if     SI4440_1672_TOLERATE
    // LP and MP are reversed prior to fix rev, which PHY now uses :-(
    if (pro2GetFwRev() < SI4440_1672_FIX_FWREV) {
      if (newPaSel == PROP_PA_MODE_PA_SEL_ENUM_LP) { // Really MP on ROM5
        newPaSel = PROP_PA_MODE_PA_SEL_ENUM_MP;
      } else
      if (newPaSel == PROP_PA_MODE_PA_SEL_ENUM_MP) { // Really LP on ROM5
        newPaSel = PROP_PA_MODE_PA_SEL_ENUM_LP;
      }
    }
   #endif//SI4440_1672_TOLERATE
  } else {
    // At this point txPowerMode is positive logic.
    // Bit 0: powerLevel:1 for boost mode
    //                  :0 for normal power mode
    // Bit 1: paSwitch:1 for alternate PA path
    //                :0 for bi-directional PA path
    // Bit 2: extPaOnBiDirRfPath:1 for PA present on bi-directional path
    //                          :0 for PA not present on bi-directional path.
    // PRO2+ mapping is thus:
    // Bit 2 1 0
    //     x 0 0 select PRO2+ LP +13dBm PA
    //     x 0 1 select PRO2+ MP +16dBm PA
    //     x 1 0 select PRO2+ HP +20dBm PA in FINE   mode
    //     x 1 1 select PRO2+ HP +20dBm PA in COARSE mode <-- default
    static const uint8_t paSels[] = { PROP_PA_MODE_PA_SEL_ENUM_LP,
                                      PROP_PA_MODE_PA_SEL_ENUM_MP,
                                      PROP_PA_MODE_PA_SEL_ENUM_HP_FINE,
                                      PROP_PA_MODE_PA_SEL_ENUM_HP_COARSE, };
    newPaSel = paSels[txPowerMode & 3];
  }
  // Verify that the PA selection makes sense for the device.
  // If not then we force it into compliance so users can't
  // easily shoot themselves in the foot
  switch (pro2GetPartNum()) {
    case 0x4467: // LP -- -- : PRO2+IE154
      // Force choice to LP
      txPowerMode = (txPowerMode & ~3) | 0;
      newPaSel = PROP_PA_MODE_PA_SEL_ENUM_LP;
      break;
    case 0x4468: // -- -- HP : PRO2+IE154
      // Negate LP choice for HP_FINE
      if (newPaSel == PROP_PA_MODE_PA_SEL_ENUM_LP) {
        txPowerMode = (txPowerMode & ~3) | 2;
        newPaSel = PROP_PA_MODE_PA_SEL_ENUM_HP_FINE;
      }
    // Fall through
    case 0x4469: // LP -- HP : PRO2+IE154
      // Negate MP choice for HP_FINE
      if (newPaSel == PROP_PA_MODE_PA_SEL_ENUM_MP) {
        txPowerMode = (txPowerMode & ~3) | 2;
        newPaSel = PROP_PA_MODE_PA_SEL_ENUM_HP_FINE;
      }
    // Fall through
    default:     // unknown  : PRO2+IE154 assumed
    {
      // Let txPowerMode and newPaSel thru unscathed
      // These parts support writing the PA_MODE property
      // Switch radio onto proper PA if not already there
      uint8_t newPaMode = (newPaSel << PROP_PA_MODE_PA_SEL_LSB);
     #if     SI4440_1672_TOLERATE
      // LP and MP are reversed prior to fix rev, which PHY now uses :-(
      if (pro2GetFwRev() < SI4440_1672_FIX_FWREV) {
        if (newPaSel == PROP_PA_MODE_PA_SEL_ENUM_LP) { // Really MP on ROM5
          newPaMode = (PROP_PA_MODE_PA_SEL_ENUM_MP << PROP_PA_MODE_PA_SEL_LSB);
        } else
        if (newPaSel == PROP_PA_MODE_PA_SEL_ENUM_MP) { // Really LP on ROM5
          newPaMode = (PROP_PA_MODE_PA_SEL_ENUM_LP << PROP_PA_MODE_PA_SEL_LSB);
        }
      }
     #endif//SI4440_1672_TOLERATE
      if ((curPaMode & PROP_PA_MODE_PA_SEL_MASK) != newPaMode ) {
        pro2WriteOneProperty(PROP_GRP_ID_PA, PROP_PA_MODE_OFFSET,
                             ((curPaMode & ~PROP_PA_MODE_PA_SEL_MASK)
                              | newPaMode)
                             );
      }
      break;
    }
  }

  // Now deal with PA Clock Duty Cycle which depends on the PA selection
  // Name     Value   Description
  // DIFF_50    0     Complementary drive signals, 50% duty cycle.
  //                  Recommended for use with Si4463/Si4464 high-power (e.g.,
  //                  +20dBm)applications, and as an option for Si4461
  //                  medium-power (e.g., +13 to +16 dBm) applications.
  // SINGLE_25  3     Single-ended drive signal, 25% duty cycle.
  //                  Recommended for use with Si4460 low-power (e.g., +10 to
  //                  +13 dBm, or lower) applications, and as an option for
  //                  Si4461 medium-power (e.g., +13 to +16 dBm) applications.
  // Other values are reserved for factory use only.
  uint8_t curPaBias = pro2ReadOneProperty(PROP_GRP_ID_PA,
                                          PROP_PA_BIAS_CLKDUTY_OFFSET);
  uint8_t newPaClk = ((curPaBias & PROP_PA_BIAS_CLKDUTY_CLK_DUTY_MASK)
                      >> PROP_PA_BIAS_CLKDUTY_CLK_DUTY_LSB);
  switch (newPaSel) {
    case PROP_PA_MODE_PA_SEL_ENUM_HP_COARSE:
    case PROP_PA_MODE_PA_SEL_ENUM_HP_FINE:
      newPaClk = PROP_PA_BIAS_CLKDUTY_CLK_DUTY_ENUM_DIFF_50;
      break;
    case PROP_PA_MODE_PA_SEL_ENUM_MP:
      // Leave newPaClk as already configured
      break;
    case PROP_PA_MODE_PA_SEL_ENUM_LP:
      newPaClk = PROP_PA_BIAS_CLKDUTY_CLK_DUTY_ENUM_SINGLE_25;
      break;
    default:
      PHY_ASSERT(false);
      break;
  }
  uint8_t newPaBias = ((curPaBias & ~PROP_PA_BIAS_CLKDUTY_CLK_DUTY_MASK)
                       | (newPaClk << PROP_PA_BIAS_CLKDUTY_CLK_DUTY_LSB));
  if (curPaBias != newPaBias) {
    pro2WriteOneProperty(PROP_GRP_ID_PA, PROP_PA_BIAS_CLKDUTY_OFFSET, newPaBias);
  }

  // Ensure our power-mapping equation is updated per the chosen PA
  pro2SetTxPA(newPaSel);
  return txPowerMode;
}

static void pro2UpdateSymbolTimingProperties(void)
{
  // Set the CSMA and ACK symbol timing properties, which may depend on
  // band configuration settings like data rate, preamble or sfd size, etc.
  // and sub-band parameters for LBT/CSMA and ACKs.

  // COMMON_XTAL_TICKS_PER_SYMBOL
  #define SymbolTicks ((phy2BandConfig->radioXoFreq * SL_ZIGBEE_PHY_SYMBOL_DURATION_US) / 1000000)
  uint8_t propValue[2] = { HIGH_BYTE(SymbolTicks), LOW_BYTE(SymbolTicks) };
  pro2WriteNProperties(PROP_GRP_ID_IE154_COMMON,
                       PROP_IE154_COMMON_XTAL_TICK_PER_SYMBOL_OFFSET,
                       sizeof(propValue), propValue);

  // TX_ACK_TIMEOUT_SYMBOLS
  uint16_t ackTimeoutSym = (phy2SubBandParams == NULL)
                           ? SL_ZIGBEE_PHY_ACK_TIMEOUT_SYMBOLS
                           : phy2SubBandParams->ackMaxSym;
  propValue[0] = HIGH_BYTE(ackTimeoutSym);
  propValue[1] =  LOW_BYTE(ackTimeoutSym);
 #if     SI4440_1468_TOLERATE
  // SI4440-1468: only low-order 12 bits are actually used for timing in PRO2+
  // Anything larger, peg at max currently supported and hope for the best
  if (propValue[0] > 0x0F) {
    //PHY_ASSERT(false);
    propValue[0] = 0x0F;
    propValue[1] = 0xFF;
  }
 #endif//SI4440_1468_TOLERATE
  pro2WriteNProperties(PROP_GRP_ID_IE154_TX,
                       PROP_IE154_TX_ACK_TIMEOUT_OFFSET,
                       sizeof(propValue), propValue);

  // TX_CSMA_BACKOFF_BASE_SYMBOLS
  uint16_t ccaBackoffSym = (phy2SubBandParams == NULL)
                           ? SL_ZIGBEE_PHY_BACKOFF_SYMBOLS
                           : phy2SubBandParams->lbtBackoffSym;
  propValue[0] = HIGH_BYTE(ccaBackoffSym);
  propValue[1] =  LOW_BYTE(ccaBackoffSym);
 #if     SI4440_1468_TOLERATE
  // SI4440-1468: only low-order 8 bits are actually used for timing in PRO2+
  // Anything larger, peg at max currently supported and hope for the best
  if (propValue[0] > 0) {
    //PHY_ASSERT(false);
    propValue[0] = 0;
    propValue[1] = 0xFF;
  }
 #endif//SI4440_1468_TOLERATE
  pro2WriteNProperties(PROP_GRP_ID_IE154_TX,
                       PROP_IE154_TX_CSMA_BACKOFF_BASE_SYMBOLS_OFFSET,
                       sizeof(propValue), propValue);

 #if     RAIL_LIB_4848_TALK_CCA
  // TX_CSMA_ON_SYMBOLS -- moved to pro2UpdateTxCsmaProperties()
 #else//!RAIL_LIB_4848_TALK_CCA
  // TX_CSMA_ON_SYMBOLS
  uint16_t ccaCheckSym = (phy2SubBandParams == NULL)
                         ? SL_ZIGBEE_PHY_CCA_SYMBOLS
                         : phy2SubBandParams->lbtCcaSym;
  propValue[0] = HIGH_BYTE(ccaCheckSym);
  propValue[1] =  LOW_BYTE(ccaCheckSym);
 #if     SI4440_1468_TOLERATE
  // SI4440-1468: only low-order 12 bits are actually used for timing in PRO2+
  // Anything larger, peg at max currently supported and hope for the best
  if (propValue[0] > 0x0F) {
    //PHY_ASSERT(false);
    propValue[0] = 0x0F;
    propValue[1] = 0xFF;
  }
 #endif//SI4440_1468_TOLERATE
  pro2WriteNProperties(PROP_GRP_ID_IE154_TX,
                       PROP_IE154_TX_CSMA_ON_SYMBOLS_OFFSET,
                       sizeof(propValue), propValue);
 #endif//RAIL_LIB_4848_TALK_CCA

  // RX_BACKOFF_TURNAROUND_TIME (symbols)
  uint16_t turnaroundSymbols = (phy2SubBandParams == NULL)
                               ? SL_ZIGBEE_PHY_TURNAROUND_SYMBOLS
                               : phy2SubBandParams->ackMinSym;
  PHY_ASSERT(turnaroundSymbols <= 0xFF);
  pro2WriteOneProperty(PROP_GRP_ID_IE154_RX,
                       PROP_IE154_RX_BACKOFF_TURNAROUND_TIME_OFFSET,
                       turnaroundSymbols
                       );
}

static void pro2SetCcaThreshold(int8_t thresholddBm)
{
  // If we're to use SUBBAND config, then use that if it's been set already
  PHY_ASSERT(thresholddBm != ED_CCA_THRESHOLD_UNINIT);
  if (thresholddBm == ED_CCA_THRESHOLD_SUBBAND) {
    if (phy2SubBandConfig == NULL) {
      return; // switchBand() will take care of this later
    }
    thresholddBm = phy2SubBandConfig->edThresholddBm;
  }
  // Convert dBm to PRO2 RSSI units per datasheet equation
  //   RSSIdBm = (RSSIpro2 / 2) - RSSIcal
  // or calculating for RSSIpro2:
  //   RSSIpro2 = (RSSIdBm + RSSIcal) * 2
  int16_t newThreshold = 2 * ((int16_t)thresholddBm + (int16_t)emPhyConfigMHz.rssidBmOffset);
  // Make sure it's sane, fitting the range of the PRO2 register
  if (newThreshold < 0x0000) {
    newThreshold = 0x0000;
  } else
  if (newThreshold > 0x00FF) {
    newThreshold = 0x00FF;
  }
  if (pro2CcaThreshold != newThreshold) {
    pro2PowerUp(); // Ensure PRO2+ is up
    pro2WriteOneProperty(PROP_GRP_ID_IE154_TX,
                         PROP_IE154_TX_CCA_THRESHOLD_OFFSET,
                         (uint8_t)newThreshold);
    // This change won't take effect until next START_TX
    pro2CcaThreshold = (uint8_t)newThreshold;
  }
}

/**
 * Set the RSSI offset, adjusting the radio's RSSI readings.
 *
 * @param[in] dB The RSSI offset in dB to apply. Positive values increase
 *   RSSI dBm readings by the indicated dB up to an upper limit of -3 dBm;
 *   negative values decrease the readings in similar fashion down do a
 *   lower limit of -130 dBm.
 * @return SL_STATUS_OK if the offset could be applied or SL_ZIGBEE_BAD_ARGUMENT
 *   if the requested offset is too large or too small.
 *
 * @note Applying an RSSI offset will impact the RSSI-based clear channel
 *   assessment (CCA) during LBT/CSMA transmits. The CCA threshold may be
 *   adjusted if needed via emRadioSetEdCcaThreshold().
 */
sl_status_t pro2SetRssiOffset(int8_t dB)
{
  int16_t rssiComp = (PROP_MODEM_RSSI_COMP_DEFAULT + dB);
  if ((rssiComp < 0) || (rssiComp > PROP_MODEM_RSSI_COMP_RSSI_COMP_MASK)) {
    return SL_STATUS_INVALID_PARAMETER;
  }
  if (pro2RssiOffsetdB != dB) {
    pro2PowerUp(); // Ensure PRO2+ is up
    pro2WriteOneProperty(PROP_GRP_ID_MODEM,
                         PROP_MODEM_RSSI_COMP_OFFSET,
                         (uint8_t)rssiComp);
    pro2RssiOffsetdB = dB;
  }
  return SL_STATUS_OK;
}

static void pro2UpdateXoTune(void)
{
  // If the XO_TUNE token exists and is valid, it overrides the radio config
 #ifdef  CREATOR_MFG_XO_TUNE
  tokTypeMfgXoTune mfgXoTuneToken = 0xFFFF;
  halCommonGetMfgToken(&mfgXoTuneToken, TOKEN_MFG_XO_TUNE);
  if ((BIT(PROP_GLOBAL_XO_TUNE_TUNE_VALUE_SIZE) & mfgXoTuneToken) == 0) {
    pro2WriteOneProperty(PROP_GRP_ID_GLOBAL, PROP_GLOBAL_XO_TUNE_OFFSET,
                         mfgXoTuneToken & PROP_GLOBAL_XO_TUNE_TUNE_VALUE_MASK);
  }
 #endif//CREATOR_MFG_XO_TUNE
}

static sl_status_t switchBand(uint8_t phyChan, bool forceIdle)
{
  bool updateTimingProperties = false;
  // Locate the proper band associated with this PHY channel
  EmPhyBandConfig* newBandConfig = mapChannelToBand(phyChan);
  if (newBandConfig == NULL) {
    return SL_STATUS_TRANSMIT_INVALID_CHANNEL;
  }
  if (newBandConfig != phy2BandConfig) {
    // Switch to new band configuration!
    if (forceIdle) {
      pro2IdleRadio(); // Force radio Idle to flush any RX_ABORTED events
    }
   #if     NO_RADIO_CONFIG
    // Don't apply any radio configurations
   #else//!NO_RADIO_CONFIG
    // Set the band's properties from its radioConfigArray[]
    pro2SetRadioProperties(newBandConfig->radioConfigArray);
   #endif//NO_RADIO_CONFIG
    pro2UpdateXoTune();

    // If any of the CSMA / ACK timing properties might be affected by
    // the new band configuration, ensure those properties get updated
    if ((phy2BandConfig == NULL)
        || (phy2BandConfig->dataRateBps  != newBandConfig->dataRateBps)
        || (phy2BandConfig->symbolBits   != newBandConfig->symbolBits)
        || (phy2BandConfig->preambleBits != newBandConfig->preambleBits)
        || (phy2BandConfig->sfdBits      != newBandConfig->sfdBits)
        ) {
      updateTimingProperties = true;
    }
    phy2BandConfig = newBandConfig;

    // Potentially override the band's PA setting
    emRadioTxPowerMode = pro2OverridePaSelection(emRadioTxPowerMode);
  }

  // Now deal with potentially new sub-band config
  EmPhySubBandConfig* newSubBandConfig = subBandConfig(phyChan, newBandConfig);
  PHY_ASSERT(newSubBandConfig != NULL);
  const EmPhySubBandParams* newSubBandParams = newSubBandConfig->subBandParams;
  if (newSubBandConfig != phy2SubBandConfig) {
    phy2SubBandConfig = newSubBandConfig;
    // Emplace LBT/CSMA config properties
    if (newSubBandConfig->ccaConfigArray != NULL) {
      pro2SetRadioProperties(newSubBandConfig->ccaConfigArray);
    }
    // Update CCA threshold if per sub-band
    if (edCcaThreshold == ED_CCA_THRESHOLD_SUBBAND) {
      pro2SetCcaThreshold(newSubBandConfig->edThresholddBm);
    }
    // And now deal with the sub-band parameters related to the
    // symbol timing properties
    if (newSubBandParams != phy2SubBandParams) {
      updateTimingProperties |= ((newSubBandParams == NULL)
                                 || (phy2SubBandParams == NULL)
                                 || (phy2SubBandParams->lbtBackoffSym
                                     != newSubBandParams->lbtBackoffSym)
                                 || (phy2SubBandParams->lbtCcaSym
                                     != newSubBandParams->lbtCcaSym)
                                 || (phy2SubBandParams->ackMaxSym
                                     != newSubBandParams->ackMaxSym)
                                 || (phy2SubBandParams->ackMinSym
                                     != newSubBandParams->ackMinSym)
                                 );
      phy2SubBandParams = newSubBandParams;
    }
  }

  if (updateTimingProperties) {
    // The phy2BandConfig, phy2SubBandConfig, and phy2SubBandParams
    // need to point to the new band prior to this call.
    pro2UpdateSymbolTimingProperties();
  }

  return SL_STATUS_OK;
}

static void pro2StartEd(void)
{
  PHY_ASSERT(!pro2Listening);
  // Ensure we're on the proper band for the current channel.
  // Radio should be IDLE here.
  PHY_ATTEST_SUCCESS(switchBand(phy2Channel, false));
  uint8_t cmdBuf[] = {
    /*0-Command */ CMD_IE154_START_ENERGY_DETECT,
    /*1-Config  */ (CMD_IE154_START_ENERGY_DETECT_ARG_CONFIG_LATCH_POINT_ENUM_ENERGY8
                    << CMD_IE154_START_ENERGY_DETECT_ARG_CONFIG_LATCH_POINT_LSB),
    /*2-Channel */ phy2Channel,
  };
  PHY_ATTEST_SUCCESS(siPro2SendCommandNoResponse(sizeof(cmdBuf), cmdBuf));
  PHY_ATTEST_SUCCESS(siPro2WaitForReady()); // Avoids deferring this into the ISR
  // It seems easiest to place the delay here so callers can obtain
  // valid RSSI samples after another SL_ZIGBEE_PHY_CCA_DURATION_US
  halCommonDelayMicroseconds(PRO2PLUS_START_ED_DELAY_US);
  // This radio state is tracked by callers using radioCurrentPowerStatus
}

static void pro2StartRx(void)
{
  PHY_ASSERT(!pro2Listening);
  // Ensure we're on the proper band for the current channel.
  // Radio should be IDLE here.
  PHY_ATTEST_SUCCESS(switchBand(phy2Channel, false));
  uint8_t cmdBuf[] = {
    /*0-Command */ CMD_IE154_START_RX,
    /*1-Channel */ phy2Channel,
    /*2-NextMode*/ (CMD_IE154_START_RX_ARG_NEXT_MODE_RX_COMPLETE_MODE_ENUM_LISTEN
                    << CMD_IE154_START_RX_ARG_NEXT_MODE_RX_COMPLETE_MODE_LSB),
    // Stay listening -- currently there is no concept of
    // receiving just one frame then going to sleep.
  };
  if (pro2ForceCalibration) {
    pro2ForceCalibration = false;
    //TODO: How to force calibration?  Used to be a flag arg to START_RX...
    //TODO: The IE154_CALIBRATE command seems to be more a property than
    //TODO: a command?  What is "cold boot" vs. "warm boot"?
  }
  PHY_ATTEST_SUCCESS(siPro2SendCommandNoResponse(sizeof(cmdBuf), cmdBuf));
 #if     (PRO2_MAX_NETWORKS > 2)
  listeningPhyChan = phy2Channel;
 #endif//(PRO2_MAX_NETWORKS > 2)
  phy2Listening = true; // Mark PHY as listening
  PHY_DEBUG(pro2Listening = true); // Mark radio as listening
  PHY_ATTEST_SUCCESS(siPro2WaitForReady()); // Avoids deferring this into the ISR
}

static void pro2SetTxSfdTime(void)
{
  //TODO: This should be no-op when retrieving SFD time from PRO2+
  lastTxSfdTime = sli_802154phy_get_mac_timer();
}

// Returns the number of CCA checks to perform
static uint8_t pro2UpdateTxCsmaProperties(void)
{
  // Sync possibly-changed radioTransmitConfig with pro2RadioTransmitConfig
  // and Radio -- but this is only necessary if requesting CSMA:
  if (!radioTransmitConfig.checkCca) {
    return 0;
  }
  // Even if we've requested CSMA, the Duty Cycle Pkt Mode may override that:
  EmPhyDcMode pktMode = emPhyDcGetPktMode();
  // Determine the CSMA/LBT parameters for this transmit
  int8_t ccaAttemptMax;
  uint8_t backoffExponentMin;
  uint8_t backoffExponentMax;
  uint8_t minimumBackoff;
  int8_t backoffAdjust;
 #if     RAIL_LIB_4848_TALK_CCA
  bool myUseZeroBackoff = emRadioAlwaysUseZeroBackoff;
  uint16_t ccaCheckSym = (phy2SubBandParams == NULL)
                         ? SL_ZIGBEE_PHY_CCA_SYMBOLS
                         : phy2SubBandParams->lbtCcaSym;

  if ((pktMode == EM_PHY_DC_MODE_TALK)
      || (pktMode == EM_PHY_DC_MODE_DIALOG)) {
    // RAIL_LIB-4848 - to avoid transmit aborting a receive do a minimal 'CCA'
    ccaAttemptMax      = -1;
    backoffExponentMin = 0;
    backoffExponentMax = 0;
    minimumBackoff     = 0;
    backoffAdjust      = 0;
    myUseZeroBackoff   = true;
    ccaCheckSym        = 4; // quick measurement
  } else
 #else//!RAIL_LIB_4848_TALK_CCA
  #define myUseZeroBackoff emRadioAlwaysUseZeroBackoff

  if ((pktMode == EM_PHY_DC_MODE_TALK)
      || (pktMode == EM_PHY_DC_MODE_DIALOG)) {
    return 0;
  }
 #endif//RAIL_LIB_4848_TALK_CCA
  if ((phy2SubBandParams == NULL)
      || (pktMode == EM_PHY_DC_MODE_CSMA)) {
    // If no EmPhySubBandParams{}, or Duty Cycle Mode is CSMA,
    // then the radioTransmitConfig{} CSMA applies:
    ccaAttemptMax      = (int8_t)radioTransmitConfig.ccaAttemptMax;
    backoffExponentMin = radioTransmitConfig.backoffExponentMin;
    backoffExponentMax = radioTransmitConfig.backoffExponentMax;
    minimumBackoff     = radioTransmitConfig.minimumBackoff;
    backoffAdjust      = 0;
  } else {
    // Otherwise the EmPhySubBandParams{} LBT/CSMA applies:
    ccaAttemptMax      = phy2SubBandParams->lbtTries;
    backoffExponentMin = phy2SubBandParams->lbtBackoffMinExp;
    backoffExponentMax = phy2SubBandParams->lbtBackoffMaxExp;
    minimumBackoff     = phy2SubBandParams->lbtMinBackoffs;
    backoffAdjust      = phy2SubBandParams->lbtBackoffAdjust;
  }
  // If ccaAttemptMax is negative, that is a sentinal indicating
  // the first try should *not* be preceded by a random backoff period.
  // ccaAttempts is abs(ccaAttemptMax).
  uint8_t ccaAttempts = (ccaAttemptMax < 0)
                        ? (uint8_t) -ccaAttemptMax
                        : (uint8_t) ccaAttemptMax;
  if (((uint8_t)ccaAttemptMax != pro2TxCsmaConfig.ccaAttemptMax)
      || (backoffExponentMin     != pro2TxCsmaConfig.backoffExponentMin)
      || (backoffExponentMax     != pro2TxCsmaConfig.backoffExponentMax)
      || (minimumBackoff         != pro2TxCsmaConfig.minimumBackoff)
      || (backoffAdjust          != pro2TxCsmaBackoffAdjust)
      || (myUseZeroBackoff       != pro2TxCsmaConfig.useZeroBackoff)) {
    uint8_t csmaProps[RADIO_CCA_ATTEMPT_MAX_802_15_4 * 2];
    PHY_ASSERT(ccaAttempts <= RADIO_CCA_ATTEMPT_MAX_802_15_4);
    if (myUseZeroBackoff) {
      memset(csmaProps, 0, sizeof(csmaProps));
    } else {
      uint8_t i = 0;
      uint8_t expon = backoffExponentMin;
      if (ccaAttemptMax < 0) { // Sentinal that says:
        // first try has no backoff period
        csmaProps[0] = csmaProps[1] = 0;
        i += 1;
      }
      for (; i < ccaAttempts; i++) {
        csmaProps[i * 2 + 0] = minimumBackoff;
        csmaProps[i * 2 + 1] = (uint8_t)((1u << expon) - 1 + backoffAdjust);
        PHY_ASSERT(csmaProps[i * 2 + 0] <= csmaProps[i * 2 + 1]);
        if (expon < backoffExponentMax) {
          expon += 1;
        }
      }
     #if     TX_WAIT_CCA_MIN_1
      // Enforce a minimum backoff of 1 for first CCA check only.
      // This is an easy but conservative way to avoid CCA failure
      // due to an ACK going out since we pass up RX traffic before
      // its ACK is sent, so a new TX operation can be initiated
      // before then too.
      if (csmaProps[0] == 0) {
        csmaProps[0] = 1;
        if (csmaProps[1] == 0) {
          csmaProps[1] = 1;
        }
      }
     #endif//TX_WAIT_CCA_MIN_1
    }
    pro2WriteNProperties(PROP_GRP_ID_IE154_TX,
                         PROP_IE154_TX_CSMA_BACKOFF_OFFSET,
                         ccaAttempts * 2,
                         csmaProps);
    pro2TxCsmaConfig.ccaAttemptMax      = (uint8_t)ccaAttemptMax;
    pro2TxCsmaConfig.backoffExponentMin = backoffExponentMin;
    pro2TxCsmaConfig.backoffExponentMax = backoffExponentMax;
    pro2TxCsmaConfig.minimumBackoff     = minimumBackoff;
    pro2TxCsmaConfig.useZeroBackoff     = myUseZeroBackoff;
    pro2TxCsmaBackoffAdjust             = backoffAdjust;
  }

 #if     RAIL_LIB_4848_TALK_CCA
  static uint16_t pro2CcaCheckSym = 8; // PROP_IE154_TX_CSMA_ON_SYMBOLS_DEFAULT
  if (ccaCheckSym != pro2CcaCheckSym) {
    // TX_CSMA_ON_SYMBOLS
    pro2CcaCheckSym = ccaCheckSym;
    uint8_t propValue[2] = { HIGH_BYTE(ccaCheckSym), LOW_BYTE(ccaCheckSym) };
   #if     SI4440_1468_TOLERATE
    // SI4440-1468: only low-order 12 bits are actually used for timing in PRO2+
    // Anything larger, peg at max currently supported and hope for the best
    if (propValue[0] > 0x0F) {
      //PHY_ASSERT(false);
      propValue[0] = 0x0F;
      propValue[1] = 0xFF;
    }
   #endif//SI4440_1468_TOLERATE
    pro2WriteNProperties(PROP_GRP_ID_IE154_TX,
                         PROP_IE154_TX_CSMA_ON_SYMBOLS_OFFSET,
                         sizeof(propValue), propValue);
  }
 #endif//RAIL_LIB_4848_TALK_CCA
  return ccaAttempts;
}

static void pro2UpdateRxFlags(uint8_t mask, uint8_t values)
{
  uint8_t newRxFlags = ((phy2RxFlags & ~(mask)) | (values & mask));
  if (newRxFlags != pro2RxFlags) {
    DECLARE_INTERRUPT_STATE;
    DISABLE_INTERRUPTS(); // Protect against emRadioHoldOffIsr() while changing RxFlags
    // Only allow the shadow state to be updated when Radio HoldOff is active
    // but not the real state of the radio
    if (!emRadioHoldOffIsActive()) {
      pro2PowerUp(); // Ensure PRO2+ is up
      pro2WriteOneProperty(PROP_GRP_ID_IE154_RX,
                           PROP_IE154_RX_FLAGS_OFFSET,
                           newRxFlags);
      pro2RxFlags = newRxFlags;
    }
    RESTORE_INTERRUPTS();
    phy2RxFlags = newRxFlags;
  }
}

static void pro2UpdateTxFlags(uint8_t mask, uint8_t values)
{
  uint8_t newTxFlags = ((phy2TxFlags & ~(mask)) | (values & mask));
  if (newTxFlags != pro2TxFlags) {
    pro2PowerUp(); // Ensure PRO2+ is up
    pro2WriteOneProperty(PROP_GRP_ID_IE154_TX,
                         PROP_IE154_TX_FLAGS_OFFSET,
                         newTxFlags);
    phy2TxFlags = pro2TxFlags = newTxFlags;
  }
}

static void pro2UpdateCoFlags(uint8_t mask, uint8_t values)
{
  uint8_t newCoFlags = ((phy2CoFlags & ~(mask)) | (values & mask));
  if (newCoFlags != pro2CoFlags) {
    pro2PowerUp(); // Ensure PRO2+ is up
    pro2WriteOneProperty(PROP_GRP_ID_IE154_COMMON,
                         PROP_IE154_COMMON_FLAGS_OFFSET,
                         newCoFlags);
    phy2CoFlags = pro2CoFlags = newCoFlags;
  }
}

static void pro2UpdateMaxPayloadSize(uint8_t coPhCfg)
{
  uint16_t maxPayload = SL_ZIGBEE_PHY_MAX_PAYLOAD_BYTES;
  if (coPhCfg  & PROP_IE154_COMMON_PHY_CFG_CHECK_CRC_MASK) {
    // Only worry about 2-byte CRC; radio will further reduce
    // this itself for 4-byte CRC based on incoming PHR.
    maxPayload -= 2;
  }
  maxPayload = HTONS(maxPayload); // Put into PRO2+'s big-endian order
  pro2WriteNProperties(PROP_GRP_ID_IE154_COMMON,
                       PROP_IE154_COMMON_MAX_PAYLOAD_SIZE_OFFSET,
                       sizeof(maxPayload), (uint8_t*) &maxPayload);
}

static void pro2UpdateCoPhCfg(uint8_t mask, uint8_t values)
{
  uint8_t newCoPhCfg = ((phy2CoPhCfg & ~(mask)) | (values & mask));
  if (newCoPhCfg != pro2CoPhCfg) {
    pro2PowerUp(); // Ensure PRO2+ is up
    pro2WriteOneProperty(PROP_GRP_ID_IE154_COMMON,
                         PROP_IE154_COMMON_PHY_CFG_OFFSET,
                         newCoPhCfg);
    // If CHECK_CRC was changed, we need to adjust COMMON_MAX_PAYLOAD_SIZE
    // which does not include CRC.
    if ((newCoPhCfg  & PROP_IE154_COMMON_PHY_CFG_CHECK_CRC_MASK)
        != (pro2CoPhCfg & PROP_IE154_COMMON_PHY_CFG_CHECK_CRC_MASK)) {
      pro2UpdateMaxPayloadSize(newCoPhCfg);
    }
    phy2CoPhCfg = pro2CoPhCfg = newCoPhCfg;
  }
}

uint16_t pro2GetPtiLogging(void)
{
  return phy2PtiLog;
}

void pro2SetPtiLogging(uint16_t newPtiLog)
{
  if (newPtiLog != pro2PtiLog) {
    uint8_t ptiEn[2] = { LOW_BYTE(newPtiLog), HIGH_BYTE(newPtiLog) };
    pro2WriteNProperties(PROP_GRP_ID_PTI,
                         PROP_PTI_LOG_EN_OFFSET,
                         sizeof(ptiEn), ptiEn);
    phy2PtiLog = pro2PtiLog = newPtiLog;
  }
}

static void pro2CleanupTxState(uint8_t txReason)
{
  // Clean up Tx state -- which involves its state machine:
  // If a packet was being transmitted, abort it with TX_UNDERFLOW or
  // TX_INCOMPLETE, per caller.
  // If transmit finished and was waiting for an ACK, abort with ACK TIMEOUT.
  sl_status_t reason = SL_STATUS_OK;

  // For all Tx errors except TX_CCA_CANCELLED, assume something
  // went out over the air that's now stopped.
  // This should also cover end of tone & stream.
  if ((txReason & (CMD_IE154_GET_INT_STATUS_REP_IE154_TX_ERR_STATUS_TX_ABORTED_MASK
                   | CMD_IE154_GET_INT_STATUS_REP_IE154_TX_ERR_STATUS_TX_CCA_CANCELLED_MASK
                   | CMD_IE154_GET_INT_STATUS_REP_IE154_TX_ERR_STATUS_TX_FIFO_UNDERFLOW_MASK))
      != CMD_IE154_GET_INT_STATUS_REP_IE154_TX_ERR_STATUS_TX_CCA_CANCELLED_MASK ) {
    lastTxFinishedTimeMs = halCommonGetInt32uMillisecondTick();
    if (radioTransmitConfig.checkCca) {
      lastCsmaTxFinishedTimeMs = lastTxFinishedTimeMs;
    }
  }
  if (emPhyTxState == PHY_TX_STATE_TRIG_ACKING) {
    emPhyTxState = PHY_TX_STATE_OFF;
    reason = SL_STATUS_MAC_NO_ACK_RECEIVED;
  } else
  if (emPhyTxState > PHY_TX_STATE_OFF) {
    emPhyTxState = PHY_TX_STATE_OFF;
    reason = (txReason & CMD_IE154_GET_INT_STATUS_REP_IE154_TX_ERR_STATUS_TX_FIFO_UNDERFLOW_MASK)
             ? SL_STATUS_TRANSMIT_UNDERFLOW : SL_STATUS_TRANSMIT_INCOMPLETE;
  }

  if (reason != SL_STATUS_OK) {
    lastTxStartedTimeMs = -1; // Deem Tx completed
    if (sli_legacy_mfglib_mode) {
      sli_legacy_mfglib_transmit_complete = true;
    } else {
      sli_802154phy_radio_transmit_complete_callback(reason, sli_802154phy_get_mac_timer(), false);
    }
  }
}

static void pro2RestartRadio(uint8_t txReason)
{
  bool resetRx = true; // Always reset Rx side
  bool resetTx = (txReason || (emPhyTxState == PHY_TX_STATE_OFF));
  // Only reset Tx side for a Tx error, or when no transmit is in progress

  // Force Radio to Idle if doing a complete reset of both Rx and Tx
  // In theory it may already be Idle, but this ensures it is.
  if (resetRx && resetTx) {
    pro2IdleRadio();
  }
  pro2RxTxFifoInfo(resetRx, resetTx, NULL, NULL);
  if (resetTx) {
    pro2CleanupTxState(txReason);
  }
  if (resetRx) {
    // Clean up Rx state -- no Rx state machine to worry about
    // though rxBufLen will have been cleared by pro2RxTxFifoInfo().
    // Restart Rx now if we were listening -- but not if Tx is in progress
    // The Tx will ensure the radio transitions to proper state itself.
    if (resetTx) {
      if (phy2Listening) {
        pro2StartRx();
      } else
      if (radioCurrentPowerStatus == SL_ZIGBEE_RADIO_POWER_MODE_ED_ON) {
        pro2StartEd();
      }
    }
  }
}

static void pro2SleepRadio(void)
{
  PHY_ASSERT(!pro2Listening); // Radio must be IDLE to SLEEP it
  // Since SLEEP mode shuts down PRO2's system clock, that would
  // affect our SysClk if we're running off that.  So only SLEEP
  // if we're running on an independent SysClk.
 #if     (SI4440_1462_TOLERATE > 1)
  // Avoid use of START_SLEEP because it disrupts the PTI signal causing it
  // to go low and become innumerable framing errors from ISA3 and Saleae
  // perspective.
  // Ensure IDLE mode was achieved before returning.
  PHY_ATTEST_SUCCESS(siPro2WaitForReady());
 #else//!(SI4440_1462_TOLERATE > 1)
 #if     (SI4440_1462_TOLERATE == 1)
  // SLEEP disrupts the PTI signal causing it to go low and become
  // innumerable framing errors from ISA3 and Saleae perspective.
  // So reprogram this GPIO to be output-high during SLEEP.
  // N.B. This appears to glitch the PTI low causing spurrious 0x1FF
  //      9-bit value to appear on PTI :-(
  if (pro2PtiGpio > PRO2_GPIO_NONE) {
    // Changing GPIO to output-high appears to glitch the PTI low for 0.5 us
    PHY_ATTEST_SUCCESS(pro2GpioConfig(pro2PtiGpio,
                                      (CMD_GPIO_PIN_CFG_ARG_GPIO_GPIO_MODE_ENUM_DRIVE1
                                       << CMD_GPIO_PIN_CFG_ARG_GPIO_GPIO_MODE_LSB)
                                      ));
    pro2PtiGpio = PRO2_GPIO_NONE;
  }
 #endif//(SI4440_1462_TOLERATE == 1)

  uint8_t cmdBuf[] = {
    /*0-Command */ CMD_IE154_START_SLEEP,
  };

  PHY_ATTEST_SUCCESS(siPro2SendCommandNoResponse(sizeof(cmdBuf), cmdBuf));
  // Waking the PRO2+ from its SLEEP mode is easy:  simply assert nSEL
  // and start SCLK clocking for a new SPI transaction.  We purposefully
  // do *NOT* probe CTS here as that would, of course, wake it!
 #endif//(SI4440_1462_TOLERATE > 1)
}

#define MODEM_MOD_TYPE_STREAM (0                                                                                    \
                               | (PROP_MODEM_MOD_TYPE_MOD_TYPE_ENUM_2GFSK << PROP_MODEM_MOD_TYPE_MOD_TYPE_LSB)      \
                               | (PROP_MODEM_MOD_TYPE_MOD_SOURCE_ENUM_PSEUDO << PROP_MODEM_MOD_TYPE_MOD_SOURCE_LSB) \
                               )
#define MODEM_MOD_TYPE_TONE   (0                                                                                    \
                               | (PROP_MODEM_MOD_TYPE_MOD_TYPE_ENUM_CW << PROP_MODEM_MOD_TYPE_MOD_TYPE_LSB)         \
                               | (PROP_MODEM_MOD_TYPE_MOD_SOURCE_ENUM_PACKET << PROP_MODEM_MOD_TYPE_MOD_SOURCE_LSB) \
                               )
#define MODEM_MOD_TYPE_UNKNOWN 0x00 // A value not used in normal operation

static void pro2ToneOrStream(bool start, bool stream)
{
  static uint8_t prevModTypeMode = MODEM_MOD_TYPE_UNKNOWN;
  phy2Listening = false;
  pro2RestartRadio(ANY_TX_ERROR); // Idle the Radio
  if (start) { // Start
    uint8_t slot = phy2NetworkParamsSlotForIndex[emPhyCurrentRadioNetworkIndex()];
    if (prevModTypeMode == MODEM_MOD_TYPE_UNKNOWN) {
      prevModTypeMode = pro2ReadOneProperty(PROP_GRP_ID_MODEM,
                                            PROP_MODEM_MOD_TYPE_OFFSET);
    }
    MAC_INTERRUPTS_OFF(); // Safety: We shouldn't get events during tone/stream
    PHY_ATTEST_SUCCESS(switchBand(phy2NetworkParams[slot].phyChan, false));
    pro2WriteOneProperty(PROP_GRP_ID_MODEM,
                         PROP_MODEM_MOD_TYPE_OFFSET,
                         stream ? MODEM_MOD_TYPE_STREAM : MODEM_MOD_TYPE_TONE);
    uint8_t cmdBuf[] = {
      /*0-Command */ CMD_IE154_START_TX,
      /*1-Conditio*/ 0x00,  // no flags needed for Stream or Tone
      /*2-Network */ slot,
      /*3-NextMode*/ 0x00,  // Next mode ignored for Stream or Tone
    };
    uint8_t sateFifoBuf[2] = { 0U, 0U };
    PHY_ATTEST_SUCCESS(siPro2WriteTxFifo(sizeof(sateFifoBuf), sateFifoBuf));  // Sate the FIFO
    PHY_ATTEST_SUCCESS(siPro2SendCommandNoResponse(sizeof(cmdBuf), cmdBuf));
    PHY_ATTEST_SUCCESS(siPro2WaitForReady()); // Avoids deferring this into the ISR
  } else { // Stop
    pro2WriteOneProperty(PROP_GRP_ID_MODEM,
                         PROP_MODEM_MOD_TYPE_OFFSET,
                         prevModTypeMode);
    prevModTypeMode = MODEM_MOD_TYPE_UNKNOWN;
   #if     SI4440_1403_TOLERATE
    // Stream seems to mess up the bit ordering, but rewriting the
    // PKT_CONFIG1 property with its current value appears to fix it.
    if (stream) {
      pro2WriteOneProperty(PROP_GRP_ID_PKT, PROP_PKT_CONFIG1_OFFSET,
                           pro2ReadOneProperty(PROP_GRP_ID_PKT, PROP_PKT_CONFIG1_OFFSET));
    }
   #endif//SI4440_1403_TOLERATE
    // Resume listening if our power mode says to, otherwise leave radio idle
    switch (radioCurrentPowerStatus) {
      case SL_ZIGBEE_RADIO_POWER_MODE_RX_ON:  pro2StartRx(); break;
      case SL_ZIGBEE_RADIO_POWER_MODE_ED_ON:  pro2StartEd(); break;
      default:                                           break;
    }
    MAC_INTERRUPTS_ON();  // Safety: we shouldn't get events during tone/stream
  }
}

static bool processChEvents(Events *events)
{
  // Events are processed more or less in time and severity order,
  // in case latency causes several to appear simultaneously.
  if (events->chPend & CMD_IE154_GET_INT_STATUS_REP_IE154_CHIP_PEND_CMD_ERROR_PEND_MASK) {
    // Handle CMD_ERROR
    uint8_t cmdRsp[5] = { // Sized for reply
      /*0-Command */ CMD_GET_CHIP_STATUS,
      /*1-CLR_PEND*/ 0xFF,
    };
    PHY_ATTEST_SUCCESS(siPro2SendCommandGetResponse(2, cmdRsp,
                                                    sizeof(cmdRsp), cmdRsp));
    PHY_TRACE_RAW('E', cmdRsp, sizeof(cmdRsp));
    PHY_ASSERT(false);
  }
  if (events->chPend & CMD_IE154_GET_INT_STATUS_REP_IE154_CHIP_PEND_FIFO_UNDERFLOW_OVERFLOW_ERROR_PEND_MASK) {
    // Handle UNDERFLOW/OVERFLOW error
    // This is signalled when the Host sucks too much from the RX FIFO,
    // or puts too much into the TX FIFO.  I.e. it should not happen.
    PHY_ASSERT(false);
  }
  if (events->chPend & CMD_IE154_GET_INT_STATUS_REP_IE154_CHIP_PEND_CHIP_READY_PEND_MASK) {
    // Handle READY
    // Ignore
  }
  if (events->chPend & CMD_IE154_GET_INT_STATUS_REP_IE154_CHIP_PEND_STATE_CHANGE_PEND_MASK) {
    // Handle STATE_CHANGE
    // Ignore
  }
  if (events->chPend & CMD_IE154_GET_INT_STATUS_REP_IE154_CHIP_PEND_CAL_PEND_MASK) {
    //TODO: handle CAL_PEND
    PHY_ASSERT(false);
  }
  if (events->chPend & CMD_IE154_GET_INT_STATUS_REP_IE154_CHIP_PEND_LOW_BATT_PEND_MASK) {
    // Handle LOW_BATT
    PHY_ASSERT(false);
  }
  if (events->chPend & CMD_IE154_GET_INT_STATUS_REP_IE154_CHIP_PEND_WUT_PEND_MASK) {
    // Handle WUT
    PHY_ASSERT(false);
  }
  return false;
}

static void updateRadioCurrentPowerStatusPostTx(void)
{
  // Update radioCurrentPowerStatus and listening state to reflect
  // the post-Tx setting recorded when START_TX was issued.
  phy2Listening = radioPostTxListening;
  PHY_DEBUG(pro2Listening = radioPostTxListening);
  radioCurrentPowerStatus = radioPostTxListening
                            ? SL_ZIGBEE_RADIO_POWER_MODE_RX_ON
                            : SL_ZIGBEE_RADIO_POWER_MODE_TX_ON;
}

static bool processTxEvents(Events *events)
{
  // In case latency causes several events to appear simultaneously,
  // process them more or less in their expected time order, with
  // errors last, because they likely pertain to events occurring after
  // the good events.
  if (events->txPend & CMD_IE154_GET_INT_STATUS_REP_IE154_TX_PEND_TX_FIFO_ALMOST_EMPTY_PEND_MASK) {
    // Handle TX_FIFO_ALMOST_EMPTY
    if ((emPhyTxState > PHY_TX_STATE_OFF) && (bytesToWrite > 0)) {
      uint8_t txAvail = events->txAvail;
      // We have more to send, and we know the TX FIFO now has at least
      // txAvail bytes of space available -- fill it!
      if (bytesToWrite > txAvail) {
        PHY_ATTEST_SUCCESS(siPro2WriteTxFifo(txAvail, txBufPending));
        bytesToWrite -= txAvail;
        txBufPending += txAvail;
      } else {
        PHY_ATTEST_SUCCESS(siPro2WriteTxFifo(bytesToWrite, txBufPending));
        bytesToWrite = 0;
        txBufPending = NULL;
        // In theory we could turn off this event now, but that takes ISR time...
      }
    }
  }
  if (events->txPend & CMD_IE154_GET_INT_STATUS_REP_IE154_TX_PEND_TX_SFD_SENT_PEND_MASK) {
    // Handle SFD_SENT
    pro2SetTxSfdTime();
  }
  if (events->txPend & CMD_IE154_GET_INT_STATUS_REP_IE154_TX_PEND_TX_COMPLETE_PEND_MASK) {
    // Handle TX_COMPLETE
    lastTxFinishedTimeMs = halCommonGetInt32uMillisecondTick();
    PHY_ASSERT(emPhyTxState > PHY_TX_STATE_OFF);
    if (emPhyTxState == PHY_TX_STATE_TRIG_W_ACK) {
      emPhyTxState = PHY_TX_STATE_TRIG_ACKING;
    } else {
      updateRadioCurrentPowerStatusPostTx();
      emPhyTxState = PHY_TX_STATE_OFF;
    }
    if (radioTransmitConfig.checkCca) {
      lastCsmaTxFinishedTimeMs = lastTxFinishedTimeMs;
    }
    // Mfglib handles Tx completion differently, below.
    // Don't issue callback in the case we're waiting on the ACK
    if (!sli_legacy_mfglib_mode && (emPhyTxState == PHY_TX_STATE_OFF)) {
      PHY_TRACE_PKT('x', NULL);
      lastTxStartedTimeMs = -1; // Deem Tx completed
      sli_802154phy_radio_transmit_complete_callback(SL_STATUS_OK,
                                                     pro2GetTxSfdTime(),
                                                     false);
    }
  }
  if (events->txPend & (CMD_IE154_GET_INT_STATUS_REP_IE154_TX_PEND_TX_ACK_RECEIVED_W_FP_PEND_MASK
                        | CMD_IE154_GET_INT_STATUS_REP_IE154_TX_PEND_TX_ACK_RECEIVED_NO_FP_PEND_MASK)) {
    uint8_t framePending = !!(events->txPend & CMD_IE154_GET_INT_STATUS_REP_IE154_TX_PEND_TX_ACK_RECEIVED_W_FP_PEND_MASK);
    // Handle TX_ACK_RECEIVED -- partly an Rx event
    // When the PRO2+ signals either of these, the ACK has been received
    // and might be in the RX FIFO to be consumed.
    // It will be in the FIFO if we've asked for it to be, or filtering is off
    // (receiver is promiscuous).
    if ((phy2TxFlags & PROP_IE154_TX_FLAGS_ACK_IN_FIFO_MASK)
        || ((phy2RxFlags & PROP_IE154_RX_FLAGS_FILTERING_MASK) == 0)
        ) {
      // If so, the PRO2+ won't signal RX_COMPLETE for it, so we manually
      // post that to cause the ACK to be retrieved by Rx processing code.
      events->rxPend |= CMD_IE154_GET_INT_STATUS_REP_IE154_RX_PEND_RX_COMPLETE_PEND_MASK;
    } else {
      // If not, then clear the TX_ACK_RECEIVED event so if RX_COMPLETE
      // was also posted, we won't interpret that frame as an expected ACK.
      events->txPend &= ~(CMD_IE154_GET_INT_STATUS_REP_IE154_TX_PEND_TX_ACK_RECEIVED_W_FP_PEND_MASK
                          | CMD_IE154_GET_INT_STATUS_REP_IE154_TX_PEND_TX_ACK_RECEIVED_NO_FP_PEND_MASK);
    }
    if (emPhyTxState >= PHY_TX_STATE_TRIG_W_ACK) {
      // This Ack is expected.
      PHY_TRACE_PKT('\x29', NULL);    // right paren
      if (framePending) {
        // This is a HACK to tell sli_802154phy_radio_transmit_complete_callback() that
        // PRO2+ needs a longer frame response timeout than normal 802.15.4.
        framePending = SL_ZIGBEE_PHY_MAX_FRAME_RESPONSE_MULTIPLIER;
        // An ACK with framePending set also indicates that the radio
        // has ignored START_TX's TX_COMPLETE mode and is LISTENing
        // in the PEND_DATA phase.  We must correct our internal state
        // that emRadioTransmit() assumed we would be in after the Tx:
        radioCurrentPowerStatus = SL_ZIGBEE_RADIO_POWER_MODE_RX_ON;
        phy2Listening = true;
        PHY_DEBUG(pro2Listening = true);
         #if     (PRO2_MAX_NETWORKS > 2)
        // In multi-network, the PRO2+ will switch its Rx to continue
        // listening on the channel of the Tx when an ACK is received
        // with FP set, so note this to ensure we properly restore the
        // listening Rx channel when upper layers say we can via
        // sli_802154phy_radio_ok_to_idle().
        listeningPhyChan = lastTransmitPhyChan;
         #endif//(PRO2_MAX_NETWORKS > 2)
      } else {
        updateRadioCurrentPowerStatusPostTx();
      }
      emPhyTxState = PHY_TX_STATE_OFF;
      // Process Ack, indicating whether frame pending was set in FCF or not.
      lastTxStartedTimeMs = -1;   // Deem Tx completed
      sli_802154phy_radio_transmit_complete_callback(SL_STATUS_TRANSMIT_ACK_RECEIVED,
                                                     pro2GetTxSfdTime(),
                                                     framePending);
    }
  }
  if (events->txPend & CMD_IE154_GET_INT_STATUS_REP_IE154_TX_PEND_TX_ERROR_PEND_MASK) {
    // Handle TX_ERROR set -- refined by txError field
    if (events->txError & CMD_IE154_GET_INT_STATUS_REP_IE154_TX_ERR_STATUS_TX_ACK_EXPIRED_MASK) {
      // Handle TX_ACK_EXPIRED (let ACK_RECEIVED take precedence above)
      if (emPhyTxState >= PHY_TX_STATE_TRIG_W_ACK) {
        updateRadioCurrentPowerStatusPostTx();
        emPhyTxState = PHY_TX_STATE_OFF;
        PHY_TRACE_PKT('?', NULL);
        lastTxStartedTimeMs = -1; // Deem Tx completed
        sli_802154phy_radio_transmit_complete_callback(SL_STATUS_MAC_NO_ACK_RECEIVED,
                                                       pro2GetTxSfdTime(),
                                                       false);
      }
    }
    if (events->txError & CMD_IE154_GET_INT_STATUS_REP_IE154_TX_ERR_STATUS_TX_CCA_FAIL_MASK) {
      // Handle CCA fail -- all CCA backoffs have failed
      PHY_ASSERT(emPhyTxState > PHY_TX_STATE_OFF);
      updateRadioCurrentPowerStatusPostTx();
      emPhyTxState = PHY_TX_STATE_OFF;
      PHY_TRACE_PKT('-', NULL);
      lastTxStartedTimeMs = -1; // Deem Tx completed
      emPhyDcSetExtendedStatus(EM_PHY_DC_EXT_STATUS_LBT_TRIES_EXCEEDED);
      if (!sli_legacy_mfglib_mode) {
        sli_802154phy_radio_transmit_complete_callback(SL_STATUS_CCA_FAILURE,
                                                       0, // No SFD time if never sent
                                                       false);
      }
      // N.B. The PRO2+ TX FIFO still contains the packet that failed to be
      // transmitted.  That will be cleaned out on the next emRadio+
    }
    if (events->txError & (CMD_IE154_GET_INT_STATUS_REP_IE154_TX_ERR_STATUS_TX_ABORTED_MASK
                           | CMD_IE154_GET_INT_STATUS_REP_IE154_TX_ERR_STATUS_TX_CCA_CANCELLED_MASK
                           | CMD_IE154_GET_INT_STATUS_REP_IE154_TX_ERR_STATUS_TX_FIFO_UNDERFLOW_MASK)) {
      // Technically, FIFO_UNDERFLOW should indicate a bug and assert()
      // but looking forward to the day we support extra long frames, it
      // will mean we couldn't keep pace, and will need to clean up and
      // try again.  FIFO_UNDERFLOW may also trigger TX_ABORTED.
      // The others, TX_ABORTED and TX_CCA_CANCELLED, can occur if we cancel
      // a transmit.  The latter simply indicates the TX FIFO is intact if
      // we want to retransmit -- but we don't so clean up in both cases.
      PHY_TRACE_PKT('e', NULL);
      pro2RestartRadio(events->txError);
      return true; // No sense continuing with any other Tx events
    }
    if ((events->txError == 0)
        || (events->txError & ~(CMD_IE154_GET_INT_STATUS_REP_IE154_TX_ERR_STATUS_TX_FIFO_UNDERFLOW_MASK
                                | CMD_IE154_GET_INT_STATUS_REP_IE154_TX_ERR_STATUS_TX_ABORTED_MASK
                                | CMD_IE154_GET_INT_STATUS_REP_IE154_TX_ERR_STATUS_TX_CCA_CANCELLED_MASK
                                | CMD_IE154_GET_INT_STATUS_REP_IE154_TX_ERR_STATUS_TX_ACK_EXPIRED_MASK
                                | CMD_IE154_GET_INT_STATUS_REP_IE154_TX_ERR_STATUS_TX_CCA_FAIL_MASK))) {
      // TX_ERROR with TX_ERR_STATUS showing unsupported bits or no bits at all
      PHY_ASSERT(false);
    }
  }

  // mfglibSendPacket waits after calling emRadioTransmit until
  // sli_legacy_mfglib_transmit_complete is set to true. This needs to be set whenever
  // this ISR is called except for FIFO_ALMOST_EMPTY and SFD_SENT interrupts.
  if (sli_legacy_mfglib_mode
      && (events->txPend
          & ~(CMD_IE154_GET_INT_STATUS_REP_IE154_TX_PEND_TX_FIFO_ALMOST_EMPTY_PEND_MASK
              | CMD_IE154_GET_INT_STATUS_REP_IE154_TX_PEND_TX_SFD_SENT_PEND_MASK))) {
    lastTxStartedTimeMs = -1; // Deem Tx completed
    sli_legacy_mfglib_transmit_complete = true;
  }
  return false;
}

static sl_status_t parsePhr(int16_t* payLen, uint8_t* crcLen, uint8_t** payload)
{
  if (IE154G_ACTIVE()) {
    // 802.15.4g-2012 MR-FSK 2-byte PHR format
    // The PRO2+ has rejiggered the PHR bits to be in a Host-friendly format:
    // Byte 0 = [ MS | 0 | 0 | FCS | DW | Length<10:8> ]
    // Byte 1 = [              Length<7:0>             ]
    if (rxBufLen < 2) {
      PHY_TRACE_RAW('h', &rxBufLen, 2);
      return SL_STATUS_FAIL;
    }
    if ((rxBuffer[0] != ((PHR_154G_FCS_MASK | PHR_154G_DW_MASK) >> 8))
        || (rxBuffer[1] > SL_ZIGBEE_PHY_MAX_PAYLOAD_BYTES)) {
      PHY_TRACE_RAW('g', rxBuffer, 2);
      // RAIL_LIB-4848: GB868 only allows whitened 2-byte CRC frames < 128 bytes
      return SL_STATUS_FAIL;
    }
    *payLen = rxBuffer[1];
    *payload = &rxBuffer[2];
    if (emRadioReceiveCrcEnabled()) {
      *crcLen = (rxBuffer[0] & (PHR_154G_FCS_MASK >> 8)) ? 2 : 4;
    } else {
      *crcLen = 0;
    }
    // payload[-1] contains LSB of length byte -- just like 802.15.4-2011 PHR
  } else {
    if (rxBufLen < 1) {
      PHY_TRACE_RAW('h', &rxBufLen, 2);
      return SL_STATUS_FAIL;
    }
    // 802.15.4-2003/6/11 format with PHR 7-bit length
    if (rxBuffer[0] > SL_ZIGBEE_PHY_MAX_PAYLOAD_BYTES) {
      // RAIL_LIB-4848: Only allow frames < 128 bytes
      PHY_TRACE_RAW('g', rxBuffer, 1);
      return SL_STATUS_FAIL;
    }
    *payLen = rxBuffer[0];
    *payload = &rxBuffer[1];
    if (emRadioReceiveCrcEnabled()) {
      *crcLen = 2;
    } else {
      *crcLen = 0;
    }
  }
  if (*payLen < *crcLen) {
    PHY_TRACE_RAW('i', payLen, 2);
    return SL_STATUS_FAIL;
  }
  PHY_TRACE_RAW('H', rxBuffer, rxBufLen);
  return SL_STATUS_OK;
}

// Read packet payload and appended information
static sl_status_t parsePay(uint16_t payLen, uint8_t crcLen, uint8_t* payload,
                            int16_t* frameBufLen)
{
  int16_t bytesToRead = (payload - rxBuffer)     // PHR length
                        + payLen                // Payload length (incl CRC)
                        + PRO2_APPENDED_INFO_BYTES // Appended Info length
                        - crcLen;               // What radio won't give us
  if (bytesToRead > (int16_t) rxBufLen) {
    PHY_TRACE_RAW('j', &bytesToRead, 2);
    PHY_TRACE_RAW('k', &rxBufLen, 2);
    return SL_STATUS_FAIL;
  }
  PHY_ASSERT(bytesToRead >= 0);
  // RAIL_LIB-4848 - PRO2+ only implemented the first 3 bytes of its
  // 8-byte appended info (PHY Channel Number, RxFrame Status, Latched RSSI)
  // so the remaining 5 bytes (LQI, Gain, MAC Timer Bytes 2,1,0) should all
  // be zero.  If they are not, something is screwy, e.g. a packet got
  // aborted and a subsequent packet came in completely.  We don't know the
  // order of RX_ABORTED vs. RX_COMPLETE events if both are handled
  // simultaneously and usually assume RX_COMPLETE occurred first, but this
  // helps trap the opposite order or some other funky thing that happened.
  if ((rxBuffer[bytesToRead - 1] != 0U)
      || (rxBuffer[bytesToRead - 2] != 0U)
      || (rxBuffer[bytesToRead - 3] != 0U)
      || (rxBuffer[bytesToRead - 4] != 0U)
      || (rxBuffer[bytesToRead - 5] != 0U)) {
    PHY_TRACE_RAW('j', &bytesToRead, 2);
    // Don't emit 'k' trace to distinguish this error path
    return SL_STATUS_FAIL;
  }
  // Make it appear we read the CRC into the buffer
  // by shifting the appended info over by crcLen bytes
  // and also insert byte of padding like em3xx does if
  // the payload length is even.
  uint8_t padLen = ((payLen & 1) ? 0 : 1);
  // Hack to trip compiler error if PRO2_APPENDED_INFO_BYTES != NUM_APPENDED_INFO_BYTES
  // #if won't work because NUM_APPENDED_INFO_BYTES is an enum not a #define
  // Below code will need reworking if this compiler error trips.
  _Static_assert(PRO2_APPENDED_INFO_BYTES == NUM_APPENDED_INFO_BYTES,
                 "PRO2_APPENDED_INFO_BYTES != NUM_APPENDED_INFO_BYTES");
  memmove(payload + payLen + padLen,
          payload + payLen - crcLen,
          PRO2_APPENDED_INFO_BYTES + rxBufLen - bytesToRead);
  rxBufLen += (crcLen + padLen);
  *frameBufLen = (payload - rxBuffer) + payLen + padLen + NUM_APPENDED_INFO_BYTES;
  PHY_ASSERT((int16_t)rxBufLen >= *frameBufLen);
  PHY_ASSERT(*frameBufLen < (int16_t)sizeof(rxBuffer));

  return SL_STATUS_OK;
}

// Bitmasks per 802.15.4e-2012 Table 2a, indexed by:
//   SrcAddrMode!=0,FrameVersion<1>,DstAddrMode!=0,PanIdCompression
// * indicates illegal configurations, not flagged herein
static
const uint16_t dstPanIdPresent = (0              // Src Version Dst PanIdComp Line
                                  | (false <<  0)//  No  2003/6  No    No      1
                                  | (false <<  1)//  No  2003/6  No   Yes      *
                                  | (true  <<  2)//  No  2003/6 Yes    No      7
                                  | (true  <<  3)//  No  2003/6 Yes   Yes      *
                                  | (false <<  4)//  No  2012    No    No      2
                                  | (true  <<  5)//  No  2012    No   Yes      3
                                  | (true  <<  6)//  No  2012   Yes    No      8
                                  | (false <<  7)//  No  2012   Yes   Yes      9
                                  | (false <<  8)// Yes  2003/6  No    No      4
                                  | (false <<  9)// Yes  2003/6  No   Yes      *
                                  | (true  << 10)// Yes  2003/6 Yes    No     10
                                  | (true  << 11)// Yes  2003/6 Yes   Yes     11
                                  | (false << 12) // Yes  2012    No    No      5
                                  | (false << 13) // Yes  2012    No   Yes      6
                                  | (true  << 14)// Yes  2012   Yes    No     12
                                  | (false << 15) // Yes  2012   Yes   Yes     13
                                  );
static
const uint16_t srcPanIdPresent = (0              // Src Version Dst PanIdComp Line
                                  | (false <<  0)//  No  2003/6  No    No      1
                                  | (false <<  1)//  No  2003/6  No   Yes      *
                                  | (false <<  2)//  No  2003/6 Yes    No      7
                                  | (false <<  3)//  No  2003/6 Yes   Yes      *
                                  | (false <<  4)//  No  2012    No    No      2
                                  | (false <<  5)//  No  2012    No   Yes      3
                                  | (false <<  6)//  No  2012   Yes    No      8
                                  | (false <<  7)//  No  2012   Yes   Yes      9
                                  | (true  <<  8)// Yes  2003/6  No    No      4
                                  | (true  <<  9)// Yes  2003/6  No   Yes      *
                                  | (true  << 10)// Yes  2003/6 Yes    No     10
                                  | (false << 11) // Yes  2003/6 Yes   Yes     11
                                  | (true  << 12)// Yes  2012    No    No      5
                                  | (false << 13) // Yes  2012    No   Yes      6
                                  | (false << 14) // Yes  2012   Yes    No     12
                                  | (false << 15) // Yes  2012   Yes   Yes     13
                                  );
#define PAN_ID_PRESENT_BIT(control) (0                                                                         \
                                     | ((!!((control) & MAC_FRAME_SOURCE_MODE_MASK)) << 3)                     \
                                     | ((((control) & MAC_FRAME_VERSION_MASK) >= MAC_FRAME_VERSION_2012) << 2) \
                                     | ((!!((control) & MAC_FRAME_DESTINATION_MODE_MASK)) << 1)                \
                                     | ((!!((control) & MAC_FRAME_FLAG_INTRA_PAN)) << 0)                       \
                                     )
#define DST_PAN_ID_PRESENT(control) (!!(dstPanIdPresent & (1 << PAN_ID_PRESENT_BIT(control))))
#define SRC_PAN_ID_PRESENT(control) (!!(srcPanIdPresent & (1 << PAN_ID_PRESENT_BIT(control))))

static bool processRxSrcAddr(Events* events)
{
  UNUSED_VAR(events);
  int16_t payLen;
  uint8_t crcLen;
  uint8_t* macHeader;
  // This assert() may have a race with test code changing filtering on the fly
  PHY_ASSERT(phy2RxFlags & PROP_IE154_RX_FLAGS_FILTERING_MASK);
  if (parsePhr(&payLen, &crcLen, &macHeader) != SL_STATUS_OK) {
    // Malformed frame
    // RAIL_LIB-4848 - no longer assert, just ignore it and let later
    // RX_COMPLETE / RX_ERROR events clean up.
    PHY_TRACE_RAW('B', &payLen, 2);
    return true;
  }
  // We should have at least up through the source address fields of this frame
  uint16_t control = HIGH_LOW_TO_INT(macHeader[1], macHeader[0]);

  // Only need to do frame pending determination on ACK-requesting data polls
  // when AUTO-ACK is enabled and a source address is presented.
  // For "data poll" here, we initially just ensure the frame is of type
  // MAC_FRAME_TYPE_CONTROL.
  if (((control & (MAC_FRAME_FLAG_ACK_REQUIRED
                   | MAC_FRAME_TYPE_MASK))
       == (MAC_FRAME_FLAG_ACK_REQUIRED | MAC_FRAME_TYPE_CONTROL))
      && ((control & MAC_FRAME_SOURCE_MODE_MASK) != MAC_FRAME_SOURCE_MODE_NONE)
      && (phy2RxFlags & PROP_IE154_RX_FLAGS_AUTO_ACK_DEFAULT_VALUE_MASK)) {
    uint16_t srcAddressMode = (control & MAC_FRAME_SOURCE_MODE_MASK);
    uint16_t dstAddressMode = (control & MAC_FRAME_DESTINATION_MODE_MASK);
    sl_802154_pan_id_t panId = 0xFFFF; // Default panId if none within frame
    UNUSED_VAR(panId);  //TODO: Remove when this actually does get used
    bool havePendingData = false;
    uint8_t payIndex = 2; // Start search right after frame control
    uint8_t seqNumber;

    // Rely on the PRO2+'s filtering to pass us only valid stuff, so no need to
    // check FRAME_VERSION before using upward compatible fields like
    // SEQ_SUPPRESSION or the BYTE (simple) addressing modes.
    if (control & MAC_FRAME_FLAG_SEQ_SUPPRESSION) {
      seqNumber = 0xFF; // Flag for unknown seq# when suppressed
    } else {
      seqNumber = macHeader[payIndex];
      payIndex += 1;
    }
    // Source address comes after destination PAN ID and address
    if (DST_PAN_ID_PRESENT(control)) {
      panId = HIGH_LOW_TO_INT(macHeader[payIndex + 1], macHeader[payIndex]);
      payIndex += 2; // pan id size
    }

    switch (dstAddressMode) {
      case MAC_FRAME_DESTINATION_MODE_NONE:
        break;
      case MAC_FRAME_DESTINATION_MODE_BYTE:
        payIndex += 1; // simple address size
        break;
      case MAC_FRAME_DESTINATION_MODE_SHORT:
        payIndex += 2; // short address size
        break;
      case MAC_FRAME_DESTINATION_MODE_LONG:
        payIndex += 8; // long address size
        break;
      default:
        PHY_ASSERT(false);
        break;
    }
    if (SRC_PAN_ID_PRESENT(control)) {
      // Source PAN Id overrides destination PAN Id if it was specified
      panId = HIGH_LOW_TO_INT(macHeader[payIndex + 1], macHeader[payIndex]);
      payIndex += 2; // pan id size
    }

    // Ask the network layer if it has any pending data for this device.
    switch (srcAddressMode) {
      case MAC_FRAME_SOURCE_MODE_LONG:
        if ((payIndex + 8) > payLen) {
          // Malformed frame
          // RAIL_LIB-4848 - no longer assert, just ignore it and let later
          // RX_COMPLETE / RX_ERROR events clean up.
          PHY_TRACE_RAW('B', &payLen, 2);
          return true;
        }
        havePendingData = emberRadioDataPendingLongIdPanIdIsrCallback(&macHeader[payIndex], panId);
        payIndex += 8;
        break;
      case MAC_FRAME_SOURCE_MODE_SHORT:
      case MAC_FRAME_SOURCE_MODE_BYTE:
      {
        uint16_t shortAddress;
        if (srcAddressMode == MAC_FRAME_SOURCE_MODE_SHORT) { // 16-bit short address
          if ((payIndex + 2) > payLen) {
            // Malformed frame
            // RAIL_LIB-4848 - no longer assert, just ignore it and let later
            // RX_COMPLETE / RX_ERROR events clean up.
            PHY_TRACE_RAW('B', &payLen, 2);
            return true;
          }
          shortAddress = HIGH_LOW_TO_INT(macHeader[payIndex + 1], macHeader[payIndex]);
          payIndex += 2;
        } else { // 8-bit short address
          if ((payIndex + 1) > payLen) {
            // Malformed frame
            // RAIL_LIB-4848 - no longer assert, just ignore it and let later
            // RX_COMPLETE / RX_ERROR events clean up.
            PHY_TRACE_RAW('B', &payLen, 2);
            return true;
          }
          shortAddress = macHeader[payIndex];
          payIndex += 1;
        }
        havePendingData = emberRadioDataPendingShortIdPanIdIsrCallback(shortAddress, panId);
        break;
      }
      case MAC_FRAME_SOURCE_MODE_NONE:
        if (payIndex > payLen) {
          // Malformed frame
          // RAIL_LIB-4848 - no longer assert, just ignore it and let later
          // RX_COMPLETE / RX_ERROR events clean up.
          PHY_TRACE_RAW('B', &payLen, 2);
          return true;
        }
        break;
      default:
        PHY_ASSERT(false);
        break;
    }

    // On PRO2+ it is tricky and time-consuming to wait around for the MAC
    // control byte to verify the control really is a data poll.  It is
    // usually the last byte in the frame prior to CRC, and the PRO2+ FW
    // generally won't place this byte into the RX FIFO until *after* CRC
    // has been processed -- a signficant delay leaving precious little
    // time to do things like check FIFO_INFO or for RX_COMPLETE and read
    // the byte.  Hence this code is under conditional compilation.
    // The consequence is we might set Frame Pending in an outgoing ACK
    // for a non-data-poll.  Only a stupid peer would actually pay attention
    // to that bit and stay awake, as the peer obviously knows whether it
    // sent a data poll or not.
   #if    FP_DATA_POLL_COMPLY
    // Wait for MAC control byte to determine if this is a data poll.
    // We only want to set the Frame pending bit if this is a data poll.
    // For ZigBee, the next byte is the MAC control byte.
    // For Thread, there may be an Aux Security Header present before the
    // desired byte; but a Data Poll is the only MAC command Thread uses
    // security, so if such a header is present then we can assume it must
    // be a data poll without actually having to check.
    if (havePendingData && (!(control & MAC_FRAME_FLAG_SECURITY_ENABLED))) {
      // This is a MAC control frame with the appropriate header length.
      // Wait for the control byte.
      //TODO: Fix this to handle 802.15.4e IEs, Security, etc.
      PHY_ASSERT(payIndex < payLen);
      halResetWatchdog(); // Give ourselves a new watchdog slice
      while (rxBufLen < (&macHeader[payIndex + 1] - rxBuffer)) {
        uint8_t rxAvail;
        // Unfortunately there is not an FRR to quickly get #bytes in RX FIFO
        pro2RxTxFifoInfo(false, false, &rxAvail, NULL);
        if (rxAvail > 0) {
          PHY_ATTEST_SUCCESS(siPro2ReadRxFifo(rxAvail, &rxBuffer[rxBufLen]));
          rxBufLen += rxAvail;
        }
      }
      halResetWatchdog(); // Give ourselves a new watchdog slice

      if (macHeader[payIndex] != DATA_REQUEST_COMMAND) {
        // This control packet is not a data poll; Do not set Frame Pending.
        havePendingData = false;
      }
    }
   #endif//DATA_POLL_COMPLY

    // Set the state of the frame pending flag for later Rx completion callback
    framePendingSetInOutgoingAck = havePendingData;

    // Toggle the frame pending bit if necessary.
    if (havePendingData
        != (bool) !!(phy2RxFlags & PROP_IE154_RX_FLAGS_FRAME_PENDING_DEFAULT_VALUE_MASK)) {
      uint8_t cmdRsp[] = {
        /*0-Command */ CMD_IE154_OVERRIDE_NEXT_ACK,
        /*1-Sequence*/ seqNumber,
        /*2-Next    */ (((uint8_t)havePendingData)
                        << CMD_IE154_OVERRIDE_NEXT_ACK_ARG_NEXT_FRAME_PENDING_LSB),
        // No AUTO_ACK override is supported here
      };
      // Reuse cmdRsp[] for response
      PHY_ATTEST_SUCCESS(siPro2SendCommandGetResponse(sizeof(cmdRsp), cmdRsp,
                                                      1, cmdRsp));
      if (cmdRsp[0] != CMD_IE154_OVERRIDE_NEXT_ACK_REP_STATUS_STATUS_ENUM_STATUS_SUCCESS) {
        // cmdRsp[0] could be ACK_TIMING_INVALID or SEQUENCE_NUMBER_INVALID
        // but both generally mean the same:  we missed the timing window.
        // Flag that we missed it :-(
        framePendingSetInOutgoingAck = !!(phy2RxFlags & PROP_IE154_RX_FLAGS_FRAME_PENDING_DEFAULT_VALUE_MASK);
      }
    }
  }
  return false;
}

static void resetForNextFrame(int16_t frameBufLen)
{
  // Reset for next Rx by removing frameBufLen bytes from head of rxBuffer[]
  if (rxBufLen > frameBufLen) {
    memmove(rxBuffer, &rxBuffer[frameBufLen], rxBufLen - frameBufLen);
  }
  rxBufLen -= frameBufLen;
}

static bool processedExtraRxComplete = false;

static bool processRxComplete(Events* events, bool ignorePartial)
{
  int16_t payLen = 0;
  uint8_t crcLen;
  uint8_t* payload;
  int16_t frameBufLen;
  // At this point, rxBuffer[] should contain one aligned frame and
  // possibly the start of another
  if ((parsePhr(&payLen, &crcLen, &payload) != SL_STATUS_OK)
      || (parsePay(payLen, crcLen, payload, &frameBufLen) != SL_STATUS_OK)) {
    // Malformed frame
    if (!ignorePartial) {
      // RAIL_LIB-4848 - no longer assert, just attempt to recover by
      // treating this like an RX_ABORTED error without disturbing any
      // TX in progress and hope PRO2+ can recover/resync itself
      // without getting pathologically stuck in a rut.
      PHY_TRACE_RAW('l', &payLen, 2);
      PHY_TRACE_RAW('m', &ignorePartial, 1);
      PHY_TRACE_RAW('n', &processedExtraRxComplete, 1);
      PHY_TRACE_OP(emPhyTracePrint(phyTracePort); );
      pro2RestartRadio(NO_TX_ERRORS);
    }
    return true; // Tell caller we failed to find any packet to send up
  }

  // Appended Info gets word-aligned to be compatible with em3xx radio DMA
  uint8_t *appendedInfo = &payload[payLen + ((payLen & 1) ? 0 : 1)];

  // PRO2+ Rx Appended info from the Rx FIFO is of the form:
  // Byte 0: PHY Channel Number
  //         (overwritten by MacPg channel below)
  // Byte 1: RxFrame Status (CRC_ERROR 0x80, EXPECTED_ACK 0x40, NET_MASK 0x0F)
  //         (overwritten by Network Index below)
  // Byte 2: Latched RSSI
  //         (converted from PRO2 to dBm units below)
  // Byte 3: LQI              placeholder (not implemented; 0)
  //         (overwritten with an LQI derived from RSSI below)
  // Byte 4: Gain             placeholder (not implemented; 0)
  //         (left intact)
  // Byte 5: MAC Timer Byte 2 placeholder (not implemented; 0)
  //         (overwritten with sli_802154phy_get_mac_timer() value)
  // Byte 6: MAC Timer Byte 1 placeholder (not implemented; 0)
  //         (overwritten with sli_802154phy_get_mac_timer() value)
  // Byte 7: MAC Timer Byte 0 placeholder (not implemented; 0)
  //         (overwritten with sli_802154phy_get_mac_timer() value)
  // These definitions are unfortunately not in any PRO2+ API include headers:
  #define PRO2_RX_APPENDED_INFO_STATUS_CRC_ERROR     0x80
  #define PRO2_RX_APPENDED_INFO_STATUS_EXPECTED_ACK  0x40
  #define PRO2_RX_APPENDED_INFO_STATUS_NETWORK_MASK  0x0F

  uint8_t pro2RxStatus = appendedInfo[APPENDED_INFO_FRAME_STATUS_BYTE_0_INDEX];
  // N.B. PRO2_RX_APPENDED_INFO_STATUS_EXPECTED_ACK never got implemented.
  if ((pro2RxStatus & PRO2_RX_APPENDED_INFO_STATUS_CRC_ERROR)
      && (emRadioReceiveCrcEnabled())) {
    // Not an Ack.  This is a packet with CRC (or other) failure.
    resetForNextFrame(frameBufLen);
    return false;
  }

  // Per Si446x Datasheet:  RSSIdBm = (RSSIpro2 / 2) - RSSIcal
  // In our realm, RSSIcal is represented by RSSI_DBM_OFFSET.
  int8_t rssidBm = (appendedInfo[APPENDED_INFO_RSSI_BYTE_INDEX] / 2)
                   + RSSI_DBM_OFFSET_PRO2CLASS;
  appendedInfo[APPENDED_INFO_RSSI_BYTE_INDEX] = rssidBm;
  // The PRO2+ currently does not give us an LQI value, so we
  // must come up with one, based on RSSI.  We'll use ED for
  // convenience since test houses like to see 0 and 255 achieved.
  appendedInfo[APPENDED_INFO_LQI_BYTE_INDEX] = sli_802154phy_radio_convert_rssi_to_ed(rssidBm);
  if ((appendedInfo[APPENDED_INFO_MAC_TIMER_BYTE_2_INDEX] == 0)
      && (appendedInfo[APPENDED_INFO_MAC_TIMER_BYTE_1_INDEX] == 0)
      && (appendedInfo[APPENDED_INFO_MAC_TIMER_BYTE_0_INDEX] == 0)) {
    uint32_t time = sli_802154phy_get_mac_timer();
    appendedInfo[APPENDED_INFO_MAC_TIMER_BYTE_2_INDEX] = (uint8_t)(time >> 16);
    appendedInfo[APPENDED_INFO_MAC_TIMER_BYTE_1_INDEX] = (uint8_t)(time >>  8);
    appendedInfo[APPENDED_INFO_MAC_TIMER_BYTE_0_INDEX] = (uint8_t)(time >>  0);
  }

  uint8_t netIndex = sli_mac_get_current_radio_network_index(0); //mac_index
 #if     (PRO2_MAX_NETWORKS > 2)
  if (emRadioAddressMatchingEnabled()) {
    uint8_t pktNetMask = (pro2RxStatus & PRO2_RX_APPENDED_INFO_STATUS_NETWORK_MASK);
    // If packet seems not for the current network then find first network
    // it's for.  If none, then deem it belongs to the current network.
    //TODO: Should we assert in this case??
    //TODO: Should we unroll this loop for efficiency?
    if ((pktNetMask & BIT(phy2NetworkParamsSlotForIndex[netIndex])) == 0) {
      uint8_t index;
      for (index = 0; index < PRO2_MAX_NETWORKS; index++) {
        if (pktNetMask & BIT(phy2NetworkParamsSlotForIndex[index])) {
          netIndex = index;
          break;
        }
      }
    }
  }
 #endif//(PRO2_MAX_NETWORKS > 2)
  appendedInfo[APPENDED_INFO_NETWORK_OVERWRITE_BYTE_INDEX]
    = netIndex | (PHY_THIS_ID << APPENDED_INFO_NETWORK_OVERWRITE_PHY_ID_SHIFT);
  // Need to convert the PRO2+'s appended info phyChan to macPgChan
  // Since it could take some time to do a proper phy-to-macPgChan conversion,
  // instead we use the phy2MacPageChan[] cached value per network index.
  appendedInfo[APPENDED_INFO_CHANNEL_OVERWRITE_BYTE_INDEX]
    = (uint8_t) phy2MacPageChan[netIndex];

  // em3xx would process expected 802.15.4 ACKs here.
  // PRO2+ gives us this indication better as a TX event so
  // we'll defer such processing to processTxEvents().
  // Below we'll handle censoring any such ACKs to the upper layers.

  // This is a valid packet:
  // 1. It does not have any drop conditions that we care about.
  // 2. If we care, CRC has passed.
  // 3. It is not too long.
  // Simple expected ACKs are not passed up.  "Simple" means:
  //   SOURCE_MODE      = 0 (NONE)
  //   FRAME_VERSION    = any
  //   DESTINATION_MODE = 0 (NONE)
  //   IE_LIST_PRESENT  = 0
  //   SEQ_SUPPRESSION  = 0
  //   RESERVED         = 0
  //   INTRA_PAN        = any [per 802.15.4e]
  //   ACK requested    = 0
  //   FRAME_PENDING    = any
  //   SECURITY_ENABLED = 0
  //   FRAME_TYPE       = ACK

  //N.B. Can't use emMacAckExpected here because it might have been cleared
  //     during TX processing above.  PRO2+ or above code assures that
  //     TX_ACK_RECEIVED_x will be posted simultaneously with RX_COMPLETE
  //     for an expected ACK.
  if ((events->txPend & (CMD_IE154_GET_INT_STATUS_REP_IE154_TX_PEND_TX_ACK_RECEIVED_W_FP_PEND_MASK
                         | CMD_IE154_GET_INT_STATUS_REP_IE154_TX_PEND_TX_ACK_RECEIVED_NO_FP_PEND_MASK))
      && ((HIGH_LOW_TO_INT(payload[1], payload[0])
           & ~(MAC_FRAME_VERSION_MASK | MAC_FRAME_FLAG_FRAME_PENDING | MAC_FRAME_FLAG_INTRA_PAN))
          == MAC_FRAME_TYPE_ACK)) {
    // An expected ACK -- don't pass up
  } else {
    uint8_t* phr;
    // If arriving frame had 802.15.4g 2-byte PHR, convert that to
    // 15.4-2003/2006/2011 1-byte PHR for the upper layers, unless
    // the frame's length is such that it requires we pass up the
    // 2-byte PHR.  When we do pass up 2-byte PHR, let upper layers
    // know this by setting the MSB 'MS' 15.4g feature bit, which
    // isn't supported, as a sentinal.  This way upper layer can
    // check what it assumes is the sole length byte, find it's
    // >= 128 which is illegal for 1-byte PHR, and hence know it
    // must really be the MSB of a 2-byte PHR to process.
    if (IE154G_ACTIVE() && (payLen > 127)) {
      phr = &payload[-2]; // Pass up 15.4g PHR
      phr[0] |= 0x80; // Sentinal to upper layer this is 2-byte PHR
    } else {
      phr = &payload[-1]; // Pass up 15.4-2003/2006/2011 1-byte PHR
    }
    sli_zigbee_packet_received_internal_isr_callback(phr, (payload - phr)
                                                     + payLen
                                                     + ((payLen & 1) ? 0 : 1)
                                                     + NUM_APPENDED_INFO_BYTES,
                                                     framePendingSetInOutgoingAck);
    // Reset flag for next packet
    framePendingSetInOutgoingAck = !!(phy2RxFlags & PROP_IE154_RX_FLAGS_FRAME_PENDING_DEFAULT_VALUE_MASK);
  }
  resetForNextFrame(frameBufLen);
  return false;
}

static bool processRxEvents(Events* events)
{
  // In case latency causes several events to appear simultaneously,
  // process them more or less in their expected time order, with
  // errors last, because they likely pertain to events occurring after
  // the good events.
  // First, however, we suck in as much of the RX FIFO data we can
  PHY_TRACE_RAW('F', rxBuffer, rxBufLen);
  uint8_t rxAvail = events->rxAvail;
  if ((rxBufLen + rxAvail) > RX_BUFFER_FIFO_SPACE) {
    rxAvail = (RX_BUFFER_FIFO_SPACE - rxBufLen);
  }
  if (rxAvail > 0) {
    PHY_ATTEST_SUCCESS(siPro2ReadRxFifo(rxAvail, &rxBuffer[rxBufLen]));
    PHY_TRACE_RAW('P', &rxBuffer[rxBufLen], rxAvail);
    rxBufLen += rxAvail;
  }
  if (events->rxPend & CMD_IE154_GET_INT_STATUS_REP_IE154_RX_PEND_RX_FIFO_ALMOST_FULL_PEND_MASK) {
    // Handle RX_FIFO_ALMOST_FULL
    // Since we just sucked in all the RX FIFO data available above,
    // nothing further needs to be done for this event.
  }
  if (events->rxPend & CMD_IE154_GET_INT_STATUS_REP_IE154_RX_PEND_RX_SFD_PEND_MASK) {
    // Handle RX_SFD
    emRadioSfdRecvIsrCallback();
  }
  // RAIL_LIB-4848 - Ignore RX_SRC_ADDR and RX_COMPLETE if an RX_ABORTED/OVERFLOW likely happened too.
  if ((events->rxPend & CMD_IE154_GET_INT_STATUS_REP_IE154_RX_PEND_RX_ERROR_PEND_MASK)
      && (events->rxError != CMD_IE154_GET_INT_STATUS_REP_IE154_RX_ERR_STATUS_RX_ACK_TX_FAILED_MASK)) {
    // One of the bad RX_ERRORs happened, don't trust RX FIFO
    // but defer processing this error to a bit later.
    processedExtraRxComplete = false;
  } else {
    if (events->rxPend & CMD_IE154_GET_INT_STATUS_REP_IE154_RX_PEND_RX_SRC_ADDR_PEND_MASK) {
      // Handle RX_SRC_ADDR
      (void) processRxSrcAddr(events);
      // Ignore malformed-frame errors returned by this event;
      // defer to RX_COMPLETE / RX_ERROR handling.
    }
    // Attempt to deal with RAIL_LIB-1565 situation where, due to interrupt
    // latency, there are *multiple* complete packets in the PRO2+'s RX FIFO.
    // Without these changes we'd send the oldest one up to the MAC and hold
    // onto the remaining one(s) until another packet arrives, at which point
    // we'll send up the oldest one -- forcing the MAC to lag behind.
    // The PRO2+ unfortunately doesn't provide a count of packets in the RX
    // FIFO, only this RX_COMPLETE event that tells us there's at least one.
    // We jump through the hoops below to support back-to-back receives and
    // deal with a potential race of us seeing a subsequent complete packet
    // in the RX FIFO before the PRO2+ has signalled its presence to us by
    // pending RX_COMPLETE.
    // Note in this scenario, there might be other RX events that happened
    // more than once for each of the packets in the RX FIFO -- but we can
    // only process those events for the first packet and hope for the best.
    // Certainly there are pathological cases where this will cause minor
    // problems -- like two data polls from different devices pending the
    // SRCADDR interrupt twice -- our processing it above was for the first
    // poller and likely would be affecting the ACK going out to the second
    // poller.  Hopefully this doesn't happen too frequently.
    if (events->rxPend & CMD_IE154_GET_INT_STATUS_REP_IE154_RX_PEND_RX_COMPLETE_PEND_MASK) {
      // Handle RX_COMPLETE event(s)
      uint8_t packetsProcessed = 0;
      bool ignorePartial = processedExtraRxComplete;
      while (true) { // Loop trying to process complete packets in the RX FIFO...
        if (processRxComplete(events, ignorePartial)) {
          // Malformed or partial packet
          if (ignorePartial) { // Not expecting any, and there weren't any
            break;
          }
          // Here we were expecting something, but didn't get it, a surprise.
          processedExtraRxComplete = false;
          return true;
        }
        // Here a packet was sent up to the MAC -- check for possibly more
        // by iterating and allowing partials to not be treated as an error.
        ignorePartial = true;
        packetsProcessed++;
      }
      processedExtraRxComplete = (packetsProcessed > 1);
    } else {
      processedExtraRxComplete = false;
    }
  }
  if (events->rxPend & CMD_IE154_GET_INT_STATUS_REP_IE154_RX_PEND_RX_ACK_TX_COMPLETE_PEND_MASK) {
    // Handle RX_ACK_TX_COMPLETE
    // Since something went out, update lastTxFinishedTimeMs
    lastTxFinishedTimeMs = halCommonGetInt32uMillisecondTick();
    // ACKs don't update lastCsmaTxFinishedTimeMs
    sli_802154phy_radio_tx_ack_isr_callback();
    // Account for this ACK in DC plugin
    emPhyDcAckSent();
  }
  if (events->rxPend & CMD_IE154_GET_INT_STATUS_REP_IE154_RX_PEND_RX_ERROR_PEND_MASK) {
    // Handle RX_ERROR set -- refined by rxError field
    if (events->rxError == 0) { // James Dean in "RX_ERROR Without a Cause"
      // While this theoretically shouldn't happen, a PRO2+ FW bug can
      // clear rxError on SYNC-detect of a new packet, rather than when
      // this RX_ERROR event is acknowledged.  Since an RX_OVERFLOW would
      // cause the PRO2+ to automatically idle itself, we know that can't
      // be the lost rxError, but we don't know which of the other two it
      // might have been, so we conservatively assume both occurred.
      // PHY_ASSERT(false); // Catch FW or Emul bug
      events->rxError = (CMD_IE154_GET_INT_STATUS_REP_IE154_RX_ERR_STATUS_RX_ACK_TX_FAILED_MASK
                         | CMD_IE154_GET_INT_STATUS_REP_IE154_RX_ERR_STATUS_RX_ABORTED_MASK);
    }
    // First the expected "good" ones:
    if (events->rxError & CMD_IE154_GET_INT_STATUS_REP_IE154_RX_ERR_STATUS_RX_ACK_TX_FAILED_MASK) {
      // Handle RX_ACK_TX_FAILED 'error'
      // Ignore this -- if our ACK failed to go out then treat this the same
      // as if it did go out but peer failed to hear it.  FIFOs are okay so
      // we should be safe to just continue along.
      // Since some portion of ACK may have gone out, update lastTxFinishedTimeMs
      lastTxFinishedTimeMs = halCommonGetInt32uMillisecondTick();
      // ACKs don't update lastCsmaTxFinishedTimeMs
    }
    if (events->rxError & CMD_IE154_GET_INT_STATUS_REP_IE154_RX_ERR_STATUS_RX_OVERFLOW_MASK) {
      // Handle RX_OVERFLOW error
      PHY_DEBUG(pro2Listening = false); // Radio implicitly idles itself with this error
      pro2RestartRadio(NO_TX_ERRORS);
      return true; // No sense continuing with any other Rx events
    }
    if (events->rxError & CMD_IE154_GET_INT_STATUS_REP_IE154_RX_ERR_STATUS_RX_ABORTED_MASK) {
      // Handle RX_ABORTED error
      // This error is host-induced via START_IDLE, START_TX (non-CSMA), or
      // START_RX while a frame was coming in and host-visible (passed
      // filtering) -- otherwise the radio will invisibly ignore it.
      // We don't permit a START_RX except from IDLE, so the appropriate
      // thing to do in the other cases is just to reset the RX FIFO.
      pro2RxTxFifoInfo(true, false, NULL, NULL);
      return true; // No sense continuing with any other Rx events
    }
  }
  return false;
}

static void pro2Isr(void)
{
  // Find out what happened and act accordingly
  // Leave pending any events we've masked off
  Events events = {
    0, 0,
    0, phy2TxEvents,
    0, phy2RxEvents,
    0, phy2ChEvents,
    0, 0,
    0, 0   // TX_FIFO_SPACE, RX_FIFO_COUNT
  };
  pro2GetAckEvents(&events);
  PHY_TRACE_RAW('I', (uint8_t*)&events, sizeof(Events));
  events.rxPend &= phy2RxEvents; // Ignore masked events
  events.txPend &= phy2TxEvents; // Ignore masked events
  events.chPend &= phy2ChEvents; // Ignore masked events

  PHY_ASSERT((events.intPend != 0));   // Why are we here??!!
  PHY_ASSERT(((events.intPend & CMD_IE154_GET_INT_STATUS_REP_IE154_INT_PEND_CHIP_INT_PEND_MASK) == 0)
             || (events.chPend != 0));  // Bogus Chip event
  PHY_ASSERT(((events.intPend & CMD_IE154_GET_INT_STATUS_REP_IE154_INT_PEND_RX_MODE_INT_PEND_MASK) == 0)
             || (events.rxPend != 0));  // Bogus Rx event
  PHY_ASSERT(((events.intPend & CMD_IE154_GET_INT_STATUS_REP_IE154_INT_PEND_TX_MODE_INT_PEND_MASK) == 0)
             || (events.txPend != 0));  // Bogus Tx event

  // Events are processed more or less in time and severity order,
  // in case latency causes several to appear simultaneously.
  // Here we do Chip then Tx then Rx.
  // Depending on severity, some might short-circuit the need to
  // consider later ones.
  if (events.chPend) {
    if (processChEvents(&events)) {
      return;
    }
  }
  if (events.txPend) {
    (void) processTxEvents(&events);
  }
  if (events.rxPend) {
    (void) processRxEvents(&events);
  }
}

static uint16_t pro2CurrentMode(void)
{
  struct reply_IE154_QUERY_MODE_map rspBuf;
  uint8_t cmd = CMD_IE154_QUERY_MODE;
  PHY_ATTEST_SUCCESS(siPro2SendCommandGetResponse(sizeof(cmd), &cmd,
                                                  sizeof(rspBuf), (uint8_t*) &rspBuf));
  return rspBuf.OPERATING_MODE_PHASE;
}

//== Bootup & Clocking ========================================================

void pro2SetRadioBootMode(uint8_t bootOptions)
{
  pro2RadioBootMode = bootOptions;
}

uint8_t pro2GetRadioBootMode(void)
{
  return pro2RadioBootMode;
}

static uint8_t pro2PowerLevel(int8_t* dBmPower)
{
  uint8_t i;
  PHY_ASSERT(phy2TxPA != NULL);
  for (i = phy2TxPA->numPowerLevels - 1;
       (i > 0) && (phy2TxPA->powerLevels[i].dBm > *dBmPower);
       i--) {
  }
  *dBmPower = phy2TxPA->powerLevels[i].dBm;
  return phy2TxPA->powerLevels[i].regValue;
}

static bool pro2IsIe154Part(uint16_t part)
{
  switch (part) {
    case 0x4467: // PRO2+ IE154  low-power PA
    case 0x4468: // PRO2+ IE154 high-power PA
    case 0x4469: // PRO2+ IE154 dual-power PA
      return true;
      break;
    default:
      break;
  }
  return false;
}

static void pro2PowerUp(void)
{
  if (pro2PoweredUp) {
    return; // Already up
  }
  // The GPIOs are set up now -- we can deassert SDN and try to power up
  // the PRO2.
  PHY_ATTEST_SUCCESS(siPro2Init(&pro2Isr));
  // PRO2+ is powered UP but with its interrupt Disabled at this point
  PHY_ASSERT(radioCurrentPowerStatus == SL_ZIGBEE_RADIO_POWER_MODE_OFF);

  switch (pro2RadioBootMode & CMD_POWER_UP_ARG_BOOT_OPTIONS_FUNC_MASK) {
    case CMD_POWER_UP_ARG_BOOT_OPTIONS_FUNC_ENUM_IE154:
      pro2GetInfo(false); // Preliminary check on OTP and get DIE_INFO
      if (pro2RadioBootMode & CMD_POWER_UP_ARG_BOOT_OPTIONS_PATCH_MASK) {
        switch (pro2GetRomId()) {
          case 6:
            if (pro2GetOtpId() <= OTPID_ROM6_ie154) {
              pro2SetRadioRamPatch(radioRamPatchImage446xRom6Ie154);
            } else {
              // OTP already has this patch so cancel specifying one
              pro2RadioBootMode &= ~CMD_POWER_UP_ARG_BOOT_OPTIONS_PATCH_MASK;
            }
            break;
          default:
            PHY_ASSERT(false); // Unrecognized/unsupported ROM for IE154 mode
            break;
        }
      }
      break;
    default:
      PHY_ASSERT(false); // Unsupported PRO2+ boot mode
      break;
  }
  pro2PowerUpCommand(pro2RadioBootMode);
  pro2GetInfo(true);
  // Assert if device not kosher (not enforced for test apps)
  PHY_ASSERT(pro2IsIe154Part(pro2GetPartNum()));
  PHY_ASSERT(pro2GetFunc() == (pro2RadioBootMode & CMD_POWER_UP_ARG_BOOT_OPTIONS_FUNC_MASK));
  PHY_ASSERT(pro2GetRomId() >= 6);
  PHY_ASSERT(pro2GetFwSvn() != 0);

  // Apply board header's power-up configuration
  pro2SendRadioCommands((const uint8_t*) pro2GpioCfgPowerUp);

  // Reinit static state representing PRO2+'s default power-up configuration
  phy2BandConfig = NULL;
  phy2SubBandConfig = NULL;
  phy2SubBandParams = NULL;
  phy2Channel = INVALID_CHANNEL;
  { uint8_t slot;
    Pro2NetworkParams defaultNetworkParams = { PRO2_NETWORK_PARAM_DEFAULTS };
    for (slot = 0; slot < PRO2_MAX_NETWORKS; slot++) {
      memcpy((void*)&phy2NetworkParams[slot], &defaultNetworkParams,
             sizeof(defaultNetworkParams));
      phy2NetworkParamsSlotForIndex[slot] = slot;
      phy2NetworkParamsIndexForSlot[slot] = slot;
    }
  }
  pro2IsCoord  = PROP_IE154_NETWORK_COORDINATOR_MASK_DEFAULT;
  pro2RxFlags  = PROP_IE154_RX_FLAGS_DEFAULT;
  pro2TxFlags  = PROP_IE154_TX_FLAGS_DEFAULT;
  pro2CoFlags  = PROP_IE154_COMMON_FLAGS_DEFAULT;
  pro2CoPhCfg  = PROP_IE154_COMMON_PHY_CFG_DEFAULT;
  pro2L1Events = PROP_IE154_INT_CTL_ENABLE_DEFAULT;
  pro2RxEvents = PROP_IE154_INT_CTL_RX_ENABLE_DEFAULT;
  pro2TxEvents = PROP_IE154_INT_CTL_TX_ENABLE_DEFAULT;
  pro2ChEvents = PROP_IE154_INT_CTL_IE154_CHIP_ENABLE_DEFAULT;
  pro2PtiLog   = (PROP_PTI_LOG_EN_DEFAULT
                  | (PROP_PTI_LOG_EN_2_DEFAULT << 8)
                  );
  pro2PtiGpio  = PRO2_GPIO_NONE; // uninited
  pro2CcaThreshold = PROP_IE154_TX_CCA_THRESHOLD_DEFAULT;

  // Reinit with values to force sli_802154phy_radio_init() to fix all properties
  // in next emRadioTransmit().
  pro2TxCsmaConfig.useZeroBackoff = false;
  pro2TxCsmaConfig.ccaAttemptMax = 0;
  pro2TxCsmaConfig.backoffExponentMin = 0;
  pro2TxCsmaConfig.backoffExponentMax = 0;
  pro2TxCsmaConfig.minimumBackoff = 0;
  pro2TxCsmaBackoffAdjust = 0;

 #if     SI4440_1087_TOLERATE
  // For C0/C1 PRO2+ devices for which PRO2+ is not supplying their
  // system clock, defer fixing GPIO0 operation to pro2WakeUp() time.
 #endif//SI4440_1087_TOLERATE
  pro2PoweredUp = true;
}

//== emPhy API ================================================================

uint32_t emPhySymbolsToUs(uint32_t symbols)
{
  return SL_ZIGBEE_PHY_SYMBOLS_TO_US(symbols);
}

void emPhyTick(bool isrContext)
{
  // Called by sl_zigbee_tick() as well as MAC_TIMER ISR on wraps (so at least
  // every second, possibly longer if system clock is running slower than
  // 24 MHz).
  if (!isrContext) {
    emPhyDcTick();
    MAC_INTERRUPTS_OFF(); // Protect against re-entrancy
  }
  int32_t myLastTxStartedTimeMs = lastTxStartedTimeMs; // non-volatile copy
  if ((myLastTxStartedTimeMs >= 0)
      && (elapsedTimeInt16u((uint16_t)myLastTxStartedTimeMs,
                            halCommonGetInt16uMillisecondTick())
          > MAC_TX_TIMEOUT_MS)) {
    PHY_ASSERT(emPhyTxState > PHY_TX_STATE_OFF);
    PHY_ASSERT(false); // TODO: Remove when PRO2+ FW stable
    pro2CleanupTxState(ANY_TX_ERROR);
  }
  if (!isrContext) {
    MAC_INTERRUPTS_ON();
  }
}

bool emPhyIsTransmitting(void)
{
  // Radio has a transmit in progress, or is waiting for that transmit's ACK
  // Currently done the quick way without actually checking with radio.
  return (emPhyTxState > PHY_TX_STATE_OFF);
}

void emPhyCancelTransmit(void)
{
  sli_802154phy_radio_sleep();
  //TODO: Need any delays here?
  sli_802154phy_radio_wake_up();
}

bool emPhyDeviceIsCoordinator(void)
{
  return !!(phy2IsCoord & BIT(emPhyCurrentRadioNetworkIndex()));
}

uint8_t sli_802154phy_get_radio_channel(void)  // AKA sli_802154phy_get_phy_radio_channel()
{
  // Return the macPgChan representation encoding page and channel
  // We use the cached phy2MacPageChan[] for this.
  return (uint8_t) phy2MacPageChan[emPhyCurrentRadioNetworkIndex()];
}

int8_t emPhyGetRadioPower(void)    // AKA sli_802154phy_get_phy_radio_power()
{
  return phy2TxPowerGranted[emPhyCurrentRadioNetworkIndex()];
}

#if     MAC_HAS_CHANNEL_PAGES

uint8_t sli_802154phy_get_channel_page_in_use(void)
{
  return (uint8_t) (phy2MacPageChan[emPhyCurrentRadioNetworkIndex()] >> CHANNEL_BITS);
}

// Retain the least-significant bits from Page
#define macPage2Pg(macPage)      ((macPage) & (BIT(8 - CHANNEL_BITS) - 1))
// For pages other than 0, set the most-significant bits dropped in Pg encoding
// E.g. 3-bit Pgs 1-7 map to 5-bit Pages 24-31.
#define macPg2Page(macPg)        (((macPg) == 0) ? 0                                         \
                                  : ((macPg) | ((((1ul << (32 - MAX_CHANNELS_PER_PAGE)) - 1) \
                                                 >> (8 - CHANNEL_BITS)) << (8 - CHANNEL_BITS))))

uint8_t sli_802154phy_get_channel_page_for_channel(uint8_t macPgChan)
{
  // Convert internal 3-bit abbreviated Pg to full 5-bit Page
  // Here treat macPgChan verbatim -- needs no remapping
  return macPg2Page(sli_802154mac_pg_chan_pg(macPgChan));
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

uint8_t sli_802154phy_get_phy_channel(uint8_t macPgChan)
{
  uint8_t phyChan;
  EmPhyBandConfig* bandConfig = mapMacPgChanToPhyChanBand(macPgChan, &phyChan,
                                                          NULL, NULL);
  if (bandConfig == NULL) {
    return INVALID_CHANNEL;
  }
  return phyChan;
}

// Default to first MAC channel on first band's macMap in subBandConfigs{}
// Note: If this channel is NOT valid
// sli_802154phy_radio_init() will likely assert.  Fix the radio configs so this
// choice *is* valid, or app must explicitly set channel to a valid
// one to avoid this poor choice of default channel.
#define selectDefaultChannel()                                                       \
  (sli_802154mac_pg_chan(emPhyConfigMHz.bandConfigs[0]->subBandConfigs->macChanPage, \
                         emPhyConfigMHz.bandConfigs[0]->subBandConfigs->macChanOffset))

#else//!MAC_HAS_CHANNEL_PAGES

// Default to first channel on first band
#define selectDefaultChannel() (emPhyConfigMHz.bandConfigs[0]->minPhyChan)

static EmPhyBandConfig* mapMacPgChanToPhyChanBand(uint8_t macPgChan,
                                                  uint8_t* phyChanPtr,
                                                  uint8_t* macPagePtr,
                                                  EmPhySubBandConfig** subBandConfigPtr)
{
  // Here we assume macPgChan has already been mapped
  if (phyChanPtr != NULL) {
    *phyChanPtr = macPgChan;
  }
  if (macPagePtr != NULL) {
    *macPagePtr = 0;
  }
  EmPhyBandConfig* bandConfig = mapChannelToBand(macPgChan);
  if (subBandConfigPtr != NULL) {
    *subBandConfigPtr = subBandConfig(macPgChan, bandConfig);
  }
  return bandConfig;
}

uint8_t sli_802154phy_get_phy_channel(uint8_t macPgChan)
{
  return (mapChannelToBand(macPgChan) ? macPgChan : INVALID_CHANNEL);
}

#endif//MAC_HAS_CHANNEL_PAGES

uint32_t emPhyGetChannelFreqHz(uint8_t macPgChan)
{
  uint8_t phyChan;
  EmPhyBandConfig* bandConfig = mapMacPgChanToPhyChanBand(macPgChan, &phyChan,
                                                          NULL, NULL);
  if (bandConfig == NULL) {
    return PHY_INVALID_FREQ_HZ; // Sentinal for illegal channel
  }
  return (bandConfig->baseFreqHz + (bandConfig->chanFreqHz * phyChan));
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
  PHY_ASSERT(dutyCycleParams != NULL);
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

uint32_t emPhyGetLastTxCompleteTimeMs(bool lastCsmaTx)
{
  return (lastCsmaTx ? lastCsmaTxFinishedTimeMs : lastTxFinishedTimeMs);
}

void sli_802154phy_set_coordinatorOnNetwork(bool coordinator,
                                            EmberNetworkIndex networkIndex)
{
  PHY_ASSERT(networkIndex < PRO2_MAX_NETWORKS);
  int8_t slot = phy2NetworkParamsSlotForIndex[networkIndex];
  phy2IsCoord = ((phy2IsCoord & ~BIT(slot))
                 | (coordinator ?  BIT(slot) : 0));
  if (phy2IsCoord != pro2IsCoord) {
    pro2WriteOneProperty(PROP_GRP_ID_IE154_NETWORK,
                         PROP_IE154_NETWORK_COORDINATOR_MASK_OFFSET,
                         phy2IsCoord);
    pro2IsCoord = phy2IsCoord;
  }
}

void sli_802154phy_set_coordinator(bool coordinator)
{
  sli_802154phy_set_coordinatorOnNetwork(coordinator, emPhyCurrentRadioNetworkIndex());
 #ifdef  emPhyMaxPowerNetworkIndex
  sli_802154phy_set_coordinatorOnNetwork(coordinator, emPhyMaxPowerNetworkIndex());
 #endif//emPhyMaxPowerNetworkIndex
}

void sli_802154phy_set_eui64(void)
{
  uint8_t * eui64 = sli_zigbee_stack_get_eui64();
  uint8_t cmdBuf[] = {
    /*0-Command */ CMD_SET_PROPERTY,
    /*1-Group   */ PROP_GRP_ID_IE154_RX,
    /*2-Num     */ sizeof(sl_802154_long_addr_t),
    /*3-Property*/ PROP_IE154_RX_EUI_64_OFFSET,
    /*4-Eui64_m */ eui64[7],
    /*5-Eui64   */ eui64[6],
    /*6-Eui64   */ eui64[5],
    /*7-Eui64   */ eui64[4],
    /*8-Eui64   */ eui64[3],
    /*9-Eui64   */ eui64[2],
    /*A-Eui64   */ eui64[1],
    /*B-Eui64_l */ eui64[0],
  };
  pro2PowerUp(); // Ensure PRO2+ is up
  // sli_802154mac_local_eui64[] is stored in LSB-to-MSB order, same as seen over-the-air,
  // but PRO2+ API is big-endian order for properties, so need to reverse.
  PHY_ATTEST_SUCCESS(siPro2SendCommandNoResponse(sizeof(cmdBuf), cmdBuf));
}

sl_status_t emPhySetPhyMode(EmPhyMode phyMode)
{
  // Drastic mode changes like these can only occur when radio is idle
  if (phy2Listening || (emPhyTxState != PHY_TX_STATE_OFF)) {
    return SL_STATUS_TRANSMIT_BUSY;
  }

  // Map FILTER    in bits 7-6
  pro2UpdateCoFlags(PROP_IE154_COMMON_FLAGS_FRAME_VERSION_FILTER_MASK,
                    (phyMode >> 6)
                    << PROP_IE154_COMMON_FLAGS_FRAME_VERSION_FILTER_LSB);
  // Map FPdefault in bit  5
  pro2UpdateRxFlags(PROP_IE154_RX_FLAGS_FRAME_PENDING_DEFAULT_VALUE_MASK,
                    (phyMode >> 5)
                    << PROP_IE154_RX_FLAGS_FRAME_PENDING_DEFAULT_VALUE_LSB);
  framePendingSetInOutgoingAck = !!(phy2RxFlags & PROP_IE154_RX_FLAGS_FRAME_PENDING_DEFAULT_VALUE_MASK);
  //TODO: Map MR-encode in bit  4
  //TODO: Map MR-SFD    in bit  3
  // Map FSK-Mode  in bits 2-1
  // Map PHRformat in bit  0
  pro2UpdateCoPhCfg((PROP_IE154_COMMON_PHY_CFG_PHY_VER_MASK
                     | PROP_IE154_COMMON_PHY_CFG_CODING_MASK),
                    ((((phyMode >> 0) & 1)
                      << PROP_IE154_COMMON_PHY_CFG_PHY_VER_LSB)
                     | (((phyMode >> 2) & 1)
                        << PROP_IE154_COMMON_PHY_CFG_CODING_LSB)));
  return SL_STATUS_OK;
}

sl_status_t emPhySetRadioChannelOnNetwork(uint8_t macPgChan,
                                          EmberNetworkIndex networkIndex)
{
  PHY_ASSERT(networkIndex < PRO2_MAX_NETWORKS);

  // Quickly reject an illegal channel#
  if (sli_802154mac_pg_chan_ch(macPgChan) >= MAX_CHANNELS_PER_PAGE) {
    return SL_STATUS_TRANSMIT_INVALID_CHANNEL;
  }

  // Try to map macPgChan to phyChan and its band
  uint8_t phyChan, macPage;
  EmPhyBandConfig* bandConfig = mapMacPgChanToPhyChanBand(macPgChan, &phyChan,
                                                          &macPage, NULL);
  if (bandConfig == NULL) {
    return SL_STATUS_TRANSMIT_INVALID_CHANNEL;
  }

  pro2PowerUp(); // Ensure PRO2+ is up

  int8_t slot = phy2NetworkParamsSlotForIndex[networkIndex];
  if (phy2NetworkParams[slot].phyChan != phyChan) {
    phy2NetworkParams[slot].phyChan = phyChan;
    pro2WriteOneProperty(PROP_GRP_ID_IE154_NETWORK,
                         PROP_IE154_NETWORK_LIST_OFFSET
                         + (sizeof(Pro2NetworkParams) * slot)
                         + offsetof(Pro2NetworkParams, phyChan),
                         phyChan);
    // Must impose new channel's power limits
    emPhySetRadioPowerOnNetwork(phy2TxPowerRequested[networkIndex], networkIndex);
    // Cache macPgChan into phy2MacPageChan[] for use in
    // Rx appended info, etc., but with full macPage information.
    phy2MacPageChan[networkIndex] = (((uint16_t) macPage) << CHANNEL_BITS)
                                    | sli_802154mac_pg_chan_ch(macPgChan);
  }

  // Activate this channel, if it is for the current network
  if ((networkIndex == emPhyCurrentRadioNetworkIndex())
      && ((phy2Channel != phyChan) || pro2ForceCalibration)) {
    phy2Channel = phyChan;
    // If PHY was listening, then we need to switch it to the new channel
    pro2RestartRadio(ANY_TX_ERROR);
  }
  return SL_STATUS_OK;
}

sl_status_t emPhySetRadioChannel(uint8_t macPgChan) // AKA sli_802154phy_set_phy_radio_channel()
{
  sl_status_t status;
  if (macPgChan == INVALID_CHANNEL) {
    macPgChan = selectDefaultChannel();
  }
  status = emPhySetRadioChannelOnNetwork(macPgChan,
                                         emPhyCurrentRadioNetworkIndex());
 #ifdef  emPhyMaxPowerNetworkIndex
  if (status == SL_STATUS_OK) {
    // On channel change, reset Max Power to current radio network setting
    phy2TxPowerRequested[emPhyMaxPowerNetworkIndex()] =
      phy2TxPowerRequested[emPhyCurrentRadioNetworkIndex()];
    status = emPhySetRadioChannelOnNetwork(macPgChan, emPhyMaxPowerNetworkIndex());
  }
 #endif//emPhyMaxPowerNetworkIndex
  return status;
}

sl_status_t emPhySetRadioPowerOnNetwork(int8_t dBmPower,
                                        EmberNetworkIndex networkIndex)
{
  PHY_ASSERT(networkIndex < PRO2_MAX_NETWORKS);
  // First check against overall Radio limit
  if ((dBmPower < MIN_RADIO_POWER)
      || (dBmPower > MAX_RADIO_POWER)) {
    return SL_STATUS_TRANSMIT_INVALID_POWER;
  }

  phy2TxPowerRequested[networkIndex] = dBmPower;  // Record what was requested

  int8_t slot = phy2NetworkParamsSlotForIndex[networkIndex];
  // Next enforce channel's power limits per band configurations.
  // Here we don't return an error, but peg setting to the limit.
  EmPhySubBandConfig* powerLimit = subBandConfig(phy2NetworkParams[slot].phyChan, NULL);
  if ((powerLimit == NULL)
      || ((emRadioTxPowerMode & SL_ZIGBEE_TX_POWER_MODE_BAND_NO_LIMITS_MASK)
          == SL_ZIGBEE_TX_POWER_MODE_BAND_NO_LIMITS_VALUE)
      || (sli_legacy_mfglib_mode)) {
    // No further power limits apply to this channel
  } else
  if (dBmPower < powerLimit->txMinPowerdBm) {
    dBmPower = powerLimit->txMinPowerdBm;
  } else
  if (dBmPower > powerLimit->txMaxPowerdBm) {
    dBmPower = powerLimit->txMaxPowerdBm;
  }

  // The radio may not provide access to all integer power levels.
  // In particular, there are gaps at lower power levels.
  // pro2PowerLevel() will update dBmPower to reflect the chosen level.
  uint8_t pro2Power = pro2PowerLevel(&dBmPower);

  pro2PowerUp(); // Ensure PRO2+ is up

  // In the event an incoming frame passes filtering for multiple networks,
  // the PRO2+ will choose to ACK it using the power from the lowest-numbered
  // network.  To avoid problems with networks on the same channel but with
  // different power levels, ensure the networks are sorted on the PRO2+ in
  // order of highest power to lowest; in ambiguous situations like this we
  // want the ACK to go out at the higher power level.
  if (phy2NetworkParams[slot].power != pro2Power) {
    int8_t newSlot = slot;
    Pro2NetworkParams newParams = phy2NetworkParams[slot]; // Struct copy
    newParams.power = pro2Power;
    if (pro2Power > phy2NetworkParams[slot].power) {
      // Power higher, may need to move this entry up higher
      while (((newSlot - 1) >= 0)
             && (pro2Power > phy2NetworkParams[newSlot - 1].power)) {
        phy2NetworkParams[newSlot] = phy2NetworkParams[newSlot - 1]; // Struct copy
        phy2NetworkParamsIndexForSlot[newSlot] = phy2NetworkParamsIndexForSlot[newSlot - 1];
        phy2NetworkParamsSlotForIndex[phy2NetworkParamsIndexForSlot[newSlot]] = newSlot;
        pro2WriteNProperties(PROP_GRP_ID_IE154_NETWORK,
                             PROP_IE154_NETWORK_LIST_OFFSET
                             + (sizeof(Pro2NetworkParams) * newSlot),
                             sizeof(Pro2NetworkParams),
                             (uint8_t*) &phy2NetworkParams[newSlot]);
        newSlot -= 1;
      }
    } else
    if (pro2Power < phy2NetworkParams[slot].power) {
      // Power lower, may need to move this entry down lower
      while (((newSlot + 1) < PRO2_MAX_NETWORKS)
             && (pro2Power < phy2NetworkParams[newSlot + 1].power)) {
        phy2NetworkParams[newSlot] = phy2NetworkParams[newSlot + 1]; // Struct copy
        phy2NetworkParamsIndexForSlot[newSlot] = phy2NetworkParamsIndexForSlot[newSlot + 1];
        phy2NetworkParamsSlotForIndex[phy2NetworkParamsIndexForSlot[newSlot]] = newSlot;
        pro2WriteNProperties(PROP_GRP_ID_IE154_NETWORK,
                             PROP_IE154_NETWORK_LIST_OFFSET
                             + (sizeof(Pro2NetworkParams) * newSlot),
                             sizeof(Pro2NetworkParams),
                             (uint8_t*) &phy2NetworkParams[newSlot]);
        newSlot += 1;
      }
    }
    if (newSlot == slot) {
      // Nothing needs to be shifted, so can just update the power property
      phy2NetworkParams[slot].power = pro2Power;
      pro2WriteOneProperty(PROP_GRP_ID_IE154_NETWORK,
                           PROP_IE154_NETWORK_LIST_OFFSET
                           + (sizeof(Pro2NetworkParams) * slot)
                           + offsetof(Pro2NetworkParams, power),
                           pro2Power);
    } else {
      phy2NetworkParams[newSlot] = newParams; // Struct copy
      phy2NetworkParamsIndexForSlot[newSlot] = networkIndex;
      phy2NetworkParamsSlotForIndex[networkIndex] = newSlot;
      pro2WriteNProperties(PROP_GRP_ID_IE154_NETWORK,
                           PROP_IE154_NETWORK_LIST_OFFSET
                           + (sizeof(Pro2NetworkParams) * newSlot),
                           sizeof(Pro2NetworkParams), (uint8_t*) &newParams);
    }
  }

  phy2TxPowerGranted[networkIndex] = dBmPower;  // Record end result granted

  // The new power will get activated on the next transmit

  return SL_STATUS_OK;
}

sl_status_t emPhySetRadioPower(int8_t dBmPower) // AKA sli_802154phy_set_phy_radio_power()
{
  if (dBmPower == INVALID_RADIO_POWER) {
    dBmPower = MAX_RADIO_POWER;
  }
 #ifdef  emPhyMaxPowerNetworkIndex
 #if     (INVALID_RADIO_POWER != -128)
  #error "INVALID_RADIO_POWER needs to be smallest possible int8_t value"
 #endif//(INVALID_RADIO_POWER != -128)
  if (dBmPower > MAX_RADIO_POWER) {
    // Special hack to set Max Power only by adding 100 to desired max power
    dBmPower -= 100;
    // But it still must be >= current radio network's power
    if (dBmPower < phy2TxPowerRequested[emPhyCurrentRadioNetworkIndex()]) {
      return SL_STATUS_TRANSMIT_INVALID_POWER;
    }
    return emPhySetRadioPowerOnNetwork(dBmPower, emPhyCurrentRadioNetworkIndex());
  }
  // If new network power is more than Max Power, increase Max Power accordingly
  if (dBmPower > phy2TxPowerRequested[emPhyMaxPowerNetworkIndex()]) {
    if (emPhySetRadioPowerOnNetwork(dBmPower, emPhyMaxPowerNetworkIndex())
        != SL_STATUS_OK) { // If this failed, so would below attempt
      return SL_STATUS_TRANSMIT_INVALID_POWER;
    }
  }
 #endif//emPhyMaxPowerNetworkIndex
  // Set power on current radio network
  return emPhySetRadioPowerOnNetwork(dBmPower, emPhyCurrentRadioNetworkIndex());
}

void emPhyStartTransmitStream(void) // AKA sli_802154phy_start_transmit_stream()
{
  pro2ToneOrStream(true, true);
}

void emPhyStartTransmitTone(void) // AKA sli_802154phy_start_transmit_tone()
{
  pro2ToneOrStream(true, false);
}

void emPhyStopTransmitStream(void) // AKA sli_802154phy_stop_transmit_stream()
{
  pro2ToneOrStream(false, true);
}

void emPhyStopTransmitTone(void) // AKA sli_802154phy_stop_transmit_tone()
{
  pro2ToneOrStream(false, false);
}

//== emRadio API ==============================================================

uint8_t emRadioAddressMatchingEnabled(void)
{
  uint8_t result = !!(phy2RxFlags & PROP_IE154_RX_FLAGS_FILTERING_MASK);
  // Hack: if filtering and RX_FILTERED for PTI is off, return a value > true
  if ( result && !(phy2RxFlags & PROP_IE154_RX_FLAGS_RX_FILTERED_MASK)) {
    result = 3; // Keep LSB 1
  }
  return (bool)result;
}

bool sli_802154phy_radio_auto_ack_enabled(void)
{
  return !!(phy2RxFlags & PROP_IE154_RX_FLAGS_AUTO_ACK_DEFAULT_VALUE_MASK);
}

bool sli_802154phy_radio_check_radio(void)
{
  // Nothing to do
  return false;
}

// sli_802154phy_radio_convert_rssi_to_ed() ported verbatim from em250class/phy-common.c
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

void emRadioEnableAddressMatching(uint8_t enable)
{
  // Hack: if enable is > true, allow filtered frames to be censored on PTI
  uint8_t setting;
  if (enable > true) { // filter and censor  filtered frames on PTI
    setting = PROP_IE154_RX_FLAGS_FILTERING_MASK;
  } else
  if (enable) {        // filter but include filtered frames on PTI
   #if     SI4440_1734_TOLERATE
    // Must always censor filtered frames on PTI due to firmware bug :-(
    setting = PROP_IE154_RX_FLAGS_FILTERING_MASK;
   #else//!SI4440_1734_TOLERATE
    setting = PROP_IE154_RX_FLAGS_FILTERING_MASK
              | PROP_IE154_RX_FLAGS_RX_FILTERED_MASK;
   #endif//SI4440_1734_TOLERATE
  } else {       // don't filter but include filtered frames on PTI
    setting = PROP_IE154_RX_FLAGS_RX_FILTERED_MASK;
  }
  pro2UpdateRxFlags(PROP_IE154_RX_FLAGS_FILTERING_MASK
                    | PROP_IE154_RX_FLAGS_RX_FILTERED_MASK, setting);
}

void sli_802154mac_radio_enable_auto_ack(bool enable)
{
  pro2UpdateRxFlags(PROP_IE154_RX_FLAGS_AUTO_ACK_DEFAULT_VALUE_MASK, (enable
                                                                      ? PROP_IE154_RX_FLAGS_AUTO_ACK_DEFAULT_VALUE_MASK
                                                                      : 0));
  // Should not need to bounce Radio mode to pick up this change.
}

void emRadioEnableOverflowNotification(bool enable)
{
  UNUSED_VAR(enable);
  // No-op -- PRO2+ always has overflow notification enabled
}

void emRadioEnablePacketTrace(bool enable)
{
  int8_t newPtiGpio;
  if ((enable && (phy2PtiGpio < PRO2_GPIO_NONE))        // We need to enable it
      || (!enable && (phy2PtiGpio > PRO2_GPIO_NONE))) {   // We need to disable it
    newPtiGpio = -phy2PtiGpio;
  } else {                                 // No change needed
    newPtiGpio = phy2PtiGpio;
  }
  if (newPtiGpio != pro2PtiGpio) {         // Chip setting needs to be changed
    if (newPtiGpio > PRO2_GPIO_NONE) {
      PHY_ATTEST_SUCCESS(pro2GpioConfig(newPtiGpio,
                                        (CMD_GPIO_PIN_CFG_ARG_GPIO_GPIO_MODE_ENUM_PKT_TRACE
                                         << CMD_GPIO_PIN_CFG_ARG_GPIO_GPIO_MODE_LSB)
                                        ));
      uint8_t ptiProps[] = {
        /*0-PTI_CTL*/ PROP_PTI_CTL_PTI_EN_MASK,
        /*1-PTI_BAUD[15:8]*/ 0x13, // 500,000 bps
        /*2-PTI_BAUD[7:0] */ 0x88, // (divided by 100)
        /*3-PTI_LOG_EN    */ LOW_BYTE(phy2PtiLog),
        /*4-PTI_LOG_EN_2  */ HIGH_BYTE(phy2PtiLog),
      };
      pro2WriteNProperties(PROP_GRP_ID_PTI,
                           PROP_PTI_CTL_OFFSET,
                           sizeof(ptiProps), ptiProps);
      phy2PtiGpio = pro2PtiGpio = newPtiGpio;
      pro2PtiLog = phy2PtiLog;
    } else {
      pro2WriteOneProperty(PROP_GRP_ID_PTI,
                           PROP_PTI_CTL_OFFSET,
                           PROP_PTI_CTL_PTI_EN_FALSE_BIT);
      phy2PtiGpio = pro2PtiGpio = newPtiGpio;
    }
  }
}

void emRadioEnableReceiveCompleteInt(bool enable)
{
  pro2UpdateEvents(phy2L1Events,
                   ((phy2RxEvents & ~PROP_IE154_INT_CTL_RX_ENABLE_RX_COMPLETE_MASK)
                    | (enable << PROP_IE154_INT_CTL_RX_ENABLE_RX_COMPLETE_LSB)),
                   phy2TxEvents,
                   phy2ChEvents);
}

void emRadioEnableReceiveCrc(bool enable)
{
  pro2UpdateCoPhCfg(PROP_IE154_COMMON_PHY_CFG_CHECK_CRC_MASK, (enable
                                                               ? PROP_IE154_COMMON_PHY_CFG_CHECK_CRC_MASK
                                                               : 0));
  // Should not need to bounce Radio mode to pick up this change.
}

void emRadioEnableSfdRecvNotification(bool enable)
{
  pro2UpdateEvents(phy2L1Events,
                   ((phy2RxEvents & ~PROP_IE154_INT_CTL_RX_ENABLE_RX_SFD_MASK)
                    | (enable << PROP_IE154_INT_CTL_RX_ENABLE_RX_SFD_LSB)),
                   phy2TxEvents,
                   phy2ChEvents);
}

void emRadioEnableSfdSentNotification(bool enable)
{
  pro2UpdateEvents(phy2L1Events, phy2RxEvents,
                   ((phy2TxEvents & ~PROP_IE154_INT_CTL_TX_ENABLE_TX_SFD_SENT_MASK)
                    | (enable << PROP_IE154_INT_CTL_TX_ENABLE_TX_SFD_SENT_LSB)),
                   phy2ChEvents);
}

#if     RAIL_LIB_4848_CCA_RX
static int8_t myRadioEnergyDetection(bool *pRxActive);
int8_t sli_802154phy_radio_energy_detection(void)
{
  return myRadioEnergyDetection(NULL);
}
static int8_t myRadioEnergyDetection(bool *pRxActive)
#else//!RAIL_LIB_4848_CCA_RX
#define myRadioEnergyDetection(pRxActive) sli_802154phy_radio_energy_detection()
int8_t sli_802154phy_radio_energy_detection(void)
#endif//RAIL_LIB_4848_CCA_RX
{
  struct reply_IE154_GET_RX_STATUS_map rspBuf;
  switch (sli_802154phy_radio_get_power_status()) {
    case SL_ZIGBEE_RADIO_POWER_MODE_ED_ON: // Use continually-updated Latched RSSI
      rspBuf.CURRENT_RSSI = siPro2GetFRR(CMD_FRR_READ_LATCHED_RSSI);
     #if     RAIL_LIB_4848_CCA_RX
      if (pRxActive != NULL) {
        rspBuf.IE154_RX_STATUS = siPro2GetFRR(CMD_FRR_READ_INT_IE154_RX_STATUS);
      }
     #endif//RAIL_LIB_4848_CCA_RX
      break;
    case SL_ZIGBEE_RADIO_POWER_MODE_RX_ON: // Use GET_RX_STATUS reply CURRENT_RSSI
      // SI4440-1624 fix in ROM6 patch exposes CURRENT_RSSI
      if ((pro2GetRomId() > 6)
          || ((pro2GetRomId() == 6) && (pro2GetPatchRev() != 0))) {
        uint8_t cmdBuf[] = {
          /*0-Command */ CMD_IE154_GET_RX_STATUS,
          /*1-RxAck   */ 0xFF, // Don't acknowledge any events
        };
        PHY_ATTEST_SUCCESS(siPro2SendCommandGetResponse(sizeof(cmdBuf), cmdBuf,
                                                        sizeof(rspBuf), (uint8_t*) &rspBuf));
      } else {
        //TODO: No patch for ROM5 fixes SI4440-1624
        return SL_ZIGBEE_PHY_INVALID_RSSI;
      }
      break;
    default: // Radio is off, hence deaf
      return SL_ZIGBEE_PHY_INVALID_RSSI;
      break;
  }
 #if     RAIL_LIB_4848_CCA_RX
  if (pRxActive != NULL) {
    // RAIL_LIB-4848 - Per Logan, RX_STATUS' !RXERROR && !RX_COMPLETE && RX_SFD
    //                 indicates actively receiving packet.
    *pRxActive = ((rspBuf.IE154_RX_STATUS & (CMD_IE154_GET_RX_STATUS_REP_IE154_RX_STATUS_RX_COMPLETE_MASK
                                             | CMD_IE154_GET_RX_STATUS_REP_IE154_RX_STATUS_RX_ERROR_MASK
                                             | CMD_IE154_GET_RX_STATUS_REP_IE154_RX_STATUS_RX_SFD_MASK))
                  == CMD_IE154_GET_RX_STATUS_REP_IE154_RX_STATUS_RX_SFD_MASK);
  }
 #endif//RAIL_LIB_4848_CCA_RX
  if (rspBuf.CURRENT_RSSI == 0) {
    return SL_ZIGBEE_PHY_INVALID_RSSI;
  }
  int16_t rssidBm = (((int16_t)rspBuf.CURRENT_RSSI / 2)
                     - (int16_t)emPhyConfigMHz.rssidBmOffset);
  return (int8_t)((rssidBm < -127) ? -127 : rssidBm);
}

uint8_t emRadioGetCca(void)
{
  // Indicate CCA_BUSY if a transmit or its ACK is still in progress
  // or RHO is active
  if ((emPhyTxState > PHY_TX_STATE_OFF)
      || (emRadioHoldOffIsActive())) {
    return CCA_BUSY_TX_ACTIVE;
  }
  // If radio isn't on, force it on for the duration of the CCA check
  // N.B. This could take a lot longer than the 8 symbols to make a CCA
  //      assessment, so callers should be aware of this limitation.
  RadioPowerMode origPowerStatus = radioCurrentPowerStatus;
  if ((origPowerStatus != SL_ZIGBEE_RADIO_POWER_MODE_ED_ON)
      && (origPowerStatus != SL_ZIGBEE_RADIO_POWER_MODE_RX_ON)) {
    pro2WakeUp(SL_ZIGBEE_RADIO_POWER_MODE_ED_ON);
    //TODO: Compensate for sli_802154phy_radio_energy_detection() execution time as well
    halCommonDelayMicroseconds(SL_ZIGBEE_PHY_CCA_DURATION_US);
  } else {
    // The overhead of sli_802154phy_radio_energy_detection() seems to be at least
    // SL_ZIGBEE_PHY_CCA_DURATION_US, if not more, so no need to delay
    // further if the radio was already on.  If caller just turned
    // the radio on themselves, they should ensure enough time has
    // passed before calling this routine.
  }
  bool rxActive = false;
  int8_t rssidBm = myRadioEnergyDetection(&rxActive);
  // Restore original radio power state
  if (origPowerStatus == SL_ZIGBEE_RADIO_POWER_MODE_OFF) {
    sli_802154phy_radio_sleep();
  } else
  if (origPowerStatus == SL_ZIGBEE_RADIO_POWER_MODE_TX_ON) {
    // Leave ourselves in ED mode for further CCAs
  } else {
    pro2WakeUp(origPowerStatus);
  }
  if (rxActive) {
    return (rssidBm > emRadioGetEdCcaThreshold()) ? CCA_BUSY_RX_ACTIVE_RSSI : CCA_BUSY_RX_ACTIVE;
  }
  return (rssidBm > emRadioGetEdCcaThreshold()) ? CCA_BUSY : CCA_CLEAR;
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

RadioPowerMode sli_802154phy_radio_get_idle_mode(void)
{
  return radioIdlePowerState;
}

sl_802154_short_addr_t sli_802154mac_radio_get_node_id(void)
{
  uint8_t* idBytes = phy2NetworkParams[phy2NetworkParamsSlotForIndex[
                                         emPhyCurrentRadioNetworkIndex()]].nodeId;
  return HIGH_LOW_TO_INT(idBytes[0], idBytes[1]);
}

sl_802154_short_addr_t sli_802154phy_radio_get_pan_id(void)
{
  uint8_t* idBytes = phy2NetworkParams[phy2NetworkParamsSlotForIndex[
                                         emPhyCurrentRadioNetworkIndex()]].panId;
  return HIGH_LOW_TO_INT(idBytes[0], idBytes[1]);
}

RadioPowerMode sli_802154phy_radio_get_power_status(void)
{
  return radioCurrentPowerStatus;
}

bool emRadioIsReceivingFrame(void)
{
  uint16_t mode = pro2CurrentMode();
  return ((mode >= CMD_IE154_QUERY_MODE_REP_OPERATING_MODE_PHASE_OPERATING_MODE_PHASE_ENUM_IE154_MODE_TX_PHASE_RECV_PHY_HDR)
          && (mode <= CMD_IE154_QUERY_MODE_REP_OPERATING_MODE_PHASE_OPERATING_MODE_PHASE_ENUM_IE154_MODE_TX_PHASE_RECV_PKT));
}

void sli_802154phy_radio_ok_to_idle(void)
{
  // Cancel any prepared Tx that never went out
  if (emPhyTxState < PHY_TX_STATE_OFF) {
    // Clean up global state modified from Prepare phase that is
    // no longer relevant because we can't complete this transmit.
    // pro2RxTxFifoInfo(false, true, NULL, NULL); // Flush Tx FIFO
    // Above line unnecessary since PrepareTx() always flishes first
    emPhyTxState = PHY_TX_STATE_OFF;
    bytesToWrite = 0;
  }
  if (radioIdlePowerState == SL_ZIGBEE_RADIO_POWER_MODE_OFF) {
    sli_802154phy_radio_sleep();
  }
 #if     (PRO2_MAX_NETWORKS > 2)
  else {
    // Upper layer transmit / data poll operation is complete:
    // In multi-network scenario, the transmit might have been on another
    // network (band/channel) so we need to ensure the PRO2+ is put back
    // on the proper listening network (band/channel).
    // Note that the PRO2+ will automatically return to its original channel
    // (but not band) after transmit (and ACK reception) unless the ACK had
    // frame-pending set, in which case it stays on the Tx band/channel
    // until the polled data arrives or times out, which brings us here.
    // So if a data poll 'stayed' us on the Tx channel, or the Tx left us
    // on the wrong band, we need to abort & restart listening on the
    // current network's channel (phy2Channel).
    if ((phy2Channel != listeningPhyChan)  // a data poll 'stay'
        || (mapChannelToBand(phy2Channel) != phy2BandConfig)) {
      pro2RestartRadio(ANY_TX_ERROR);
    }
  }
 #endif//(PRO2_MAX_NETWORKS > 2)
}

bool emRadioOverflowNotificationEnabled(void)
{
  return true; // PRO2+ always has overflow notification enabled
}

bool sli_802154phy_radio_packet_trace_enabled(void)
{
  return (phy2PtiGpio > PRO2_GPIO_NONE);
}

void emRadioPowerDown(void)
{
  if (pro2PoweredUp) {
    sli_802154phy_radio_sleep();
    siPro2PowerDown();
    pro2PoweredUp = false;
    memset(&pro2PartInfo, 0, sizeof(pro2PartInfo));
    memset(&pro2FuncInfo, 0, sizeof(pro2FuncInfo));
  }
}

void emRadioPowerUp(void)
{
  if (!pro2PoweredUp) {
    pro2PowerUp(); // Will set pro2PoweredUp true
  }
}

bool emRadioReceiveCompleteIntEnabled(void)
{
  return !!(phy2RxEvents & PROP_IE154_INT_CTL_RX_ENABLE_RX_COMPLETE_MASK);
}

bool emRadioReceiveCompleteIntPending(bool acknowledgePending)
{
  bool pending = (siPro2GetFRR(CMD_FRR_READ_INT_IE154_RX_PEND)
                  & CMD_IE154_GET_RX_STATUS_REP_IE154_RX_PEND_RX_COMPLETE_PEND_MASK);
  if (acknowledgePending && pending) {
    // Acknowledge the RX_COMPLETE interrupt
    uint8_t cmdBuf[] = {
      /*0-Command */ CMD_IE154_GET_RX_STATUS,
      /*1-RxAck   */ ~(CMD_IE154_GET_INT_STATUS_ARG_IE154_RX_CLR_PEND_RX_COMPLETE_PEND_CLR_MASK),
    };
    PHY_ATTEST_SUCCESS(siPro2SendCommandNoResponse(sizeof(cmdBuf), cmdBuf));
  }
  return pending;
}

bool emRadioReceiveCrcEnabled(void)
{
  return !!(phy2CoPhCfg & PROP_IE154_COMMON_PHY_CFG_CHECK_CRC_MASK);
}

sl_status_t emRadioSetChannelAndForceCalibration(uint8_t macPgChan)
{
  sl_status_t status;
  pro2ForceCalibration = true;
  status = emPhySetRadioChannel(macPgChan);
  if (status != SL_STATUS_OK) {
    pro2ForceCalibration = false;
  }
  return status;
}

void emRadioSetEdCcaThreshold(int8_t thresholddBm)
{
  if (thresholddBm == ED_CCA_THRESHOLD_UNINIT) {
    // The first time emRadioSetEdCcaThreshold() is called we use the
    // CCA_THRESHOLD mfg token, if present, else we use the default.
    // Subsequent calls to emRadioSetEdCcaThreshold will ignore the
    // token/default value and use whatever value the user passes in.
    tokTypeMfgCcaThreshold ccaThresholdToken = 0xFFFF; // Assume unset
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
    // This PHY only supports ccaThresholdSubNeg:
    uint8_t ccaThresholdSubNeg = (ccaThresholdToken >> 9);
    if ((ccaThresholdSubNeg > 0) && (ccaThresholdSubNeg < 127)) {
      thresholddBm = -(int8_t)ccaThresholdSubNeg;
    } else {
      // Default is to use per-sub-band edThresholddBm which
      // is emplaced during switchBand(), so just ensure
      // edCcaThreshold get set to _SUBBAND sentinal.
      thresholddBm = ED_CCA_THRESHOLD_SUBBAND;
    }
  }
  {
    DECLARE_INTERRUPT_STATE;
    DISABLE_INTERRUPTS(); // Protect against emRadioHoldOffIsr() while changing radio state
    // Only allow the shadow state to be updated when Radio HoldOff is active
    // but not the real state of the radio
    if (!emRadioHoldOffIsActive()) {
      pro2SetCcaThreshold(thresholddBm);
    }
    edCcaThreshold = thresholddBm;
    RESTORE_INTERRUPTS();
  }
}

sl_status_t sli_802154phy_radio_set_idle_mode(RadioPowerMode mode)
{
  if (mode > SL_ZIGBEE_RADIO_POWER_MODE_ED_ON) {
    return SL_STATUS_FAIL;
  }
  radioIdlePowerState = mode;
  if (radioIdlePowerState != radioCurrentPowerStatus) {
    switch (radioIdlePowerState) {
      case SL_ZIGBEE_RADIO_POWER_MODE_OFF:
        sli_802154phy_radio_sleep();
        break;
      case SL_ZIGBEE_RADIO_POWER_MODE_RX_ON:
      case SL_ZIGBEE_RADIO_POWER_MODE_ED_ON:
        pro2WakeUp(radioIdlePowerState);
        break;
      default:
        PHY_ASSERT(false);
        break;
    }
  }
  return SL_STATUS_OK;
}

void sli_802154phy_radio_set_node_idOnNetwork(sl_802154_short_addr_t nodeId, EmberNetworkIndex networkIndex)
{
  PHY_ASSERT(networkIndex < PRO2_MAX_NETWORKS);
  pro2PowerUp(); // Ensure PRO2+ is up
  int8_t slot = phy2NetworkParamsSlotForIndex[networkIndex];
  if ((phy2NetworkParams[slot].nodeId[0] != HIGH_BYTE(nodeId))
      || (phy2NetworkParams[slot].nodeId[1] != LOW_BYTE(nodeId))) {
    phy2NetworkParams[slot].nodeId[0] = HIGH_BYTE(nodeId);
    phy2NetworkParams[slot].nodeId[1] = LOW_BYTE(nodeId);
    pro2WriteNProperties(PROP_GRP_ID_IE154_NETWORK,
                         PROP_IE154_NETWORK_LIST_OFFSET
                         + (sizeof(Pro2NetworkParams) * slot)
                         + offsetof(Pro2NetworkParams, nodeId),
                         sizeof(nodeId),
                         &phy2NetworkParams[slot].nodeId[0]);
  }
}

void sli_802154phy_radio_set_node_id(sl_802154_short_addr_t nodeId)
{
  sli_802154phy_radio_set_node_idOnNetwork(nodeId, emPhyCurrentRadioNetworkIndex());
 #ifdef  emPhyMaxPowerNetworkIndex
  sli_802154phy_radio_set_node_idOnNetwork(nodeId, emPhyMaxPowerNetworkIndex());
 #endif//emPhyMaxPowerNetworkIndex
}

void sli_802154phy_radio_set_pan_idOnNetwork(sl_802154_pan_id_t panId, EmberNetworkIndex networkIndex)
{
  PHY_ASSERT(networkIndex < PRO2_MAX_NETWORKS);
  pro2PowerUp(); // Ensure PRO2+ is up
  int8_t slot = phy2NetworkParamsSlotForIndex[networkIndex];
  if ((phy2NetworkParams[slot].panId[0] != HIGH_BYTE(panId))
      || (phy2NetworkParams[slot].panId[1] != LOW_BYTE(panId))) {
    phy2NetworkParams[slot].panId[0] = HIGH_BYTE(panId);
    phy2NetworkParams[slot].panId[1] = LOW_BYTE(panId);
    pro2WriteNProperties(PROP_GRP_ID_IE154_NETWORK,
                         PROP_IE154_NETWORK_LIST_OFFSET
                         + (sizeof(Pro2NetworkParams) * slot)
                         + offsetof(Pro2NetworkParams, panId),
                         sizeof(panId),
                         &phy2NetworkParams[slot].panId[0]);
  }
}

void sli_802154phy_radio_set_pan_id(sl_802154_pan_id_t panId)
{
  sli_802154phy_radio_set_pan_idOnNetwork(panId, emPhyCurrentRadioNetworkIndex());
 #ifdef  emPhyMaxPowerNetworkIndex
  sli_802154phy_radio_set_pan_idOnNetwork(panId, emPhyMaxPowerNetworkIndex());
 #endif//emPhyMaxPowerNetworkIndex
}

WEAK(void emRadioSfdRecvIsrCallback(void))
{
  // no-op
}

bool emRadioSfdRecvNotificationEnabled(void)
{
  return !!(phy2RxEvents & PROP_IE154_INT_CTL_RX_ENABLE_RX_SFD_MASK);
}

bool emRadioSfdSentNotificationEnabled(void)
{
  return !!(phy2TxEvents & PROP_IE154_INT_CTL_TX_ENABLE_TX_SFD_SENT_MASK);
}

void sli_802154phy_radio_sleep(void)
{
  //TODO: Need to protect against re-entrancy from ISR context?
  if (radioCurrentPowerStatus != SL_ZIGBEE_RADIO_POWER_MODE_OFF) {
    MAC_INTERRUPTS_OFF();
    phy2Listening = false;
    pro2IdleRadio();
    // We won't be re-enabling interrupts to process anything the radio reports
    // from yanking it to Idle mode, so need to clean up Tx state machine here.
    pro2CleanupTxState(NO_TX_ERRORS);
    pro2SleepRadio(); // Must IDLE before SLEEP; don't confuse with PowerDown

    radioCurrentPowerStatus = SL_ZIGBEE_RADIO_POWER_MODE_OFF;
  }
}

#if     (!PHY_USE_PREPARE_TX_API)
static // Let the WEAK one be used by outside callers
#endif//(!PHY_USE_PREPARE_TX_API)
sl_status_t emRadioPrepareTransmit(uint8_t* memptr)
{
  sl_status_t status;

  if (emPhyTxState != PHY_TX_STATE_OFF) {
    return SL_STATUS_TRANSMIT_BUSY;
  }

  uint8_t slot = phy2NetworkParamsSlotForIndex[emPhyCurrentRadioNetworkIndex()];

  // Ensure we're on the proper band for this network's channel.
  // If band switch is needed, radio must be IDLEd first.
  status = switchBand(phy2NetworkParams[slot].phyChan, true);
  if (status != SL_STATUS_OK) {
    return status;
  }

  emPhyTxState = PHY_TX_STATE_PREP_NOACK;
  lastTxSfdTime = 0;
  radioPostTxListening = (radioIdlePowerState == SL_ZIGBEE_RADIO_POWER_MODE_RX_ON);

  txCmdBuf[0] = /*0-Command */ CMD_IE154_START_TX;
  txCmdBuf[1] = /*1-Conditio*/ (pro2UpdateTxCsmaProperties()
                                << CMD_IE154_START_TX_ARG_OPTIONS_NUM_CCA_CHECKS_LSB);
  txCmdBuf[2] = /*2-Network */ slot;
  txCmdBuf[3] = /*3-NextMode*/ (0
                                | (((radioPostTxListening)
                                    ? CMD_IE154_START_TX_ARG_NEXT_MODE_TX_FAIL_MODE_ENUM_LISTEN
                                    : CMD_IE154_START_TX_ARG_NEXT_MODE_TX_FAIL_MODE_ENUM_IDLE
                                    ) << CMD_IE154_START_TX_ARG_NEXT_MODE_TX_FAIL_MODE_LSB)
                                | (((radioPostTxListening)
                                    ? CMD_IE154_START_TX_ARG_NEXT_MODE_TX_COMPLETE_MODE_ENUM_LISTEN
                                    : CMD_IE154_START_TX_ARG_NEXT_MODE_TX_COMPLETE_MODE_ENUM_IDLE
                                    ) << CMD_IE154_START_TX_ARG_NEXT_MODE_TX_COMPLETE_MODE_LSB)
                                );

  if (radioTransmitConfig.appendCrc != (bool) !!(phy2CoPhCfg & PROP_IE154_COMMON_PHY_CFG_APPEND_CRC_MASK)) {
    pro2UpdateCoPhCfg(PROP_IE154_COMMON_PHY_CFG_APPEND_CRC_MASK,
                      radioTransmitConfig.appendCrc << PROP_IE154_COMMON_PHY_CFG_APPEND_CRC_LSB);
  }

  if (radioCurrentPowerStatus == SL_ZIGBEE_RADIO_POWER_MODE_OFF) {
    // The radio is asleep.  Power up the radio for the Tx.
    // On PRO2+, this is done here in this routine; it differs
    // from em3xx where a power-savings trick delays this to a
    // CCA_FAIL event cleverly scheduled just before real CCA needs
    // to occur.
    pro2WakeUp(SL_ZIGBEE_RADIO_POWER_MODE_TX_ON); // Just wake up PRO2 for Tx
    // MAC_INTERRUPTS_ON() performed by pro2WakeUp()
  } else {
    // Radio is awake
    //TODO: Need to reset TX side prior to every Tx??  Remove if not.
    Events events = {
      0, 0,
      0, ACK_ALL /*txAck*/,
      0, 0 /*rxAck*/,
      0, 0 /*chAck*/,
      0, 0,
      0, 0   // TX_FIFO_SPACE, RX_FIFO_COUNT
    };
    pro2ResetEvents(&events);
    MAC_INTERRUPTS_ON();
  }

  // Load the TX FIFO -- When Radio is doing CRC, do NOT include those bytes
  // If we're in 15.4G mode, upper layers can pass us a 15.4g 16-bit PHR
  // in bit-reversed "host order", or a 15.4 8-bit PHR.  We distingiush
  // the two by the most-significant bit of the first byte, which for a
  // 15.4g PHR is the Mode Switch bit, which we don't support so subvert
  // for this purpose.
  uint8_t bytesWritten = 0;
  uint8_t crcLen;
  int16_t macLength;
  uint8_t* macPayload;
  if (memptr[0] & 0x80) { // Caller passed down a 15.4g PHR
    uint16_t phr154g = HIGH_LOW_TO_INT((memptr[0] & ~0x80), memptr[1]);
    if (IE154G_ACTIVE()) {
      // 15.4g PHR passed down and 15.4g mode is enabled:
      memptr[0] &= ~0x80; // Toggle MS bit off temporarily thru FIFO load
      macLength = (int16_t) (phr154g & PHR_154G_FRAME_LENGTH_MASK);
      macPayload = &memptr[2];
      bytesToWrite = macLength + 2;                         // PSDU+PHR length
      crcLen = ((phr154g & PHR_154G_FCS_MASK) ? 2 : 4);     // FCS  length
    } else {
      // 15.4g PHR passed down, but 15.4g mode not enabled:
      // If this frame requests any 15.4g features (like 4-byte FCS or DW
      // or packet > 127 bytes in length) then fail, otherwise convert to
      // 15.4-2003/2006/2011 1-byte PHR
      if ((phr154g & ~(((uint16_t)PHR_154G_FRAME_LENGTH_MASK) & 0x007F))
          != PHR_154G_FCS_MASK) {
        return SL_STATUS_FAIL; // Illegal/unsupported PHR content
      }
      memptr++; // Skip over unused byte of 15.4g PHR to point to length byte
      macLength = (int16_t) memptr[0];
      macPayload = &memptr[1];
      bytesToWrite = macLength + 1;                         // PSDU+PHR length
      crcLen = 2;                                           // FCS  length
    }
  } else {
    // Old-style max-127 byte 15.4-2003/2006/2011 frame
    macLength = (int16_t) memptr[0];
    macPayload = &memptr[1];
    bytesToWrite = macLength + 1;                           // PSDU+PHR length
    crcLen = 2;                                             // FCS  length
    if (IE154G_ACTIVE()) {
      // Need to provide 15.4g PHR to PRO2+, e.g. 2-byte FCS, DW, etc.
      PHY_ATTEST_SUCCESS(siPro2WriteTxFifo(1, &emPhyPhr154gFeatures));
      bytesWritten = 1;
    } else {
      // Nothing special -- just fall thru to write the frame as-is!
    }
  }
  PHY_ASSERT((bytesToWrite >= 0)
             && (bytesToWrite <= SL_ZIGBEE_PHY_MAX_PAYLOAD_BYTES
                 + (IE154G_ACTIVE() ? 2 : 1)));

  if (radioTransmitConfig.appendCrc) {
    PHY_ASSERT(bytesToWrite > crcLen);
    bytesToWrite -= crcLen;
  }

  // waitForAck is ignored in sli_legacy_mfglib_mode, even if the packet's AR bit is set
  if (!sli_legacy_mfglib_mode && radioTransmitConfig.waitForAck) {
    txCmdBuf[1] |= CMD_IE154_START_TX_ARG_OPTIONS_WAIT_FOR_ACK_MASK;
    // Note whether this outgoing packet will need an ACK.  This is used
    // later to delay emberRadioTransmitCompleteCallback() until ACK is
    // either received or timed out.
    if ((macLength > 0) && (macPayload[0] & MAC_FRAME_FLAG_ACK_REQUIRED)) {
      emPhyTxState = PHY_TX_STATE_PREP_W_ACK;
    }
  }

 #ifdef   DEBUG_BACKOFF_STATS
  emMacTransmits++;
 #endif// DEBUG_BACKOFF_STATS
  PHY_TRACE_PKT(((emPhyTxState == PHY_TX_STATE_PREP_W_ACK) ? '\x29' : '>'), memptr);  // right paren

  if ((bytesToWrite + bytesWritten) > PRO2_TX_FIFO_SIZE) {
    PHY_ATTEST_SUCCESS(siPro2WriteTxFifo(PRO2_TX_FIFO_SIZE - bytesWritten, memptr));
    bytesToWrite -= (PRO2_TX_FIFO_SIZE - bytesWritten);
    txBufPending = memptr + PRO2_TX_FIFO_SIZE - bytesWritten;
    // Callers won't change buffer before Tx complete callback
    // Enable TX_FIFO_ALMOST_FULL event since we'll need it
    pro2UpdateEvents(phy2L1Events,
                     phy2RxEvents,
                     phy2TxEvents | PROP_IE154_INT_CTL_TX_ENABLE_TX_FIFO_ALMOST_EMPTY_MASK,
                     phy2ChEvents);
  } else {
    PHY_ATTEST_SUCCESS(siPro2WriteTxFifo(bytesToWrite, memptr));
    bytesToWrite = 0;
    txBufPending = NULL;
    // Disable TX_FIFO_ALMOST_FULL event since we don't need it
    pro2UpdateEvents(phy2L1Events,
                     phy2RxEvents,
                     phy2TxEvents & ~PROP_IE154_INT_CTL_TX_ENABLE_TX_FIFO_ALMOST_EMPTY_MASK,
                     phy2ChEvents);
  }
  if (IE154G_ACTIVE() && (macPayload == &memptr[2])) {
    // Restore MS bit in PHR that caller had set which we temporarily
    // turned off, now that FIFO has been loaded.
    memptr[0] |= 0x80;
  }

  if (pro2ForceCalibration) {
    pro2ForceCalibration = false;
    //TODO: How to force calibration?  Used to be a flag arg to START_TX...
    //TODO: The IE154_CALIBRATE command seems to be more a property than
    //TODO: a command?  What is "cold boot" vs. "warm boot"?
  }

 #if     TX_WAIT_RX_ACK_SENT
  // Wait while Tx ACK completes.
  // This is a hack to avoid CCA or TX failure
  // due to an ACK going out since we pass up RX traffic before
  // its ACK is sent, so a new TX operation can be initiated
  // before then too.
  // We should not spin here too long -- but keep an eye on it in case we do.
  uint32_t startTime = sli_802154phy_get_mac_timer();
  // Basically, we look at the INT_IE154_RX_STATUS, which will have RX_SRC_ADDR
  // set for an RX packet that came in, passed filtering, and requests an ACK;
  // and wait for either RX_ACK_TX_COMPLETE or RX_ERROR indicating ACK has gone
  // out or failed.  This spans the Rx-to-Tx turnaround.
  while ((siPro2GetFRR(CMD_FRR_READ_INT_IE154_RX_STATUS)
          & (CMD_IE154_GET_RX_STATUS_REP_IE154_RX_STATUS_RX_SRC_ADDR_MASK
             | CMD_IE154_GET_RX_STATUS_REP_IE154_RX_STATUS_RX_ACK_TX_COMPLETE_MASK
             | CMD_IE154_GET_RX_STATUS_REP_IE154_RX_STATUS_RX_ERROR_MASK))
         == (CMD_IE154_GET_RX_STATUS_REP_IE154_RX_STATUS_RX_SRC_ADDR_MASK)) {
    // spin
    if (elapsedTimeMacTimer(startTime, sli_802154phy_get_mac_timer()) > 5000) {
      PHY_ASSERT(false);  // Should never spin for more than 1ms, let alone 5!
      break;
    }
  }
 #endif//TX_WAIT_RX_ACK_SENT

 #if     (PRO2_MAX_NETWORKS > 2)
  lastTransmitPhyChan = phy2NetworkParams[slot].phyChan;
 #endif//(PRO2_MAX_NETWORKS > 2)

  return SL_STATUS_OK;
}

sl_status_t emRadioTransmit(uint8_t* memptr)
{
  // If no Tx has been prepared, try to prepare it
  if (emPhyTxState == PHY_TX_STATE_OFF) {
    sl_status_t status = emRadioPrepareTransmit(memptr);
    return status;
  } else
  if (emPhyTxState > PHY_TX_STATE_OFF) {
    return SL_STATUS_TRANSMIT_BUSY;
  }
  // Tx has been prepared, so send it out!
  emPhyTxState = (EmPhyTxState) - emPhyTxState; // PREP to TRIG equivalent
  lastTxStartedTimeMs = (int32_t) halCommonGetInt16uMillisecondTick();
  PHY_ATTEST_SUCCESS(siPro2SendCommandNoResponse(sizeof(txCmdBuf), txCmdBuf));
  PHY_ATTEST_SUCCESS(siPro2WaitForReady()); // Avoids deferring this into the ISR
  return SL_STATUS_OK;
}

static void pro2WakeUp(RadioPowerMode mode)
{
  if (radioCurrentPowerStatus != mode) {
    // The following mode transitions are possible here:
    //   OFF   => RX_ON or ED_ON or TX_ON
    //   TX_ON => RX_ON or ED_ON
    //   RX_ON => ED_ON
    //   ED_ON => RX_ON
    // All mode transitions except from TX_ON require radio to be off first
    if (radioCurrentPowerStatus == SL_ZIGBEE_RADIO_POWER_MODE_TX_ON) {
      // Don't need to clear any stale Rx or Chip events because
      // that was done by OFF => TX_ON transition already.
      radioCurrentPowerStatus = mode;
    } else {
      sli_802154phy_radio_sleep(); // Will be no-op if radio is already off
      radioCurrentPowerStatus = mode;

     #if     (SI4440_1462_TOLERATE == 1)
      // Need to re-enable PTI GPIO configuration when waking from SLEEP
      emRadioEnablePacketTrace(sli_802154phy_radio_packet_trace_enabled());
     #endif//(SI4440_1462_TOLERATE == 1)

      // Clear out any stale events, except CMD_ERRORs
      Events events = {
        0, 0,
        0, ACK_ALL /*txAck*/,
        0, ACK_ALL /*rxAck*/,
        0, CMD_IE154_GET_INT_STATUS_REP_IE154_CHIP_PEND_CMD_ERROR_PEND_MASK
        ^ ACK_ALL /*chAck*/,
        0, 0,
        0, 0   // TX_FIFO_SPACE, RX_FIFO_COUNT
      };
      pro2ResetEvents(&events);
      siPro2IntClearPend();
      MAC_INTERRUPTS_ON();
    }

    switch (mode) {
      case SL_ZIGBEE_RADIO_POWER_MODE_RX_ON:
        pro2StartRx();
        break;
      case SL_ZIGBEE_RADIO_POWER_MODE_ED_ON:
        pro2StartEd();
        break;
      case SL_ZIGBEE_RADIO_POWER_MODE_TX_ON:
        break;
      default:
        PHY_ASSERT(false);
        break;
    }
  }
}

void sli_802154phy_radio_wake_up(void)
{
  pro2WakeUp(SL_ZIGBEE_RADIO_POWER_MODE_RX_ON);
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

static void emRadioHoldOffInternalIsr(uint8_t active)
{
  if (!!active != !!rhoActive) {
    rhoActive = active; // Update rhoActive early
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
      pro2PowerUp(); // Ensure PRO2+ is up
      pro2SetCcaThreshold(0);
      if (pro2RxFlags & PROP_IE154_RX_FLAGS_AUTO_ACK_DEFAULT_VALUE_MASK) {
        pro2RxFlags &= ~PROP_IE154_RX_FLAGS_AUTO_ACK_DEFAULT_VALUE_MASK;
        pro2WriteOneProperty(PROP_GRP_ID_IE154_RX,
                             PROP_IE154_RX_FLAGS_OFFSET,
                             pro2RxFlags);
      }
    } else {
      // Restore pre-Radio-HoldOff settings (which stack might have
      // changed while Radio HoldOff was active) using the shadow
      // copies the stack maintains in RAM.
      emRadioSetEdCcaThreshold(edCcaThreshold); // Don't use emRadioGetEdCcaThreshold() here
      sli_802154mac_radio_enable_auto_ack(sli_802154phy_radio_auto_ack_enabled());
    }
  } else {
    rhoActive = active; // Update for possibly new active state
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

void sli_802154phy_radio_init(RadioPowerMode initialRadioPowerMode)
{
  PHY_ASSERT(initialRadioPowerMode <= SL_ZIGBEE_RADIO_POWER_MODE_OFF);
  // sli_802154phy_radio_init() is not expecting to be called with the radio already on,
  // but if it is, simply return.
  if (radioCurrentPowerStatus != SL_ZIGBEE_RADIO_POWER_MODE_OFF) {
    return;
  }

  pro2PowerUp(); // Ensure PRO2+ is up -- last chance!

  // em35x should be running off PRO2+-provided system clock at this point,
  // so we know the PRO2+ is powered up and partly configured already.

  // Set all the internal properties in the radio-internal-config.h file.
  // Modem and other properties will get set below based on channel selection.
  pro2SetRadioProperties(radioInternalConfigArray);

  // Per-band CSMA and ACK symbol timing properties are now done in switchBand()

  pro2UpdateMaxPayloadSize(pro2CoPhCfg);

  //TODO: Need to (re-)establish phy2NetworkParams[] settings, other state??

  pro2SetRadioRngSeed();

  pro2UpdateRxFlags(ALL_FLAGS, phy2RxFlags);
  pro2UpdateTxFlags(ALL_FLAGS, phy2TxFlags);
  pro2UpdateCoFlags(ALL_FLAGS, phy2CoFlags);
  pro2UpdateCoPhCfg(ALL_FLAGS, phy2CoPhCfg);
  pro2UpdateEvents(phy2L1Events, phy2RxEvents, phy2TxEvents, phy2ChEvents);
  PHY_ATTEST_SUCCESS(siPro2WaitForReady());
  // The POWER_UP command issued earlier can assert nIRQ,
  // likely (hopefully!) while interrupts were disabled.
  // Now that we've configured the events we really want,
  // nIRQ should deassert.  Clear any pended interrupt
  // now so we don't take a phantom interrupt later when
  // first enabling MAC interrupts.
  siPro2IntClearPend();

  sli_802154_stack_set_tx_power_mode(emGetTxPowerMode());

  sli_zigbee_init_eui64();
  sli_802154phy_set_eui64();

  emRadioEnableAddressMatching(emRadioAddressMatchingEnabled());
  sli_802154mac_radio_enable_auto_ack(sli_802154phy_radio_auto_ack_enabled());
  // Begin network parameters -- the order is important here
  PHY_ATTEST_SUCCESS(emPhySetRadioChannel(sli_802154phy_get_radio_channel()));
  emPhySetRadioPower(phy2TxPowerRequested[emPhyCurrentRadioNetworkIndex()]);
  sli_802154phy_radio_set_node_id(sli_802154mac_radio_get_node_id());
  sli_802154phy_radio_set_pan_id(sli_802154phy_radio_get_pan_id());
  sli_802154phy_set_coordinator(emPhyDeviceIsCoordinator());
  // End network parameters -- last one should be Coordinator
  emRadioEnablePacketTrace(sli_802154phy_radio_packet_trace_enabled());

  emRadioSetEdCcaThreshold(edCcaThreshold); // Don't use emRadioGetEdCcaThreshold() here
  emRadioEnableReceiveCrc(emRadioReceiveCrcEnabled());

  if (initialRadioPowerMode == SL_ZIGBEE_RADIO_POWER_MODE_OFF) {
    // Above code has PRO2 in IDLE; we want it to be in SLEEP.
    // But calling sli_802154phy_radio_sleep() would be a no-op and if we
    // forced a call, the ext-driver's interrupt counter would
    // be off by a level deeper than we want.  So use internal
    // routines to sleep radio and ensure GPIOs are set right:
    pro2IdleRadio(); // Shouldn't be needed, but just in case.
    pro2SleepRadio();
  } else {
    pro2WakeUp(initialRadioPowerMode);
  }
}

// From em250class/phy-common.c

// For PRO2+, recall LQI = ((RSSI < 0) ? (RSSI + 128) * 2 : 255)
// as placed into the APPENDED_INFO_LQI_BYTE_INDEX of received frames.
static const uint8_t sli_802154phy_cost_divisions[] = {
// >=RSSI                LQI  255-LQI  index  cost
//   ----                ---  -------  -----  ----
  (-62 + 128) * 2,  // = 132    123      0      1
  (-72 + 128) * 2,  // = 112    143      1      3
  (-92 + 128) * 2,  // =  72    183      2      5
  (-128 + 128) * 2, // =   0    255      3      7
};

uint8_t sli_802154phy_cost_division(uint8_t cost)
{
  return sli_802154phy_cost_divisions[cost >> 1];
}

// For PRO2, LQI mapping is done in PHY so the value in the appended info
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

//== ember API ================================================================

void sl_802154_calibrate_current_channel(void)
{
  emRadioSetChannelAndForceCalibration(sli_802154phy_get_radio_channel());
}

uint16_t emGetTxPowerMode(void)
{
  return emRadioTxPowerMode;
}

sl_status_t sli_802154_stack_set_tx_power_mode(uint16_t txPowerMode)
{
  // This will override any radio per-band PA configuration settings,
  // so use with care!
  // First check to see if we are using defaults from the PHY_CONFIG token.
  if (txPowerMode == SL_ZIGBEE_TX_POWER_MODE_USE_TOKEN) {
    // The application has not overriden the configuration stored in the token.
    tokTypeMfgPhyConfig phyConfig = SL_ZIGBEE_TX_POWER_MODE_PER_BAND_CONFIG;
    halCommonGetMfgToken(&phyConfig, TOKEN_MFG_PHY_CONFIG);

    // The high byte of PHY_CONFIG contains the value to assign to TX_STEP_TIME
    // when the radio is using the alternate RF path.
    // On PRO2+ this is currently ignored.

    // The bits 7:3 PHY_CONFIG contain the boost tx power offset from the first
    // value above nominal max (+3dBm).  0:+4dBm, 1:+5dBm, ..., 4:+8dBm.
    // On PRO2+ this is currently ignored.  Instead:
    // bits 7:6 PHY_CONFIG specify whether to bypass per-band Tx power limits
    // if set to 0b10

    // The bits 2:0 PHY_CONFIG contain PA selection flags
    // On PRO2+ bit 2 is currently ignored and
    // bits 1:0 PHY_CONFIG specify the power PA to use (and these bits are
    // *not* inverted like on em3xx because default is max power PA).
    txPowerMode = (uint16_t) phyConfig;
  }

  // Store the new power mode configuration, capturing anything that was loaded
  // from the PHY_CONFIG token along the way.
  emRadioTxPowerMode = pro2OverridePaSelection(txPowerMode);
  return SL_STATUS_OK;
}

// Unsupported on this PHY
sl_status_t sli_802154_stack_radio_get_scheduler_priorities(sl_802154_radio_priorities_t *priorities)
{
  (void) priorities;
  return SL_STATUS_INVALID_STATE;
}

// Unsupported on this PHY
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

#endif//(PHY_DUALRAIL && !PHY_THIS && !PHY_INCLUDE_SOURCE)
