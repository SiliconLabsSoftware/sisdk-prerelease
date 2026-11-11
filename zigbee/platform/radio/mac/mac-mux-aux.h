/***************************************************************************//**
 * @file mac-mux-aux.h
 * @brief MAC index bridge to RAIL mux aux context (same-PAN secondary channel).
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

#ifndef MAC_MUX_AUX_H
#define MAC_MUX_AUX_H

#include "sl_status.h"
#include "sl_rail.h"
#include "stack/include/sl_zigbee_types.h"

#include "upper-mac.h"
#include "sl_token_manager_defines.h"

#ifndef SL_MAC_AUX_INDEX
#define SL_MAC_AUX_INDEX  1U
#endif

bool sli_mac_is_mux_aux_mac_index(uint8_t mac_index);

sl_status_t sli_mac_mux_aux_bring_up_from_primary(uint8_t primary_mac_index);
sl_status_t sli_mac_mux_aux_sync_identity(uint8_t primary_mac_index);
sl_status_t sli_mac_mux_aux_tear_down(void);

bool sli_mac_mux_aux_is_active(void);
sl_rail_handle_t sli_mac_mux_aux_get_rail_handle(void);
void sli_mac_mux_aux_set_channel(uint8_t channel);

// Apply the PAN-coordinator role to the aux context so the coordinator responds to
// coordinator-addressed frames on the aux channel too. No-op when the aux is inactive.
sl_status_t sli_mac_mux_aux_set_coordinator(bool is_coordinator);

// Schedule a deferred reconcile of the aux context to the primary's radio parameters. Safe to
// call from the radio-parameter/form path; the actual bring-up/tear-down runs from the event loop.
void sli_mac_mux_aux_request_reconcile(void);

// Schedule deferred aux register/RX after register (and after NETWORK_UP when a
// non-zero aux channel is already stored). No-op while the stored channel is 0.
void sli_mac_mux_aux_request_bring_up(void);

// Mux aux context was released (unregister). Clears MAC activity. An app
// unregister stores channel 0. An internal unregister keeps the stored channel.
void sli_mac_mux_aux_on_unregistered(void);

// Unregister the mux aux context without clearing the stored channel (used when
// bring-up rolls back a partial register, or NETWORK_DOWN tears the radio down).
void sli_mac_mux_aux_internal_unregister_protocol(void);

// True when the Zigbee stack is joined enough to register the aux RAIL context. Used to avoid
// reconciling/bringing up aux during network formation (conflicts with DMP OT/BLE on same radio).
bool sli_mac_mux_aux_should_reconcile(void);

// Always-callable debug hook for prebuilt stack objects. Weak no-op in the
// library; rail_mux_aux provides the strong body. Printing is gated by the
// application config SL_MAC_MUX_AUX_DEBUG (sl_mac_mux_aux_config.h).
void sli_mac_mux_aux_dbg(uint8_t tag, uint32_t val);
void sli_mac_mux_aux_dbg_println(const char *format, ...);
bool sli_mac_mux_aux_params_ready_for_bring_up(const sl_mac_radio_parameters_t *params);

// Remember which upper-MAC index (0=primary, SL_MAC_AUX_INDEX=aux) last heard a short
// address on, so later unicast TX picks the correct radio/channel under dual listen.
void sli_mac_mux_aux_stamp_short_device_mac(sl_802154_short_addr_t id, uint8_t mac_index);

// Deliver a received PSDU to the aux upper MAC (mac_index SL_MAC_AUX_INDEX).
sl_status_t sli_mac_mux_aux_inject_rx_to_upper_mac(uint8_t *psdu,
                                                   uint16_t len,
                                                   uint8_t lqi,
                                                   int8_t rssi,
                                                   uint32_t time,
                                                   uint8_t channel);
#endif // MAC_MUX_AUX_H
