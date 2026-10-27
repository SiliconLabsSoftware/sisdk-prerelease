/*****************************************************************************
 * @file lfn_parent_stubs.c
 * @brief Stubs for direct LFN parenting when libwisun_lfn_parent is not linked
 *****************************************************************************
 * Copyright (c) 2026 Silicon Laboratories Inc. (www.silabs.com)
 *
 * The licensor of this software is Silicon Laboratories Inc. Your use of this software
 * is governed by the terms of the Silicon Labs Master Software License Agreement (MSLA)
 * available at www.silabs.com/about-us/legal/master-software-license-agreement.
 * This software is distributed to you in Object Code format and/or Source Code format and
 * is governed by the sections of the MSLA applicable to Object Code, Source Code and
 * Modified Open Source Code. By using this software, you agree to the terms of the MSLA.
 *
 * This software is a modified version of the ARM/Pelion Wi-SUN FAN software stack which is
 * licensed under Apache 2.0 (see below). Modifications to the ARM/Pelion Wi-SUN software stack
 * within this software are subject to the above copyright notice and licensed pursuant to the MSLA.
 *
 * The original ARM/Pelion Wi-SUN FAN software stack is subject to the following copyright notice.
 *
 * Copyright (c) 2014-2018, Pelion and affiliates.
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *******************************************************************************/

#include <stdint.h>
#include <stdbool.h>

struct protocol_interface_info_entry;
struct mcps_data_ind_s;
struct mcps_data_ie_list;
typedef struct mac_neighbor_table_entry mac_neighbor_table_entry_t;
typedef struct ws_lutt_ie ws_lutt_ie_t;
typedef struct ws_lus_ie ws_lus_ie_t;
typedef struct rpl_instance rpl_instance_t;
typedef struct rpl_dao_target rpl_dao_target_t;
typedef struct buffer buffer_t;

uint8_t ws_config_lfn_limit_get(void)
{
    return 0;
}

void lfn_parent_limit_set(struct protocol_interface_info_entry *interface, uint8_t lfn_limit)
{
    (void)interface;
    (void)lfn_limit;
}

void lfn_parent_init(struct protocol_interface_info_entry *interface)
{
    (void)interface;
}

void lfn_parent_session_setup(struct protocol_interface_info_entry *interface)
{
    (void)interface;
}

void lfn_parent_clean_lfn_neighbors(struct protocol_interface_info_entry *interface, uint8_t lfn_limit)
{
    (void)interface;
    (void)lfn_limit;
}

bool lfn_parent_check_lfn_timing(struct protocol_interface_info_entry *cur, mac_neighbor_table_entry_t *neighbor)
{
    (void)cur;
    (void)neighbor;
    return true;
}

bool lfn_parent_set_lfn_timings(struct protocol_interface_info_entry *interface, mac_neighbor_table_entry_t *neighbor, uint32_t listening_interval_ms, uint32_t listening_offset_ms)
{
    (void)interface;
    (void)neighbor;
    (void)listening_interval_ms;
    (void)listening_offset_ms;
    return false;
}

void lfn_parent_lpas_analyse(struct protocol_interface_info_entry *cur,
                             const struct mcps_data_ind_s *data,
                             const struct mcps_data_ie_list *ie_ext,
                             ws_lutt_ie_t *ws_lutt,
                             ws_lus_ie_t *ws_lus)
{
    (void)cur;
    (void)data;
    (void)ie_ext;
    (void)ws_lutt;
    (void)ws_lus;
}

void lfn_parent_lpcs_analyse(struct protocol_interface_info_entry *cur,
                             const struct mcps_data_ind_s *data,
                             const struct mcps_data_ie_list *ie_ext,
                             ws_lutt_ie_t *ws_lutt)
{
    (void)cur;
    (void)data;
    (void)ie_ext;
    (void)ws_lutt;
}

void lfn_parent_prepare_broadcast_lpc(struct protocol_interface_info_entry *cur)
{
    (void)cur;
}

void lfn_parent_cannot_route(struct protocol_interface_info_entry *cur)
{
    (void)cur;
}

bool lfn_parent_add_lfn_in_mc_list(struct protocol_interface_info_entry *cur,
                                   const uint8_t ipv6[16],
                                   uint8_t mac64[8],
                                   uint32_t lifetime_s)
{
    (void)cur;
    (void)ipv6;
    (void)mac64;
    (void)lifetime_s;
    return true;
}

bool lfn_parent_remove_lfn_from_mc_list(struct protocol_interface_info_entry *cur,
    const uint8_t ipv6[16],
    uint8_t mac64[8])
{
    (void)cur;
    (void)ipv6;
    (void)mac64;
    return true;
}

void lfn_parent_consider_forwarding_lfn_multicast_packet(buffer_t *buf, bool for_us, bool from_us)
{
    (void)buf;
    (void)for_us;
    (void)from_us;
}

bool ws_rpl_proxy_handle_ns_earo(struct protocol_interface_info_entry *cur, const uint8_t *target_addr, uint8_t tid, uint16_t lifetime, uint8_t eui64[8])
{
    (void)cur;
    (void)target_addr;
    (void)tid;
    (void)lifetime;
    (void)eui64;
    return false;
}

void ws_rpl_proxy_handle_dao_ack(rpl_instance_t *rpl_instance, rpl_dao_target_t *target, uint8_t status, bool r_flag)
{
    (void)rpl_instance;
    (void)target;
    (void)status;
    (void)r_flag;
}
