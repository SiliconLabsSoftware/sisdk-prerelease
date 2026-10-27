/******************************************************************************
 * @file
 * @brief Embedded BGAPI NCP host proof-of-concept application.
 ******************************************************************************
 * # License
 * <b>Copyright 2026 Silicon Laboratories Inc. www.silabs.com</b>
 ******************************************************************************
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
 *****************************************************************************/
#include "app.h"
#include "app_assert.h"
#include "app_log.h"
#include "app_timer.h"

#include "sl_bluetooth.h"
#include "sl_bt_api.h"
#include "sl_bt_bgapi_link.h"
#include "sl_bt_ncp_host.h"

#include "sl_bt_link.h"
#include "sl_bt_link_runtime.h"

#define STOP_TIMEOUT_MS    5000
#define BOOT_TIMEOUT_MS    2000

// Passive 1% duty-cycle scanning keeps background traffic low while still
// exercising asynchronous event delivery between BGAPI commands and responses.
#define SCAN_INTERVAL      1600  // 1000 ms
#define SCAN_WINDOW        16    // 10 ms

typedef enum {
  HOST_STATE_IDLE,
  HOST_STATE_STOPPING,
  HOST_STATE_FORCE_STOPPING,
  HOST_STATE_STARTING,
  HOST_STATE_READY,
} host_state_t;

typedef struct {
  host_state_t stopping;
  host_state_t force_stopping;
  host_state_t starting;
} timeout_context_t;

static host_state_t host_state = HOST_STATE_IDLE;
static volatile host_state_t pending_timeout_state = HOST_STATE_IDLE;

static const timeout_context_t timeout_context = {
  .stopping = HOST_STATE_STOPPING,
  .force_stopping = HOST_STATE_FORCE_STOPPING,
  .starting = HOST_STATE_STARTING,
};

static uint8_t advertising_set_handle = 0xff;
static bool link_was_connected;

static app_timer_t timeout_timer;

/******************************************************************************
 * Forward declarations.
 *****************************************************************************/
static void start_bgapi_sequence(void);
static sl_status_t start_bluetooth(void);
static void force_stop_bgstack(void);
static void start_scanning(void);
static void stop_scanning(void);
static void discard_pending_events(void);
static void process_timeout(void);

static void timeout_cb(app_timer_t *timer, void *data);

/******************************************************************************
 * Start low-duty-cycle passive scanning.
 *****************************************************************************/
static void start_scanning(void)
{
  sl_status_t status;

  status = sl_bt_scanner_set_parameters(sl_bt_scanner_scan_mode_passive,
                                        SCAN_INTERVAL,
                                        SCAN_WINDOW);
  if (status != SL_STATUS_OK) {
    app_log_error("Failed to configure scanning (0x%04lx)." APP_LOG_NL,
                  (unsigned long)status);
    return;
  }

  status = sl_bt_scanner_start(sl_bt_scanner_scan_phy_1m,
                               sl_bt_scanner_discover_generic);
  if (status == SL_STATUS_OK) {
    app_log_info("Passive scanning started." APP_LOG_NL);
  } else {
    app_log_error("Failed to start scanning (0x%04lx)." APP_LOG_NL,
                  (unsigned long)status);
  }
}

/******************************************************************************
 * Stop scanning.
 *****************************************************************************/
static void stop_scanning(void)
{
  sl_status_t status = sl_bt_scanner_stop();

  if (status == SL_STATUS_OK) {
    app_log_info("Scanning stopped." APP_LOG_NL);
  } else {
    app_log_error("Failed to stop scanning (0x%04lx)." APP_LOG_NL,
                  (unsigned long)status);
  }
}

/******************************************************************************
 * Discard events received before Bluetooth stack synchronization.
 *****************************************************************************/
static void discard_pending_events(void)
{
  sl_bt_msg_t event;

  while (sl_bt_pop_event(&event) == SL_STATUS_OK) {
  }
}

