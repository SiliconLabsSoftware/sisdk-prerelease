/***************************************************************************//**
 * @file sl_mesh_decoder.h
 * @brief Bluetooth Mesh message decoder component header file
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

#ifndef SL_MESH_DECODER_H
#define SL_MESH_DECODER_H

// -----------------------------------------------------------------------------
// Includes
#include <stdint.h>
#include <sl_status.h>
#include "sl_bt_api.h"
#include "sl_btmesh_sensor_model_capi_types.h"

#include "config/sl_mesh_decoder_config.h"

#ifdef __cplusplus
extern "C"
{
#endif

// -----------------------------------------------------------------------------
// Macros

/// Invalid cryptographic key
#define SL_MESH_KEY_HANDLE_INVALID  ~0
/// Maximum network PDU length
#define SL_MESH_MAX_NET_PDU_LEN     29
/// Maximum transport PDU length
#define SL_MESH_MAX_CONTROL_PDU_LEN 256
/// Maximum access message length
#define SL_MESH_MAX_ACCESS_PDU_LEN  384
/// Property ID length
#define SL_MESH_PROPERTY_ID_LEN     2

// Scannable and connectable
#define ADV_IND         0x00
// Directed connectable
#define ADV_DIRECT_IND  0x01
// Non-connectable and non-scannable
#define ADV_NONCONN_IND 0x02
// Non-connectable and scannable
#define ADV_SCAN_IND    0x06

// -----------------------------------------------------------------------------
// Enums, structs, typedefs

/// Mesh beacon type enumeration
enum sl_mesh_beacon {
  sl_mesh_beacon_unprovisioned = 0x00, //< Unprovisioned device beacon
  sl_mesh_beacon_secure = 0x01,        //< Secure network beacon
  sl_mesh_beacon_private = 0x02,       //< Mesh private beacon
};

/// Transport control message opcode enumeration
enum sl_mesh_control_opcode {
  sl_mesh_control_opcode_segack = 0x00,                      //< Segment acknowledgement
  sl_mesh_control_opcode_friend_poll = 0x01,                 //< Friend Poll message
  sl_mesh_control_opcode_friend_update = 0x02,               //< Friend Update message
  sl_mesh_control_opcode_friend_request = 0x03,              //< Friend Request message
  sl_mesh_control_opcode_friend_offer = 0x04,                //< Friend Offer message
  sl_mesh_control_opcode_friend_clear = 0x05,                //< Friend Clear message
  sl_mesh_control_opcode_friend_clear_confirm = 0x06,        //< Friend Clear Confirm message
  sl_mesh_control_opcode_friend_subscription_add = 0x07,     //< Friend Subscription List Add message
  sl_mesh_control_opcode_friend_subscription_remove = 0x08,  //< Friend Subscription List Remove message
  sl_mesh_control_opcode_friend_subscription_confirm = 0x09, //< Friend Subscription List Confirm message
  sl_mesh_control_opcode_heartbeat = 0x0a,                   //< Heartbeat message
  sl_mesh_control_opcode_path_request = 0x0b,                //< Path Request message
  sl_mesh_control_opcode_path_reply = 0x0c,                  //< Path Reply message
  sl_mesh_control_opcode_path_confirmation = 0x0d,           //< Path Confirmation message
  sl_mesh_control_opcode_path_echo_request = 0x0e,           //< Path Echo Request message
  sl_mesh_control_opcode_path_echo_reply = 0x0f,             //< Path Echo Reply message
  sl_mesh_control_opcode_dependent_node_update = 0x10,       //< Dependent Node Update message
  sl_mesh_control_opcode_path_request_solicitation = 0x11    //< Path Request Solicitation message
};

/// Advertisement data structure
typedef struct {
  uint8_t ad_len;        ///< Length of the advertisement data field including type
  uint8_t ad_type;       ///< Advertisement data type (AD Type)
  uint8_t *data;         ///< Pointer to the advertisement data payload
} advertisement_data_t;

/// Advertisement header structure
typedef struct {
  uint8_t pdu_type;      ///< PDU Type field indicating advertising packet type
  uint8_t tx_addr_type;  ///< Transmitter address type (0=public, 1=random)
  bd_addr tx_addr;       ///< Transmitter Bluetooth device address
  uint8_t rx_addr_type;  ///< Target address type for directed advertising
  uint8_t data_num;      ///< Number of advertisement data fields
  advertisement_data_t adv_data[MAX_ADV_DATA_NUM]; ///< Array of advertisement data fields
} advertisement_header_t;

/// Network Protocol Data Unit
typedef struct {
  uint8_t ivi;                   ///< Least significant bit of IV Index
  uint8_t nid;                   ///< Network Identifier
  uint8_t ctl;                   ///< Network Control
  uint8_t ttl;                   ///< Time To Live
  uint32_t seq;                  ///< Sequence number
  uint16_t src;                  ///< Source address
  uint16_t dst;                  ///< Destination address
  size_t trans_pdu_len;          ///< Length of the Transport PDU
  const uint8_t *transport_pdu;  ///< Transport Protocol Data Unit
  size_t net_mic_len;            ///< Length of the NetMIC
  const uint8_t *net_mic;        ///< Pointer to the NetMIC data
} net_pdu_t;

/// Transport Protocol Data Unit
typedef struct {
  uint8_t seg;           ///< Segmentation flag (1 if segmented, 0 if unsegmented)
  uint8_t opcode;        ///< Opcode for control messages
  uint8_t rfu;           ///< Reserved for future use
  uint8_t akf;           ///< Application Key Flag (only for access messages)
  uint8_t aid;           ///< Application Identifier (only for access messages)
  uint8_t szmic;         ///< Size of MIC (only for access messages)
  uint16_t seq_0;        ///< SeqZero value
  uint8_t seg_o;         ///< Segment offset
  uint8_t seg_n;         ///< Last segment number
  size_t segment_len;    ///< Length of the segment data
  const uint8_t *segment;///< Pointer to the segment data buffer
} trans_pdu_t;

/// Decoder context
struct sl_mesh_decoder;

/// Mesh friendship key material context
struct sl_mesh_friendship {
  uint16_t lpn_addr;       //< LPN address
  uint16_t lpn_counter;    //< LPN counter
  uint16_t friend_addr;    //< Friend address
  uint16_t friend_counter; //< Friend counter
};

/// Segment Acknowledgement message structure
typedef struct {
  uint8_t obo;             ///< On-behalf-of flag
  uint16_t seq_zero;       ///< SeqZero of the Upper Transport PDU
  uint8_t rfu;             ///< Reserved for future use
  uint32_t block_ack;      ///< Acknowledgment for segments
} segack_t;

/// Friend Poll message structure
typedef struct {
  uint8_t padding;         ///< Padding
  uint8_t fsn;             ///< Friend Sequence Numbe
} friend_poll_t;

/// Friend Update message structure
typedef struct {
  uint8_t rfu;             ///< Reserved for future use
  uint8_t ivu;             ///< IV Update flag
  uint8_t kr;              ///< Key Refresh flag
  uint32_t ivi;            ///< IV Index
  uint8_t md;              ///< Indicates whether the Friend Queue is empty (0) or not (1)
} friend_update_t;

/// Friend Request message structure
typedef struct {
  uint8_t rfu;           ///< Reserved for future use
  uint8_t rssi_factor;   ///< RSSI factor
  uint8_t window_factor; ///< Receive window factor
  uint8_t min_queue;     ///< Minimum queue size log
  uint8_t receive_delay; ///< Receive delay
  uint32_t poll_timeout; ///< Poll timeout
  uint16_t prev_addr;    ///< Previous address
  uint8_t num_elements;  ///< Number of elements
  uint16_t lpn_counter;  ///< LPN counter
} friend_request_t;

/// Friend Offer message structure
typedef struct {
  uint8_t receive_window;  ///< Receive window
  uint8_t queue_size;      ///< Queue size
  uint8_t subs_list;       ///< Subscription list size
  uint8_t rssi;            ///< RSSI
  uint16_t friend_counter; ///< Friend counter
} friend_offer_t;

/// Friend Clear message structure
typedef struct {
  uint16_t lpn_address; ///< LPN address
  uint16_t lpn_counter; ///< LPN counter
} friend_clear_t;

/// Friend Subscription message structure
typedef struct {
  uint16_t tid;         ///< Transaction Identifier
  const uint16_t *addr; ///< Pointer to address list
} friend_subscription_t;

/// Heartbeat message structure
typedef struct {
  uint8_t rfu;       ///< Reserved for future use
  uint8_t ttl;       ///< Time To Live
  uint16_t features; ///< Features field
} heartbeat_t;

/// Path Request message structure
typedef struct {
  uint8_t obo_dep;    ///< On-behalf-of dependent flag
  uint8_t ptype;      ///< Path type
  uint8_t plife;      ///< Path lifetime
  uint8_t pdisc;      ///< Path discovery interval
  uint8_t prohib;     ///< Prohibited
  uint8_t fwdnum;     ///< Forward number
  uint8_t metric;     ///< Path metric
  uint16_t pdst;      ///< Destination address of path
  size_t prange_len;  ///< Path range length
  uint32_t prange;    ///< Path range
  size_t drange_len;  ///< Destination range length
  uint32_t drange;    ///< Destination range
} path_request_t;

/// Path Reply message structure
typedef struct {
  uint8_t uni_dst;      ///< Unicast destination
  uint8_t obo_dep;      ///< On-behalf-of dependent flag
  uint8_t confirm;      ///< Confirmation flag
  uint8_t prohib;       ///< Prohibited
  uint16_t path_origin; ///< Path origin address
  uint8_t fwdnum;       ///< Forward number
  size_t prange_len;    ///< Path range length
  uint32_t prange;      ///< Path range
  size_t drange_len;    ///< Destination range length
  uint32_t drange;      ///< Destination range
} path_reply_t;

/// Path Confirmation message structure
typedef struct {
  uint16_t path_origin; ///< Path origin address
  uint16_t path_target; ///< Path target address
} path_confirmation_t;

/// Dependent Node Update message structure
typedef struct {
  uint8_t type;           ///< Update type
  uint8_t prohib;         ///< Prohibited flag
  uint16_t path_endpoint; ///< Path endpoint address
  size_t drange_len;      ///< Destination range length
  uint32_t drange;        ///< Destination range
} dependent_node_update_t;

/// Path Request Solicitation message structure
typedef struct {
  const uint16_t *addr; ///< Pointer to address list
} path_request_solicitation_t;

/// Mesh network PDU
struct sl_mesh_net_pdu {
  uint8_t pdu_len; //< PDU length
  uint8_t pdu_data[SL_MESH_MAX_NET_PDU_LEN]; //< PDU data
};

/// Access message
struct sl_mesh_access_pdu {
  uint8_t pdu_len; //< Access message data
  uint8_t pdu_data[SL_MESH_MAX_ACCESS_PDU_LEN]; //< Access message length
};

/// Cryptographic key abstraction
typedef void* sl_mesh_key_handle;

/// This structure contains information about a marshalled property,
/// including its format, data length, and associated sensor property ID.
typedef struct {
  uint8_t format;                   //< Marshalled property format
  uint8_t length;                   //< Data length
  sensor_property_id_t property_id; //< Sensor Property ID
} marshalled_property;

/// Represents the content of a Sensor Status message.
typedef struct {
  marshalled_property marshalled_property_id; //< Sensor Property ID
  uint16_t raw_value_length;                  //< Data length
  uint8_t *raw_value;                         //< Raw value
} sensor_status_msg_t;

/// Represents the content of a Sensor Column Get message.
typedef struct {
  sensor_property_id_t property_id; //< Sensor Property ID
  uint16_t raw_value_length;        //< Data length/
  uint8_t *raw_value;               //< Raw setting value
} sensor_get_column_msg_t;

/// Represents the content of a Sensor Column Status message.
typedef struct {
  sensor_property_id_t property_id; //< Sensor Property ID
  uint16_t raw_value_length;        //< Data length
  uint8_t *raw_value_x;             //< Raw value X
  uint8_t *column_width;            //< Column Width
  uint8_t *raw_value_y;             //< Raw value Y
} sensor_column_status_msg_t;

/// Represents the content of a Sensor Series Get message.
typedef struct {
  sensor_property_id_t property_id; //< Sensor Property ID
  uint16_t raw_value_length;        //< Data length
  uint8_t *raw_value_x1;            //< Raw value X1
  uint8_t *raw_value_x2;            //< Raw value X2
} sensor_get_series_msg_t;

/// Represents the content of a Sensor Series Status message.
typedef struct {
  sensor_property_id_t property_id; //< Sensor Property ID
  uint16_t raw_value_length;        //< Data length
  uint8_t *raw_value_x;             //< Raw value X
  uint8_t *column_width;            //< Column Width
  uint8_t *raw_value_y;             //< Raw value Y
} sensor_series_status_msg_t;

// -----------------------------------------------------------------------------
// Function declarations

/**
 * @mainpage Bluetooth Mesh Message Decoder Library
 *
 * @section intro Introduction
 *
 * This library provides utilities for decrypting, authenticating, and
 * decoding Bluetooth Mesh messages. It relies on the PSA Crypto API
 * for cryptographic functionality.
 *
 * The library supports the following Mesh stack features:
 *     - Decrypting and authenticating Mesh Network PDUs with
 *           - Flooding credentials
 *           - Directed forwarding credentials
 *           - Friendship credentials (provided that friendship
 *             establishment is observed)
 *     - Decoding decrypted Mesh Network PDU contents
 *     - Decoding Lower Transport PDU contents
 *     - Reassembling segmented Upper Transport messages
 *           - Provided that all segments are observed
 *     - Decoding Transport Control messages
 *     - Decrypting and authenticating Access messages
 *     - Authenticating Mesh beacons
 *     - Decoding Mesh beacons
 *
 * The library does *not* support the following Mesh stack features:
 *     - Network cache
 *           - It is up to the application to process or not process
 *             data that is decoded from receives messages when it
 *             can be retransmitted
 *     - Replay protection
 *           - It is up to the application to process or not process
 *             data that is decoded from receives messages when it
 *             can be replayed
 *     - Scanning for Mesh traffic
 *           - It is the application's responsibility to supply
 *             scanned data to the library
 *     - Decoding model messages
 *           - This is up to the application or a higher layer
 *             library
 *     - GATT traffic
 *           - Potentially to be added later
 *     - The provisioning protocol
 *           - Potentially to be added later
 *
 * @section quickstart Quickstart
 *
 * When building an application the library needs to be configured accordingly by
 * - Setting the optional feature support;
 * - Setting the number of encryption keys to support;
 * - Setting the number of label UUIDs to support;
 * - Setting the number of SAR reassemblies to support;
 * - Setting the number of friendships to track;
 *
 * These can be done by editing sl_mesh_decoder_config.h
 *
 * Furthermore, it needs to be ensured that PSA Crypto is compiled
 * with enough resources to manage all necessary encryption keys
 * including all derivations for the network key (seven if directed
 * forwarding credentials are supported) and friendship derivations
 * (two per friendship).
 *
 * An application using the library should do the following:
 *
 * - Initialize a decoder context: sl_mesh_decoder_create()
 * - Install an initial set of encryption keys:
 *     - sl_mesh_decoder_add_network_key() -- note that this
 *       will generate multiple derived keys internally
 *     - sl_mesh_decoder_add_application_key()
 *     - sl_mesh_decoder_add_device_key()
 * - Install a set of label UUIDs (if any) by sl_mesh_decoder_add_virtual_label_uuid()
 * - Know the current IV index for the network
 * - Feed incoming network PDU data to sl_mesh_decoder_decrypt_net_pdu()
 * - If decryption succeeds:
 *     - Examine the network PDU contents, starting with sl_mesh_net_pdu_is_valid()
 *     - Examine the lower transport PDU within network PDU payload, starting with sl_mesh_net_pdu_contains_valid_trans_pdu()
 *     - Make use of the SAR reassembler for segmented messages (sl_mesh_reassembler_add_segment())
 *     - Examine transport control message contents, starting with sl_mesh_pointer_contains_valid_control_message()
 *         - Make use of friendship tracking (sl_mesh_friendship_add_friend_request(), sl_mesh_friendship_add_friend_offer())
 *     - Feed access message data to sl_mesh_decoder_decrypt_access_pdu()
 *     - If decryption succeeds:
 *         - Examine access message contents, starting with sl_mesh_access_pdu_contains_valid_access_message()
 *
 */

