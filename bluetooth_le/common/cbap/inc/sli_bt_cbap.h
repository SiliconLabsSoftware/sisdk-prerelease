/***************************************************************************//**
 * @file
 * @brief Certificate Based Authentication and Pairing internal header
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
#ifndef SLI_BT_CBAP_H
#define SLI_BT_CBAP_H

// -----------------------------------------------------------------------------
// Includes

#include <stdint.h>
#include <stddef.h>
#include "sl_status.h"
#include "sl_component_catalog.h"
#include "sl_bt_api.h"
#include "sl_bt_cbap_config.h"
#include "sl_bt_cbap_key_id.h"

// -----------------------------------------------------------------------------
// Defines

// A prover device sends its own certificates to prove itself, a verifier device
// validates the certificates received from the remote device. One device can
// be both, which is the Full role.
#define SLI_BT_CBAP_ROLE_IS_PROVER \
  ((SL_BT_CBAP_ROLE & SL_BT_CBAP_ROLE_PROVER) != 0)
#define SLI_BT_CBAP_ROLE_IS_VERIFIER \
  ((SL_BT_CBAP_ROLE & SL_BT_CBAP_ROLE_VERIFIER) != 0)

// Uncompressed P-256 public key followed by an optional ECDSA signature. Only a
// prover signs its key, so the session data of a verifier-only device is the
// public key alone.
#define SLI_BT_CBAP_SESSION_PUBLIC_KEY_LEN  65U
#define SLI_BT_CBAP_SESSION_SIGNATURE_LEN   64U
#define SLI_BT_CBAP_SESSION_DATA_LEN  \
  (SLI_BT_CBAP_SESSION_PUBLIC_KEY_LEN \
   + SLI_BT_CBAP_SESSION_SIGNATURE_LEN)

// The pairing parameters are the passkey in network byte order, followed by
// the IO capability of the peripheral device as the Security Manager Protocol
// encodes it. They are encrypted, and followed by the CCM tag.
#define SLI_BT_CBAP_PASSKEY_LEN             4U
#define SLI_BT_CBAP_IO_CAPABILITY_LEN       1U
#define SLI_BT_CBAP_PAIRING_PARAMETERS_LEN \
  (SLI_BT_CBAP_PASSKEY_LEN + SLI_BT_CBAP_IO_CAPABILITY_LEN)
#define SLI_BT_CBAP_CCM_TAG_LEN             16U
#define SLI_BT_CBAP_ENCRYPTED_PAIRING_PARAMETERS_LEN \
  (SLI_BT_CBAP_PAIRING_PARAMETERS_LEN + SLI_BT_CBAP_CCM_TAG_LEN)
// The Bluetooth passkey has six decimal digits.
#define SLI_BT_CBAP_PASSKEY_LIMIT           1000000UL

// Logging
#define SLI_BT_CBAP_LOG_PREFIX              "[CBAP] "
#if defined(SL_CATALOG_APP_LOG_PRESENT)
#include "app_log.h"
#define SLI_BT_CBAP_LOG_DEBUG(...) \
  app_log_debug(SLI_BT_CBAP_LOG_PREFIX __VA_ARGS__)
#define SLI_BT_CBAP_LOG_INFO(...) \
  app_log_info(SLI_BT_CBAP_LOG_PREFIX __VA_ARGS__)
#define SLI_BT_CBAP_LOG_ERROR(...) \
  app_log_error(SLI_BT_CBAP_LOG_PREFIX __VA_ARGS__)
#define SLI_BT_CBAP_LOG_NL                  APP_LOG_NL
#else // SL_CATALOG_APP_LOG_PRESENT
#define SLI_BT_CBAP_LOG_DEBUG(...)
#define SLI_BT_CBAP_LOG_INFO(...)
#define SLI_BT_CBAP_LOG_ERROR(...)
#define SLI_BT_CBAP_LOG_NL
#endif // SL_CATALOG_APP_LOG_PRESENT

// -----------------------------------------------------------------------------
// Private function declarations
#ifdef __cplusplus
extern "C" {
#endif

/******************************************************************************
 * Initialize the cryptographic part of the component.
 *
 * Validate the part of the provisioned certificate chain that the configured
 * role needs: the root and factory certificates for a verifier, the batch and
 * device certificates and the device key for a prover. The device cannot take
 * part in a CBAP procedure without them, therefore a missing or invalid
 * certificate asserts.
 *
 * Called by the component framework at the internal_app_init stage.
 *****************************************************************************/
void sli_bt_cbap_crypto_init(void);

