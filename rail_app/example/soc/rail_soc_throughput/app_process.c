/***************************************************************************//**
 * @file
 * @brief app_process.c
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

// -----------------------------------------------------------------------------
//                                   Includes
// -----------------------------------------------------------------------------
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <inttypes.h>
#include "sl_component_catalog.h"
#include "sl_rail.h"
#include "sl_rail_sdk_packet_assistant.h"
#include "sl_code_classification.h"
#include "sl_sleeptimer.h"
#include "app_log.h"
#include "app_process.h"
#include "app_config.h"
#include "sl_status.h"
#include "sl_memory_manager.h"
#include "em_chip.h"

#if defined(SL_CATALOG_KERNEL_PRESENT)
#include "app_task_init.h"
#endif

// -----------------------------------------------------------------------------
//                              Macros and Typedefs
// -----------------------------------------------------------------------------

#define THROUGHPUT_CMD_ACK_TIMEOUT_MS 80
/// If ACK is lost, transmitter retransmits the command
#define THROUGHPUT_CMD_RETRANSMIT_TIMOUT_MS 150
/// Receiver times out if it receives no frames in the measurement window
#define THROUGHPUT_TRANSMIT_START_TIMEOUT_MS 1000

#define THROUGHPUT_CMD_MAX_RETRIES 100
#define THROUGHPUT_MEASUREMENT_REPORT_DELAY_US 100U
/// Extra RX measurement window so the receiver outlives the transmitter
#define THROUGHPUT_MEASUREMENT_RX_EXTRA_MS 300

#define IEEE802154_FCF_FRAME_TYPE_DATA 0x0001U
#define IEEE802154_FCF_ACK_REQUEST     0x0020U
#define IEEE802154_FCF_PAN_ID_COMP     0x0040U
#define IEEE802154_FCF_DEST_ADDR_SHORT 0x0800U
#define IEEE802154_FCF_SRC_ADDR_SHORT  0x8000U
/// Sequence number is the third byte of the MHR (after 2-byte FCF)
#define IEEE802154_MAC_SEQ_OFFSET     2U

// -----------------------------------------------------------------------------
//                          Static Function Declarations
// -----------------------------------------------------------------------------
static void app_process_action_transmitter(void);
static void app_process_action_receiver(void);
static void app_process_action_unassigned(void);
static sl_status_t apply_ieee802154_settings(void);
static sl_status_t create_ieee802154_mac_frame(uint8_t *payload);
static void fill_pn9_sequence(uint8_t *buffer, uint16_t length);
static sl_status_t prepare_and_send_packet(uint8_t *payload, uint16_t length);
static uint16_t get_measurement_payload_length(void);
static uint32_t calculate_goodput(uint32_t n_packets);
static sl_status_t send_command(const uint8_t *payload, uint16_t length);
static sl_status_t copy_rx_cmd_packet_to_buffer();
static bool is_received_packet_ack(void);
static sl_status_t process_received_packets(void);
static sl_status_t start_cmd_timer(uint32_t timeout_ms);
static sl_status_t stop_cmd_timer(void);
static void cmd_ack_timer_callback(sl_sleeptimer_timer_handle_t *handle, void *data);
static sl_status_t start_measurement_timer(uint16_t extra_ms);
static sl_status_t stop_measurement_timer(void);
static void measurement_timer_callback(sl_sleeptimer_timer_handle_t *handle, void *data);
static transmitter_state_t transmitter_get_next_state(bool ack_successful);
static receiver_state_t receiver_get_next_state(bool ack_successful);

#if THROUGHPUT_DEBUG_LOGS_ENABLED == 1
static const char *transmitter_state_name(transmitter_state_t state);
static const char *receiver_state_name(receiver_state_t state);
#endif

// -----------------------------------------------------------------------------
//                                Global Variables
// -----------------------------------------------------------------------------
app_role_t app_role = APP_ROLE_UNASSIGNED;
transmitter_state_t transmitter_state = TRANSMITTER_STATE_IDLE;
receiver_state_t receiver_state = RECEIVER_STATE_IDLE;

ieee802154_settings_t ieee802154_settings;
csma_settings_t csma_settings;

sl_rail_handle_t rail_handle;
uint16_t channel_measurement = 0U;
uint16_t channel_commands = 0U;
/// TX buffer for command packets
uint8_t *tx_cmd_buffer = NULL;
/// RX buffer for command packets (including PHR as well)
uint8_t *rx_cmd_frame_buffer = NULL;

// -----------------------------------------------------------------------------
//                                Static Variables
// -----------------------------------------------------------------------------
/// Notify reception of a packet (held in RAIL RX FIFO for later processing)
static volatile bool packet_received = false;
/// Notify successful packet transmission
static volatile bool packet_sent = false;
/// Notify RX completion error (frame error, abort, overflow, etc.)
static volatile bool rx_error = false;
/// RX frame error counter
static volatile uint32_t n_frame_errors = 0;
/// Notify TX completion error
static volatile bool tx_error = false;
/// Notify calibration failure
static volatile bool cal_error = false;
/// Notify that the command response wait timed out
static volatile bool cmd_timeout = false;
/// Notify that the IEEE 802.15.4 measurement ACK wait timed out
static volatile bool measurement_ack_timeout = false;
/// Notify that the measurement period has elapsed
static volatile bool measurement_over = false;
/// Last RAIL events bitmask for logging
static volatile sl_rail_events_t last_rail_events = SL_RAIL_EVENTS_NONE;
/// Status of the last calibration attempt
static volatile sl_rail_status_t calibration_status = SL_RAIL_STATUS_NO_ERROR;
/// Payload buffer for measurement TX packets (PN9-filled at measurement init)
static uint8_t *tx_measurement_buffer = NULL;
/// Buffer for storing the actual command payload (does not include PHR)
static uint8_t *rx_cmd_buffer = NULL;
/// Sleeptimer used while waiting for a command response
static sl_sleeptimer_timer_handle_t cmd_ack_timer;
/// Sleeptimer used for the measurement window
static sl_sleeptimer_timer_handle_t measurement_timer;
/// Number of command retransmissions since the last successful exchange
static uint16_t cmd_retransmit_count = 0U;
/// Command ID of the last command sent (used for ACK validation and transitions)
static uint8_t last_sent_cmd_id = 0U;
/// Command ID of the last command received (used for ACK validation and transitions)
static uint8_t last_received_cmd_id = 0U;
/// Sequence number of the last received 802.15.4 message
static uint8_t last_received_seq_num = 0xFF;
/// Number of measurement packets received
static uint32_t n_packets_received = 0U;
/// Number of measurement packets send
static uint32_t n_packets_sent = 0U;
/// Number of retries due to no ACK received 
static uint32_t n_ack_resends = 0U;
/// Number of retries due to busy channel detected by CSMA 
static volatile uint32_t n_csma_retries = 0U;
/// Sequence number of last sent message
static uint8_t sequence_number = 0U;
/// The receiver times out if it receives no messages in the measurement window
static bool meas_timeout_running;

// -----------------------------------------------------------------------------
//                          Public Function Definitions
// -----------------------------------------------------------------------------
/******************************************************************************
 * Get the payload length for the command packets (frame length excluding PHR)
 *****************************************************************************/
uint16_t get_cmd_payload_length(void)
{
  uint16_t frame_length;
  uint16_t result = sl_rail_set_fixed_length(rail_handle, SL_RAIL_SET_FIXED_LENGTH_INVALID);
  if ((result == 0) || (result == 1)) {
    // IEEE 802.15.4 PHY
    frame_length = THROUGHPUT_IEEE802154_DEFAULT_FRAME_LENGTH;
  } else {
    // Proprietary fixed-length PHY
    frame_length = result + 1;
  }
  return frame_length;
}

 /******************************************************************************
 * RAIL callback, called if a RAIL event occurs
 *****************************************************************************/
