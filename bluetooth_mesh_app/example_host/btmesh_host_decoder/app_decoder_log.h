/***************************************************************************//**
 * @file app_decoder_log.h
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

#ifndef APP_DECODER_LOG_H
#define APP_DECODER_LOG_H

// -----------------------------------------------------------------------------
// Includes
#include "sl_btmesh_sensor_model_capi_types.h"
#include "sl_btmesh_model_specification_defs.h"
#include "config/decoder_app_config.h"
#include "sl_mesh_decoder.h"

#ifdef __cplusplus
extern "C" {
#endif

// -----------------------------------------------------------------------------
// Macros

#define ADV_DATA_LOG_PREFIX         "[ADV] "
#define NET_LAYER_LOG_PREFIX        "[NET] "
#define TRANS_LAYER_LOG_PREFIX      "[TRANS] "
#define ACCESS_LAYER_LOG_PREFIX     "[ACCESS] "
#define SENSOR_MODEL_LOG_PREFIX     "[SENSOR] "
#define FRIENDSHIP_LOG_PREFIX       "[FRIEND] "
#define SEPARATOR_LOG               "---------------------------"

// -----------------------------------------------------------------------------
// Function declarations

/***************************************************************************//**
 * @brief Logs a hexdump of the provided data with a line prefix.
 *
 * This function outputs a hexadecimal representation of the data pointed to
 * by `data_ptr`. Each line of the hexdump can be prefixed with the string
 * provided in `line_prefix`.
 *
 * @param[in] line_prefix A string to prefix each line of the hexdump.
 * @param[in] data_ptr Pointer to the data to be dumped in hexadecimal format.
 * @param[in] data_len Length of the data to be dumped, in bytes.
 ******************************************************************************/
void app_log_hexdump(const char *line_prefix,
                     const uint8_t *data_ptr,
                     size_t data_len);

#if DECODER_APP_ADV_HEADER_LOG
/***************************************************************************//**
 * @brief Logs the contents of a Bluetooth advertisement header.
 *
 * @param[in] adv_header Structure containing the advertisement header information
 *                       to be logged
 ******************************************************************************/
void app_adv_header_log(advertisement_header_t adv_header);
#endif // DECODER_APP_ADV_HEADER_LOG

#if DECODER_APP_NETWORK_LAYER_LOG
/***************************************************************************//**
 * @brief Logs information about a network layer Protocol Data Unit (PDU).
 *
 * @param[in] net_pdu The network layer PDU to be logged.
 ******************************************************************************/
void app_network_layer_log(net_pdu_t net_pdu);
#endif // DECODER_APP_NETWORK_LAYER_LOG

#if DECODER_APP_LOWER_TRANSPORT_LAYER_LOG
/***************************************************************************//**
 * @brief Logs information about the lower transport layer PDU
 *
 * @param trans_pdu The transport PDU to be logged
 * @param ctl       Control field indicating the type of PDU (access or control message)
 ******************************************************************************/
void app_lower_transport_layer_log(trans_pdu_t trans_pdu, uint8_t ctl);
#endif

#if DECODER_APP_UPPER_TRANSPORT_LAYER_LOG
/***************************************************************************//**
 * @brief Logs information about the upper transport layer PDU
 *
 * @param[in] trans_pdu The transport PDU to be logged
 ******************************************************************************/
void app_upper_transport_layer_log(trans_pdu_t trans_pdu);
#endif

#if DECODER_APP_UPPER_TRANSPORT_LAYER_CONTROL_LOG
/***************************************************************************//**
 * @brief Logs information about a received segmented acknowledgment (SegAck) message
 *
 * @param segack    The SegAck structure containing acknowledgment information
 ******************************************************************************/
void app_segack_log(segack_t segack);

/***************************************************************************//**
 * @brief Logs information about a Friend Poll message in the Bluetooth Mesh network
 *
 * @param friend_poll  The Friend Poll message structure to be logged
 ******************************************************************************/
void app_friend_poll_log(friend_poll_t friend_poll);

/***************************************************************************//**
 * @brief Logs information about a Friend Update message in the Bluetooth Mesh network
 *
 * @param friend_update  The Friend Update message structure to be logged
 ******************************************************************************/
void app_friend_update_log(friend_update_t friend_update);

/***************************************************************************//**
 * @brief Logs information about a Friend Request message in the Bluetooth Mesh network
 *
 * @param friend_request  The Friend Request message structure to be logged
 ******************************************************************************/
