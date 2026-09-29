/***************************************************************************//**
 * @file
 * @brief Certificate Based Authentication and Pairing crypto implementation
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
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "em_system.h"
#include "sl_common.h"

#include "mbedtls/pk.h"
#include "mbedtls/x509.h"
#include "mbedtls/x509_csr.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/oid.h"
#include "mbedtls/x509_crt.h"
#include "psa/crypto.h"
#include "psa/crypto_values.h"
#include "psa_crypto_its.h"

#include "app_assert.h"
#include "sli_bt_cbap.h"
#include "sl_bt_cbap.h"

// -----------------------------------------------------------------------------
// Defines

#define EC_PUB_KEY_LEN                65

// The certificate chain
typedef enum {
  CERT_ID_ROOT = 0,
  CERT_ID_FACTORY,
  CERT_ID_BATCH,
  CERT_ID_DEVICE,
  CERT_ID_COUNT,
} cert_id_t;

// Local certificate storage
typedef enum {
  LOCAL_CERT_ISSUER = 0,
  LOCAL_CERT_SUBJECT,
  LOCAL_CERT_COUNT,
} local_cert_t;

// Certificate structure
typedef struct {
  cbap_key_id_t    uid;
  mbedtls_x509_crt context;
  uint8_t          der[SL_BT_CBAP_CERTIFICATE_MAX_SIZE];
  size_t           der_len;
} cert_t;

// -----------------------------------------------------------------------------
// Private variables

// Array for storing two certificates simultaneously in order to validate
// them against each other
static cert_t local_certs[LOCAL_CERT_COUNT];

// Storage ID of each certificate of the chain that belongs to this device.
// (Certificate chain for the remote is handled separately.)
static const cbap_key_id_t cert_chain_ids[CERT_ID_COUNT] = {
  [CERT_ID_ROOT]    = SL_BT_CBAP_PSA_ROOT_CERT,
  [CERT_ID_FACTORY] = SL_BT_CBAP_PSA_FACTORY_CERT,
  [CERT_ID_BATCH]   = SL_BT_CBAP_PSA_BATCH_CERT,
  [CERT_ID_DEVICE]  = SL_BT_CBAP_PSA_DEVICE_CERT,
};

// Name of each certificate of the chain, for logging purposes.
static const char * const cert_chain_names[CERT_ID_COUNT] = {
  [CERT_ID_ROOT]    = "root",
  [CERT_ID_FACTORY] = "factory",
  [CERT_ID_BATCH]   = "batch",
  [CERT_ID_DEVICE]  = "device",
};

// The roles that need each certificate of the chain. A prover sends its device
// and batch certificates to prove itself, a verifier validates the received
// certificates against its factory and root certificates.
static const uint8_t cert_chain_roles[CERT_ID_COUNT] = {
  [CERT_ID_ROOT]    = SL_BT_CBAP_ROLE_VERIFIER,
  [CERT_ID_FACTORY] = SL_BT_CBAP_ROLE_VERIFIER,
  [CERT_ID_BATCH]   = SL_BT_CBAP_ROLE_PROVER,
  [CERT_ID_DEVICE]  = SL_BT_CBAP_ROLE_PROVER,
};

// Certificate chain security profile
static const mbedtls_x509_crt_profile cert_chain_profile = {
  .allowed_mds    = MBEDTLS_X509_ID_FLAG(MBEDTLS_MD_SHA256),
  .allowed_pks    = (MBEDTLS_X509_ID_FLAG(MBEDTLS_PK_ECDSA)
                     | MBEDTLS_X509_ID_FLAG(MBEDTLS_PK_ECKEY)),
  .allowed_curves = MBEDTLS_X509_ID_FLAG(MBEDTLS_ECP_DP_SECP256R1),
  .rsa_min_bitlen = 0,
};

// Public key ID of the remote device
static mbedtls_svc_key_id_t remote_pub_key_id = 0;

// -----------------------------------------------------------------------------
// Private function declarations

// Load and parse a certificate into one of the local certificate slots.
static sl_status_t load_certificate(uint8_t slot, cbap_key_id_t id);

// Validate a certificate against the given issuer.
static sl_status_t validate_certificate(cbap_key_id_t cert,
                                        cbap_key_id_t issuer);

// Check that the device certificate belongs to the device key.
static sl_status_t check_device_key(void);

// Import the public key of the remote device from its device certificate.
static sl_status_t import_remote_pub_key(void);

// Convert PSA status code to SL status code.
static sl_status_t psa_status_to_sl_status(psa_status_t sc);

// -----------------------------------------------------------------------------
// Private function definitions

// Initialize module. Validate the provisioned certificate chain.
void sli_bt_cbap_crypto_init(void)
{
  // Check device security capability.
  SYSTEM_SecurityCapability_TypeDef capability;
  capability = SYSTEM_GetSecurityCapability();
  if (capability != securityCapabilityRoT
      && capability != securityCapabilitySE
      && capability != securityCapabilityVault) {
    CBAP_LOG_ERROR("Invalid security capability: %d" CBAP_LOG_NL, capability);
    app_assert_status(SL_STATUS_NOT_SUPPORTED);
  }

  // Initialize PSA crypto.
  sl_status_t sc;
  sc = psa_status_to_sl_status(psa_crypto_init());
  if (sc != SL_STATUS_OK) {
    CBAP_LOG_ERROR("Failed to initialize PSA crypto: 0x%04lx" CBAP_LOG_NL, sc);
    app_assert_status(sc);
  }

  // The configured role determines which part of the chain the device has to
  // be provisioned with.
  //   - prover role:   batch and device
  //   - verifier role: root and factory
  //   - both roles:    the full chain
#if SL_BT_CBAP_ROLE == SL_BT_CBAP_ROLE_PROVER
  CBAP_LOG_INFO("Role: Prover" CBAP_LOG_NL);
#elif SL_BT_CBAP_ROLE == SL_BT_CBAP_ROLE_VERIFIER
  CBAP_LOG_INFO("Role: Verifier" CBAP_LOG_NL);
#else
  CBAP_LOG_INFO("Role: Full (Prover and Verifier)" CBAP_LOG_NL);
#endif
  bool required[CERT_ID_COUNT];
  struct psa_storage_info_t psa_storage_info;

  for (int i = 0; i < CERT_ID_COUNT; i++) {
    required[i] = ((SL_BT_CBAP_ROLE & cert_chain_roles[i]) != 0);
    if (!required[i]) {
      continue;
    }
    // A missing certificate is a provisioning error, the device cannot
    // authenticate in the configured role without it.
    sc = psa_status_to_sl_status(psa_its_get_info(cert_chain_ids[i],
                                                  &psa_storage_info));
    if (sc != SL_STATUS_OK) {
      CBAP_LOG_ERROR("Failed to get storage info [0x%04lx] for the %s "          \
                     "certificate! Please make sure the device was provisioned " \
                     "with success!" CBAP_LOG_NL,
                     sc,
                     cert_chain_names[i]);
      app_assert_status(sc);
    }
  }

  // The root certificate is self-signed, therefore it is its own issuer.
  if (required[CERT_ID_ROOT]) {
    sc = validate_certificate(cert_chain_ids[CERT_ID_ROOT],
                              cert_chain_ids[CERT_ID_ROOT]);
    app_assert_status(sc);
    CBAP_LOG_INFO("%s certificate verified." CBAP_LOG_NL,
                  cert_chain_names[CERT_ID_ROOT]);
  }

  // Validate every required certificate against the one that issued it.
  // (Skip if issuer is outside of the required section.)
  for (int i = CERT_ID_ROOT + 1; i < CERT_ID_COUNT; i++) {
    if (!required[i] || !required[i - 1]) {
      continue;
    }
    sc = validate_certificate(cert_chain_ids[i], cert_chain_ids[i - 1]);
    app_assert_status(sc);
    CBAP_LOG_INFO("%s certificate verified against the %s certificate." CBAP_LOG_NL,
                  cert_chain_names[i],
                  cert_chain_names[i - 1]);
  }

  // If the device certificate is required, the corresponding key should exist
  // as well, and the certificate should have been issued for it.
  if (required[CERT_ID_DEVICE]) {
    sc = check_device_key();
    app_assert_status(sc);
    CBAP_LOG_INFO("%s certificate belongs to the device key." CBAP_LOG_NL,
                  cert_chain_names[CERT_ID_DEVICE]);
  }

  CBAP_LOG_INFO("Initialized." CBAP_LOG_NL);
}

// Read a certificate from the persistent storage in DER format.
sl_status_t sli_bt_cbap_get_certificate(cbap_key_id_t id,
                                        uint8_t *data,
                                        size_t *data_len,
                                        size_t max_len)
{
  if (data == NULL || data_len == NULL) {
    return SL_STATUS_NULL_POINTER;
  }

  *data_len = 0;

  // Check if certificate exists.
  sl_status_t sc;
  struct psa_storage_info_t psa_storage_info;
  sc = psa_status_to_sl_status(psa_its_get_info(id, &psa_storage_info));
  if (sc != SL_STATUS_OK) {
    CBAP_LOG_ERROR("Failed to get storage info at id 0x%04lx: 0x%04lx!" CBAP_LOG_NL,
                   (unsigned long)id,
                   sc);
    return sc;
  }

  // Check size.
  if (psa_storage_info.size > max_len) {
    CBAP_LOG_ERROR("The PSA ITS object is larger (%lu) than allowed maximum " \
                   "length (%lu)!" CBAP_LOG_NL,
                   (unsigned long)psa_storage_info.size,
                   (unsigned long)max_len);
    return SL_STATUS_NO_MORE_RESOURCE;
  }

  // Retrieve from PSA ITS. The object is known to fit into the buffer.
  sc = psa_status_to_sl_status(psa_its_get(id,
                                           0,
                                           psa_storage_info.size,
                                           data,
                                           data_len));
  if (sc != SL_STATUS_OK) {
    *data_len = 0;
    CBAP_LOG_ERROR("Failed to get certificate at id 0x%04lx: 0x%04lx. Please make " \
                   "sure the device was provisioned with success!" CBAP_LOG_NL,
                   (unsigned long)id,
                   sc);
  }

  return sc;
}

// Write a certificate to the persistent storage in DER format.
sl_status_t sli_bt_cbap_set_certificate(cbap_key_id_t id,
                                        const uint8_t *data,
                                        size_t data_len)
{
  if (data == NULL) {
    return SL_STATUS_NULL_POINTER;
  }
  if (data_len == 0) {
    return SL_STATUS_INVALID_PARAMETER;
  }

  sl_status_t sc;
  sc = psa_status_to_sl_status(psa_its_set(id,
                                           data_len,
                                           data,
                                           PSA_STORAGE_FLAG_NONE));
  if (sc != SL_STATUS_OK) {
    CBAP_LOG_ERROR("Failed to store certificate at id 0x%04lx: " \
                   "0x%04lx!" CBAP_LOG_NL,
                   (unsigned long)id,
                   sc);
  }

  return sc;
}

// Validate the certificate chain presented by the remote device.
sl_status_t sli_bt_cbap_validate_remote_chain(void)
{
  sl_status_t sc;

  // The batch certificate of the remote device has to be issued by the same
  // factory certificate this device was provisioned with. That certificate was
  // already validated against the root certificate during initialization,
  // therefore it can be used as the trust anchor of the remote chain.
  sc = validate_certificate(SL_BT_CBAP_PSA_REMOTE_BATCH_CERT,
                            cert_chain_ids[CERT_ID_FACTORY]);
  if (sc != SL_STATUS_OK) {
    CBAP_LOG_ERROR("The remote %s certificate was not issued by the local %s " \
                   "certificate!" CBAP_LOG_NL,
                   cert_chain_names[CERT_ID_BATCH],
                   cert_chain_names[CERT_ID_FACTORY]);
    return sc;
  }
  CBAP_LOG_INFO("Remote %s certificate verified against the local %s " \
                "certificate." CBAP_LOG_NL,
                cert_chain_names[CERT_ID_BATCH],
                cert_chain_names[CERT_ID_FACTORY]);

  // The device certificate is the end entity of the chain. Its issuer is the
  // batch certificate verified above, so trust reaches down to it.
  sc = validate_certificate(SL_BT_CBAP_PSA_REMOTE_DEVICE_CERT,
                            SL_BT_CBAP_PSA_REMOTE_BATCH_CERT);
  if (sc != SL_STATUS_OK) {
    CBAP_LOG_ERROR("The remote %s certificate was not issued by the " \
                   "remote %s certificate!" CBAP_LOG_NL,
                   cert_chain_names[CERT_ID_DEVICE],
                   cert_chain_names[CERT_ID_BATCH]);
    return sc;
  }
  CBAP_LOG_INFO("Remote %s certificate verified against the remote %s " \
                "certificate." CBAP_LOG_NL,
                cert_chain_names[CERT_ID_DEVICE],
                cert_chain_names[CERT_ID_BATCH]);

  // The chain is trusted, so the public key it ends with can be trusted too.
  // It is needed to verify the OOB data that the remote device signs with the
  // matching private key, which is what proves that it owns the chain.
  return import_remote_pub_key();
}

// Sign and combine OOB data.
sl_status_t sli_bt_cbap_sign_oob_data(uint8_t *device_random,
                                      uint8_t *device_confirm,
                                      uint8_t *output_data,
                                      size_t *output_len)
{
  sl_status_t sc;
  uint8_t input_data[SL_BT_CBAP_OOB_DATA_LEN];
  uint8_t signature[SL_BT_CBAP_OOB_SIGNATURE_LEN];
  size_t signature_length;

  if (output_data == NULL
      || output_len == NULL
      || device_random == NULL
      || device_confirm == NULL) {
    return SL_STATUS_NULL_POINTER;
  }

  memcpy(input_data, device_random, SL_BT_CBAP_OOB_RANDOM_LEN);
  memcpy(&input_data[SL_BT_CBAP_OOB_RANDOM_LEN],
         device_confirm,
         SL_BT_CBAP_OOB_RANDOM_LEN);

  // Sign the OOB
  sc = psa_status_to_sl_status(psa_sign_message(SL_BT_CBAP_PSA_DEVICE_KEY,
                                                PSA_ALG_ECDSA(PSA_ALG_SHA_256),
                                                input_data,
                                                sizeof(input_data),
                                                signature,
                                                sizeof(signature),
                                                &signature_length));
  if (sc != SL_STATUS_OK) {
    CBAP_LOG_ERROR("Failed to sign OOB data: 0x%04lx" CBAP_LOG_NL, sc);
    return sc;
  }

  // The remote device verifies a fixed length signature, so a shorter one
  // would leave stale bytes at the end of the output and fail there.
  if (signature_length != SL_BT_CBAP_OOB_SIGNATURE_LEN) {
    CBAP_LOG_ERROR("The device key produced a %u byte signature instead of " \
                   "%u" CBAP_LOG_NL,
                   (unsigned)signature_length,
                   (unsigned)SL_BT_CBAP_OOB_SIGNATURE_LEN);
    return SL_STATUS_INVALID_SIGNATURE;
  }

  memcpy(output_data, input_data, SL_BT_CBAP_OOB_DATA_LEN);
  memcpy(&output_data[SL_BT_CBAP_OOB_DATA_LEN], signature, signature_length);

  *output_len = SL_BT_CBAP_SIGNED_OOB_DATA_LEN;
  return SL_STATUS_OK;
}

// Verify the remote device OOB data signature.
sl_status_t sli_bt_cbap_verify_remote_oob_data(uint8_t *remote_random,
                                               uint8_t *remote_confirm,
                                               uint8_t *remote_oob_signature)
{
  uint8_t input_data[SL_BT_CBAP_OOB_DATA_LEN];
  sl_status_t sc;

  if (remote_random == NULL || remote_confirm == NULL || remote_oob_signature == NULL) {
    return SL_STATUS_NULL_POINTER;
  }

  memcpy(input_data, remote_random, SL_BT_CBAP_OOB_RANDOM_LEN);
  memcpy(&input_data[SL_BT_CBAP_OOB_RANDOM_LEN],
         remote_confirm,
         SL_BT_CBAP_OOB_RANDOM_LEN);

  sc = psa_status_to_sl_status(
    psa_verify_message(remote_pub_key_id,
                       PSA_ALG_ECDSA(PSA_ALG_SHA_256),
                       input_data,
                       sizeof(input_data),
                       remote_oob_signature,
                       SL_BT_CBAP_OOB_SIGNATURE_LEN));
  if (sc != SL_STATUS_OK) {
    CBAP_LOG_ERROR("Failed to verify remote OOB data signature: 0x%04lx" CBAP_LOG_NL, sc);
    return sc;
  }

  return SL_STATUS_OK;
}

// Destroy the keys which were used during the CBAP process.
sl_status_t sli_destroy_remote_pub_key(void)
{
  if (remote_pub_key_id == 0) {
    // Nothing was imported, for example because the local device is a prover
    // and has no remote certificate to take a key from.
    return SL_STATUS_OK;
  }

  sl_status_t sc;
  sc = psa_status_to_sl_status(psa_destroy_key(remote_pub_key_id));
  if (sc != SL_STATUS_OK) {
    CBAP_LOG_ERROR("Failed to destroy the public key of the remote device: " \
                   "0x%04lx!" CBAP_LOG_NL, sc);
    return sc;
  }

  remote_pub_key_id = 0;

  return SL_STATUS_OK;
}

/*******************************************************************************
 * Load and parse a certificate into one of the local certificate slots.
 *
 * The certificate is read from the persistent storage into the DER buffer of
 * the slot, and parsed in place, so that the parsed context refers to that
 * buffer instead of allocating a copy of it. The caller owns the resulting
 * context and has to release it with mbedtls_x509_crt_free().
 *
 * @param[in] slot Index of the target slot in @ref local_certs.
 * @param[in] id Storage ID of the certificate to load.
 *
 * @return SL_STATUS_OK on success, error code otherwise.
 ******************************************************************************/
