/**************************************************************************//**
 * @file
 * @brief Private runtime contract for cpc-ng message-link events.
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
#ifndef SLI_BT_LINK_RUNTIME_H
#define SLI_BT_LINK_RUNTIME_H

#include "sl_cpc_buf.h"
#include "sl_bt_link.h"
#include "sl_bt_link_runtime.h"

/// Deferred link event posted to the runtime queue.
typedef enum {
  sli_bt_link_event_receive = 0, ///< A complete CPC receive buffer is ready.
  sli_bt_link_event_transmit,    ///< Transmit of the pending payload finished.
  sli_bt_link_event_connected,   ///< Endpoint handshake completed.
  sli_bt_link_event_closed,      ///< Endpoint reached CLOSED.
  sli_bt_link_event_error        ///< Endpoint or CPC reported an error.
} sli_bt_link_event_type_t;

/// Runtime queue element that carries one link event.
typedef struct {
  sl_bt_link_t             *link;   ///< Link that owns the event.
  sl_cpc_buf_t             *buffer; ///< Receive or transmit buffer, or NULL.
  sl_status_t               status; ///< Completion or error status.
  sli_bt_link_event_type_t  type;   ///< Event class.
} sli_bt_link_event_t;

/******************************************************************************
 * Queue a link event for processing on the runtime context.
 *
 * Safe to call from CPC callbacks. The event is copied into the runtime queue.
 *
 * @param[in] runtime Runtime object.
 * @param[in] event Event to enqueue.
 * @return SL_STATUS_OK on success, otherwise an error code.
 *****************************************************************************/
sl_status_t sli_bt_link_runtime_post(sl_bt_link_runtime_t *runtime,
                                     const sli_bt_link_event_t *event);

/******************************************************************************
 * Acquire the runtime guard before mutating link state.
 *
 * @param[in] runtime Runtime object.
 * @return SL_STATUS_OK on success, otherwise an error code.
 *****************************************************************************/
sl_status_t sli_bt_link_runtime_acquire(sl_bt_link_runtime_t *runtime);

/******************************************************************************
 * Release the runtime guard acquired with @ref sli_bt_link_runtime_acquire.
 *
 * @param[in] runtime Runtime object.
 * @return SL_STATUS_OK on success, otherwise an error code.
 *****************************************************************************/
sl_status_t sli_bt_link_runtime_release(sl_bt_link_runtime_t *runtime);

/******************************************************************************
 * Reserve queue capacity for a link's receive buffers.
 *
 * @param[in] runtime Runtime object.
 * @param[in] event_count Number of queue elements required by the link.
 * @return SL_STATUS_OK on success, otherwise an error code.
 *****************************************************************************/
sl_status_t sli_bt_link_runtime_reserve(sl_bt_link_runtime_t *runtime,
                                        size_t event_count);

/******************************************************************************
 * Release queue capacity previously reserved for a link.
 *
 * @param[in] runtime Runtime object.
 * @param[in] event_count Number of queue elements to release.
 * @return SL_STATUS_OK on success, otherwise an error code.
 *****************************************************************************/
sl_status_t sli_bt_link_runtime_unreserve(sl_bt_link_runtime_t *runtime,
                                          size_t event_count);

/******************************************************************************
 * Apply a queued event to the link and invoke binding callbacks as needed.
 *
 * Called on the runtime context after @ref sli_bt_link_runtime_post.
 *
 * @param[in] event Event popped from the runtime queue.
 *****************************************************************************/
void sli_bt_link_process_event(const sli_bt_link_event_t *event);

#endif // SLI_BT_LINK_RUNTIME_H
