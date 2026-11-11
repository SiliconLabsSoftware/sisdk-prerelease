/***************************************************************************//**
 * @file ws_ffn_uc_idle_schedule_peer_stubs.c
 * @brief Stubs for FFN peer schedule prediction.
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

#include <stdbool.h>
#include <stdint.h>

struct eapol_target_info_s;
struct mac_neighbor_table_entry;
typedef struct ws_ffn_uc_idle_schedule_ie ws_ffn_uc_idle_schedule_ie_t;
typedef struct ws_ffn_uc_idle_schedule_rx_frame ws_ffn_uc_idle_schedule_rx_frame_t;

typedef enum {
    INVALID_IE = 0,
} ws_ffn_uc_idle_schedule_ie_status_t;

bool ws_ffn_uc_idle_schedule_peer_allows_tx(const uint8_t dst_addr[8],
                                                              uint32_t tx_time_us,
                                                              uint32_t sequence_length)
{
    (void)dst_addr;
    (void)tx_time_us;
    (void)sequence_length;
    return true;
}

void ws_ffn_uc_idle_schedule_invalidate_peer_bc_timing(const void *owner)
{
    (void)owner;
}

ws_ffn_uc_idle_schedule_ie_status_t ws_ffn_uc_idle_schedule_read_header_ie(const uint8_t *header_ie,
                                                                       uint16_t header_ie_length,
                                                                       ws_ffn_uc_idle_schedule_ie_t *ffn_uc_idle_schedule_ie)
{
    (void)header_ie;
    (void)header_ie_length;
    (void)ffn_uc_idle_schedule_ie;
    return INVALID_IE;
}

void ws_ffn_uc_idle_schedule_learn_peer(const ws_ffn_uc_idle_schedule_rx_frame_t *rx)
{
    (void)rx;
}

void ws_ffn_uc_idle_schedule_drop_peer_by_address(const uint8_t *src_addr)
{
    (void)src_addr;
}

void ws_ffn_uc_idle_schedule_promote_candidate(struct eapol_target_info_s *parent,
                                                                  struct mac_neighbor_table_entry *neighbor)
{
    (void)parent;
    (void)neighbor;
}

void ws_ffn_uc_idle_schedule_on_neighbor_uc_invalidated(struct mac_neighbor_table_entry *neighbor,
                                                                  uint8_t dwell_interval_ms,
                                                                  uint8_t channel_function)
{
    (void)neighbor;
    (void)dwell_interval_ms;
    (void)channel_function;
}

void ws_ffn_uc_idle_schedule_deinit(void)
{
}

void ws_ffn_uc_idle_schedule_remove_peer(const void *owner)
{
    (void)owner;
}
