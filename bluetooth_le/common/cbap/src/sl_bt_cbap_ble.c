/***************************************************************************//**
 * @file
 * @brief Certificate Based Authentication and Pairing BLE implementation
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
#include <string.h>
#include "sl_status.h"
#include "sl_bt_api.h"
#include "gatt_db.h"
#include "app_assert.h"
#include "app_timer.h"
#include "sli_bt_cbap.h"
#include "sl_bt_cbap.h"

// -----------------------------------------------------------------------------
// Definitions

// Value of a GATT handle that has not been discovered yet.
#define HANDLE_NOT_INITIALIZED  0

// The number of characteristics carrying certificates.
#define CERT_CHAR_COUNT  ((uint8_t)(SL_BT_CBAP_CHAR_DEVICE_CERT_CLIENT + 1))

// Every CBAP fragment carries a one-byte continuation flag before its
// payload. A value of one means that another fragment follows; zero marks the
// final fragment. This keeps the individual GATT values below ATT's 512-byte
// attribute-value limit while allowing the full value to be larger.
#define GATT_FRAGMENT_HEADER_LEN  1U
#define GATT_FRAGMENT_MORE        1U
#define GATT_FRAGMENT_LAST        0U
#define ATT_ATTRIBUTE_VALUE_MAX_LENGTH  512U
// Opcode and attribute handle in front of the value of an ATT indication or
// Write Request.
#define ATT_VALUE_HEADER_LEN  3U
// Connection, characteristic, opcode, offset and byte-array length in a
// received GATT value event. Keep fragments within the BGAPI event buffer too.
#define GATT_VALUE_EVENT_HEADER_LEN  8U

// ATT error code that reports a request served successfully.
#define ATT_SUCCESS  0

// True if the pairing creates a bond, see SL_BT_CBAP_BONDING.
#define BONDING_ENABLED  (SL_BT_CBAP_BONDING != 0)

// The passkey is exchanged by CBAP, so the pairing is authenticated by the
// certificates. Bonding requests are confirmed only once CBAP has
// authenticated the remote device. Encryption requires bonding only if
// bonding is enabled, otherwise the pairing could not secure the link.
#define SM_CONFIGURATION_FLAGS                       \
  (SL_BT_SM_CONFIGURATION_SC_ONLY                    \
   | SL_BT_SM_CONFIGURATION_MITM_REQUIRED            \
   | SL_BT_SM_CONFIGURATION_BONDING_REQUEST_REQUIRED \
   | (BONDING_ENABLED ? SL_BT_SM_CONFIGURATION_BONDING_REQUIRED : 0))

// The peripheral device displays the passkey that it generates. The central
// device enters it, unless the peripheral device has a keyboard: then the
// central device displays the passkey for the duration of the procedure.
#define PERIPHERAL_IO_CAPABILITY  sl_bt_sm_io_capability_displayonly
#define CENTRAL_IO_CAPABILITY     sl_bt_sm_io_capability_keyboardonly

// Shorthand for the event data type of the user write requests.
typedef sl_bt_evt_gatt_server_user_write_request_t user_write_request_t;

// The certificates that the remote device presents about itself.
typedef enum {
  REMOTE_CERT_BATCH,
  REMOTE_CERT_DEVICE,
  REMOTE_CERT_COUNT
} remote_cert_t;

// Properties of a characteristic that carries a certificate.
typedef struct {
  // Handle of the characteristic in the local GATT database.
  uint16_t local_handle;
  // True if the GATT server presents its own certificate on the
  // characteristic, false if the GATT client sends its own certificate on it.
  bool server;
  // Storage ID of the local certificate presented on the characteristic.
  cbap_key_id_t local_id;
  // Storage ID the certificate received on the characteristic is written to.
  cbap_key_id_t remote_id;
  // The remote certificate carried by the characteristic, a remote_cert_t.
  uint8_t remote_cert;
} cert_char_t;

// Central device states.
typedef enum {
  CENTRAL_IDLE,
  CENTRAL_DISCOVER_SERVICES,
  CENTRAL_DISCOVER_CHARACTERISTICS,
  CENTRAL_EXCHANGE_CERTIFICATES,
  CENTRAL_CREATE_SESSION,
  CENTRAL_RECEIVE_PAIRING_PARAMETERS,
  CENTRAL_INCREASE_SECURITY,
  CENTRAL_DONE,
  CENTRAL_STATE_NUM
} central_state_t;

// GATT procedure completions during framed exchanges belong to the
// CCCD write that enables indications or to one outgoing write fragment. The
// indication stream itself is advanced by characteristic-status events.
typedef enum {
  CENTRAL_GATT_OPERATION_NONE,
  CENTRAL_GATT_OPERATION_ENABLE_INDICATIONS,
  CENTRAL_GATT_OPERATION_WRITE_FRAGMENT
} central_gatt_operation_t;

// -----------------------------------------------------------------------------
// Private variables

// State of the central device.
static central_state_t central_state = CENTRAL_IDLE;
// The characteristic that the central device discovers or transfers next.
static sl_bt_cbap_characteristics_t char_state =
  SL_BT_CBAP_CHAR_BATCH_CERT_SERVER;
// Flag indicating if the device is in the central role.
static bool is_central = false;

// UUID of the CBAP service.
static const uint8_t cbap_service_uuid[] = { SL_BT_CBAP_SERVICE_UUID };

// Handle of the CBAP service on the remote device.
static uint32_t cbap_service_handle = HANDLE_NOT_INITIALIZED;

// UUIDs of the CBAP characteristics (stored in flash)
static const uuid_128 cbap_char_uuids[SL_BT_CBAP_CHAR_COUNT] = {
  [SL_BT_CBAP_CHAR_BATCH_CERT_SERVER] = {
    .data = { SL_BT_CBAP_BATCH_CERT_SERVER_CHAR_UUID }
  },
  [SL_BT_CBAP_CHAR_BATCH_CERT_CLIENT] = {
    .data = { SL_BT_CBAP_BATCH_CERT_CLIENT_CHAR_UUID }
  },
  [SL_BT_CBAP_CHAR_DEVICE_CERT_SERVER] = {
    .data = { SL_BT_CBAP_DEVICE_CERT_SERVER_CHAR_UUID }
  },
  [SL_BT_CBAP_CHAR_DEVICE_CERT_CLIENT] = {
    .data = { SL_BT_CBAP_DEVICE_CERT_CLIENT_CHAR_UUID }
  },
  [SL_BT_CBAP_CHAR_SESSION_KEY_SERVER] = {
    .data = { SL_BT_CBAP_SESSION_KEY_SERVER_CHAR_UUID }
  },
  [SL_BT_CBAP_CHAR_SESSION_KEY_CLIENT] = {
    .data = { SL_BT_CBAP_SESSION_KEY_CLIENT_CHAR_UUID }
  },
  [SL_BT_CBAP_CHAR_PAIRING_PARAMETERS_SERVER] = {
    .data = { SL_BT_CBAP_PAIRING_PARAMETERS_SERVER_CHAR_UUID }
  }
};

// Handles of the CBAP characteristics on the remote device, discovered again
// for every candidate device.
static uint16_t cbap_char_handles[SL_BT_CBAP_CHAR_COUNT] = {
  HANDLE_NOT_INITIALIZED
};

// The certificate carried by each certificate characteristic. The server
// characteristics indicate the certificate to the client, while the client
// characteristics receive it through framed write requests.
static const cert_char_t cert_chars[CERT_CHAR_COUNT] = {
  [SL_BT_CBAP_CHAR_BATCH_CERT_SERVER] = {
    .local_handle = gattdb_batch_cert_server,
    .server = true,
    .local_id = SL_BT_CBAP_PSA_BATCH_CERT,
    .remote_id = SL_BT_CBAP_PSA_REMOTE_BATCH_CERT,
    .remote_cert = REMOTE_CERT_BATCH
  },
  [SL_BT_CBAP_CHAR_BATCH_CERT_CLIENT] = {
    .local_handle = gattdb_batch_cert_client,
    .server = false,
    .local_id = SL_BT_CBAP_PSA_BATCH_CERT,
    .remote_id = SL_BT_CBAP_PSA_REMOTE_BATCH_CERT,
    .remote_cert = REMOTE_CERT_BATCH
  },
  [SL_BT_CBAP_CHAR_DEVICE_CERT_SERVER] = {
    .local_handle = gattdb_device_cert_server,
    .server = true,
    .local_id = SL_BT_CBAP_PSA_DEVICE_CERT,
    .remote_id = SL_BT_CBAP_PSA_REMOTE_DEVICE_CERT,
    .remote_cert = REMOTE_CERT_DEVICE
  },
  [SL_BT_CBAP_CHAR_DEVICE_CERT_CLIENT] = {
    .local_handle = gattdb_device_cert_client,
    .server = false,
    .local_id = SL_BT_CBAP_PSA_DEVICE_CERT,
    .remote_id = SL_BT_CBAP_PSA_REMOTE_DEVICE_CERT,
    .remote_cert = REMOTE_CERT_DEVICE
  }
};

// Name of each remote certificate, for logging purposes.
static const char * const remote_cert_names[REMOTE_CERT_COUNT] = {
  [REMOTE_CERT_BATCH]  = "batch",
  [REMOTE_CERT_DEVICE] = "device"
};

// Buffer for GATT transfer in progress.
// A certificate rarely fits into a single ATT PDU, so it is assembled here
// before it is stored, and kept here while it is being sent in parts.
static uint8_t gatt_buf[SL_BT_CBAP_CERTIFICATE_MAX_SIZE];
// Number of valid bytes in gatt_buf
static size_t gatt_buf_len = 0;
// True while gatt_buf collects an incoming framed value. Incoming writes are
// rejected while the same buffer supplies an outgoing indication stream.
static bool gatt_receiving = false;

// Index of the characteristic gatt_buf belongs to
static uint8_t gatt_char = SL_BT_CBAP_CHAR_COUNT;
// Outgoing framed transfer state. Indications are sent one at a time and each
// write request is completed before the next fragment is sent, so a shared
// fragment buffer is sufficient for either transport direction.
static uint8_t gatt_fragment[SL_BT_CBAP_CERTIFICATE_MAX_SIZE
                             + GATT_FRAGMENT_HEADER_LEN];
static size_t gatt_send_offset = 0;
static bool gatt_sending = false;

// The GATT operation the central has in flight during a framed exchange.
static central_gatt_operation_t central_gatt_operation
  = CENTRAL_GATT_OPERATION_NONE;

// Flags of the remote certificates that have been received and stored.
static bool remote_cert_stored[REMOTE_CERT_COUNT] = { false };

// True once the certificates of the remote device have been built into a chain
// that is anchored to the trust hierarchy of this device. It says that the
// certificates are genuine, not that the remote device owns them.
static bool remote_chain_valid = false;

// Retain our signed key while gatt_buf assembles the peer's key.
static uint8_t local_session_data[SLI_BT_CBAP_SESSION_DATA_LEN];
static size_t local_session_len = 0;
static bool local_session_sent = false;
static bool session_ready = false;

// The server generates one passkey and one CCM ciphertext per fresh session.
static uint32_t pairing_passkey = 0;
static bool passkey_ready = false;
static bool pairing_parameters_transferred = false;
// True if the local device displays the passkey in passkey entry pairing,
// false if it enters the passkey.
static bool displays_passkey = false;
// Fresh pairings must consume the exchanged passkey before they can succeed.
static bool passkey_used = false;

// A stored bond skips CBAP, but its key must still secure the current link.
static bool bonded_reconnect = false;
static uint8_t candidate_bonding = SL_BT_INVALID_BONDING_HANDLE;
static bool link_secured = false;
// True once the stack reported the pairing complete, and stored the bond if
// bonding is enabled.
static bool pairing_complete = false;

// The connection handle and the Bluetooth address of the remote device we have
// CBAP in progress with.
static sl_bt_cbap_conn_t candidate_device = {
  .handle = SL_BT_INVALID_CONNECTION_HANDLE,
  .address = { .addr = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff } }
};

// True while the connection to the candidate device is open. The error handler
// closes the connection of a device that failed the procedure, but the
// procedure can just as well fail because the connection is already gone.
static bool candidate_connected = false;

// Function that the result of every procedure is reported to.
static sl_bt_cbap_cb_t callback = NULL;

// Timer that limits the duration of the procedure.
static app_timer_t timer;

// -----------------------------------------------------------------------------
// Private function declarations

static sl_status_t discover_service(void);
static sl_status_t discover_characteristic(void);
static sl_status_t continue_cert_exchange(void);
static sl_status_t prepare_data_fragment(size_t *fragment_length);
static sl_status_t send_indication_fragment(uint8_t index);
static sl_status_t send_write_fragment(uint8_t index);
static uint8_t find_cert_char(uint16_t local_handle, bool server);
static sl_status_t collect_part(uint8_t *buf,
                                size_t *buf_len,
                                size_t capacity,
                                uint16_t offset,
                                const uint8_t *data,
                                size_t len);
static sl_status_t collect_fragment(const uint8_t *data,
                                    size_t len,
                                    size_t capacity,
                                    bool *complete);
static sl_status_t store_remote_cert(uint8_t index);
static sl_status_t check_remote_certs(bool report);
static sl_status_t validate_remote_cert_chain(void);
static sl_status_t ensure_local_session(void);
static sl_status_t continue_session(void);
static sl_status_t prepare_pairing_parameters(void);
static sl_status_t process_pairing_parameters(void);
static sl_status_t adapt_io_capability(uint8_t peer_io_capability);
static bool is_procedure_complete(void);
static void receive_indication(
  const sl_bt_evt_gatt_characteristic_value_t *value);
static void receive_data_write(const user_write_request_t *req, uint8_t index);
static void refuse_write(const user_write_request_t *req, uint8_t att_error);
static uint8_t resolve_write_target(uint16_t local_handle);
static void receive_user_write(const user_write_request_t *req);
static void handle_indication_status(
  const sl_bt_evt_gatt_server_characteristic_status_t *status);
static void reset_procedure(void);
static void complete_connection(void);
static void forget_stale_bond(void);
static void send_security_request(void);
static void stop_procedure_timer(void);
static void on_timer_expired(app_timer_t *handle, void *data);
static void on_error(sl_status_t sc);

// -----------------------------------------------------------------------------
// Public function definitions

/******************************************************************************
 * Initialize the CBAP component.
 *****************************************************************************/
