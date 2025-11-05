/***************************************************************************//**
 * @file app_decode.c
 * @brief Bluetooth Mesh message decoder example
 *******************************************************************************
 * # License
 * <b>Copyright 2025 Silicon Laboratories Inc. www.silabs.com</b>
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
// Includes
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "app_assert.h"
#include "app_log.h"
#include "sl_mesh_decoder.h"
#include "sl_mesh_decoder_config.h"
#include "app_decoder_log.h"
#include "config/decoder_app_config.h"

#include "sl_btmesh_sensor_model_capi_types.h"
#include "sl_btmesh_model_specification_defs.h"

// -----------------------------------------------------------------------------
// Enums, structs, typedefs

// Structure representing an entry in the Replay Protection List
typedef struct {
  uint16_t src;       // Source address
  uint64_t ivi_seq;   // IVISeq value
} rpl_entry_t;

// Replay Protection List to store the last sequence number for each source address
static rpl_entry_t replay_protection_list[MAX_RPL_ENTRIES];

// -----------------------------------------------------------------------------
// Private function definitions

/*******************************************************************************
 * This function determines if a received message is a replay attack by comparing
 * the source address, sequence number, and IV Index against the entries in the
 * Replay Protection List (RPL). The IVISeq value is constructed from the IV Index
 * and the sequence number. If the source address and IVISeq value indicate a replay,
 * the function returns true. Otherwise, it updates the RPL with the new IVISeq value
 * or adds a new entry if the source address is not present.
 ******************************************************************************/
static bool is_replay_attack(net_pdu_t net_pdu)
{
  // IVISeq value is 7 octets, where the IV Index is the four most significant
  // octets and the sequence number is the three least significant octets
  uint64_t ivi_seq = (((uint64_t)net_pdu.ivi) << 24) | (net_pdu.seq & 0xFFFFFF);

  for (int i = 0; i < MAX_RPL_ENTRIES; i++) {
    if (replay_protection_list[i].src == net_pdu.src) {
      if (ivi_seq <= replay_protection_list[i].ivi_seq) {
        return true;
      } else {
        // Update the sequence number
        replay_protection_list[i].ivi_seq = ivi_seq;
        return false;
      }
    }
  }

  // If the address is not in the list, add it
  for (int i = 0; i < MAX_RPL_ENTRIES; i++) {
    // Find an empty slot in the replay protection list
    if (replay_protection_list[i].src == 0) {
      replay_protection_list[i].src = net_pdu.src;
      replay_protection_list[i].ivi_seq = ivi_seq;
      return false;
    }
  }

  return false;
}

/*******************************************************************************
 * Decodes transport control messages based on the provided PDU data.
 *
 * This function processes various Bluetooth Mesh transport control messages by
 * decoding the PDU data and invoking the appropriate logging and handler functions
 * for each control opcode. It supports all standard control opcodes defined in
 * the Bluetooth Mesh specification.
 ******************************************************************************/
