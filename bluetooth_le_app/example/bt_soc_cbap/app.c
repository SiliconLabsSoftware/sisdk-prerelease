/***************************************************************************//**
 * @file
 * @brief Certificate Based Authentication and Pairing application source
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
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "app_assert.h"
#include "app_log.h"
#include "app_timer.h"
#include "sl_simple_led_instances.h"
#include "sl_main_init.h"
#include "sl_bt_api.h"
#include "sl_bluetooth_connection_config.h"
#include "gatt_db.h"
#include "sl_bt_cbap.h"
#include "app_config.h"

#if SL_BT_CONFIG_MAX_CONNECTIONS < 1
  #error At least 1 connection has to be enabled!
#endif

// -----------------------------------------------------------------------------
// GATT
//
// The CBAP component runs the authentication and the pairing, and owns the
// characteristics of the CBAP service. What it achieves is a connection in
// security mode 1, level 4: encrypted with an authenticated key. That protects
// the characteristics of the local GATT database which require an authenticated
// and encrypted connection, and nothing else. A characteristic that is
// readable or writable without security stays accessible to every device that
// connects, no matter whether it passed CBAP or not.
//
// The Digital characteristic of the Automation IO service demonstrates the
// difference. It requires an authenticated and encrypted connection, so it can
// only be written once CBAP has succeeded, which is how the two devices blink
// the LED of each other. Keep this in mind when adding characteristics to
// gatt_configuration.btconf: CBAP does not protect them by itself, their own
// security requirements do.

/// Service UUID lengths as they appear in an advertisement
#define UUID_16_LEN                   2
#define UUID_128_LEN                  16

/// GAP advertising data types carrying service UUIDs
/// Incomplete List of 16-bit Service Class UUIDs
#define GAP_INCOMPLETE_16B_UUID       0x02
/// Complete List of 16-bit Service Class UUIDs
#define GAP_COMPLETE_16B_UUID         0x03
/// Incomplete List of 128-bit Service Class UUIDs
#define GAP_INCOMPLETE_128B_UUID      0x06
/// Complete List of 128-bit Service Class UUIDs
#define GAP_COMPLETE_128B_UUID        0x07

// UUID of the CBAP service, advertised by a device that supports CBAP
static const uint8_t cbap_service_uuid[] = { SL_BT_CBAP_SERVICE_UUID };

// Value written to the Digital characteristic to turn the LED on and off
#define LED_ON                 0x01
#define LED_OFF                0x00

// -----------------------------------------------------------------------------
// Role

// Bluetooth role of this device. It decides whether the device advertises and
// waits for a central device, or scans for a peripheral device to connect to.
static sl_bt_connection_role_t role = CONNECTION_ROLE;

#define IS_CENTRAL             (role == sl_bt_connection_role_central)

// -----------------------------------------------------------------------------
// Advertising and scanning

// The advertising set handle allocated from Bluetooth stack
static uint8_t advertising_set_handle = SL_BT_INVALID_ADVERTISING_SET_HANDLE;

// Advertising interval in 0.625 ms units
#define ADV_INTERVAL_MS        100
#define ADV_INTERVAL_UNITS     ((ADV_INTERVAL_MS) * 8 / 5)

// True while the device is advertising or scanning
static bool discovery_active = false;

// True after the central requested a connection and before the stack reports
// connection_opened. This flag is required because CBAP cannot report itself
// busy during this short period.
static bool connection_open_pending = false;

// Should we search for a specified peripheral device or not
static bool peripheral_target_defined = ADDR_ENABLE;
// Target device Bluetooth address
static bd_addr peripheral_target_addr;

// Start advertising or scanning, depending on the Bluetooth role.
static void start_discovery(void);
// Stop advertising or scanning, depending on the Bluetooth role.
static void stop_discovery(void);
// Examine a scan report and decide if a connection should be established.
static bool check_scan_report(
  const sl_bt_evt_scanner_legacy_advertisement_report_t *report);
// Search for a Service UUID in scan report.
static bool find_service_in_advertisement(const uint8_t *scan_data,
                                          uint8_t scan_data_len,
                                          const uint8_t *uuid,
                                          uint8_t uuid_len);

// -----------------------------------------------------------------------------
// Connections

// Number of open connections, authenticated by CBAP or not
static uint8_t open_connections = 0;

// Properties of the connections that completed the CBAP procedure
static sl_bt_cbap_conn_t trusted_devices[SL_BT_CONFIG_MAX_CONNECTIONS];

// Bluetooth addresses of the devices that failed the CBAP procedure. The
// application does not start a new procedure with them.
static bd_addr disallowlist[DISALLOWLIST_SIZE];
// Number of valid entries in disallowlist
static uint8_t disallowlist_len = 0;

// Report the outcome of a CBAP procedure. Called by the CBAP component.
static void on_cbap_result(sl_bt_cbap_conn_t conn, sl_status_t sc);
// Add an authenticated connection to the trusted devices array.
static void add_trusted_device(sl_bt_cbap_conn_t conn);
// Remove a closed connection from the trusted devices array.
static void remove_trusted_device(uint8_t connection);
// Log the connection handle and the Bluetooth address of the trusted devices.
static void print_trusted_devices(void);
// Remember a device that failed the CBAP procedure.
static void add_to_disallowlist(bd_addr address);
// True if the device failed the CBAP procedure before.
static bool is_disallowed(bd_addr address);

// -----------------------------------------------------------------------------
// LED

// Time the LED is kept on to indicate a successful CBAP procedure
#define LED_BLINK_TIME_MS      500

// Timer handle of the LED
static app_timer_t led_timer;

// Turn the LED on for LED_BLINK_TIME_MS to indicate a successful procedure.
static void blink_led(void);
// Timer callback turning the LED off.
static void led_timer_cb(app_timer_t *handle, void *data);

// -----------------------------------------------------------------------------
// Bluetooth addresses

// Length of a Bluetooth address printed as a string, with the terminator
#define ADDR_STR_LEN           18

// Print a Bluetooth address into a buffer of ADDR_STR_LEN bytes.
static void format_address(bd_addr address, char *buffer);
// Convert an address string to address data bytes.
static bool decode_address(const char *address_str, bd_addr *address);

/**************************************************************************//**
 * Application Init.
 *****************************************************************************/