sl_status_t sl_bt_cbap_init(sl_bt_cbap_cb_t cb, bool central)
{
  // Register callback.
  if (cb == NULL) {
    return SL_STATUS_NULL_POINTER;
  }
  if (callback != NULL) {
    return SL_STATUS_ALREADY_INITIALIZED;
  }
  callback = cb;

  // Set the role of the device.
  is_central = central;
  return SL_STATUS_OK;
}

/******************************************************************************
 * Check whether the component is serving a connection.
 *****************************************************************************/
bool sl_bt_cbap_is_procedure_in_progress(void)
{
  return candidate_device.handle != SL_BT_INVALID_CONNECTION_HANDLE;
}

// -----------------------------------------------------------------------------
// Internal function definitions

/******************************************************************************
 * Handle the Bluetooth events of the CBAP procedure.
 *****************************************************************************/
void sli_bt_cbap_on_event(sl_bt_msg_t *evt)
{
  // Check if the component was initialized.
  if (callback == NULL) {
    return;
  }

  sl_status_t sc;
  switch (SL_BT_MSG_ID(evt->header)) {
    // -------------------------------
    // This event indicates the device has started and the radio is ready.
    case sl_bt_evt_system_boot_id:
      sc = sl_bt_sm_configure(SM_CONFIGURATION_FLAGS,
                              is_central ? CENTRAL_IO_CAPABILITY
                              : PERIPHERAL_IO_CAPABILITY);
      if (sc != SL_STATUS_OK) {
        SLI_BT_CBAP_LOG_ERROR("Failed to configure sm: [0x%04lx]"
                              SLI_BT_CBAP_LOG_NL,
                              sc);
        app_assert_status(sc);
      }
      sc = sl_bt_sm_set_bondable_mode(BONDING_ENABLED ? 1 : 0);
      app_assert_status(sc);
      if (!BONDING_ENABLED) {
        // A bond stored while bonding was enabled would still let its peer
        // device skip the procedure.
        sc = sl_bt_sm_delete_bondings();
        app_assert_status(sc);
      }
      break;

    // -------------------------------
    // This event indicates that a new connection was opened.
    case sl_bt_evt_connection_opened_id:
      // Check if there is a CBAP procedure in progress already.
      if (candidate_device.handle != SL_BT_INVALID_CONNECTION_HANDLE) {
        SLI_BT_CBAP_LOG_ERROR("There cannot be multiple procedures in "
                              "progress at the same time. Closing new "
                              "connection." SLI_BT_CBAP_LOG_NL);
        (void)sl_bt_connection_close(
          evt->data.evt_connection_opened.connection);
        break;
      }

      // CBAP procedure can be started. Start timeout.
      sc = app_timer_start(&timer,
                           SL_BT_CBAP_PROCEDURE_TIMEOUT_MS,
                           on_timer_expired,
                           (void *)NULL, // Callback has no parameters
                           false);
      app_assert_status(sc);

      // Store candidate connection.
      candidate_device.handle = evt->data.evt_connection_opened.connection;
      candidate_device.address = evt->data.evt_connection_opened.address;
      candidate_connected = true;

      // Discard the handles discovered on the previous remote device.
      reset_procedure();

      candidate_bonding = evt->data.evt_connection_opened.bonding;
      bonded_reconnect = (candidate_bonding != SL_BT_INVALID_BONDING_HANDLE);
      if (bonded_reconnect) {
        SLI_BT_CBAP_LOG_INFO("Restoring bonded connection %d."
                             SLI_BT_CBAP_LOG_NL, candidate_device.handle);
        // The central restores encryption with the stored key, and the
        // peripheral asks for it. Neither device starts a new certificate
        // exchange.
        central_state = CENTRAL_INCREASE_SECURITY;
        if (is_central) {
          sc = sl_bt_sm_increase_security(candidate_device.handle);
          if (sc != SL_STATUS_OK) {
            on_error(sc);
          }
        } else {
          send_security_request();
        }
        break;
      }

      // If in central role, invoke CBAP procedure.
      if (is_central) {
        // Continue with GATT discovery.
        sc = discover_service();
        if (sc != SL_STATUS_OK) {
          on_error(sc);
        }
      }
      break;

    // -------------------------------
    // This event indicates that a connection was closed.
    case sl_bt_evt_connection_closed_id:
      if (evt->data.evt_connection_closed.connection
          == candidate_device.handle) {
        SLI_BT_CBAP_LOG_ERROR("The connection was closed in mid-procedure! "
                              "Reason: 0x%04x" SLI_BT_CBAP_LOG_NL,
                              evt->data.evt_connection_closed.reason);

        // The error handler reports the candidate device to the application,
        // therefore it clears the connection parameters, not this handler.
        candidate_connected = false;

        // A peer device that has lost the bond may close the connection
        // instead of rejecting the stored key.
        if (bonded_reconnect
            && (evt->data.evt_connection_closed.reason
                == SL_STATUS_BT_CTRL_PIN_OR_KEY_MISSING)) {
          forget_stale_bond();
        } else {
          on_error(evt->data.evt_connection_closed.reason);
        }
      }
      break;

    // -------------------------------
    // This event is generated when a new service is discovered.
    case sl_bt_evt_gatt_service_id:
      if ((evt->data.evt_gatt_service.connection == candidate_device.handle)
          && is_central
          && (cbap_service_handle == HANDLE_NOT_INITIALIZED)) {
        // Save the handle of the first matching service for future reference.
        cbap_service_handle = evt->data.evt_gatt_service.service;
        SLI_BT_CBAP_LOG_DEBUG("CBAP service found. Handle: %lu"
                              SLI_BT_CBAP_LOG_NL,
                              cbap_service_handle);
      }
      break;

    // -------------------------------
    // This event is generated when a new characteristic is discovered.
    case sl_bt_evt_gatt_characteristic_id:
      if ((evt->data.evt_gatt_characteristic.connection
           == candidate_device.handle)
          && is_central
          && (central_state == CENTRAL_DISCOVER_CHARACTERISTICS)
          && (char_state < SL_BT_CBAP_CHAR_COUNT)
          && (cbap_char_handles[char_state] == HANDLE_NOT_INITIALIZED)) {
        // Save the handle of the characteristic being discovered.
        cbap_char_handles[char_state] =
          evt->data.evt_gatt_characteristic.characteristic;
        SLI_BT_CBAP_LOG_DEBUG("Characteristic %d found. Handle: %d"
                              SLI_BT_CBAP_LOG_NL,
                              char_state,
                              cbap_char_handles[char_state]);
      }
      break;

    // -------------------------------
    // This event is generated for various procedure completions, e.g. when a
    // write procedure is completed, or service discovery is completed.
    case sl_bt_evt_gatt_procedure_completed_id:
      if (bonded_reconnect) {
        break;
      }
      if (evt->data.evt_gatt_procedure_completed.connection
          == candidate_device.handle) {
        // Check result
        if (evt->data.evt_gatt_procedure_completed.result != 0) {
          SLI_BT_CBAP_LOG_ERROR(
            "GATT procedure completed error. Connection: %d. "
            "Error: 0x%04x." SLI_BT_CBAP_LOG_NL,
            evt->data.evt_gatt_procedure_completed.connection,
            evt->data.evt_gatt_procedure_completed.result);
          on_error(evt->data.evt_gatt_procedure_completed.result);
          break;
        }

        if (is_central) {
          switch (central_state) {
            case CENTRAL_DISCOVER_SERVICES:
              // A discovery that found nothing also completes successfully,
              // so the handle has to be checked here.
              if (cbap_service_handle == HANDLE_NOT_INITIALIZED) {
                SLI_BT_CBAP_LOG_ERROR("CBAP service not found on the remote "
                                      "device." SLI_BT_CBAP_LOG_NL);
                on_error(SL_STATUS_NOT_FOUND);
                break;
              }

              // Continue by finding the characteristics of the CBAP service,
              // starting with the first one.
              char_state = (sl_bt_cbap_characteristics_t)0;
              sc = discover_characteristic();
              if (sc != SL_STATUS_OK) {
                on_error(sc);
              }
              break;

            case CENTRAL_DISCOVER_CHARACTERISTICS:
              if (cbap_char_handles[char_state] == HANDLE_NOT_INITIALIZED) {
                SLI_BT_CBAP_LOG_ERROR("Characteristic %d not found on the "
                                      "remote device." SLI_BT_CBAP_LOG_NL,
                                      char_state);
                on_error(SL_STATUS_NOT_FOUND);
                break;
              }

              char_state++;
              if (char_state < SL_BT_CBAP_CHAR_COUNT) {
                // Continue with the next characteristic.
                sc = discover_characteristic();
                if (sc != SL_STATUS_OK) {
                  on_error(sc);
                }
              } else {
                SLI_BT_CBAP_LOG_DEBUG("GATT discovery complete."
                                      SLI_BT_CBAP_LOG_NL);

                // Continue with the certificate exchange, starting with the
                // first certificate characteristic.
                char_state = (sl_bt_cbap_characteristics_t)0;
                central_state = CENTRAL_EXCHANGE_CERTIFICATES;
                sc = continue_cert_exchange();
                if (sc != SL_STATUS_OK) {
                  on_error(sc);
                }
              }
              break;

            case CENTRAL_EXCHANGE_CERTIFICATES:
            case CENTRAL_CREATE_SESSION:
              if (central_gatt_operation
                  == CENTRAL_GATT_OPERATION_ENABLE_INDICATIONS) {
                central_gatt_operation = CENTRAL_GATT_OPERATION_NONE;
                break;
              }
              if (central_gatt_operation
                  != CENTRAL_GATT_OPERATION_WRITE_FRAGMENT) {
                on_error(SL_STATUS_INVALID_STATE);
                break;
              }
              if (gatt_send_offset < gatt_buf_len) {
                sc = send_write_fragment((uint8_t)char_state);
              } else {
                if (central_state == CENTRAL_CREATE_SESSION) {
                  local_session_sent = true;
                }
                gatt_sending = false;
                gatt_buf_len = 0;
                gatt_char = SL_BT_CBAP_CHAR_COUNT;
                central_gatt_operation = CENTRAL_GATT_OPERATION_NONE;
                char_state++;
                sc = (central_state == CENTRAL_EXCHANGE_CERTIFICATES)
                     ? continue_cert_exchange() : continue_session();
              }
              if (sc != SL_STATUS_OK) {
                on_error(sc);
              }
              break;

            case CENTRAL_RECEIVE_PAIRING_PARAMETERS:
              if (central_gatt_operation
                  == CENTRAL_GATT_OPERATION_ENABLE_INDICATIONS) {
                central_gatt_operation = CENTRAL_GATT_OPERATION_NONE;
              }
              break;

            default:
              break;
          }
        }
      }
      break;

    // -------------------------------
    // This event is generated when a characteristic value was received from
    // the remote GATT server.
    case sl_bt_evt_gatt_characteristic_value_id:
      if ((evt->data.evt_gatt_characteristic_value.connection
           != candidate_device.handle)
          || !is_central
          || bonded_reconnect) {
        break;
      }

      receive_indication(&evt->data.evt_gatt_characteristic_value);
      break;

    // -------------------------------
    // This event indicates that the remote GATT client is writing a
    // characteristic handled by the application.
    case sl_bt_evt_gatt_server_user_write_request_id:
      receive_user_write(&evt->data.evt_gatt_server_user_write_request);
      break;

    // -------------------------------
    // This event reports a CBAP indication subscription or its
    // confirmation. The peripheral advances one indication at a time.
    case sl_bt_evt_gatt_server_characteristic_status_id:
      handle_indication_status(
        &evt->data.evt_gatt_server_characteristic_status);
      break;

    // -------------------------------
    // This event requests the passkey on the input side of passkey pairing,
    // which is the central device, unless the peripheral device has a
    // keyboard. Enter the passkey exchanged by CBAP.
    case sl_bt_evt_sm_passkey_request_id:
      if (evt->data.evt_sm_passkey_request.connection
          == candidate_device.handle) {
        if (bonded_reconnect || displays_passkey || !is_procedure_complete()) {
          on_error(SL_STATUS_INVALID_STATE);
          break;
        }
        sc = sl_bt_sm_enter_passkey(candidate_device.handle,
                                    (int32_t)pairing_passkey);
        if (sc != SL_STATUS_OK) {
          on_error(sc);
        } else {
          passkey_used = true;
          stop_procedure_timer();
        }
      }
      break;

    // -------------------------------
    // This event reports the passkey on the display side of passkey pairing,
    // which is the peripheral device, unless it has a keyboard. It has to be
    // the passkey that CBAP installed.
    case sl_bt_evt_sm_passkey_display_id:
      if (evt->data.evt_sm_passkey_display.connection
          == candidate_device.handle) {
        if (bonded_reconnect || !displays_passkey || !is_procedure_complete()
            || (evt->data.evt_sm_passkey_display.passkey != pairing_passkey)) {
          on_error(SL_STATUS_INVALID_CREDENTIALS);
        } else {
          passkey_used = true;
          stop_procedure_timer();
        }
      }
      break;

    // -------------------------------
    // This event indicates that the remote device requests bonding. Every
    // request has to be confirmed, which is how a remote device that has not
    // been authenticated yet is kept from pairing. A pairing that creates no
    // bond is also refused by the passkey and security level checks.
    case sl_bt_evt_sm_confirm_bonding_id: {
      uint8_t connection = evt->data.evt_sm_confirm_bonding.connection;
      bool accept = (connection == candidate_device.handle)
                    && !bonded_reconnect
                    && (evt->data.evt_sm_confirm_bonding.bonding_handle
                        == SL_BT_INVALID_BONDING_HANDLE)
                    && is_procedure_complete();

      if (!accept) {
        SLI_BT_CBAP_LOG_ERROR("Rejecting the bonding request of connection "
                              "%d, CBAP has not authenticated it."
                              SLI_BT_CBAP_LOG_NL,
                              connection);
      }

      sc = sl_bt_sm_bonding_confirm(connection, accept ? 1 : 0);
      if (sc != SL_STATUS_OK) {
        SLI_BT_CBAP_LOG_ERROR("Failed to answer the bonding request: "
                              "0x%04lx!" SLI_BT_CBAP_LOG_NL, sc);
      }

      if (!accept && (connection == candidate_device.handle)) {
        if (bonded_reconnect) {
          // A bonded peer device that asks for a new pairing has lost the
          // bond. A new bond requires the CBAP procedure.
          forget_stale_bond();
        } else {
          // A remote device that tries to pair too early is not trustworthy.
          on_error(SL_STATUS_INVALID_STATE);
        }
      }
      break;
    }

    // -------------------------------
    // This event indicates that the pairing completed, and that the bond was
    // stored if it has a bonding handle. A secured link alone does not
    // guarantee that its key was persisted.
    case sl_bt_evt_sm_bonded_id: {
      if (evt->data.evt_sm_bonded.connection != candidate_device.handle) {
        break;
      }
      // With bonding enabled the pairing has to create a bond, and without it
      // the pairing must not create one.
      bool bonded = (evt->data.evt_sm_bonded.bonding
                     != SL_BT_INVALID_BONDING_HANDLE);
      if (bonded_reconnect || !is_procedure_complete() || !passkey_used
          || (bonded != BONDING_ENABLED)
          || (evt->data.evt_sm_bonded.security_mode
              != sl_bt_connection_mode1_level4)) {
        on_error(SL_STATUS_INVALID_CREDENTIALS);
        break;
      }
      candidate_bonding = evt->data.evt_sm_bonded.bonding;
      pairing_complete = true;
      complete_connection();
      break;
    }

    // -------------------------------
    // This event indicates that authenticated passkey pairing failed.
    case sl_bt_evt_sm_bonding_failed_id:
      if (evt->data.evt_sm_bonding_failed.connection
          == candidate_device.handle) {
        SLI_BT_CBAP_LOG_ERROR(
          "Pairing failed: 0x%04lx!" SLI_BT_CBAP_LOG_NL,
          (unsigned long)evt->data.evt_sm_bonding_failed.reason);

        // A bonded peer device that rejects the stored key has lost the bond.
        if (bonded_reconnect
            && (evt->data.evt_sm_bonding_failed.reason
                == SL_STATUS_BT_CTRL_PIN_OR_KEY_MISSING)) {
          forget_stale_bond();
        } else {
          on_error((sl_status_t)evt->data.evt_sm_bonding_failed.reason);
        }
      }
      break;

    // -------------------------------
    // This event is generated when the connection parameters or the security
    // mode of the connection change.
    case sl_bt_evt_connection_parameters_id:
      if (evt->data.evt_connection_parameters.connection
          != candidate_device.handle) {
        break;
      }

      SLI_BT_CBAP_LOG_DEBUG("Security mode: %d" SLI_BT_CBAP_LOG_NL,
                            evt->data.evt_connection_parameters.security_mode);

      if (evt->data.evt_connection_parameters.security_mode
          == sl_bt_connection_mode1_level1) {
        // The link is not secured, so this is a plain parameter update.
        link_secured = false;
        break;
      }

      // Securing the link is the last step of the procedure. If it happens
      // before the local role is done, the remote device was not authenticated,
      // so the connection cannot be trusted.
      if (!bonded_reconnect && (!is_procedure_complete() || !passkey_used)) {
        SLI_BT_CBAP_LOG_ERROR("The security level was raised before CBAP "
                              "could authenticate the remote device!"
                              SLI_BT_CBAP_LOG_NL);
        on_error(SL_STATUS_INVALID_STATE);
        break;
      }

      if (evt->data.evt_connection_parameters.security_mode
          != sl_bt_connection_mode1_level4) {
        // Anything below Secure Connections pairing with an authenticated key
        // means that authenticated passkey pairing did not complete. The stack
        // is configured to rule this out, this is the second line of defence.
        SLI_BT_CBAP_LOG_ERROR(
          "The link was secured in mode %d instead of mode 1 level 4, the "
          "keys are not authenticated by CBAP!" SLI_BT_CBAP_LOG_NL,
          evt->data.evt_connection_parameters.security_mode);
        on_error(SL_STATUS_INVALID_CREDENTIALS);
        break;
      }

      link_secured = true;
      complete_connection();
      break;

    default:
      break;
  }
}