static sl_status_t decode_transport_control_messages(struct sl_mesh_decoder *context,
                                                     sl_mesh_key_handle net_key,
                                                     net_pdu_t net_pdu,
                                                     trans_pdu_t trans_pdu)
{
  sl_status_t sc;
  switch (trans_pdu.opcode) {
    case sl_mesh_control_opcode_segack:
      segack_t segack = { 0 };
      sl_mesh_segack_from_buf(trans_pdu.segment, &segack);
#if DECODER_APP_UPPER_TRANSPORT_LAYER_CONTROL_LOG
      app_segack_log(segack);
#endif // DECODER_APP_UPPER_TRANSPORT_LAYER_CONTROL_LOG
      break;
    case sl_mesh_control_opcode_friend_poll:
      friend_poll_t friend_poll = { 0 };
      sl_mesh_friend_poll_from_buf(trans_pdu.segment, &friend_poll);
#if DECODER_APP_UPPER_TRANSPORT_LAYER_CONTROL_LOG
      app_friend_poll_log(friend_poll);
#endif // DECODER_APP_UPPER_TRANSPORT_LAYER_CONTROL_LOG
      break;
    case sl_mesh_control_opcode_friend_update:
      friend_update_t friend_update = { 0 };
      sl_mesh_friend_update_from_buf(trans_pdu.segment, &friend_update);
#if DECODER_APP_UPPER_TRANSPORT_LAYER_CONTROL_LOG
      app_friend_update_log(friend_update);
#endif // DECODER_APP_UPPER_TRANSPORT_LAYER_CONTROL_LOG
      break;
    case sl_mesh_control_opcode_friend_request:
      friend_request_t friend_request = { 0 };
      sl_mesh_friend_request_from_buf(trans_pdu.segment, &friend_request);
#if DECODER_APP_UPPER_TRANSPORT_LAYER_CONTROL_LOG
      app_friend_request_log(friend_request);
#endif // DECODER_APP_UPPER_TRANSPORT_LAYER_CONTROL_LOG
#if DECODE_FRIENDSHIP
      sc = sl_mesh_friendship_add_friend_request(context, net_key, net_pdu.src, friend_request.lpn_counter);
      return sc;
#endif // DECODE_FRIENDSHIP
      break;
    case sl_mesh_control_opcode_friend_offer:
      friend_offer_t friend_offer = { 0 };
      sl_mesh_friend_offer_from_buf(trans_pdu.segment, &friend_offer);
#if DECODER_APP_UPPER_TRANSPORT_LAYER_CONTROL_LOG
      app_friend_offer_log(friend_offer);
#endif // DECODER_APP_UPPER_TRANSPORT_LAYER_CONTROL_LOG
#if DECODE_FRIENDSHIP
      sc = sl_mesh_friendship_add_friend_offer(context, net_key, net_pdu.dst, net_pdu.src, friend_offer.friend_counter);
      return sc;
#endif // DECODE_FRIENDSHIP
      break;
    case sl_mesh_control_opcode_friend_clear:
      friend_clear_t friend_clear = { 0 };
      sl_mesh_friend_clear_from_buf(trans_pdu.segment, &friend_clear);
#if DECODER_APP_UPPER_TRANSPORT_LAYER_CONTROL_LOG
      app_friend_clear_log(friend_clear);
#endif // DECODER_APP_UPPER_TRANSPORT_LAYER_CONTROL_LOG
#if DECODE_FRIENDSHIP
      sl_mesh_key_handle cleared_key;
      if (sl_mesh_decoder_get_friendship_key(context,
                                             friend_clear.lpn_address,
                                             friend_clear.lpn_counter,
                                             &cleared_key) == SL_STATUS_OK) {
        sc = sl_mesh_decoder_remove_key(context, cleared_key);
        return sc;
      }
#endif // DECODE_FRIENDSHIP
      break;
    case sl_mesh_control_opcode_friend_subscription_add:
    case sl_mesh_control_opcode_friend_subscription_remove:
      friend_subscription_t friend_subscription = { 0 };
      sl_mesh_friend_subscription_from_buf(trans_pdu.segment, &friend_subscription);
#if DECODER_APP_UPPER_TRANSPORT_LAYER_CONTROL_LOG
      app_friend_subscription_log(trans_pdu, friend_subscription);
#endif // DECODER_APP_UPPER_TRANSPORT_LAYER_CONTROL_LOG
      break;
    case sl_mesh_control_opcode_friend_subscription_confirm:
      uint8_t tid = trans_pdu.segment[0];
#if DECODER_APP_UPPER_TRANSPORT_LAYER_CONTROL_LOG
      app_friend_subscription_confirm_log(tid);
#endif // DECODER_APP_UPPER_TRANSPORT_LAYER_CONTROL_LOG
      break;
    case sl_mesh_control_opcode_heartbeat:
      heartbeat_t heartbeat = { 0 };
      sl_mesh_heartbeat_from_buf(trans_pdu.segment, &heartbeat);
#if DECODER_APP_UPPER_TRANSPORT_LAYER_CONTROL_LOG
      app_heartbeat_log(heartbeat);
#endif // DECODER_APP_UPPER_TRANSPORT_LAYER_CONTROL_LOG
      break;
    case sl_mesh_control_opcode_path_request:
      path_request_t path_request = { 0 };
      sl_mesh_path_request_from_buf(trans_pdu.segment, &path_request);
#if DECODER_APP_UPPER_TRANSPORT_LAYER_CONTROL_LOG
      app_path_request_log(path_request);
#endif // DECODER_APP_UPPER_TRANSPORT_LAYER_CONTROL_LOG
      break;
    case sl_mesh_control_opcode_path_reply:
      path_reply_t path_reply = { 0 };
      sl_mesh_path_reply_from_buf(trans_pdu.segment, &path_reply);
#if DECODER_APP_UPPER_TRANSPORT_LAYER_CONTROL_LOG
      app_path_reply_log(path_reply);
#endif // DECODER_APP_UPPER_TRANSPORT_LAYER_CONTROL_LOG
      break;
    case sl_mesh_control_opcode_path_confirmation:
      path_confirmation_t path_confirmation = { 0 };
      sl_mesh_path_confirmation_from_buf(trans_pdu.segment, &path_confirmation);
#if DECODER_APP_UPPER_TRANSPORT_LAYER_CONTROL_LOG
      app_path_confirmation_log(path_confirmation);
#endif // DECODER_APP_UPPER_TRANSPORT_LAYER_CONTROL_LOG
      break;
    case sl_mesh_control_opcode_path_echo_request:
#if DECODER_APP_UPPER_TRANSPORT_LAYER_CONTROL_LOG
      app_path_echo_request_log();
#endif // DECODER_APP_UPPER_TRANSPORT_LAYER_CONTROL_LOG
      break;
    case sl_mesh_control_opcode_path_echo_reply:
      uint16_t path_dst = (trans_pdu.segment[0] << 8) | trans_pdu.segment[1];
#if DECODER_APP_UPPER_TRANSPORT_LAYER_CONTROL_LOG
      app_path_echo_reply_log(path_dst);
#endif // DECODER_APP_UPPER_TRANSPORT_LAYER_CONTROL_LOG
      break;
    case sl_mesh_control_opcode_dependent_node_update:
      dependent_node_update_t dependent_node_update = { 0 };
      sl_mesh_dependent_node_update_from_buf(trans_pdu.segment, &dependent_node_update);
#if DECODER_APP_UPPER_TRANSPORT_LAYER_CONTROL_LOG
      app_path_dependent_node_update_log(dependent_node_update);
#endif // DECODER_APP_UPPER_TRANSPORT_LAYER_CONTROL_LOG
      break;
    case sl_mesh_control_opcode_path_request_solicitation:
      path_request_solicitation_t path_request_solicitation = { 0 };
      sl_mesh_path_request_solicitation_from_buf(trans_pdu.segment, &path_request_solicitation);
#if DECODER_APP_UPPER_TRANSPORT_LAYER_CONTROL_LOG
      app_path_path_request_solicitation_log(trans_pdu, path_request_solicitation);
#endif // DECODER_APP_UPPER_TRANSPORT_LAYER_CONTROL_LOG
      break;
    default:
      return SL_STATUS_INVALID_PARAMETER;
      break;
  }

  return SL_STATUS_OK;
}

