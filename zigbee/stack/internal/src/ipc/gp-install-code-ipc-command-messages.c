/***************************************************************************//**
 * @file gp-install-code-ipc-command-messages.c
 * @brief internal wrappers for 'gp-install-code' ipc commands
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
#include "stack/include/gp-install-code.h"
#include "stack/internal/inc/gp-install-code-internal-def.h"
#include "stack/internal/src/ipc/gp-install-code-ipc-command-messages.h"
#include "stack/internal/src/ipc/zigbee_ipc_command_messages.h"

// ipc command dispatch

void sli_zigbee_stack_gp_derive_key_from_install_code_process_ipc_command(sli_zigbee_ipc_cmd_t *msg)
{
  msg->data.gp_derive_key_from_install_code.response.result = sli_zigbee_stack_gp_derive_key_from_install_code(msg->data.gp_derive_key_from_install_code.request.ic,
                                                                                                               &msg->data.gp_derive_key_from_install_code.request.derivedKey);
}

void sli_zigbee_stack_gp_encode_gpd_device_address_process_ipc_command(sli_zigbee_ipc_cmd_t *msg)
{
  msg->data.gp_encode_gpd_device_address.response.result = sli_zigbee_stack_gp_encode_gpd_device_address(msg->data.gp_encode_gpd_device_address.request.appId,
                                                                                                         msg->data.gp_encode_gpd_device_address.request.srcId,
                                                                                                         msg->data.gp_encode_gpd_device_address.request.ieee,
                                                                                                         msg->data.gp_encode_gpd_device_address.request.deviceAddressOut);
}

void sli_zigbee_stack_gp_lookup_ic_derived_key_process_ipc_command(sli_zigbee_ipc_cmd_t *msg)
{
  msg->data.gp_lookup_ic_derived_key.response.result = sli_zigbee_stack_gp_lookup_ic_derived_key(&msg->data.gp_lookup_ic_derived_key.request.gpdAddr,
                                                                                                 &msg->data.gp_lookup_ic_derived_key.request.keyOut);
}

void sli_zigbee_stack_gp_tc_register_install_code_process_ipc_command(sli_zigbee_ipc_cmd_t *msg)
{
  msg->data.gp_tc_register_install_code.response.result = sli_zigbee_stack_gp_tc_register_install_code(&msg->data.gp_tc_register_install_code.request.gpdAddr,
                                                                                                       msg->data.gp_tc_register_install_code.request.ic);
}

void sli_zigbee_stack_gp_tc_remove_install_code_process_ipc_command(sli_zigbee_ipc_cmd_t *msg)
{
  msg->data.gp_tc_remove_install_code.response.result = sli_zigbee_stack_gp_tc_remove_install_code(&msg->data.gp_tc_remove_install_code.request.gpdAddr);
}

void sli_zigbee_stack_gp_validate_install_code_crc_process_ipc_command(sli_zigbee_ipc_cmd_t *msg)
{
  msg->data.gp_validate_install_code_crc.response.result = sli_zigbee_stack_gp_validate_install_code_crc(msg->data.gp_validate_install_code_crc.request.ic);
}

// public entrypoints

#if defined(__GNUC__)
__attribute__((noinline))
#endif
sl_status_t sl_zigbee_gp_derive_key_from_install_code(const uint8_t *ic,
                                                      sl_zigbee_key_data_t *derivedKey)
{
  sli_zigbee_ipc_cmd_t msg = { 0, };

  if (ic != NULL) {
    memmove(msg.data.gp_derive_key_from_install_code.request.ic, ic, sizeof(uint8_t) * SL_ZIGBEE_GP_INSTALL_CODE_SIZE);
  }

  if (derivedKey != NULL) {
    msg.data.gp_derive_key_from_install_code.request.derivedKey = *derivedKey;
  }

  sli_zigbee_send_ipc_cmd(sli_zigbee_stack_gp_derive_key_from_install_code_process_ipc_command, &msg);

  if (derivedKey != NULL) {
    *derivedKey = msg.data.gp_derive_key_from_install_code.request.derivedKey;
  }

  return msg.data.gp_derive_key_from_install_code.response.result;
}

#if defined(__GNUC__)
__attribute__((noinline))
#endif
sl_status_t sl_zigbee_gp_encode_gpd_device_address(sl_zigbee_gp_application_id_t appId,
                                                   uint32_t srcId,
                                                   sl_802154_long_addr_t ieee,
                                                   sl_802154_long_addr_t deviceAddressOut)
{
  sli_zigbee_ipc_cmd_t msg = { 0, };
  msg.data.gp_encode_gpd_device_address.request.appId = appId;
  msg.data.gp_encode_gpd_device_address.request.srcId = srcId;

  if (ieee != NULL) {
    memmove(msg.data.gp_encode_gpd_device_address.request.ieee, ieee, sizeof(sl_802154_long_addr_t));
  }

  if (deviceAddressOut != NULL) {
    memmove(msg.data.gp_encode_gpd_device_address.request.deviceAddressOut, deviceAddressOut, sizeof(sl_802154_long_addr_t));
  }

  sli_zigbee_send_ipc_cmd(sli_zigbee_stack_gp_encode_gpd_device_address_process_ipc_command, &msg);

  if (ieee != NULL) {
    memmove(ieee, msg.data.gp_encode_gpd_device_address.request.ieee, sizeof(sl_802154_long_addr_t));
  }

  if (deviceAddressOut != NULL) {
    memmove(deviceAddressOut, msg.data.gp_encode_gpd_device_address.request.deviceAddressOut, sizeof(sl_802154_long_addr_t));
  }

  return msg.data.gp_encode_gpd_device_address.response.result;
}

#if defined(__GNUC__)
__attribute__((noinline))
#endif
sl_status_t sl_zigbee_gp_lookup_ic_derived_key(const sl_zigbee_gp_address_t *gpdAddr,
                                               sl_zigbee_key_data_t *keyOut)
{
  sli_zigbee_ipc_cmd_t msg = { 0, };

  if (gpdAddr != NULL) {
    msg.data.gp_lookup_ic_derived_key.request.gpdAddr = *gpdAddr;
  }

  if (keyOut != NULL) {
    msg.data.gp_lookup_ic_derived_key.request.keyOut = *keyOut;
  }

  sli_zigbee_send_ipc_cmd(sli_zigbee_stack_gp_lookup_ic_derived_key_process_ipc_command, &msg);

  if (keyOut != NULL) {
    *keyOut = msg.data.gp_lookup_ic_derived_key.request.keyOut;
  }

  return msg.data.gp_lookup_ic_derived_key.response.result;
}

#if defined(__GNUC__)
__attribute__((noinline))
#endif
sl_status_t sl_zigbee_gp_tc_register_install_code(const sl_zigbee_gp_address_t *gpdAddr,
                                                  const uint8_t *ic)
{
  sli_zigbee_ipc_cmd_t msg = { 0, };

  if (gpdAddr != NULL) {
    msg.data.gp_tc_register_install_code.request.gpdAddr = *gpdAddr;
  }

  if (ic != NULL) {
    memmove(msg.data.gp_tc_register_install_code.request.ic, ic, sizeof(uint8_t) * SL_ZIGBEE_GP_INSTALL_CODE_SIZE);
  }

  sli_zigbee_send_ipc_cmd(sli_zigbee_stack_gp_tc_register_install_code_process_ipc_command, &msg);

  return msg.data.gp_tc_register_install_code.response.result;
}

#if defined(__GNUC__)
__attribute__((noinline))
#endif
sl_status_t sl_zigbee_gp_tc_remove_install_code(const sl_zigbee_gp_address_t *gpdAddr)
{
  sli_zigbee_ipc_cmd_t msg = { 0, };

  if (gpdAddr != NULL) {
    msg.data.gp_tc_remove_install_code.request.gpdAddr = *gpdAddr;
  }

  sli_zigbee_send_ipc_cmd(sli_zigbee_stack_gp_tc_remove_install_code_process_ipc_command, &msg);

  return msg.data.gp_tc_remove_install_code.response.result;
}

#if defined(__GNUC__)
__attribute__((noinline))
#endif
sl_status_t sl_zigbee_gp_validate_install_code_crc(const uint8_t *ic)
{
  sli_zigbee_ipc_cmd_t msg = { 0, };

  if (ic != NULL) {
    memmove(msg.data.gp_validate_install_code_crc.request.ic, ic, sizeof(uint8_t) * SL_ZIGBEE_GP_INSTALL_CODE_SIZE);
  }

  sli_zigbee_send_ipc_cmd(sli_zigbee_stack_gp_validate_install_code_crc_process_ipc_command, &msg);

  return msg.data.gp_validate_install_code_crc.response.result;
}