void app_friend_request_log(friend_request_t friend_request);

/***************************************************************************//**
 * @brief Logs information about a Friend Offer message in the Bluetooth Mesh network
 *
 * @param friend_offer   The Friend Offer message structure to be logged
 ******************************************************************************/
void app_friend_offer_log(friend_offer_t friend_offer);

/***************************************************************************//**
 * @brief Logs information about a Friend Clear message in the Bluetooth Mesh network
 *
 * @param friend_clear   The Friend Clear message structure to be logged
 ******************************************************************************/
void app_friend_clear_log(friend_clear_t friend_clear);

/***************************************************************************//**
 * @brief Logs information about a Friend Subscription message in the Bluetooth Mesh network
 *
 * @param trans_pdu            Transport layer PDU containing the message data.
 * @param friend_subscription  The Friend Subscription message structure to be logged
 ******************************************************************************/
void app_friend_subscription_log(trans_pdu_t trans_pdu, friend_subscription_t friend_subscription);

/***************************************************************************//**
 * @brief Logs information about a Friend Subscription Confirm message in the Bluetooth Mesh network
 *
 * @param tid            Transaction ID
 ******************************************************************************/
void app_friend_subscription_confirm_log(uint8_t tid);

/***************************************************************************//**
 * @brief Logs information about a Heartbead message in the Bluetooth Mesh network
 *
 * @param heartbeat      The Heartbead message structure to be logged
 ******************************************************************************/
void app_heartbeat_log(heartbeat_t heartbeat);

/***************************************************************************//**
 * @brief Logs information about a Path Request message in the Bluetooth Mesh network
 *
 * @param path_request   The Path Request message structure to be logged
 ******************************************************************************/
void app_path_request_log(path_request_t path_request);

/***************************************************************************//**
 * @brief Logs information about a Path Reply message in the Bluetooth Mesh network
 *
 * @param path_reply     The Path Reply message structure to be logged
 ******************************************************************************/
void app_path_reply_log(path_reply_t path_reply);

/***************************************************************************//**
 * @brief Logs information about a Path Confirmation message in the Bluetooth Mesh network
 *
 * @param path_confirmation  The Path Confirmation message structure to be logged
 ******************************************************************************/
void app_path_confirmation_log(path_confirmation_t path_confirmation);

/***************************************************************************//**
 * @brief Logs information about a Path Echo message in the Bluetooth Mesh network
 ******************************************************************************/
void app_path_echo_request_log();

/***************************************************************************//**
 * @brief Logs information about a Path Echo Reply message in the Bluetooth Mesh network
 *
 * @param path_dst
 ******************************************************************************/
void app_path_echo_reply_log(uint16_t path_dst);

/***************************************************************************//**
 * @brief Logs information about a Dependent Node message in the Bluetooth Mesh network
 *
 * @param dependent_node_update  The Friend Request message structure to be logged
 ******************************************************************************/
void app_path_dependent_node_update_log(dependent_node_update_t dependent_node_update);

/***************************************************************************//**
 * @brief Logs information about a Path Request Solicitation message in the Bluetooth Mesh network
 *
 * @param trans_pdu                  Transport layer PDU containing the message data.
 * @param path_request_solicitation  The Path Request Solicitation message structure to be logged
 ******************************************************************************/
void app_path_path_request_solicitation_log(trans_pdu_t trans_pdu,
                                            path_request_solicitation_t path_request_solicitation);

/***************************************************************************//**
 * @brief Converts a Bluetooth Mesh control opcode to its corresponding string representation
 *
 * @param opcode The control opcode to be converted
 * @return A constant pointer to a string describing the opcode
 ******************************************************************************/
const char* control_opcode_to_str(uint8_t opcode);

#endif // DECODER_APP_UPPER_TRANSPORT_LAYER_CONTROL_LOG

#if DECODER_APP_SENSOR_MODEL_LOG
/***************************************************************************//**
 * @brief Logs the details of a sensor setting set message.
 *
 * @param[in] descriptor Pointer to the sensor descriptor structure to be logged.
 * @param[in] pdu_data_len Length of the PDU data associated with the descriptor.
 ******************************************************************************/
void app_sensor_descriptors_log(sensor_descriptor_t *descriptor,
                                const size_t pdu_data_len);

/***************************************************************************//**
 * @brief Logs the sensor status message.
 *
 * @param[in] sensor_status Pointer to the sensor status message structure.
 * @param[in] pdu_data_len  Length of the PDU data associated with the sensor
 *                          status message.
 ******************************************************************************/
