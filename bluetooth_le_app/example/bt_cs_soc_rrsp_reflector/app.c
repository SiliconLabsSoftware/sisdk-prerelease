/***************************************************************************//**
 * @file
 * @brief CS SoC RRSP reflector example application logic
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
#include "app_assert.h"
#include "sl_bt_api.h"
#include "gatt_db.h"
#include "app_log.h"
#include "app.h"
#include "app_config.h"
#include "sl_main_init.h"
#include "sl_bt_peer_manager_peripheral.h"
#include "cs_manager.h"
#include "cs_manager_config.h"
#include "cs_antenna.h"
#include "cs_antenna_config.h"
#include "cs_common.h"
#include "cs_ras_server.h"
// Security
#include "sl_bt_peer_security.h"
#include "app_button_press.h"

// TODO: needs testing
#ifdef SL_CATALOG_CS_REFLECTOR_CLI_PRESENT
#include "cs_reflector_cli.h"
#endif // SL_CATALOG_CS_REFLECTOR_CLI_PRESENT

#define APP_PREFIX                  "[APP] "
#define INSTANCE_PREFIX             "[%u] "
#define APP_INSTANCE_PREFIX         APP_PREFIX INSTANCE_PREFIX

static cs_manager_instance_config_t cs_manager_config;

static void on_connection_opened_with_initiator(uint8_t conn_handle);
static void on_connection_closed(uint8_t conn_handle);
static void on_cs_manager_event(uint8_t conn_handle,
                                uint8_t config_id,
                                cs_manager_event_type_t event,
                                sl_status_t status);
static void on_cs_manager_error(uint8_t conn_handle,
                                cs_manager_error_t error,
                                sl_status_t sc);

void cs_ras_server_on_mode_change(uint8_t connection, cs_ras_mode_t mode,
                                  bool indication)
{
  app_log_debug(APP_INSTANCE_PREFIX "RAS mode changed to %u" APP_LOG_NL, connection,
                mode);
  (void)connection;
  (void)mode;
  (void)indication;
}

/**************************************************************************//**
 * Application Init
 *****************************************************************************/
