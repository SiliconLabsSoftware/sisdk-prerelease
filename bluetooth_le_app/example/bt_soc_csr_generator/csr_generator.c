/***************************************************************************//**
 * @file
 * @brief SoC Certificate Generator
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

#include <string.h>
#include <stdio.h>

#include "sl_status.h"
#include "sl_component_catalog.h"

#include "app_assert.h"
#include "app_log.h"

#include "ecode.h"
#include "sl_common.h"
#include "em_system.h"
#include "nvm3.h"

#include "psa/crypto.h"
#include "psa/crypto_values.h"

#include "cbap_key_id.h"
#include "csr_generator.h"
#include "csr_generator_config.h"

#include "der.h"

// -----------------------------------------------------------------------------
// Defines

#define UUID_LEN                  (16)
#define CRYPTO_EC_PRIVATE_KEY_LEN (32)

// -----------------------------------------------------------------------------
// Type definitions.

typedef struct  __attribute__((__packed__)) csr_output_t {
  uint8_t completed;
  uint8_t static_auth[CRYPTO_AUTH_256_LEN];
  uint16_t csr_len; /* DER is binary format so length is required */
  uint8_t csr[];
} csr_output_t;

typedef struct csr_config_t {
  size_t subject_name_field_count;                        // CSR subject name, excluding CN (see below)
  const subject_name_field_t *subject_name_field_array;
} csr_config_t;

// -----------------------------------------------------------------------------
// Module variables.

// UUID Namespace
static const uint8_t uid64_namespace[16] = {
  0x70, 0x0b, 0xaf, 0xdf, 0xd5, 0xec, 0xc3, 0x9b,
  0x37, 0x04, 0xa1, 0x2c, 0x07, 0x67, 0x86, 0x9c,
};

// Static authentication data buffer
static uint8_t auth_data[CRYPTO_AUTH_256_LEN] = { 0 };

static char btmesh_common_name_uuid[37] = { 0 };

// CSR Subject name fields
static subject_name_field_t subject_name_fields[] =
{
  {
    .name_len = 1,
    .value_len = (sizeof(CSR_GENERATOR_SUBJECT_COUNTRY) - 1),
    .name = "C",      // shorthand for "countryName"
    .value = CSR_GENERATOR_SUBJECT_COUNTRY
  },
  {
    .name_len = 2,
    .value_len = (sizeof(CSR_GENERATOR_SUBJECT_STATE) - 1),
    .name = "ST",     // shorthand for "stateOrProvinceName"
    .value = CSR_GENERATOR_SUBJECT_STATE
  },
  {
    .name_len = 1,
    .value_len = (sizeof(CSR_GENERATOR_SUBJECT_LOCALITY) - 1),
    .name = "L",      // shorthand for "locality"
    .value = CSR_GENERATOR_SUBJECT_LOCALITY
  },
  {
    .name_len = 1,
    .value_len = (sizeof(CSR_GENERATOR_SUBJECT_ORGANIZATION) - 1),
    .name = "O",      // shorthand for "organization"
    .value = CSR_GENERATOR_SUBJECT_ORGANIZATION
  },
  {
    .name_len = 2,
    .value_len = (sizeof(CSR_GENERATOR_SUBJECT_ORGANIZATION_UNIT) - 1),
    .name = "OU",     // shorthand for "organizationalUnitName"
    .value = CSR_GENERATOR_SUBJECT_ORGANIZATION_UNIT
  },
  {
    .name_len = 2,
    .value_len = 36, // 128-bit Mesh device UUID as hexes and dashes
    .name = "CN", // shorthand for "commonName"
    .value = btmesh_common_name_uuid // To be filled in
  },
};

// Position of RAM structure for communicating with PLT
static volatile csr_output_t * const csr_output = (csr_output_t *)CSR_GENERATOR_CSR_RAM_ADDRESS;

// CSR configuration
static const csr_config_t config = {
  .subject_name_field_count = 6,
  .subject_name_field_array = subject_name_fields,
};

// -----------------------------------------------------------------------------
// Private function declarations.

static sl_status_t crypto_init(void);

static sl_status_t create_static_auth_data(void);

static sl_status_t create_device_ec_key(mbedtls_svc_key_id_t *ecdh_key_ptr,
                                        mbedtls_svc_key_id_t *signing_key_ptr);

static sl_status_t generate_uuid(uint8_t *const uuid);

// -----------------------------------------------------------------------------
// Public function definitions.

/**************************************************************************//**
 * This function is generating the device EC key pair, the signing request
 * for the device certificate, and other related data.
 *****************************************************************************/
