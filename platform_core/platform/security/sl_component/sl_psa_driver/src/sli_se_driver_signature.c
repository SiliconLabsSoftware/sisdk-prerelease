/***************************************************************************//**
 * @file
 * @brief Silicon Labs PSA Crypto Secure Engine Signature Driver functions.
 *******************************************************************************
 * # License
 * <b>Copyright 2020 Silicon Laboratories Inc. www.silabs.com</b>
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

#include "sli_psa_driver_features.h"

#if defined(SLI_MBEDTLS_DEVICE_HSE)

#include "psa/crypto.h"

#include "sli_psa_driver_common.h"  // sli_psa_zeroize()
#include "sli_se_transparent_types.h"
#include "sli_se_transparent_functions.h"
#include "sli_se_driver_key_management.h"
#include "sli_se_version_dependencies.h"

#include "sl_se_manager.h"
#include "sli_se_manager_internal.h"
#include "sl_se_manager_signature.h"

#if defined(SLI_PSA_DRIVER_FEATURE_RSA_SIGN) && defined(SLI_SE_SUPPORTS_RSA)
// Private mbedtls headers for DER-encoded RSA key parsing. The SE wants raw
// modulus/exponent buffers, so we use mbedtls to load the PSA representation
// and then re-export the components into the SE layout.
#include "psa_crypto_rsa.h"
#include "mbedtls/rsa.h"
#include "mbedtls/bignum.h"
#include "mbedtls/platform.h"   // mbedtls_free()
#endif

#include <string.h>
// -----------------------------------------------------------------------------
// Static functions

// -------------------------------------
// Helpers

#if defined(SLI_PSA_DRIVER_FEATURE_SIGNATURE)

/**
 * @brief
 *   Validate that the curve and algorithm combination is supported by hardware
 */
static psa_status_t check_curve_availability(
  const psa_key_attributes_t *attributes,
  psa_algorithm_t alg,
  bool is_sign,
  bool is_hash)
{
  psa_key_type_t key_type = psa_get_key_type(attributes);

  if (PSA_ALG_IS_RSA_PSS(alg) || PSA_ALG_IS_RSA_PKCS1V15_SIGN(alg)) {
    // We shouldn't have a RSA-type alg for a ECC key.
    return PSA_ERROR_INVALID_ARGUMENT;
  }

  #if !defined(SLI_PSA_DRIVER_FEATURE_ECDSA) \
    && !defined(SLI_PSA_DRIVER_FEATURE_EDDSA)
  (void) key_type;
  (void) is_sign;
  (void) is_hash;
  return PSA_ERROR_NOT_SUPPORTED;
  #else

  psa_ecc_family_t curvetype = PSA_KEY_TYPE_ECC_GET_FAMILY(key_type);

  #if defined(SLI_PSA_DRIVER_FEATURE_ECDSA)
  if (curvetype == PSA_ECC_FAMILY_SECP_R1) {
    if (is_sign) {
  #if defined(_SILICON_LABS_32B_SERIES_2_CONFIG_11)
      // Curl supports both randomized and deterministic ECDSA signing.
      // Deterministic ECDSA hashes in hardware (psa_sign_message only).
      if (!PSA_ALG_IS_ECDSA(alg)) {
        return PSA_ERROR_NOT_SUPPORTED;
      }
      if (is_hash && PSA_ALG_ECDSA_IS_DETERMINISTIC(alg)) {
        return PSA_ERROR_NOT_SUPPORTED;
      }
  #else
      // Non-Curl SE can only sign with randomized ECDSA
      if (!PSA_ALG_IS_RANDOMIZED_ECDSA(alg)) {
        return PSA_ERROR_NOT_SUPPORTED;
      }
      (void) is_hash;
  #endif // _SILICON_LABS_32B_SERIES_2_CONFIG_11
    } else {
      // SE can verify hashes and messages with any ECDSA
      // Randomized and deterministic ECDSA use the same verification
      if (!PSA_ALG_IS_ECDSA(alg)) {
        return PSA_ERROR_NOT_SUPPORTED;
      }
    }

    switch (psa_get_key_bits(attributes)) {
      #if defined(SLI_PSA_DRIVER_FEATURE_P192R1)
      case 192:     // Intentional
      #endif
      #if defined(SLI_PSA_DRIVER_FEATURE_P224R1)
      case 224:     // Intentional
      #endif
      #if defined(SLI_PSA_DRIVER_FEATURE_P256R1)
      case 256:     // Intentional
      #endif
      #if defined(SLI_PSA_DRIVER_FEATURE_P384R1)
      case 384:     // Intentional
      #endif
      #if defined(SLI_PSA_DRIVER_FEATURE_P521R1)
      case 521:
      #endif
      break;
      default:
        return PSA_ERROR_NOT_SUPPORTED;
    }
  } else if (curvetype == PSA_ECC_FAMILY_SECP_K1) {
    // TODO: introduce custom domains to enable secpxxxk1
    return PSA_ERROR_NOT_SUPPORTED;
  } else
  #endif // SLI_PSA_DRIVER_FEATURE_ECDSA

  #if defined(SLI_PSA_DRIVER_FEATURE_EDDSA)
  if (curvetype == PSA_ECC_FAMILY_TWISTED_EDWARDS) {
    (void) is_sign;
    // SE can sign and verify messages (not hashes) only
    if (is_hash) {
      return PSA_ERROR_NOT_SUPPORTED;
    }

    switch (psa_get_key_bits(attributes)) {
      #if defined(SLI_PSA_DRIVER_FEATURE_EDWARDS25519)
      case 255:
        // Only Ed25519 is supported (and only in context of EdDSA)
        if (alg != PSA_ALG_PURE_EDDSA) {
          return PSA_ERROR_NOT_SUPPORTED;
        }
        break;
      #endif   // SLI_PSA_DRIVER_FEATURE_EDWARDS25519
      default:
        return PSA_ERROR_NOT_SUPPORTED;
    }
  } else
  #endif // SLI_PSA_DRIVER_FEATURE_EDDSA

  {
    return PSA_ERROR_NOT_SUPPORTED;
  }

  return PSA_SUCCESS;
  #endif // SLI_PSA_DRIVER_FEATURE_ECDSA || SLI_PSA_DRIVER_FEATURE_EDDSA
}

static sl_se_hash_type_t get_hash_for_algorithm(psa_algorithm_t alg)
{
  #if defined(SLI_PSA_DRIVER_FEATURE_EDDSA)
  if (alg == PSA_ALG_PURE_EDDSA) {
    // The hash alg parameter is ignored for EdDSA, as it is decided uniqely by
    // the alorithm. Return magic value which isn't SL_SE_HASH_NONE.
    return (sl_se_hash_type_t)255;
  }
  #endif // SLI_PSA_DRIVER_FEATURE_EDDSA

  switch (PSA_ALG_SIGN_GET_HASH(alg)) {
    #if defined(SLI_PSA_DRIVER_FEATURE_SHA1)
    case PSA_ALG_SHA_1:
      return SL_SE_HASH_SHA1;
    #endif // SLI_PSA_DRIVER_FEATURE_SHA1

    #if defined(SLI_PSA_DRIVER_FEATURE_SHA224)
    case PSA_ALG_SHA_224:
      return SL_SE_HASH_SHA224;
    #endif // SLI_PSA_DRIVER_FEATURE_SHA224

    #if defined(SLI_PSA_DRIVER_FEATURE_SHA256)
    case PSA_ALG_SHA_256:
      return SL_SE_HASH_SHA256;
    #endif // SLI_PSA_DRIVER_FEATURE_SHA256

    #if defined(SLI_PSA_DRIVER_FEATURE_SHA384)
    case PSA_ALG_SHA_384:
      return SL_SE_HASH_SHA384;
    #endif // SLI_PSA_DRIVER_FEATURE_SHA384

    #if defined(SLI_PSA_DRIVER_FEATURE_SHA512)
    case PSA_ALG_SHA_512:
      return SL_SE_HASH_SHA512;
    #endif // SLI_PSA_DRIVER_FEATURE_SHA512

    default:
      return SL_SE_HASH_NONE;
  }
}

