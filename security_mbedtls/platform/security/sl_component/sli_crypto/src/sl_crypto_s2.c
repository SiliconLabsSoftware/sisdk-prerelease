/***************************************************************************//**
 * @file
 * @brief Provides hardware accelerated cryptographic primitives for series-2.
 *******************************************************************************
 * # License
 * <b>Copyright 2024 Silicon Laboratories Inc. www.silabs.com</b>
 *******************************************************************************
 *
 * SPDX-License-Identifier: Zlib
 *
 * The licensor of this software is Silicon Laboratories Inc.
 *
 * This software is provided 'as-is', without any express or implied
 * warranty. In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 *
 ******************************************************************************/
#include "em_device.h"
#include "sli_crypto.h"
#include "sl_assert.h"
#include "sli_protocol_crypto.h"
#if defined(CRYPTOACC_PRESENT)
#include "cryptoacc_management.h"
#include "sli_cryptoacc_driver_trng.h"
// for !defined(SLI_MBEDTLS_DEVICE_VSE2)
#include "sli_mbedtls_omnipresent.h"
#else
#include "sl_se_manager.h"
#include "sl_se_manager_types.h"
#include "sl_se_manager_entropy.h"
#endif

// In test the "trng" is mocked to provide predictable values
#if !defined(SLI_CRYPTO_TRNG_MOCK)
sl_status_t sli_crypto_trng_get(uint8_t *dest, size_t nbytes)
{
  if ((NULL == dest) || (0 == nbytes)) {
    return (NULL == dest ? SL_STATUS_NULL_POINTER : SL_STATUS_INVALID_PARAMETER);
  }
  #if defined(CRYPTOACC_PRESENT)
  psa_status_t rc = sli_cryptoacc_trng_get_random(dest, nbytes);
  #else
  sl_se_command_context_t cmd_ctx = { 0 };
  sl_status_t rc = sl_se_init_command_context(&cmd_ctx);
  /// Initialize Secure Element command context
  if ( rc != SL_STATUS_OK) {
    return rc;
  }
  rc = sl_se_get_random(&cmd_ctx, dest, nbytes);

  sl_se_deinit_command_context(&cmd_ctx);
  #endif
  return rc;
}
#endif

sl_status_t sli_crypto_engine_cm_reseed(sli_crypto_engine_t engine, sli_crypto_seed_t *seed)
{
  (void)engine;  // Unused on S2 - only CRYPTOACC exists
  #if !defined(CRYPTOACC_PRESENT) || !defined(SLI_MBEDTLS_DEVICE_VSE_V2)
  (void)seed;
  return SL_STATUS_NOT_SUPPORTED;
  #else
  if (NULL == seed) {
    return SL_STATUS_NULL_POINTER;
  }

  // Reseed the cryptoacc...
  psa_status_t status = cryptoacc_reseed_countermeasures(seed->u32[0]);
  if (status != PSA_SUCCESS) {
    return SL_STATUS_FAIL;
  } else {
    return SL_STATUS_OK;
  }
  #endif
}

sl_status_t sli_crypto_init(void)
{
  #if defined(SLI_RADIOAES_REQUIRES_MASKING)
  sli_aes_seed_mask();
  #endif
  sl_status_t rc = SL_STATUS_FAIL;
  #if defined(CRYPTOACC_PRESENT) && defined(SLI_MBEDTLS_DEVICE_VSE_V2)
  if ((rc = cryptoacc_initialize_countermeasures()) != PSA_SUCCESS) {
    return rc;
  }
  #endif

  // Initialize crypto lock (no-op on bare-metal systems)
  rc = sli_crypto_init_lock();
  #if defined(SL_CATALOG_MICRIUMOS_KERNEL_PRESENT) || defined(SL_CATALOG_FREERTOS_KERNEL_PRESENT)
  // this code path results in warning-is-error "dead code" path on baremetal
  if (rc != SL_STATUS_OK) {
    return rc;
  }
  #endif

  #if defined(SL_CRYPTO_USE_HOST_ENTROPY) && (SL_CRYPTO_USE_HOST_ENTROPY != 0)
  // If using the host entropy pool, accumulate host entropy now
  rc = sli_crypto_entropy_pool_accumulate();
  #endif

  return rc;
}

