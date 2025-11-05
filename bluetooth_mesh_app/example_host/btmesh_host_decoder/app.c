/***************************************************************************//**
 * @file app.c
 * @brief Bluetooth Mesh message decoder example
 *******************************************************************************
 * # License
 * <b>Copyright 2025 Silicon Laboratories Inc. www.silabs.com</b>
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
// Includes
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <errno.h>
#include <fcntl.h>
#include <psa/crypto.h>

#include "sl_status.h"
#include "system.h"
#include "ncp_host.h"
#include "app_log.h"
#include "app_log_cli.h"
#include "app_assert.h"

#include "sl_btmesh_api.h"
#include "sl_bt_api.h"
#include "sl_bt_ncp_host.h"

#include "app.h"
#include "sl_mesh_decoder.h"
#include "app_decode.h"
#include "app_decoder_log.h"

// -----------------------------------------------------------------------------
// Macros

#define AD_TYPE_MESH_PBADV (0x29)
#define AD_TYPE_MESH_PDU (0x2a)
#define AD_TYPE_MESH_BEACON (0x2b)

#ifndef MIN
#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#endif

// Optstring argument for getopt.
#define OPTSTRING      NCP_HOST_OPTSTRING APP_LOG_OPTSTRING "hN:A:D:L:I:"

// Usage info.
#define USAGE          "\n%s " NCP_HOST_USAGE APP_LOG_USAGE "[-h] [-N net_key] [-A app_key] [-D dev_key_addr] [-L label_uuid] [-I iv_index]\n"

// Options info.

#define OPTIONS                                                                                                                 \
  "\nOPTIONS\n"                                                                                                                 \
  NCP_HOST_OPTIONS                                                                                                              \
  APP_LOG_OPTIONS                                                                                                               \
  "    -N  Network key option\n"                                                                                                \
  "        <net_key>        One 128-bit network key defined as a hex string. \n"                                                \
  "                         Will be used to decrypt and authenticate network PDUs and Mesh beacons. \n"                         \
  "                         The option can be given multiple times to define multiple keys. \n"                                 \
  "    -A  Application key option\n"                                                                                            \
  "        <app_key>        One 128-bit application key defined as a hex string. \n"                                            \
  "                         Will be used to decrypt and authenticate access messages. \n"                                       \
  "                         The option can be given multiple times to define multiple keys. \n"                                 \
  "    -D  Device key option\n"                                                                                                 \
  "        <dev_key_addr>   One 128-bit device key, immediately followed by 16-bit device address, defined as a hex string. \n" \
  "                         Will be used to decrypt and authenticate access messages. \n"                                       \
  "                         The option can be given multiple times to define multiple keys. \n"                                 \
  "    -L  Label UUID option\n"                                                                                                 \
  "        <label_uuid>     One 128-bit virtual address label UUID. \n"                                                         \
  "                         Will be used to decrypt and authenticate access messages when virtual addresses are used. \n"       \
  "                         The option can be given multiple times to define multiple labels. \n"                               \
  "    -I  IV Index option\n"                                                                                                   \
  "        <iv_index>       The 32-bit IV index of the network as an integer. \n"                                               \
  "                         Defaults to zero. \n"                                                                               \
  "    -h  Print this help message.\n"

// -----------------------------------------------------------------------------
// Static variables

static struct sl_mesh_decoder *context = NULL;
static uint32_t current_ivindex = 0;

// -----------------------------------------------------------------------------
// Private function definitions

static int ishexdigit(int c)
{
  if (c >= '0' && c <= '9') {
    return 1;
  }
  if (c >= 'a' && c <= 'f') {
    return 1;
  }
  if (c >= 'A' && c <= 'F') {
    return 1;
  }
  return 0;
}

static int hexdigit(int c)
{
  if (c >= '0' && c <= '9') {
    return c - '0';
  }
  if (c >= 'a' && c <= 'f') {
    return 10 + c - 'a';
  }
  if (c >= 'A' && c <= 'F') {
    return 10 + c - 'A';
  }
  return 0;
}

static sl_status_t parse_hexstring(const char *hexstring,
                                   size_t min_octets,
                                   size_t max_octets,
                                   uint8_t *result_ptr,
                                   size_t *result_len)
{
  size_t len, pos;

  len = strlen(hexstring);
  if ((len & 1) || len < min_octets * 2 || len > max_octets * 2) {
    return SL_STATUS_INVALID_PARAMETER;
  }

  for (pos = 0; pos < len; pos += 2) {
    if (!ishexdigit(hexstring[pos]) || !ishexdigit(hexstring[pos + 1])) {
      return SL_STATUS_INVALID_PARAMETER;
    }
    result_ptr[pos / 2] = (hexdigit(hexstring[pos]) << 4) | hexdigit(hexstring[pos + 1]);
  }

  if (result_len) {
    *result_len = pos / 2;
  }
  return SL_STATUS_OK;
}

static void rx(sl_bt_msg_t *evt)
{
  size_t pos;
  uint8_t data_len;
  advertisement_header_t adv_header = { 0 };

  // Process only the interesting AD structures
  pos = 0;
  sl_mesh_advertisement_header_from_buf(evt, &adv_header);
  while (pos < adv_header.data_num) {
    // The Length field (ad_len) includes itself in the total count,
    // so subtract 1 to get the actual data length
    data_len = adv_header.adv_data[pos].ad_len - 1;
    if (adv_header.adv_data->ad_type == AD_TYPE_MESH_PDU) {
#if DECODER_APP_ADV_HEADER_LOG
      app_adv_header_log(adv_header);
#endif // DECODER_APP_ADV_HEADER_LOG
      app_rx_net(context,
                 current_ivindex,
                 adv_header.adv_data[pos].data,
                 data_len);
    } else if (adv_header.adv_data->ad_type == AD_TYPE_MESH_BEACON) {
#if DECODER_APP_ADV_HEADER_LOG
      app_adv_header_log(adv_header);
#endif // DECODER_APP_ADV_HEADER_LOG
      app_rx_beacon(context,
                    adv_header.adv_data[pos].data,
                    data_len);
    }
    pos++;
  }
}

// -----------------------------------------------------------------------------
// Public function definitions

/**************************************************************************//**
 * Application Init.
 *****************************************************************************/
