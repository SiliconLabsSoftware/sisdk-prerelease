/***************************************************************************//**
 * @file
 * @brief Support for non-volatile-seed based entropy on Silicon Labs devices
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

#include <tf-psa-crypto/build_info.h>

#if defined(MBEDTLS_PLATFORM_NV_SEED_ALT)

#include <string.h>
#include "em_device.h"
#include "nvm3_default.h"
#include "psa/crypto.h"

// -----------------------------------------------------------------------------
// Defines

#ifndef SLI_NV_SEED_NVM3_ID
/** This NVM3 ID has been specifically allocated for the purpose of storing a
 * non-volatile DRBG seed. The ID for where to store the seed can be overridden,
 * but its default value should not be reused for any other purpose. */
#define SLI_NV_SEED_NVM3_ID  (0x870FFUL)
#endif

// -----------------------------------------------------------------------------
// Static variables

static int sli_nv_seed_has_been_opened = 0;

// -----------------------------------------------------------------------------
// Static functions

static int sli_nv_seed_init(void)
{
  if ( sli_nv_seed_has_been_opened == 0 ) {
    Ecode_t nvm3_status = nvm3_initDefault();
    if ( nvm3_status != ECODE_NVM3_OK ) {
      return PSA_ERROR_DATA_INVALID;
    }
    sli_nv_seed_has_been_opened = 1;
  }
  return 0;
}

// If the seed hasn't been generated yet, or has somehow been lost (NVM3 area got wiped)
// then we generate a device unique seed by hashing the contents of the device unique
// data area (containing serial number, calibration data, etc) and the entire RAM content.
static int sli_nv_seed_generate(uint8_t *buffer, size_t requested_length)
{
  psa_status_t status = PSA_ERROR_GENERIC_ERROR;
  psa_hash_operation_t hash_operation = PSA_HASH_OPERATION_INIT;
  uint8_t hash_buffer[PSA_HASH_LENGTH(MBEDTLS_PSA_CRYPTO_RNG_HASH)];
  size_t hash_length;

  status = psa_hash_setup(&hash_operation, MBEDTLS_PSA_CRYPTO_RNG_HASH);
  if (status != PSA_SUCCESS) {
    goto exit;
  }

  // Device info
  status = psa_hash_update(&hash_operation, (const unsigned char *)DEVINFO, sizeof(DEVINFO_TypeDef));
  if (status != PSA_SUCCESS) {
    goto exit;
  }
 
  // SRAM
  status = psa_hash_update(&hash_operation, (const unsigned char *)SRAM_BASE, SRAM_SIZE);
  if (status != PSA_SUCCESS) {
    goto exit;
  }

  status = psa_hash_finish(&hash_operation, hash_buffer, sizeof(hash_buffer), &hash_length);
  if (status != PSA_SUCCESS) {
    goto exit;
  }

  if (hash_length < requested_length) {
    status = PSA_ERROR_BUFFER_TOO_SMALL;
    goto exit;
  }

  exit:
  psa_hash_abort(&hash_operation);

  if (status == PSA_SUCCESS) {
    memcpy(buffer, hash_buffer, requested_length);
  }
  return status;
}

// -----------------------------------------------------------------------------
// Public functions

/**
 * This function implements the signature expected by the mbed TLS entropy
 * module for reading a non-volatile seed.
 * On Silicon Labs devices, this seed is auto-generated from the device-
 * unique data (calibration data, serial number, ...) when no NV seed exists in
 * non-volatile storage yet.
 * When a seed is stored through sli_nv_seed_write it will be returned again
 * by this function.
 *
 * The mbed TLS entropy module will call the seed write function itself each time
 * the entropy module is initialised.
 *
 * This function is exposed to mbed TLS through setting the macro
 * MBEDTLS_PLATFORM_STD_NV_SEED_READ to the function name (sli_nv_seed_read),
 * in addition to MBEDTLS_PLATFORM_NV_SEED_ALT.
 */
int sli_nv_seed_read(unsigned char *buf, size_t buf_len)
{
  Ecode_t nvm3_status;
  uint32_t obj_type;
  size_t obj_len;
  int ret;

  ret = sli_nv_seed_init();
  if ( ret != 0 ) {
    return ret;
  }

  nvm3_status = nvm3_getObjectInfo(nvm3_defaultHandle, SLI_NV_SEED_NVM3_ID,
                                   &obj_type, &obj_len);
  if ( nvm3_status == ECODE_NVM3_OK ) {
    /* Fail safe when the NV seed is not large enough to satisfy the
     * polling function from the entropy module. */
    if ( buf_len > obj_len ) {
      return PSA_ERROR_DATA_INVALID;
    }

    /* Read the requested amount of data from the seed */
    nvm3_status = nvm3_readPartialData(nvm3_defaultHandle, SLI_NV_SEED_NVM3_ID,
                                       buf, 0, buf_len);
    if ( nvm3_status != ECODE_NVM3_OK ) {
      return PSA_ERROR_DATA_INVALID;
    }

    return buf_len;
  } else if ( nvm3_status == ECODE_NVM3_ERR_KEY_NOT_FOUND ) {
    /* Generate a device-unique seed on first run */
    return sli_nv_seed_generate(buf, buf_len);
  } else {
    return PSA_ERROR_DATA_INVALID;
  }
}

/**
 * This function implements the signature expected by the mbed TLS entropy
 * module for writing a seed value to non-volatile memory.
 * When the storage backend fails, it will return an error code.
 *
 * This function is exposed to mbed TLS through setting the macro
 * MBEDTLS_PLATFORM_STD_NV_SEED_WRITE to the function name (sli_nv_seed_write),
 * in addition to MBEDTLS_PLATFORM_NV_SEED_ALT.
 */
int sli_nv_seed_write(unsigned char *buf, size_t buf_len)
{
  Ecode_t nvm3_status;
  int ret;
  ret = sli_nv_seed_init();
  if ( ret != 0 ) {
    return ret;
  }

  nvm3_status = nvm3_writeData(nvm3_defaultHandle, SLI_NV_SEED_NVM3_ID, buf, buf_len);
  if ( nvm3_status == ECODE_NVM3_OK ) {
    return buf_len;
  } else {
    return PSA_ERROR_DATA_INVALID;
  }
}

#endif // MBEDTLS_PLATFORM_NV_SEED_ALT
