/***************************************************************************//**
 * @file
 * @brief PSA Crypto acceleration claims for Silicon Labs SDK configurations.
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

#ifndef SLI_PSA_ACCELERATION_OPTIM_H
#define SLI_PSA_ACCELERATION_OPTIM_H

#if !defined(SLI_PSA_ACCELERATION_H)
#error "sli_psa_acceleration.h must be included before sli_psa_acceleration_optim.h"
#endif

// -----------------------------------------------------------------------------
// Configuration-dependent MBEDTLS_PSA_ACCEL_* claims.
//
// sli_psa_acceleration.h only claims mechanisms that the drivers cover for
// every compatible key type and variant, which keeps it usable by
// integrations that do not use the SDK configuration (e.g. Zephyr). For the
// configurations the SDK generates, the drivers also fully cover some of the
// remaining mechanisms. Claiming them here keeps TF-PSA-Crypto from enabling
// redundant built-in implementations and their software prerequisites.
//
// This file must be included after every PSA_WANT_* and MBEDTLS_PSA_BUILTIN_*
// definition of the SDK configuration. TF-PSA-Crypto may still add
// PSA_WANT_KEY_TYPE_AES, PSA_WANT_ALG_CMAC, PSA_WANT_ALG_ECB_NO_PADDING and
// PSA_WANT_ALG_HMAC as dependencies, so no claim below may become incomplete
// when those are added.
//
// Define SLI_PSA_DISABLE_BUILTIN_OPTIMIZATIONS to restore upstream built-in
// selection, for example when adding a vendor hash not checked below.

// An explicitly selected AES built-in needs the legacy AES module, even when
// AES and every requested block mode are accelerated.
#if defined(MBEDTLS_PSA_BUILTIN_KEY_TYPE_AES) && !defined(MBEDTLS_AES_C)
#define MBEDTLS_AES_C
#endif

#if (defined(SLI_MBEDTLS_DEVICE_HSE) || defined(SLI_MBEDTLS_DEVICE_VSE)) \
  && !defined(SLI_MBEDTLS_DEVICE_SI91X)                                  \
  && !defined(SLI_PSA_DISABLE_BUILTIN_OPTIMIZATIONS)                     \
  && !defined(MBEDTLS_CONFIG_ADJUST_TEST_ACCELERATORS)                   \
  && !defined(TF_PSA_CRYPTO_TEST_LIBTESTDRIVER1)

// -------------------------------------
// Block modes, CMAC, CCM and GCM
//
// These modes can also operate on ARIA or Camellia keys, which the drivers do
// not cover. Claim them only when neither of those key types is requested.
// An explicitly selected built-in (or legacy CMAC/CCM/GCM module) keeps its
// software prerequisites by not being claimed.

#if defined(SLI_PSA_ACCEL_KEY_TYPE_AES) \
  && !defined(PSA_WANT_KEY_TYPE_ARIA)   \
  && !defined(PSA_WANT_KEY_TYPE_CAMELLIA)

#if defined(SLI_PSA_ACCEL_ALG_CTR) && !defined(MBEDTLS_PSA_BUILTIN_ALG_CTR)
#define MBEDTLS_PSA_ACCEL_ALG_CTR
#endif

#if defined(SLI_PSA_ACCEL_ALG_CFB) && !defined(MBEDTLS_PSA_BUILTIN_ALG_CFB)
#define MBEDTLS_PSA_ACCEL_ALG_CFB
#endif

#if defined(SLI_PSA_ACCEL_ALG_OFB) && !defined(MBEDTLS_PSA_BUILTIN_ALG_OFB)
#define MBEDTLS_PSA_ACCEL_ALG_OFB
#endif

#if defined(SLI_PSA_ACCEL_ALG_ECB_NO_PADDING) \
  && !defined(MBEDTLS_PSA_BUILTIN_ALG_ECB_NO_PADDING)
#define MBEDTLS_PSA_ACCEL_ALG_ECB_NO_PADDING
#endif

#if defined(SLI_PSA_ACCEL_ALG_CBC_NO_PADDING) \
  && !defined(MBEDTLS_PSA_BUILTIN_ALG_CBC_NO_PADDING)
#define MBEDTLS_PSA_ACCEL_ALG_CBC_NO_PADDING
#endif

#if defined(SLI_PSA_ACCEL_ALG_CBC_PKCS7) \
  && !defined(MBEDTLS_PSA_BUILTIN_ALG_CBC_PKCS7)
#define MBEDTLS_PSA_ACCEL_ALG_CBC_PKCS7
#endif

#if defined(SLI_PSA_ACCEL_ALG_CMAC)          \
  && !defined(MBEDTLS_PSA_BUILTIN_ALG_CMAC) \
  && !defined(MBEDTLS_CMAC_C)
#define MBEDTLS_PSA_ACCEL_ALG_CMAC
#endif

#if defined(SLI_PSA_ACCEL_ALG_CCM)          \
  && !defined(MBEDTLS_PSA_BUILTIN_ALG_CCM) \
  && !defined(MBEDTLS_CCM_C)
#define MBEDTLS_PSA_ACCEL_ALG_CCM
#endif

#if defined(SLI_PSA_ACCEL_ALG_GCM)          \
  && !defined(MBEDTLS_PSA_BUILTIN_ALG_GCM) \
  && !defined(MBEDTLS_GCM_C)
#define MBEDTLS_PSA_ACCEL_ALG_GCM
#endif

// Driver support for CCM* without a tag is enabled by PSA_WANT_ALG_CCM.
// Requesting PSA_WANT_ALG_CCM_STAR_NO_TAG alone still needs software fallback.
#if defined(SLI_PSA_ACCEL_ALG_CCM) && defined(PSA_WANT_ALG_CCM) \
  && !defined(MBEDTLS_PSA_BUILTIN_ALG_CCM_STAR_NO_TAG)          \
  && !defined(MBEDTLS_CCM_C)
#define MBEDTLS_PSA_ACCEL_ALG_CCM_STAR_NO_TAG
#endif

#endif // AES is the only requested block key type

// -------------------------------------
// HMAC
//
// Claim HMAC only if the MAC driver supports every requested hash for both
// single-shot and multipart operations.

#if defined(SLI_PSA_ACCEL_ALG_HMAC)                                   \
  && !defined(MBEDTLS_PSA_BUILTIN_ALG_HMAC)                           \
  && !defined(PSA_WANT_ALG_MD5) && !defined(PSA_WANT_ALG_RIPEMD160)   \
  && !defined(PSA_WANT_ALG_SHA3_224) && !defined(PSA_WANT_ALG_SHA3_256) \
  && !defined(PSA_WANT_ALG_SHA3_384) && !defined(PSA_WANT_ALG_SHA3_512) \
  && !defined(PSA_WANT_ALG_SHAKE128) && !defined(PSA_WANT_ALG_SHAKE256) \
  && !defined(PSA_WANT_ALG_AES_MMO_ZIGBEE)                            \
  && (!defined(PSA_WANT_ALG_SHA_1) || defined(SLI_PSA_ACCEL_ALG_SHA_1))       \
  && (!defined(PSA_WANT_ALG_SHA_224) || defined(SLI_PSA_ACCEL_ALG_SHA_224))   \
  && (!defined(PSA_WANT_ALG_SHA_256) || defined(SLI_PSA_ACCEL_ALG_SHA_256))   \
  && (!defined(PSA_WANT_ALG_SHA_384) || defined(SLI_PSA_ACCEL_ALG_SHA_384))   \
  && (!defined(PSA_WANT_ALG_SHA_512) || defined(SLI_PSA_ACCEL_ALG_SHA_512))
#define MBEDTLS_PSA_ACCEL_ALG_HMAC
#endif

#endif // HSE or VSE, built-in optimizations enabled

#endif // SLI_PSA_ACCELERATION_OPTIM_H