void csr_generate(void)
{
  sl_status_t sc;
  int mbedtls_ret = 0;
  size_t auth_len = 0;

  mbedtls_svc_key_id_t signing_key_id = 0;
  mbedtls_svc_key_id_t ecdh_key_id = 0;

  uint8_t uuid[UUID_LEN + 1] = { 0 };

  app_log_info("Starting BT CSR creation" APP_LOG_NL);

  // Clear the RAM output area
  for (size_t i = 0; i < sizeof(csr_output_t); i++) {
    ((volatile uint8_t *)csr_output)[i] = 0;
  }

  sc = crypto_init();
  app_assert_status(sc);

  // Check if the certificates already exist
  uint32_t type;
  size_t value_len;
  sc = nvm3_getObjectInfo(CBAP_NVM3_HANDLE, CBAP_NVM_DEVICE_CERT, &type, &value_len);
  bool device_certificate_present = (sc == SL_STATUS_OK);
  sc = nvm3_getObjectInfo(CBAP_NVM3_HANDLE, CBAP_NVM_ROOT_CERT, &type, &value_len);
  bool root_certificate_present = (sc == SL_STATUS_OK);
  (void)type;
  (void)value_len;

  if (device_certificate_present != root_certificate_present) {
    app_log_warning("Either the device certificate or the root certificate is already present. "
                    "Please clear the NVM (FLASH) and retry." APP_LOG_NL);
    return;
  } else if (device_certificate_present && root_certificate_present) {
    app_log_info("Device and root certificates already exist. Nothing to do." APP_LOG_NL);
    return;
  }

  // Create static authentication data
  sc = create_static_auth_data();
  app_assert((sc == SL_STATUS_OK) || (sc == SL_STATUS_ALREADY_EXISTS),
             "Failed to create static authentication data." APP_LOG_NL);
  if (sc == SL_STATUS_OK) {
    app_log_info("Static authentication data created successfully." APP_LOG_NL);
  } else {
    app_log_info("Static authentication data already exists." APP_LOG_NL);
    sc = SL_STATUS_OK;
  }

  if (sc == SL_STATUS_OK) {
    sc = export_static_auth_data(auth_data, sizeof(auth_data), &auth_len);
    app_assert((sc == SL_STATUS_OK), "Failed to read static authentication data." APP_LOG_NL);
    for (size_t i = 0; i < auth_len; i++) {
      csr_output->static_auth[i] = auth_data[i];
    }
    app_log_info("Data: ");
    app_log_hexdump_info(auth_data, CRYPTO_AUTH_256_LEN);
    app_log_append(APP_LOG_NL);
  }

  // Generate persistent EC key for ECDH, temporary key for CSR signing
  sc = create_device_ec_key(&ecdh_key_id, &signing_key_id);
  app_assert((sc == SL_STATUS_OK), "Failed to create EC key ITS." APP_LOG_NL);
  app_log_info("EC key created successfully, signing key ID is %04X" APP_LOG_NL,
               (int)signing_key_id);

  // Generate UUID
  sc = generate_uuid(uuid);
  app_assert((sc == SL_STATUS_OK),
             "Failed to generate UUID, e: %d" APP_LOG_NL, (int)sc);
  app_log_info("Generated UUID: ");
  app_log_hexdump_info(uuid, UUID_LEN);
  app_log_append(APP_LOG_NL);

  // Generate CSR
  unsigned char csr_der_buf[1024] = { 0 };
  size_t csr_der_len = 0;

  /* Fill in CN to subject name */
  char* p = subject_name_fields[5].value;

  for (int i = 0; i < UUID_LEN; i++) {
    if ((i > 3) && (i < 11) && ((i & 1) == 0)) { /* index 4, 6, 8, 10) */
      *p++ = '-';
    }
    sprintf(p, "%02x", uuid[i]);
    p += 2;
  }

  mbedtls_ret = der_encode_csr(config.subject_name_field_array,
                               config.subject_name_field_count,
                               signing_key_id,
                               csr_der_buf,
                               sizeof(csr_der_buf),
                               &csr_der_len);
  app_assert((mbedtls_ret == 0), "Failed to write CSR PEM file, error: %d" APP_LOG_NL, mbedtls_ret);

  app_log_info("CSR created successfully." APP_LOG_NL);

  // This assignment won't lose integer precision because sizeof(csr_der_buf) <= UINT16_MAX.
  csr_output->csr_len = (uint16_t)csr_der_len;
  for (size_t i = 0; i < csr_der_len; i++) {
    csr_output->csr[i] = csr_der_buf[i];
  }

  app_log_info("RAM output address %p" APP_LOG_NL, csr_output);
  csr_output->completed = 1;

  return;
}

// -----------------------------------------------------------------------------
// Private function definitions.

sl_status_t psa_status_to_sl_status(psa_status_t sc)
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

