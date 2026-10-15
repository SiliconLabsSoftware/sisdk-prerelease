/***************************************************************************//**
 * @file
 * @brief GATT Client Manager - GATT operation wrappers
 *******************************************************************************
 * # License
 * <b>Copyright 2026 Silicon Laboratories Inc. www.silabs.com</b>
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

#include "sl_bt_api.h"
#include "app_rta.h"
#include "sl_gatt_client_manager.h"
#include "sli_gatt_client_manager.h"

/// If a public GATT call returned Database Out Of Sync, abort and rediscover.
void sli_gcm_handle_gatt_call_result(connection_entry_t *entry, sl_status_t sc)
{
  if (sc != SL_STATUS_BT_ATT_OUT_OF_SYNC) {
    return;
  }
  if (sli_gcm_discovery_busy(entry)) {
    bool was_profile = sli_gcm_profile_discovery_in_progress(entry);
    uint16_t profile_handle = entry->current_profile;
    uint8_t pair = entry->current_pair;
    discovery_state_t drain_state;

    sli_gcm_abort_run(entry, true);
    if (was_profile && sli_gcm_profile_handle_is_valid(profile_handle)) {
      drain_state = entry->state;
      sli_gcm_emit_error(entry->connection, profile_handle, pair, sc);
      sli_gcm_emit_discovery_complete(entry, sc);
      entry->state = drain_state;
      sli_gcm_clear_profile_results(entry, profile_handle);
    }
  }
  sli_gcm_trigger_rediscovery(entry, sc);
}

sl_status_t sl_gatt_client_manager_read_characteristic_value(
  uint8_t connection,
  uint16_t characteristic)
{
  connection_entry_t *entry;
  sl_status_t sc = app_rta_acquire(sli_gcm_ctx);

  if (sc != SL_STATUS_OK) {
    sli_gcm_on_runtime_error(APP_RTA_ERROR_ACQUIRE_FAILED, sc);
    return sc;
  }

  if (!sli_gcm_is_initialized()) {
    gcm_log_debug("read_characteristic_value: not initialized conn=%u"
                  APP_LOG_NL,
                  connection);
    (void)app_rta_release(sli_gcm_ctx);
    return SL_STATUS_NOT_INITIALIZED;
  }
  entry = sli_gcm_find_connection_entry(connection);
  if (entry == NULL) {
    gcm_log_debug("read_characteristic_value: unknown connection %u" APP_LOG_NL,
                  connection);
    (void)app_rta_release(sli_gcm_ctx);
    return SL_STATUS_NOT_FOUND;
  }
  if (sli_gcm_discovery_busy(entry)) {
    gcm_log_debug("read_characteristic_value: discovery in progress conn=%u"
                  APP_LOG_NL,
                  connection);
    (void)app_rta_release(sli_gcm_ctx);
    return SL_STATUS_IN_PROGRESS;
  }

  sc = sl_bt_gatt_read_characteristic_value(connection, characteristic);
  if (sc != SL_STATUS_OK) {
    gcm_log_debug("read_characteristic_value failed conn=%u char=0x%04X"
                  " status=0x%04lx" APP_LOG_NL,
                  connection,
                  characteristic,
                  (unsigned long)sc);
  }
  sli_gcm_handle_gatt_call_result(entry, sc);
  (void)app_rta_release(sli_gcm_ctx);
  return sc;
}

sl_status_t sl_gatt_client_manager_write_characteristic_value(
  uint8_t connection,
  uint16_t characteristic,
  size_t value_len,
  const uint8_t *value)
{
  connection_entry_t *entry;
  sl_status_t sc = app_rta_acquire(sli_gcm_ctx);

  if (sc != SL_STATUS_OK) {
    sli_gcm_on_runtime_error(APP_RTA_ERROR_ACQUIRE_FAILED, sc);
    return sc;
  }

  if (!sli_gcm_is_initialized()) {
    gcm_log_debug("write_characteristic_value: not initialized conn=%u"
                  APP_LOG_NL,
                  connection);
    (void)app_rta_release(sli_gcm_ctx);
    return SL_STATUS_NOT_INITIALIZED;
  }
  entry = sli_gcm_find_connection_entry(connection);
  if (entry == NULL) {
    gcm_log_debug("write_characteristic_value: unknown connection %u"
                  APP_LOG_NL,
                  connection);
    (void)app_rta_release(sli_gcm_ctx);
    return SL_STATUS_NOT_FOUND;
  }
  if (sli_gcm_discovery_busy(entry)) {
    gcm_log_debug("write_characteristic_value: discovery in progress conn=%u"
                  APP_LOG_NL,
                  connection);
    (void)app_rta_release(sli_gcm_ctx);
    return SL_STATUS_IN_PROGRESS;
  }

  sc = sl_bt_gatt_write_characteristic_value(connection,
                                             characteristic,
                                             value_len,
                                             value);
  if (sc != SL_STATUS_OK) {
    gcm_log_debug("write_characteristic_value failed conn=%u char=0x%04X"
                  " status=0x%04lx" APP_LOG_NL,
                  connection,
                  characteristic,
                  (unsigned long)sc);
  }
  sli_gcm_handle_gatt_call_result(entry, sc);
  (void)app_rta_release(sli_gcm_ctx);
  return sc;
}

sl_status_t sl_gatt_client_manager_write_characteristic_value_without_response(
  uint8_t connection,
  uint16_t characteristic,
  size_t value_len,
  const uint8_t *value)
{
  connection_entry_t *entry;
  sl_status_t sc;
  uint16_t sent_len = 0;

  sc = app_rta_acquire(sli_gcm_ctx);
  if (sc != SL_STATUS_OK) {
    sli_gcm_on_runtime_error(APP_RTA_ERROR_ACQUIRE_FAILED, sc);
    return sc;
  }

  if (!sli_gcm_is_initialized()) {
    gcm_log_debug("write_characteristic_value_without_response: not initialized"
                  " conn=%u" APP_LOG_NL,
                  connection);
    (void)app_rta_release(sli_gcm_ctx);
    return SL_STATUS_NOT_INITIALIZED;
  }
  entry = sli_gcm_find_connection_entry(connection);
  if (entry == NULL) {
    gcm_log_debug("write_characteristic_value_without_response: unknown"
                  " connection %u" APP_LOG_NL,
                  connection);
    (void)app_rta_release(sli_gcm_ctx);
    return SL_STATUS_NOT_FOUND;
  }
  if (sli_gcm_discovery_busy(entry)) {
    gcm_log_debug("write_characteristic_value_without_response: discovery in"
                  " progress conn=%u" APP_LOG_NL,
                  connection);
    (void)app_rta_release(sli_gcm_ctx);
    return SL_STATUS_IN_PROGRESS;
  }

  sc = sl_bt_gatt_write_characteristic_value_without_response(connection,
                                                              characteristic,
                                                              value_len,
                                                              value,
                                                              &sent_len);
  if (sc != SL_STATUS_OK) {
    gcm_log_debug("write_characteristic_value_without_response failed"
                  " conn=%u char=0x%04X status=0x%04lx" APP_LOG_NL,
                  connection,
                  characteristic,
                  (unsigned long)sc);
  }
  sli_gcm_handle_gatt_call_result(entry, sc);
  (void)app_rta_release(sli_gcm_ctx);
  return sc;
}

sl_status_t sl_gatt_client_manager_set_characteristic_notification(
  uint8_t connection,
  uint16_t characteristic,
  uint8_t flags)
{
  connection_entry_t *entry;
  sl_status_t sc = app_rta_acquire(sli_gcm_ctx);

  if (sc != SL_STATUS_OK) {
    sli_gcm_on_runtime_error(APP_RTA_ERROR_ACQUIRE_FAILED, sc);
    return sc;
  }

  if (!sli_gcm_is_initialized()) {
    gcm_log_debug("set_characteristic_notification: not initialized conn=%u"
                  APP_LOG_NL,
                  connection);
    (void)app_rta_release(sli_gcm_ctx);
    return SL_STATUS_NOT_INITIALIZED;
  }
  entry = sli_gcm_find_connection_entry(connection);
  if (entry == NULL) {
    gcm_log_debug("set_characteristic_notification: unknown connection %u"
                  APP_LOG_NL,
                  connection);
    (void)app_rta_release(sli_gcm_ctx);
    return SL_STATUS_NOT_FOUND;
  }
  if (sli_gcm_discovery_busy(entry)) {
    gcm_log_debug("set_characteristic_notification: discovery in progress"
                  " conn=%u" APP_LOG_NL,
                  connection);
    (void)app_rta_release(sli_gcm_ctx);
    return SL_STATUS_IN_PROGRESS;
  }

  sc = sl_bt_gatt_set_characteristic_notification(connection,
                                                  characteristic,
                                                  sli_gcm_subscription_flag_to_gatt(flags));
  if (sc != SL_STATUS_OK) {
    gcm_log_debug("set_characteristic_notification failed conn=%u char=0x%04X"
                  " flags=0x%02X status=0x%04lx" APP_LOG_NL,
                  connection,
                  characteristic,
                  flags,
                  (unsigned long)sc);
  }
  sli_gcm_handle_gatt_call_result(entry, sc);
  (void)app_rta_release(sli_gcm_ctx);
  return sc;
}

sl_status_t sl_gatt_client_manager_send_characteristic_confirmation(
  uint8_t connection)
{
  connection_entry_t *entry;
  sl_status_t sc = app_rta_acquire(sli_gcm_ctx);

  if (sc != SL_STATUS_OK) {
    sli_gcm_on_runtime_error(APP_RTA_ERROR_ACQUIRE_FAILED, sc);
    return sc;
  }

  if (!sli_gcm_is_initialized()) {
    gcm_log_debug("send_characteristic_confirmation: not initialized conn=%u"
                  APP_LOG_NL,
                  connection);
    (void)app_rta_release(sli_gcm_ctx);
    return SL_STATUS_NOT_INITIALIZED;
  }
  entry = sli_gcm_find_connection_entry(connection);
  if (entry == NULL) {
    gcm_log_debug("send_characteristic_confirmation: unknown connection %u"
                  APP_LOG_NL,
                  connection);
    (void)app_rta_release(sli_gcm_ctx);
    return SL_STATUS_NOT_FOUND;
  }

  sc = sl_bt_gatt_send_characteristic_confirmation(connection);
  if (sc != SL_STATUS_OK) {
    gcm_log_debug("send_characteristic_confirmation failed conn=%u"
                  " status=0x%04lx" APP_LOG_NL,
                  connection,
                  (unsigned long)sc);
  }
  sli_gcm_handle_gatt_call_result(entry, sc);
  (void)app_rta_release(sli_gcm_ctx);
  return sc;
}
