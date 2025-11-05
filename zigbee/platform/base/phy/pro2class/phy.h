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
#ifndef __PHY_PRO2CLASS_PHY_H__
#define __PHY_PRO2CLASS_PHY_H__

#ifndef PRO2_FW_API
#define PRO2_FW_API 0x06000A00ul // fw_6_0_10 (on A2A otp_6_0_7)
#endif//PRO2_FW_API

/*----------------------------------------------------------------------------*/
/*                                Constants                                   */
/*----------------------------------------------------------------------------*/

#include "phy-params-subGHz.h"

// These are no longer constants, but configurable:
#define RSSI_DBM_OFFSET_PRO2CLASS       (-(int16_t)emPhyConfigMHz.rssidBmOffset)
#define ED_CCA_THRESHOLD_UNINIT         0x7F
#define ED_CCA_THRESHOLD_SUBBAND        (ED_CCA_THRESHOLD_UNINIT - 1)

#define MIN_RADIO_POWER                 -43
#undef  MAX_RADIO_POWER // Replace any prior def with pro2class max
#define MAX_RADIO_POWER                 21
#define INVALID_RADIO_POWER             SL_ZIGBEE_PHY_INVALID_RSSI // -128

// PRO2+ needs a distinct mode for Energy Detection.
// This definition overrides enum in phy/phy.h
#define SL_ZIGBEE_RADIO_POWER_MODE_ED_ON (SL_ZIGBEE_RADIO_POWER_MODE_OFF + 1)
#define SL_ZIGBEE_RADIO_POWER_MODE_TX_ON (SL_ZIGBEE_RADIO_POWER_MODE_OFF + 2)

// PRO2+ sli_802154_stack_set_tx_power_mode() value to not override per-band config settings
#define SL_ZIGBEE_TX_POWER_MODE_PER_BAND_CONFIG       (0xFFFFu)
#define SL_ZIGBEE_TX_POWER_MODE_PA_SELECTION_MASK     (0x0003u)
#define SL_ZIGBEE_TX_POWER_MODE_BAND_NO_LIMITS_MASK   (0x00C0u)
#define SL_ZIGBEE_TX_POWER_MODE_BAND_NO_LIMITS_VALUE  (0x0080u)

#define NUM_RX_BUFFER_BYTES (SL_ZIGBEE_PHY_MAX_PHR_BYTES       \
                             + SL_ZIGBEE_PHY_MAX_PAYLOAD_BYTES \
                             + 1 /*optional pad byte*/         \
                             + NUM_APPENDED_INFO_BYTES         \
                             )

#define RADIO_CCA_ATTEMPT_MAX_802_15_4  5 // PRO2+ supports max 5 attempts

/*----------------------------------------------------------------------------*/
/*                                Data Types                                  */
/*----------------------------------------------------------------------------*/

typedef uint8_t EmberNetworkIndex;    // This really belongs in multi-network.h