static sl_status_t crypto_init(void)
{
  psa_status_t sc = PSA_ERROR_NOT_SUPPORTED;
  SYSTEM_SecurityCapability_TypeDef capability;

  capability = SYSTEM_GetSecurityCapability();
  if (capability == securityCapabilityRoT
      || capability == securityCapabilitySE
      || capability == securityCapabilityVault) {
    sc = psa_crypto_init();
  }

  return psa_status_to_sl_status(sc);
}

static sl_status_t create_static_auth_data(void)
{
  sl_status_t sc;

  mbedtls_svc_key_id_t key = CBAP_PSA_AUTH_DATA;
  psa_algorithm_t alg = 0;
  psa_key_attributes_t attributes;
  attributes = psa_key_attributes_init();
  psa_key_lifetime_t lifetime =
    PSA_KEY_LIFETIME_FROM_PERSISTENCE_AND_LOCATION(PSA_KEY_PERSISTENCE_DEFAULT,
                                                   CSR_GENERATOR_KEY_LOCATION);

  // Destroy key if exists
  sc = psa_status_to_sl_status(psa_get_key_attributes(key, &attributes));
  if (sc == SL_STATUS_OK) {
    app_log_info("Static authentication data already exists. Destroying..." APP_LOG_NL);
    sc = psa_status_to_sl_status(psa_destroy_key(key));
    if (sc != SL_STATUS_OK) {
      return sc;
    }

    attributes = psa_key_attributes_init(); // Reset key attributes
  }

  psa_set_key_algorithm(&attributes, alg);
  psa_set_key_type(&attributes, PSA_KEY_TYPE_RAW_DATA);
  psa_set_key_bits(&attributes, CRYPTO_AUTH_256_LEN * 8);
  psa_set_key_usage_flags(&attributes, PSA_KEY_USAGE_EXPORT);
  psa_set_key_id(&attributes, key);
  psa_set_key_lifetime(&attributes, lifetime);

  sc = psa_status_to_sl_status(psa_generate_key(&attributes,
                                                &key));

  return sc;
}

sl_status_t export_static_auth_data(uint8_t *data, size_t data_size, size_t *data_length)
{
  psa_status_t sc;
  mbedtls_svc_key_id_t key = CBAP_PSA_AUTH_DATA;

  sc = psa_export_key(key, data, data_size, data_length);

  return psa_status_to_sl_status(sc);
}

static sl_status_t create_device_ec_key(mbedtls_svc_key_id_t *ecdh_key_ptr,
                                        mbedtls_svc_key_id_t *signing_key_ptr)
{
  if (!ecdh_key_ptr || !signing_key_ptr) {
    return SL_STATUS_NULL_POINTER;
  }

  sl_status_t sc;
  psa_key_attributes_t key_attr;
  key_attr = psa_key_attributes_init();
  mbedtls_svc_key_id_t key = CBAP_PSA_DEVICE_KEY;

  // Destroy key if exists
  sc = psa_status_to_sl_status(psa_get_key_attributes(key, &key_attr));

  if (sc == SL_STATUS_OK) {
    app_log_info("Device key already exists. Destroying..." APP_LOG_NL);
    sc = psa_status_to_sl_status(psa_destroy_key(key));

    if (sc != SL_STATUS_OK) {
      return sc;
    }
  }

  psa_key_lifetime_t persistent_wrapped =
    PSA_KEY_LIFETIME_FROM_PERSISTENCE_AND_LOCATION(PSA_KEY_PERSISTENCE_DEFAULT,
                                                   CSR_GENERATOR_KEY_LOCATION);
  psa_key_lifetime_t volatile_wrapped =
    PSA_KEY_LIFETIME_FROM_PERSISTENCE_AND_LOCATION(PSA_KEY_PERSISTENCE_VOLATILE,
                                                   CSR_GENERATOR_KEY_LOCATION);

  mbedtls_svc_key_id_t ecdh_key = 0;
  mbedtls_svc_key_id_t signing_key = 0;

  psa_key_attributes_t ecdh_attributes = psa_key_attributes_init();
  psa_key_attributes_t signing_attributes = psa_key_attributes_init();

  psa_set_key_algorithm(&signing_attributes, PSA_ALG_ECDSA(PSA_ALG_SHA_256));
  psa_set_key_type(&signing_attributes, PSA_KEY_TYPE_ECC_KEY_PAIR(PSA_ECC_FAMILY_SECP_R1));
  psa_set_key_bits(&signing_attributes, CRYPTO_EC_PRIVATE_KEY_LEN * 8);
  psa_set_key_usage_flags(&signing_attributes, PSA_KEY_USAGE_SIGN_HASH | PSA_KEY_USAGE_EXPORT);
  psa_set_key_lifetime(&signing_attributes, volatile_wrapped);

  sc = psa_status_to_sl_status(psa_generate_key(&signing_attributes, &signing_key));
  if (sc != SL_STATUS_OK) {
    return sc;
  }

  psa_set_key_algorithm(&ecdh_attributes, CSR_GENERATOR_EC_KEY_ALGO);
  psa_set_key_type(&ecdh_attributes, PSA_KEY_TYPE_ECC_KEY_PAIR(PSA_ECC_FAMILY_SECP_R1));
  psa_set_key_bits(&ecdh_attributes, CRYPTO_EC_PRIVATE_KEY_LEN * 8);
  psa_set_key_usage_flags(&ecdh_attributes, CSR_GENERATOR_EC_KEY_USAGE);
  psa_set_key_lifetime(&ecdh_attributes, persistent_wrapped);
  psa_set_key_id(&ecdh_attributes, key);

  uint8_t buf[32];
  size_t buf_len = sizeof(buf);
  sc = psa_status_to_sl_status(psa_export_key(signing_key, buf, buf_len, &buf_len));
  if (sc != SL_STATUS_OK) {
    return sc;
  }

  sc = psa_status_to_sl_status(psa_import_key(&ecdh_attributes, buf, sizeof(buf), &ecdh_key));
  memset(buf, 0, sizeof(buf));
  if (sc != SL_STATUS_OK) {
    return sc;
  }

  *ecdh_key_ptr = ecdh_key;
  ecdh_key = 0;
  *signing_key_ptr = signing_key;
  signing_key = 0;

  psa_destroy_key(ecdh_key);
  psa_destroy_key(signing_key);

  return SL_STATUS_OK;
}

