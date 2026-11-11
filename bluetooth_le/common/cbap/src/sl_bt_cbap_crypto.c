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

#include "mbedtls/x509_crt.h"
#include "psa/crypto.h"
#include "psa/crypto_values.h"
#include "psa_crypto_its.h"

#include "app_assert.h"
#include "sli_bt_cbap.h"
#include "sl_bt_cbap.h"

// -----------------------------------------------------------------------------
// Defines

#define EC_PUB_KEY_LEN                  SLI_BT_CBAP_SESSION_PUBLIC_KEY_LEN
#define ECDSA_P256_SIG_LEN              SLI_BT_CBAP_SESSION_SIGNATURE_LEN
#define SESSION_DATA_LEN                SLI_BT_CBAP_SESSION_DATA_LEN
#define SHARED_SECRET_LEN               (32U)
#define P256_CURVE_SIZE_BITS            (256U)
#define UNCOMPRESSED_KEY_FORMAT_PREFIX  (0x04)

#define ENCRYPTION_KEY_SIZE_BITS        (128U)
#define DERIVATION_IV_LEN               (12U)
#define DERIVATION_SALT                 "CBAP+HKDFv1"
#define DERIVATION_ENC_INFO             "CBAP_PLUS_KEYS"
#define DERIVATION_IV_INFO              "CBAP_PLUS_IV"

// For 6-digit passkey generation, rejection sampling exhaustion is extremely
// unlikely.
#define ITERATIONS_MAX                  (64)

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
  cbap_key_id_t uid;
  mbedtls_x509_crt context;
  uint8_t der[SL_BT_CBAP_CERTIFICATE_MAX_SIZE];
  size_t der_len;
} cert_t;

// Security session structure
typedef struct {
  // Session key
  mbedtls_svc_key_id_t key_id;
  // Derived AES encryption key
  mbedtls_svc_key_id_t encryption_key_id;
  // Derived initialization vector (raw bytes)
  uint8_t iv[DERIVATION_IV_LEN];
} session_t;

// -----------------------------------------------------------------------------
// Private variables

// Array for storing two certificates simultaneously in order to validate
// them against each other
static cert_t local_certs[LOCAL_CERT_COUNT];

// Storage ID of each certificate of the chain that belongs to this device.
// (Certificate chain for the remote is handled separately.)
static const cbap_key_id_t cert_chain_ids[CERT_ID_COUNT] = {
  [CERT_ID_ROOT] = SL_BT_CBAP_PSA_ROOT_CERT,
  [CERT_ID_FACTORY] = SL_BT_CBAP_PSA_FACTORY_CERT,
  [CERT_ID_BATCH] = SL_BT_CBAP_PSA_BATCH_CERT,
  [CERT_ID_DEVICE] = SL_BT_CBAP_PSA_DEVICE_CERT,
};

// Name of each certificate of the chain, for logging purposes.
static const char *const cert_chain_names[CERT_ID_COUNT] = {
  [CERT_ID_ROOT] = "root",
  [CERT_ID_FACTORY] = "factory",
  [CERT_ID_BATCH] = "batch",
  [CERT_ID_DEVICE] = "device",
};

// The roles that need each certificate of the chain. A prover sends its device
// and batch certificates to prove itself, a verifier validates the received
// certificates against its factory and root certificates.
static const uint8_t cert_chain_roles[CERT_ID_COUNT] = {
  [CERT_ID_ROOT] = SL_BT_CBAP_ROLE_VERIFIER,
  [CERT_ID_FACTORY] = SL_BT_CBAP_ROLE_VERIFIER,
  [CERT_ID_BATCH] = SL_BT_CBAP_ROLE_PROVER,
  [CERT_ID_DEVICE] = SL_BT_CBAP_ROLE_PROVER,
};

// Certificate chain security profile, set up by sli_bt_cbap_crypto_init().
static mbedtls_x509_crt_profile cert_chain_profile;

// Public key ID of the remote device
static mbedtls_svc_key_id_t remote_pub_key_id;

// Security session
static session_t session = {
  .key_id = MBEDTLS_SVC_KEY_ID_INIT,
  .encryption_key_id = MBEDTLS_SVC_KEY_ID_INIT,
};

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
// Internal function definitions

/******************************************************************************
 * Initialize the cryptographic part of the component.
 *****************************************************************************/
