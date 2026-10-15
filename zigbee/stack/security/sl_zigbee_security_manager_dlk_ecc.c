/***************************************************************************//**
 * @file sl_zigbee_security_manager_dlk_ecc.c
 * @brief stateless computation of dynamic link key elliptic curve cryptography
 *******************************************************************************
 * # License
 * <b>Copyright 2023 Silicon Laboratories Inc. www.silabs.com</b>
 *******************************************************************************
 *
 * The licensor of this software is Silicon Laboratories Inc. Your use of this
 * software is governed by the terms of Silicon Labs Master Software License
 * Agreement (MSLA) available at
 * www.silabs.com/about-us/legal/master-software-license-agreement. This
 * software is distributed to you in Source Code format and is governed by the
 * sections of the MSLA applicable to Source Code.
 *
 ******************************************************************************/

#include "sl_zigbee_security_manager_dlk_ecc.h"
#include "sli_zigbee_security_manager_dlk_ecc.h"

#if defined(SL_COMPONENT_CATALOG_PRESENT)
#include "sl_component_catalog.h"
#endif

#ifdef SL_CATALOG_ZIGBEE_SECURITY_MANAGER_DLK_ECC_TEST_VECTORS_PRESENT
#include "stack/include/sl_zigbee_security_manager_dlk_ecc_test_vectors.h"
#endif

// NOTE for sl_status_t
#include "sl_zigbee_types.h"
// NOTE for sha-256 primitives
#include "mbedtls/build_info.h"
#if defined(MBEDTLS_VERSION_MAJOR) && (MBEDTLS_VERSION_MAJOR >= 4)
#ifndef MBEDTLS_DECLARE_PRIVATE_IDENTIFIERS
#define MBEDTLS_DECLARE_PRIVATE_IDENTIFIERS
#endif
#include "mbedtls/private/sha256.h"
#else
#include "mbedtls/sha256.h"
#endif
// NOTE for sl_util_reverse_mem_copy
#include "byte-utilities.h"
// NOTE for sl_zigbee_get_strong_random_number...
#include "sl_zigbee_random_api.h"
#include "stack/internal/inc/sl_zigbee_random_api_internal_def.h"
// NOTE for sli_zigbee_stack_aes_hash_simple
#include "stack/include/aes-mmo.h"

#include "stack/include/zigbee-security-manager.h"
#include "stack/internal/inc/internal-defs-patch.h"
#include <string.h>

// PSA uncompressed public key: 0x04 || X || Y
#define DLK_ECC_PSA_P256_PUBLIC_KEY_SIZE (1 + DLK_ECC_P256_PUBLIC_KEY_SIZE)

// Native sim: PSA ECC keygen is unreliable (no TRNG / heavy stack). 
// Use residual mbedtls ECP for ECDHE there; keep PSA on device builds.
#if defined(SL_CATALOG_ZIGBEE_SIMULATION_PRESENT)
#define SLI_ZB_DLK_ECDHE_USE_MBEDTLS 1
#endif

static sl_status_t sli_zigbee_dlk_ecc_crypto_state_ensure_allocated(sl_zigbee_sec_man_dlk_ecc_context_t *dlk_ecc_ctx)
{
  if (dlk_ecc_ctx->crypto_state != NULL) {
    return SL_STATUS_OK;
  }
  dlk_ecc_ctx->crypto_state = calloc(1, sizeof(sli_zigbee_dlk_ecc_crypto_state_t));
  if (dlk_ecc_ctx->crypto_state == NULL) {
    return SL_STATUS_ALLOCATION_FAILED;
  }
  return SL_STATUS_OK;
}

static void sli_zigbee_dlk_ecc_crypto_state_init_mbedtls(sl_zigbee_sec_man_dlk_ecc_context_t *dlk_ecc_ctx)
{
  sli_zigbee_dlk_ecc_crypto_state_t *cs = sli_zigbee_dlk_ecc_get_crypto_state(dlk_ecc_ctx);
  mbedtls_ecp_group_init(&cs->ecc_group);
  mbedtls_mpi_init(&cs->d);
  mbedtls_ecp_point_init(&cs->Q);
  mbedtls_ecp_point_init(&cs->Qp);
  mbedtls_mpi_init(&cs->x_k);
}

#if !defined(SLI_ZB_DLK_ECDHE_USE_MBEDTLS)
// PSA ECDHE helpers (device builds only; unused when sim uses residual mbedtls)
static void sli_zigbee_dlk_ecc_crypto_state_clear_psa(sl_zigbee_sec_man_dlk_ecc_context_t *dlk_ecc_ctx)
{
  sli_zigbee_dlk_ecc_crypto_state_t *cs = sli_zigbee_dlk_ecc_get_crypto_state(dlk_ecc_ctx);
  cs->psa_private_key_id = 0;
  cs->psa_key_valid = false;
  memset(cs->our_public_key, 0, sizeof(cs->our_public_key));
  memset(cs->peer_public_key, 0, sizeof(cs->peer_public_key));
  memset(cs->shared_x_le, 0, sizeof(cs->shared_x_le));
}

static void sli_zb_sec_man_ecc_reverse_bytes(uint8_t *dst, const uint8_t *src, size_t len)
{
  for (size_t i = 0; i < len; i++) {
    dst[i] = src[len - 1U - i];
  }
}
#endif // !SLI_ZB_DLK_ECDHE_USE_MBEDTLS

/// operation specific
// NOTE the below procedures are specific to the underlying key agreement scheme
// [ECDHE-PSK] Elliptic Curve Diffie-Hellman Ephemeral (with PSK salting)

static sl_status_t ecdhe_init(sl_zigbee_sec_man_dlk_ecc_context_t *dlk_ecc_ctx);

static sl_status_t ecdhe_generate_keypair(sl_zigbee_sec_man_dlk_ecc_context_t *dlk_ecc_ctx);

static sl_status_t ecdhe_expand_shared_secret(sl_zigbee_sec_man_dlk_ecc_context_t *dlk_ecc_ctx);

static sl_status_t ecdhe_derive_link_key(sl_zigbee_sec_man_dlk_ecc_context_t *dlk_ecc_ctx);

// [SPEKE] Secure Passphrase Ephemeral Key Exchange

static sl_status_t speke_init(sl_zigbee_sec_man_dlk_ecc_context_t *dlk_ecc_ctx);

static sl_status_t speke_generate_keypair(sl_zigbee_sec_man_dlk_ecc_context_t *dlk_ecc_ctx);

sl_status_t sli_zigbee_stack_sec_man_speke_expand_shared_secret(sl_zigbee_sec_man_dlk_ecc_context_t *dlk_ecc_ctx,
                                                                const uint8_t *our_eui,
                                                                const uint8_t *their_eui);

static sl_status_t speke_derive_link_key(sl_zigbee_sec_man_dlk_ecc_context_t *dlk_ecc_ctx);

#define HMAC_SHA_256_OUTPUT_SIZE 32
/**
 * @brief performs a keyed hmac using SHA-256 as the underlying hash function
 * @param key a pointer to the key data
 * @param key_len the length of 'key' in bytes
 * @param data a pointer to the input data
 * @param data_len the length of 'data' in bytes
 * @param result[OUT] a pointer argument where the resulting digest will be written
 * @note result must contain enough memory for a 32-byte output digest
 */
void sl_zb_sec_man_hmac_sha_256(
  uint8_t *key,
  uint8_t key_len,
  uint8_t *data,
  uint8_t data_len,
  uint8_t *result);