#endif // SLI_PSA_DRIVER_FEATURE_SIGNATURE

#if defined(SLI_PSA_DRIVER_FEATURE_RSA_SIGN) && defined(SLI_SE_SUPPORTS_RSA)

/** Largest RSA modulus size handled here (4096-bit), in bytes. */
#define SLI_SE_DRIVER_RSA_MAX_MODULUS_BYTES   (512U)
/** Max on-stack SE plaintext RSA key blob: N||D (sign) or N||E (verify, full-width E). */
#define SLI_SE_DRIVER_RSA_MAX_SE_KEY_BUF_BYTES  (2U * SLI_SE_DRIVER_RSA_MAX_MODULUS_BYTES)

/**
 * @brief Map a PSA RSA signature algorithm to an SE RSA padding scheme.
 *
 * PSA_ALG_RSA_PSS_ANY_SALT returns NOT_SUPPORTED so that the driver wrapper
 * falls back to mbedtls_psa_rsa_verify_hash, which can recover the salt
 * length from the signature (the SE API only accepts a fixed salt length).
 */
static psa_status_t rsa_padding_for_alg(psa_algorithm_t alg,
                                        sl_se_rsa_padding_t *padding)
{
  if (PSA_ALG_IS_RSA_PSS_ANY_SALT(alg)) {
    return PSA_ERROR_NOT_SUPPORTED;
  }
  if (PSA_ALG_IS_RSA_PKCS1V15_SIGN(alg)) {
    *padding = SL_SE_RSA_PADDING_PKCS1V15;
    return PSA_SUCCESS;
  }
  if (PSA_ALG_IS_RSA_PSS(alg)) {
    *padding = SL_SE_RSA_PADDING_PSS;
    return PSA_SUCCESS;
  }
  return PSA_ERROR_NOT_SUPPORTED;
}

/**
 * @brief Map an RSA modulus size to the matching SE key type.
 */
static psa_status_t rsa_se_key_type_for_bits(size_t key_bits,
                                             sl_se_key_type_t *type)
{
  switch (key_bits) {
    case 2048:
      *type = SL_SE_KEY_TYPE_RSA_2048;
      break;
    case 3072:
      *type = SL_SE_KEY_TYPE_RSA_3072;
      break;
    case 4096:
      *type = SL_SE_KEY_TYPE_RSA_4096;
      break;
    default:
      return PSA_ERROR_NOT_SUPPORTED;
  }
  return PSA_SUCCESS;
}

/**
 * @brief Build an SE key descriptor for an RSA sign or verify operation
 *        from a PSA-encoded key buffer.
 *
 * Writes the raw SE key layout into @p se_key_buf:
 *   - For @p for_private_key == true:  N || D, each @p key_size_out bytes.
 *   - For @p for_private_key == false: N || E. If the public exponent fits in
 *     4 bytes, E is written to a 4-byte big-endian slot and
 *     SL_SE_KEY_FLAG_ASYMMETRIC_SHORT_EXPONENT is set; otherwise E is written
 *     to a modulus-width slot (zero-padded) and that flag is clear, matching
 *     sli_key_get_storage_size() for RSA public keys.
 */
static psa_status_t build_rsa_se_key_desc(
  const psa_key_attributes_t *attributes,
  const uint8_t *key_buffer,
  size_t key_buffer_size,
  bool for_private_key,
  uint8_t *se_key_buf,
  size_t se_key_buf_size,
  sl_se_key_descriptor_t *key_desc,
  size_t *key_size_out)
{
  mbedtls_rsa_context *rsa = NULL;
  psa_status_t status = mbedtls_psa_rsa_load_representation(
    psa_get_key_type(attributes),
    key_buffer,
    key_buffer_size,
    &rsa);
  if (status != PSA_SUCCESS) {
    return status;
  }

  size_t key_size = mbedtls_rsa_get_len(rsa);
  sl_se_key_type_t se_type;
  status = rsa_se_key_type_for_bits(PSA_BYTES_TO_BITS(key_size), &se_type);
  if (status != PSA_SUCCESS) {
    goto cleanup;
  }

  mbedtls_mpi e_mpi;
  mbedtls_mpi_init(&e_mpi);
  int ret = mbedtls_rsa_export(rsa, NULL, NULL, NULL, NULL, &e_mpi);
  if (ret != 0) {
    mbedtls_mpi_free(&e_mpi);
    status = PSA_ERROR_INVALID_ARGUMENT;
    goto cleanup;
  }

  const size_t e_mpi_bytes = mbedtls_mpi_size(&e_mpi);
  if (e_mpi_bytes == 0 || e_mpi_bytes > key_size) {
    mbedtls_mpi_free(&e_mpi);
    status = PSA_ERROR_INVALID_ARGUMENT;
    goto cleanup;
  }

  const bool use_short_exponent = (e_mpi_bytes <= 4U);

  if (for_private_key) {
    if (se_key_buf_size < 2 * key_size) {
      mbedtls_mpi_free(&e_mpi);
      status = PSA_ERROR_BUFFER_TOO_SMALL;
      goto cleanup;
    }
    ret = mbedtls_rsa_export_raw(rsa,
                                 se_key_buf, key_size,
                                 NULL, 0, NULL, 0,
                                 se_key_buf + key_size, key_size,
                                 NULL, 0);
    mbedtls_mpi_free(&e_mpi);
    if (ret != 0) {
      // D is not present (e.g. the caller passed a public key type).
      status = PSA_ERROR_INVALID_ARGUMENT;
      goto cleanup;
    }
  } else {
    const size_t e_slot = use_short_exponent ? 4U : key_size;
    if (se_key_buf_size < key_size + e_slot) {
      mbedtls_mpi_free(&e_mpi);
      status = PSA_ERROR_BUFFER_TOO_SMALL;
      goto cleanup;
    }
    ret = mbedtls_rsa_export_raw(rsa,
                                 se_key_buf, key_size,
                                 NULL, 0, NULL, 0, NULL, 0,
                                 NULL, 0);
    if (ret != 0) {
      mbedtls_mpi_free(&e_mpi);
      status = PSA_ERROR_INVALID_ARGUMENT;
      goto cleanup;
    }
    ret = mbedtls_mpi_write_binary(&e_mpi,
                                   se_key_buf + key_size,
                                   e_slot);
    mbedtls_mpi_free(&e_mpi);
    if (ret != 0) {
      status = PSA_ERROR_INVALID_ARGUMENT;
      goto cleanup;
    }
  }

  memset(key_desc, 0, sizeof(*key_desc));
  key_desc->type = se_type;
  key_desc->flags = (for_private_key
                     ? SL_SE_KEY_FLAG_ASYMMETRIC_BUFFER_HAS_PRIVATE_KEY
                     : SL_SE_KEY_FLAG_ASYMMETRIC_BUFFER_HAS_PUBLIC_KEY);
  if (use_short_exponent) {
    key_desc->flags |= SL_SE_KEY_FLAG_ASYMMETRIC_SHORT_EXPONENT;
  }
  key_desc->storage.method = SL_SE_KEY_STORAGE_EXTERNAL_PLAINTEXT;
  key_desc->storage.location.buffer.pointer = se_key_buf;
  key_desc->storage.location.buffer.size = se_key_buf_size;
  *key_size_out = key_size;

cleanup:
  mbedtls_rsa_free(rsa);
  mbedtls_free(rsa);
  return status;
}

