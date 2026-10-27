/***************************************************************************//**
 * @file
 * @brief Shared types and constants for Green Power v1.1.3 security.
 *******************************************************************************
 * # License
 * <b>Copyright 2026 Silicon Laboratories Inc. www.silabs.com</b>
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

#ifndef GREEN_POWER_V113_H
#define GREEN_POWER_V113_H

#include "green-power-v113-config.h"
#include "stack/include/gp-install-code.h"
#include "stack/include/gp-types.h"

#define SL_ZIGBEE_GP_CLUSTER_REVISION_V113  0x0003U

#define ZCL_GP_TC_POLICY_ALLOW_GPD_ATTRIBUTE_ID              0x0030U
#define ZCL_GP_TC_POLICY_IC_MODE_ATTRIBUTE_ID                0x0031U
#define ZCL_GP_TC_POLICY_SECURITY_LEVEL_ATTRIBUTE_ID        0x0032U
#define ZCL_GP_TC_POLICY_SHARED_SECURITY_KEY_TYPE_ATTRIBUTE_ID 0x0033U
#define ZCL_GP_TC_POLICY_SHARED_SECURITY_KEY_ATTRIBUTE_ID   0x0034U
#define ZCL_GP_TC_POLICY_LINK_KEY_ATTRIBUTE_ID              0x0035U

#define SL_ZIGBEE_GP_TC_RESPONSE_STATUS_SUCCESS       0x00U
#define SL_ZIGBEE_GP_TC_RESPONSE_STATUS_FAILURE       0x01U
#define SL_ZIGBEE_GP_TC_RESPONSE_STATUS_ACTION_DENIED 0x02U
// Internal only (Figure 120): do not transmit a GP TC Response; let pairing time out.
#define SL_ZIGBEE_GP_TC_RESPONSE_STATUS_DROP          0xFFU

// Table 40 – ICEncryptedKeyType sub-field of GP TC Response Options.
#define SL_ZIGBEE_GP_IC_ENCRYPTED_KEY_TYPE_NONE              0x00U
#define SL_ZIGBEE_GP_IC_ENCRYPTED_KEY_TYPE_NWK_KEY           0x01U
#define SL_ZIGBEE_GP_IC_ENCRYPTED_KEY_TYPE_GPD_GROUP_KEY     0x02U
#define SL_ZIGBEE_GP_IC_ENCRYPTED_KEY_TYPE_NWK_DERIVED_GROUP 0x03U
#define SL_ZIGBEE_GP_IC_ENCRYPTED_KEY_TYPE_DERIVED_INDIVIDUAL 0x07U
// Legacy aliases used by policy/client as link-key-source hints (not Table 40).
#define SL_ZIGBEE_GP_IC_ENCRYPTED_KEY_TYPE_INSTALL_CODE      SL_ZIGBEE_GP_IC_ENCRYPTED_KEY_TYPE_NWK_KEY
#define SL_ZIGBEE_GP_IC_ENCRYPTED_KEY_TYPE_TC_LINK_KEY       SL_ZIGBEE_GP_IC_ENCRYPTED_KEY_TYPE_GPD_GROUP_KEY
#define SL_ZIGBEE_GP_IC_ENCRYPTED_KEY_TYPE_DEFAULT_TC_LINK_KEY SL_ZIGBEE_GP_IC_ENCRYPTED_KEY_TYPE_NWK_DERIVED_GROUP

#define SL_ZIGBEE_GP_TC_RESPONSE_KEY_TYPE_INDIVIDUAL_GPD_KEY 0x04U
#define SL_ZIGBEE_GP_TC_RESPONSE_KEY_TYPE_KEY_TYPE_ELSEWHERE 0x05U
#define SL_ZIGBEE_GP_TC_RESPONSE_KEY_TYPE_OOB_GPD_KEY        0x06U

// Figure 72 – GP TC Request Options.
#define SL_ZIGBEE_GP_TC_REQUEST_OPTION_APPLICATION_ID        0x07U
#define SL_ZIGBEE_GP_TC_REQUEST_OPTION_OOB_KEY_PRESENT       0x08U

// Figure 53 – GP TC Response Options.
#define SL_ZIGBEE_GP_TC_RESPONSE_OPTION_APPLICATION_ID       0x0007U
#define SL_ZIGBEE_GP_TC_RESPONSE_OPTION_IN_CLEAR_KEY         0x0008U
#define SL_ZIGBEE_GP_TC_RESPONSE_OPTION_GPD_KEY_PRESENT      0x0010U
#define SL_ZIGBEE_GP_TC_RESPONSE_OPTION_GPD_KEY_ENCRYPTION   0x0020U
#define SL_ZIGBEE_GP_TC_RESPONSE_OPTION_SECURITY_LEVEL       0x00C0U
#define SL_ZIGBEE_GP_TC_RESPONSE_OPTION_SECURITY_LEVEL_OFFSET 6U
#define SL_ZIGBEE_GP_TC_RESPONSE_OPTION_KEY_TYPE             0x0700U
#define SL_ZIGBEE_GP_TC_RESPONSE_OPTION_KEY_TYPE_OFFSET      8U
#define SL_ZIGBEE_GP_TC_RESPONSE_OPTION_IC_ENCRYPTED_KEY_TYPE 0x3800U
#define SL_ZIGBEE_GP_TC_RESPONSE_OPTION_IC_ENCRYPTED_KEY_TYPE_OFFSET 11U
#define SL_ZIGBEE_GP_TC_RESPONSE_OPTION_EXTENDED_OPTIONS     0x8000U

#ifndef SL_ZIGBEE_GP_TC_REQUEST_TIMEOUT_MS
#define SL_ZIGBEE_GP_TC_REQUEST_TIMEOUT_MS  5000U
#endif
#ifndef SL_ZIGBEE_GP_TC_REQUEST_RETRY_MAX
#define SL_ZIGBEE_GP_TC_REQUEST_RETRY_MAX   1U
#endif

#define SL_GP_TC_COMMISSIONING_PAYLOAD_MAX  70U

#define GPD_IC_FALLBACK_THRESHOLD  3U

/** Persistent / RAM entry for a pre-configured OOB GPD key (GP-113-12). */
typedef struct {
  bool inUse;
  sl_zigbee_gp_address_t gpdAddr;
  sl_zigbee_key_data_t oobKey;
} sli_zigbee_gp_oob_key_entry_t;