// NOTE here's a wrapper function for providing rng to crypto calls
static int f_rng_wrapper(void *prng, unsigned char *out, size_t num)
{
  (void)prng;
  if (num % 2) {
    uint16_t temp;
    sli_zigbee_stack_get_strong_random_number_array(&temp, 1);
    *out = LOW_BYTE(temp);
    out++;
    num--;
  }
  int count = num / 2;
  uint16_t *shortPtr = (uint16_t *)out;
  return sli_zigbee_stack_get_strong_random_number_array(shortPtr, count);
}

static sl_zb_dlk_ecc_config_t dlk_ecc_valid_configurations[] =
{
  { DLK_ECC_OPERATION_ECDHE_PSK, DLK_ECC_CURVE_P256, DLK_ECC_HASH_SHA_256 },
  { DLK_ECC_OPERATION_SPEKE, DLK_ECC_CURVE_25519, DLK_ECC_HASH_SHA_256 },
  { DLK_ECC_OPERATION_SPEKE, DLK_ECC_CURVE_25519, DLK_ECC_HASH_AES_MMO_128 },
  // NOTE sentinel value
  { DLK_ECC_OPERATION_INVALID, DLK_ECC_CURVE_INVALID, DLK_ECC_HASH_INVALID }
};

bool sli_zigbee_is_supported_ecc_operation(sl_zb_dlk_ecc_config_t *ecc_config)
{
  sl_zb_dlk_ecc_config_t *config_cursor = dlk_ecc_valid_configurations;
  while (config_cursor != NULL) {
    if (config_cursor->operation_id == ecc_config->operation_id
        && config_cursor->curve_id == ecc_config->curve_id
        && config_cursor->hash_id == ecc_config->hash_id) {
      break;
    } else if (config_cursor->operation_id == DLK_ECC_OPERATION_INVALID
               && config_cursor->curve_id == DLK_ECC_CURVE_INVALID
               && config_cursor->hash_id == DLK_ECC_HASH_INVALID) {
      // NOTE sentinel value
      config_cursor = NULL;
    } else {
      config_cursor++;
    }
  }
  return config_cursor != NULL;
}

// generic ecc operations
sl_status_t sli_zigbee_stack_sec_man_ecc_init(sl_zigbee_sec_man_dlk_ecc_context_t *dlk_ecc_ctx,
                                              sl_zb_dlk_ecc_config_t *ecc_config,
                                              const uint8_t *psk)
{
  if (dlk_ecc_ctx == NULL || psk == NULL) {
    return SL_STATUS_NULL_POINTER;
  }

  if (!sli_zigbee_is_supported_ecc_operation(ecc_config)) {
    return SL_STATUS_NOT_AVAILABLE;
  }

  sl_status_t alloc_status = sli_zigbee_dlk_ecc_crypto_state_ensure_allocated(dlk_ecc_ctx);
  if (alloc_status != SL_STATUS_OK) {
    return alloc_status;
  }

  // NOTE: assume if ecc_config points to internal struct, it has already been set
  if (&dlk_ecc_ctx->config != ecc_config) {
    memmove(&dlk_ecc_ctx->config, ecc_config, sizeof(sl_zb_dlk_ecc_config_t));
  }
  // set the psk
  memmove(dlk_ecc_ctx->psk, psk, DLK_KEY_SIZE);
  // perform additional steps per key negotiation scheme
  switch (dlk_ecc_ctx->config.operation_id) {
    case DLK_ECC_OPERATION_ECDHE_PSK:
#if defined(SLI_ZB_DLK_ECDHE_USE_MBEDTLS)
      sli_zigbee_dlk_ecc_crypto_state_init_mbedtls(dlk_ecc_ctx);
#else
      sli_zigbee_dlk_ecc_crypto_state_clear_psa(dlk_ecc_ctx);
#endif
      return ecdhe_init(dlk_ecc_ctx);
    case DLK_ECC_OPERATION_SPEKE:
      sli_zigbee_dlk_ecc_crypto_state_init_mbedtls(dlk_ecc_ctx);
      return speke_init(dlk_ecc_ctx);
    default:
      // UNREACHABLE
      return SL_STATUS_NOT_SUPPORTED;
  }
}

void sli_zigbee_stack_sec_man_ecc_free(sl_zigbee_sec_man_dlk_ecc_context_t *dlk_ecc_ctx)
{
  if (dlk_ecc_ctx == NULL) {
    return;
  }
  sli_zigbee_dlk_ecc_crypto_state_t *cs = sli_zigbee_dlk_ecc_get_crypto_state(dlk_ecc_ctx);
  if (cs != NULL) {
    if (dlk_ecc_ctx->config.operation_id == DLK_ECC_OPERATION_ECDHE_PSK) {
#if defined(SLI_ZB_DLK_ECDHE_USE_MBEDTLS)
      mbedtls_mpi_free(&cs->d);
      mbedtls_ecp_group_free(&cs->ecc_group);
      mbedtls_ecp_point_free(&cs->Q);
      mbedtls_ecp_point_free(&cs->Qp);
      mbedtls_mpi_free(&cs->x_k);
#else
      if (cs->psa_key_valid) {
        (void)psa_destroy_key(cs->psa_private_key_id);
        cs->psa_key_valid = false;
        cs->psa_private_key_id = 0;
      }
#endif
    } else if (dlk_ecc_ctx->config.operation_id == DLK_ECC_OPERATION_SPEKE) {
      mbedtls_mpi_free(&cs->d);
      mbedtls_ecp_group_free(&cs->ecc_group);
      mbedtls_ecp_point_free(&cs->Q);
      mbedtls_ecp_point_free(&cs->Qp);
      mbedtls_mpi_free(&cs->x_k);
    }
    free(cs);
    dlk_ecc_ctx->crypto_state = NULL;
  }
  if (dlk_ecc_ctx->test != NULL) {
    free(dlk_ecc_ctx->test);
    dlk_ecc_ctx->test = NULL;
  }
  // clear out struct
  memset(dlk_ecc_ctx, 0, sizeof(sl_zigbee_sec_man_dlk_ecc_context_t));
}

sl_status_t sli_zigbee_stack_sec_man_ecc_generate_keypair(sl_zigbee_sec_man_dlk_ecc_context_t *dlk_ecc_ctx,
                                                          uint8_t *public_key_buff,
                                                          size_t *key_len_out)
{
  if (dlk_ecc_ctx == NULL || public_key_buff == NULL || key_len_out == NULL) {
    return SL_STATUS_NULL_POINTER;
  }
  sl_status_t status;
  switch (dlk_ecc_ctx->config.operation_id) {
    case DLK_ECC_OPERATION_ECDHE_PSK:
      status = ecdhe_generate_keypair(dlk_ecc_ctx);
      break;
    case DLK_ECC_OPERATION_SPEKE:
      status = speke_generate_keypair(dlk_ecc_ctx);
      break;
    default:
      // UNREACHABLE
      return SL_STATUS_NOT_SUPPORTED;
  }
  if (status != SL_STATUS_OK) {
    return status;
  }
  return sli_zb_sec_man_ecc_export_public_key(dlk_ecc_ctx, false, public_key_buff, key_len_out);
}

