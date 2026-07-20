/***************************************************************************//**
 * @file
 * @brief Certificate Based Authentication and Pairing implementation
 *******************************************************************************
 * # License
 * <b>Copyright 2026 Silicon Laboratories Inc. www.silabs.com</b>
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
#include <stdbool.h>
#include <string.h>
#include "em_system.h"
#include "sl_common.h"
#include "sl_component_catalog.h"

#include "ecode.h"
#include "nvm3.h"
#include "mbedtls/pk.h"
#include "mbedtls/x509.h"
#include "mbedtls/x509_csr.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/oid.h"
#include "mbedtls/x509_crt.h"
#include "mbedtls/base64.h"
#include "psa/crypto.h"
#include "psa/crypto_values.h"
#include "cbap_key_id.h"
#include "sl_bt_cbap.h"

// -----------------------------------------------------------------------------
// Defines

#define OOB_SIGNATURE_LEN             64
#define CERT_IND_CHUNK_LEN            100
#define EC_PUB_KEY_LEN                65
#define PUB_KEY_OFFSET                26

#define OOB_RANDOM_LEN                (16)
#define OOB_DATA_LEN                  (2 * OOB_RANDOM_LEN)
#define SIGNED_OOB_DATA_LEN           (OOB_DATA_LEN + OOB_SIGNATURE_LEN)

// Logging
#define LOG_PREFIX                    "[CBAP] "
#if defined(SL_CATALOG_APP_LOG_PRESENT)
#include "app_log.h"
#define LOG_NL                        APP_LOG_NL
#define LOG_ERROR(...)                app_log_error(LOG_PREFIX __VA_ARGS__)
#else // SL_CATALOG_APP_LOG_PRESENT
#define LOG_NL
#define LOG_ERROR(...)
#endif // SL_CATALOG_APP_LOG_PRESENT

// -----------------------------------------------------------------------------
// Module variables

// Root certificate context
static mbedtls_x509_crt root_certificate_context;

// Public key ID of the remote device
static mbedtls_svc_key_id_t remote_pub_key_id = 0;

// -----------------------------------------------------------------------------
// Private function declarations

// Read certificate from NVM3.
static sl_status_t get_certificate(uint8_t *cert, uint32_t *cert_len, uint32_t nvm3_key);

// Read data from NVM3 located at a specified NVM3 key.
static sl_status_t get_nvm3_object(uint32_t key, uint8_t *buf, size_t *len, size_t maxlen);

// Converts PSA status code to SL status code.
static sl_status_t psa_status_to_sl_status(psa_status_t sc);

// -----------------------------------------------------------------------------
// Public function definitions

/******************************************************************************
 * Imports and validates the device with root certificate.
 *
 * @param[out] device_certificate_der device certificate in DER format.
 * @param[out] device_certificate_der_len device certificate length.
 *
 * @return SL_STATUS_OK if device certificate is validated, error code otherwise.
 *****************************************************************************/
