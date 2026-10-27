/***************************************************************************//**
 * @file
 * @brief ZCL encode/decode for GP TC Request/Response (GP v1.1.3).
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

#ifndef GREEN_POWER_TC_MSG_H
#define GREEN_POWER_TC_MSG_H

#include "green-power-v113.h"

typedef struct {
  uint8_t gpdfOptions;
  uint8_t gpdfExtendedOptions;
  uint8_t securityLevel;
  uint8_t securityKeyType;
  bool gpdKeyPresent;
  bool gpdKeyEncrypted;
  sl_zigbee_key_data_t gpdKey;
  uint32_t gpdKeyMic;
  bool outgoingFrameCounterPresent;
  uint32_t outgoingFrameCounter;
  // Figure 128 Options bit 3: GPD can protect the key exchange with IC-based TCLK.
  bool icTclkSupported;
  // Figure 130 Application Information bit 6: GPDkey encrypted with IC-based TCLK.
  bool applicationInfoPresent;
  uint8_t applicationInfo;
  bool icTclkUsed;
} sli_gp_tc_commissioning_info_t;

sl_status_t sli_gp_tc_parse_commissioning_payload(const uint8_t *payload,
                                                   uint8_t payloadLen,
                                                   sli_gp_tc_commissioning_info_t *info);

// Figures 73 / 75: Options (1) | GPD ID | Endpoint?
sl_status_t sli_gp_tc_parse_notify(const uint8_t *buffer,
                                    uint8_t bufLen,
                                    sl_zigbee_gp_address_t *gpdAddr,
                                    uint32_t *frameCounter);

sl_status_t sli_gp_tc_parse_request(const uint8_t *buffer,
                                     uint8_t bufLen,
                                     sli_zigbee_gp_tc_request_context_t *req);

uint8_t sli_gp_tc_encode_notify(uint8_t options,
                                 const sl_zigbee_gp_address_t *gpdAddr,
                                 uint8_t *buffer,
                                 uint8_t bufferLen);

uint8_t sli_gp_tc_encode_request(const sli_zigbee_gp_tc_request_context_t *req,
                                  uint8_t *buffer,
                                  uint8_t bufferLen);

uint8_t sli_gp_tc_encode_response(const sli_zigbee_gp_tc_response_t *resp,
                                   const sl_zigbee_gp_address_t *gpdAddr,
                                   uint8_t *buffer,
                                   uint8_t bufferLen);

sl_status_t sli_gp_tc_parse_response(const uint8_t *buffer,
                                      uint8_t bufLen,
                                      sli_zigbee_gp_tc_response_t *resp,
                                      sl_zigbee_gp_address_t *gpdAddr);

uint16_t sli_gp_tc_build_response_options(const sl_zigbee_gp_address_t *gpdAddr,
                                            const sli_zigbee_gp_tc_response_t *resp);

#endif // GREEN_POWER_TC_MSG_H