// EmPhyMode is encoded:
// [  7 | 6  | 5 | 4         |     3    |    2   |    1   |      0        ]
// [ FILTER  |FPD| MR-encode | MR-SFD   |    FSK MODE     |   PHR Format  ]
// [ 00=2003 | 0 | x (2011)  | x (2011) | 00=2011 GFSK    | 0=2003/6/11   ]
// [ 01=2006 | 0 | 0=UNCODED | 0=SFD0   | 01=2012 MR-2FSK | 1=2012 MR-FSK ]
// [ 10=2012 | 0 | 1=ENCODED | 1=SFD1   | 02=2012 MR-4FSK |               ]
// FPD is Frame Pending Default, and must-be-zero currently.
enum {
  //          PHR   FSK  SFD   CODE  FILTER
  EM_PHY_MODE_2011_GFSK______________F2003 = 0x00,
  EM_PHY_MODE_2011_GFSK______________F2006 = 0x40,
  EM_PHY_MODE_2011_GFSK______________F2012 = 0x80,
  EM_PHY_MODE_2012_2FSK_SFD0_UNCODED_F2006 = 0x43,
  EM_PHY_MODE_2012_2FSK_SFD0_UNCODED_F2012 = 0x83,
  EM_PHY_MODE_2012_2FSK_SFD0_ENCODED_F2006 = 0x53,
  EM_PHY_MODE_2012_2FSK_SFD0_ENCODED_F2012 = 0x93,
  EM_PHY_MODE_2012_2FSK_SFD1_UNCODED_F2006 = 0x4B,
  EM_PHY_MODE_2012_2FSK_SFD1_UNCODED_F2012 = 0x8B,
  EM_PHY_MODE_2012_2FSK_SFD1_ENCODED_F2006 = 0x5B,
  EM_PHY_MODE_2012_2FSK_SFD1_ENCODED_F2012 = 0x9B,
  EM_PHY_MODE_2012_4FSK_SFD0_UNCODED_F2006 = 0x45,
  EM_PHY_MODE_2012_4FSK_SFD0_UNCODED_F2012 = 0x85,
  EM_PHY_MODE_2012_4FSK_SFD0_ENCODED_F2006 = 0x55,
  EM_PHY_MODE_2012_4FSK_SFD0_ENCODED_F2012 = 0x95,
  EM_PHY_MODE_2012_4FSK_SFD1_UNCODED_F2006 = 0x4D,
  EM_PHY_MODE_2012_4FSK_SFD1_UNCODED_F2012 = 0x8D,
  EM_PHY_MODE_2012_4FSK_SFD1_ENCODED_F2006 = 0x5D,
  EM_PHY_MODE_2012_4FSK_SFD1_ENCODED_F2012 = 0x9D,
};
typedef uint8_t EmPhyMode;

#ifndef PHY_HAS_DUTY_CYCLE
#define PHY_HAS_DUTY_CYCLE 1    // This PHY has duty cycle requirements
#endif//PHY_HAS_DUTY_CYCLE

#define EM_PHY_LBT_FLAG_MODE_MASK 0x01u // Mask for the parameter's mode
#define EM_PHY_LBT_FLAG_MODE_CSMA 0x00u // CSMA-mode parameters
#define EM_PHY_LBT_FLAG_MODE_LBT  0x01u // LBT-mode parameters

enum {
  PRO2_GPIO_NONE = 0,
  PRO2_GPIO_0    = 1,
  PRO2_GPIO_1    = 2,
  PRO2_GPIO_2    = 3,
  PRO2_GPIO_3    = 4,
  PRO2_GPIO_MAX // Must be last
};
typedef int8_t Pro2Gpio; // Positive=enabled, Negative=disabled, 0=none

/*----------------------------------------------------------------------------*/
/*                             API prototypes                                 */
/*----------------------------------------------------------------------------*/
// From em250class/analogue.h
// These literals are used in the implementation of sli_802154_stack_set_tx_power_mode().
#define SL_ZIGBEE_TX_POWER_MODE_BOOST_BIT       0
#define SL_ZIGBEE_TX_POWER_MODE_ALTERNATE_BIT   1
// This option is exposed via the PHY_CONFIG token and via emberRadioSetPower():
#define SL_ZIGBEE_TX_POWER_MODE_EXT_PA_ON_BI_DIR_RF_PATH 0x0004
#define SL_ZIGBEE_TX_POWER_MODE_EXT_PA_ON_BI_DIR_RF_PATH_BIT 2
// but a non-default Tx Step Time is only exposed via the PHY_CONFIG token.
// This option is exposed via the PHY_CONFIG token and via emberRadioSetPower():
#define SL_ZIGBEE_TX_POWER_BOOST_LEVEL_BIT      3
#define SL_ZIGBEE_TX_POWER_BOOST_LEVEL_BITS     5

sl_status_t emRadioSetChannelAndForceCalibration(uint8_t channel);

// From em250class/phy.h
void sli_802154phy_set_coordinator(bool coordinator);
bool emPhyDeviceIsCoordinator(void);
void emRadioSetEdCcaThreshold(int8_t thresholddBm);
int8_t emRadioGetEdCcaThreshold(void);
uint16_t sli_802154phy_radio_unpack_chip_errors(uint8_t internalChipErrors);
uint8_t sli_802154phy_radio_calculate_link_quality(uint16_t unpackedChipErrors,
                                                   uint16_t packetLength);
