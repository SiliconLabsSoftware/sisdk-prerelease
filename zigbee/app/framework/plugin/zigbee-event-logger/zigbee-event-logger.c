/***************************************************************************//**
 * @file
 * @brief Routines for the Zigbee Event Logger plugin, which provide a means of
 *        recording zigbee network events in a ring buffer.
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
#include "zigbee-event-logger-internal.h"
#include "stack/include/zigbee-event-logger-gen.h"
#include "compact-logger.h"
#include "stack/config/sl_zigbee_token_defines.h"
#include "stack/include/sl_zigbee_token.h"

void sl_zigbee_af_zigbee_event_logger_init_cb(uint8_t init_level)
{
  (void)init_level;

  sl_zigbee_af_compact_logger_init();
#ifndef EZSP_HOST
  uint32_t bootCount;
  sl_status_t status = slx_zigbee_token_manager_get_data(COMMON_TOKEN_STACK_BOOT_COUNTER, (void *)&bootCount, sizeof(uint32_t));
  if (status != SL_STATUS_OK) {
    return;
  }

  uint16_t resetType = halGetExtendedResetInfo();
  sl_zigbee_af_zig_bee_event_logger_add_reset(RESET_BASE_TYPE(resetType),
                                              RESET_EXTENDED_FIELD(resetType));
  sl_zigbee_af_zig_bee_event_logger_add_boot_event(bootCount, resetType);
#endif // EZSP_HOST
}

void sl_zigbee_af_zigbee_event_logger_stack_status_cb(sl_status_t status)
{
  sl_status_t retVal;
  sl_zigbee_network_parameters_t networkParameters;
  sl_zigbee_multi_phy_radio_parameters_t radioParameters;
  sl_zigbee_node_type_t nodeType;

  // We'll do a join network message (if applicable), followed by a stack status
  // message

  if ((status == SL_STATUS_NETWORK_UP) || (status == SL_STATUS_ZIGBEE_CHANNEL_CHANGED)) {
    retVal = sl_zigbee_get_network_parameters(&nodeType, &networkParameters);
    if (retVal == SL_STATUS_OK) {
      // The following does not work for simultaneous dual radio devices
      retVal = sl_zigbee_get_radio_parameters(0, &radioParameters);
    }
    if (retVal == SL_STATUS_OK) {
      if (status == SL_STATUS_NETWORK_UP) {
        sl_zigbee_af_zig_bee_event_logger_add_join_network(networkParameters.panId,
                                                           networkParameters.radioChannel,
                                                           radioParameters.radioPage,
                                                           networkParameters.extendedPanId);
      } else {
        sl_zigbee_af_zig_bee_event_logger_add_channel_change(radioParameters.radioPage,
                                                             networkParameters.radioChannel);
      }
    }
  }

  sl_zigbee_af_zig_bee_event_logger_add_stack_status(status);
}

void sl_zigbee_af_zigbee_event_logger_zigbee_key_establishment_cb(sl_802154_long_addr_t partner,
                                                                  sl_zigbee_key_status_t status)
{
  sl_status_t sl_zigbee_status;
  sl_802154_long_addr_t trustCenterEui64;

  if (status == SL_ZIGBEE_VERIFY_LINK_KEY_SUCCESS) {
    sl_zigbee_status = sl_zigbee_lookup_eui64_by_node_id(SL_ZIGBEE_TRUST_CENTER_NODE_ID,
                                                         trustCenterEui64);
    if ((sl_zigbee_status == SL_STATUS_OK)
        && (0 == memcmp(partner, trustCenterEui64, EUI64_SIZE))) {
      sl_zigbee_af_zig_bee_event_logger_add_trust_center_link_key_change();
    }
  }
}

void sl_zigbee_af_compact_logger_utc_time_set_cb(uint32_t currentUtcTimeSeconds)
{
  uint32_t secondsSinceBoot = halCommonGetInt32uMillisecondTick()
                              / MILLISECOND_TICKS_PER_SECOND;
  sl_zigbee_af_zig_bee_event_logger_add_time_sync(currentUtcTimeSeconds,
                                                  secondsSinceBoot);
}