/******************************************************************************
 * Handle the Bluetooth events of the CBAP procedure.
 *
 * Called by the component framework for every Bluetooth event.
 *
 * @param[in] evt Bluetooth event.
 *****************************************************************************/
void sli_bt_cbap_on_event(sl_bt_msg_t *evt);

/******************************************************************************
 * Initialize the security session.
 *
 * Create the P-256 ECDH ephemeral key of the session and export its public
 * part. A prover also signs the public key with the key that belongs to its
 * device certificate, so that a verifier peer can check that the certificate
 * belongs to this device.
 *
 * @param[out] data Output buffer for the session data: the public key in
 *                  uncompressed format, followed by its ECDSA signature if the
 *                  device is a prover.
 * @param[out] data_len Length of the data written to @p data, which is
 *                      @ref SLI_BT_CBAP_SESSION_DATA_LEN for a prover and
 *                      @ref SLI_BT_CBAP_SESSION_PUBLIC_KEY_LEN otherwise. Set
 *                      to zero on error.
 * @param[in] max_len Size of the @p data buffer in bytes. It has to be at
 *                    least @ref SLI_BT_CBAP_SESSION_DATA_LEN.
 *
 * @return SL_STATUS_OK on success, SL_STATUS_INVALID_STATE if a session
 *         already exists, error code otherwise.
 *****************************************************************************/
sl_status_t sli_bt_cbap_session_init(uint8_t *data,
                                     size_t *data_len,
                                     size_t max_len);

/******************************************************************************
 * Complete the security session with the session data of the peer device.
 *
 * A verifier checks the signature of the peer public key against the public
 * key of the peer device certificate, so the peer session data has to be
 * signed. Then the shared secret is computed with ECDH, and the AES-128-CCM
 * key and the nonce of the session are derived from it with HKDF-SHA256.
 *
 * A verifier has to validate the certificate chain of the peer device with
 * @ref sli_bt_cbap_validate_remote_chain() beforehand, which imports the key
 * the signature is checked with.
 *
 * @param[in] data The session data received from the peer device.
 * @param[in] data_len Length of the data. A verifier only accepts
 *                     @ref SLI_BT_CBAP_SESSION_DATA_LEN, a prover-only device
 *                     also accepts @ref SLI_BT_CBAP_SESSION_PUBLIC_KEY_LEN.
 *
 * @return SL_STATUS_OK on success, SL_STATUS_INVALID_STATE if the session was
 *         not initialized, error code otherwise.
 *****************************************************************************/
sl_status_t sli_bt_cbap_session_process(const uint8_t *data, size_t data_len);

/******************************************************************************
 * Clear the security session.
 *
 * Destroy the keys of the session and clear the derived nonce. Safe to call
 * when no session exists.
 *
 * @return SL_STATUS_OK on success, error code otherwise.
 *****************************************************************************/
sl_status_t sli_bt_cbap_session_clear(void);

/******************************************************************************
 * Encrypt a message with the security session.
 *
 * The session has to be completed with @ref sli_bt_cbap_session_init() and
 * @ref sli_bt_cbap_session_process() beforehand.
 *
 * @param[in] data_in Data to encrypt.
 * @param[in] data_in_len Length of the data.
 * @param[out] data_out The encrypted data, followed by the CCM tag.
 * @param[out] data_out_len Length of the data written to @p data_out, which is
 *                          @p data_in_len + @ref SLI_BT_CBAP_CCM_TAG_LEN. Set
 *                          to zero on error.
 * @param[in] data_out_maxlen Size of the @p data_out buffer in bytes.
 *
 * @return SL_STATUS_OK on success, SL_STATUS_INVALID_STATE if the session is
 *         not complete, error code otherwise.
 *
 * @note The nonce of the session is derived, not random. Encrypt only one
 * message per session, and establish a new session before encrypting another
 * one, otherwise the nonce is reused.
 *****************************************************************************/
sl_status_t sli_bt_cbap_session_encrypt(const uint8_t *data_in,
                                        size_t data_in_len,
                                        uint8_t *data_out,
                                        size_t *data_out_len,
                                        size_t data_out_maxlen);

/******************************************************************************
 * Authenticate and decrypt a message with the security session.
 *
 * The session has to be completed with @ref sli_bt_cbap_session_init() and
 * @ref sli_bt_cbap_session_process() beforehand.
 *
 * @param[in] data_in Data to decrypt, followed by the CCM tag.
 * @param[in] data_in_len Length of the data, including the tag.
 * @param[out] data_out The decrypted data.
 * @param[out] data_out_len Length of the data written to @p data_out. Set to
 *                          zero on error.
 * @param[in] data_out_maxlen Size of the @p data_out buffer in bytes.
 *
 * @return SL_STATUS_OK on success, SL_STATUS_INVALID_SIGNATURE if the tag does
 *         not authenticate the data, SL_STATUS_INVALID_STATE if the session is
 *         not complete, error code otherwise.
 *****************************************************************************/