void app_init(void)
{
  sl_status_t sc;

  app_assert(role == sl_bt_connection_role_peripheral
             || role == sl_bt_connection_role_central,
             "Invalid Bluetooth role!");

  // Mark every trusted device slot free.
  for (uint8_t i = 0; i < SL_BT_CONFIG_MAX_CONNECTIONS; i++) {
    trusted_devices[i].handle = SL_BT_INVALID_CONNECTION_HANDLE;
    memset(trusted_devices[i].address.addr, 0xff, sizeof(bd_addr));
  }

  // Hand the CBAP procedure over to the component. It authenticates every
  // device that connects and reports the outcome through the callback. The
  // responsibility of the application is to open a connection (one at a time)
  // and to handle CBAP success or failure (disallowlist).
  sc = sl_bt_cbap_init(on_cbap_result, IS_CENTRAL);
  app_assert_status(sc);

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
 * Bluetooth stack event handler.
 * This overrides the default weak implementation.
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
      if (IS_CENTRAL) {
        app_log_info("Central connection role selected." APP_LOG_NL);

        if (peripheral_target_defined) {
          if (decode_address(ADDR, &peripheral_target_addr)) {
            char addr_str[ADDR_STR_LEN];
            format_address(peripheral_target_addr, addr_str);
            app_log_info("Searching for %s." APP_LOG_NL, addr_str);
          } else {
            // decode_address() has logged why. Fall back to accepting any
            // device that advertises the CBAP service.
            peripheral_target_defined = false;
          }
        }

        if (!peripheral_target_defined) {
          app_log_info("Searching for any device advertising the CBAP " \
                       "Service." APP_LOG_NL);
        }
      } else {
        app_log_info("Peripheral connection role selected." APP_LOG_NL);

        // Create an advertising set and fill it with data generated from the
        // local GATT database. The CBAP service is marked as advertised, so
        // its UUID gets into the advertisement, which is what a central device
        // running this example scans for.
        sc = sl_bt_advertiser_create_set(&advertising_set_handle);
        app_assert_status(sc);

        sc = sl_bt_legacy_advertiser_generate_data(
          advertising_set_handle,
          sl_bt_advertiser_general_discoverable);
        app_assert_status(sc);

        sc = sl_bt_advertiser_set_timing(advertising_set_handle,
                                         ADV_INTERVAL_UNITS, // min. interval
                                         ADV_INTERVAL_UNITS, // max. interval
                                         0,                  // duration
                                         0);                 // max. num. events
        app_assert_status(sc);
      }

      start_discovery();
      break;

    // -------------------------------
    // This event is generated when an advertisement packet or a scan response
    // is received from a responder. Only a central device scans, so only a
    // central device gets here.
    case sl_bt_evt_scanner_legacy_advertisement_report_id:
      if (!check_scan_report(
            &evt->data.evt_scanner_legacy_advertisement_report)) {
        break;
      }

      // The component serves a single CBAP procedure at a time, so stop
      // scanning before the connection is opened.
      stop_discovery();

      connection_open_pending = true;
      sc = sl_bt_connection_open(
        evt->data.evt_scanner_legacy_advertisement_report.address,
        evt->data.evt_scanner_legacy_advertisement_report.address_type,
        sl_bt_gap_phy_1m,
        NULL);
      if (sc != SL_STATUS_OK) {
        connection_open_pending = false;
        app_log_error("Failed to open connection: 0x%04lx" APP_LOG_NL, sc);
        start_discovery();
      }
      break;

    // -------------------------------
    // This event indicates that a new connection was opened.
    case sl_bt_evt_connection_opened_id:
      connection_open_pending = false;
      open_connections++;
      app_log_info("Connection %d opened." APP_LOG_NL,
                   evt->data.evt_connection_opened.connection);

      // The component has started the CBAP procedure with this device already,
      // and it can only serve one at a time. Stop advertising, and the
      // scanning of a central device that did not get here from a scan report.
      stop_discovery();

      if (evt->data.evt_connection_opened.bonding
          != SL_BT_INVALID_BONDING_HANDLE) {
        app_log_error("Devices are already bonded and CBAP does not support " \
                      "bonding." APP_LOG_NL);
      }

      // A peripheral device cannot filter out a disallowlisted device before
      // it connects, so it refuses the connection here instead.
      if (is_disallowed(evt->data.evt_connection_opened.address)) {
        app_log_error("The remote device failed the CBAP procedure before. " \
                      "Closing connection." APP_LOG_NL);
        sc = sl_bt_connection_close(
          evt->data.evt_connection_opened.connection);
        app_log_status_error(sc);
      }
      break;

    // -------------------------------
    // This event indicates that a connection was closed.
    case sl_bt_evt_connection_closed_id:
      app_log_info("Connection %d closed." APP_LOG_NL,
                   evt->data.evt_connection_closed.connection);

      if (open_connections > 0) {
        open_connections--;
      }
      remove_trusted_device(evt->data.evt_connection_closed.connection);

      // A connection slot became free. Discovery only resumes when no other
      // connection is being established and CBAP is not authenticating one.
      start_discovery();
      break;

    // -------------------------------
    // This event indicates that the value of an attribute in the local GATT
    // database was changed by a remote GATT client.
    case sl_bt_evt_gatt_server_attribute_value_id:
      if (evt->data.evt_gatt_server_attribute_value.attribute
          != gattdb_aio_digital_out
          || evt->data.evt_gatt_server_attribute_value.value.len == 0) {
        break;
      }

      // The Digital characteristic requires an authenticated and encrypted
      // connection, so only a device that passed CBAP can get here.
      if (evt->data.evt_gatt_server_attribute_value.value.data[0] == LED_OFF) {
        sl_led_turn_off(SL_SIMPLE_LED_INSTANCE(0));
        app_log_info("LED off." APP_LOG_NL);
      } else {
        blink_led();
      }
      break;

    ///////////////////////////////////////////////////////////////////////////
    // Add additional event handlers here as your application requires!      //
    ///////////////////////////////////////////////////////////////////////////

    // -------------------------------
    // Default event handler.
    default:
      break;
  }
}