static sl_status_t rx_trans_control(struct sl_mesh_decoder *context,
                                    sl_mesh_key_handle net_key,
                                    net_pdu_t net_pdu,
                                    trans_pdu_t trans_pdu)
{
  bool valid = sl_mesh_pointer_contains_valid_control_message(trans_pdu.opcode, trans_pdu.segment, trans_pdu.segment_len);
  if (!valid) {
    app_log_error("Failed to validate Control Message. " APP_LOG_NL);
    return SL_STATUS_INVALID_PARAMETER;
  }

  // Replay protection
  if (is_replay_attack(net_pdu)) {
    return SL_STATUS_INVALID_PARAMETER;
  }

#if DECODER_APP_NETWORK_LAYER_LOG
  app_network_layer_log(net_pdu);
#endif // DECODER_APP_NETWORK_LAYER_LOG

#if DECODER_APP_LOWER_TRANSPORT_LAYER_LOG
  app_lower_transport_layer_log(trans_pdu, net_pdu.ctl);
#endif // DECODER_APP_LOWER_TRANSPORT_LAYER_LOG

#if DECODER_APP_UPPER_TRANSPORT_LAYER_CONTROL_LOG
  app_log(TRANS_LAYER_LOG_PREFIX "Control Opcode: 0x%02x (%s)" APP_LOG_NL, trans_pdu.opcode,
          control_opcode_to_str(trans_pdu.opcode));
  decode_transport_control_messages(context, net_key, net_pdu, trans_pdu);
#endif // DECODER_APP_UPPER_TRANSPORT_LAYER_CONTROL_LOG

  return SL_STATUS_OK;
}

