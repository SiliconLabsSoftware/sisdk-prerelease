/***************************************************************************//**
 * @file sli_wisun_task_direct_connect_server_stubs.c
 * @brief Wi-SUN Direct Connect Server task request stubs
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
#include "sl_wisun_msg_api.h"

void sli_wisun_task_req_set_direct_connect_state(const sl_wisun_msg_set_direct_connect_state_req_t *req,
                                                 sl_wisun_msg_set_direct_connect_state_cnf_t *cnf)
{
  (void)req;
  cnf->header.length = sizeof(sl_wisun_msg_set_direct_connect_state_cnf_t);
  cnf->header.id = SL_WISUN_MSG_SET_DIRECT_CONNECT_STATE_CNF_ID;
  cnf->header.info = 0;
  cnf->body.status = SL_STATUS_NOT_AVAILABLE;
}

void sli_wisun_task_req_accept_direct_connect_link(const sl_wisun_msg_accept_direct_connect_link_req_t *req,
                                                   sl_wisun_msg_accept_direct_connect_link_cnf_t *cnf)
{
  (void)req;
  cnf->header.length = sizeof(sl_wisun_msg_accept_direct_connect_link_cnf_t);
  cnf->header.id = SL_WISUN_MSG_ACCEPT_DIRECT_CONNECT_LINK_CNF_ID;
  cnf->header.info = 0;
  cnf->body.status = SL_STATUS_NOT_AVAILABLE;
}

void sli_wisun_task_req_set_direct_connect_pmk_id(const sl_wisun_msg_set_direct_connect_pmk_id_req_t *req,
                                                  sl_wisun_msg_set_direct_connect_pmk_id_cnf_t *cnf)
{
  (void)req;
  cnf->header.length = sizeof(sl_wisun_msg_set_direct_connect_pmk_id_cnf_t);
  cnf->header.id = SL_WISUN_MSG_SET_DIRECT_CONNECT_PMK_ID_CNF_ID;
  cnf->header.info = 0;
  cnf->body.status = SL_STATUS_NOT_AVAILABLE;
}

void sli_wisun_task_req_advert_direct_connect_server_id(const sl_wisun_msg_advert_direct_connect_server_id_req_t *req,
                                                        sl_wisun_msg_advert_direct_connect_server_id_cnf_t *cnf)
{
  (void)req;
  cnf->header.length = sizeof(sl_wisun_msg_advert_direct_connect_server_id_cnf_t);
  cnf->header.id = SL_WISUN_MSG_ADVERT_DIRECT_CONNECT_SERVER_ID_CNF_ID;
  cnf->header.info = 0;
  cnf->body.status = SL_STATUS_NOT_AVAILABLE;
}