/***************************************************************************//**
 * @brief          CCM buffer authenticated decryption optimized for BLE
 ******************************************************************************/
sl_status_t sli_crypto_ccm_auth_decrypt_ble(sli_crypto_descriptor_t     *key_descriptor,
                                            unsigned char               *data,
                                            size_t                      length,
                                            const unsigned char         *iv,
                                            unsigned char               header,
                                            unsigned char               *tag)
{
  EFM_ASSERT(key_descriptor != NULL);
  EFM_ASSERT(data != NULL);
  EFM_ASSERT(iv != NULL);
  EFM_ASSERT(tag != NULL);
  EFM_ASSERT(key_descriptor->location == SLI_CRYPTO_KEY_LOCATION_PLAINTEXT);
  EFM_ASSERT(key_descriptor->key.plaintext_key.buffer.pointer != NULL);

  return sli_ccm_auth_decrypt_ble(data,
                                  length,
                                  (const unsigned char *)key_descriptor->key.plaintext_key.buffer.pointer,
                                  iv,
                                  header,
                                  tag);
}

/***************************************************************************//**
 * @brief          CCM buffer encryption optimized for BLE
 ******************************************************************************/
sl_status_t sli_crypto_ccm_encrypt_and_tag_ble(sli_crypto_descriptor_t     *key_descriptor,
                                               unsigned char               *data,
                                               size_t                      length,
                                               const unsigned char         *iv,
                                               unsigned char               header,
                                               unsigned char               *tag)
{
  EFM_ASSERT(key_descriptor != NULL);
  EFM_ASSERT(data != NULL);
  EFM_ASSERT(iv != NULL);
  EFM_ASSERT(tag != NULL);
  EFM_ASSERT(key_descriptor->location == SLI_CRYPTO_KEY_LOCATION_PLAINTEXT);
  EFM_ASSERT(key_descriptor->key.plaintext_key.buffer.pointer != NULL);

  return sli_ccm_encrypt_and_tag_ble(data,
                                     length,
                                     (const unsigned char *)key_descriptor->key.plaintext_key.buffer.pointer,
                                     iv,
                                     header,
                                     tag);
}

/***************************************************************************//**
 * @brief          CCM buffer authenticated decryption optimized for Zigbee
 ******************************************************************************/
sl_status_t sli_crypto_ccm_zigbee(sli_crypto_descriptor_t     *key_descriptor,
                                  bool                        encrypt,
                                  const unsigned char         *data_in,
                                  unsigned char               *data_out,
                                  size_t                      length,
                                  const unsigned char         *iv,
                                  const unsigned char         *aad,
                                  size_t                      aad_len,
                                  unsigned char               *tag,
                                  size_t                      tag_len)
{
  EFM_ASSERT(key_descriptor != NULL);
  EFM_ASSERT(data_in != NULL);
  EFM_ASSERT(iv != NULL);
  EFM_ASSERT(key_descriptor->location == SLI_CRYPTO_KEY_LOCATION_PLAINTEXT);
  EFM_ASSERT(key_descriptor->key.plaintext_key.buffer.pointer != NULL);

  return sli_ccm_zigbee(encrypt,
                        data_in,
                        data_out,
                        length,
                        (const unsigned char *)key_descriptor->key.plaintext_key.buffer.pointer,
                        iv,
                        aad,
                        aad_len,
                        tag,
                        tag_len);
}

/***************************************************************************//**
 * @brief          Process a table of BLE RPA device keys and look for a
 *                 match against the supplied hash
 ******************************************************************************/
sl_status_t sli_crypto_process_rpa(sli_crypto_descriptor_t     *key_descriptor,
                                   size_t                      irk_len,
                                   uint64_t                    keymask,
                                   uint32_t                    prand,
                                   uint32_t                    hash,
                                   int                         *irk_index)
{
  EFM_ASSERT(key_descriptor != NULL);
  EFM_ASSERT(irk_index != NULL);
  EFM_ASSERT(key_descriptor->location == SLI_CRYPTO_KEY_LOCATION_PLAINTEXT);
  EFM_ASSERT(key_descriptor->key.plaintext_key.buffer.pointer != NULL);
  (void)irk_len;
  const unsigned char *keytable
    = (const unsigned char *)key_descriptor->key.plaintext_key.buffer.pointer;
  *irk_index = sli_process_ble_rpa(keytable,
                                   (uint32_t)keymask,
                                   prand,
                                   hash);
  if (*irk_index == -1) {
    return SL_STATUS_FAIL;
  }

  return SL_STATUS_OK;
}

