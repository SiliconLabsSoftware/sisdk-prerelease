/***************************************************************************//**
 * @file
 * @brief Provides hardware accelerated cryptographic primitives.
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
#ifndef SLI_CRYPTO_H
#define SLI_CRYPTO_H

#include "em_device.h"
#if defined(_SILICON_LABS_32B_SERIES_2)
  #include "sli_crypto_s2.h"
#elif defined(_SILICON_LABS_32B_SERIES_3)
  #include "sli_crypto_s3.h"
#else
  #error Unsupported device.
#endif
#include "sl_status.h"
#include "sl_code_classification.h"
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "sli_mbedtls_omnipresent.h"
#if ((defined(CRYPTOACC_PRESENT) && defined(SLI_MBEDTLS_DEVICE_VSE_V2)) || defined(_SILICON_LABS_32B_SERIES_3)) && (!defined(SL_TRUSTZONE_NONSECURE))
// Required for SL_CRYPTO_USE_HOST_ENTROPY and SL_USE_CM_RESEED
#include "psa_crypto_config.h"
#endif
#if defined(SL_COMPONENT_CATALOG_PRESENT)
#include "sl_component_catalog.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

// Helper macros to reduce boilerplate in driver files
// VSE_V1 lacks countermeasures.
#if ((defined(CRYPTOACC_PRESENT) && defined(SLI_MBEDTLS_DEVICE_VSE_V2))) || defined(_SILICON_LABS_32B_SERIES_3)
#define SLI_AES_COUNTERMEASURES_PRESENT 1
#else
#define SLI_AES_COUNTERMEASURES_PRESENT 0
#endif
// SLI_CM_COUNTERS_ENABLED: true if op counters should be compiled (mode 1 or 2)
// SLI_CM_AUTO_RESEED_ENABLED: true if automatic reseed should be compiled (mode 2 only)
#if defined(SL_USE_CM_RESEED) \
    && SLI_AES_COUNTERMEASURES_PRESENT \
    && (!defined(SL_TRUSTZONE_NONSECURE))
  #define SLI_CM_COUNTERS_ENABLED      (SL_USE_CM_RESEED != 0)
  #define SLI_CM_AUTO_RESEED_ENABLED   (SL_USE_CM_RESEED >= 2)
#else
  #define SLI_CM_COUNTERS_ENABLED      0
  #define SLI_CM_AUTO_RESEED_ENABLED   0
  #undef SL_USE_CM_RESEED
#endif

#if defined(SL_CRYPTO_USE_HOST_ENTROPY) && (!defined(SL_TRUSTZONE_NONSECURE))
  #define SLI_CRYPTO_USE_HOST_ENTROPY (SL_CRYPTO_USE_HOST_ENTROPY != 0)
#else
  #define SLI_CRYPTO_USE_HOST_ENTROPY 0
  #undef SL_CRYPTO_USE_HOST_ENTROPY
#endif

#if (SLI_CRYPTO_USE_HOST_ENTROPY || SLI_CM_COUNTERS_ENABLED)
/// Defines the physical dimensions of the entropy pool
typedef enum _sli_entropy_pool_counts {
  /// The number of bytes in a single seed word
  CRYPTO_CM_SEED_WORD_BYTES = sizeof(uint32_t),
  /// The number of bytes in the complete countermeasure mask
  CRYPTO_CM_SEED_BYTES_MAX = CRYPTO_CM_MASKBITS / (8 * sizeof(uint8_t)),
  /// The number of seed words in the complete countermeasure mask
  CRYPTO_CM_SEED_WORDS_MAX = CRYPTO_CM_MASKBITS / (8 * CRYPTO_CM_SEED_WORD_BYTES),
  #if (SLI_CRYPTO_USE_HOST_ENTROPY)
  /// The number of complete countermeasure masks to store in the host entropy pool
  CRYPTO_CM_SEED_POOL_SIZE = 8,
  /// The size of the host entropy pool
  CRYPTO_CM_HOST_ENTROPY_POOL_BYTES = (CRYPTO_CM_SEED_POOL_SIZE * CRYPTO_CM_SEED_BYTES_MAX)
  #else
  CRYPTO_CM_SEED_POOL_SIZE = 0,
  CRYPTO_CM_HOST_ENTROPY_POOL_BYTES = 0,
  #endif
} sli_crypto_cm_count_t;

// Countermeasure reseed threshold (AES block operations before reseed recommended)
// DPA countermeasures may become predictable after 2^30 operations
#ifndef SLI_CRYPTO_CM_RESEED_THRESH
// default to 90% of 2^30
#define SLI_CRYPTO_CM_RESEED_THRESH ((1UL << 30) - (1UL << 30)/10)
#endif
// Maximum safe threshold is  2^30 - 1 ops
#define SLI_CRYPTO_CM_RESEED_THRESH_MAX    ((1UL << 30) - 1) 
#if (SLI_CRYPTO_CM_RESEED_THRESH > SLI_CRYPTO_CM_RESEED_THRESH_MAX)
  #error "SLI_CRYPTO_CM_RESEED_THRESH exceeds maximum safe value"
#endif

/// AES mode for operation count calculation
typedef enum {
  SLI_CM_AES_MODE_BLOCK = 0,  ///< ECB, CBC, CTR, CFB, OFB - 1 block per 16 bytes
  SLI_CM_AES_MODE_GCM,        ///< GCM - blocks + 1 for tag
  SLI_CM_AES_MODE_CCM,        ///< CCM - blocks + 1 for tag (AAD handled separately)
  SLI_CM_AES_MODE_CMAC,       ///< CMAC - blocks + 1 for subkey generation
} sli_cm_aes_mode_t;
#endif // (SLI_CRYPTO_USE_HOST_ENTROPY || SLI_CM_COUNTERS_ENABLED)

#if (SLI_CRYPTO_USE_HOST_ENTROPY)

/// If nonzero, this will cause the host entropy pool to accumulate trng while waiting
/// for "long" transactions to finish. Default to transactions of 1kb or larger
#define HOST_ENTROPY_ACCUMULATE_ON_LONG_TRANSACTIONS (1024)

/// refill when only two CM words are left
#define SLI_CRYPTO_HOST_ENTROPY_POOL_DEFAULT_FILL_THRESH \
  (CRYPTO_CM_SEED_WORD_BYTES * 2)

#if (CRYPTO_CM_HOST_ENTROPY_POOL_BYTES < SLI_CRYPTO_HOST_ENTROPY_POOL_DEFAULT_FILL_THRESH)
#error "CRYPTO_CM_HOST_ENTROPY_POOL_BYTES is too small for default fill threshold"
#endif

/***************************************************************************//**
 * @brief                Accumulate CRYPTO_CM_HOST_ENTROPY_POOL_BYTES of entropy
 *                       on host for later use
 *
 * @return               SL_STATUS_OK if successful, relevant status code on error
 ******************************************************************************/
