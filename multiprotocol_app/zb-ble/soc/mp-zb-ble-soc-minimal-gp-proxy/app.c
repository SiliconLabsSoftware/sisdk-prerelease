/***************************************************************************//**
 * @file
 * @brief
 *******************************************************************************
 * # License
 * <b>Copyright 2021 Silicon Laboratories Inc. www.silabs.com</b>
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

#include PLATFORM_HEADER
#include "hal.h"
#include "sl_zigbee.h"
#include "zigbee_app_framework_event.h"
#include "sl_component_catalog.h"

#ifdef SL_CATALOG_ZIGBEE_DEBUG_PRINT_PRESENT
#include "sl_zigbee_debug_print.h"
#endif // SL_CATALOG_ZIGBEE_DEBUG_PRINT_PRESENT
#include "sl_zigbee_system_common.h"
#include "app.h"
#include "stack/core/sl_zigbee_multi_network.h"
#include <stdlib.h>

#ifdef SL_COMPONENT_CATALOG_PRESENT
#include "sl_component_catalog.h"
#endif

#if defined(SL_CATALOG_BLUETOOTH_PRESENT)
#include "sl_bluetooth.h"
#include "sl_zigbee_system_common.h"
#include "sl_bluetooth.h"
#include "sl_bluetooth_advertiser_config.h"
#include "sl_bluetooth_connection_config.h"
#include "sl_component_catalog.h"
#endif //SL_CATALOG_BLUETOOTH_PRESENT
//------------------------------------------------------------------------------
// Global

static const uint16_t gpCluster[] = { 0x21 };
static sl_zigbee_endpoint_description_t const gpDescriptor = {
  0xA1E0,  // Profile ID.
  102,     // SRC device ID.
  0,       // No complex or user descriptor.
  1,       // One input clusters.
  1        // One output cluster.
};
uint8_t sl_zigbee_endpoint_count = 1;
sl_zigbee_endpoint_t sl_zigbee_endpoints[] = {
  { 242, &gpDescriptor, gpCluster, gpCluster },
};

uint8_t joinDecision = SL_ZIGBEE_SEND_KEY_IN_THE_CLEAR;

static sl_zigbee_af_event_t appEvent;

//------------------------------------------------------------------------------
// Forward declarations

static void printBuffer(const uint8_t* buffer, uint8_t length);
static void appEventHandler(sl_zigbee_af_event_t *event);

//------------------------------------------------------------------------------
// Stack callbacks
extern void sl_zigbee_af_init(uint8_t init_level);
extern void sl_zigbee_af_endpoint_configure(void);

void sl_zigbee_af_main_init_cb(void)
{
  sl_zigbee_app_debug_println("BOOT: Simple ZigBee App");

  sl_zigbee_af_event_init(&appEvent, appEventHandler);

  sl_zigbee_af_endpoint_configure();
  sl_zigbee_af_init(SL_ZIGBEE_INIT_LEVEL_DONE);
}

void sl_zigbee_af_stack_status_cb(sl_status_t status)
{
  (void)status;

  sl_zigbee_app_debug_println("Stack status handler, 0x%08X", status);
}

sl_zigbee_join_decision_t sli_zigbee_af_trust_center_pre_join_callback(sl_802154_short_addr_t newNodeId,
                                                                       sl_802154_long_addr_t newNodeEui64,
                                                                       sl_zigbee_device_update_t status,
                                                                       sl_802154_short_addr_t parentOfNewNode)
{
  (void)parentOfNewNode;
  (void)newNodeId;

  bool secured = (status == SL_ZIGBEE_STANDARD_SECURITY_SECURED_REJOIN);
  bool rejoin = (status == SL_ZIGBEE_STANDARD_SECURITY_SECURED_REJOIN
                 || status == SL_ZIGBEE_STANDARD_SECURITY_UNSECURED_REJOIN);
  sl_zigbee_join_decision_t handlerDecision = joinDecision;
  UNUSED char * decisionText[] = { "USE_PRE", "SEND_KEY", "DENY", "NADA" };

  sl_zigbee_current_security_state_t securityState;
  sl_status_t securityStatus = sl_zigbee_get_current_security_state(&securityState);

  if ((status == SL_ZIGBEE_DEVICE_LEFT)
      || (secured && rejoin)
      // 4.6.3.3.2 - TC rejoins rejected in distributed TC mode
      || ((SL_STATUS_OK == securityStatus)
          && (securityState.bitmask & SL_ZIGBEE_DISTRIBUTED_TRUST_CENTER_MODE)
          && !secured && rejoin)) {
    handlerDecision = SL_ZIGBEE_NO_ACTION;
  }

  if (status == SL_ZIGBEE_DEVICE_LEFT) {
    sl_zigbee_app_debug_print("Device leave: 0x%04X, ", newNodeId);
  } else {
    // Note that the decision here is not final
    // If the device is using the well-known link key, even if the status here
    // is SL_ZIGBEE_USE_PRECONFIGURED_KEY, the device will not be sent the Network
    // Key if doing a rejoin unless the rejoin policy is updated (see
    // setAllowRejoins)
    sl_zigbee_app_debug_print("Join decision %s for device ", decisionText[handlerDecision]);
  }

  printBuffer(newNodeEui64, EUI64_SIZE);

  if ( status != SL_ZIGBEE_DEVICE_LEFT ) {
    sl_zigbee_app_debug_print(" (%ssecure %sjoin)",
                              (secured
                               ? ""
                               : "un"),
                              (rejoin
                               ? "re"
                               : ""));
  }
  sl_zigbee_app_debug_println("");

  return handlerDecision;
}

static void testProfileMessageHandler(sl_802154_short_addr_t sender,
                                      sl_zigbee_aps_frame_t *apsFrame,
                                      uint8_t messageLength,
                                      uint8_t *message)
{
  (void)messageLength;

  switch (apsFrame->clusterId) {
    case CLUSTER_BUFFER_TEST_REQUEST: {
      sl_status_t status;
      sl_zigbee_aps_frame_t outApsFrame;
      uint8_t sequenceLength = message[0];
      uint8_t *response = (uint8_t*)malloc(sequenceLength + 2);

      outApsFrame.sourceEndpoint = apsFrame->destinationEndpoint;
      outApsFrame.destinationEndpoint = apsFrame->sourceEndpoint;
      outApsFrame.clusterId = CLUSTER_BUFFER_TEST_RESPONSE;
      outApsFrame.profileId = TEST_PROFILE_ID;
      outApsFrame.options = apsFrame->options;

      response[0] = sequenceLength;
      response[1] = 0x00; // status: success
      for (uint8_t i = 0; i < sequenceLength; i++) {
        response[2 + i] = i;
      }

      status = sl_zigbee_send_unicast(SL_ZIGBEE_OUTGOING_DIRECT,
                                      sender,
                                      &outApsFrame,
                                      0x00, // tag
                                      sequenceLength + 2,
                                      response,
                                      NULL);
      free(response);

      if (status != SL_STATUS_OK) {
        sl_zigbee_app_debug_println("sl_zigbee_send_unicast fails");
        return;
      }
      sl_zigbee_app_debug_println("Responded to buffer request, length 0x%02X", sequenceLength);
      break;
    }

    case CLUSTER_BUFFER_TEST_RESPONSE: {
      sl_zigbee_app_debug_println("Buffer response: length 0x%02X, status 0x%02X",
                                  message[0], message[1]);
      break;
    }

    default:
      break;
  }
}

// The entry functions to send message broadcast, multicast or unicast
#define ZA_MAX_HOPS 12
#define INVALID_MESSAGE_TAG 0x0000

void sl_zigbee_af_incoming_message_cb(sl_zigbee_incoming_message_type_t type,
                                      sl_zigbee_aps_frame_t *apsFrame,
                                      sl_zigbee_rx_packet_info_t *packetInfo,
                                      uint8_t messageLength,
                                      uint8_t *message)
{
  (void)type;
  (void)packetInfo;

  if (apsFrame->profileId == TEST_PROFILE_ID) {
    testProfileMessageHandler(packetInfo->sender_short_id, apsFrame, messageLength, message);
  }
}

// This function is defined in AF or NCP
// Since this app does not use AF, we have to define it by our own

extern void sli_zigbee_af_incoming_message_handler(sl_zigbee_incoming_message_type_t type,
                                                   sl_zigbee_aps_frame_t *apsFrame,
                                                   sl_zigbee_rx_packet_info_t *packetInfo,
                                                   uint16_t messageLength,
                                                   uint8_t *messageContents);

void sli_zigbee_af_incoming_message_callback(sl_zigbee_incoming_message_type_t type,
                                             sl_zigbee_aps_frame_t *apsFrame,
                                             sl_zigbee_rx_packet_info_t *packetInfo,
                                             uint8_t messageLength,
                                             uint8_t *message)
{
  sli_zigbee_af_incoming_message_handler(type,
                                         apsFrame,
                                         packetInfo,
                                         messageLength,
                                         message);
}

// same as sli_zigbee_af_incoming_message_handler above, which does not exist in this app
void sli_zigbee_af_fragmentation_message_sent_handler(sl_status_t status,
                                                      sl_zigbee_outgoing_message_type_t type,
                                                      uint16_t indexOrDestination,
                                                      sl_zigbee_aps_frame_t *apsFrame,
                                                      uint16_t messageTag,
                                                      uint8_t *buffer,
                                                      uint16_t bufLen)
{
  (void)status;
  (void)type;
  (void)indexOrDestination;
  (void)apsFrame;
  (void)buffer;
  (void)bufLen;
  (void)messageTag;
}

//------------------------------------------------------------------------------
// Internal APIs
// The entry functions to send message broadcast, multicast or unicast
#define ZA_MAX_HOPS 12
#define INVALID_MESSAGE_TAG 0x0000
uint16_t sli_zigbee_af_calculate_message_tag_hash(uint8_t *messageContents,
                                                  uint8_t messageLength)
{
  uint8_t temp[SL_ZIGBEE_ENCRYPTION_KEY_SIZE];
  uint16_t hashReturn = 0;
  sl_zigbee_aes_hash_simple(messageLength, messageContents, temp);
  for (uint8_t i = 0; i < SL_ZIGBEE_ENCRYPTION_KEY_SIZE; i += 2) {
    hashReturn ^= *((uint16_t *)(temp + i));
  }
  if (hashReturn == INVALID_MESSAGE_TAG) {
    hashReturn = 1;
  }
  return hashReturn;
}

sl_status_t sli_zigbee_af_send(sl_zigbee_outgoing_message_type_t type,
                               uint16_t indexOrDestination,
                               sl_zigbee_aps_frame_t *apsFrame,
                               uint8_t messageLength,
                               uint8_t *message,
                               uint16_t *messageTag,
                               sl_802154_short_addr_t alias,
                               uint8_t sequence)
{
  sl_status_t status;

  *messageTag = sli_zigbee_af_calculate_message_tag_hash(message, messageLength);
  uint8_t nwkRadius = ZA_MAX_HOPS;
  sl_802154_short_addr_t nwkAlias = SL_ZIGBEE_NULL_NODE_ID;

  switch (type) {
    case SL_ZIGBEE_OUTGOING_DIRECT:
    case SL_ZIGBEE_OUTGOING_VIA_ADDRESS_TABLE:
    case SL_ZIGBEE_OUTGOING_VIA_BINDING:
      status = sl_zigbee_send_unicast(type,
                                      indexOrDestination,
                                      apsFrame,
                                      *messageTag,
                                      messageLength,
                                      message,
                                      NULL);
      break;
    case SL_ZIGBEE_OUTGOING_MULTICAST:
    case SL_ZIGBEE_OUTGOING_MULTICAST_WITH_ALIAS:
      if (type == SL_ZIGBEE_OUTGOING_MULTICAST_WITH_ALIAS
          || (apsFrame->sourceEndpoint == SL_ZIGBEE_GP_ENDPOINT
              && apsFrame->destinationEndpoint == SL_ZIGBEE_GP_ENDPOINT
              && apsFrame->options & SL_ZIGBEE_APS_OPTION_USE_ALIAS_SEQUENCE_NUMBER)) {
        nwkRadius = apsFrame->radius;
        nwkAlias = alias;
      }
      status = sl_zigbee_send_multicast(apsFrame,
                                        nwkRadius,      //radius
                                        0,      // broadcast Addr
                                        nwkAlias,
                                        sequence,
                                        *messageTag,
                                        messageLength,
                                        message,
                                        NULL);
      break;
    case SL_ZIGBEE_OUTGOING_BROADCAST:
    case SL_ZIGBEE_OUTGOING_BROADCAST_WITH_ALIAS:
      if (type == SL_ZIGBEE_OUTGOING_BROADCAST_WITH_ALIAS
          || (apsFrame->sourceEndpoint == SL_ZIGBEE_GP_ENDPOINT
              && apsFrame->destinationEndpoint == SL_ZIGBEE_GP_ENDPOINT
              && apsFrame->options & SL_ZIGBEE_APS_OPTION_USE_ALIAS_SEQUENCE_NUMBER)) {
        nwkRadius = apsFrame->radius;
        nwkAlias = alias;
      }
      status = sl_zigbee_send_broadcast(nwkAlias,
                                        indexOrDestination,
                                        sequence,
                                        apsFrame,
                                        nwkRadius, // radius
                                        *messageTag, // tag
                                        messageLength,
                                        message,
                                        NULL);
      break;
    default:
      status = SL_STATUS_INVALID_PARAMETER;
      break;
  }

  return status;
}

//------------------------------------------------------------------------------
// Event handlers

static void appEventHandler(sl_zigbee_af_event_t *event)
{
  (void)event;

  // app event handler
}

//------------------------------------------------------------------------------
// Utility functions

static void printBuffer(const uint8_t* buffer, uint8_t length)
{
  UNUSED_VAR(buffer);
  for (uint8_t i = 0; i < length; i++) {
    sl_zigbee_app_debug_print("%02X", buffer[i]);
  }
}

//------------------------------------------------------------------------------
// Stubs

WEAK(void halButtonIsr(uint8_t button, uint8_t state))
{
  (void)button;
  (void)state;
}

#ifdef SL_CATALOG_BLUETOOTH_PRESENT
//------------------------------------------------------------------------------
// Bluetooth Event handler
static uint8_t cli_adv_handle;
void zb_ble_dmp_print_ble_address(uint8_t *address)
{
  UNUSED_VAR(address);
  sl_zigbee_app_debug_print("\nBLE address: [%02X %02X %02X %02X %02X %02X]\n",
                            address[5], address[4], address[3],
                            address[2], address[1], address[0]);
}

void sl_bt_on_event(sl_bt_msg_t* evt)
{
  halResetWatchdog();
  switch (SL_BT_MSG_ID(evt->header)) {
    case sl_bt_evt_system_boot_id: {
      bd_addr ble_address;
      uint8_t type;
      UNUSED sl_status_t status = sl_bt_system_hello();
      sl_zigbee_app_debug_println("BLE hello: %s",
                                  (status == SL_STATUS_OK) ? "success" : "error");

      sl_bt_system_get_identity_address(&ble_address, &type);
      zb_ble_dmp_print_ble_address(ble_address.addr);

      status = sl_bt_advertiser_create_set(&cli_adv_handle);
      if (status) {
        sl_zigbee_app_debug_println("sl_bt_advertiser_create_set status 0x%02x", status);
      }
    }
    break;

    case sl_bt_evt_connection_opened_id: {
      sl_zigbee_app_debug_println("sl_bt_evt_connection_opened_id \n");
      const sl_bt_evt_connection_opened_t *conn_evt =
        (sl_bt_evt_connection_opened_t*) &(evt->data);
      sl_bt_connection_set_preferred_phy(conn_evt->connection, sl_bt_test_phy_1m, 0xff);
      sl_zigbee_app_debug_println("BLE connection opened");
    }
    break;
    case sl_bt_evt_connection_phy_status_id: {
      UNUSED const sl_bt_evt_connection_phy_status_t *conn_evt =
        (sl_bt_evt_connection_phy_status_t *)&(evt->data);
      // indicate the PHY that has been selected
      sl_zigbee_app_debug_println("now using the %dMPHY\r\n",
                                  conn_evt->phy);
    }
    break;
    case sl_bt_evt_connection_closed_id: {
      UNUSED const sl_bt_evt_connection_closed_t *conn_evt =
        (sl_bt_evt_connection_closed_t*) &(evt->data);

      sl_zigbee_app_debug_println(
        "BLE connection closed, handle=0x%02x, reason=0x%02x",
        conn_evt->connection, conn_evt->reason);
    }
    break;

    case sl_bt_evt_scanner_legacy_advertisement_report_id: {
      sl_zigbee_app_debug_print("Scan response, address type=0x%02x",
                                evt->data.evt_scanner_legacy_advertisement_report.address_type);
      zb_ble_dmp_print_ble_address(evt->data.evt_scanner_legacy_advertisement_report.address.addr);
      sl_zigbee_app_debug_println("");
    }
    break;

    case sl_bt_evt_connection_parameters_id: {
      UNUSED const sl_bt_evt_connection_parameters_t* param_evt =
        (sl_bt_evt_connection_parameters_t*) &(evt->data);
      sl_zigbee_app_debug_println(
        "BLE connection parameters are updated, handle=0x%02x, interval=0x%02x, latency=0x%02x, timeout=0x%02x, security=0x%02x",
        param_evt->connection,
        param_evt->interval,
        param_evt->latency,
        param_evt->timeout,
        param_evt->security_mode);
    }
    break;

    case sl_bt_evt_gatt_service_id: {
      const sl_bt_evt_gatt_service_t* service_evt =
        (sl_bt_evt_gatt_service_t*) &(evt->data);
      sl_zigbee_app_debug_println(
        "GATT service, conn_handle=0x%02x, service_handle=0x%04x",
        service_evt->connection, service_evt->service);
      sl_zigbee_app_debug_print("UUID=[");
      for (uint8_t i = 0; i < service_evt->uuid.len; i++) {
        sl_zigbee_app_debug_print("0x%04x ", service_evt->uuid.data[i]);
      }
      sl_zigbee_app_debug_println("]");
    }
    break;

    default:
      break;
  }
}
#endif //SL_CATALOG_BLUETOOTH_PRESENT