SL_CODE_RAM void sl_rail_util_on_event(sl_rail_handle_t source_rail_handle, sl_rail_events_t events)
{
  last_rail_events = events;
  // Handle Rx events
  if (events & SL_RAIL_EVENTS_RX_COMPLETION) {
    if (events & SL_RAIL_EVENT_RX_PACKET_RECEIVED) {
      // Keep the packet in the radio buffer, download it later at the state machine
      sl_rail_hold_rx_packet(source_rail_handle);
      packet_received = true;
    } else { 
      rx_error = true;
      if (events & SL_RAIL_EVENT_RX_FRAME_ERROR) {
        n_frame_errors++;
      }
    }
  }

  if (events & SL_RAIL_EVENT_TX_CCA_RETRY) {
    n_csma_retries++;
  }

  if (events & SL_RAIL_EVENT_RX_ACK_TIMEOUT) {
    measurement_ack_timeout = true;
  }

  // Handle Tx events
  if (events & SL_RAIL_EVENTS_TX_COMPLETION) {
    if (events & SL_RAIL_EVENT_TX_PACKET_SENT) {
      packet_sent = true;
    } else {
      tx_error = true;
      if (events & SL_RAIL_EVENT_TX_CHANNEL_BUSY) {
        n_csma_retries++;
      }
    }
  }

  // Handle Tx ACK events (IEEE 802.15.4 ACK)
  if ((events & SL_RAIL_EVENTS_TXACK_COMPLETION) && 
      !(events & SL_RAIL_EVENT_TXACK_PACKET_SENT)) {
    (void)events; // placeholder
  }

  // Perform all calibrations when needed
  if (events & SL_RAIL_EVENT_CAL_NEEDED) {
    calibration_status = sl_rail_calibrate(source_rail_handle, NULL, SL_RAIL_CAL_ALL_PENDING);
    if (calibration_status != SL_RAIL_STATUS_NO_ERROR) {
      cal_error = true;
    }
  }

  #if defined(SL_CATALOG_KERNEL_PRESENT)
  app_task_notify();
  #endif
}

/******************************************************************************
 * Application main loop
 *****************************************************************************/
void app_process_action(void)
{
  if (cal_error) {
    cal_error = false;
    app_log_error("RAIL calibration error, status: 0x%08" PRIX32 "\n",
      (uint32_t)calibration_status);
    }

  switch (app_role) {
    case APP_ROLE_UNASSIGNED:
      app_process_action_unassigned();
      break;
    case APP_ROLE_RECEIVER:
      app_process_action_receiver();
      break;
    case APP_ROLE_TRANSMITTER:
      app_process_action_transmitter();
      break;
    default:
      break;
  }

  #if defined(SL_CATALOG_KERNEL_PRESENT)
  app_task_notify();
  #endif
}

// -----------------------------------------------------------------------------
//                          Static Function Definitions
// -----------------------------------------------------------------------------
/******************************************************************************
 * Unassigned-role state machine: wait for a valid sync command
 *****************************************************************************/
static void app_process_action_unassigned(void)
{
  if (packet_received) {
    packet_received = false;

    sl_status_t status = copy_rx_cmd_packet_to_buffer();
    if (status != SL_STATUS_OK) {
      return;
    }
    // Validate that the message is indeed a sync command
    if ((rx_cmd_buffer[0] != CMD_MAGIC)
        || (rx_cmd_buffer[1] != CMD_ID_SYNC_SETTINGS)) {
      return;
    }
    last_received_cmd_id = CMD_ID_SYNC_SETTINGS;
    app_role = APP_ROLE_RECEIVER;
    receiver_state = RECEIVER_STATE_SYNC;
  }
}

/******************************************************************************
 * Transmitter state machine
 *****************************************************************************/