// -------------------------------
// Library initialization / deinitialization

/**
 * @brief Create a decoder context
 *
 * @param context Pointer to an allocated context is returned here on success
 * @return SL_STATUS_OK if a context has been allocated and is ready to use
 */
sl_status_t sl_mesh_decoder_create(struct sl_mesh_decoder **context);

/**
 * @brief Delete a decoder context
 *
 * @param context Context to delete
 * @return SL_STATUS_OK if the given context was deleted
 */
sl_status_t sl_mesh_decoder_delete(struct sl_mesh_decoder *context);

// -------------------------------
// Crypto key management

/**
 * @brief Add a network key
 *
 * Add a known network key to a decoder. The decoder will internally
 * construct the various derivations Mesh Protocol uses, and use those
 * to decrypt and authenticate Mesh network PDUs.
 *
 * @param context Context to use
 * @param key_value Exactly 16 octets of AES key material
 * @param handle A reference to the key is returned here on success
 * @return SL_STATUS_OK if the key has been added
 */
sl_status_t sl_mesh_decoder_add_network_key(struct sl_mesh_decoder *context,
                                            const uint8_t *key_value,
                                            sl_mesh_key_handle *handle);

/**
 * @brief Add an application key
 *
 * Add a known application key to a decoder. The decoder will use the
 * key to decrypt and authenticate Mesh access messages.
 *
 * @param context Context to use
 * @param key_value Exactly 16 octets of AES key material
 * @param handle A reference to the key is returned here on success
 * @return SL_STATUS_OK if the key has been added
 */