static sl_status_t collect_trans_control(struct sl_mesh_decoder *context,
                                         sl_mesh_key_handle net_key,
                                         net_pdu_t *net_pdu,
                                         trans_pdu_t *trans_pdu)
{
  bool now_completed = false;
  sl_status_t sc = sl_mesh_reassembler_add_segment(context,
                                                   net_key,
                                                   *net_pdu,
                                                   *trans_pdu,
                                                   &now_completed);
  if (sc == SL_STATUS_OK && now_completed) {
    const uint8_t *segment_ptr = NULL;
    size_t segment_len = 0;
    uint32_t seq_auth = 0;
    sc = sl_mesh_reassembler_get_data(context,
                                      net_pdu->src,
                                      net_pdu->dst,
                                      trans_pdu->seq_0,
                                      &segment_ptr,
                                      &segment_len,
                                      &seq_auth);
    if (sc == SL_STATUS_OK) {
      trans_pdu->segment = segment_ptr;
      trans_pdu->segment_len = segment_len;
      net_pdu->seq = seq_auth;
      sc = rx_trans_control(context,
                            net_key,
                            *net_pdu,
                            *trans_pdu);
    }
  }

  return sc;
}

/*******************************************************************************
 * Decodes sensor model messages based on the provided opcode and PDU data.
 *
 * This function processes various sensor model messages by decoding the PDU data
 * and invoking appropriate logging functions. It supports all standard
 * sensor model message opcodes defined in the Bluetooth Mesh specification.
 ******************************************************************************/