// -----------------------------------------------------------------------------
// Private function definitions

/******************************************************************************
 * Start the discovery of the CBAP service on the remote device.
 *****************************************************************************/
static sl_status_t discover_service(void)
{
  sl_status_t sc;

  sc = sl_bt_gatt_discover_primary_services_by_uuid(candidate_device.handle,
                                                    sizeof(cbap_service_uuid),
                                                    cbap_service_uuid);
  if (sc != SL_STATUS_OK) {
    return sc;
  }

  central_state = CENTRAL_DISCOVER_SERVICES;
  SLI_BT_CBAP_LOG_DEBUG("Discovering CBAP service." SLI_BT_CBAP_LOG_NL);

  return sc;
}

/******************************************************************************
 * Start the discovery of the characteristic selected by char_state.
 *****************************************************************************/
static sl_status_t discover_characteristic(void)
{
  sl_status_t sc;

  sc = sl_bt_gatt_discover_characteristics_by_uuid(
    candidate_device.handle,
    cbap_service_handle,
    sizeof(cbap_char_uuids[char_state].data),
    cbap_char_uuids[char_state].data);
  if (sc != SL_STATUS_OK) {
    return sc;
  }

  central_state = CENTRAL_DISCOVER_CHARACTERISTICS;
  SLI_BT_CBAP_LOG_DEBUG("Discovering characteristic %d." SLI_BT_CBAP_LOG_NL,
                        char_state);

  return sc;
}