/**************************************************************************//**
 * Report the outcome of a CBAP procedure.
 *
 * On success the connection is encrypted with an authenticated key, so the
 * secured characteristics of the remote device become accessible. On failure
 * the component has already requested the connection to be closed.
 *
 * @param[in] conn Connection handle and address of the remote device.
 * @param[in] sc SL_STATUS_OK if the procedure succeeded, error code otherwise.
 *****************************************************************************/
static void on_cbap_result(sl_bt_cbap_conn_t conn, sl_status_t sc)
{
  char addr_str[ADDR_STR_LEN];
  format_address(conn.address, addr_str);

  if (sc != SL_STATUS_OK) {
    if (sc == SL_STATUS_BT_CTRL_CONNECTION_FAILED_TO_BE_ESTABLISHED) {
      // A device can reboot and start a new connection before its peer has
      // released the old one. No CBAP data was exchanged, so this is a
      // retryable link-layer race, not an authentication failure.
      app_log_warning("Connection to %s was not established; retrying." \
                      APP_LOG_NL,
                      addr_str);
      return;
    }

    app_log_error("CBAP procedure with %s failed: 0x%04lx" APP_LOG_NL,
                  addr_str,
                  sc);

    // Do not authenticate this device again. Advertising or scanning is
    // restarted once the connection the component closed is gone.
    add_to_disallowlist(conn.address);
    return;
  }

  app_log_info("CBAP procedure with %s succeeded." APP_LOG_NL, addr_str);
  add_trusted_device(conn);
  print_trusted_devices();

  // The success is indicated on the peripheral device only, by the central
  // device writing its Digital characteristic. The write is only permitted over
  // the authenticated connection that CBAP has just created, which makes the
  // blink the proof of the outcome. Both devices run this example, so the
  // attribute handle of the local database is valid on the remote one too.
  if (IS_CENTRAL) {
    uint8_t led_state = LED_ON;
    sl_status_t write_sc;

    write_sc = sl_bt_gatt_write_characteristic_value(
      conn.handle,
      gattdb_aio_digital_out,
      sizeof(led_state),
      &led_state);
    app_log_status_error(write_sc);
  }

  // The component is free again, so another device can be authenticated.
  start_discovery();
}