void app_sensor_status_log(sensor_status_msg_t *sensor_status,
                           const size_t pdu_data_len);

/***************************************************************************//**
 * @brief Logs the sensor cadence state information.
 *
 * @param[in] cadence Pointer to the sensor cadence state structure.
 * @param[in] pdu_data_len Length of the PDU data associated with the
 *                         cadence state.
 ******************************************************************************/
void app_sensor_cadence_log(struct sensort_cadence_state* cadence,
                            const size_t pdu_data_len);

/***************************************************************************//**
 * @brief Logs the sensor settings status information.
 *
 * @param[in] settings_status Pointer to the sensor settings status structure
 *                            containing the status information to be logged.
 * @param[in] pdu_data_len    Length of the PDU data associated with the sensor
 *                            settings status.
 ******************************************************************************/
void app_sensor_settings_status_log(sensor_settings_status_t* settings_status,
                                    const size_t pdu_data_len);

/***************************************************************************//**
 * @brief Logs the details of a sensor setting get message.
 *
 * @param[in] setting_get  Pointer to the sensor_get_setting_msg_t structure
 *                         containing the sensor setting get message.
 * @param[in] pdu_data_len Length of the PDU data associated with the message.
 ******************************************************************************/
void app_sensor_setting_get_log(sensor_get_setting_msg_t *setting_get,
                                const size_t pdu_data_len);

/***************************************************************************//**
 * @brief Logs the details of a sensor setting set message.
 *
 * @param[in] setting_set Pointer to the sensor_set_setting_msg_t
 *                        structure containing the sensor setting
 *                        set message to be logged.
 ******************************************************************************/
void app_sensor_setting_set_log(sensor_set_setting_msg_t *setting_set);

/***************************************************************************//**
 * @brief Logs the status of a sensor setting.
 *
 * @param setting_status Pointer to the sensor setting status structure.
 * @param pdu_data_len   Length of the PDU data.
 ******************************************************************************/
void app_sensor_setting_status_log(struct sensor_setting_status* setting_status,
                                   const size_t pdu_data_len);

/***************************************************************************//**
 * @brief Logs the details of a sensor column get message.
 *
 * @param[in] column_get Pointer to the sensor column get message structure.
 ******************************************************************************/
void app_sensor_column_get_log(sensor_get_column_msg_t *column_get);

/***************************************************************************//**
 * @brief Logs the sensor series status message.
 *
 * @param[in] series_status Pointer to the sensor series status
 *                          message structure.
 * @param[in] pdu_data_len  Length of the PDU data associated with the message.
 ******************************************************************************/
void app_sensor_series_status_log(sensor_series_status_msg_t *series_status,
                                  const size_t pdu_data_len);

/***************************************************************************//**
 * @brief Logs the sensor property id from the provided PDU data.
 *
 * @param[in] pdu_data     Pointer to the PDU data buffer.
 * @param[in] pdu_data_len Length of the PDU data buffer.
 ******************************************************************************/
void app_sensor_get_log(const uint8_t* pdu_data, const size_t pdu_data_len);

/***************************************************************************//**
 * @brief Logs the sensor column status message.
 *
 * @param[in] column_status Pointer to the column status message structure.
 * @param[in] pdu_data_len  Length of the PDU data associated with the message.
 ******************************************************************************/
void app_sensor_column_status_log(sensor_column_status_msg_t *column_status,
                                  const size_t pdu_data_len);

/***************************************************************************//**
 * @brief Logs the details of a sensor series get message.
 *
 * @param[in] series_get   Pointer to the sensor_get_series_msg_t structure
 *                         containing the series get message details.
 * @param[in] pdu_data_len Length of the PDU data associated with the
 *                         series get message.
 ******************************************************************************/
void app_sensor_series_get_log(sensor_get_series_msg_t *series_get,
                               const size_t pdu_data_len);

/***************************************************************************//**
 * @brief Converts a sensor model opcode to its corresponding string representation
 *
 * @param opcode The 16-bit sensor model opcode to convert
 * @return       A constant pointer to a string describing the opcode
 ******************************************************************************/
const char* sensor_model_opcode_to_str(uint16_t opcode);
#endif // DECODER_APP_SENSOR_MODEL_LOG

#ifdef __cplusplus
};
#endif

#endif // APP_DECODER_LOG_H
