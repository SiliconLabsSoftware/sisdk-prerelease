/***************************************************************************//**
 * @file
 * @brief OOB key type 0x06 signature encrypt/verify for GP v1.1.3.
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
#include "green-power-v113.h"
#include "stack/include/zigbee-security-manager.h"

#define GP_OOB_SIGNATURE_SIZE  16U
#define GP_OOB_MIC_SIZE        4U
#define GP_OOB_NONCE_SIZE      13U
#define GP_OOB_NONCE_SC        0x05U

static void sli_gp_oob_build_nonce(const sl_zigbee_gp_address_t *gpdAddr,
                                    uint32_t frameCounter,
                                    uint8_t nonce[GP_OOB_NONCE_SIZE])
{
  memset(nonce, 0, GP_OOB_NONCE_SIZE);

  if (gpdAddr->applicationId == SL_ZIGBEE_GP_APPLICATION_IEEE_ADDRESS) {
    memmove(nonce, gpdAddr->id.gpdIeeeAddress, EUI64_SIZE);
    sl_util_store_low_high_int32u(&nonce[8], frameCounter);
  } else {
    sl_util_store_low_high_int32u(&nonce[1], gpdAddr->id.sourceId);
    sl_util_store_low_high_int32u(&nonce[5], gpdAddr->id.sourceId);
    sl_util_store_low_high_int32u(&nonce[8], frameCounter);
  }
  nonce[12] = GP_OOB_NONCE_SC;
}

static void sli_gp_oob_load_key(const uint8_t oobKey[16])
{
  sl_zigbee_sec_man_key_t key;
  sl_zigbee_sec_man_context_t context;

  memcpy(key.key, oobKey, SL_ZIGBEE_ENCRYPTION_KEY_SIZE);
  sl_zigbee_sec_man_init_context(&context);
  context.core_key_type = SL_ZB_SEC_MAN_KEY_TYPE_INTERNAL;
  sl_zigbee_sec_man_import_key(&context, &key);
  sl_zigbee_sec_man_load_key_context(&context);
}

sl_status_t sl_zigbee_af_gp_oob_build_clear_signature(const sl_zigbee_gp_address_t *gpdAddr,
                                                       const uint8_t randomOctets[2],
                                                       uint8_t signature[16])
{
  if (gpdAddr == NULL || randomOctets == NULL || signature == NULL) {
    return SL_STATUS_NULL_POINTER;
  }

  memset(signature, 0, GP_OOB_SIGNATURE_SIZE);

  if (gpdAddr->applicationId == SL_ZIGBEE_GP_APPLICATION_IEEE_ADDRESS) {
    memmove(signature, gpdAddr->id.gpdIeeeAddress, EUI64_SIZE);
    signature[8] = randomOctets[0];
    signature[9] = randomOctets[1];
  } else {
    sl_util_store_low_high_int32u(signature, gpdAddr->id.sourceId);
    signature[4] = randomOctets[0];
    signature[5] = randomOctets[1];
  }

  return SL_STATUS_OK;
}

sl_status_t sl_zigbee_af_gp_oob_encrypt_signature(const sl_zigbee_gp_address_t *gpdAddr,
                                                   const uint8_t oobKey[16],
                                                   const uint8_t clearSignature[16],
                                                   uint8_t encryptedSignature[16],
                                                   uint8_t mic[4])
{
  uint8_t nonce[GP_OOB_NONCE_SIZE];
  uint8_t plaintext[GP_OOB_SIGNATURE_SIZE];
  uint8_t temp[GP_OOB_SIGNATURE_SIZE + GP_OOB_MIC_SIZE];
  uint32_t frameCounter;
  sl_status_t status;

  if (gpdAddr == NULL || oobKey == NULL || clearSignature == NULL
      || encryptedSignature == NULL || mic == NULL) {
    return SL_STATUS_NULL_POINTER;
  }

  frameCounter = (gpdAddr->applicationId == SL_ZIGBEE_GP_APPLICATION_IEEE_ADDRESS)
                 ? sl_util_fetch_low_high_int32u(gpdAddr->id.gpdIeeeAddress)
                 : gpdAddr->id.sourceId;

  sli_gp_oob_build_nonce(gpdAddr, frameCounter, nonce);
  sli_gp_oob_load_key(oobKey);

  memcpy(plaintext, clearSignature, GP_OOB_SIGNATURE_SIZE);
  // CCM* encrypts under the loaded OOB key; ciphertext + MIC land in temp.
  status = sl_zigbee_sec_man_aes_ccm(nonce,
                                      true,
                                      plaintext,
                                      0,
                                      GP_OOB_SIGNATURE_SIZE,
                                      temp);
  if (status != SL_STATUS_OK) {
    return status;
  }

  memcpy(encryptedSignature, temp, GP_OOB_SIGNATURE_SIZE);
  memcpy(mic, &temp[GP_OOB_SIGNATURE_SIZE], GP_OOB_MIC_SIZE);
  return SL_STATUS_OK;
}

sl_status_t sl_zigbee_af_gp_oob_verify_signature(const sl_zigbee_gp_address_t *gpdAddr,
                                                  const uint8_t oobKey[16],
                                                  const uint8_t encryptedSignature[16],
                                                  const uint8_t mic[4])
{
  uint8_t nonce[GP_OOB_NONCE_SIZE];
  uint8_t input[GP_OOB_SIGNATURE_SIZE + GP_OOB_MIC_SIZE];
  uint8_t temp[GP_OOB_SIGNATURE_SIZE + GP_OOB_MIC_SIZE];
  uint32_t frameCounter;
  sl_status_t status;

  if (gpdAddr == NULL || oobKey == NULL || encryptedSignature == NULL || mic == NULL) {
    return SL_STATUS_NULL_POINTER;
  }

  frameCounter = (gpdAddr->applicationId == SL_ZIGBEE_GP_APPLICATION_IEEE_ADDRESS)
                 ? sl_util_fetch_low_high_int32u(gpdAddr->id.gpdIeeeAddress)
                 : gpdAddr->id.sourceId;

  sli_gp_oob_build_nonce(gpdAddr, frameCounter, nonce);
  sli_gp_oob_load_key(oobKey);

  memcpy(input, encryptedSignature, GP_OOB_SIGNATURE_SIZE);
  memcpy(&input[GP_OOB_SIGNATURE_SIZE], mic, GP_OOB_MIC_SIZE);

  status = sl_zigbee_sec_man_aes_ccm(nonce,
                                      false,
                                      input,
                                      0,
                                      GP_OOB_SIGNATURE_SIZE,
                                      temp);
  return (status == SL_STATUS_OK) ? SL_STATUS_OK : SL_STATUS_INVALID_SIGNATURE;
}
