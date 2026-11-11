/***************************************************************************/ /**
 * @file sl_cpc_ep.h
 * @brief CPC public endpoint API.
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

#ifndef SL_CPC_EP_H
#define SL_CPC_EP_H

#include <stdalign.h>
#include <stdbool.h>
#include <stdint.h>

#include "sl_component_catalog.h"
#include "sl_cpc_config.h"
#include "sl_slist.h"
#include "sl_status.h"

#if defined(SL_CATALOG_KERNEL_PRESENT)
#include "cmsis_os2.h"
#if defined(SL_CATALOG_CMSIS_OS_COMMON_PRESENT)
#include "sl_cmsis_os2_common.h"
#endif
#endif

#include "sl_cpc_frame.h"
#include "sl_cpc_msgq.h"

#include "sli_cpc_timer_types.h"
#include "sli_cpc_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/***************************************************************************/ /**
 * @addtogroup cpc CPC
 * @{
 ******************************************************************************/

/******************************************************************************/
/*                                   Types                                    */
/******************************************************************************/

typedef struct sl_cpc_bus sl_cpc_bus_t;

typedef struct sl_cpc_ep sl_cpc_ep_t;

/// Maximum CPC application payload size, in bytes.
#define SL_CPC_EP_MAX_PAYLOAD_SIZE 4096U

/// Reserved endpoint IDs.
///
/// IDs 0–1 are reserved for CPC services.
/// IDs 2–89 are reserved for Silicon Labs services.
/// IDs 90–99 are reserved for application-defined endpoints.
/// IDs 100–255 are reserved for temporary endpoints allocated dynamically at runtime.
typedef enum sl_cpc_reserved_ep_id {
  SL_CPC_EP_ID_CONTROL = 0,           ///< CPC control endpoint.
  SL_CPC_EP_ID_RESERVED = 1,          ///< Reserved; do not use.
  SL_CPC_EP_ID_BLUETOOTH = 2,         ///< Bluetooth endpoint.
  SL_CPC_EP_ID_RAIL_DOWNSTREAM = 3,   ///< RAIL downstream endpoint.
  SL_CPC_EP_ID_RAIL_UPSTREAM = 4,     ///< RAIL upstream endpoint.
  SL_CPC_EP_ID_ZIGBEE = 5,            ///< Zigbee EZSP endpoint.
  SL_CPC_EP_ID_ZWAVE = 6,             ///< Z-Wave endpoint.
  SL_CPC_EP_ID_CONNECT = 7,           ///< Connect endpoint.
  SL_CPC_EP_ID_GPIO = 8,              ///< GPIO endpoint.
  SL_CPC_EP_ID_OPENTHREAD = 9,        ///< OpenThread Spinel endpoint.
  SL_CPC_EP_ID_WISUN = 10,            ///< Wi-SUN endpoint.
  SL_CPC_EP_ID_WIFI = 11,             ///< Wi-Fi endpoint (main control).
  SL_CPC_EP_ID_15_4 = 12,             ///< 802.15.4 endpoint.
  SL_CPC_EP_ID_CLI = 13,              ///< ASCII-based CLI for stacks and applications.
  SL_CPC_EP_ID_BLUETOOTH_RCP = 14,    ///< Bluetooth RCP endpoint.
  SL_CPC_EP_ID_ACP = 15,              ///< ACP endpoint.
  SL_CPC_EP_ID_SE = 16,               ///< Secure Engine endpoint.
  SL_CPC_EP_ID_NVM3 = 17,             ///< NVM3 endpoint.
  SL_CPC_EP_ID_MATTER = 18,           ///< Matter endpoint.
  SL_CPC_EP_ID_USER_START = 90,       ///< First application-defined endpoint ID.
  SL_CPC_EP_ID_USER_END = 99,         ///< Last application-defined endpoint ID.
  SL_CPC_EP_ID_TEMPORARY_START = 100, ///< First dynamically allocated temporary endpoint ID.
  SL_CPC_EP_ID_TEMPORARY_END = 255    ///< Last dynamically allocated temporary endpoint ID.
} sl_cpc_reserved_ep_id_t;