sl_status_t sl_mesh_decoder_add_application_key(struct sl_mesh_decoder *context,
                                                const uint8_t *key_value,
                                                sl_mesh_key_handle *handle);

/**
 * @brief Add a device key
 *
 * Add a known device key to a decoder. The decoder will use the
 * key to decrypt and authenticate Mesh access messages.
 *
 * @param context Context to use
 * @param key_value Exactly 16 octets of AES key material
 * @param device_addr Primary address of the device; used to match keys to messages
 * @param handle A reference to the key is returned here on success
 * @return SL_STATUS_OK if the key has been added
 */
sl_status_t sl_mesh_decoder_add_device_key(struct sl_mesh_decoder *context,
                                           const uint8_t *key_value,
                                           uint16_t device_addr,
                                           sl_mesh_key_handle *handle);

/**
 * @brief Remove a key by reference
 *
 * @param context Context to use
 * @param handle Reference to the key to remove
 * @return SL_STATUS_OK if the key has been removed
 */
sl_status_t sl_mesh_decoder_remove_key(struct sl_mesh_decoder *context,
                                       sl_mesh_key_handle handle);

/**
 * @brief Get friendship context for a key
 *
 * If a network PDU is encrypted using friendship key material,
 * sl_mesh_decoder_decrypt_pdu() will return a reference to the
 * friendship key that decrypted the PDU successfully. The
 * friendship context can be extracted by this API call.
 *
 * @param context Context to use
 * @param handle Reference to the friendship key to query
 * @param friendship The friendship context will be returned here
 * @return SL_STATUS_OK if the key was a friendship key and the context was read;
 * SL_STATUS_NOT_SUPPORTED if library was compiled without friendship support.
 */
sl_status_t sl_mesh_decoder_get_friendship_key_context(struct sl_mesh_decoder *context,
                                                       sl_mesh_key_handle handle,
                                                       struct sl_mesh_friendship *friendship);

/**
 * @brief Get a friendship key by LPN friendship parameters
 *
 * @param context Context to use
 * @param lpn_addr LPN address for the friendship
 * @param lpn_counter LPN counter for the friendship
 * @param handle The friendship key handle is returned here
 * @return SL_STATUS_OK if the friendship key was returned;
 * SL_STATUS_NOT_SUPPORTED if library was compiled without friendship support.
 */
sl_status_t sl_mesh_decoder_get_friendship_key(struct sl_mesh_decoder *context,
                                               uint16_t lpn_addr,
                                               uint16_t lpn_counter,
                                               sl_mesh_key_handle *handle);

/**
 * @brief Remove a friendship key by LPN address and network key
 *
 * @param context Context to use
 * @param lpn_addr LPN address for the friendship
 * @param net_key_handle Network key from which friendship credentials were derived
 * @return SL_STATUS_OK if the friendship key was removed;
 * SL_STATUS_NOT_SUPPORTED if library was compiled without friendship support.
 */
sl_status_t sl_mesh_decoder_remove_friendship_key(struct sl_mesh_decoder *context,
                                                  uint16_t lpn_addr,
                                                  sl_mesh_key_handle net_key_handle);

// -------------------------------
// Network layer decoding

