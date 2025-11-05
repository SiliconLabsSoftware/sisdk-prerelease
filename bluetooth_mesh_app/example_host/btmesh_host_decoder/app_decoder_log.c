/***************************************************************************//**
 * @file app_decoder_log.c
 * @brief Bluetooth Mesh Decoder log
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
#include "app_log.h"
#include "app_decoder_log.h"
#include "sl_mesh_decoder_util.h"
#include "sl_mesh_decoder.h"

// -----------------------------------------------------------------------------
// Macros

#define LINE_LEN 32

// -----------------------------------------------------------------------------
// Public function definitions

void app_log_hexdump(const char *line_prefix,
                     const uint8_t *data_ptr,
                     size_t data_len)
{
  size_t pos = 0;
  while (pos < data_len) {
    char line[2 * LINE_LEN + 1] = { 0 };
    size_t p;
    for (p = 0; p < LINE_LEN; p++) {
      if (pos + p < data_len) {
        snprintf(line + 2 * p, 3, "%02x", data_ptr[pos + p]);
      } else {
        snprintf(line + 2 * p, 3, "  ");
      }
    }
    app_log_append("%s%s" APP_LOG_NL, line_prefix, line);
    pos += LINE_LEN;
  }
}

#if DECODER_APP_ADV_HEADER_LOG
void app_adv_header_log(advertisement_header_t adv_header)
{
  uint8_t idx = 0;

  app_log(ADV_DATA_LOG_PREFIX "PDU type: %d ", adv_header.pdu_type);
  switch (adv_header.pdu_type) {
    case ADV_NONCONN_IND:
      app_log_append("(Non-connectable undirected)" APP_LOG_NL);
      break;
    case ADV_SCAN_IND:
      app_log_append("(Scannable undirected)" APP_LOG_NL);
      break;
    case ADV_IND:
      app_log_append("(Connectable undirected)" APP_LOG_NL);
      break;
    case ADV_DIRECT_IND:
      app_log_append("(Connectable directed)" APP_LOG_NL);
      break;
    default:
      app_log_append("Unknown PDU type" APP_LOG_NL);
  }
  app_log_append(ADV_DATA_LOG_PREFIX "Rx Address type: %s" APP_LOG_NL, adv_header.rx_addr_type ? "Random" : "Public");
  app_log_append(ADV_DATA_LOG_PREFIX "Tx Address type: %s" APP_LOG_NL, adv_header.rx_addr_type ? "Random" : "Public");
  app_log_append(ADV_DATA_LOG_PREFIX "Tx bluetooth address: %02X:%02X:%02X:%02X:%02X:%02X" APP_LOG_NL,
                 adv_header.tx_addr.addr[5],
                 adv_header.tx_addr.addr[4],
                 adv_header.tx_addr.addr[3],
                 adv_header.tx_addr.addr[2],
                 adv_header.tx_addr.addr[1],
                 adv_header.tx_addr.addr[0]);

  while (idx < adv_header.data_num) {
    app_log_append(ADV_DATA_LOG_PREFIX "Adv_len_%d: 0x%02X" APP_LOG_NL, idx, adv_header.adv_data[idx].ad_len);
    app_log_append(ADV_DATA_LOG_PREFIX "Adv_type_%d: 0x%02X" APP_LOG_NL, idx, adv_header.adv_data[idx].ad_type);
    idx++;
  }
  app_log_append(SEPARATOR_LOG APP_LOG_NL);
}
#endif // DECODER_APP_ADV_HEADER_LOG

#if DECODER_APP_NETWORK_LAYER_LOG
void app_network_layer_log(net_pdu_t net_pdu)
{
  app_log(NET_LAYER_LOG_PREFIX
          "IVI: 0x%02x NID: 0x%02x CTL: 0x%02x TTL: 0x%02x SEQ: 0x%06x SRC: 0x%04x DST: 0x%04x" APP_LOG_NL,
          net_pdu.ivi,
          net_pdu.nid,
          net_pdu.ctl,
          net_pdu.ttl,
          net_pdu.seq,
          net_pdu.src,
          net_pdu.dst);
  app_log_hexdump(NET_LAYER_LOG_PREFIX "Transport PDU: 0x", net_pdu.transport_pdu, net_pdu.trans_pdu_len);
  app_log_hexdump(NET_LAYER_LOG_PREFIX "Net MIC: 0x", net_pdu.net_mic, net_pdu.net_mic_len);
  app_log_append(SEPARATOR_LOG APP_LOG_NL);
}
#endif // DECODER_APP_NETWORK_LAYER_LOG

#ifdef DECODER_APP_LOWER_TRANSPORT_LAYER_LOG
void app_lower_transport_layer_log(trans_pdu_t trans_pdu, uint8_t ctl)
{
  app_log(TRANS_LAYER_LOG_PREFIX "SEG: 0x%01x, ", trans_pdu.seg);
  if (!ctl) {
    app_log_append("AKF: 0x%02x, AID: 0x%02x" APP_LOG_NL, trans_pdu.akf, trans_pdu.aid);
  } else {
    app_log_append("Opcode: 0x%02x" APP_LOG_NL, trans_pdu.opcode);
  }

  if (trans_pdu.seg == 1) {
    app_log_append(TRANS_LAYER_LOG_PREFIX "SZMIC: 0x%01x, SeqZero: 0x%04x, SegO: 0x%02x, SegN: 0x%02x" APP_LOG_NL,
                   trans_pdu.szmic,
                   trans_pdu.seq_0,
                   trans_pdu.seg_o,
                   trans_pdu.seg_n);
  }
  app_log_append(SEPARATOR_LOG APP_LOG_NL);
}
#endif // DECODER_APP_LOWER_TRANSPORT_LAYER_LOG

#if DECODER_APP_UPPER_TRANSPORT_LAYER_CONTROL_LOG
void app_segack_log(segack_t segack)
{
  app_log_append(TRANS_LAYER_LOG_PREFIX "OBO: 0x%01x, SeqZero: 0x%04x, RFU: 0x%01x, BlockAck: 0x%08x" APP_LOG_NL,
                 segack.obo, segack.seq_zero, segack.rfu, segack.block_ack);
  app_log_append(SEPARATOR_LOG APP_LOG_NL);
}

void app_friend_poll_log(friend_poll_t friend_poll)
{
  app_log_append_info(TRANS_LAYER_LOG_PREFIX "Padding: 0x%02x, FSN: 0x%01x" APP_LOG_NL, friend_poll.padding, friend_poll.fsn);
  app_log_append(SEPARATOR_LOG APP_LOG_NL);
}

void app_friend_update_log(friend_update_t friend_update)
{
  app_log_append_info(TRANS_LAYER_LOG_PREFIX "RFU: 0x%02x, IV Update: 0x%01x, Key Refresh: 0x%01x, IV Index: 0x%08x, MD: 0x%02x"
                      APP_LOG_NL,
                      friend_update.rfu,
                      friend_update.ivu,
                      friend_update.kr,
                      friend_update.ivi,
                      friend_update.md);
  app_log_append(SEPARATOR_LOG APP_LOG_NL);
}

void app_friend_request_log(friend_request_t friend_request)
{
  app_log_append(TRANS_LAYER_LOG_PREFIX "RFU:%01x" APP_LOG_NL, friend_request.rfu);
  app_log_append(TRANS_LAYER_LOG_PREFIX "RSSIFactor: 0x%01x" APP_LOG_NL, friend_request.rssi_factor);
  app_log_append(TRANS_LAYER_LOG_PREFIX "ReceiveWindowFactor: 0x%01x" APP_LOG_NL, friend_request.window_factor);
  app_log_append(TRANS_LAYER_LOG_PREFIX "MinQueueSizeLog: 0x%01x" APP_LOG_NL, friend_request.min_queue);
  app_log_append(TRANS_LAYER_LOG_PREFIX "ReceiveDelay: 0x%02x" APP_LOG_NL, friend_request.receive_delay);
  app_log_append(TRANS_LAYER_LOG_PREFIX "PollTimeout: 0x%06x" APP_LOG_NL, friend_request.poll_timeout);
  app_log_append(TRANS_LAYER_LOG_PREFIX "PreviousAddress: 0x%04x" APP_LOG_NL, friend_request.prev_addr);
  app_log_append(TRANS_LAYER_LOG_PREFIX "NumElements 0x%02x" APP_LOG_NL, friend_request.num_elements);
  app_log_append(TRANS_LAYER_LOG_PREFIX "LPNCounter: 0x%04x" APP_LOG_NL, friend_request.lpn_counter);
  app_log_append(SEPARATOR_LOG APP_LOG_NL);
}

void app_friend_offer_log(friend_offer_t friend_offer)
{
  app_log_append(TRANS_LAYER_LOG_PREFIX "ReceiveWindow: 0x%02x" APP_LOG_NL, friend_offer.receive_window);
  app_log_append(TRANS_LAYER_LOG_PREFIX "QueueSize: 0x%02x" APP_LOG_NL, friend_offer.queue_size);
  app_log_append(TRANS_LAYER_LOG_PREFIX "SubscriptionListSize: 0x%02x" APP_LOG_NL, friend_offer.subs_list);
  app_log_append(TRANS_LAYER_LOG_PREFIX "RSSI: 0x%02x" APP_LOG_NL, friend_offer.rssi);
  app_log_append(TRANS_LAYER_LOG_PREFIX "FriendCounter: 0x%04x" APP_LOG_NL, friend_offer.friend_counter);
  app_log_append(SEPARATOR_LOG APP_LOG_NL);
}

void app_friend_clear_log(friend_clear_t friend_clear)
{
  app_log_append(TRANS_LAYER_LOG_PREFIX "LPNAddress: 0x%04x, LPNCounter: 0x%04x" APP_LOG_NL,
                 friend_clear.lpn_address,
                 friend_clear.lpn_counter);
  app_log_append(SEPARATOR_LOG APP_LOG_NL);
}

void app_friend_subscription_log(trans_pdu_t trans_pdu, friend_subscription_t friend_subscription)
{
  uint8_t pos = 0;

  app_log_append(TRANS_LAYER_LOG_PREFIX "TransactionNumber: 0x%02x" APP_LOG_NL, friend_subscription.tid);
  while (pos < trans_pdu.segment_len - 1) {
    app_log_append(TRANS_LAYER_LOG_PREFIX "AddressList[%d]:%04x" APP_LOG_NL,
                   pos, friend_subscription.addr[pos]);
    pos++;
  }
  app_log_append(SEPARATOR_LOG APP_LOG_NL);
}

void app_friend_subscription_confirm_log(uint8_t tid)
{
  app_log_append(TRANS_LAYER_LOG_PREFIX "TransactionNumber: 0x%02x" APP_LOG_NL, tid);
  app_log_append(SEPARATOR_LOG APP_LOG_NL);
}

void app_heartbeat_log(heartbeat_t heartbeat)
{
  app_log_append(TRANS_LAYER_LOG_PREFIX "RFU: 0x%01x, InitTTL: 0x%02x, Features: 0x%04x" APP_LOG_NL,
                 heartbeat.rfu,
                 heartbeat.ttl,
                 heartbeat.features);
  app_log_append(SEPARATOR_LOG APP_LOG_NL);
}

void app_path_request_log(path_request_t path_request)
{
  app_log_append(TRANS_LAYER_LOG_PREFIX "On_Behalf_Of_Dependent_Origin: 0x%01x" APP_LOG_NL, path_request.obo_dep);
  app_log_append(TRANS_LAYER_LOG_PREFIX "Path_Origin_Path_Metric_Type: 0x%01x" APP_LOG_NL, path_request.ptype);
  app_log_append(TRANS_LAYER_LOG_PREFIX "Path_Origin_Path_Lifetime: 0x%01x" APP_LOG_NL, path_request.plife);
  app_log_append(TRANS_LAYER_LOG_PREFIX "Path_Discovery_Interval: 0x%01x" APP_LOG_NL, path_request.pdisc);
  app_log_append(TRANS_LAYER_LOG_PREFIX "Prohibited: 0x%01x" APP_LOG_NL, path_request.prohib);
  app_log_append(TRANS_LAYER_LOG_PREFIX "Path_Origin_Forwarding_Number: 0x%02x" APP_LOG_NL, path_request.fwdnum);
  app_log_append(TRANS_LAYER_LOG_PREFIX "Path_Origin_Path_Metric: 0x%02x" APP_LOG_NL, path_request.metric);
  app_log_append(TRANS_LAYER_LOG_PREFIX "Destination: 0x%04x" APP_LOG_NL, path_request.pdst);
  app_log_append(TRANS_LAYER_LOG_PREFIX "Path_Origin_Unicast_Addr_Range: 0x%06x" APP_LOG_NL, path_request.prange);
  if (path_request.obo_dep) {
    app_log_append(TRANS_LAYER_LOG_PREFIX "Dependent_Origin_Unicast_Addr_Range: 0x%06x", path_request.drange);
  }
  app_log_append(SEPARATOR_LOG APP_LOG_NL);
}

void app_path_reply_log(path_reply_t path_reply)
{
  app_log_append(TRANS_LAYER_LOG_PREFIX "Unicast_Destination: 0x%01x" APP_LOG_NL, path_reply.uni_dst);
  app_log_append(TRANS_LAYER_LOG_PREFIX "On_Behalf_Of_Dependent_Target: 0x%01x" APP_LOG_NL, path_reply.obo_dep);
  app_log_append(TRANS_LAYER_LOG_PREFIX "Confirmation_Request: 0x%01x" APP_LOG_NL, path_reply.confirm);
  app_log_append(TRANS_LAYER_LOG_PREFIX "Prohibited: 0x%02x" APP_LOG_NL, path_reply.prohib);
  app_log_append(TRANS_LAYER_LOG_PREFIX "Path_Origin: 0x%04x" APP_LOG_NL, path_reply.path_origin);
  app_log_append(TRANS_LAYER_LOG_PREFIX "Path_Origin_Forwarding_Number: 0x%02x" APP_LOG_NL, path_reply.fwdnum);
  app_log_append(TRANS_LAYER_LOG_PREFIX "Path_Origin_Unicast_Addr_Range: 0x%06x" APP_LOG_NL, path_reply.prange);
  if (path_reply.obo_dep) {
    app_log_append(TRANS_LAYER_LOG_PREFIX "Dependent_Origin_Unicast_Addr_Range: 0x%06x", path_reply.drange);
  }
  app_log_append(SEPARATOR_LOG APP_LOG_NL);
}

void app_path_confirmation_log(path_confirmation_t path_confirmation)
{
  app_log_append(TRANS_LAYER_LOG_PREFIX "Path_Origin: 0x%04x" APP_LOG_NL, path_confirmation.path_origin);
  app_log_append(TRANS_LAYER_LOG_PREFIX "Path_Target: 0x%04x" APP_LOG_NL, path_confirmation.path_target);
  app_log_append(SEPARATOR_LOG APP_LOG_NL);
}

void app_path_echo_request_log()
{
  app_log_append(SEPARATOR_LOG APP_LOG_NL);
}

void app_path_echo_reply_log(uint16_t path_dst)
{
  app_log_append(TRANS_LAYER_LOG_PREFIX "Destination: 0x%04x" APP_LOG_NL, path_dst);
  app_log_append(SEPARATOR_LOG APP_LOG_NL);
}

void app_path_dependent_node_update_log(dependent_node_update_t dependent_node_update)
{
  app_log_append(TRANS_LAYER_LOG_PREFIX "Type: 0x%01x" APP_LOG_NL, dependent_node_update.type);
  app_log_append(TRANS_LAYER_LOG_PREFIX "Prohibited: 0x%02x" APP_LOG_NL, dependent_node_update.prohib);
  app_log_append(TRANS_LAYER_LOG_PREFIX "Path_Endpoint: 0x%04x" APP_LOG_NL, dependent_node_update.path_endpoint);
  app_log_append(TRANS_LAYER_LOG_PREFIX "Dependent_Origin_Unicast_Addr_Range: 0x%06x", dependent_node_update.drange);
  app_log_append(SEPARATOR_LOG APP_LOG_NL);
}

void app_path_path_request_solicitation_log(trans_pdu_t trans_pdu, path_request_solicitation_t path_request_solicitation)
{
  uint8_t pos = 0;
  while (pos < trans_pdu.segment_len) {
    app_log_append(TRANS_LAYER_LOG_PREFIX "Addr_List[%d]:%04x" APP_LOG_NL,
                   pos,
                   path_request_solicitation.addr[pos]);
    pos++;
  }
  app_log_append(SEPARATOR_LOG APP_LOG_NL);
}

const char* control_opcode_to_str(uint8_t opcode)
{
  switch ((enum sl_mesh_control_opcode)opcode) {
    case sl_mesh_control_opcode_segack:                      return "Segment Acknowledgement";
    case sl_mesh_control_opcode_friend_poll:                 return "Friend Poll";
    case sl_mesh_control_opcode_friend_update:               return "Friend Update";
    case sl_mesh_control_opcode_friend_request:              return "Friend Request";
    case sl_mesh_control_opcode_friend_offer:                return "Friend Offer";
    case sl_mesh_control_opcode_friend_clear:                return "Friend Clear";
    case sl_mesh_control_opcode_friend_clear_confirm:        return "Friend Clear Confirm";
    case sl_mesh_control_opcode_friend_subscription_add:     return "Friend Subscription List Add";
    case sl_mesh_control_opcode_friend_subscription_remove:  return "Friend Subscription List Remove";
    case sl_mesh_control_opcode_friend_subscription_confirm: return "Friend Subscription List Confirm";
    case sl_mesh_control_opcode_heartbeat:                   return "Heartbeat";
    case sl_mesh_control_opcode_path_request:                return "Path Request";
    case sl_mesh_control_opcode_path_reply:                  return "Path Reply";
    case sl_mesh_control_opcode_path_confirmation:           return "Path Confirmation";
    case sl_mesh_control_opcode_path_echo_request:           return "Path Echo Request";
    case sl_mesh_control_opcode_path_echo_reply:             return "Path Echo Reply";
    case sl_mesh_control_opcode_dependent_node_update:       return "Dependent Node Update";
    case sl_mesh_control_opcode_path_request_solicitation:   return "Path Request Solicitation";
    default:                                                 return "Unknown Control Opcode";
  }
}
#endif // DECODER_APP_UPPER_TRANSPORT_LAYER_CONTROL_LOG

#if DECODER_APP_SENSOR_MODEL_LOG
void app_sensor_descriptors_log(sensor_descriptor_t* descriptor, const size_t pdu_data_len)
{
  size_t pos = 0;
  uint8_t idx = 0;
  float sensor_mesurement_period = 0;
  float sensor_update_interval = 0;

  if (pdu_data_len == 2) {
    app_log_append(SENSOR_MODEL_LOG_PREFIX "Sensor Property ID_%d: 0x%04x (%s)" APP_LOG_NL,
                   idx,
                   descriptor[idx].property_id,
                   sl_mesh_decoder_util_get_property_str(descriptor[idx].property_id));
    return;
  }

  while (pos < pdu_data_len) {
    sensor_mesurement_period = sl_mesh_decoder_time_to_sec(descriptor[idx].measurement_period);
    sensor_update_interval = sl_mesh_decoder_time_to_sec(descriptor[idx].update_interval);

    app_log_append(SENSOR_MODEL_LOG_PREFIX "Sensor Property ID_%d: 0x%04x (%s)" APP_LOG_NL, idx, descriptor[idx].property_id,
                   sl_mesh_decoder_util_get_property_str(descriptor[idx].property_id));
    app_log_append(SENSOR_MODEL_LOG_PREFIX "Sensor Positive Tolerance_%d: 0x%04x (%.2f%%)" APP_LOG_NL, idx,
                   descriptor[idx].positive_tolerance,
                   sl_mesh_decoder_tolerance_to_error_percentage(descriptor[idx].positive_tolerance));
    app_log_append(SENSOR_MODEL_LOG_PREFIX "Sensor Negative Tolerance_%d: 0x%04x (%.2f%%)" APP_LOG_NL, idx,
                   descriptor[idx].negative_tolerance,
                   sl_mesh_decoder_tolerance_to_error_percentage(descriptor[idx].negative_tolerance));
    app_log_append(SENSOR_MODEL_LOG_PREFIX "Sensor Sampling Function_%d: 0x%02x (%s)" APP_LOG_NL, idx,
                   descriptor[idx].sampling_function,
                   sl_mesh_decoder_util_get_sampling_function_str(descriptor[idx].sampling_function));

    if (sensor_mesurement_period < 1) {
      app_log_append(SENSOR_MODEL_LOG_PREFIX "Sensor Measurement Period_%d: 0x%02x (%.2f ms)" APP_LOG_NL, idx,
                     descriptor[idx].measurement_period, sensor_mesurement_period * 1000);
    } else {
      app_log_append(SENSOR_MODEL_LOG_PREFIX "Sensor Measurement Period_%d: 0x%02x (%.2f s)" APP_LOG_NL, idx,
                     descriptor[idx].measurement_period, sensor_mesurement_period);
    }

    if (descriptor[idx].update_interval == 0) {
      app_log_append(SENSOR_MODEL_LOG_PREFIX "Sensor Update Interval_%d: 0x%02x (Not Applicable)" APP_LOG_NL, idx,
                     descriptor[idx].update_interval);
    } else if (sensor_update_interval < 1) {
      app_log_append(SENSOR_MODEL_LOG_PREFIX "Sensor Update Interval_%d: 0x%02x (%.2f ms)" APP_LOG_NL, idx,
                     descriptor[idx].update_interval, sensor_update_interval * 1000);
    } else {
      app_log_append(SENSOR_MODEL_LOG_PREFIX "Sensor Update Interval_%d: 0x%02x (%.2f s)" APP_LOG_NL, idx,
                     descriptor[idx].update_interval, sensor_update_interval);
    }
    pos += 8;
    idx++;
  }
  app_log_append(SEPARATOR_LOG APP_LOG_NL);
}

void app_sensor_status_log(sensor_status_msg_t *sensor_status, const size_t pdu_data_len)
{
  size_t pos = 0;
  uint8_t idx = 0;

  while (pos < pdu_data_len) {
    app_log_append(SENSOR_MODEL_LOG_PREFIX "Marshalled Proprety ID_%d:" APP_LOG_NL, idx);
    app_log_append(SENSOR_MODEL_LOG_PREFIX " Format: 0x%02x" APP_LOG_NL, sensor_status[idx].marshalled_property_id.format);
    app_log_append(SENSOR_MODEL_LOG_PREFIX " Length: 0x%02x" APP_LOG_NL, sensor_status[idx].marshalled_property_id.length);
    app_log_append(SENSOR_MODEL_LOG_PREFIX " Property ID: 0x%04x (%s)" APP_LOG_NL,
                   sensor_status[idx].marshalled_property_id.property_id,
                   sl_mesh_decoder_util_get_property_str(sensor_status[idx].marshalled_property_id.property_id));
    app_log_hexdump(SENSOR_MODEL_LOG_PREFIX " Raw Value 0x", sensor_status[idx].raw_value, sensor_status[idx].raw_value_length);

    if (sensor_status[idx].marshalled_property_id.format == 0) {
      pos = 2 + sensor_status[idx].raw_value_length + pos;
    } else {
      pos = 3 + sensor_status[idx].raw_value_length + pos;
    }
    idx++;
  }
  app_log_append(SEPARATOR_LOG APP_LOG_NL);
}

void app_sensor_cadence_log(struct sensort_cadence_state* cadence, const size_t pdu_data_len)
{
  app_log_append(SENSOR_MODEL_LOG_PREFIX "Sensor Property ID: %04x (%s)" APP_LOG_NL,
                 cadence->property_id,
                 sl_mesh_decoder_util_get_property_str(cadence->property_id));

  if (pdu_data_len == 2) {
    return;
  }

  app_log_append(SENSOR_MODEL_LOG_PREFIX "Sensor Fast Cadence Divisor: %01x" APP_LOG_NL, cadence->period_divisor);

  if (cadence->status_trigger_type == 1) { // UINT16, unitless
    app_log_append(SENSOR_MODEL_LOG_PREFIX "Sensor Status Trigger Type: %01x (Unitless)" APP_LOG_NL,
                   cadence->status_trigger_type);
  } else { // format defined by Sensor Property ID
    app_log_append(SENSOR_MODEL_LOG_PREFIX "Sensor Status Trigger Type: %01x (Format defined by Sensor Property ID)" APP_LOG_NL,
                   cadence->status_trigger_type);
  }

  app_log_hexdump(SENSOR_MODEL_LOG_PREFIX "Sensor Status Trigger Delta Down: 0x",
                  cadence->status_trigger_delta_down.value,
                  cadence->status_trigger_delta_down.size);
  app_log_hexdump(SENSOR_MODEL_LOG_PREFIX "Sensor Status Trigger Delta Up: 0x",
                  cadence->status_trigger_delta_up.value,
                  cadence->status_trigger_delta_up.size);
  app_log_append(SENSOR_MODEL_LOG_PREFIX "Sensor Status Min Interval: %01x" APP_LOG_NL, cadence->min_interval);
  app_log_hexdump(SENSOR_MODEL_LOG_PREFIX "Sensor Fast Cadence Low: 0x",
                  cadence->fast_cadence_low.value,
                  cadence->fast_cadence_low.size);
  app_log_hexdump(SENSOR_MODEL_LOG_PREFIX "Sensor Fast Cadence High: 0x",
                  cadence->fast_cadence_high.value,
                  cadence->fast_cadence_high.size);
  app_log_append(SEPARATOR_LOG APP_LOG_NL);
}

void app_sensor_settings_status_log(sensor_settings_status_t* settings_status, const size_t pdu_data_len)
{
  size_t pos = 0;
  uint8_t idx = 0;

  app_log_append(SENSOR_MODEL_LOG_PREFIX "Sensor Property ID: %04x (%s)" APP_LOG_NL,
                 settings_status->property_id,
                 sl_mesh_decoder_util_get_property_str(settings_status->property_id));

  if (pdu_data_len == 2) {
    return;
  }

  while (pos < pdu_data_len - 2) {
    app_log_append(SENSOR_MODEL_LOG_PREFIX "Sensor Setting Property ID: %04x (%s)" APP_LOG_NL,
                   settings_status->property_id,
                   sl_mesh_decoder_util_get_property_str(settings_status[idx].property_id));
    pos += 2;
    idx++;
  }
  app_log_append(SEPARATOR_LOG APP_LOG_NL);
}

void app_sensor_setting_get_log(sensor_get_setting_msg_t *setting_get, const size_t pdu_data_len)
{
  app_log_append(SENSOR_MODEL_LOG_PREFIX "Sensor Property ID: %04x (%s)" APP_LOG_NL,
                 setting_get->property_id,
                 sl_mesh_decoder_util_get_property_str(setting_get->property_id));
  app_log_append(SENSOR_MODEL_LOG_PREFIX "Sensor Setting Property ID: %04x (%s)" APP_LOG_NL,
                 setting_get->setting_property_id,
                 sl_mesh_decoder_util_get_property_str(setting_get->setting_property_id));
  app_log_append(SEPARATOR_LOG APP_LOG_NL);
}

void app_sensor_setting_set_log(sensor_set_setting_msg_t *setting_set)
{
  app_log_append(SENSOR_MODEL_LOG_PREFIX "Sensor Property ID: %04x (%s)" APP_LOG_NL,
                 setting_set->property_id,
                 sl_mesh_decoder_util_get_property_str(setting_set->property_id));
  app_log_append(SENSOR_MODEL_LOG_PREFIX "Sensor Setting Property ID: %04x (%s)" APP_LOG_NL,
                 setting_set->setting_property_id,
                 sl_mesh_decoder_util_get_property_str(setting_set->setting_property_id));

  app_log_hexdump(SENSOR_MODEL_LOG_PREFIX "Sensor Setting Raw: 0x", setting_set->setting_raw, setting_set->setting_raw_len);
  app_log_append(SEPARATOR_LOG APP_LOG_NL);
}

void app_sensor_setting_status_log(struct sensor_setting_status* setting_status, const size_t pdu_data_len)
{
  app_log_append(SENSOR_MODEL_LOG_PREFIX "Sensor Property ID: %04x (%s)" APP_LOG_NL,
                 setting_status->property_id,
                 sl_mesh_decoder_util_get_property_str(setting_status->property_id));
  app_log_append(SENSOR_MODEL_LOG_PREFIX "Sensor Setting Property ID: %04x (%s)" APP_LOG_NL,
                 setting_status->settings_property_id,
                 sl_mesh_decoder_util_get_property_str(setting_status->settings_property_id));

  // access and setting_raw fields are optional
  if (pdu_data_len == 4) {
    return;
  }

  app_log_append(SENSOR_MODEL_LOG_PREFIX "Sensor Setting Access: 0x%01x (%s)" APP_LOG_NL,
                 setting_status->access,
                 sl_mesh_decoder_util_get_sensor_setting_access_str(setting_status->access));

  app_log_hexdump(SENSOR_MODEL_LOG_PREFIX "Sensor Setting Raw: 0x",
                  setting_status->setting_raw,
                  sizeof(setting_status->setting_raw));
  app_log_append(SEPARATOR_LOG APP_LOG_NL);
}

void app_sensor_column_get_log(sensor_get_column_msg_t *column_get)
{
  app_log_append(SENSOR_MODEL_LOG_PREFIX "Sensor Property ID: %04x (%s)" APP_LOG_NL,
                 column_get->property_id,
                 sl_mesh_decoder_util_get_property_str(column_get->property_id));
  app_log_hexdump(SENSOR_MODEL_LOG_PREFIX "Raw Value X: 0x",
                  column_get->raw_value,
                  column_get->raw_value_length);
  app_log_append(SEPARATOR_LOG APP_LOG_NL);
}

void app_sensor_column_status_log(sensor_column_status_msg_t *column_status, const size_t pdu_data_len)
{
  app_log_append(SENSOR_MODEL_LOG_PREFIX "Sensor Property ID: %04x (%s)" APP_LOG_NL,
                 column_status->property_id,
                 sl_mesh_decoder_util_get_property_str(column_status->property_id));

  if (pdu_data_len == 2) {
    return;
  }

  app_log_hexdump(SENSOR_MODEL_LOG_PREFIX "Raw Value X: 0x", column_status->raw_value_x, column_status->raw_value_length);
  app_log_hexdump(SENSOR_MODEL_LOG_PREFIX "Column Width: 0x", column_status->column_width, column_status->raw_value_length);
  app_log_hexdump(SENSOR_MODEL_LOG_PREFIX "Raw Value Y: 0x", column_status->raw_value_y, column_status->raw_value_length);
  app_log_append(SEPARATOR_LOG APP_LOG_NL);
}

void app_sensor_series_get_log(sensor_get_series_msg_t *series_get, const size_t pdu_data_len)
{
  app_log_append(SENSOR_MODEL_LOG_PREFIX "Sensor Property ID: %04x (%s)" APP_LOG_NL,
                 series_get->property_id,
                 sl_mesh_decoder_util_get_property_str(series_get->property_id));

  if (pdu_data_len == 2) {
    return;
  }

  app_log_hexdump(SENSOR_MODEL_LOG_PREFIX "Raw Value X1: 0x", series_get->raw_value_x1, series_get->raw_value_length);
  app_log_hexdump(SENSOR_MODEL_LOG_PREFIX "Raw Value X2: 0x", series_get->raw_value_x2, series_get->raw_value_length);
  app_log_append(SEPARATOR_LOG APP_LOG_NL);
}

void app_sensor_series_status_log(sensor_series_status_msg_t *series_status, const size_t pdu_data_len)
{
  size_t pos = 0;
  uint8_t idx = 0;

  app_log_append(SENSOR_MODEL_LOG_PREFIX "Sensor Property ID: %04x (%s)" APP_LOG_NL,
                 series_status->property_id,
                 sl_mesh_decoder_util_get_property_str(series_status->property_id));

  if (pdu_data_len == 2) {
    return;
  }

  while (pos < pdu_data_len - 2) {
    app_log_hexdump(SENSOR_MODEL_LOG_PREFIX "Raw Value X: 0x",
                    series_status[idx].raw_value_x,
                    series_status->raw_value_length);
    app_log_hexdump(SENSOR_MODEL_LOG_PREFIX "Column Width: 0x",
                    series_status[idx].column_width,
                    series_status->raw_value_length);
    app_log_hexdump(SENSOR_MODEL_LOG_PREFIX "Raw Value Y: 0x",
                    series_status[idx].raw_value_y,
                    series_status->raw_value_length);

    pos = pos + 3 * series_status->raw_value_length;
    idx++;
  }
  app_log_append(SEPARATOR_LOG APP_LOG_NL);
}

void app_sensor_get_log(const uint8_t* pdu_data, const size_t pdu_data_len)
{
  uint16_t property_id = pdu_data[0] | (pdu_data[1] << 8);
  app_log_append(SENSOR_MODEL_LOG_PREFIX "Sensor Property ID: %04x (%s)" APP_LOG_NL,
                 property_id,
                 sl_mesh_decoder_util_get_property_str(property_id));
  app_log_append(SEPARATOR_LOG APP_LOG_NL);
}

const char* sensor_model_opcode_to_str(uint16_t opcode)
{
  switch (opcode) {
    case SENSOR_DESCRIPTOR_GET:         return "Sensor Descriptor Get";
    case SENSOR_DESCRIPTOR_STATUS:      return "Sensor Descriptor Status";
    case SENSOR_GET:                    return "Sensor Get";
    case SENSOR_STATUS:                 return "Sensor Status";
    case SENSOR_COLUMN_GET:             return "Sensor Column Get";
    case SENSOR_COLUMN_STATUS:          return "Sensor Column Status";
    case SENSOR_SERIES_GET:             return "Sensor Series Get";
    case SENSOR_SERIES_STATUS:          return "Sensor Series Status";
    case SENSOR_CADENCE_GET:            return "Sensor Cadence Get";
    case SENSOR_CADENCE_SET:            return "Sensor Cadence Set";
    case SENSOR_CADENCE_SET_UNACK:      return "Sensor Cadence Set Unacknowledged";
    case SENSOR_CADENCE_STATUS:         return "Sensor Cadence Status";
    case SENSOR_SETTINGS_GET:           return "Sensor Settings Get";
    case SENSOR_SETTINGS_STATUS:        return "Sensor Settings Status";
    case SENSOR_SETTING_GET:            return "Sensor Setting Get";
    case SENSOR_SETTING_SET:            return "Sensor Setting Set";
    case SENSOR_SETTING_SET_UNACK:      return "Sensor Setting Set Unacknowledged";
    case SENSOR_SETTING_STATUS:         return "Sensor Setting Status";
    default:                            return "Unknown opcode";
  }
}
#endif // DECODER_APP_SENSOR_MODEL_LOG