typedef struct {
  bool allowGpd;
  bool icModeMandatory;
  uint8_t minSecurityLevel;
  uint8_t sharedSecurityKeyType;
  sl_zigbee_key_data_t sharedKey;
  sl_zigbee_key_data_t linkKey;
} sli_zigbee_gp_tc_policy_t;

typedef struct {
  sl_zigbee_gp_address_t gpdAddr;
  uint8_t options; // Figure 72: AppID + OOB key present
  // Not on the Request wire; filled from GPD Commissioning payload / local state.
  uint32_t frameCounter;
  uint8_t securityLevel;
  uint8_t securityKeyType;
  uint8_t gpdCommandId;
  uint8_t gpdCommandPayloadLen;
  uint8_t gpdCommandPayload[SL_GP_TC_COMMISSIONING_PAYLOAD_MAX];
  // Optional Request field when options bit 3 is set (Figure 71).
  bool gpdSecurityKeyPresent;
  sl_zigbee_key_data_t gpdSecurityKey;
} sli_zigbee_gp_tc_request_context_t;

typedef struct {
  uint8_t status;
  uint16_t options; // Figure 53
  uint8_t keyType;
  uint8_t icEncryptedKeyType;
  uint8_t securityLevel;
  bool gpdSecurityKeyPresent;
  bool gpdKeyEncryption;
  sl_zigbee_key_data_t gpdSecurityKey;
  uint32_t frameCounter;
  uint32_t mic;
  // Figure 52: GPDkeyMIC present when key present and encrypted.
  bool gpdKeyMicPresent;
  // Figure 52 / bit 3: InClearGPDsecurityKey.
  bool inClearGpdSecurityKeyPresent;
  sl_zigbee_key_data_t inClearGpdSecurityKey;
} sli_zigbee_gp_tc_response_t;

struct sli_zigbee_gp_comm_data_saved;
typedef struct sli_zigbee_gp_comm_data_saved sli_zigbee_gp_comm_data_saved_t;

