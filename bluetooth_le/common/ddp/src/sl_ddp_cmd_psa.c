/***************************************************************************//**
 * @file
 * @brief DDP commands for PSA Crypto
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

#include <string.h>
#include "sl_common.h"
#if defined(SL_COMPONENT_CATALOG_PRESENT)
#include "sl_component_catalog.h"
#endif // SL_COMPONENT_CATALOG_PRESENT
#include "psa/crypto.h"
#include "psa/crypto_types.h"
#include "psa_crypto_its.h"
#include "sl_psa_values.h"
#include "sl_ddp_types.h"
#include "sl_ddp_cmd_psa_init.h"

// -----------------------------------------------------------------------------
// Definitions

#define PSA_KEY_LIFETIME                          \
  PSA_KEY_LIFETIME_FROM_PERSISTENCE_AND_LOCATION( \
    PSA_KEY_PERSISTENCE_DEFAULT,                  \
    DDP_PSA_KEY_LOCATION)

// Logging
#define LOG_PREFIX                    "[DDP_PSA] "
#if defined(SL_CATALOG_APP_LOG_PRESENT)
#include "app_log.h"
#define LOG_NL                        APP_LOG_NL
#define LOG_INFO(...)                 app_log_info(LOG_PREFIX __VA_ARGS__)
#define LOG_ERROR(...)                app_log_error(LOG_PREFIX __VA_ARGS__)
#else // SL_CATALOG_APP_LOG_PRESENT
#define LOG_NL
#define LOG_INFO(...)
#define LOG_ERROR(...)
#endif // SL_CATALOG_APP_LOG_PRESENT

// Input structure of DDP PSA ITS set command
SL_PACK_START(1)
typedef struct {
  uint64_t psa_storage_uid; // PSA persistent storage ID as psa_storage_uid_t
  uint16_t data_len; // Amount of data to write in bytes
  uint8_t data[]; // Data to save
} SL_ATTRIBUTE_PACKED sl_ddp_cmd_psa_its_set_t;
SL_PACK_END()

// Input structure of DDP PSA ITS get command
SL_PACK_START(1)
typedef struct {
  uint64_t psa_storage_uid; // PSA persistent storage ID as psa_storage_uid_t
} SL_ATTRIBUTE_PACKED sl_ddp_cmd_psa_its_get_t;
SL_PACK_END()

// Input structure of DDP command for generating a PSA Crypto key
SL_PACK_START(1)
typedef struct {
  uint32_t usage_flags; // Permitted usage of the key as psa_key_usage_t
  uint32_t bits; // Length of key in bits
  uint32_t algo; // Permitted algorithms of the key as psa_algorithm_t
  uint16_t type; // Type of the key as psa_key_type_t
  uint32_t key_id; // PSA Key ID as psa_key_id_t
} SL_ATTRIBUTE_PACKED sl_ddp_cmd_psa_key_gen_req_t;
SL_PACK_END()

// Output structure of DDP command for generating a PSA Crypto key
SL_PACK_START(1)
typedef struct {
  // Length of the generated public key in bytes. This field may be
  // zero if no corresponding public key has been generated.
  uint32_t key_len;
  uint8_t key[]; // Generated public key
} SL_ATTRIBUTE_PACKED sl_ddp_cmd_psa_key_gen_rsp_t;
SL_PACK_END()

// Input structure of DDP command for injecting a PSA Crypto key
SL_PACK_START(1)
typedef struct {
  uint32_t usage_flags; // Permitted usage of the key as psa_key_usage_t
  uint32_t bits; // Length of key in bits
  uint32_t algo; // Permitted algorithms of the key as psa_algorithm_t
  uint16_t type; // Type of the key as psa_key_type_t
  uint32_t key_id; // PSA Key ID as psa_key_id_t
  uint32_t key_len; // Length of the injected key in bytes
  uint8_t key[]; // Injected key
} SL_ATTRIBUTE_PACKED sl_ddp_cmd_psa_key_inj_t;
SL_PACK_END()

// Input structure of DDP command PSA key get attribute
SL_PACK_START(1)
typedef struct {
  uint32_t key_id; // PSA key ID as psa_key_id_t
} SL_ATTRIBUTE_PACKED sl_ddp_cmd_psa_key_get_att_req_t;
SL_PACK_END()

// Output structure of DDP command PSA key get attribute
SL_PACK_START(1)
typedef struct {
  uint32_t usage_flags; // Permitted usage of the key as psa_key_usage_t
  uint32_t bits; // Length of key in bits
  uint32_t algo; // Permitted algorithms of the key as psa_algorithm_t
  uint16_t type; // Type of the key as psa_key_type_t
  uint32_t key_id; // PSA Key ID as psa_key_id_t
} SL_ATTRIBUTE_PACKED sl_ddp_cmd_psa_key_get_att_rsp_t;
SL_PACK_END()

// -----------------------------------------------------------------------------
// Public functions

/******************************************************************************
 * DDP Command for PSA ITS set.
 *****************************************************************************/