sl_status_t sli_zigbee_stack_sec_man_ecc_extract_shared_secret(sl_zigbee_sec_man_dlk_ecc_context_t *dlk_ecc_ctx,
                                                               const uint8_t *peer_public_key,
                                                               size_t peer_key_len)
{
  if (dlk_ecc_ctx == NULL || peer_public_key == NULL) {
    return SL_STATUS_NULL_POINTER;
  }
  // HOWEVER the format of the public key might be different depending on the curve...
  // NOTE right now the protocols are such that we do not need to validate the expected len by curve
  sl_status_t status = sli_zb_sec_man_ecc_import_peer_public_key(dlk_ecc_ctx, peer_public_key, peer_key_len);
  if (status != SL_STATUS_OK) {
    return status;
  }

  if (dlk_ecc_ctx->config.operation_id == DLK_ECC_OPERATION_ECDHE_PSK) {
    sli_zigbee_dlk_ecc_crypto_state_t *cs = sli_zigbee_dlk_ecc_get_crypto_state(dlk_ecc_ctx);
#if defined(SLI_ZB_DLK_ECDHE_USE_MBEDTLS)
    mbedtls_ecp_point P;
    mbedtls_ecp_point_init(&P);
    int crypto_ret = mbedtls_ecp_mul(&cs->ecc_group,
                                     &P,
                                     &cs->d,
                                     &cs->Qp,
                                     f_rng_wrapper,
                                     NULL);
    if (crypto_ret == 0) {
      crypto_ret = mbedtls_mpi_write_binary_le(&P.MBEDTLS_PRIVATE(X),
                                               cs->shared_x_le,
                                               DLK_ECC_COORDINATE_SIZE);
    }
    mbedtls_ecp_point_free(&P);
    return sli_zb_sec_man_ecc_map_crypto_status(crypto_ret);
#else
    uint8_t peer_psa[DLK_ECC_PSA_P256_PUBLIC_KEY_SIZE];
    uint8_t shared_x_be[DLK_ECC_COORDINATE_SIZE];
    size_t shared_len = 0;

    if (!cs->psa_key_valid) {
      return SL_STATUS_INVALID_STATE;
    }

    peer_psa[0] = 0x04;
    memcpy(peer_psa + 1, cs->peer_public_key, DLK_ECC_P256_PUBLIC_KEY_SIZE);

    psa_status_t psa_status = psa_raw_key_agreement(PSA_ALG_ECDH,
                                                    cs->psa_private_key_id,
                                                    peer_psa,
                                                    sizeof(peer_psa),
                                                    shared_x_be,
                                                    sizeof(shared_x_be),
                                                    &shared_len);
    if (psa_status != PSA_SUCCESS || shared_len != DLK_ECC_COORDINATE_SIZE) {
      return sli_zb_sec_man_ecc_map_psa_status(psa_status != PSA_SUCCESS ? psa_status : PSA_ERROR_GENERIC_ERROR);
    }
    // Zigbee ECDHE expand uses little-endian shared X (matches former mpi_write_binary_le)
    sli_zb_sec_man_ecc_reverse_bytes(cs->shared_x_le, shared_x_be, DLK_ECC_COORDINATE_SIZE);
    return SL_STATUS_OK;
#endif
  }

  {
    sli_zigbee_dlk_ecc_crypto_state_t *cs = sli_zigbee_dlk_ecc_get_crypto_state(dlk_ecc_ctx);
    mbedtls_ecp_point P;
    mbedtls_ecp_point_init(&P);
    int crypto_ret = mbedtls_ecp_mul(&cs->ecc_group,
                                     &P,
                                     &cs->d,
                                     &cs->Qp,
                                     f_rng_wrapper,
                                     NULL);
    if (crypto_ret == 0) {
      crypto_ret = mbedtls_mpi_copy(&cs->x_k, &P.MBEDTLS_PRIVATE(X));
    }
    mbedtls_ecp_point_free(&P);
    return sli_zb_sec_man_ecc_map_crypto_status(crypto_ret);
  }
}

sl_status_t sli_zigbee_stack_sec_man_ecc_expand_shared_secret(
  sl_zigbee_sec_man_dlk_ecc_context_t *dlk_ecc_ctx,
  const uint8_t *our_eui,
  const uint8_t *their_eui)
{
  if (dlk_ecc_ctx == NULL || our_eui == NULL || their_eui == NULL) {
    return SL_STATUS_NULL_POINTER;
  }
  switch (dlk_ecc_ctx->config.operation_id) {
    case DLK_ECC_OPERATION_ECDHE_PSK:
      // NOTE euis are not needed for ecdhe
      return ecdhe_expand_shared_secret(dlk_ecc_ctx);
    case DLK_ECC_OPERATION_SPEKE:
      return sli_zigbee_stack_sec_man_speke_expand_shared_secret(dlk_ecc_ctx, our_eui, their_eui);
    default:
      // UNREACHABLE
      return SL_STATUS_NOT_SUPPORTED;
  }
}

sl_status_t sli_zigbee_stack_sec_man_ecc_derive_link_key(sl_zigbee_sec_man_dlk_ecc_context_t *dlk_ecc_ctx)
{
  if (dlk_ecc_ctx == NULL) {
    return SL_STATUS_NULL_POINTER;
  }
  switch (dlk_ecc_ctx->config.operation_id) {
    case DLK_ECC_OPERATION_SPEKE:
      return speke_derive_link_key(dlk_ecc_ctx);
    case DLK_ECC_OPERATION_ECDHE_PSK:
      return ecdhe_derive_link_key(dlk_ecc_ctx);
    default:
      // UNREACHABLE
      return SL_STATUS_NOT_SUPPORTED;
  }
}

sl_status_t sli_zb_sec_man_ecc_export_public_key(sl_zigbee_sec_man_dlk_ecc_context_t *dlk_ecc_ctx,
                                                 bool is_peer,
                                                 uint8_t *public_key_buff,
                                                 size_t *public_key_len)
{
  if (dlk_ecc_ctx == NULL || public_key_buff == NULL || public_key_len == NULL) {
    return SL_STATUS_NULL_POINTER;
  }

  if (dlk_ecc_ctx->config.operation_id == DLK_ECC_OPERATION_ECDHE_PSK) {
    const sli_zigbee_dlk_ecc_crypto_state_t *cs = sli_zigbee_dlk_ecc_get_crypto_state(dlk_ecc_ctx);
    const uint8_t *src = is_peer ? cs->peer_public_key : cs->our_public_key;
    memcpy(public_key_buff, src, DLK_ECC_P256_PUBLIC_KEY_SIZE);
    *public_key_len = DLK_ECC_P256_PUBLIC_KEY_SIZE;
    return SL_STATUS_OK;
  }

  const mbedtls_ecp_point *Q = is_peer ? &sli_zigbee_dlk_ecc_get_crypto_state(dlk_ecc_ctx)->Qp : &sli_zigbee_dlk_ecc_get_crypto_state(dlk_ecc_ctx)->Q;
  size_t bytes_written;
  int crypto_ret = -1;
  // get the x coordinate
  if (dlk_ecc_ctx->config.curve_id == DLK_ECC_CURVE_P256) {
    // P256 uses big endianess
    crypto_ret = mbedtls_mpi_write_binary(&Q->MBEDTLS_PRIVATE(X), public_key_buff, DLK_ECC_COORDINATE_SIZE);
  } else {
    // CURVE_25519 uses little endianess
    crypto_ret = mbedtls_mpi_write_binary_le(&Q->MBEDTLS_PRIVATE(X), public_key_buff, DLK_ECC_COORDINATE_SIZE);
  }
  if (crypto_ret != 0) {
    return SL_STATUS_OBJECT_WRITE;
  }
  bytes_written = DLK_ECC_COORDINATE_SIZE;
  if (dlk_ecc_ctx->config.curve_id == DLK_ECC_CURVE_P256) {
    // get the y coordinate
    crypto_ret = mbedtls_mpi_write_binary(&Q->MBEDTLS_PRIVATE(Y), public_key_buff + bytes_written, DLK_ECC_COORDINATE_SIZE);
    if (crypto_ret != 0) {
      return SL_STATUS_OBJECT_WRITE;
    }
    bytes_written += DLK_ECC_COORDINATE_SIZE;
  }
  *public_key_len = bytes_written;
  return SL_STATUS_OK;
}

