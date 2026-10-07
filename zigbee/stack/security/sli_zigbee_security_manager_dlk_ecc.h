/***************************************************************************//**
 * @file sli_zigbee_security_manager_dlk_ecc.h
 * @brief stateless computation of dynamic link key elliptic curve cryptography
 * (INTERNAL)
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

#ifndef SLI_ZIGBEE_SECURITY_MANAGER_DLK_ECC_H
#define SLI_ZIGBEE_SECURITY_MANAGER_DLK_ECC_H

#include "stack/include/sl_zigbee_security_manager_dlk_ecc.h"

#include "mbedtls/build_info.h"
#if defined(MBEDTLS_VERSION_MAJOR) && (MBEDTLS_VERSION_MAJOR >= 4)
#ifndef MBEDTLS_DECLARE_PRIVATE_IDENTIFIERS
#define MBEDTLS_DECLARE_PRIVATE_IDENTIFIERS
#endif
#include "mbedtls/private/bignum.h"
#include "mbedtls/private/ecp.h"
#else
#include "mbedtls/bignum.h"
#include "mbedtls/ecp.h"
#endif
#include "psa/crypto.h"

typedef struct {
  mbedtls_ecp_group ecc_group;    // elliptic curve group
  mbedtls_mpi d;                  // private key
  mbedtls_ecp_point Q;            // public-point
  mbedtls_ecp_point Qp;           // peer's public point
  mbedtls_mpi x_k;                // common point X coordinate

  // ECDHE-PSK (PSA / P-256)
  psa_key_id_t psa_private_key_id;
  bool psa_key_valid;
  uint8_t our_public_key[DLK_ECC_P256_PUBLIC_KEY_SIZE];
  uint8_t peer_public_key[DLK_ECC_P256_PUBLIC_KEY_SIZE];
  // Zigbee ECDHE expand/export uses little-endian shared X
  uint8_t shared_x_le[DLK_ECC_COORDINATE_SIZE];
} sli_zigbee_dlk_ecc_crypto_state_t;

static inline sli_zigbee_dlk_ecc_crypto_state_t *
sli_zigbee_dlk_ecc_get_crypto_state(sl_zigbee_sec_man_dlk_ecc_context_t *dlk_ecc_ctx)
{
  return (sli_zigbee_dlk_ecc_crypto_state_t *)dlk_ecc_ctx->crypto_state;
}

/**
 * @brief checks if given ecc operation, curve, and hash are valid
 */
bool sli_zigbee_is_supported_ecc_operation(sl_zb_dlk_ecc_config_t *ecc_config);

/**
 * @brief serializes the generated public key into the given buffer
 * @param dlk_ecc_ctx a pointer to the context struct containing generated key
 * @param is_peer if true serialize the peer public key
 * @param public_key_buff the destination buffer to write the public key data to
 * @note must contain enough bytes to hold the key (64 bytes for DLK_ECC_CURVE_P256)
 * (32 bytes for DLK_ECC_CURVE_25519)
 * @param public_key_len a pointer arg that will contain the number of key bytes serialized
 * @return status indicating if anything went wrong
 */
sl_status_t sli_zb_sec_man_ecc_export_public_key(sl_zigbee_sec_man_dlk_ecc_context_t *dlk_ecc_ctx,
                                                 bool is_peer,
                                                 uint8_t *public_key_buff,
                                                 size_t *public_key_len);

/**
 * @brief incorporates peer public key data into the ecc context
 * @param dlk_ecc_ctx a pointer to the context struct containing partial computation
 * @param public_key_buff the source buffer containing peer public key data
 * @param public_key_len the amount of key data contained in the input buffer
 * @return status indicating if something went wrong
 */
sl_status_t sli_zb_sec_man_ecc_import_peer_public_key(sl_zigbee_sec_man_dlk_ecc_context_t *dlk_ecc_ctx,
                                                      const uint8_t *public_key_buff,
                                                      size_t public_key_len);

/**
 * @brief exports the shared x-coordinate (little-endian) for debug/test use
 * @param dlk_ecc_ctx a pointer to the context containing the computed shared secret coordinate
 * @param shared_x_out destination buffer for DLK_ECC_COORDINATE_SIZE bytes
 * @param shared_x_len size of shared_x_out in bytes
 * @return status indicating whether export succeeded
 */
sl_status_t sli_zb_sec_man_ecc_export_shared_x(sl_zigbee_sec_man_dlk_ecc_context_t *dlk_ecc_ctx,
                                               uint8_t *shared_x_out,
                                               size_t shared_x_len);

/**
 * @brief maps a low-level mbedtls return code to an sl_status_t
 * @param crypto_ret return code from an underlying mbedtls operation
 * @return specific sl_status_t where possible (parameter/key/alloc/support), else FAIL
 */
sl_status_t sli_zb_sec_man_ecc_map_crypto_status(int crypto_ret);

/**
 * @brief maps a PSA status code to an sl_status_t
 * @return specific sl_status_t where possible (parameter/key/alloc/support), else FAIL
 */
sl_status_t sli_zb_sec_man_ecc_map_psa_status(psa_status_t psa_status);

#endif // SLI_ZIGBEE_SECURITY_MANAGER_DLK_ECC_H