/// Endpoint events delivered to @ref sl_cpc_ep_event_cb_t.
typedef enum sl_cpc_ep_event_type {
  SL_CPC_EP_EVENT_RECV,      ///< Data received.
  SL_CPC_EP_EVENT_SEND_DONE, ///< Transmission completed (acknowledged or aborted).
  SL_CPC_EP_EVENT_CONNECTED, ///< Endpoint connected.
  SL_CPC_EP_EVENT_CLOSED,    ///< Endpoint closed, safe to reconnect or deinit after this event.
  SL_CPC_EP_EVENT_ERROR,     ///< Fatal error on the endpoint.
} sl_cpc_ep_event_type_t;

/**
 * @brief Error payload for @ref SL_CPC_EP_EVENT_ERROR.
 */
typedef struct sl_cpc_ep_event_error {
  sl_status_t status; ///< Error status.
} sl_cpc_ep_event_error_t;

/**
 * @brief Receive payload for @ref SL_CPC_EP_EVENT_RECV.
 */
typedef struct sl_cpc_ep_event_recv {
  sl_cpc_buf_t *buf; ///< Buffer with the received payload.
} sl_cpc_ep_event_recv_t;

/**
 * @brief Send-done payload for @ref SL_CPC_EP_EVENT_SEND_DONE.
 */
typedef struct sl_cpc_ep_event_send_done {
  sl_cpc_buf_t *buf;     ///< Buffer that finished transmission.
  sl_cpc_frame_t *frame; ///< Send context from @ref sl_cpc_ep_send.
  sl_status_t status;    ///< Transmission status.
  void *arg;             ///< @p arg passed to @ref sl_cpc_ep_send.
} sl_cpc_ep_event_send_done_t;

/**
 * @brief Event-specific data for @ref sl_cpc_ep_event_cb_t.
 */
typedef union sl_cpc_ep_event {
  sl_cpc_ep_event_error_t error;
  sl_cpc_ep_event_recv_t recv;
  sl_cpc_ep_event_send_done_t send_done;
} sl_cpc_ep_event_t;

/***************************************************************************/ /**
 * Endpoint event callback.
 *
 * All endpoint events are delivered here. Use @p event_type to select which
 * fields of @p event are valid. Runs in CPC's execution context: do minimal
 * work, do not block, and hand the event off to the application if needed.
 *
 * @param ep         Endpoint that produced the event.
 * @param event_type Event that occurred.
 * @param event      Event-specific data; valid only for this call.
 * @param arg        User argument from @ref sl_cpc_ep_init.
 ******************************************************************************/
typedef void(sl_cpc_ep_event_cb_t)(sl_cpc_ep_t *ep, sl_cpc_ep_event_type_t event_type, const sl_cpc_ep_event_t *event,
                                   void *arg);

/**
 * @brief Round-trip time estimator for one endpoint.
 *
 * The RTT is measured only once per window so frames do not carry a uint64_t
 * timestamp. This keeps the sequence number of the frame used for the
 * measurement and the timestamp at which it was sent.
 */
struct sl_cpc_ep_rtt {
  uint8_t frame_seq;   ///< Sequence number of the frame used for measurement.
  uint64_t sent_ticks; ///< Timestamp at which the frame with sequence `frame_seq` was sent.
  uint64_t smoothed;   ///< smoothed rtt
  uint64_t variation;  ///< rtt variation
};

/**
 * @brief CPC endpoint.
 *
 * Bidirectional communication channel on a bus.
 */