sl_status_t sli_zb_sec_man_ecc_import_peer_public_key(sl_zigbee_sec_man_dlk_ecc_context_t *dlk_ecc_ctx,
                                                      const uint8_t *public_key_buff,
                                                      size_t public_key_len)
{
  (void)public_key_len;
  if (dlk_ecc_ctx == NULL || public_key_buff == NULL) {
    return SL_STATUS_NULL_POINTER;
  }

  if (dlk_ecc_ctx->config.operation_id == DLK_ECC_OPERATION_ECDHE_PSK) {
    sli_zigbee_dlk_ecc_crypto_state_t *cs = sli_zigbee_dlk_ecc_get_crypto_state(dlk_ecc_ctx);
    memcpy(cs->peer_public_key, public_key_buff, DLK_ECC_P256_PUBLIC_KEY_SIZE);
#if defined(SLI_ZB_DLK_ECDHE_USE_MBEDTLS)
    int crypto_ret = mbedtls_mpi_lset(&cs->Qp.MBEDTLS_PRIVATE(Z), 1);
    if (crypto_ret != 0) {
      return SL_STATUS_FAIL;
    }
    crypto_ret = mbedtls_mpi_read_binary(&cs->Qp.MBEDTLS_PRIVATE(X), public_key_buff, DLK_ECC_COORDINATE_SIZE);
    if (crypto_ret != 0) {
      return SL_STATUS_OBJECT_READ;
    }
    crypto_ret = mbedtls_mpi_read_binary(&cs->Qp.MBEDTLS_PRIVATE(Y),
                                         public_key_buff + DLK_ECC_COORDINATE_SIZE,
                                         DLK_ECC_COORDINATE_SIZE);
    if (crypto_ret != 0) {
      return SL_STATUS_OBJECT_READ;
    }
    crypto_ret = mbedtls_ecp_check_pubkey(&cs->ecc_group, &cs->Qp);
    return sli_zb_sec_man_ecc_map_crypto_status(crypto_ret);
#else
    uint8_t peer_psa[DLK_ECC_PSA_P256_PUBLIC_KEY_SIZE];
    psa_key_attributes_t peer_attr = PSA_KEY_ATTRIBUTES_INIT;
    psa_key_id_t peer_key_id = 0;

    peer_psa[0] = 0x04;
    memcpy(peer_psa + 1, cs->peer_public_key, DLK_ECC_P256_PUBLIC_KEY_SIZE);

    psa_set_key_type(&peer_attr, PSA_KEY_TYPE_ECC_PUBLIC_KEY(PSA_ECC_FAMILY_SECP_R1));
    psa_set_key_bits(&peer_attr, 256);
    psa_set_key_usage_flags(&peer_attr, PSA_KEY_USAGE_DERIVE);
    psa_set_key_algorithm(&peer_attr, PSA_ALG_ECDH);

    psa_status_t psa_status = psa_import_key(&peer_attr, peer_psa, sizeof(peer_psa), &peer_key_id);
    psa_reset_key_attributes(&peer_attr);
    if (psa_status != PSA_SUCCESS) {
      return sli_zb_sec_man_ecc_map_psa_status(psa_status);
    }
    (void)psa_destroy_key(peer_key_id);
    return SL_STATUS_OK;
#endif
  }

  int crypto_ret = -1;
  // set the Z coord
  crypto_ret = mbedtls_mpi_lset(&sli_zigbee_dlk_ecc_get_crypto_state(dlk_ecc_ctx)->Qp.MBEDTLS_PRIVATE(Z), 1);
  if (crypto_ret != 0) {
    return SL_STATUS_FAIL;
  }
  // read in the x coordinate
  if (dlk_ecc_ctx->config.curve_id == DLK_ECC_CURVE_P256) {
    // P256 uses big endianess
    crypto_ret = mbedtls_mpi_read_binary(&sli_zigbee_dlk_ecc_get_crypto_state(dlk_ecc_ctx)->Qp.MBEDTLS_PRIVATE(X), public_key_buff, DLK_ECC_COORDINATE_SIZE);
  } else {
    // CURVE_25519 uses little endianess
    crypto_ret = mbedtls_mpi_read_binary_le(&sli_zigbee_dlk_ecc_get_crypto_state(dlk_ecc_ctx)->Qp.MBEDTLS_PRIVATE(X), public_key_buff, DLK_ECC_COORDINATE_SIZE);
  }
  if (crypto_ret != 0) {
    return SL_STATUS_OBJECT_READ;
  }
  if (dlk_ecc_ctx->config.curve_id == DLK_ECC_CURVE_P256) {
    // read in the y coordinate
    crypto_ret = mbedtls_mpi_read_binary(&sli_zigbee_dlk_ecc_get_crypto_state(dlk_ecc_ctx)->Qp.MBEDTLS_PRIVATE(Y), public_key_buff + DLK_ECC_COORDINATE_SIZE, DLK_ECC_COORDINATE_SIZE);
  }
  if (crypto_ret != 0) {
    return SL_STATUS_OBJECT_READ;
  }
  crypto_ret = mbedtls_ecp_check_pubkey(&sli_zigbee_dlk_ecc_get_crypto_state(dlk_ecc_ctx)->ecc_group, &sli_zigbee_dlk_ecc_get_crypto_state(dlk_ecc_ctx)->Qp);
  return sli_zb_sec_man_ecc_map_crypto_status(crypto_ret);
}

sl_status_t sli_zb_sec_man_ecc_export_shared_x(sl_zigbee_sec_man_dlk_ecc_context_t *dlk_ecc_ctx,
                                               uint8_t *shared_x_out,
                                               size_t shared_x_len)
{
  if (dlk_ecc_ctx == NULL || shared_x_out == NULL) {
    return SL_STATUS_NULL_POINTER;
  }
  if (shared_x_len < DLK_ECC_COORDINATE_SIZE) {
    return SL_STATUS_WOULD_OVERFLOW;
  }

  if (dlk_ecc_ctx->config.operation_id == DLK_ECC_OPERATION_ECDHE_PSK) {
    memcpy(shared_x_out,
           sli_zigbee_dlk_ecc_get_crypto_state(dlk_ecc_ctx)->shared_x_le,
           DLK_ECC_COORDINATE_SIZE);
    return SL_STATUS_OK;
  }

  int crypto_ret = mbedtls_mpi_write_binary_le(&sli_zigbee_dlk_ecc_get_crypto_state(dlk_ecc_ctx)->x_k,
                                               shared_x_out,
                                               DLK_ECC_COORDINATE_SIZE);
  return (crypto_ret == 0) ? SL_STATUS_OK : SL_STATUS_OBJECT_WRITE;
}