/**
 * @brief Decrypt a network PDU
 *
 * Decrypt a network PDU. The decoder will try to decrypt and
 * authenticate the given encrypted network PDU with the encryption
 * keys it has; and if successful, it will return the decrypted PDU
 * contents as well as a reference to the matching network key, and a
 * reference to the matching friendship key derivation in case the PDU
 * was encrypted using friendship key material that could be decoded.
 *
 * On successful decryption the pdu structure will contain the
 * plaintext IVI and NID values; the deobfuscated CTL, TTL, sequence
 * number, and source address values; the decrypted and authenticated
 * destination address and payload; and the plaintext NetMIC at the
 * end for reference.
 *
 * @param context Context to use
 * @param current_ivindex Network IV Index value
 * @param net_pdu_ptr Encrypted network PDU data
 * @param net_pdu_len Encrypted network PDU length
 * @param pdu The decrypted PDU is returned here
 * @param matching_net_key A reference to the matching network key is returned here
 * @param matching_friend_key A reference to the matching friendship is returned here,
 * if the PDU was encrypted with friendship key material; otherwise,
 * SL_MESH_KEY_HANDLE_INVALID is returned.
 * @return SL_STATUS_OK if the PDU is successfully decrypted and authenticated
 */
sl_status_t sl_mesh_decoder_decrypt_net_pdu(struct sl_mesh_decoder *context,
                                            uint32_t current_ivindex,
                                            const uint8_t *net_pdu_ptr,
                                            size_t net_pdu_len,
                                            struct sl_mesh_net_pdu *pdu,
                                            sl_mesh_key_handle *matching_net_key,
                                            sl_mesh_key_handle *matching_friend_key);

/**
 * @brief Verify NetPDU length
 *
 * Verify that a decrypted NetPDU is long enough to parse. Note that
 * this only verifies that there is enough data to read all network
 * PDU headers, that there is some payload, and that there is MIC
 * data; it does not check the header field values or the higher layer
 * PDU structure.
 *
 * This API must be called before calling any of the network
 * PDU parsing functions, if the PDU structure is not created
 * by sl_mesh_decoder_decrypt_net_pdu().
 *
 * @param pdu Decrypted Network PDU
 * @return true if the Network PDU contains enough data to be parsed
 */
bool sl_mesh_net_pdu_is_valid(const struct sl_mesh_net_pdu *pdu);

/**
 * @brief Get Network PDU IVI header
 *
 * @param pdu Decrypted Network PDU
 * @return Low bit of the IV index used to send the PDU
 */
uint32_t sl_mesh_net_pdu_ivi(const struct sl_mesh_net_pdu *pdu);

/**
 * @brief Get Network PDU NID header
 *
 * @param pdu Decrypted Network PDU
 * @return NID header value
 */
uint8_t sl_mesh_net_pdu_nid(const struct sl_mesh_net_pdu *pdu);

/**
 * @brief Get Network PDU CTL header
 *
 * Get the CTL header value of a networkPDU.  This indicates whether
 * the lower transport PDU contained in the network PDU is a control
 * message or an access message.
 *
 * @param pdu Decrypted Network PDU
 * @return CTL header value
 */
uint8_t sl_mesh_net_pdu_ctl(const struct sl_mesh_net_pdu *pdu);

/**
 * @brief Get Network PDU TTL header
 *
 * @param pdu Decrypted Network PDU
 * @return TTL header value
 */
uint8_t sl_mesh_net_pdu_ttl(const struct sl_mesh_net_pdu *pdu);

/**
 * @brief Get Network PDU sequence number
 *
 * @param pdu Decrypted Network PDU
 * @return Sequence number value
 */
uint32_t sl_mesh_net_pdu_seq(const struct sl_mesh_net_pdu *pdu);

/**
 * @brief Get Network PDU source address
 *
 * @param pdu Decrypted Network PDU
 * @return Source address value
 */
uint16_t sl_mesh_net_pdu_src(const struct sl_mesh_net_pdu *pdu);

/**
 * @brief Get Network PDU destination address
 *
 * @param pdu Decrypted Network PDU
 * @return Destination address value
 */
uint16_t sl_mesh_net_pdu_dst(const struct sl_mesh_net_pdu *pdu);

/**
 * @brief Get Network PDU TransportPDU data
 *
 * @param pdu Decrypted Network PDU
 * @return Pointer to the beginning of the TransportPDU
 */
const uint8_t *sl_mesh_net_pdu_data_ptr(const struct sl_mesh_net_pdu *pdu);

/**
 * @brief Get Network PDU TransportPDU length
 *
 * @param pdu Decrypted Network PDU
 * @return TransportPDU length
 */
size_t sl_mesh_net_pdu_data_len(const struct sl_mesh_net_pdu *pdu);

/**
 * @brief Get Network PDU NetMIC data
 *
 * @param pdu Decrypted Network PDU
 * @return Pointer to the beginning of NetMIC
 */
const uint8_t *sl_mesh_net_pdu_mic_ptr(const struct sl_mesh_net_pdu *pdu);

/**
 * @brief Get Network PDU NetMIC length
 *
 * @param pdu Decrypted Network PDU
 * @return NetMIC length
 */
size_t sl_mesh_net_pdu_mic_len(const struct sl_mesh_net_pdu *pdu);

/**
 * @brief Extracts all relevant fields from a decoded network PDU.
 *
 * @param[in]  pdu      Pointer to the decoded network PDU structure.
 * @param[out] net_pdu  Pointer to the net_pdu_t structure where the extracted fields will be stored.
 */
void sl_mesh_net_pdu_data(const struct sl_mesh_net_pdu *pdu, net_pdu_t *net_pdu);

/**
 * @brief Extracts the lower transport PDU from a network PDU.
 *
 * @param[in]  pdu               Pointer to the decoded network PDU structure.
 * @param[out] trans_pdu_access  Pointer to the structure where the extracted transport PDU information will be stored.
 * @param[in]  ctl               Control flag (0 for access message, 1 for control message).
 */
void sl_mesh_trans_pdu_data(const struct sl_mesh_net_pdu *pdu,
                            trans_pdu_t *trans_pdu_access,
                            uint8_t ctl);

/**
 * @brief Decodes an advertisement header from a Bluetooth event buffer.
 *
 * @param[in]  evt        Pointer to the Bluetooth scanner event structure containing
 *                        the raw advertisement data.
 * @param[out] adv_header Pointer to the advertisement header structure where the
 *                        decoded information will be stored.
 */
void sl_mesh_advertisement_header_from_buf(sl_bt_msg_t *evt, advertisement_header_t *adv_header);

/**
 * @brief Decodes a Segment Acknowledgement message from a buffer.
 *
 * @param[in]  ptr     Pointer to the buffer containing the message data.
 * @param[out] segack  Pointer to the structure where the decoded information will be stored.
 */
void sl_mesh_segack_from_buf(const uint8_t *ptr, segack_t *segack);

/**
 * @brief Decodes a Friend Poll message from a buffer.
 *
 * @param[in]  ptr         Pointer to the buffer containing the message data.
 * @param[out] friend_poll Pointer to the structure where the decoded information will be stored.
 */
void sl_mesh_friend_poll_from_buf(const uint8_t *ptr, friend_poll_t *friend_poll);

/**
 * @brief Decodes a Friend Update message from a buffer.
 *
 * @param[in]  ptr           Pointer to the buffer containing the message data.
 * @param[out] friend_update Pointer to the structure where the decoded information will be stored.
 */
void sl_mesh_friend_update_from_buf(const uint8_t *ptr, friend_update_t *friend_update);

/**
 * @brief Decodes a Friend Request message from a buffer.
 *
 * @param[in]  ptr            Pointer to the buffer containing the message data.
 * @param[out] friend_request Pointer to the structure where the decoded information will be stored.
 */