typedef struct sl_cpc_ep {
  sl_slist_node_t node; ///< node
  uint8_t id;           ///< id
  uint8_t flags;        ///< flags

  struct {
    /// Send next, the next sequence number that will be used for transmission
    /// Used to keep track of unacknowledged frames and frame sequence allowed
    /// for transmission
    uint8_t nxt;

    /// Send Unacknowledged, the oldest unacknowledged sequence number
    uint8_t una;

    /// Send window, maximum number of frames that the remote can accept
    /// TX frames should have a sequence in the range [send.una; send.una + send.wnd)
    uint8_t wnd;
  } send;

  struct sl_cpc_ep_rtt rtt;

  struct {
    /// Max payload length this endpoint is able to receive.
    uint16_t local;

    /// Maximum payload length the remote endpoint is able to receive.
    uint16_t remote;
  } mtu;

  uint8_t ack;                       ///< ack
  uint8_t packet_re_transmit_count;  ///< packet re transmit count
  uint32_t re_transmit_timeout;      ///< re transmit timeout
  sli_cpc_timer_t re_transmit_timer; ///< re transmit timer
  sli_cpc_ep_state_t state;          ///< state

  struct {
    uint32_t timeout; ///< window probe schedule timeout
    uint8_t count;    ///< window probes sent while still blocked
  } wnd_probe;

  /// Usage counter of the endpoint. This is used to guarantee that the
  /// endpoint is not going away while some objects have a reference to it.
  ///   - TX frames take a reference to the endpoint when transmitted
  ///     (no matter if they come from the application or from CPC itself).
  ///     This reference is dropped when the transmission completes, or
  ///     when the TX frame and buffer are passed back to the application.
  ///     This ensures that the ep->event_cb refers to a valid endpoint
  ///     pointer.
  ///   - RX frames take a reference in the drivers. frame->ep either points
  ///     to NULL or to a valid endpoint pointer. This ensures that when the
  ///     RX frame is processed in CPC's loop, the endpoint is still valid.
  ///     This reference is dropped when CPC is done with the frame if
  ///     the frame doesn't have a payload, or when the application gets
  ///     the frame if it has a payload. Note that the endpoint's state can
  ///     still change between the reception by the driver and the processing
  ///     of that frame by CPC's loop.
  ///   - @ref sl_cpc_ep_listen / @ref sl_cpc_ep_connect take a reference when
  ///     the endpoint is added to the bus list so drivers always see list
  ///     members with ref_cnt >= 1. That reference is dropped in
  ///     @ref sl_cpc_ep_close, or in @ref sl_cpc_ep_deinit if open never
  ///     succeeded.
  ///   - public open APIs also set @c SLI_CPC_EP_FLAG_OPENED (immediately, or
  ///     when a deferred open is pending); closing then drops the list
  ///     reference taken above.
  ///
  /// When all references are dropped, the endpoint is considered closed and
  /// the event callback is called to notify the application. After that, the
  /// endpoint can be freed or reused for another connection.
  uint16_t ref_cnt;

  sl_cpc_ep_event_cb_t *event_cb;           ///< unified endpoint event callback
  void *event_cb_arg;                       ///< Opaque user context passed to @ref event_cb.
  sli_cpc_ep_frame_list_t re_transmit_list; ///< re-transmit list
  sli_cpc_ep_frame_list_t holding_list;     ///< holding list
  sl_cpc_msgq_t rx_buffer_queue;            ///< reception buffer queue
#if (SL_CPC_DEBUG_EP_EVENT_COUNTERS == 1)
  sli_cpc_ep_debug_counters_t debug_counters;
#endif
#if defined(SL_CATALOG_KERNEL_PRESENT)
  struct {
#if defined(SL_CATALOG_CMSIS_OS_COMMON_PRESENT)
    alignas(4) uint8_t lock_cb[osMutexCbSize]; ///< lock cb
#endif
    osMutexId_t lock;                   ///< lock
    osSemaphoreId_t state_event_signal; ///< endpoint state has changed
  } kernel;
#endif
#if defined(SL_CATALOG_CPC_SECURITY_PRESENT)
  uint32_t frame_counter_rx;
  uint32_t frame_counter_tx;
  bool encrypted;
  bool packets_held_for_security;
#endif

  sl_cpc_bus_t *bus;
} sl_cpc_ep_t;

/******************************************************************************/
/*                                    APIs                                    */
/******************************************************************************/

/***************************************************************************/ /**
 * Initialize an endpoint.
 *
 * After this returns, receive buffers may be queued with
 * @ref sl_cpc_ep_push_recv_buf. @p rx_size is the largest payload this endpoint
 * can receive; buffers pushed for RX must be at least that size. If buffers are
 * larger than that size, that extra space will be wasted as CPC won't be able to
 * use it.
 *
 * Re-initializing an already initialized endpoint is undefined behavior.
 *
 * @param[in] ep      Endpoint to initialize.
 * @param[in] id      Endpoint ID shared with the remote side.
 * @param[in] rx_size Maximum receive payload size; must be a multiple of
 *                    @ref SL_CPC_BUF_MIN_ALIGNMENT.
 * @param[in] cb      Event callback.
 * @param[in] cb_arg  User argument passed to @p cb. May be @c NULL.
 *
 * @retval SL_STATUS_OK           Endpoint initialized successfully.
 * @retval SL_STATUS_NULL_POINTER @p cb is NULL.
 * @retval Other                  An error occurred.
 ******************************************************************************/