static void app_process_action_transmitter(void)
{
  static transmitter_state_t transmitter_old_state = TRANSMITTER_STATE_IDLE;
  static bool tx_requested = false;
  sl_status_t status;

  if (transmitter_old_state != transmitter_state) {
    #if THROUGHPUT_DEBUG_LOGS_ENABLED == 1
    app_log_info("State transition: %s -> %s\n",
                 transmitter_state_name(transmitter_old_state),
                 transmitter_state_name(transmitter_state));
    #endif
    transmitter_old_state = transmitter_state;
  }

  uint8_t cmd_frame[CMD_PACKET_LENGTH] = { 0 };

  switch (transmitter_state) {
    case TRANSMITTER_STATE_SYNC: // --------------------------------------------
      sl_free(tx_measurement_buffer);
      tx_measurement_buffer = (uint8_t *)sl_calloc(get_measurement_payload_length(),
                                                  sizeof(uint8_t));
      if (tx_measurement_buffer == NULL) {
        app_log_error("Failed to allocate measurement TX buffer\n");
        transmitter_state = TRANSMITTER_STATE_TEARDOWN;
        break;
      }

      cmd_sync_t sync_cmd;

      sync_cmd.channel = channel_measurement;
      sync_cmd.ieee802154.enabled = ieee802154_settings.enabled;
      sync_cmd.ieee802154.pan_id = ieee802154_settings.pan_id;
      sync_cmd.ieee802154.rx_address = ieee802154_settings.rx_address;
      sync_cmd.ieee802154.tx_address = ieee802154_settings.tx_address;
      sync_cmd.ieee802154.ack_enabled = ieee802154_settings.ack_enabled;

      cmd_frame[0] = CMD_MAGIC;
      cmd_frame[1] = CMD_ID_SYNC_SETTINGS;
      memcpy(&cmd_frame[CMD_HEADER_LENGTH], &sync_cmd, sizeof(sync_cmd));

      status = send_command(cmd_frame, CMD_PACKET_LENGTH);
      if (status != SL_STATUS_OK) {
        app_log_error("send_command() failed: 0x%08" PRIX32 "\n", status);
        break;
      }
 
      status = start_cmd_timer(THROUGHPUT_CMD_ACK_TIMEOUT_MS);
      if (status != SL_STATUS_OK) {
        app_log_error("start_cmd_timer() failed: 0x%08" PRIX32 "\n", status);
        transmitter_state = TRANSMITTER_STATE_TEARDOWN;
        break;
      }
      last_sent_cmd_id = CMD_ID_SYNC_SETTINGS;
      transmitter_state = TRANSMITTER_STATE_CMD_TX_IN_PROGRESS;
      break;

    case TRANSMITTER_STATE_INITIATE_MEASUREMENT: // -------------------------------
    {
      cmd_frame[0] = CMD_MAGIC;
      cmd_frame[1] = CMD_ID_INITIATE_MEASUREMENT;

      last_sent_cmd_id = CMD_ID_INITIATE_MEASUREMENT;
      status = send_command(cmd_frame, CMD_PACKET_LENGTH);
      if (status != SL_STATUS_OK) {
        app_log_error("send_command() failed: 0x%08" PRIX32 "\n", status);
        break;
      }
      status = start_cmd_timer(THROUGHPUT_CMD_ACK_TIMEOUT_MS);
      if (status != SL_STATUS_OK) {
        app_log_error("start_cmd_timer() failed: 0x%08" PRIX32 "\n", status);
        break;
      }
      transmitter_state = TRANSMITTER_STATE_CMD_TX_IN_PROGRESS;
      break;
    }

    case TRANSMITTER_STATE_CMD_TX_IN_PROGRESS: // ----------------------------------
      if (packet_sent) {
        packet_sent = false;
        if (sl_rail_start_rx(rail_handle, channel_commands, NULL) != SL_RAIL_STATUS_NO_ERROR) {
          app_log_error("sl_rail_start_rx() failed\n");
          transmitter_state = TRANSMITTER_STATE_TEARDOWN;
          break;
        } 
        transmitter_state = TRANSMITTER_STATE_WAIT_FOR_ACK;
      } else if (tx_error) {
        tx_error = false;
        app_log_error("command sending failed, events: 0x%08" PRIx64"\n", last_rail_events);
        transmitter_state = TRANSMITTER_STATE_TEARDOWN;
      }
      break;

    case TRANSMITTER_STATE_WAIT_FOR_ACK: // ------------------------------------
      if (cmd_timeout) {
        cmd_timeout = false;
        cmd_retransmit_count++;
        if (cmd_retransmit_count > THROUGHPUT_CMD_MAX_RETRIES) {
          (void)stop_cmd_timer();
          app_log_error("No other device available "
                        "(no ACK after %" PRIu8 " retransmissions)\n",
                        (uint8_t)THROUGHPUT_CMD_MAX_RETRIES);
          cmd_retransmit_count = 0U;
          last_sent_cmd_id = 0U;
          transmitter_state = TRANSMITTER_STATE_TEARDOWN;
          break;
        }
        transmitter_state = transmitter_get_next_state(false);
        break;
      }
      if (packet_received) {
        packet_received = false;
        status = copy_rx_cmd_packet_to_buffer();
        if (status != SL_STATUS_OK) {
          app_log_error("copy_rx_cmd_packet_to_buffer() failed: 0x%08" PRIX32 "\n",
                        status);
          break;
        }
        // Validate ACK for the command we last sent
        if ((rx_cmd_buffer[0] != CMD_MAGIC)
            || (rx_cmd_buffer[1] != CMD_ID_ACK)
            || (rx_cmd_buffer[2] != last_sent_cmd_id)) {
          break;
        }
        status = stop_cmd_timer();
        if (status != SL_STATUS_OK) {
          app_log_error("stop_cmd_timer() failed: 0x%08" PRIX32 "\n", status);
        }
        cmd_retransmit_count = 0U;
        transmitter_state = transmitter_get_next_state(true);
      }
      break;

    case TRANSMITTER_STATE_MEASUREMENT_INIT: // --------------------------------
      status = apply_ieee802154_settings();
      if (status != SL_STATUS_OK) {
        app_log_error("apply_ieee802154_settings() failed: 0x%08" PRIX32 "\n",
                      status);
        transmitter_state = TRANSMITTER_STATE_TEARDOWN;
        break;
      }
      fill_pn9_sequence(tx_measurement_buffer, get_measurement_payload_length());
      status = start_measurement_timer(THROUGHPUT_CMD_RETRANSMIT_TIMOUT_MS);
      if (status != SL_STATUS_OK) {
        app_log_error("start_measurement_timer() failed: 0x%08" PRIX32 "\n", status);
        transmitter_state = TRANSMITTER_STATE_TEARDOWN;
        break;
      }
      n_packets_sent = 0;
      tx_requested = true;
      // receiver is waiting for a retransmit, make sure it already started the measurement
      sl_sleeptimer_delay_millisecond(THROUGHPUT_CMD_RETRANSMIT_TIMOUT_MS);
      app_log_info("Measurement is in progress...\n");
      transmitter_state = TRANSMITTER_STATE_MEASUREMENT;
      break;

    case TRANSMITTER_STATE_MEASUREMENT: // --------------------------------
      if (packet_sent) {
        if (ieee802154_settings.ack_enabled) {
          if (packet_received) {
            if (is_received_packet_ack()) {
              // ACK received, increment counter and proceed to next transfer
              packet_received = false;
              packet_sent = false;
              n_packets_sent++;
              tx_requested = true;
              //app_log_info("%"PRIu32" packet sent (and ACKed) with seq num: %"PRIu8"\n", n_packets_sent, sequence_number);
            } else {
              // packet was not an ACK
              packet_received = false;
            }
          } else if (measurement_ack_timeout) {
            // ACK timeout, proceed to next transfer without incrementing counter
            n_ack_resends++;
            measurement_ack_timeout = false;
            packet_received = false;
            packet_sent = false;
            tx_requested = true;
          } else {
            // waiting for ACK
            break;
          }
        } else {
          packet_sent = false;
          n_packets_sent++;
          tx_requested = true;
        }
      } 
      if (tx_error && !packet_sent) {
        tx_requested = true;
        tx_error = false;
      }
      if (tx_requested) {
        if (measurement_over) {
          measurement_over = false;
          transmitter_state = TRANSMITTER_STATE_MEASUREMENT_DONE;
          break;
        }
        packet_sent = false;
        tx_error = false;
        tx_requested = false;
        status = prepare_and_send_packet(tx_measurement_buffer, get_measurement_payload_length());
        if (status != SL_STATUS_OK) {
          app_log_error("prepare_and_send_packet() failed: 0x%08" PRIX32 "\n", status);
          transmitter_state = TRANSMITTER_STATE_TEARDOWN;
        }
      }
      break;

    case TRANSMITTER_STATE_MEASUREMENT_DONE: // --------------------------------
      (void)sl_rail_idle(rail_handle, SL_RAIL_IDLE, true);
      sl_rail_ieee802154_deinit(rail_handle);
      status = sl_rail_start_rx(rail_handle, channel_commands, NULL);
      if (status != SL_RAIL_STATUS_NO_ERROR) {
        app_log_error("sl_rail_start_rx() failed: 0x%08" PRIX32 "\n",
                      (uint32_t)status);
        break;
      }
      app_log_info("Measurement done\n");
      transmitter_state = TRANSMITTER_STATE_WAIT_FOR_MEASUREMENT_REPORT;
      break;

    case TRANSMITTER_STATE_WAIT_FOR_MEASUREMENT_REPORT: // ---------------------
    {
      cmd_rx_measurement_report_t report;

      if (packet_received) {
        packet_received = false;
        
        status = copy_rx_cmd_packet_to_buffer();
        if (status != SL_STATUS_OK) {
          app_log_error("copy_rx_cmd_packet_to_buffer() failed: 0x%08" PRIX32 "\n",
                        status);
          break;
        }
        if ((rx_cmd_buffer[0] != CMD_MAGIC)
            || (rx_cmd_buffer[1] != CMD_ID_RX_MEASUREMENT_REPORT)) {
          break;
        }
  
        memcpy(&report, &rx_cmd_buffer[CMD_HEADER_LENGTH], sizeof(report));

        // the last message got sent, but the ACK did not arrive 
        if (report.packets_received == n_packets_sent + 1) {
          report.packets_received = n_packets_sent;
        }
        
        app_log_info("********************************************\n");
        app_log_info("Packets sent: %" PRIu32 "\n", n_packets_sent);
        app_log_info("Packets received: %" PRIu32 "\n", report.packets_received);
        if (ieee802154_settings.ack_enabled) {
          app_log_info("ACK retries: %" PRIu32 "\n", n_ack_resends);
        }
        if (csma_settings.enabled) {
          app_log_info("CSMA retries: %" PRIu32 "\n", n_csma_retries);
        }
        app_log_info("Frame errors: %" PRIu32 "\n", report.frame_errors);
        if (n_packets_sent > 0U) {
          app_log_info("Packet delivery rate: %" PRIu32 ".%" PRIu32 "%%\n",
                       (report.packets_received * 100U) / n_packets_sent,
                       ((report.packets_received * 10000U) / n_packets_sent) % 100);
        } else {
          app_log_info("Packet delivery rate: n/a\n");
        }
          
        uint32_t offered_tx_bps = calculate_goodput(n_packets_sent);
        uint32_t rx_goodput_bps = calculate_goodput(report.packets_received);
        uint32_t goodput_loss_bps = (offered_tx_bps > rx_goodput_bps)
                                    ? (offered_tx_bps - rx_goodput_bps) : 0U;
        app_log_info("\n");                                    
        app_log_info("TX goodput: %" PRIu32 ".%" PRIu32 " kbits/s\n",
                     (offered_tx_bps + 50) / 1000U,
                     ((offered_tx_bps + 50) / 100) % 10U);
        app_log_info("RX goodput: %" PRIu32 ".%" PRIu32 " kbits/s\n",
                     (rx_goodput_bps + 50) / 1000U,
                     ((rx_goodput_bps + 50) / 100) % 10U);
        app_log_info("Over-the-air loss: %" PRIu32 ".%" PRIu32 " kbits/s\n",
                     (goodput_loss_bps + 50) / 1000U,
                     ((goodput_loss_bps + 50) / 100) % 10U);

        uint16_t overhead_length = (uint16_t)sl_packet_assistant_get_phr_length() 
                                  + get_ieee802154_mhr_length();
        uint16_t total_length = get_measurement_payload_length();
        app_log_info("Protocol overhead: %" PRIu16 ".%" PRIu16 "%%\n",
                    (overhead_length * 100U) / total_length,
                    (overhead_length * 10000U / total_length) % 100U);

        app_log_info("********************************************\n");
        last_received_cmd_id = CMD_ID_RX_MEASUREMENT_REPORT;
        transmitter_state = TRANSMITTER_STATE_MEASUREMENT_REPORT_SEND_ACK;
      }
      break;
    }

    case TRANSMITTER_STATE_MEASUREMENT_REPORT_SEND_ACK: // ----------------------------------------
    {
      cmd_ack_t ack_cmd;

      ack_cmd.acked_command_id = last_received_cmd_id;
      cmd_frame[0] = CMD_MAGIC;
      cmd_frame[1] = CMD_ID_ACK;
      memcpy(&cmd_frame[CMD_HEADER_LENGTH], &ack_cmd, sizeof(ack_cmd));

      (void)sl_rail_idle(rail_handle, SL_RAIL_IDLE, true);
      packet_sent = false;
      status = send_command(cmd_frame, CMD_PACKET_LENGTH);
      if (status != SL_STATUS_OK) {
        app_log_error("send_command() failed: 0x%08" PRIX32 "\n", status);
        transmitter_state = TRANSMITTER_STATE_TEARDOWN;
        break;
      }
      transmitter_state = TRANSMITTER_STATE_WAIT_FOR_ACK_SENT;
      break;
    }

    case TRANSMITTER_STATE_WAIT_FOR_ACK_SENT: // -------------------------------
      if (packet_sent) {
        packet_sent = false;
        start_cmd_timer(THROUGHPUT_CMD_RETRANSMIT_TIMOUT_MS);
        transmitter_state = TRANSMITTER_STATE_WAIT_FOR_RETRANSMIT;
      } else if (tx_error) {
        tx_error = false;
        transmitter_state = TRANSMITTER_STATE_MEASUREMENT_REPORT_SEND_ACK;
      }
      break;

    case TRANSMITTER_STATE_WAIT_FOR_RETRANSMIT:
      if (packet_received) {
        packet_received = false;
        stop_cmd_timer();
        transmitter_state = TRANSMITTER_STATE_MEASUREMENT_REPORT_SEND_ACK;
      } else if (cmd_timeout) {
        // no retransmissions, receiver is done
        cmd_timeout = false;
        transmitter_state = TRANSMITTER_STATE_TEARDOWN;
      }
      break;

    case TRANSMITTER_STATE_TEARDOWN: // ----------------------------------------
      tx_requested = false;
      sl_sleeptimer_delay_millisecond(100);
      CHIP_Reset();
      break;

    default:
      break;
  }
}
 
 /******************************************************************************
  * Receiver state machine
  *****************************************************************************/
 static void app_process_action_receiver(void)
 {
    static receiver_state_t receiver_old_state = RECEIVER_STATE_IDLE;
    sl_status_t status;

    if (receiver_old_state != receiver_state) {
      #if THROUGHPUT_DEBUG_LOGS_ENABLED == 1
      app_log_info("State transition: %s -> %s\n",
                   receiver_state_name(receiver_old_state),
                   receiver_state_name(receiver_state));
      #endif
      receiver_old_state = receiver_state;
    }

    switch (receiver_state) {
      case RECEIVER_STATE_SYNC: // ---------------------------------------------
        cmd_sync_t sync_cmd;
        memcpy(&sync_cmd, &rx_cmd_buffer[CMD_HEADER_LENGTH], sizeof(sync_cmd));
   
        channel_measurement = sync_cmd.channel;
        ieee802154_settings.enabled = sync_cmd.ieee802154.enabled;
        ieee802154_settings.pan_id = sync_cmd.ieee802154.pan_id;
        ieee802154_settings.rx_address = sync_cmd.ieee802154.rx_address;
        ieee802154_settings.tx_address = sync_cmd.ieee802154.tx_address;
        ieee802154_settings.ack_enabled = sync_cmd.ieee802154.ack_enabled;

        receiver_state = RECEIVER_STATE_SEND_ACK;
        break;

      case RECEIVER_STATE_SEND_ACK: // -----------------------------------------
        uint8_t cmd_frame[CMD_PACKET_LENGTH] = { 0 };
        cmd_ack_t ack_cmd;

        ack_cmd.acked_command_id = last_received_cmd_id;

        cmd_frame[0] = CMD_MAGIC;
        cmd_frame[1] = CMD_ID_ACK;
        memcpy(&cmd_frame[CMD_HEADER_LENGTH], &ack_cmd, sizeof(ack_cmd));

        sl_rail_idle(rail_handle, SL_RAIL_IDLE, true);
        status = send_command(cmd_frame, CMD_PACKET_LENGTH);
        if (status != SL_STATUS_OK) {
          app_log_error("send_command() failed: 0x%08" PRIX32 "\n", status);
          receiver_state = RECEIVER_STATE_TEARDOWN;
          break;
        }
        start_cmd_timer(THROUGHPUT_CMD_RETRANSMIT_TIMOUT_MS);
        receiver_state = RECEIVER_STATE_WAIT_FOR_RETRANSMIT;
        break;
        
      case RECEIVER_STATE_WAIT_FOR_RETRANSMIT: // -------------------------------
        // if ACK is lost, transmitter will re-send the cmd
        if (packet_received) {
          packet_received = false;
          status = copy_rx_cmd_packet_to_buffer();
          if (status != SL_STATUS_OK) {
            app_log_error("copy_rx_cmd_packet_to_buffer() failed: 0x%08" PRIX32 "\n",
                          status);
            break;
          }
          if (rx_cmd_buffer[0] != CMD_MAGIC) {
            break;
          }
          stop_cmd_timer();
          last_received_cmd_id = rx_cmd_buffer[1];
          receiver_state = receiver_get_next_state(false);
        } else if (cmd_timeout) {
          // no retransmission, proceed
          cmd_timeout = false;
          receiver_state = receiver_get_next_state(true);
        }

        break;

      case RECEIVER_STATE_SYNCED: // ------------------------------------------
        if (packet_received) {
          packet_received = false;
          status = copy_rx_cmd_packet_to_buffer();
          if (status != SL_STATUS_OK) {
            app_log_error("copy_rx_cmd_packet_to_buffer() failed: 0x%08" PRIX32 "\n",
                          status);
            break;
          }
          if (rx_cmd_buffer[0] != CMD_MAGIC) {
            break;
          }
          if (rx_cmd_buffer[1] == CMD_ID_INITIATE_MEASUREMENT) {
            last_received_cmd_id = CMD_ID_INITIATE_MEASUREMENT;
            receiver_state = RECEIVER_STATE_SEND_ACK;
          } else if (rx_cmd_buffer[1] == CMD_ID_SYNC_SETTINGS) {
            receiver_state = RECEIVER_STATE_SYNC;
          }
        }
        break;

      case RECEIVER_STATE_MEASUREMENT_INIT: // ---------------------------------
        // wait for ACK to be sent
        if (packet_sent) {
          packet_sent = false;
          status = apply_ieee802154_settings();
          if (status != SL_STATUS_OK) {
            app_log_error("apply_ieee802154_settings() failed: 0x%08" PRIX32 "\n",
                          status);
            receiver_state = RECEIVER_STATE_TEARDOWN;
            break;
          }
          status = sl_rail_start_rx(rail_handle, channel_measurement, NULL);
          if (status != SL_RAIL_STATUS_NO_ERROR) {
            app_log_error("sl_rail_start_rx() failed: 0x%08" PRIX32 "\n",
                          (uint32_t)status);
            receiver_state = RECEIVER_STATE_TEARDOWN;
            break;
          }
          status = start_measurement_timer(THROUGHPUT_MEASUREMENT_RX_EXTRA_MS);
          if (status != SL_STATUS_OK) {
            app_log_error("start_measurement_timer() failed: 0x%08" PRIX32 "\n",
                          status);
            receiver_state = RECEIVER_STATE_TEARDOWN;
            break;
          }
          n_packets_received = 0;
          n_frame_errors = 0;
          app_log_info("Measurement is in progress...\n");
          start_cmd_timer(THROUGHPUT_TRANSMIT_START_TIMEOUT_MS);
          meas_timeout_running = true;
          receiver_state = RECEIVER_STATE_MEASUREMENT;
        } else if (tx_error) {
          tx_error = false;
          receiver_state = RECEIVER_STATE_SEND_ACK;
        }
        break;

      case RECEIVER_STATE_MEASUREMENT: // ---------------------------------------
        if (packet_received) {
          packet_received = false;
          if (meas_timeout_running) {
            meas_timeout_running = false;
            stop_cmd_timer();
          }
          (void)process_received_packets();
        }
        if (measurement_over) {
          measurement_over = false;
          app_log_info("Measurement done\n");
          receiver_state = RECEIVER_STATE_SEND_MEASUREMENT_REPORT;
          break;
        }
        if (cmd_timeout) {
          // transmitter did not start measurement, go back
          cmd_timeout = false;
          stop_measurement_timer();
          app_log_info("Timeout: no measurement frames received\n");
          sl_rail_idle(rail_handle, SL_RAIL_IDLE, true);
          sl_rail_ieee802154_deinit(rail_handle);
          sl_rail_start_rx(rail_handle, channel_commands, NULL);
          receiver_state = RECEIVER_STATE_SYNCED;
        }
        break;

      case RECEIVER_STATE_SEND_MEASUREMENT_REPORT: // --------------------------
      {
        uint8_t cmd_frame[CMD_PACKET_LENGTH] = { 0 };
        cmd_rx_measurement_report_t report = {
          .packets_received = n_packets_received,
          .frame_errors = n_frame_errors
        };

        status = sl_rail_idle(rail_handle, SL_RAIL_IDLE, true);
        if (status != SL_RAIL_STATUS_NO_ERROR) {
          app_log_error("sl_rail_idle() failed: 0x%08" PRIX32 "\n",
                        (uint32_t)status);
          break;
        }
        // wait for the transmitter to switch to RX mode
        sl_rail_delay_us(rail_handle, THROUGHPUT_MEASUREMENT_REPORT_DELAY_US);

        cmd_frame[0] = CMD_MAGIC;
        cmd_frame[1] = CMD_ID_RX_MEASUREMENT_REPORT;
        memcpy(&cmd_frame[CMD_HEADER_LENGTH], &report, sizeof(report));
        
        packet_sent = false;
        last_sent_cmd_id = CMD_ID_RX_MEASUREMENT_REPORT;
        
        sl_rail_ieee802154_deinit(rail_handle);
        status = send_command(cmd_frame, CMD_PACKET_LENGTH);
        if (status != SL_STATUS_OK) {
          app_log_error("send_command() failed: 0x%08" PRIX32 "\n", status);
          receiver_state = RECEIVER_STATE_TEARDOWN;
          break;
        }
        
        receiver_state = RECEIVER_STATE_MEASUREMENT_REPORT_TX_IN_PROGRESS;
        break;
      }

      case RECEIVER_STATE_MEASUREMENT_REPORT_TX_IN_PROGRESS: // ----------------
        if (packet_sent) {
          packet_sent = false;
          if (sl_rail_start_rx(rail_handle, channel_commands, NULL) != SL_RAIL_STATUS_NO_ERROR) {
            app_log_error("sl_rail_start_rx() failed\n");
            receiver_state = RECEIVER_STATE_TEARDOWN;
            break;
          }
          status = start_cmd_timer(THROUGHPUT_CMD_ACK_TIMEOUT_MS);
          if (status != SL_STATUS_OK) {
            app_log_error("start_cmd_timer() failed: 0x%08" PRIX32 "\n", status);
            receiver_state = RECEIVER_STATE_TEARDOWN;
            break;
          }
          receiver_state = RECEIVER_STATE_WAIT_FOR_ACK;
        } else if (tx_error) {
          tx_error = false;
          receiver_state = RECEIVER_STATE_SEND_MEASUREMENT_REPORT;
        }
        break;

      case RECEIVER_STATE_WAIT_FOR_ACK: // -------------------------------------
        if (cmd_timeout) {
          cmd_timeout = false;
          cmd_retransmit_count++;
          if (cmd_retransmit_count > 50) {
            app_log_error("No other device available "
                          "(no ACK after %" PRIu8 " retransmissions)\n",
                          (uint8_t)50);
            receiver_state = RECEIVER_STATE_TEARDOWN;
            break;
          }
          receiver_state = RECEIVER_STATE_SEND_MEASUREMENT_REPORT;
          break;
        }
        if (packet_received) {
          packet_received = false;
          status = copy_rx_cmd_packet_to_buffer();
          if (status != SL_STATUS_OK) {
            app_log_error("copy_rx_cmd_packet_to_buffer() failed: 0x%08" PRIX32 "\n",
                          status);
            break;
          }
          if ((rx_cmd_buffer[0] != CMD_MAGIC)
              || (rx_cmd_buffer[1] != CMD_ID_ACK)
              || (rx_cmd_buffer[2] != last_sent_cmd_id)) {
            break;
          }
          status = stop_cmd_timer();
          if (status != SL_STATUS_OK) {
            app_log_error("stop_cmd_timer() failed: 0x%08" PRIX32 "\n", status);
          }
          receiver_state = RECEIVER_STATE_TEARDOWN;
        }
        break;

      case RECEIVER_STATE_TEARDOWN: // -----------------------------------------
        sl_sleeptimer_delay_millisecond(100);
        CHIP_Reset();
        break;

      default:
        break;
    }
 }