/**
 * @brief Map an sl_status_t from the SE RSA sign/verify APIs to a
 *        psa_status_t, honoring PSA semantics for fallback.
 */
static psa_status_t rsa_se_status_to_psa(sl_status_t status, bool verify)
{
  switch (status) {
    case SL_STATUS_OK:
      return PSA_SUCCESS;
    case SL_STATUS_INVALID_SIGNATURE:
      return verify ? PSA_ERROR_INVALID_SIGNATURE : PSA_ERROR_HARDWARE_FAILURE;
    case SL_STATUS_COMMAND_IS_INVALID:
      // Key type or parameter not supported by SE firmware.
      return PSA_ERROR_NOT_SUPPORTED;
    case SL_STATUS_FAIL:
      // Unlike ECDSA, RSA keys on this path are always external plaintext;
      // SL_STATUS_FAIL is an SE internal/crypto error, not a missing key.
      return PSA_ERROR_HARDWARE_FAILURE;
    default:
      return PSA_ERROR_HARDWARE_FAILURE;
  }
}

#endif // SLI_PSA_DRIVER_FEATURE_RSA_SIGN && SLI_SE_SUPPORTS_RSA

// -------------------------------------
// Generic (indirect) driver entry points

static psa_status_t sli_se_sign_message(
  const psa_key_attributes_t *attributes,
  const uint8_t *key_buffer,
  size_t key_buffer_size,
  psa_algorithm_t alg,
  const uint8_t *input,
  size_t input_length,
  uint8_t *signature,
  size_t signature_size,
  size_t *signature_length)
{
  #if defined(SLI_PSA_DRIVER_FEATURE_SIGNATURE)

  uint8_t* tmp_signature_p = signature;
  size_t tmp_signature_size = signature_size;
  psa_status_t psa_status = PSA_ERROR_CORRUPTION_DETECTED;

  // Argument check
  if (attributes == NULL
      || key_buffer == NULL
      || key_buffer_size == 0
      || (input == NULL && input_length > 0)
      || signature == NULL
      || signature_size == 0
      || signature_length == NULL) {
    return PSA_ERROR_INVALID_ARGUMENT;
  }

  #if defined(SLI_PSA_DRIVER_FEATURE_RSA_SIGN)
  if (PSA_KEY_TYPE_IS_RSA(psa_get_key_type(attributes))) {
    #if defined(SLI_SE_SUPPORTS_RSA)
    // RSA sign_message: the SE internally hashes the message, so this path is
    // only reached from psa_sign_message. RSA psa_sign_hash is dispatched to
    // sli_se_sign_hash, where the SE driver returns NOT_SUPPORTED and the
    // mbedtls software fallback handles it.
    if (psa_get_key_type(attributes) != PSA_KEY_TYPE_RSA_KEY_PAIR) {
      return PSA_ERROR_INVALID_ARGUMENT;
    }

    sl_se_rsa_padding_t padding;
    psa_status = rsa_padding_for_alg(alg, &padding);
    if (psa_status != PSA_SUCCESS) {
      return psa_status;
    }

    sl_se_hash_type_t hash = get_hash_for_algorithm(alg);
    if (hash == SL_SE_HASH_NONE) {
      return PSA_ERROR_NOT_SUPPORTED;
    }

    uint8_t se_key_buf[SLI_SE_DRIVER_RSA_MAX_SE_KEY_BUF_BYTES] = { 0 };
    sl_se_command_context_t rsa_cmd_ctx = { 0 };
    sl_se_key_descriptor_t rsa_key_desc = { 0 };
    size_t rsa_key_size = 0;

    psa_status = build_rsa_se_key_desc(attributes,
                                       key_buffer,
                                       key_buffer_size,
                                       true,
                                       se_key_buf,
                                       sizeof(se_key_buf),
                                       &rsa_key_desc,
                                       &rsa_key_size);
    if (psa_status != PSA_SUCCESS) {
      sli_psa_zeroize(se_key_buf, sizeof(se_key_buf));
      return psa_status;
    }

    if (signature_size < rsa_key_size) {
      sli_psa_zeroize(se_key_buf, sizeof(se_key_buf));
      return PSA_ERROR_BUFFER_TOO_SMALL;
    }

    size_t salt_length = (padding == SL_SE_RSA_PADDING_PSS)
                         ? PSA_HASH_LENGTH(PSA_ALG_SIGN_GET_HASH(alg))
                         : 0;

    sl_status_t se_status = sl_se_init_command_context(&rsa_cmd_ctx);
    if (se_status != SL_STATUS_OK) {
      sli_psa_zeroize(se_key_buf, sizeof(se_key_buf));
      return PSA_ERROR_HARDWARE_FAILURE;
    }

    se_status = sl_se_rsa_sign(&rsa_cmd_ctx,
                               &rsa_key_desc,
                               hash,
                               padding,
                               salt_length,
                               input,
                               input_length,
                               signature,
                               rsa_key_size);

    sli_psa_zeroize(se_key_buf, sizeof(se_key_buf));

    psa_status = rsa_se_status_to_psa(se_status, false);
    if (psa_status == PSA_SUCCESS) {
      *signature_length = rsa_key_size;
    }

    sl_status_t deinit_status = sl_se_deinit_command_context(&rsa_cmd_ctx);
    if (deinit_status != SL_STATUS_OK) {
      psa_status = PSA_ERROR_HARDWARE_FAILURE;
    }
    return psa_status;
    #else // SLI_SE_SUPPORTS_RSA
    // RSA sign is wanted in PSA config but this SE firmware has no RSA support.
    // Defer to mbedtls software fallback (driver wrapper only falls through on
    // NOT_SUPPORTED, not INVALID_ARGUMENT).
    return PSA_ERROR_NOT_SUPPORTED;
    #endif // SLI_SE_SUPPORTS_RSA
  }
  #endif // SLI_PSA_DRIVER_FEATURE_RSA_SIGN

  // Check the requested algorithm is supported
  if (PSA_KEY_TYPE_IS_ECC_KEY_PAIR(psa_get_key_type(attributes))) {
    psa_status = check_curve_availability(attributes, alg, true, false);
    if (psa_status != PSA_SUCCESS) {
      return psa_status;
    }
  } else {
    // Not able to sign using non-ECC keys, or using public keys
    return PSA_ERROR_INVALID_ARGUMENT;
  }

  // Check the requested hashing algorithm is supported
  if (get_hash_for_algorithm(alg) == SL_SE_HASH_NONE) {
    return PSA_ERROR_NOT_SUPPORTED;
  }

  // Ephemeral contexts
  sl_se_command_context_t cmd_ctx = { 0 };
  sl_se_key_descriptor_t key_desc = { 0 };

  // Initialize key descriptor and verify key buffer size
  psa_status = sli_se_key_desc_from_input(attributes,
                                          key_buffer,
                                          key_buffer_size,
                                          &key_desc);
  if (psa_status != PSA_SUCCESS) {
    return psa_status;
  }

  // Validate that the output buffer can contain the full signature.
  // Both ECDSA and EdDSA share the same signature size.
  if (signature_size
      < PSA_ECDSA_SIGNATURE_SIZE(psa_get_key_bits(attributes))) {
    return PSA_ERROR_BUFFER_TOO_SMALL;
  }

  size_t key_size = PSA_BITS_TO_BYTES(psa_get_key_bits(attributes));

  #if defined(SLI_SE_KEY_PADDING_REQUIRED)
  size_t offset = sli_se_get_padding(key_size);
  #else
  size_t offset = 0;
  #endif

  #if defined(SLI_SE_KEY_PADDING_REQUIRED) && defined(SLI_PSA_DRIVER_FEATURE_ECDSA)
  // P-521 (or any curve size that's not word-multiple) requires alignment on
  // word boundaries, instead of byte boundaries such as PSA Crypto defines as
  // input here.
  uint8_t temp_key_buf[SLI_SE_MAX_PADDED_KEY_PAIR_SIZE] = { 0 };
  uint8_t temp_signature_buffer[SLI_SE_MAX_PADDED_SIGNATURE_SIZE] = { 0 };
  psa_key_location_t location =
    PSA_KEY_LIFETIME_GET_LOCATION(psa_get_key_lifetime(attributes));

  if (offset > 0) {
    // We can only manipulate the input key for transparent keys.
    // For opaque keys, we will just have to rely on the key import operation
    // handling this
    if (location == PSA_KEY_LOCATION_LOCAL_STORAGE) {
      if ((offset + key_buffer_size) > sizeof(temp_key_buf)) {
        return PSA_ERROR_INVALID_ARGUMENT;
      }
      sli_se_pad_big_endian(temp_key_buf, key_buffer, key_buffer_size);
      // Since we know that this must be a plaintext key, we can freely
      // modify the key descriptor
      key_desc.storage.location.buffer.pointer = temp_key_buf;
      key_desc.storage.location.buffer.size = sizeof(temp_key_buf);
    }

    tmp_signature_p = temp_signature_buffer;
    tmp_signature_size = sizeof(temp_signature_buffer);
  }
  #endif   // SLI_SE_KEY_PADDING_REQUIRED || SLI_PSA_DRIVER_FEATURE_ECDSA

  if (tmp_signature_size < 2 * (offset + key_size)) {
    return PSA_ERROR_INVALID_ARGUMENT;
  }
  tmp_signature_size = 2 * (offset + key_size);

  sl_status_t status = sl_se_init_command_context(&cmd_ctx);
  if (status != SL_STATUS_OK) {
    return PSA_ERROR_HARDWARE_FAILURE;
  }

  #if defined(SLI_SE_VERSION_ED25519_ERRATA_CHECK_REQUIRED)
  psa_status = sli_se_check_eddsa_errata(attributes, &cmd_ctx);
  if (psa_status != PSA_SUCCESS) {
    return psa_status;
  }
  #endif   // SLI_SE_VERSION_ED25519_ERRATA_CHECK_REQUIRED

  // Run signature generation
  #if defined(_SILICON_LABS_32B_SERIES_2_CONFIG_11)
  if (PSA_ALG_ECDSA_IS_DETERMINISTIC(alg)) {
    // Use deterministic ECDSA (RFC 6979) on Curl (xG2B) devices
    status = sl_se_ecc_sign_deterministic(&cmd_ctx,
                                          &key_desc,
                                          get_hash_for_algorithm(alg),
                                          input,
                                          input_length,
                                          tmp_signature_p,
                                          tmp_signature_size);
  } else
  #endif // _SILICON_LABS_32B_SERIES_2_CONFIG_11
  {
    status = sl_se_ecc_sign(&cmd_ctx,
                            &key_desc,
                            get_hash_for_algorithm(alg),
                            false,
                            input,
                            input_length,
                            tmp_signature_p,
                            tmp_signature_size);
  }

  #if defined(SLI_SE_KEY_PADDING_REQUIRED) && defined(SLI_PSA_DRIVER_FEATURE_ECDSA)
  if (offset > 0) {
    sli_psa_zeroize(temp_key_buf, sizeof(temp_key_buf));
    // Copy over from temp signature
    sli_se_unpad_curve_point(temp_signature_buffer, signature, key_size);
  }
  #endif // SLI_SE_KEY_PADDING_REQUIRED && SLI_PSA_DRIVER_FEATURE_ECDSA

  if (status == SL_STATUS_OK) {
    *signature_length = PSA_ECDSA_SIGNATURE_SIZE(psa_get_key_bits(attributes));
    psa_status = PSA_SUCCESS;
  } else {
    if (status == SL_STATUS_FAIL) {
      // Will be returned for missing built-in keys.
      psa_status = PSA_ERROR_DOES_NOT_EXIST;
    } else if (status == SL_STATUS_COMMAND_IS_INVALID) {
      // Will be returned if a key type is not supported (for example).
      psa_status = PSA_ERROR_NOT_SUPPORTED;
    } else {
      psa_status = PSA_ERROR_HARDWARE_FAILURE;
    }
  }

  // Cleanup
  status = sl_se_deinit_command_context(&cmd_ctx);
  if (status != SL_STATUS_OK) {
    return PSA_ERROR_HARDWARE_FAILURE;
  }
  return psa_status;

  #else // SLI_PSA_DRIVER_FEATURE_SIGNATURE

  (void) attributes;
  (void) key_buffer;
  (void) key_buffer_size;
  (void) alg;
  (void) input;
  (void) input_length;
  (void) signature;
  (void) signature_size;
  (void) signature_length;

  return PSA_ERROR_NOT_SUPPORTED;

  #endif // SLI_PSA_DRIVER_FEATURE_SIGNATURE
}

