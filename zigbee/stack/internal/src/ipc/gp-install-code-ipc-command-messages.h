/***************************************************************************//**
 * @file gp-install-code-ipc-command-messages.h
 * @brief defines structured format for 'gp-install-code' ipc messages
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
// automatically generated from gp-install-code.h.  Do not manually edit
#ifndef GP_INSTALL_CODE_IPC_COMMAND_MESSAGES_H
#define GP_INSTALL_CODE_IPC_COMMAND_MESSAGES_H

#include "stack/include/gp-install-code.h"
#include "stack/internal/inc/gp-install-code-internal-def.h"

typedef struct {
  uint8_t ic[SL_ZIGBEE_GP_INSTALL_CODE_SIZE];
  sl_zigbee_key_data_t derivedKey;
} sli_zigbee_stack_gp_derive_key_from_install_code_ipc_req_t;

typedef struct {
  sl_status_t result;
} sli_zigbee_stack_gp_derive_key_from_install_code_ipc_rsp_t;

typedef struct {
  sli_zigbee_stack_gp_derive_key_from_install_code_ipc_req_t request;
  sli_zigbee_stack_gp_derive_key_from_install_code_ipc_rsp_t response;
} sli_zigbee_stack_gp_derive_key_from_install_code_ipc_msg_t;

typedef struct {
  sl_zigbee_gp_application_id_t appId;
  uint32_t srcId;
  sl_802154_long_addr_t ieee;
  sl_802154_long_addr_t deviceAddressOut;
} sli_zigbee_stack_gp_encode_gpd_device_address_ipc_req_t;

typedef struct {
  sl_status_t result;
} sli_zigbee_stack_gp_encode_gpd_device_address_ipc_rsp_t;

typedef struct {
  sli_zigbee_stack_gp_encode_gpd_device_address_ipc_req_t request;
  sli_zigbee_stack_gp_encode_gpd_device_address_ipc_rsp_t response;
} sli_zigbee_stack_gp_encode_gpd_device_address_ipc_msg_t;

typedef struct {
  sl_zigbee_gp_address_t gpdAddr;
  sl_zigbee_key_data_t keyOut;
} sli_zigbee_stack_gp_lookup_ic_derived_key_ipc_req_t;

typedef struct {
  sl_status_t result;
} sli_zigbee_stack_gp_lookup_ic_derived_key_ipc_rsp_t;

typedef struct {
  sli_zigbee_stack_gp_lookup_ic_derived_key_ipc_req_t request;
  sli_zigbee_stack_gp_lookup_ic_derived_key_ipc_rsp_t response;
} sli_zigbee_stack_gp_lookup_ic_derived_key_ipc_msg_t;

typedef struct {
  sl_zigbee_gp_address_t gpdAddr;
  uint8_t ic[SL_ZIGBEE_GP_INSTALL_CODE_SIZE];
} sli_zigbee_stack_gp_tc_register_install_code_ipc_req_t;

typedef struct {
  sl_status_t result;
} sli_zigbee_stack_gp_tc_register_install_code_ipc_rsp_t;

typedef struct {
  sli_zigbee_stack_gp_tc_register_install_code_ipc_req_t request;
  sli_zigbee_stack_gp_tc_register_install_code_ipc_rsp_t response;
} sli_zigbee_stack_gp_tc_register_install_code_ipc_msg_t;

typedef struct {
  sl_zigbee_gp_address_t gpdAddr;
} sli_zigbee_stack_gp_tc_remove_install_code_ipc_req_t;

typedef struct {
  sl_status_t result;
} sli_zigbee_stack_gp_tc_remove_install_code_ipc_rsp_t;

typedef struct {
  sli_zigbee_stack_gp_tc_remove_install_code_ipc_req_t request;
  sli_zigbee_stack_gp_tc_remove_install_code_ipc_rsp_t response;
} sli_zigbee_stack_gp_tc_remove_install_code_ipc_msg_t;

typedef struct {
  uint8_t ic[SL_ZIGBEE_GP_INSTALL_CODE_SIZE];
} sli_zigbee_stack_gp_validate_install_code_crc_ipc_req_t;

typedef struct {
  sl_status_t result;
} sli_zigbee_stack_gp_validate_install_code_crc_ipc_rsp_t;

typedef struct {
  sli_zigbee_stack_gp_validate_install_code_crc_ipc_req_t request;
  sli_zigbee_stack_gp_validate_install_code_crc_ipc_rsp_t response;
} sli_zigbee_stack_gp_validate_install_code_crc_ipc_msg_t;

#endif // GP_INSTALL_CODE_IPC_COMMAND_MESSAGES_H