#if THROUGHPUT_DEBUG_LOGS_ENABLED == 1
/******************************************************************************
 * Transmitter state name without the TRANSMITTER_STATE_ prefix
 *****************************************************************************/
static const char *transmitter_state_name(transmitter_state_t state)
{
  switch (state) {
    case TRANSMITTER_STATE_IDLE:
      return "IDLE";
    case TRANSMITTER_STATE_SYNC:
      return "SYNC";
    case TRANSMITTER_STATE_INITIATE_MEASUREMENT:
      return "START_MEASUREMENT";
    case TRANSMITTER_STATE_MEASUREMENT_INIT:
      return "MEASUREMENT_INIT";
    case TRANSMITTER_STATE_MEASUREMENT:
      return "MEASUREMENT";
    case TRANSMITTER_STATE_CMD_TX_IN_PROGRESS:
      return "CMD_TX_IN_PROGRESS";
    case TRANSMITTER_STATE_WAIT_FOR_ACK:
      return "WAIT_FOR_ACK";
    case TRANSMITTER_STATE_MEASUREMENT_DONE:
      return "MEASUREMENT_DONE";
    case TRANSMITTER_STATE_WAIT_FOR_MEASUREMENT_REPORT:
      return "WAIT_FOR_MEASUREMENT_REPORT";
    case TRANSMITTER_STATE_MEASUREMENT_REPORT_SEND_ACK:
      return "MEASUREMENT_REPORT_SEND_ACK";
    case TRANSMITTER_STATE_WAIT_FOR_ACK_SENT:
      return "WAIT_FOR_ACK_SENT";
    case TRANSMITTER_STATE_TEARDOWN:
      return "TEARDOWN";
    default:
      return "UNKNOWN";
  }
}