/**************************************************************************//**
 * Start advertising or scanning, depending on the Bluetooth role.
 *
 * Both are the way to a new CBAP procedure, and the component serves one at a
 * time. Do not begin another one while a connection is opening or CBAP owns a
 * candidate connection.
 *****************************************************************************/
static void start_discovery(void)
{
  sl_status_t sc;

  if (discovery_active
      || connection_open_pending
      || sl_bt_cbap_is_procedure_in_progress()) {
    return;
  }

  if (open_connections >= SL_BT_CONFIG_MAX_CONNECTIONS) {
    app_log_warning("Maximum number of connections reached " \
                    "(SL_BT_CONFIG_MAX_CONNECTIONS: %d)." APP_LOG_NL,
                    SL_BT_CONFIG_MAX_CONNECTIONS);
    return;
  }

  if (IS_CENTRAL) {
    sc = sl_bt_scanner_start(sl_bt_scanner_scan_phy_1m,
                             sl_bt_scanner_discover_generic);
    app_assert_status(sc);
    app_log_info("Scanning started." APP_LOG_NL);
  } else {
    sc = sl_bt_legacy_advertiser_start(advertising_set_handle,
                                       sl_bt_legacy_advertiser_connectable);
    app_assert_status(sc);
    app_log_info("Advertising started." APP_LOG_NL);
  }

  discovery_active = true;
}

/**************************************************************************//**
 * Stop advertising or scanning, depending on the Bluetooth role.
 *****************************************************************************/
static void stop_discovery(void)
{
  sl_status_t sc;

  if (!discovery_active) {
    return;
  }

  if (IS_CENTRAL) {
    sc = sl_bt_scanner_stop();
  } else {
    // The stack stops connectable advertising on its own when a connection is
    // opened, so this can fail without anything being wrong.
    sc = sl_bt_advertiser_stop(advertising_set_handle);
  }
  app_log_status_error(sc);

  discovery_active = false;
}

/**************************************************************************//**
 * Examine a scan report and decide if a connection should be established.
 *
 * @param[in] report Scan report coming from the Bluetooth stack event.
 * @return true if a connection should be established with the device.
 *****************************************************************************/
static bool check_scan_report(
  const sl_bt_evt_scanner_legacy_advertisement_report_t *report)
{
  // Reports that were queued before the scanner was stopped can still arrive.
  // Acting on them would open a second connection behind the back of the CBAP
  // component, so only the reports of an active scan are considered.
  if (!discovery_active) {
    return false;
  }

  // Only a connectable advertisement is worth a connection attempt.
  if ((report->event_flags & SL_BT_SCANNER_EVENT_FLAG_CONNECTABLE) == 0) {
    return false;
  }

  // Skip the devices that failed the CBAP procedure before.
  if (is_disallowed(report->address)) {
    return false;
  }

  // Skip the devices we are connected to already.
  for (uint8_t i = 0; i < SL_BT_CONFIG_MAX_CONNECTIONS; i++) {
    if (memcmp(report->address.addr,
               trusted_devices[i].address.addr,
               sizeof(bd_addr)) == 0) {
      return false;
    }
  }

  // If a target device is configured, only that one is accepted.
  if (peripheral_target_defined
      && memcmp(report->address.addr,
                peripheral_target_addr.addr,
                sizeof(bd_addr)) != 0) {
    return false;
  }

  // Look for the CBAP service in the advertisement packet.
  return find_service_in_advertisement(report->data.data,
                                       report->data.len,
                                       cbap_service_uuid,
                                       sizeof(cbap_service_uuid));
}

