/***************************************************************************//**
 * @file
 * @brief GP-aware Trust Center policy engine for GP v1.1.3 (Figures 120-125).
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

#include "af.h"
#include "green-power-tc.h"
#include "green-power-tc-msg.h"
#include "green-power-tc-crypto.h"
#include "green-power-v113.h"
#include "stack/include/gp-install-code.h"

#define GP_DEFAULT_LINK_KEY { 0x5A, 0x69, 0x67, 0x42, 0x65, 0x65, 0x41, 0x6C, \
                              0x6C, 0x69, 0x61, 0x6E, 0x63, 0x65, 0x30, 0x39 }

static bool sli_gpTcIcModeOverrideSet = false;
static bool sli_gpTcIcModeOverride = false;

void sli_zigbee_gp_tc_set_ic_mode_mandatory(bool mandatory)
{
  sli_gpTcIcModeOverrideSet = true;
  sli_gpTcIcModeOverride = mandatory;
}

void sli_zigbee_gp_tc_load_policy(sli_zigbee_gp_tc_policy_t *policy)
{
  sl_zigbee_af_status_t status;
  sl_zigbee_af_attribute_type_t type;
  uint8_t boolVal;
  uint8_t enumVal;

  if (policy == NULL) {
    return;
  }

  memset(policy, 0, sizeof(sli_zigbee_gp_tc_policy_t));

  boolVal = 1;
  status = sl_zigbee_af_read_attribute(SL_ZIGBEE_GP_ENDPOINT,
                                       ZCL_GREEN_POWER_CLUSTER_ID,
                                       ZCL_GP_TC_POLICY_ALLOW_GPD_ATTRIBUTE_ID,
                                       CLUSTER_MASK_SERVER,
                                       &boolVal,
                                       sizeof(boolVal),
                                       &type);
  policy->allowGpd = (status == SL_ZIGBEE_ZCL_STATUS_SUCCESS) ? (boolVal != 0) : true;

  boolVal = 0;
  status = sl_zigbee_af_read_attribute(SL_ZIGBEE_GP_ENDPOINT,
                                       ZCL_GREEN_POWER_CLUSTER_ID,
                                       ZCL_GP_TC_POLICY_IC_MODE_ATTRIBUTE_ID,
                                       CLUSTER_MASK_SERVER,
                                       &boolVal,
                                       sizeof(boolVal),
                                       &type);
  policy->icModeMandatory = (status == SL_ZIGBEE_ZCL_STATUS_SUCCESS) ? (boolVal != 0) : false;
  if (sli_gpTcIcModeOverrideSet) {
    policy->icModeMandatory = sli_gpTcIcModeOverride;
  }

  enumVal = 0x02;
  status = sl_zigbee_af_read_attribute(SL_ZIGBEE_GP_ENDPOINT,
                                       ZCL_GREEN_POWER_CLUSTER_ID,
                                       ZCL_GP_TC_POLICY_SECURITY_LEVEL_ATTRIBUTE_ID,
                                       CLUSTER_MASK_SERVER,
                                       &enumVal,
                                       sizeof(enumVal),
                                       &type);
  policy->minSecurityLevel = (status == SL_ZIGBEE_ZCL_STATUS_SUCCESS) ? enumVal : 0x02;

  enumVal = 0;
  status = sl_zigbee_af_read_attribute(SL_ZIGBEE_GP_ENDPOINT,
                                       ZCL_GREEN_POWER_CLUSTER_ID,
                                       ZCL_GP_TC_POLICY_SHARED_SECURITY_KEY_TYPE_ATTRIBUTE_ID,
                                       CLUSTER_MASK_SERVER,
                                       &enumVal,
                                       sizeof(enumVal),
                                       &type);
  if (status == SL_ZIGBEE_ZCL_STATUS_SUCCESS) {
    policy->sharedSecurityKeyType = enumVal;
  } else {
    // Fall back to gpsSharedSecurityKeyType until TC policy attrs are in ZAP.
    enumVal = 0;
    status = sl_zigbee_af_read_attribute(SL_ZIGBEE_GP_ENDPOINT,
                                         ZCL_GREEN_POWER_CLUSTER_ID,
                                         ZCL_GP_SERVER_GP_SHARED_SECURITY_KEY_TYPE_ATTRIBUTE_ID,
                                         CLUSTER_MASK_SERVER,
                                         &enumVal,
                                         sizeof(enumVal),
                                         &type);
    policy->sharedSecurityKeyType = (status == SL_ZIGBEE_ZCL_STATUS_SUCCESS) ? enumVal : 0;
  }
  // Table 58: 0x04..0x06 are reserved for gpTC_Policy_SharedSecurityKeyType;
  // treat them (and anything out of range) as "no shared key".
  if (policy->sharedSecurityKeyType >= SL_ZIGBEE_GP_SECURITY_KEY_GPD_OOB
      && policy->sharedSecurityKeyType <= SL_ZIGBEE_GP_SECURITY_KEY_OOB_V113) {
    policy->sharedSecurityKeyType = 0U;
  }
  policy->sharedSecurityKeyType &= 0x07U;

  status = sl_zigbee_af_read_attribute(SL_ZIGBEE_GP_ENDPOINT,
                                       ZCL_GREEN_POWER_CLUSTER_ID,
                                       ZCL_GP_TC_POLICY_SHARED_SECURITY_KEY_ATTRIBUTE_ID,
                                       CLUSTER_MASK_SERVER,
                                       policy->sharedKey.contents,
                                       SL_ZIGBEE_ENCRYPTION_KEY_SIZE,
                                       &type);
  if (status != SL_ZIGBEE_ZCL_STATUS_SUCCESS) {
    (void) sl_zigbee_af_read_attribute(SL_ZIGBEE_GP_ENDPOINT,
                                       ZCL_GREEN_POWER_CLUSTER_ID,
                                       ZCL_GP_SERVER_GP_SHARED_SECURITY_KEY_ATTRIBUTE_ID,
                                       CLUSTER_MASK_SERVER,
                                       policy->sharedKey.contents,
                                       SL_ZIGBEE_ENCRYPTION_KEY_SIZE,
                                       &type);
  }

  status = sl_zigbee_af_read_attribute(SL_ZIGBEE_GP_ENDPOINT,
                                       ZCL_GREEN_POWER_CLUSTER_ID,
                                       ZCL_GP_TC_POLICY_LINK_KEY_ATTRIBUTE_ID,
                                       CLUSTER_MASK_SERVER,
                                       policy->linkKey.contents,
                                       SL_ZIGBEE_ENCRYPTION_KEY_SIZE,
                                       &type);
  if (status != SL_ZIGBEE_ZCL_STATUS_SUCCESS) {
    const uint8_t za09[] = GP_DEFAULT_LINK_KEY;
    memcpy(policy->linkKey.contents, za09, SL_ZIGBEE_ENCRYPTION_KEY_SIZE);
  }
}

// ---------------------------------------------------------------------------
// Response construction helpers
// ---------------------------------------------------------------------------

// Initialises a response for the current request. Every figure 120-125 result
// carries the Request frame counter and the received SecurityLevelCapabilities.
static void sli_gp_tc_init_response(sli_zigbee_gp_tc_response_t *resp,
                                    const sli_zigbee_gp_tc_request_context_t *req,
                                    const sli_gp_tc_commissioning_info_t *comm,
                                    uint8_t status)
{
  memset(resp, 0, sizeof(*resp));
  resp->status = status;
  resp->frameCounter = req->frameCounter;
  resp->securityLevel = (comm != NULL) ? comm->securityLevel : req->securityLevel;
  resp->keyType = (comm != NULL) ? comm->securityKeyType : 0U;
}

// SUCCESS with the key in clear only (GPDSecurityKeyInClearPresent 0b1,
// GPDSecurityKeyPresent 0b0, GPDKeyEncryption 0b0, ICEncryptedKeyType 0b000).
static void sli_gp_tc_respond_in_clear(sli_zigbee_gp_tc_response_t *resp,
                                       const sli_zigbee_gp_tc_request_context_t *req,
                                       const sli_gp_tc_commissioning_info_t *comm,
                                       uint8_t keyType,
                                       const uint8_t *keyInClear)
{
  sli_gp_tc_init_response(resp, req, comm, SL_ZIGBEE_GP_TC_RESPONSE_STATUS_SUCCESS);
  resp->keyType = keyType;
  resp->icEncryptedKeyType = SL_ZIGBEE_GP_IC_ENCRYPTED_KEY_TYPE_NONE;
  resp->gpdSecurityKeyPresent = false;
  resp->gpdKeyEncryption = false;
  resp->inClearGpdSecurityKeyPresent = true;
  memcpy(resp->inClearGpdSecurityKey.contents, keyInClear, SL_ZIGBEE_ENCRYPTION_KEY_SIZE);
}

// SUCCESS carrying the policy shared key, encrypted with linkKey at FC+1
// (GPDSecurityKeyPresent 0b1, GPDKeyEncryption 0b1, KeyType = keyType,
// ICEncryptedKeyType = icEncryptedKeyType, key in clear = shared key).
static bool sli_gp_tc_respond_encrypted_shared_key(sli_zigbee_gp_tc_response_t *resp,
                                                   const sli_zigbee_gp_tc_policy_t *policy,
                                                   const sli_zigbee_gp_tc_request_context_t *req,
                                                   const sli_gp_tc_commissioning_info_t *comm,
                                                   const sl_zigbee_key_data_t *linkKey,
                                                   uint8_t keyType,
                                                   uint8_t icEncryptedKeyType)
{
  sli_gp_tc_init_response(resp, req, comm, SL_ZIGBEE_GP_TC_RESPONSE_STATUS_SUCCESS);
  resp->keyType = keyType;
  resp->icEncryptedKeyType = icEncryptedKeyType;
  resp->frameCounter = req->frameCounter + 1U;
  resp->gpdSecurityKeyPresent = true;
  resp->gpdKeyEncryption = true;
  resp->inClearGpdSecurityKeyPresent = true;
  memcpy(resp->inClearGpdSecurityKey.contents, policy->sharedKey.contents, SL_ZIGBEE_ENCRYPTION_KEY_SIZE);
  memcpy(resp->gpdSecurityKey.contents, policy->sharedKey.contents, SL_ZIGBEE_ENCRYPTION_KEY_SIZE);
  if (!sli_gp_tc_encrypt_gpd_key(&req->gpdAddr,
                                 resp->frameCounter,
                                 linkKey,
                                 &resp->gpdSecurityKey,
                                 &resp->mic)) {
    sli_gp_tc_init_response(resp, req, comm, SL_ZIGBEE_GP_TC_RESPONSE_STATUS_FAILURE);
    return false;
  }
  resp->gpdKeyMicPresent = true;
  return true;
}

// Decrypts a copy of the received GPDkey with linkKey and verifies the MIC.
// On success keyOut holds the key in clear.
static bool sli_gp_tc_received_key_decrypts_with(const sli_zigbee_gp_tc_request_context_t *req,
                                                 const sli_gp_tc_commissioning_info_t *comm,
                                                 const sl_zigbee_key_data_t *linkKey,
                                                 uint8_t *keyOut)
{
  sl_zigbee_key_data_t key;
  memcpy(key.contents, comm->gpdKey.contents, SL_ZIGBEE_ENCRYPTION_KEY_SIZE);
  if (!sli_gp_tc_decrypt_gpd_key(&req->gpdAddr,
                                 req->frameCounter,
                                 linkKey,
                                 &key,
                                 comm->gpdKeyMic)) {
    return false;
  }
  memcpy(keyOut, key.contents, SL_ZIGBEE_ENCRYPTION_KEY_SIZE);
  return true;
}

// ---------------------------------------------------------------------------
// Commissioning payload
// ---------------------------------------------------------------------------

static sl_status_t sli_gp_tc_load_commissioning_info(sli_zigbee_gp_tc_request_context_t *req,
                                                     sli_gp_tc_commissioning_info_t *comm)
{
  memset(comm, 0, sizeof(*comm));
  if (req->gpdCommandPayloadLen == 0U) {
    // No Commissioning GPDF carried: the Request security fields are all we have.
    comm->securityLevel = req->securityLevel;
    comm->securityKeyType = req->securityKeyType;
    return SL_STATUS_OK;
  }
  if (req->gpdCommandPayloadLen > SL_GP_TC_COMMISSIONING_PAYLOAD_MAX) {
    return SL_STATUS_INVALID_PARAMETER;
  }

  // The parser consumes the ZCL octet-string form: length | deviceId | options | ...
  uint8_t wrapped[SL_GP_TC_COMMISSIONING_PAYLOAD_MAX + 1U];
  wrapped[0] = req->gpdCommandPayloadLen;
  memcpy(wrapped + 1, req->gpdCommandPayload, req->gpdCommandPayloadLen);
  if (sli_gp_tc_parse_commissioning_payload(wrapped,
                                            req->gpdCommandPayloadLen + 1U,
                                            comm) != SL_STATUS_OK) {
    return SL_STATUS_INVALID_PARAMETER;
  }

  // The Commissioning GPDF is authoritative for the GPD security context.
  if (comm->outgoingFrameCounterPresent) {
    req->frameCounter = comm->outgoingFrameCounter;
  }
  req->securityLevel = comm->securityLevel;
  req->securityKeyType = comm->securityKeyType;
  return SL_STATUS_OK;
}

// ---------------------------------------------------------------------------
// Figure 121: OOB key (KeyType 0b110)
// ---------------------------------------------------------------------------

static void sli_gp_tc_evaluate_oob_v113(const sli_zigbee_gp_tc_request_context_t *req,
                                        const sli_gp_tc_commissioning_info_t *comm,
                                        sli_zigbee_gp_tc_response_t *resp)
{
  sl_zigbee_key_data_t oobKey = { { 0 } };
  bool oobKeyAvailable = false;

  // The OOB key configured on the TC takes precedence; otherwise use the OOB
  // key the sink supplied in the GP TC Request (GPD OOB key present 0b1).
  if (sl_zigbee_af_gp_lookup_oob_key(&req->gpdAddr, &oobKey) == SL_STATUS_OK) {
    oobKeyAvailable = true;
  } else if (req->gpdSecurityKeyPresent) {
    memcpy(oobKey.contents, req->gpdSecurityKey.contents, SL_ZIGBEE_ENCRYPTION_KEY_SIZE);
    oobKeyAvailable = true;
  }

  if (!oobKeyAvailable) {
    // Fig 121: no OOB key at the TC and none in the Request -> Drop.
    sli_gp_tc_init_response(resp, req, comm, SL_ZIGBEE_GP_TC_RESPONSE_STATUS_DROP);
    return;
  }

  if (!comm->gpdKeyPresent) {
    // An OOB key is known, but the Commissioning GPDF carries no key to
    // verify. That is a transmitted FAILURE, not a silent Drop.
    sli_gp_tc_init_response(resp, req, comm, SL_ZIGBEE_GP_TC_RESPONSE_STATUS_FAILURE);
    return;
  }

  uint8_t mic[4];
  sl_util_store_low_high_int32u(mic, comm->gpdKeyMic);
  if (sl_zigbee_af_gp_oob_verify_signature(&req->gpdAddr,
                                           oobKey.contents,
                                           comm->gpdKey.contents,
                                           mic) != SL_STATUS_OK) {
    // Fig 121: signature verification failed -> FAILURE.
    sli_gp_tc_init_response(resp, req, comm, SL_ZIGBEE_GP_TC_RESPONSE_STATUS_FAILURE);
    return;
  }

  // Fig 121 SUCCESS: InClearPresent 0b1, GPDSecurityKeyPresent 0b0,
  // Encryption 0b0, KeyType 0b110, key in clear = OOB key.
  sli_gp_tc_respond_in_clear(resp, req, comm, SL_ZIGBEE_GP_TC_RESPONSE_KEY_TYPE_OOB_GPD_KEY, oobKey.contents);
}

// ---------------------------------------------------------------------------
// Figure 122: unidirectional commissioning, IC mode mandatory
// ---------------------------------------------------------------------------

static void sli_gp_tc_evaluate_unidir_ic_mandatory(const sli_zigbee_gp_tc_policy_t *policy,
                                                   const sli_zigbee_gp_tc_request_context_t *req,
                                                   const sli_gp_tc_commissioning_info_t *comm,
                                                   const sl_zigbee_key_data_t *icLinkKey,
                                                   sli_zigbee_gp_tc_response_t *resp)
{
  // KeyType 0b000, key not encrypted, or no outgoing counter -> Drop.
  if (comm->securityKeyType == 0U
      || !comm->gpdKeyEncrypted
      || !comm->outgoingFrameCounterPresent) {
    sli_gp_tc_init_response(resp, req, comm, SL_ZIGBEE_GP_TC_RESPONSE_STATUS_DROP);
    return;
  }

  // IC-based TCLK used 0b0 -> ACTION_DENIED (the GPD must retry with IC TCLK).
  if (!comm->icTclkUsed) {
    sli_gp_tc_init_response(resp, req, comm, SL_ZIGBEE_GP_TC_RESPONSE_STATUS_ACTION_DENIED);
    return;
  }

  // KeyType must be individual (0b100) or the policy shared key type.
  if (comm->securityKeyType != SL_ZIGBEE_GP_SECURITY_KEY_GPD_OOB
      && !(policy->sharedSecurityKeyType != 0U && comm->securityKeyType == policy->sharedSecurityKeyType)) {
    sli_gp_tc_init_response(resp, req, comm, SL_ZIGBEE_GP_TC_RESPONSE_STATUS_DROP);
    return;
  }

  uint8_t key[SL_ZIGBEE_ENCRYPTION_KEY_SIZE];
  if (!sli_gp_tc_received_key_decrypts_with(req, comm, icLinkKey, key)) {
    sli_gp_tc_init_response(resp, req, comm, SL_ZIGBEE_GP_TC_RESPONSE_STATUS_FAILURE);
    return;
  }
  sli_gp_tc_respond_in_clear(resp, req, comm, comm->securityKeyType, key);
}

// ---------------------------------------------------------------------------
// Figure 123: bidirectional commissioning, IC mode mandatory
// ---------------------------------------------------------------------------

static void sli_gp_tc_respond_switch_to_ic_tclk(sli_zigbee_gp_tc_response_t *resp,
                                                const sli_zigbee_gp_tc_request_context_t *req,
                                                const sli_gp_tc_commissioning_info_t *comm)
{
  // KeyType 0b101, ICEncryptedKeyType 0b000, no key fields: the GPD switches
  // to the install-code TC link key.
  sli_gp_tc_init_response(resp, req, comm, SL_ZIGBEE_GP_TC_RESPONSE_STATUS_SUCCESS);
  resp->keyType = SL_ZIGBEE_GP_TC_RESPONSE_KEY_TYPE_KEY_TYPE_ELSEWHERE;
  resp->icEncryptedKeyType = SL_ZIGBEE_GP_IC_ENCRYPTED_KEY_TYPE_NONE;
}

static bool sli_gp_tc_key_type_is_individual_or_shared(uint8_t securityKeyType,
                                                       uint8_t sharedSecurityKeyType)
{
  return (securityKeyType == SL_ZIGBEE_GP_SECURITY_KEY_GPD_OOB)
         || (sharedSecurityKeyType != 0U && securityKeyType == sharedSecurityKeyType);
}

// used 0b1 selects the install-code TC link key (Drop if the GPD is not in
// apsDeviceKeyPairSet). used 0b0 selects gpTC_Policy_LinkKey even when an
// install code is registered.
static bool sli_gp_tc_select_optional_link_key(const sli_zigbee_gp_tc_policy_t *policy,
                                               const sli_zigbee_gp_tc_request_context_t *req,
                                               const sli_gp_tc_commissioning_info_t *comm,
                                               const sl_zigbee_key_data_t *icLinkKey,
                                               bool icLinkKeyAvailable,
                                               const sl_zigbee_key_data_t **linkKey,
                                               sli_zigbee_gp_tc_response_t *resp)
{
  if (comm->icTclkUsed) {
    if (!icLinkKeyAvailable || icLinkKey == NULL) {
      sli_gp_tc_init_response(resp, req, comm, SL_ZIGBEE_GP_TC_RESPONSE_STATUS_DROP);
      return false;
    }
    *linkKey = icLinkKey;
    return true;
  }
  *linkKey = &policy->linkKey;
  return true;
}

static void sli_gp_tc_evaluate_bidir_ic_mandatory(const sli_zigbee_gp_tc_policy_t *policy,
                                                  const sli_zigbee_gp_tc_request_context_t *req,
                                                  const sli_gp_tc_commissioning_info_t *comm,
                                                  const sl_zigbee_key_data_t *icLinkKey,
                                                  sli_zigbee_gp_tc_response_t *resp)
{
  uint8_t key[SL_ZIGBEE_ENCRYPTION_KEY_SIZE];

  // No key in the Commissioning GPDF. These arms do not decrypt.
  if (!comm->gpdKeyPresent) {
    if (policy->sharedSecurityKeyType == 0U) {
      sli_gp_tc_respond_switch_to_ic_tclk(resp, req, comm);
      return;
    }
    // Deliver the shared key encrypted with the IC-based TCLK at FC+1:
    // KeyType 0b101, ICEncryptedKeyType = shared key type.
    (void) sli_gp_tc_respond_encrypted_shared_key(resp, policy, req, comm,
                                                  icLinkKey,
                                                  SL_ZIGBEE_GP_TC_RESPONSE_KEY_TYPE_KEY_TYPE_ELSEWHERE,
                                                  policy->sharedSecurityKeyType);
    return;
  }

  // A key is present: IC-based TCLK used is checked before the decrypt.
  if (!comm->icTclkUsed) {
    sli_gp_tc_init_response(resp, req, comm, SL_ZIGBEE_GP_TC_RESPONSE_STATUS_ACTION_DENIED);
    return;
  }
  if (!sli_gp_tc_key_type_is_individual_or_shared(comm->securityKeyType,
                                                  policy->sharedSecurityKeyType)) {
    sli_gp_tc_init_response(resp, req, comm, SL_ZIGBEE_GP_TC_RESPONSE_STATUS_DROP);
    return;
  }
  if (!sli_gp_tc_received_key_decrypts_with(req, comm, icLinkKey, key)) {
    sli_gp_tc_init_response(resp, req, comm, SL_ZIGBEE_GP_TC_RESPONSE_STATUS_FAILURE);
    return;
  }
  sli_gp_tc_respond_in_clear(resp, req, comm, comm->securityKeyType, key);
}

// ---------------------------------------------------------------------------
// Figure 124: unidirectional commissioning, IC mode optional
// ---------------------------------------------------------------------------

static void sli_gp_tc_evaluate_unidir_ic_optional(const sli_zigbee_gp_tc_policy_t *policy,
                                                  const sli_zigbee_gp_tc_request_context_t *req,
                                                  const sli_gp_tc_commissioning_info_t *comm,
                                                  const sl_zigbee_key_data_t *icLinkKey,
                                                  bool icLinkKeyAvailable,
                                                  sli_zigbee_gp_tc_response_t *resp)
{
  const sl_zigbee_key_data_t *linkKey = NULL;
  uint8_t key[SL_ZIGBEE_ENCRYPTION_KEY_SIZE];

  if (!sli_gp_tc_key_type_is_individual_or_shared(comm->securityKeyType,
                                                  policy->sharedSecurityKeyType)) {
    sli_gp_tc_init_response(resp, req, comm, SL_ZIGBEE_GP_TC_RESPONSE_STATUS_DROP);
    return;
  }
  if (!sli_gp_tc_select_optional_link_key(policy, req, comm, icLinkKey,
                                          icLinkKeyAvailable, &linkKey, resp)) {
    return;
  }
  if (!sli_gp_tc_received_key_decrypts_with(req, comm, linkKey, key)) {
    sli_gp_tc_init_response(resp, req, comm, SL_ZIGBEE_GP_TC_RESPONSE_STATUS_FAILURE);
    return;
  }
  sli_gp_tc_respond_in_clear(resp, req, comm, comm->securityKeyType, key);
}

// ---------------------------------------------------------------------------
// Figure 125: bidirectional commissioning, IC mode optional
// ---------------------------------------------------------------------------

static void sli_gp_tc_evaluate_bidir_ic_optional(const sli_zigbee_gp_tc_policy_t *policy,
                                                 const sli_zigbee_gp_tc_request_context_t *req,
                                                 const sli_gp_tc_commissioning_info_t *comm,
                                                 const sl_zigbee_key_data_t *icLinkKey,
                                                 bool icLinkKeyAvailable,
                                                 sli_zigbee_gp_tc_response_t *resp)
{
  const sl_zigbee_key_data_t *linkKey = NULL;
  uint8_t key[SL_ZIGBEE_ENCRYPTION_KEY_SIZE];

  // Wrapping / decrypt key: IC-based TCLK used and apsDeviceKeyPairSet.
  if (!sli_gp_tc_select_optional_link_key(policy, req, comm, icLinkKey,
                                          icLinkKeyAvailable, &linkKey, resp)) {
    return;
  }

  if (!comm->gpdKeyPresent) {
    if (policy->sharedSecurityKeyType == 0U) {
      // Shared type 0 is not a Drop. No shared key to deliver and no received
      // key to decrypt: the GPD keeps an individual key.
      sli_gp_tc_init_response(resp, req, comm, SL_ZIGBEE_GP_TC_RESPONSE_STATUS_SUCCESS);
      resp->keyType = SL_ZIGBEE_GP_SECURITY_KEY_GPD_OOB;
      resp->icEncryptedKeyType = SL_ZIGBEE_GP_IC_ENCRYPTED_KEY_TYPE_NONE;
      return;
    }
    // No key in the request: deliver the shared key encrypted at FC+1 with
    // the link key selected above. KeyType = shared type, ICEncryptedKeyType 0.
    (void) sli_gp_tc_respond_encrypted_shared_key(resp, policy, req, comm,
                                                  linkKey,
                                                  policy->sharedSecurityKeyType,
                                                  SL_ZIGBEE_GP_IC_ENCRYPTED_KEY_TYPE_NONE);
    return;
  }

  if (!sli_gp_tc_key_type_is_individual_or_shared(comm->securityKeyType,
                                                  policy->sharedSecurityKeyType)) {
    sli_gp_tc_init_response(resp, req, comm, SL_ZIGBEE_GP_TC_RESPONSE_STATUS_DROP);
    return;
  }
  if (!sli_gp_tc_received_key_decrypts_with(req, comm, linkKey, key)) {
    sli_gp_tc_init_response(resp, req, comm, SL_ZIGBEE_GP_TC_RESPONSE_STATUS_FAILURE);
    return;
  }
  sli_gp_tc_respond_in_clear(resp, req, comm, comm->securityKeyType, key);
}

// ---------------------------------------------------------------------------
// Figure 120: GP TC Request evaluation entry point
// ---------------------------------------------------------------------------

sl_status_t sli_zigbee_gp_tc_evaluate_request(const sli_zigbee_gp_tc_policy_t *policy,
                                              sli_zigbee_gp_tc_request_context_t *req,
                                              sli_zigbee_gp_tc_response_t *resp)
{
  if (policy == NULL || req == NULL || resp == NULL) {
    return SL_STATUS_INVALID_PARAMETER;
  }

  sli_gp_tc_init_response(resp, req, NULL, SL_ZIGBEE_GP_TC_RESPONSE_STATUS_DROP);

  // Fig 120: gpTC_Policy_AllowGPD FALSE -> Drop.
  if (!policy->allowGpd) {
    return SL_STATUS_OK;
  }

  sli_gp_tc_commissioning_info_t comm;
  if (sli_gp_tc_load_commissioning_info(req, &comm) != SL_STATUS_OK) {
    // Malformed Commissioning GPDF payload: caller answers with a
    // ZCL Default Response (MALFORMED_COMMAND).
    return SL_STATUS_INVALID_PARAMETER;
  }
  resp->securityLevel = comm.securityLevel;
  resp->keyType = comm.securityKeyType;

  // Fig 120: policy SecurityLevel above the received capabilities -> Drop.
  if (policy->minSecurityLevel > comm.securityLevel) {
    return SL_STATUS_OK;
  }
  // Fig 120: KeyType 0b110 -> Figure 121, including a Commissioning GPDF
  // that carries no key (that arm is FAILURE, not the key-present Drop).
  if (comm.securityKeyType == SL_ZIGBEE_GP_SECURITY_KEY_OOB_V113) {
    sli_gp_tc_evaluate_oob_v113(req, &comm, resp);
    return SL_STATUS_OK;
  }

  const bool keyRequest = ((comm.gpdfOptions
                            & SL_ZIGBEE_AF_GP_GPD_COMMISSIONING_OPTIONS_GP_SECURITY_KEY_REQUEST) != 0U);
  // Unidirectional: a missing or unencrypted key is a Drop (Figs 122/124).
  // A key request with no GPD key continues into Figs 123/125.
  // A key that is present but not encrypted is a Drop on either path.
  if (!keyRequest && (!comm.gpdKeyPresent || !comm.gpdKeyEncrypted)) {
    return SL_STATUS_OK;
  }
  if (keyRequest && comm.gpdKeyPresent && !comm.gpdKeyEncrypted) {
    return SL_STATUS_OK;
  }
  sl_zigbee_key_data_t icLinkKey = { { 0 } };
  const bool icLinkKeyAvailable = (sl_zigbee_gp_lookup_ic_derived_key(&req->gpdAddr, &icLinkKey) == SL_STATUS_OK);

  if (policy->icModeMandatory) {
    // Fig 120 (IC mode TRUE): IC-based TCLK supported 0b0 -> Drop;
    // GPD not in apsDeviceKeyPairSet -> Drop.
    if (!comm.icTclkSupported || !icLinkKeyAvailable) {
      return SL_STATUS_OK;
    }
    if (keyRequest) {
      sli_gp_tc_evaluate_bidir_ic_mandatory(policy, req, &comm, &icLinkKey, resp);
    } else {
      sli_gp_tc_evaluate_unidir_ic_mandatory(policy, req, &comm, &icLinkKey, resp);
    }
    return SL_STATUS_OK;
  }

  // Fig 120 (IC mode FALSE).
  if (keyRequest) {
    sli_gp_tc_evaluate_bidir_ic_optional(policy, req, &comm, &icLinkKey, icLinkKeyAvailable, resp);
  } else {
    sli_gp_tc_evaluate_unidir_ic_optional(policy, req, &comm, &icLinkKey, icLinkKeyAvailable, resp);
  }
  return SL_STATUS_OK;
}
