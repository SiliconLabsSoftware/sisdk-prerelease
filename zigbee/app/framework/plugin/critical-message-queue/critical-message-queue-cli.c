/***************************************************************************//**
 * @file
 * @brief CLI for the Critical Message Queue plugin.
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
#include "critical-message-queue.h"
void sli_zigbee_af_critical_message_print_queue_command(sl_cli_command_arg_t *arguments)
{
  UNUSED_VAR(arguments);
  sl_zigbee_af_critical_message_queue_print_queue();
}

void sli_zigbee_af_critical_message_print_config_command(sl_cli_command_arg_t *arguments)
{
  UNUSED_VAR(arguments);
  sl_zigbee_af_critical_message_queue_print_config();
}