void sli_bt_cbap_crypto_init(void)
{
  // Check device security capability.
  SYSTEM_SecurityCapability_TypeDef capability;
  capability = SYSTEM_GetSecurityCapability();
  if ((capability != securityCapabilityRoT)
      && (capability != securityCapabilitySE)
      && (capability != securityCapabilityVault)) {
    SLI_BT_CBAP_LOG_ERROR("Invalid security capability: %d" SLI_BT_CBAP_LOG_NL,
                          capability);
    app_assert_status(SL_STATUS_NOT_SUPPORTED);
  }

  // Initialize PSA crypto.
  sl_status_t sc;
  sc = psa_status_to_sl_status(psa_crypto_init());
  if (sc != SL_STATUS_OK) {
    SLI_BT_CBAP_LOG_ERROR(
      "Failed to initialize PSA crypto: 0x%04lx" SLI_BT_CBAP_LOG_NL, sc);
    app_assert_status(sc);
  }

  remote_pub_key_id = MBEDTLS_SVC_KEY_ID_INIT;

  // The configured role determines which part of the chain the device has to
  // be provisioned with.
  //   - prover role:   batch and device
  //   - verifier role: root and factory
  //   - both roles:    the full chain
#if SL_BT_CBAP_ROLE == SL_BT_CBAP_ROLE_PROVER
  SLI_BT_CBAP_LOG_INFO("Role: Prover" SLI_BT_CBAP_LOG_NL);
#elif SL_BT_CBAP_ROLE == SL_BT_CBAP_ROLE_VERIFIER
  SLI_BT_CBAP_LOG_INFO("Role: Verifier" SLI_BT_CBAP_LOG_NL);
#else
  SLI_BT_CBAP_LOG_INFO("Role: Full (Prover and Verifier)" SLI_BT_CBAP_LOG_NL);
#endif
  SLI_BT_CBAP_LOG_INFO("Bonding: %sabled" SLI_BT_CBAP_LOG_NL,
                       SL_BT_CBAP_BONDING ? "en" : "dis");

  bool required[CERT_ID_COUNT];
  struct psa_storage_info_t psa_storage_info;

  for (int i = 0; i < CERT_ID_COUNT; i++) {
    required[i] = ((SL_BT_CBAP_ROLE & cert_chain_roles[i]) != 0);
    if (!required[i]) {
      continue;
    }
    // A missing certificate is a provisioning error, the device cannot
    // authenticate in the configured role without it.
    sc = psa_status_to_sl_status(
      psa_its_get_info(cert_chain_ids[i], &psa_storage_info));
    if (sc != SL_STATUS_OK) {
      SLI_BT_CBAP_LOG_ERROR("Failed to get storage info [0x%04lx] for the %s "
                            "certificate! Please make sure the device was "
                            "provisioned with success!" SLI_BT_CBAP_LOG_NL,
                            sc,
                            cert_chain_names[i]);
      app_assert_status(sc);
    }
  }

  // Mbed TLS matches allowed_pks against two different identifiers: the
  // signature algorithm of every certificate of the chain, and the public key
  // type of the end entity. An ECDSA signed certificate carrying an EC public
  // key needs a bit for each, and the key type has no public name, so both are
  // inherited from the Suite B preset, which is the ECDSA only profile of the
  // library. Narrow it down to the single hash and curve that CBAP uses.
  cert_chain_profile = mbedtls_x509_crt_profile_suiteb;
  cert_chain_profile.allowed_mds = MBEDTLS_X509_ID_FLAG(MBEDTLS_MD_SHA256);
  cert_chain_profile.allowed_curves =
    MBEDTLS_X509_ID_FLAG(MBEDTLS_ECP_DP_SECP256R1);

  // The root certificate is self-signed, therefore it is its own issuer.
  if (required[CERT_ID_ROOT]) {
    sc = validate_certificate(cert_chain_ids[CERT_ID_ROOT],
                              cert_chain_ids[CERT_ID_ROOT]);
    app_assert_status(sc);
    SLI_BT_CBAP_LOG_DEBUG("%s certificate verified." SLI_BT_CBAP_LOG_NL,
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
    SLI_BT_CBAP_LOG_DEBUG("%s certificate verified against the %s "
                          "certificate." SLI_BT_CBAP_LOG_NL,
                          cert_chain_names[i],
                          cert_chain_names[i - 1]);
  }

  // If the device certificate is required, the corresponding key should exist
  // as well, and the certificate should have been issued for it.
  if (required[CERT_ID_DEVICE]) {
    sc = check_device_key();
    app_assert_status(sc);
    SLI_BT_CBAP_LOG_DEBUG("%s certificate belongs to the device key."
                          SLI_BT_CBAP_LOG_NL,
                          cert_chain_names[CERT_ID_DEVICE]);
  }

  sc = sli_bt_cbap_session_clear();
  app_assert_status(sc);
  SLI_BT_CBAP_LOG_INFO("Initialized." SLI_BT_CBAP_LOG_NL);
}

/******************************************************************************
 * Read a certificate from the persistent storage in DER format.
 *****************************************************************************/
sl_status_t sli_bt_cbap_get_certificate(cbap_key_id_t id,
                                        uint8_t *data,
                                        size_t *data_len,
                                        size_t max_len)
{
  if ((data == NULL) || (data_len == NULL)) {
    return SL_STATUS_NULL_POINTER;
  }

  *data_len = 0;

  // Check if certificate exists.
  sl_status_t sc;
  struct psa_storage_info_t psa_storage_info;
  sc = psa_status_to_sl_status(psa_its_get_info(id, &psa_storage_info));
  if (sc != SL_STATUS_OK) {
    SLI_BT_CBAP_LOG_ERROR(
      "Failed to get storage info at id 0x%04lx: 0x%04lx!" SLI_BT_CBAP_LOG_NL,
      (unsigned long)id,
      sc);
    return sc;
  }

  // Check size.
  if (psa_storage_info.size > max_len) {
    SLI_BT_CBAP_LOG_ERROR(
      "The PSA ITS object is larger (%lu) than allowed maximum "
      "length (%lu)!" SLI_BT_CBAP_LOG_NL,
      (unsigned long)psa_storage_info.size,
      (unsigned long)max_len);
    return SL_STATUS_NO_MORE_RESOURCE;
  }

  // Retrieve from PSA ITS. The object is known to fit into the buffer.
  sc = psa_status_to_sl_status(
    psa_its_get(id, 0, psa_storage_info.size, data, data_len));
  if (sc != SL_STATUS_OK) {
    *data_len = 0u;
    SLI_BT_CBAP_LOG_ERROR(
      "Failed to get certificate at id 0x%04lx: 0x%04lx. Please make "
      "sure the device was provisioned with success!" SLI_BT_CBAP_LOG_NL,
      (unsigned long)id,
      sc);
  }

  return sc;
}

/******************************************************************************
 * Write a certificate to the persistent storage in DER format.
 *****************************************************************************/
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
  sc = psa_status_to_sl_status(
    psa_its_set(id, data_len, data, PSA_STORAGE_FLAG_NONE));
  if (sc != SL_STATUS_OK) {
    SLI_BT_CBAP_LOG_ERROR("Failed to store certificate at id 0x%04lx: "
                          "0x%04lx!" SLI_BT_CBAP_LOG_NL,
                          (unsigned long)id,
                          sc);
  }

  return sc;
}