static sl_status_t decode_sensor_model_messages(const uint8_t *pdu_data,
                                                const size_t pdu_data_len,
                                                const uint16_t opcode)
{
  sl_status_t sc = SL_STATUS_OK;

  switch (opcode) {
    case SENSOR_DESCRIPTOR_GET:
    case SENSOR_CADENCE_GET:
    case SENSOR_SETTINGS_GET:
    case SENSOR_GET:
#if DECODER_APP_SENSOR_MODEL_LOG
      app_sensor_get_log(pdu_data, pdu_data_len);
#endif // DECODER_APP_SENSOR_MODEL_LOG
      break;
    case SENSOR_DESCRIPTOR_STATUS:
      sensor_descriptor_t descriptor[SL_MESH_SENSOR_ARRAY_SIZE] = { 0 };
      sc = sl_mesh_sensor_descriptors_from_buf(descriptor, (uint8_t*)pdu_data, pdu_data_len);
      if (sc != SL_STATUS_OK) {
        return sc;
      }
#if DECODER_APP_SENSOR_MODEL_LOG
      app_sensor_descriptors_log(descriptor, pdu_data_len);
#endif // DECODER_APP_SENSOR_MODEL_LOG
      break;
    case SENSOR_CADENCE_SET_UNACK:
    case SENSOR_CADENCE_SET:
    case SENSOR_CADENCE_STATUS:
      struct sensort_cadence_state cadence = { 0 };
      sc = sl_mesh_sensor_cadence_from_buf(&cadence, (uint8_t*)pdu_data, pdu_data_len);
      if (sc != SL_STATUS_OK) {
        return sc;
      }
#if DECODER_APP_SENSOR_MODEL_LOG
      app_sensor_cadence_log(&cadence, pdu_data_len);
#endif // DECODER_APP_SENSOR_MODEL_LOG
      break;
    case SENSOR_SETTINGS_STATUS:
      sensor_settings_status_t settings_status = { 0 };
      sc = sl_mesh_sensor_settings_status_from_buf(&settings_status, (uint8_t*)pdu_data, pdu_data_len);
      if (sc != SL_STATUS_OK) {
        return sc;
      }
#if DECODER_APP_SENSOR_MODEL_LOG
      app_sensor_settings_status_log(&settings_status, pdu_data_len);
#endif // DECODER_APP_SENSOR_MODEL_LOG
      break;
    case SENSOR_SETTING_GET:
      sensor_get_setting_msg_t setting_get = { 0 };
      sc = sl_mesh_sensor_setting_get_from_buf(&setting_get, (uint8_t*)pdu_data, pdu_data_len);
      if (sc != SL_STATUS_OK) {
        return sc;
      }
#if DECODER_APP_SENSOR_MODEL_LOG
      app_sensor_setting_get_log(&setting_get, pdu_data_len);
#endif // DECODER_APP_SENSOR_MODEL_LOG
      break;
    case SENSOR_SETTING_SET_UNACK:
    case SENSOR_SETTING_SET:
      sensor_set_setting_msg_t setting_set = { 0 };
      sc = sl_mesh_sensor_setting_set_from_buf(&setting_set, (uint8_t*)pdu_data, pdu_data_len);
      if (sc != SL_STATUS_OK) {
        return sc;
      }
#if DECODER_APP_SENSOR_MODEL_LOG
      app_sensor_setting_set_log(&setting_set);
#endif // DECODER_APP_SENSOR_MODEL_LOG
      break;
    case SENSOR_SETTING_STATUS:
      struct sensor_setting_status setting_status = { 0 };
      sc = sl_mesh_sensor_setting_status_from_buf(&setting_status, (uint8_t*)pdu_data, pdu_data_len);
      if (sc != SL_STATUS_OK) {
        return sc;
      }
#if DECODER_APP_SENSOR_MODEL_LOG
      app_sensor_setting_status_log(&setting_status, pdu_data_len);
#endif // DECODER_APP_SENSOR_MODEL_LOG
      break;
    case SENSOR_STATUS:
      sensor_status_msg_t sensor_status[SL_MESH_SENSOR_ARRAY_SIZE] = { 0 };
      sc = sl_mesh_sensor_status_from_buf(sensor_status, (uint8_t*)pdu_data, pdu_data_len);
      if (sc != SL_STATUS_OK) {
        return sc;
      }
#if DECODER_APP_SENSOR_MODEL_LOG
      app_sensor_status_log(sensor_status, pdu_data_len);
#endif // DECODER_APP_SENSOR_MODEL_LOG
      break;
    case SENSOR_COLUMN_STATUS:
      sensor_column_status_msg_t column_status = { 0 };
      sc = sl_mesh_sensor_column_status_from_buf(&column_status, (uint8_t*)pdu_data, pdu_data_len);
      if (sc != SL_STATUS_OK) {
        return sc;
      }
#if DECODER_APP_SENSOR_MODEL_LOG
      app_sensor_column_status_log(&column_status, pdu_data_len);
#endif // DECODER_APP_SENSOR_MODEL_LOG
      break;
    case SENSOR_COLUMN_GET:
      sensor_get_column_msg_t column_get = { 0 };
      sc = sl_mesh_sensor_column_get_from_buf(&column_get, (uint8_t*)pdu_data, pdu_data_len);
      if (sc != SL_STATUS_OK) {
        return sc;
      }
#if DECODER_APP_SENSOR_MODEL_LOG
      app_sensor_column_get_log(&column_get);
#endif // DECODER_APP_SENSOR_MODEL_LOG
      break;
    case SENSOR_SERIES_GET:
      sensor_get_series_msg_t series_get = { 0 };
      sc = sl_mesh_sensor_series_get_from_buf(&series_get, (uint8_t*)pdu_data, pdu_data_len);
      if (sc != SL_STATUS_OK) {
        return sc;
      }
#if DECODER_APP_SENSOR_MODEL_LOG
      app_sensor_series_get_log(&series_get, pdu_data_len);
#endif // DECODER_APP_SENSOR_MODEL_LOG
      break;
    case SENSOR_SERIES_STATUS:
      sensor_series_status_msg_t series_status[SL_MESH_SENSOR_ARRAY_SIZE] = { 0 };
      sc = sl_mesh_sensor_series_status_from_buf(series_status, (uint8_t*)pdu_data, pdu_data_len);
      if (sc != SL_STATUS_OK) {
        return sc;
      }
#if DECODER_APP_SENSOR_MODEL_LOG
      app_sensor_series_status_log(series_status, pdu_data_len);
#endif // DECODER_APP_SENSOR_MODEL_LOG
      break;
    default:
      return SL_STATUS_INVALID_PARAMETER;
      break;
  }
  return SL_STATUS_OK;
}

