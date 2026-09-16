/***************************************************************************//**
 * @file sli_wisun_direct_connect_server_stubs.c
 * @brief Wi-SUN Direct Connect Server API stubs
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
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct protocol_interface_info_entry protocol_interface_info_entry_t;
typedef struct in6_addr in6_addr_t;
typedef struct mcps_data_ind_s mcps_data_ind_t;
typedef struct mcps_data_ie_list mcps_data_ie_list_t;

typedef struct sli_dc_server_ctx_t {
  int dummy;
} sli_dc_server_ctx_t;

sli_dc_server_ctx_t *sli_wisun_dc_server_get_ctx(void)
{
  return NULL;
}

bool sli_wisun_dc_server_is_enabled(void)
{
  return false;
}

void sli_wisun_dc_server_recv_eapol(uint8_t kmp_id, const uint8_t *buf, size_t buf_len,
                                    const uint8_t authenticator_eui64[8])
{
  (void)kmp_id;
  (void)buf;
  (void)buf_len;
  (void)authenticator_eui64;
}

void sli_wisun_dc_server_init(protocol_interface_info_entry_t *cur)
{
  (void)cur;
}

void sli_wisun_dc_server_stop(protocol_interface_info_entry_t *cur)
{
  (void)cur;
}

void sli_wisun_dc_server_dca_confirm(protocol_interface_info_entry_t *cur, uint8_t status, const uint8_t client_eui64[8])
{
  (void)cur;
  (void)status;
  (void)client_eui64;
}

void sli_wisun_dc_server_connection_lost(protocol_interface_info_entry_t *cur)
{
  (void)cur;
}

void sli_wisun_dc_server_restart_link_lifetime(void)
{
}

void sli_wisun_dc_server_confirm(protocol_interface_info_entry_t *cur, sl_status_t status, const uint8_t client_eui64[8])
{
  (void)cur;
  (void)status;
  (void)client_eui64;
}

void sli_wisun_dc_server_handle_auth_success(protocol_interface_info_entry_t *cur,
                                             const uint8_t client_eui64[8])
{
  (void)cur;
  (void)client_eui64;
}

void sli_wisun_dc_server_handle_connection_solicit(protocol_interface_info_entry_t *cur, const uint8_t client_eui64[8])
{
  (void)cur;
  (void)client_eui64;
}

void sli_wisun_dc_server_handle_identity_solicit(const uint8_t client_eui64[8], const uint8_t *dc_id)
{
  (void)client_eui64;
  (void)dc_id;
}

bool sli_wisun_dc_server_is_client_frame(const uint8_t eui64[8], bool tmp)
{
  (void)eui64;
  (void)tmp;
  return false;
}

bool sli_wisun_dc_server_connection_in_progress(void)
{
  return false;
}

void sli_wisun_dc_server_id_advert_confirm(uint8_t status, const uint8_t client_eui64[8])
{
  (void)status;
  (void)client_eui64;
}

sl_status_t sli_wisun_dc_server_accept_link(const in6_addr_t *link_local_ipv6)
{
  (void)link_local_ipv6;
  return SL_STATUS_NOT_SUPPORTED;
}

sl_status_t sli_wisun_dc_server_advertise_server_id(const in6_addr_t *link_local_ipv6, const uint8_t *dc_id)
{
  (void)link_local_ipv6;
  (void)dc_id;
  return SL_STATUS_NOT_SUPPORTED;
}

bool sli_wisun_dc_server_llc_indication(const mcps_data_ind_t *data, const mcps_data_ie_list_t *ie_ext)
{
  (void)data;
  (void)ie_ext;
  return false;
}
