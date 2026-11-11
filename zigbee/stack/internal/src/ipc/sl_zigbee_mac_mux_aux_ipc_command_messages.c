/***************************************************************************//**
 * @file sl_zigbee_mac_mux_aux_ipc_command_messages.c
 * @brief internal wrappers for 'sl_zigbee_mac_mux_aux' ipc commands
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
// automatically generated from sl_zigbee_mac_mux_aux.h.  Do not manually edit
#include "app/framework/include/sl_zigbee_mac_mux_aux.h"
#include "stack/internal/inc/sl_zigbee_mac_mux_aux_internal_def.h"
#include "stack/internal/src/ipc/sl_zigbee_mac_mux_aux_ipc_command_messages.h"
#include "stack/internal/src/ipc/zigbee_ipc_command_messages.h"

// ipc command dispatch

void sli_zigbee_stack_mac_mux_aux_get_channel_process_ipc_command(sli_zigbee_ipc_cmd_t *msg)
{
  msg->data.mac_mux_aux_get_channel.response.result = sli_zigbee_stack_mac_mux_aux_get_channel();
}

void sli_zigbee_stack_mac_mux_aux_is_interpan_enabled_process_ipc_command(sli_zigbee_ipc_cmd_t *msg)
{
  msg->data.mac_mux_aux_is_interpan_enabled.response.result = sli_zigbee_stack_mac_mux_aux_is_interpan_enabled();
}

void sli_zigbee_stack_mac_mux_aux_send_raw_message_process_ipc_command(sli_zigbee_ipc_cmd_t *msg)
{
  msg->data.mac_mux_aux_send_raw_message.response.result = sli_zigbee_stack_mac_mux_aux_send_raw_message(msg->data.mac_mux_aux_send_raw_message.request.message,
                                                                                                         msg->data.mac_mux_aux_send_raw_message.request.message_length,
                                                                                                         msg->data.mac_mux_aux_send_raw_message.request.priority,
                                                                                                         msg->data.mac_mux_aux_send_raw_message.request.useCca);
}

void sli_zigbee_stack_mac_mux_aux_set_interpan_enabled_process_ipc_command(sli_zigbee_ipc_cmd_t *msg)
{
  sli_zigbee_stack_mac_mux_aux_set_interpan_enabled(msg->data.mac_mux_aux_set_interpan_enabled.request.enable);
}

// public entrypoints

uint8_t sl_zigbee_mac_mux_aux_get_channel(void)
{
  sli_zigbee_ipc_cmd_t msg = { 0, };

  sli_zigbee_send_ipc_cmd(sli_zigbee_stack_mac_mux_aux_get_channel_process_ipc_command, &msg);

  return msg.data.mac_mux_aux_get_channel.response.result;
}

bool sl_zigbee_mac_mux_aux_is_interpan_enabled(void)
{
  sli_zigbee_ipc_cmd_t msg = { 0, };

  sli_zigbee_send_ipc_cmd(sli_zigbee_stack_mac_mux_aux_is_interpan_enabled_process_ipc_command, &msg);

  return msg.data.mac_mux_aux_is_interpan_enabled.response.result;
}

sl_status_t sl_zigbee_mac_mux_aux_send_raw_message(const uint8_t *message,
                                                   uint8_t message_length,
                                                   sl_zigbee_transmit_priority_t priority,
                                                   bool useCca)
{
  sli_zigbee_ipc_cmd_t msg = { 0, };

  if (message_length > MAX_IPC_VEC_ARG_CAPACITY) {
    assert(false); // "vector message length exceeds expected maximum
    return msg.data.mac_mux_aux_send_raw_message.response.result;
  }

  memmove(msg.data.mac_mux_aux_send_raw_message.request.message, message, sizeof(uint8_t) * message_length);
  msg.data.mac_mux_aux_send_raw_message.request.message_length = message_length;
  msg.data.mac_mux_aux_send_raw_message.request.priority = priority;
  msg.data.mac_mux_aux_send_raw_message.request.useCca = useCca;
  sli_zigbee_send_ipc_cmd(sli_zigbee_stack_mac_mux_aux_send_raw_message_process_ipc_command, &msg);

  return msg.data.mac_mux_aux_send_raw_message.response.result;
}

void sl_zigbee_mac_mux_aux_set_interpan_enabled(bool enable)
{
  sli_zigbee_ipc_cmd_t msg = { 0, };
  msg.data.mac_mux_aux_set_interpan_enabled.request.enable = enable;
  sli_zigbee_send_ipc_cmd(sli_zigbee_stack_mac_mux_aux_set_interpan_enabled_process_ipc_command, &msg);
}