void sl_mesh_friend_request_from_buf(const uint8_t *ptr, friend_request_t *friend_request);

/**
 * @brief Decodes a Friend Offer message from a buffer.
 *
 * @param[in]  ptr          Pointer to the buffer containing the message data.
 * @param[out] friend_offer Pointer to the structure where the decoded information will be stored.
 */
void sl_mesh_friend_offer_from_buf(const uint8_t *ptr, friend_offer_t *friend_offer);

/**
 * @brief Decodes a Friend Clear message from a buffer.
 *
 * @param[in]  ptr          Pointer to the buffer containing the message data.
 * @param[out] friend_clear Pointer to the structure where the decoded information will be stored.
 */
void sl_mesh_friend_clear_from_buf(const uint8_t *ptr, friend_clear_t *friend_clear);

/**
 * @brief Decodes a Friend Subscription message from a buffer.
 *
 * @param[in]  ptr                  Pointer to the buffer containing the message data.
 * @param[out] friend_subscription  Pointer to the structure where the decoded information will be stored.
 */
void sl_mesh_friend_subscription_from_buf(const uint8_t *ptr, friend_subscription_t *friend_subscription);

/**
 * @brief Decodes a Heartbeat message from a buffer.
 *
 * @param[in]  ptr        Pointer to the buffer containing the message data.
 * @param[out] heartbeat  Pointer to the structure where the decoded information will be stored.
 */
void sl_mesh_heartbeat_from_buf(const uint8_t *ptr, heartbeat_t *heartbeat);

/**
 * @brief Decodes a Path Request message from a buffer.
 *
 * @param[in]  ptr           Pointer to the buffer containing the message data.
 * @param[out] path_request  Pointer to the structure where the decoded information will be stored.
 */
void sl_mesh_path_request_from_buf(const uint8_t *ptr, path_request_t *path_request);

/**
 * @brief Decodes a Path Reply message from a buffer.
 *
 * @param[in]  ptr        Pointer to the buffer containing the message data.
 * @param[out] path_reply Pointer to the structure where the decoded information will be stored.
 */
void sl_mesh_path_reply_from_buf(const uint8_t *ptr, path_reply_t *path_reply);

/**
 * @brief Decodes a Path Confirmation message from a buffer.
 *
 * @param[in]  ptr                 Pointer to the buffer containing the message data.
 * @param[out] path_confirmation   Pointer to the structure where the decoded information will be stored.
 */
void sl_mesh_path_confirmation_from_buf(const uint8_t *ptr, path_confirmation_t *path_confirmation);

/**
 * @brief Decodes a Dependent Node Update message from a buffer.
 *
 * @param[in]  ptr                   Pointer to the buffer containing the message data.
 * @param[out] dependent_node_update Pointer to the structure where the decoded information will be stored.
 */
void sl_mesh_dependent_node_update_from_buf(const uint8_t *ptr, dependent_node_update_t *dependent_node_update);

/**
 * @brief Decodes a Path Request Solicitation message from a buffer.
 *
 * @param[in]  ptr                        Pointer to the buffer containing the message data.
 * @param[out] path_request_solicitation  Pointer to the structure where the decoded information will be stored.
 */
void sl_mesh_path_request_solicitation_from_buf(const uint8_t *ptr,
                                                path_request_solicitation_t *path_request_solicitation);
//
// Lower transport layer decoding

/**
 * @brief Verify TransportPDU length
 *
 * Verify that the TransportPDU field in a decrypted PDU is long
 * enough to parse. Note that this only verifies that there is enough
 * data to read all lower transport layer headers and that there is
 * some payload; it does not check the header field values or the
 * higher layer PDU structure.
 *
 * This API must be called before calling any of the lower transport
 * PDU parsing functions.
 *
 * @param pdu Decrypted Network PDU
 * @return true if the TransportPDU field contains enough data to be parsed
 */
bool sl_mesh_net_pdu_contains_valid_trans_pdu(const struct sl_mesh_net_pdu *pdu);

/**
 * @brief Get Lower Transport PDU SEG header
 *
 * Get the SEG header value of a lower transport PDU.  This
 * indicates whether the lower transport PDU is segmented or not.
 *
 * @param pdu Decrypted Network PDU
 * @return Value of the SEG header
 */
uint8_t sl_mesh_trans_pdu_seg(const struct sl_mesh_net_pdu *pdu);

/**
 * @brief Get Lower Transport PDU SeqZero header
 *
 * Get the SeqZero header value of a segmented lower transport PDU.
 * This function should only be called for PDUs that are segmented.
 *
 * @param pdu Decrypted Network PDU
 * @return Value of the SeqZero header
 */
uint16_t sl_mesh_trans_pdu_seg_seq0(const struct sl_mesh_net_pdu *pdu);

/**
 * @brief Get Lower Transport PDU SegO header
 *
 * Get the SegO header value of a segmented lower transport PDU.
 * This function should only be called for PDUs that are segmented.
 *
 * @param pdu Decrypted Network PDU
 * @return Value of the SegO header
 */
uint8_t sl_mesh_trans_pdu_seg_o(const struct sl_mesh_net_pdu *pdu);

/**
 * @brief Get Lower Transport PDU SegN header
 *
 * Get the SegN header value of a segmented lower transport PDU.
 * This function should only be called for PDUs that are segmented.
 *
 * @param pdu Decrypted Network PDU
 * @return Value of the SegN header
 */
uint8_t sl_mesh_trans_pdu_seg_n(const struct sl_mesh_net_pdu *pdu);

/**
 * @brief Get Lower Transport PDU AKF header
 *
 * Get the AKF header value of a lower transport access message. This
 * function should only be called for PDUs that contain access
 * messages.
 *
 * @param pdu Decrypted Network PDU
 * @return Value of the AKF header
 */
uint8_t sl_mesh_trans_pdu_access_akf(const struct sl_mesh_net_pdu *pdu);

/**
 * @brief Get Lower Transport PDU AID header
 *
 * Get the AID header value of a lower transport access message. This
 * function should only be called for PDUs that contain access
 * messages.
 *
 * @param pdu Decrypted Network PDU
 * @return Value of the AID header
 */
uint8_t sl_mesh_trans_pdu_access_aid(const struct sl_mesh_net_pdu *pdu);

/**
 * @brief Get Lower Transport PDU SZMIC header
 *
 * Get the SZMIC header value of a lower transport access message. This
 * function should only be called for PDUs that contain access
 * messages. For non-segmented access messages this function will always
 * return 0.
 *
 * @param pdu Decrypted Network PDU
 * @return Value of the SZMIC header
 */
uint8_t sl_mesh_trans_pdu_access_szmic(const struct sl_mesh_net_pdu *pdu);

/**
 * @brief Get Lower Transport PDU Opcode header
 *
 * Get the Opcode header value of a lower transport control
 * message. This function should only be called for PDUs that contain
 * control messages.
 *
 * @param pdu Decrypted Network PDU
 * @return Value of the Opcode header
 */
uint8_t sl_mesh_trans_pdu_control_opcode(const struct sl_mesh_net_pdu *pdu);

