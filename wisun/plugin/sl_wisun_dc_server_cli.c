/***************************************************************************//**
 * @file sl_wisun_dc_server_cli.c
 * @brief CLI commands and event handlers for Wi-SUN Direct Connect servers
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
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "sl_status.h"
#include "sl_wisun_cli_core.h"
#include "sl_wisun_dc_server_cli.h"
#include "sl_wisun_direct_connect_pmk_config.h"
#include "sl_wisun_ip6string.h"


#define SL_WISUN_DC_SERVER_CLI_DEFAULT_ID "DC_ID_DEFAULT"

static bool dc_server_enabled;
static bool dc_server_auto_mode;
static sl_wisun_dc_id_t dc_server_auto_id = {
  .id = SL_WISUN_DC_SERVER_CLI_DEFAULT_ID
};

void sl_wisun_dc_server_cli_handle_dc_server_stopped(void)
{
  dc_server_enabled = false;
  dc_server_auto_mode = false;
}

void sl_wisun_dc_server_cli_set_state(sl_cli_command_arg_t *arguments)
{
  sl_status_t status;
  bool do_enable;
  bool auto_mode = false;
  const char *dc_id = NULL;

  app_wisun_cli_mutex_lock();

  do_enable = (bool)sl_cli_get_argument_uint8(arguments, 0);
  if (sl_cli_get_argument_count(arguments) >= 2) {
    auto_mode = (bool)sl_cli_get_argument_uint8(arguments, 1);
  }
  if (sl_cli_get_argument_count(arguments) >= 3) {
    dc_id = sl_cli_get_argument_string(arguments, 2);
  }

  if (do_enable && dc_server_enabled) {
    printf("[Direct Connect server already enabled]\r\n");
    goto cleanup;
  }

  if (do_enable && auto_mode) {
    memset(&dc_server_auto_id, 0, sizeof(dc_server_auto_id));
    strncpy((char *)dc_server_auto_id.id,
            dc_id != NULL ? dc_id : SL_WISUN_DC_SERVER_CLI_DEFAULT_ID,
            SL_WISUN_DC_ID_LEN - 1);
  }

  if (do_enable) {
    status = sl_wisun_set_direct_connect_pmk(SL_WISUN_DIRECT_CONNECT_PMK_KEY_ID);
    if (status != SL_STATUS_OK) {
      printf("[Failed setting Direct Connect PMK: %"PRIu32"]\r\n", status);
      goto cleanup;
    }
  }

  status = sl_wisun_set_direct_connect_state(do_enable);
  if (status != SL_STATUS_OK) {
    printf("[Failed: error %"PRIu32" when setting Direct Connect state]\r\n", status);
  } else {
    if (do_enable) {
      printf("[Direct Connect enabled, mode %s, DC ID %s]\r\n",
             auto_mode ? "auto (advertise/accept)" : "manual",
             auto_mode ? (const char *)dc_server_auto_id.id : "not used");
    } else {
      printf("[Direct Connect disabled]\r\n");
    }

    dc_server_enabled = do_enable;
    dc_server_auto_mode = do_enable && auto_mode;
  }

cleanup:
  app_wisun_cli_mutex_unlock();
}

void sl_wisun_dc_server_cli_accept_link(sl_cli_command_arg_t *arguments)
{
  const char *client_address_string = NULL;
  in6_addr_t client_address;
  sl_status_t status;

  app_wisun_cli_mutex_lock();

  if (sl_cli_get_argument_count(arguments) != 1) {
    printf("[Failed: invalid number of parameters]\r\n");
    goto cleanup;
  }

  client_address_string = sl_cli_get_argument_string(arguments, 0);
  if (!stoip6(client_address_string, strlen(client_address_string), client_address.address)) {
    printf("[Failed: invalid client address: %s]\r\n", client_address_string);
    goto cleanup;
  }

  status = sl_wisun_accept_direct_connect_link(&client_address);
  if (status == SL_STATUS_OK) {
    printf("[Accepted connection request]\r\n");
  } else {
    printf("[Failed: error %"PRIu32" when accepting connection request]\r\n",
           status);
  }

cleanup:
  app_wisun_cli_mutex_unlock();
}

void sl_wisun_dc_server_cli_advertise_id(sl_cli_command_arg_t *arguments)
{
  sl_status_t status;
  char ipv6_string[40];
  in6_addr_t client_address;
  char *client_address_string;
  sl_wisun_dc_id_t dc_id = { 0 };

  app_wisun_cli_mutex_lock();

  if (sl_cli_get_argument_count(arguments) != 2) {
    printf("[Failed: missing parameters]\r\n");
    goto cleanup;
  }

  client_address_string = sl_cli_get_argument_string(arguments, 0);
  if (!stoip6(client_address_string, strlen(client_address_string), client_address.address)) {
    printf("[Failed: invalid client address: %s]\r\n", client_address_string);
    goto cleanup;
  }

  strncpy((char *)dc_id.id,
          sl_cli_get_argument_string(arguments, 1),
          SL_WISUN_DC_ID_LEN - 1);

  status = sl_wisun_advert_direct_connect_server_id(&client_address, &dc_id);
  if (status != SL_STATUS_OK) {
    printf("[Failed: unable to advertise Direct Connect server ID: %"PRIu32"]\r\n",
           status);
    goto cleanup;
  }

  ip6tos(&client_address, ipv6_string);
  printf("[Direct Connect server has sent its DC_ID: %s to client %s]\r\n",
         (char *)dc_id.id,
         ipv6_string);

cleanup:
  app_wisun_cli_mutex_unlock();
}


// Event handlers

void sl_wisun_dc_server_cli_handle_link_available(sl_wisun_evt_t *evt)
{
  char ipv6_string[40];
  sl_status_t status;

  ip6tos(&evt->evt.direct_connect_link_available.link_local_ipv6, ipv6_string);
  printf("[Direct Connection request from %s]\r\n", ipv6_string);

  if (dc_server_enabled && dc_server_auto_mode) {
    status = sl_wisun_accept_direct_connect_link(
      &evt->evt.direct_connect_link_available.link_local_ipv6);
    if (status == SL_STATUS_OK) {
      printf("[Accepted connection request from %s]\r\n", ipv6_string);
    } else {
      printf("[Failed to accept connection from %s: %"PRIu32"]\r\n",
             ipv6_string,
             status);
    }
  }
}

void sl_wisun_dc_server_cli_handle_link_status(sl_wisun_evt_t *evt)
{
  char ipv6_string[40];

  ip6tos(&evt->evt.direct_connect_link_status.link_local_ipv6, ipv6_string);

  switch (evt->evt.direct_connect_link_status.link_status) {
    case SL_WISUN_DIRECT_CONNECT_LINK_STATUS_CONNECTED:
      printf("[Direct Connect Link %s: connected]\r\n", ipv6_string);
      break;
    case SL_WISUN_DIRECT_CONNECT_LINK_STATUS_ERROR:
      printf("[Direct Connect Link %s: error]\r\n", ipv6_string);
      break;
    case SL_WISUN_DIRECT_CONNECT_LINK_STATUS_DISCONNECTED:
      printf("[Direct Connect Link %s: disconnected]\r\n", ipv6_string);
      break;
    default:
      break;
  }
}

void sl_wisun_dc_server_cli_handle_id_solicit(sl_wisun_evt_t *evt)
{
  char ipv6_string[40];
  sl_status_t status;

  ip6tos(&evt->evt.direct_connect_id_solicit.link_local_ipv6, ipv6_string);
  printf("[Direct Connect identity request from client %s with DC_ID:%s]\r\n",
         ipv6_string,
         (char *)evt->evt.direct_connect_id_solicit.dc_id.id);

  if (dc_server_enabled
      && dc_server_auto_mode
      && strncmp((const char *)evt->evt.direct_connect_id_solicit.dc_id.id,
                 (const char *)dc_server_auto_id.id,
                 SL_WISUN_DC_ID_LEN) == 0) {
    status = sl_wisun_advert_direct_connect_server_id(
      &evt->evt.direct_connect_id_solicit.link_local_ipv6,
      &dc_server_auto_id);
    if (status == SL_STATUS_OK) {
      printf("[Advertised DC_ID:%s to client %s]\r\n",
             (char *)dc_server_auto_id.id,
             ipv6_string);
    } else {
      printf("[Failed to advertise DC_ID:%s to client %s: %"PRIu32"]\r\n",
             (char *)dc_server_auto_id.id,
             ipv6_string,
             status);
    }
  }
}