/******************************************************************************
 * Continue the certificate exchange on the characteristic selected by
 * char_state, or finish it if there is none left.
 *
 * The central drives the exchange by walking the certificate characteristics.
 * It enables indications for the certificate of the peripheral from a server
 * characteristic if it has to verify it, and writes its own certificate to a
 * client characteristic if it has one to prove itself with. The characteristics
 * that are left for the peripheral to serve are skipped.
 *****************************************************************************/
static sl_status_t continue_cert_exchange(void)
{
  sl_status_t sc;

  while (char_state < CERT_CHAR_COUNT) {
    const cert_char_t *desc = &cert_chars[char_state];

    if (desc->server && SLI_BT_CBAP_ROLE_IS_VERIFIER) {
      // The peripheral/server presents its certificate through an indication
      // stream. Each indication is framed below the ATT value limit.
      gatt_buf_len = 0;
      gatt_char = char_state;
      gatt_receiving = true;
      central_gatt_operation = CENTRAL_GATT_OPERATION_ENABLE_INDICATIONS;
      sc = sl_bt_gatt_set_characteristic_notification(
        candidate_device.handle,
        cbap_char_handles[char_state],
        sl_bt_gatt_indication);
      if (sc == SL_STATUS_OK) {
        SLI_BT_CBAP_LOG_DEBUG("Requesting the %s certificate of the remote "
                              "device." SLI_BT_CBAP_LOG_NL,
                              remote_cert_names[desc->remote_cert]);
      }
      return sc;
    }

    if (!desc->server && SLI_BT_CBAP_ROLE_IS_PROVER) {
      // Send our own (central/client) certificate to the peripheral.
      sc = sli_bt_cbap_get_certificate(desc->local_id,
                                       gatt_buf,
                                       &gatt_buf_len,
                                       sizeof(gatt_buf));
      if (sc != SL_STATUS_OK) {
        SLI_BT_CBAP_LOG_ERROR("Failed to load the local %s certificate: "
                              "0x%04lx!" SLI_BT_CBAP_LOG_NL,
                              remote_cert_names[desc->remote_cert],
                              sc);
        return sc;
      }

      gatt_char = char_state;
      gatt_send_offset = 0;
      gatt_sending = true;
      central_gatt_operation = CENTRAL_GATT_OPERATION_WRITE_FRAGMENT;
      sc = send_write_fragment(char_state);
      if (sc == SL_STATUS_OK) {
        SLI_BT_CBAP_LOG_DEBUG("Sending the local %s certificate "
                              "(%u bytes)." SLI_BT_CBAP_LOG_NL,
                              remote_cert_names[desc->remote_cert],
                              (unsigned int)gatt_buf_len);
      }
      return sc;
    }

    // The local role takes no part in the transfer on this characteristic.
    char_state++;
  }

  // Every certificate characteristic has been walked.
  sc = check_remote_certs(true);
  if (sc != SL_STATUS_OK) {
    return sc;
  }
  SLI_BT_CBAP_LOG_INFO("Certificate exchange complete." SLI_BT_CBAP_LOG_NL);

  // The certificates have to lead back to the trust hierarchy of this device
  // before the procedure continues. The caller closes the connection if they do
  // not. The signed ephemeral key subsequently proves ownership.
  sc = validate_remote_cert_chain();
  if (sc != SL_STATUS_OK) {
    return sc;
  }

  // Continue with the signed session key exchange.
  central_state = CENTRAL_CREATE_SESSION;
  return continue_session();
}

/******************************************************************************
 * Build the next framed CBAP fragment in gatt_fragment.
 *
 * An ATT indication or Write Request carries at most ATT_MTU - 3 bytes in its
 * value. One byte is reserved for the CBAP continuation flag.
 *
 * @param[out] fragment_length Length of the fragment, including its flag.
 *
 * @return SL_STATUS_OK on success, SL_STATUS_INVALID_STATE if there is nothing
 *         left to send, error code otherwise.
 *****************************************************************************/
static sl_status_t prepare_data_fragment(size_t *fragment_length)
{
  sl_status_t sc;
  uint16_t mtu;
  size_t payload_length;
  size_t maximum_payload;
  size_t remaining;

  if (!gatt_sending || (gatt_send_offset >= gatt_buf_len)) {
    return SL_STATUS_INVALID_STATE;
  }

  sc = sl_bt_gatt_server_get_mtu(candidate_device.handle, &mtu);
  if (sc != SL_STATUS_OK) {
    return sc;
  }
  if (mtu <= (ATT_VALUE_HEADER_LEN + GATT_FRAGMENT_HEADER_LEN)) {
    return SL_STATUS_INVALID_PARAMETER;
  }

  maximum_payload = ((size_t)mtu
                     - ATT_VALUE_HEADER_LEN
                     - GATT_FRAGMENT_HEADER_LEN);
  if (maximum_payload > (ATT_ATTRIBUTE_VALUE_MAX_LENGTH
                         - GATT_FRAGMENT_HEADER_LEN)) {
    maximum_payload = (ATT_ATTRIBUTE_VALUE_MAX_LENGTH
                       - GATT_FRAGMENT_HEADER_LEN);
  }
  if (maximum_payload > (SL_BGAPI_MAX_PAYLOAD_SIZE
                         - GATT_VALUE_EVENT_HEADER_LEN
                         - GATT_FRAGMENT_HEADER_LEN)) {
    maximum_payload = (SL_BGAPI_MAX_PAYLOAD_SIZE
                       - GATT_VALUE_EVENT_HEADER_LEN
                       - GATT_FRAGMENT_HEADER_LEN);
  }
  if (maximum_payload > (sizeof(gatt_fragment) - GATT_FRAGMENT_HEADER_LEN)) {
    maximum_payload = (sizeof(gatt_fragment) - GATT_FRAGMENT_HEADER_LEN);
  }

  remaining = gatt_buf_len - gatt_send_offset;
  payload_length = (remaining < maximum_payload) ? remaining : maximum_payload;
  gatt_fragment[0] = (remaining > payload_length)
                     ? GATT_FRAGMENT_MORE : GATT_FRAGMENT_LAST;
  memcpy(&gatt_fragment[GATT_FRAGMENT_HEADER_LEN],
         &gatt_buf[gatt_send_offset],
         payload_length);
  gatt_send_offset += payload_length;
  *fragment_length = payload_length + GATT_FRAGMENT_HEADER_LEN;

  return SL_STATUS_OK;
}

/******************************************************************************
 * Indicate the next CBAP fragment from the local GATT server.
 *
 * @param[in] index The server characteristic the fragment is indicated on.
 *
 * @return SL_STATUS_OK on success, error code otherwise.
 *****************************************************************************/
static sl_status_t send_indication_fragment(uint8_t index)
{
  sl_status_t sc;
  size_t fragment_length;

  if ((index >= SL_BT_CBAP_CHAR_COUNT) || (gatt_char != index)
      || ((index < CERT_CHAR_COUNT) && !cert_chars[index].server)
      || ((index >= CERT_CHAR_COUNT)
          && (index != SL_BT_CBAP_CHAR_SESSION_KEY_SERVER)
          && (index != SL_BT_CBAP_CHAR_PAIRING_PARAMETERS_SERVER))) {
    return SL_STATUS_INVALID_STATE;
  }

  sc = prepare_data_fragment(&fragment_length);
  if (sc != SL_STATUS_OK) {
    return sc;
  }

  uint16_t handle = (index < CERT_CHAR_COUNT)
                    ? cert_chars[index].local_handle
                    : ((index == SL_BT_CBAP_CHAR_SESSION_KEY_SERVER)
                       ? gattdb_session_key_server
                       : gattdb_pairing_parameters_server);
  sc = sl_bt_gatt_server_send_indication(candidate_device.handle,
                                         handle,
                                         fragment_length,
                                         gatt_fragment);
  if (sc == SL_STATUS_OK) {
    SLI_BT_CBAP_LOG_DEBUG(
      "Sent %u bytes of local CBAP data." SLI_BT_CBAP_LOG_NL,
      (unsigned int)(fragment_length - GATT_FRAGMENT_HEADER_LEN));
  }

  return sc;
}

