/***************************************************************************//**
 * @file
 * @brief DDP commands for certificate handling
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

#include <stdio.h>
#include <string.h>
#include "sl_status.h"
#include "em_system.h"
#include "psa/crypto.h"
#include "psa/crypto_values.h"
#include "sl_ddp_types.h"

// -----------------------------------------------------------------------------
// Definitions

#define UUID_LEN                  (16)
#define UUID_STR_LEN              (37)
#define CRYPTO_SHA_1_LEN          (20)

// -----------------------------------------------------------------------------
// Private function declarations and variables

// UUID Namespace
static const uint8_t uuid64_namespace[16] = {
  0x70, 0x0b, 0xaf, 0xdf, 0xd5, 0xec, 0xc3, 0x9b,
  0x37, 0x04, 0xa1, 0x2c, 0x07, 0x67, 0x86, 0x9c,
};

// PSA_ALG_SHA_1 hashing
static psa_status_t calculate_sha_1(const uint8_t *ptr,
                                    size_t len,
                                    uint8_t *hash);

// -----------------------------------------------------------------------------
// Public functions

/******************************************************************************
 * DDP Command for getting the Common Name.
 *****************************************************************************/
int sl_ddp_cmd_cert_get_cn(const uint8_t *input,
                           uint16_t input_len,
                           uint8_t *output,
                           uint16_t output_size,
                           uint16_t *output_len)
{
  (void)input;
  (void)input_len;

  if ((output == NULL) || (output_len == NULL)) {
    return SL_DDP_ERROR_ARG;
  }

  if (output_size < UUID_STR_LEN) {
    return SL_DDP_ERROR_LENGTH;
  }

  // Get UUID so it can be used as the Common Name of the certificate
  // UUID generation procedure shall follow the standard UUID format as defined in RFC4122
  // https://www.ietf.org/rfc/rfc4122.txt

  psa_status_t status;
  uint64_t uid = SYSTEM_GetUnique();
  uint8_t tmp[24];
  uint8_t digest[CRYPTO_SHA_1_LEN];

  memcpy(tmp, uuid64_namespace, 16);
  tmp[16] = (uid >> 56) & 0xff;
  tmp[17] = (uid >> 48) & 0xff;
  tmp[18] = (uid >> 40) & 0xff;
  tmp[19] = (uid >> 32) & 0xff;
  tmp[20] = (uid >> 24) & 0xff;
  tmp[21] = (uid >> 16) & 0xff;
  tmp[22] = (uid >>  8) & 0xff;
  tmp[23] = (uid >>  0) & 0xff;

  status = calculate_sha_1(tmp, sizeof(tmp), digest);
  if (status != PSA_SUCCESS) {
    return (int)status;
  }

  uint8_t uuid[UUID_LEN] = { 0 };
  memcpy(uuid, digest, UUID_LEN);
  uuid[6] &= 0x0f; // Set the four most significant bits (bits 12 through 15) of the time_hi_and_version field
  uuid[6] |= 0x50; // to the 4-bit version number (version 5: name-based, SHA-1)
  uuid[8] &= 0x3f; // Set the two most significant bits (bits 6 and 7) of the clock_seq_hi_and_reserved field
  uuid[8] |= 0x80; // to zero and one, respectively

  // Construct UUID string
  char uuid_str[UUID_STR_LEN];
  char *p = uuid_str;

  for (int i = 0; i < UUID_LEN; i++) {
    if ((i == 4) || (i == 6) || (i == 8) || (i == 10)) {
      *p++ = '-'; // Add hyphen between the UUID segments
    }
    sprintf(p, "%02x", uuid[i]);
    p += 2;
  }

  // Set output
  *output_len = sizeof(uuid_str);
  memcpy(output, uuid_str, sizeof(uuid_str));
  return (int)status;
}

// -----------------------------------------------------------------------------
// Private functions

/******************************************************************************
 * PSA_ALG_SHA_1 hashing.
 *****************************************************************************/
static psa_status_t calculate_sha_1(const uint8_t *ptr,
                                    size_t len,
                                    uint8_t *hash)
{
  psa_status_t status;
  psa_hash_operation_t operation = { 0 };
  psa_algorithm_t alg = PSA_ALG_SHA_1;
  size_t out_len = 0;
  uint8_t result[CRYPTO_SHA_1_LEN];

  if (!ptr || !len  || !hash) {
    return PSA_ERROR_INVALID_ARGUMENT;
  }

  status = psa_hash_setup(&operation, alg);
  if (status != PSA_SUCCESS) {
    return status;
  }

  status = psa_hash_update(&operation, ptr, len);
  if (status != PSA_SUCCESS) {
    return status;
  }

  status = psa_hash_finish(&operation, result, CRYPTO_SHA_1_LEN, &out_len);
  if (status != PSA_SUCCESS) {
    return status;
  }

  memcpy(hash, result, CRYPTO_SHA_1_LEN);
  return status;
}