sl_status_t sl_bt_cbap_init(uint8_t *device_certificate_der, uint32_t *device_certificate_der_len)
{
  sl_status_t sc;
  int mbedtls_ret = 0;
  mbedtls_x509_crt dev_certificate_context;

  if (device_certificate_der == NULL || device_certificate_der_len == NULL) {
    return SL_STATUS_NULL_POINTER;
  }

  SYSTEM_SecurityCapability_TypeDef capability;
  capability = SYSTEM_GetSecurityCapability();
  if (capability != securityCapabilityRoT
      && capability != securityCapabilitySE
      && capability != securityCapabilityVault) {
    LOG_ERROR("Invalid security capability: %d" LOG_NL, capability);
    return SL_STATUS_NOT_SUPPORTED;
  }

  sc = psa_status_to_sl_status(psa_crypto_init());
  if (sc != SL_STATUS_OK) {
    LOG_ERROR("Failed to initialize PSA crypto: 0x%04lx" LOG_NL, sc);
    return sc;
  }

  // Get device certificate
  sc = get_certificate(device_certificate_der, device_certificate_der_len, CBAP_NVM_DEVICE_CERT);
  if (sc != SL_STATUS_OK) {
    LOG_ERROR("Failed to get device certificate: 0x%04lx" LOG_NL, sc);
    LOG_ERROR("Please make the device was provisioned with success." LOG_NL);
    return sc;
  }

  mbedtls_x509_crt_init(&dev_certificate_context);
  mbedtls_ret = mbedtls_x509_crt_parse(&dev_certificate_context,
                                       (const unsigned char *)device_certificate_der,
                                       *device_certificate_der_len);
  if (mbedtls_ret != 0) {
    LOG_ERROR("Failed to parse device certificate: %d" LOG_NL, mbedtls_ret);
    return SL_STATUS_FAIL;
  }

  // Get root certificate
  uint8_t root_certificate_der[SL_BT_CBAP_CERTIFICATE_MAX_SIZE] = { 0 };
  size_t root_certificate_der_len;

  sc = get_certificate(root_certificate_der, (uint32_t *)&root_certificate_der_len, CBAP_NVM_ROOT_CERT);
  if (sc != SL_STATUS_OK) {
    LOG_ERROR("Failed to get root certificate: 0x%04lx" LOG_NL, sc);
    LOG_ERROR("Please make the device was provisioned with success." LOG_NL);
    return sc;
  }

  mbedtls_x509_crt_init(&root_certificate_context);
  mbedtls_ret = mbedtls_x509_crt_parse(&root_certificate_context,
                                       (const unsigned char *)root_certificate_der,
                                       root_certificate_der_len);
  if (mbedtls_ret != 0) {
    LOG_ERROR("Failed to parse root certificate: %d" LOG_NL, mbedtls_ret);
    return SL_STATUS_FAIL;
  }

  // Validate device certificate with the root certificate
  uint32_t flags;
  mbedtls_ret = mbedtls_x509_crt_verify(&dev_certificate_context,
                                        &root_certificate_context,
                                        NULL,
                                        NULL,
                                        &flags,
                                        NULL,
                                        NULL);
  (void)flags;
  if (mbedtls_ret != 0) {
    LOG_ERROR("Failed to verify device certificate against root certificate: %d" LOG_NL, mbedtls_ret);
    return SL_STATUS_FAIL;
  }

  // Check the presence of the device key
  psa_key_attributes_t key_attr;
  key_attr = psa_key_attributes_init();
  sc = psa_status_to_sl_status(psa_get_key_attributes(CBAP_PSA_DEVICE_KEY, &key_attr));
  if (sc != SL_STATUS_OK) {
    LOG_ERROR("Device key cannot be found: 0x%04lx" LOG_NL, sc);
    return sc;
  }

  return SL_STATUS_OK;
}

/*******************************************************************************
 * Parse and validate remote certificate and extract remote public key.
 *
 * @param[in] remote_certificate_der Certificate from remote device in DER.
 * @param[in] remote_certificate_der_len Length of the remote certificate.
 *
 * @return SL_STATUS_OK if remote certificate is verified, error code otherwise.
 ******************************************************************************/
sl_status_t sl_bt_cbap_process_remote_cert(uint8_t *remote_certificate_der, uint32_t remote_certificate_der_len)
{
  sl_status_t sc;
  int mbedtls_ret = 0;
  mbedtls_x509_crt remote_certificate_context;

  if (remote_certificate_der == NULL || remote_certificate_der_len == 0) {
    return SL_STATUS_NULL_POINTER;
  }

  // Initialize and parse remote certificate
  mbedtls_x509_crt_init(&remote_certificate_context);
  mbedtls_ret = mbedtls_x509_crt_parse(&remote_certificate_context,
                                       (const unsigned char *)remote_certificate_der,
                                       remote_certificate_der_len);
  if (mbedtls_ret != 0) {
    LOG_ERROR("Failed to parse remote certificate: %d" LOG_NL, mbedtls_ret);
    return SL_STATUS_FAIL;
  }

  unsigned char buf[1024];
  size_t olen;

  // Log
  mbedtls_ret = mbedtls_base64_encode(buf,
                                      sizeof(buf),
                                      &olen,
                                      remote_certificate_context.raw.p,
                                      remote_certificate_context.raw.len);
  (void)olen;
  if (mbedtls_ret != 0) {
    LOG_ERROR("Failed to encode remote certificate: %d" LOG_NL, mbedtls_ret);
    return SL_STATUS_FAIL;
  }

  // Validate it with the root certificate
  uint32_t flags;
  mbedtls_ret = mbedtls_x509_crt_verify(&remote_certificate_context,
                                        &root_certificate_context,
                                        NULL,
                                        NULL,
                                        &flags,
                                        NULL,
                                        NULL);
  (void)flags;
  if (mbedtls_ret != 0) {
    LOG_ERROR("Failed to verify remote certificate against root certificate: %d" LOG_NL, mbedtls_ret);
    return SL_STATUS_FAIL;
  }

  // Get the public key from the remote certificate and set attributes
  psa_key_attributes_t remote_pub_key_attr = PSA_KEY_ATTRIBUTES_INIT;
  psa_set_key_algorithm(&remote_pub_key_attr, PSA_ALG_ECDSA(PSA_ALG_SHA_256));
  psa_set_key_type(&remote_pub_key_attr, PSA_KEY_TYPE_ECC_PUBLIC_KEY(PSA_ECC_FAMILY_SECP_R1));
  psa_set_key_usage_flags(&remote_pub_key_attr, PSA_KEY_USAGE_VERIFY_MESSAGE);

  sc = psa_status_to_sl_status(psa_import_key(&remote_pub_key_attr,
                                              &remote_certificate_context.pk_raw.p[PUB_KEY_OFFSET],
                                              EC_PUB_KEY_LEN,
                                              &remote_pub_key_id));

  if (sc != SL_STATUS_OK) {
    LOG_ERROR("Failed to import public key from remote certificate: 0x%04lx" LOG_NL, sc);
    return sc;
  }

  mbedtls_x509_crt_free(&remote_certificate_context);

  return SL_STATUS_OK;
}