sl_status_t sl_cpc_ep_init(sl_cpc_ep_t *ep, uint8_t id, uint16_t rx_size, sl_cpc_ep_event_cb_t *cb, void *cb_arg);

/***************************************************************************/ /**
 * Deinitialize an endpoint.
 *
 * If the endpoint was opened with @ref sl_cpc_ep_listen or @ref sl_cpc_ep_connect,
 * it must be closed with @ref sl_cpc_ep_close prior to calling this function.
 * Note that @sl_cpc_ep_close is asynchronous, refer to its documentation for
 * proper usage.
 *
 * After a successful return, the endpoint is invalid and must be reinitialized with
 * @ref sl_cpc_ep_init before it can be used again.
 *
 * @param[in] ep Endpoint to deinitialize.
 *
 * @retval SL_STATUS_OK            Endpoint deinitialized.
 * @retval SL_STATUS_INVALID_STATE Endpoint is still open, closing, or has
 *                                 already been deinitialized.
 ******************************************************************************/
sl_status_t sl_cpc_ep_deinit(sl_cpc_ep_t *ep);

/***************************************************************************/ /**
 * Get an endpoint's ID.
 *
 * @param[in] ep Endpoint to query.
 *
 * @return Endpoint ID.
 ******************************************************************************/
uint8_t sl_cpc_ep_get_id(const sl_cpc_ep_t *ep);

/***************************************************************************/ /**
 * Add a buffer (or buffer chain) to the endpoint's receive pool.
 *
 * @p buf can be a single buffer or a buffer chain. All elements in the chain
 * must have pointers aligned on @ref SL_CPC_BUF_MIN_ALIGNMENT. This function
 * sets the length of each buffer in the chain to the `rx_size` value that was
 * configured by @ref sl_cpc_ep_init. The caller is responsible for not passing
 * buffers smaller than this value. Buffers can have a larger underlying
 * storage than `rx_size` but this extra space will never be used by CPC.
 *
 * @param[in] ep   Endpoint that will receive into @p buf.
 * @param[in] buf  Buffer, or chain of buffers, available for reception.
 *
 * @retval SL_STATUS_OK                Buffer queued successfully.
 * @retval SL_STATUS_NULL_POINTER      @p buf or @p buf->ptr is NULL.
 * @retval SL_STATUS_INVALID_PARAMETER Buffer is not correctly aligned.
 * @retval SL_STATUS_NOT_AVAILABLE     Endpoint has no configured MTU.
 ******************************************************************************/
sl_status_t sl_cpc_ep_push_recv_buf(sl_cpc_ep_t *ep, sl_cpc_buf_t *buf);

/***************************************************************************/ /**
 * Pop a single reception buffer from endpoint's receive pool.
 *
 * If a buffer chain was pushed with @ref sl_cpc_ep_push_recv_buf, this
 * function must be called once for each element that was part of the chain to
 * pop all buffers.
 *
 * Can only be called when the endpoint is closed.
 *
 * @param[in]  ep      Endpoint whose RX pool to pop from.
 * @param[out] out_buf Set to the popped buffer on success.
 *
 * @retval SL_STATUS_OK               Buffer popped successfully.
 * @retval SL_STATUS_NO_MORE_RESOURCE Endpoint has no available RX buffer.
 * @retval SL_STATUS_INVALID_STATE    Endpoint is not closed.
 ******************************************************************************/
sl_status_t sl_cpc_ep_pop_recv_buf(sl_cpc_ep_t *ep, sl_cpc_buf_t **out_buf);

