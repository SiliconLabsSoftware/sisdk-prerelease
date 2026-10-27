/***************************************************************************//**
 * @file
 * @brief GP v1.1.3 hook implementations for Green Power Server integration.
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
#include "green-power-server.h"
#include "green-power-v113.h"
#include "stack/include/zigbee-security-manager.h"

static void import_load_zb_sec_man_internal_key(sl_zigbee_sec_man_key_t *key)
{
  sl_zigbee_sec_man_context_t context;
  sl_zigbee_sec_man_init_context(&context);
  context.core_key_type = SL_ZB_SEC_MAN_KEY_TYPE_INTERNAL;
  sl_zigbee_sec_man_import_key(&context, key);
  sl_zigbee_sec_man_load_key_context(&context);
}

static bool sli_gp_v113_involve_tc_set(void)
{
  uint8_t gpsSecurityLevel = 0;
  sl_zigbee_af_attribute_type_t attrType;
  sl_zigbee_af_status_t attrStatus = sl_zigbee_af_read_attribute(SL_ZIGBEE_GP_ENDPOINT,
                                                                 ZCL_GREEN_POWER_CLUSTER_ID,
                                                                 ZCL_GP_SERVER_GPS_SECURITY_LEVEL_ATTRIBUTE_ID,
                                                                 CLUSTER_MASK_SERVER,
                                                                 &gpsSecurityLevel,
                                                                 sizeof(gpsSecurityLevel),
                                                                 &attrType);
  return (attrStatus == SL_ZIGBEE_ZCL_STATUS_SUCCESS
          && (gpsSecurityLevel & GREEN_POWER_SERVER_GPS_SECURITY_LEVEL_ATTRIBUTE_FIELD_INVOLVE_TC) != 0U);
}

bool sli_gp_v113_commissioning_mode_involve_tc_ok(void)
{
  return sli_zigbee_af_gp_is_tc_aware_and_capable();
}

void sli_gp_v113_update_involve_tc_link_key(sl_zigbee_key_data_t *linkKeyOut,
                                             sl_zigbee_gp_link_key_source_t *linkKeySourceOut)
{
  sl_zigbee_af_gp_resolve_link_key(NULL,
                                    SL_ZIGBEE_GP_LINK_KEY_SOURCE_TC_LINK,
                                    linkKeyOut,
                                    linkKeySourceOut);
}

void sli_gp_v113_on_decommission(const sl_zigbee_gp_address_t *gpdAddr,
                                   uint8_t gpsSecurityLevelAttribute)
{
  if (gpdAddr != NULL
      && (gpsSecurityLevelAttribute & GREEN_POWER_SERVER_GPS_SECURITY_LEVEL_ATTRIBUTE_FIELD_INVOLVE_TC)) {
    sli_zigbee_af_gp_send_tc_decommissioning_notification(gpdAddr);
  }
}

bool sli_gp_v113_apply_commissioning_reply_override(sli_zigbee_gp_comm_data_saved_t *commissioningGpd,
                                                     uint8_t *sendSecurityKeyType,
                                                     bool *sendKeyinReply,
                                                     bool *sendKeyEncryption,
                                                     sl_zigbee_key_data_t *sendKey)
{
  sl_zigbee_af_green_power_server_commissioning_state_t *commissioningState
    = sl_zigbee_af_green_power_cluster_get_server_commissioning_sate();

  if (commissioningState == NULL || !commissioningState->tcMediated) {
    return false;
  }

  const sli_zigbee_gp_tc_response_t *tcResp = sli_zigbee_af_gp_get_tc_response();
  if (tcResp == NULL || tcResp->status != SL_ZIGBEE_GP_TC_RESPONSE_STATUS_SUCCESS) {
    return false;
  }

  // The TC Response (Figures 121-125) fully determines the Commissioning
  // Reply key fields; the sink never re-encrypts a TC-delivered key.
  commissioningGpd->tcReplyKeyPreEncrypted = false;
  commissioningGpd->tcResponseKeyType = tcResp->keyType;
  commissioningGpd->tcReplyIcEncryptedKeyType = tcResp->icEncryptedKeyType;
  commissioningGpd->securityLevel = tcResp->securityLevel;

  // Table 63: KeyType 0b101 is kept on the air; the actual type travels in the
  // Security Information field. 0b101 + ICEncryptedKeyType 0b000 means "no
  // key, continue with the IC-based TCLK" and the GPD keeps its own key type.
  *sendSecurityKeyType = tcResp->keyType;
  if (tcResp->keyType == SL_ZIGBEE_GP_SECURITY_KEY_KEY_TYPE_ELSEWHERE) {
    if (tcResp->icEncryptedKeyType != SL_ZIGBEE_GP_IC_ENCRYPTED_KEY_TYPE_NONE) {
      commissioningGpd->securityKeyType = tcResp->icEncryptedKeyType;
    }
  } else {
    commissioningGpd->securityKeyType = tcResp->keyType;
  }

  // The key in clear is what the sink table operates with.
  if (tcResp->inClearGpdSecurityKeyPresent) {
    memcpy(commissioningGpd->key.contents,
           tcResp->inClearGpdSecurityKey.contents,
           SL_ZIGBEE_ENCRYPTION_KEY_SIZE);
  }

  if (tcResp->gpdSecurityKeyPresent) {
    *sendKeyinReply = true;
    *sendKeyEncryption = tcResp->gpdKeyEncryption;
    memcpy(sendKey->contents,
           tcResp->gpdSecurityKey.contents,
           SL_ZIGBEE_ENCRYPTION_KEY_SIZE);
    if (tcResp->gpdKeyEncryption) {
      // Figures 123/125: key already encrypted by the TC at FC+1 with MIC.
      sl_util_store_low_high_int32u(commissioningGpd->tcReplyKeyMic, tcResp->mic);
      commissioningGpd->tcReplyKeyFrameCounter = tcResp->frameCounter;
      commissioningGpd->tcReplyKeyPreEncrypted = true;
    }
  } else {
    *sendKeyinReply = false;
    *sendKeyEncryption = false;
  }

  return true;
}

void sli_gp_v113_on_pairing_success(sli_zigbee_gp_comm_data_saved_t *commissioningGpd,
                                     sl_zigbee_af_green_power_server_commissioning_state_t *commissioningState)
{
  if (commissioningState != NULL && commissioningState->tcMediated) {
    sli_zigbee_af_gp_send_tc_pairing_completion(&(commissioningGpd->addr));
    commissioningState->tcMediated = false;
  }
  sli_zigbee_af_gp_tc_forget_response(&(commissioningGpd->addr));
}

void sli_gp_v113_on_commissioning_instance_deleted(const sl_zigbee_gp_address_t *gpdAddr)
{
  // Session over (pairing done, abort, or commissioning window timeout):
  // a later Commissioning GPDF from this GPD goes to the TC again.
  sli_zigbee_af_gp_tc_forget_response(gpdAddr);
}

bool sli_gp_v113_involve_tc_commissioning_cn_drop_early(bool involveTcSet,
                                                         uint16_t commNotificationOptions)
{
  if (!involveTcSet) {
    return false;
  }
  // A.3.9.1: only an unprotected Commissioning GPDF (SecurityLevel 0b00 and
  // SecurityProcessingFailed 0b0) is forwarded to the GP-aware TC.
  const uint8_t securityLevel = (uint8_t)((commNotificationOptions
                                           & SL_ZIGBEE_AF_GP_COMMISSIONING_NOTIFICATION_OPTION_SECURITY_LEVEL)
                                          >> SL_ZIGBEE_AF_GP_COMMISSIONING_NOTIFICATION_OPTION_SECURITY_LEVEL_OFFSET);
  const bool securityProcessingFailed = ((commNotificationOptions
                                          & SL_ZIGBEE_AF_GP_COMMISSIONING_NOTIFICATION_OPTION_SECURITY_PROCESSING_FAILED) != 0U);
  return (securityLevel != 0U) || securityProcessingFailed;
}

int8_t sli_gp_v113_check_encrypted_commissioning_key(uint8_t securityKeyType,
                                                      sl_zigbee_gp_address_t *gpdAddr,
                                                      sl_zigbee_key_data_t *key,
                                                      sli_zigbee_gp_comm_data_saved_t *commissioningGpd)
{
  (void)securityKeyType;
  (void)gpdAddr;
  (void)key;
  (void)commissioningGpd;
  // OOB 0x06 verification runs in sli_gp_v113_process_encrypted_commissioning_key
  // where the payload MIC is available.
  return 0;
}

bool sli_gp_v113_handle_involve_tc_commissioning(uint16_t commNotificationOptions,
                                                  uint8_t gpdCommandId,
                                                  uint8_t *gpdCommandPayload,
                                                  sli_zigbee_gp_comm_data_saved_t *commissioningGpd,
                                                  sl_zigbee_af_green_power_server_commissioning_state_t *commissioningState)
{
  if (!sli_zigbee_af_gp_is_tc_aware_and_capable()) {
    sl_zigbee_af_green_power_cluster_println("DROP - GP CN : Involve TC set but GP-aware TC not available!");
    return true;
  }

  // A GPD repeats its Commissioning GPDF (RxAfterTx) until the Commissioning
  // Reply arrives, and a combo also hears the GP Commissioning Notification
  // for a GPDF it already processed. Only one GpTcRequest per session:
  // - Request still in flight: keep waiting for GpTcResponse.
  // - Same Commissioning GPDF already answered by the TC: reuse that answer
  //   and continue (re-queue the Commissioning Reply / finalise).
  {
    const bool samePayload = (gpdCommandPayload != NULL
                              && commissioningGpd->gpdCommandPayloadLen > 0U
                              && commissioningGpd->gpdCommandPayloadLen == (uint8_t)(gpdCommandPayload[0] + 1U)
                              && memcmp(commissioningGpd->gpdCommandPayload,
                                        gpdCommandPayload,
                                        commissioningGpd->gpdCommandPayloadLen) == 0);
    if (samePayload && sli_zigbee_af_gp_tc_request_in_flight(commissioningGpd)) {
      sl_zigbee_af_green_power_cluster_println("GP CN : GP TC Request pending - duplicate Commissioning GPDF ignored");
      commissioningGpd->commissionState = GP_SINK_COMM_STATE_AWAITING_TC_RESPONSE;
      return true;
    }
    if (samePayload
        && commissioningState->tcMediated
        && sli_zigbee_af_gp_tc_reuse_response(commissioningGpd)) {
      sl_zigbee_af_green_power_cluster_println("GP CN : Reusing GP TC Response for repeated Commissioning GPDF");
      commissioningGpd->commNotificationOptions = commNotificationOptions;
      commissioningState->awaitingTcResponse = false;
      return false; // commissionState already set by the caller; finalise.
    }
  }

  commissioningGpd->commNotificationOptions = commNotificationOptions;
  commissioningGpd->gpdCommandId = gpdCommandId;
  if (gpdCommandPayload != NULL && gpdCommandPayload[0] > 0U
      && (gpdCommandPayload[0] + 1U) <= sizeof(commissioningGpd->gpdCommandPayload)) {
    commissioningGpd->gpdCommandPayloadLen = gpdCommandPayload[0] + 1U;
    memcpy(commissioningGpd->gpdCommandPayload, gpdCommandPayload, gpdCommandPayload[0] + 1U);
  }

  commissioningState->tcMediated = true;
  commissioningState->awaitingTcResponse = true;
  commissioningGpd->commissionState = GP_SINK_COMM_STATE_AWAITING_TC_RESPONSE;
  {
    uint8_t payloadLen = 0U;

    if (gpdCommandPayload != NULL) {
      payloadLen = gpdCommandPayload[0];
    }
    sli_zigbee_af_gp_send_tc_request(commissioningGpd, gpdCommandPayload, payloadLen);
  }
  return true;
}

bool sli_gp_v113_process_encrypted_commissioning_key(uint8_t securityKeyType,
                                                      sl_zigbee_gp_address_t *gpdAddr,
                                                      uint8_t *gpdCommandPayload,
                                                      uint8_t *index,
                                                      sl_zigbee_key_data_t *key,
                                                      sli_zigbee_gp_comm_data_saved_t *commissioningGpd)
{
  sl_zigbee_key_data_t linkKey = { { 0 } };
  sl_zigbee_gp_link_key_source_t linkKeySource = SL_ZIGBEE_GP_LINK_KEY_SOURCE_DEFAULT_TCLK;
  uint8_t mic[4] = { 0 };

  const bool involveTc = sli_gp_v113_involve_tc_set();

  if (securityKeyType == SL_ZIGBEE_GP_SECURITY_KEY_OOB_V113) {
    sl_zigbee_key_data_t oobKey = { { 0 } };
    const uint8_t *rxMic;

    if (involveTc || sl_zigbee_af_gp_lookup_oob_key(gpdAddr, &oobKey) != SL_STATUS_OK) {
      // Figure 121: with Involve TC the GP-aware TC verifies the signature
      // (it may be the only node holding the OOB key). Keep the encrypted
      // signature in the payload and let GpTcRequest carry it.
      if (gpdCommandPayload == NULL || index == NULL) {
        return true;
      }
      *index += 4;
      sl_zigbee_af_green_power_cluster_println("GP CN : OOB signature deferred to GP-aware TC");
      return false;
    }
    if (gpdCommandPayload == NULL || index == NULL || key == NULL || commissioningGpd == NULL) {
      return true;
    }
    rxMic = gpdCommandPayload + *index;
    *index += 4;
    if (sl_zigbee_af_gp_oob_verify_signature(gpdAddr,
                                             oobKey.contents,
                                             key->contents,
                                             rxMic) != SL_STATUS_OK) {
      sl_zigbee_af_green_power_cluster_println("DROP - GP CN : OOB v1.1.3 signature verify failed!");
      return true;
    }
    // The 16-byte field is the encrypted signature, not the operational key.
    // Sink table and later GPDF MIC checks use the pre-configured OOB key.
    memcpy(key->contents, oobKey.contents, SL_ZIGBEE_ENCRYPTION_KEY_SIZE);
    commissioningGpd->oobSignatureVerified = true;
    return false;
  }

  sli_gp_v113_resolve_link_key_for_decrypt(gpdAddr, &linkKey, &linkKeySource);
  import_load_zb_sec_man_internal_key((sl_zigbee_sec_man_key_t *)&linkKey);
  {
    uint32_t keyMic = sl_util_fetch_low_high_int32u(gpdCommandPayload + *index);
    *index += 4;
    sli_zigbee_af_gp_key_tc_lk_derivation(gpdAddr,
                                          0,
                                          mic,
                                          key,
                                          true);
    sl_zigbee_af_green_power_cluster_println("rx MIC %08X", keyMic);
    sl_zigbee_af_green_power_cluster_print("Calculated MIC :");
    sl_zigbee_af_green_power_cluster_print_buffer(mic, 4, 0);
    sl_zigbee_af_green_power_cluster_print("\nKey : ");
    sl_zigbee_af_green_power_cluster_print_buffer(key->contents, 16, 0);
    sl_zigbee_af_green_power_cluster_println("");
    if (0 != memcmp(mic, (uint8_t *)(&keyMic), 4)) {
      // Figures 122-125: the sink may not hold the IC-based TCLK. Forward the
      // ciphertext to the GP-aware TC, which answers with the key in clear.
      if (involveTc) {
        sl_zigbee_af_green_power_cluster_println("GP CN : supplied key MIC not local; defer to GP-aware TC");
        return false;
      }
      sl_zigbee_af_green_power_cluster_println("DROP - GP CN : Supplied Key MIC mismatch");
      return true;
    }
  }
  return false;
}

void sli_gp_v113_server_init(uint8_t initLevel)
{
  if (initLevel == SL_ZIGBEE_INIT_LEVEL_EVENT) {
    sl_zigbee_af_green_power_server_tc_client_init_cb(initLevel);
  } else if (initLevel == SL_ZIGBEE_INIT_LEVEL_DONE) {
    sli_zigbee_af_gp_oob_key_table_init();
    uint16_t clusterRevision = SL_ZIGBEE_GP_CLUSTER_REVISION_V113;

    (void) sl_zigbee_af_write_attribute(SL_ZIGBEE_GP_ENDPOINT,
                                        ZCL_GREEN_POWER_CLUSTER_ID,
                                        ZCL_GREEN_POWER_CLUSTER_CLUSTER_REVISION_SERVER_ATTRIBUTE_ID,
                                        CLUSTER_MASK_SERVER,
                                        (uint8_t *)&clusterRevision,
                                        ZCL_INT16U_ATTRIBUTE_TYPE);
  }
}

void sli_gp_v113_resolve_link_key_for_decrypt(const sl_zigbee_gp_address_t *gpdAddr,
                                               sl_zigbee_key_data_t *linkKeyOut,
                                               sl_zigbee_gp_link_key_source_t *linkKeySourceOut)
{
  sl_zigbee_af_gp_resolve_link_key(gpdAddr,
                                    SL_ZIGBEE_GP_LINK_KEY_SOURCE_IC_DERIVED,
                                    linkKeyOut,
                                    linkKeySourceOut);
}

void sli_gp_v113_resolve_link_key(const sl_zigbee_gp_address_t *gpdAddr,
                                   sl_zigbee_gp_link_key_source_t preferredSource,
                                   sl_zigbee_key_data_t *linkKeyOut,
                                   sl_zigbee_gp_link_key_source_t *linkKeySourceOut)
{
  sl_zigbee_af_gp_resolve_link_key(gpdAddr, preferredSource, linkKeyOut, linkKeySourceOut);
}

// Weak no-op so v113 CLI links without zigbee_green_power_tc; TC overrides.
SL_WEAK void sli_zigbee_gp_tc_set_ic_mode_mandatory(bool mandatory)
{
  (void)mandatory;
}
