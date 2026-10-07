/*****************************************************************************
 * @file fhss_ws_lfn_parent.c
 * @brief Wi-SUN FHSS LFN parent related functions
 *****************************************************************************
 * Copyright (c) 2022 Silicon Laboratories Inc. (www.silabs.com)
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

#include <stdbool.h>
#include <stdint.h>

typedef struct fhss_structure fhss_structure_t;
typedef struct fhss_ws_neighbor_timing_info fhss_ws_neighbor_timing_info_t;
typedef struct mac_pre_build_frame mac_pre_build_frame_t;
struct ws_ie_t;

int fhss_ws_lfn_parent_init(fhss_structure_t *fhss)
{
    (void)fhss;
    return 0;
}

int fhss_ws_start_ffn_to_lfn_broadcast_schedule(fhss_structure_t *fhss)
{
    (void)fhss;
    return 0;
}

void fhss_ws_stop_ffn_to_lfn_broadcast_schedule(fhss_structure_t *fhss)
{
    (void)fhss;
}

uint16_t fhss_ws_calculate_lpa_channel(fhss_structure_t *fhss,
                                       fhss_ws_neighbor_timing_info_t *neighbor_timing_info,
                                       uint64_t timestamp_us,
                                       uint8_t mac_address[8])
{
    (void)fhss;
    (void)neighbor_timing_info;
    (void)timestamp_us;
    (void)mac_address;
    return 0;
}

uint16_t fhss_ws_calculate_lfn_channel(fhss_structure_t *fhss,
                                       fhss_ws_neighbor_timing_info_t *neighbor_timing_info,
                                       uint64_t timestamp_us,
                                       uint8_t mac_address[8],
                                       uint32_t *timeout_ms)
{
    (void)fhss;
    (void)neighbor_timing_info;
    (void)timestamp_us;
    (void)mac_address;
    (void)timeout_ms;
    return 0;
}

void fhss_ws_lfn_parent_write_ies(fhss_structure_t *fhss, uint8_t *ptr, uint16_t length, mac_pre_build_frame_t *buffer, uint32_t tx_time_us, struct ws_ie_t *header_ie)
{
    (void)fhss;
    (void)ptr;
    (void)length;
    (void)buffer;
    (void)tx_time_us;
    (void)header_ie;
}

uint32_t fhss_ws_get_lfn_bc_interval_ms(fhss_structure_t *fhss)
{
    (void)fhss;
    return 0;
}

uint64_t fhss_ws_time_to_lfn_uc(fhss_ws_neighbor_timing_info_t *neighbor_timing_info,
                                uint64_t timestamp_us)
{
    (void)neighbor_timing_info;
    (void)timestamp_us;
    return 0;
}

bool fhss_ws_is_lfn_bc_overlap(const fhss_structure_t *fhss,
                               uint64_t tx_start_us,
                               uint64_t tx_duration_us)
{
    (void)fhss;
    (void)tx_start_us;
    (void)tx_duration_us;
    return false;
}