/******************************************************************************
 * Receiver state name without the RECEIVER_STATE_ prefix
 *****************************************************************************/
static const char *receiver_state_name(receiver_state_t state)
{
  switch (state) {
    case RECEIVER_STATE_IDLE:
      return "IDLE";
    case RECEIVER_STATE_SYNC:
      return "SYNC";
    case RECEIVER_STATE_SYNCED:
      return "SYNCED";
    case RECEIVER_STATE_SEND_ACK:
      return "SEND_ACK";
    case RECEIVER_STATE_MEASUREMENT_INIT:
      return "MEASUREMENT_INIT";
    case RECEIVER_STATE_MEASUREMENT:
      return "MEASUREMENT";
    case RECEIVER_STATE_SEND_MEASUREMENT_REPORT:
      return "SEND_MEASUREMENT_REPORT";
    case RECEIVER_STATE_MEASUREMENT_REPORT_TX_IN_PROGRESS:
      return "MEASUREMENT_REPORT_TX_IN_PROGRESS";
    case RECEIVER_STATE_WAIT_FOR_ACK:
      return "WAIT_FOR_ACK";
    case RECEIVER_STATE_TEARDOWN:
      return "TEARDOWN";
    default:
      return "UNKNOWN";
  }
}
#endif

/******************************************************************************
 * Map a command ID to the next transmitter state after ACK success or timeout
 *****************************************************************************/
