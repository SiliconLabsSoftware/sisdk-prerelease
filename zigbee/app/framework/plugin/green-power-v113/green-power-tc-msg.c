/***************************************************************************//**
 * @file
 * @brief ZCL encode/decode for GP TC Request/Response (GP v1.1.3).
 *
 * Wire formats from 25-67604-001 (candidate 14-0563-19):
 *   Request  – Figure 71 / Options Figure 72
 *   Response – Figure 52 / Options Figure 53
 *   Pairing Completion / Decommissioning – Figures 73–76
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
#include "green-power-tc-msg.h"

static bool sli_gp_tc_buffer_has_room(uint8_t index, uint8_t need, uint8_t bufferLen)
{
  return ((uint16_t)index + need) <= bufferLen;
}

static void sli_gp_tc_parse_gpd_id(uint8_t applicationId,
                                    const uint8_t *buffer,
                                    uint8_t *index,
                                    uint8_t bufLen,
                                    sl_zigbee_gp_address_t *gpdAddr)
{
  gpdAddr->applicationId = (sl_zigbee_gp_application_id_t)(applicationId & 0x07U);

  if (gpdAddr->applicationId == SL_ZIGBEE_GP_APPLICATION_SOURCE_ID) {
    if ((*index + 4U) > bufLen) {
      return;
    }
    gpdAddr->id.sourceId = sl_util_fetch_low_high_int32u(buffer + *index);
    *index += 4;
  } else if (gpdAddr->applicationId == SL_ZIGBEE_GP_APPLICATION_IEEE_ADDRESS) {
    if ((*index + EUI64_SIZE + 1U) > bufLen) {
      return;
    }
    memmove(gpdAddr->id.gpdIeeeAddress, buffer + *index, EUI64_SIZE);
    *index += EUI64_SIZE;
    gpdAddr->endpoint = buffer[(*index)++];
  }
}

static uint8_t sli_gp_tc_encode_gpd_id(const sl_zigbee_gp_address_t *gpdAddr,
                                         uint8_t *buffer,
                                         uint8_t index,
                                         uint8_t bufferLen)
{
  if (gpdAddr->applicationId == SL_ZIGBEE_GP_APPLICATION_SOURCE_ID) {
    if (!sli_gp_tc_buffer_has_room(index, 4U, bufferLen)) {
      return 0;
    }
    sl_util_store_low_high_int32u(buffer + index, gpdAddr->id.sourceId);
    return (uint8_t)(index + 4U);
  }
  if (gpdAddr->applicationId == SL_ZIGBEE_GP_APPLICATION_IEEE_ADDRESS) {
    if (!sli_gp_tc_buffer_has_room(index, EUI64_SIZE + 1U, bufferLen)) {
      return 0;
    }
    memmove(buffer + index, gpdAddr->id.gpdIeeeAddress, EUI64_SIZE);
    index = (uint8_t)(index + EUI64_SIZE);
    buffer[index++] = gpdAddr->endpoint;
    return index;
  }
  return 0;
}

uint16_t sli_gp_tc_build_response_options(const sl_zigbee_gp_address_t *gpdAddr,
                                            const sli_zigbee_gp_tc_response_t *resp)
{
  uint16_t options = (uint16_t)(gpdAddr->applicationId & SL_ZIGBEE_GP_TC_RESPONSE_OPTION_APPLICATION_ID);

  if (resp->inClearGpdSecurityKeyPresent) {
    options |= SL_ZIGBEE_GP_TC_RESPONSE_OPTION_IN_CLEAR_KEY;
  }
  if (resp->gpdSecurityKeyPresent) {
    options |= SL_ZIGBEE_GP_TC_RESPONSE_OPTION_GPD_KEY_PRESENT;
  }
  if (resp->gpdKeyEncryption) {
    options |= SL_ZIGBEE_GP_TC_RESPONSE_OPTION_GPD_KEY_ENCRYPTION;
  }
  options |= (uint16_t)((resp->securityLevel << SL_ZIGBEE_GP_TC_RESPONSE_OPTION_SECURITY_LEVEL_OFFSET)
                        & SL_ZIGBEE_GP_TC_RESPONSE_OPTION_SECURITY_LEVEL);
  options |= (uint16_t)((resp->keyType << SL_ZIGBEE_GP_TC_RESPONSE_OPTION_KEY_TYPE_OFFSET)
                        & SL_ZIGBEE_GP_TC_RESPONSE_OPTION_KEY_TYPE);
  options |= (uint16_t)((resp->icEncryptedKeyType << SL_ZIGBEE_GP_TC_RESPONSE_OPTION_IC_ENCRYPTED_KEY_TYPE_OFFSET)
                        & SL_ZIGBEE_GP_TC_RESPONSE_OPTION_IC_ENCRYPTED_KEY_TYPE);
  return options;
}

sl_status_t sli_gp_tc_parse_commissioning_payload(const uint8_t *payload,
                                                   uint8_t payloadLen,
                                                   sli_gp_tc_commissioning_info_t *info)
{
  uint8_t index = 1;

  if (payload == NULL || info == NULL || payloadLen < 3U) {
    return SL_STATUS_INVALID_PARAMETER;
  }

  memset(info, 0, sizeof(sli_gp_tc_commissioning_info_t));

  if (payload[0] == 0U || payload[0] >= payloadLen) {
    return SL_STATUS_INVALID_PARAMETER;
  }

  // GPD commissioning payload: length | deviceId | options | ...
  index++; // skip deviceId
  info->gpdfOptions = payload[index++];
  info->icTclkSupported = ((info->gpdfOptions
                            & SL_ZIGBEE_AF_GP_GPD_COMMISSIONING_OPTIONS_IC_BASED_TCLK_SUPPORTED) != 0U);
  info->applicationInfoPresent = ((info->gpdfOptions
                                   & SL_ZIGBEE_AF_GP_GPD_COMMISSIONING_OPTIONS_APPLICATION_INFORMATION_PRESENT) != 0U);
  if (info->gpdfOptions & SL_ZIGBEE_AF_GP_GPD_COMMISSIONING_OPTIONS_EXTENDED_OPTIONS_FIELD) {
    if (index >= payloadLen) {
      return SL_STATUS_INVALID_PARAMETER;
    }
    info->gpdfExtendedOptions = payload[index++];
    info->securityLevel = (info->gpdfExtendedOptions
                           & SL_ZIGBEE_AF_GP_GPD_COMMISSIONING_EXTENDED_OPTIONS_SECURITY_LEVEL_CAPABILITIES);
    info->securityKeyType = (uint8_t)((info->gpdfExtendedOptions
                                       & SL_ZIGBEE_AF_GP_GPD_COMMISSIONING_EXTENDED_OPTIONS_KEY_TYPE)
                                      >> SL_ZIGBEE_AF_GP_GPD_COMMISSIONING_EXTENDED_OPTIONS_KEY_TYPE_OFFSET);
    info->gpdKeyPresent = ((info->gpdfExtendedOptions
                            & SL_ZIGBEE_AF_GP_GPD_COMMISSIONING_EXTENDED_OPTIONS_GPD_KEY_PRESENT) != 0U);
    info->gpdKeyEncrypted = ((info->gpdfExtendedOptions
                              & SL_ZIGBEE_AF_GP_GPD_COMMISSIONING_EXTENDED_OPTIONS_GPD_KEY_ENCRYPTION) != 0U);
  }

  if (info->gpdKeyPresent) {
    if ((index + SL_ZIGBEE_ENCRYPTION_KEY_SIZE) > payloadLen) {
      return SL_STATUS_INVALID_PARAMETER;
    }
    memmove(info->gpdKey.contents, payload + index, SL_ZIGBEE_ENCRYPTION_KEY_SIZE);
    index += SL_ZIGBEE_ENCRYPTION_KEY_SIZE;
    if (info->gpdKeyEncrypted) {
      if ((index + 4U) > payloadLen) {
        return SL_STATUS_INVALID_PARAMETER;
      }
      info->gpdKeyMic = sl_util_fetch_low_high_int32u(payload + index);
      index += 4;
    }
  }

  if ((info->gpdfExtendedOptions
       & SL_ZIGBEE_AF_GP_GPD_COMMISSIONING_EXTENDED_OPTIONS_GPD_OUTGOING_COUNTER_PRESENT) != 0U) {
    if ((index + 4U) > payloadLen) {
      return SL_STATUS_INVALID_PARAMETER;
    }
    info->outgoingFrameCounter = sl_util_fetch_low_high_int32u(payload + index);
    info->outgoingFrameCounterPresent = true;
    index += 4;
  }

  // Figure 127 order: ... GPDoutgoingCounter, Application information, ...
  if (info->applicationInfoPresent) {
    if (index >= payloadLen) {
      return SL_STATUS_INVALID_PARAMETER;
    }
    info->applicationInfo = payload[index++];
    info->icTclkUsed = ((info->applicationInfo
                         & SL_ZIGBEE_AF_GP_APPLICATION_INFORMATION_IC_BASED_TCLK_USED) != 0U);
  }

  return SL_STATUS_OK;
}

sl_status_t sli_gp_tc_parse_notify(const uint8_t *buffer,
                                    uint8_t bufLen,
                                    sl_zigbee_gp_address_t *gpdAddr,
                                    uint32_t *frameCounter)
{
  uint8_t index = 0;
  uint8_t options;

  if (buffer == NULL || gpdAddr == NULL || bufLen < 1U) {
    return SL_STATUS_INVALID_PARAMETER;
  }

  memset(gpdAddr, 0, sizeof(sl_zigbee_gp_address_t));
  options = buffer[index++];
  sli_gp_tc_parse_gpd_id(options, buffer, &index, bufLen, gpdAddr);
  if (gpdAddr->applicationId != SL_ZIGBEE_GP_APPLICATION_SOURCE_ID
      && gpdAddr->applicationId != SL_ZIGBEE_GP_APPLICATION_IEEE_ADDRESS) {
    return SL_STATUS_INVALID_PARAMETER;
  }
  if (frameCounter != NULL) {
    *frameCounter = 0;
  }
  (void)options;
  return SL_STATUS_OK;
}

uint8_t sli_gp_tc_encode_notify(uint8_t options,
                                 const sl_zigbee_gp_address_t *gpdAddr,
                                 uint8_t *buffer,
                                 uint8_t bufferLen)
{
  uint8_t index = 0;
  sl_zigbee_gp_address_t wireAddr;

  if (gpdAddr == NULL || buffer == NULL) {
    return 0;
  }

  if (!sli_gp_tc_buffer_has_room(index, 1U, bufferLen)) {
    return 0;
  }
  (void)options;
  buffer[index++] = (uint8_t)(gpdAddr->applicationId & 0x07U);

  // A.3.3.6.4 / A.3.3.6.5: IEEE Endpoint on Pairing Completion /
  // Decommissioning Notification is reserved and SHALL be 0xFF.
  wireAddr = *gpdAddr;
  if (wireAddr.applicationId == SL_ZIGBEE_GP_APPLICATION_IEEE_ADDRESS) {
    wireAddr.endpoint = 0xFF;
  }
  return sli_gp_tc_encode_gpd_id(&wireAddr, buffer, index, bufferLen);
}

sl_status_t sli_gp_tc_parse_request(const uint8_t *buffer,
                                     uint8_t bufLen,
                                     sli_zigbee_gp_tc_request_context_t *req)
{
  uint8_t index = 0;
  uint8_t options;
  sli_gp_tc_commissioning_info_t comm = { 0 };

  if (buffer == NULL || req == NULL || bufLen < 3U) {
    return SL_STATUS_INVALID_PARAMETER;
  }

  memset(req, 0, sizeof(sli_zigbee_gp_tc_request_context_t));

  options = buffer[index++];
  req->options = options;
  sli_gp_tc_parse_gpd_id(options, buffer, &index, bufLen, &req->gpdAddr);
  if (req->gpdAddr.applicationId != SL_ZIGBEE_GP_APPLICATION_SOURCE_ID
      && req->gpdAddr.applicationId != SL_ZIGBEE_GP_APPLICATION_IEEE_ADDRESS) {
    return SL_STATUS_INVALID_PARAMETER;
  }

  if (index >= bufLen) {
    return SL_STATUS_INVALID_PARAMETER;
  }
  req->gpdCommandId = buffer[index++];

  if (index >= bufLen) {
    return SL_STATUS_INVALID_PARAMETER;
  }
  req->gpdCommandPayloadLen = buffer[index++];
  if (req->gpdCommandPayloadLen > SL_GP_TC_COMMISSIONING_PAYLOAD_MAX
      || (index + req->gpdCommandPayloadLen) > bufLen) {
    return SL_STATUS_INVALID_PARAMETER;
  }
  if (req->gpdCommandPayloadLen > 0U) {
    memmove(req->gpdCommandPayload, buffer + index, req->gpdCommandPayloadLen);
    index += req->gpdCommandPayloadLen;
  }

  if ((options & SL_ZIGBEE_GP_TC_REQUEST_OPTION_OOB_KEY_PRESENT) != 0U) {
    if ((index + SL_ZIGBEE_ENCRYPTION_KEY_SIZE) > bufLen) {
      return SL_STATUS_INVALID_PARAMETER;
    }
    memmove(req->gpdSecurityKey.contents, buffer + index, SL_ZIGBEE_ENCRYPTION_KEY_SIZE);
    index += SL_ZIGBEE_ENCRYPTION_KEY_SIZE;
    req->gpdSecurityKeyPresent = true;
  }
  (void)index;

  // Security level / key type / frame counter come from the GPD Commissioning
  // payload (not from Request options — Figure 72 is AppID + OOB present only).
  if (req->gpdCommandPayloadLen > 0U) {
    uint8_t wrapped[SL_GP_TC_COMMISSIONING_PAYLOAD_MAX + 1U];

    wrapped[0] = req->gpdCommandPayloadLen;
    memmove(wrapped + 1, req->gpdCommandPayload, req->gpdCommandPayloadLen);
    if (sli_gp_tc_parse_commissioning_payload(wrapped,
                                               req->gpdCommandPayloadLen + 1U,
                                               &comm) == SL_STATUS_OK) {
      req->securityLevel = comm.securityLevel;
      req->securityKeyType = comm.securityKeyType;
      if (comm.outgoingFrameCounterPresent) {
        req->frameCounter = comm.outgoingFrameCounter;
      }
    }
  }

  return SL_STATUS_OK;
}

uint8_t sli_gp_tc_encode_request(const sli_zigbee_gp_tc_request_context_t *req,
                                  uint8_t *buffer,
                                  uint8_t bufferLen)
{
  uint8_t index = 0;
  uint8_t options;

  if (req == NULL || buffer == NULL) {
    return 0;
  }

  options = (uint8_t)(req->gpdAddr.applicationId & SL_ZIGBEE_GP_TC_REQUEST_OPTION_APPLICATION_ID);
  if (req->gpdSecurityKeyPresent) {
    options |= SL_ZIGBEE_GP_TC_REQUEST_OPTION_OOB_KEY_PRESENT;
  }

  if (!sli_gp_tc_buffer_has_room(index, 1U, bufferLen)) {
    return 0;
  }
  buffer[index++] = options;

  index = sli_gp_tc_encode_gpd_id(&req->gpdAddr, buffer, index, bufferLen);
  if (index == 0U) {
    return 0;
  }

  if (!sli_gp_tc_buffer_has_room(index, 2U, bufferLen)) {
    return 0;
  }
  buffer[index++] = req->gpdCommandId;
  buffer[index++] = req->gpdCommandPayloadLen;
  if (req->gpdCommandPayloadLen > 0U) {
    if (!sli_gp_tc_buffer_has_room(index, req->gpdCommandPayloadLen, bufferLen)) {
      return 0;
    }
    memmove(buffer + index, req->gpdCommandPayload, req->gpdCommandPayloadLen);
    index += req->gpdCommandPayloadLen;
  }

  if (req->gpdSecurityKeyPresent) {
    if (!sli_gp_tc_buffer_has_room(index, SL_ZIGBEE_ENCRYPTION_KEY_SIZE, bufferLen)) {
      return 0;
    }
    memmove(buffer + index, req->gpdSecurityKey.contents, SL_ZIGBEE_ENCRYPTION_KEY_SIZE);
    index += SL_ZIGBEE_ENCRYPTION_KEY_SIZE;
  }

  return index;
}

uint8_t sli_gp_tc_encode_response(const sli_zigbee_gp_tc_response_t *resp,
                                   const sl_zigbee_gp_address_t *gpdAddr,
                                   uint8_t *buffer,
                                   uint8_t bufferLen)
{
  uint8_t index = 0;
  uint16_t options;
  bool keyMicAndFcPresent;

  if (resp == NULL || gpdAddr == NULL || buffer == NULL) {
    return 0;
  }

  options = sli_gp_tc_build_response_options(gpdAddr, resp);
  keyMicAndFcPresent = (resp->gpdSecurityKeyPresent
                        && resp->gpdKeyEncryption
                        && (resp->securityLevel == 0x02U || resp->securityLevel == 0x03U));

  if (!sli_gp_tc_buffer_has_room(index, 1U + 2U, bufferLen)) {
    return 0;
  }
  buffer[index++] = resp->status;
  sl_util_store_low_high_int16u(buffer + index, options);
  index += 2;

  // Extended Options reserved in this release (bit 15 SHALL be 0).
  index = sli_gp_tc_encode_gpd_id(gpdAddr, buffer, index, bufferLen);
  if (index == 0U) {
    return 0;
  }

  if (resp->gpdSecurityKeyPresent) {
    if (!sli_gp_tc_buffer_has_room(index, SL_ZIGBEE_ENCRYPTION_KEY_SIZE, bufferLen)) {
      return 0;
    }
    memmove(buffer + index, resp->gpdSecurityKey.contents, SL_ZIGBEE_ENCRYPTION_KEY_SIZE);
    index += SL_ZIGBEE_ENCRYPTION_KEY_SIZE;

    if (resp->gpdKeyEncryption) {
      if (!sli_gp_tc_buffer_has_room(index, 4U, bufferLen)) {
        return 0;
      }
      sl_util_store_low_high_int32u(buffer + index, resp->mic);
      index += 4;
    }

    if (keyMicAndFcPresent) {
      if (!sli_gp_tc_buffer_has_room(index, 4U, bufferLen)) {
        return 0;
      }
      sl_util_store_low_high_int32u(buffer + index, resp->frameCounter);
      index += 4;
    }
  }

  if (resp->inClearGpdSecurityKeyPresent) {
    if (!sli_gp_tc_buffer_has_room(index, SL_ZIGBEE_ENCRYPTION_KEY_SIZE, bufferLen)) {
      return 0;
    }
    memmove(buffer + index,
            resp->inClearGpdSecurityKey.contents,
            SL_ZIGBEE_ENCRYPTION_KEY_SIZE);
    index += SL_ZIGBEE_ENCRYPTION_KEY_SIZE;
  }

  return index;
}

sl_status_t sli_gp_tc_parse_response(const uint8_t *buffer,
                                      uint8_t bufLen,
                                      sli_zigbee_gp_tc_response_t *resp,
                                      sl_zigbee_gp_address_t *gpdAddr)
{
  uint8_t index = 0;
  uint16_t options;
  bool keyMicAndFcPresent;

  if (buffer == NULL || resp == NULL || gpdAddr == NULL || bufLen < 3U) {
    return SL_STATUS_INVALID_PARAMETER;
  }

  memset(resp, 0, sizeof(sli_zigbee_gp_tc_response_t));
  memset(gpdAddr, 0, sizeof(sl_zigbee_gp_address_t));

  resp->status = buffer[index++];
  options = sl_util_fetch_low_high_int16u(buffer + index);
  index += 2;
  resp->options = options;
  resp->inClearGpdSecurityKeyPresent = ((options & SL_ZIGBEE_GP_TC_RESPONSE_OPTION_IN_CLEAR_KEY) != 0U);
  resp->gpdSecurityKeyPresent = ((options & SL_ZIGBEE_GP_TC_RESPONSE_OPTION_GPD_KEY_PRESENT) != 0U);
  resp->gpdKeyEncryption = ((options & SL_ZIGBEE_GP_TC_RESPONSE_OPTION_GPD_KEY_ENCRYPTION) != 0U);
  resp->securityLevel = (uint8_t)((options & SL_ZIGBEE_GP_TC_RESPONSE_OPTION_SECURITY_LEVEL)
                                  >> SL_ZIGBEE_GP_TC_RESPONSE_OPTION_SECURITY_LEVEL_OFFSET);
  resp->keyType = (uint8_t)((options & SL_ZIGBEE_GP_TC_RESPONSE_OPTION_KEY_TYPE)
                            >> SL_ZIGBEE_GP_TC_RESPONSE_OPTION_KEY_TYPE_OFFSET);
  resp->icEncryptedKeyType = (uint8_t)((options & SL_ZIGBEE_GP_TC_RESPONSE_OPTION_IC_ENCRYPTED_KEY_TYPE)
                                       >> SL_ZIGBEE_GP_TC_RESPONSE_OPTION_IC_ENCRYPTED_KEY_TYPE_OFFSET);

  if ((options & SL_ZIGBEE_GP_TC_RESPONSE_OPTION_EXTENDED_OPTIONS) != 0U) {
    if (index >= bufLen) {
      return SL_STATUS_INVALID_PARAMETER;
    }
    index++; // Extended Options present but unused in this release
  }

  sli_gp_tc_parse_gpd_id((uint8_t)(options & SL_ZIGBEE_GP_TC_RESPONSE_OPTION_APPLICATION_ID),
                          buffer,
                          &index,
                          bufLen,
                          gpdAddr);
  if (gpdAddr->applicationId != SL_ZIGBEE_GP_APPLICATION_SOURCE_ID
      && gpdAddr->applicationId != SL_ZIGBEE_GP_APPLICATION_IEEE_ADDRESS) {
    return SL_STATUS_INVALID_PARAMETER;
  }

  keyMicAndFcPresent = (resp->gpdSecurityKeyPresent
                        && resp->gpdKeyEncryption
                        && (resp->securityLevel == 0x02U || resp->securityLevel == 0x03U));

  if (resp->gpdSecurityKeyPresent) {
    if ((index + SL_ZIGBEE_ENCRYPTION_KEY_SIZE) > bufLen) {
      return SL_STATUS_INVALID_PARAMETER;
    }
    memmove(resp->gpdSecurityKey.contents, buffer + index, SL_ZIGBEE_ENCRYPTION_KEY_SIZE);
    index += SL_ZIGBEE_ENCRYPTION_KEY_SIZE;

    if (resp->gpdKeyEncryption) {
      if ((index + 4U) > bufLen) {
        return SL_STATUS_INVALID_PARAMETER;
      }
      resp->mic = sl_util_fetch_low_high_int32u(buffer + index);
      index += 4;
      resp->gpdKeyMicPresent = true;
    }

    if (keyMicAndFcPresent) {
      if ((index + 4U) > bufLen) {
        return SL_STATUS_INVALID_PARAMETER;
      }
      resp->frameCounter = sl_util_fetch_low_high_int32u(buffer + index);
      index += 4;
    }
  }

  if (resp->inClearGpdSecurityKeyPresent) {
    if ((index + SL_ZIGBEE_ENCRYPTION_KEY_SIZE) > bufLen) {
      return SL_STATUS_INVALID_PARAMETER;
    }
    memmove(resp->inClearGpdSecurityKey.contents, buffer + index, SL_ZIGBEE_ENCRYPTION_KEY_SIZE);
  }

  return SL_STATUS_OK;
}