/*******************************************************************************
 * Signs and combines OOB data.
 *
 * @param[in] device_random OOB data generated by the bt stack.
 * @param[in] device_confirm OOB data generated by the bt stack.
 * @param[out] output_data The signed OOB data
 * @param[out] output_len The signed OOB data length
 *
 * @return SL_STATUS_OK if OOB data signed, error code otherwise.
 ******************************************************************************/
sl_status_t sl_bt_cbap_sign_device_oob_data(uint8_t *device_random,
                                            uint8_t *device_confirm,
                                            uint8_t *output_data,
                                            size_t *output_len)
{
  sl_status_t sc;
  uint8_t input_data[OOB_DATA_LEN];
  uint8_t signature[OOB_SIGNATURE_LEN];
  size_t signature_length;

  if (output_data == NULL || output_len == NULL || device_random == NULL || device_confirm == NULL) {
    return SL_STATUS_NULL_POINTER;
  }

  memcpy(input_data, device_random, OOB_RANDOM_LEN);
  memcpy(&input_data[OOB_RANDOM_LEN], device_confirm, OOB_RANDOM_LEN);

  // Sign the OOB
  sc = psa_status_to_sl_status(psa_sign_message(CBAP_PSA_DEVICE_KEY,
                                                PSA_ALG_ECDSA(PSA_ALG_SHA_256),
                                                input_data,
                                                sizeof(input_data),
                                                signature,
                                                sizeof(signature),
                                                &signature_length));
  if (sc != SL_STATUS_OK) {
    LOG_ERROR("Failed to sign OOB data: 0x%04lx" LOG_NL, sc);
    return sc;
  }

  memcpy(output_data, input_data, OOB_DATA_LEN);
  memcpy(&output_data[OOB_DATA_LEN], signature, signature_length);

  *output_len = SIGNED_OOB_DATA_LEN;
  return SL_STATUS_OK;
}

/*******************************************************************************
 * Verifies the remote device OOB data signature.
 *
 * @param[in] remote_random OOB data from remote device.
 * @param[in] remote_confirm OOB data from remote device.
 * @param[in] remote_oob_signature Remote OOB signature.
 *
 * @return SL_STATUS_OK if OOB data signature is OK, error code otherwise.
 ******************************************************************************/
sl_status_t sl_bt_cbap_verify_remote_oob_data(uint8_t *remote_random,
                                              uint8_t *remote_confirm,
                                              uint8_t *remote_oob_signature)
{
  uint8_t input_data[OOB_DATA_LEN];
  sl_status_t sc;

  if (remote_random == NULL || remote_confirm == NULL || remote_oob_signature == NULL) {
    return SL_STATUS_NULL_POINTER;
  }

  memcpy(input_data, remote_random, OOB_RANDOM_LEN);
  memcpy(&input_data[OOB_RANDOM_LEN], remote_confirm, OOB_RANDOM_LEN);

  sc = psa_status_to_sl_status(psa_verify_message(remote_pub_key_id,
                                                  PSA_ALG_ECDSA(PSA_ALG_SHA_256),
                                                  input_data,
                                                  sizeof(input_data),
                                                  remote_oob_signature,
                                                  OOB_SIGNATURE_LEN));
  if (sc != SL_STATUS_OK) {
    LOG_ERROR("Failed to verify remote OOB data signature: 0x%04lx" LOG_NL, sc);
    return sc;
  }

  return SL_STATUS_OK;
}

/*******************************************************************************
 * Destroys the keys which were used during the CBAP process.
 *
 * @return SL_STATUS_OK if OK, error code otherwise.
 ******************************************************************************/
sl_status_t sl_bt_cbap_destroy_key(void)
{
  return psa_status_to_sl_status(psa_destroy_key(remote_pub_key_id));
}

// -----------------------------------------------------------------------------
// Private function definitions

