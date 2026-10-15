/*****************************************************************************
 * @file mac_lfn_parent_stubs.c
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

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

typedef struct protocol_interface_rf_mac_setup protocol_interface_rf_mac_setup_s;
typedef struct mac_mcps_lfn_queue mac_mcps_lfn_queue_t;
typedef struct buffer_queue buffer_queue_t;
typedef struct mac_pre_build_frame mac_pre_build_frame_t;
typedef struct fhss_ws_neighbor_timing_info fhss_ws_neighbor_timing_info_t;

int8_t mac_lfn_parent_init(protocol_interface_rf_mac_setup_s *rf_mac_setup)
{
    (void)rf_mac_setup;
    return 0;
}

void mac_lfn_parent_deinit(protocol_interface_rf_mac_setup_s *rf_mac_setup)
{
    (void)rf_mac_setup;
}

buffer_queue_t *mac_mcps_create_lfn_queue(protocol_interface_rf_mac_setup_s *rf_mac_setup,
                                          const uint8_t mac_address[8])
{
    (void)rf_mac_setup;
    (void)mac_address;
    return NULL;
}

buffer_queue_t *mac_mcps_find_lfn_queue(protocol_interface_rf_mac_setup_s *rf_mac_setup,
                                        uint8_t addr_mode,
                                        const uint8_t mac_address[8])
{
    (void)rf_mac_setup;
    (void)addr_mode;
    (void)mac_address;
    return NULL;
}

int8_t mac_mcps_cleanup_lfn_queue(protocol_interface_rf_mac_setup_s *rf_mac_setup, mac_mcps_lfn_queue_t *lfn_queue)
{
    (void)rf_mac_setup;
    (void)lfn_queue;
    return -1;
}

int8_t mac_lfn_parent_update_lfn_info(protocol_interface_rf_mac_setup_s *rf_mac_setup,
                                      uint8_t handle,
                                      const fhss_ws_neighbor_timing_info_t *timing_info)
{
    (void)rf_mac_setup;
    (void)handle;
    (void)timing_info;
    return -1;
}

bool mcps_sap_check_buffer_timeout_lfn(protocol_interface_rf_mac_setup_s *mac_setup, const mac_pre_build_frame_t *buffer)
{
    (void)mac_setup;
    (void)buffer;
    return false;
}

void mcps_calculate_next_lfn_trig(protocol_interface_rf_mac_setup_s *rf_ptr, mac_mcps_lfn_queue_t *lfn_queue)
{
    (void)rf_ptr;
    (void)lfn_queue;
}

void mcps_evaluate_lfn_triggers(protocol_interface_rf_mac_setup_s *rf_mac_setup)
{
    (void)rf_mac_setup;
}

bool mac_lfn_parent_can_transmit(protocol_interface_rf_mac_setup_s *rf_mac_setup, const mac_pre_build_frame_t *buffer)
{
    (void)rf_mac_setup;
    (void)buffer;
    return true;
}

void mac_lfn_parent_adjust_tx_time(protocol_interface_rf_mac_setup_s *rf_ptr, mac_pre_build_frame_t *buffer)
{
    (void)rf_ptr;
    (void)buffer;
}

bool mac_lfn_parent_purge_from_lfn_queues(protocol_interface_rf_mac_setup_s *rf_mac_setup, uint8_t msduhandle)
{
    (void)rf_mac_setup;
    (void)msduhandle;
    return false;
}

void mac_lfn_parent_lfn_queues_free(protocol_interface_rf_mac_setup_s *rf_mac_setup)
{
    (void)rf_mac_setup;
}
