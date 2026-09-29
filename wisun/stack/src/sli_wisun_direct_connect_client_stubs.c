/***************************************************************************//**
 * @file sli_wisun_direct_connect_client_stubs.c
 * @brief Wi-SUN Direct Connect Client API stubs
 *******************************************************************************
 * # License
 * <b>Copyright 2025 Silicon Laboratories Inc. www.silabs.com</b>
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

#include "sl_status.h"
#include "sl_wisun_types.h"
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

// Generic functions

typedef struct sli_dc_client_ctx_t {
  // empty stub
  int dummy;
} sli_dc_client_ctx_t;

typedef struct mcps_data_ind_s mcps_data_ind_t;
typedef struct mcps_data_ie_list mcps_data_ie_list_t;

sli_dc_client_ctx_t *sli_wisun_dc_client_get_ctx(void)
{
  return NULL;
}

bool sli_wisun_dc_client_is_enabled(void)
{
  return false;
}

bool sli_wisun_dc_client_is_server_frame(const uint8_t *eui64)
{
  (void)eui64;
  return false;
}

bool sli_wisun_dc_client_is_server_auth_frame(const uint8_t *eui64)
{
  (void)eui64;
  return false;
}

void sli_wisun_dc_client_restart_link_lifetime(void)
{
  // Empty stub
}

void sli_wisun_dc_client_handle_ns_tx_result(bool success)
{
  (void)success;
}

// Requests

sl_status_t sli_wisun_dc_client_start(void)
{
  return SL_STATUS_NOT_SUPPORTED;
}

sl_status_t sli_wisun_dc_client_scan(const sl_wisun_dc_id_t *dc_id, uint8_t max_solicits_count)
{
  (void)dc_id;
  (void)max_solicits_count;
  return SL_STATUS_NOT_SUPPORTED;
}

sl_status_t sli_wisun_dc_client_stop_scan(void)
{
  return SL_STATUS_NOT_SUPPORTED;
}

sl_status_t sli_wisun_connect_to_direct_connect_server(const sl_wisun_mac_address_t *server_mac, uint32_t pmk_id, uint8_t max_solicits_count)
{
  (void)server_mac;
  (void)pmk_id;
  (void)max_solicits_count;
  return SL_STATUS_NOT_SUPPORTED;
}

sl_status_t sli_wisun_dc_client_stop()
{
  return SL_STATUS_NOT_SUPPORTED;
}

// Handlers

void sli_wisun_dc_client_handle_id_received(const uint8_t *server_eui64, const uint8_t *dc_id)
{
  (void)server_eui64;
  (void)dc_id;
}

void sli_wisun_dc_client_handle_dca(const uint8_t *server_eui64)
{
  (void)server_eui64;
}

void sli_wisun_dc_client_handle_auth_success(void)
{
  // Empty stub
}

void sli_wisun_dc_client_handle_connection_lost(void)
{
  // Empty stub
}

void sli_wisun_dc_client_recv_eapol(uint8_t kmp_id, const uint8_t src_eui64[8],
                                    const uint8_t *buf, size_t buf_len)
{
  (void)kmp_id;
  (void)src_eui64;
  (void)buf;
  (void)buf_len;
}

bool sli_wisun_dc_client_llc_indication(const mcps_data_ind_t *data, const mcps_data_ie_list_t *ie_ext)
{
  (void)data;
  (void)ie_ext;
  return false;
}
