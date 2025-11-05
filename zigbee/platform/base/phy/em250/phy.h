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
#include "phy/em250class/phy.h"
#include "phy/em250class/analogue.h"
#include "hal/micro/unix/simulation/em250/regs-indirect-analog.h"

#define ED_CCA_THRESHOLD -58 // dBm.  We've always used this value on the 250.

#define MANUALVCO()
#define AUTOVCO()
#define OPENVCO()  emRadioWriteSerialReg(LOOPFILTER_ID,                      \
                                         1 << LOOP_FILTER_OPEN_VCO_LOOP_BIT, \
                                         LOOP_FILTER_OPEN_VCO_LOOP_MASK)
#define CLOSEVCO() emRadioWriteSerialReg(LOOPFILTER_ID,                      \
                                         0 << LOOP_FILTER_OPEN_VCO_LOOP_BIT, \
                                         LOOP_FILTER_OPEN_VCO_LOOP_MASK)
// The following emRadioReadSerialReg calls do not use the "id" indirection.
// The register address interface is hardcoded.
#ifdef   CACHE_SERIAL_REG_DATA
#error   Cached serial registers not supported on this phy.
#endif// CACHE_SERIAL_REG_DATA
#define READVCO()  emRadioReadSerialReg(VCO_H_REG,          \
                                        VCO_H_TUNE_VCO_BIT, \
                                        VCO_H_TUNE_VCO_MASK)

#define MAC_TX_INT_SRC_RX_ACK_COMMON (MAC_TX_INT_SRC_RX_ACK_SRC)

// parameters for Vref calibration
// Vdd/2 in mV units (Vdd is 1.72V)
#define VDD_2_MV            860
#define VREF_MV             1200
#define VREF_CAL_TIME       ADC_CONVERSION_TIME_US_256
#define VREF_CAL_VGND       ADC_SOURCE_VGND
#define VREF_CAL_VDD_2      ADC_SOURCE_VDD_GND
#define VREF_CAL_VREF       ADC_SOURCE_VREF

#define MAC_TX_INTS (0                                             \
                     | MAC_TX_INT_MASK_TX_COMPLETE_MSK_MASK        \
                     /* | MAC_TX_INT_MASK_RX_ACK_MSK_MASK       */ \
                     /* | MAC_TX_INT_MASK_BO_COMPLETE_MSK_MASK  */ \
                     /* | MAC_TX_INT_MASK_SFD_SENT_MSK_MASK     */ \
                     | MAC_TX_INT_MASK_CCA_FAIL_MSK_MASK           \
                     | MAC_TX_INT_MASK_TX_UNDERFLOW_MSK_MASK       \
                     | MAC_TX_INT_MASK_TX_LOCK_FAIL_MSK_MASK       \
                     | MAC_TX_INT_MASK_ACK_EXPIRED_MSK_MASK        \
                     /* | MAC_TX_INT_MASK_TX_A_UNLOAD_MSK_MASK  */ \
                     /* | MAC_TX_INT_MASK_TX_B_UNLOAD_MSK_MASK  */ \
                     )

#define MAC_RX_INTS (0                                                 \
                     | MAC_RX_INT_MASK_RX_COMPLETE_MSK_MASK            \
                     | MAC_RX_INT_MASK_TX_ACK_MSK_MASK                 \
                     /* | MAC_RX_INT_MASK_RX_FRAME_MSK_MASK         */ \
                     /* | MAC_RX_INT_MASK_RX_FILT_COMP_MSK_MASK     */ \
                     | MAC_RX_INT_MASK_RX_ADDR_REC_COMP_MSK_MASK       \
                     /* | MAC_RX_INT_MASK_RX_ERROR_MSK_MASK         */ \
                     /* | MAC_RX_INT_MASK_BB_RX_LEN_ERR_MSK_MASK    */ \
                     | MAC_RX_INT_MASK_RX_OVFLW_MSK_MASK               \
                     /* | MAC_RX_INT_MASK_TX_ACK_ERR_MSK_MASK       */ \
                     | MAC_RX_INT_MASK_TX_COLL_RX_MSK_MASK             \
                     | MAC_RX_INT_MASK_RX_ROLLBACK_MSK_MASK            \
                     | MAC_RX_INT_MASK_RX_A_UNLOAD_MSK_MASK            \
                     | MAC_RX_INT_MASK_RX_B_UNLOAD_MSK_MASK            \
                     )

// This block is required for LBT and duty cycle support in simulation
#if defined(PHY_SIMULATION_DUAL) || defined(PHY_SIMULATION_GB)
extern const uint8_t emPhyCcaLbtConfigArray[];
extern const EmPhyConfig emPhyConfigEfr;   // EFR32 band PHY
extern EmPhyBandConfig* emPhyCurrentBandConfig;

// Convenience macros to select default and current band config or default
// if current is not yet set
  #ifdef PHY_SIMULATION_DUAL
// Let PRO2CLASS definition rule on PHY_SIMULATION_DUAL test app builds
  #else//!PHY_SIMULATION_DUAL
  #define EM_PHY_DEFAULT_BAND_CONFIG (emPhyConfigEfr.bandConfigs[0])
  #endif//PHY_SIMULATION_DUAL
  #define EM_PHY_CURRENT_BAND_CONFIG ((emPhyCurrentBandConfig == NULL) \
                                      ? EM_PHY_DEFAULT_BAND_CONFIG     \
                                      : emPhyCurrentBandConfig)

  #ifndef PHY_HAS_DUTY_CYCLE
  #define PHY_HAS_DUTY_CYCLE (IEEE802154_GB868_SUPPORTED \
                              || (PHY_SIMULATION_DUAL && (PHY_THIS == PHY_THIS_UNSPECIFIED)))
  #endif//PHY_HAS_DUTY_CYCLE

  #define EM_PHY_LBT_FLAG_MODE_MASK 0x01u // Mask for the parameter's mode
  #define EM_PHY_LBT_FLAG_MODE_CSMA 0x00u // CSMA-mode parameters
  #define EM_PHY_LBT_FLAG_MODE_LBT  0x01u // LBT-mode parameters

sl_status_t emPhyGetDutyCycleParams(uint8_t macPgChan,
                                    EmPhyDutyCycleParams* dutyCycleParams);
extern uint32_t emPhyGetLastTxCompleteTimeMs(bool lastCsmaTx);   // For Duty Cycle
#endif  // PHY_SIMULATION_DUAL || PHY_SIMULATION_GB
