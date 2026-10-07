/***************************************************************************//**
 * @file
 * @brief Mbed TLS 'omnipresent' config content.
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

#ifndef SLI_MBEDTLS_OMIPRESENT_H
#define SLI_MBEDTLS_OMIPRESENT_H

#if defined(SL_COMPONENT_CATALOG_PRESENT)
  #include "sl_component_catalog.h"
#endif

#if !defined(SL_CATALOG_SE_CPC_PRIMARY_PRESENT)
  #include "em_device.h"
#endif

// -----------------------------------------------------------------------------
// Device differentiation logic

#if defined(CRYPTO_PRESENT)

#elif defined(SEMAILBOX_PRESENT) && defined(_SILICON_LABS_32B_SERIES_2)

  #define SLI_MBEDTLS_DEVICE_S2
  #define SLI_MBEDTLS_DEVICE_HSE

  #if defined(_SILICON_LABS_32B_SERIES_2_CONFIG_1)
    #define SLI_MBEDTLS_DEVICE_SE_V1
    #define SLI_MBEDTLS_DEVICE_HSE_V1
  #else
    #define SLI_MBEDTLS_DEVICE_SE_V2
    #define SLI_MBEDTLS_DEVICE_HSE_V2
  #endif

  #if (_SILICON_LABS_SECURITY_FEATURE == _SILICON_LABS_SECURITY_FEATURE_VAULT)
    #define SLI_MBEDTLS_DEVICE_HSE_VAULT_HIGH
  #else
    #define SLI_MBEDTLS_DEVICE_HSE_VAULT_MID
  #endif

#elif defined(SEMAILBOX_PRESENT) && defined(_SILICON_LABS_32B_SERIES_3)

  #define SLI_MBEDTLS_DEVICE_S3
  #define SLI_MBEDTLS_DEVICE_HC

  #if defined(_SILICON_LABS_32B_SERIES_3_CONFIG_301) && defined(SYMCRYPTO_PRESENT)
    #define SLI_MBEDTLS_DEVICE_HC_LPW
  #endif
  #define SLI_MBEDTLS_DEVICE_HSE
  #define SLI_MBEDTLS_DEVICE_SE_V2
  #define SLI_MBEDTLS_DEVICE_HSE_V2
  #if (_SILICON_LABS_SECURITY_FEATURE == _SILICON_LABS_SECURITY_FEATURE_VAULT)
    #define SLI_MBEDTLS_DEVICE_HSE_VAULT_HIGH
  #else
    #define SLI_MBEDTLS_DEVICE_HSE_VAULT_MID
  #endif

#elif defined(CRYPTOACC_PRESENT)

  #define SLI_MBEDTLS_DEVICE_S2
  #define SLI_MBEDTLS_DEVICE_VSE

  #if defined(_SILICON_LABS_32B_SERIES_2_CONFIG_2)
    #define SLI_MBEDTLS_DEVICE_SE_V1
    #define SLI_MBEDTLS_DEVICE_VSE_V1
  #else
    #define SLI_MBEDTLS_DEVICE_SE_V2
    #define SLI_MBEDTLS_DEVICE_VSE_V2
  #endif

#elif defined(SL_CATALOG_SE_CPC_PRIMARY_PRESENT)

  #define SLI_MBEDTLS_DEVICE_S2
  #define SLI_MBEDTLS_DEVICE_HSE

// #define SLI_MBEDTLS_DEVICE_SE_V1
// #define SLI_MBEDTLS_DEVICE_SE_V2
// #define SLI_MBEDTLS_DEVICE_HSE_V1
// #define SLI_MBEDTLS_DEVICE_HSE_V2
// #define SLI_MBEDTLS_DEVICE_HSE_VAULT_HIGH
// #define SLI_MBEDTLS_DEVICE_HSE_VAULT_MID

#elif defined(SLI_CRYPTOACC_PRESENT_SI91X)
  #define SLI_MBEDTLS_DEVICE_SI91X

#endif

// -----------------------------------------------------------------------------
// TrustZone Non-Secure adjustments
//
// On NS the legacy AES implementation is not compiled in (PSA reaches it via
// TF-M across the TZ boundary). Route CTR_DRBG through its PSA path so
// mbedtls/ctr_drbg.h does not transitively include mbedtls/aes.h, which is
// intentionally not exposed on NS.

#if defined(SL_TRUSTZONE_NONSECURE)
#define MBEDTLS_CTR_DRBG_USE_PSA_CRYPTO
#endif

#endif // SLI_MBEDTLS_OMIPRESENT_H
