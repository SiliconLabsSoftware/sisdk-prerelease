/***************************************************************************//**
 * @file
 * @brief PSA Crypto device acceleration capabilities.
 *******************************************************************************
 * # License
 * <b>Copyright 2023 Silicon Laboratories Inc. www.silabs.com</b>
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

#ifndef SLI_PSA_ACCELERATION_H
#define SLI_PSA_ACCELERATION_H

#include "sli_mbedtls_omnipresent.h"
#if !defined(SLI_MBEDTLS_DEVICE_SI91X)
#include "sli_se_manager_features.h"
#endif

// -----------------------------------------------------------------------------
// Silicon Labs driver capabilities.
//
// SLI_PSA_ACCEL_* means that a driver implements some variants of a mechanism.
// Keep this device capability table independent of PSA_WANT_* and upstream
// builtin selection. sli_psa_driver_features.h combines it with the finalized
// PSA configuration to enable driver code.

// -------------------------------------
// Hash

#define SLI_PSA_ACCEL_ALG_SHA_1
#define SLI_PSA_ACCEL_ALG_SHA_224
#define SLI_PSA_ACCEL_ALG_SHA_256

#if defined(SLI_MBEDTLS_DEVICE_HSE_VAULT_HIGH) || defined (SLI_MBEDTLS_DEVICE_SI91X)
  #define SLI_PSA_ACCEL_ALG_SHA_384
  #define SLI_PSA_ACCEL_ALG_SHA_512
#endif

// -------------------------------------
// Cipher

#define SLI_PSA_ACCEL_KEY_TYPE_AES
#define SLI_PSA_ACCEL_ALG_ECB_NO_PADDING
#define SLI_PSA_ACCEL_ALG_CBC_NO_PADDING
#define SLI_PSA_ACCEL_ALG_CTR

#if !defined(SLI_MBEDTLS_DEVICE_SI91X)
#define SLI_PSA_ACCEL_ALG_CBC_PKCS7
#define SLI_PSA_ACCEL_ALG_CFB
#define SLI_PSA_ACCEL_ALG_OFB
#endif

#if (defined(SLI_MBEDTLS_DEVICE_HSE_VAULT_HIGH) && !defined(SLI_MBEDTLS_DEVICE_HC_LPW)) || defined(SLI_MBEDTLS_DEVICE_SI91X) || (defined(SLI_MBEDTLS_DEVICE_HC) && !defined(SLI_MBEDTLS_DEVICE_HC_LPW))
  #define SLI_PSA_ACCEL_KEY_TYPE_CHACHA20
#endif

// -------------------------------------
// AEAD

#define SLI_PSA_ACCEL_ALG_GCM
#define SLI_PSA_ACCEL_ALG_CCM

#if (defined(SLI_MBEDTLS_DEVICE_HSE_VAULT_HIGH) && !defined(SLI_MBEDTLS_DEVICE_HC_LPW)) || defined(SLI_MBEDTLS_DEVICE_SI91X) || (defined(SLI_MBEDTLS_DEVICE_HC) && !defined(SLI_MBEDTLS_DEVICE_HC_LPW))
  #define SLI_PSA_ACCEL_ALG_CHACHA20_POLY1305
#endif

// -------------------------------------
// MAC

#define SLI_PSA_ACCEL_ALG_CMAC

#define SLI_PSA_ACCEL_ALG_HMAC

#if defined(SLI_MBEDTLS_DEVICE_HSE_V1)
  #define SLI_PSA_ACCEL_ALG_CBC_MAC
#endif

// -------------------------------------
// Elliptic curves

#define SLI_PSA_ACCEL_KEY_TYPE_ECC_KEY_PAIR
#define SLI_PSA_ACCEL_KEY_TYPE_ECC_PUBLIC_KEY
#define SLI_PSA_ACCEL_KEY_TYPE_ECC_KEY_PAIR_BASIC
#define SLI_PSA_ACCEL_KEY_TYPE_ECC_KEY_PAIR_IMPORT
#define SLI_PSA_ACCEL_KEY_TYPE_ECC_KEY_PAIR_EXPORT
#define SLI_PSA_ACCEL_KEY_TYPE_ECC_KEY_PAIR_GENERATE
#define SLI_PSA_ACCEL_ECC_SECP_R1_192
#define SLI_PSA_ACCEL_ECC_SECP_R1_256

#if (defined(SLI_MBEDTLS_DEVICE_S2) && !defined(SLI_MBEDTLS_DEVICE_HSE_V1)) || defined(SLI_MBEDTLS_DEVICE_SI91X)
  #define SLI_PSA_ACCEL_ECC_SECP_R1_224
#endif

#if defined(SLI_SE_SUPPORTS_LARGER_THAN_256_BIT_KEYS)
  #define SLI_PSA_ACCEL_ECC_SECP_R1_384
  #define SLI_PSA_ACCEL_ECC_SECP_R1_521
#endif

#if defined(SLI_MBEDTLS_DEVICE_VSE)
  #define SLI_PSA_ACCEL_ECC_SECP_K1_256
#endif

#if defined(SLI_MBEDTLS_DEVICE_HSE_V1) && defined(SLI_MBEDTLS_DEVICE_HSE_VAULT_HIGH) \
  || defined(SLI_MBEDTLS_DEVICE_HSE_V2)
  #define SLI_PSA_ACCEL_ECC_MONTGOMERY_255
#endif

#if defined(SLI_SE_SUPPORTS_LARGER_THAN_256_BIT_KEYS)
  #define SLI_PSA_ACCEL_ECC_MONTGOMERY_448
#endif

// -------------------------------------
// Key agreement

#define SLI_PSA_ACCEL_ALG_ECDH

#if defined(SLI_MBEDTLS_DEVICE_HSE)
  #define SLI_PSA_ACCEL_ALG_JPAKE
#endif

// -------------------------------------
// Signature

#define SLI_PSA_ACCEL_ALG_ECDSA

// -----------------------------------------------------------------------------
// Upstream driver-only claims.
//
// MBEDTLS_PSA_ACCEL_* suppresses upstream builtin implementations. Unlike the
// capability table above, these claims must cover a complete mechanism and are
// unconditional for a device (never conditional on PSA_WANT_*).
//
// Block modes, CMAC and CBC-MAC are not claimed: the drivers implement AES,
// but not the other compatible key types (ARIA/Camellia). HMAC is not claimed:
// even Vault High does not implement every hash supported by PSA. CCM/GCM and
// ChaCha20-Poly1305 retain their builtins for unsupported key types, nonces or
// multipart variants. J-PAKE retains its builtin for passwords longer than
// the SE driver's 32-byte limit (both backends restrict the peer identities).
//
// ECC is described upstream by orthogonal curve/key-operation/algorithm
// mechanisms. The claims below follow its ECC triplet rules: they apply to
// accelerated curves, while unclaimed curves retain software support. There
// is no claim for deterministic ECDSA, key-pair derivation or restartable APIs.
//
// SiSDK optimizations for codesize are defined in
// sli_psa_acceleration_optim.h.

#if defined(SLI_MBEDTLS_DEVICE_HSE) \
  || defined(SLI_MBEDTLS_DEVICE_VSE) \
  || defined(SLI_MBEDTLS_DEVICE_SI91X)

#define MBEDTLS_PSA_ACCEL_ALG_SHA_1
#define MBEDTLS_PSA_ACCEL_ALG_SHA_224
#define MBEDTLS_PSA_ACCEL_ALG_SHA_256

#if defined(SLI_PSA_ACCEL_ALG_SHA_384)
#define MBEDTLS_PSA_ACCEL_ALG_SHA_384
#endif
#if defined(SLI_PSA_ACCEL_ALG_SHA_512)
#define MBEDTLS_PSA_ACCEL_ALG_SHA_512
#endif

#define MBEDTLS_PSA_ACCEL_KEY_TYPE_AES
#if defined(SLI_PSA_ACCEL_KEY_TYPE_CHACHA20)
#define MBEDTLS_PSA_ACCEL_KEY_TYPE_CHACHA20
#endif

#define MBEDTLS_PSA_ACCEL_KEY_TYPE_ECC_PUBLIC_KEY
#define MBEDTLS_PSA_ACCEL_KEY_TYPE_ECC_KEY_PAIR_BASIC
#define MBEDTLS_PSA_ACCEL_KEY_TYPE_ECC_KEY_PAIR_IMPORT
#define MBEDTLS_PSA_ACCEL_KEY_TYPE_ECC_KEY_PAIR_EXPORT
#define MBEDTLS_PSA_ACCEL_KEY_TYPE_ECC_KEY_PAIR_GENERATE
#define MBEDTLS_PSA_ACCEL_ECC_SECP_R1_256
#if defined(SLI_PSA_ACCEL_ECC_SECP_R1_224)
#define MBEDTLS_PSA_ACCEL_ECC_SECP_R1_224
#endif
#if defined(SLI_PSA_ACCEL_ECC_SECP_R1_384)
#define MBEDTLS_PSA_ACCEL_ECC_SECP_R1_384
#endif
#if defined(SLI_PSA_ACCEL_ECC_SECP_R1_521)
#define MBEDTLS_PSA_ACCEL_ECC_SECP_R1_521
#endif
#if defined(SLI_PSA_ACCEL_ECC_SECP_K1_256)
#define MBEDTLS_PSA_ACCEL_ECC_SECP_K1_256
#endif
#if defined(SLI_PSA_ACCEL_ECC_MONTGOMERY_255)
#define MBEDTLS_PSA_ACCEL_ECC_MONTGOMERY_255
#endif
#if defined(SLI_PSA_ACCEL_ECC_MONTGOMERY_448)
#define MBEDTLS_PSA_ACCEL_ECC_MONTGOMERY_448
#endif
#define MBEDTLS_PSA_ACCEL_ALG_ECDH
#define MBEDTLS_PSA_ACCEL_ALG_ECDSA

#endif // HSE, VSE or SI91X

#endif // SLI_PSA_ACCELERATION_H