int sl_ddp_cmd_psa_its_set(const uint8_t *input,
                           uint16_t input_len,
                           uint8_t *output,
                           uint16_t output_size,
                           uint16_t *output_len)
{
  (void)output;
  (void)output_size;
  (void)output_len;

  if (!sl_ddp_cmd_psa_is_initialized()) {
    LOG_ERROR("[%s] not initialized!" LOG_NL, __func__);
    return SL_DDP_ERROR_NOT_INITIALIZED;
  }

  if ((input == NULL) || (input_len < sizeof(sl_ddp_cmd_psa_its_set_t))) {
    return SL_DDP_ERROR_ARG;
  }

  const sl_ddp_cmd_psa_its_set_t *req = (const sl_ddp_cmd_psa_its_set_t *)input;
  psa_status_t status;
  status = psa_its_set((psa_storage_uid_t)(req->psa_storage_uid),
                       req->data_len,
                       req->data,
                       PSA_STORAGE_FLAG_NONE);
  if (status != PSA_SUCCESS) {
    LOG_ERROR("[psa_its_set] %ld" LOG_NL, status);
  }

  LOG_INFO("[%s] %ld" LOG_NL, __func__, status);
  return (int)status;
}

/******************************************************************************
 * DDP Command for PSA ITS get.
 *****************************************************************************/
int sl_ddp_cmd_psa_its_get(const uint8_t *input,
                           uint16_t input_len,
                           uint8_t *output,
                           uint16_t output_size,
                           uint16_t *output_len)
{
  if ((input == NULL)
      || (input_len < sizeof(sl_ddp_cmd_psa_its_get_t))
      || (output == NULL)
      || (output_len == NULL)) {
    return SL_DDP_ERROR_ARG;
  }

  if (!sl_ddp_cmd_psa_is_initialized()) {
    LOG_ERROR("[%s] not initialized!" LOG_NL, __func__);
    return SL_DDP_ERROR_NOT_INITIALIZED;
  }

  const sl_ddp_cmd_psa_its_get_t *req = (const sl_ddp_cmd_psa_its_get_t *)input;
  psa_status_t status;
  size_t len;
  status = psa_its_get((psa_storage_uid_t)(req->psa_storage_uid),
                       0,
                       output_size,
                       output,
                       &len);
  if (status != PSA_SUCCESS) {
    LOG_ERROR("[psa_its_get] %ld" LOG_NL, status);
    return (int)status;
  }

  *output_len = (uint16_t)len;
  LOG_INFO("[%s] %ld" LOG_NL, __func__, status);
  return (int)status;
}

/******************************************************************************
 * Generate a PSA Crypto key.
 *****************************************************************************/
