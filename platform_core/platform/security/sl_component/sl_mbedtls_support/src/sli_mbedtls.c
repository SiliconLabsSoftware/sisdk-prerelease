/***************************************************************************//**
 * @file
 * @brief Initialize the Silicon Labs platform integration of mbedTLS.
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
#include "sli_mbedtls.h"

size_t sli_mbedtls_context_get_size(sli_mbedtls_context_name_t ctx_type)
{
  switch (ctx_type) {
    #if defined(MBEDTLS_DECLARE_PRIVATE_IDENTIFIERS)
    case SLI_MBEDTLS_CONTEXT_ENUM_NAME(mbedtls_aes_context):
      return sizeof(mbedtls_aes_context);
    case SLI_MBEDTLS_CONTEXT_ENUM_NAME(mbedtls_ccm_context):
      return sizeof(mbedtls_ccm_context);
    case SLI_MBEDTLS_CONTEXT_ENUM_NAME(mbedtls_cipher_context_t):
      return sizeof(mbedtls_cipher_context_t);
    case SLI_MBEDTLS_CONTEXT_ENUM_NAME(mbedtls_ctr_drbg_context):
      return sizeof(mbedtls_ctr_drbg_context);
    case SLI_MBEDTLS_CONTEXT_ENUM_NAME(mbedtls_entropy_context):
      return sizeof(mbedtls_entropy_context);
    case SLI_MBEDTLS_CONTEXT_ENUM_NAME(mbedtls_sha1_context):
      return sizeof(mbedtls_sha1_context);
    case SLI_MBEDTLS_CONTEXT_ENUM_NAME(mbedtls_sha256_context):
      return sizeof(mbedtls_sha256_context);
    #endif
    case SLI_MBEDTLS_CONTEXT_ENUM_NAME(mbedtls_md_context_t):
      return sizeof(mbedtls_md_context_t);
    case SLI_MBEDTLS_CONTEXT_ENUM_NAME(mbedtls_pk_context):
      return sizeof(mbedtls_pk_context);
    case SLI_MBEDTLS_CONTEXT_ENUM_NAME(mbedtls_ssl_config):
      return sizeof(mbedtls_ssl_config);
    case SLI_MBEDTLS_CONTEXT_ENUM_NAME(mbedtls_ssl_context):
      return sizeof(mbedtls_ssl_context);
    case SLI_MBEDTLS_CONTEXT_ENUM_NAME(mbedtls_ssl_cookie_ctx):
      return sizeof(mbedtls_ssl_cookie_ctx);
    case SLI_MBEDTLS_CONTEXT_ENUM_NAME(mbedtls_x509_crt):
      return sizeof(mbedtls_x509_crt);
    default:
      return 0;
  }
}