void app_init(void)
{
  app_log_info(APP_LOG_NL);
  app_log_info("+-[CS Reflector by Silicon Labs]---------------+" APP_LOG_NL);

  cs_manager_event_callback_t manager_callbacks = {
    .on_event = on_cs_manager_event,
    .on_error = on_cs_manager_error,
  };
  sl_status_t sc = cs_manager_set_event_callbacks(&manager_callbacks);
  if (sc != SL_STATUS_OK) {
    app_log_error(APP_PREFIX "Failed to register CS Manager event callbacks! [sc: 0x%lx]" APP_LOG_NL,
                  (unsigned long)sc);
    app_assert_status(sc);
  }

  cs_manager_get_default_instance_config(false, // Reflector CS role
                                         false, // Peripheral role
                                         &cs_manager_config);
  cs_manager_config.request_conn_phy = false;

  app_log_info(APP_PREFIX "Maximum concurrent connections: %u" APP_LOG_NL,
               CS_MANAGER_CONFIG_MAX_INSTANCES);
  app_log_debug(APP_PREFIX "Default minimum transmit power: %d dBm" APP_LOG_NL,
                CS_MANAGER_CONFIG_DEFAULT_MIN_TX_POWER_DBM);
  app_log_debug(APP_PREFIX "Default maximum transmit power: %d dBm" APP_LOG_NL,
                CS_MANAGER_CONFIG_DEFAULT_MAX_TX_POWER_DBM);

  app_log_info(APP_PREFIX "Wire%s antenna offset will be used." APP_LOG_NL,
               CS_ANTENNA_CONFIG_DEFAULT_ANTENNA_OFFSET ? "d" : "less");

  switch (cs_manager_config.cs_sync_antenna) {
    case CS_SYNC_ANTENNA_1:
      app_log_info(APP_PREFIX "Antenna 1 will be used for RTT" APP_LOG_NL);
      break;
    case CS_SYNC_ANTENNA_2:
      app_log_info(APP_PREFIX "Antenna 2 will be used for RTT" APP_LOG_NL);
      break;
    default:
      if (cs_manager_config.cs_sync_antenna != CS_SYNC_SWITCHING) {
        app_log_warning(APP_PREFIX "Unknown RTT antenna usage (%d)! " APP_LOG_NL,
                        cs_manager_config.cs_sync_antenna);
        cs_manager_config.cs_sync_antenna = CS_SYNC_SWITCHING;
      }
      app_log_info(APP_PREFIX "Switching between all antennas for RTT" APP_LOG_NL);
      break;
  }

  app_log_info("+-------------------------------------------------------+" APP_LOG_NL);

  /////////////////////////////////////////////////////////////////////////////
  // Put your additional application init code here!                         //
  // This is called once during start-up.                                    //
  /////////////////////////////////////////////////////////////////////////////
}
/**************************************************************************//**
 * Application Process Action
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
 * Bluetooth stack event handler
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
    {
      bd_addr address;
      uint8_t address_type;
      sc = sl_bt_gap_get_identity_address(&address, &address_type);
      app_assert_status(sc);
      // Print the Bluetooth address
      app_log_info(APP_PREFIX "Bluetooth %s address: %02X:%02X:%02X:%02X:%02X:%02X\n",
                   address_type ? "static random" : "public device",
                   address.addr[5],
                   address.addr[4],
                   address.addr[3],
                   address.addr[2],
                   address.addr[1],
                   address.addr[0]);

      // Start advertising for initiator connections
      if (!cs_manager_is_full()) {
#ifndef SL_CATALOG_CS_REFLECTOR_CLI_PRESENT
        sc = sl_bt_peer_manager_peripheral_start_advertising(SL_BT_INVALID_ADVERTISING_SET_HANDLE);
        app_assert_status(sc);
        app_log_info(APP_PREFIX "Advertising started for initiator connections..." APP_LOG_NL);
#else
        app_log_info(APP_PREFIX "CS CLI is active." APP_LOG_NL);
#endif // SL_CATALOG_CS_REFLECTOR_CLI_PRESENT
      }
    }
    break;

    // -------------------------------
    // This event indicates that the BT stack buffer resources were exhausted
    case sl_bt_evt_system_resource_exhausted_id:
      app_log_error(APP_PREFIX "BT stack buffers exhausted, data loss may have occurred! "
                               "buf_discarded='%u' buf_alloc_fail='%u' heap_alloc_fail='%u'" APP_LOG_NL,
                    evt->data.evt_system_resource_exhausted.num_buffers_discarded,
                    evt->data.evt_system_resource_exhausted.num_buffer_allocation_failures,
                    evt->data.evt_system_resource_exhausted.num_heap_allocation_failures);
      break;

    // -------------------------------
    // This event is received when a BT stack system error occurs
    case sl_bt_evt_system_error_id:
      app_log_error(APP_PREFIX "System error occurred; reason='0x%02x'" APP_LOG_NL,
                    evt->data.evt_system_error.reason);
      break;

    // -------------------------------
    // This event indicates that a MTU exchange has finished
    case sl_bt_evt_gatt_mtu_exchanged_id:
    {
      const sl_bt_evt_gatt_mtu_exchanged_t *p = &evt->data.evt_gatt_mtu_exchanged;
      app_log_debug(APP_INSTANCE_PREFIX "MTU exchange completed; mtu=%u" APP_LOG_NL,
                    p->connection,
                    p->mtu);
      break;
    }

    // -------------------------------
    // This event indicates that the connection parameters were changed
    case sl_bt_evt_connection_parameters_id:
    {
      const sl_bt_evt_connection_parameters_t *p = &evt->data.evt_connection_parameters;
      app_log_debug(APP_INSTANCE_PREFIX "Connection parameters changed; "
                                        "interval=%u, latency=%u, timeout=%u, security_mode=%u" APP_LOG_NL,
                    p->connection,
                    p->interval,
                    p->latency,
                    p->timeout,
                    p->security_mode);
      break;
    }

    // -------------------------------
    // This event indicates that a PHY update procedure has finished
    case sl_bt_evt_connection_phy_status_id:
    {
      const sl_bt_evt_connection_phy_status_t *p = &evt->data.evt_connection_phy_status;
      cs_manager_config.actual_conn_phy = p->phy;
      app_log_debug(APP_INSTANCE_PREFIX "PHY update procedure completed; phy=%u" APP_LOG_NL,
                    p->connection,
                    p->phy);
      break;
    }

    // -------------------------------
    // This event indicates that the features supported by the remote device's LL were updated
    case sl_bt_evt_connection_remote_used_features_id:
      app_log_debug(APP_INSTANCE_PREFIX "Remote LL supported features updated" APP_LOG_NL,
                    evt->data.evt_connection_remote_used_features.connection);
      break;

    // -------------------------------
    // This event indicates that the maximum Rx/Tx data length was changed
    case sl_bt_evt_connection_data_length_id:
      app_log_debug(APP_INSTANCE_PREFIX "Maximum payload length changed: %u" APP_LOG_NL,
                    evt->data.evt_connection_data_length.connection,
                    evt->data.evt_connection_data_length.tx_data_len);
      break;

    // -------------------------------
    // This event indicates that a bonding procedure has successfully finished
    case sl_bt_evt_sm_bonded_id:
      app_log_debug(APP_INSTANCE_PREFIX "Paring/bonding procedure successfully completed" APP_LOG_NL,
                    evt->data.evt_sm_bonded.connection);
      break;

    // -------------------------------
    // This event indicates that the radio transmit power was changed
    case sl_bt_evt_connection_tx_power_id:
      app_log_debug(APP_INSTANCE_PREFIX "Transmit power changed; tx_power='%d'" APP_LOG_NL,
                    evt->data.evt_connection_tx_power.connection,
                    evt->data.evt_connection_tx_power.power_level);
      break;

    // -------------------------------
    // Default event handler
    default:
      break;
  }
}

static void on_connection_opened_with_initiator(uint8_t conn_handle)
{
  sl_status_t sc;
#ifdef SL_CATALOG_CS_REFLECTOR_CLI_PRESENT
  cs_manager_config.cs_sync_antenna = cs_reflector_cli_get_cs_sync_antenna_usage();
#endif // SL_CATALOG_CS_REFLECTOR_CLI_PRESENT
  // Create a new instance for the connection handle
  sc = cs_manager_create(conn_handle, &cs_manager_config, NULL);
  if (sc != SL_STATUS_OK) {
    app_log_error(APP_INSTANCE_PREFIX "Failed to create reflector instance'" APP_LOG_NL, conn_handle);
    sl_bt_peer_manager_peripheral_close_connection(conn_handle);
    return;
  }

  // Advertise for new initiator connections if we have room for more
  if (!cs_manager_is_full()) {
    sc = sl_bt_peer_manager_peripheral_start_advertising(SL_BT_INVALID_ADVERTISING_SET_HANDLE);
    app_assert_status(sc);
    app_log_info(APP_PREFIX "Advertising restarted for new initiator connections..." APP_LOG_NL);
  }
}

static void on_connection_closed(uint8_t conn_handle)
{
  sl_status_t sc;
  bool advertisement_should_be_restarted = false;
  // Remove the instance for the connection handle
  sc = cs_manager_delete(conn_handle);
  app_assert_status_f(sc, "Failed to delete instance");
  // If we are at the maximum capacity - it means that the advertisement is not running
  // Restart advertising for new initiator connections if we were at the limit
  if (!cs_manager_is_full()) {
    advertisement_should_be_restarted = true;
  }

  // Restart advertising if needed
  if (advertisement_should_be_restarted) {
    sc = sl_bt_peer_manager_peripheral_start_advertising(SL_BT_INVALID_ADVERTISING_SET_HANDLE);
    app_assert_status(sc);
    app_log_info(APP_PREFIX "Advertising restarted for new initiator connections..." APP_LOG_NL);
  }
}

static void on_cs_manager_event(uint8_t conn_handle,
                                uint8_t config_id,
                                cs_manager_event_type_t event,
                                sl_status_t status)
{
  (void)config_id;

  switch (event) {
    case CS_MANAGER_EVENT_INSTANCE_CREATE_COMPLETE:
      if (status != SL_STATUS_OK) {
        app_log_error(APP_INSTANCE_PREFIX "Failed to create CS Manager instance"
                                          " [sc: 0x%lx]" APP_LOG_NL,
                      conn_handle, (unsigned long)status);
        (void)sl_bt_peer_manager_peripheral_close_connection(conn_handle);
        return;
      }
      app_log_info(APP_INSTANCE_PREFIX "CS Manager instance created" APP_LOG_NL,
                   conn_handle);
      break;

    case CS_MANAGER_EVENT_INSTANCE_REMOVE_COMPLETE:
      app_log_info(APP_INSTANCE_PREFIX "CS Manager instance removed" APP_LOG_NL,
                   conn_handle);
      break;

    case CS_MANAGER_EVENT_CONFIG_CREATE_COMPLETE:
      app_log_info(APP_INSTANCE_PREFIX "CS configuration created by initiator"
                   APP_LOG_NL, conn_handle);
      break;

    case CS_MANAGER_EVENT_CONFIG_OVERWRITTEN:
      app_log_info(APP_INSTANCE_PREFIX "CS configuration overwritten by initiator"
                   APP_LOG_NL, conn_handle);
      break;

    case CS_MANAGER_EVENT_CONFIG_REMOVE_COMPLETE:
      app_log_info(APP_INSTANCE_PREFIX "CS configuration removed" APP_LOG_NL,
                   conn_handle);
      break;

    case CS_MANAGER_EVENT_PROCEDURE_START_COMPLETE:
      app_log_info(APP_INSTANCE_PREFIX "CS procedure started" APP_LOG_NL,
                   conn_handle);
      break;

    case CS_MANAGER_EVENT_PROCEDURE_STOP_COMPLETE:
      app_log_info(APP_INSTANCE_PREFIX "CS procedure stopped" APP_LOG_NL,
                   conn_handle);
      break;

    case CS_MANAGER_EVENT_ERROR:
      app_log_error(APP_INSTANCE_PREFIX "CS Manager general error"
                                        " [sc: 0x%lx]" APP_LOG_NL,
                    conn_handle, (unsigned long)status);
      if (conn_handle != SL_BT_INVALID_CONNECTION_HANDLE) {
        (void)sl_bt_peer_manager_peripheral_close_connection(conn_handle);
      }
      break;

    default:
      app_log_debug(APP_INSTANCE_PREFIX "Unhandled CS Manager event (%u)"
                                        " [sc: 0x%lx]" APP_LOG_NL,
                    conn_handle, (unsigned)event, (unsigned long)status);
      break;
  }
}

static void on_cs_manager_error(uint8_t conn_handle,
                                cs_manager_error_t error,
                                sl_status_t sc)
{
  app_log_error(APP_INSTANCE_PREFIX "CS Manager error (%u) [sc: 0x%lx]" APP_LOG_NL,
                conn_handle, (unsigned)error, (unsigned long)sc);
}

void sl_bt_peer_manager_on_event_reflector(const sl_bt_peer_manager_evt_type_t *event)
{
  switch (event->evt_id) {
    case SL_BT_PEER_MANAGER_ON_CONN_OPENED_PERIPHERAL:
    {
      #if APP_LOG_ENABLE
      const bd_addr *address = sl_bt_peer_manager_get_bt_address(event->connection_id);
      app_log_info(APP_INSTANCE_PREFIX "Connection opened as peripheral with CS Initiator"
                                       " '%02X:%02X:%02X:%02X:%02X:%02X'" APP_LOG_NL,
                   event->connection_id,
                   address->addr[5],
                   address->addr[4],
                   address->addr[3],
                   address->addr[2],
                   address->addr[1],
                   address->addr[0]);
      #endif // APP_LOG_ENABLE
      on_connection_opened_with_initiator(event->connection_id);
    }
    break;

    case SL_BT_PEER_MANAGER_ON_CONN_CLOSED:
      app_log_info(APP_INSTANCE_PREFIX "Connection closed" APP_LOG_NL, event->connection_id);
      on_connection_closed(event->connection_id);
      break;

    case SL_BT_PEER_MANAGER_ON_ADV_STOPPED:
      app_log_info(APP_INSTANCE_PREFIX "Advertisement stopped" APP_LOG_NL, event->connection_id);
      break;

    case SL_BT_PEER_MANAGER_ERROR:
      app_log_error(APP_INSTANCE_PREFIX "Peer Manager error" APP_LOG_NL, event->connection_id);
      break;

    default:
      app_log_debug(APP_INSTANCE_PREFIX "Unhandled Peer Manager event (%u)" APP_LOG_NL, event->connection_id, event->evt_id);
      break;
  }
}

void sl_bt_peer_security_on_event(uint8_t handle)
{
  app_log_info(APP_INSTANCE_PREFIX "Security process started" APP_LOG_NL, handle);
}

void app_button_press_cb(uint8_t button, uint8_t duration)
{
  (void)duration; //unused parameter
  if (button == 0) {
    sl_bt_peer_security_send_confirmation(true);
  } else if (button == 1) {
    sl_bt_peer_security_send_confirmation(false);
  }
}
