/***************************************************************************//**
 * @file
 * @brief Implementation of AES.
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
#include PLATFORM_HEADER
#include "hal/hal.h"
#include "stack/platform/micro/aes.h"
#include <stdbool.h>
#include "include/security.h"

#ifdef SL_COMPONENT_CATALOG_PRESENT
#include "sl_component_catalog.h"
#endif

#if !defined(EZSP_HOST)
#include "internal/inc/internal-defs-patch.h"
#endif

#if defined(SL_CATALOG_SLI_PROTOCOL_CRYPTO_PRESENT)
// Quick-and-dirty implementation on top of RADIOAES / CRYPTO2 for increased speed
#include "sli_protocol_crypto.h"

// PSA Crypto exposes stateless AES hooks. Use these with a static key buffer
// until we can fully migrate to PSA Crypto / Vault key management, at which point
// these calls would become PSA key handling calls with the correct key ID.
static uint8_t loadedKey[SL_ZIGBEE_ENCRYPTION_KEY_SIZE] = { 0 };

// Load the passed key into the encryption core.
void sli_util_load_key_into_core(const uint8_t* key)
{
  memcpy(loadedKey, key, sizeof(loadedKey));
}

void sli_zigbee_get_key_from_core(uint8_t* key)
{
  memcpy(key, loadedKey, sizeof(loadedKey));
}

void sli_zigbee_security_hardware_init(void)
{
  return;
}

void sli_util_stand_alone_encrypt_block(uint8_t* block)
{
  // Encrypt this block in place with the current key
  sl_status_t status = sli_aes_crypt_ecb_radio(
    true,
    loadedKey,
    SL_ZIGBEE_ENCRYPTION_KEY_SIZE * 8,
    block,
    block);
  assert(status == SL_STATUS_OK);
}

//----------------------------------------------------------------
// Wrapper for those that just want access to AES.

void sli_zigbee_aes_encrypt(uint8_t* block, const uint8_t* key)
{
  // Encrypt this block in place with the current key
  sl_status_t status = sli_aes_crypt_ecb_radio(
    true,
    key,
    SL_ZIGBEE_ENCRYPTION_KEY_SIZE * 8,
    block,
    block);
  assert(status == SL_STATUS_OK);
}

void sli_zigbee_aes_decrypt(uint8_t* block, const uint8_t* key)
{
  // Encrypt this block in place with the current key
  sl_status_t status = sli_aes_crypt_ecb_radio(
    false,
    key,
    SL_ZIGBEE_ENCRYPTION_KEY_SIZE * 8,
    block,
    block);
  assert(status == SL_STATUS_OK);
}

#else // !SL_CATALOG_SLI_PROTOCOL_CRYPTO_PRESENT
// Software / PSA-accel paths need Mbed TLS build_info for feature macros.
#if defined(__has_include)
#if __has_include(<mbedtls/build_info.h>)
#include <mbedtls/build_info.h>
#elif __has_include(<tf-psa-crypto/build_info.h>)
#include <tf-psa-crypto/build_info.h>
#endif
#else
#include <mbedtls/build_info.h>
#endif

#if (defined(MBEDTLS_PSA_ACCEL_KEY_TYPE_AES) && defined(MBEDTLS_PSA_ACCEL_ALG_ECB_NO_PADDING) && defined(PSA_WANT_ALG_ECB_NO_PADDING) && defined(MBEDTLS_PSA_CRYPTO_DRIVERS)) \
  || defined(SEMAILBOX_PRESENT) || defined(CRYPTOACC_PRESENT) || defined(CRYPTO_PRESENT)
// PSA Crypto driver implementation
#include "psa/crypto.h"

#if defined(SEMAILBOX_PRESENT)
#include "sli_se_transparent_types.h"
#include "sli_se_transparent_functions.h"
#define CIPHER_SINGLE_SHOT_ENC_FCT sli_se_transparent_cipher_encrypt
#define CIPHER_SINGLE_SHOT_DEC_FCT sli_se_transparent_cipher_decrypt
#elif defined(CRYPTOACC_PRESENT)
#include "sli_cryptoacc_transparent_types.h"
#include "sli_cryptoacc_transparent_functions.h"
#define CIPHER_SINGLE_SHOT_ENC_FCT sli_cryptoacc_transparent_cipher_encrypt
#define CIPHER_SINGLE_SHOT_DEC_FCT sli_cryptoacc_transparent_cipher_decrypt
#elif defined(CRYPTO_PRESENT)
#include "sli_crypto_transparent_types.h"
#include "sli_crypto_transparent_functions.h"
#define CIPHER_SINGLE_SHOT_ENC_FCT sli_crypto_transparent_cipher_encrypt
#define CIPHER_SINGLE_SHOT_DEC_FCT sli_crypto_transparent_cipher_decrypt
#else
#error "Compiling with PSA drivers, but not for a target part"
#endif

// PSA Crypto exposes stateless AES hooks. Use these with a static key buffer
// until we can fully migrate to PSA Crypto / Vault key management, at which point
// these calls would become PSA key handling calls with the correct key ID.
static uint8_t loadedKey[SL_ZIGBEE_ENCRYPTION_KEY_SIZE] = { 0 };

// Load the passed key into the encryption core.
void sli_util_load_key_into_core(const uint8_t* key)
{
  memcpy(loadedKey, key, sizeof(loadedKey));
}

void sli_zigbee_get_key_from_core(uint8_t* key)
{
  memcpy(key, loadedKey, sizeof(loadedKey));
}

void sli_zigbee_security_hardware_init(void)
{
}

void sli_util_stand_alone_encrypt_block(uint8_t* block)
{
  // Encrypt this block in place with the current key
  size_t output_size = 0;
  psa_key_attributes_t key_attr = PSA_KEY_ATTRIBUTES_INIT;

  psa_set_key_type(&key_attr, PSA_KEY_TYPE_AES);
  psa_set_key_bits(&key_attr, PSA_BYTES_TO_BITS(SL_ZIGBEE_ENCRYPTION_KEY_SIZE));

  psa_status_t status = CIPHER_SINGLE_SHOT_ENC_FCT(
    &key_attr,
    loadedKey,
    sizeof(loadedKey),
    PSA_ALG_ECB_NO_PADDING,
    NULL, 0,
    block, SECURITY_BLOCK_SIZE,
    block, SECURITY_BLOCK_SIZE,
    &output_size);

  psa_reset_key_attributes(&key_attr);

  assert(status == PSA_SUCCESS);
}

//----------------------------------------------------------------
// Wrapper for those that just want access to AES.

void sli_zigbee_aes_encrypt(uint8_t* block, const uint8_t* key)
{
  size_t output_size = 0;
  psa_key_attributes_t key_attr = PSA_KEY_ATTRIBUTES_INIT;

  psa_set_key_type(&key_attr, PSA_KEY_TYPE_AES);
  psa_set_key_bits(&key_attr, PSA_BYTES_TO_BITS(SL_ZIGBEE_ENCRYPTION_KEY_SIZE));

  psa_status_t status = CIPHER_SINGLE_SHOT_ENC_FCT(
    &key_attr,
    key,
    SL_ZIGBEE_ENCRYPTION_KEY_SIZE,
    PSA_ALG_ECB_NO_PADDING,
    NULL, 0,
    block, SECURITY_BLOCK_SIZE,
    block, SECURITY_BLOCK_SIZE,
    &output_size);

  psa_reset_key_attributes(&key_attr);

  assert(status == PSA_SUCCESS);
  assert(output_size == SECURITY_BLOCK_SIZE);
}

void sli_zigbee_aes_decrypt(uint8_t* block, const uint8_t* key)
{
  size_t output_size = 0;
  psa_key_attributes_t key_attr = PSA_KEY_ATTRIBUTES_INIT;

  psa_set_key_type(&key_attr, PSA_KEY_TYPE_AES);
  psa_set_key_bits(&key_attr, PSA_BYTES_TO_BITS(SL_ZIGBEE_ENCRYPTION_KEY_SIZE));

  psa_status_t status = CIPHER_SINGLE_SHOT_DEC_FCT(
    &key_attr,
    key,
    SL_ZIGBEE_ENCRYPTION_KEY_SIZE,
    PSA_ALG_ECB_NO_PADDING,
    block, SECURITY_BLOCK_SIZE,
    block, SECURITY_BLOCK_SIZE,
    &output_size);

  psa_reset_key_attributes(&key_attr);

  assert(status == PSA_SUCCESS);
  assert(output_size == SECURITY_BLOCK_SIZE);
}

#else
// Software / host AES via portable PSA Crypto (no legacy mbedtls_aes_* APIs).
// Mbed TLS 4.x only defines MBEDTLS_AES_C via TF-PSA builtins when PSA_WANT_KEY_TYPE_AES
// is set; simulation/native builds must use PSA cipher APIs instead.
#include "psa/crypto.h"

static uint8_t loadedKey[SL_ZIGBEE_ENCRYPTION_KEY_SIZE] = { 0 };

static psa_status_t sli_zigbee_psa_aes_ecb(bool encrypt,
                                           const uint8_t *key,
                                           const uint8_t *input,
                                           uint8_t *output)
{
  psa_key_attributes_t attributes = PSA_KEY_ATTRIBUTES_INIT;
  psa_key_id_t key_id = 0;
  psa_status_t status;
  size_t output_length = 0;
  uint8_t temp[SECURITY_BLOCK_SIZE];

  // Native/simulation does not auto-register psa_crypto_init (unlike device).
  // Legacy mbedtls_aes_* needed no global init; PSA does. Safe if already inited.
  status = psa_crypto_init();
  if (status != PSA_SUCCESS) {
    return status;
  }

  psa_set_key_type(&attributes, PSA_KEY_TYPE_AES);
  psa_set_key_bits(&attributes, PSA_BYTES_TO_BITS(SL_ZIGBEE_ENCRYPTION_KEY_SIZE));
  psa_set_key_algorithm(&attributes, PSA_ALG_ECB_NO_PADDING);
  psa_set_key_usage_flags(&attributes, PSA_KEY_USAGE_ENCRYPT | PSA_KEY_USAGE_DECRYPT);

  status = psa_import_key(&attributes, key, SL_ZIGBEE_ENCRYPTION_KEY_SIZE, &key_id);
  psa_reset_key_attributes(&attributes);
  if (status != PSA_SUCCESS) {
    return status;
  }

  if (encrypt) {
    status = psa_cipher_encrypt(key_id,
                                PSA_ALG_ECB_NO_PADDING,
                                input,
                                SECURITY_BLOCK_SIZE,
                                temp,
                                sizeof(temp),
                                &output_length);
  } else {
    status = psa_cipher_decrypt(key_id,
                                PSA_ALG_ECB_NO_PADDING,
                                input,
                                SECURITY_BLOCK_SIZE,
                                temp,
                                sizeof(temp),
                                &output_length);
  }

  (void)psa_destroy_key(key_id);

  if (status == PSA_SUCCESS && output_length == SECURITY_BLOCK_SIZE) {
    memcpy(output, temp, SECURITY_BLOCK_SIZE);
  } else if (status == PSA_SUCCESS) {
    status = PSA_ERROR_GENERIC_ERROR;
  }

  return status;
}

void sli_util_load_key_into_core(const uint8_t* key)
{
  memcpy(loadedKey, key, sizeof(loadedKey));
}

void sli_zigbee_get_key_from_core(uint8_t* key)
{
  memcpy(key, loadedKey, sizeof(loadedKey));
}

void sli_zigbee_security_hardware_init(void)
{
  (void)psa_crypto_init();
}

void sli_util_stand_alone_encrypt_block(uint8_t* block)
{
  psa_status_t status = sli_zigbee_psa_aes_ecb(true, loadedKey, block, block);
  assert(status == PSA_SUCCESS);
}

void sli_zigbee_aes_encrypt(uint8_t* block, const uint8_t* key)
{
  psa_status_t status = sli_zigbee_psa_aes_ecb(true, key, block, block);
  assert(status == PSA_SUCCESS);
}

void sli_zigbee_aes_decrypt(uint8_t* block, const uint8_t* key)
{
  psa_status_t status = sli_zigbee_psa_aes_ecb(false, key, block, block);
  assert(status == PSA_SUCCESS);
}

#if defined(MBEDTLS_PSA_BUILTIN_GET_ENTROPY) || defined(MBEDTLS_PSA_DRIVER_GET_ENTROPY)
// Some mbedtls package builds omit entropy_poll.c, leaving
// mbedtls_entropy_poll_platform undefined when GET_ENTROPY is enabled.
// Weak thin adapter around mbedtls_platform_get_entropy(); ignored if the
// package already provides a strong mbedtls_entropy_poll_platform.
#include "mbedtls/platform.h"
#include "mbedtls/private/entropy.h"
#include <psa/crypto_driver_random.h>

__attribute__((weak))
int mbedtls_entropy_poll_platform(void *data, unsigned char *output, size_t len, size_t *olen)
{
  size_t estimate_bits = 0;
  int ret;
  (void)data;

  /* Same contract as tf-psa-crypto entropy_poll.c: whole buffer is useful. */
  *olen = len;

  ret = mbedtls_platform_get_entropy(PSA_DRIVER_GET_ENTROPY_FLAGS_NONE,
                                     &estimate_bits, output, len);
  if (ret != 0) {
    return ret;
  }
  if (estimate_bits < (8 * len)) {
    return MBEDTLS_ERR_ENTROPY_SOURCE_FAILED;
  }
  return 0;
}
#endif // GET_ENTROPY

#endif // MBEDTLS_PSA_ACCEL... / software PSA
#endif // SL_CATALOG_SLI_PROTOCOL_CRYPTO_PRESENT