static transmitter_state_t transmitter_get_next_state(bool ack_successful)
{
  switch (last_sent_cmd_id) {
    case CMD_ID_SYNC_SETTINGS:
      return ack_successful ? TRANSMITTER_STATE_INITIATE_MEASUREMENT
                            : TRANSMITTER_STATE_SYNC;
    case CMD_ID_INITIATE_MEASUREMENT:
      return ack_successful ? TRANSMITTER_STATE_MEASUREMENT_INIT
                            : TRANSMITTER_STATE_INITIATE_MEASUREMENT;
    default:
      return TRANSMITTER_STATE_TEARDOWN;
  }
}

/******************************************************************************
 * Map a received command ID to the next receiver state after sending ACK
 *****************************************************************************/
static receiver_state_t receiver_get_next_state(bool ack_successful)
{
  if (ack_successful) {
    switch (last_received_cmd_id) {
      case CMD_ID_SYNC_SETTINGS:
        return RECEIVER_STATE_SYNCED;
      case CMD_ID_INITIATE_MEASUREMENT:
        return RECEIVER_STATE_MEASUREMENT_INIT;
      default:
        return RECEIVER_STATE_TEARDOWN;
    }
  } else {
    switch (last_received_cmd_id) {
      case CMD_ID_SYNC_SETTINGS:
        return RECEIVER_STATE_SYNC;
      case CMD_ID_INITIATE_MEASUREMENT:
        return RECEIVER_STATE_SYNCED;
      default:
        return RECEIVER_STATE_TEARDOWN;
    }
  }
}

/******************************************************************************
 * @brief Drain completed RX packets; true if an ACK matches sequence_number
 *****************************************************************************/
static bool is_received_packet_ack(void)
{
  sl_rail_rx_packet_info_t packet_info;
  sl_rail_rx_packet_handle_t rx_packet_handle;
  sl_rail_rx_packet_details_t packet_details;
  sl_rail_status_t status;
  bool is_ack = false;

  rx_packet_handle = sl_rail_get_rx_packet_info(rail_handle,
                                                SL_RAIL_RX_PACKET_HANDLE_OLDEST_COMPLETE,
                                                &packet_info);
  // First call must yield a packet; later INVALID means the queue is drained
  if (rx_packet_handle == SL_RAIL_RX_PACKET_HANDLE_INVALID) {
    app_log_error("sl_rail_get_rx_packet_info() returned invalid handle\n");
    return false;
  }

  while (rx_packet_handle != SL_RAIL_RX_PACKET_HANDLE_INVALID) {
    memset(&packet_details, 0, sizeof(packet_details));
    status = sl_rail_get_rx_packet_details(rail_handle,
                                           rx_packet_handle,
                                           &packet_details);
    if (status != SL_RAIL_STATUS_NO_ERROR) {
      app_log_error("sl_rail_get_rx_packet_details() failed: 0x%" PRIX32 "\n",
                    (uint32_t)status);
    } else if (packet_details.is_ack) {
      uint8_t seq_num;
      uint16_t seq_offset = (uint16_t)(IEEE802154_MAC_SEQ_OFFSET
                                       + sl_packet_assistant_get_phr_length());
      (void)sl_rail_peek_rx_packet(rail_handle,
                                   rx_packet_handle,
                                   &seq_num,
                                   1,
                                   seq_offset);
      if (seq_num == sequence_number) {
        is_ack = true;
      }
    }

    status = sl_rail_release_rx_packet(rail_handle, rx_packet_handle);
    if (status != SL_RAIL_STATUS_NO_ERROR) {
      app_log_error("sl_rail_release_rx_packet() failed: 0x%" PRIX32 "\n",
                    (uint32_t)status);
      return false;
    }
    if (is_ack) {
      break;
    }

    rx_packet_handle = sl_rail_get_rx_packet_info(rail_handle,
                                                  SL_RAIL_RX_PACKET_HANDLE_OLDEST_COMPLETE,
                                                  &packet_info);
  }

  return is_ack;
}

/******************************************************************************
 * @brief Unpack the oldest completed RX command into the provided buffer
 *
 * Uses the packet assistant so a PHY PHR is stripped before the command
 * payload is copied.
 *****************************************************************************/
static sl_status_t copy_rx_cmd_packet_to_buffer()
{
  sl_rail_rx_packet_info_t packet_info;
  sl_rail_rx_packet_handle_t rx_packet_handle;
  sl_status_t status;
  uint16_t payload_size = 0U;

  rx_packet_handle = sl_rail_get_rx_packet_info(rail_handle,
                                                SL_RAIL_RX_PACKET_HANDLE_OLDEST_COMPLETE,
                                                &packet_info);
  
  if (rx_packet_handle == SL_RAIL_RX_PACKET_HANDLE_INVALID) {
    app_log_error("sl_rail_get_rx_packet_info() returned invalid handle\n");
    return SL_STATUS_NOT_FOUND;
  }

  // full frame length (PHR + payload)
  uint16_t frame_length = get_cmd_payload_length() + (uint16_t)sl_packet_assistant_get_phr_length();
  if (packet_info.packet_bytes > frame_length) {
    app_log_error("RX packet too large for buffer (%" PRIu16 " > %" PRIu16 ")\n",
                  packet_info.packet_bytes,
                  frame_length);
    status = SL_STATUS_INVALID_PARAMETER;
  } else {
    status = sl_packet_assistant_unpack_packet(rail_handle,
                                               &packet_info,
                                               rx_cmd_frame_buffer,
                                               &rx_cmd_buffer,
                                               &payload_size);
    if (status != SL_STATUS_OK) {
      app_log_error("sl_packet_assistant_unpack_packet() failed: 0x%08" PRIX32 "\n",
                    (uint32_t)status);
    }
  }

  if (sl_rail_release_rx_packet(rail_handle, rx_packet_handle) != SL_RAIL_STATUS_NO_ERROR) {
    app_log_error("sl_rail_release_rx_packet() failed\n");
    return SL_STATUS_FAIL;
  }
  
  return status;
}

/******************************************************************************
 * @brief Iterate over received packets and process them
 *****************************************************************************/
static sl_status_t process_received_packets()
{
  sl_rail_rx_packet_info_t packet_info;
  sl_rail_rx_packet_handle_t rx_packet_handle;
  sl_status_t status = SL_STATUS_OK;

  rx_packet_handle = sl_rail_get_rx_packet_info(rail_handle,
                                                SL_RAIL_RX_PACKET_HANDLE_OLDEST_COMPLETE,
                                                &packet_info);

  // First call must yield a packet; later INVALID means the queue is drained
  if (rx_packet_handle == SL_RAIL_RX_PACKET_HANDLE_INVALID) {
    app_log_error("sl_rail_get_rx_packet_info() returned invalid handle\n");
    return SL_STATUS_NOT_FOUND;
  }

  while (rx_packet_handle != SL_RAIL_RX_PACKET_HANDLE_INVALID) {
    // in 802.15.4 ACK mode validate the sequence number to avoid counting duplicated packets
    if (ieee802154_settings.ack_enabled) {
      uint8_t seq_num;
      (void)sl_rail_peek_rx_packet(rail_handle, 
                                   rx_packet_handle, 
                                   &seq_num, 
                                   1, 
                                   2 + sl_packet_assistant_get_phr_length());
                                   // MHR_SEQ_NUM_OFFSET + PHR_LENGTH
      if (seq_num != last_received_seq_num) {
        n_packets_received++;
      }
      last_received_seq_num = seq_num;
    } else {
      n_packets_received++;
    }
    status = sl_rail_release_rx_packet(rail_handle, rx_packet_handle);
    if (status != SL_RAIL_STATUS_NO_ERROR) {
      app_log_error("sl_rail_release_rx_packet() failed: 0x%" PRIX32 "\n",
                    (uint32_t)status);
      status = SL_STATUS_FAIL;
      break;
    }

    rx_packet_handle = sl_rail_get_rx_packet_info(rail_handle,
                                                  SL_RAIL_RX_PACKET_HANDLE_OLDEST_COMPLETE,
                                                  &packet_info);
  }

  return status;
}

