/***************************************************************************//**
 * @file sli_rail_mux_aux.c
 * @brief RAIL multiplexer aux (register): RX FIFO drain and Zigbee stack
 *        @c sli_zigbee_stack_rail_mux_aux_* bodies / default-aux state.
 *******************************************************************************
 * # License
 * <b>Copyright 2018 Silicon Laboratories Inc. www.silabs.com</b>
 *******************************************************************************
 *
 * The licensor of this software is Silicon Laboratories Inc. Your use of this
 * software is governed by the terms of Silicon Labs Master Software License
 * Agreement (MSLA) available at
 * www.silabs.com/about-us/legal/master-software-license-agreement. This
 * software is distributed to you in Source Code format and is governed by the
 * sections of the MSLA applicable to Source Code.
 *
 ******************************************************************************/
#ifdef SL_COMPONENT_CATALOG_PRESENT
#include "sl_component_catalog.h"
#endif

#include PLATFORM_HEADER
#ifdef SL_CATALOG_HIGH_DATARATE_PHY_PRESENT
#include "high_datarate_phy_config.h"
#endif

#include "sl_code_classification.h"
#include "sl_rail.h"
#include "sl_rail_ieee802154.h"
#ifdef SL_CATALOG_RAIL_MULTIPLEXER_PRESENT
#include "sl_rail_mux_rename.h"
#endif
#include "app/framework/include/sl_zigbee_rail_mux_aux_raw.h"
#include "event_queue/event-queue.h"
#include "sli_rail_mux_aux.h"
#include "mac-mux-aux.h"
#include "mac-phy.h"
#include "lower-mac-mux-aux.h"

extern uint8_t sli_zigbee_stack_mac_mux_aux_get_channel(void);
extern sli_zigbee_event_queue_t sli_zigbee_stack_event_queue;

extern void sli_zigbee_stack_rail_mux_aux_event_callback(sl_rail_handle_t rail_handle,
                                                         sl_rail_events_t events,
                                                         uint8_t *psdu,
                                                         uint16_t psdu_length,
                                                         uint8_t lqi,
                                                         int8_t rssi_dbm,
                                                         uint32_t packet_time);
extern sl_rail_status_t sli_zigbee_stack_rail_mux_aux_register_protocol(sl_rail_handle_t *out_rail_handle,
                                                                        sl_rail_config_t *rail_config,
                                                                        sl_rail_init_complete_callback_t init_complete_callback);
extern sl_rail_status_t sli_zigbee_stack_rail_mux_aux_unregister_protocol(void);
extern void sli_zigbee_stack_rtos_stack_wakeup_isr_handler(void);

// ----- RX drain (register aux handle) ---------------------------------

// 802.15.4 2.4 GHz PSDU max 127 octets; RAIL may report additional bytes; cap at one PHY frame.
#define SLI_RAIL_MUX_AUX_RX_SCRATCH_BYTES 256u
// Bound work per RX callback so OT active scan / bursty RF cannot wedge the system in a tight loop.
#define SLI_RAIL_MUX_AUX_RX_MAX_DRAIN_PER_EVENT 16u

// Static scratch avoids large stack use if the events callback runs in interrupt context.
static uint8_t sli_rail_mux_aux_rx_scratch[SLI_RAIL_MUX_AUX_RX_SCRATCH_BYTES];