/******************************************************************************
 * Validate the certificate chain presented by the remote device.
 *****************************************************************************/
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
    SLI_BT_CBAP_LOG_ERROR(
      "The remote %s certificate was not issued by the local %s "
      "certificate!" SLI_BT_CBAP_LOG_NL,
      cert_chain_names[CERT_ID_BATCH],
      cert_chain_names[CERT_ID_FACTORY]);
    return sc;
  }
  SLI_BT_CBAP_LOG_INFO("Remote %s certificate verified against the local %s "
                       "certificate." SLI_BT_CBAP_LOG_NL,
                       cert_chain_names[CERT_ID_BATCH],
                       cert_chain_names[CERT_ID_FACTORY]);

  // The device certificate is the end entity of the chain. Its issuer is the
  // batch certificate verified above, so trust reaches down to it.
  sc = validate_certificate(SL_BT_CBAP_PSA_REMOTE_DEVICE_CERT,
                            SL_BT_CBAP_PSA_REMOTE_BATCH_CERT);
  if (sc != SL_STATUS_OK) {
    SLI_BT_CBAP_LOG_ERROR("The remote %s certificate was not issued by the "
                          "remote %s certificate!" SLI_BT_CBAP_LOG_NL,
                          cert_chain_names[CERT_ID_DEVICE],
                          cert_chain_names[CERT_ID_BATCH]);
    return sc;
  }
  SLI_BT_CBAP_LOG_INFO("Remote %s certificate verified against the remote %s "
                       "certificate." SLI_BT_CBAP_LOG_NL,
                       cert_chain_names[CERT_ID_DEVICE],
                       cert_chain_names[CERT_ID_BATCH]);

  // The chain is now trusted. Continue with the security session to prove that
  // the peer device actually owns this certificate chain and to establish
  // authenticated secure connection.
  return import_remote_pub_key();
}

/******************************************************************************
 * Initialize the security session.
 *****************************************************************************/
sl_status_t sli_bt_cbap_session_init(uint8_t *data,
                                     size_t *data_len,
                                     size_t max_len)
{
  if ((data == NULL) || (data_len == NULL)) {
    return SL_STATUS_NULL_POINTER;
  }
  *data_len = 0u;
  if (max_len < SESSION_DATA_LEN) {
    return SL_STATUS_WOULD_OVERFLOW;
  }

  sl_status_t sc;
  psa_key_attributes_t attributes = PSA_KEY_ATTRIBUTES_INIT;

  if (session.key_id != MBEDTLS_SVC_KEY_ID_INIT) {
    SLI_BT_CBAP_LOG_ERROR("Session already exists!" SLI_BT_CBAP_LOG_NL);
    return SL_STATUS_INVALID_STATE;
  }

  // Generate P-256 ECDH ephemeral key
  psa_set_key_usage_flags(&attributes, PSA_KEY_USAGE_DERIVE);
  psa_set_key_algorithm(&attributes, PSA_ALG_ECDH);
  psa_set_key_type(&attributes,
                   PSA_KEY_TYPE_ECC_KEY_PAIR(PSA_ECC_FAMILY_SECP_R1));
  psa_set_key_bits(&attributes, P256_CURVE_SIZE_BITS);
  psa_set_key_lifetime(&attributes, PSA_KEY_LIFETIME_VOLATILE);
  sc = psa_status_to_sl_status(psa_generate_key(&attributes, &session.key_id));
  if (sc != SL_STATUS_OK) {
    SLI_BT_CBAP_LOG_ERROR("Failed to generate ephemeral ECDH key: 0x%04lx"
                          SLI_BT_CBAP_LOG_NL,
                          sc);
    goto abort;
  }

  // Export the public part
  uint8_t key_public[EC_PUB_KEY_LEN];
  size_t key_public_len = 0u;
  sc = psa_status_to_sl_status(psa_export_public_key(session.key_id,
                                                     key_public,
                                                     sizeof(key_public),
                                                     &key_public_len));
  if (sc != SL_STATUS_OK) {
    SLI_BT_CBAP_LOG_ERROR("Failed to export public key: 0x%04lx"
                          SLI_BT_CBAP_LOG_NL,
                          sc);
    goto abort;
  }
  SLI_BT_CBAP_LOG_DEBUG("Ephemeral ECDH key generated." SLI_BT_CBAP_LOG_NL);

  // Sign the public key with the private key of the device certificate.
  uint8_t key_signature[ECDSA_P256_SIG_LEN];
  size_t key_signature_len = 0u;

  if (SLI_BT_CBAP_ROLE_IS_PROVER) {
    sc = psa_status_to_sl_status(
      psa_sign_message(SL_BT_CBAP_PSA_DEVICE_KEY,
                       PSA_ALG_ECDSA(PSA_ALG_SHA_256),
                       key_public,
                       key_public_len,
                       key_signature,
                       sizeof(key_signature),
                       &key_signature_len));

    if (sc != SL_STATUS_OK) {
      SLI_BT_CBAP_LOG_ERROR("Failed to sign public key: 0x%04lx"
                            SLI_BT_CBAP_LOG_NL,
                            sc);
      goto abort;
    }

    SLI_BT_CBAP_LOG_DEBUG("ECDH key signature created." SLI_BT_CBAP_LOG_NL);
  }

  if ((key_public_len != EC_PUB_KEY_LEN)
      || (SLI_BT_CBAP_ROLE_IS_PROVER
          && (key_signature_len != ECDSA_P256_SIG_LEN))) {
    sc = SL_STATUS_INVALID_SIGNATURE;
    goto abort;
  }

  memcpy(data, key_public, key_public_len);
  memcpy(&data[key_public_len], key_signature, key_signature_len);
  *data_len = key_public_len + key_signature_len;
  psa_reset_key_attributes(&attributes);

  SLI_BT_CBAP_LOG_INFO("Session created." SLI_BT_CBAP_LOG_NL);
  return SL_STATUS_OK;

  abort:
  *data_len = 0u;
  psa_reset_key_attributes(&attributes);
  if (session.key_id != MBEDTLS_SVC_KEY_ID_INIT) {
    (void)psa_destroy_key(session.key_id);
    session.key_id = MBEDTLS_SVC_KEY_ID_INIT;
  }
  return sc;
}

