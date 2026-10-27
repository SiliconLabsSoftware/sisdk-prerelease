/***************************************************************************//**
 * @file
 * @brief CLI for Green Power v1.1.3 OOB key and install-code pre-configuration.
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

#include "af.h"
#include "app/util/serial/sl_zigbee_command_interpreter.h"
#include "green-power-v113.h"
#include "stack/include/gp-install-code.h"

void sl_zigbee_af_green_power_server_cli_set_oob_key(SL_CLI_COMMAND_ARG)
{
  sl_zigbee_gp_address_t gpdAddr = { 0 };
  uint8_t key[SL_ZIGBEE_ENCRYPTION_KEY_SIZE];
  sl_status_t status;

  gpdAddr.applicationId = SL_ZIGBEE_GP_APPLICATION_SOURCE_ID;
  gpdAddr.id.sourceId = sl_cli_get_argument_uint32(arguments, 0);
  sl_zigbee_copy_hex_arg(arguments, 1, key, SL_ZIGBEE_ENCRYPTION_KEY_SIZE, true);
  status = sl_zigbee_af_gp_set_oob_key(&gpdAddr, key);
  sl_zigbee_af_cli_println("set-oob-key status: 0x%02X srcId=0x%08lX",
                           status,
                           (unsigned long)gpdAddr.id.sourceId);
}

void sl_zigbee_af_green_power_server_cli_clear_oob_key(SL_CLI_COMMAND_ARG)
{
  sl_zigbee_gp_address_t gpdAddr = { 0 };
  sl_status_t status;

  gpdAddr.applicationId = SL_ZIGBEE_GP_APPLICATION_SOURCE_ID;
  gpdAddr.id.sourceId = sl_cli_get_argument_uint32(arguments, 0);
  status = sl_zigbee_af_gp_clear_oob_key(&gpdAddr);
  sl_zigbee_af_cli_println("clear-oob-key status: 0x%02X srcId=0x%08lX",
                           status,
                           (unsigned long)gpdAddr.id.sourceId);
}

void sl_zigbee_af_green_power_server_cli_register_install_code(SL_CLI_COMMAND_ARG)
{
  sl_zigbee_gp_address_t gpdAddr = { 0 };
  uint8_t ic[SL_ZIGBEE_GP_INSTALL_CODE_SIZE];
  sl_status_t status;

  gpdAddr.applicationId = SL_ZIGBEE_GP_APPLICATION_SOURCE_ID;
  gpdAddr.id.sourceId = sl_cli_get_argument_uint32(arguments, 0);
  sl_zigbee_copy_hex_arg(arguments, 1, ic, SL_ZIGBEE_GP_INSTALL_CODE_SIZE, true);
  status = sl_zigbee_gp_tc_register_install_code(&gpdAddr, ic);
  sl_zigbee_af_cli_println("register-install-code status: 0x%02X srcId=0x%08lX",
                           status,
                           (unsigned long)gpdAddr.id.sourceId);
}

void sl_zigbee_af_green_power_server_cli_remove_install_code(SL_CLI_COMMAND_ARG)
{
  sl_zigbee_gp_address_t gpdAddr = { 0 };
  sl_status_t status;

  gpdAddr.applicationId = SL_ZIGBEE_GP_APPLICATION_SOURCE_ID;
  gpdAddr.id.sourceId = sl_cli_get_argument_uint32(arguments, 0);
  status = sl_zigbee_gp_tc_remove_install_code(&gpdAddr);
  sl_zigbee_af_cli_println("remove-install-code status: 0x%02X srcId=0x%08lX",
                           status,
                           (unsigned long)gpdAddr.id.sourceId);
}

void sl_zigbee_af_green_power_server_cli_set_tc_ic_mode(SL_CLI_COMMAND_ARG)
{
  uint8_t mode = sl_cli_get_argument_uint8(arguments, 0);

  sli_zigbee_gp_tc_set_ic_mode_mandatory(mode != 0U);
  sl_zigbee_af_cli_println("set-tc-ic-mode: %u", (unsigned)(mode != 0U));
}