SL_CODE_CLASSIFY(SL_CODE_COMPONENT_RAIL_MUX_15_4, SL_CODE_CLASS_TIME_CRITICAL)
static uint32_t sli_rail_mux_aux_rx_drain(sl_rail_handle_t rail_handle,
                                          sli_rail_mux_aux_rx_packet_callback_t callback)
{
  uint32_t drained = 0U;
  for (uint32_t iter = 0U; iter < SLI_RAIL_MUX_AUX_RX_MAX_DRAIN_PER_EVENT; iter++) {
    sl_rail_rx_packet_info_t packet_info;
    sl_rail_rx_packet_handle_t packet_handle = sl_rail_get_rx_packet_info(rail_handle,
                                                                          SL_RAIL_RX_PACKET_HANDLE_NEWEST,
                                                                          &packet_info);
    if (packet_handle == SL_RAIL_RX_PACKET_HANDLE_INVALID) {
      break;
    }

    // RAIL may raise RX_PACKET_RECEIVED for READY_SUCCESS or READY_CRC_ERROR (see sl_rail_types.h).
    if (packet_info.packet_status != SL_RAIL_RX_PACKET_READY_SUCCESS
        && packet_info.packet_status != SL_RAIL_RX_PACKET_READY_CRC_ERROR) {
      (void) sl_rail_release_rx_packet(rail_handle, packet_handle);
      continue;
    }

    if (packet_info.packet_bytes > SLI_RAIL_MUX_AUX_RX_SCRATCH_BYTES) {
      (void) sl_rail_release_rx_packet(rail_handle, packet_handle);
      continue;
    }

    sl_rail_status_t cpy = sl_rail_copy_rx_packet(rail_handle,
                                                  sli_rail_mux_aux_rx_scratch,
                                                  &packet_info);
    if (cpy == SL_RAIL_STATUS_NO_ERROR) {
      if (callback != NULL) {
        sl_rail_rx_packet_details_t details;
        uint8_t lqi = 0U;
        int8_t rssi = 0;
        uint32_t t = 0U;
        uint16_t rx_channel = 0xFFFFU;
        if (sl_rail_get_rx_packet_details(rail_handle, packet_handle, &details) == SL_RAIL_STATUS_NO_ERROR) {
          lqi = details.lqi;
          rssi = details.rssi_dbm;
          t = (uint32_t)details.time_received.packet_time;
          rx_channel = (uint16_t)details.channel;
        }
        // Carry the received channel to the deferred handler (this drain runs in the RAIL events /
        // ISR path). The handler prints it and filters out primary-channel frames that the shared
        // FCS radio also hears, so the aux MAC only processes genuine aux-channel traffic.
        sli_rail_mux_aux_rx_packet_t packet = {
          .psdu = sli_rail_mux_aux_rx_scratch,
          .packet_time = t,
          .psdu_length = packet_info.packet_bytes,
          .channel = rx_channel,
          .lqi = lqi,
          .rssi_dbm = rssi,
        };
        callback(&packet);
      }
      drained++;
    }
    (void) sl_rail_release_rx_packet(rail_handle, packet_handle);
  }
  return drained;
}

uint32_t sli_rail_mux_aux_rx_on_event(sl_rail_handle_t rail_handle)
{
  return sli_rail_mux_aux_rx_drain(rail_handle, NULL);
}

uint32_t sli_rail_mux_aux_rx_on_event_with_packet_callback(sl_rail_handle_t rail_handle,
                                                           sli_rail_mux_aux_rx_packet_callback_t callback)
{
  return sli_rail_mux_aux_rx_drain(rail_handle, callback);
}

// ----- Zigbee stack rail-mux-aux default listen / IPC stack bodies ----------

// Per-aux-protocol TX staging for the RAIL multiplexer: when another mux context owns the
// hardware TX FIFO, sl_rail_write_tx_fifo() copies into fifo_tx_info.data_ptr (sl_rail_mux.c).
// The aux listen rail_config must supply a non-NULL buffer and tx_fifo_bytes or that memcpy
// targets NULL (EFM_ASSERT may be disabled in release → corruption, WDG, bogus SP/PC).
#if defined(SL_HDR_PHY_MAX_PACKET_SIZE)
#define SLI_RAIL_MUX_AUX_MAX_FLAT_PACKET_SIZE ((SL_HDR_PHY_MAX_PACKET_SIZE) + 4)
#else
#define SLI_RAIL_MUX_AUX_MAX_FLAT_PACKET_SIZE 127
#endif
#define SLI_RAIL_MUX_AUX_ALIGNED_MAX_FLAT_PACKET_SIZE \
  ((((SLI_RAIL_MUX_AUX_MAX_FLAT_PACKET_SIZE) + 3) / 4) * 4)
#define SLI_RAIL_MUX_AUX_TX_FIFO_BYTES_UNCLAMPED \
  ((SLI_RAIL_MUX_AUX_ALIGNED_MAX_FLAT_PACKET_SIZE) << 1)
#if SLI_RAIL_MUX_AUX_TX_FIFO_BYTES_UNCLAMPED > SL_RAIL_MAXIMUM_FIFO_BYTES
#define SLI_RAIL_MUX_AUX_TX_FIFO_BYTES SL_RAIL_MAXIMUM_FIFO_BYTES
#else
#define SLI_RAIL_MUX_AUX_TX_FIFO_BYTES SLI_RAIL_MUX_AUX_TX_FIFO_BYTES_UNCLAMPED
#endif