sl_status_t sl_zigbee_af_gp_resolve_link_key(const sl_zigbee_gp_address_t *gpdAddr,
                                              sl_zigbee_gp_link_key_source_t preferredSource,
                                              sl_zigbee_key_data_t *linkKeyOut,
                                              sl_zigbee_gp_link_key_source_t *actualSourceOut);

bool sli_zigbee_af_gp_is_tc_aware_and_capable(void);

sl_status_t sli_zigbee_af_gp_send_tc_request(sli_zigbee_gp_comm_data_saved_t *commissioningGpd,
                                              const uint8_t *gpdPayload,
                                              uint8_t payloadLen);

// Test/application override for gpTC_Policy_ICMode until the ZCL attribute is in ZAP.
void sli_zigbee_gp_tc_set_ic_mode_mandatory(bool mandatory);

void sli_zigbee_af_gp_tc_response_handler(const sli_zigbee_gp_tc_response_t *response);

const sli_zigbee_gp_tc_response_t *sli_zigbee_af_gp_get_tc_response(void);

// True while a GpTcRequest for this commissioning GPD is waiting for GpTcResponse.
bool sli_zigbee_af_gp_tc_request_in_flight(const sli_zigbee_gp_comm_data_saved_t *commissioningGpd);

// A GPD retransmits its Commissioning GPDF (RxAfterTx) until it gets the
// Commissioning Reply. The TC is consulted once per commissioning session:
// when a SUCCESS GpTcResponse is already held for this GPD, re-apply it and
// return true so the caller skips a new GpTcRequest.
bool sli_zigbee_af_gp_tc_reuse_response(sli_zigbee_gp_comm_data_saved_t *commissioningGpd);

// Drop the held GpTcResponse (commissioning session for this GPD ended).
void sli_zigbee_af_gp_tc_forget_response(const sl_zigbee_gp_address_t *gpdAddr);

sl_status_t sli_zigbee_af_gp_send_tc_pairing_completion(const sl_zigbee_gp_address_t *gpdAddr);

sl_status_t sli_zigbee_af_gp_send_tc_decommissioning_notification(const sl_zigbee_gp_address_t *gpdAddr);

sl_zigbee_af_zcl_request_status_t sl_zigbee_af_green_power_cluster_gp_tc_response_cb(sl_zigbee_af_cluster_command_t *cmd);

void sl_zigbee_af_green_power_server_tc_client_init_cb(uint8_t init_level);

void sli_zigbee_af_gp_continue_tc_mediated_commissioning(sli_zigbee_gp_comm_data_saved_t *commissioningGpd);

sl_status_t sl_zigbee_af_gp_oob_build_clear_signature(const sl_zigbee_gp_address_t *gpdAddr,
                                                       const uint8_t randomOctets[2],
                                                       uint8_t signature[16]);

sl_status_t sl_zigbee_af_gp_oob_encrypt_signature(const sl_zigbee_gp_address_t *gpdAddr,
                                                   const uint8_t oobKey[16],
                                                   const uint8_t clearSignature[16],
                                                   uint8_t encryptedSignature[16],
                                                   uint8_t mic[4]);

sl_status_t sl_zigbee_af_gp_oob_verify_signature(const sl_zigbee_gp_address_t *gpdAddr,
                                                  const uint8_t oobKey[16],
                                                  const uint8_t encryptedSignature[16],
                                                  const uint8_t mic[4]);

/**
 * @brief Pre-configure an OOB key for a GPD (never sent OTA).
 *
 * Used by sink and GP-aware TC when verifying OOB key type 0x06 signatures
 * and when populating GP TC Request "OOB key present".
 */
sl_status_t sl_zigbee_af_gp_set_oob_key(const sl_zigbee_gp_address_t *gpdAddr,
                                        const uint8_t key[SL_ZIGBEE_ENCRYPTION_KEY_SIZE]);

sl_status_t sl_zigbee_af_gp_lookup_oob_key(const sl_zigbee_gp_address_t *gpdAddr,
                                           sl_zigbee_key_data_t *keyOut);

sl_status_t sl_zigbee_af_gp_clear_oob_key(const sl_zigbee_gp_address_t *gpdAddr);

void sli_zigbee_af_gp_oob_key_table_init(void);

#endif // GREEN_POWER_V113_H
