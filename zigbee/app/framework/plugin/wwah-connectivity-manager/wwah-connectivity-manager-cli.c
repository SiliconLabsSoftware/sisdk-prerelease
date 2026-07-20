/***************************************************************************//**
 * @file
 * @brief CLI for the WWAH Connectivity Manager plugin.
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
#include "wwah-connectivity-manager.h"

#include "app/util/serial/sl_zigbee_command_interpreter.h"

void sli_zigbee_af_wwah_connectivity_manager_info_command(sl_cli_command_arg_t *arguments)
{
  UNUSED_VAR(arguments);
  wwahConnectivityManagerPrintInfo();
}

void sli_zigbee_af_wwah_connectivity_manager_enable_command(sl_cli_command_arg_t *arguments)
{
  uint8_t endpoint = sl_cli_get_argument_uint8(arguments, 0);
  sl_zigbee_af_wwah_connectivity_manager_enable_rejoin_algorithm(endpoint);
}

void sli_zigbee_af_wwah_connectivity_manager_disable_command(sl_cli_command_arg_t *arguments)
{
  uint8_t endpoint = sl_cli_get_argument_uint8(arguments, 0);
  sl_zigbee_af_wwah_connectivity_manager_disable_rejoin_algorithm(endpoint);
}

void sli_zigbee_af_wwah_connectivity_manager_show_command(sl_cli_command_arg_t *arguments)
{
  UNUSED_VAR(arguments);
  wwahConnectivityManagerShow();
}
// e.g. plugin wwah-connectivity-manager parent-recovery 1 0
// badParentRejoinPeriod == 0 means use the default value of 24 hours (24*60 mins)

void sli_zigbee_af_wwah_connectivity_manager_bad_parent_recovery_command(sl_cli_command_arg_t *arguments)
{
  uint8_t enabled = sl_cli_get_argument_uint8(arguments, 0);
  uint16_t badParentRejoinPeriod = sl_cli_get_argument_uint16(arguments, 1);
  if (enabled) {
    sl_zigbee_af_wwah_connectivity_manager_enable_bad_parent_recovery(badParentRejoinPeriod);
  } else {
    sl_zigbee_af_wwah_connectivity_manager_disable_bad_parent_recovery();
  }
}
// e.g. plugin wwah-connectivity-manager minRssi 50
void sli_zigbee_af_wwah_connectivity_manager_set_min_rssi_command(sl_cli_command_arg_t *arguments)
{
  int8_t rssi = sl_cli_get_argument_int8(arguments, 0);
  sl_zigbee_beacon_classification_params_t param;
  sl_zigbee_get_beacon_classification_params(&param);
  param.minRssiForReceivingPkts = rssi;
  sl_zigbee_set_beacon_classification_params(&param);
}