SL_ALIGN(4) static uint8_t sli_rail_mux_aux_tx_fifo_buffer[SLI_RAIL_MUX_AUX_TX_FIFO_BYTES] SL_ATTRIBUTE_ALIGN(4);

static volatile uint32_t sli_rail_mux_aux_rx_packets;
static sl_rail_handle_t sli_rail_mux_aux_registered_handle = NULL;

// Defer aux callback dispatch to stack event context (never directly from RAIL ISR path).
#define SLI_RAIL_MUX_AUX_DEFERRED_CB_QUEUE_LEN 8u
typedef struct {
  sl_rail_handle_t rail_handle;
  sl_rail_events_t events;
  uint16_t psdu_length;
  uint8_t lqi;
  int8_t rssi_dbm;
  uint32_t packet_time;
  uint16_t channel;
  uint32_t tx_attempt_id;
  uint8_t psdu[SLI_RAIL_MUX_AUX_RX_SCRATCH_BYTES];
} sli_rail_mux_aux_deferred_cb_t;

static sli_rail_mux_aux_deferred_cb_t sli_rail_mux_aux_deferred_cb_queue[SLI_RAIL_MUX_AUX_DEFERRED_CB_QUEUE_LEN];
static volatile uint8_t sli_rail_mux_aux_deferred_cb_head;
static volatile uint8_t sli_rail_mux_aux_deferred_cb_tail;
static volatile uint8_t sli_rail_mux_aux_deferred_cb_count;
static volatile uint32_t sli_rail_mux_aux_deferred_cb_dropped;

SL_WEAK void sli_rail_mux_aux_deferred_cb_drop_hook(uint32_t dropped_count)
{
  (void)dropped_count;
}

static void sli_rail_mux_aux_isr_event_handler(sli_zigbee_event_t *event);
static void sli_rail_mux_aux_deferred_event_handler(sli_zigbee_event_t *event);

static sli_zigbee_event_t sli_rail_mux_aux_isr_event = {
  {
    &sli_zigbee_stack_event_queue,
    sli_rail_mux_aux_isr_event_handler,
    sli_zigbee_isr_event_marker,
    ""
  },
  NULL,
  0,
  0,
  NULL
};

static sli_zigbee_event_t sli_rail_mux_aux_deferred_event = {
  {
    &sli_zigbee_stack_event_queue,
    sli_rail_mux_aux_deferred_event_handler,
    NULL,
    ""
  },
  NULL,
  0,
  0,
  NULL
};

static void sli_rail_mux_aux_reset_deferred_cb_queue(void)
{
  ATOMIC(
    sli_rail_mux_aux_deferred_cb_head = 0U;
    sli_rail_mux_aux_deferred_cb_tail = 0U;
    sli_rail_mux_aux_deferred_cb_count = 0U;
    sli_rail_mux_aux_deferred_cb_dropped = 0U;
    );
}

static bool sli_rail_mux_aux_enqueue_deferred_cb(sl_rail_handle_t rail_handle,
                                                 sl_rail_events_t events,
                                                 const sli_rail_mux_aux_rx_packet_t *packet)
{
  bool queued = false;
  bool dropped = false;
  uint32_t dropped_count = 0U;
  const sli_rail_mux_aux_rx_packet_t empty_packet = {
    .psdu = NULL,
    .packet_time = 0U,
    .psdu_length = 0U,
    .channel = 0xFFFFU,
    .lqi = 0U,
    .rssi_dbm = 0,
  };
  if (packet == NULL) {
    packet = &empty_packet;
  }
  uint16_t copy_len = packet->psdu_length;

  if (copy_len > SLI_RAIL_MUX_AUX_RX_SCRATCH_BYTES) {
    copy_len = SLI_RAIL_MUX_AUX_RX_SCRATCH_BYTES;
  }

  ATOMIC(
    if (sli_rail_mux_aux_deferred_cb_count < SLI_RAIL_MUX_AUX_DEFERRED_CB_QUEUE_LEN) {
    sli_rail_mux_aux_deferred_cb_t *entry = &sli_rail_mux_aux_deferred_cb_queue[sli_rail_mux_aux_deferred_cb_tail];
    entry->rail_handle = rail_handle;
    entry->events = events;
    entry->psdu_length = (packet->psdu != NULL) ? copy_len : 0U;
    entry->lqi = packet->lqi;
    entry->rssi_dbm = packet->rssi_dbm;
    entry->packet_time = packet->packet_time;
    entry->channel = packet->channel;
    entry->tx_attempt_id = sli_mac_lower_mac_mux_aux_get_tx_attempt_id();
    if ((packet->psdu != NULL) && (copy_len > 0U)) {
      for (uint16_t i = 0U; i < copy_len; i++) {
        entry->psdu[i] = packet->psdu[i];
      }
    }
    sli_rail_mux_aux_deferred_cb_tail = (uint8_t)((sli_rail_mux_aux_deferred_cb_tail + 1U)
                                                  % SLI_RAIL_MUX_AUX_DEFERRED_CB_QUEUE_LEN);
    sli_rail_mux_aux_deferred_cb_count++;
    queued = true;
  } else {
    sli_rail_mux_aux_deferred_cb_dropped++;
    dropped = true;
    dropped_count = sli_rail_mux_aux_deferred_cb_dropped;
  }
    );

  if (dropped) {
    sli_rail_mux_aux_deferred_cb_drop_hook(dropped_count);
  }

  return queued;
}

