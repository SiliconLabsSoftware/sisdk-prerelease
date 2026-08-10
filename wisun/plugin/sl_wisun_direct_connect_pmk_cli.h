/***************************************************************************//**
 * @file sl_wisun_direct_connect_pmk_cli.h
 * @brief CLI commands for Wi-SUN Direct Connect PMK provisioning
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

#ifndef SL_WISUN_DIRECT_CONNECT_PMK_CLI_H
#define SL_WISUN_DIRECT_CONNECT_PMK_CLI_H

#include "sl_cli.h"

/***************************************************************************//**
 * @brief CLI handler for wisun set_dc_pmk.
 *
 * Parses the PMK argument, imports it into #SL_WISUN_DIRECT_CONNECT_PMK_KEY_ID,
 * and prints the result.
 ******************************************************************************/
void sl_wisun_set_direct_connect_pmk_cli(sl_cli_command_arg_t *arguments);

#endif // SL_WISUN_DIRECT_CONNECT_PMK_CLI_H
