/***************************************************************************/ /**
 * @file sli_lwip_mqtt_psa_crypto_config.h
 * @brief Mbed TLS 4.x / TF-PSA-Crypto configuration for MQTT with LwIP on Silicon Labs devices.
 *******************************************************************************
 * # License
 * <b>Copyright 2025 Silicon Laboratories Inc. www.silabs.com</b>
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
/* ---- TF-PSA-Crypto 1.x ---- */
#define MBEDTLS_PSA_CRYPTO_C
#define MBEDTLS_ASN1_PARSE_C
#define MBEDTLS_ASN1_WRITE_C
#define MBEDTLS_BASE64_C
#define MBEDTLS_PEM_PARSE_C
#define MBEDTLS_PK_C
#define MBEDTLS_PK_PARSE_C

#define PSA_WANT_ALG_ECDH                       1
#define PSA_WANT_ALG_ECDSA                      1
#define PSA_WANT_ALG_GCM                        1
#define PSA_WANT_ALG_HMAC                       1
#define PSA_WANT_ALG_RSA_PKCS1V15_SIGN          1
#define PSA_WANT_ALG_RSA_PSS                    1
#define PSA_WANT_ALG_SHA_256                    1
#define PSA_WANT_ALG_SHA_384                    1
#define PSA_WANT_ALG_SHA_512                    1
#define PSA_WANT_ALG_TLS12_PRF                  1
#define PSA_WANT_ECC_SECP_R1_256                1
#define PSA_WANT_KEY_TYPE_AES                   1
#define PSA_WANT_KEY_TYPE_ECC_PUBLIC_KEY        1
#define PSA_WANT_KEY_TYPE_ECC_KEY_PAIR_GENERATE 1
#define PSA_WANT_KEY_TYPE_RSA_PUBLIC_KEY        1
#define PSA_WANT_KEY_TYPE_RSA_KEY_PAIR_BASIC    1

#ifdef MBEDTLS_MPI_MAX_SIZE
#undef MBEDTLS_MPI_MAX_SIZE
#endif
#define MBEDTLS_MPI_MAX_SIZE 256

#ifdef MBEDTLS_THREADING_C
#undef MBEDTLS_THREADING_C
#endif
#ifdef MBEDTLS_LMS_C
#undef MBEDTLS_LMS_C
#endif
#ifdef MBEDTLS_PSA_CRYPTO_STORAGE_C
#undef MBEDTLS_PSA_CRYPTO_STORAGE_C
#endif

#define MBEDTLS_USE_PSA_CRYPTO