static psa_status_t sli_se_sign_hash(
  const psa_key_attributes_t *attributes,
  const uint8_t *key_buffer,
  size_t key_buffer_size,
  psa_algorithm_t alg,
  const uint8_t *hash,
  size_t hash_length,
  uint8_t *signature,
  size_t signature_size,
  size_t *signature_length)
{
  #if defined(SLI_PSA_DRIVER_FEATURE_ECDSA)
  uint8_t* tmp_signature_p = signature;
  size_t tmp_signature_size = signature_size;

  // Argument check
  if (attributes == NULL
      || key_buffer == NULL
      || key_buffer_size == 0
      || hash == NULL
      || hash_length == 0
      || signature == NULL
      || signature_size == 0
      || signature_length == NULL) {
    return PSA_ERROR_INVALID_ARGUMENT;
  }

  if (PSA_KEY_TYPE_IS_RSA(psa_get_key_type(attributes))) {
    // RSA psa_sign_hash is handled by the mbedtls software fallback.
    return PSA_ERROR_NOT_SUPPORTED;
  }

  // Ephemeral contexts
  sl_se_command_context_t cmd_ctx = { 0 };
  sl_se_key_descriptor_t key_desc = { 0 };

  // Initialize key descriptor and verify key buffer size
  psa_status_t psa_status = sli_se_key_desc_from_input(attributes,
                                                       key_buffer,
                                                       key_buffer_size,
                                                       &key_desc);
  if (psa_status != PSA_SUCCESS) {
    return psa_status;
  }

  // Verify and set key attributes
  psa_key_type_t keytype = psa_get_key_type(attributes);

  if (PSA_KEY_TYPE_IS_ECC_KEY_PAIR(keytype)) {
    // Validate that the input
    psa_status = check_curve_availability(attributes, alg, true, true);
    if (psa_status != PSA_SUCCESS) {
      return psa_status;
    }
    if (signature_size
        < PSA_ECDSA_SIGNATURE_SIZE(psa_get_key_bits(attributes))) {
      return PSA_ERROR_BUFFER_TOO_SMALL;
    }
  } else {
    // Not able to sign using non-ECC keys, or using public keys
    return PSA_ERROR_INVALID_ARGUMENT;
  }

  size_t key_bits = psa_get_key_bits(attributes);
  size_t key_size = PSA_BITS_TO_BYTES(key_bits);

  #if defined(SLI_SE_KEY_PADDING_REQUIRED)
  size_t offset = sli_se_get_padding(key_size);
  #else
  size_t offset = 0;
  #endif

  #if defined(SLI_SE_KEY_PADDING_REQUIRED)
  // P-521 (or any curve size that's not word-multiple) requires alignment on
  // word boundaries, instead of byte boundaries such as PSA Crypto defines as
  // input here.
  uint8_t temp_key_buf[SLI_SE_MAX_PADDED_KEY_PAIR_SIZE] = { 0 };
  uint8_t temp_signature_buffer[SLI_SE_MAX_PADDED_SIGNATURE_SIZE] = { 0 };
  psa_key_location_t location =
    PSA_KEY_LIFETIME_GET_LOCATION(psa_get_key_lifetime(attributes));

  if (offset > 0) {
    // We can only manipulate the input key for transparent keys.
    // For opaque keys, we will just have to rely on the key import operation
    // handling this
    if (location == PSA_KEY_LOCATION_LOCAL_STORAGE) {
      if ((offset + key_buffer_size) > sizeof(temp_key_buf)) {
        return PSA_ERROR_INVALID_ARGUMENT;
      }
      sli_se_pad_big_endian(temp_key_buf, key_buffer, key_buffer_size);
      // Since we know that this must be a plaintext key, we can freely
      // modify the key descriptor
      key_desc.storage.location.buffer.pointer = temp_key_buf;
      key_desc.storage.location.buffer.size = sizeof(temp_key_buf);
    }

    tmp_signature_p = temp_signature_buffer;
    tmp_signature_size = sizeof(temp_signature_buffer);
  }
  #endif   // SLI_SE_KEY_PADDING_REQUIRED

  if (tmp_signature_size < 2 * (offset + key_size)) {
    return PSA_ERROR_INVALID_ARGUMENT;
  }
  tmp_signature_size = 2 * (offset + key_size);

  sl_status_t status = sl_se_init_command_context(&cmd_ctx);
  if (status != SL_STATUS_OK) {
    return PSA_ERROR_HARDWARE_FAILURE;
  }

  // Run signature generation
  status = sl_se_ecc_sign(&cmd_ctx,
                          &key_desc,
                          SL_SE_HASH_NONE,
                          true,
                          hash,
                          hash_length,
                          tmp_signature_p,
                          tmp_signature_size);

  #if defined(SLI_SE_KEY_PADDING_REQUIRED)
  if (offset > 0) {
    sli_psa_zeroize(temp_key_buf, sizeof(temp_key_buf));
    // Copy over from temp signature
    sli_se_unpad_curve_point(temp_signature_buffer, signature, key_size);
  }
  #endif   // SLI_SE_KEY_PADDING_REQUIRED

  if (status == SL_STATUS_OK) {
    *signature_length = PSA_ECDSA_SIGNATURE_SIZE(key_bits);
    psa_status = PSA_SUCCESS;
  } else {
    if (status == SL_STATUS_FAIL) {
      psa_status = PSA_ERROR_DOES_NOT_EXIST;
    } else {
      psa_status = PSA_ERROR_HARDWARE_FAILURE;
    }
  }

  // Cleanup
  status = sl_se_deinit_command_context(&cmd_ctx);
  if (status != SL_STATUS_OK) {
    return PSA_ERROR_HARDWARE_FAILURE;
  }
  return psa_status;

  #else // SLI_PSA_DRIVER_FEATURE_ECDSA

  (void) attributes;
  (void) key_buffer;
  (void) key_buffer_size;
  (void) alg;
  (void) hash;
  (void) hash_length;
  (void) signature;
  (void) signature_size;
  (void) signature_length;

  return PSA_ERROR_NOT_SUPPORTED;

  #endif  // SLI_PSA_DRIVER_FEATURE_ECDSA
}

