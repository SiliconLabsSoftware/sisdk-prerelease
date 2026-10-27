/**************************************************************************//**
 * @file
 * @brief Protocol-agnostic message link over a cpc-ng endpoint.
 ******************************************************************************
 * # License
 * <b>Copyright 2026 Silicon Laboratories Inc. www.silabs.com</b>
 ******************************************************************************
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
 *****************************************************************************/
#ifndef SL_BT_LINK_H
#define SL_BT_LINK_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "sl_common.h"
#include "sl_cpc_buf.h"
#include "sl_cpc_bus.h"
#include "sl_cpc_frame.h"
#include "sl_bt_link_runtime.h"
#include "sl_status.h"

/***********************************************************************************************//**
 * @addtogroup sl_bt_link
 * @{
 **************************************************************************************************/

/// Opaque link with one endpoint and binding-provided transfer storage.
typedef struct sli_bt_link sl_bt_link_t;
/// Opaque received payload held in a CPC receive buffer.
typedef struct sl_bt_link_message sl_bt_link_message_t;

/// Internal state for one receive buffer supplied by link storage.
typedef struct sl_bt_link_receive_slot {
  sl_cpc_buf_t                    buffer;
  struct sl_bt_link_receive_slot *next;
  sl_bt_link_t                    *owner;
  uint8_t                         *data;
  size_t                          length;
  size_t                          offset;
  bool                            allocated;
} sl_bt_link_receive_slot_t;

/// Internal state for one transmit buffer supplied by link storage.
typedef struct sl_bt_link_transmit_slot {
  sl_cpc_buf_t                     buffer;
  sl_cpc_frame_t                   frame;
  uint8_t                          *data;
  size_t                           length;
  bool                             allocated;
  bool                             active;
} sl_bt_link_transmit_slot_t;

/// Binding-owned storage used exclusively by one live link.
typedef struct {
  sl_bt_link_receive_slot_t  *receive_slot;
  sl_bt_link_transmit_slot_t *transmit_slot;
  uint8_t                     *receive_data;
  uint8_t                     *transmit_data;
  size_t                      message_size;
  size_t                      buffer_stride;
  size_t                      receive_count;
  bool                        allocated;
} sl_bt_link_storage_t;

/// Round a message capacity up to the alignment required by the link backend.
#define SL_BT_LINK_BUFFER_SIZE(message_size)        \
  (((message_size) + SL_CPC_BUF_MIN_ALIGNMENT - 1u) \
   & ~(SL_CPC_BUF_MIN_ALIGNMENT - 1u))

/**
 * Declare statically allocated storage for one link binding.
 *
 * The storage object must outlive its link. The link has exclusive use of all
 * generated arrays from successful create until successful destroy.
 */
#define SL_BT_LINK_STORAGE_DEFINE(name, message_size_, rx_count_)      \
  static sl_bt_link_receive_slot_t name##_receive_slot[(rx_count_)];   \
  static sl_bt_link_transmit_slot_t name##_transmit_slot;              \
  static __ALIGNED(SL_CPC_BUF_MIN_ALIGNMENT)                           \
  uint8_t name##_receive_data[(rx_count_)]                             \
  [SL_BT_LINK_BUFFER_SIZE(message_size_)];                             \
  static __ALIGNED(SL_CPC_BUF_MIN_ALIGNMENT)                           \
  uint8_t name##_transmit_data[SL_BT_LINK_BUFFER_SIZE(message_size_)]; \
  static sl_bt_link_storage_t name = {                                 \
    .receive_slot = name##_receive_slot,                               \
    .transmit_slot = &name##_transmit_slot,                            \
    .receive_data = &name##_receive_data[0][0],                        \
    .transmit_data = name##_transmit_data,                             \
    .message_size = (message_size_),                                   \
    .buffer_stride = SL_BT_LINK_BUFFER_SIZE(message_size_),            \
    .receive_count = (rx_count_),                                      \
    .allocated = false,                                                \
  }

/// Endpoint role of the local device.
typedef enum {
  sl_bt_link_role_primary = 0, ///< CPC primary that issues connect.
  sl_bt_link_role_secondary    ///< CPC secondary that listens.
} sl_bt_link_role_t;

/// Optional payload validator. NULL accepts any non-empty fitting message.
typedef bool (*sl_bt_link_validator_t)(const uint8_t *data, size_t length);

/// Notification emitted when at least one complete payload is available.
typedef void (*sl_bt_link_message_callback_t)(sl_bt_link_t *link,
                                              void *user);

/// Notification emitted when one accepted transmission is completed.
typedef void (*sl_bt_link_message_transmit_callback_t)(sl_bt_link_t *link,
                                                       sl_status_t status,
                                                       void *user);

/// Notification emitted when the endpoint or runtime reports an error.
typedef void (*sl_bt_link_error_callback_t)(sl_bt_link_t *link,
                                            sl_status_t status,
                                            void *user);

/// Notification emitted when the endpoint handshake completes.
typedef void (*sl_bt_link_connect_callback_t)(sl_bt_link_t *link,
                                              void *user);

