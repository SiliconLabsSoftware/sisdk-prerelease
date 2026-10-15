/***************************************************************************//**
 * @file sl_rail_sdk_wmbus_crypto_shim.h
 * @brief PSA Crypto includes for Wireless M-Bus mode 5 (unit-test stub optional).
 *******************************************************************************
 * SPDX-License-Identifier: Zlib
 ******************************************************************************/

#ifndef SL_RAIL_SDK_WMBUS_CRYPTO_SHIM_H
#define SL_RAIL_SDK_WMBUS_CRYPTO_SHIM_H

#if defined(SL_RAIL_SDK_WMBUS_CRYPTO_STUB)

#include <stddef.h>
#include <stdint.h>

typedef int32_t psa_status_t;
typedef uint32_t psa_key_id_t;

struct psa_key_attributes_s {
  uint8_t opaque[128];
};

struct psa_cipher_operation_s {
  uint8_t opaque[128];
};

typedef struct psa_key_attributes_s psa_key_attributes_t;
typedef struct psa_cipher_operation_s psa_cipher_operation_t;

#define PSA_SUCCESS                     ((psa_status_t)0)
#define PSA_KEY_ID_NULL                 ((psa_key_id_t)0)
#define PSA_KEY_USAGE_ENCRYPT           ((psa_key_usage_t)0x00000001)
#define PSA_KEY_USAGE_DECRYPT           ((psa_key_usage_t)0x00000002)
#define PSA_ALG_CBC_NO_PADDING          ((psa_algorithm_t)0x04401000)
#define PSA_KEY_TYPE_AES                ((psa_key_type_t)0x2400)
#define PSA_KEY_ATTRIBUTES_INIT         { 0 }
#define PSA_CIPHER_OPERATION_INIT       { 0 }

typedef uint32_t psa_key_usage_t;
typedef uint32_t psa_algorithm_t;
typedef uint16_t psa_key_type_t;

void sl_psa_crypto_init(void);

void psa_set_key_usage_flags(psa_key_attributes_t *attributes, psa_key_usage_t usage);
void psa_set_key_algorithm(psa_key_attributes_t *attributes, psa_algorithm_t alg);
void psa_set_key_type(psa_key_attributes_t *attributes, psa_key_type_t type);
void psa_set_key_bits(psa_key_attributes_t *attributes, size_t bits);

psa_status_t psa_import_key(const psa_key_attributes_t *attributes,
                            const uint8_t *data,
                            size_t data_length,
                            psa_key_id_t *key);

psa_status_t psa_cipher_encrypt_setup(psa_cipher_operation_t *operation,
                                      psa_key_id_t key,
                                      psa_algorithm_t alg);

psa_status_t psa_cipher_decrypt_setup(psa_cipher_operation_t *operation,
                                      psa_key_id_t key,
                                      psa_algorithm_t alg);

psa_status_t psa_cipher_set_iv(psa_cipher_operation_t *operation,
                               const uint8_t *iv,
                               size_t iv_length);

psa_status_t psa_cipher_update(psa_cipher_operation_t *operation,
                               const uint8_t *input,
                               size_t input_length,
                               uint8_t *output,
                               size_t output_size,
                               size_t *output_length);

psa_status_t psa_cipher_finish(psa_cipher_operation_t *operation,
                               uint8_t *output,
                               size_t output_size,
                               size_t *output_length);

psa_status_t psa_cipher_abort(psa_cipher_operation_t *operation);

psa_status_t psa_destroy_key(psa_key_id_t key);

#else

#include "psa/crypto.h"
#include "sl_psa_crypto.h"

#endif

#endif // SL_RAIL_SDK_WMBUS_CRYPTO_SHIM_H