sl_status_t sli_crypto_entropy_pool_accumulate(void);

/***************************************************************************//**
 * @brief                Consume bytes from the entropy pool
 *
 * @param out            Pointer to the destination buffer
 * @param nbytes         As-input: number of bytes requested
 *                       As-output: number of bytes actually stored in 'out'
 *
 * @return               SL_STATUS_OK if successful, relevant status code on error
 ******************************************************************************/
sl_status_t sli_crypto_entropy_pool_consume(uint8_t *out, size_t *nbytes);

/***************************************************************************//**
 * @brief                Returns the current number of bytes remaining in the
 *                       host entropy pool.
 * @return               Number of bytes remaining in the host entropy pool.
 ******************************************************************************/
uint16_t sli_crypto_entropy_pool_bytes_remaining(void);
#if defined(UNITY_TEST)
/***************************************************************************//**
 * @brief                Test-only function to zero the host entropy pool
 ******************************************************************************/
void host_entropy_pool_flush(void);
#endif // defined(UNITY_TEST)
#endif // defined(SLI_CRYPTO_USE_HOST_ENTROPY)
/***************************************************************************//**
 * @brief                seeds the AES countermeasures for the first time
 *                       fills host entropy buffer and resets AES opcount, when they are present
 * @return               SL_STATUS_OK if successful
 *                       or relevant status code on other error
 ******************************************************************************/
