/***************************************************************************/ /**
* @file sl_log_weak.c
* @brief weak implementation of the Silicon Labs Debug Logger functions 
* @version 1.0.0
*******************************************************************************
* # License
* <b>Copyright 2025 Silicon Laboratories Inc. www.silabs.com</b>
*******************************************************************************
*
* SPDX-License-Identifier: Zlib
*
* The licensor of this software is Silicon Laboratories Inc.
*
* This software is provided 'as-is', without any express or implied
* warranty. In no event will the authors be held liable for any damages
* arising from the use of this software.
*
* Permission is granted to anyone to use this software for any purpose,
* including commercial applications, and to alter it and redistribute it
* freely, subject to the following restrictions:
*
* 1. The origin of this software must not be misrepresented; you must not
*    claim that you wrote the original software. If you use this software
*    in a product, an acknowledgment in the product documentation would be
*    appreciated but is not required.
* 2. Altered source versions must be plainly marked as such, and must not be
*    misrepresented as being the original software.
* 3. This notice may not be removed or altered from any source distribution.
*
******************************************************************************/


#include "sl_log.h"
#include "sl_common.h"


/********************************************* 
 * Weak Symbol Implementations
 *********************************************/

SL_WEAK void sl_log_init_stage1(void) {
}

SL_WEAK sl_status_t sl_log_init_stage2(void) {
  return SL_STATUS_OK;
}

SL_WEAK void sl_log_send_no_args(uint32_t event_id, uint8_t flags) {
(void) event_id;
(void) flags;
}

 SL_WEAK void sl_log_send_arg1(uint32_t event_id, uint8_t flags, uint32_t arg1) {
  (void)event_id;
  (void)flags;
  (void)arg1;
}

SL_WEAK void sl_log_send_arg2(uint32_t event_id, uint8_t flags, uint32_t arg1,
                      uint32_t arg2) {
  (void)event_id;
  (void)flags;
  (void)arg1;
  (void)arg2;
}


SL_WEAK void sl_log_send_arg3(uint32_t event_id, uint8_t flags, uint32_t arg1,
                      uint32_t arg2, uint32_t arg3) {
  (void)event_id;
  (void)flags;
  (void)arg1;
  (void)arg2;
  (void)arg3;
}

SL_WEAK sl_status_t sl_log_flush(void) {
  return SL_STATUS_OK; // Data successfully sent
}

SL_WEAK sl_status_t sl_log_set_loglevel(sl_log_level_t level) {
    (void)level;
  return SL_STATUS_OK;
}
SL_WEAK sl_log_level_t sl_log_get_loglevel(void) { return SL_LOG_ENUM_CONFIG_NONE; }

SL_WEAK int sl_log_get_timestamp_delta(void) { return 0; }

SL_WEAK sl_status_t sl_log_write_to_ring_buffer(sl_log_event_t *event_buffer,
                                        uint32_t event_size) {
  (void)event_size;
  (void)event_buffer;
  return SL_STATUS_OK;
}

SL_WEAK sl_status_t sl_log_platform_core_init(void) {
  return SL_STATUS_OK;
}

SL_WEAK uint32_t sl_log_get_timestamp_count(uint8_t core_id) {
    (void)core_id;
   return 0U;
}

SL_WEAK sl_status_t sl_log_platform_core_deinit(void) {
 return SL_STATUS_OK;
}

SL_WEAK sl_status_t sl_log_backend_init(void) {
  return SL_STATUS_OK;
}

SL_WEAK sl_status_t sl_log_pre_sleep_process(void * args) {
    (void) args;
  return SL_STATUS_OK;
}

SL_WEAK sl_status_t sl_log_post_sleep_process(void * args) {
    (void)args;
  return SL_STATUS_OK;
}

SL_WEAK sl_status_t sl_log_set_configurations(void *args, uint8_t core_id) {
    (void)args;
    (void)core_id;
   return SL_STATUS_OK;
}

SL_WEAK sl_status_t sl_log_get_configurations(void *args, uint8_t core_id) {
    (void)args;
    (void)core_id;
   return SL_STATUS_OK;
}

SL_WEAK sl_status_t sl_log_backend_write(sl_log_event_t *buffer, uint32_t read_index,
                                 uint32_t event_count) {
   (void)buffer;
   (void)read_index;
   (void)event_count;                                 
  return SL_STATUS_OK;
}

SL_WEAK uint32_t sl_log_get_timestamp_timer_frequency(uint8_t core_id){
    (void)core_id;
   return 0U;
}

SL_WEAK sl_status_t sl_log_sync_timestamp(uint8_t core_id, void *args) {
    (void)core_id;
    (void)args;
   return SL_STATUS_OK;
}

SL_WEAK sl_log_ring_buffer_t *sl_log_get_ring_buffer_config(void){
  return (void*)0;
}

SL_WEAK void sli_log_assert_implementation(const char* string_value){
  (void)string_value;
}