static sl_status_t load_certificate(uint8_t slot, cbap_key_id_t id)
{
  if (slot >= LOCAL_CERT_COUNT) {
    CBAP_LOG_ERROR("Invalid local certificate slot: %u" CBAP_LOG_NL,
                   (unsigned)slot);
    return SL_STATUS_INVALID_INDEX;
  }

  cert_t *cert = &local_certs[slot];
  sl_status_t sc;
  int mbedtls_ret;

  cert->uid = id;

  sc = sli_bt_cbap_get_certificate(id,
                                   cert->der,
                                   &cert->der_len,
                                   sizeof(cert->der));
  if (sc != SL_STATUS_OK) {
    return sc;
  }

  // Parse certificate with using the already allocated DER encoded buffer.
  mbedtls_x509_crt_free(&cert->context);
  mbedtls_x509_crt_init(&cert->context);
  mbedtls_ret = mbedtls_x509_crt_parse_der_nocopy(&cert->context,
                                                  (const unsigned char *)cert->der,
                                                  cert->der_len);
  if (mbedtls_ret != 0) {
    CBAP_LOG_ERROR("Failed to parse certificate at id 0x%04lx: %d" CBAP_LOG_NL,
                   (unsigned long)id,
                   mbedtls_ret);
    return SL_STATUS_FAIL;
  }

  return SL_STATUS_OK;
}

