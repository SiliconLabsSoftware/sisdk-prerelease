/***************************************************************************//**
 * @file
 * @brief Host routines for the Zigbee Event Logger plugin.
 *******************************************************************************
 * # License
 * <b>Copyright 2018 Silicon Laboratories Inc. www.silabs.com</b>
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

#include PLATFORM_HEADER
#include "app/framework/include/af.h"
#include "stack/include/zigbee-event-logger-gen.h"
#include "compact-logger.h"

void sli_zigbee_af_zigbee_event_logger_switch_network_key_callback(uint8_t sequenceNumber)
{
  sl_zigbee_af_zig_bee_event_logger_add_network_key_sequence_change(sequenceNumber);
}

void sli_zigbee_af_zigbee_event_logger_child_join_callback(uint8_t index,
                                                           bool joining,
                                                           sl_802154_short_addr_t childId,
                                                           sl_802154_long_addr_t childEui64,
                                                           sl_zigbee_node_type_t childType)
{
  if (joining) {
    sl_zigbee_af_zig_bee_event_logger_add_child_added(childId,
                                                      childEui64);
  } else {
    sl_zigbee_af_zig_bee_event_logger_add_child_removed(childId,
                                                        childEui64);
  }
}

void sli_zigbee_af_zigbee_event_logger_duty_cycle_callback(uint8_t channelPage,
                                                           uint8_t channel,
                                                           sl_zigbee_duty_cycle_state_t state,
                                                           uint8_t totalDevices,
                                                           sl_zigbee_per_device_duty_cycle_t *arrayOfDeviceDutyCycles)
{
  sl_status_t sl_zigbee_status;
  sl_zigbee_duty_cycle_limits_t dutyCycleLimits;

  sl_zigbee_status = sl_zigbee_ezsp_get_duty_cycle_limits(&dutyCycleLimits);

  if (sl_zigbee_status == SL_STATUS_OK) {
    sl_zigbee_af_zig_bee_event_logger_add_duty_cycle_change(state,
                                                            dutyCycleLimits.limitThresh,
                                                            dutyCycleLimits.critThresh,
                                                            dutyCycleLimits.suspLimit);
  }
}
