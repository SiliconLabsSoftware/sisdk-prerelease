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
#ifndef __PHY_RAIL_PHY_H__
#define __PHY_RAIL_PHY_H__

#include "rail.h"

#ifndef IEEE802154_GB868_SUPPORTED
#define IEEE802154_GB868_SUPPORTED (PHY_RAILGB || PHY_RAILGB_MP)
#endif//IEEE802154_GB868_SUPPORTED

#ifndef PHY_HAS_DUTY_CYCLE
#define PHY_HAS_DUTY_CYCLE (IEEE802154_GB868_SUPPORTED \
                            || (PHY_DUAL && (PHY_THIS == PHY_THIS_UNSPECIFIED)))
#endif//PHY_HAS_DUTY_CYCLE

#define EM_PHY_LBT_FLAG_MODE_MASK 0x01u // Mask for the parameter's mode
#define EM_PHY_LBT_FLAG_MODE_CSMA 0x00u // CSMA-mode parameters
#define EM_PHY_LBT_FLAG_MODE_LBT  0x01u // LBT-mode parameters

// These are borrowed from ieee802154_phy.h
#define XTALFREQ_KHZ                         38400UL
#define SYNTH_RF_FREQUENCY_24GHZ        2405000000UL
#define SYNTH_RF_FREQUENCY_868MHZ        868300000UL
#define SYNTH_RF_FREQUENCY_915MHZ        906000000UL
#define SYNTH_RF_FREQUENCY_GB868_868MHZ  863250000UL
#define SYNTH_RF_FREQUENCY_GB868_915MHZ  915350000UL
#define SYNTH_CHANNEL_SPACING_24GHZ        5000000UL
#define SYNTH_CHANNEL_SPACING_868MHZ       1000000UL
#define SYNTH_CHANNEL_SPACING_915MHZ       2000000UL
#define SYNTH_CHANNEL_SPACING_GB868        200000UL

#define MIN_RADIO_POWER  -43 // dBm
#define MAX_RADIO_POWER   20 // dBm
#define INIT_RADIO_POWER   6 // dBm

#define ED_CCA_THRESHOLD_UNINIT         0x7F
#define ED_CCA_THRESHOLD_SUBBAND        (ED_CCA_THRESHOLD_UNINIT - 1)
#define ED_CCA_THRESHOLD_MINIMUM        -128 // CCA's must fail at this level
#define ED_CCA_THRESHOLD -75 // dBm
#define RSSI_DBM_OFFSET_RAIL  0   // Assume RAIL RSSI values are accurate

// EFR32 benefits from having a distinct mode for Tx and ED
// This definition overrides enum in phy/phy.h
#define SL_ZIGBEE_RADIO_POWER_MODE_ED_ON (SL_ZIGBEE_RADIO_POWER_MODE_OFF + 1)
#define SL_ZIGBEE_RADIO_POWER_MODE_TX_ON (SL_ZIGBEE_RADIO_POWER_MODE_OFF + 2)

// EFR32 sli_802154_stack_set_tx_power_mode() value to not override per-band config settings
#define SL_ZIGBEE_TX_POWER_MODE_PER_BAND_CONFIG       (0xFFFFu)
#define SL_ZIGBEE_TX_POWER_MODE_BAND_NO_LIMITS_MASK   (0x00C0u)
#define SL_ZIGBEE_TX_POWER_MODE_BAND_NO_LIMITS_VALUE  (0x0080u)

#if     IEEE802154_GB868_SUPPORTED

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
  #define SL_ZIGBEE_PHY_RAMP_TIME_BITS     2        // Ramp up or down time,  ~8 us
  #define SL_ZIGBEE_PHY_RAMP_SETTLE_BITS   4        // Settle after ramp up, ~16 us

#endif//IEEE802154_GB868_SUPPORTED

extern RAIL_Handle_t emPhyRailHandle;
extern const uint8_t emPhyCcaCsmaConfigArray[];
extern const uint8_t emPhyCcaLbtConfigArray[];
extern const EmPhyConfig emPhyConfigEfr; // EFR32 band PHY
extern EmPhyBandConfig* emPhyCurrentBandConfig;
// Convenience macros to select default and current band config or default
// if current is not yet set
#if     (PHY_DUAL && (PHY_THIS != PHY_THIS_NATIVE))
// Let PRO2CLASS definition rule on PHY_DUAL test app builds
#else//!(PHY_DUAL && (PHY_THIS != PHY_THIS_NATIVE))
#define EM_PHY_DEFAULT_BAND_CONFIG (emPhyConfigEfr.bandConfigs[0])
#endif//(PHY_DUAL && (PHY_THIS != PHY_THIS_NATIVE))
#define EM_PHY_CURRENT_BAND_CONFIG ((emPhyCurrentBandConfig == NULL) \
                                    ? EM_PHY_DEFAULT_BAND_CONFIG     \
                                    : emPhyCurrentBandConfig)

void sli_802154phy_set_coordinator(bool coordinator);
bool emPhyDeviceIsCoordinator(void);

void emRadioSetEdCcaThreshold(int8_t threshold);
int8_t emRadioGetEdCcaThreshold(void);

uint16_t sli_802154phy_radio_unpack_chip_errors(uint8_t internalChipErrors);
uint8_t sli_802154phy_radio_calculate_link_quality(uint16_t unpackedChipErrors,
                                                   uint16_t packetLength);
uint8_t sli_802154phy_radio_convert_rssi_to_ed(int8_t rssiDbm);
int8_t sli_802154phy_radio_energy_detection(void);

uint32_t sli_802154phy_get_mac_timer(void);

void sli_802154phy_radio_tx_ack_isr_callback(void);
void sli_zigbee_packet_received_internal_isr_callback(uint8_t *packet,
                                                      uint8_t length,
                                                      bool framePendingSetInTxAck);

// Called by the PHY to implement the radio off when idle function.
// Note:  doesn't check to see if we are in the middle of a receive.
void sli_802154phy_radio_ok_to_idle(void);

// Returns the state that the radio will be in when idle.
RadioPowerMode sli_802154phy_radio_get_idle_mode(void);

// Sets the state that the radio will be in when idle.
// Note:  Applies state immediately (will wake up or sleep the radio).
sl_status_t sli_802154phy_radio_set_idle_mode(RadioPowerMode mode);

sl_status_t emRadioSetChannelAndForceCalibration(uint8_t channel);

bool emRadioIsReceivingFrame(void);
bool emPhyIsTransmitting(void);
void emPhyCancelTransmit(void);
void emRadioEnableOverflowNotification(bool enable);
bool emRadioOverflowNotificationEnabled(void);
void emberRadioOverflowIsrCallback(void);
void emberRadioSfdSentIsrCallback(uint32_t sfdSentTime);
void emRadioEnableSfdSentNotification(bool enable);
bool emRadioSfdSentNotificationEnabled(void);

sl_status_t emPhyGetDutyCycleParams(uint8_t macPgChan,
                                    EmPhyDutyCycleParams* dutyCycleParams);
uint32_t emPhyGetLastTxCompleteTimeMs(bool lastCsmaTx); // For Duty Cycle

#endif//__PHY_RAIL_PHY_H__