static sl_status_t rx_trans_access(struct sl_mesh_decoder *context,
                                   uint32_t current_ivindex,
                                   sl_mesh_key_handle net_key,
                                   net_pdu_t net_pdu,
                                   trans_pdu_t trans_pdu)
{
  if (!sl_mesh_pointer_contains_valid_access_pdu(trans_pdu.szmic, trans_pdu.segment, trans_pdu.segment_len)) {
    app_log_error("Failed to decode Access PDU. " APP_LOG_NL);
    return SL_STATUS_INVALID_PARAMETER;
  }

  // Replay protection
  if (is_replay_attack(net_pdu)) {
    return SL_STATUS_INVALID_PARAMETER;
  }

  sl_status_t sc;

  struct sl_mesh_access_pdu pdu;
  sl_mesh_key_handle app_key;
  const uint8_t *label_uuid;

  sc = sl_mesh_decoder_decrypt_access_pdu(context,
                                          current_ivindex,
                                          &pdu,
                                          &app_key,
                                          &label_uuid,
                                          net_pdu,
                                          trans_pdu);
  if (sc) {
    return sc;
  }

  bool valid = sl_mesh_access_pdu_contains_valid_access_message(&pdu);
  if (!valid) {
    return SL_STATUS_INVALID_PARAMETER;
  }

  uint16_t opcode = (sl_mesh_access_pdu_opcode_len(&pdu) == 1)
                    ? sl_mesh_access_pdu_opcode_ptr(&pdu)[0]
                    : (sl_mesh_access_pdu_opcode_ptr(&pdu)[0] << 8) | sl_mesh_access_pdu_opcode_ptr(&pdu)[1];

#if DECODER_APP_NETWORK_LAYER_LOG
  app_network_layer_log(net_pdu);
#endif // DECODER_APP_NETWORK_LAYER_LOG

#if DECODER_APP_LOWER_TRANSPORT_LAYER_LOG
  app_lower_transport_layer_log(trans_pdu, net_pdu.ctl);
#endif // DECODER_APP_LOWER_TRANSPORT_LAYER_LOG

#if DECODER_APP_SENSOR_MODEL_LOG
  app_log(SENSOR_MODEL_LOG_PREFIX "Opcode: 0x%04x (%s)" APP_LOG_NL, opcode, sensor_model_opcode_to_str(opcode));
  sc = decode_sensor_model_messages(sl_mesh_access_pdu_data_ptr(&pdu), sl_mesh_access_pdu_data_len(&pdu), opcode);
  if (sc != SL_STATUS_OK) {
    app_log_error("Failed to decode sensor model message: %d" APP_LOG_NL, sc);
    return sc;
  }
#endif // DECODER_APP_SENSOR_MODEL_LOG

  return SL_STATUS_OK;
}

static sl_status_t collect_trans_access(struct sl_mesh_decoder *context,
                                        uint32_t current_ivindex,
                                        sl_mesh_key_handle net_key,
                                        net_pdu_t *net_pdu,
                                        trans_pdu_t *trans_pdu)
{
  bool now_completed = false;
  sl_status_t sc = sl_mesh_reassembler_add_segment(context,
                                                   net_key,
                                                   *net_pdu,
                                                   *trans_pdu,
                                                   &now_completed);
  if (sc == SL_STATUS_OK && now_completed) {
    const uint8_t *segment_ptr = NULL;
    size_t segment_len = 0;
    uint32_t seq_auth = 0;
    sc = sl_mesh_reassembler_get_data(context,
                                      net_pdu->src,
                                      net_pdu->dst,
                                      trans_pdu->seq_0,
                                      &segment_ptr,
                                      &segment_len,
                                      &seq_auth);
    if (sc == SL_STATUS_OK) {
      trans_pdu->segment = segment_ptr;
      trans_pdu->segment_len = segment_len;
      net_pdu->seq = seq_auth;
      sc = rx_trans_access(context,
                           current_ivindex,
                           net_key,
                           *net_pdu,
                           *trans_pdu);
    }
  }

  return sc;
}

