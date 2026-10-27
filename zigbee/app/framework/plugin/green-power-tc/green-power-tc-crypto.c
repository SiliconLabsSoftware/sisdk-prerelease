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

#include "af.h"
#include "green-power-tc-crypto.h"
#include "stack/include/gp-install-code.h"
#include "stack/include/zigbee-security-manager.h"
#include "stack/include/zigbee_packet_types.h"

#ifndef SECURITY_BLOCK_SIZE
#define SECURITY_BLOCK_SIZE 16
#endif

#define NONCE_SECURITY_CONTROL                 0x05
#define NONCE_SECURITY_CONTROL_APP_ID2_OUTGOING 0xC5

static void import_load_zb_sec_man_internal_key(sl_zigbee_sec_man_key_t *key)
{
  sl_zigbee_sec_man_context_t context;
  sl_zigbee_sec_man_init_context(&context);
  context.core_key_type = SL_ZB_SEC_MAN_KEY_TYPE_INTERNAL;
  sl_zigbee_sec_man_import_key(&context, key);
  sl_zigbee_sec_man_load_key_context(&context);
}

static void sli_gp_tc_initialise_key_derivation_nonce(uint8_t *nonce,
                                                       const sl_zigbee_gp_address_t *gpdAddr,
                                                       uint32_t gpdSecurityFrameCounter,
                                                       bool directionIncoming)
{
  memset(nonce, 0, SECURITY_BLOCK_SIZE);

  if (gpdAddr->applicationId == SL_ZIGBEE_GP_APPLICATION_IEEE_ADDRESS) {
    memmove(nonce, gpdAddr->id.gpdIeeeAddress, EUI64_SIZE);
  } else if (directionIncoming) {
    sl_util_store_low_high_int32u(&(nonce[STANDALONE_NONCE_SOURCE_ADDR_INDEX]), gpdAddr->id.sourceId);
    sl_util_store_low_high_int32u(&(nonce[STANDALONE_NONCE_SOURCE_ADDR_INDEX + 4]), gpdAddr->id.sourceId);
  } else {
    sl_util_store_low_high_int32u(&(nonce[STANDALONE_NONCE_SOURCE_ADDR_INDEX]), 0);
    sl_util_store_low_high_int32u(&(nonce[STANDALONE_NONCE_SOURCE_ADDR_INDEX + 4]), gpdAddr->id.sourceId);
  }

  if (directionIncoming) {
    if (gpdAddr->applicationId == SL_ZIGBEE_GP_APPLICATION_IEEE_ADDRESS) {
      memmove(nonce + STANDALONE_NONCE_FRAME_COUNTER_INDEX, gpdAddr->id.gpdIeeeAddress, 4);
    } else {
      sl_util_store_low_high_int32u(&nonce[STANDALONE_NONCE_FRAME_COUNTER_INDEX], gpdAddr->id.sourceId);
    }
  } else {
    sl_util_store_low_high_int32u(&nonce[STANDALONE_NONCE_FRAME_COUNTER_INDEX], gpdSecurityFrameCounter);
  }

  if (gpdAddr->applicationId == SL_ZIGBEE_GP_APPLICATION_IEEE_ADDRESS && !directionIncoming) {
    nonce[STANDALONE_NONCE_SECURITY_CONTROL_INDEX] = NONCE_SECURITY_CONTROL_APP_ID2_OUTGOING;
  } else {
    nonce[STANDALONE_NONCE_SECURITY_CONTROL_INDEX] = NONCE_SECURITY_CONTROL;
  }
}

static bool sli_gp_tc_key_tc_lk_transform(const sl_zigbee_gp_address_t *gpdAddr,
                                           uint32_t gpdSecurityFrameCounter,
                                           sl_zigbee_key_data_t *key,
                                           bool directionIncoming,
                                           uint8_t mic[4])
{
  uint8_t nonce[SECURITY_BLOCK_SIZE] = { 0 };
  uint8_t payload[20] = { 0 };
  uint8_t temp[24] = { 0 };

  sli_gp_tc_initialise_key_derivation_nonce(nonce,
                                             gpdAddr,
                                             gpdSecurityFrameCounter,
                                             directionIncoming);

  if (gpdAddr->applicationId == SL_ZIGBEE_GP_APPLICATION_IEEE_ADDRESS) {
    memmove(payload, gpdAddr->id.gpdIeeeAddress, 4);
  } else {
    sl_util_store_low_high_int32u(payload, gpdAddr->id.sourceId);
  }

  if (directionIncoming) {
    sl_zigbee_sec_man_aes_ccm(nonce, true, key->contents, 0, SL_ZIGBEE_ENCRYPTION_KEY_SIZE, temp);
    memcpy(payload + 4, temp, SL_ZIGBEE_ENCRYPTION_KEY_SIZE);
    memcpy(key->contents, temp, SL_ZIGBEE_ENCRYPTION_KEY_SIZE);
  } else {
    memcpy(payload + 4, key->contents, SL_ZIGBEE_ENCRYPTION_KEY_SIZE);
    sl_zigbee_sec_man_aes_ccm(nonce, true, key->contents, 0, SL_ZIGBEE_ENCRYPTION_KEY_SIZE, temp);
    memcpy(key->contents, temp, SL_ZIGBEE_ENCRYPTION_KEY_SIZE);
  }

  sl_zigbee_sec_man_aes_ccm(nonce, true, payload, 4, 20, temp);
  memcpy(mic, &temp[20], 4);
  return true;
}

bool sli_gp_tc_decrypt_gpd_key(const sl_zigbee_gp_address_t *gpdAddr,
                                uint32_t frameCounter,
                                const sl_zigbee_key_data_t *linkKey,
                                sl_zigbee_key_data_t *keyInOut,
                                uint32_t expectedMic)
{
  uint8_t mic[4] = { 0 };
  uint32_t calculatedMic;

  if (gpdAddr == NULL || linkKey == NULL || keyInOut == NULL) {
    return false;
  }

  import_load_zb_sec_man_internal_key((sl_zigbee_sec_man_key_t *)linkKey);
  if (!sli_gp_tc_key_tc_lk_transform(gpdAddr, frameCounter, keyInOut, true, mic)) {
    return false;
  }

  calculatedMic = sl_util_fetch_low_high_int32u(mic);
  return (calculatedMic == expectedMic);
}

bool sli_gp_tc_encrypt_gpd_key(const sl_zigbee_gp_address_t *gpdAddr,
                                uint32_t frameCounter,
                                const sl_zigbee_key_data_t *linkKey,
                                sl_zigbee_key_data_t *keyInOut,
                                uint32_t *micOut)
{
  uint8_t mic[4] = { 0 };

  if (gpdAddr == NULL || linkKey == NULL || keyInOut == NULL) {
    return false;
  }

  import_load_zb_sec_man_internal_key((sl_zigbee_sec_man_key_t *)linkKey);
  if (!sli_gp_tc_key_tc_lk_transform(gpdAddr, frameCounter, keyInOut, false, mic)) {
    return false;
  }

  if (micOut != NULL) {
    *micOut = sl_util_fetch_low_high_int32u(mic);
  }
  return true;
}
