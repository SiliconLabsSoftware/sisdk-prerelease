/***************************************************************************//**
 * @file
 * @brief A __very bad__ implementation of a non-volatile seed for randomness
          generation in Mbed TLS. In fact, this implemention is both predictable
          and volatile. The purpose of this entropy source is to be used before
          NVM3 and/or a proper TRNG is available on S3 devices.
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

#include <tf-psa-crypto/build_info.h>

#if defined(MBEDTLS_PLATFORM_NV_SEED_ALT)

#include <string.h>
#include <psa/crypto.h>

// -----------------------------------------------------------------------------
// Static variables

static uint32_t sli_volatile_seed = 0;

// -----------------------------------------------------------------------------
// Public functions

int sli_nv_seed_read(unsigned char *buf, size_t buf_len)
{
  psa_status_t status = PSA_ERROR_GENERIC_ERROR;
  uint8_t hash_buffer[PSA_HASH_LENGTH(MBEDTLS_PSA_CRYPTO_RNG_HASH)];
  psa_hash_operation_t hash_operation = PSA_HASH_OPERATION_INIT;
  size_t hash_length;

  status = psa_hash_setup(&hash_operation, MBEDTLS_PSA_CRYPTO_RNG_HASH);
  if (status != PSA_SUCCESS) {
    goto exit;
  }

  // Volatile seed
  status = psa_hash_update(&hash_operation, (const unsigned char *)&sli_volatile_seed, sizeof(sli_volatile_seed));
  if (status != PSA_SUCCESS) {
    goto exit;
  }

  // SRAM
  status = psa_hash_update(&hash_operation, (const unsigned char *)SRAM_BASE, SRAM_SIZE);
  if (status != PSA_SUCCESS) {
    goto exit;
  }

  status = psa_hash_finish(&hash_operation, hash_buffer, &hash_length);
  if (status != PSA_SUCCESS) {
    goto exit;
  }

  if (sizeof(hash_buffer) < buf_len) {
    status = PSA_ERROR_BUFFER_TOO_SMALL;
    goto exit;
  }

  exit:
  psa_hash_abort(&hash_operation);

  if (status == PSA_SUCCESS) {
    memcpy(buf, hash_buffer, buf_len);
  }

  return status;
}

int sli_nv_seed_write(unsigned char *buf, size_t buf_len)
{
  // Do nothing. The non-volatile dummy entropy seed is very much a predictable
  // and volatile one here.
  (void)buf;
  return buf_len;
}

#endif // MBEDTLS_PLATFORM_NV_SEED_ALT