/*******************************************************************************
 * Validate a certificate against the given issuer.
 *
 * Both certificates are read from the persistent storage into the module owned
 * buffers and parsed, then the signature of the certificate is verified
 * against the public key of the issuer, applying the certificate chain
 * security profile. The parsed contexts are released before returning.
 *
 * @note Passing the same ID as both parameters is supported, and is the
 * intended way of validating the self-signed root certificate.
 *
 * @param[in] cert Storage ID of the certificate to be validated.
 * @param[in] issuer Storage ID of the certificate that issued @p cert.
 *
 * @return SL_STATUS_OK if the certificate was issued by the issuer, error code
 *         otherwise.
 ******************************************************************************/
static sl_status_t validate_certificate(cbap_key_id_t cert,
                                        cbap_key_id_t issuer)
{
  sl_status_t sc;
  int mbedtls_ret;
  uint32_t flags;

  sc = load_certificate(LOCAL_CERT_ISSUER, issuer);
  if (sc != SL_STATUS_OK) {
    goto exit;
  }

  sc = load_certificate(LOCAL_CERT_SUBJECT, cert);
  if (sc != SL_STATUS_OK) {
    goto exit;
  }

  // Verify certificate against issuer certificate.
  mbedtls_ret = mbedtls_x509_crt_verify_with_profile(&local_certs[LOCAL_CERT_SUBJECT].context,
                                                     &local_certs[LOCAL_CERT_ISSUER].context,
                                                     NULL,
                                                     &cert_chain_profile,
                                                     NULL,
                                                     &flags,
                                                     NULL,
                                                     NULL);
  if (mbedtls_ret != 0) {
    // The flags identify the failing check, see MBEDTLS_X509_BADCERT_*.
    CBAP_LOG_ERROR("Failed to verify certificate at id 0x%04lx against the issuer " \
                   "at id 0x%04lx: %d, flags: 0x%08lx" CBAP_LOG_NL,
                   (unsigned long)local_certs[LOCAL_CERT_SUBJECT].uid,
                   (unsigned long)local_certs[LOCAL_CERT_ISSUER].uid,
                   mbedtls_ret,
                   (unsigned long)flags);
    sc = SL_STATUS_FAIL;
  }

  exit:
  // Unallocate certificate contexts (RAM heap).
  mbedtls_x509_crt_free(&local_certs[LOCAL_CERT_ISSUER].context);
  mbedtls_x509_crt_free(&local_certs[LOCAL_CERT_SUBJECT].context);
  return sc;
}

