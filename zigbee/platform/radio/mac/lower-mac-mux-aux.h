/***************************************************************************//**
 * @file lower-mac-mux-aux.h
 * @brief Lower MAC dispatch for mac_index SL_MAC_AUX_INDEX (mux aux handle).
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

#ifndef LOWER_MAC_MUX_AUX_H
#define LOWER_MAC_MUX_AUX_H

#include "lower-mac.h"
#include "mac-mux-aux.h"

void sli_mac_lower_mac_mux_aux_init(uint8_t mac_index, RadioPowerMode mode);
void sli_mac_lower_mac_mux_aux_send(uint8_t mac_index, uint8_t *packet, sl_mac_tx_options_bitmask_t tx_options);
bool sli_mac_lower_mac_mux_aux_is_idle(uint8_t mac_index);
void sli_mac_lower_mac_mux_aux_set_idle(uint8_t mac_index);
sl_status_t sli_mac_lower_mac_mux_aux_set_radio_channel(uint8_t mac_index, uint8_t channel);
uint8_t sli_mac_lower_mac_mux_aux_get_radio_channel(uint8_t mac_index);
sl_status_t sli_mac_lower_mac_mux_aux_set_radio_power(uint8_t mac_index, int8_t power);
int8_t sli_mac_lower_mac_mux_aux_get_radio_power(uint8_t mac_index);
void sli_mac_lower_mac_mux_aux_set_pan_id(uint8_t mac_index, uint8_t addr_filter_index, sl_802154_pan_id_t panId);
void sli_mac_lower_mac_mux_aux_set_node_id(uint8_t mac_index, uint8_t addr_filter_index, sl_802154_short_addr_t nodeId);
void sli_mac_lower_mac_mux_aux_set_eui64(uint8_t mac_index, uint8_t addr_filter_index, sl_802154_long_addr_t eui64);
sl_status_t sli_mac_lower_mac_mux_aux_set_radio_idle_mode(uint8_t mac_index, RadioPowerMode mode);
bool sli_mac_lower_mac_mux_aux_channel_is_valid(uint8_t mac_index, uint8_t mac_page_chan);
bool sli_mac_lower_mac_mux_aux_incoming_pending(uint8_t mac_index);
void sli_mac_lower_mac_mux_aux_purge_incoming_queue(uint8_t mac_index);
// If packet is an ACK, complete the matching aux TX and return true so the
// caller does not inject it into the upper MAC. Non-ACKs return false.
bool sli_mac_lower_mac_mux_aux_handle_ack(const uint8_t *packet,
                                                uint16_t length);
void sli_mac_lower_mac_mux_aux_handle_tx_events(sl_rail_events_t events,
                                                  uint32_t tx_attempt_id);
// Monotonic id of the current/last aux TX attempt (not the 802.15.4 sequence).
uint32_t sli_mac_lower_mac_mux_aux_get_tx_attempt_id(void);

#endif // LOWER_MAC_MUX_AUX_H