/******************************************************************************
 * Write the next CBAP fragment from the central GATT client.
 *
 * @param[in] index The client characteristic the fragment is written to.
 *
 * @return SL_STATUS_OK on success, error code otherwise.
 *****************************************************************************/
static sl_status_t send_write_fragment(uint8_t index)
{
  sl_status_t sc;
  size_t fragment_length;

  if ((index >= SL_BT_CBAP_CHAR_COUNT) || (gatt_char != index)
      || ((index < CERT_CHAR_COUNT) && cert_chars[index].server)
      || ((index >= CERT_CHAR_COUNT)
          && (index != SL_BT_CBAP_CHAR_SESSION_KEY_CLIENT))) {
    return SL_STATUS_INVALID_STATE;
  }

  sc = prepare_data_fragment(&fragment_length);
  if (sc != SL_STATUS_OK) {
    return sc;
  }

  sc = sl_bt_gatt_write_characteristic_value(candidate_device.handle,
                                             cbap_char_handles[index],
                                             fragment_length,
                                             gatt_fragment);
  if (sc == SL_STATUS_OK) {
    SLI_BT_CBAP_LOG_DEBUG(
      "Sent %u bytes of local CBAP data." SLI_BT_CBAP_LOG_NL,
      (unsigned int)(fragment_length - GATT_FRAGMENT_HEADER_LEN));
  }

  return sc;
}

/******************************************************************************
 * Find a certificate characteristic by its local handle and direction.
 *
 * @param[in] local_handle Handle of the characteristic in the local GATT
 *                         database.
 * @param[in] server True to look for a server characteristic, false to look
 *                   for a client characteristic.
 *
 * @return Index of the characteristic, SL_BT_CBAP_CHAR_COUNT if there is no
 *         such certificate characteristic.
 *****************************************************************************/
static uint8_t find_cert_char(uint16_t local_handle, bool server)
{
  for (uint8_t index = 0; index < CERT_CHAR_COUNT; index++) {
    if ((cert_chars[index].local_handle == local_handle)
        && (cert_chars[index].server == server)) {
      return index;
    }
  }
  return (uint8_t)SL_BT_CBAP_CHAR_COUNT;
}

/******************************************************************************
 * Collect a part of a value being received into the given buffer.
 *
 * Used by all framed CBAP transfers.
 *
 * @param[out] buf Buffer collecting the parts.
 * @param[in,out] buf_len Number of bytes already collected, advanced by the
 *   length of the part on success.
 * @param[in] capacity Size of @p buf in bytes.
 * @param[in] offset Offset the part was received at.
 * @param[in] data The part received.
 * @param[in] len Length of @p data.
 *
 * @return SL_STATUS_OK on success, SL_STATUS_INVALID_STATE if the part is out
 *         of order, SL_STATUS_WOULD_OVERFLOW if it does not fit into @p buf.
 *****************************************************************************/
static sl_status_t collect_part(uint8_t *buf,
                                size_t *buf_len,
                                size_t capacity,
                                uint16_t offset,
                                const uint8_t *data,
                                size_t len)
{
  // The parts are expected in order, without gaps between them.
  if (offset != *buf_len) {
    SLI_BT_CBAP_LOG_ERROR("A part arrived at offset %u instead of %u."
                          SLI_BT_CBAP_LOG_NL,
                          (unsigned int)offset,
                          (unsigned int)*buf_len);
    return SL_STATUS_INVALID_STATE;
  }

  if (((*buf_len) > capacity) || (len > (capacity - (*buf_len)))) {
    SLI_BT_CBAP_LOG_ERROR("The value of the remote device is longer than "
                          "%u bytes." SLI_BT_CBAP_LOG_NL,
                          (unsigned int)capacity);
    return SL_STATUS_WOULD_OVERFLOW;
  }

  memcpy(&buf[*buf_len], data, len);
  *buf_len += len;

  return SL_STATUS_OK;
}

/******************************************************************************
 * Write the certificate collected in gatt_buf to the persistent storage.
 *
 * @param[in] index The certificate characteristic the certificate arrived on.
 *
 * @return SL_STATUS_OK on success, SL_STATUS_EMPTY if nothing was collected,
 *         error code otherwise.
 *****************************************************************************/
static sl_status_t store_remote_cert(uint8_t index)
{
  sl_status_t sc;
  remote_cert_t remote_cert = cert_chars[index].remote_cert;

  if (gatt_buf_len == 0) {
    SLI_BT_CBAP_LOG_ERROR("The remote device presented an empty %s "
                          "certificate." SLI_BT_CBAP_LOG_NL,
                          remote_cert_names[remote_cert]);
    return SL_STATUS_EMPTY;
  }

  sc = sli_bt_cbap_set_certificate(cert_chars[index].remote_id,
                                   gatt_buf,
                                   gatt_buf_len);
  if (sc != SL_STATUS_OK) {
    return sc;
  }

  remote_cert_stored[remote_cert] = true;
  SLI_BT_CBAP_LOG_DEBUG("The %s certificate of the remote device is stored "
                        "(%u bytes)." SLI_BT_CBAP_LOG_NL,
                        remote_cert_names[remote_cert],
                        (unsigned int)gatt_buf_len);

  gatt_buf_len = 0;
  gatt_char = SL_BT_CBAP_CHAR_COUNT;
  gatt_receiving = false;

  return SL_STATUS_OK;
}

/******************************************************************************
 * Check that every remote certificate the local role requires has arrived.
 *
 * Only a verifier has certificates to receive, a prover presents its own ones
 * and has nothing to check.
 *
 * @param[in] report Log the first missing certificate. Pass false when the
 *   transfer may still be in progress, as a missing certificate is expected
 *   in that case.
 *
 * @return SL_STATUS_OK if every required certificate is stored,
 *         SL_STATUS_NOT_FOUND otherwise.
 *****************************************************************************/
static sl_status_t check_remote_certs(bool report)
{
  if (!SLI_BT_CBAP_ROLE_IS_VERIFIER) {
    return SL_STATUS_OK;
  }

  for (uint8_t i = 0; i < REMOTE_CERT_COUNT; i++) {
    if (!remote_cert_stored[i]) {
      if (report) {
        SLI_BT_CBAP_LOG_ERROR("The %s certificate of the remote device is "
                              "missing." SLI_BT_CBAP_LOG_NL,
                              remote_cert_names[i]);
      }
      return SL_STATUS_NOT_FOUND;
    }
  }

  return SL_STATUS_OK;
}

/******************************************************************************
 * Validate the certificate chain the remote device presented.
 *
 * The certificate chain of the remote device is built from the batch and device
 * certificates it sent, and anchored to the trust hierarchy of the local
 * device. Only a verifier receives certificates, a prover has nothing to
 * validate.
 *
 * A valid chain proves that the certificates were issued by the expected
 * authorities, not that the remote device owns them. The certificates are
 * public, so anyone could replay them. Authentication is only complete once the
 * remote device also proves that it holds the private key of the chain, which
 * is what its signed ephemeral session key proves.
 *
 * @note Call only once every required remote certificate has been stored.
 *
 * @return SL_STATUS_OK if the chain is valid, error code otherwise.
 *****************************************************************************/
static sl_status_t validate_remote_cert_chain(void)
{
  if (!SLI_BT_CBAP_ROLE_IS_VERIFIER || remote_chain_valid) {
    return SL_STATUS_OK;
  }

  sl_status_t sc;
  sc = sli_bt_cbap_validate_remote_chain();
  if (sc != SL_STATUS_OK) {
    SLI_BT_CBAP_LOG_ERROR("The certificate chain of the remote device is "
                          "invalid, it does not belong to the trust hierarchy "
                          "of this device!" SLI_BT_CBAP_LOG_NL);
    return sc;
  }

  remote_chain_valid = true;
  SLI_BT_CBAP_LOG_INFO("The certificate chain of the remote device is "
                       "valid." SLI_BT_CBAP_LOG_NL);
  return SL_STATUS_OK;
}

/******************************************************************************
 * Check whether the CBAP exchange is complete, so pairing may proceed.
 *
 * @return true once the session is established, the passkey is ready for
 *         authenticated pairing and, for a verifier, the certificate chain of
 *         the remote device is valid. false otherwise.
 *****************************************************************************/
static bool is_procedure_complete(void)
{
  // The central must authenticate the complete ciphertext. The server is
  // ready once it installs the passkey: SMP may arrive before the last GATT
  // indication confirmation, because those protocols use different channels.
  return (session_ready && local_session_sent && passkey_ready
          && (!is_central || pairing_parameters_transferred)
          && (!SLI_BT_CBAP_ROLE_IS_VERIFIER || remote_chain_valid));
}

/******************************************************************************
 * Continue the session on the characteristic selected by char_state.
 *
 * The central device first subscribes to the session key of the peripheral
 * device, then writes its own one. Once the session is established, it
 * subscribes to the pairing parameters of the peripheral device.
 *
 * @return SL_STATUS_OK on success, SL_STATUS_INVALID_STATE if char_state
 *         selects no session step, error code otherwise.
 *****************************************************************************/