sl_status_t sli_crypto_init(void);

#if SLI_AES_COUNTERMEASURES_PRESENT && (!defined(SL_TRUSTZONE_NONSECURE))
/// Structure for a single countermeasure seed
typedef union _hostcrypto_seed {
  uint8_t  u8[CRYPTO_CM_SEED_BYTES_MAX];
  uint32_t u32[CRYPTO_CM_SEED_WORDS_MAX];
} sli_crypto_seed_t;
/***************************************************************************//**
 * @brief                Reseeds the AES countermeasures and resets the AES op counter
 * @param engine         The crypto engine to reseed
 * @param opt_seed       Optional pointer to a seed to use for reseeding. Note that
 *                       if sli_crypto is operating from secure space, the opt_seed 
 *                       must be from secure space or be NULL (use random from secure side)
 *                       to avoid time-of-use vulnerabilities.
 *                       if NULL, a new random seed will be used from internal TRNG
 * @return               SL_STATUS_OK if successful, relevant status code on error
 ******************************************************************************/
sl_status_t sli_crypto_countermeasure_reseed(sli_crypto_engine_t engine,
                                             sli_crypto_seed_t *opt_seed);

#if (SLI_CM_COUNTERS_ENABLED)
/***************************************************************************//**
 * @brief                Gets the current AES operation count for a specific engine
 * @param engine         The crypto engine to query
 * @return               The current AES operation count, or (uint32_t)(-1) on error
 ******************************************************************************/
uint32_t sli_crypto_get_engine_aes_op_count(sli_crypto_engine_t engine);

/***************************************************************************//**
 * @brief                Increments the AES operation count for a specific engine
 * @param engine         The crypto engine to update
 * @param n_ops          The number of operations to add
 * @return               The new AES engine operation count,
 *                       or (uint32_t)(-1) on error
 ******************************************************************************/
uint32_t sli_crypto_inc_engine_aes_op_count(sli_crypto_engine_t engine, uint32_t n_ops);

/***************************************************************************//**
 * @brief                Calculate number of AES block operations for a given mode
 *                       and data length.
 *
 * @param mode           AES mode (BLOCK, GCM, CCM, CMAC)
 * @param nbytes         Number of data bytes to process
 *
 * @return               Number of AES block operations required
 *
 * @note                 For CCM with AAD, caller must add ((aad_len + 15) / 16)
 *                       separately since AAD length is not included in this
 *                       calculation.
 ******************************************************************************/
uint32_t sli_crypto_cm_get_opcount(sli_cm_aes_mode_t mode, size_t nbytes);

/***************************************************************************//**
 * @brief                Check if an operation would cross a countermeasure
 *                       threshold and optionally attempt reseed.
 *
 * @param engine         The crypto engine to check
 * @param n_ops          Number of operations about to be performed
 * @param reseed         If true, attempt reseed when threshold is crossed;
 *                       if false, only return the status without reseeding
 *
 * @return               SL_STATUS_OK if threshold not crossed (or reseed succeeded)
 *                       SL_STATUS_SECURITY_AES_CM_RESEED_NEEDED if reseed threshold
 *                         crossed and reseed failed or was not attempted
 *                       SL_STATUS_SECURITY_AES_CM_FAIL if security threshold
 *                         would be exceeded
 *
 * @note                 In automatic mode (SL_USE_CM_RESEED >= 2), CM_FAIL blocks
 *                       the operation. In manual mode, it's a warning only.
 ******************************************************************************/
sl_status_t sli_crypto_cm_check_threshold(sli_crypto_engine_t engine,
                                          uint32_t n_ops,
                                          bool reseed);
#endif // (SLI_CM_COUNTERS_ENABLED)
#endif // SLI_AES_COUNTERMEASURES_PRESENT && (!defined(SL_TRUSTZONE_NONSECURE))