/***************************************************************************/ /**
 * Listen for an incoming connection on an endpoint.
 *
 * If @p bus is set, the endpoint listens on that specific bus only.
 *
 * If @p bus is NULL, the endpoint listens on any bus. It is bound to the first
 * bus that receives a connection for its ID.
 *
 * An endpoint that listens on a specific bus has priority over ones that listen
 * on any bus. For example, if one endpoint listens for ID 10 on busX while
 * another endpoint listens for ID 10 on any bus, the connection on busX will be
 * routed to the endpoint that specifically listens for ID 10 on busX.
 *
 * Note that the same ID cannot be used by two endpoints on the same specific
 * bus. For example, two endpoints cannot both listen for ID 10 on the same bus.
 *
 * However, several endpoints may listen for the same ID on any bus, because in
 * this case each endpoint will be bound to a different bus for each connection.
 * For example, if multiple endpoints listen for ID 10 on any bus, the first
 * connection will be routed to the first endpoint, and the second connection
 * will be routed to the second endpoint, and so on.
 *
 * @ref SL_CPC_EP_ID_CONTROL is reserved; the bus owns that endpoint.
 *
 * @param[in] ep  Endpoint that will accept the connection.
 * @param[in] bus Bus to listen on, or NULL to listen on any bus.
 *
 * @retval SL_STATUS_OK                    Listen started successfully.
 * @retval SL_STATUS_INVALID_PARAMETER     @p ep is the control endpoint.
 * @retval SL_STATUS_INVALID_STATE         Endpoint is not in a valid state to listen.
 * @retval SL_STATUS_ALREADY_INITIALIZED   Another endpoint already listens for this ID
 *                                         on @p bus.
 * @retval SL_STATUS_INVALID_CONFIGURATION Endpoint is not properly configured.
 ******************************************************************************/
sl_status_t sl_cpc_ep_listen(sl_cpc_ep_t *ep, sl_cpc_bus_t *bus);

/***************************************************************************/ /**
 * Connect an endpoint.
 *
 * @param[in] ep  Endpoint that initiates the connection.
 * @param[in] bus Bus the endpoint belongs to.
 *
 * @retval SL_STATUS_OK                    Connect started successfully.
 * @retval SL_STATUS_NULL_POINTER          @p bus is NULL.
 * @retval SL_STATUS_INVALID_PARAMETER     @p ep is the control endpoint.
 * @retval SL_STATUS_INVALID_STATE         Endpoint is not in a valid state to connect.
 * @retval SL_STATUS_ALREADY_INITIALIZED   Another endpoint with the same ID is open.
 * @retval SL_STATUS_INVALID_CONFIGURATION Endpoint is not properly configured.
 ******************************************************************************/
sl_status_t sl_cpc_ep_connect(sl_cpc_ep_t *ep, sl_cpc_bus_t *bus);

/***************************************************************************/ /**
 * Start the closure of an endpoint.
 *
 * Pending sends are cancelled and complete with status @ref SL_STATUS_TRANSMIT_INCOMPLETE.
 * If the application needs to wait for buffers to be transmitted before
 * closing, it must wait for @ref SL_CPC_EP_EVENT_SEND_DONE with status @ref
 * SL_STATUS_OK for every outstanding send.
 *
 * When endpoint is closed, event callback is called with event @ref SL_CPC_EP_EVENT_CLOSED.
 * It can then be re-opened with @ref sl_cpc_ep_listen or @ref sl_cpc_ep_connect,
 * or destroyed with @ref sl_cpc_ep_deinit.
 *
 * @param[in] ep Endpoint to close.
 ******************************************************************************/
void sl_cpc_ep_close(sl_cpc_ep_t *ep);

/***************************************************************************/ /**
 * Queue a buffer or buffer chain for transmission to the endpoint.
 *
 * @p buf and @p frame must remain valid until @ref SL_CPC_EP_EVENT_SEND_DONE.
 * All buffers in the buffer chain must have data pointer aligned on
 * @ref SL_CPC_BUF_MIN_ALIGNMENT. The chain can be constructed with
 * @ref sl_cpc_buf_chain.
 *
 * @param[in] ep    Endpoint to send on.
 * @param[in] buf   Buffer, or chain of buffers, to transmit.
 * @param[in] frame Per-send context used while processing @p buf.
 * @param[in] arg   Returned in @ref SL_CPC_EP_EVENT_SEND_DONE.
 *
 * @retval SL_STATUS_OK Buffer queued successfully.
 * @retval Other        An error occurred.
 ******************************************************************************/
sl_status_t sl_cpc_ep_send(sl_cpc_ep_t *ep, sl_cpc_buf_t *buf, sl_cpc_frame_t *frame, void *arg);

/** @} (end addtogroup cpc) */

#ifdef __cplusplus
}
#endif

#endif // SL_CPC_EP_H