/*******************************************************************************
 * Read certificate from NVM3.
 *
 * @param[out] cert Certificate buffer.
 * @param[out] cert_len Certificate length.
 * @param[in]  nvm3_key the NVM3 key.
 ******************************************************************************/
static sl_status_t get_certificate(uint8_t *cert, uint32_t *cert_len, uint32_t nvm3_key)
{
  sl_status_t sc;
  size_t nvm3_obj_len;
  sc = get_nvm3_object(nvm3_key,
                       cert,
                       &nvm3_obj_len,
                       SL_BT_CBAP_CERTIFICATE_MAX_SIZE);
  *cert_len = (uint32_t)nvm3_obj_len;
  return sc;
}

/*******************************************************************************
 * Read data from NVM3 located at a specified NVM3 key.
 *
 * @param[in] key the key of the NVM3 object.
 * @param[out] buf buffer to write.
 * @param[out] len the size of the NVM3 object.
 * @param[in] maxlen maximum size to read.
 * @return SL_STATUS_OK - if successful, error code otherwise.
 ******************************************************************************/
static sl_status_t get_nvm3_object(uint32_t key, uint8_t *buf, size_t *len, size_t maxlen)
{
  sl_status_t sc;
  uint32_t type;
  size_t obj_len;

  // Clamp read size to maxlen
  sc = nvm3_getObjectInfo(CBAP_NVM3_HANDLE, key, &type, &obj_len);
  (void)type;

  if (sc != SL_STATUS_OK) {
    *len = 0u;
    return sc;
  }

  *len = obj_len;
  if (*len > maxlen) {
    LOG_ERROR("NVM3 object is larger then allowed maximum length!" LOG_NL);
    *len = maxlen;
  }

  // Read NVM3 data
  sc = nvm3_readData(CBAP_NVM3_HANDLE, key, buf, *len);
  return sc;
}

/*******************************************************************************
 * Converts PSA status code to SL status code.
 *
 * @param[in] sc PSA status code
 * @return SL status code.
 ******************************************************************************/
static sl_status_t psa_status_to_sl_status(psa_status_t sc)
{
  switch (sc) {
    case PSA_SUCCESS:                     return SL_STATUS_OK;
    case PSA_ERROR_GENERIC_ERROR:         return SL_STATUS_FAIL;
    case PSA_ERROR_NOT_SUPPORTED:         return SL_STATUS_NOT_SUPPORTED;
    case PSA_ERROR_NOT_PERMITTED:         return SL_STATUS_PERMISSION;
    case PSA_ERROR_BUFFER_TOO_SMALL:      return SL_STATUS_WOULD_OVERFLOW;
    case PSA_ERROR_ALREADY_EXISTS:        return SL_STATUS_ALREADY_EXISTS;
    case PSA_ERROR_DOES_NOT_EXIST:        return SL_STATUS_FAIL;
    case PSA_ERROR_BAD_STATE:             return SL_STATUS_INVALID_STATE;
    case PSA_ERROR_INVALID_ARGUMENT:      return SL_STATUS_INVALID_PARAMETER;
    case PSA_ERROR_INSUFFICIENT_MEMORY:   return SL_STATUS_NO_MORE_RESOURCE;
    case PSA_ERROR_INSUFFICIENT_STORAGE:  return SL_STATUS_NO_MORE_RESOURCE;
    case PSA_ERROR_COMMUNICATION_FAILURE: return SL_STATUS_IO;
    case PSA_ERROR_STORAGE_FAILURE:       return SL_STATUS_BT_HARDWARE;
    case PSA_ERROR_HARDWARE_FAILURE:      return SL_STATUS_BT_HARDWARE;
    case PSA_ERROR_CORRUPTION_DETECTED:   return SL_STATUS_BT_DATA_CORRUPTED;
    case PSA_ERROR_INSUFFICIENT_ENTROPY:  return SL_STATUS_BT_CRYPTO;
    case PSA_ERROR_INVALID_SIGNATURE:     return SL_STATUS_INVALID_SIGNATURE;
    case PSA_ERROR_INVALID_PADDING:       return SL_STATUS_BT_CRYPTO;
    case PSA_ERROR_INSUFFICIENT_DATA:     return SL_STATUS_BT_CRYPTO;
    case PSA_ERROR_INVALID_HANDLE:        return SL_STATUS_INVALID_HANDLE;
    case PSA_ERROR_DATA_CORRUPT:          return SL_STATUS_BT_DATA_CORRUPTED;
    case PSA_ERROR_DATA_INVALID:          return SL_STATUS_BT_CRYPTO;
    default:                              return SL_STATUS_BT_UNSPECIFIED;
  }
}