/******************************************************************************
 * Get the measurement payload length (frame length excluding PHR)
 *****************************************************************************/
 static uint16_t get_measurement_payload_length(void)
 {
    uint16_t frame_length;
    if (ieee802154_settings.frame_length != 0) {
      // IEEE 802.15.4 PHY - user set the length via CLI
      frame_length = ieee802154_settings.frame_length;
    } else if ((sl_rail_set_fixed_length(rail_handle, SL_RAIL_SET_FIXED_LENGTH_INVALID) == 0) ||
              // exception: OFDM returns 1
               (sl_rail_set_fixed_length(rail_handle, SL_RAIL_SET_FIXED_LENGTH_INVALID) == 1)) {
      // IEEE 802.15.4 PHY - user did not set the length
      frame_length = THROUGHPUT_IEEE802154_DEFAULT_FRAME_LENGTH;
    } else {
      // Proprietary fixed-length PHY
      frame_length = sl_rail_set_fixed_length(rail_handle, SL_RAIL_SET_FIXED_LENGTH_INVALID) + 1;
    }

    return frame_length;
 }

/******************************************************************************
 * IEEE 802.15.4 MHR length for the current addressing / ACK settings.
 *
 * Layout: 2-byte FCF + 1-byte sequence number + dest PAN (2) + dest short
 * address (2) = 7. Source addressing adds a 2-byte short address, and a
 * 2-byte source PAN unless PAN ID compression is enabled.
 * Returns 0 when IEEE 802.15.4 MAC framing is disabled (proprietary mode).
 *****************************************************************************/
uint16_t get_ieee802154_mhr_length(void)
{
  uint16_t mhr_length = 0U;

  if (!ieee802154_settings.enabled) {
    return 0U;
  }

  mhr_length = 7U;
  if (ieee802154_settings.source_addressing) {
    mhr_length += ieee802154_settings.pan_id_compression ? 2U : 4U;
  }

  return mhr_length;
}

/******************************************************************************
 * Maximum IEEE 802.15.4 PSDU length encoded by the current PHY PHR.
 *
 * 1-byte PHR uses a 7-bit length (classic 127-byte PSDU). A 2-byte or longer
 * PHR codes length in 11 bits.
 *****************************************************************************/
uint16_t get_ieee802154_max_frame_length(void)
{
  int8_t phr_length = sl_packet_assistant_get_phr_length();

  if (phr_length >= 2) {
    return (uint16_t)(1U << 11);
  }

  return (uint16_t)(1U << 7);
}

/******************************************************************************
 * Application goodput in bits per second over the measurement window.
 *
 * The configured frame length is the PSDU written to the packet assistant
 * (MHR + MSDU). The PHR is prepended by the packet assistant and is not
 * part of that length; hardware FCS is also excluded. Goodput counts only
 * MSDU bytes: frame_length minus MHR when IEEE 802.15.4 framing is enabled.
 *****************************************************************************/
static uint32_t calculate_goodput(uint32_t n_packets)
{
  uint16_t frame_length = get_measurement_payload_length();
  uint16_t mhr_length = get_ieee802154_mhr_length();
  uint16_t payload_length;
  uint64_t payload_bits;
  uint32_t period_sec = (uint32_t)THROUGHPUT_MEASUREMENT_PERIOD_SEC;

  if (frame_length < mhr_length) {
    payload_length = 0U;
  } else {
    payload_length = (uint16_t)(frame_length - mhr_length);
  }

  payload_bits = (uint64_t)n_packets * (uint64_t)payload_length * 8U;

  if (period_sec == 0U) {
    return 0U;
  }

  return (uint32_t)(payload_bits / period_sec);
}

/******************************************************************************
 * Fill a buffer with an ITU-T O.150 PN9 sequence (x^9 + x^5 + 1, seed 0x1FF)
 *****************************************************************************/
static void fill_pn9_sequence(uint8_t *buffer, uint16_t length)
{
  uint16_t lfsr = 0x01FFU;

  for (uint16_t i = 0U; i < length; i++) {
    uint8_t byte = 0U;
    for (uint8_t bit = 0U; bit < 8U; bit++) {
      uint8_t out_bit = (uint8_t)(lfsr & 0x01U);
      byte |= (uint8_t)(out_bit << bit);
      uint16_t feedback = (uint16_t)((lfsr ^ (lfsr >> 5)) & 0x01U);
      lfsr = (uint16_t)((lfsr >> 1) | (feedback << 8));
    }
    buffer[i] = byte;
  }
}

/******************************************************************************
 * Write an IEEE 802.15.4 MAC header at the start of the measurement buffer.
 *
 * The remainder of the buffer is left as the PN9 payload. Addressing, ACK
 * request, and PAN ID compression come from ieee802154_settings.
 *****************************************************************************/
static sl_status_t create_ieee802154_mac_frame(uint8_t *payload)
{
  uint16_t frame_control = 0U;
  uint16_t frame_length = get_measurement_payload_length();
  uint16_t header_length = get_ieee802154_mhr_length();
  uint16_t index = 0U;

  if (payload == NULL) {
    return SL_STATUS_NULL_POINTER;
  }

  frame_control |= IEEE802154_FCF_FRAME_TYPE_DATA;
  if (ieee802154_settings.ack_enabled) {
    frame_control |= IEEE802154_FCF_ACK_REQUEST;
  }
  if (ieee802154_settings.source_addressing) {
    frame_control |= IEEE802154_FCF_SRC_ADDR_SHORT;
    if (ieee802154_settings.pan_id_compression) {
      frame_control |= IEEE802154_FCF_PAN_ID_COMP;
    }
  }
  frame_control |= IEEE802154_FCF_DEST_ADDR_SHORT;

  uint16_t max_frame_length = (uint16_t)(get_ieee802154_max_frame_length()
                                        - header_length
                                        - 4U); // assumed CRC length

  if ((frame_length < header_length)
      || (frame_length > max_frame_length)) {
    app_log_error("IEEE 802.15.4 frame length must be %" PRIu16
                  "..%" PRIu16 " bytes for the selected addressing mode\n",
                  header_length,
                  max_frame_length);
    return SL_STATUS_INVALID_PARAMETER;
  }

  sequence_number = n_packets_sent % 256;  // as the sequence number is one byte in size

  // FCF: data frame, security/frame-pending disabled, frame version 00.
  payload[index++] = (uint8_t)frame_control;
  payload[index++] = (uint8_t)(frame_control >> 8);
  payload[index++] = sequence_number;

  // Destination addressing is always short and always carries the PAN ID.
  payload[index++] = (uint8_t)ieee802154_settings.pan_id;
  payload[index++] = (uint8_t)(ieee802154_settings.pan_id >> 8);
  payload[index++] = (uint8_t)ieee802154_settings.rx_address;
  payload[index++] = (uint8_t)(ieee802154_settings.rx_address >> 8);

  if (ieee802154_settings.source_addressing) {
    if (!ieee802154_settings.pan_id_compression) {
      payload[index++] = (uint8_t)ieee802154_settings.pan_id;
      payload[index++] = (uint8_t)(ieee802154_settings.pan_id >> 8);
    }
    payload[index++] = (uint8_t)ieee802154_settings.tx_address;
    payload[index++] = (uint8_t)(ieee802154_settings.tx_address >> 8);
  }

  return SL_STATUS_OK;
}


/******************************************************************************
 * Prepare payload and start TX
 *****************************************************************************/