/***************************************************************************//**
 * @brief                CCM buffer authenticated decryption optimized for BLE
 *
 * @param key_descriptor AES key descriptor
 * @param data           Input/output buffer of payload data of BLE packet
 * @param length         length of input data
 * @param iv             nonce (initialization vector)
 *                       must be 13 bytes
 * @param header         header of BLE packet (1 byte)
 * @param tag            authentication tag of BLE packet (4 bytes)
 *
 * @return               SL_STATUS_OK if successful and authenticated,
 *                       SL_STATUS_INVALID_SIGNATURE if tag does not match payload,
 *                       SL_STATUS_SECURITY_AES_CM_RESEED_NEEDED if the AES
 *                         countermeasure reseed threshold was crossed - call
 *                         sli_crypto_countermeasure_reseed() to maintain DPA resistance,
 *                       SL_STATUS_SECURITY_AES_CM_FAIL if the AES countermeasure
 *                         security threshold was exceeded - operation blocked,
 *                         reseed is mandatory before further AES operations,
 *                       relevant status code on other error
 ******************************************************************************/
SL_CODE_CLASSIFY(SL_CODE_COMPONENT_SLI_CRYPTO, SL_CODE_CLASS_TIME_CRITICAL)
sl_status_t sli_crypto_ccm_auth_decrypt_ble(sli_crypto_descriptor_t     *key_descriptor,
                                            unsigned char               *data,
                                            size_t                      length,
                                            const unsigned char         *iv,
                                            unsigned char               header,
                                            unsigned char               *tag);

/***************************************************************************//**
 * @brief                CCM buffer encryption optimized for BLE
 *
 * @param key_descriptor AES key descriptor
 * @param data           Input/output buffer of payload data of BLE packet
 * @param length         length of input data
 * @param iv             nonce (initialization vector)
 *                       must be 13 bytes
 * @param header         header of BLE packet (1 byte)
 * @param tag            buffer where the BLE packet tag (4 bytes) will be written
 *
 * @return               SL_STATUS_OK if successful,
 *                       SL_STATUS_SECURITY_AES_CM_RESEED_NEEDED if the AES
 *                         countermeasure reseed threshold was crossed - call
 *                         sli_crypto_countermeasure_reseed() to maintain DPA resistance,
 *                       SL_STATUS_SECURITY_AES_CM_FAIL if the AES countermeasure
 *                         security threshold was exceeded - operation blocked,
 *                         reseed is mandatory before further AES operations,
 *                       relevant status code on other error
 ******************************************************************************/
SL_CODE_CLASSIFY(SL_CODE_COMPONENT_SLI_CRYPTO, SL_CODE_CLASS_TIME_CRITICAL)
sl_status_t sli_crypto_ccm_encrypt_and_tag_ble(sli_crypto_descriptor_t     *key_descriptor,
                                               unsigned char               *data,
                                               size_t                      length,
                                               const unsigned char         *iv,
                                               unsigned char               header,
                                               unsigned char               *tag);

/***************************************************************************//**
 * @brief                CCM buffer authenticated decryption optimized for Zigbee
 *
 * @param key_descriptor AES key descriptor
 * @param encrypt        Encrypt operation
 * @param data_in        Input buffer of payload data (decrypt-in-place)
 * @param data_out       output buffer of payload data (decrypt-in-place)
 * @param length         length of input data
 * @param iv             nonce (initialization vector)
 *                       must be 13 bytes
 * @param aad            Input buffer of Additional Authenticated Data
 * @param aad_len        Length of buffer aad
 * @param tag            authentication tag
 * @param tag_len        Length of authentication tag
 *
 * @return               SL_STATUS_OK if successful and authenticated,
 *                       SL_STATUS_INVALID_SIGNATURE if tag does not match payload,
 *                       SL_STATUS_SECURITY_AES_CM_RESEED_NEEDED if the AES
 *                         countermeasure reseed threshold was crossed - call
 *                         sli_crypto_countermeasure_reseed() to maintain DPA resistance,
 *                       SL_STATUS_SECURITY_AES_CM_FAIL if the AES countermeasure
 *                         security threshold was exceeded - operation blocked,
 *                         reseed is mandatory before further AES operations,
 *                       relevant status code on other error
 ******************************************************************************/