static sl_status_t continue_session(void)
{
  sl_status_t sc;

  if (char_state == SL_BT_CBAP_CHAR_SESSION_KEY_SERVER) {
    // Create our key before processing the peer's public key. Keep our signed
    // bundle separate from the shared receive buffer until it can be written.
    sc = ensure_local_session();
    if (sc != SL_STATUS_OK) {
      return sc;
    }
    gatt_buf_len = 0;
    gatt_char = (uint8_t)char_state;
    gatt_receiving = true;
    central_gatt_operation = CENTRAL_GATT_OPERATION_ENABLE_INDICATIONS;
    return sl_bt_gatt_set_characteristic_notification(
      candidate_device.handle,
      cbap_char_handles[char_state],
      sl_bt_gatt_indication);
  }
  if (char_state == SL_BT_CBAP_CHAR_SESSION_KEY_CLIENT) {
    memcpy(gatt_buf, local_session_data, local_session_len);
    gatt_buf_len = local_session_len;
    gatt_char = (uint8_t)char_state;
    gatt_send_offset = 0;
    gatt_sending = true;
    central_gatt_operation = CENTRAL_GATT_OPERATION_WRITE_FRAGMENT;
    return send_write_fragment((uint8_t)char_state);
  }
  if ((char_state != SL_BT_CBAP_CHAR_PAIRING_PARAMETERS_SERVER)
      || !session_ready || !local_session_sent) {
    return SL_STATUS_INVALID_STATE;
  }

  SLI_BT_CBAP_LOG_INFO("Session key exchange complete." SLI_BT_CBAP_LOG_NL);
  central_state = CENTRAL_RECEIVE_PAIRING_PARAMETERS;
  gatt_buf_len = 0;
  gatt_char = (uint8_t)char_state;
  gatt_receiving = true;
  central_gatt_operation = CENTRAL_GATT_OPERATION_ENABLE_INDICATIONS;
  return sl_bt_gatt_set_characteristic_notification(
    candidate_device.handle,
    cbap_char_handles[char_state],
    sl_bt_gatt_indication);
}

/******************************************************************************
 * Collect a fragment that the central device writes to a client
 * characteristic, and answer the write request.
 *
 * A complete certificate is stored, a complete session key completes the
 * session. A prover-only device has no use for the certificates of the
 * central device, so it collects and discards them.
 *
 * @param[in] req The write request.
 * @param[in] index The client characteristic the request writes.
 *****************************************************************************/
static void receive_data_write(const user_write_request_t *req, uint8_t index)
{
  sl_status_t sc = SL_STATUS_OK;
  bool complete = false;
  bool session_data = (index == SL_BT_CBAP_CHAR_SESSION_KEY_CLIENT);
  bool discard = !session_data && !SLI_BT_CBAP_ROLE_IS_VERIFIER;

  if ((req->offset != 0) || gatt_sending
      || (gatt_receiving && (gatt_char != index))) {
    sc = SL_STATUS_INVALID_STATE;
  } else {
    if (!gatt_receiving) {
      gatt_buf_len = 0;
      gatt_char = index;
      gatt_receiving = true;
    }
    sc = collect_fragment(req->value.data, req->value.len,
                          session_data ? SLI_BT_CBAP_SESSION_DATA_LEN
                          : sizeof(gatt_buf),
                          &complete);
  }
  if ((sc == SL_STATUS_OK) && complete) {
    if (session_data) {
      sc = sli_bt_cbap_session_process(gatt_buf, gatt_buf_len);
      session_ready = (sc == SL_STATUS_OK);
    } else if (!discard) {
      sc = store_remote_cert(index);
    }
    gatt_buf_len = 0;
    gatt_receiving = false;
    gatt_char = SL_BT_CBAP_CHAR_COUNT;
  }

  // Validate the last certificate before acknowledging it. The client can
  // subscribe to the session characteristic immediately after this response.
  if ((sc == SL_STATUS_OK) && complete && !session_data
      && (check_remote_certs(false) == SL_STATUS_OK)) {
    sc = validate_remote_cert_chain();
  }
  uint8_t att_error = (sc == SL_STATUS_OK) ? ATT_SUCCESS
                      : (uint8_t)SL_STATUS_BT_ATT_WRITE_REQUEST_REJECTED;
  sl_status_t response_sc = sl_bt_gatt_server_send_user_write_response(
    req->connection, req->characteristic, att_error);
  if ((sc != SL_STATUS_OK) || (response_sc != SL_STATUS_OK)) {
    on_error((sc != SL_STATUS_OK) ? sc : response_sc);
  }
}

/******************************************************************************
 * Serve the indications of the server characteristics on the peripheral.
 *
 * Enabling the indications of a server characteristic starts the transfer of
 * its value: a certificate, the local session key, or the encrypted pairing
 * parameters, which are generated at that point. The confirmation of each
 * indication sends the next fragment, until the whole value is transferred.
 * Disabling the indications in mid-transfer aborts the procedure.
 *
 * @param[in] status The characteristic status event.
 *****************************************************************************/
static void handle_indication_status(
  const sl_bt_evt_gatt_server_characteristic_status_t *status)
{
  if ((status->connection != candidate_device.handle) || is_central
      || bonded_reconnect) {
    return;
  }

  uint8_t index = find_cert_char(status->characteristic, true);
  if (status->characteristic == gattdb_session_key_server) {
    index = (uint8_t)SL_BT_CBAP_CHAR_SESSION_KEY_SERVER;
  } else if (status->characteristic == gattdb_pairing_parameters_server) {
    index = (uint8_t)SL_BT_CBAP_CHAR_PAIRING_PARAMETERS_SERVER;
  }
  if (index == SL_BT_CBAP_CHAR_COUNT) {
    return;
  }

  sl_status_t sc;
  if (status->status_flags == sl_bt_gatt_server_client_config) {
    if ((status->client_config_flags & sl_bt_gatt_server_indication) == 0U) {
      if (gatt_sending && (gatt_char == index)) {
        on_error(SL_STATUS_ABORT);
      }
      return;
    }
    if (gatt_receiving || gatt_sending) {
      on_error(SL_STATUS_INVALID_STATE);
      return;
    }

    if (index == SL_BT_CBAP_CHAR_SESSION_KEY_SERVER) {
      if ((local_session_len != 0) || session_ready
          || (check_remote_certs(false) != SL_STATUS_OK)) {
        on_error(SL_STATUS_INVALID_STATE);
        return;
      }
      sc = validate_remote_cert_chain();
      if (sc == SL_STATUS_OK) {
        sc = ensure_local_session();
      }
      if (sc == SL_STATUS_OK) {
        memcpy(gatt_buf, local_session_data, local_session_len);
        gatt_buf_len = local_session_len;
      }
    } else if (index == SL_BT_CBAP_CHAR_PAIRING_PARAMETERS_SERVER) {
      if (!session_ready || !local_session_sent || passkey_ready) {
        on_error(SL_STATUS_INVALID_STATE);
        return;
      }
      sc = prepare_pairing_parameters();
    } else {
      if (!SLI_BT_CBAP_ROLE_IS_PROVER || (local_session_len != 0)
          || session_ready) {
        on_error(SL_STATUS_INVALID_STATE);
        return;
      }
      sc = sli_bt_cbap_get_certificate(cert_chars[index].local_id,
                                       gatt_buf,
                                       &gatt_buf_len,
                                       sizeof(gatt_buf));
    }
    if ((sc != SL_STATUS_OK) || (gatt_buf_len == 0)) {
      on_error((sc == SL_STATUS_OK) ? SL_STATUS_EMPTY : sc);
      return;
    }
    gatt_char = index;
    gatt_send_offset = 0;
    gatt_sending = true;
  } else if (status->status_flags == sl_bt_gatt_server_confirmation) {
    if (!gatt_sending || (gatt_char != index)) {
      return;
    }
    if (gatt_send_offset == gatt_buf_len) {
      if (index == SL_BT_CBAP_CHAR_SESSION_KEY_SERVER) {
        local_session_sent = true;
      } else if (index == SL_BT_CBAP_CHAR_PAIRING_PARAMETERS_SERVER) {
        pairing_parameters_transferred = true;
        send_security_request();
      }
      gatt_sending = false;
      gatt_send_offset = 0;
      gatt_buf_len = 0;
      gatt_char = SL_BT_CBAP_CHAR_COUNT;
      return;
    }
  } else {
    return;
  }

  sc = send_indication_fragment(index);
  if (sc != SL_STATUS_OK) {
    on_error(sc);
  }
}

/******************************************************************************
 * Refuse a write request of the remote device with the given ATT error.
 *
 * A prepared write has a response of its own, which normally echoes the part
 * that was collected. A refused one has nothing to echo.
 *
 * @param[in] req The write request.
 * @param[in] att_error The ATT error code to respond with.
 *****************************************************************************/
static void refuse_write(const user_write_request_t *req, uint8_t att_error)
{
  sl_status_t sc;

  if (req->att_opcode == sl_bt_gatt_prepare_write_request) {
    sc = sl_bt_gatt_server_send_user_prepare_write_response(
      req->connection,
      req->characteristic,
      att_error,
      req->offset,
      0,     // value_len
      NULL); // value
  } else {
    sc = sl_bt_gatt_server_send_user_write_response(req->connection,
                                                    req->characteristic,
                                                    att_error);
  }

  if (sc != SL_STATUS_OK) {
    SLI_BT_CBAP_LOG_ERROR("Failed to refuse the write: 0x%04lx!"
                          SLI_BT_CBAP_LOG_NL, sc);
  }
}

/******************************************************************************
 * Find the CBAP characteristic a write request of the remote device targets.
 *
 * @param[in] local_handle Handle of the characteristic in the local GATT
 *                         database.
 *
 * @return Index of the characteristic, SL_BT_CBAP_CHAR_COUNT if the handle
 *         belongs to no CBAP characteristic that the remote device may write.
 *****************************************************************************/
static uint8_t resolve_write_target(uint16_t local_handle)
{
  if (local_handle == gattdb_session_key_client) {
    return (uint8_t)SL_BT_CBAP_CHAR_SESSION_KEY_CLIENT;
  }
  return find_cert_char(local_handle, false);
}

/******************************************************************************
 * Check a write request to a CBAP characteristic before collecting it.
 *
 * Only the candidate device may write, with plain write requests, and only in
 * the order of the procedure. Any other write is refused, and a write in the
 * wrong order fails the procedure.
 *
 * @param[in] req The write request.
 *****************************************************************************/
