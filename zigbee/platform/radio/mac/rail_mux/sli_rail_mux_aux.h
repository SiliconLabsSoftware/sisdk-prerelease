/***************************************************************************//**
 * @file sli_rail_mux_aux.h
 * @brief RAIL multiplexer aux (register): RX FIFO drain API and declarations
 *        shared with the Zigbee stack rail-mux-aux implementation.
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

#ifndef SLI_RAIL_MUX_AUX_H
#define SLI_RAIL_MUX_AUX_H

#include "sl_rail.h"

typedef struct {
  uint8_t *psdu;
  uint32_t packet_time;
  uint16_t psdu_length;
  uint16_t channel;
  uint8_t lqi;
  int8_t rssi_dbm;
} sli_rail_mux_aux_rx_packet_t;

typedef void (*sli_rail_mux_aux_rx_packet_callback_t)(sli_rail_mux_aux_rx_packet_t *packet);

/** Drain RAIL RX for this mux handle; returns count of packets copied+released (SUCCESS). */
uint32_t sli_rail_mux_aux_rx_on_event(sl_rail_handle_t rail_handle);

/**
 * @brief Same drain path as @ref sli_rail_mux_aux_rx_on_event, but invokes @p callback
 *        for each READY_SUCCESS / READY_CRC_ERROR packet (PSDU in scratch buffer; valid only
 *        for the duration of the callback). If @p callback is NULL, only counts drained packets.
 */
uint32_t sli_rail_mux_aux_rx_on_event_with_packet_callback(sl_rail_handle_t rail_handle,
                                                           sli_rail_mux_aux_rx_packet_callback_t callback);

void sli_rail_mux_aux_on_register_success(sl_rail_handle_t rail_handle);
void sli_rail_mux_aux_on_unregister_begin(void);
void sli_rail_mux_aux_on_unregister_success(void);

sl_rail_status_t sli_rail_mux_aux_register_for_mac(sl_802154_pan_id_t pan_id,
                                                   sl_802154_short_addr_t short_addr,
                                                   const sl_802154_long_addr_t eui64,
                                                   uint8_t channel,
                                                   bool promiscuous);

sl_rail_status_t sli_rail_mux_aux_update_mac_identity(sl_802154_pan_id_t pan_id,
                                                      sl_802154_short_addr_t short_addr,
                                                      const sl_802154_long_addr_t eui64);

// Set/clear the PAN-coordinator role on the registered aux mux context.
sl_rail_status_t sli_rail_mux_aux_set_pan_coordinator(bool is_coordinator);

sl_rail_handle_t sli_rail_mux_aux_get_registered_handle(void);

void sli_rail_mux_aux_dbg_step(uint8_t step, uint32_t val);

// Packs FCS slot_base + channels[0..1] + aux context channel into dbg_step 0xC9.
void sli_rail_mux_dbg_fcs(void);

// True if the last mux context has a queued, armed, or in-flight TX.
bool sli_rail_mux_aux_tx_is_active(void);

// COUNT<<24 | active_tx_idx<<16 | aux_idx<<8 | (sched_pend<<5)|(tx_sched<<4)|(fifo<<3)|(ack<<2)|(in_prog<<1)|pending
uint32_t sli_rail_mux_aux_tx_debug_word(void);

#endif // SLI_RAIL_MUX_AUX_H
