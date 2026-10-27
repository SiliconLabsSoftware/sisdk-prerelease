/***************************************************************************//**
 * @file
 * @brief BGAPI over cpc-ng component initialization.
 *******************************************************************************
 * # License
 * <b>Copyright 2026 Silicon Laboratories Inc. www.silabs.com</b>
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
#include <stddef.h>
#include <stdint.h>
#include "sl_bt_bgapi_link.h"
#include "sl_bt_bgapi_link_config.h"
#include "sl_bgapi.h"
#include "sli_bt_bgapi_link.h"
#include "sli_bt_bgapi_link_binding.h"
#include "sl_bt_link.h"
#include "sl_bt_link_runtime.h"
#include "sl_component_catalog.h"
#include "sl_cpc_bus_instances.h"

static sl_bt_link_t *bgapi_link_link;
static sl_status_t bgapi_link_init_status = SL_STATUS_NOT_INITIALIZED;

static void sli_bt_bgapi_link_report_init_error(sl_status_t status)
{
#if defined(SL_CATALOG_CPC_NG_PRIMARY_PRESENT)
  sli_bt_bgapi_link_binding_on_error(bgapi_link_link, status, NULL);
#else
  sli_bt_bgapi_link_binding_on_init_error(status);
#endif
}

#if defined(SL_CATALOG_CPC_NG_PRIMARY_PRESENT)
static const sl_bt_link_message_callback_t bgapi_link_on_message = NULL;
static const sl_bt_link_error_callback_t bgapi_link_on_error =
  sli_bt_bgapi_link_binding_on_error;
static const sl_bt_link_connect_callback_t bgapi_link_on_connected =
  sli_bt_bgapi_link_binding_on_connected;
#else
static const sl_bt_link_message_callback_t bgapi_link_on_message =
  sli_bt_bgapi_link_binding_on_message;
static const sl_bt_link_error_callback_t bgapi_link_on_error = NULL;
static const sl_bt_link_connect_callback_t bgapi_link_on_connected = NULL;
#endif

SL_BT_LINK_STORAGE_DEFINE(bgapi_link_storage,
                          SL_BT_BGAPI_LINK_MESSAGE_SIZE,
                          SL_BT_BGAPI_LINK_RX_DEPTH);

static bool sli_bt_bgapi_link_message_is_valid(const uint8_t *data,
                                               size_t length)
{
  size_t payload_length;

  if ((data == NULL)
      || (length < SL_BGAPI_MSG_HEADER_LEN)
      || (length > SL_BT_BGAPI_LINK_MESSAGE_SIZE)) {
    return false;
  }

  payload_length = (((size_t)(data[0] & 0x07u) << 8) | data[1]);

  return length == (SL_BGAPI_MSG_HEADER_LEN + payload_length);
}

void sli_bt_bgapi_link_init(void)
{
  sl_bt_link_config_t config = {
    .bus = g_bus,
    .runtime = sl_bt_link_runtime_get(),
    .storage = &bgapi_link_storage,
    .is_valid = sli_bt_bgapi_link_message_is_valid,
    .on_message = bgapi_link_on_message,
    .on_transmit = sli_bt_bgapi_link_binding_on_transmit,
    .on_error = bgapi_link_on_error,
    .on_connected = bgapi_link_on_connected,
    .user = NULL,
#if defined(SL_CATALOG_CPC_NG_PRIMARY_PRESENT)
    .role = sl_bt_link_role_primary,
#else
    .role = sl_bt_link_role_secondary,
#endif // SL_CATALOG_CPC_NG_PRIMARY_PRESENT
    .endpoint_id = SL_BT_BGAPI_LINK_ENDPOINT_ID,
  };

  if (config.runtime == NULL) {
    bgapi_link_init_status = sl_bt_link_runtime_get_status();
    sli_bt_bgapi_link_report_init_error(bgapi_link_init_status);
    return;
  }

  bgapi_link_init_status = sl_bt_link_create(&config, &bgapi_link_link);
  if (bgapi_link_init_status != SL_STATUS_OK) {
    sli_bt_bgapi_link_report_init_error(bgapi_link_init_status);
  }
}

void sli_bt_bgapi_link_ready(void)
{
  sl_status_t status;

  if (bgapi_link_init_status != SL_STATUS_OK) {
    return;
  }

  status = sl_bt_link_start(bgapi_link_link);
  if (status != SL_STATUS_OK) {
    sli_bt_bgapi_link_report_init_error(status);
  }
}

sl_bt_link_t *sl_bt_bgapi_link_get_link(void)
{
  return bgapi_link_link;
}

sl_bt_link_runtime_t *sl_bt_bgapi_link_get_runtime(void)
{
  return sl_bt_link_runtime_get();
}