/// Link creation parameters.
typedef struct {
  sl_cpc_bus_t                  *bus;          ///< CPC bus instance.
  sl_bt_link_runtime_t          *runtime;      ///< Shared event queue owner.
  sl_bt_link_storage_t          *storage;      ///< Binding-owned link storage.
  sl_bt_link_validator_t        is_valid;      ///< Payload check, or NULL.
  sl_bt_link_message_callback_t on_message;    ///< RX complete, or NULL.
  sl_bt_link_message_transmit_callback_t on_transmit;  ///< TX complete, or NULL.
  sl_bt_link_error_callback_t   on_error;      ///< Link error, or NULL.
  sl_bt_link_connect_callback_t on_connected;  ///< Handshake done, or NULL.
  void                          *user;         ///< User data.
  sl_bt_link_role_t             role;          ///< Local CPC role.
  uint8_t                       endpoint_id;   ///< CPC endpoint identifier.
} sl_bt_link_config_t;

/**************************************************************************//**
 * Create a message link from the static object pool.
 *
 * @param[in] config Link configuration.
 * @param[out] link Created link object.
 * @return SL_STATUS_OK on success, otherwise an error code.
 *****************************************************************************/
sl_status_t sl_bt_link_create(const sl_bt_link_config_t *config,
                              sl_bt_link_t **link);

/**************************************************************************//**
 * Destroy a stopped link object and return it to the object pool.
 *
 * @param[in] link Link object.
 * @return SL_STATUS_OK on success, otherwise an error code.
 *****************************************************************************/
sl_status_t sl_bt_link_destroy(sl_bt_link_t *link);

/**************************************************************************//**
 * Start connecting or listening on the configured endpoint.
 *
 * @param[in] link Link object.
 * @return SL_STATUS_OK on success, otherwise an error code.
 *****************************************************************************/
sl_status_t sl_bt_link_start(sl_bt_link_t *link);

/**************************************************************************//**
 * Close the endpoint asynchronously.
 *
 * @param[in] link Link object.
 *****************************************************************************/
void sl_bt_link_close(sl_bt_link_t *link);

/**************************************************************************//**
 * Send exactly one complete payload in one endpoint frame.
 *
 * The payload is copied to binding-provided link storage and remains owned by
 * the link until transmit completion. SL_STATUS_BUSY indicates that the
 * transmit storage is occupied.
 *
 * @param[in] link Link object.
 * @param[in] data Complete payload.
 * @param[in] length Payload length in bytes.
 * @return SL_STATUS_OK on success, otherwise an error code.
 *****************************************************************************/
sl_status_t sl_bt_link_send(sl_bt_link_t *link,
                            const uint8_t *data,
                            size_t length);

/**************************************************************************//**
 * Remove the oldest complete payload from the receive queue.
 *
 * @param[in] link Link object.
 * @param[out] message Received message object.
 * @return SL_STATUS_OK on success or SL_STATUS_EMPTY if no message is ready.
 *****************************************************************************/
sl_status_t sl_bt_link_pop(sl_bt_link_t *link,
                           sl_bt_link_message_t **message);

/**************************************************************************//**
 * Return a received payload and its storage to the link.
 *
 * @param[in] link Link object.
 * @param[in] message Message previously returned by @ref sl_bt_link_pop.
 * @return SL_STATUS_OK on success, otherwise an error code.
 *****************************************************************************/
sl_status_t sl_bt_link_release(sl_bt_link_t *link,
                               sl_bt_link_message_t *message);

/**************************************************************************//**
 * Check whether the endpoint is connected.
 *
 * @param[in] link Link object.
 * @return true if connected, otherwise false.
 *****************************************************************************/
bool sl_bt_link_is_open(sl_bt_link_t *link);

/**************************************************************************//**
 * Get the total payload length.
 *
 * @param[in] message Message object.
 * @return Payload length in bytes.
 *****************************************************************************/
size_t sl_bt_link_get_length(const sl_bt_link_message_t *message);

/**************************************************************************//**
 * Get the number of unread bytes in a payload.
 *
 * @param[in] message Message object.
 * @return Number of unread bytes.
 *****************************************************************************/
size_t sl_bt_link_get_remaining(const sl_bt_link_message_t *message);

/**************************************************************************//**
 * Get a read-only pointer to the complete payload.
 *
 * @param[in] message Message object.
 * @return Pointer to the payload or NULL for an invalid object.
 *****************************************************************************/
const uint8_t *sl_bt_link_get_data(const sl_bt_link_message_t *message);

/**************************************************************************//**
 * Read bytes from the current payload position.
 *
 * @param[in] message Message object.
 * @param[out] data Destination buffer.
 * @param[in] length Number of bytes to read.
 * @return SL_STATUS_OK on success, otherwise an error code.
 *****************************************************************************/
sl_status_t sl_bt_link_read(sl_bt_link_message_t *message,
                            uint8_t *data,
                            size_t length);

/** @} (end addtogroup sl_bt_link) */

#endif // SL_BT_LINK_H