/******************************************************************************
 * Start Bluetooth stack synchronization sequence.
 *
 * The example synchronizes with the remote stack using BGAPI command/response
 * transactions, which remain valid across CPC session reconnections.
 *****************************************************************************/
static void start_bgapi_sequence(void)
{
  sl_status_t status;

  if (host_state != HOST_STATE_IDLE) {
    return;
  }

  app_log_info("CPC endpoint is connected." APP_LOG_NL);
  app_log_info("Synchronizing remote Bluetooth stack." APP_LOG_NL);

  discard_pending_events();

  status = sl_bt_system_stop_bluetooth();

  if (status != SL_STATUS_OK) {
    app_log_warning("Graceful stop failed (0x%04lx), forcing stop."
                    APP_LOG_NL,
                    (unsigned long)status);

    force_stop_bgstack();
    return;
  } else {
    // sl_bt_system_stop_bluetooth() may return OK on already stopped status, yet it's seemingly not fully idempotent
    // as it doesn't emit the expected @ref sl_bt_evt_system_stopped event in this case - so we try to start immediately
    // as a cross-check step: if the request fails, then it is stopping, otherwise it was stopped already.
    status = start_bluetooth();  // will set HOST_STATE_STARTING on success, or leave the state as is

    if (status != SL_STATUS_OK) {
      host_state = HOST_STATE_STOPPING;
      app_assert_status(app_timer_start(&timeout_timer,
                                        STOP_TIMEOUT_MS,
                                        timeout_cb,
                                        (void *)&timeout_context.stopping,
                                        false));
    }
  }
}

/******************************************************************************
 * Start Bluetooth stack.
 *****************************************************************************/
static sl_status_t start_bluetooth(void)
{
  sl_status_t status = sl_bt_system_start_bluetooth();

  if (status == SL_STATUS_OK) {
    host_state = HOST_STATE_STARTING;

    app_assert_status(app_timer_start(&timeout_timer,
                                      BOOT_TIMEOUT_MS,
                                      timeout_cb,
                                      (void *)&timeout_context.starting,
                                      false));
  }

  return status;
}

/******************************************************************************
 * Force Bluetooth stack stop.
 *****************************************************************************/
static void force_stop_bgstack(void)
{
  sl_status_t status;

  host_state = HOST_STATE_FORCE_STOPPING;

  status = sl_bt_system_forcefully_stop_bluetooth();
  app_log_warning("Forcefully stop request resulted in 0x%04lx."
                  APP_LOG_NL,
                  (unsigned long)status);

  if (status == SL_STATUS_OK) {
    app_assert_status(app_timer_start(&timeout_timer,
                                      STOP_TIMEOUT_MS,
                                      timeout_cb,
                                      (void *)&timeout_context.force_stopping,
                                      false));
  }
}

/******************************************************************************
 * Defer timeout handling to the application task.
 *****************************************************************************/
static void timeout_cb(app_timer_t *timer, void *data)
{
  (void)timer;

  pending_timeout_state = *((const host_state_t *)data);
}

/******************************************************************************
 * Process a timeout if the state that armed it is still active.
 *****************************************************************************/
static void process_timeout(void)
{
  host_state_t state = pending_timeout_state;

  if (state == HOST_STATE_IDLE) {
    return;
  }

  pending_timeout_state = HOST_STATE_IDLE;
  if (host_state != state) {
    return;
  }

  switch (state) {
    case HOST_STATE_STOPPING:
      app_log_warning("Bluetooth stop timeout, forcing shutdown." APP_LOG_NL);
      force_stop_bgstack();
      break;

    case HOST_STATE_FORCE_STOPPING:
      app_log_error("Bluetooth force stop timeout, shutdown failed."
                    APP_LOG_NL);
      host_state = HOST_STATE_IDLE;
      break;

    case HOST_STATE_STARTING:
      app_log_error("Bluetooth boot timeout." APP_LOG_NL);
      host_state = HOST_STATE_IDLE;
      break;

    default:
      break;
  }
}