/******************************************************************************
 * Complete the security session with the session data of the peer device.
 *****************************************************************************/
sl_status_t sli_bt_cbap_session_process(const uint8_t *data, size_t data_len)
{
  if (data == NULL) {
    return SL_STATUS_NULL_POINTER;
  }
  if ((SLI_BT_CBAP_ROLE_IS_VERIFIER && (data_len != SESSION_DATA_LEN))
      || (!SLI_BT_CBAP_ROLE_IS_VERIFIER
          && (data_len != EC_PUB_KEY_LEN)
          && (data_len != SESSION_DATA_LEN))) {
    return SL_STATUS_INVALID_PARAMETER;
  }
  if (session.key_id == MBEDTLS_SVC_KEY_ID_INIT) {
    SLI_BT_CBAP_LOG_ERROR("Session was not initialized!" SLI_BT_CBAP_LOG_NL);
    return SL_STATUS_INVALID_STATE;
  }

  psa_key_attributes_t attributes = PSA_KEY_ATTRIBUTES_INIT;
  psa_key_derivation_operation_t derivation = PSA_KEY_DERIVATION_OPERATION_INIT;
  mbedtls_svc_key_id_t shared_secret_key_id = MBEDTLS_SVC_KEY_ID_INIT;
  uint8_t shared_secret[SHARED_SECRET_LEN];
  size_t shared_secret_length;

  // Peer session key
  uint8_t peer_key_public[EC_PUB_KEY_LEN];
  memcpy(peer_key_public, data, EC_PUB_KEY_LEN);
  // Peer session key signature
  uint8_t peer_key_signature[ECDSA_P256_SIG_LEN];
  memset(peer_key_signature, 0, sizeof(peer_key_signature));
  if (data_len == SESSION_DATA_LEN) {
    memcpy(peer_key_signature, &data[EC_PUB_KEY_LEN], ECDSA_P256_SIG_LEN);
  }

  // Only verifiers have a trusted peer certificate public key.
  sl_status_t sc = SL_STATUS_OK;
  if (SLI_BT_CBAP_ROLE_IS_VERIFIER) {
    sc = psa_status_to_sl_status(
      psa_verify_message(remote_pub_key_id,
                         PSA_ALG_ECDSA(PSA_ALG_SHA_256),
                         peer_key_public,
                         EC_PUB_KEY_LEN,
                         peer_key_signature,
                         ECDSA_P256_SIG_LEN));
    if (sc != SL_STATUS_OK) {
      SLI_BT_CBAP_LOG_ERROR("Signature verification error: 0x%04lx"
                            SLI_BT_CBAP_LOG_NL,
                            sc);
      goto cleanup;
    }
    SLI_BT_CBAP_LOG_DEBUG("Signature verified." SLI_BT_CBAP_LOG_NL);
  }

  // Compute shared secret
  if (peer_key_public[0] != UNCOMPRESSED_KEY_FORMAT_PREFIX) {
    SLI_BT_CBAP_LOG_ERROR("Peer public key is expected to be in uncompressed "
                          "format!" SLI_BT_CBAP_LOG_NL);
    sc = SL_STATUS_INVALID_PARAMETER;
    goto cleanup;
  }

  // Perform ECDH key agreement to get raw shared secret
  sc = psa_status_to_sl_status(
    psa_raw_key_agreement(PSA_ALG_ECDH,
                          session.key_id,
                          peer_key_public,
                          EC_PUB_KEY_LEN,
                          shared_secret,
                          SHARED_SECRET_LEN,
                          &shared_secret_length));
  if (sc != SL_STATUS_OK) {
    SLI_BT_CBAP_LOG_ERROR("Key agreement error: 0x%04lx" SLI_BT_CBAP_LOG_NL,
                          sc);
    goto cleanup;
  }

  // Import shared secret as PSA key for derivation
  psa_set_key_usage_flags(&attributes, PSA_KEY_USAGE_DERIVE);
  psa_set_key_algorithm(&attributes, PSA_ALG_HKDF(PSA_ALG_SHA_256));
  psa_set_key_type(&attributes, PSA_KEY_TYPE_DERIVE);
  psa_set_key_bits(&attributes, PSA_BYTES_TO_BITS(SHARED_SECRET_LEN));
  psa_set_key_lifetime(&attributes, PSA_KEY_LIFETIME_VOLATILE);

  sc = psa_status_to_sl_status(
    psa_import_key(&attributes,
                   shared_secret,
                   shared_secret_length,
                   &shared_secret_key_id));

  if (sc != SL_STATUS_OK) {
    SLI_BT_CBAP_LOG_ERROR("Shared secret import error: 0x%04lx"
                          SLI_BT_CBAP_LOG_NL,
                          sc);
    goto cleanup;
  }
  SLI_BT_CBAP_LOG_DEBUG("Shared secret created." SLI_BT_CBAP_LOG_NL);

  // Derive encryption key
  if (session.encryption_key_id != MBEDTLS_SVC_KEY_ID_INIT) {
    SLI_BT_CBAP_LOG_ERROR("Encryption key already exists! Destroying."
                          SLI_BT_CBAP_LOG_NL);
    sc = psa_status_to_sl_status(psa_destroy_key(session.encryption_key_id));
    if (sc != SL_STATUS_OK) {
      SLI_BT_CBAP_LOG_ERROR("Failed to destroy existing encryption key: "
                            "0x%04lx" SLI_BT_CBAP_LOG_NL,
                            sc);
      goto cleanup;
    }
    session.encryption_key_id = MBEDTLS_SVC_KEY_ID_INIT;
  }

  const uint8_t *salt = (const uint8_t *)DERIVATION_SALT;
  const uint8_t *enc_info = (const uint8_t *)DERIVATION_ENC_INFO;
  const uint8_t *iv_info = (const uint8_t *)DERIVATION_IV_INFO;

  sc = psa_status_to_sl_status(
    psa_key_derivation_setup(&derivation, PSA_ALG_HKDF(PSA_ALG_SHA_256)));
  if (sc == SL_STATUS_OK) {
    sc = psa_status_to_sl_status(
      psa_key_derivation_input_bytes(&derivation,
                                     PSA_KEY_DERIVATION_INPUT_SALT,
                                     salt,
                                     (sizeof(DERIVATION_SALT) - 1)));
  }
  if (sc == SL_STATUS_OK) {
    sc = psa_status_to_sl_status(
      psa_key_derivation_input_key(&derivation,
                                   PSA_KEY_DERIVATION_INPUT_SECRET,
                                   shared_secret_key_id));
  }
  if (sc == SL_STATUS_OK) {
    sc = psa_status_to_sl_status(
      psa_key_derivation_input_bytes(&derivation,
                                     PSA_KEY_DERIVATION_INPUT_INFO,
                                     enc_info,
                                     (sizeof(DERIVATION_ENC_INFO) - 1)));
  }
  if (sc == SL_STATUS_OK) {
    psa_reset_key_attributes(&attributes);
    psa_set_key_usage_flags(&attributes,
                            PSA_KEY_USAGE_ENCRYPT | PSA_KEY_USAGE_DECRYPT);
    psa_set_key_algorithm(&attributes, PSA_ALG_CCM);
    psa_set_key_type(&attributes, PSA_KEY_TYPE_AES);
    psa_set_key_bits(&attributes, ENCRYPTION_KEY_SIZE_BITS);
    psa_set_key_lifetime(&attributes, PSA_KEY_LIFETIME_VOLATILE);

    sc = psa_status_to_sl_status(
      psa_key_derivation_output_key(&attributes,
                                    &derivation,
                                    &session.encryption_key_id));
    psa_reset_key_attributes(&attributes);
  }
  (void)psa_key_derivation_abort(&derivation);

  if (sc != SL_STATUS_OK) {
    SLI_BT_CBAP_LOG_ERROR("Failed to derive encryption key: 0x%04lx"
                          SLI_BT_CBAP_LOG_NL,
                          sc);
    (void)psa_destroy_key(session.encryption_key_id);
    session.encryption_key_id = MBEDTLS_SVC_KEY_ID_INIT;
    goto cleanup;
  }
  SLI_BT_CBAP_LOG_DEBUG("Encryption key derived." SLI_BT_CBAP_LOG_NL);

  // Derive initialization vector (IV)
  sc = psa_status_to_sl_status(
    psa_key_derivation_setup(&derivation, PSA_ALG_HKDF(PSA_ALG_SHA_256)));
  if (sc == SL_STATUS_OK) {
    sc = psa_status_to_sl_status(
      psa_key_derivation_input_bytes(&derivation,
                                     PSA_KEY_DERIVATION_INPUT_SALT,
                                     salt,
                                     (sizeof(DERIVATION_SALT) - 1)));
  }
  if (sc == SL_STATUS_OK) {
    sc = psa_status_to_sl_status(
      psa_key_derivation_input_key(&derivation,
                                   PSA_KEY_DERIVATION_INPUT_SECRET,
                                   shared_secret_key_id));
  }
  if (sc == SL_STATUS_OK) {
    sc = psa_status_to_sl_status(
      psa_key_derivation_input_bytes(&derivation,
                                     PSA_KEY_DERIVATION_INPUT_INFO,
                                     iv_info,
                                     (sizeof(DERIVATION_IV_INFO) - 1)));
  }
  if (sc == SL_STATUS_OK) {
    sc = psa_status_to_sl_status(
      psa_key_derivation_output_bytes(&derivation,
                                      session.iv,
                                      DERIVATION_IV_LEN));
  }

  if (sc != SL_STATUS_OK) {
    SLI_BT_CBAP_LOG_ERROR("Failed to derive initialization vector: 0x%04lx"
                          SLI_BT_CBAP_LOG_NL,
                          sc);
    goto cleanup;
  }

  SLI_BT_CBAP_LOG_DEBUG("Initialization vector derived." SLI_BT_CBAP_LOG_NL);
  SLI_BT_CBAP_LOG_INFO("Session complete." SLI_BT_CBAP_LOG_NL);

  cleanup:
  // Clear local variables
  psa_reset_key_attributes(&attributes);
  (void)psa_key_derivation_abort(&derivation);
  memset(shared_secret, 0, sizeof(shared_secret));
  memset(peer_key_public, 0, sizeof(peer_key_public));
  memset(peer_key_signature, 0, sizeof(peer_key_signature));

  if (shared_secret_key_id != MBEDTLS_SVC_KEY_ID_INIT) {
    (void)psa_destroy_key(shared_secret_key_id);
    shared_secret_key_id = MBEDTLS_SVC_KEY_ID_INIT;
  }

  // Clear output on failure
  if (sc != SL_STATUS_OK) {
    if (session.encryption_key_id != MBEDTLS_SVC_KEY_ID_INIT) {
      (void)psa_destroy_key(session.encryption_key_id);
      session.encryption_key_id = MBEDTLS_SVC_KEY_ID_INIT;
    }

    memset(session.iv, 0, DERIVATION_IV_LEN);
  }

  return sc;
}