uint8_t sli_802154phy_radio_convert_rssi_to_ed(int8_t rssiDbm);
RadioPowerMode sli_802154phy_radio_get_idle_mode(void);
void sli_802154phy_radio_ok_to_idle(void);
sl_status_t sli_802154phy_radio_set_idle_mode(RadioPowerMode mode);
int8_t sli_802154phy_radio_energy_detection(void);
uint32_t sli_802154phy_get_mac_timer(void);
void emRadioEnableOverflowNotification(bool enable);
bool emRadioOverflowNotificationEnabled(void);
void emRadioEnableSfdSentNotification(bool enable);
bool emRadioSfdSentNotificationEnabled(void);

bool emPhyIsTransmitting(void);
void emPhyCancelTransmit(void);

// From em3xx/phy.h
void sli_802154phy_radio_tx_ack_isr_callback(void);
void sli_zigbee_packet_received_internal_isr_callback(uint8_t *packet,
                                                      uint8_t length,
                                                      bool framePendingSetInTxAck);

int8_t emGetTempFromAdc(void);
bool emRadioIsReceivingFrame(void);

// New for pro2plus
sl_status_t emPhyGetDutyCycleParams(uint8_t macPgChan,
                                    EmPhyDutyCycleParams* dutyCycleParams);
sl_status_t emPhySetPhyMode(EmPhyMode phyMode);

//TODO: Provide these multi-network routines for real in MAC layer
#define emberRadioDataPendingLongIdPanIdIsrCallback(longId, panId) \
  emberRadioDataPendingLongIdIsrCallback(longId)
#define emberRadioDataPendingShortIdPanIdIsrCallback(shortId, panId) \
  emberRadioDataPendingShortIdIsrCallback(shortId)
void emRadioPowerDown(void);
void emRadioPowerUp(void);
sl_status_t emPhySetRadioChannelOnNetwork(uint8_t macPgChan,
                                          EmberNetworkIndex networkIndex);
sl_status_t emPhySetRadioPowerOnNetwork(int8_t dBmPower,
                                        EmberNetworkIndex networkIndex);
extern uint32_t emPhyGetLastTxCompleteTimeMs(bool lastCsmaTx); // For Duty Cycle
extern const uint8_t emPhyCcaCsmaConfigArray[];
extern const uint8_t emPhyCcaLbtConfigArray[];
extern const EmPhyConfig emPhyConfigMHz; // Sub-GHz band PHY
extern EmPhyBandConfig* emPhyCurrentBandConfig;
// Convenience macros to select default and current band config or default
// if current is not yet set
#define EM_PHY_DEFAULT_BAND_CONFIG (emPhyConfigMHz.bandConfigs[0])
#define EM_PHY_CURRENT_BAND_CONFIG ((emPhyCurrentBandConfig == NULL) \
                                    ? EM_PHY_DEFAULT_BAND_CONFIG     \
                                    : emPhyCurrentBandConfig)

// 'Internals' exposed for tests
uint8_t pro2GetChipRev(void);
const uint8_t* pro2GetDieName(void);
uint8_t pro2GetFunc(void);
uint32_t pro2GetFwRev(void);
uint32_t pro2GetFwSvn(void);
uint16_t pro2GetOtpId(void);
uint16_t pro2GetPartNum(void);
uint16_t pro2GetPatchRev(void);
uint8_t pro2GetRomId(void);
uint8_t pro2Peek(uint16_t addr);
void pro2Poke(uint16_t addr, uint8_t value);
uint8_t pro2ReadOneProperty(uint8_t propGrp, uint8_t prop);
void pro2WriteOneProperty(uint8_t propGrp, uint8_t prop, uint8_t value);
uint16_t pro2GetPtiLogging(void);
void pro2SetPtiLogging(uint16_t newPtiLogging);
void pro2SetRadioBootMode(uint8_t bootOptions);
uint8_t pro2GetRadioBootMode(void);
uint16_t pro2GetRadioTxPowerMode(void);
sl_status_t pro2GpioConfig(Pro2Gpio gpio, uint8_t config);
sl_status_t pro2SetRssiOffset(int8_t dB);
extern uint8_t emPhyPhr154gFeatures;

#endif// __PHY_PRO2CLASS_PHY_H__