sl_status_t export_public_key(mbedtls_svc_key_id_t key, uint8_t *data, size_t data_size, size_t *data_length)
{
  psa_status_t sc;

  if (!key || !data || !data_size || !data_length) {
    return SL_STATUS_INVALID_PARAMETER;
  }

  sc = psa_export_public_key(key, data, data_size, data_length);

  return psa_status_to_sl_status(sc);
}

sl_status_t calculate_sha_1(const uint8_t *ptr,
                            size_t len,
                            uint8_t *hash)
{
  psa_hash_operation_t operation = { 0 };
  psa_algorithm_t alg = PSA_ALG_SHA_1;
  size_t out_len = 0;
  uint8_t result[CRYPTO_SHA_1_LEN];

  sl_status_t sc = SL_STATUS_OK;

  if (!ptr || !len  || !hash) {
    return SL_STATUS_INVALID_PARAMETER;
  }

  sc = psa_status_to_sl_status(psa_hash_setup(&operation, alg));
  if (sc != SL_STATUS_OK) {
    return sc;
  }

  sc = psa_status_to_sl_status(psa_hash_update(&operation, ptr, len));
  if (sc != SL_STATUS_OK) {
    return sc;
  }

  sc = psa_status_to_sl_status(psa_hash_finish(&operation, result, CRYPTO_SHA_1_LEN, &out_len));
  if (sc != SL_STATUS_OK) {
    return sc;
  }

  memcpy(hash, result, CRYPTO_SHA_1_LEN);

  return sc;
}

static sl_status_t generate_uuid(uint8_t *const uuid)
{
  sl_status_t sc = SL_STATUS_OK;

  if (!uuid) {
    return SL_STATUS_INVALID_PARAMETER;
  }

  // UUID generation procedure shall follow the standard UUID format as defined in RFC4122
  // https://www.ietf.org/rfc/rfc4122.txt

  uint64_t uid = SYSTEM_GetUnique();
  uint8_t tmp[24], digest[CRYPTO_SHA_1_LEN];
  memcpy(tmp, uid64_namespace, 16);
  tmp[16] = (uid >> 56) & 0xff;
  tmp[17] = (uid >> 48) & 0xff;
  tmp[18] = (uid >> 40) & 0xff;
  tmp[19] = (uid >> 32) & 0xff;
  tmp[20] = (uid >> 24) & 0xff;
  tmp[21] = (uid >> 16) & 0xff;
  tmp[22] = (uid >>  8) & 0xff;
  tmp[23] = (uid >>  0) & 0xff;

  sc = calculate_sha_1(tmp, sizeof(tmp), digest);

  memcpy(uuid, digest, 16);
  uuid[6] &= 0x0f; // Set the four most significant bits (bits 12 through 15) of the time_hi_and_version field
  uuid[6] |= 0x50; // to the 4-bit version number (version 5: name-based, SHA-1)
  uuid[8] &= 0x3f; // Set the two most significant bits (bits 6 and 7) of the clock_seq_hi_and_reserved field
  uuid[8] |= 0x80; // to zero and one, respectively

  return sc;
}