void app_init(int argc, char *argv[])
{
  sl_mesh_key_handle handle;
  uint8_t tmp[18];
  uint16_t addr;
  sl_status_t sc;
  int opt;
  char *endptr;

  sc = psa_crypto_init();
  app_assert_status_f(sc, "Failed to initialize PSA crypto. " APP_LOG_NL);

  sc = sl_mesh_decoder_create(&context);
  app_assert_status_f(sc, "Failed to initialize decoder. " APP_LOG_NL);

  // Process command line options.
  while ((opt = getopt(argc, argv, OPTSTRING)) != -1) {
    switch (opt) {
      // Print help.
      case 'h':
        app_log(USAGE, argv[0]);
        app_log(OPTIONS);
        exit(EXIT_SUCCESS);

      case 'N':
        if (parse_hexstring(optarg, 16, 16, tmp, NULL) != SL_STATUS_OK) {
          app_log(USAGE, argv[0]);
          exit(EXIT_FAILURE);
        }
        sc = sl_mesh_decoder_add_network_key(context, tmp, &handle);
        app_assert_status_f(sc, "Failed to add network key '%s'. " APP_LOG_NL, optarg);
        break;

      case 'A':
        if (parse_hexstring(optarg, 16, 16, tmp, NULL) != SL_STATUS_OK) {
          app_log(USAGE, argv[0]);
          exit(EXIT_FAILURE);
        }
        sc = sl_mesh_decoder_add_application_key(context, tmp, &handle);
        app_assert_status_f(sc, "Failed to add application key '%s'. " APP_LOG_NL, optarg);
        break;

      case 'D':
        if (parse_hexstring(optarg, 18, 18, tmp, NULL) != SL_STATUS_OK) {
          app_log(USAGE, argv[0]);
          exit(EXIT_FAILURE);
        }
        addr = ((uint16_t)tmp[16] << 8) | (uint16_t)tmp[17];
        sc = sl_mesh_decoder_add_device_key(context, tmp, addr, &handle);
        app_assert_status_f(sc, "Failed to add device key '%s'. " APP_LOG_NL, optarg);
        break;

      case 'L':
        if (parse_hexstring(optarg, 16, 16, tmp, NULL) != SL_STATUS_OK) {
          app_log(USAGE, argv[0]);
          exit(EXIT_FAILURE);
        }
        sc = sl_mesh_decoder_add_virtual_label_uuid(context, tmp);
        app_assert_status_f(sc, "Failed to add Label UUID '%s'. " APP_LOG_NL, optarg);
        break;

      case 'I':
        if (!strlen(optarg)) {
          app_log(USAGE, argv[0]);
          exit(EXIT_FAILURE);
        }
        endptr = NULL;
        current_ivindex = strtol(optarg, &endptr, 0);
        if (endptr == NULL || (endptr != NULL && *endptr != '\0')) {
          app_log(USAGE, argv[0]);
          exit(EXIT_FAILURE);
        }
        break;
      // Process options for other modules.
      default:
        sc = ncp_host_set_option((char)opt, optarg);
        if (sc != SL_STATUS_OK) {
          app_log(USAGE, argv[0]);
          exit(EXIT_FAILURE);
        }
        break;
    }
  }

  // Initialize NCP connection.
  sc = ncp_host_init();
  if (sc == SL_STATUS_INVALID_PARAMETER) {
    app_log(USAGE, argv[0]);
    exit(EXIT_FAILURE);
  }
  app_assert_status(sc);

  app_log_info("Empty NCP-host initialised." APP_LOG_NL);

  /////////////////////////////////////////////////////////////////////////////
  // Put your additional application init code here!                         //
  // This is called once during start-up.                                    //
  /////////////////////////////////////////////////////////////////////////////
}