sl_status_t sli_zb_sec_man_ecc_map_crypto_status(int crypto_ret)
{
  int high;

  if (crypto_ret == 0) {
    return SL_STATUS_OK;
  }

  // mbedtls may return compound (high + low) codes; isolate the high-level part.
  high = crypto_ret & -0x80;

  switch (high) {
    case MBEDTLS_ERR_ECP_INVALID_KEY:
    case MBEDTLS_ERR_ECP_VERIFY_FAILED:
      return SL_STATUS_INVALID_KEY;
    case MBEDTLS_ERR_ECP_BAD_INPUT_DATA:
      return SL_STATUS_INVALID_PARAMETER;
    case MBEDTLS_ERR_ECP_FEATURE_UNAVAILABLE:
      return SL_STATUS_NOT_SUPPORTED;
    case MBEDTLS_ERR_ECP_ALLOC_FAILED:
      return SL_STATUS_ALLOCATION_FAILED;
    case MBEDTLS_ERR_ECP_BUFFER_TOO_SMALL:
      return SL_STATUS_WOULD_OVERFLOW;
    default:
      break;
  }

  // Standalone / low-level MPI codes (and exact matches when not compounded).
  switch (crypto_ret) {
    case MBEDTLS_ERR_MPI_BAD_INPUT_DATA:
      return SL_STATUS_INVALID_PARAMETER;
    case MBEDTLS_ERR_MPI_ALLOC_FAILED:
      return SL_STATUS_ALLOCATION_FAILED;
    case MBEDTLS_ERR_MPI_BUFFER_TOO_SMALL:
      return SL_STATUS_WOULD_OVERFLOW;
    default:
      return SL_STATUS_FAIL;
  }
}

sl_status_t sli_zb_sec_man_ecc_map_psa_status(psa_status_t psa_status)
{
  switch (psa_status) {
    case PSA_SUCCESS:
      return SL_STATUS_OK;
    case PSA_ERROR_INVALID_ARGUMENT:
    case PSA_ERROR_INVALID_PADDING:
      return SL_STATUS_INVALID_PARAMETER;
    case PSA_ERROR_NOT_SUPPORTED:
    case PSA_ERROR_NOT_PERMITTED:
      return SL_STATUS_NOT_SUPPORTED;
    case PSA_ERROR_INSUFFICIENT_MEMORY:
      return SL_STATUS_ALLOCATION_FAILED;
    case PSA_ERROR_BUFFER_TOO_SMALL:
      return SL_STATUS_WOULD_OVERFLOW;
    case PSA_ERROR_INVALID_HANDLE:
    case PSA_ERROR_DOES_NOT_EXIST:
      return SL_STATUS_INVALID_KEY;
    case PSA_ERROR_INVALID_SIGNATURE:
      return SL_STATUS_INVALID_SIGNATURE;
    default:
      return SL_STATUS_FAIL;
  }
}

sl_status_t sli_zigbee_stack_sec_man_ecc_export_link_key_result(sl_zigbee_sec_man_dlk_ecc_context_t *dlk_ecc_ctx,
                                                                uint8_t **derived_key_out)
{
  if (dlk_ecc_ctx == NULL || derived_key_out == NULL) {
    return SL_STATUS_NULL_POINTER;
  }
  *derived_key_out = dlk_ecc_ctx->derived_key;
  return SL_STATUS_OK;
}

sl_status_t sl_zigbee_sec_man_ecc_export_link_key_result(sl_zigbee_sec_man_dlk_ecc_context_t *dlk_ecc_ctx,
                                                         uint8_t **derived_key_out)
{
  return sli_zigbee_stack_sec_man_ecc_export_link_key_result(dlk_ecc_ctx, derived_key_out);
}

// NOTE the below procedures are specific to the underlying key agreement scheme
// [ECDHE-PSK] Elliptic Curve Diffie-Hellman Ephemeral (with PSK salting)

static sl_status_t ecdhe_init(sl_zigbee_sec_man_dlk_ecc_context_t *dlk_ecc_ctx)
{
  // NOTE right now only p-256 is supported for ecdhe
  if (dlk_ecc_ctx->config.curve_id != DLK_ECC_CURVE_P256) {
    return SL_STATUS_NOT_SUPPORTED;
  }
#if defined(SLI_ZB_DLK_ECDHE_USE_MBEDTLS)
  int crypto_ret = mbedtls_ecp_group_load(&sli_zigbee_dlk_ecc_get_crypto_state(dlk_ecc_ctx)->ecc_group,
                                          MBEDTLS_ECP_DP_SECP256R1);
  return sli_zb_sec_man_ecc_map_crypto_status(crypto_ret);
#else
  psa_status_t psa_status = psa_crypto_init();
  return sli_zb_sec_man_ecc_map_psa_status(psa_status);
#endif
}

static sl_status_t ecdhe_generate_keypair(sl_zigbee_sec_man_dlk_ecc_context_t *dlk_ecc_ctx)
{
  sli_zigbee_dlk_ecc_crypto_state_t *cs = sli_zigbee_dlk_ecc_get_crypto_state(dlk_ecc_ctx);
#if defined(SLI_ZB_DLK_ECDHE_USE_MBEDTLS)
  int crypto_ret = mbedtls_ecp_gen_keypair(&cs->ecc_group,
                                           &cs->d,
                                           &cs->Q,
                                           f_rng_wrapper,
                                           NULL);
  if (crypto_ret != 0) {
    return sli_zb_sec_man_ecc_map_crypto_status(crypto_ret);
  }
  // Store Zigbee wire format X||Y (big-endian P-256)
  crypto_ret = mbedtls_mpi_write_binary(&cs->Q.MBEDTLS_PRIVATE(X),
                                        cs->our_public_key,
                                        DLK_ECC_COORDINATE_SIZE);
  if (crypto_ret != 0) {
    return SL_STATUS_OBJECT_WRITE;
  }
  crypto_ret = mbedtls_mpi_write_binary(&cs->Q.MBEDTLS_PRIVATE(Y),
                                        cs->our_public_key + DLK_ECC_COORDINATE_SIZE,
                                        DLK_ECC_COORDINATE_SIZE);
  return (crypto_ret == 0) ? SL_STATUS_OK : SL_STATUS_OBJECT_WRITE;
#else
  psa_key_attributes_t key_attr = PSA_KEY_ATTRIBUTES_INIT;
  uint8_t exported_pub[DLK_ECC_PSA_P256_PUBLIC_KEY_SIZE];
  size_t exported_len = 0;

  if (cs->psa_key_valid) {
    (void)psa_destroy_key(cs->psa_private_key_id);
    cs->psa_key_valid = false;
    cs->psa_private_key_id = 0;
  }

  psa_set_key_type(&key_attr, PSA_KEY_TYPE_ECC_KEY_PAIR(PSA_ECC_FAMILY_SECP_R1));
  psa_set_key_bits(&key_attr, 256);
  psa_set_key_algorithm(&key_attr, PSA_ALG_ECDH);
  psa_set_key_usage_flags(&key_attr, PSA_KEY_USAGE_DERIVE);
  psa_set_key_lifetime(&key_attr, PSA_KEY_LIFETIME_VOLATILE);

  psa_status_t psa_status = psa_generate_key(&key_attr, &cs->psa_private_key_id);
  psa_reset_key_attributes(&key_attr);
  if (psa_status != PSA_SUCCESS) {
    return sli_zb_sec_man_ecc_map_psa_status(psa_status);
  }
  cs->psa_key_valid = true;

  psa_status = psa_export_public_key(cs->psa_private_key_id,
                                     exported_pub,
                                     sizeof(exported_pub),
                                     &exported_len);
  if (psa_status != PSA_SUCCESS
      || exported_len != DLK_ECC_PSA_P256_PUBLIC_KEY_SIZE
      || exported_pub[0] != 0x04) {
    (void)psa_destroy_key(cs->psa_private_key_id);
    cs->psa_key_valid = false;
    cs->psa_private_key_id = 0;
    return sli_zb_sec_man_ecc_map_psa_status(psa_status != PSA_SUCCESS ? psa_status : PSA_ERROR_GENERIC_ERROR);
  }

  // Store Zigbee wire format X||Y (drop uncompressed prefix)
  memcpy(cs->our_public_key, exported_pub + 1, DLK_ECC_P256_PUBLIC_KEY_SIZE);
  return SL_STATUS_OK;
#endif
}

