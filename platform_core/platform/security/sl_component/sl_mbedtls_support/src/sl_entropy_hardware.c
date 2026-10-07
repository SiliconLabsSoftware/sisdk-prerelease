/***************************************************************************//**
 * @file
 * @brief Entropy driver for Silicon Labs devices.
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

// -------------------------------------
// Includes

#include "tf-psa-crypto/build_info.h"

#if defined(MBEDTLS_ENTROPY_HARDWARE_ALT)
#include "entropy_poll.h"
#include "psa/crypto.h"

psa_status_t mbedtls_psa_external_get_random(
  mbedtls_psa_external_random_context_t *context,
  uint8_t *output, size_t output_size, size_t *output_length);

#if !defined(MBEDTLS_ERR_ENTROPY_SOURCE_FAILED)
/* Repeat declaration of MBEDTLS_ERR_ENTROPY_SOURCE_FAILED since the full entropy.h
 * header is not always a clean include. I.e. when mbedtls_hardware_poll is used
 * without having the full entropy module (with collector) present, the header will
 * potentially complain about missing a SHA256/SHA512 context structure definition. */
#define MBEDTLS_ERR_ENTROPY_SOURCE_FAILED -0x003C
#endif

// -------------------------------------
// Global function definitions

int mbedtls_hardware_poll(void *data,
                          unsigned char *output,
                          size_t len,
                          size_t *olen)
{
  psa_status_t status = mbedtls_psa_external_get_random((mbedtls_psa_external_random_context_t *)data, output, len, olen);
  if (status == PSA_SUCCESS) {
    return 0;
  } else {
    return MBEDTLS_ERR_ENTROPY_SOURCE_FAILED;
  }
}

#endif // MBEDTLS_ENTROPY_HARDWARE_ALT
