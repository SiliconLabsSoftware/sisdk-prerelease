/***************************************************************************//**
 * @file
 * @brief CLI for the Compact Logger plugin.
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
#include "compact-logger.h"

// plugin compact-logger clear
void sli_zigbee_af_compact_logger_clear_log(sl_cli_command_arg_t *args)
{
  UNUSED_VAR(args);
  sl_zigbee_af_compact_logger_init();
}

// plugin compact-logger set-severity
void sli_zigbee_af_compact_logger_set_severity(sl_cli_command_arg_t *args)
{
  sl_status_t status;
  uint8_t severity = sl_cli_get_argument_uint8(args, 0);
  status = sl_zigbee_af_compact_logger_set_severity(severity);
  sl_zigbee_af_cli_println("Set severity to %d, status=0x%02X", severity, status);
}

// plugin compact-logger set-facility
void sli_zigbee_af_compact_logger_set_facility(sl_cli_command_arg_t *args)
{
  uint16_t facility = sl_cli_get_argument_uint16(args, 0);
  sl_zigbee_af_compact_logger_set_facility(facility);
  sl_zigbee_af_cli_println("Set facility to 0x%04X", facility);
}
