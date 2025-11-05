/*
 * File: radio-internal-config.h
 * Description: This file contains the properties that we don't want to be
 *   generated using WDS3. This properties are the interrupt settings, the
 *   fast register read settings, the RSSI setting and the packet handler
 *   settings.
 *
 * Copyright 2013 by Silicon Laboratories. All rights reserved.
 */

#ifndef __RADIO_INTERNAL_CONFIG_H__
#define __RADIO_INTERNAL_CONFIG_H__

#define PRO2_RX_FIFO_SIZE (2 + 127 + 8) // Can hold full 15.4g frame + appended info
#define PRO2_TX_FIFO_SIZE (127)     // In theory can hold 16 more, but ...
#define PRO2_RX_FIFO_THRESHOLD 0x30 // Default seems fine for now
#define PRO2_TX_FIFO_THRESHOLD 0x30 // Default seems fine for now

#if     (PRO2_FW_API >= 0x06000500ul) //FIXME: Remove when PRO2+ has fixed CRC defn's
#define PH_CRC_CONFIG
#else//!(PRO2_FW_API >= 0x06000500ul) //FIXME: Remove when PRO2+ has fixed CRC defn's
#define PH_CRC_CONFIG                                                               \
  CMD_SET_PROPERTY,                                                                 \
  PROP_GRP_ID_PKT,                                                                  \
  1,                                                                                \
  PROP_PKT_CRC_CONFIG_OFFSET,                                                       \
  (0                                                                                \
/*Configure normal CRC engine for IEEE_802_3 and ALT CRC engine for CCITT-16*/      \
   | (PROP_PKT_CRC_CONFIG_ALT_CRC_POLYNOMIAL_ENUM_CCITT_16                          \
      << PROP_PKT_CRC_CONFIG_ALT_CRC_POLYNOMIAL_LSB)                                \
   | (PROP_PKT_CRC_CONFIG_CRC_POLYNOMIAL_ENUM_IEEE_802_3                            \
      << PROP_PKT_CRC_CONFIG_CRC_POLYNOMIAL_LSB)                                    \
   | (PROP_PKT_CRC_CONFIG_CRC_SEED_ENUM_CRC_SEED_1                                  \
      << PROP_PKT_CRC_CONFIG_CRC_SEED_LSB)                                          \
  ),                                                                                \
/*PROP_PKT_CONFIG1_CRC_INVERT_write $::PROP_PKT_CONFIG1_CRC_INVERT_ENUM_NO_INVERT*/ \
  CMD_SET_PROPERTY,                                                                 \
  PROP_GRP_ID_PKT,                                                                  \
  1,                                                                                \
  PROP_PKT_CONFIG1_OFFSET,                                                          \
  (0                                                                                \
   | (PROP_PKT_CONFIG1_CRC_INVERT_ENUM_NO_INVERT /* See SI4440-1342 here */         \
      << PROP_PKT_CONFIG1_CRC_INVERT_LSB)                                           \
   | (PROP_PKT_CONFIG1_CRC_ENDIAN_ENUM_MSBYTE_FIRST                                 \
      << PROP_PKT_CONFIG1_CRC_ENDIAN_LSB)                                           \
   | (PROP_PKT_CONFIG1_BIT_ORDER_ENUM_LSBIT_FIRST                                   \
      << PROP_PKT_CONFIG1_BIT_ORDER_LSB)                                            \
  ),                                                                                \
  CMD_SET_PROPERTY,                                                                 \
  PROP_GRP_ID_PKT,                                                                  \
  1,                                                                                \
  PROP_PKT_CONFIG2_OFFSET,                                                          \
  (0                                                                                \
   | (PROP_PKT_CONFIG2_CRC_BIT_ENDIAN_ENUM_MSBIT_FIRST                              \
      << PROP_PKT_CONFIG2_CRC_BIT_ENDIAN_LSB)                                       \
   | (PROP_PKT_CONFIG2_CRC_PADDING_ENUM_PAD_ZEROS                                   \
      << PROP_PKT_CONFIG2_CRC_PADDING_LSB)                                          \
   | (PROP_PKT_CONFIG2_ALT_CRC_SEED_ENUM_ALT_CRC_SEED_0                             \
      << PROP_PKT_CONFIG2_ALT_CRC_SEED_LSB)                                         \
  ),
