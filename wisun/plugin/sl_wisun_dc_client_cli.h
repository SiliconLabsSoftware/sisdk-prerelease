/***************************************************************************//**
 * @file sl_wisun_dc_client_cli.h
 * @brief CLI commands and event handlers for the Wi-SUN Direct Connect client
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

#ifndef SL_WISUN_DC_CLIENT_CLI_H
#define SL_WISUN_DC_CLIENT_CLI_H

#include "sl_cli.h"
#include "sl_wisun_api.h"

void sl_wisun_dc_client_cli_start(sl_cli_command_arg_t *arguments);
void sl_wisun_dc_client_cli_stop(sl_cli_command_arg_t *arguments);
void sl_wisun_dc_client_cli_scan(sl_cli_command_arg_t *arguments);
void sl_wisun_dc_client_cli_stop_scan(sl_cli_command_arg_t *arguments);
void sl_wisun_dc_client_cli_connect(sl_cli_command_arg_t *arguments);

void sl_wisun_dc_client_cli_handle_id_received(sl_wisun_evt_t *evt);
void sl_wisun_dc_client_cli_handle_client_state_changed(sl_wisun_evt_t *evt);

#endif /* SL_WISUN_DC_CLIENT_CLI_H */