/**************************************************************************//**
 * Search for a Service UUID in scan report.
 *
 * @param[in] scan_data Data received in scanner advertisement report event
 * @param[in] scan_data_len Length of the scan data
 * @param[in] uuid Service UUID to search for
 * @param[in] uuid_len Service UUID length
 * @return true if the service is found
 *****************************************************************************/
static bool find_service_in_advertisement(const uint8_t *scan_data,
                                          uint8_t scan_data_len,
                                          const uint8_t *uuid,
                                          uint8_t uuid_len)
{
  uint16_t i = 0;

  // An advertising data field is at least a length and a type byte long.
  while (i + 1 < scan_data_len) {
    uint8_t ad_field_length = scan_data[i]; // Length byte itself not counted
    uint8_t ad_field_type = scan_data[i + 1];

    // A malformed packet could claim a field longer than the data received.
    if (i + ad_field_length + 1 > scan_data_len) {
      break;
    }

    if ((uuid_len == UUID_16_LEN
         && (ad_field_type == GAP_INCOMPLETE_16B_UUID
             || ad_field_type == GAP_COMPLETE_16B_UUID))
        || (uuid_len == UUID_128_LEN
            && (ad_field_type == GAP_INCOMPLETE_128B_UUID
                || ad_field_type == GAP_COMPLETE_128B_UUID))) {
      // A list of complete or incomplete service UUIDs was found. Loop through
      // the UUIDs that fit into the field.
      uint16_t j = 2;
      while (j + uuid_len <= ad_field_length + 1) {
        if (memcmp(&scan_data[i + j], uuid, uuid_len) == 0) {
          return true;
        }
        j += uuid_len;
      }
    }

    // Advance to the next field.
    i += ad_field_length + 1;
  }

  return false;
}

/**************************************************************************//**
 * Add an authenticated connection to the trusted devices array.
 *
 * @param[in] conn Connection handle and address of the remote device.
 *****************************************************************************/
static void add_trusted_device(sl_bt_cbap_conn_t conn)
{
  for (uint8_t i = 0; i < SL_BT_CONFIG_MAX_CONNECTIONS; i++) {
    if (trusted_devices[i].handle == SL_BT_INVALID_CONNECTION_HANDLE) {
      trusted_devices[i] = conn;
      app_log_info("Trusted device [%d] added." APP_LOG_NL, conn.handle);
      return;
    }
  }

  // There are as many slots as connections the stack can keep open, so a
  // device that got this far always has one.
  app_assert(false, "The trusted devices array is full!");
}

/**************************************************************************//**
 * Remove a closed connection from the trusted devices array.
 *
 * @param[in] connection Handle of the connection that was closed.
 *****************************************************************************/
static void remove_trusted_device(uint8_t connection)
{
  for (uint8_t i = 0; i < SL_BT_CONFIG_MAX_CONNECTIONS; i++) {
    if (trusted_devices[i].handle == connection) {
      trusted_devices[i].handle = SL_BT_INVALID_CONNECTION_HANDLE;
      memset(trusted_devices[i].address.addr, 0xff, sizeof(bd_addr));
      app_log_info("Trusted device [%d] removed." APP_LOG_NL, connection);
    }
  }
}

/**************************************************************************//**
 * Log the connection handle and the Bluetooth address of the trusted devices.
 *****************************************************************************/
static void print_trusted_devices(void)
{
  bool found = false;

  app_log_info("List of trusted connections:" APP_LOG_NL);

  for (uint8_t i = 0; i < SL_BT_CONFIG_MAX_CONNECTIONS; i++) {
    if (trusted_devices[i].handle != SL_BT_INVALID_CONNECTION_HANDLE) {
      char addr_str[ADDR_STR_LEN];

      found = true;
      format_address(trusted_devices[i].address, addr_str);
      app_log_info("  Connection handle: %d  Address: %s" APP_LOG_NL,
                   trusted_devices[i].handle,
                   addr_str);
    }
  }

  if (!found) {
    app_log_info("  None." APP_LOG_NL);
  }
}