static bool sli_rail_mux_aux_dequeue_deferred_cb(sli_rail_mux_aux_deferred_cb_t *out)
{
  bool has_item = false;

  ATOMIC(
    if (sli_rail_mux_aux_deferred_cb_count > 0U) {
    *out = sli_rail_mux_aux_deferred_cb_queue[sli_rail_mux_aux_deferred_cb_head];
    sli_rail_mux_aux_deferred_cb_head = (uint8_t)((sli_rail_mux_aux_deferred_cb_head + 1U)
                                                  % SLI_RAIL_MUX_AUX_DEFERRED_CB_QUEUE_LEN);
    sli_rail_mux_aux_deferred_cb_count--;
    has_item = true;
  }
    );

  return has_item;
}

static void sli_rail_mux_aux_isr_event_handler(sli_zigbee_event_t *event)
{
  (void)event;
  sli_zigbee_event_set_active(&sli_rail_mux_aux_deferred_event);
  sli_zigbee_stack_rtos_stack_wakeup_isr_handler();
}

static void sli_rail_mux_aux_deferred_event_handler(sli_zigbee_event_t *event)
{
  sli_rail_mux_aux_deferred_cb_t cb_event;
  (void)event;

  while (sli_rail_mux_aux_dequeue_deferred_cb(&cb_event)) {
    // Inject RX into the Zigbee MAC only from stack event context. Doing this from the
    // RAIL events callback (ISR) dropped beacon-request handling on the aux channel, so
    // the dual-channel coordinator never answered joins on the aux listen channel.
    if (((cb_event.events & SL_RAIL_EVENT_RX_PACKET_RECEIVED) != 0U)
        && (cb_event.psdu_length > 0U)) {
      sli_mac_mux_aux_dbg(0xD5, (uint32_t)cb_event.channel);
      // An ACK is the result of this context's own TX, never upper-MAC RX. sl_rail_mux only
      // raises an ACK to the context that owns the active TX, so terminate the TX here
      // without a channel test.
      if (!sli_mac_lower_mac_mux_aux_handle_ack(cb_event.psdu,
                                                cb_event.psdu_length)) {
        // Everything else belongs to the upper MAC, but only when it really arrived on the aux
        // channel: the shared FCS/concurrent-listen radio also hears the primary channel, and
        // injecting a primary-channel frame here makes the coordinator answer a primary-channel
        // peer on the aux channel. Fail open when the channel is unknown (0xFFFF).
        // HDR frames are received on aux_channel + SL_HDR_PHY_CHANNEL_OFFSET (= +16).
        if (cb_event.channel == 0xFFFFU
            || cb_event.channel == (uint16_t)sli_zigbee_stack_mac_mux_aux_get_channel()
#ifdef HIGH_DATARATE_PHY
            || cb_event.channel == ((uint16_t)sli_zigbee_stack_mac_mux_aux_get_channel()
                                    + SL_HDR_PHY_CHANNEL_OFFSET)
#endif // HIGH_DATARATE_PHY
            ) {
          (void)sli_mac_mux_aux_inject_rx_to_upper_mac(cb_event.psdu,
                                                       cb_event.psdu_length,
                                                       cb_event.lqi,
                                                       cb_event.rssi_dbm,
                                                       cb_event.packet_time,
                                                       (cb_event.channel == 0xFFFFU)
                                                       ? 0xFFU
                                                       : (uint8_t)cb_event.channel);
        }
      }
    }

    sl_rail_events_t tx_events = cb_event.events
                                 & (SL_RAIL_EVENT_TX_PACKET_SENT
                                    | SL_RAIL_EVENT_TX_ABORTED
                                    | SL_RAIL_EVENT_TX_UNDERFLOW
                                    | SL_RAIL_EVENT_TX_CHANNEL_BUSY
                                    | SL_RAIL_EVENT_TX_BLOCKED
                                    | SL_RAIL_EVENT_RX_ACK_TIMEOUT
                                    | SL_RAIL_EVENT_SCHEDULER_STATUS);
    if (tx_events != 0U) {
      sli_mac_mux_aux_dbg(0xC8, ((uint32_t)((tx_events & SL_RAIL_EVENT_SCHEDULER_STATUS) != 0U ? 1U : 0U) << 16)
                          | (uint32_t)((tx_events >> 16) & 0xFFFFU));
      sli_mac_lower_mac_mux_aux_handle_tx_events(tx_events,
                                                 cb_event.tx_attempt_id);
    }
    bool is_hdr_rx = false;
#ifdef HIGH_DATARATE_PHY
    // HDR packet is being handled by different callback
    is_hdr_rx = ((cb_event.events & SL_RAIL_EVENT_RX_PACKET_RECEIVED) != 0U)
                && SLI_MAC_IS_HDR_PHY_CHANNEL((uint8_t)cb_event.channel);
#endif // !HIGH_DATARATE_PHY
    if (!is_hdr_rx) {
      sli_zigbee_stack_rail_mux_aux_event_callback(cb_event.rail_handle,
                                                   cb_event.events,
                                                   (cb_event.psdu_length > 0U) ? cb_event.psdu : NULL,
                                                   cb_event.psdu_length,
                                                   cb_event.lqi,
                                                   cb_event.rssi_dbm,
                                                   cb_event.packet_time);
    }
  }
}