SL_CODE_CLASSIFY(SL_CODE_COMPONENT_SLI_CRYPTO, SL_CODE_CLASS_TIME_CRITICAL)
sl_status_t sli_crypto_ccm_zigbee(sli_crypto_descriptor_t     *key_descriptor,
                                  bool                        encrypt,
                                  const unsigned char         *data_in,
                                  unsigned char               *data_out,
                                  size_t                      length,
                                  const unsigned char         *iv,
                                  const unsigned char         *aad,
                                  size_t                      aad_len,
                                  unsigned char               *tag,
                                  size_t                      tag_len);

/***************************************************************************//**
 * @brief                Process a table of BLE RPA device keys and look for a
 *                       match against the supplied hash
 *
 * @param key_descriptor SLI crypto descriptor. If plaintext keys are used the
 *                       descriptor provides a pointer to an array of AES-128 keys.
 *                       If KSU stored keys are used, the descriptor provides the
 *                       starting key slot ID of the KSU RAM where the IRK list is
 *                       located
 * @param irk_len        Number of IRK to be resolved for the RPA operation
 * @param keymask        Bitmask indicating with key indices in key table are valid
 * @param prand          24-bit BLE nonce to encrypt with each key and match against
 *                       hash
 * @param hash           BLE RPA hash to match against (last 24 bits of AES result)
 * @param irk_index      0-based index of matching key if a match is found,
 *                       -1 for no match or error
 *
 * @return               SL_STATUS_OK if successful,
 *                       SL_STATUS_SECURITY_AES_CM_RESEED_NEEDED if the AES
 *                         countermeasure reseed threshold was crossed - call
 *                         sli_crypto_countermeasure_reseed() to maintain DPA resistance,
 *                       SL_STATUS_SECURITY_AES_CM_FAIL if the AES countermeasure
 *                         security threshold was exceeded - operation blocked,
 *                         reseed is mandatory before further AES operations,
 *                       relevant status code on other error
 ******************************************************************************/
SL_CODE_CLASSIFY(SL_CODE_COMPONENT_SLI_CRYPTO, SL_CODE_CLASS_TIME_CRITICAL)
sl_status_t sli_crypto_process_rpa(sli_crypto_descriptor_t     *key_descriptor,
                                   size_t                      irk_len,
                                   uint64_t                    keymask,
                                   uint32_t                    prand,
                                   uint32_t                    hash,
                                   int                         *irk_index);

/***************************************************************************//**
 * @brief                AES-CTR block encryption/decryption optimized for radio
 *
 * @param key_descriptor AES key descriptor
 * @param keybits        must be 128 or 256
 * @param input          16-byte input block
 * @param iv_in          16-byte counter/IV starting value
 * @param iv_out         16-byte counter/IV output after block round
 * @param output         16-byte output block
 *
 * @return               SL_STATUS_OK if successful,
 *                       SL_STATUS_SECURITY_AES_CM_RESEED_NEEDED if the AES
 *                         countermeasure reseed threshold was crossed - call
 *                         sli_crypto_countermeasure_reseed() to maintain DPA resistance,
 *                       SL_STATUS_SECURITY_AES_CM_FAIL if the AES countermeasure
 *                         security threshold was exceeded - operation blocked,
 *                         reseed is mandatory before further AES operations,
 *                       relevant status code on other error
 ******************************************************************************/
