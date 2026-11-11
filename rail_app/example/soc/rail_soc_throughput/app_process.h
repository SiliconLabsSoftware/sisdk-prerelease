/***************************************************************************//**
 * @file
 * @brief app_process.h
 *******************************************************************************
 * # License
 * <b>Copyright 2018 Silicon Laboratories Inc. www.silabs.com</b>
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
#ifndef APP_PROCESS_H
#define APP_PROCESS_H

// -----------------------------------------------------------------------------
//                                   Includes
// -----------------------------------------------------------------------------
#include <stdint.h>
#include <stdbool.h>
#include "sl_rail.h"
#include "sl_common.h"
#include "sl_rail_ieee802154.h"

// -----------------------------------------------------------------------------
//                              Macros and Typedefs
// -----------------------------------------------------------------------------
/// Application role in the two-node throughput measurement
typedef enum {
  APP_ROLE_UNASSIGNED = 0,
  APP_ROLE_TRANSMITTER,
  APP_ROLE_RECEIVER,
} app_role_t;

/// Transmitter state machine
typedef enum {
  TRANSMITTER_STATE_IDLE,
  TRANSMITTER_STATE_SYNC,
  TRANSMITTER_STATE_INITIATE_MEASUREMENT,
  /// Apply radio settings before measurement
  TRANSMITTER_STATE_MEASUREMENT_INIT,
  TRANSMITTER_STATE_MEASUREMENT,
  /// Wait for TX completion of the last command, then listen for ACK
  TRANSMITTER_STATE_CMD_TX_IN_PROGRESS,
  /// Wait for an ACK matching last_sent_command_id
  TRANSMITTER_STATE_WAIT_FOR_ACK,
  /// Switch to RX for the measurement report
  TRANSMITTER_STATE_MEASUREMENT_DONE,
  /// Wait in RX for the receiver's measurement report
  TRANSMITTER_STATE_WAIT_FOR_MEASUREMENT_REPORT,
  /// ACK the received measurement report
  TRANSMITTER_STATE_MEASUREMENT_REPORT_SEND_ACK,
  /// Wait for the measurement report ACK transmission to complete
  TRANSMITTER_STATE_WAIT_FOR_ACK_SENT,
  /// Wait for the retransmit timeout to expire
  TRANSMITTER_STATE_WAIT_FOR_RETRANSMIT,
  /// Free allocated resources, then go to IDLE
  TRANSMITTER_STATE_TEARDOWN,
} transmitter_state_t;

/// Receiver state machine
typedef enum {
  RECEIVER_STATE_IDLE,
  /// Apply settings from a received sync command, then ACK
  RECEIVER_STATE_SYNC,
  RECEIVER_STATE_SYNCED,
  RECEIVER_STATE_SEND_ACK,
  RECEIVER_STATE_WAIT_FOR_RETRANSMIT,
  /// Apply radio settings before measurement
  RECEIVER_STATE_MEASUREMENT_INIT,
  RECEIVER_STATE_MEASUREMENT,
  /// Delay, then send the measurement report
  RECEIVER_STATE_SEND_MEASUREMENT_REPORT,
  /// Wait for the report transmission to complete
  RECEIVER_STATE_MEASUREMENT_REPORT_TX_IN_PROGRESS,
  /// Wait for the other device to ACK the measurement report
  RECEIVER_STATE_WAIT_FOR_ACK,
  /// Restore the initial radio and application state, then go to IDLE
  RECEIVER_STATE_TEARDOWN,
} receiver_state_t;

/// IEEE 802.15.4 settings set by the ieee802154_configure CLI command
typedef struct {
  bool enabled;             // IEEE 802.15.4 frame format enabled
  uint16_t pan_id;          // Common PAN ID for both nodes
  uint16_t rx_address;      // Receiver address
  uint16_t tx_address;      // Transmitter address
  bool ack_enabled;                 // ACK enabled
  uint16_t ack_timeout_us;  // ACK timeout in microseconds
  bool source_addressing;   // Source addressing enabled
  bool pan_id_compression;  // PAN ID compression enabled
  uint16_t frame_length;    // Frame length in bytes
} ieee802154_settings_t;

/// CSMA/CA settings applied by the csma_configure CLI command
typedef struct {
  bool enabled;
  sl_rail_csma_config_t config;
} csma_settings_t;

/// Inter-device command protocol ---------------------------------------------
/* Command structure: [MAGIC] [CMD_ID] [CMD_BODY] */
/// Magic number in the first byte of every command packet
#define CMD_MAGIC 0xA5

#define CMD_ID_INVALID 0x00

#define CMD_ID_SYNC_SETTINGS 0x01
SL_PACK_START(1)
typedef struct {
  uint8_t enabled; // bool
  uint16_t pan_id;
  uint16_t rx_address;
  uint16_t tx_address;
  uint8_t ack_enabled; // bool
} SL_ATTRIBUTE_PACKED cmd_sync_ieee802154_t;
SL_PACK_END()

SL_PACK_START(1)
typedef struct {
  uint16_t channel;
  cmd_sync_ieee802154_t ieee802154;
} SL_ATTRIBUTE_PACKED cmd_sync_t;
SL_PACK_END()

#define CMD_ID_INITIATE_MEASUREMENT 0x02
// empty command body

#define CMD_ID_RX_MEASUREMENT_REPORT 0x03
SL_PACK_START(1)
typedef struct {
  uint32_t packets_received;  // Number of packets received successfully
  uint32_t frame_errors;      // Number of packets received with frame errors
} cmd_rx_measurement_report_t;
SL_PACK_END()

#define CMD_ID_ACK 0x04
typedef struct {
  uint8_t acked_command_id;
} cmd_ack_t;

/// Magic + command ID header length
#define CMD_HEADER_LENGTH 2U
/// Command air size (header + largest body). Shorter commands are zero-padded to this.
#define CMD_PACKET_LENGTH (CMD_HEADER_LENGTH + sizeof(cmd_sync_t))

// -----------------------------------------------------------------------------
//                                Global Variables
// -----------------------------------------------------------------------------
/// Current application role
extern app_role_t app_role;
/// Transmitter state machine state
extern transmitter_state_t transmitter_state;
/// Receiver state machine state
extern receiver_state_t receiver_state;
/// CSMA/CA settings
extern csma_settings_t csma_settings;
/// IEEE 802.15.4 settings
extern ieee802154_settings_t ieee802154_settings;
/// Rail handle
extern sl_rail_handle_t rail_handle;
/// RAIL channel for the command protocol
extern uint16_t channel_commands;
/// RAIL channel for the measurement protocol
extern uint16_t channel_measurement;
/// RX buffer for command packets (includes PHR as well)
extern uint8_t *rx_cmd_frame_buffer;
/// TX buffer for command packets
extern uint8_t *tx_cmd_buffer;

// -----------------------------------------------------------------------------
//                          Public Function Declarations
// -----------------------------------------------------------------------------
/*******************************************************************************
 * Application state machine, called infinitely
 ******************************************************************************/
void app_process_action(void);

/*******************************************************************************
 * IEEE 802.15.4 MHR length for the current addressing settings.
 * Returns 0 when IEEE 802.15.4 MAC framing is disabled.
 ******************************************************************************/
uint16_t get_ieee802154_mhr_length(void);

/*******************************************************************************
 * Maximum IEEE 802.15.4 PSDU length encoded by the current PHY PHR.
 ******************************************************************************/
uint16_t get_ieee802154_max_frame_length(void);

/*******************************************************************************
 * Get the length of the command frame
 ******************************************************************************/
uint16_t get_cmd_payload_length(void);

#endif  // APP_PROCESS_H