/*******************************************************************************
 * Check that the device certificate belongs to the device key.
 *
 * The public part of the key pair in the secure storage is compared against
 * the public key of the device certificate. A mismatch means that the device
 * was provisioned with a certificate that was issued for a different key, in
 * which case the remote device cannot verify the signed OOB data.
 *
 * The device certificate is loaded into the subject slot for the duration of
 * the check, and released before returning.
 *
 * @return SL_STATUS_OK if the certificate and the key match, error code
 *         otherwise.
 ******************************************************************************/
static sl_status_t check_device_key(void)
{
  uint8_t public_key[EC_PUB_KEY_LEN];
  size_t public_key_len;
  sl_status_t sc;
  const mbedtls_x509_buf *pk_raw;

  sc = load_certificate(LOCAL_CERT_SUBJECT, SL_BT_CBAP_PSA_DEVICE_CERT);
  if (sc != SL_STATUS_OK) {
    goto exit;
  }

  sc = psa_status_to_sl_status(psa_export_public_key(SL_BT_CBAP_PSA_DEVICE_KEY,
                                                     public_key,
                                                     sizeof(public_key),
                                                     &public_key_len));
  if (sc != SL_STATUS_OK) {
    CBAP_LOG_ERROR("Device key cannot be found: 0x%04lx" CBAP_LOG_NL, sc);
    sc = SL_STATUS_INVALID_KEY;
    goto exit;
  }

  // The public key point is the last element of the SubjectPublicKeyInfo
  // structure of the certificate.
  pk_raw = &local_certs[LOCAL_CERT_SUBJECT].context.pk_raw;
  if (pk_raw->len < public_key_len
      || memcmp(public_key,
                &pk_raw->p[pk_raw->len - public_key_len],
                public_key_len) != 0) {
    CBAP_LOG_ERROR("The device certificate was not issued for the device key!" CBAP_LOG_NL);
    sc = SL_STATUS_INVALID_KEY;
  }

  exit:
  mbedtls_x509_crt_free(&local_certs[LOCAL_CERT_SUBJECT].context);
  return sc;
}