// Always defer RX delivery to the stack event loop. Never call into the Zigbee MAC
// (buffer alloc / receive_callback / beacon scheduling) directly from the RAIL ISR path.
static void sli_rail_mux_aux_rx_to_event_bridge(sli_rail_mux_aux_rx_packet_t *packet)
{
  if (sli_rail_mux_aux_enqueue_deferred_cb(sli_rail_mux_aux_registered_handle,
                                           SL_RAIL_EVENT_RX_PACKET_RECEIVED,
                                           packet)) {
    sli_zigbee_event_set_active(&sli_rail_mux_aux_isr_event);
    sli_zigbee_stack_rtos_stack_wakeup_isr_handler();
  }
}

static void sli_rail_mux_aux_rail_events_cb(sl_rail_handle_t rail_handle, sl_rail_events_t events)
{
  /* One app callback receives both RX-delivery and non-RX event notifications. */
  if (rail_handle != sli_rail_mux_aux_registered_handle) {
    return;
  }
  if ((events & SL_RAIL_EVENT_RX_PACKET_RECEIVED) != 0U) {
    sli_rail_mux_aux_rx_packets += sli_rail_mux_aux_rx_on_event_with_packet_callback(rail_handle,
                                                                                     sli_rail_mux_aux_rx_to_event_bridge);
  }
  sl_rail_events_t non_rx_events = events & ~SL_RAIL_EVENT_RX_PACKET_RECEIVED;
  if (non_rx_events != 0U) {
    if (sli_rail_mux_aux_enqueue_deferred_cb(rail_handle,
                                             non_rx_events,
                                             NULL)) {
      sli_zigbee_event_set_active(&sli_rail_mux_aux_isr_event);
      sli_zigbee_stack_rtos_stack_wakeup_isr_handler();
    }
  }
}

static sl_rail_config_t sli_rail_mux_aux_rail_cfg = {
  .events_callback = sli_rail_mux_aux_rail_events_cb,
  .p_tx_fifo_buffer = (sl_rail_fifo_buffer_align_t *)sli_rail_mux_aux_tx_fifo_buffer,
  .tx_fifo_bytes = SLI_RAIL_MUX_AUX_TX_FIFO_BYTES,
  .tx_fifo_init_bytes = 0U,
};

