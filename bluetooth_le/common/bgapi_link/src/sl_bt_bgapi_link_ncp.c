/***************************************************************************//**
 * @file
 * @brief NCP target transport binding for BGAPI over cpc-ng.
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
#include "sli_bt_bgapi_link_binding.h"
#include "sl_bt_ncp_transport.h"
#include "sl_bt_link.h"
#include "sl_common.h"
#include "sl_component_catalog.h"
#if defined(SL_CATALOG_APP_ASSERT_PRESENT)
#include "app_assert.h"
#endif

void sl_bt_ncp_transport_transmit(uint32_t length, const uint8_t *data)
{
  sl_status_t status;

  status = sl_bt_link_send(sl_bt_bgapi_link_get_link(),
                           data,
                           length);
  if (status != SL_STATUS_OK) {
    // Complete the failed request to release the NCP transport guard.
    sl_bt_ncp_transport_on_transmit(status);
    sl_bt_ncp_transport_on_error(SL_BT_NCP_TRANSPORT_ERROR_RUNTIME,
                                 status);
  }
}

void sl_bt_ncp_transport_receive(void)
{
  // Transport API compatibility no-op, only.
  // No action is needed because reception is callback-driven and buffers are
  // armed during endpoint initialization.
}

void sli_bt_bgapi_link_binding_on_init_error(sl_status_t status)
{
  sl_bt_ncp_transport_on_error(SL_BT_NCP_TRANSPORT_ERROR_INIT, status);
}

void sli_bt_bgapi_link_binding_on_message(sl_bt_link_t *link,
                                          void *user)
{
  sl_bt_link_message_t *message;
  sl_status_t status;

  (void)user;

  do {
    status = sl_bt_link_pop(link, &message);
    if (status == SL_STATUS_OK) {
      sl_bt_ncp_transport_on_receive(SL_STATUS_OK,
                                     (uint32_t)sl_bt_link_get_length(message),
                                     (uint8_t *)sl_bt_link_get_data(message));
      status = sl_bt_link_release(link, message);
    }
  } while (status == SL_STATUS_OK);

  if (status != SL_STATUS_EMPTY) {
    sl_bt_ncp_transport_on_error(SL_BT_NCP_TRANSPORT_ERROR_RX, status);
  }
}

void sli_bt_bgapi_link_binding_on_transmit(sl_bt_link_t *link,
                                           sl_status_t status,
                                           void *user)
{
  (void)link;
  (void)user;
  sl_bt_ncp_transport_on_transmit(status);
}

SL_WEAK void sl_bt_ncp_transport_on_error(sl_bt_ncp_transport_error_t error,
                                          sl_status_t status)
{
  (void)error;
  (void)status;
#if defined(SL_CATALOG_APP_ASSERT_PRESENT)
  app_assert_status_f(status,
                      "bluetooth_ncp_transport: Error %u occurred.",
                      error);
#endif
}
