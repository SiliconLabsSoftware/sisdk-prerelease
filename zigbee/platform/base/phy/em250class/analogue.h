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
#ifndef __EM250_ANALOGUE_H__
#define __EM250_ANALOGUE_H__

#ifndef __PHY_H__
#error do not include this file directly - include phy/phy.h
#endif//__PHY_H__

// Most analogue indirect register and subfield names changed from the 250 to
// the 350, while most of the functionality remained the same.  The portions of
// this module that support both the 250 and the 350 reference the 350 names.
// This header file translates 350 names to 250 names.
#include "hal/micro/unix/simulation/em250/regs-indirect-analogue-translation.h"

#define MIN_RADIO_POWER      -43 // dBm
#define MAX_RADIO_POWER        3 // dBm
#ifdef PHY_SIMULATION_DUAL
#define INVALID_RADIO_POWER   SL_ZIGBEE_PHY_INVALID_RSSI // -128
#endif  //PHY_SIMULATION_DUAL

#define SYNTH_FREQ_1MHZ        0x0055 // Each 1MHz corresponds to this reg value

#define CAL_DATA_CAL_NEEDED_MASK 0x80

// This structure contains the constant data associated with each channel.
typedef struct channelInfoTag {
  uint16_t synthFreqL; // Low word of synthesizer frequency
  uint8_t vcoTuneOffset; // Offset from channel zero value for TUNE_VCO in VCO_H
} channelInfoType;

#ifdef   CACHE_SERIAL_REG_DATA

#define NUM_SERIAL_REGS 17

#define IFFILTER_H_INDEX   0
#define IFFILTER_L_INDEX   1
#define LNA_INDEX          2
#define IFAMP_INDEX        3
#define RXADC_H_INDEX      4
#define RXADC_L_INDEX      5
#define REVMARK_INDEX      6
#define PRESCALER_INDEX    7
#define PHDET_INDEX        8
#define VCO_INDEX          9
#define LOOPFILTER_INDEX  10
#define PA_INDEX          11
#define MODDAC_INDEX      12
#define AUXADC_INDEX      13
#define CALADC_INDEX      14
#define IQMIXER_INDEX     15
#define BIAS_MASTER_INDEX 16

#define IFFILTER_H_ID  IFFILTER_H_INDEX
#define IFFILTER_L_ID  IFFILTER_L_INDEX
#define LNA_ID         LNA_INDEX
#define IFAMP_ID       IFAMP_INDEX
#define RXADC_H_ID     RXADC_H_INDEX
#define RXADC_L_ID     RXADC_L_INDEX
#define REVMARK_ID     REVMARK_INDEX
#define PRESCALER_ID   PRESCALER_INDEX
#define PHDET_ID       PHDET_INDEX
#define VCO_ID         VCO_INDEX
#define LOOPFILTER_ID  LOOPFILTER_INDEX
#define PA_ID          PA_INDEX
#define MODDAC_ID      MODDAC_INDEX
#define AUXADC_ID      AUXADC_INDEX
#define CALADC_ID      CALADC_INDEX
#define IQMIXER_ID     IQMIXER_INDEX
#define BIAS_MASTER_ID BIAS_MASTER_INDEX

#else //!CACHE_SERIAL_REG_DATA

#define IFFILTER_H_ID  IFFILTER_H_REG
#define IFFILTER_L_ID  IFFILTER_L_REG
#define LNA_ID         LNA_REG
#define IFAMP_ID       IFAMP_REG
#define RXADC_H_ID     RXADC_H_REG
#define RXADC_L_ID     RXADC_L_REG
#define REVMARK_ID     REVMARK_REG
#define PRESCALER_ID   PRESCALER_REG
#define PHDET_ID       PHDET_REG
#define VCO_ID         VCO_REG
#define LOOPFILTER_ID  LOOPFILTER_REG
#define PA_ID          PA_REG
#define MODDAC_ID      MODDAC_REG
#define AUXADC_ID      AUXADC_REG
#define CALADC_ID      CALADC_REG
#define IQMIXER_ID     IQMIXER_REG
#define BIAS_MASTER_ID BIAS_MASTER_REG

#endif//!CACHE_SERIAL_REG_DATA

// emRadioReadSerialReg
// This function reads data from an analog control register.
// These registers are accessed via the serial control bus.
// Only bits specified by mask parameter are read.
// The value is right shifted as needed before being returned.
uint16_t emRadioReadSerialReg(uint16_t id,
                              uint16_t bit,
                              uint16_t mask);

// emRadioWriteSerialReg
// This function writes data to an analog control register.
// These registers are accessed via the serial control bus.
// Only bits specified by mask parameter are changed,
// other existing register contents are preserved.
void emRadioWriteSerialReg(uint16_t id,
                           uint16_t value,
                           uint16_t mask);

// Analog Calibration Status Register Flags.
// VCO control line window detector thresholds.
// We want the tune line to be between the thresholds.
// window and synth lock are read 8 times on the emulator to provide some
// glitch filtering in software
#define ABOVE_VCO_TUNE_WINDOW ((BASEBAND->ANCALSTATUS &  0x0008) || (BASEBAND->ANCALSTATUS &  0x0008)    \
                               || (BASEBAND->ANCALSTATUS &  0x0008) || (BASEBAND->ANCALSTATUS &  0x0008) \
                               || (BASEBAND->ANCALSTATUS &  0x0008) || (BASEBAND->ANCALSTATUS &  0x0008) \
                               || (BASEBAND->ANCALSTATUS &  0x0008) || (BASEBAND->ANCALSTATUS &  0x0008))
#define VCO_SETTLE_TIME_MICROSECONDS 100
#define BELOW_VCO_TUNE_WINDOW (!(BASEBAND->ANCALSTATUS &  0x0004))
#define IN_VCO_TUNE_WINDOW    ((BASEBAND->ANCALSTATUS == 0x0004))

#define VCO_AUTO_TUNE_TIME_MICROSECONDS  1000

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

// emRadioSetChannelAndForceCalibration
// This function is identical to sli_802154phy_set_phy_radio_channel except that it calibrates
// all radio parameters regardless of whether calibration is required.
// This function does not ever need to be called under normal operating
// conditions.  It is available to recover from problems that may occur during
// hardware development resulting in incorrect calibration data being stored in
// tokens.
sl_status_t emRadioSetChannelAndForceCalibration(uint8_t channel);

void emStartCalibrateCurrentChannel(void);
void emFinishCalibrateCurrentChannel(void);

void emStartRadioSetChannel(void);
sl_status_t emFinishRadioSetChannel(uint8_t channel);

extern sl_zigbee_event_control_t emCalPadsEvent;
void emCalPadsEventHandler(void);

#endif//__EM250_ANALOGUE_H__