/******************************************************************************
 * Clear the security session.
 *****************************************************************************/
sl_status_t sli_bt_cbap_session_clear(void)
{
  sl_status_t sc;

  // Destroy keys if exist
  if (session.key_id != MBEDTLS_SVC_KEY_ID_INIT) {
    SLI_BT_CBAP_LOG_DEBUG("Destroying session key." SLI_BT_CBAP_LOG_NL);
    sc = psa_status_to_sl_status(psa_destroy_key(session.key_id));
    if (sc != SL_STATUS_OK) {
      SLI_BT_CBAP_LOG_ERROR("Failed to destroy session key."
                            SLI_BT_CBAP_LOG_NL);
      return sc;
    }
    // Clear ID in case we fail to destroy the encryption key
    session.key_id = MBEDTLS_SVC_KEY_ID_INIT;
  }
  if (session.encryption_key_id != MBEDTLS_SVC_KEY_ID_INIT) {
    SLI_BT_CBAP_LOG_DEBUG("Destroying encryption key." SLI_BT_CBAP_LOG_NL);
    sc = psa_status_to_sl_status(psa_destroy_key(session.encryption_key_id));
    if (sc != SL_STATUS_OK) {
      SLI_BT_CBAP_LOG_ERROR("Failed to destroy encryption key."
                            SLI_BT_CBAP_LOG_NL);
      return sc;
    }
  }

  // Clear structure
  memset(&session, 0, sizeof(session_t));

  // Initialize to invalid ID
  session.key_id = MBEDTLS_SVC_KEY_ID_INIT;
  session.encryption_key_id = MBEDTLS_SVC_KEY_ID_INIT;

  return SL_STATUS_OK;
}