/**************************************************************************//**
 * Application Process Action.
 *****************************************************************************/
void app_process_action(void)
{
  /////////////////////////////////////////////////////////////////////////////
  // Put your additional application code here!                              //
  // This is called infinitely.                                              //
  // Do not call blocking functions from here!                               //
  /////////////////////////////////////////////////////////////////////////////
}

/**************************************************************************//**
 * Application Deinit.
 *****************************************************************************/
void app_deinit(void)
{
  ncp_host_deinit();

  /////////////////////////////////////////////////////////////////////////////
  // Put your additional application deinit code here!                       //
  // This is called once during termination.                                 //
  /////////////////////////////////////////////////////////////////////////////

  sl_mesh_decoder_delete(context);
  context = NULL;
}

// -----------------------------------------------------------------------------
// Event / callback definitions

/**************************************************************************//**
 * Bluetooth stack event handler.
 * This overrides the dummy weak implementation.
 *
 * @param[in] evt Event coming from the Bluetooth stack.
 *****************************************************************************/
void sl_bt_on_event(sl_bt_msg_t *evt)
{
  sl_status_t sc;

  switch (SL_BT_MSG_ID(evt->header)) {
    // -------------------------------
    // This event indicates the device has started and the radio is ready.
    // Do not call any stack command before receiving this boot event!
    case sl_bt_evt_system_boot_id:

      sc = sl_bt_scanner_start(1, 2);
      app_assert_status_f(sc, "Failed to start scanner. " APP_LOG_NL);

      break;

    case sl_bt_evt_scanner_legacy_advertisement_report_id:
      rx(evt);
      break;

    // -------------------------------
    // Default event handler.
    default:
      break;
  }
}

/**************************************************************************//**
 * Bluetooth Mesh stack event handler.
 * This overrides the dummy weak implementation.
 *
 * @param[in] evt Event coming from the Bluetooth Mesh stack.
 *****************************************************************************/
void sl_btmesh_on_event(sl_btmesh_msg_t *evt)
{
  switch (SL_BT_MSG_ID(evt->header)) {
    ///////////////////////////////////////////////////////////////////////////
    // Add event handlers here as your application requires!                 //
    ///////////////////////////////////////////////////////////////////////////

    // -------------------------------
    // Default event handler.
    default:
      break;
  }
}