static sl_status_t ecdhe_expand_shared_secret(sl_zigbee_sec_man_dlk_ecc_context_t *dlk_ecc_ctx)
{
  uint8_t buffer[DLK_ECC_COORDINATE_SIZE + DLK_KEY_SIZE];
  int crypto_ret = -1;
  const sli_zigbee_dlk_ecc_crypto_state_t *cs = sli_zigbee_dlk_ecc_get_crypto_state(dlk_ecc_ctx);

  if (dlk_ecc_ctx->config.hash_id != DLK_ECC_HASH_SHA_256) {
    return SL_STATUS_NOT_SUPPORTED;
  }

  memcpy(buffer, cs->shared_x_le, DLK_ECC_COORDINATE_SIZE);
  memcpy(buffer + DLK_ECC_COORDINATE_SIZE, dlk_ecc_ctx->psk, DLK_KEY_SIZE);

  crypto_ret = mbedtls_sha256(buffer, sizeof(buffer), dlk_ecc_ctx->secret, 0);
  return (crypto_ret == 0) ? SL_STATUS_OK : SL_STATUS_FAIL;
}

static sl_status_t ecdhe_derive_link_key(sl_zigbee_sec_man_dlk_ecc_context_t *dlk_ecc_ctx)
{
  uint8_t result[SHA_HASH_DIGEST_LENGTH];
  uint8_t data[1] = { 1 };

  if (dlk_ecc_ctx->config.hash_id != DLK_ECC_HASH_SHA_256) {
    return SL_STATUS_NOT_SUPPORTED;
  }

  sl_zb_sec_man_hmac_sha_256(dlk_ecc_ctx->secret, MAX_SHARED_SECRET_LEN, data, 1, result);

  // NOTE max digest is 256 bits, we only need 128 bits for encryption key
  memcpy(dlk_ecc_ctx->derived_key, result, DLK_KEY_SIZE);
  return SL_STATUS_OK;
}

// [SPEKE] Secure Passphrase Ephemeral Key Exchange

// x25519 procedure for processing generator points
// non-canonical generator points G must be reduced to the modulo field
// https://tools.ietf.org/html/rfc7748#section-5
#define X25519_BITS 255
#define X25519_MASK ((1 << (X25519_BITS % 8)) - 1)
#define X25519_U_DECODE(u) ((u)[31] &= X25519_MASK)

// NOTE key clamp is required for SPEKE with curve25519
static void x25519_key_clamp(uint8_t keyBytes[32])
{
  keyBytes[0]  &= 248;
  keyBytes[31] &= 127;
  keyBytes[31] |= 64;
}

#ifdef SL_CATALOG_ZIGBEE_SECURITY_MANAGER_DLK_ECC_TEST_VECTORS_PRESENT
static inline int speke_test_vector_load_private_key(sl_zigbee_sec_man_dlk_ecc_context_t *dlk_ecc_ctx)
{
  // TEST VECTORS

  sl_zigbee_dlk_ecc_test_vector_profile_data_t *profile = ((sl_zigbee_dlk_ecc_test_vector_bundle_t*)dlk_ecc_ctx->test)->profile;
  if (dlk_ecc_ctx->config.curve_id == DLK_ECC_CURVE_25519) {
    // perform a key clamp on the pre-set values
    // X25519 only
    x25519_key_clamp(profile->GIVEN_privkey);
  }
  // read the big endian private key into the context
  int crypto_ret = mbedtls_mpi_read_binary_le(&sli_zigbee_dlk_ecc_get_crypto_state(dlk_ecc_ctx)->d, profile->GIVEN_privkey, DLK_ECC_COORDINATE_SIZE);
  if (crypto_ret != 0) {
    return crypto_ret;
  }
  // calculate the public point by multiplying the private key with the generator point
  return mbedtls_ecp_mul(&sli_zigbee_dlk_ecc_get_crypto_state(dlk_ecc_ctx)->ecc_group,
                         &sli_zigbee_dlk_ecc_get_crypto_state(dlk_ecc_ctx)->Q,
                         &sli_zigbee_dlk_ecc_get_crypto_state(dlk_ecc_ctx)->d,
                         &sli_zigbee_dlk_ecc_get_crypto_state(dlk_ecc_ctx)->ecc_group.G,
                         f_rng_wrapper,
                         NULL);
}
#else
#define speke_test_vector_load_private_key(ctx) ((void) (ctx))
#endif // SL_CATALOG_ZIGBEE_SECURITY_MANAGER_DLK_ECC_TEST_VECTORS_PRESENT

static sl_status_t speke_init(sl_zigbee_sec_man_dlk_ecc_context_t *dlk_ecc_ctx)
{
  int crypto_ret = mbedtls_ecp_group_load(&sli_zigbee_dlk_ecc_get_crypto_state(dlk_ecc_ctx)->ecc_group, MBEDTLS_ECP_DP_CURVE25519);
  if (crypto_ret != 0) {
    return SL_STATUS_INITIALIZATION;
  }
  // "hash generator point"
  uint8_t speke_generator_data[DLK_ECC_COORDINATE_SIZE];
  mbedtls_ecp_point *g = &sli_zigbee_dlk_ecc_get_crypto_state(dlk_ecc_ctx)->ecc_group.G;
  // hash psk in little endian
  if (dlk_ecc_ctx->config.hash_id == DLK_ECC_HASH_AES_MMO_128) {
    // perform a cyclic extension
    for (uint8_t i = 0; i < 2; i++) {
      sl_status_t status = sli_zigbee_stack_aes_hash_simple(DLK_KEY_SIZE, dlk_ecc_ctx->psk, speke_generator_data + (i * AES_HASH_DIGEST_LENGTH));
      if (status != 0) {
        return status;
      }
    }
  } else { // SHA256
    mbedtls_sha256(dlk_ecc_ctx->psk, DLK_KEY_SIZE, speke_generator_data, 0);
    // set byte[0] = 0x09
    speke_generator_data[0] = 0x09;
  }
  // set coordiantes of G
  crypto_ret = mbedtls_mpi_lset(&g->MBEDTLS_PRIVATE(Z), 1);
  if (crypto_ret != 0) {
    return SL_STATUS_FAIL;
  }
  crypto_ret = mbedtls_mpi_read_binary_le(&g->MBEDTLS_PRIVATE(X), speke_generator_data, DLK_ECC_COORDINATE_SIZE);
  return (crypto_ret == 0) ? SL_STATUS_OK : SL_STATUS_FAIL;
}

