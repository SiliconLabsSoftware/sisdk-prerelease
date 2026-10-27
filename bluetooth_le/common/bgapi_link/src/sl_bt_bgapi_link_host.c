/***************************************************************************//**
 * @file
 * @brief Embedded NCP host binding for BGAPI over cpc-ng.
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
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "sl_bt_bgapi_link.h"
#include "sli_bt_bgapi_link.h"
#include "sli_bt_bgapi_link_binding.h"
#include "sl_bt_ncp_host.h"
#include "sl_bt_link.h"
#include "sl_bt_link_runtime.h"
#include "sl_core.h"

static sl_bt_link_message_t *current_message;
static volatile sl_status_t host_status = SL_STATUS_OK;
static bool discard_current_message;

static sl_status_t sl_bt_bgapi_link_host_release_current(sl_bt_link_t *link)
{
  sl_status_t status = SL_STATUS_OK;

  if (current_message != NULL) {
    status = sl_bt_link_release(link, current_message);
    current_message = NULL;
  }

  return status;
}

static void sl_bt_bgapi_link_host_request_discard(void)
{
  CORE_ATOMIC_SECTION(discard_current_message = true; )
}

static sl_status_t sl_bt_bgapi_link_host_discard_requested(sl_bt_link_t *link)
{
  bool discard;

  CORE_DECLARE_IRQ_STATE;
  CORE_ENTER_ATOMIC();
  discard = discard_current_message;
  discard_current_message = false;
  CORE_EXIT_ATOMIC();

  return (discard ? sl_bt_bgapi_link_host_release_current(link)
          : SL_STATUS_OK);
}

static void sl_bt_bgapi_link_host_output(uint32_t length, uint8_t *data)
{
  sl_status_t status;

  status = sl_bt_link_send(sl_bt_bgapi_link_get_link(),
                           data,
                           length);
  if (status != SL_STATUS_OK) {
    host_status = status;
  }
}

static int32_t sl_bt_bgapi_link_host_input(uint32_t length, uint8_t *data)
{
  sl_status_t status;
  sl_bt_link_t *link = sl_bt_bgapi_link_get_link();

  if (data == NULL) {
    return -1;
  }

  while ((current_message == NULL) && (host_status == SL_STATUS_OK)) {
    status = sl_bt_bgapi_link_host_discard_requested(link);
    if (status == SL_STATUS_OK) {
      status = sl_bt_link_pop(link, &current_message);
    }
    if (status == SL_STATUS_EMPTY) {
      status = sl_bt_link_runtime_wait(sl_bt_bgapi_link_get_runtime(),
                                       1);
    }
    if ((status != SL_STATUS_OK) && (status != SL_STATUS_EMPTY)) {
      host_status = status;
    }
  }

  if ((host_status != SL_STATUS_OK) || (current_message == NULL)) {
    return -1;
  }

  status = sl_bt_bgapi_link_host_discard_requested(link);
  if ((status != SL_STATUS_OK) || (current_message == NULL)) {
    host_status = status;
    return -1;
  }

  status = sl_bt_link_read(current_message, data, length);
  if (status != SL_STATUS_OK) {
    host_status = status;
    return -1;
  }

  if (sl_bt_link_get_remaining(current_message) == 0) {
    status = sl_bt_link_release(link, current_message);
    current_message = NULL;
    if (status != SL_STATUS_OK) {
      host_status = status;
      return -1;
    }
  }

  return (int32_t)length;
}

static int32_t sl_bt_bgapi_link_host_peek(void)
{
  sl_status_t status;
  sl_bt_link_t *link = sl_bt_bgapi_link_get_link();

  status = sl_bt_bgapi_link_host_discard_requested(link);
  if (status != SL_STATUS_OK) {
    host_status = status;
    return -1;
  }

  if (host_status != SL_STATUS_OK) {
    return -1;
  }

  if (current_message == NULL) {
    status = sl_bt_link_pop(link, &current_message);
    if (status == SL_STATUS_EMPTY) {
      return 0;
    }
    if (status != SL_STATUS_OK) {
      host_status = status;
      return -1;
    }
  }

  return (int32_t)sl_bt_link_get_remaining(current_message);
}

void sli_bt_bgapi_link_host_init(void)
{
  sl_status_t status;

  status = sl_bt_api_initialize_nonblock(sl_bt_bgapi_link_host_output,
                                         sl_bt_bgapi_link_host_input,
                                         sl_bt_bgapi_link_host_peek);
  if (status != SL_STATUS_OK) {
    host_status = status;
  }
}

void sli_bt_bgapi_link_binding_on_transmit(sl_bt_link_t *link,
                                           sl_status_t status,
                                           void *user)
{
  (void)link;
  (void)user;

  if (status != SL_STATUS_OK) {
    host_status = status;
  }
}

void sli_bt_bgapi_link_binding_on_error(sl_bt_link_t *link,
                                        sl_status_t status,
                                        void *user)
{
  (void)link;
  (void)status;
  (void)user;

  // A restarted peer is recovered by closing and reconnecting the endpoint.
  // Keep the host input path alive so commands can be issued after the next
  // CONNECTED event, even if system_boot was already consumed.
  sl_bt_bgapi_link_host_request_discard();
  host_status = SL_STATUS_OK;
}

void sli_bt_bgapi_link_binding_on_connected(sl_bt_link_t *link,
                                            void *user)
{
  (void)link;
  (void)user;
  sl_bt_bgapi_link_host_request_discard();
  host_status = SL_STATUS_OK;
}