/******************************************************************************
 * Application Init.
 *****************************************************************************/
void app_init(void)
{
  app_log_info("Waiting for the CPC endpoint to connect." APP_LOG_NL);
}

/******************************************************************************
 * Application Process Action.
 *****************************************************************************/
void app_process_action(void)
{
  sl_bt_link_t *link;
  sl_bt_msg_t event;
  sl_status_t status;
  bool connected;

  link = sl_bt_bgapi_link_get_link();
  connected = sl_bt_link_is_open(link);

  if (link_was_connected && !connected) {
    app_timer_stop(&timeout_timer);

    pending_timeout_state = HOST_STATE_IDLE;
    advertising_set_handle = 0xff;
    host_state = HOST_STATE_IDLE;

    app_log_warning("CPC endpoint dropped; waiting to reconnect."
                    APP_LOG_NL);
  }

  link_was_connected = connected;

  if (!connected) {
    (void)sl_bt_link_runtime_wait(sl_bt_bgapi_link_get_runtime(), 1);
    return;
  }

  process_timeout();

  if (connected && host_state == HOST_STATE_IDLE) {
    start_bgapi_sequence();
  }

  status = sl_bt_pop_event(&event);

  if (status == SL_STATUS_OK) {
    sl_bt_process_event(&event);
  } else {
    (void)sl_bt_link_runtime_wait(sl_bt_bgapi_link_get_runtime(), 1);
  }
}

/******************************************************************************
 * Bluetooth stack event handler.
 *
 * @param[in] evt Event coming from the remote Bluetooth stack.
 *****************************************************************************/