static sl_status_t speke_generate_keypair(sl_zigbee_sec_man_dlk_ecc_context_t *dlk_ecc_ctx)
{
  (void) x25519_key_clamp;
  int crypto_ret = -1;
  bool clipped = false;
  if (dlk_ecc_ctx->config.curve_id == DLK_ECC_CURVE_25519) {
    // NOTE clearing the high bit of the generator point G forces it to fall into the canonical values Curve25519,
    // which prevents mbedtls from throwing an error.  We need to keep track of when we need to reset
    // the bit so we don't interfere with hashing operations later on.
    clipped = mbedtls_mpi_cmp_mpi(&sli_zigbee_dlk_ecc_get_crypto_state(dlk_ecc_ctx)->ecc_group.G.MBEDTLS_PRIVATE(X), &sli_zigbee_dlk_ecc_get_crypto_state(dlk_ecc_ctx)->ecc_group.P) == 1;
    crypto_ret = mbedtls_mpi_set_bit(&sli_zigbee_dlk_ecc_get_crypto_state(dlk_ecc_ctx)->ecc_group.G.MBEDTLS_PRIVATE(X), sli_zigbee_dlk_ecc_get_crypto_state(dlk_ecc_ctx)->ecc_group.pbits, 0);
    if (crypto_ret != 0) {
      return SL_STATUS_FAIL;
    }
  }
  if (dlk_ecc_ctx->test != NULL) {
    speke_test_vector_load_private_key(dlk_ecc_ctx);
  } else {
    uint8_t gen_private_key[DLK_ECC_COORDINATE_SIZE] = { 0, };
    crypto_ret = f_rng_wrapper(NULL, gen_private_key, DLK_ECC_COORDINATE_SIZE);
    if (crypto_ret != 0) {
      return SL_STATUS_SECURITY_RANDOM_NUM_GEN_ERROR;
    }
    // clamp the private key
    x25519_key_clamp(gen_private_key);
    crypto_ret = mbedtls_mpi_read_binary_le(&sli_zigbee_dlk_ecc_get_crypto_state(dlk_ecc_ctx)->d, gen_private_key, DLK_ECC_COORDINATE_SIZE);
    if (crypto_ret != 0) {
      return SL_STATUS_SECURITY_KEY_ERROR;
    }
    // perform point multiplication dG = Q
    crypto_ret = mbedtls_ecp_mul(&sli_zigbee_dlk_ecc_get_crypto_state(dlk_ecc_ctx)->ecc_group,
                                 &sli_zigbee_dlk_ecc_get_crypto_state(dlk_ecc_ctx)->Q,
                                 &sli_zigbee_dlk_ecc_get_crypto_state(dlk_ecc_ctx)->d,
                                 &sli_zigbee_dlk_ecc_get_crypto_state(dlk_ecc_ctx)->ecc_group.G,
                                 f_rng_wrapper,
                                 NULL);
  }
  if (crypto_ret != 0) {
    return SL_STATUS_FAIL;
  }
  if (clipped) {
    // NOTE restoring the high bit
    crypto_ret = mbedtls_mpi_set_bit(&sli_zigbee_dlk_ecc_get_crypto_state(dlk_ecc_ctx)->ecc_group.G.MBEDTLS_PRIVATE(X), sli_zigbee_dlk_ecc_get_crypto_state(dlk_ecc_ctx)->ecc_group.pbits, 1);
  }
  if (crypto_ret != 0) {
    return SL_STATUS_FAIL;
  }
  return SL_STATUS_OK;
}

// shared secret components
#define EUI64_SIZE 8
#define SESSION_ID_COMPONENT_LENGTH (EUI64_SIZE + 2 * DLK_ECC_COORDINATE_SIZE)
#define SESSION_IDENTITY_LENGTH (2 * SESSION_ID_COMPONENT_LENGTH)
#define SECRET_HASH_INPUT_LENGTH (2 * DLK_ECC_COORDINATE_SIZE + SESSION_IDENTITY_LENGTH)

// NOTE compare eui64 as uint64_t values
typedef union EUI64_u {
  uint64_t as_word;
  uint8_t as_bytes[EUI64_SIZE];
} sli_802154_long_addr_t;

typedef union {
  mbedtls_sha256_context sha;
  sl_zigbee_aes_mmo_hash_context_t aes_mmo;
} sl_zigbee_dlk_ecc_hash_ctx_t;

static void speke_hash_free_sha(sl_zigbee_dlk_ecc_hash_ctx_t *hash_ctx,
                                const sl_zb_dlk_ecc_config_t *config)
{
  if (config->hash_id == DLK_ECC_HASH_SHA_256) {
    mbedtls_sha256_free(&hash_ctx->sha);
  }
}

static sl_status_t speke_hash_init(sl_zigbee_dlk_ecc_hash_ctx_t *hash_ctx,
                                   const sl_zb_dlk_ecc_config_t *config)
{
  if (config->hash_id == DLK_ECC_HASH_SHA_256) {
    mbedtls_sha256_init(&hash_ctx->sha);
    // NOTE 0 == !is224
    mbedtls_sha256_starts(&hash_ctx->sha, 0);
    return SL_STATUS_OK;
  }
  if (config->hash_id == DLK_ECC_HASH_AES_MMO_128) {
    sli_zigbee_stack_aes_mmo_hash_init(&hash_ctx->aes_mmo);
    return SL_STATUS_OK;
  }
  return SL_STATUS_NOT_SUPPORTED;
}

static sl_status_t speke_hash_update(sl_zigbee_dlk_ecc_hash_ctx_t *hash_ctx,
                                     const sl_zb_dlk_ecc_config_t *config,
                                     const uint8_t *data,
                                     size_t len)
{
  if (config->hash_id == DLK_ECC_HASH_SHA_256) {
    if (mbedtls_sha256_update(&hash_ctx->sha, data, len) != 0) {
      mbedtls_sha256_free(&hash_ctx->sha);
      return SL_STATUS_FAIL;
    }
    return SL_STATUS_OK;
  }
  if (config->hash_id == DLK_ECC_HASH_AES_MMO_128) {
    return sli_zigbee_stack_aes_mmo_hash_update(&hash_ctx->aes_mmo,
                                                (uint32_t)len,
                                                data);
  }
  return SL_STATUS_NOT_SUPPORTED;
}

static sl_status_t speke_hash_finish(sl_zigbee_dlk_ecc_hash_ctx_t *hash_ctx,
                                     const sl_zb_dlk_ecc_config_t *config,
                                     uint8_t *digest)
{
  if (config->hash_id == DLK_ECC_HASH_SHA_256) {
    int crypto_ret = mbedtls_sha256_finish(&hash_ctx->sha, digest);
    mbedtls_sha256_free(&hash_ctx->sha);
    return (crypto_ret == 0) ? SL_STATUS_OK : SL_STATUS_FAIL;
  }
  if (config->hash_id == DLK_ECC_HASH_AES_MMO_128) {
    sl_status_t status = sli_zigbee_stack_aes_mmo_hash_final(&hash_ctx->aes_mmo, 0, NULL);
    if (status != SL_STATUS_OK) {
      return status;
    }
    memmove(digest, hash_ctx->aes_mmo.result, SL_ZIGBEE_AES_HASH_BLOCK_SIZE);
    return SL_STATUS_OK;
  }
  return SL_STATUS_NOT_SUPPORTED;
}

static sl_status_t speke_build_session_identity(sl_zigbee_sec_man_dlk_ecc_context_t *dlk_ecc_ctx,
                                                const uint8_t *our_eui,
                                                const uint8_t *their_eui,
                                                uint8_t *identity_out,
                                                size_t *identity_len_out)
{
  uint8_t *identity_cursor = identity_out;
  sli_802154_long_addr_t our_eui_compare;
  sli_802154_long_addr_t their_eui_compare;
  size_t public_key_len;

  memmove(our_eui_compare.as_bytes, our_eui, EUI64_SIZE);
  memmove(their_eui_compare.as_bytes, their_eui, EUI64_SIZE);
  // NOTE technically euis cannot be equal because then they would not be unique
  const sli_802154_long_addr_t *first_component = (their_eui_compare.as_word < our_eui_compare.as_word)
                                            ? &their_eui_compare : &our_eui_compare;
  const sli_802154_long_addr_t *second_component = (their_eui_compare.as_word < our_eui_compare.as_word)
                                             ? &our_eui_compare : &their_eui_compare;
  // NOTE polarity here matches export_public_key
  bool get_peer = (first_component->as_word == their_eui_compare.as_word);

  memmove(identity_cursor, first_component->as_bytes, EUI64_SIZE);
  identity_cursor += EUI64_SIZE;
  sl_status_t status = sli_zb_sec_man_ecc_export_public_key(dlk_ecc_ctx, get_peer, identity_cursor, &public_key_len);
  if (status != SL_STATUS_OK) {
    return status;
  }
  identity_cursor += public_key_len;

  get_peer = !get_peer;
  memmove(identity_cursor, second_component->as_bytes, EUI64_SIZE);
  identity_cursor += EUI64_SIZE;
  status = sli_zb_sec_man_ecc_export_public_key(dlk_ecc_ctx, get_peer, identity_cursor, &public_key_len);
  if (status != SL_STATUS_OK) {
    return status;
  }
  identity_cursor += public_key_len;
  *identity_len_out = (size_t)(identity_cursor - identity_out);
  return SL_STATUS_OK;
}

