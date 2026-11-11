/***************************************************************************//**
 * @file sli_mac_mux_aux_bridge.c
 * @brief Glue between upper/lower MAC mac_index 1 and RAIL mux aux context.
 *******************************************************************************
 * # License
 * <b>Copyright 2026 Silicon Laboratories Inc. www.silabs.com</b>
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

#include PLATFORM_HEADER
#include "mac-mux-aux.h"
#include "upper-mac.h"
#include "lower-mac.h"
#include "mac-header.h"
#include "mac-phy.h"
#include "sli_rail_mux_aux.h"
#include "buffer_manager/buffer-management.h"
#include "event_queue/event-queue.h"
#include "stack/core/sl_zigbee_multi_network.h"
#include "sl_token_manager_api.h"
#include "stack/include/sl_zigbee_token.h"
#include "stack/config/sl_zigbee_token_defines.h"

extern sl_rail_status_t sli_zigbee_stack_rail_mux_aux_unregister_protocol(void);
extern sli_zigbee_event_queue_t sli_zigbee_stack_event_queue;

static bool sli_mac_mux_aux_bridge_active;
static uint8_t sli_mac_mux_aux_channel;
static bool sli_mac_mux_aux_channel_cached;
static bool sli_mac_mux_aux_interpan_enabled;

extern void sli_802154mac_packet_send_complete_callback(uint8_t mac_index,
                                                        sl_status_t status,
                                                        sli_zigbee_packet_header_t packet,
                                                        uint8_t tag);
// Nesting count for an internal unregister (leave / failed bring-up). While it
// is non-zero, on_unregistered keeps the stored channel. An app unregister
// leaves it at 0 and stores channel 0.
static uint8_t sli_mac_mux_aux_internal_unregister_count;

// Deferred aux reconcile: bringing the aux up/down idles and reconfigures the shared mux radio
// (sl_rail_start_rx -> CONFIGURE_RX_CHANNEL_SWITCHING -> sl_rail_idle(wait)). Doing that inline
// from sli_mac_set_nwk_radio_parameters() during network formation hung the radio and tripped the
// watchdog, so the reconcile is scheduled to run from the stack event loop where the radio is
// between operations.
static void sli_mac_mux_aux_reconcile_handler(sli_zigbee_event_t *event);

static sli_zigbee_event_t sli_mac_mux_aux_reconcile_event = {
  {
    &sli_zigbee_stack_event_queue,
    sli_mac_mux_aux_reconcile_handler,
    NULL,
    ""
  },
  NULL,
  0,
  0,
  NULL
};

// Register/start aux RX only after the stack is joined and OT/BLE has had time to settle.
#define SLI_MAC_MUX_AUX_BRING_UP_DELAY_MS 2000U
#define SLI_MAC_MUX_AUX_BRING_UP_RETRY_MS 5000U

static void sli_mac_mux_aux_bring_up_handler(sli_zigbee_event_t *event);

static sli_zigbee_event_t sli_mac_mux_aux_bring_up_event = {
  {
    &sli_zigbee_stack_event_queue,
    sli_mac_mux_aux_bring_up_handler,
    NULL,
    ""
  },
  NULL,
  0,
  0,
  NULL
};

static bool sli_mac_mux_aux_network_is_up(void)
{
  return (sli_zigbee_state == SL_ZIGBEE_JOINED_NETWORK
          || sli_zigbee_state == SL_ZIGBEE_JOINED_NETWORK_NO_PARENT
          || sli_zigbee_state == SL_ZIGBEE_JOINED_NETWORK_S2S_INITIATOR
          || sli_zigbee_state == SL_ZIGBEE_JOINED_NETWORK_S2S_TARGET);
}

bool sli_mac_mux_aux_should_reconcile(void)
{
  // Identity sync only after aux is live; never reconcile (or schedule bring_up) during form.
  return sli_mac_mux_aux_network_is_up() && sli_mac_mux_aux_bridge_active;
}

bool sli_mac_is_mux_aux_mac_index(uint8_t mac_index)
{
  return (mac_index == SL_MAC_AUX_INDEX);
}

bool sli_mac_mux_aux_is_active(void)
{
  return sli_mac_mux_aux_bridge_active;
}

sl_rail_handle_t sli_mac_mux_aux_get_rail_handle(void)
{
  return sli_rail_mux_aux_get_registered_handle();
}

uint8_t sli_zigbee_stack_mac_mux_aux_get_channel(void)
{
  if (!sli_mac_mux_aux_channel_cached) {
    uint8_t channel = 0;
    if (slx_zigbee_token_manager_get_data(COMMON_TOKEN_MAC_MUX_AUX_CHANNEL,
                                          &channel,
                                          sizeof(channel)) == SL_STATUS_OK) {
      sli_mac_mux_aux_channel = channel;
    }
    sli_mac_mux_aux_channel_cached = true;
  }
  return sli_mac_mux_aux_channel;
}

void sli_mac_mux_aux_set_channel(uint8_t channel)
{
  sli_mac_mux_aux_channel = channel;
  sli_mac_mux_aux_channel_cached = true;
  (void)slx_zigbee_token_manager_set_data(COMMON_TOKEN_MAC_MUX_AUX_CHANNEL,
                                          &channel,
                                          sizeof(channel));
}

void sli_zigbee_stack_mac_mux_aux_set_interpan_enabled(bool enable)
{
  sli_mac_mux_aux_interpan_enabled = enable;
}

bool sli_zigbee_stack_mac_mux_aux_is_interpan_enabled(void)
{
  return sli_mac_mux_aux_interpan_enabled;
}

sl_status_t sli_zigbee_stack_mac_mux_aux_send_raw_message(const uint8_t *message, uint8_t message_length, sl_zigbee_transmit_priority_t priority, bool useCca)
{
  if ((message == NULL) || (message_length == 0U)) {
    return SL_STATUS_INVALID_PARAMETER;
  }

  if (!sli_mac_mux_aux_is_active()
      || (sli_zigbee_stack_mac_mux_aux_get_channel() == 0U)) {
    return SL_STATUS_NOT_AVAILABLE;
  }

  sli_buffer_manager_buffer_t buffer =
    sli_legacy_buffer_manager_really_fill_buffer(message, message_length, false);
  if (buffer == NULL_BUFFER) {
    return SL_STATUS_ALLOCATION_FAILED;
  }

  sli_mac_mux_aux_dbg_println("[mux-aux] sl_zigbee_mux_aux_send_raw_message tx ch=%u mac=%u len=%u",
                              sli_zigbee_stack_mac_mux_aux_get_channel(),
                              SL_MAC_AUX_INDEX,
                              message_length);

  return sli_mac_send_raw_message(SL_MAC_AUX_INDEX,
                                  sli_zigbee_get_current_network_index(),
                                  buffer,
                                  (sl_mac_transmit_priority_t)priority,
                                  useCca,
                                  sli_802154mac_packet_send_complete_callback);
}

static sl_mac_radio_parameters_t *sli_mac_mux_aux_primary_params(uint8_t primary_mac_index)
{
  sl_mac_upper_mac_state_t *primary = sli_mac_upper_mac_state + primary_mac_index;
  if (primary->current_nwk >= SL_MAC_MAX_SUPPORTED_NETWORKS) {
    return primary->nwk_radio_parameters;
  }
  return primary->nwk_radio_parameters + primary->current_nwk;
}

static void sli_mac_mux_aux_mirror_upper_state(uint8_t primary_mac_index,
                                               sl_mac_radio_parameters_t *primary_params)
{
  sl_mac_upper_mac_state_t *aux_state = sli_mac_upper_mac_state + SL_MAC_AUX_INDEX;
  sl_mac_radio_parameters_t *aux_params = aux_state->nwk_radio_parameters;
  uint8_t current_nwk = sli_mac_upper_mac_state[primary_mac_index].current_nwk;
  uint8_t mirrored_nwk = (current_nwk < SL_MAC_MAX_SUPPORTED_NETWORKS) ? current_nwk : 0U;

  for (uint8_t i = 0; i < SL_MAC_MAX_SUPPORTED_NETWORKS; i++) {
    if (i == mirrored_nwk) {
      sl_mac_radio_parameters_t *dst = aux_params + i;
      memcpy(dst, primary_params, sizeof(sl_mac_radio_parameters_t));
      dst->channel = sli_zigbee_stack_mac_mux_aux_get_channel();
      dst->mac_index = SL_MAC_AUX_INDEX;
    }
  }
  aux_state->current_nwk = mirrored_nwk;
}

static void sli_mac_mux_aux_patch_aux_node_id(sl_802154_short_addr_t node_id)
{
  if (node_id == SL_ZIGBEE_USE_LONG_ADDRESS) {
    return;
  }
  sl_mac_upper_mac_state_t *aux_state = sli_mac_upper_mac_state + SL_MAC_AUX_INDEX;
  for (uint8_t i = 0; i < SL_MAC_MAX_SUPPORTED_NETWORKS; i++) {
    aux_state->nwk_radio_parameters[i].local_node_id = node_id;
  }
}

static void sli_mac_mux_aux_deactivate_mac(void)
{
  sli_mac_mux_aux_bridge_active = false;
  sli_zigbee_event_set_inactive(&sli_mac_mux_aux_bring_up_event);

  sl_mac_upper_mac_state_t *aux_state = sli_mac_upper_mac_state + SL_MAC_AUX_INDEX;
  for (uint8_t i = 0; i < SL_MAC_MAX_SUPPORTED_NETWORKS; i++) {
    aux_state->nwk_radio_parameters[i].rx_state = SL_MAC_RADIO_OFF;
  }
}

void sli_mac_mux_aux_on_unregistered(void)
{
  sli_mac_mux_aux_deactivate_mac();
  if (sli_mac_mux_aux_internal_unregister_count == 0U) {
    sli_mac_mux_aux_interpan_enabled = false;
    sli_mac_mux_aux_set_channel(0U);
  }
}

void sli_mac_mux_aux_internal_unregister_protocol(void)
{
  sli_mac_mux_aux_internal_unregister_count++;
  (void)sli_zigbee_stack_rail_mux_aux_unregister_protocol();
  sli_mac_mux_aux_internal_unregister_count--;
}

static void sli_mac_mux_aux_bring_up_handler(sli_zigbee_event_t *event)
{
  (void)event;
  sli_zigbee_event_set_inactive(&sli_mac_mux_aux_bring_up_event);
  sli_mac_mux_aux_dbg_println("[mux-aux] bring_up event fire");
  if (sli_zigbee_stack_mac_mux_aux_get_channel() == 0U) {
    sli_mac_mux_aux_dbg_println("[mux-aux] bring_up skip, aux channel not configured");
    return;
  }
  if (!sli_mac_mux_aux_network_is_up()) {
    sli_mac_mux_aux_dbg_println("[mux-aux] bring_up skip network down");
    return;
  }
  sl_status_t st = sli_mac_mux_aux_bring_up_from_primary(0);
  if (st != SL_STATUS_OK && !sli_mac_mux_aux_bridge_active) {
    sli_mac_mux_aux_dbg_println("[mux-aux] bring_up retry in %ums st=0x%02X",
                                SLI_MAC_MUX_AUX_BRING_UP_RETRY_MS,
                                (uint8_t)st);

    sli_zigbee_event_set_delay_ms(&sli_mac_mux_aux_bring_up_event, SLI_MAC_MUX_AUX_BRING_UP_RETRY_MS);
  }
}

static bool sli_mac_mux_aux_primary_ready(sl_mac_radio_parameters_t *params)
{
  return sli_mac_mux_aux_params_ready_for_bring_up(params);
}

bool sli_mac_mux_aux_params_ready_for_bring_up(const sl_mac_radio_parameters_t *params)
{
  return (params != NULL
          && params->rx_state != SL_MAC_RADIO_OFF
          && params->pan_id != SL_ZIGBEE_BROADCAST_PAN_ID
          && params->local_node_id != SL_ZIGBEE_USE_LONG_ADDRESS);
}

sl_status_t sli_mac_mux_aux_bring_up_from_primary(uint8_t primary_mac_index)
{
  uint8_t aux_channel = sli_zigbee_stack_mac_mux_aux_get_channel();
  if (aux_channel == 0U) {
    sli_mac_mux_aux_dbg_println("[mux-aux] bring_up skip, aux channel not configured");
    return SL_STATUS_INVALID_STATE;
  }

  sl_mac_radio_parameters_t *primary_params = sli_mac_mux_aux_primary_params(primary_mac_index);

  if (!sli_mac_mux_aux_primary_ready(primary_params)) {
    sli_mac_mux_aux_dbg_println("[mux-aux] bring_up skip not ready rx=%d node=0x%04X pan=0x%04X",
                                (int)primary_params->rx_state,
                                primary_params->local_node_id,
                                primary_params->pan_id);

    return SL_STATUS_INVALID_STATE;
  }

  if (sli_mac_mux_aux_bridge_active) {
    return sli_mac_mux_aux_sync_identity(primary_mac_index);
  }

  sl_802154_short_addr_t node_id = primary_params->local_node_id;
  sli_mac_mux_aux_dbg_println("[mux-aux] register pan=0x%04X node=0x%04X ch=%d",
                              primary_params->pan_id,
                              node_id,
                              aux_channel);

  sl_rail_status_t st = sli_rail_mux_aux_register_for_mac(primary_params->pan_id,
                                                          node_id,
                                                          primary_params->local_eui,
                                                          aux_channel,
                                                          false);
  if (st != SL_RAIL_STATUS_NO_ERROR) {
    sli_mac_mux_aux_dbg_println("[mux-aux] bring_up FAIL reg_st=0x%02X pan=0x%04X node=0x%04X",
                                (uint8_t)st,
                                primary_params->pan_id,
                                node_id);

    return SL_STATUS_FAIL;
  }

  sli_mac_mux_aux_bridge_active = true;
  sli_rail_mux_aux_dbg_step(0xF0, (uint32_t)st);
  sli_mac_mux_aux_mirror_upper_state(primary_mac_index, primary_params);
  sli_mac_mux_aux_patch_aux_node_id(node_id);
  sli_rail_mux_aux_dbg_step(0xF1, (uint32_t)(int32_t)primary_params->tx_power);
  (void)sli_mac_lower_mac_set_radio_power(SL_MAC_AUX_INDEX, primary_params->tx_power);
  sli_rail_mux_aux_dbg_step(0xF2, 0);
  // Apply the current coordinator role so a coordinator that was already formed before the
  // aux came up still responds to coordinator-addressed frames on the aux channel.
  (void)sli_mac_mux_aux_set_coordinator(sli_mac_get_coordinator());
  sli_rail_mux_aux_dbg_step(0xF3, 0);
  sli_mac_mux_aux_dbg_println("[mux-aux] bring_up ch=%d pan=0x%04X reg_st=0x%02X coord=%d",
                              aux_channel,
                              primary_params->pan_id,
                              (uint8_t)st,
                              (int)sli_mac_get_coordinator());
  sli_rail_mux_dbg_fcs();

  return SL_STATUS_OK;
}

sl_status_t sli_mac_mux_aux_sync_identity(uint8_t primary_mac_index)
{
  if (!sli_mac_mux_aux_bridge_active) {
    return SL_STATUS_INVALID_STATE;
  }

  sl_mac_radio_parameters_t *primary_params = sli_mac_mux_aux_primary_params(primary_mac_index);
  sl_802154_short_addr_t node_id = primary_params->local_node_id;
  sl_rail_status_t st = sli_rail_mux_aux_update_mac_identity(primary_params->pan_id,
                                                             node_id,
                                                             primary_params->local_eui);
  if (st != SL_RAIL_STATUS_NO_ERROR) {
    return SL_STATUS_FAIL;
  }

  sli_mac_mux_aux_mirror_upper_state(primary_mac_index, primary_params);
  sli_mac_mux_aux_patch_aux_node_id(node_id);
  (void)sli_mac_lower_mac_set_radio_power(SL_MAC_AUX_INDEX, primary_params->tx_power);
  (void)sli_mac_mux_aux_set_coordinator(sli_mac_get_coordinator());
  return SL_STATUS_OK;
}

sl_status_t sli_mac_mux_aux_set_coordinator(bool is_coordinator)
{
  if (!sli_mac_mux_aux_bridge_active) {
    return SL_STATUS_INVALID_STATE;
  }
  sl_rail_status_t st = sli_rail_mux_aux_set_pan_coordinator(is_coordinator);
  return (st == SL_RAIL_STATUS_NO_ERROR) ? SL_STATUS_OK : SL_STATUS_FAIL;
}

sl_status_t sli_mac_mux_aux_tear_down(void)
{
  if (!sli_mac_mux_aux_bridge_active) {
    sli_zigbee_event_set_inactive(&sli_mac_mux_aux_bring_up_event);
    return SL_STATUS_OK;
  }

  // Leave keeps the stored channel so a later join can bring aux back without another register.
  sli_mac_mux_aux_internal_unregister_protocol();
  return SL_STATUS_OK;
}

sl_status_t sli_mac_mux_aux_inject_rx_to_upper_mac(uint8_t *psdu,
                                                   uint16_t len,
                                                   uint8_t lqi,
                                                   int8_t rssi,
                                                   uint32_t time,
                                                   uint8_t channel)
{
  if (!sli_mac_mux_aux_bridge_active || psdu == NULL || len == 0U) {
    return SL_STATUS_INVALID_STATE;
  }

  sli_mac_mux_aux_dbg_println("[mux-aux] rx len=%d b=%02X %02X %02X",
                              (int)len,
                              psdu[0],
                              (len > 1U) ? psdu[1] : 0U,
                              (len > 2U) ? psdu[2] : 0U);

  sli_buffer_manager_buffer_t rxPacketBuffer =
    sli_legacy_buffer_manager_allocate_async_buffer(len + NUM_APPENDED_INFO_BYTES);
  if (rxPacketBuffer == NULL_BUFFER) {
    return SL_STATUS_NO_MORE_RESOURCE;
  }

  uint8_t *rxPacket = sli_legacy_buffer_manager_get_buffer_pointer(rxPacketBuffer);
  for (uint16_t i = 0; i < len; i++) {
    rxPacket[i] = psdu[i];
  }

  // Prefer the RAIL-reported RX channel. Fall back to the aux MAC's configured
  // channel when RAIL did not provide one (0xFF / out of range).
  // HDR frames arrive on aux_channel + SL_HDR_PHY_CHANNEL_OFFSET (≥ 27): those
  // are intentionally above the 802.15.4 range, so keep them intact so that
  // SLI_MAC_IS_HDR_PHY_CHANNEL() in upper-mac.c can recognise them.
  if (((channel < SL_ZIGBEE_MIN_802_15_4_CHANNEL_NUMBER)
       || (channel > SL_ZIGBEE_MAX_802_15_4_CHANNEL_NUMBER))
#ifdef HIGH_DATARATE_PHY
      && !SLI_MAC_IS_HDR_PHY_CHANNEL(channel)
#endif
      ) {
    channel = sli_mac_upper_mac_state[SL_MAC_AUX_INDEX].nwk_radio_parameters[0].channel;
  }

  uint8_t *appendedInfo = rxPacket + len;
  appendedInfo[APPENDED_INFO_CHANNEL_OVERWRITE_BYTE_INDEX] = channel;
  appendedInfo[APPENDED_INFO_NETWORK_OVERWRITE_BYTE_INDEX] =
    sli_mac_get_current_radio_network_index(SL_MAC_AUX_INDEX);
  appendedInfo[APPENDED_INFO_RSSI_BYTE_INDEX] = (uint8_t)rssi;
  appendedInfo[APPENDED_INFO_LQI_BYTE_INDEX] = lqi;
  appendedInfo[APPENDED_INFO_MAC_TIMER_BYTE_3_INDEX] = (uint8_t)(time >> 24);
  appendedInfo[APPENDED_INFO_MAC_TIMER_BYTE_2_INDEX] = (uint8_t)(time >> 16);
  appendedInfo[APPENDED_INFO_MAC_TIMER_BYTE_1_INDEX] = (uint8_t)(time >> 8);
  appendedInfo[APPENDED_INFO_MAC_TIMER_BYTE_0_INDEX] = (uint8_t)(time);

  (void)sli_mac_receive_callback(SL_MAC_AUX_INDEX, rxPacketBuffer);
  return SL_STATUS_OK;
}

void sli_mac_mux_aux_request_reconcile(void)
{
  sli_zigbee_event_set_active(&sli_mac_mux_aux_reconcile_event);
}

void sli_mac_mux_aux_request_bring_up(void)
{
  if (sli_zigbee_stack_mac_mux_aux_get_channel() == 0U) {
    return;
  }
  if (!sli_mac_mux_aux_bridge_active
      && !sli_zigbee_event_is_scheduled(&sli_mac_mux_aux_bring_up_event)) {
    sli_mac_mux_aux_dbg_println("[mux-aux] bring_up scheduled %ums",
                                SLI_MAC_MUX_AUX_BRING_UP_DELAY_MS);

    sli_zigbee_event_set_delay_ms(&sli_mac_mux_aux_bring_up_event, SLI_MAC_MUX_AUX_BRING_UP_DELAY_MS);
  }
}

// Runs from the stack event loop. Syncs aux identity while active; tears down when primary RX off.
static void sli_mac_mux_aux_reconcile_handler(sli_zigbee_event_t *event)
{
  (void)event;
  sl_mac_radio_parameters_t *primary_params = sli_mac_mux_aux_primary_params(0);

  if (primary_params == NULL || primary_params->rx_state == SL_MAC_RADIO_OFF) {
    (void)sli_mac_mux_aux_tear_down();
    return;
  }

  if (!sli_mac_mux_aux_bridge_active) {
    return;
  }

  sli_mac_mux_aux_dbg_println("[mux-aux] reconcile sync pan=0x%04X node=0x%04X",
                              primary_params->pan_id,
                              primary_params->local_node_id);

  (void)sli_mac_mux_aux_sync_identity(0);
}