/**
 * @brief Get Lower Transport PDU payload data
 *
 * Get a pointer to the beginning of lower transport layer PDU payload.
 * This returns a pointer to the beginning of upper transport access PDU
 * for unsegmented access messages; a pointer to the beginning of an
 * access message segment for segmented access messages; a pointer to
 * the beginning of control message parameters for unsegmented control
 * messages; and a pointer to the beginning of a control message segment
 * for segmented control messages.
 *
 * @param pdu Decrypted Network PDU
 * @return Pointer to lower transport PDU payload
 */
const uint8_t *sl_mesh_trans_pdu_data_ptr(const struct sl_mesh_net_pdu *pdu);

/**
 * @brief Get Lower Transport PDU payload length
 *
 * @param pdu Decrypted Network PDU
 * @return Lower transport PDU payload length
 */
size_t sl_mesh_trans_pdu_data_len(const struct sl_mesh_net_pdu *pdu);

// -------------------------------
// Transport layer reassembly

/**
 * @brief Add segment data to SAR reassembly
 *
 * Segmented access and control message data can be reassembled by the decoder
 * as long as all segment data is received at least once. This function should
 * be called when segmented data is received. The decoder will make the full
 * message available to the application when all segment data is at hand.
 *
 * The decoder will do a consistency check on incoming segments and will not
 * consider badly formatted segments or segments with conflicting parameters
 * for reassembly.
 *
 * The caller does not need to filter for retransmitted segments as the
 * decoder will take care of that.
 *
 * Reassembler structures are internally allocated and deallocated by
 * the decoder. The maximum number of parallel reassemblies that can
 * be attempted is defined by the #MAX_REASSEMBLER_COUNT macro.
 *
 * @param context        Context to use
 * @param net_key        Reference to the network key the data was encrypted with
 * @param net_pdu        Decoded network PDU structure
 * @param trans_pdu      Decoded transport PDU structure
 * @param now_completed  Set to true on first reception of a segment that
 *                       completes the reassembly; the caller can make a decision on when to
 *                       process the full message data based on this value, if it wants to process
 *                       the data only once
 * @return SL_STATUS_OK if segment data has been successfully processed
 * for reassembly.
 */
sl_status_t sl_mesh_reassembler_add_segment(struct sl_mesh_decoder *context,
                                            sl_mesh_key_handle net_key,
                                            net_pdu_t net_pdu,
                                            trans_pdu_t trans_pdu,
                                            bool *now_completed);

/**
 * @brief Get reassembled data
 *
 * Get the reassembled access or control message data, if possible.
 *
 * @param context Context to use
 * @param src Source address for the network PDU the data was received in
 * @param dst Destination address for the network PDU the data was received in
 * @param seq_0 SeqZero for the lower transport PDU the data was received in
 * @param ptr Pointer to the beginning of reassembled data is returned here
 * @param len Length of the reassembled data is returned here
 * @param seq_auth The SeqAuth value for the reassembled message, used in
 * access message decryption and authentication.
 * @return SL_STATUS_OK if reassembled data is available and could be returned
 */
sl_status_t sl_mesh_reassembler_get_data(struct sl_mesh_decoder *context,
                                         uint16_t src,
                                         uint16_t dst,
                                         uint16_t seq_0,
                                         const uint8_t **ptr,
                                         size_t *len,
                                         uint32_t *seq_auth);

// -------------------------------
// Transport control

/**
 * @brief Verify upper transport control message length
 *
 * Verify that the upper transport control message is long enough to
 * parse. Note that this only verifies that there is enough data to
 * read all parameters for the given opcode; it does not check
 * parameter value validity.
 *
 * This API must be called before starting to interpret the data.
 *
 * @param opcode Transport control opcode
 * @param param_ptr Parameters array data
 * @param param_len Parameters array length
 * @return true if the parameters array contains enough data to be parsed
 */
bool sl_mesh_pointer_contains_valid_control_message(uint8_t opcode,
                                                    const uint8_t *param_ptr,
                                                    size_t param_len);

/**
 * @brief Verify upper transport access PDU length
 *
 * Verify that the upper transport access PDU is long enough to
 * parse. Note that this only verifies that there is enough data to
 * read all fields; it does not check field value validity.
 *
 * This API must be called before starting to interpret the data.
 *
 * @param szmic The SZMIC value for the PDU
 * @param pdu_ptr PDU data array
 * @param pdu_len PDU data length
 * @return true if PDU data array contains enough data to be parsed
 */
bool sl_mesh_pointer_contains_valid_access_pdu(uint8_t szmic,
                                               const uint8_t *pdu_ptr,
                                               size_t pdu_len);

// -------------------------------
// Friendship

/**
 * @brief Add a Friend Request for pending friendship tracking
 *
 * The decoder must be made aware of the friendships it should keep track of
 * as friendship negotiation is taking place. When a Friend Request PDU that
 * is sent by an LPN that is of interest is seen, the application should call
 * this API to add the pending friendship into the list of friendships to
 * follow.
 *
 * Once decoder is aware of both a Friend Request and the matching
 * Friend Offer -- see sl_mesh_friendship_add_friend_offer() -- it can
 * track a friendship that is established by being able to decode a
 * PDU (such as a Friend Poll) that is encrypted with friendship key
 * material derived from the parameters carried in these PDUs. It will
 * do this automatically, provided that it is able to store the
 * derived key material for use.
 *
 * The decoder will internally manage the resources for friendship tracking.
 * The macro #MAX_INCOMPLETE_FRIENDSHIP_COUNT defines how many pending
 * friendships can be tracked in parallel; and the macro #MAX_KEYS affects
 * how many friendship key derivations can be stored in addition to the
 * other cryptographic keys.
 *
 * The application should call sl_mesh_decoder_remove_key() to drop friendship
 * key material for any friendships that are no longer of interest, such as
 * terminated ones, so that it can derive keys for new friendships as needed.
 *
 * @param context The context to use
 * @param net_key Reference to the network key from which friendship key material is derived
 * @param lpn_addr Address of the LPN making the Friend Request
 * @param lpn_counter LPN's friendship counter
 * @return SL_STATUS_OK if the pending friendship is being monitored;
 * SL_STATUS_NOT_SUPPORTED if library was compiled without friendship support.
 */
sl_status_t sl_mesh_friendship_add_friend_request(struct sl_mesh_decoder *context,
                                                  sl_mesh_key_handle net_key,
                                                  uint16_t lpn_addr,
                                                  uint16_t lpn_counter);

/**
 * @brief Add a Friend Offer for pending friendship tracking
 *
 * This is the Friend Offer control message counterpart for
 * sl_mesh_friendship_add_friend_request().
 *
 * @param context The context to use
 * @param net_key Reference to the network key from which friendship key material is derived
 * @param lpn_addr Address of the LPN that made a Friend Request
 * @param friend_addr Address of the Friend that made a Friend Offer
 * @param friend_counter Friend's friendship counter
 * @return SL_STATUS_OK if the pending friendship is being monitored;
 * SL_STATUS_NOT_SUPPORTED if library was compiled without friendship support.
 */
sl_status_t sl_mesh_friendship_add_friend_offer(struct sl_mesh_decoder *context,
                                                sl_mesh_key_handle net_key,
                                                uint16_t lpn_addr,
                                                uint16_t friend_addr,
                                                uint16_t friend_counter);

// -------------------------------
// Access layer decoding

