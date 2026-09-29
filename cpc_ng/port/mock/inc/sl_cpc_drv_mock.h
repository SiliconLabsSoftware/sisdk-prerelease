#ifndef SL_CPC_DRV_MOCK_H
#define SL_CPC_DRV_MOCK_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "sl_slist.h"

#include "../../../src/sli_cpc_drv.h"
#include "../../../src/sli_cpc_frame_list.h"
#include "../../../src/sli_cpc_hdr.h"

#include "../../../src/unit/frame.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief A copy of an outgoing CPC frame captured by the mock driver.
 *
 * The mock driver does not retain the live `sl_cpc_frame_t` objects the
 * core hands to `write()`; those are completed immediately so the core can
 * recycle them. Instead it makes a heap copy of the on-wire content
 * (header followed by `payload_size` bytes of payload) using a single
 * allocation thanks to the flexible array at the end of the struct.
 * Payload bytes are flattened from a scatter-gather buffer chain when
 * present (`sl_cpc_buf_t::tot_len` on the chain head).
 *
 * Ownership of captured packets is transferred to the test on
 * `mock_cpc_drv_pop_tx_packet()`. Use `mock_cpc_tx_packet_free()` to
 * release them.
 */
typedef struct mock_cpc_tx_packet {
  /// Linkage in the driver's TX capture list (FIFO, via `sl_slist_*`).
  sl_slist_node_t node;
  /// CPC protocol header captured verbatim at `write()` time.
  sli_cpc_hdr_t header;
  /// Payload bytes, allocated contiguously with the struct.
  uint8_t payload[];
} mock_cpc_tx_packet_t;

/**
 * @brief Private data carried by the mock CPC driver (`sl_cpc_drv_mock_t`).
 *
 * Ops take `sl_cpc_bus_t *` and recover the driver handle with
 * `container_of(bus, sl_cpc_drv_mock_t, bus)`.
 *
 * Exposed in the header so test cases can inspect/poke internal queues
 * directly when convenient.
 */
typedef struct sl_cpc_drv_mock {
  /// Embedded CPC core bus. Ops recover this driver with
  /// `container_of(bus, sl_cpc_drv_mock_t, bus)`.
  sl_cpc_bus_t bus;

  /// FIFO of captured outgoing frames (`mock_cpc_tx_packet_t` nodes). Tests
  /// pop from the head with `mock_cpc_drv_pop_tx_packet()`.
  sl_slist_node_t *tx_head;

  /// Frames pre-queued by the test that will be delivered to the core on
  /// the next `ops.read()` call.
  sli_cpc_frame_list_t rx_pending_queue;

  /// Optional validator invoked once per outgoing frame at `write()` time.
  /// Returns 0 on success or a non-zero (e.g. line number) on failure.
  int (*tx_validate_fn)(sl_cpc_frame_t *frame);

  /// Capability blob returned by `ops.get_local_capabilities()`. NULL by
  /// default which yields an empty capability set.
  const void *local_capabilities;
  uint16_t local_capabilities_size;

  /// Capability blob most recently received via `ops.set_remote_capabilities()`,
  /// captured here so tests can inspect what the core negotiated.
  const void *remote_capabilities;
  uint16_t remote_capabilities_size;

  /// Driver-reported "free TX slots". Tests may lower this to simulate
  /// back-pressure from the bus.
  uint32_t available_write_frame_slots;

  /// When true, `write()` queues frames in @ref held_tx_frames instead of
  /// notifying the core immediately. Use `mock_cpc_drv_complete_held_tx()` to
  /// simulate a slow driver finishing transmission.
  bool defer_tx_complete;

  /// Frames accepted by `write()` but not yet reported as transmitted.
  sli_cpc_frame_list_t held_tx_frames;

  /// Counters that tests can assert on without reading the full queues.
  uint32_t hw_init_count;
  uint32_t init_count;
  uint32_t start_rx_count;
  uint32_t on_rx_frame_free_count;

  bool rx_enabled;
} sl_cpc_drv_mock_t;

/** @brief Mock CPC driver configuration. */
typedef struct sl_cpc_drv_mock_config {
} sl_cpc_drv_mock_config_t;

/**
 * @brief Initialize a mock CPC driver and its embedded bus.
 *
 * Sets up private queues/defaults, then initializes the CPC bus.
 *
 * @param[in] drv  Mock driver handle. Must not be NULL.
 * @param[in] cfg  Mock driver configuration. Must not be NULL.
 * @param[in] bus_cfg  Bus configuration. Must not be NULL.
 *
 * @retval SL_STATUS_OK Driver and bus initialized successfully.
 * @retval Other        An error occurred.
 */
sl_status_t sl_cpc_drv_mock_init(sl_cpc_drv_mock_t *drv, const sl_cpc_drv_mock_config_t *cfg,
                                 const sl_cpc_bus_config_t *bus_cfg);

/**
 * @brief Get the CPC bus embedded in a mock driver instance.
 *
 * @param[in] drv  Mock driver handle. Must not be NULL.
 * @return Pointer to the driver's bus.
 */
static inline sl_cpc_bus_t *sl_cpc_drv_mock_get_bus(sl_cpc_drv_mock_t *drv)
{
  return &drv->bus;
}