#endif//(PRO2_FW_API >= 0x06000500ul) //FIXME: Remove when PRO2+ has fixed CRC defn's

#if     (PRO2_FW_API >= 0x06000500ul) //FIXME: Remove when PRO2+ has fixed WHT defn's
#define PH_WHT_CONFIG
#else//!(PRO2_FW_API >= 0x06000500ul) //FIXME: Remove when PRO2+ has fixed WHT defn's
#define PH_WHT_CONFIG                                          \
  CMD_SET_PROPERTY,                                            \
  PROP_GRP_ID_PKT,                                             \
  3,                                                           \
  /* Can leave default PN9 choice for PKT_WHT_POLY */          \
  PROP_PKT_WHT_SEED_OFFSET,                                    \
  /* Configure initial seed to be 0x010F per PRO2 "feature" */ \
  0x01,                                                        \
  0x0F,                                                        \
  (0                                                           \
   /* Configure forward direction (>= C2 ROM6) and bit 7 */    \
   | (PROP_PKT_WHT_BIT_NUM_PN_DIRECTION_ENUM_FORWARD           \
      << PROP_PKT_WHT_BIT_NUM_PN_DIRECTION_LSB)                \
   | (PROP_PKT_WHT_BIT_NUM_WHT_BIT_NUM_ENUM_ENUM_7             \
      << PROP_PKT_WHT_BIT_NUM_WHT_BIT_NUM_LSB)                 \
  ),
#endif//(PRO2_FW_API >= 0x06000500ul) //FIXME: Remove when PRO2+ has fixed WHT defn's

#define FRR_CONFIG                                         \
  CMD_SET_PROPERTY,                                        \
  PROP_GRP_ID_FRR_CTL,                                     \
  1,                                                       \
  PROP_FRR_CTL_A_MODE_OFFSET,                              \
  PROP_FRR_CTL_A_MODE_FRR_A_MODE_ENUM_INT_IE154_RX_STATUS, \
                                                           \
  CMD_SET_PROPERTY,                                        \
  PROP_GRP_ID_FRR_CTL,                                     \
  1,                                                       \
  PROP_FRR_CTL_B_MODE_OFFSET,                              \
  PROP_FRR_CTL_B_MODE_FRR_B_MODE_ENUM_INT_IE154_RX_PEND,   \
                                                           \
  CMD_SET_PROPERTY,                                        \
  PROP_GRP_ID_FRR_CTL,                                     \
  1,                                                       \
  PROP_FRR_CTL_C_MODE_OFFSET,                              \
  PROP_FRR_CTL_C_MODE_FRR_C_MODE_ENUM_CURRENT_STATE,       \
                                                           \
  CMD_SET_PROPERTY,                                        \
  PROP_GRP_ID_FRR_CTL,                                     \
  1,                                                       \
  PROP_FRR_CTL_D_MODE_OFFSET,                              \
  PROP_FRR_CTL_D_MODE_FRR_D_MODE_ENUM_LATCHED_RSSI,        \
  /* Please don't remove this comment! */

// Keep above in sync with below!
#define CMD_FRR_READ_INT_IE154_RX_STATUS  CMD_FRR_A_READ
#define CMD_FRR_READ_INT_IE154_RX_PEND    CMD_FRR_B_READ
#define CMD_FRR_READ_CURRENT_STATE        CMD_FRR_C_READ
#define CMD_FRR_READ_LATCHED_RSSI         CMD_FRR_D_READ

#define RADIO_INTERNAL_CONFIGURATION_DATA_ARRAY { \
    PH_CRC_CONFIG                                 \
    PH_WHT_CONFIG                                 \
      FRR_CONFIG                                  \
    0x00                                          \
}

#endif //__RADIO_INTERNAL_CONFIG_H__
