/***************************************************************************//**
 * @file
 * @brief GP-aware TC key decrypt/encrypt helpers (GP v1.1.3).
 *******************************************************************************
 * # License
 * <b>Copyright 2026 Silicon Laboratories Inc. www.silabs.com</b>
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

#ifndef GREEN_POWER_TC_CRYPTO_H
#define GREEN_POWER_TC_CRYPTO_H

#include "green-power-tc-msg.h"
#include "green-power-v113.h"

bool sli_gp_tc_decrypt_gpd_key(const sl_zigbee_gp_address_t *gpdAddr,
                                uint32_t frameCounter,
                                const sl_zigbee_key_data_t *linkKey,
                                sl_zigbee_key_data_t *keyInOut,
                                uint32_t expectedMic);

bool sli_gp_tc_encrypt_gpd_key(const sl_zigbee_gp_address_t *gpdAddr,
                                uint32_t frameCounter,
                                const sl_zigbee_key_data_t *linkKey,
                                sl_zigbee_key_data_t *keyInOut,
                                uint32_t *micOut);

#endif // GREEN_POWER_TC_CRYPTO_H