// /***************************************************************************//**
// * @brief          AES-CTR block encryption/decryption optimized for radio
// *******************************************************************************/
sl_status_t sli_crypto_aes_ctr_radio(sli_crypto_descriptor_t  *key_descriptor,
                                     unsigned int             keybits,
                                     const unsigned char      input[SLI_CRYPTO_AES_BLOCK_SIZE],
                                     const unsigned char      iv_in[SLI_CRYPTO_AES_BLOCK_SIZE],
                                     volatile unsigned char   iv_out[SLI_CRYPTO_AES_BLOCK_SIZE],
                                     volatile unsigned char   output[SLI_CRYPTO_AES_BLOCK_SIZE])
{
  EFM_ASSERT(key_descriptor != NULL);
  EFM_ASSERT(keybits == 128 || keybits == 192 || keybits == 256);
  EFM_ASSERT(input != NULL);
  EFM_ASSERT(iv_in != NULL);
  EFM_ASSERT(output != NULL);
  EFM_ASSERT(key_descriptor->location == SLI_CRYPTO_KEY_LOCATION_PLAINTEXT);
  EFM_ASSERT(key_descriptor->key.plaintext_key.key_size == keybits / 8);
  EFM_ASSERT(key_descriptor->key.plaintext_key.buffer.pointer != NULL);

  return sli_aes_crypt_ctr_radio( (const unsigned char *)key_descriptor->key.plaintext_key.buffer.pointer,
                                  keybits,
                                  input,
                                  iv_in,
                                  iv_out,
                                  output);
}

/***************************************************************************//**
 * @brief          AES-ECB block encryption/decryption optimized for radio
 ********************************************************************************/
sl_status_t sli_crypto_aes_ecb_radio(bool                    encrypt,
                                     sli_crypto_descriptor_t *key_descriptor,
                                     unsigned int            keybits,
                                     const unsigned char     input[SLI_CRYPTO_AES_BLOCK_SIZE],
                                     volatile unsigned char  output[SLI_CRYPTO_AES_BLOCK_SIZE])
{
  EFM_ASSERT(key_descriptor != NULL);
  EFM_ASSERT(keybits == 128 || keybits == 192 || keybits == 256);
  EFM_ASSERT(input != NULL);
  EFM_ASSERT(output != NULL);
  EFM_ASSERT(key_descriptor->location == SLI_CRYPTO_KEY_LOCATION_PLAINTEXT);
  EFM_ASSERT(key_descriptor->key.plaintext_key.key_size == keybits / 8);
  EFM_ASSERT(key_descriptor->key.plaintext_key.buffer.pointer != NULL);

  return sli_aes_crypt_ecb_radio(encrypt,
                                 (const unsigned char *)key_descriptor->key.plaintext_key.buffer.pointer,
                                 keybits,
                                 input,
                                 output);
}

/***************************************************************************//**
 * @brief          AES-CMAC calculation optimized for radio
 ********************************************************************************/
sl_status_t sli_crypto_aes_cmac_radio(sli_crypto_descriptor_t *key_descriptor,
                                      unsigned int            keybits,
                                      const unsigned char     *input,
                                      unsigned int            length,
                                      volatile unsigned char  output[SLI_CRYPTO_AES_BLOCK_SIZE])
{
  EFM_ASSERT(key_descriptor != NULL);
  EFM_ASSERT(keybits == 128 || keybits == 192 || keybits == 256);
  EFM_ASSERT(input != NULL);
  EFM_ASSERT(length == SLI_CRYPTO_AES_BLOCK_SIZE);
  EFM_ASSERT(output != NULL);
  EFM_ASSERT(key_descriptor->location == SLI_CRYPTO_KEY_LOCATION_PLAINTEXT);
  EFM_ASSERT(key_descriptor->key.plaintext_key.key_size == keybits / 8);
  EFM_ASSERT(key_descriptor->key.plaintext_key.buffer.pointer != NULL);

  return sli_aes_cmac_radio((const unsigned char *)key_descriptor->key.plaintext_key.buffer.pointer,
                            keybits,
                            input,
                            length,
                            output);
}