static psa_status_t sli_se_verify_message(
  const psa_key_attributes_t *attributes,
  const uint8_t *key_buffer,
  size_t key_buffer_size,
  psa_algorithm_t alg,
  const uint8_t *input,
  size_t input_length,
  const uint8_t *signature,
  size_t signature_length)
{
  #if defined(SLI_PSA_DRIVER_FEATURE_SIGNATURE)
  psa_status_t psa_status = PSA_ERROR_CORRUPTION_DETECTED;

  // Argument check.
  if (attributes == NULL
      || key_buffer == NULL
      || key_buffer_size == 0
      || (input == NULL && input_length > 0)
      || (signature == NULL && signature_length != 0)) {
    return PSA_ERROR_INVALID_ARGUMENT;
  }

  #if defined(SLI_PSA_DRIVER_FEATURE_RSA_SIGN)
  // RSA verify_message: same rationale as in sli_se_sign_message. Accepts
  // both RSA_KEY_PAIR and RSA_PUBLIC_KEY.
  if (PSA_KEY_TYPE_IS_RSA(psa_get_key_type(attributes))) {
    #if defined(SLI_SE_SUPPORTS_RSA)
    sl_se_rsa_padding_t padding;
    psa_status = rsa_padding_for_alg(alg, &padding);
    if (psa_status != PSA_SUCCESS) {
      return psa_status;
    }

    sl_se_hash_type_t hash = get_hash_for_algorithm(alg);
    if (hash == SL_SE_HASH_NONE) {
      return PSA_ERROR_NOT_SUPPORTED;
    }

    if (signature_length == 0) {
      return PSA_ERROR_INVALID_SIGNATURE;
    }

    uint8_t se_key_buf[SLI_SE_DRIVER_RSA_MAX_SE_KEY_BUF_BYTES] = { 0 };
    sl_se_command_context_t rsa_cmd_ctx = { 0 };
    sl_se_key_descriptor_t rsa_key_desc = { 0 };
    size_t rsa_key_size = 0;

    psa_status = build_rsa_se_key_desc(attributes,
                                       key_buffer,
                                       key_buffer_size,
                                       false,
                                       se_key_buf,
                                       sizeof(se_key_buf),
                                       &rsa_key_desc,
                                       &rsa_key_size);
    if (psa_status != PSA_SUCCESS) {
      return psa_status;
    }

    if (signature_length != rsa_key_size) {
      return PSA_ERROR_INVALID_SIGNATURE;
    }

    size_t salt_length = (padding == SL_SE_RSA_PADDING_PSS)
                         ? PSA_HASH_LENGTH(PSA_ALG_SIGN_GET_HASH(alg))
                         : 0;

    sl_status_t se_status = sl_se_init_command_context(&rsa_cmd_ctx);
    if (se_status != SL_STATUS_OK) {
      return PSA_ERROR_HARDWARE_FAILURE;
    }

    se_status = sl_se_rsa_verify(&rsa_cmd_ctx,
                                 &rsa_key_desc,
                                 hash,
                                 padding,
                                 salt_length,
                                 input,
                                 input_length,
                                 signature,
                                 signature_length);

    psa_status = rsa_se_status_to_psa(se_status, true);

    sl_status_t deinit_status = sl_se_deinit_command_context(&rsa_cmd_ctx);
    if (deinit_status != SL_STATUS_OK) {
      psa_status = PSA_ERROR_HARDWARE_FAILURE;
    }
    return psa_status;
    #else // SLI_SE_SUPPORTS_RSA
    // Defer to mbedtls software fallback (driver wrapper only falls through on
    // NOT_SUPPORTED, not INVALID_ARGUMENT).
    return PSA_ERROR_NOT_SUPPORTED;
    #endif // SLI_SE_SUPPORTS_RSA
  }
  #endif // SLI_PSA_DRIVER_FEATURE_RSA_SIGN

  // Verify can happen with a public or private key
  if (PSA_KEY_TYPE_IS_ECC_KEY_PAIR(psa_get_key_type(attributes))
      || PSA_KEY_TYPE_IS_ECC_PUBLIC_KEY(psa_get_key_type(attributes))) {
    // Check the requested algorithm is supported and matches the key type
    psa_status = check_curve_availability(attributes, alg, false, false);
    if (psa_status != PSA_SUCCESS) {
      return psa_status;
    }
  } else {
    return PSA_ERROR_NOT_SUPPORTED;
  }

  // Check the requested hashing algorithm is supported
  if (get_hash_for_algorithm(alg) == SL_SE_HASH_NONE) {
    return PSA_ERROR_NOT_SUPPORTED;
  }

  if (signature_length == 0) {
    return PSA_ERROR_INVALID_SIGNATURE;
  }

  // Ephemeral contexts
  sl_se_command_context_t cmd_ctx = { 0 };
  sl_se_key_descriptor_t key_desc = { 0 };

  psa_status = sli_se_key_desc_from_input(attributes,
                                          key_buffer,
                                          key_buffer_size,
                                          &key_desc);
  if (psa_status != PSA_SUCCESS) {
    return psa_status;
  }

  // Validate that the signature input is of the expected length.
  // Both ECDSA and EdDSA share the same signature size.
  if (signature_length
      != PSA_ECDSA_SIGNATURE_SIZE(psa_get_key_bits(attributes))) {
    return PSA_ERROR_INVALID_SIGNATURE;
  }

  // Verify and set key attributes
  uint8_t temp_key_buf[SLI_SE_MAX_PADDED_PUBLIC_KEY_SIZE] = { 0 };
  psa_key_type_t keytype = psa_get_key_type(attributes);

  #if defined(SLI_SE_KEY_PADDING_REQUIRED) && defined(SLI_PSA_DRIVER_FEATURE_ECDSA)
  // P-521 (or any curve size that's not word-multiple) requires alignment on word
  // boundaries, instead of byte boundaries such as PSA Crypto defines as input here.
  uint8_t temp_signature_buffer[SLI_SE_MAX_PADDED_SIGNATURE_SIZE] = { 0 };
  size_t key_size = PSA_BITS_TO_BYTES(psa_get_key_bits(attributes));
  size_t offset = sli_se_get_padding(key_size);
  if (offset > 0) {
    psa_key_location_t location =
      PSA_KEY_LIFETIME_GET_LOCATION(psa_get_key_lifetime(attributes));

    // Only pad transparent keys.
    if (location == PSA_KEY_LOCATION_LOCAL_STORAGE) {
      if (PSA_KEY_TYPE_IS_ECC_KEY_PAIR(keytype)) {
        if (offset + key_size > sizeof(temp_key_buf)) {
          return PSA_ERROR_INVALID_ARGUMENT;
        }
        sli_se_pad_big_endian(temp_key_buf,
                              key_desc.storage.location.buffer.pointer,
                              key_size);
      } else if (PSA_KEY_TYPE_IS_ECC_PUBLIC_KEY(keytype)) {
        if ((2 * (offset + key_size)) > sizeof(temp_key_buf)) {
          return PSA_ERROR_INVALID_ARGUMENT;
        }
        sli_se_pad_curve_point(temp_key_buf,
                               key_desc.storage.location.buffer.pointer,
                               key_size);
      } else {
        return PSA_ERROR_CORRUPTION_DETECTED;
      }
      key_desc.storage.location.buffer.pointer = temp_key_buf;
      key_desc.storage.location.buffer.size = sizeof(temp_key_buf);
    }

    // Always pad signature
    if ((2 * (offset + key_size)) > sizeof(temp_signature_buffer)) {
      return PSA_ERROR_INVALID_ARGUMENT;
    }
    sli_se_pad_curve_point(temp_signature_buffer, signature, key_size);

    signature = temp_signature_buffer;
    signature_length = signature_length + 2 * offset;
  }
  #endif   // SLI_SE_KEY_PADDING_REQUIRED && SLI_PSA_DRIVER_FEATURE_ECDSA

  // SE manager only accepts public keys for signature verification,
  // so we must generate a public key if we are passed a private one
  sl_status_t status = SL_STATUS_INVALID_PARAMETER;
  if (PSA_KEY_TYPE_IS_ECC_KEY_PAIR(keytype)) {
    #if defined(SLI_SE_VERSION_ED25519_ERRATA_CHECK_REQUIRED)
    psa_status = sli_se_check_eddsa_errata(attributes, &cmd_ctx);
    if (psa_status != PSA_SUCCESS) {
      return psa_status;
    }
    #endif   // SLI_SE_VERSION_ED25519_ERRATA_CHECK_REQUIRED

    // Create similar key descriptor for temporary public key.
    sl_se_key_descriptor_t pubkey_desc = key_desc;
    pubkey_desc.flags &= ~SL_SE_KEY_FLAG_ASYMMETRIC_BUFFER_HAS_PRIVATE_KEY;
    pubkey_desc.flags &= ~SL_SE_KEY_FLAG_IS_RESTRICTED;
    pubkey_desc.flags |= SL_SE_KEY_FLAG_ASYMMETRIC_BUFFER_HAS_PUBLIC_KEY;
    sli_se_key_descriptor_set_plaintext(&pubkey_desc, temp_key_buf, sizeof(temp_key_buf));

    status = sl_se_init_command_context(&cmd_ctx);
    if (status != SL_STATUS_OK) {
      return PSA_ERROR_HARDWARE_FAILURE;
    }

    status = sl_se_export_public_key(&cmd_ctx, &key_desc, &pubkey_desc);
    if (status != SL_STATUS_OK) {
      if (status == SL_STATUS_COMMAND_IS_INVALID) {
        // This error will be returned if the key type isn't supported.
        return PSA_ERROR_NOT_SUPPORTED;
      } else {
        return PSA_ERROR_HARDWARE_FAILURE;
      }
    }

    // Set the key desc to the public key, and move on.
    key_desc = pubkey_desc;
  }

  status = sl_se_init_command_context(&cmd_ctx);
  if (status != SL_STATUS_OK) {
    return PSA_ERROR_HARDWARE_FAILURE;
  }

  // Run signature verification
  status = sl_se_ecc_verify(&cmd_ctx,
                            &key_desc,
                            get_hash_for_algorithm(alg),
                            false,
                            input,
                            input_length,
                            signature,
                            signature_length);

  if (status == SL_STATUS_OK) {
    psa_status = PSA_SUCCESS;
  } else if (status == SL_STATUS_INVALID_SIGNATURE) {
    // Signature was invalid.
    psa_status = PSA_ERROR_INVALID_SIGNATURE;
  } else if (status == SL_STATUS_FAIL) {
    // Built-in key does not exist.
    psa_status = PSA_ERROR_DOES_NOT_EXIST;
  } else if (status == SL_STATUS_COMMAND_IS_INVALID) {
    // Key type is not supported.
    psa_status = PSA_ERROR_NOT_SUPPORTED;
  } else {
    psa_status = PSA_ERROR_HARDWARE_FAILURE;
  }

  // Cleanup
  status = sl_se_deinit_command_context(&cmd_ctx);
  if (status != SL_STATUS_OK) {
    return PSA_ERROR_HARDWARE_FAILURE;
  }

  return psa_status;

  #else // SLI_PSA_DRIVER_FEATURE_SIGNATURE

  (void) attributes;
  (void) key_buffer;
  (void) key_buffer_size;
  (void) alg;
  (void) input;
  (void) input_length;
  (void) signature;
  (void) signature_length;

  return PSA_ERROR_NOT_SUPPORTED;

  #endif  // SLI_PSA_DRIVER_FEATURE_SIGNATURE
}