/**************************************************************************//**
 * Remember a device that failed the CBAP procedure.
 *
 * @param[in] address Bluetooth address of the remote device.
 *****************************************************************************/
static void add_to_disallowlist(bd_addr address)
{
  char addr_str[ADDR_STR_LEN];

  // A disallowlisted device that connects again fails the procedure again, so
  // the same address can be reported more than once.
  if (is_disallowed(address)) {
    return;
  }

  app_assert(disallowlist_len < DISALLOWLIST_SIZE,
             "The disallowlist is full (DISALLOWLIST_SIZE: %d)!",
             DISALLOWLIST_SIZE);

  disallowlist[disallowlist_len] = address;
  disallowlist_len++;

  format_address(address, addr_str);
  app_log_info("Device %s added to the disallowlist (%d/%d)." APP_LOG_NL,
               addr_str,
               disallowlist_len,
               DISALLOWLIST_SIZE);
}

/**************************************************************************//**
 * True if the device failed the CBAP procedure before.
 *
 * @param[in] address Bluetooth address of the remote device.
 * @return true if the device is on the disallowlist.
 *****************************************************************************/
static bool is_disallowed(bd_addr address)
{
  for (uint8_t i = 0; i < disallowlist_len; i++) {
    if (memcmp(address.addr, disallowlist[i].addr, sizeof(bd_addr)) == 0) {
      return true;
    }
  }

  return false;
}

/**************************************************************************//**
 * Turn the LED on for LED_BLINK_TIME_MS to indicate a successful procedure.
 *****************************************************************************/
static void blink_led(void)
{
  sl_status_t sc;

  sl_led_turn_on(SL_SIMPLE_LED_INSTANCE(0));
  app_log_info("LED on." APP_LOG_NL);

  sc = app_timer_start(&led_timer,
                       LED_BLINK_TIME_MS,
                       led_timer_cb,
                       (void *)NULL, // Callback has no parameters
                       false);
  app_assert_status(sc);
}

/**************************************************************************//**
 * Timer callback turning the LED off.
 *
 * @param[in] handle Pointer to handle instance
 * @param[in] data Pointer to input data
 *****************************************************************************/
static void led_timer_cb(app_timer_t *handle, void *data)
{
  sl_status_t sc;
  uint8_t led_state = LED_OFF;

  (void)handle;
  (void)data;

  // Keep the local GATT database in sync with the state of the LED.
  sc = sl_bt_gatt_server_write_attribute_value(gattdb_aio_digital_out,
                                               0, // offset
                                               sizeof(led_state),
                                               &led_state);
  app_log_status_error(sc);

  sl_led_turn_off(SL_SIMPLE_LED_INSTANCE(0));
  app_log_info("LED off." APP_LOG_NL);
}

/**************************************************************************//**
 * Print a Bluetooth address into a buffer of ADDR_STR_LEN bytes.
 *
 * @param[in] address Bluetooth address byte array
 * @param[out] buffer Buffer of at least ADDR_STR_LEN bytes
 *****************************************************************************/
static void format_address(bd_addr address, char *buffer)
{
  (void)snprintf(buffer, ADDR_STR_LEN, "%02X:%02X:%02X:%02X:%02X:%02X",
                 address.addr[5],
                 address.addr[4],
                 address.addr[3],
                 address.addr[2],
                 address.addr[1],
                 address.addr[0]);
}

/**************************************************************************//**
 * Convert an address string to address data bytes.
 *
 * @param[in] address_str Address string
 * @param[out] address Bluetooth address byte array
 * @return true if operation was successful
 *****************************************************************************/
static bool decode_address(const char *address_str, bd_addr *address)
{
  int retval;
  unsigned int address_cache[sizeof(bd_addr)];

  retval = sscanf(address_str, "%02X:%02X:%02X:%02X:%02X:%02X",
                  &address_cache[5],
                  &address_cache[4],
                  &address_cache[3],
                  &address_cache[2],
                  &address_cache[1],
                  &address_cache[0]);

  if (retval != (int)sizeof(bd_addr)) {
    app_log_error("Invalid Bluetooth address: %s" APP_LOG_NL, address_str);
    return false;
  }

  for (uint8_t i = 0; i < sizeof(bd_addr); i++) {
    address->addr[i] = (uint8_t)(address_cache[i]);
  }

  return true;
}