SL_CODE_CLASSIFY(SL_CODE_COMPONENT_SLI_CRYPTO, SL_CODE_CLASS_TIME_CRITICAL)
sl_status_t sli_crypto_aes_ctr_radio(sli_crypto_descriptor_t   *key_descriptor,
                                     unsigned int              keybits,
                                     const unsigned char       input[SLI_CRYPTO_AES_BLOCK_SIZE],
                                     const unsigned char       iv_in[SLI_CRYPTO_AES_BLOCK_SIZE],
                                     volatile unsigned char    iv_out[SLI_CRYPTO_AES_BLOCK_SIZE],
                                     volatile unsigned char    output[SLI_CRYPTO_AES_BLOCK_SIZE]);

/***************************************************************************//**
 * @brief                AES-ECB block encryption/decryption optimized for radio
 *
 * @param encrypt        true for encryption, false for decryption
 * @param key_descriptor AES key descriptor
 * @param keybits        must be 128 or 256
 * @param input          16-byte input block
 * @param output         16-byte output block
 *
 * @return               SL_STATUS_OK if successful,
 *                       SL_STATUS_SECURITY_AES_CM_RESEED_NEEDED if the AES
 *                         countermeasure reseed threshold was crossed - call
 *                         sli_crypto_countermeasure_reseed() to maintain DPA resistance,
 *                       SL_STATUS_SECURITY_AES_CM_FAIL if the AES countermeasure
 *                         security threshold was exceeded - operation blocked,
 *                         reseed is mandatory before further AES operations,
 *                       relevant status code on other error
 ******************************************************************************/
SL_CODE_CLASSIFY(SL_CODE_COMPONENT_SLI_CRYPTO, SL_CODE_CLASS_TIME_CRITICAL)
sl_status_t sli_crypto_aes_ecb_radio(bool                    encrypt,
                                     sli_crypto_descriptor_t *key_descriptor,
                                     unsigned int            keybits,
                                     const unsigned char     input[SLI_CRYPTO_AES_BLOCK_SIZE],
                                     volatile unsigned char  output[SLI_CRYPTO_AES_BLOCK_SIZE]);

/***************************************************************************//**
 * @brief                AES-CMAC calculation optimized for radio
 *
 * @param key_descriptor AES key descriptor
 * @param keybits        Must be 128 or 256
 * @param input          Input buffer containing the message to be signed
 * @param length         Amount of bytes in the input buffer
 * @param output         16-byte output block for calculated CMAC
 *
 * @return               SL_STATUS_OK if successful,
 *                       SL_STATUS_SECURITY_AES_CM_RESEED_NEEDED if the AES
 *                         countermeasure reseed threshold was crossed - call
 *                         sli_crypto_countermeasure_reseed() to maintain DPA resistance,
 *                       SL_STATUS_SECURITY_AES_CM_FAIL if the AES countermeasure
 *                         security threshold was exceeded - operation blocked,
 *                         reseed is mandatory before further AES operations,
 *                       relevant status code on other error
 ******************************************************************************/
SL_CODE_CLASSIFY(SL_CODE_COMPONENT_SLI_CRYPTO, SL_CODE_CLASS_TIME_CRITICAL)
sl_status_t sli_crypto_aes_cmac_radio(sli_crypto_descriptor_t *key_descriptor,
                                      unsigned int            keybits,
                                      const unsigned char     *input,
                                      unsigned int            length,
                                      volatile unsigned char  output[SLI_CRYPTO_AES_BLOCK_SIZE]);
// #endif

/***************************************************************************//**
 * @brief                Initialize the crypto subsystem lock.
 *                       This must be called during sli_crypto_init() on RTOS
 *                       systems. On bare-metal systems, this is a no-op.
 *                       Locks are only needed when RTOS is present and at least one
 *                       of host entropy or CM counters is enabled.
 *
 * @return               SL_STATUS_OK if successful, error code otherwise.
 ******************************************************************************/
#if (defined(SL_CATALOG_MICRIUMOS_KERNEL_PRESENT) || defined(SL_CATALOG_FREERTOS_KERNEL_PRESENT)) \
    && (SLI_CRYPTO_USE_HOST_ENTROPY || SLI_CM_COUNTERS_ENABLED)
sl_status_t sli_crypto_init_lock(void);
#endif

#ifdef __cplusplus
}
#endif

#endif // SLI_CRYPTO_H