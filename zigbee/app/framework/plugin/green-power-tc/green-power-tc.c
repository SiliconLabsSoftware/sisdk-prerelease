/***************************************************************************//**
 * @file
 * @brief GP-aware Trust Center ZCL command handlers for GP v1.1.3.
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
#include "green-power-v113.h"
#include "stack/include/gp-install-code.h"
#include "green-power-common.h"

// GPDs authorized by a successful GP TC Response (Figure 73/75 "known to TC").
#ifndef SL_ZIGBEE_GP_TC_AUTHORIZED_GPD_TABLE_SIZE
#define SL_ZIGBEE_GP_TC_AUTHORIZED_GPD_TABLE_SIZE 4
#endif

static sl_zigbee_gp_address_t sli_gpTcAuthorizedGpd[SL_ZIGBEE_GP_TC_AUTHORIZED_GPD_TABLE_SIZE];
static bool sli_gpTcAuthorizedGpdInUse[SL_ZIGBEE_GP_TC_AUTHORIZED_GPD_TABLE_SIZE];

static void sli_gp_tc_remember_authorized_gpd(const sl_zigbee_gp_address_t *gpdAddr)
{
  uint8_t i;
  int8_t freeSlot = -1;

  if (gpdAddr == NULL) {
    return;
  }
  for (i = 0; i < SL_ZIGBEE_GP_TC_AUTHORIZED_GPD_TABLE_SIZE; i++) {
    if (sli_gpTcAuthorizedGpdInUse[i]
        && sl_zigbee_af_green_power_common_gp_addr_compare(gpdAddr, &sli_gpTcAuthorizedGpd[i])) {
      return;
    }
    if (!sli_gpTcAuthorizedGpdInUse[i] && freeSlot < 0) {
      freeSlot = (int8_t)i;
    }
  }
  if (freeSlot >= 0) {
    sli_gpTcAuthorizedGpd[freeSlot] = *gpdAddr;
    sli_gpTcAuthorizedGpdInUse[freeSlot] = true;
  }
}

static bool sli_gp_tc_gpd_is_known(const sl_zigbee_gp_address_t *gpdAddr)
{
  uint8_t i;
  sl_zigbee_key_data_t unusedKey = { { 0 } };

  if (gpdAddr == NULL) {
    return false;
  }
  // Known via IC registration or a prior Success Response.
  if (sl_zigbee_gp_lookup_ic_derived_key(gpdAddr, &unusedKey) == SL_STATUS_OK) {
    return true;
  }
  for (i = 0; i < SL_ZIGBEE_GP_TC_AUTHORIZED_GPD_TABLE_SIZE; i++) {
    if (sli_gpTcAuthorizedGpdInUse[i]
        && sl_zigbee_af_green_power_common_gp_addr_compare(gpdAddr, &sli_gpTcAuthorizedGpd[i])) {
      return true;
    }
  }
  return false;
}

static void sli_gp_tc_forget_authorized_gpd(const sl_zigbee_gp_address_t *gpdAddr)
{
  uint8_t i;

  if (gpdAddr == NULL) {
    return;
  }
  for (i = 0; i < SL_ZIGBEE_GP_TC_AUTHORIZED_GPD_TABLE_SIZE; i++) {
    if (sli_gpTcAuthorizedGpdInUse[i]
        && sl_zigbee_af_green_power_common_gp_addr_compare(gpdAddr, &sli_gpTcAuthorizedGpd[i])) {
      sli_gpTcAuthorizedGpdInUse[i] = false;
      memset(&sli_gpTcAuthorizedGpd[i], 0, sizeof(sl_zigbee_gp_address_t));
      return;
    }
  }
}

// A.3.5.2.x GP TC Request effect on receipt:
//  - non-unicast: silently dropped;
//  - incorrectly formatted: dropped, ZCL Default Response MALFORMED_COMMAND;
//  - policy Drop (Figures 120-125): no GP TC Response, no Default Response;
//  - otherwise GP TC Response with SUCCESS / FAILURE / ACTION_DENIED.
static sl_zigbee_af_zcl_request_status_t sli_gp_tc_handle_request(sl_zigbee_af_cluster_command_t *cmd)
{
  sli_zigbee_gp_tc_request_context_t req = { 0 };
  sli_zigbee_gp_tc_response_t resp = { 0 };
  sli_zigbee_gp_tc_policy_t policy = { 0 };
  uint8_t responsePayload[96];
  uint8_t responseLen;

  if (cmd->type != SL_ZIGBEE_INCOMING_UNICAST) {
    sl_zigbee_af_green_power_cluster_println("GP TC Request dropped (not unicast)");
    return SL_ZIGBEE_ZCL_STATUS_INTERNAL_COMMAND_HANDLED;
  }

  if (cmd->bufLen <= cmd->payloadStartIndex
      || sli_gp_tc_parse_request(cmd->buffer + cmd->payloadStartIndex,
                                 (uint8_t)(cmd->bufLen - cmd->payloadStartIndex),
                                 &req) != SL_STATUS_OK) {
    return SL_ZIGBEE_ZCL_STATUS_MALFORMED_COMMAND;
  }

  sli_zigbee_gp_tc_load_policy(&policy);
  if (sli_zigbee_gp_tc_evaluate_request(&policy, &req, &resp) != SL_STATUS_OK) {
    // Malformed Commissioning GPDF payload carried in the Request.
    return SL_ZIGBEE_ZCL_STATUS_MALFORMED_COMMAND;
  }

  // Figures 120-125 "Drop": no GP TC Response (the sink pairing times out).
  if (resp.status == SL_ZIGBEE_GP_TC_RESPONSE_STATUS_DROP) {
    sl_zigbee_af_green_power_cluster_println("GP TC Request dropped (policy)");
    return SL_ZIGBEE_ZCL_STATUS_INTERNAL_COMMAND_HANDLED;
  }

  responseLen = sli_gp_tc_encode_response(&resp, &req.gpdAddr, responsePayload, sizeof(responsePayload));
  if (responseLen == 0U) {
    return SL_ZIGBEE_ZCL_STATUS_FAILURE;
  }

  if (resp.status == SL_ZIGBEE_GP_TC_RESPONSE_STATUS_SUCCESS) {
    sli_gp_tc_remember_authorized_gpd(&req.gpdAddr);
  }
  sl_zigbee_af_green_power_cluster_println("GP TC Response status 0x%X keyType 0x%X icEncType 0x%X",
                                           resp.status, resp.keyType, resp.icEncryptedKeyType);

  // Table 49: TC generates GpTcResponse on the GP client side (client→server).
  sl_zigbee_af_fill_external_buffer((ZCL_CLUSTER_SPECIFIC_COMMAND
                                     | ZCL_FRAME_CONTROL_CLIENT_TO_SERVER
                                     | ZCL_DISABLE_DEFAULT_RESPONSE_MASK),
                                    ZCL_GREEN_POWER_CLUSTER_ID,
                                    ZCL_GP_TC_RESPONSE_COMMAND_ID,
                                    "b",
                                    responsePayload,
                                    responseLen);

  sl_zigbee_aps_frame_t *apsFrame = sl_zigbee_af_get_command_aps_frame();
  apsFrame->sourceEndpoint = SL_ZIGBEE_GP_ENDPOINT;
  apsFrame->destinationEndpoint = SL_ZIGBEE_GP_ENDPOINT;

  sl_zigbee_af_send_command_unicast(SL_ZIGBEE_OUTGOING_DIRECT, cmd->source);
  return SL_ZIGBEE_ZCL_STATUS_INTERNAL_COMMAND_HANDLED;
}

// Figures 73/75: Default Response SUCCESS if the GPD is known, NOT_FOUND otherwise.
// Pairing Completion keeps the install code; Decommissioning Notification MAY
// remove it (we do) and forgets the GPD.
static sl_zigbee_af_zcl_request_status_t sli_gp_tc_handle_notify_with_default_response(
  sl_zigbee_af_cluster_command_t *cmd,
  bool decommissioning)
{
  sl_zigbee_gp_address_t gpdAddr = { 0 };
  uint32_t frameCounter = 0;

  if (cmd->type != SL_ZIGBEE_INCOMING_UNICAST) {
    // Spec: non-unicast SHALL be dropped (no Default Response).
    return SL_ZIGBEE_ZCL_STATUS_INTERNAL_COMMAND_HANDLED;
  }

  if (cmd->bufLen <= cmd->payloadStartIndex
      || sli_gp_tc_parse_notify(cmd->buffer + cmd->payloadStartIndex,
                                (uint8_t)(cmd->bufLen - cmd->payloadStartIndex),
                                &gpdAddr,
                                &frameCounter) != SL_STATUS_OK) {
    return SL_ZIGBEE_ZCL_STATUS_MALFORMED_COMMAND;
  }
  (void)frameCounter;

  if (!sli_gp_tc_gpd_is_known(&gpdAddr)) {
    return SL_ZIGBEE_ZCL_STATUS_NOT_FOUND;
  }

  if (decommissioning) {
    (void)sl_zigbee_gp_tc_remove_install_code(&gpdAddr);
    sli_gp_tc_forget_authorized_gpd(&gpdAddr);
  }
  return SL_ZIGBEE_ZCL_STATUS_SUCCESS;
}

void sl_zigbee_af_green_power_tc_init_cb(uint8_t init_level)
{
  if (init_level == SL_ZIGBEE_INIT_LEVEL_DONE) {
    sli_zigbee_af_gp_oob_key_table_init();
    memset(sli_gpTcAuthorizedGpdInUse, 0, sizeof(sli_gpTcAuthorizedGpdInUse));
    memset(sli_gpTcAuthorizedGpd, 0, sizeof(sli_gpTcAuthorizedGpd));
  }
}

// Table 48: GP client receives server-generated TC Request / Pairing / Decomm.
uint32_t sl_zigbee_af_green_power_tc_cluster_client_command_parse(sl_service_opcode_t opcode,
                                                                   sl_service_function_context_t *context)
{
  sl_zigbee_af_cluster_command_t *cmd = context->data;
  sl_zigbee_af_zcl_request_status_t status = SL_ZIGBEE_ZCL_STATUS_UNSUP_COMMAND;

  (void)opcode;

  if (!cmd->mfgSpecific) {
    switch (cmd->commandId) {
      case ZCL_GP_TC_REQUEST_COMMAND_ID:
        status = sli_gp_tc_handle_request(cmd);
        break;
      case ZCL_GP_TC_PAIRING_COMPLETION_COMMAND_ID:
        status = sli_gp_tc_handle_notify_with_default_response(cmd, false);
        break;
      case ZCL_GP_TC_DECOMMISSIONING_NOTIFICATION_COMMAND_ID:
        status = sli_gp_tc_handle_notify_with_default_response(cmd, true);
        break;
      default:
        break;
    }
  }

  return status;
}
