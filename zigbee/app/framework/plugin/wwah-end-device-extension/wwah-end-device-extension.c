/***************************************************************************//**
 * @file
 * @brief Routines for the WWAH End Device Extension plugin, which sets up
 *        additional configuration needed by end devices operating on a WWAH
 *        network.
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

#include "app/framework/include/af.h"
#include "wwah-end-device-extension.h"
#include "../end-device-support/end-device-support.h"

#include "wwah-end-device-extension-config.h"
static sl_zigbee_af_event_t unenforceFastPollNetworkEvents[SL_ZIGBEE_SUPPORTED_NETWORKS];
static void unenforceFastPollNetworkEventHandler(sl_zigbee_af_event_t * event);

void sl_zigbee_af_wwah_end_device_extension_stack_status_cb(sl_status_t status)
{
  if (status == SL_STATUS_NETWORK_UP) {
    sl_zigbee_af_add_to_current_app_tasks_cb(SL_ZIGBEE_AF_FORCE_SHORT_POLL);
    sl_zigbee_af_event_set_delay_ms(unenforceFastPollNetworkEvents,
                                    SL_ZIGBEE_AF_PLUGIN_WWAH_END_DEVICE_EXTENSION_FAST_POLL_ON_NETWORK_UP * 1000);
  } else if (status == SL_STATUS_NETWORK_DOWN) {
    sl_zigbee_af_event_set_inactive(unenforceFastPollNetworkEvents);
  }
}

void sli_zigbee_af_wwah_end_device_extension_init_callback(uint8_t init_level)
{
  (void)init_level;

  sl_zigbee_af_network_event_init(unenforceFastPollNetworkEvents,
                                  unenforceFastPollNetworkEventHandler);
}

static void unenforceFastPollNetworkEventHandler(sl_zigbee_af_event_t * event)
{
  UNUSED_VAR(event);
  sl_zigbee_af_remove_from_current_app_tasks_cb(SL_ZIGBEE_AF_FORCE_SHORT_POLL);
}
