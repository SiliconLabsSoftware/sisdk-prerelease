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
#include "sl_bt_cbap_key_id.h"

// -----------------------------------------------------------------------------
// Defines

// OOB data
/// Length of the OOB random and of the OOB confirm value
#define SL_BT_CBAP_OOB_RANDOM_LEN       16
/// Length of the OOB data that is signed: the random and the confirm value
#define SL_BT_CBAP_OOB_DATA_LEN         (2 * SL_BT_CBAP_OOB_RANDOM_LEN)
/// Length of the ECDSA P-256 signature of the OOB data
#define SL_BT_CBAP_OOB_SIGNATURE_LEN    64
/// Length of the signed OOB data exchanged by the devices
#define SL_BT_CBAP_SIGNED_OOB_DATA_LEN  (SL_BT_CBAP_OOB_DATA_LEN \
                                         + SL_BT_CBAP_OOB_SIGNATURE_LEN)

// Logging
#define CBAP_LOG_PREFIX               "[CBAP] "
#if defined(SL_CATALOG_APP_LOG_PRESENT)
#include "app_log.h"
#define CBAP_LOG_DEBUG(...)           app_log_debug(CBAP_LOG_PREFIX __VA_ARGS__)
#define CBAP_LOG_INFO(...)            app_log_info(CBAP_LOG_PREFIX __VA_ARGS__)
#define CBAP_LOG_ERROR(...)           app_log_error(CBAP_LOG_PREFIX __VA_ARGS__)
#define CBAP_LOG_NL                   APP_LOG_NL
#else // SL_CATALOG_APP_LOG_PRESENT
#define CBAP_LOG_DEBUG(...)
#define CBAP_LOG_INFO(...)
#define CBAP_LOG_ERROR(...)
#define CBAP_LOG_NL
#endif // SL_CATALOG_APP_LOG_PRESENT

// -----------------------------------------------------------------------------
// Private function declarations
#ifdef __cplusplus
extern "C" {
#endif

/******************************************************************************
 * Initialize module. Validate the provisioned certificate chain.
 *****************************************************************************/
void sli_bt_cbap_crypto_init(void);

/******************************************************************************
 * Bluetooth event handler.
 * @param[in] evt Bluetooth event
 *****************************************************************************/
void sli_bt_cbap_on_event(sl_bt_msg_t *evt);

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
 * Both remote certificates have to be stored with
 * @ref sli_bt_cbap_set_certificate() before calling this function.
 *
 * @note Only meaningful for a device in the verifier role, as it is the one
 * provisioned with the root and factory certificates.
 *
 * @return SL_STATUS_OK on success, error code otherwise.
 *****************************************************************************/
sl_status_t sli_bt_cbap_validate_remote_chain(void);

/***************************************************************************//**
 * Sign and combine OOB data.
 *
 * @param[in] device_random OOB data generated by the bt stack.
 * @param[in] device_confirm OOB data generated by the bt stack.
 * @param[out] output_data The signed OOB data
 * @param[out] output_len The signed OOB data length
 *
 * @return SL_STATUS_OK if OOB data signed, error code otherwise.
 ******************************************************************************/
sl_status_t sli_bt_cbap_sign_oob_data(uint8_t *device_random,
                                      uint8_t *device_confirm,
                                      uint8_t *output_data,
                                      size_t *output_len);

/***************************************************************************//**
 * Verify the remote device OOB data signature.
 *
 * @param[in] remote_random OOB data from remote device.
 * @param[in] remote_confirm OOB data from remote device.
 * @param[in] remote_oob_signature Remote OOB signature.
 *
 * @return SL_STATUS_OK if OOB data signature is OK, error code otherwise.
 ******************************************************************************/
sl_status_t sli_bt_cbap_verify_remote_oob_data(uint8_t *remote_random,
                                               uint8_t *remote_confirm,
                                               uint8_t *remote_oob_signature);

/***************************************************************************//**
 * Destroy the keys which were used during the CBAP process.
 *
 * @return SL_STATUS_OK if OK, error code otherwise.
 ******************************************************************************/
sl_status_t sli_destroy_remote_pub_key(void);

#ifdef __cplusplus
};
#endif

#endif // SLI_BT_CBAP_H
