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

// Value of a GATT handle that has not been discovered yet
#define HANDLE_NOT_INITIALIZED  0

// A prover device sends its own certificates to prove itself, a verifier device
// validates the certificates received from the remote device. One device can
// be both (Full-role)
#define IS_PROVER    ((SL_BT_CBAP_ROLE & SL_BT_CBAP_ROLE_PROVER) != 0)
#define IS_VERIFIER  ((SL_BT_CBAP_ROLE & SL_BT_CBAP_ROLE_VERIFIER) != 0)

// The characteristics carrying certificates are the leading ones of
// sl_bt_cbap_characteristics_t, followed by the OOB data characteristics.
#define CERT_CHAR_COUNT  ((uint8_t)SL_BT_CBAP_CHAR_OOB_DATA_SERVER)

// Every certificate fragment carries a one-byte continuation flag before its
// payload. A value of one means that another fragment follows; zero marks the
// final fragment. This keeps the individual GATT values below ATT's 512-byte
// attribute-value limit while allowing the full DER certificate to be larger.
#define CERT_FRAGMENT_HEADER_LEN  1U
#define CERT_FRAGMENT_MORE        1U
#define CERT_FRAGMENT_LAST        0U
#define ATT_ATTRIBUTE_VALUE_MAX_LENGTH  512U

// ATT error code that reports a request served successfully
#define ATT_SUCCESS  0

// Shorthands for event data types of the user attribute requests
typedef sl_bt_evt_gatt_server_user_read_request_t  user_read_request_t;
typedef sl_bt_evt_gatt_server_user_write_request_t user_write_request_t;

// The certificates that the remote device presents about itself
typedef enum {
  REMOTE_CERT_BATCH,
  REMOTE_CERT_DEVICE,
  REMOTE_CERT_COUNT
} remote_cert_t;

// Properties of a characteristic that carries a certificate
typedef struct {
  // Handle of the characteristic in the local GATT database
  uint16_t local_handle;
  // True if the GATT server presents its own certificate on the
  // characteristic, false if the GATT client sends its own certificate on it.
  bool server;
  // Storage ID of the local certificate presented on the characteristic
  cbap_key_id_t local_id;
  // Storage ID the certificate received on the characteristic is written to
  cbap_key_id_t remote_id;
  // The remote certificate carried by the characteristic
  remote_cert_t remote_cert;
} cert_char_t;

// Central device states
typedef enum {
  CENTRAL_IDLE,
  CENTRAL_DISCOVER_SERVICES,
  CENTRAL_DISCOVER_CHARACTERISTICS,
  CENTRAL_EXCHANGE_CERTIFICATES,
  CENTRAL_EXCHANGE_OOB_DATA,
  CENTRAL_INCREASE_SECURITY,
  CENTRAL_DONE,
  CENTRAL_STATE_NUM
} central_state_t;

// GATT procedure completions during the certificate phase only belong to the
// CCCD write that enables indications or to one outgoing write fragment. The
// indication stream itself is advanced by characteristic-status events.
typedef enum {
  CENTRAL_CERT_OPERATION_NONE,
  CENTRAL_CERT_OPERATION_ENABLE_INDICATIONS,
  CENTRAL_CERT_OPERATION_WRITE_FRAGMENT
} central_cert_operation_t;

// -----------------------------------------------------------------------------
// Private variables