/******************************************************************************
 * Encrypt a message with the security session.
 *****************************************************************************/
sl_status_t sli_bt_cbap_session_encrypt(const uint8_t *data_in,
                                        size_t data_in_len,
                                        uint8_t *data_out,
                                        size_t *data_out_len,
                                        size_t data_out_maxlen)
{
  if ((data_in == NULL) || (data_out == NULL) || (data_out_len == NULL)) {
    return SL_STATUS_NULL_POINTER;
  }
  if ((data_in_len == 0) || (data_out_maxlen == 0)) {
    return SL_STATUS_INVALID_PARAMETER;
  }
  if (session.encryption_key_id == MBEDTLS_SVC_KEY_ID_INIT) {
    SLI_BT_CBAP_LOG_ERROR("Session is incomplete!" SLI_BT_CBAP_LOG_NL);
    return SL_STATUS_INVALID_STATE;
  }

  sl_status_t sc;
  sc = psa_status_to_sl_status(
    psa_aead_encrypt(session.encryption_key_id,
                     PSA_ALG_CCM,
                     session.iv,
                     DERIVATION_IV_LEN,
                     NULL,
                     0u,
                     data_in,
                     data_in_len,
                     data_out,
                     data_out_maxlen,
                     data_out_len));
  if (sc != SL_STATUS_OK) {
    SLI_BT_CBAP_LOG_DEBUG("Encryption failure: 0x%04lx" SLI_BT_CBAP_LOG_NL,
                          sc);
    memset(data_out, 0, data_out_maxlen);
    *data_out_len = 0u;
  }

  return sc;
}

/******************************************************************************
 * Authenticate and decrypt a message with the security session.
 *****************************************************************************/