/**
 * @brief Add a virtual address label UUID
 *
 * Virtual address label UUIDs need to be known by the decoder in order
 * to be able to decrypt and authenticate access messages that are sent
 * to virtual address destinations.
 *
 * @param context The context to use
 * @param label_uuid The label UUID to add
 * @return SL_STATUS_OK if the label UUID was successfully added
 */
sl_status_t sl_mesh_decoder_add_virtual_label_uuid(struct sl_mesh_decoder *context,
                                                   const uint8_t *label_uuid);

/**
 * @brief Remove a virtual address label UUID
 *
 * @param context The context to use
 * @param label_uuid The label UUID to remove
 * @return SL_STATUS_OK if the label UUID was successfully removed
 */
sl_status_t sl_mesh_decoder_remove_virtual_label_uuid(struct sl_mesh_decoder *context,
                                                      const uint8_t *label_uuid);

/**
 * @brief Decrypt an access message
 *
 * Decrypt an access message. The decoder will try to decrypt and
 * authenticate the given encrypted access message with the encryption
 * keys it has; and if successful, it will return the decrypted PDU
 * contents as well as a reference to the matching application or
 * device key, and a reference to the matching label UUID if the
 * destination address is a virtual address or the NULL pointer.
 *
 * On successful decryption the message structure will contain the
 * decrypted and authenticated access message; and the plaintext
 * TransMIC at the end for reference.
 *
 * @param context             Context to use
 * @param current_ivindex     Network IV Index value
 * @param pdu                 The decrypted message is returned here
 * @param matching_app_key    A reference to the matching application or device key is returned here
 * @param matching_label_uuid Pointer to the matching label UUID for virtual destination addresses,
 *                            or NULL for non-virtual destination addresses, is returned here
 * @param trans_pdu           Transport protocol data unit
 * @param net_pdu             Network protocol data unit
 * @return                    SL_STATUS_OK if the message is successfully decrypted and authenticated
 */
sl_status_t sl_mesh_decoder_decrypt_access_pdu(struct sl_mesh_decoder *context,
                                               uint32_t current_ivindex,
                                               struct sl_mesh_access_pdu *pdu,
                                               sl_mesh_key_handle *matching_app_key,
                                               const uint8_t **matching_label_uuid,
                                               net_pdu_t net_pdu,
                                               trans_pdu_t trans_pdu);

/**
 * @brief Verify access message length
 *
 * Verify that the access message is long enough to parse. Note that
 * this only verifies that there is enough data to read the access
 * message opcode; it does not check parameter value validity for the
 * particular message.
 *
 * This API must be called before starting to interpret the data.
 *
 * @param pdu Decrypted access message
 * @return true if the access message contains enough data so that opcode can be parsed
 */
bool sl_mesh_access_pdu_contains_valid_access_message(const struct sl_mesh_access_pdu *pdu);

/**
 * @brief Get a pointer to the access message payload
 *
 * @param pdu Decrypted access message
 * @return a pointer to the payload
 */
const uint8_t *sl_mesh_access_pdu_opcode_ptr(const struct sl_mesh_access_pdu *pdu);

/**
 * @brief Get access message payload length
 *
 * @param pdu Decrypted access message
 * @return length of the payload
 */
size_t sl_mesh_access_pdu_opcode_len(const struct sl_mesh_access_pdu *pdu);

/**
 * @brief Get a pointer to the access message parameters
 *
 * @param pdu Decrypted access message
 * @return a pointer to the beginning of parameters
 */
const uint8_t *sl_mesh_access_pdu_data_ptr(const struct sl_mesh_access_pdu *pdu);

/**
 * @brief Get access message parameter length
 *
 * @param pdu Decrypted access message
 * @return length of the parameters
 */
size_t sl_mesh_access_pdu_data_len(const struct sl_mesh_access_pdu *pdu);

// -------------------------------
// Beacon decoding

/**
 * @brief Authenticate beacon and get beacon contents
 *
 * @param context The context to use
 * @param beacon_ptr Beacon data pointer
 * @param beacon_len Beacon data length
 * @param type Beacon type will be returned here
 * @param flags Beacon flags will be returned here
 * @param iv_index Beacon IV index value will be returned here
 * @param matching_net_key The network key with which the beacon authenticates will be returned here
 * @return SL_STATUS_OK if beacon was authenticated successfully
 */
sl_status_t sl_mesh_decoder_authenticate_network_beacon(struct sl_mesh_decoder *context,
                                                        const uint8_t *beacon_ptr,
                                                        size_t beacon_len,
                                                        enum sl_mesh_beacon *type,
                                                        uint8_t *flags,
                                                        uint32_t *iv_index,
                                                        sl_mesh_key_handle *matching_net_key);

// -------------------------------
// Sensor model decoding

/**
 * @brief Converts a time interval in milliseconds to seconds.
 *
 * This function takes a time interval in milliseconds and converts it to seconds.
 *
 * @param[in] interval The time interval in milliseconds.
 * @return The time interval in seconds as a float.
 */
float sl_mesh_decoder_time_to_sec(uint32_t interval);

/**
 * @brief Converts a tolerance value to an error percentage.
 *
 * This function takes a tolerance value and converts it to an error percentage.
 *
 * @param[in] tolerance The tolerance value.
 * @return The error percentage as a float.
 */
float sl_mesh_decoder_tolerance_to_error_percentage(uint16_t tolerance);

/**
 * @brief Parses sensor descriptors from a buffer.
 *
 * This function extracts sensor descriptor information from the provided buffer
 * and populates the given `sensor_descriptor_t` structure.
 *
 * @param[out] descriptor     Pointer to the structure where the parsed sensor descriptor
 *                            information will be stored.
 * @param[in] pdu_data        Pointer to the buffer containing the sensor descriptor data.
 * @param[in] pdu_data_len    Length of the buffer in bytes.
 *
 * @return sl_status_t Status code indicating the success or failure of the operation.
 *                     - SL_STATUS_OK: The operation was successful.
 *                     - Error code: An error occurred during parsing.
 */
sl_status_t sl_mesh_sensor_descriptors_from_buf(sensor_descriptor_t *descriptor,
                                                const uint8_t *pdu_data,
                                                const size_t pdu_data_len);

/**
 * @brief Decodes sensor cadence state from a buffer.
 *
 * This function parses the provided buffer to extract the sensor cadence state
 * and populates the given `sensort_cadence_state` structure with the decoded data.
 *
 * @param[out] cadence       Pointer to the structure where the decoded sensor cadence state will be stored.
 * @param[in]  pdu_data      Pointer to the buffer containing the encoded sensor cadence data.
 * @param[in]  pdu_data_len  Length of the buffer containing the encoded data.
 *
 * @return sl_status_t Status code indicating the success or failure of the operation.
 *                     - SL_STATUS_OK: The operation was successful.
 *                     - Error code: An error occurred during parsing.
 */
sl_status_t sl_mesh_sensor_cadence_from_buf(struct sensort_cadence_state *cadence,
                                            uint8_t *pdu_data,
                                            const size_t pdu_data_len);

/**
 * @brief Parses the sensor setting from a buffer.
 *
 * This function parses the provided buffer and populates the sensor setting
 * structure with the extracted data.
 *
 * @param[out] set_setting  Pointer to the structure where the parsed sensor setting
 *                          information will be stored.
 * @param[in] pdu_data      Pointer to the buffer containing the sensor setting data.
 * @param[in] pdu_data_len  Length of the buffer in bytes.
 *
 * @return sl_status_t Status code indicating the success or failure of the operation.
 *                     - SL_STATUS_OK: The operation was successful.
 *                     - Error code: An error occurred during parsing.
 */
