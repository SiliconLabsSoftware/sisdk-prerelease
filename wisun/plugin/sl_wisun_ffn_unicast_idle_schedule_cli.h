/***************************************************************************//**
 * @file sl_wisun_ffn_unicast_idle_schedule_cli.h
 * @brief CLI command for the Wi-SUN FFN unicast idle schedule
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

#ifndef SL_WISUN_FFN_UNICAST_IDLE_SCHEDULE_CLI_H
#define SL_WISUN_FFN_UNICAST_IDLE_SCHEDULE_CLI_H

#include "sl_cli.h"

/***************************************************************************//**
 * @brief CLI handler for wisun set_ffn_unicast_idle_schedule.
 *
 * Arguments: listen_slots (uint8), idle_slots (uint8). Calls
 * sl_wisun_set_ffn_unicast_idle_schedule() and prints the result.
 ******************************************************************************/
void sl_wisun_ffn_unicast_idle_schedule_cli_set(sl_cli_command_arg_t *arguments);

#endif // SL_WISUN_FFN_UNICAST_IDLE_SCHEDULE_CLI_H
