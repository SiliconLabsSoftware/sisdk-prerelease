/***************************************************************************//**
 * @file sl_wisun_direct_connect_pmk_cli.c
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

#include <stdio.h>

#include "sl_cli.h"
#include "sl_status.h"
#include "sl_wisun_cli_core.h"
#include "sl_wisun_direct_connect_pmk.h"
#include "sl_wisun_direct_connect_pmk_cli.h"

void sl_wisun_set_direct_connect_pmk_cli(sl_cli_command_arg_t *arguments)
{
  sl_status_t status;
  const char *pmk_str;

  app_wisun_cli_mutex_lock();

  if (sl_cli_get_argument_count(arguments) != 1U) {
    printf("[Failed: usage: set_dc_pmk <pmk>]\r\n");
    goto cleanup;
  }

  pmk_str = sl_cli_get_argument_string(arguments, 0);

  status = sl_wisun_direct_connect_pmk_import(pmk_str);
  if (status != SL_STATUS_OK) {
    printf("[Failed: Direct Connect PMK import failed]\r\n");
    goto cleanup;
  }

  printf("[Direct Connect PMK updated]\r\n");

cleanup:
  app_wisun_cli_mutex_unlock();
}