static void receive_user_write(const user_write_request_t *req)
{
  uint8_t index = resolve_write_target(req->characteristic);
  if (index == (uint8_t)SL_BT_CBAP_CHAR_COUNT) {
    return;
  }
  if ((req->connection != candidate_device.handle) || is_central
      || bonded_reconnect) {
    refuse_write(req, (uint8_t)SL_STATUS_BT_ATT_INSUFFICIENT_AUTHORIZATION);
    return;
  }
  if (req->att_opcode != sl_bt_gatt_write_request) {
    refuse_write(req, (uint8_t)SL_STATUS_BT_ATT_REQUEST_NOT_SUPPORTED);
    return;
  }

  // Freeze identity once its signed session key is accepted. Certificates must
  // also stay fixed after validation, before any session data is processed.
  if (session_ready
      || ((index < CERT_CHAR_COUNT)
          && (remote_chain_valid || (local_session_len != 0)))
      || ((index == SL_BT_CBAP_CHAR_SESSION_KEY_CLIENT)
          && ((local_session_len == 0) || !local_session_sent
              || (SLI_BT_CBAP_ROLE_IS_VERIFIER && !remote_chain_valid)))) {
    refuse_write(req, (uint8_t)SL_STATUS_BT_ATT_WRITE_REQUEST_REJECTED);
    on_error(SL_STATUS_INVALID_STATE);
    return;
  }
  receive_data_write(req, index);
}

/******************************************************************************
 * Reset the procedure states and the handles discovered on the remote device.
 *****************************************************************************/
static void reset_procedure(void)
{
  // States
  central_state = CENTRAL_IDLE;
  char_state = (sl_bt_cbap_characteristics_t)0;

  // GATT handles
  cbap_service_handle = HANDLE_NOT_INITIALIZED;
  for (uint8_t i = 0; i < SL_BT_CBAP_CHAR_COUNT; i++) {
    cbap_char_handles[i] = HANDLE_NOT_INITIALIZED;
  }

  // Framed transfer. The certificates of the previous remote device are
  // left in the storage, they are overwritten as the new ones arrive.
  gatt_buf_len = 0;
  gatt_char = SL_BT_CBAP_CHAR_COUNT;
  gatt_receiving = false;
  gatt_send_offset = 0;
  gatt_sending = false;
  central_gatt_operation = CENTRAL_GATT_OPERATION_NONE;
  for (uint8_t i = 0; i < REMOTE_CERT_COUNT; i++) {
    remote_cert_stored[i] = false;
  }
  remote_chain_valid = false;

  memset(local_session_data, 0, sizeof(local_session_data));
  local_session_len = 0;
  local_session_sent = false;
  session_ready = false;
  pairing_passkey = 0;
  passkey_ready = false;
  pairing_parameters_transferred = false;
  passkey_used = false;
  bonded_reconnect = false;
  candidate_bonding = SL_BT_INVALID_BONDING_HANDLE;
  link_secured = false;
  pairing_complete = false;

  sl_status_t clear_sc = sli_bt_cbap_session_clear();
  if (clear_sc != SL_STATUS_OK) {
    SLI_BT_CBAP_LOG_ERROR("Failed to clear the session: 0x%04lx!"
                          SLI_BT_CBAP_LOG_NL, clear_sc);
  }
  // The stack's fixed display passkey is global. Release it for the next link.
  clear_sc = sl_bt_sm_set_passkey(-1);
  if (clear_sc != SL_STATUS_OK) {
    SLI_BT_CBAP_LOG_ERROR("Failed to reset the passkey: 0x%04lx!"
                          SLI_BT_CBAP_LOG_NL, clear_sc);
  }
  // The IO capability is global too. The central device displays the passkey
  // only for the peripheral device that asked for it.
  if (is_central && displays_passkey) {
    clear_sc = sl_bt_sm_configure(SM_CONFIGURATION_FLAGS,
                                  CENTRAL_IO_CAPABILITY);
    if (clear_sc != SL_STATUS_OK) {
      SLI_BT_CBAP_LOG_ERROR("Failed to restore the IO capability: 0x%04lx!"
                            SLI_BT_CBAP_LOG_NL, clear_sc);
    }
  }
  displays_passkey = false;

  sl_status_t sc = sli_bt_cbap_destroy_peer_pub_key();
  if (sc != SL_STATUS_OK) {
    SLI_BT_CBAP_LOG_ERROR("Failed to release the key of the remote device: "
                          "0x%04lx!" SLI_BT_CBAP_LOG_NL, sc);
  }
}

/******************************************************************************
 * Report success once the authenticated link is ready, and its bond is stored
 * if bonding is enabled.
 *
 * Fresh pairing can report encryption and its completion in either order. A
 * reconnect already has a bond, and only waits for encryption to be restored.
 *****************************************************************************/
static void complete_connection(void)
{
  if (!link_secured || (!bonded_reconnect && !pairing_complete)) {
    return;
  }

  sl_bt_cbap_conn_t completed_device = candidate_device;
  sl_status_t sc = app_timer_stop(&timer);
  app_assert_status(sc);

  if (bonded_reconnect) {
    SLI_BT_CBAP_LOG_INFO("Bonded connection authenticated."
                         SLI_BT_CBAP_LOG_NL);
  } else if (BONDING_ENABLED) {
    SLI_BT_CBAP_LOG_INFO("CBAP pairing and bonding complete."
                         SLI_BT_CBAP_LOG_NL);
  } else {
    SLI_BT_CBAP_LOG_INFO("CBAP pairing complete, no bond stored."
                         SLI_BT_CBAP_LOG_NL);
  }

  // Release the candidate before notifying the application, which may start
  // looking for another peer from its callback.
  candidate_connected = false;
  candidate_device.handle = SL_BT_INVALID_CONNECTION_HANDLE;
  memset(candidate_device.address.addr, 0xff, sizeof(bd_addr));
  reset_procedure();
  if (callback != NULL) {
    callback(completed_device, SL_STATUS_OK);
  }
}

/******************************************************************************
 * Validate one fragment and collect its payload into gatt_buf.
 *
 * A fragment without payload cannot advance a transfer, so it is invalid.
 *
 * @param[in] data The fragment, starting with its continuation flag.
 * @param[in] len Length of the fragment.
 * @param[in] capacity Maximum length of the complete value.
 * @param[out] complete Set to true if this was the last fragment of the value.
 *
 * @return SL_STATUS_OK on success, SL_STATUS_INVALID_PARAMETER if the fragment
 *         is malformed, SL_STATUS_WOULD_OVERFLOW if the value is longer than
 *         @p capacity, error code otherwise.
 *****************************************************************************/
static sl_status_t collect_fragment(const uint8_t *data,
                                    size_t len,
                                    size_t capacity,
                                    bool *complete)
{
  *complete = false;
  if ((len <= GATT_FRAGMENT_HEADER_LEN)
      || ((data[0] != GATT_FRAGMENT_MORE)
          && (data[0] != GATT_FRAGMENT_LAST))) {
    return SL_STATUS_INVALID_PARAMETER;
  }
  sl_status_t sc = collect_part(gatt_buf, &gatt_buf_len, capacity,
                                (uint16_t)gatt_buf_len,
                                &data[GATT_FRAGMENT_HEADER_LEN],
                                len - GATT_FRAGMENT_HEADER_LEN);
  if (sc == SL_STATUS_OK) {
    *complete = (data[0] == GATT_FRAGMENT_LAST);
    if (!(*complete) && (gatt_buf_len == capacity)) {
      return SL_STATUS_WOULD_OVERFLOW;
    }
  }
  return sc;
}

/******************************************************************************
 * Create the local session data, unless it already exists.
 *
 * The session data is kept until it is sent, because gatt_buf is in use for
 * receiving the session data of the remote device in the meantime.
 *
 * @return SL_STATUS_OK on success, error code otherwise.
 *****************************************************************************/
static sl_status_t ensure_local_session(void)
{
  if (local_session_len != 0) {
    return SL_STATUS_OK;
  }
  return sli_bt_cbap_session_init(local_session_data, &local_session_len,
                                  sizeof(local_session_data));
}

/******************************************************************************
 * Generate the pairing parameters on the peripheral and encrypt them into
 * gatt_buf.
 *
 * The pairing parameters are a random passkey and the IO capability of the
 * peripheral device, which tells the central device to enter the passkey. The
 * passkey is installed in the Bluetooth stack before it is sent, so that the
 * pairing that the central device starts on receiving it can use it.
 *
 * @return SL_STATUS_OK on success, error code otherwise.
 *****************************************************************************/
static sl_status_t prepare_pairing_parameters(void)
{
  sl_status_t sc = sli_bt_cbap_generate_random(0,
                                               SLI_BT_CBAP_PASSKEY_LIMIT,
                                               &pairing_passkey);
  if (sc != SL_STATUS_OK) {
    return sc;
  }
  // Network byte order, most significant byte first.
  uint8_t plaintext[SLI_BT_CBAP_PAIRING_PARAMETERS_LEN];
  for (size_t i = 0; i < SLI_BT_CBAP_PASSKEY_LEN; i++) {
    size_t shift = 8U * (SLI_BT_CBAP_PASSKEY_LEN - 1U - i);
    plaintext[i] = (uint8_t)(pairing_passkey >> shift);
  }
  plaintext[SLI_BT_CBAP_PASSKEY_LEN] = (uint8_t)PERIPHERAL_IO_CAPABILITY;
  sc = sli_bt_cbap_session_encrypt(plaintext, sizeof(plaintext),
                                   gatt_buf, &gatt_buf_len, sizeof(gatt_buf));
  memset(plaintext, 0, sizeof(plaintext));
  if (sc != SL_STATUS_OK) {
    return sc;
  }
  if (gatt_buf_len != SLI_BT_CBAP_ENCRYPTED_PAIRING_PARAMETERS_LEN) {
    return SL_STATUS_INVALID_COUNT;
  }
  sc = sl_bt_sm_set_passkey((int32_t)pairing_passkey);
  if (sc == SL_STATUS_OK) {
    displays_passkey = true;
    passkey_ready = true;
  }
  return sc;
}

/******************************************************************************
 * Decrypt the pairing parameters that the central device collected in
 * gatt_buf, and prepare the pairing with them.
 *
 * The CCM tag authenticates the ciphertext, so pairing parameters that were
 * not encrypted with the session key are rejected.
 *
 * @return SL_STATUS_OK on success, SL_STATUS_INVALID_PARAMETER if the passkey
 *         has more than six digits, SL_STATUS_NOT_SUPPORTED if the IO
 *         capability of the peripheral device rules out passkey entry
 *         pairing, error code otherwise.
 *****************************************************************************/
