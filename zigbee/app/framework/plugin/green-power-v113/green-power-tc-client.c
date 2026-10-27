/***************************************************************************//**
 * @file
 * @brief Sink-side GP TC client for GP v1.1.3 TC-mediated commissioning.
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
#include "green-power-common.h"
#include "green-power-server.h"
#include "green-power-v113.h"
#include "green-power-tc-msg.h"
#include "green-power-v113-config.h"
#include "green-power-server-config.h"

static sli_zigbee_gp_comm_data_saved_t *sli_gpTcPendingCommissioning = NULL;
static sli_zigbee_gp_tc_response_t sli_gpTcLastResponse = { 0 };
// GPD the held SUCCESS response belongs to (valid while a commissioning
// session for that GPD is open).
static bool sli_gpTcLastResponseValid = false;
static sl_zigbee_gp_address_t sli_gpTcLastResponseAddr = { 0 };
static uint8_t sli_gpTcRequestRetries = 0;
#ifdef SL_CATALOG_ZIGBEE_GREEN_POWER_TC_PRESENT
static uint16_t sli_gpTcClusterRevision = 0;
#endif
static sl_zigbee_af_event_t sl_zigbee_af_green_power_server_tc_request_timeout_event;
#define tcRequestTimeout (&sl_zigbee_af_green_power_server_tc_request_timeout_event)

void sl_zigbee_af_green_power_server_tc_request_timeout_event_handler(sl_zigbee_af_event_t *event)
{
  (void)event;

  if (sli_gpTcPendingCommissioning == NULL) {
    return;
  }

  // Commissioning instance reset (window timeout / mode exit) while waiting.
  if (sli_gpTcPendingCommissioning->commissionState != GP_SINK_COMM_STATE_AWAITING_TC_RESPONSE) {
    sli_gpTcPendingCommissioning = NULL;
    sli_gpTcRequestRetries = 0;
    return;
  }

  if (sli_gpTcRequestRetries < SL_ZIGBEE_GP_TC_REQUEST_RETRY_MAX) {
    sli_gpTcRequestRetries++;
    sli_zigbee_af_gp_send_tc_request(sli_gpTcPendingCommissioning, NULL, 0);
    sl_zigbee_af_event_set_delay_ms(tcRequestTimeout, SL_ZIGBEE_GP_TC_REQUEST_TIMEOUT_MS);
  } else {
    sl_zigbee_af_green_power_cluster_println("GP TC Request timeout - abort commissioning");
    sli_zigbee_gp_comm_data_saved_t *aborted = sli_gpTcPendingCommissioning;
    sli_gpTcPendingCommissioning = NULL;
    sli_gpTcRequestRetries = 0;
    sl_zigbee_af_green_power_server_delete_commissioning_gpd_instance(&aborted->addr);
  }
}

bool sli_zigbee_af_gp_tc_request_in_flight(const sli_zigbee_gp_comm_data_saved_t *commissioningGpd)
{
  return (commissioningGpd != NULL
          && sli_gpTcPendingCommissioning == commissioningGpd
          && sl_zigbee_af_event_is_scheduled(tcRequestTimeout));
}

void sli_zigbee_af_gp_tc_forget_response(const sl_zigbee_gp_address_t *gpdAddr)
{
  if (sli_gpTcLastResponseValid
      && (gpdAddr == NULL
          || sl_zigbee_af_green_power_common_gp_addr_compare(gpdAddr, &sli_gpTcLastResponseAddr))) {
    sli_gpTcLastResponseValid = false;
  }
}

// Apply a SUCCESS GpTcResponse to the commissioning GPD (Figures 121-125):
// key type / level from the Response, the key in clear fills the sink table and
// never goes on the air in the Commissioning Reply.
static void sli_gp_tc_apply_response(sli_zigbee_gp_comm_data_saved_t *commissioningGpd,
                                     const sli_zigbee_gp_tc_response_t *response)
{
  // Paragraphs 5050-5053: KeyType 0b101 means the actual key type is the
  // ICEncryptedKeyType and the key (if any) is protected with the IC-based TCLK.
  commissioningGpd->tcResponseKeyType = response->keyType;
  commissioningGpd->tcReplyIcEncryptedKeyType = response->icEncryptedKeyType;
  if (response->keyType == SL_ZIGBEE_GP_SECURITY_KEY_KEY_TYPE_ELSEWHERE) {
    commissioningGpd->linkKeySourceUsed = SL_ZIGBEE_GP_LINK_KEY_SOURCE_IC_DERIVED;
    if (response->icEncryptedKeyType != SL_ZIGBEE_GP_IC_ENCRYPTED_KEY_TYPE_NONE) {
      commissioningGpd->securityKeyType = response->icEncryptedKeyType;
    }
  } else {
    commissioningGpd->securityKeyType = response->keyType;
  }
  commissioningGpd->securityLevel = response->securityLevel;

  if (response->inClearGpdSecurityKeyPresent) {
    memcpy(commissioningGpd->key.contents,
           response->inClearGpdSecurityKey.contents,
           SL_ZIGBEE_ENCRYPTION_KEY_SIZE);
    commissioningGpd->gpdfExtendedOptions
      |= SL_ZIGBEE_AF_GP_GPD_COMMISSIONING_EXTENDED_OPTIONS_GPD_KEY_PRESENT;
    if (response->securityLevel == 0x02U || response->securityLevel == 0x03U) {
      commissioningGpd->gpdfExtendedOptions
        |= SL_ZIGBEE_AF_GP_GPD_COMMISSIONING_EXTENDED_OPTIONS_GPD_KEY_ENCRYPTION;
    }
  }
}

bool sli_zigbee_af_gp_tc_reuse_response(sli_zigbee_gp_comm_data_saved_t *commissioningGpd)
{
  if (commissioningGpd == NULL
      || !sli_gpTcLastResponseValid
      || sli_gpTcLastResponse.status != SL_ZIGBEE_GP_TC_RESPONSE_STATUS_SUCCESS
      || !sl_zigbee_af_green_power_common_gp_addr_compare(&commissioningGpd->addr,
                                                          &sli_gpTcLastResponseAddr)) {
    return false;
  }
  sli_gp_tc_apply_response(commissioningGpd, &sli_gpTcLastResponse);
  return true;
}

bool sli_zigbee_af_gp_is_tc_aware_and_capable(void)
{
  // v113 InvolveTC client (this TU) can always unicast GpTcRequest to 0x0000.
  // Co-located or remote GP TC capability is confirmed by GpTcResponse success
  // (see remote-TC multi-hop BSH). Optional local revision cache for debug.
#ifdef SL_CATALOG_ZIGBEE_GREEN_POWER_TC_PRESENT
  if (sli_gpTcClusterRevision < SL_ZIGBEE_GP_CLUSTER_REVISION_V113) {
    sl_zigbee_af_attribute_type_t type;
    uint16_t revision = 0;
    if (sl_zigbee_af_read_attribute(SL_ZIGBEE_GP_ENDPOINT,
                                    ZCL_GREEN_POWER_CLUSTER_ID,
                                    ZCL_GREEN_POWER_CLUSTER_CLUSTER_REVISION_SERVER_ATTRIBUTE_ID,
                                    CLUSTER_MASK_SERVER,
                                    (uint8_t *)&revision,
                                    sizeof(revision),
                                    &type) == SL_ZIGBEE_ZCL_STATUS_SUCCESS) {
      sli_gpTcClusterRevision = revision;
    }
  }
#endif
  return true;
}

static sl_status_t sli_gp_tc_send_payload(uint8_t commandId,
                                           const uint8_t *payload,
                                           uint8_t payloadLen)
{
  // Table 41: sink generates these on the GP server side (server→client).
  // Pairing / Decommissioning expect a ZCL Default Response; Request uses
  // GpTcResponse as its reply so Default Response is disabled.
  uint8_t frameControl = (uint8_t)(ZCL_CLUSTER_SPECIFIC_COMMAND
                                   | ZCL_FRAME_CONTROL_SERVER_TO_CLIENT);
  if (commandId == ZCL_GP_TC_REQUEST_COMMAND_ID) {
    frameControl |= ZCL_DISABLE_DEFAULT_RESPONSE_MASK;
  }

  sl_zigbee_af_fill_external_buffer(frameControl,
                                    ZCL_GREEN_POWER_CLUSTER_ID,
                                    commandId,
                                    "b",
                                    payload,
                                    payloadLen);

  sl_zigbee_aps_frame_t *apsFrame = sl_zigbee_af_get_command_aps_frame();
  apsFrame->sourceEndpoint = SL_ZIGBEE_GP_ENDPOINT;
  apsFrame->destinationEndpoint = SL_ZIGBEE_GP_ENDPOINT;

  return sl_zigbee_af_send_command_unicast(SL_ZIGBEE_OUTGOING_DIRECT, 0x0000);
}

static void sli_gp_tc_populate_request_context(sli_zigbee_gp_comm_data_saved_t *commissioningGpd,
                                                sli_zigbee_gp_tc_request_context_t *req)
{
  sl_zigbee_key_data_t oobKey = { { 0 } };

  memset(req, 0, sizeof(sli_zigbee_gp_tc_request_context_t));
  req->gpdAddr = commissioningGpd->addr;
  // Not on the Request wire (Figure 71); retained for local TC-side decrypt when
  // the commissioning payload has no outgoing counter.
  req->frameCounter = commissioningGpd->outgoingFrameCounter;
  req->securityLevel = commissioningGpd->securityLevel;
  req->securityKeyType = commissioningGpd->securityKeyType;
  req->gpdCommandId = commissioningGpd->gpdCommandId;
  req->options = (uint8_t)(commissioningGpd->addr.applicationId
                           & SL_ZIGBEE_GP_TC_REQUEST_OPTION_APPLICATION_ID);

  if (commissioningGpd->gpdCommandPayloadLen > 1U) {
    req->gpdCommandPayloadLen = commissioningGpd->gpdCommandPayload[0];
    if (req->gpdCommandPayloadLen > 0U
        && (req->gpdCommandPayloadLen + 1U) <= commissioningGpd->gpdCommandPayloadLen) {
      memcpy(req->gpdCommandPayload,
             commissioningGpd->gpdCommandPayload + 1,
             req->gpdCommandPayloadLen);
    }
  }

  // Figure 71/72: optional OOB GPD key when the sink has one configured.
  if (sl_zigbee_af_gp_lookup_oob_key(&commissioningGpd->addr, &oobKey) == SL_STATUS_OK) {
    memcpy(req->gpdSecurityKey.contents, oobKey.contents, SL_ZIGBEE_ENCRYPTION_KEY_SIZE);
    req->gpdSecurityKeyPresent = true;
    req->options |= SL_ZIGBEE_GP_TC_REQUEST_OPTION_OOB_KEY_PRESENT;
  }
}

sl_status_t sli_zigbee_af_gp_send_tc_request(sli_zigbee_gp_comm_data_saved_t *commissioningGpd,
                                              const uint8_t *gpdPayload,
                                              uint8_t payloadLen)
{
  sli_zigbee_gp_tc_request_context_t req = { 0 };
  uint8_t buffer[96];
  uint8_t bufferLen;

  if (commissioningGpd == NULL) {
    return SL_STATUS_NULL_POINTER;
  }

  if (gpdPayload != NULL && payloadLen > 0U
      && (payloadLen + 1U) <= sizeof(commissioningGpd->gpdCommandPayload)) {
    commissioningGpd->gpdCommandPayloadLen = payloadLen + 1U;
    memcpy(commissioningGpd->gpdCommandPayload, gpdPayload, payloadLen + 1U);
  }

  sli_gpTcPendingCommissioning = commissioningGpd;
  sli_gp_tc_populate_request_context(commissioningGpd, &req);
  bufferLen = sli_gp_tc_encode_request(&req, buffer, sizeof(buffer));
  sl_zigbee_af_event_set_delay_ms(tcRequestTimeout, SL_ZIGBEE_GP_TC_REQUEST_TIMEOUT_MS);

  return sli_gp_tc_send_payload(ZCL_GP_TC_REQUEST_COMMAND_ID, buffer, bufferLen);
}

void sli_zigbee_af_gp_tc_response_handler(const sli_zigbee_gp_tc_response_t *response)
{
  if (response == NULL || sli_gpTcPendingCommissioning == NULL) {
    return;
  }

  sl_zigbee_af_event_set_inactive(tcRequestTimeout);
  sli_gpTcRequestRetries = 0;

  // The commissioning instance may have been deleted (window timeout, abort)
  // while the Request was in flight; the memory is then zeroed and no longer
  // in the AWAITING_TC_RESPONSE state.
  if (sli_gpTcPendingCommissioning->commissionState != GP_SINK_COMM_STATE_AWAITING_TC_RESPONSE) {
    sl_zigbee_af_green_power_cluster_println("GP TC Response: no commissioning in progress - dropped");
    sli_gpTcPendingCommissioning = NULL;
    return;
  }

  memcpy(&sli_gpTcLastResponse, response, sizeof(sli_zigbee_gp_tc_response_t));
  sli_gpTcLastResponseAddr = sli_gpTcPendingCommissioning->addr;
  sli_gpTcLastResponseValid = (response->status == SL_ZIGBEE_GP_TC_RESPONSE_STATUS_SUCCESS);

  if (response->status != SL_ZIGBEE_GP_TC_RESPONSE_STATUS_SUCCESS) {
    // Table 45: FAILURE (key could not be verified/decrypted) and
    // ACTION_DENIED (policy requires IC-based TCLK) end this commissioning
    // attempt; the GPD has to re-commission.
    sl_zigbee_af_green_power_cluster_println("GP TC Response status %s (0x%X): commissioning aborted",
                                             (response->status == SL_ZIGBEE_GP_TC_RESPONSE_STATUS_ACTION_DENIED)
                                             ? "ACTION_DENIED" : "FAILURE",
                                             response->status);
    sli_zigbee_gp_comm_data_saved_t *aborted = sli_gpTcPendingCommissioning;
    sli_gpTcPendingCommissioning = NULL;
    sl_zigbee_af_green_power_server_delete_commissioning_gpd_instance(&aborted->addr);
    return;
  }

  sli_gp_tc_apply_response(sli_gpTcPendingCommissioning, response);

  if (response->inClearGpdSecurityKeyPresent) {
    sl_zigbee_af_green_power_cluster_print("GP TC Response InClear key: ");
    sl_zigbee_af_green_power_cluster_print_buffer(response->inClearGpdSecurityKey.contents,
                                                   SL_ZIGBEE_ENCRYPTION_KEY_SIZE,
                                                   true);
    sl_zigbee_af_green_power_cluster_println("");
  } else if (response->keyType == SL_ZIGBEE_GP_SECURITY_KEY_KEY_TYPE_ELSEWHERE
             && response->icEncryptedKeyType == SL_ZIGBEE_GP_IC_ENCRYPTED_KEY_TYPE_NONE) {
    // Figure 123: "switch to IC-based TCLK" - no key for this attempt; the
    // Commissioning Reply tells the GPD to re-commission with IC protection.
    sl_zigbee_af_green_power_cluster_println("GP TC Response: GPD to switch to IC-based TCLK");
  }
  // Figures 123/125: an encrypted GPDSecurityKey (with MIC and FC) is relayed
  // verbatim in the Commissioning Reply (see green-power-server-v113.c).
  // Restore the post-TC commissioning path: bidirectional → reply, else finalise.
  if ((sli_gpTcPendingCommissioning->commNotificationOptions
       & SL_ZIGBEE_AF_GP_COMMISSIONING_NOTIFICATION_OPTION_RX_AFTER_TX) != 0U) {
    sli_gpTcPendingCommissioning->commissionState = GP_SINK_COMM_STATE_SEND_COMM_REPLY;
  } else {
    sli_gpTcPendingCommissioning->commissionState = GP_SINK_COMM_STATE_FINALISE_PAIRING;
    sli_gpTcPendingCommissioning->preSinkCbSource = GP_PRE_SINK_PAIRING_CALLBACK_COMMISSONING_FINALIZE;
  }
  sli_zigbee_af_gp_continue_tc_mediated_commissioning(sli_gpTcPendingCommissioning);
  sli_gpTcPendingCommissioning = NULL;
}

const sli_zigbee_gp_tc_response_t *sli_zigbee_af_gp_get_tc_response(void)
{
  return &sli_gpTcLastResponse;
}

sl_status_t sli_zigbee_af_gp_send_tc_pairing_completion(const sl_zigbee_gp_address_t *gpdAddr)
{
  uint8_t buffer[24];
  uint8_t bufferLen;

  if (gpdAddr == NULL) {
    return SL_STATUS_NULL_POINTER;
  }

  bufferLen = sli_gp_tc_encode_notify((uint8_t)(gpdAddr->applicationId & 0x07U),
                                       gpdAddr,
                                       buffer,
                                       sizeof(buffer));
  return sli_gp_tc_send_payload(ZCL_GP_TC_PAIRING_COMPLETION_COMMAND_ID, buffer, bufferLen);
}

sl_status_t sli_zigbee_af_gp_send_tc_decommissioning_notification(const sl_zigbee_gp_address_t *gpdAddr)
{
  uint8_t buffer[24];
  uint8_t bufferLen;

  if (gpdAddr == NULL) {
    return SL_STATUS_NULL_POINTER;
  }

  bufferLen = sli_gp_tc_encode_notify((uint8_t)(gpdAddr->applicationId & 0x07U),
                                       gpdAddr,
                                       buffer,
                                       sizeof(buffer));
  return sli_gp_tc_send_payload(ZCL_GP_TC_DECOMMISSIONING_NOTIFICATION_COMMAND_ID, buffer, bufferLen);
}

sl_zigbee_af_zcl_request_status_t sl_zigbee_af_green_power_cluster_gp_tc_response_cb(sl_zigbee_af_cluster_command_t *cmd)
{
  sli_zigbee_gp_tc_response_t response = { 0 };
  sl_zigbee_gp_address_t gpdAddr = { 0 };

  if (cmd == NULL
      || cmd->bufLen <= cmd->payloadStartIndex
      || sli_gp_tc_parse_response(cmd->buffer + cmd->payloadStartIndex,
                                   (uint8_t)(cmd->bufLen - cmd->payloadStartIndex),
                                   &response,
                                   &gpdAddr) != SL_STATUS_OK) {
    return SL_ZIGBEE_ZCL_STATUS_MALFORMED_COMMAND;
  }

  sli_zigbee_af_gp_tc_response_handler(&response);
  return SL_ZIGBEE_ZCL_STATUS_SUCCESS;
}

void sl_zigbee_af_green_power_server_tc_client_init_cb(uint8_t init_level)
{
  if (init_level == SL_ZIGBEE_INIT_LEVEL_EVENT) {
    sl_zigbee_af_event_init(tcRequestTimeout,
                            sl_zigbee_af_green_power_server_tc_request_timeout_event_handler);
  }
}