// State of the central device
static central_state_t central_state = (central_state_t)0;
// Pointing to the characteristic that shall be discovered next
static sl_bt_cbap_characteristics_t char_state = (sl_bt_cbap_characteristics_t)0;
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
  [SL_BT_CBAP_CHAR_OOB_DATA_SERVER] = {
    .data = { SL_BT_CBAP_OOB_DATA_SERVER_CHAR_UUID }
  },
  [SL_BT_CBAP_CHAR_OOB_DATA_CLIENT] = {
    .data = { SL_BT_CBAP_OOB_DATA_CLIENT_CHAR_UUID }
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

// Buffer of the certificate transfer in progress. A certificate rarely fits
// into a single ATT PDU, so it is assembled here before it is stored, and kept
// here while it is being sent in parts.
static uint8_t cert_buf[SL_BT_CBAP_CERTIFICATE_MAX_SIZE];
// Number of valid bytes in cert_buf
static size_t cert_len = 0;
// Index of the certificate characteristic cert_buf belongs to
static uint8_t cert_char = CERT_CHAR_COUNT;
// True while cert_buf is collecting the framed parts of a certificate of the
// remote device that has not been stored yet. Incoming writes are rejected
// while this buffer is instead sending a local certificate indication.
static bool cert_receiving = false;

// Outgoing certificate state. Indications are sent one at a time and each
// write request is completed before the next fragment is sent, so a shared
// fragment buffer is sufficient for either transport direction.
static uint8_t cert_fragment[SL_BT_CBAP_CERTIFICATE_MAX_SIZE
                             + CERT_FRAGMENT_HEADER_LEN];
static size_t cert_send_offset = 0;
static bool cert_sending = false;

// Only the fixed-size OOB data may use a legacy prepared write when a peer has
// a very small ATT MTU. Certificate transfers always use the framed protocol.
static uint8_t prepared_write_char = (uint8_t)SL_BT_CBAP_CHAR_COUNT;

// The GATT operation the central has in flight during certificate exchange.
static central_cert_operation_t central_cert_operation
  = CENTRAL_CERT_OPERATION_NONE;

// Flags of the remote certificates that have been received and stored.
static bool remote_cert_stored[REMOTE_CERT_COUNT] = { false };

// True once the certificates of the remote device have been built into a chain
// that is anchored to the trust hierarchy of this device. It says that the
// certificates are genuine, not that the remote device owns them.
static bool remote_chain_valid = false;

// The signed OOB data of this device, presented to the remote device. Only a
// prover has the device key that is needed to sign it, so on a verifier only
// device it stays empty.
static uint8_t local_oob[SL_BT_CBAP_SIGNED_OOB_DATA_LEN];
// Number of valid bytes in local_oob
static size_t local_oob_len = 0;

// The signed OOB data being received from the remote device.
static uint8_t remote_oob[SL_BT_CBAP_SIGNED_OOB_DATA_LEN];
// Number of bytes collected into remote_oob
static size_t remote_oob_len = 0;

// True once the OOB data of this pairing has been generated by the stack.
// Generating it again would invalidate the confirm value that the remote
// device has already received, therefore it is done once per procedure.
static bool local_oob_ready = false;

// True once the OOB data of the remote device has been verified with its
// validated device certificate and handed over to the stack. This is what
// completes the authentication of the remote device.
static bool remote_oob_verified = false;

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

// Callback storage
static sl_bt_cbap_cb_t callback = NULL;

// Timer handle
static app_timer_t timer;

// -----------------------------------------------------------------------------
// Private function declarations

// Start the discovery of the CBAP service on the remote device.
static sl_status_t discover_service(void);

// Start the discovery of the characteristic selected by char_state.
static sl_status_t discover_characteristic(void);

// Continue the certificate exchange on the characteristic selected by
// char_state, or finish it if there is none left.
static sl_status_t continue_cert_exchange(void);

// Send the next framed certificate indication from the local GATT server.
static sl_status_t send_certificate_indication(uint8_t index);

// Send the next framed certificate write from the central GATT client.
static sl_status_t send_certificate_write(uint8_t index);

// Find the certificate characteristic by its handle in the local GATT database.
static uint8_t find_cert_char(uint16_t local_handle, bool server);

// Collect a part of a value being received into the given buffer.
static sl_status_t collect_part(uint8_t *buf,
                                size_t *buf_len,
                                size_t capacity,
                                uint16_t offset,
                                const uint8_t *data,
                                size_t len);

// Write the certificate collected in cert_buf to the persistent storage.
static sl_status_t store_remote_cert(uint8_t index);

// Check that every remote certificate the local role requires has arrived.
static sl_status_t check_remote_certs(bool report);

// Validate the certificate chain the remote device presented.
static sl_status_t validate_remote_cert_chain(void);

// Generate the OOB data of this pairing, and sign it if this device can.
static sl_status_t ensure_local_oob(void);

// Continue the OOB data exchange on the characteristic selected by char_state,
// or finish it if there is none left.
static sl_status_t continue_oob_exchange(void);

// Verify the OOB data collected from the remote device and pass it to the
// stack.
static sl_status_t process_remote_oob(void);

// Check that the OOB data of the remote device arrived if it is required.
static sl_status_t check_remote_oob(void);

// True if this device has done everything its role requires of it, so that the
// security level may be increased.
static bool procedure_complete(void);

// Serve a read request of the remote device on the OOB data characteristic.
static void serve_oob_read(const user_read_request_t *req);

// Handle a write request of the remote device on the OOB data characteristic.
static void receive_oob_write(const user_write_request_t *req);

// Get the number of bytes that fit in a user-read response for this request.
static sl_status_t get_read_response_length(const user_read_request_t *req,
                                            size_t remaining,
                                            size_t *response_length);

// Handle a write request of the remote device on a certificate characteristic.
static void receive_cert_write(const user_write_request_t *req, uint8_t index);

// Refuse a write request of the remote device with the given ATT error.
static void refuse_write(const user_write_request_t *req, uint8_t att_error);

// Find the CBAP characteristic a read request of the remote device targets.
static uint8_t resolve_read_target(uint16_t local_handle);

// Find the CBAP characteristic a write request of the remote device targets.
static uint8_t resolve_write_target(uint16_t local_handle);

// Route a read request of the remote device to the handler of its
// characteristic.
static void serve_user_read(const user_read_request_t *req);

// Route a write request of the remote device to the handler of its
// characteristic.
static void receive_user_write(const user_write_request_t *req);

// Advance a local certificate indication after a CCCD change or confirmation.
static void handle_cert_indication_status(
  const sl_bt_evt_gatt_server_characteristic_status_t *status);

// Reset the procedure states and the handles discovered on the remote device.
static void reset_procedure(void);

// Timer callback.
static void timer_cb(app_timer_t *handle, void *data);

// Error handler.
static void on_error(sl_status_t sc);

// -----------------------------------------------------------------------------
// Public function definitions

// Initialize CBAP BLE module.
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
 * Check whether this component currently owns a CBAP procedure.
 *****************************************************************************/
bool sl_bt_cbap_is_procedure_in_progress(void)
{
  return candidate_device.handle != SL_BT_INVALID_CONNECTION_HANDLE;
}

// -----------------------------------------------------------------------------
// Private function definitions

// Bluetooth event handler.
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
    case sl_bt_evt_system_boot_id: {
      // Configure the security manager for the role of the device. Pairing is
      // always Secure Connections, and Man-In-The-Middle protection comes from
      // the OOB association model. There is nothing to display or enter, the
      // OOB data travels over the CBAP characteristics.
      //
      // Requiring MITM protection here is not an option: with no input and no
      // output the stack has no association model to provide it at this point,
      // as the OOB data of the procedure does not exist yet, and it rejects the
      // combination. What the stack cannot demand up front, this module checks
      // when the link is secured: a procedure that ends in anything but mode 1
      // level 4 fails.
      //
      // Every bonding request has to be confirmed by this module, so that the
      // remote device cannot pair before it is authenticated.
      uint8_t sm_flags = SL_BT_SM_CONFIGURATION_SC_ONLY
                         | SL_BT_SM_CONFIGURATION_BONDING_REQUEST_REQUIRED;

      if (SL_BT_CBAP_ROLE == SL_BT_CBAP_ROLE_FULL) {
        // A Full-role device authenticates the remote device and proves itself
        // in return, so the OOB exchange has to run in both directions. Were
        // one direction enough, an attacker could drop the OOB data of one side
        // and silently downgrade the pairing to a one-sided one.
        //
        // This is the half of the requirement that the stack can enforce and
        // this module cannot: whether the remote device consumed the OOB data
        // of this device is only visible inside the pairing. The other half,
        // that the data of the remote device carries a signature of a validated
        // certificate chain, is checked by this module.
        //
        // A Full-role device is therefore only compatible with a remote device
        // that both proves itself and verifies, so with another Full-role one.
        sm_flags |= SL_BT_SM_CONFIGURATION_OOB_FROM_BOTH_DEVICES_REQUIRED;
      }

      sc = sl_bt_sm_configure(sm_flags,
                              sl_bt_sm_io_capability_noinputnooutput);
      if (sc != SL_STATUS_OK) {
        CBAP_LOG_ERROR("Failed to configure sm: [0x%04lx]" CBAP_LOG_NL, sc);
        app_assert_status(sc);
      }
      break;
    }

    // -------------------------------
    // This event indicates that a new connection was opened.
    case sl_bt_evt_connection_opened_id:
      // Check if there is a CBAP prodecure in progress already.
      if (candidate_device.handle != SL_BT_INVALID_CONNECTION_HANDLE) {
        CBAP_LOG_ERROR("There cannot be multiple procedures in progress at " \
                       "the same time. Closing new connection." CBAP_LOG_NL);
        (void)sl_bt_connection_close(evt->data.evt_connection_opened.connection);
        break;
      }

      // CBAP procedure can be started. Start timeout.
      sc = app_timer_start(&timer,
                           SL_BT_CBAP_PROCEDURE_TIMEOUT,
                           timer_cb,
                           (void *)NULL, // Callback has no parameters
                           false);
      app_assert_status(sc);

      // Store candidate connection.
      candidate_device.handle = evt->data.evt_connection_opened.connection;
      candidate_device.address = evt->data.evt_connection_opened.address;
      candidate_connected = true;

      // Discard the handles discovered on the previous remote device.
      reset_procedure();

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
        CBAP_LOG_ERROR("The connection was closed in mid-procedure! " \
                       "Reason: 0x%04x" CBAP_LOG_NL,
                       evt->data.evt_connection_closed.reason);

        // The error handler reports the candidate device to the application,
        // therefore it clears the connection parameters, not this handler.
        candidate_connected = false;
        on_error(evt->data.evt_connection_closed.reason);
      }
      break;

    // -------------------------------
    // This event is generated when a new service is discovered
    case sl_bt_evt_gatt_service_id:
      if (evt->data.evt_gatt_service.connection == candidate_device.handle
          && is_central
          && cbap_service_handle == HANDLE_NOT_INITIALIZED) {
        // Save the handle of the first matching service for future reference.
        cbap_service_handle = evt->data.evt_gatt_service.service;
        CBAP_LOG_DEBUG("CBAP service found. Handle: %lu" CBAP_LOG_NL,
                       cbap_service_handle);
      }
      break;

    // -------------------------------
    // This event is generated when a new characteristic is discovered
    case sl_bt_evt_gatt_characteristic_id:
      if (evt->data.evt_gatt_characteristic.connection == candidate_device.handle
          && is_central
          && cbap_char_handles[char_state] == HANDLE_NOT_INITIALIZED) {
        // Save the handle of the characteristic being discovered.
        cbap_char_handles[char_state] =
          evt->data.evt_gatt_characteristic.characteristic;
        CBAP_LOG_DEBUG("Characteristic %d found. Handle: %d" CBAP_LOG_NL,
                       char_state,
                       cbap_char_handles[char_state]);
      }
      break;

    // -------------------------------
    // This event is generated for various procedure completions, e.g. when a
    // write procedure is completed, or service discovery is completed
    case sl_bt_evt_gatt_procedure_completed_id:
      if (evt->data.evt_gatt_procedure_completed.connection
          == candidate_device.handle) {
        // Check result
        if (evt->data.evt_gatt_procedure_completed.result != 0) {
          CBAP_LOG_ERROR("GATT procedure completed error. Connection: %d. " \
                         "Error: 0x%04x." CBAP_LOG_NL,
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
                CBAP_LOG_ERROR("CBAP service not found on the remote " \
                               "device." CBAP_LOG_NL);
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
                CBAP_LOG_ERROR("Characteristic %d not found on the remote " \
                               "device." CBAP_LOG_NL, char_state);
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
                CBAP_LOG_INFO("GATT discovery complete." CBAP_LOG_NL);

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
              if (central_cert_operation
                  == CENTRAL_CERT_OPERATION_ENABLE_INDICATIONS) {
                // The peripheral starts its indication stream only after the
                // CCCD write is complete. Its final fragment advances the
                // characteristic walk in the value-event handler.
                central_cert_operation = CENTRAL_CERT_OPERATION_NONE;
              } else if (central_cert_operation
                         == CENTRAL_CERT_OPERATION_WRITE_FRAGMENT) {
                if (cert_send_offset < cert_len) {
                  sc = send_certificate_write(char_state);
                } else {
                  // The final write response acknowledged the complete
                  // certificate. The peripheral has stored it by now.
                  cert_sending = false;
                  cert_len = 0;
                  cert_char = CERT_CHAR_COUNT;
                  char_state++;
                  central_cert_operation = CENTRAL_CERT_OPERATION_NONE;
                  sc = continue_cert_exchange();
                }

                if (sc != SL_STATUS_OK) {
                  on_error(sc);
                }
              }
              break;

            case CENTRAL_EXCHANGE_OOB_DATA:
              // The transfer on the current characteristic is over: a read has
              // delivered the signed OOB data of the remote device by now, a
              // write has delivered ours.
              if (char_state == SL_BT_CBAP_CHAR_OOB_DATA_SERVER) {
                sc = process_remote_oob();
                if (sc != SL_STATUS_OK) {
                  on_error(sc);
                  break;
                }
              }

              char_state++;
              sc = continue_oob_exchange();
              if (sc != SL_STATUS_OK) {
                on_error(sc);
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
      if (evt->data.evt_gatt_characteristic_value.connection
          != candidate_device.handle
          || !is_central) {
        break;
      }

      if (central_state == CENTRAL_EXCHANGE_CERTIFICATES) {
        const sl_bt_evt_gatt_characteristic_value_t *value
          = &evt->data.evt_gatt_characteristic_value;
        uint8_t more;

        if (char_state >= CERT_CHAR_COUNT
            || !cert_chars[char_state].server
            || central_cert_operation != CENTRAL_CERT_OPERATION_NONE
            || value->characteristic != cbap_char_handles[char_state]
            || value->att_opcode != sl_bt_gatt_handle_value_indication
            || value->value.len < CERT_FRAGMENT_HEADER_LEN) {
          CBAP_LOG_ERROR("Received an unexpected certificate indication." \
                         CBAP_LOG_NL);
          on_error(SL_STATUS_INVALID_STATE);
          break;
        }

        more = value->value.data[0];
        if (more != CERT_FRAGMENT_MORE && more != CERT_FRAGMENT_LAST) {
          CBAP_LOG_ERROR("Received a certificate fragment with an invalid " \
                         "continuation flag." CBAP_LOG_NL);
          on_error(SL_STATUS_INVALID_PARAMETER);
          break;
        }

        sc = collect_part(
          cert_buf,
          &cert_len,
          sizeof(cert_buf),
          cert_len,
          &value->value.data[CERT_FRAGMENT_HEADER_LEN],
          value->value.len - CERT_FRAGMENT_HEADER_LEN);
        if (sc == SL_STATUS_OK) {
          CBAP_LOG_DEBUG("Received %u certificate bytes (%u total)." \
                         CBAP_LOG_NL,
                         (unsigned int)(value->value.len
                                        - CERT_FRAGMENT_HEADER_LEN),
                         (unsigned int)cert_len);
          sc = sl_bt_gatt_send_characteristic_confirmation(value->connection);
        }

        if (sc == SL_STATUS_OK && more == CERT_FRAGMENT_LAST) {
          sc = store_remote_cert(char_state);
          if (sc == SL_STATUS_OK) {
            char_state++;
            sc = continue_cert_exchange();
          }
        }
      } else if (central_state == CENTRAL_EXCHANGE_OOB_DATA) {
        sc = collect_part(
          remote_oob,
          &remote_oob_len,
          sizeof(remote_oob),
          evt->data.evt_gatt_characteristic_value.offset,
          evt->data.evt_gatt_characteristic_value.value.data,
          evt->data.evt_gatt_characteristic_value.value.len);
      } else {
        break;
      }

      if (sc != SL_STATUS_OK) {
        on_error(sc);
      }
      break;

    // -------------------------------
    // This event indicates that the remote GATT client is reading a
    // characteristic handled by the application.
    case sl_bt_evt_gatt_server_user_read_request_id:
      serve_user_read(&evt->data.evt_gatt_server_user_read_request);
      break;

    // -------------------------------
    // This event indicates that the remote GATT client is writing a
    // characteristic handled by the application.
    case sl_bt_evt_gatt_server_user_write_request_id:
      receive_user_write(&evt->data.evt_gatt_server_user_write_request);
      break;

    // -------------------------------
    // This event reports a certificate indication subscription or its
    // confirmation. The peripheral advances one indication at a time.
    case sl_bt_evt_gatt_server_characteristic_status_id:
      handle_cert_indication_status(
        &evt->data.evt_gatt_server_characteristic_status);
      break;

    // -------------------------------
    // This event indicates that the remote device requests bonding. Every
    // request has to be confirmed, which is how a remote device that has not
    // been authenticated yet is kept from pairing.
    case sl_bt_evt_sm_confirm_bonding_id: {
      uint8_t connection = evt->data.evt_sm_confirm_bonding.connection;
      bool accept = (connection == candidate_device.handle)
                    && procedure_complete();

      if (!accept) {
        CBAP_LOG_ERROR("Rejecting the bonding request of connection %d, " \
                       "CBAP has not authenticated it." CBAP_LOG_NL,
                       connection);
      }

      sc = sl_bt_sm_bonding_confirm(connection, accept ? 1 : 0);
      if (sc != SL_STATUS_OK) {
        CBAP_LOG_ERROR("Failed to answer the bonding request: " \
                       "0x%04lx!" CBAP_LOG_NL, sc);
      }

      // A remote device that tries to pair too early is not trustworthy.
      if (!accept && connection == candidate_device.handle) {
        on_error(SL_STATUS_INVALID_STATE);
      }
      break;
    }

    // -------------------------------
    // This event indicates that the pairing failed. A Full-role device requires
    // the OOB exchange to be mutual, and the stack rejects the pairing if the
    // remote device did not consume the OOB data of this one. That happens when
    // the remote device does not verify, so when the two roles do not fit.
    case sl_bt_evt_sm_bonding_failed_id:
      if (evt->data.evt_sm_bonding_failed.connection
          == candidate_device.handle) {
        CBAP_LOG_ERROR("Pairing failed: 0x%04lx!" CBAP_LOG_NL,
                       (unsigned long)evt->data.evt_sm_bonding_failed.reason);
        on_error((sl_status_t)evt->data.evt_sm_bonding_failed.reason);
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

      CBAP_LOG_DEBUG("Security mode: %d" CBAP_LOG_NL,
                     evt->data.evt_connection_parameters.security_mode);

      if (evt->data.evt_connection_parameters.security_mode
          == sl_bt_connection_mode1_level1) {
        // The link is not secured, so this is a plain parameter update.
        break;
      }

      // Securing the link is the last step of the procedure. If it happens
      // before the local role is done, the remote device was not authenticated,
      // so the connection cannot be trusted.
      if (!procedure_complete()) {
        CBAP_LOG_ERROR("The security level was raised before CBAP could " \
                       "authenticate the remote device!" CBAP_LOG_NL);
        on_error(SL_STATUS_INVALID_STATE);
        break;
      }

      if (evt->data.evt_connection_parameters.security_mode
          != sl_bt_connection_mode1_level4) {
        // Anything below Secure Connections pairing with an authenticated key
        // means that the pairing did not run the OOB association model, so the
        // session keys are not tied to the certificate chain. The stack is
        // configured to rule this out, this is the second line of defence.
        CBAP_LOG_ERROR("The link was secured in mode %d instead of "        \
                       "mode 1 level 4, the keys are not authenticated by " \
                       "CBAP!" CBAP_LOG_NL,
                       evt->data.evt_connection_parameters.security_mode);
        on_error(SL_STATUS_INVALID_CREDENTIALS);
        break;
      }

      // Secure Connections pairing with an authenticated key. The OOB
      // association model tied it to the validated certificate chain.
      {
        sl_bt_cbap_conn_t completed_device = candidate_device;
        sl_status_t timer_sc = app_timer_stop(&timer);
        app_assert_status(timer_sc);

        central_state = CENTRAL_DONE;
        CBAP_LOG_INFO("CBAP procedure complete." CBAP_LOG_NL);

        // The connection is authenticated, so it belongs to the application
        // from here on. Release CBAP before the callback: the application can
        // safely start looking for its next candidate from that callback.
        candidate_connected = false;
        candidate_device.handle = SL_BT_INVALID_CONNECTION_HANDLE;
        for (uint8_t i = 0; i < sizeof(bd_addr); i++) {
          candidate_device.address.addr[i] = 0xff;
        }
        reset_procedure();

        if (callback != NULL) {
          callback(completed_device, SL_STATUS_OK);
        }
      }
      break;

    default:
      break;
  }
}

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
  CBAP_LOG_DEBUG("Discovering CBAP service." CBAP_LOG_NL);

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
  CBAP_LOG_DEBUG("Discovering characteristic %d." CBAP_LOG_NL, char_state);

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

    if (desc->server && IS_VERIFIER) {
      // The peripheral/server presents its certificate through an indication
      // stream. Each indication is framed below the ATT value limit.
      cert_len = 0;
      cert_char = char_state;
      cert_receiving = true;
      central_cert_operation = CENTRAL_CERT_OPERATION_ENABLE_INDICATIONS;
      sc = sl_bt_gatt_set_characteristic_notification(
        candidate_device.handle,
        cbap_char_handles[char_state],
        sl_bt_gatt_indication);
      if (sc == SL_STATUS_OK) {
        CBAP_LOG_INFO("Requesting the %s certificate of the remote " \
                      "device." CBAP_LOG_NL,
                      remote_cert_names[desc->remote_cert]);
      }
      return sc;
    }

    if (!desc->server && IS_PROVER) {
      // Send our own (central/client) certificate to the peripheral.
      sc = sli_bt_cbap_get_certificate(desc->local_id,
                                       cert_buf,
                                       &cert_len,
                                       sizeof(cert_buf));
      if (sc != SL_STATUS_OK) {
        CBAP_LOG_ERROR("Failed to load the local %s certificate: " \
                       "0x%04lx!" CBAP_LOG_NL,
                       remote_cert_names[desc->remote_cert],
                       sc);
        return sc;
      }

      cert_char = char_state;
      cert_send_offset = 0;
      cert_sending = true;
      central_cert_operation = CENTRAL_CERT_OPERATION_WRITE_FRAGMENT;
      sc = send_certificate_write(char_state);
      if (sc == SL_STATUS_OK) {
        CBAP_LOG_INFO("Sending the local %s certificate " \
                      "(%u bytes)." CBAP_LOG_NL,
                      remote_cert_names[desc->remote_cert],
                      (unsigned int)cert_len);
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
  CBAP_LOG_INFO("Certificate exchange complete." CBAP_LOG_NL);

  // The certificates have to lead back to the trust hierarchy of this device
  // before the procedure continues. The caller closes the connection if they do
  // not. Whether the remote device owns them is settled later, by the OOB data.
  sc = validate_remote_cert_chain();
  if (sc != SL_STATUS_OK) {
    return sc;
  }

  // Continue with the OOB data exchange. char_state already selects the first
  // OOB characteristic, as that is where the certificate walk stopped.
  central_state = CENTRAL_EXCHANGE_OOB_DATA;
  return continue_oob_exchange();
}

/******************************************************************************
 * Build the next framed certificate fragment in cert_fragment.
 *
 * An ATT indication or Write Request carries at most ATT_MTU - 3 bytes in its
 * value. One byte is reserved for the CBAP continuation flag.
 *****************************************************************************/
static sl_status_t prepare_certificate_fragment(size_t *fragment_length)
{
  sl_status_t sc;
  uint16_t mtu;
  size_t payload_length;
  size_t maximum_payload;
  size_t remaining;

  if (!cert_sending || cert_send_offset >= cert_len) {
    return SL_STATUS_INVALID_STATE;
  }

  sc = sl_bt_gatt_server_get_mtu(candidate_device.handle, &mtu);
  if (sc != SL_STATUS_OK) {
    return sc;
  }
  if (mtu <= CERT_FRAGMENT_HEADER_LEN + 3U) {
    return SL_STATUS_INVALID_PARAMETER;
  }

  maximum_payload = (size_t)mtu - 3U - CERT_FRAGMENT_HEADER_LEN;
  if (maximum_payload > ATT_ATTRIBUTE_VALUE_MAX_LENGTH
      - CERT_FRAGMENT_HEADER_LEN) {
    maximum_payload = ATT_ATTRIBUTE_VALUE_MAX_LENGTH
                      - CERT_FRAGMENT_HEADER_LEN;
  }
  if (maximum_payload > sizeof(cert_fragment) - CERT_FRAGMENT_HEADER_LEN) {
    maximum_payload = sizeof(cert_fragment) - CERT_FRAGMENT_HEADER_LEN;
  }

  remaining = cert_len - cert_send_offset;
  payload_length = (remaining < maximum_payload) ? remaining : maximum_payload;
  cert_fragment[0] = (remaining > payload_length)
                     ? CERT_FRAGMENT_MORE : CERT_FRAGMENT_LAST;
  memcpy(&cert_fragment[CERT_FRAGMENT_HEADER_LEN],
         &cert_buf[cert_send_offset],
         payload_length);
  cert_send_offset += payload_length;
  *fragment_length = payload_length + CERT_FRAGMENT_HEADER_LEN;

  return SL_STATUS_OK;
}

/******************************************************************************
 * Send the next certificate fragment from the local GATT server.
 *****************************************************************************/
static sl_status_t send_certificate_indication(uint8_t index)
{
  sl_status_t sc;
  size_t fragment_length;

  if (index >= CERT_CHAR_COUNT || cert_char != index) {
    return SL_STATUS_INVALID_STATE;
  }

  sc = prepare_certificate_fragment(&fragment_length);
  if (sc != SL_STATUS_OK) {
    return sc;
  }

  sc = sl_bt_gatt_server_send_indication(candidate_device.handle,
                                         cert_chars[index].local_handle,
                                         fragment_length,
                                         cert_fragment);
  if (sc == SL_STATUS_OK) {
    CBAP_LOG_DEBUG("Sent %u bytes of the local %s certificate." CBAP_LOG_NL,
                   (unsigned int)(fragment_length - CERT_FRAGMENT_HEADER_LEN),
                   remote_cert_names[cert_chars[index].remote_cert]);
  }

  return sc;
}

/******************************************************************************
 * Send the next certificate fragment from the central GATT client.
 *****************************************************************************/
static sl_status_t send_certificate_write(uint8_t index)
{
  sl_status_t sc;
  size_t fragment_length;

  if (index >= CERT_CHAR_COUNT || cert_char != index) {
    return SL_STATUS_INVALID_STATE;
  }

  sc = prepare_certificate_fragment(&fragment_length);
  if (sc != SL_STATUS_OK) {
    return sc;
  }

  sc = sl_bt_gatt_write_characteristic_value(candidate_device.handle,
                                             cbap_char_handles[index],
                                             fragment_length,
                                             cert_fragment);
  if (sc == SL_STATUS_OK) {
    CBAP_LOG_DEBUG("Sent %u bytes of the local %s certificate." CBAP_LOG_NL,
                   (unsigned int)(fragment_length - CERT_FRAGMENT_HEADER_LEN),
                   remote_cert_names[cert_chars[index].remote_cert]);
  }

  return sc;
}

/******************************************************************************
 * Find the certificate characteristic by its handle in the local GATT database.
 *
 * @return Index of the characteristic, CERT_CHAR_COUNT if there is no
 *         certificate characteristic with the given handle and direction.
 *****************************************************************************/
static uint8_t find_cert_char(uint16_t local_handle, bool server)
{
  uint8_t index;

  for (index = 0; index < CERT_CHAR_COUNT; index++) {
    if (cert_chars[index].local_handle == local_handle
        && cert_chars[index].server == server) {
      break;
    }
  }

  return index;
}

/******************************************************************************
 * Collect a part of a value being received into the given buffer.
 *
 * Serves both the certificate and the OOB data transfers, each of which keeps
 * its own buffer and length.
 *
 * @param[in] buf Buffer collecting the parts.
 * @param[in,out] buf_len Number of bytes already collected, advanced by the
 *   length of the part on success.
 * @param[in] capacity Size of @p buf in bytes.
 * @param[in] offset Offset the part was received at.
 * @param[in] data The part received.
 * @param[in] len Length of @p data.
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
    CBAP_LOG_ERROR("A part arrived at offset %u instead of %u." CBAP_LOG_NL,
                   (unsigned int)offset,
                   (unsigned int)*buf_len);
    return SL_STATUS_INVALID_STATE;
  }

  if (len > capacity - *buf_len) {
    CBAP_LOG_ERROR("The value of the remote device is longer than " \
                   "%u bytes." CBAP_LOG_NL,
                   (unsigned int)capacity);
    return SL_STATUS_WOULD_OVERFLOW;
  }

  memcpy(&buf[*buf_len], data, len);
  *buf_len += len;

  return SL_STATUS_OK;
}

/******************************************************************************
 * Write the certificate collected in cert_buf to the persistent storage.
 *****************************************************************************/
static sl_status_t store_remote_cert(uint8_t index)
{
  sl_status_t sc;
  remote_cert_t remote_cert = cert_chars[index].remote_cert;

  if (cert_len == 0) {
    CBAP_LOG_ERROR("The remote device presented an empty %s " \
                   "certificate." CBAP_LOG_NL,
                   remote_cert_names[remote_cert]);
    return SL_STATUS_EMPTY;
  }

  sc = sli_bt_cbap_set_certificate(cert_chars[index].remote_id,
                                   cert_buf,
                                   cert_len);
  if (sc != SL_STATUS_OK) {
    return sc;
  }

  remote_cert_stored[remote_cert] = true;
  CBAP_LOG_INFO("The %s certificate of the remote device is stored " \
                "(%u bytes)." CBAP_LOG_NL,
                remote_cert_names[remote_cert],
                (unsigned int)cert_len);

  cert_len = 0;
  cert_char = CERT_CHAR_COUNT;
  cert_receiving = false;

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
 *****************************************************************************/
static sl_status_t check_remote_certs(bool report)
{
  if (!IS_VERIFIER) {
    return SL_STATUS_OK;
  }

  for (uint8_t i = 0; i < REMOTE_CERT_COUNT; i++) {
    if (!remote_cert_stored[i]) {
      if (report) {
        CBAP_LOG_ERROR("The %s certificate of the remote device is " \
                       "missing." CBAP_LOG_NL,
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
 * is what the OOB data it signs is for.
 *
 * @note Call only once every required remote certificate has been stored.
 *
 * @return SL_STATUS_OK if the chain is valid, error code otherwise.
 *****************************************************************************/
static sl_status_t validate_remote_cert_chain(void)
{
  if (!IS_VERIFIER) {
    return SL_STATUS_OK;
  }

  sl_status_t sc;
  sc = sli_bt_cbap_validate_remote_chain();
  if (sc != SL_STATUS_OK) {
    CBAP_LOG_ERROR("The certificate chain of the remote device is invalid, " \
                   "it does not belong to the trust hierarchy of this "      \
                   "device!" CBAP_LOG_NL);
    return sc;
  }

  remote_chain_valid = true;
  CBAP_LOG_INFO("The certificate chain of the remote device is " \
                "valid." CBAP_LOG_NL);
  return SL_STATUS_OK;
}

/******************************************************************************
 * Generate the OOB data of this pairing, and sign it if this device can.
 *
 * Enabling OOB makes the stack generate the random and the confirm value of the
 * pairing, and discards any OOB data that was set for a remote device earlier.
 * It is therefore done exactly once per procedure, and before the data of the
 * remote device is passed to the stack. Generating the values again would
 * invalidate the confirm value the remote device may already have received.
 *
 * A prover signs the values with its device key. That signature is what proves
 * to the remote device that this device owns the certificate chain it
 * presented. A verifier only device has no device key, so it leaves the
 * generated values unsigned and unsent, and only enables OOB so that it is
 * able to accept the data of the remote device.
 *****************************************************************************/
static sl_status_t ensure_local_oob(void)
{
  if (local_oob_ready) {
    return SL_STATUS_OK;
  }

  sl_status_t sc;
  aes_key_128 random;
  aes_key_128 confirm;

  sc = sl_bt_sm_set_oob(1, &random, &confirm);
  if (sc != SL_STATUS_OK) {
    CBAP_LOG_ERROR("Failed to generate the local OOB data: " \
                   "0x%04lx!" CBAP_LOG_NL, sc);
    return sc;
  }

  if (IS_PROVER) {
    sc = sli_bt_cbap_sign_oob_data(random.data,
                                   confirm.data,
                                   local_oob,
                                   &local_oob_len);
    if (sc != SL_STATUS_OK) {
      CBAP_LOG_ERROR("Failed to sign the local OOB data: " \
                     "0x%04lx!" CBAP_LOG_NL, sc);
      local_oob_len = 0;
      return sc;
    }
  }

  local_oob_ready = true;

  return SL_STATUS_OK;
}

/******************************************************************************
 * Verify the OOB data collected from the remote device and pass it to the
 * stack.
 *
 * The signature is verified with the public key of the device certificate that
 * the validated certificate chain of the remote device ends with. The
 * confirm value commits the remote device to the public key it uses in this
 * pairing, so a valid signature means that the pairing runs with the very
 * device that owns the chain, and not with someone relaying it.
 *****************************************************************************/
static sl_status_t process_remote_oob(void)
{
  sl_status_t sc;
  aes_key_128 random;
  aes_key_128 confirm;

  if (remote_oob_len != SL_BT_CBAP_SIGNED_OOB_DATA_LEN) {
    CBAP_LOG_ERROR("The remote device presented %u bytes of OOB data " \
                   "instead of %u." CBAP_LOG_NL,
                   (unsigned int)remote_oob_len,
                   (unsigned int)SL_BT_CBAP_SIGNED_OOB_DATA_LEN);
    return SL_STATUS_INVALID_COUNT;
  }

  // The stack only accepts the data of the remote device once OOB is enabled
  // locally. That is already done if this device presented its own data first,
  // but a device that only receives has not reached it yet.
  sc = ensure_local_oob();
  if (sc != SL_STATUS_OK) {
    return sc;
  }

  memcpy(random.data, remote_oob, SL_BT_CBAP_OOB_RANDOM_LEN);
  memcpy(confirm.data,
         &remote_oob[SL_BT_CBAP_OOB_RANDOM_LEN],
         SL_BT_CBAP_OOB_RANDOM_LEN);

  sc = sli_bt_cbap_verify_remote_oob_data(
    random.data,
    confirm.data,
    &remote_oob[SL_BT_CBAP_OOB_DATA_LEN]);
  if (sc != SL_STATUS_OK) {
    CBAP_LOG_ERROR("The OOB data of the remote device does not match the " \
                   "device certificate it presented!" CBAP_LOG_NL);
    return sc;
  }

  // The stack checks the confirm value against the public key that the remote
  // device uses in the pairing, which is what carries the authentication over
  // into the encrypted link.
  sc = sl_bt_sm_set_remote_oob(1, random, confirm);
  if (sc != SL_STATUS_OK) {
    CBAP_LOG_ERROR("Failed to set the OOB data of the remote device: " \
                   "0x%04lx!" CBAP_LOG_NL, sc);
    return sc;
  }

  remote_oob_verified = true;
  CBAP_LOG_INFO("The OOB data of the remote device is verified." CBAP_LOG_NL);

  // The public key of the remote device has served its purpose.
  return sli_destroy_remote_pub_key();
}

/******************************************************************************
 * Check that the OOB data of the remote device arrived if it is required.
 *
 * A verifier authenticates the remote device by verifying the OOB data it
 * signed, so the procedure cannot succeed without it. A prover only device
 * cannot verify anything and expects no OOB data in return.
 *****************************************************************************/
static sl_status_t check_remote_oob(void)
{
  if (IS_VERIFIER && !remote_oob_verified) {
    CBAP_LOG_ERROR("The remote device presented no OOB data to prove that " \
                   "it owns the certificates it sent!" CBAP_LOG_NL);
    return SL_STATUS_NOT_FOUND;
  }

  return SL_STATUS_OK;
}

/******************************************************************************
 * True if this device has done everything its role requires of it, so that the
 * security level may be increased.
 *
 * A verifier has to have authenticated the remote device: its certificate chain
 * validated and the OOB data it signed verified. A prover has nothing to
 * verify, it only has to have presented its own signed OOB data.
 *****************************************************************************/
static bool procedure_complete(void)
{
  if (IS_VERIFIER
      && (check_remote_certs(false) != SL_STATUS_OK
          || !remote_chain_valid
          || !remote_oob_verified)) {
    return false;
  }

  return local_oob_ready;
}

/******************************************************************************
 * Continue the OOB data exchange on the characteristic selected by char_state,
 * or finish it if there is none left.
 *
 * The central drives the exchange the same way as the certificate exchange. It
 * reads the OOB data of the peripheral if it has to verify it, and writes its
 * own if it has a signature to prove itself with. Between two Full-role devices
 * both directions are used, otherwise only the one that ends at the verifier.
 *****************************************************************************/
static sl_status_t continue_oob_exchange(void)
{
  sl_status_t sc;

  // The values of this pairing have to exist before the data of the remote
  // device can be accepted, and before there is anything to sign and send.
  sc = ensure_local_oob();
  if (sc != SL_STATUS_OK) {
    return sc;
  }

  while (char_state < SL_BT_CBAP_CHAR_COUNT) {
    if (char_state == SL_BT_CBAP_CHAR_OOB_DATA_SERVER && IS_VERIFIER) {
      // Read the OOB data that the peripheral signed with its device key.
      remote_oob_len = 0;
      sc = sl_bt_gatt_read_characteristic_value(
        candidate_device.handle,
        cbap_char_handles[char_state]);
      if (sc == SL_STATUS_OK) {
        CBAP_LOG_INFO("Reading the OOB data of the remote " \
                      "device." CBAP_LOG_NL);
      }
      return sc;
    }

    if (char_state == SL_BT_CBAP_CHAR_OOB_DATA_CLIENT && IS_PROVER) {
      // Send our own signed OOB data to the peripheral.
      sc = sl_bt_gatt_write_characteristic_value(
        candidate_device.handle,
        cbap_char_handles[char_state],
        local_oob_len,
        local_oob);
      if (sc == SL_STATUS_OK) {
        CBAP_LOG_INFO("Sending the signed OOB data of this device " \
                      "(%u bytes)." CBAP_LOG_NL,
                      (unsigned int)local_oob_len);
      }
      return sc;
    }

    // The local role takes no part in the transfer on this characteristic.
    char_state++;
  }

  // Both directions have been walked.
  sc = check_remote_oob();
  if (sc != SL_STATUS_OK) {
    return sc;
  }
  CBAP_LOG_INFO("OOB data exchange complete." CBAP_LOG_NL);

  // Everything the local role requires is done, so the link can be secured.
  // The pairing uses the OOB association model, which ties the session keys to
  // the certificate chain that was validated above.
  sc = sl_bt_sm_increase_security(candidate_device.handle);
  if (sc != SL_STATUS_OK) {
    CBAP_LOG_ERROR("Failed to increase the security level: " \
                   "0x%04lx!" CBAP_LOG_NL, sc);
    return sc;
  }

  central_state = CENTRAL_INCREASE_SECURITY;
  CBAP_LOG_INFO("Increasing security level." CBAP_LOG_NL);

  return SL_STATUS_OK;
}

/******************************************************************************
 * Get the number of bytes that fit in a user-read response for this request.
 *****************************************************************************/
static sl_status_t get_read_response_length(const user_read_request_t *req,
                                            size_t remaining,
                                            size_t *response_length)
{
  sl_status_t sc;
  uint16_t mtu;
  size_t maximum_length;

  sc = sl_bt_gatt_server_get_mtu(req->connection, &mtu);
  if (sc != SL_STATUS_OK) {
    return sc;
  }

  // A read response carries ATT_MTU - 1 bytes, except a Read By Type response
  // which also contains the attribute handle and therefore carries ATT_MTU - 4.
  maximum_length = (size_t)mtu
                   - ((req->att_opcode == sl_bt_gatt_read_by_type_request)
                      ? 4U : 1U);
  *response_length = (remaining < maximum_length) ? remaining : maximum_length;

  return SL_STATUS_OK;
}

/******************************************************************************
 * Handle a write request of the remote device on a certificate characteristic.
 *
 * A fragment starts with a CBAP continuation flag, followed by certificate
 * bytes. The final flag lets certificates larger than an ATT value travel as a
 * sequence of ordinary Write Requests rather than an unsupported Write Long.
 *****************************************************************************/
static void receive_cert_write(const user_write_request_t *req, uint8_t index)
{
  sl_status_t sc = SL_STATUS_OK;
  uint8_t att_error = ATT_SUCCESS;
  bool complete = false;
  uint8_t more = CERT_FRAGMENT_LAST;

  // A prover only device has no trust anchor to validate a certificate
  // against, so it accepts what it cannot use and drops it. Failing the
  // transfer would be misleading, as nothing is wrong with the certificate
  // itself. Whether the two roles fit together at all is decided by the
  // pairing requirements once the procedure reaches that point.
  bool discard = !IS_VERIFIER;

  if (req->att_opcode != sl_bt_gatt_write_request
      || req->offset != 0
      || req->value.len < CERT_FRAGMENT_HEADER_LEN) {
    att_error = (uint8_t)SL_STATUS_BT_ATT_REQUEST_NOT_SUPPORTED;
  } else if (cert_sending) {
    // cert_buf holds the local certificate being indicated. Do not let an
    // incoming write replace it before the client confirms the final fragment.
    att_error = (uint8_t)SL_STATUS_BT_ATT_PROCEDURE_ALREADY_IN_PROGRESS;
  } else {
    more = req->value.data[0];
    if (more != CERT_FRAGMENT_MORE && more != CERT_FRAGMENT_LAST) {
      att_error = (uint8_t)SL_STATUS_BT_ATT_INVALID_ATT_LENGTH;
    } else if (!discard) {
      if (!cert_receiving) {
        cert_len = 0;
        cert_char = index;
        cert_receiving = true;
      }

      if (cert_char != index) {
        CBAP_LOG_ERROR("The remote device interleaved the transfer of its " \
                       "%s certificate with another one." CBAP_LOG_NL,
                       remote_cert_names[cert_chars[index].remote_cert]);
        att_error = (uint8_t)SL_STATUS_BT_ATT_PROCEDURE_ALREADY_IN_PROGRESS;
      } else {
        sc = collect_part(cert_buf,
                          &cert_len,
                          sizeof(cert_buf),
                          cert_len,
                          &req->value.data[CERT_FRAGMENT_HEADER_LEN],
                          req->value.len - CERT_FRAGMENT_HEADER_LEN);
        if (sc == SL_STATUS_WOULD_OVERFLOW) {
          att_error = (uint8_t)SL_STATUS_BT_ATT_INSUFFICIENT_RESOURCES;
        } else if (sc != SL_STATUS_OK) {
          att_error = (uint8_t)SL_STATUS_BT_ATT_INVALID_OFFSET;
        } else {
          CBAP_LOG_DEBUG("Received %u bytes of the remote %s certificate " \
                         "(%u total)." CBAP_LOG_NL,
                         (unsigned int)(req->value.len
                                        - CERT_FRAGMENT_HEADER_LEN),
                         remote_cert_names[cert_chars[index].remote_cert],
                         (unsigned int)cert_len);
        }
      }
    }

    complete = (att_error == ATT_SUCCESS && more == CERT_FRAGMENT_LAST);
  }

  if (complete) {
    if (discard) {
      CBAP_LOG_DEBUG("Discarded the %s certificate of the remote device, " \
                     "this device cannot validate it." CBAP_LOG_NL,
                     remote_cert_names[cert_chars[index].remote_cert]);
    } else {
      sc = store_remote_cert(index);
      if (sc != SL_STATUS_OK) {
        att_error = (uint8_t)SL_STATUS_BT_ATT_WRITE_REQUEST_REJECTED;
      }
    }
  }

  // A rejected write is reported to the central, which cannot complete the
  // procedure without the response either.
  sc = sl_bt_gatt_server_send_user_write_response(req->connection,
                                                  req->characteristic,
                                                  att_error);
  if (sc != SL_STATUS_OK) {
    CBAP_LOG_ERROR("Failed to send the write response: " \
                   "0x%04lx!" CBAP_LOG_NL, sc);
    on_error(sc);
    return;
  }

  // Every certificate the peripheral has to verify may have arrived by now. It
  // cannot tell when the central is done, so it checks on every stored one.
  if (!is_central && att_error == ATT_SUCCESS
      && check_remote_certs(false) == SL_STATUS_OK) {
    CBAP_LOG_INFO("Certificate exchange complete." CBAP_LOG_NL);

    // The write is already confirmed at this point, so a remote device whose
    // certificates do not check out is rejected by closing the connection.
    sc = validate_remote_cert_chain();
    if (sc != SL_STATUS_OK) {
      on_error(sc);
    }
  }
}

/******************************************************************************
 * Advance the indication stream that presents a local certificate.
 *
 * The first fragment follows the CCCD update. Every later fragment waits for
 * the confirmation of the preceding indication, as required by ATT.
 *****************************************************************************/
static void handle_cert_indication_status(
  const sl_bt_evt_gatt_server_characteristic_status_t *status)
{
  sl_status_t sc;
  uint8_t index;

  if (status->connection != candidate_device.handle || is_central) {
    return;
  }

  index = find_cert_char(status->characteristic, true);
  if (index >= CERT_CHAR_COUNT) {
    return;
  }

  if (status->status_flags == sl_bt_gatt_server_client_config) {
    if ((status->client_config_flags & sl_bt_gatt_server_indication) == 0U) {
      if (cert_sending && cert_char == index) {
        CBAP_LOG_ERROR("The remote device disabled the %s certificate " \
                       "indications before the transfer completed."     \
                       CBAP_LOG_NL,
                       remote_cert_names[cert_chars[index].remote_cert]);
        on_error(SL_STATUS_ABORT);
      }
      return;
    }

    if (!IS_PROVER || cert_receiving || cert_sending) {
      CBAP_LOG_ERROR("The remote device requested a certificate indication " \
                     "at an invalid time." CBAP_LOG_NL);
      on_error(SL_STATUS_INVALID_STATE);
      return;
    }

    sc = sli_bt_cbap_get_certificate(cert_chars[index].local_id,
                                     cert_buf,
                                     &cert_len,
                                     sizeof(cert_buf));
    if (sc != SL_STATUS_OK || cert_len == 0) {
      CBAP_LOG_ERROR("Failed to load the local %s certificate: 0x%04lx!" \
                     CBAP_LOG_NL,
                     remote_cert_names[cert_chars[index].remote_cert],
                     (sc == SL_STATUS_OK) ? SL_STATUS_EMPTY : sc);
      on_error((sc == SL_STATUS_OK) ? SL_STATUS_EMPTY : sc);
      return;
    }

    cert_char = index;
    cert_send_offset = 0;
    cert_sending = true;
    CBAP_LOG_INFO("Presenting the local %s certificate (%u bytes)." \
                  CBAP_LOG_NL,
                  remote_cert_names[cert_chars[index].remote_cert],
                  (unsigned int)cert_len);
    sc = send_certificate_indication(index);
  } else if (status->status_flags == sl_bt_gatt_server_confirmation) {
    if (!cert_sending || cert_char != index) {
      return;
    }

    if (cert_send_offset == cert_len) {
      cert_sending = false;
      cert_send_offset = 0;
      cert_len = 0;
      cert_char = CERT_CHAR_COUNT;
      CBAP_LOG_DEBUG("Finished sending the local %s certificate." CBAP_LOG_NL,
                     remote_cert_names[cert_chars[index].remote_cert]);
      return;
    }

    sc = send_certificate_indication(index);
  } else {
    return;
  }

  if (sc != SL_STATUS_OK) {
    CBAP_LOG_ERROR("Failed to send the local %s certificate fragment: " \
                   "0x%04lx!" CBAP_LOG_NL,
                   remote_cert_names[cert_chars[index].remote_cert],
                   sc);
    on_error(sc);
  }
}

/******************************************************************************
 * Serve a read request of the remote device on the OOB data characteristic.
 *
 * The data is generated and signed when the remote device starts reading it at
 * offset zero, and the consecutive reads of the same transfer are served from
 * local_oob. Each response is limited to the negotiated ATT MTU, and the rest
 * is requested by the remote device at the next offset.
 *****************************************************************************/
static void serve_oob_read(const user_read_request_t *req)
{
  sl_status_t sc;
  uint8_t att_error = ATT_SUCCESS;
  size_t response_length = 0;
  uint16_t sent_length;

  if (!IS_PROVER) {
    // Without a device key there is nothing to sign the OOB data with, and
    // unsigned OOB data would prove nothing to the remote device.
    CBAP_LOG_ERROR("The remote device is reading OOB data that this device " \
                   "cannot sign." CBAP_LOG_NL);
    att_error = (uint8_t)SL_STATUS_BT_ATT_REQUEST_NOT_SUPPORTED;
  } else if (req->offset == 0) {
    // Start of a transfer, generate and sign the data to be sent.
    sc = ensure_local_oob();
    if (sc != SL_STATUS_OK) {
      att_error = (uint8_t)SL_STATUS_BT_ATT_UNLIKELY_ERROR;
    } else {
      CBAP_LOG_INFO("Presenting the signed OOB data of this device " \
                    "(%u bytes)." CBAP_LOG_NL,
                    (unsigned int)local_oob_len);
    }
  } else if (req->offset > local_oob_len) {
    // The remote device continues a transfer that was not started, or reads
    // past the end of the data.
    CBAP_LOG_ERROR("Invalid read offset %u on the OOB data." CBAP_LOG_NL,
                   (unsigned int)req->offset);
    att_error = (uint8_t)SL_STATUS_BT_ATT_INVALID_OFFSET;
  }

  if (att_error == ATT_SUCCESS) {
    sc = get_read_response_length(req,
                                  local_oob_len - req->offset,
                                  &response_length);
    if (sc != SL_STATUS_OK) {
      CBAP_LOG_ERROR("Failed to get the ATT MTU: 0x%04lx!" CBAP_LOG_NL, sc);
      on_error(sc);
      return;
    }
  }

  // On a rejected read the remote device gets an empty response carrying the
  // error code. Aborting is left to the central, which cannot complete the
  // procedure without the OOB data either.
  sc = sl_bt_gatt_server_send_user_read_response(
    req->connection,
    req->characteristic,
    att_error,
    response_length,
    (att_error == ATT_SUCCESS) ? &local_oob[req->offset] : NULL,
    &sent_length);
  if (sc != SL_STATUS_OK) {
    CBAP_LOG_ERROR("Failed to send the read response: " \
                   "0x%04lx!" CBAP_LOG_NL, sc);
    on_error(sc);
  } else if (att_error == ATT_SUCCESS) {
    CBAP_LOG_INFO("Sent %u bytes of signed OOB data at offset %u." \
                  CBAP_LOG_NL,
                  (unsigned int)sent_length,
                  (unsigned int)req->offset);
  }
}

/******************************************************************************
 * Handle a write request of the remote device on the OOB data characteristic.
 *
 * OOB data that fits into a single ATT PDU arrives in one write request. A
 * longer one arrives in consecutive prepared writes that are collected until
 * the remote device executes them.
 *****************************************************************************/
static void receive_oob_write(const user_write_request_t *req)
{
  sl_status_t sc = SL_STATUS_OK;
  uint8_t att_error = ATT_SUCCESS;
  bool complete = false;

  // A prover only device cannot verify a signature, as it has no trust anchor
  // to authenticate the remote device with. It accepts what it cannot use and
  // drops it, which leaves the OOB data of the remote device unconsumed. A
  // remote device that requires the exchange to be mutual notices exactly that
  // and fails the pairing, so the roles are matched up where it matters.
  bool discard = !IS_VERIFIER;

  if (req->att_opcode == sl_bt_gatt_execute_write_request) {
    // The collected parts are complete.
    complete = true;
  } else {
    if (!discard) {
      if (req->offset == 0) {
        // Start of a transfer, discard what an aborted one has left behind.
        remote_oob_len = 0;
      }

      sc = collect_part(remote_oob,
                        &remote_oob_len,
                        sizeof(remote_oob),
                        req->offset,
                        req->value.data,
                        req->value.len);
      if (sc == SL_STATUS_WOULD_OVERFLOW) {
        att_error = (uint8_t)SL_STATUS_BT_ATT_INSUFFICIENT_RESOURCES;
      } else if (sc != SL_STATUS_OK) {
        att_error = (uint8_t)SL_STATUS_BT_ATT_INVALID_OFFSET;
      }
    }

    if (req->att_opcode == sl_bt_gatt_prepare_write_request) {
      // More parts are on the way, the data is verified on the execute write.
      // The response echoes the collected part.
      sc = sl_bt_gatt_server_send_user_prepare_write_response(
        req->connection,
        req->characteristic,
        att_error,
        req->offset,
        req->value.len,
        req->value.data);
      if (sc != SL_STATUS_OK) {
        CBAP_LOG_ERROR("Failed to send the prepare write response: " \
                       "0x%04lx!" CBAP_LOG_NL, sc);
        on_error(sc);
      }
      return;
    }

    // The OOB data arrived in a single write.
    complete = (att_error == ATT_SUCCESS);
  }

  if (complete) {
    if (discard) {
      CBAP_LOG_DEBUG("Discarded the OOB data of the remote device, this " \
                     "device cannot verify it." CBAP_LOG_NL);
    } else {
      // The signature has to match the validated device certificate of the
      // remote device. It is the proof that the remote device owns the private
      // key of the certificate chain it presented.
      sc = process_remote_oob();
      if (sc != SL_STATUS_OK) {
        att_error = (uint8_t)SL_STATUS_BT_ATT_WRITE_REQUEST_REJECTED;
      }
    }
  }

  sc = sl_bt_gatt_server_send_user_write_response(req->connection,
                                                  req->characteristic,
                                                  att_error);
  if (sc != SL_STATUS_OK) {
    CBAP_LOG_ERROR("Failed to send the write response: " \
                   "0x%04lx!" CBAP_LOG_NL, sc);
    on_error(sc);
    return;
  }

  if (complete && att_error != ATT_SUCCESS) {
    // The remote device failed to prove itself. The write is rejected above,
    // and the connection is dropped so that it cannot try again.
    on_error(SL_STATUS_INVALID_SIGNATURE);
  }
}

/******************************************************************************
 * Refuse a write request of the remote device with the given ATT error.
 *
 * A prepared write has a response of its own, which normally echoes the part
 * that was collected. A refused one has nothing to echo.
 *****************************************************************************/
static void refuse_write(const user_write_request_t *req, uint8_t att_error)
{
  sl_status_t sc;

  if (req->att_opcode == sl_bt_gatt_prepare_write_request) {
    sc = sl_bt_gatt_server_send_user_prepare_write_response(req->connection,
                                                            req->characteristic,
                                                            att_error,
                                                            req->offset,
                                                            0,
                                                            NULL);
  } else {
    sc = sl_bt_gatt_server_send_user_write_response(req->connection,
                                                    req->characteristic,
                                                    att_error);
  }

  if (sc != SL_STATUS_OK) {
    CBAP_LOG_ERROR("Failed to refuse the write: 0x%04lx!" CBAP_LOG_NL, sc);
  }
}

/******************************************************************************
 * Find the CBAP characteristic a read request of the remote device targets.
 *
 * @return Index of the characteristic, SL_BT_CBAP_CHAR_COUNT if the handle
 *         belongs to no CBAP characteristic that the remote device may read.
 *****************************************************************************/
static uint8_t resolve_read_target(uint16_t local_handle)
{
  if (local_handle == gattdb_oob_data_server) {
    return (uint8_t)SL_BT_CBAP_CHAR_OOB_DATA_SERVER;
  }

  return (uint8_t)SL_BT_CBAP_CHAR_COUNT;
}

/******************************************************************************
 * Find the CBAP characteristic a write request of the remote device targets.
 *
 * @return Index of the characteristic, SL_BT_CBAP_CHAR_COUNT if the handle
 *         belongs to no CBAP characteristic that the remote device may write.
 *****************************************************************************/
static uint8_t resolve_write_target(uint16_t local_handle)
{
  if (local_handle == gattdb_oob_data_client) {
    return (uint8_t)SL_BT_CBAP_CHAR_OOB_DATA_CLIENT;
  }

  uint8_t index = find_cert_char(local_handle, false);

  return (index < CERT_CHAR_COUNT) ? index : (uint8_t)SL_BT_CBAP_CHAR_COUNT;
}

/******************************************************************************
 * Route a read request of the remote device to the handler of its
 * characteristic.
 *
 * The CBAP characteristics carry no security requirement, as they have to be
 * reachable before the link is secured. Any connected device can therefore
 * request them, but only the one the procedure is running with is served. The
 * rest are refused instead of ignored, otherwise they would be left waiting
 * for a response that never comes.
 *****************************************************************************/
static void serve_user_read(const user_read_request_t *req)
{
  uint8_t index = resolve_read_target(req->characteristic);

  if (index == (uint8_t)SL_BT_CBAP_CHAR_COUNT) {
    // Not a CBAP characteristic, it belongs to the application.
    return;
  }

  if (req->connection != candidate_device.handle) {
    CBAP_LOG_ERROR("Refusing the CBAP read of connection %d, there is no " \
                   "procedure in progress with it." CBAP_LOG_NL,
                   req->connection);
    (void)sl_bt_gatt_server_send_user_read_response(
      req->connection,
      req->characteristic,
      (uint8_t)SL_STATUS_BT_ATT_INSUFFICIENT_AUTHORIZATION,
      0,
      NULL,
      NULL);
    return;
  }

  if (index == (uint8_t)SL_BT_CBAP_CHAR_OOB_DATA_SERVER) {
    serve_oob_read(req);
  }
}

/******************************************************************************
 * Route a write request of the remote device to the handler of its
 * characteristic.
 *
 * The fixed-size OOB data still accepts prepared writes for links using a
 * small ATT MTU. Certificate writes deliberately do not: they are framed
 * ordinary Write Requests. Requests that do not belong to the procedure in
 * progress are refused, see @ref serve_user_read.
 *****************************************************************************/
static void receive_user_write(const user_write_request_t *req)
{
  uint8_t index;

  if (req->att_opcode == sl_bt_gatt_execute_write_request) {
    index = prepared_write_char;
    prepared_write_char = (uint8_t)SL_BT_CBAP_CHAR_COUNT;
  } else {
    index = resolve_write_target(req->characteristic);
  }

  if (index == (uint8_t)SL_BT_CBAP_CHAR_COUNT) {
    // Not a CBAP characteristic, it belongs to the application.
    return;
  }

  if (req->connection != candidate_device.handle) {
    CBAP_LOG_ERROR("Refusing the CBAP write of connection %d, there is no " \
                   "procedure in progress with it." CBAP_LOG_NL,
                   req->connection);
    refuse_write(req, (uint8_t)SL_STATUS_BT_ATT_INSUFFICIENT_AUTHORIZATION);
    return;
  }

  if (index < CERT_CHAR_COUNT && req->att_opcode != sl_bt_gatt_write_request) {
    refuse_write(req, (uint8_t)SL_STATUS_BT_ATT_REQUEST_NOT_SUPPORTED);
    return;
  }

  // Verifying the OOB data of the remote device closes the exchange. Letting it
  // write again afterwards would let it authenticate with the certificates of
  // one identity and then leave those of another behind in the storage, which
  // is what the application reads the remote device back from.
  if (remote_oob_verified) {
    CBAP_LOG_ERROR("The remote device is still writing after it has been " \
                   "authenticated." CBAP_LOG_NL);
    refuse_write(req, (uint8_t)SL_STATUS_BT_ATT_WRITE_REQUEST_REJECTED);
    on_error(SL_STATUS_INVALID_STATE);
    return;
  }

  if (index == (uint8_t)SL_BT_CBAP_CHAR_OOB_DATA_CLIENT
      && req->att_opcode == sl_bt_gatt_prepare_write_request) {
    prepared_write_char = index;
  }

  if (index == (uint8_t)SL_BT_CBAP_CHAR_OOB_DATA_CLIENT) {
    receive_oob_write(req);
  } else {
    receive_cert_write(req, index);
  }
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

  // Certificate transfer. The certificates of the previous remote device are
  // left in the storage, they are overwritten as the new ones arrive.
  cert_len = 0;
  cert_char = CERT_CHAR_COUNT;
  cert_receiving = false;
  cert_send_offset = 0;
  cert_sending = false;
  prepared_write_char = (uint8_t)SL_BT_CBAP_CHAR_COUNT;
  central_cert_operation = CENTRAL_CERT_OPERATION_NONE;
  for (uint8_t i = 0; i < REMOTE_CERT_COUNT; i++) {
    remote_cert_stored[i] = false;
  }
  remote_chain_valid = false;

  // OOB data. The values are bound to a single pairing, so the next procedure
  // has to generate and sign new ones.
  local_oob_len = 0;
  remote_oob_len = 0;
  local_oob_ready = false;
  remote_oob_verified = false;

  // The OOB settings of the stack are global, not per connection, so the data
  // of the finished procedure is taken out of the way of the next pairing. The
  // values are of no use to the caller here, they are only written because the
  // command reports the generated ones.
  aes_key_128 random;
  aes_key_128 confirm;
  sl_status_t oob_sc = sl_bt_sm_set_oob(0, &random, &confirm);
  if (oob_sc != SL_STATUS_OK) {
    CBAP_LOG_ERROR("Failed to disable the OOB data of the stack: " \
                   "0x%04lx!" CBAP_LOG_NL, oob_sc);
  }

  // Release the public key of the previous remote device if the procedure ended
  // before the OOB data was verified.
  sl_status_t sc = sli_destroy_remote_pub_key();
  if (sc != SL_STATUS_OK) {
    CBAP_LOG_ERROR("Failed to release the key of the remote device: " \
                   "0x%04lx!" CBAP_LOG_NL, sc);
  }
}

/******************************************************************************
 * Timer callback.
 *****************************************************************************/
static void timer_cb(app_timer_t *handle, void *data)
{
  (void)handle;
  (void)data;

  CBAP_LOG_ERROR("Procedure timeout!" CBAP_LOG_NL);
  on_error(SL_STATUS_TIMEOUT);
}

/******************************************************************************
 * Error handler.
 *****************************************************************************/
static void on_error(sl_status_t sc)
{
  // Make sure timer is stopped
  sl_status_t timer_sc;
  timer_sc = app_timer_stop(&timer);
  app_assert_status(timer_sc);

  // Reset states and discovered handles
  reset_procedure();

  CBAP_LOG_ERROR("Error for connection %d. Status: 0x%04lx" CBAP_LOG_NL,
                 candidate_device.handle,
                 sc);

  // It is the responsibility of the module to close the suspicious connection
  if (candidate_connected) {
    CBAP_LOG_ERROR("Closing connection..." CBAP_LOG_NL);
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
