/***************************************************************************//**
 * @file ws_ffn_uc_idle_schedule_local_stubs.c
 * @brief Stubs for local FFN unicast idle scheduling.
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
#include "sl_status.h"

struct fhss_structure;
typedef struct ws_ffn_uc_idle_schedule_config ws_ffn_uc_idle_schedule_config_t;
typedef struct sl_wisun_statistics_ffn_unicast_idle_schedule sl_wisun_statistics_ffn_unicast_idle_schedule_t;

bool ws_ffn_uc_idle_schedule_is_active(void)
{
    return false;
}

void ws_ffn_uc_idle_schedule_on_radio_state_change(bool radio_on)
{
    (void)radio_on;
}

sl_status_t ws_ffn_uc_idle_schedule_get_statistics(sl_wisun_statistics_ffn_unicast_idle_schedule_t *statistics)
{
    (void)statistics;
    return SL_STATUS_NOT_AVAILABLE;
}

void ws_ffn_uc_idle_schedule_reset_statistics(void)
{
    // No tracker exists: resetting is a no-op.
}

uint8_t *ws_ffn_uc_idle_schedule_write_header_ie(uint8_t *ptr)
{
    return ptr;
}

void ws_ffn_uc_idle_schedule_set(const ws_ffn_uc_idle_schedule_config_t *schedule)
{
    (void)schedule;
}

void ws_ffn_uc_idle_schedule_activate(struct fhss_structure *fhss_structure)
{
    (void)fhss_structure;
}

void ws_ffn_uc_idle_schedule_on_fhss_invalidated(struct fhss_structure *fhss)
{
    (void)fhss;
}

void ws_ffn_uc_idle_schedule_disable(void)
{
}

void ws_ffn_uc_idle_schedule_on_join_start(uint8_t uc_channel_function)
{
    (void)uc_channel_function;
}

bool ws_ffn_uc_idle_schedule_should_skip_listening(const struct fhss_structure *fhss)
{
    (void)fhss;
    return false;
}

void ws_ffn_uc_idle_schedule_stamp_local_phase(struct fhss_structure *fhss,
                                                          uint32_t tx_time_us,
                                                          uint8_t *ie_payload_phase_le16)
{
    (void)fhss;
    (void)tx_time_us;
    (void)ie_payload_phase_le16;
}