static sl_status_t prepare_and_send_packet(uint8_t *payload, uint16_t length)
{
  sl_rail_status_t rail_status;
  sl_rail_tx_options_t tx_options = SL_RAIL_TX_OPTIONS_DEFAULT;

  if (ieee802154_settings.enabled) {
    sl_status_t status = create_ieee802154_mac_frame(payload);
    if (status != SL_STATUS_OK) {
      app_log_error("create_ieee802154_mac_frame() failed: 0x%08" PRIX32 "\n", status);
      return status;
    }
  }

  sl_status_t status = sl_packet_assistant_prepare_packet(rail_handle, payload, length);
  if (status != SL_STATUS_OK) {
    app_log_error("sl_packet_assistant_prepare_packet() failed: 0x%08" PRIX32 "\n", status);
    return status;
  }

  if (ieee802154_settings.enabled && (ieee802154_settings.ack_timeout_us != 0U)) {
    tx_options |= SL_RAIL_TX_OPTION_WAIT_FOR_ACK;
  }

  if (csma_settings.enabled) {
    rail_status = sl_rail_start_cca_csma_tx(rail_handle,
                                            channel_measurement,
                                            tx_options,
                                            &csma_settings.config,
                                            NULL);
  } else {
    rail_status = sl_rail_start_tx(rail_handle,
                                   channel_measurement,
                                   tx_options,
                                   NULL);
  }
  if (rail_status != SL_RAIL_STATUS_NO_ERROR) {
    return SL_STATUS_FAIL;
  }

  return SL_STATUS_OK;
}

/******************************************************************************
 * Prepare a command with the packet assistant (adds a PHR if the PHY uses one)
 * and start TX on the command channel.
 * Every command is sent at CMD_PACKET_LENGTH; unused bytes are zero-padded.
 *****************************************************************************/
static sl_status_t send_command(const uint8_t *payload, uint16_t length)
{
  sl_rail_status_t rail_status;
  sl_status_t status;

  if ((payload == NULL) || (length == 0U) || (length > CMD_PACKET_LENGTH)) {
    return SL_STATUS_INVALID_PARAMETER;
  }
  memset(tx_cmd_buffer, 0, get_cmd_payload_length());
  memcpy(tx_cmd_buffer, payload, length);

  status = sl_packet_assistant_prepare_packet(rail_handle, tx_cmd_buffer, get_cmd_payload_length());
  if (status != SL_STATUS_OK) {
    app_log_error("sl_packet_assistant_prepare_packet() failed: 0x%08" PRIX32 "\n",
                  (uint32_t)status);
    return status;
  }

  rail_status = sl_rail_start_tx(rail_handle, channel_commands, SL_RAIL_TX_OPTIONS_DEFAULT, NULL);
  if (rail_status != SL_RAIL_STATUS_NO_ERROR) {
    app_log_error("sl_rail_start_tx failed: 0x%"PRIX32"\n", rail_status);
    return SL_STATUS_FAIL;
  }

  return SL_STATUS_OK;
}

/******************************************************************************
 * Apply stored IEEE 802.15.4 settings to RAIL
 *****************************************************************************/
static sl_status_t apply_ieee802154_settings(void)
{
  sl_rail_status_t status;
  sl_rail_ieee802154_addr_config_t addresses;
  sl_rail_ieee802154_config_t config;
  uint16_t own_short_addr;

  (void)sl_rail_idle(rail_handle, SL_RAIL_IDLE, true);

  if (!ieee802154_settings.enabled) {
    status = sl_rail_ieee802154_deinit(rail_handle);
    if (status != SL_RAIL_STATUS_NO_ERROR) {
      app_log_error("sl_rail_ieee802154_deinit() failed: 0x%" PRIX32 "\n",
                    (uint32_t)status);
      return SL_STATUS_FAIL;
    }
    return SL_STATUS_OK;
  }

  // Filter on this node's short address for the active role
  own_short_addr = (app_role == APP_ROLE_RECEIVER)
                   ? ieee802154_settings.rx_address
                   : ieee802154_settings.tx_address;

  memset(&addresses, 0, sizeof(addresses));
  addresses.pan_id[0] = ieee802154_settings.pan_id;
  addresses.pan_id[1] = 0xFFFFU;
  addresses.pan_id[2] = 0xFFFFU;
  addresses.short_addr[0] = own_short_addr;
  addresses.short_addr[1] = 0xFFFFU;
  addresses.short_addr[2] = 0xFFFFU;

  config = (sl_rail_ieee802154_config_t){
    .p_addresses = &addresses,
    .ack_config = {
      .enable = ieee802154_settings.ack_enabled,
      .ack_timeout_us = ieee802154_settings.ack_timeout_us,
      .rx_transitions = {
        .success = SL_RAIL_RF_STATE_RX,
        .error = SL_RAIL_RF_STATE_RX,
      },
      .tx_transitions = {
        .success = SL_RAIL_RF_STATE_RX,
        .error = SL_RAIL_RF_STATE_RX,
      },
    },
    .timings = {
      .idle_to_rx = 100,
      .idle_to_tx = 100,
      .rx_to_tx = THROUGHPUT_IEEE802154_RX_TO_TX_DELAY_US,
      .tx_to_rx = THROUGHPUT_IEEE802154_RX_TO_TX_DELAY_US - 10,
      .rxsearch_timeout = 0,
      .tx_to_rxsearch_timeout = 0,
    },
    .frames_mask = SL_RAIL_IEEE802154_ACCEPT_STANDARD_FRAMES,
    .promiscuous_mode = false,
    .is_pan_coordinator = false,
    .default_frame_pending_in_outgoing_acks = false,
  };

  status = sl_rail_ieee802154_init(rail_handle, &config);
  if (status != SL_RAIL_STATUS_NO_ERROR) {
    app_log_error("sl_rail_ieee802154_init() failed: 0x%" PRIX32 "\n", status);
    return SL_STATUS_FAIL;
  }

  // 802.15.4g PHYs need the GB868 option enabled after init
  if (sl_packet_assistant_get_phr_length() == 2) {
    status = sl_rail_ieee802154_config_g_options(rail_handle,
                                                SL_RAIL_IEEE802154_G_OPTION_GB868,
                                                SL_RAIL_IEEE802154_G_OPTION_GB868);
    if (status != SL_RAIL_STATUS_NO_ERROR) {
      app_log_error("sl_rail_ieee802154_config_g_options() failed: 0x%" PRIX32 "\n",
                    (uint32_t)status);
      return SL_STATUS_FAIL;
    }
  }

  return SL_STATUS_OK;
}

/******************************************************************************
 * Start (or restart) the command ACK wait timer
 *****************************************************************************/
static sl_status_t start_cmd_timer(uint32_t timeout_ms)
{
  cmd_timeout = false;
  return sl_sleeptimer_start_timer_ms(&cmd_ack_timer,
                                      timeout_ms,
                                      cmd_ack_timer_callback,
                                      NULL,
                                      0U,
                                      0U);
}

/******************************************************************************
 * Stop the command ACK wait timer
 *****************************************************************************/
static sl_status_t stop_cmd_timer(void)
{
  cmd_timeout = false;
  return sl_sleeptimer_stop_timer(&cmd_ack_timer);
}

/******************************************************************************
 * Command ACK timeout callback: signal the state machine to resend
 *****************************************************************************/
SL_CODE_RAM static void cmd_ack_timer_callback(sl_sleeptimer_timer_handle_t *handle,
                                               void *data)
{
  (void)handle;
  (void)data;

  cmd_timeout = true;
#if defined(SL_CATALOG_KERNEL_PRESENT)
  app_task_notify();
#endif
}

/******************************************************************************
 * Start the measurement window timer
 *****************************************************************************/
static sl_status_t start_measurement_timer(uint16_t extra_ms)
{
  measurement_over = false;
  return sl_sleeptimer_start_timer_ms(&measurement_timer,
                                      (uint16_t)(THROUGHPUT_MEASUREMENT_PERIOD_SEC * 1000U
                                                 + extra_ms),
                                      measurement_timer_callback,
                                      NULL,
                                      0U,
                                      0U);
}

/******************************************************************************
 * Stop the measurement window timer
 *****************************************************************************/
static sl_status_t stop_measurement_timer(void)
{
  measurement_over = false;
  return sl_sleeptimer_stop_timer(&measurement_timer);
}

/******************************************************************************
 * Measurement period elapsed callback
 *****************************************************************************/
SL_CODE_RAM static void measurement_timer_callback(sl_sleeptimer_timer_handle_t *handle,
                                                   void *data)
{
  (void)handle;
  (void)data;

  measurement_over = true;
#if defined(SL_CATALOG_KERNEL_PRESENT)
  app_task_notify();
#endif
}