int sl_ddp_cmd_psa_key_gen(const uint8_t *input,
                           uint16_t input_len,
                           uint8_t *output,
                           uint16_t output_size,
                           uint16_t *output_len)
{
  if ((input == NULL)
      || (input_len < sizeof(sl_ddp_cmd_psa_key_gen_req_t))
      || (output == NULL)
      || (output_len == NULL)) {
    return SL_DDP_ERROR_ARG;
  }

  if (!sl_ddp_cmd_psa_is_initialized()) {
    LOG_ERROR("[%s] not initialized!" LOG_NL, __func__);
    return SL_DDP_ERROR_NOT_INITIALIZED;
  }

  psa_status_t status;
  psa_key_id_t gen_key_id;
  psa_key_attributes_t key_attr;
  const sl_ddp_cmd_psa_key_gen_req_t *req = (const sl_ddp_cmd_psa_key_gen_req_t *)input;
  sl_ddp_cmd_psa_key_gen_rsp_t *rsp = (sl_ddp_cmd_psa_key_gen_rsp_t *)output;
  rsp->key_len = 0;

  // Destroy key if exists
  key_attr = psa_key_attributes_init();
  status = psa_get_key_attributes((psa_key_id_t)(req->key_id), &key_attr);

  if (status == PSA_SUCCESS) {
    status = psa_destroy_key((psa_key_id_t)(req->key_id));

    if (status != PSA_SUCCESS) {
      LOG_ERROR("[psa_destroy_key] %ld" LOG_NL, status);
      return (int)status;
    }
  }

  key_attr = psa_key_attributes_init();
  psa_set_key_lifetime(&key_attr, PSA_KEY_LIFETIME);
  psa_set_key_usage_flags(&key_attr, (psa_key_usage_t)(req->usage_flags));
  psa_set_key_bits(&key_attr, (size_t)(req->bits));
  psa_set_key_algorithm(&key_attr, (psa_algorithm_t)(req->algo));
  psa_set_key_type(&key_attr, (psa_key_type_t)(req->type));
  psa_set_key_id(&key_attr, (psa_key_id_t)(req->key_id));
  status = psa_generate_key(&key_attr, &gen_key_id);

  if (status != PSA_SUCCESS) {
    LOG_ERROR("[psa_generate_key] %ld" LOG_NL, status);
    return (int)status;
  }
  if ((psa_key_id_t)(req->key_id) != (psa_key_id_t)(gen_key_id)) {
    LOG_ERROR("[psa_generate_key] invalid length!" LOG_NL);
    return SL_DDP_ERROR_LENGTH;
  }

  size_t key_len;

  if (((psa_key_usage_t)(req->usage_flags) & PSA_KEY_USAGE_EXPORT) != 0) {
    // Export usage flag was requested.
    // Export the whole key and send back in response.
    status = psa_export_key((psa_key_id_t)(req->key_id),
                            rsp->key,
                            output_size,
                            &key_len);
    if (status != PSA_SUCCESS) {
      LOG_ERROR("[psa_export_key] %ld" LOG_NL, status);
      return (int)status;
    }
  } else {
    // The key is non-exportable.
    // Only export the public part, and send it back in response.
    status = psa_export_public_key((psa_key_id_t)(req->key_id),
                                   rsp->key,
                                   output_size,
                                   &key_len);
    if (status != PSA_SUCCESS) {
      LOG_ERROR("[psa_export_public_key] %ld" LOG_NL, status);
      return (int)status;
    }
  }

  rsp->key_len = (uint32_t)key_len;
  *output_len = sizeof(*rsp) + rsp->key_len;
  LOG_INFO("[%s] %ld" LOG_NL, __func__, status);
  return (int)status;
}

/******************************************************************************
 * Inject a PSA Crypto key.
 *****************************************************************************/
