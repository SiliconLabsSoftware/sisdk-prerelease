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
#ifndef __PHY_TRX_SIM__
#define __PHY_TRX_SIM__

#include "phy/pro2class/phy-params-subGHz.h"

uint8_t sli_802154phy_radio_calculate_link_quality(uint16_t unpackedChipErrors,
                                                   uint16_t packetLength); //defined in em250class/phy.h
uint16_t sli_802154phy_radio_unpack_chip_errors(uint8_t internalChipErrors);  //defined in em250class/phy.h

// The offset of -154 converts approximately to dBm (per N. Horne),
// but the exact value may vary from chip to chip and with temperature.
#define RSSI_DBM_OFFSET       -154    //defined in em250class/phy.h

//used in stack/mac/802.15.4/scan.c
//declare in em250class/phy.h
RadioPowerMode sli_802154phy_radio_get_idle_mode(void);  //
sl_status_t sli_802154phy_radio_set_idle_mode(RadioPowerMode mode);
int8_t sli_802154phy_radio_energy_detection(void);
void sli_802154phy_radio_tx_ack_isr_callback(void);

//used in stack/core/zigbee-stack.c
//define in em250class/phy.h
void sli_802154phy_set_coordinator(bool coordinator);
bool emPhyDeviceIsCoordinator(void);

//used in stack/mac/802.15.4/em250class/mac-em250.c
//define in em250class/phy.h
void sli_802154phy_radio_ok_to_idle(void);

//stack/core/sl_zigbee_multi_network.c: In function sli_zigbee_network_descriptor_init:
//define in em250class/analogue.h
#undef  MIN_RADIO_POWER
#define MIN_RADIO_POWER   -43 // dBm
#undef  MAX_RADIO_POWER
#define MAX_RADIO_POWER    21 // dBm
#undef  ED_CCA_THRESHOLD
#define ED_CCA_THRESHOLD  -75 // dBm

void emRadioResetAndEnableReceive(void);
void sli_zigbee_packet_received_internal_isr_callback(uint8_t *packet,
                                                      uint8_t length,
                                                      bool framePendingSetInTxAck);

// SIMULATED_TRANSCEIVER is phy agnostic but PHY_HAS_DUTY_CYCLE causes to
// pulled in some code from hardware-mac-sim which do not applicable to
// transceiver hence excluding PHY_HAS_DUTY_CYCLE for it.
#ifndef PHY_HAS_DUTY_CYCLE
#ifndef SIMULATED_TRANSCEIVER
#define PHY_HAS_DUTY_CYCLE 1    // This PHY has duty cycle requirements
#endif// SIMULATED_TRANSCEIVER
#endif//PHY_HAS_DUTY_CYCLE

#define EM_PHY_LBT_FLAG_MODE_MASK 0x01u // Mask for the parameter's mode
#define EM_PHY_LBT_FLAG_MODE_CSMA 0x00u // CSMA-mode parameters
#define EM_PHY_LBT_FLAG_MODE_LBT  0x01u // LBT-mode parameters

sl_status_t emPhyGetDutyCycleParams(uint8_t macPgChan,
                                    EmPhyDutyCycleParams* dutyCycleParams);

extern const uint8_t emPhyCcaLbtConfigArray[];
extern const EmPhyConfig emPhyConfigMHz; // Sub-GHz band PHY
extern EmPhyBandConfig* emPhyCurrentBandConfig;
// Convenience macros to select default and current band config or default
// if current is not yet set
#define EM_PHY_DEFAULT_BAND_CONFIG (emPhyConfigMHz.bandConfigs[0])
#define EM_PHY_CURRENT_BAND_CONFIG ((emPhyCurrentBandConfig == NULL) \
                                    ? EM_PHY_DEFAULT_BAND_CONFIG     \
                                    : emPhyCurrentBandConfig)

extern uint32_t emPhyGetLastTxCompleteTimeMs(bool lastCsmaTx); // For Duty Cycle

#endif //__PHY_TRX_SIM__