sl_rail_config_t *sli_zigbee_stack_rail_mux_aux_get_default_rail_config(void)
{
  return &sli_rail_mux_aux_rail_cfg;
}

uint32_t sli_zigbee_stack_rail_mux_aux_get_default_listen_rx_packet_count(void)
{
  return sli_rail_mux_aux_rx_packets;
}

void sli_rail_mux_aux_on_register_success(sl_rail_handle_t rail_handle)
{
  sli_rail_mux_aux_registered_handle = rail_handle;
  sli_rail_mux_aux_rx_packets = 0U;
  sli_rail_mux_aux_reset_deferred_cb_queue();
}

void sli_rail_mux_aux_on_unregister_begin(void)
{
  sli_mac_mux_aux_on_unregistered();
}

void sli_rail_mux_aux_on_unregister_success(void)
{
  sli_rail_mux_aux_registered_handle = NULL;
  sli_rail_mux_aux_rx_packets = 0U;
  sli_rail_mux_aux_reset_deferred_cb_queue();
#ifdef HIGH_DATARATE_PHY
  // Release the HDR routing slot — no context owns HDR while aux is absent.
  sl_rail_mux_clear_high_datarate_phy_index();
#endif // HIGH_DATARATE_PHY
}

sl_rail_handle_t sli_zigbee_stack_rail_mux_aux_get_default_rail_handle(void)
{
  return sli_rail_mux_aux_registered_handle;
}

uint16_t sli_zigbee_stack_rail_mux_aux_write_tx_fifo(sl_rail_handle_t rail_handle,
                                                     const uint8_t *data_ptr,
                                                     uint16_t write_length,
                                                     bool reset)
{
  return sl_rail_write_tx_fifo(rail_handle, data_ptr, write_length, reset);
}

sl_rail_status_t sli_zigbee_stack_rail_mux_aux_start_tx(sl_rail_handle_t rail_handle,
                                                        uint8_t channel,
                                                        sl_rail_tx_options_t options,
                                                        const sl_rail_scheduler_info_t *scheduler_info)
{
  return sl_rail_start_tx(rail_handle, channel, options, scheduler_info);
}

sl_rail_status_t sli_zigbee_stack_rail_mux_aux_start_cca_csma_tx(sl_rail_handle_t rail_handle,
                                                                 uint8_t channel,
                                                                 sl_rail_tx_options_t options,
                                                                 const sl_rail_csma_config_t *csma_config,
                                                                 const sl_rail_scheduler_info_t *scheduler_info)
{
  return sl_rail_start_cca_csma_tx(rail_handle, channel, options, csma_config, scheduler_info);
}

sl_rail_handle_t sli_rail_mux_aux_get_registered_handle(void)
{
  return sli_rail_mux_aux_registered_handle;
}

static sl_rail_status_t sli_rail_mux_aux_configure_mac_identity(sl_rail_handle_t handle,
                                                                sl_802154_pan_id_t pan_id,
                                                                sl_802154_short_addr_t short_addr,
                                                                const sl_802154_long_addr_t eui64,
                                                                bool promiscuous)
{
  sl_rail_status_t st = sl_rail_ieee802154_set_pan_id(handle, pan_id, 0);
  if (st != SL_RAIL_STATUS_NO_ERROR) {
    return st;
  }

  st = sl_rail_ieee802154_set_short_address(handle, short_addr, 0);
  if (st != SL_RAIL_STATUS_NO_ERROR) {
    return st;
  }

  if (eui64 != NULL) {
    st = sl_rail_ieee802154_set_long_address(handle, eui64, 0);
    if (st != SL_RAIL_STATUS_NO_ERROR) {
      return st;
    }
  }

  return sl_rail_ieee802154_set_promiscuous_mode(handle, promiscuous);
}