int sl_ddp_cmd_psa_key_inj(const uint8_t *input,
                           uint16_t input_len,
                           uint8_t *output,
                           uint16_t output_size,
                           uint16_t *output_len)
{
  (void)output;
  (void)output_len;
  (void)output_size;

  if ((input == NULL) || (input_len < sizeof(sl_ddp_cmd_psa_key_inj_t))) {
    return SL_DDP_ERROR_ARG;
  }

  if (!sl_ddp_cmd_psa_is_initialized()) {
    LOG_ERROR("[%s] not initialized!" LOG_NL, __func__);
    return SL_DDP_ERROR_NOT_INITIALIZED;
  }

  psa_status_t status;
  psa_key_id_t gen_key_id;
  psa_key_attributes_t key_attr;
  const sl_ddp_cmd_psa_key_inj_t *req = (const sl_ddp_cmd_psa_key_inj_t *)input;

  // Destroy key if exists
  key_attr = psa_key_attributes_init();
  status = psa_get_key_attributes((psa_key_id_t)(req->key_id), &key_attr);

  if (status == PSA_SUCCESS) {
    status = psa_destroy_key((psa_key_id_t)(req->key_id));

    if (status != PSA_SUCCESS) {
      LOG_ERROR("[psa_destroy_key] %ld" LOG_NL, status);
      return (int)status;
    }
  }

  key_attr = psa_key_attributes_init();
  psa_set_key_lifetime(&key_attr, PSA_KEY_LIFETIME);
  psa_set_key_usage_flags(&key_attr, (psa_key_usage_t)(req->usage_flags));
  psa_set_key_bits(&key_attr, (size_t)(req->bits));
  psa_set_key_algorithm(&key_attr, (psa_algorithm_t)(req->algo));
  psa_set_key_type(&key_attr, (psa_key_type_t)(req->type));
  psa_set_key_id(&key_attr, (psa_key_id_t)(req->key_id));
  status = psa_import_key(&key_attr, req->key, req->key_len, &gen_key_id);

  if (status != PSA_SUCCESS) {
    LOG_ERROR("[psa_import_key] %ld" LOG_NL, status);
    return (int)status;
  }
  if ((psa_key_id_t)(req->key_id) != (psa_key_id_t)(gen_key_id)) {
    LOG_ERROR("[psa_import_key] invalid length!" LOG_NL);
    return SL_DDP_ERROR_LENGTH;
  }

  LOG_INFO("[%s] %ld" LOG_NL, __func__, status);
  return (int)status;
}

/******************************************************************************
 * DDP Command for getting attributes of a PSA key.
 *****************************************************************************/
int sl_ddp_cmd_psa_key_get_att(const uint8_t *input,
                               uint16_t input_len,
                               uint8_t *output,
                               uint16_t output_size,
                               uint16_t *output_len)
{
  if ((input == NULL)
      || (input_len < sizeof(sl_ddp_cmd_psa_key_get_att_req_t))
      || (output == NULL)
      || (output_len == NULL)) {
    return SL_DDP_ERROR_ARG;
  }

  if (!sl_ddp_cmd_psa_is_initialized()) {
    LOG_ERROR("[%s] not initialized!" LOG_NL, __func__);
    return SL_DDP_ERROR_NOT_INITIALIZED;
  }

  psa_status_t status = PSA_SUCCESS;
  const sl_ddp_cmd_psa_key_get_att_req_t *req = (const sl_ddp_cmd_psa_key_get_att_req_t *)input;
  sl_ddp_cmd_psa_key_get_att_rsp_t *rsp = (sl_ddp_cmd_psa_key_get_att_rsp_t *)output;

  if (sizeof(*rsp) > output_size) {
    return SL_DDP_ERROR_LENGTH;
  }

  // Get key attributes
  psa_key_attributes_t key_attr;
  key_attr = psa_key_attributes_init();
  status = psa_get_key_attributes(req->key_id, &key_attr);
  if (status != PSA_SUCCESS) {
    LOG_ERROR("[psa_get_key_attributes] %ld" LOG_NL, status);
    return (int)status;
  }

  rsp->usage_flags =  (uint32_t)psa_get_key_usage_flags(&key_attr);
  rsp->bits =         (uint32_t)psa_get_key_bits(&key_attr);
  rsp->algo =         (uint32_t)psa_get_key_algorithm(&key_attr);
  rsp->type =         (uint16_t)psa_get_key_type(&key_attr);
  rsp->key_id =       (uint32_t)psa_get_key_id(&key_attr);
  *output_len =       sizeof(*rsp);

  LOG_INFO("[%s] %ld" LOG_NL, __func__, status);
  return (int)status;
}
