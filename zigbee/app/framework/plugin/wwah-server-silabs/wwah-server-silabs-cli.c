/***************************************************************************//**
 * @file
 * @brief CLI for the WWAH Server Silabs plugin.
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

#include "app/util/serial/sl_zigbee_command_interpreter.h"

#include "wwah-server-silabs.h"

extern sl_zigbee_af_zcl_request_status_t sl_zigbee_af_sl_wwah_cluster_enable_configuration_mode_cb(void);
extern sl_zigbee_af_zcl_request_status_t sl_zigbee_af_sl_wwah_cluster_disable_configuration_mode_cb(void);

static void powerNotificationCb(sl_zigbee_power_down_notification_result_t result);

// plugin wwah-server poweringOff <srcEndpoint:1> <dstEndpoint:1>
//                                <reason:1> <mfgId:2>
void sli_zigbee_af_wwah_server_send_powering_off(sl_cli_command_arg_t *arguments)
{
  uint8_t srcEndpoint = sl_cli_get_argument_uint8(arguments, 0);
  uint8_t dstEndpoint = sl_cli_get_argument_uint8(arguments, 1);
  uint8_t reason = sl_cli_get_argument_uint8(arguments, 2);
  uint16_t mfgId = sl_cli_get_argument_uint16(arguments, 3);

  sl_zigbee_af_cli_println("Sending Powering Off Notification");
  sl_zigbee_af_wwah_server_send_powering_off_notification(SL_ZIGBEE_ZIGBEE_COORDINATOR_ADDRESS,
                                                          srcEndpoint,
                                                          dstEndpoint,
                                                          reason,
                                                          mfgId,
                                                          NULL, 0,
                                                          powerNotificationCb);
}

static void powerNotificationCb(sl_zigbee_power_down_notification_result_t result)
{
  sl_zigbee_af_cli_println("Power Notification CB: result=%d", result);
}

// plugin wwah-server poweringOn <srcEndpoint:1> <dstEndpoint:1>
//                               <reason:1> <mfgId:2>
void sli_zigbee_af_wwah_server_send_powering_on(sl_cli_command_arg_t *arguments)
{
  uint8_t srcEndpoint = sl_cli_get_argument_uint8(arguments, 0);
  uint8_t dstEndpoint = sl_cli_get_argument_uint8(arguments, 1);
  uint8_t reason = sl_cli_get_argument_uint8(arguments, 2);
  uint16_t mfgId = sl_cli_get_argument_uint16(arguments, 3);

  sl_zigbee_af_cli_println("Sending Powering On Notification");
  sl_zigbee_af_wwah_server_send_powering_on_notification(SL_ZIGBEE_ZIGBEE_COORDINATOR_ADDRESS,
                                                         srcEndpoint,
                                                         dstEndpoint,
                                                         reason,
                                                         mfgId,
                                                         NULL, 0);
}

// plugin wwah-server confMode <mode:1>
void sli_zigbee_af_sl_wwah_server_configuration_mode(sl_cli_command_arg_t *arguments)
{
  uint8_t mode = sl_cli_get_argument_uint8(arguments, 0);
  if (mode) {
    sl_zigbee_af_sl_wwah_cluster_enable_configuration_mode_cb();
  } else {
    sl_zigbee_af_sl_wwah_cluster_disable_configuration_mode_cb();
  }
}
