/***************************************************************************//**
 * @file sl_zigbee_mac_mux_aux_ipc_command_messages.h
 * @brief defines structured format for 'sl_zigbee_mac_mux_aux' ipc messages
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
#ifndef SL_ZIGBEE_MAC_MUX_AUX_IPC_COMMAND_MESSAGES_H
#define SL_ZIGBEE_MAC_MUX_AUX_IPC_COMMAND_MESSAGES_H

#include "app/framework/include/sl_zigbee_mac_mux_aux.h"
#include "stack/internal/inc/sl_zigbee_mac_mux_aux_internal_def.h"

typedef struct {
  uint8_t result;
} sli_zigbee_stack_mac_mux_aux_get_channel_ipc_rsp_t;

typedef struct {
  sli_zigbee_stack_mac_mux_aux_get_channel_ipc_rsp_t response;
} sli_zigbee_stack_mac_mux_aux_get_channel_ipc_msg_t;

typedef struct {
  bool result;
} sli_zigbee_stack_mac_mux_aux_is_interpan_enabled_ipc_rsp_t;

typedef struct {
  sli_zigbee_stack_mac_mux_aux_is_interpan_enabled_ipc_rsp_t response;
} sli_zigbee_stack_mac_mux_aux_is_interpan_enabled_ipc_msg_t;

typedef struct {
  uint8_t message[MAX_IPC_VEC_ARG_CAPACITY];
  uint8_t message_length;
  sl_zigbee_transmit_priority_t priority;
  bool useCca;
} sli_zigbee_stack_mac_mux_aux_send_raw_message_ipc_req_t;

typedef struct {
  sl_status_t result;
} sli_zigbee_stack_mac_mux_aux_send_raw_message_ipc_rsp_t;

typedef struct {
  sli_zigbee_stack_mac_mux_aux_send_raw_message_ipc_req_t request;
  sli_zigbee_stack_mac_mux_aux_send_raw_message_ipc_rsp_t response;
} sli_zigbee_stack_mac_mux_aux_send_raw_message_ipc_msg_t;

typedef struct {
  bool enable;
} sli_zigbee_stack_mac_mux_aux_set_interpan_enabled_ipc_req_t;

typedef struct {
  sli_zigbee_stack_mac_mux_aux_set_interpan_enabled_ipc_req_t request;
} sli_zigbee_stack_mac_mux_aux_set_interpan_enabled_ipc_msg_t;

#endif // SL_ZIGBEE_MAC_MUX_AUX_IPC_COMMAND_MESSAGES_H