/*******************************************************************************
 * Import the public key of the remote device from its device certificate.
 *
 * The key is needed to verify the OOB data that the remote device signs with
 * the matching private key. The key of a previous remote device is destroyed
 * first, so that the key slot is not leaked from procedure to procedure.
 *
 * The certificate is loaded into the subject slot for the duration of the
 * import, and released before returning.
 *
 * @return SL_STATUS_OK on success, error code otherwise.
 ******************************************************************************/
static sl_status_t import_remote_pub_key(void)
{
  sl_status_t sc;
  psa_key_attributes_t attributes = PSA_KEY_ATTRIBUTES_INIT;
  const mbedtls_x509_buf *pk_raw;

  sc = sli_destroy_remote_pub_key();
  if (sc != SL_STATUS_OK) {
    return sc;
  }

  sc = load_certificate(LOCAL_CERT_SUBJECT, SL_BT_CBAP_PSA_REMOTE_DEVICE_CERT);
  if (sc != SL_STATUS_OK) {
    goto exit;
  }

  psa_set_key_algorithm(&attributes, PSA_ALG_ECDSA(PSA_ALG_SHA_256));
  psa_set_key_type(&attributes,
                   PSA_KEY_TYPE_ECC_PUBLIC_KEY(PSA_ECC_FAMILY_SECP_R1));
  psa_set_key_usage_flags(&attributes, PSA_KEY_USAGE_VERIFY_MESSAGE);

  // The public key point is the last element of the SubjectPublicKeyInfo
  // structure of the certificate.
  pk_raw = &local_certs[LOCAL_CERT_SUBJECT].context.pk_raw;
  if (pk_raw->len < EC_PUB_KEY_LEN) {
    CBAP_LOG_ERROR("The device certificate of the remote device holds no " \
                   "EC public key!" CBAP_LOG_NL);
    sc = SL_STATUS_INVALID_KEY;
    goto exit;
  }

  sc = psa_status_to_sl_status(
    psa_import_key(&attributes,
                   &pk_raw->p[pk_raw->len - EC_PUB_KEY_LEN],
                   EC_PUB_KEY_LEN,
                   &remote_pub_key_id));
  if (sc != SL_STATUS_OK) {
    CBAP_LOG_ERROR("Failed to import the public key of the remote device: " \
                   "0x%04lx!" CBAP_LOG_NL, sc);
  }

  exit:
  mbedtls_x509_crt_free(&local_certs[LOCAL_CERT_SUBJECT].context);
  return sc;
}

/*******************************************************************************
 * Convert PSA status code to SL status code.
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
    case PSA_ERROR_DOES_NOT_EXIST:        return SL_STATUS_NOT_FOUND;
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
