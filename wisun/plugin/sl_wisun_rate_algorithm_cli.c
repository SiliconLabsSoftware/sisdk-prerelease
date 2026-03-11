/***************************************************************************//**
 * @file sl_wisun_rate_algorithm_cli.c
 * @brief cli commands for wisun rate algorithm
 *******************************************************************************
 * # License
 * <b>Copyright 2022 Silicon Laboratories Inc. www.silabs.com</b>
 *******************************************************************************
 *
 * SPDX-License-Identifier: Zlib
 *
 * The licensor of this software is Silicon Laboratories Inc.
 *
 * This software is provided 'as-is', without any express or implied
 * warranty. In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 *
 ******************************************************************************/

// -----------------------------------------------------------------------------
//                                   Includes
// -----------------------------------------------------------------------------
#include <stdio.h>
#include <string.h>
#include "sl_assert.h"
#include "sl_component_catalog.h"
#include "sl_wisun_api.h"
#include "sl_cli.h"
#include "sl_wisun_cli_core.h"
#include "sl_wisun_api.h"
#include "sl_wisun_cli_util.h"
#include "sl_wisun_rate_algorithm_api.h"

// -----------------------------------------------------------------------------
//                              Macros and Typedefs
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
//                          Static Function Declarations
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
//                                Static Variables
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
//                                Global Variables
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
//                          Public Function Definitions
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
//                          Static Function Definitions
// -----------------------------------------------------------------------------

/* CLI app set rate algorithm */
void app_rate_algorithm(sl_cli_command_arg_t *arguments)
{
  sl_status_t res;
  sl_wisun_mac_address_t address;
  uint8_t type = sl_cli_get_argument_uint8(arguments, 0);
  char *address_str = "ff:ff:ff:ff:ff:ff:ff:ff";

  app_wisun_cli_mutex_lock();

  if (sl_cli_get_argument_count(arguments) > 1) {
    // to get the second argument that is the specified address
    address_str = sl_cli_get_argument_string(arguments, 1);
    if (address_str == NULL) {
      printf("[Failed: invalid address string argument]\r\n");
      goto cleanup;
    }
  }

  // Attempt to convert the MAC address string
  res = app_util_get_mac_address(&address, address_str);
  if (res != SL_STATUS_OK) {
    printf("[Failed: unable to parse the MAC address: %lu]\r\n", res);
    goto cleanup;
  }

  res = sl_wisun_config_rate_algorithm(type,  &address);
  switch (res) {
    case SL_STATUS_OK:
      printf("[Rate algorithm succeeded]\r\n");
      break;
    case SL_STATUS_NOT_SUPPORTED:
      printf("[Mode switch feature not supported on this chip]\r\n");
      break;
    default:
      printf("[Rate algorithm failed]\r\n");
  }

cleanup:
  app_wisun_cli_mutex_unlock();
}

/* CLI app set rate algorithm */
void app_get_rate_algorithm_stats(sl_cli_command_arg_t *arguments)
{
  sl_status_t res;
  sl_wisun_mac_address_t address;
  sl_wisun_rate_t rates[SL_WISUN_MAX_PHY_MODE_ID_COUNT];
  uint16_t rate_count = SL_WISUN_MAX_PHY_MODE_ID_COUNT;
  char *address_str = "ff:ff:ff:ff:ff:ff:ff:ff";

  app_wisun_cli_mutex_lock();

  if (sl_cli_get_argument_count(arguments) > 0) {
    // to get the third argument that is the specified address
    address_str = sl_cli_get_argument_string(arguments, 0);
    if (address_str == NULL) {
      printf("[Failed: invalid address string argument]\r\n");
      goto cleanup;
    }
  }

  // Attempt to convert the MAC address string
  res = app_util_get_mac_address(&address, address_str);
  if (res != SL_STATUS_OK) {
    printf("[Failed: unable to parse the MAC address: %lu]\r\n", res);
    goto cleanup;
  }

  res = sl_wisun_get_rate_algorithm_stats(&address, rates, &rate_count);

  switch (res) {
    case SL_STATUS_OK:
      printf("RATE_STATS[\r\n");
      printf("pmid|prob|trpt|time|count|success|fails\r\n");

      for (uint8_t idx = 0; idx < rate_count; idx++) {
        printf("%d|%d|%d|%d|%"PRIu32"|%"PRIu32"|%"PRIu32"\r\n", rates[idx].phy_mode_id,
                                                                (int)(rates[idx].stats.success_probability*100),
                                                                (int)(rates[idx].stats.throughput*1000),
                                                                (int)(rates[idx].stats.frame_duration_us/1000),
                                                                rates[idx].stats.sample_count,
                                                                rates[idx].stats.success_count,
                                                                rates[idx].stats.fail_count);
      }
      printf("]\r\n");
      break;
    default:
      printf("[Failed to export stats %d]\r\n", rate_count);
  }

cleanup:
  app_wisun_cli_mutex_unlock();
}
