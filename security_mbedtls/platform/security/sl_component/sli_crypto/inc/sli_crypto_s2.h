/***************************************************************************//**
 * @file
 * @brief Hardware accelerated cryptographic defintions specific to series-2
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
#ifndef SLI_CRYPTO_S2_H
#define SLI_CRYPTO_S2_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/// Defines the physical dimensions of the countermeasure seeds
#define CRYPTO_CM_MASKBITS 32

/// Standard buffer size in bytes
#define SLI_CRYPTO_AES_BLOCK_SIZE  16
/// Location value for keys stored in plaintext
#define SLI_CRYPTO_KEY_LOCATION_PLAINTEXT ((sli_crypto_key_location_t)0x00000000UL)
/// The SLI Crypto API supports only the RADIOAES crypto engine on Series-2
#define SLI_CRYPTO_ENGINE_RADIOAES       ((sli_crypto_engine_t)0x00000001UL)
#define SLI_CRYPTO_ENGINE_DEFAULT        (SLI_CRYPTO_ENGINE_RADIOAES)
/// Crypto engine selection value for CRYPTOACC (needs manual reseed)
#define SLI_CRYPTO_ENGINE_CRYPTOACC      ((sli_crypto_engine_t)0x00000000UL)

/// Used to choose a crypto engine.
/// @ref SLI_CRYPTO_LPWAES.
typedef uint32_t sli_crypto_engine_t;

typedef enum _sli_engine_idx {
  #if defined(CRYPTOACC_PRESENT)
  SLI_CRYPTO_CRYPTOACC_IDX = 0,
  #endif
  SLI_CRYPTO_ENGINE_COUNT,
  SLI_CRYPTO_INVALID_ENGINE_IDX = 0xFF
} sli_engine_id_t;

// RadioAES is reseeded on every operation
// only CRYPTOACC needs to be reseeded
#if defined(CRYPTOACC_PRESENT)
#define INVALID_ENGINE(e) ((e) != SLI_CRYPTO_ENGINE_CRYPTOACC)
#else
#define INVALID_ENGINE(e) (true)
#endif

/// Key storage location. Can either
/// @ref SLI_CRYPTO_KEY_LOCATION_PLAINTEXT
typedef uint32_t sli_crypto_key_location_t;

/// Describes where the plaintext key is stored
typedef struct {
  uint8_t* pointer; ///< Pointer to a key buffer.
  uint32_t size;    ///< Size of buffer.
} sli_crypto_key_buffer_t;

/// Describes the plaintext key
typedef struct {
  sli_crypto_key_buffer_t buffer; ///< Key buffer.
  uint32_t key_size;              ///< Key size.
} sli_crypto_plaintext_key_t;

typedef struct {
  /// Key storage location.
  sli_crypto_key_location_t location;
  /// Crypto engine.
  sli_crypto_engine_t engine;
  /// Describes key storage location.
  union {
    sli_crypto_plaintext_key_t plaintext_key;
  } key;
} sli_crypto_descriptor_t;

/// SLI Crypto descriptor reset values. Some of the values are not initialized.
/// The user will need to initialize the descriptor members for further use in
/// the SLI Crypto API. The purpose of these initialisation values is to set
/// the descriptor objects to a known default device specific state initially
/// when the context object is declared.
#define SLI_CRYPTO_DESCRIPTOR_INIT SLI_CRYPTO_DESCRIPTOR_INIT_PLAINTEXT_KEY(NULL, 0)

#define SLI_CRYPTO_DESCRIPTOR_INIT_PLAINTEXT_KEY(ptr, sz) \
  {                                                       \
    .location = SLI_CRYPTO_KEY_LOCATION_PLAINTEXT,        \
    .engine = SLI_CRYPTO_ENGINE_DEFAULT,                  \
    .key = {                                              \
      .plaintext_key = {                                  \
        .buffer = {                                       \
          .pointer = ptr,                                 \
          .size = sz,                                     \
        },                                                \
        .key_size = sz,                                   \
      },                                                  \
    },                                                    \
  }

#ifdef __cplusplus
}
#endif

#endif // SLI_CRYPTO_S2_H