static sl_status_t process_pairing_parameters(void)
{
  if (!session_ready || passkey_ready
      || (gatt_buf_len != SLI_BT_CBAP_ENCRYPTED_PAIRING_PARAMETERS_LEN)) {
    return SL_STATUS_INVALID_STATE;
  }
  uint8_t plaintext[SLI_BT_CBAP_PAIRING_PARAMETERS_LEN];
  size_t plaintext_len = 0;
  sl_status_t sc = sli_bt_cbap_session_decrypt(gatt_buf, gatt_buf_len,
                                               plaintext, &plaintext_len,
                                               sizeof(plaintext));
  if ((sc == SL_STATUS_OK) && (plaintext_len != sizeof(plaintext))) {
    sc = SL_STATUS_INVALID_COUNT;
  }
  if (sc == SL_STATUS_OK) {
    uint32_t value = 0;
    for (size_t i = 0; i < SLI_BT_CBAP_PASSKEY_LEN; i++) {
      value = (value << 8) | plaintext[i];
    }
    if (value >= SLI_BT_CBAP_PASSKEY_LIMIT) {
      sc = SL_STATUS_INVALID_PARAMETER;
    } else {
      pairing_passkey = value;
      passkey_ready = true;
      sc = adapt_io_capability(plaintext[SLI_BT_CBAP_PASSKEY_LEN]);
    }
  }
  memset(plaintext, 0, sizeof(plaintext));
  return sc;
}

/******************************************************************************
 * Adapt the IO capability of the central device to the peripheral device.
 *
 * Passkey entry pairing needs one device that displays the passkey and one
 * that enters it. The central device keeps entering the passkey for a
 * peripheral device without a keyboard. For a peripheral device with a
 * keyboard, it displays the passkey instead: it installs the passkey in the
 * Bluetooth stack, so the passkey that is displayed is the one CBAP exchanged.
 *
 * @param[in] peer_io_capability IO capability of the peripheral device, a
 *                               sl_bt_sm_io_capability_t.
 *
 * @return SL_STATUS_OK on success, SL_STATUS_NOT_SUPPORTED if the peripheral
 *         device can neither display nor enter a passkey, error code
 *         otherwise.
 *****************************************************************************/
static sl_status_t adapt_io_capability(uint8_t peer_io_capability)
{
  sl_status_t sc;

  switch (peer_io_capability) {
    case sl_bt_sm_io_capability_displayonly:
    case sl_bt_sm_io_capability_displayyesno:
      displays_passkey = false;
      return SL_STATUS_OK;

    case sl_bt_sm_io_capability_keyboardonly:
    case sl_bt_sm_io_capability_keyboarddisplay:
      // Set before configuring, so that the reset restores the configuration
      // even if this step fails halfway.
      displays_passkey = true;
      sc = sl_bt_sm_configure(SM_CONFIGURATION_FLAGS,
                              sl_bt_sm_io_capability_displayonly);
      if (sc == SL_STATUS_OK) {
        sc = sl_bt_sm_set_passkey((int32_t)pairing_passkey);
      }
      if (sc == SL_STATUS_OK) {
        SLI_BT_CBAP_LOG_DEBUG("The remote device enters the passkey."
                              SLI_BT_CBAP_LOG_NL);
      }
      return sc;

    default:
      SLI_BT_CBAP_LOG_ERROR("The remote device cannot take part in passkey "
                            "entry pairing, its IO capability is %u!"
                            SLI_BT_CBAP_LOG_NL,
                            (unsigned int)peer_io_capability);
      return SL_STATUS_NOT_SUPPORTED;
  }
}

/******************************************************************************
 * Collect an indicated fragment on the central, and confirm it.
 *
 * Once the whole value has arrived, the central device stores the certificate,
 * completes the session, or starts the pairing with the pairing parameters,
 * and moves on to the next step of the procedure.
 *
 * @param[in] value The characteristic value event of the indication.
 *****************************************************************************/
static void receive_indication(
  const sl_bt_evt_gatt_characteristic_value_t *value)
{
  bool certificate = ((central_state == CENTRAL_EXCHANGE_CERTIFICATES)
                      && (char_state < CERT_CHAR_COUNT)
                      && cert_chars[char_state].server);
  bool session_data = ((central_state == CENTRAL_CREATE_SESSION)
                       && (char_state == SL_BT_CBAP_CHAR_SESSION_KEY_SERVER));
  bool pairing_parameters =
    ((central_state == CENTRAL_RECEIVE_PAIRING_PARAMETERS)
     && (char_state == SL_BT_CBAP_CHAR_PAIRING_PARAMETERS_SERVER));
  if ((!certificate && !session_data && !pairing_parameters)
      || !gatt_receiving || (gatt_char != (uint8_t)char_state)
      || (central_gatt_operation != CENTRAL_GATT_OPERATION_NONE)
      || (value->characteristic != cbap_char_handles[char_state])
      || (value->att_opcode != sl_bt_gatt_handle_value_indication)
      || (value->offset != 0)) {
    on_error(SL_STATUS_INVALID_STATE);
    return;
  }

  bool complete;
  size_t capacity = certificate ? sizeof(gatt_buf)
                    : (session_data ? SLI_BT_CBAP_SESSION_DATA_LEN
                       : SLI_BT_CBAP_ENCRYPTED_PAIRING_PARAMETERS_LEN);
  sl_status_t sc = collect_fragment(value->value.data, value->value.len,
                                    capacity, &complete);
  if ((sc == SL_STATUS_OK) && complete) {
    if (certificate) {
      sc = store_remote_cert((uint8_t)char_state);
    } else if (session_data) {
      sc = sli_bt_cbap_session_process(gatt_buf, gatt_buf_len);
      session_ready = (sc == SL_STATUS_OK);
    } else {
      sc = process_pairing_parameters();
    }
  }
  if (sc == SL_STATUS_OK) {
    sc = sl_bt_gatt_send_characteristic_confirmation(value->connection);
  }
  if ((sc == SL_STATUS_OK) && complete) {
    gatt_buf_len = 0;
    gatt_receiving = false;
    gatt_char = SL_BT_CBAP_CHAR_COUNT;
    if (pairing_parameters) {
      pairing_parameters_transferred = true;
      central_state = CENTRAL_INCREASE_SECURITY;
      sc = sl_bt_sm_increase_security(candidate_device.handle);
    } else {
      char_state++;
      sc = certificate ? continue_cert_exchange() : continue_session();
    }
  }
  if (sc != SL_STATUS_OK) {
    on_error(sc);
  }
}

/******************************************************************************
 * Delete the bond that the bonded peer device has lost, and fail the
 * procedure.
 *
 * A peer device that was reset, or a phone whose user removed the device, no
 * longer has the keys of the bond, so the stored bond can never restore the
 * encryption again. Deleting it lets the next connection run the CBAP
 * procedure. A new bond requires the CBAP procedure, so a peer device that
 * only claims to have lost the bond gains nothing.
 *****************************************************************************/
static void forget_stale_bond(void)
{
  SLI_BT_CBAP_LOG_INFO("The remote device has lost bond %d. Deleting it, the "
                       "next connection runs the CBAP procedure."
                       SLI_BT_CBAP_LOG_NL,
                       candidate_bonding);

  // The stack also closes the connection if it is still open.
  sl_status_t sc = sl_bt_sm_delete_bonding(candidate_bonding);
  if (sc != SL_STATUS_OK) {
    SLI_BT_CBAP_LOG_ERROR("Failed to delete the bond: 0x%04lx!"
                          SLI_BT_CBAP_LOG_NL, sc);
    on_error(sc);
    return;
  }

  on_error(SL_STATUS_DELETED);
}

/******************************************************************************
 * Ask the central device to secure the link with a Security Request.
 *
 * A phone central neither starts the pairing nor restores the encryption with
 * a stored key by itself. A central device that is already securing the link
 * ignores the request, and the stack may refuse to send it then, which is not
 * an error: the procedure timer still limits the time until the link is
 * secured.
 *****************************************************************************/
static void send_security_request(void)
{
  sl_status_t sc = sl_bt_sm_increase_security(candidate_device.handle);
  if (sc != SL_STATUS_OK) {
    SLI_BT_CBAP_LOG_DEBUG("Security Request not sent: 0x%04lx."
                          SLI_BT_CBAP_LOG_NL, sc);
  }
}

/******************************************************************************
 * Stop the procedure timer once passkey entry pairing has started.
 *
 * The pairing may wait for a user who types the passkey on a phone, which
 * takes longer than the exchanges before it. The Security Manager Protocol
 * limits every step of the pairing to 30 seconds, and the stack reports a
 * pairing that times out, is cancelled or fails in a bonding failed event.
 *****************************************************************************/
static void stop_procedure_timer(void)
{
  sl_status_t sc = app_timer_stop(&timer);
  app_assert_status(sc);
}

/******************************************************************************
 * Fail the procedure that did not complete in time.
 *
 * @param[in] handle The expired timer.
 * @param[in] data Unused.
 *****************************************************************************/
static void on_timer_expired(app_timer_t *handle, void *data)
{
  (void)handle;
  (void)data;

  SLI_BT_CBAP_LOG_ERROR("Procedure timeout!" SLI_BT_CBAP_LOG_NL);
  on_error(SL_STATUS_TIMEOUT);
}

/******************************************************************************
 * Fail the procedure.
 *
 * Reset the procedure, close the connection of the candidate device if it is
 * still open, and report the failure to the application.
 *
 * @param[in] sc The reason of the failure.
 *****************************************************************************/
static void on_error(sl_status_t sc)
{
  // Make sure timer is stopped
  sl_status_t timer_sc;
  timer_sc = app_timer_stop(&timer);
  app_assert_status(timer_sc);

  // Reset states and discovered handles
  reset_procedure();

  SLI_BT_CBAP_LOG_ERROR("Error for connection %d. Status: 0x%04lx"
                        SLI_BT_CBAP_LOG_NL,
                        candidate_device.handle,
                        sc);

  // It is the responsibility of the module to close the suspicious connection
  if (candidate_connected) {
    SLI_BT_CBAP_LOG_ERROR("Closing connection..." SLI_BT_CBAP_LOG_NL);
    (void)sl_bt_connection_close(candidate_device.handle);
    candidate_connected = false;
  }

  // Call app callback. The candidate device is still known at this point, so
  // that the application can act on the device that failed the procedure.
  if (callback != NULL) {
    callback(candidate_device, sc);
  }

  // Clear connection parameters
  candidate_device.handle = SL_BT_INVALID_CONNECTION_HANDLE;
  for (uint8_t i = 0; i < sizeof(bd_addr); i++) {
    candidate_device.address.addr[i] = 0xff;
  }
}
