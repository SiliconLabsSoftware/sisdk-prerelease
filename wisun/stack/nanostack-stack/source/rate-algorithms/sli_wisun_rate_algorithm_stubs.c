/***************************************************************************//**
 * @file sli_wisun_rate_algorithm_stubs.c
 * @brief Wi-SUN rate algorithm stubs
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

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#include "sl_status.h"
#include "sl_wisun_types.h"
#include "sl_wisun_msg_api.h"

/* Internal stack headers are not available when building the stubs from an
 * SLC project, so the types used by the stubs are only forward declared. */
typedef struct sl_wisun_rate_algorithm sl_wisun_rate_algorithm_t;
struct mac_neighbor_table_entry;
struct ws_pom_ie;
struct mcps_data_retry_s;
struct mcps_data_conf_s;

/* Stubs of ws_rate_algorithm.c, only called by the stack when a neighbor has
 * a rate algorithm, which never happens without the rate algorithm library. */
uint8_t ws_rate_algorithm_update_rates(struct mac_neighbor_table_entry *neighbor,
                                       struct ws_pom_ie *phy_operating_modes,
                                       uint8_t base_phy_mode_id,
                                       uint16_t frame_length,
                                       struct mcps_data_retry_s *selected_rates)
{
  (void)neighbor;
  (void)phy_operating_modes;
  (void)base_phy_mode_id;
  (void)frame_length;
  (void)selected_rates;
  return 0;
}

void ws_rate_algorithm_update_stats(const struct mcps_data_conf_s *data, struct mac_neighbor_table_entry *neighbor)
{
  (void)data;
  (void)neighbor;
}

sl_wisun_rate_algorithm_t *sl_wisun_rate_algorithm_init(uint8_t algorithm)
{
  (void)algorithm;
  return NULL;
}

void sl_wisun_rate_algorithm_remove(sl_wisun_rate_algorithm_t *rate_algorithm)
{
  (void)rate_algorithm;
}

void sli_wisun_task_req_set_rate_algorithm(const sl_wisun_msg_set_rate_algorithm_req_t *req,
                                           sl_wisun_msg_set_rate_algorithm_cnf_t *cnf)
{
  (void)req;
  cnf->header.length = sizeof(sl_wisun_msg_set_rate_algorithm_cnf_t);
  cnf->header.id = SL_WISUN_MSG_SET_RATE_ALGORITHM_CNF_ID;
  cnf->header.info = 0;
  cnf->body.status = SL_STATUS_NOT_AVAILABLE;
}

void sli_wisun_task_req_get_rate_algorithm_stats(const sl_wisun_msg_get_rate_algorithm_stats_req_t *req,
                                                 sl_wisun_msg_get_rate_algorithm_stats_cnf_t *cnf,
                                                 sl_wisun_rate_t *cnf_data)
{
  (void)req;
  (void)cnf_data;
  cnf->header.length = sizeof(sl_wisun_msg_get_rate_algorithm_stats_cnf_t);
  cnf->header.id = SL_WISUN_MSG_GET_RATE_ALGORITHM_STATS_CNF_ID;
  cnf->header.info = 0;
  cnf->body.status = SL_STATUS_NOT_AVAILABLE;
}