void sl_bt_on_event(sl_bt_msg_t *evt)
{
  const uint32_t msg_id = SL_BT_MSG_ID(evt->header);

  sl_status_t status;

  switch (msg_id) {
    //-----------------------------------------------------------------------
    // Stack successfully stopped.
    case sl_bt_evt_system_stopped_id:
      if (host_state != HOST_STATE_STOPPING
          && host_state != HOST_STATE_FORCE_STOPPING) {
        app_log_debug("Ignoring unexpected Bluetooth stack stopped event."
                      APP_LOG_NL);
        break;
      }

      app_timer_stop(&timeout_timer);
      pending_timeout_state = HOST_STATE_IDLE;
      app_log_info("Remote Bluetooth stack stopped. Restarting." APP_LOG_NL);
      status = start_bluetooth();
      if (status != SL_STATUS_OK) {
        app_log_error("Failed to start Bluetooth stack (0x%04lx)."
                      APP_LOG_NL,
                      (unsigned long)status);
        host_state = HOST_STATE_IDLE;
      }
      break;

    //-----------------------------------------------------------------------
    // Stack successfully started.
    case sl_bt_evt_system_boot_id:
      if (host_state != HOST_STATE_STARTING) {
        app_log_debug("Ignoring unexpected Bluetooth stack boot event."
                      APP_LOG_NL);
        break;
      }

      app_timer_stop(&timeout_timer);
      pending_timeout_state = HOST_STATE_IDLE;
      app_log_info("Remote Bluetooth stack booted." APP_LOG_NL);

      advertising_set_handle = 0xff;
      status = sl_bt_advertiser_create_set(&advertising_set_handle);
      app_assert_status(status);

      status = sl_bt_legacy_advertiser_generate_data(advertising_set_handle,
                                                     sl_bt_advertiser_general_discoverable);
      app_assert_status(status);

      status = sl_bt_advertiser_set_timing(advertising_set_handle,
                                           160,
                                           160,
                                           10000,
                                           0);
      app_assert_status(status);

      status = sl_bt_legacy_advertiser_start(
        advertising_set_handle,
        sl_bt_legacy_advertiser_connectable);
      app_assert_status(status);

      host_state = HOST_STATE_READY;

      app_log_info("Advertising started." APP_LOG_NL);
      start_scanning();

      break;

    // -------------------------------
    // This event indicates that our advertising timeout has expired.
    case sl_bt_evt_advertiser_timeout_id:
      if (host_state == HOST_STATE_READY) {
        app_log_info("BGAPI command/response/event smoke test passed."
                     APP_LOG_NL);
        stop_scanning();
      }
      break;

    // -------------------------------
    // This event indicates that a new connection was opened.
    case sl_bt_evt_connection_opened_id:
      if (host_state == HOST_STATE_READY) {
        app_log_info("Connection opened." APP_LOG_NL);
        stop_scanning();
      }
      break;

    // -------------------------------
    // This event indicates that a connection was closed.
    case sl_bt_evt_connection_closed_id:
      if (host_state != HOST_STATE_READY
          || advertising_set_handle == 0xff) {
        app_log_debug("Ignoring connection closed event during stack "
                      "synchronization." APP_LOG_NL);
        break;
      }

      app_log_info("Connection closed." APP_LOG_NL);

      // Restart advertising after client has disconnected.
      status = sl_bt_legacy_advertiser_start(advertising_set_handle,
                                             sl_bt_legacy_advertiser_connectable);
      app_assert_status(status);
      app_log_info("Restarted advertising." APP_LOG_NL);
      start_scanning();
      break;

    // -------------------------------
    // This event reports a received legacy advertisement.
    case sl_bt_evt_scanner_legacy_advertisement_report_id:
      app_log_debug("Legacy adv. from "
                    "%02x:%02x:%02x:%02x:%02x:%02x, RSSI %d dBm, "
                    "ch: %u, flags 0x%02x, data len.: %u."
                    APP_LOG_NL,
                    evt->data.evt_scanner_legacy_advertisement_report.address.addr[5],
                    evt->data.evt_scanner_legacy_advertisement_report.address.addr[4],
                    evt->data.evt_scanner_legacy_advertisement_report.address.addr[3],
                    evt->data.evt_scanner_legacy_advertisement_report.address.addr[2],
                    evt->data.evt_scanner_legacy_advertisement_report.address.addr[1],
                    evt->data.evt_scanner_legacy_advertisement_report.address.addr[0],
                    evt->data.evt_scanner_legacy_advertisement_report.rssi,
                    evt->data.evt_scanner_legacy_advertisement_report.channel,
                    evt->data.evt_scanner_legacy_advertisement_report.event_flags,
                    evt->data.evt_scanner_legacy_advertisement_report.data.len);
      break;

    // -------------------------------
    // This event reports a received extended advertisement.
    case sl_bt_evt_scanner_extended_advertisement_report_id:
      app_log_debug("Extended adv. from "
                    "%02x:%02x:%02x:%02x:%02x:%02x, RSSI %d dBm, "
                    "ch: %u, SID %u, data len.: %u."
                    APP_LOG_NL,
                    evt->data.evt_scanner_extended_advertisement_report.address.addr[5],
                    evt->data.evt_scanner_extended_advertisement_report.address.addr[4],
                    evt->data.evt_scanner_extended_advertisement_report.address.addr[3],
                    evt->data.evt_scanner_extended_advertisement_report.address.addr[2],
                    evt->data.evt_scanner_extended_advertisement_report.address.addr[1],
                    evt->data.evt_scanner_extended_advertisement_report.address.addr[0],
                    evt->data.evt_scanner_extended_advertisement_report.rssi,
                    evt->data.evt_scanner_extended_advertisement_report.channel,
                    evt->data.evt_scanner_extended_advertisement_report.adv_sid,
                    evt->data.evt_scanner_extended_advertisement_report.data.len);
      break;

    //-----------------------------------------------------------------------
    // Handler for any other, uninterested events.
    default:
      app_log_debug("BGAPI event ID: %08lx." APP_LOG_NL,
                    (unsigned long)msg_id);
      break;
  }
}