sl_status_t sl_mesh_sensor_setting_set_from_buf(sensor_set_setting_msg_t *set_setting,
                                                uint8_t *pdu_data,
                                                const size_t pdu_data_len);

/**
 * @brief Extracts sensor setting information from a buffer.
 *
 * This function parses the provided buffer to populate the
 * sensor_get_setting_msg_t structure with sensor setting data.
 *
 * @param[out] get_setting   Pointer to the structure where the sensor setting
 *                           information will be stored.
 * @param[in]  pdu_data      Pointer to the buffer containing the sensor setting data.
 * @param[in]  pdu_data_len  Length of the buffer in bytes.
 *
 * @return sl_status_t Status code indicating the success or failure of the operation.
 *                     - SL_STATUS_OK: The operation was successful.
 *                     - Error code: An error occurred during parsing.
 */
sl_status_t sl_mesh_sensor_setting_get_from_buf(sensor_get_setting_msg_t *get_setting,
                                                uint8_t *pdu_data,
                                                const size_t pdu_data_len);

/**
 * @brief Parses a sensor setting status message from a buffer.
 *
 * This function extracts the sensor setting status information from the provided
 * data buffer and populates the given `sensor_setting_status`
 * structure with the parsed data.
 *
 * @param[out] setting_status Pointer to a `sensor_setting_status` structure where the
 *                            parsed data will be stored.
 * @param[in]  pdu_data       Pointer to the buffer containing the PDU data.
 * @param[in]  pdu_data_len   Length of the PDU data buffer.
 *
 * @return sl_status_t Returns SL_STATUS_OK if the parsing is successful, or an
 *                     appropriate error code otherwise.
 */
sl_status_t sl_mesh_sensor_setting_status_from_buf(struct sensor_setting_status *setting_status,
                                                   uint8_t *pdu_data,
                                                   const size_t pdu_data_len);

/**
 * @brief Decodes a sensor status message from a buffer.
 *
 * This function parses the provided buffer containing sensor status data
 * and populates the given `sensor_status_msg_t` structure with the decoded
 * information.
 *
 * @param[out] sensor_status Pointer to the structure where the decoded sensor
 *                           status message will be stored.
 * @param[in]  pdu_data      Pointer to the buffer containing the sensor status
 *                           message data.
 * @param[in]  pdu_data_len  Length of the buffer containing the sensor status
 *                           message data.
 *
 * @return sl_status_t Status code indicating the success or failure of the operation.
 *                     - SL_STATUS_OK: The operation was successful.
 *                     - Error code: An error occurred during parsing.
 */
sl_status_t sl_mesh_sensor_status_from_buf(sensor_status_msg_t *sensor_status,
                                           uint8_t *pdu_data,
                                           const size_t pdu_data_len);

/**
 * @brief Extracts sensor settings status message from a buffer.
 *
 * This function parses the provided buffer to extract the sensor settings
 * status information and populates the given `sensor_settings_status_t` structure.
 *
 * @param[out] settings_status Pointer to the structure where the decoded sensor
 *                             settings status will be stored.
 * @param[in] pdu_data         Pointer to the buffer containing the encoded sensor
 *                             settings status message.
 * @param[in] pdu_data_len     Length of the buffer in bytes.
 *
 * @return sl_status_t Status code indicating the success or failure of the operation.
 *                     - SL_STATUS_OK: The operation was successful.
 *                     - Error code: An error occurred during parsing.
 */
sl_status_t sl_mesh_sensor_settings_status_from_buf(sensor_settings_status_t *settings_status,
                                                    uint8_t *pdu_data,
                                                    const size_t pdu_data_len);

/**
 * @brief Extracts sensor column data from a buffer.
 *
 * This function parses the provided buffer to populate the
 * sensor_get_column_msg_t structure with sensor column data.
 *
 * @param[out] column_get     Pointer to the structure where the extracted
 *                            sensor column data will be stored.
 * @param[in]  pdu_data       Pointer to the buffer containing the sensor column data.
 * @param[in]  pdu_data_len   Length of the buffer in bytes.
 *
 * @return sl_status_t Status code indicating the success or failure of the operation.
 *                     - SL_STATUS_OK: The operation was successful.
 *                     - Error code: An error occurred during parsing.
 */
sl_status_t sl_mesh_sensor_column_get_from_buf(sensor_get_column_msg_t *column_get,
                                               uint8_t *pdu_data,
                                               const size_t pdu_data_len);

/**
 * @brief Extracts sensor series data from a buffer.
 *
 * This function parses the provided PDU data buffer to extract sensor series
 * information and populates the given `sensor_get_series_msg_t` structure.
 *
 * @param[out] series_get Pointer to the structure where the extracted sensor
 *                        series data will be stored.
 * @param[in]  pdu_data   Pointer to the buffer containing the PDU data.
 * @param[in]  pdu_data_len Length of the PDU data buffer.
 *
 * @return sl_status_t Status code indicating the success or failure of the operation.
 *                     - SL_STATUS_OK: The operation was successful.
 *                     - Error code: An error occurred during parsing.
 */
sl_status_t sl_mesh_sensor_series_get_from_buf(sensor_get_series_msg_t *series_get,
                                               uint8_t *pdu_data,
                                               const size_t pdu_data_len);

/**
 * @brief Decodes a sensor series status message from a buffer.
 *
 * This function parses the provided PDU data and populates the
 * sensor_series_status_msg_t structure with the decoded information.
 *
 * @param[out] series_status Pointer to the structure where the decoded
 *                           sensor series status will be stored.
 * @param[in]  pdu_data      Pointer to the buffer containing the PDU data.
 * @param[in]  pdu_data_len  Length of the PDU data buffer.
 *
 * @return sl_status_t Status code indicating the success or failure of the operation.
 *                     - SL_STATUS_OK: The operation was successful.
 *                     - Error code: An error occurred during parsing.
 */
sl_status_t sl_mesh_sensor_series_status_from_buf(sensor_series_status_msg_t *series_status,
                                                  uint8_t *pdu_data,
                                                  const size_t pdu_data_len);
/**
 * @brief Decodes a sensor column status message from a buffer.
 *
 * This function extracts the sensor column status information from the provided
 * buffer and populates the given `sensor_column_status_msg_t` structure.
 *
 * @param[out] column_status Pointer to the structure where the decoded sensor column
 *                           status will be stored.
 * @param[in]  pdu_data      Pointer to the buffer containing the encoded sensor column
 *                           status message.
 * @param[in]  pdu_data_len  Length of the buffer containing the encoded message.
 *
 * @return sl_status_t Status code indicating the success or failure of the operation.
 *                     - SL_STATUS_OK: The operation was successful.
 *                     - Error code: An error occurred during parsing.
 */
sl_status_t sl_mesh_sensor_column_status_from_buf(sensor_column_status_msg_t *column_status,
                                                  uint8_t *pdu_data,
                                                  const size_t pdu_data_len);

#ifdef __cplusplus
}
#endif

#endif // SL_MESH_DECODER_H
