/***************************************************************************//**
 * @file sli_wisun_lxpm_stubs.c
 * @brief Wi-SUN LXPM stubs
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

#include <sl_common.h>
#include <sl_wisun_types.h>
#include <sl_wisun_msg_api.h>
#include <sl_status.h>

typedef struct upp_domain upp_domain_t;
typedef struct protocol_interface_info_entry protocol_interface_info_entry_t;
typedef struct if_group_entry if_group_entry_t;
typedef struct mac_neighbor_table_entry mac_neighbor_table_entry_t;
typedef struct buffer buffer_t;
typedef struct sli_wisun_timer_context sli_wisun_timer_context_t;

void sli_wisun_task_req_set_lxpm_prefix(const sl_wisun_msg_set_lxpm_prefix_req_t *req, sl_wisun_msg_set_lxpm_prefix_cnf_t *cnf)
{
  (void)req;
  cnf->header.length = sizeof(sl_wisun_msg_set_lxpm_prefix_cnf_t);
  cnf->header.id = SL_WISUN_MSG_SET_LXPM_PREFIX_CNF_ID;
  cnf->header.info = 0;
  cnf->body.status = SL_STATUS_NOT_AVAILABLE;
}

void sli_wisun_task_req_get_lxpm_group_neighbor_count(const sl_wisun_msg_get_lxpm_group_neighbor_count_req_t *req, sl_wisun_msg_get_lxpm_group_neighbor_count_cnf_t *cnf)
{
  (void)req;
  cnf->header.length = sizeof(sl_wisun_msg_get_lxpm_group_neighbor_count_cnf_t);
  cnf->header.id = SL_WISUN_MSG_GET_LXPM_GROUP_NEIGHBOR_COUNT_CNF_ID;
  cnf->header.info = 0;
  cnf->body.status = SL_STATUS_NOT_AVAILABLE;
  cnf->body.neighbor_count = 0;
}

void sli_wisun_task_req_get_lxpm_group_neighbors(const sl_wisun_msg_get_lxpm_group_neighbors_req_t *req,
                                                 sl_wisun_msg_get_lxpm_group_neighbors_cnf_t *cnf,
                                                 in6_addr_t *cnf_data)
{
  (void)req;
  (void)cnf_data;
  cnf->header.length = sizeof(sl_wisun_msg_get_lxpm_group_neighbors_cnf_t);
  cnf->header.id = SL_WISUN_MSG_GET_LXPM_GROUP_NEIGHBORS_CNF_ID;
  cnf->header.info = 0;
  cnf->body.status = SL_STATUS_NOT_AVAILABLE;
  cnf->body.read_neighbor_count = 0;
}

sl_status_t sli_wisun_lfg_send_state_msg(protocol_interface_info_entry_t *cur)
{
  (void)cur;
  return SL_STATUS_OK;
}

sl_status_t sli_wisun_lfg_add_group(protocol_interface_info_entry_t *cur, if_group_entry_t *if_group)
{
  (void)cur;
  (void)if_group;
  return SL_STATUS_OK;
}

sl_status_t sli_wisun_lfg_remove_group(protocol_interface_info_entry_t *cur, const uint8_t group[16])
{
  (void)cur;
  (void)group;
  return SL_STATUS_OK;
}

void sli_wisun_lfg_remove_neighbor(mac_neighbor_table_entry_t *mac_neighbor)
{
  (void)mac_neighbor;
}

sl_status_t sli_wisun_lfg_start(protocol_interface_info_entry_t *cur)
{
  (void)cur;
  return SL_STATUS_OK;
}

sl_status_t sli_wisun_lfg_stop(protocol_interface_info_entry_t *cur)
{
  (void)cur;
  return SL_STATUS_OK;
}

void upp_init(void)
{
}

upp_domain_t *upp_domain_lookup(const uint8_t address[16])
{
  (void)address;
  return NULL;
}

bool upp_hbh_len_check(const uint8_t *opt_data, uint8_t opt_data_len)
{
  (void)opt_data;
  (void)opt_data_len;
  return false;
}

bool upp_process_hbh(buffer_t *buf, uint8_t *opt_data)
{
  (void)buf;
  (void)opt_data;
  return false;
}

bool upp_forwarder_process_message(buffer_t *buf, const uint8_t *src_eui64)
{
  (void)buf;
  (void)src_eui64;
  return false;
}

void upp_clear_realm_scope_seeds(void)
{
}

void upp_timer_context_setup(sli_wisun_timer_context_t *context)
{
  (void)context;
}