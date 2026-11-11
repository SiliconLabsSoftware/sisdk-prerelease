/***************************************************************************//**
 * @file sl_zigbee_mac_mux_aux_baremetal_wrapper.c
 * @brief internal implementations for 'sl_zigbee_mac_mux_aux' as a thin-wrapper
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

uint8_t sl_zigbee_mac_mux_aux_get_channel(void)
{
  return sli_zigbee_stack_mac_mux_aux_get_channel();
}

bool sl_zigbee_mac_mux_aux_is_interpan_enabled(void)
{
  return sli_zigbee_stack_mac_mux_aux_is_interpan_enabled();
}

sl_status_t sl_zigbee_mac_mux_aux_send_raw_message(const uint8_t *message,
                                                   uint8_t message_length,
                                                   sl_zigbee_transmit_priority_t priority,
                                                   bool useCca)
{
  return sli_zigbee_stack_mac_mux_aux_send_raw_message(message,
                                                       message_length,
                                                       priority,
                                                       useCca);
}

void sl_zigbee_mac_mux_aux_set_interpan_enabled(bool enable)
{
  sli_zigbee_stack_mac_mux_aux_set_interpan_enabled(enable);
}