// -----------------------------------------------------------------------------
// Public function definitions

sl_status_t app_rx_net(struct sl_mesh_decoder *context,
                       uint32_t current_ivindex,
                       const uint8_t *ptr,
                       size_t len)
{
  sl_mesh_key_handle net_key, friend_key;
  struct sl_mesh_net_pdu pdu;
  sl_status_t sc;
  net_pdu_t net_pdu = { 0 };
  trans_pdu_t trans_pdu = { 0 };

  //
  // Could do something vendor specific with the Bluetooth address
  // (e.g. if it's a resolved private address) but ignoring it now
  // as there's noting in the Mesh specification for it.
  //

  sc = sl_mesh_decoder_decrypt_net_pdu(context, current_ivindex, ptr, len, &pdu, &net_key, &friend_key);
  if (sc) {
    // Decryption failure may indicate that the message does not belong to this
    // mesh network or uses an unknown key
    app_log_debug("Failed to decrypt network PDU. " APP_LOG_NL);
    return sc;
  }

#if DECODE_FRIENDSHIP
  if (friend_key != (void *)SL_MESH_KEY_HANDLE_INVALID) {
    struct sl_mesh_friendship friendship;
    sc = sl_mesh_decoder_get_friendship_key_context(context, friend_key, &friendship);
    if (sc) {
      return sc;
    }

    app_log("Key: 0x%04x LPNAddr: 0x%04x FriendAddr: 0x%04x LPNCounter: 0x%04x FriendCounter: 0x%04x" APP_LOG_NL,
            (unsigned)(intptr_t)friend_key,
            friendship.lpn_addr,
            friendship.friend_addr,
            friendship.lpn_counter,
            friendship.friend_counter);
  }
#endif

  sl_mesh_net_pdu_data(&pdu, &net_pdu);

  bool valid = sl_mesh_net_pdu_contains_valid_trans_pdu(&pdu);

  if (!valid) {
    app_log_error("Failed to validate Lower Transport PDU. " APP_LOG_NL);
    return SL_STATUS_INVALID_PARAMETER;
  }

  sl_mesh_trans_pdu_data(&pdu, &trans_pdu, net_pdu.ctl);

  if (sl_mesh_net_pdu_ctl(&pdu)) {
    if (sl_mesh_trans_pdu_seg(&pdu)) {
      sc = collect_trans_control(context,
                                 net_key,
                                 &net_pdu,
                                 &trans_pdu);
    } else {
      sc = rx_trans_control(context,
                            net_key,
                            net_pdu,
                            trans_pdu);
    }
  } else {
    if (sl_mesh_trans_pdu_seg(&pdu)) {
      sc = collect_trans_access(context,
                                current_ivindex,
                                net_key,
                                &net_pdu,
                                &trans_pdu);
    } else {
      sc = rx_trans_access(context,
                           current_ivindex,
                           net_key,
                           net_pdu,
                           trans_pdu);
    }
  }

  return sc;
}

sl_status_t app_rx_beacon(struct sl_mesh_decoder *context,
                          const uint8_t *ptr,
                          size_t len)
{
  enum sl_mesh_beacon type;
  uint8_t flags;
  uint32_t iv_index;
  sl_mesh_key_handle matching_net_key;

  sl_status_t sc = sl_mesh_decoder_authenticate_network_beacon(context,
                                                               ptr,
                                                               len,
                                                               &type,
                                                               &flags,
                                                               &iv_index,
                                                               &matching_net_key);
  if (sc != SL_STATUS_OK) {
    app_log_debug("Failed to authenticate beacon: %d" APP_LOG_NL, (int)sc);
    return sc;
  }

  app_log("Beacon received" APP_LOG_NL);

  app_log_append("Key:%04x Type:%01x KR:%01x IV:%01x IVIndex:%08x" APP_LOG_NL,
                 (unsigned)(intptr_t)matching_net_key,
                 type,
                 flags & 0x01,
                 (flags >> 1) & 0x01,
                 iv_index);
  app_log_append(SEPARATOR_LOG APP_LOG_NL);

  return SL_STATUS_OK;
}