extern sl_cpc_drv_mock_t sl_cpc_drv_mock_instances[];

/**
 * @brief Deinitialize a mock CPC driver previously created with
 *        @ref sl_cpc_drv_mock_init.
 *
 * @param[in] drv Pointer to the driver. Must not be NULL.
 */
void mock_cpc_drv_deinit(sl_cpc_drv_mock_t *drv);

/**
 * @brief Pop the next captured outgoing frame.
 *
 * Packets are returned in FIFO order. Ownership of the returned packet is
 * transferred to the caller, who must release it with
 * `mock_cpc_tx_packet_free()`.
 *
 * @param[in] drv Mock driver.
 * @return The next captured packet, or NULL if the queue is empty.
 */
mock_cpc_tx_packet_t *mock_cpc_drv_pop_tx_packet(sl_cpc_drv_mock_t *drv);

/**
 * @brief Free a packet returned by `mock_cpc_drv_pop_tx_packet()`.
 *
 * Both the header copy and the trailing payload are released in a single
 * `free()` call. The pointer is cleared on return.
 *
 * @param[in,out] packet Address of the pointer to free. May be NULL or
 *                       point to NULL (no-op in either case).
 */
void mock_cpc_tx_packet_free(mock_cpc_tx_packet_t **packet);

/**
 * @brief Number of captured TX packets still waiting in the mock queue.
 */
uint32_t mock_cpc_drv_pending_tx_count(sl_cpc_drv_mock_t *drv);

/**
 * @brief Inject a frame into the driver's RX queue.
 *
 * On the next `ops.read()`, the frame is handed to the core as if it had
 * arrived from the bus.
 *
 * @param[in] drv     Mock driver.
 * @param[in] frame   CPC frame to enqueue. Its ownership is transferred
 *                    to the driver until the core consumes it.
 */
void mock_cpc_drv_push_rx_frame(sl_cpc_drv_mock_t *drv, sl_cpc_frame_t *frame);

/**
 * @brief Inject a CPC frame as if it had been received from the wire.
 *
 * Allocates a fresh RX `sl_cpc_frame_t` from the bound bus's pool,
 * populates its CPC header from `frame`, builds an RX payload buffer chain via
 * `sli_cpc_alloc_rx_payload()`, scatters `frame->payload` across the chain
 * segments, queues the frame on the driver's RX pending list and signals
 * the bus so it processes the frame on its next tick.
 *
 * @code
 *   mock_cpc_drv_inject_frame(__LINE__, sl_cpc_drv_mock_default, &(cpc_frame_expect_t){
 *     .ack   = next_expected_ack,
 *   });
 * @endcode
 *
 * @param[in] lineno  Source line to attribute failures to.
 * @param[in] drv     Mock driver. Must be bound to a CPC bus.
 * @param[in] frame   Frame description. Must not be NULL. Read-only: seq/ack
 *                    normalization is applied to a local copy so the caller's
 *                    struct is left unchanged.
 */
void mock_cpc_drv_inject_frame(unsigned int lineno, sl_cpc_drv_mock_t *drv, const cpc_frame_expect_t *frame);

/**
 * @brief Override the number of available TX frame slots the driver reports.
 *
 * Useful to simulate driver back-pressure. Defaults to UINT32_MAX.
 */
void mock_cpc_drv_set_available_write_frame_slots(sl_cpc_drv_mock_t *drv, uint32_t slots);

/**
 * @brief Hold outgoing TX frames until `mock_cpc_drv_complete_held_tx()` is called.
 *
 * Simulates a driver that has accepted frames but not yet reported them as
 * transmitted to the CPC core.
 */
void mock_cpc_drv_set_defer_tx_complete(sl_cpc_drv_mock_t *drv, bool defer);

/**
 * @brief Report held TX frames as transmitted to the CPC core.
 *
 * Invokes `sli_cpc_bus_notify_tx_data_by_drv()` for every frame
 * queued since defer mode was enabled.
 *
 * @p count indicates the number of frames to complete. Any negative
 * value complete all held frames.
 */
unsigned int mock_cpc_drv_complete_held_tx(sl_cpc_drv_mock_t *drv, int count);

/**
 * @brief Number of TX frames held by the driver pending completion.
 */
uint32_t mock_cpc_drv_held_tx_count(const sl_cpc_drv_mock_t *drv);

/**
 * @brief Register a validator invoked for each outgoing TX frame.
 *
 * The validator runs synchronously inside `ops.write()` before the frame
 * is added to the mock TX queue. Returning a non-zero value (typically
 * `__LINE__` of the failing assertion) marks the frame as failed but does
 * not prevent it from being queued.
 *
 * Pass NULL to disable validation.
 */
void mock_cpc_drv_set_tx_validation(sl_cpc_drv_mock_t *drv, int (*validate_fn)(sl_cpc_frame_t *));

/**
 * @brief Configure the capability blob returned by `get_local_capabilities()`.
 *
 * The pointer is stored as-is; it must remain valid for the lifetime of the
 * mock driver.
 */
void mock_cpc_drv_set_local_capabilities(sl_cpc_drv_mock_t *drv, const void *caps, uint16_t caps_size);

#ifdef __cplusplus
}
#endif

#endif /* SL_CPC_DRV_MOCK_H */