sl_rail_status_t sli_rail_mux_aux_register_for_mac(sl_802154_pan_id_t pan_id,
                                                   sl_802154_short_addr_t short_addr,
                                                   const sl_802154_long_addr_t eui64,
                                                   uint8_t channel,
                                                   bool promiscuous)
{
  sl_rail_handle_t out_handle = NULL;
  sli_rail_mux_aux_dbg_step(0xD0, ((uint32_t)pan_id << 16) | channel);
  sl_rail_status_t st = sli_zigbee_stack_rail_mux_aux_register_protocol(&out_handle,
                                                                        sli_zigbee_stack_rail_mux_aux_get_default_rail_config(),
                                                                        NULL);

  sli_rail_mux_aux_dbg_step(0xD1, (uint32_t)st);
  if (st != SL_RAIL_STATUS_NO_ERROR) {
    return st;
  }

#ifdef HIGH_DATARATE_PHY
  sl_rail_mux_set_high_datarate_phy_index(out_handle);
#endif // HIGH_DATARATE_PHY

  st = sli_rail_mux_aux_configure_mac_identity(out_handle, pan_id, short_addr, eui64, promiscuous);
  sli_rail_mux_aux_dbg_step(0xD2, (uint32_t)st);
  if (st != SL_RAIL_STATUS_NO_ERROR) {
    sli_mac_mux_aux_internal_unregister_protocol();
    return st;
  }

  st = sl_rail_config_events(out_handle,
                             SL_RAIL_EVENTS_ALL,
                             (SL_RAIL_EVENT_CAL_NEEDED
                              | SL_RAIL_EVENT_RX_PACKET_RECEIVED
                              | SL_RAIL_EVENT_RX_ACK_TIMEOUT
                              | SL_RAIL_EVENT_TX_PACKET_SENT
                              | SL_RAIL_EVENT_TX_ABORTED
                              | SL_RAIL_EVENT_TX_UNDERFLOW
                              | SL_RAIL_EVENT_TX_CHANNEL_BUSY
                              | SL_RAIL_EVENT_TX_BLOCKED
                              | SL_RAIL_EVENT_SCHEDULER_STATUS
#ifdef HIGH_DATARATE_PHY
                              | SL_RAIL_EVENT_RX_SYNC_1_DETECT
#endif //HIGH_DATARATE_PHY
                             ));
  sli_rail_mux_aux_dbg_step(0xD3, (uint32_t)st);
  if (st != SL_RAIL_STATUS_NO_ERROR) {
    sli_mac_mux_aux_internal_unregister_protocol();
    return st;
  }

  sli_rail_mux_aux_dbg_step(0xD4, (uint32_t)channel);
#ifdef HIGH_DATARATE_PHY
  (void)sl_rail_ieee802154_config_2_mbps_rx_channel(out_handle, channel);
#endif // HIGH_DATARATE_PHY
  st = sl_rail_start_rx(out_handle, channel, NULL);
  sli_rail_mux_aux_dbg_step(0xD5, (uint32_t)st);
  if (st != SL_RAIL_STATUS_NO_ERROR) {
    sli_mac_mux_aux_internal_unregister_protocol();
    return st;
  }

  return SL_RAIL_STATUS_NO_ERROR;
}

sl_rail_status_t sli_rail_mux_aux_update_mac_identity(sl_802154_pan_id_t pan_id,
                                                      sl_802154_short_addr_t short_addr,
                                                      const sl_802154_long_addr_t eui64)
{
  sl_rail_handle_t handle = sli_rail_mux_aux_registered_handle;
  if (handle == NULL) {
    return SL_RAIL_STATUS_INVALID_STATE;
  }
  return sli_rail_mux_aux_configure_mac_identity(handle, pan_id, short_addr, eui64, false);
}

sl_rail_status_t sli_rail_mux_aux_set_pan_coordinator(bool is_coordinator)
{
  sl_rail_handle_t handle = sli_rail_mux_aux_registered_handle;
  if (handle == NULL) {
    return SL_RAIL_STATUS_INVALID_STATE;
  }
  // Renamed to the mux per-context setter under SL_CATALOG_RAIL_MULTIPLEXER_PRESENT; the mux
  // OR-aggregates the coordinator flag across contexts before pushing it down to the radio.
  return sl_rail_ieee802154_set_pan_coordinator(handle, is_coordinator);
}

sl_rail_status_t sli_zigbee_stack_rail_mux_aux_register_and_start_rx(uint8_t channel)
{
  if (channel == 0U) {
    return SL_RAIL_STATUS_INVALID_PARAMETER;
  }
  sli_mac_mux_aux_set_channel(channel);
  sl_status_t mac_st = sli_mac_mux_aux_bring_up_from_primary(0);
  if (mac_st == SL_STATUS_OK) {
    return SL_RAIL_STATUS_NO_ERROR;
  }
  sli_mac_mux_aux_request_bring_up();
  return SL_RAIL_STATUS_NO_ERROR;
}