static psa_status_t sli_se_verify_hash(
  const psa_key_attributes_t *attributes,
  const uint8_t *key_buffer,
  size_t key_buffer_size,
  psa_algorithm_t alg,
  const uint8_t *hash,
  size_t hash_length,
  const uint8_t *signature,
  size_t signature_length)
{
  #if defined(SLI_PSA_DRIVER_FEATURE_ECDSA)

  // Argument check.
  if (attributes == NULL
      || key_buffer == NULL
      || key_buffer_size == 0
      || hash == NULL
      || hash_length == 0
      || (signature == NULL && signature_length != 0) ) {
    return PSA_ERROR_INVALID_ARGUMENT;
  }

  if (signature_length == 0) {
    return PSA_ERROR_INVALID_SIGNATURE;
  }

  if (PSA_KEY_TYPE_IS_RSA(psa_get_key_type(attributes))) {
    // RSA psa_verify_hash is handled by the mbedtls software fallback.
    return PSA_ERROR_NOT_SUPPORTED;
  }

  // Ephemeral contexts
  sl_se_command_context_t cmd_ctx = { 0 };
  sl_se_key_descriptor_t key_desc = { 0 };

  psa_status_t psa_status = sli_se_key_desc_from_input(attributes,
                                                       key_buffer,
                                                       key_buffer_size,
                                                       &key_desc);
  if (psa_status != PSA_SUCCESS) {
    return psa_status;
  }

  // Verify and set key attributes
  if (signature_length
      != PSA_ECDSA_SIGNATURE_SIZE(psa_get_key_bits(attributes))) {
    return PSA_ERROR_INVALID_SIGNATURE;
  }
  psa_status = check_curve_availability(attributes, alg, false, true);
  if (psa_status != PSA_SUCCESS) {
    return psa_status;
  }

  uint8_t temp_key_buf[SLI_SE_MAX_PADDED_PUBLIC_KEY_SIZE] = { 0 };
  psa_key_type_t keytype = psa_get_key_type(attributes);

  #if defined(SLI_SE_KEY_PADDING_REQUIRED) && defined(SLI_PSA_DRIVER_FEATURE_ECDSA)
  // P-521 (or any curve size that's not word-multiple) requires alignment on word
  // boundaries, instead of byte boundaries such as PSA Crypto defines as input here.
  uint8_t temp_signature_buffer[SLI_SE_MAX_PADDED_SIGNATURE_SIZE] = { 0 };
  size_t key_size = PSA_BITS_TO_BYTES(psa_get_key_bits(attributes));
  size_t offset = sli_se_get_padding(key_size);
  if (offset > 0) {
    psa_key_location_t location =
      PSA_KEY_LIFETIME_GET_LOCATION(psa_get_key_lifetime(attributes));

    // Only pad transparent keys.
    if (location == PSA_KEY_LOCATION_LOCAL_STORAGE) {
      if (PSA_KEY_TYPE_IS_ECC_KEY_PAIR(keytype)) {
        if (offset + key_size > sizeof(temp_key_buf)) {
          return PSA_ERROR_INVALID_ARGUMENT;
        }
        sli_se_pad_big_endian(temp_key_buf,
                              key_desc.storage.location.buffer.pointer,
                              key_size);
      } else if (PSA_KEY_TYPE_IS_ECC_PUBLIC_KEY(keytype)) {
        if ((2 * (offset + key_size)) > sizeof(temp_key_buf)) {
          return PSA_ERROR_INVALID_ARGUMENT;
        }
        sli_se_pad_curve_point(temp_key_buf,
                               key_desc.storage.location.buffer.pointer,
                               key_size);
      } else {
        return PSA_ERROR_CORRUPTION_DETECTED;
      }
      key_desc.storage.location.buffer.pointer = temp_key_buf;
      key_desc.storage.location.buffer.size = sizeof(temp_key_buf);
    }

    // Always pad signature
    if ((2 * (offset + key_size)) > sizeof(temp_signature_buffer)) {
      return PSA_ERROR_INVALID_ARGUMENT;
    }
    sli_se_pad_curve_point(temp_signature_buffer, signature, key_size);

    signature = temp_signature_buffer;
    signature_length = signature_length + 2 * offset;
  }
  #endif   // SLI_SE_KEY_PADDING_REQUIRED && SLI_PSA_DRIVER_FEATURE_ECDSA

  // SE manager only accepts public keys for signature verification,
  // so we must generate a public key if we are passed a private one
  sl_status_t status = SL_STATUS_INVALID_PARAMETER;
  if (PSA_KEY_TYPE_IS_ECC_KEY_PAIR(keytype)) {
    sl_se_key_descriptor_t pubkey_desc = key_desc;
    // Unset private key flag and set public
    pubkey_desc.flags &= ~SL_SE_KEY_FLAG_ASYMMETRIC_BUFFER_HAS_PRIVATE_KEY;
    pubkey_desc.flags &= ~SL_SE_KEY_FLAG_IS_RESTRICTED;
    pubkey_desc.flags |= SL_SE_KEY_FLAG_ASYMMETRIC_BUFFER_HAS_PUBLIC_KEY;
    sli_se_key_descriptor_set_plaintext(&pubkey_desc, temp_key_buf, sizeof(temp_key_buf));
    // Same input output region
    status = sl_se_init_command_context(&cmd_ctx);
    if (status != SL_STATUS_OK) {
      return PSA_ERROR_HARDWARE_FAILURE;
    }
    status = sl_se_export_public_key(&cmd_ctx, &key_desc, &pubkey_desc);
    if (sl_se_deinit_command_context(&cmd_ctx) != SL_STATUS_OK) {
      return PSA_ERROR_HARDWARE_FAILURE;
    }
    if (status) {
      return PSA_ERROR_HARDWARE_FAILURE;
    }
    // Set the key desc to the public key, and go on
    key_desc = pubkey_desc;
  }

  status = sl_se_init_command_context(&cmd_ctx);
  if (status != SL_STATUS_OK) {
    return PSA_ERROR_HARDWARE_FAILURE;
  }

  // Run signature verification
  status = sl_se_ecc_verify(&cmd_ctx,
                            &key_desc,
                            SL_SE_HASH_NONE,
                            true,
                            hash,
                            hash_length,
                            signature,
                            signature_length);

  if (status == SL_STATUS_OK) {
    psa_status = PSA_SUCCESS;
  } else if (status == SL_STATUS_INVALID_SIGNATURE) {
    psa_status = PSA_ERROR_INVALID_SIGNATURE;
  } else if (status == SL_STATUS_FAIL) {
    psa_status = PSA_ERROR_DOES_NOT_EXIST;
  } else {
    psa_status = PSA_ERROR_HARDWARE_FAILURE;
  }

  // Cleanup
  status = sl_se_deinit_command_context(&cmd_ctx);
  if (status != SL_STATUS_OK) {
    return PSA_ERROR_HARDWARE_FAILURE;
  }

  return psa_status;

  #else // SLI_PSA_DRIVER_FEATURE_ECDSA

  (void) attributes;
  (void) key_buffer;
  (void) key_buffer_size;
  (void) alg;
  (void) hash;
  (void) hash_length;
  (void) signature;
  (void) signature_length;

  return PSA_ERROR_NOT_SUPPORTED;

  #endif // SLI_PSA_DRIVER_FEATURE_ECDSA
}