sl_status_t sli_bt_cbap_session_decrypt(const uint8_t *data_in,
                                        size_t data_in_len,
                                        uint8_t *data_out,
                                        size_t *data_out_len,
                                        size_t data_out_maxlen)
{
  if ((data_in == NULL) || (data_out == NULL) || (data_out_len == NULL)) {
    return SL_STATUS_NULL_POINTER;
  }
  if ((data_in_len == 0) || (data_out_maxlen == 0)) {
    return SL_STATUS_INVALID_PARAMETER;
  }
  if (session.encryption_key_id == MBEDTLS_SVC_KEY_ID_INIT) {
    SLI_BT_CBAP_LOG_ERROR("Session is incomplete!" SLI_BT_CBAP_LOG_NL);
    return SL_STATUS_INVALID_STATE;
  }

  sl_status_t sc;
  sc = psa_status_to_sl_status(
    psa_aead_decrypt(session.encryption_key_id,
                     PSA_ALG_CCM,
                     session.iv,
                     DERIVATION_IV_LEN,
                     NULL,
                     0u,
                     data_in,
                     data_in_len,
                     data_out,
                     data_out_maxlen,
                     data_out_len));
  if (sc != SL_STATUS_OK) {
    SLI_BT_CBAP_LOG_DEBUG("Decryption failure: 0x%04lx" SLI_BT_CBAP_LOG_NL,
                          sc);
    memset(data_out, 0, data_out_maxlen);
    *data_out_len = 0u;
  }

  return sc;
}

/******************************************************************************
 * Generate a uniformly distributed random number in the given range.
 *****************************************************************************/
sl_status_t sli_bt_cbap_generate_random(uint32_t min_inclusive,
                                        uint32_t max_exclusive,
                                        uint32_t *random)
{
  if (random == NULL) {
    return SL_STATUS_NULL_POINTER;
  }
  if (min_inclusive >= max_exclusive) {
    return SL_STATUS_INVALID_PARAMETER;
  }
  if (min_inclusive == (max_exclusive - 1u)) {
    *random = min_inclusive;
    return SL_STATUS_OK;
  }

  const uint32_t range = max_exclusive - min_inclusive;
  const uint32_t threshold = (0u - range) % range;
  sl_status_t sc = SL_STATUS_FAIL;
  uint32_t random_value;

  for (uint8_t i = 0u; i < ITERATIONS_MAX; i++) {
    sc = psa_status_to_sl_status(psa_generate_random((uint8_t *)&random_value,
                                                     sizeof(random_value)));
    // Rejection sampling to avoid bias
    if ((sc == SL_STATUS_OK) && (random_value >= threshold)) {
      *random = min_inclusive + (random_value % range);
      return SL_STATUS_OK;
    }
  }

  if (sc == SL_STATUS_OK) {
    SLI_BT_CBAP_LOG_ERROR("Failed to generate random: rejection sampling "
                          "exhaustion." SLI_BT_CBAP_LOG_NL);
    sc = SL_STATUS_ABORT;
  } else {
    SLI_BT_CBAP_LOG_ERROR("Failed to generate random: 0x%04lx"
                          SLI_BT_CBAP_LOG_NL,
                          sc);
  }

  return sc;
}

/******************************************************************************
 * Destroy the public key imported from the remote device certificate.
 *****************************************************************************/
sl_status_t sli_bt_cbap_destroy_peer_pub_key(void)
{
  if (remote_pub_key_id == MBEDTLS_SVC_KEY_ID_INIT) {
    // Nothing was imported, for example because the local device is a prover
    // and has no remote certificate to take a key from.
    return SL_STATUS_OK;
  }

  sl_status_t sc;
  sc = psa_status_to_sl_status(psa_destroy_key(remote_pub_key_id));
  if (sc != SL_STATUS_OK) {
    SLI_BT_CBAP_LOG_ERROR(
      "Failed to destroy the public key of the remote device: "
      "0x%04lx!" SLI_BT_CBAP_LOG_NL,
      sc);
    return sc;
  }

  remote_pub_key_id = MBEDTLS_SVC_KEY_ID_INIT;

  return SL_STATUS_OK;
}

