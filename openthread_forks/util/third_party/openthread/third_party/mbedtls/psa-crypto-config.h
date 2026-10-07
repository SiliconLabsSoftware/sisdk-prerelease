/*
 *  Copyright (c) 2018, The OpenThread Authors.
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are met:
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *  3. Neither the name of the copyright holder nor the
 *     names of its contributors may be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 *  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef OT_PSA_CRYPTO_CONFIG_H
#define OT_PSA_CRYPTO_CONFIG_H

#include "openthread-core-config.h"

#include <stdio.h>
#include <stdlib.h>

#include <openthread/config.h>
#include <openthread/platform/crypto.h>

#define TF_PSA_CRYPTO_CONFIG_VERSION 0x01010000

// ==============================================================================
// PSA Crypto core
// ==============================================================================

#define MBEDTLS_PSA_CRYPTO_C

#define PSA_WANT_ALG_CCM                        1
#define PSA_WANT_ALG_CMAC                       1
#define PSA_WANT_ALG_HMAC                       1
#define PSA_WANT_ALG_SHA_224                    1
#define PSA_WANT_ALG_SHA_256                    1
#define PSA_WANT_ALG_JPAKE                      1
#define PSA_WANT_ALG_TLS12_ECJPAKE_TO_PMS       1
#define PSA_WANT_ALG_TLS12_PRF                  1
#define PSA_WANT_ECC_SECP_R1_256                1
#define PSA_WANT_KEY_TYPE_AES                   1
#define PSA_WANT_KEY_TYPE_HMAC                  1
#define PSA_WANT_KEY_TYPE_PASSWORD              1
#define PSA_WANT_KEY_TYPE_DERIVE                1
#define PSA_WANT_KEY_TYPE_RAW_DATA              1
#define PSA_WANT_KEY_TYPE_ECC_KEY_PAIR_BASIC    1
#define PSA_WANT_KEY_TYPE_ECC_KEY_PAIR_IMPORT   1
#define PSA_WANT_KEY_TYPE_ECC_KEY_PAIR_GENERATE 1
#define PSA_WANT_KEY_TYPE_ECC_PUBLIC_KEY        1
#define PSA_WANT_ALG_ECDSA                      1
#define PSA_WANT_ALG_ECDH                       1

#if OPENTHREAD_CONFIG_DETERMINISTIC_ECDSA_ENABLE
#define PSA_WANT_ALG_DETERMINISTIC_ECDSA        1
#endif

#if OPENTHREAD_CONFIG_BLE_TCAT_ENABLE
#define PSA_WANT_ALG_GCM                        1
#endif

// Size / performance tweaks retained from the OT mbedtls 3.x config.
#define MBEDTLS_AES_ONLY_128_BIT_KEY_LENGTH
#define MBEDTLS_AES_ROM_TABLES
#define MBEDTLS_BLOCK_CIPHER_NO_DECRYPT
#define MBEDTLS_SHA256_SMALLER
#define MBEDTLS_ECP_NIST_OPTIM
#define MBEDTLS_DEPRECATED_REMOVED
#define MBEDTLS_DEPRECATED_WARNING

#define MBEDTLS_MPI_WINDOW_SIZE            1 /**< Maximum windows size used. */
#define MBEDTLS_MPI_MAX_SIZE              32 /**< Maximum number of bytes for usable MPIs. */
#define MBEDTLS_ECP_WINDOW_SIZE            2 /**< Maximum window size used */
#define MBEDTLS_ECP_FIXED_POINT_OPTIM      0 /**< Enable fixed-point speed-up */

// ==============================================================================
// Platform / entropy
// ==============================================================================

#define MBEDTLS_PLATFORM_SNPRINTF_MACRO snprintf

#if OPENTHREAD_CONFIG_HEAP_EXTERNAL_ENABLE
#define MBEDTLS_PLATFORM_STD_CALLOC     otPlatCryptoCAlloc /**< Default allocator to use, can be undefined */
#define MBEDTLS_PLATFORM_STD_FREE       otPlatCryptoFree   /**< Default free to use, can be undefined */
#else
#define MBEDTLS_MEMORY_BUFFER_ALLOC_C
#endif

// For host / simulation builds:
// Use PSA's built-in CTR_DRBG seeded once from /dev/urandom.
#if OPENTHREAD_EXAMPLES_SIMULATION || OPENTHREAD_PLATFORM_POSIX
#define MBEDTLS_CTR_DRBG_C
#define MBEDTLS_PSA_BUILTIN_GET_ENTROPY
// Lift the on-device AES-128-only restriction so CTR_DRBG can use AES-256.
#undef MBEDTLS_AES_ONLY_128_BIT_KEY_LENGTH
#else
// For device builds:
// Use the platform's hardware TRNG via mbedtls_psa_external_get_random() override.
#define MBEDTLS_PSA_CRYPTO_EXTERNAL_RNG
#endif

#define MBEDTLS_PLATFORM_C
#define MBEDTLS_PLATFORM_MEMORY
#define MBEDTLS_PLATFORM_NO_STD_FUNCTIONS
#define MBEDTLS_HAVE_ASM

// MD layer (HMAC / generic digests). Required for otPlatCryptoHmacSha256*
// and for Mbed TLS 4.x where mbedtls_md_setup() no longer accepts hmac=1.
#define MBEDTLS_MD_C

// PK / PEM / BASE64 (configured via TF-PSA-Crypto in Mbed TLS 4.x)
#define MBEDTLS_PK_C
#define MBEDTLS_PK_PARSE_C

#if OPENTHREAD_CONFIG_COAP_SECURE_API_ENABLE || OPENTHREAD_CONFIG_TLS_ENABLE || OPENTHREAD_CONFIG_ECDSA_ENABLE
#define MBEDTLS_BASE64_C
#define MBEDTLS_PEM_PARSE_C
#endif

#if OPENTHREAD_CONFIG_ECDSA_ENABLE
#define MBEDTLS_PK_WRITE_C
#endif

#if defined(MBEDTLS_USER_CONFIG_FILE)
#include MBEDTLS_USER_CONFIG_FILE
#endif

#endif /* OT_PSA_CRYPTO_CONFIG_H */