// -----------------------------------------------------------------------------
// Opaque driver entry points

psa_status_t sli_se_opaque_sign_message(const psa_key_attributes_t *attributes,
                                        const uint8_t *key_buffer,
                                        size_t key_buffer_size,
                                        psa_algorithm_t alg,
                                        const uint8_t *input,
                                        size_t input_length,
                                        uint8_t *signature,
                                        size_t signature_size,
                                        size_t *signature_length)
{
  return sli_se_sign_message(attributes,
                             key_buffer,
                             key_buffer_size,
                             alg,
                             input,
                             input_length,
                             signature,
                             signature_size,
                             signature_length);
}

psa_status_t sli_se_opaque_sign_hash(const psa_key_attributes_t *attributes,
                                     const uint8_t *key_buffer,
                                     size_t key_buffer_size,
                                     psa_algorithm_t alg,
                                     const uint8_t *hash,
                                     size_t hash_length,
                                     uint8_t *signature,
                                     size_t signature_size,
                                     size_t *signature_length)
{
  return sli_se_sign_hash(attributes,
                          key_buffer,
                          key_buffer_size,
                          alg,
                          hash,
                          hash_length,
                          signature,
                          signature_size,
                          signature_length);
}

psa_status_t sli_se_opaque_verify_message(const psa_key_attributes_t *attributes,
                                          const uint8_t *key_buffer,
                                          size_t key_buffer_size,
                                          psa_algorithm_t alg,
                                          const uint8_t * input,
                                          size_t input_length,
                                          const uint8_t * signature,
                                          size_t signature_length)
{
  return sli_se_verify_message(attributes,
                               key_buffer,
                               key_buffer_size,
                               alg,
                               input,
                               input_length,
                               signature,
                               signature_length);
}

