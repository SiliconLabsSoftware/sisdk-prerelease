/***************************************************************************//**
 * @file
 * @brief CLI for APS Link Key Authorization feature.
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

#include "aps-link-key-authorization.h"

#ifdef SL_CATALOG_ZIGBEE_SIMULATION_PRESENT
#include "aps-link-key-authorization-zigbee-simulation-config.h"
#else
#include "aps-link-key-authorization-config.h"
#endif

#ifdef SL_ZIGBEE_SCRIPTED_TEST
#include "app/framework/util/af-main.h" // sl_zigbee_af_get_binding_table_size
#include "app/framework/plugin/aps-link-key-authorization/aps-link-key-authorization-test.h"
#endif

static void printEnabledState(void)
{
  sl_zigbee_af_cli_println(" APS link key authorization is %s.",
                           (sl_zigbee_af_aps_link_key_authorization_is_enabled()) ? "enabled" : "disabled");
}
// plugin aps-link-key-authorization info
void sl_zigbee_af_aps_link_key_authorization_cli_info(sl_cli_command_arg_t *args)
{
  UNUSED_VAR(args);

  uint8_t exemptListCount;
  uint16_t exemptList[SL_ZIGBEE_AF_PLUGIN_APS_LINK_KEY_AUTHORIZATION_MAX_EXEMPT_CLUSTERS];

  printEnabledState();

  sl_zigbee_af_aps_link_key_authorization_get_exempt_cluster_list(&exemptListCount, exemptList);

  sl_zigbee_af_cli_println(" Exempt list:\n   max length: %d\n   actual length: %d",
                           SL_ZIGBEE_AF_PLUGIN_APS_LINK_KEY_AUTHORIZATION_MAX_EXEMPT_CLUSTERS, exemptListCount);

  if (exemptListCount) {
    uint16_t i;
    sl_zigbee_af_cli_print("   clusters: ");

    for (i = 0; i < exemptListCount; i++) {
      sl_zigbee_af_cli_print("0x%04X ", exemptList[i]);
    }
    sl_zigbee_af_cli_println("");
  }
}

// plugin aps-link-key-authorization enable <enable>
void sl_zigbee_af_aps_link_key_authorization_cli_enable(sl_cli_command_arg_t *args)
{
  sl_zigbee_af_aps_link_key_authorization_enable(
    (bool)sl_cli_get_argument_int8(args, 0));
  printEnabledState();
}

// plugin aps-link-key-authorization enable-with-exempt-list <enable> <exemptList>
// <exemptList> is expected in 16-bit hex format, for example: 0x0001 0xACDC 0xBEEF
void sl_zigbee_af_aps_link_key_authorization_cli_enable_with_exempt_list(sl_cli_command_arg_t *args)
{
  sl_status_t retVal;
  uint16_t cliExemptList[SL_ZIGBEE_AF_PLUGIN_APS_LINK_KEY_AUTHORIZATION_MAX_EXEMPT_CLUSTERS];
  uint8_t cliExemptListCount = sl_cli_get_argument_count(args) - 1;
  uint8_t i;
  // Store the list elements
  for (i = 1; i <= cliExemptListCount; i++) {
    cliExemptList[i - 1] = sl_cli_get_argument_uint16(args, i);
  }

  if (sl_cli_get_argument_int8(args, 0)) {
    retVal = sl_zigbee_af_aps_link_key_authorization_enable_with_exempt_cluster_list(cliExemptListCount, (uint8_t *)cliExemptList);
  } else {
    retVal = sl_zigbee_af_aps_link_key_authorization_disable_with_exempt_cluster_list(cliExemptListCount, (uint8_t *)cliExemptList);
  }

  printEnabledState();

  if (retVal != SL_STATUS_OK) {
    sl_zigbee_af_cli_println("Failed to store all clusters to exempt list, list is full.");
  } else {
    sl_zigbee_af_cli_println("All clusters are stored to exempt list successfully.");
  }
}

// plugin aps-link-key-authorization clear-exempt-list
void sl_zigbee_af_aps_link_key_authorization_cli_clear(sl_cli_command_arg_t *arguments)
{
  (void) arguments;
  sl_zigbee_af_aps_link_key_authorization_clear_exempt_cluster_list();
  sl_zigbee_af_cli_println(" Exempt list is cleared.");
}

// plugin aps-link-key-authorization add-cluster-exempted <cluster>
void sl_zigbee_af_aps_link_key_authorization_cli_add_cluster_exempted(sl_cli_command_arg_t *args)
{
  sl_zigbee_af_aps_link_key_authorization_add_cluster_exempted(sl_cli_get_argument_uint16(args, 0));
}