sl_status_t sli_zigbee_stack_sec_man_speke_expand_shared_secret(sl_zigbee_sec_man_dlk_ecc_context_t *dlk_ecc_ctx,
                                                                const uint8_t *our_eui,
                                                                const uint8_t *their_eui)
{
  uint8_t hash_input[SESSION_IDENTITY_LENGTH];
  sl_zigbee_dlk_ecc_hash_ctx_t hash_ctx;
  size_t identity_len = 0;
  // == SPEKE shared secret
  // *) calculate session Identity I
  //      - determine order by comparing eui64, smaller goes first
  //      - I = A_min | Q_min | A_max | Q_max
  // *) hash x_k | I | G
  // =====
  sl_status_t status = speke_hash_init(&hash_ctx, &dlk_ecc_ctx->config);
  if (status != SL_STATUS_OK) {
    return status;
  }

  if (mbedtls_mpi_write_binary_le(&sli_zigbee_dlk_ecc_get_crypto_state(dlk_ecc_ctx)->x_k,
                                  hash_input,
                                  DLK_ECC_COORDINATE_SIZE) != 0) {
    speke_hash_free_sha(&hash_ctx, &dlk_ecc_ctx->config);
    return SL_STATUS_OBJECT_WRITE;
  }

  status = speke_hash_update(&hash_ctx, &dlk_ecc_ctx->config, hash_input, DLK_ECC_COORDINATE_SIZE);
  if (status != SL_STATUS_OK) {
    return status;
  }

  status = speke_build_session_identity(dlk_ecc_ctx, our_eui, their_eui, hash_input, &identity_len);
  if (status != SL_STATUS_OK) {
    speke_hash_free_sha(&hash_ctx, &dlk_ecc_ctx->config);
    return status;
  }

  // NOTE aes-mmo update requires 16-byte aligned length
  status = speke_hash_update(&hash_ctx, &dlk_ecc_ctx->config, hash_input, identity_len);
  if (status != SL_STATUS_OK) {
    return status;
  }

  if (mbedtls_mpi_write_binary_le(&sli_zigbee_dlk_ecc_get_crypto_state(dlk_ecc_ctx)->ecc_group.G.MBEDTLS_PRIVATE(X),
                                  hash_input,
                                  DLK_ECC_COORDINATE_SIZE) != 0) {
    speke_hash_free_sha(&hash_ctx, &dlk_ecc_ctx->config);
    return SL_STATUS_OBJECT_WRITE;
  }

  status = speke_hash_update(&hash_ctx, &dlk_ecc_ctx->config, hash_input, DLK_ECC_COORDINATE_SIZE);
  if (status != SL_STATUS_OK) {
    return status;
  }

  return speke_hash_finish(&hash_ctx, &dlk_ecc_ctx->config, dlk_ecc_ctx->secret);
}

static sl_status_t speke_derive_link_key(sl_zigbee_sec_man_dlk_ecc_context_t *dlk_ecc_ctx)
{
  // run the secret through KDF with input {1}
  uint8_t data[1] = { 1 };
  if (dlk_ecc_ctx->config.hash_id == DLK_ECC_HASH_AES_MMO_128) {
    sl_zigbee_sec_man_context_t context;
    sli_zigbee_stack_sec_man_init_context(&context);
    context.core_key_type = SL_ZB_SEC_MAN_KEY_TYPE_INTERNAL;
    sli_zigbee_stack_sec_man_import_key(&context, (sl_zigbee_sec_man_key_t*)&(dlk_ecc_ctx->secret));
    sli_zigbee_stack_sec_man_load_key_context(&context);
    sli_zigbee_stack_sec_man_hmac_aes_mmo(data, 1, dlk_ecc_ctx->derived_key);
  } else if (dlk_ecc_ctx->config.hash_id == DLK_ECC_HASH_SHA_256) {
    uint8_t sha_digest[SHA_HASH_DIGEST_LENGTH];
    sl_zb_sec_man_hmac_sha_256(dlk_ecc_ctx->secret, MAX_SHARED_SECRET_LEN, data, 1, sha_digest);
    memmove(dlk_ecc_ctx->derived_key, sha_digest, DLK_KEY_SIZE);
  } else {
    // UNREACHABLE
    return SL_STATUS_NOT_SUPPORTED;
  }
  return SL_STATUS_OK;
}

#define HMAC_SHA_256_BLOCK_SIZE 64
// NOTE relocate / consolidate
static void xorKeyWithByte(uint8_t *key,
                           uint8_t byte,
                           uint8_t *result)
{
  int i;
  for (i = 0; i < HMAC_SHA_256_BLOCK_SIZE; i++) {
    result[i] = key[i] ^ byte;
  }
}

static void sli_hmac_sha_256_impl(mbedtls_sha256_context *sha_ctx,
                                  uint8_t *key,
                                  uint8_t keyLength,
                                  uint8_t *data,
                                  uint8_t dataLength,
                                  uint8_t *result)
{
  uint8_t buffer[HMAC_SHA_256_BLOCK_SIZE];
  uint8_t keyp[HMAC_SHA_256_BLOCK_SIZE] = { 0 }; // Zero padding
  uint8_t run;

  if (keyLength > HMAC_SHA_256_BLOCK_SIZE) {
    // hash keys longer than BLOCK_SIZE
    mbedtls_sha256_starts(sha_ctx, 0);
    mbedtls_sha256_update(sha_ctx, key, keyLength);
    mbedtls_sha256_finish(sha_ctx, keyp);
  } else {
    memcpy(keyp, key, keyLength);
  }

  for (run = 0; run < 2; run++) {
    // produce inner/outer xord key
    xorKeyWithByte(keyp,
                   (run == 0 ? 0x36 : 0x5c),
                   buffer);
    // run the hash to produce the result
    mbedtls_sha256_starts(sha_ctx, 0);
    mbedtls_sha256_update(sha_ctx, buffer, HMAC_SHA_256_BLOCK_SIZE);
    if (run == 0) {
      mbedtls_sha256_update(sha_ctx, data, dataLength);
    } else {
      mbedtls_sha256_update(sha_ctx, result, HMAC_SHA_256_OUTPUT_SIZE);
    }
    mbedtls_sha256_finish(sha_ctx, result);
  }
}

void sl_zb_sec_man_hmac_sha_256(uint8_t *key,
                                uint8_t key_len,
                                uint8_t *data,
                                uint8_t data_len,
                                uint8_t *result)
{
  mbedtls_sha256_context sha_ctx;
  mbedtls_sha256_init(&sha_ctx);
  sli_hmac_sha_256_impl(&sha_ctx, key, key_len, data, data_len, result);
  mbedtls_sha256_free(&sha_ctx);
}
