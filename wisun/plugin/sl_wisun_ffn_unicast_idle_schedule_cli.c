/***************************************************************************//**
 * @file sl_wisun_ffn_unicast_idle_schedule_cli.c
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

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>

#include "sl_cli.h"
#include "sl_status.h"
#include "sl_wisun_api.h"
#include "sl_wisun_cli_core.h"
#include "sl_wisun_ffn_unicast_idle_schedule_cli.h"

void sl_wisun_ffn_unicast_idle_schedule_cli_set(sl_cli_command_arg_t *arguments)
{
  sl_status_t ret;
  uint8_t listen_slots;
  uint8_t idle_slots;

  app_wisun_cli_mutex_lock();

  listen_slots = sl_cli_get_argument_uint8(arguments, 0);
  idle_slots = sl_cli_get_argument_uint8(arguments, 1);

  ret = sl_wisun_set_ffn_unicast_idle_schedule(listen_slots, idle_slots);
  if (ret == SL_STATUS_OK) {
    printf("[FFN unicast idle schedule set: listen=%u idle=%u]\r\n",
           (unsigned)listen_slots, (unsigned)idle_slots);
  } else {
    printf("[Failed: unable to set FFN unicast idle schedule: %"PRIu32"]\r\n", ret);
  }

  app_wisun_cli_mutex_unlock();
}