sl_status_t sli_bt_cbap_session_decrypt(const uint8_t *data_in,
                                        size_t data_in_len,
                                        uint8_t *data_out,
                                        size_t *data_out_len,
                                        size_t data_out_maxlen);

/******************************************************************************
 * Generate a uniformly distributed random number in the given range.
 *
 * A random value that would bias the result of the modulo operation is
 * rejected and drawn again.
 *
 * @param[in] min_inclusive Lower bound (inclusive).
 * @param[in] max_exclusive Upper bound (exclusive).
 * @param[out] random The generated random number.
 *
 * @return SL_STATUS_OK on success, SL_STATUS_INVALID_PARAMETER if the range is
 *         empty, error code otherwise.
 *****************************************************************************/
sl_status_t sli_bt_cbap_generate_random(uint32_t min_inclusive,
                                        uint32_t max_exclusive,
                                        uint32_t *random);

/******************************************************************************
 * Read a certificate from the persistent storage in DER format.
 *
 * The certificate is copied into the buffer owned by the caller, therefore it
 * stays valid until the caller overwrites it. The component keeps no reference
 * to the buffer. Each call reads the storage again, so callers that need the
 * certificate repeatedly should read it once and keep the copy.
 *
 * @param[in] id Storage ID of the certificate, for example
 *               SL_BT_CBAP_PSA_DEVICE_CERT. Note that the certificate IDs are
 *               distinct from the key IDs, such as SL_BT_CBAP_PSA_DEVICE_KEY.
 * @param[out] data Buffer receiving the certificate in DER format.
 * @param[out] data_len Length of the certificate written to @p data. Set to
 *                      zero if the certificate could not be read.
 * @param[in] max_len Size of the @p data buffer in bytes.
 *
 * @return SL_STATUS_OK on success, error code otherwise.
 *****************************************************************************/
sl_status_t sli_bt_cbap_get_certificate(cbap_key_id_t id,
                                        uint8_t *data,
                                        size_t *data_len,
                                        size_t max_len);

/******************************************************************************
 * Write a certificate to the persistent storage in DER format.
 *
 * An object already present at @p id is overwritten, so a certificate received
 * from an earlier remote device does not have to be removed first.
 *
 * @param[in] id Storage ID of the certificate.
 * @param[in] data Certificate in DER format.
 * @param[in] data_len Length of the certificate in bytes.
 *
 * @return SL_STATUS_OK on success, error code otherwise.
 *****************************************************************************/
sl_status_t sli_bt_cbap_set_certificate(cbap_key_id_t id,
                                        const uint8_t *data,
                                        size_t data_len);

/******************************************************************************
 * Validate the certificate chain presented by the remote device.
 *
 * Builds the chain from the batch and device certificates received from the
 * remote device, anchored to the factory certificate this device was
 * provisioned with, and validates every link of it. Success means the remote
 * device certificate belongs to the same trust hierarchy.
 *
 * A valid chain does not prove that the remote device owns the certificates.
 * Therefore, the public key of the remote device certificate is imported as
 * well, which @ref sli_bt_cbap_session_process() checks the signature of the
 * remote session data with.
 *
 * Both remote certificates have to be stored with
 * @ref sli_bt_cbap_set_certificate() before calling this function.
 *
 * @note Only meaningful for a device in the verifier role, as it is the one
 * provisioned with the root and factory certificates.
 *
 * @return SL_STATUS_OK on success, SL_STATUS_INVALID_SIGNATURE if a remote
 *         certificate was not issued by the expected issuer, error code
 *         otherwise.
 *****************************************************************************/
sl_status_t sli_bt_cbap_validate_remote_chain(void);

/******************************************************************************
 * Destroy the public key imported from the remote device certificate.
 *
 * @ref sli_bt_cbap_validate_remote_chain() imports the key. Destroy it once the
 * procedure with the remote device is over, so that the key slot is released.
 * Safe to call when no key was imported.
 *
 * @return SL_STATUS_OK on success, error code otherwise.
 *****************************************************************************/
sl_status_t sli_bt_cbap_destroy_peer_pub_key(void);

#ifdef __cplusplus
}
#endif

#endif // SLI_BT_CBAP_H