psa_status_t sli_se_opaque_verify_hash(const psa_key_attributes_t *attributes,
                                       const uint8_t *key_buffer,
                                       size_t key_buffer_size,
                                       psa_algorithm_t alg,
                                       const uint8_t *hash,
                                       size_t hash_length,
                                       const uint8_t *signature,
                                       size_t signature_length)
{
  return sli_se_verify_hash(attributes,
                            key_buffer,
                            key_buffer_size,
                            alg,
                            hash,
                            hash_length,
                            signature,
                            signature_length);
}

// -----------------------------------------------------------------------------
// Transparent driver entry points

psa_status_t sli_se_transparent_sign_message(
  const psa_key_attributes_t *attributes,
  const uint8_t *key_buffer,
  size_t key_buffer_size,
  psa_algorithm_t alg,
  const uint8_t *input,
  size_t input_length,
  uint8_t *signature,
  size_t signature_size,
  size_t *signature_length)
{
  return sli_se_sign_message(attributes,
                             key_buffer,
                             key_buffer_size,
                             alg,
                             input,
                             input_length,
                             signature,
                             signature_size,
                             signature_length);
}

psa_status_t sli_se_transparent_sign_hash(const psa_key_attributes_t *attributes,
                                          const uint8_t *key_buffer,
                                          size_t key_buffer_size,
                                          psa_algorithm_t alg,
                                          const uint8_t *hash,
                                          size_t hash_length,
                                          uint8_t *signature,
                                          size_t signature_size,
                                          size_t *signature_length)
{
  return sli_se_sign_hash(attributes,
                          key_buffer,
                          key_buffer_size,
                          alg,
                          hash,
                          hash_length,
                          signature,
                          signature_size,
                          signature_length);
}

psa_status_t sli_se_transparent_verify_message(
  const psa_key_attributes_t *attributes,
  const uint8_t *key_buffer,
  size_t key_buffer_size,
  psa_algorithm_t alg,
  const uint8_t *input,
  size_t input_length,
  const uint8_t *signature,
  size_t signature_length)
{
  return sli_se_verify_message(attributes,
                               key_buffer,
                               key_buffer_size,
                               alg,
                               input,
                               input_length,
                               signature,
                               signature_length);
}

psa_status_t sli_se_transparent_verify_hash(
  const psa_key_attributes_t *attributes,
  const uint8_t *key_buffer,
  size_t key_buffer_size,
  psa_algorithm_t alg,
  const uint8_t *hash,
  size_t hash_length,
  const uint8_t *signature,
  size_t signature_length)
{
  return sli_se_verify_hash(attributes,
                            key_buffer,
                            key_buffer_size,
                            alg,
                            hash,
                            hash_length,
                            signature,
                            signature_length);
}

#endif // SLI_MBEDTLS_DEVICE_HSE