// -----------------------------------------------------------------------------
// Private function definitions

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
    SLI_BT_CBAP_LOG_ERROR(
      "Invalid local certificate slot: %u" SLI_BT_CBAP_LOG_NL,
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

  // Parse the certificate in place, in the already allocated DER buffer.
  mbedtls_x509_crt_free(&cert->context);
  mbedtls_x509_crt_init(&cert->context);
  mbedtls_ret = mbedtls_x509_crt_parse_der_nocopy(
    &cert->context,
    (const unsigned char *)cert->der,
    cert->der_len);
  if (mbedtls_ret != 0) {
    SLI_BT_CBAP_LOG_ERROR(
      "Failed to parse certificate at id 0x%04lx: %d" SLI_BT_CBAP_LOG_NL,
      (unsigned long)id, mbedtls_ret);
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
 * @return SL_STATUS_OK if the certificate was issued by the issuer,
 *         SL_STATUS_INVALID_SIGNATURE if it was not, error code otherwise.
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
  mbedtls_ret = mbedtls_x509_crt_verify_with_profile(
    &local_certs[LOCAL_CERT_SUBJECT].context,
    &local_certs[LOCAL_CERT_ISSUER].context,
    NULL,
    &cert_chain_profile,
    NULL,
    &flags,
    NULL,
    NULL);
  if (mbedtls_ret != 0) {
    // The flags identify the failing check, see MBEDTLS_X509_BADCERT_*.
    SLI_BT_CBAP_LOG_ERROR(
      "Failed to verify certificate at id 0x%04lx against the issuer "
      "at id 0x%04lx: %d, flags: 0x%08lx" SLI_BT_CBAP_LOG_NL,
      (unsigned long)local_certs[LOCAL_CERT_SUBJECT].uid,
      (unsigned long)local_certs[LOCAL_CERT_ISSUER].uid,
      mbedtls_ret,
      (unsigned long)flags);
    sc = SL_STATUS_INVALID_SIGNATURE;
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
 * which case the remote device cannot verify the signed session data.
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

  sc = psa_status_to_sl_status(
    psa_export_public_key(SL_BT_CBAP_PSA_DEVICE_KEY, public_key,
                          sizeof(public_key), &public_key_len));
  if (sc != SL_STATUS_OK) {
    SLI_BT_CBAP_LOG_ERROR(
      "Device key cannot be found: 0x%04lx" SLI_BT_CBAP_LOG_NL, sc);
    sc = SL_STATUS_INVALID_KEY;
    goto exit;
  }

  // The public key point is the last element of the SubjectPublicKeyInfo
  // structure of the certificate.
  pk_raw = &local_certs[LOCAL_CERT_SUBJECT].context.pk_raw;
  if (pk_raw->len < public_key_len
      || memcmp(public_key, &pk_raw->p[pk_raw->len - public_key_len],
                public_key_len) != 0) {
    SLI_BT_CBAP_LOG_ERROR("The device certificate was not issued for the "
                          "device key!" SLI_BT_CBAP_LOG_NL);
    sc = SL_STATUS_INVALID_KEY;
  }

  exit:
  mbedtls_x509_crt_free(&local_certs[LOCAL_CERT_SUBJECT].context);
  return sc;
}

/*******************************************************************************
 * Import the public key of the remote device from its device certificate.
 *
 * The key is needed to verify the session data that the remote device signs
 * with the matching private key. The key of a previous remote device is
 * destroyed first, so that the key slot is not leaked from procedure to
 * procedure.
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

  sc = sli_bt_cbap_destroy_peer_pub_key();
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
    SLI_BT_CBAP_LOG_ERROR(
      "The device certificate of the remote device holds no "
      "EC public key!" SLI_BT_CBAP_LOG_NL);
    sc = SL_STATUS_INVALID_KEY;
    goto exit;
  }

  sc = psa_status_to_sl_status(
    psa_import_key(&attributes,
                   &pk_raw->p[pk_raw->len - EC_PUB_KEY_LEN],
                   EC_PUB_KEY_LEN,
                   &remote_pub_key_id));
  if (sc != SL_STATUS_OK) {
    SLI_BT_CBAP_LOG_ERROR(
      "Failed to import the public key of the remote device: "
      "0x%04lx!" SLI_BT_CBAP_LOG_NL,
      sc);
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
    case PSA_SUCCESS:
      return SL_STATUS_OK;
    case PSA_ERROR_GENERIC_ERROR:
      return SL_STATUS_FAIL;
    case PSA_ERROR_NOT_SUPPORTED:
      return SL_STATUS_NOT_SUPPORTED;
    case PSA_ERROR_NOT_PERMITTED:
      return SL_STATUS_PERMISSION;
    case PSA_ERROR_BUFFER_TOO_SMALL:
      return SL_STATUS_WOULD_OVERFLOW;
    case PSA_ERROR_ALREADY_EXISTS:
      return SL_STATUS_ALREADY_EXISTS;
    case PSA_ERROR_DOES_NOT_EXIST:
      return SL_STATUS_NOT_FOUND;
    case PSA_ERROR_BAD_STATE:
      return SL_STATUS_INVALID_STATE;
    case PSA_ERROR_INVALID_ARGUMENT:
      return SL_STATUS_INVALID_PARAMETER;
    case PSA_ERROR_INSUFFICIENT_MEMORY:
      return SL_STATUS_NO_MORE_RESOURCE;
    case PSA_ERROR_INSUFFICIENT_STORAGE:
      return SL_STATUS_NO_MORE_RESOURCE;
    case PSA_ERROR_COMMUNICATION_FAILURE:
      return SL_STATUS_IO;
    case PSA_ERROR_STORAGE_FAILURE:
      return SL_STATUS_BT_HARDWARE;
    case PSA_ERROR_HARDWARE_FAILURE:
      return SL_STATUS_BT_HARDWARE;
    case PSA_ERROR_CORRUPTION_DETECTED:
      return SL_STATUS_BT_DATA_CORRUPTED;
    case PSA_ERROR_INSUFFICIENT_ENTROPY:
      return SL_STATUS_BT_CRYPTO;
    case PSA_ERROR_INVALID_SIGNATURE:
      return SL_STATUS_INVALID_SIGNATURE;
    case PSA_ERROR_INVALID_PADDING:
      return SL_STATUS_BT_CRYPTO;
    case PSA_ERROR_INSUFFICIENT_DATA:
      return SL_STATUS_BT_CRYPTO;
    case PSA_ERROR_INVALID_HANDLE:
      return SL_STATUS_INVALID_HANDLE;
    case PSA_ERROR_DATA_CORRUPT:
      return SL_STATUS_BT_DATA_CORRUPTED;
    case PSA_ERROR_DATA_INVALID:
      return SL_STATUS_BT_CRYPTO;
    default:
      return SL_STATUS_BT_UNSPECIFIED;
  }
}
