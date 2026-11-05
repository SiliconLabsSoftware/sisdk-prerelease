/***************************************************************************//**
 * @file sli_wisun_task_ffn_power_saving_stubs.c
 * @brief FFN power saving implementation
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

#include "sl_status.h"
#include "sl_common.h"

#define SL_WISUN_MSG_SET_FFN_POWER_SAVING_SCHEDULE_CNF_ID 0x68

SL_PACK_START(1)
typedef struct {
  uint16_t length;
  uint8_t id;
  uint8_t info;
} SL_ATTRIBUTE_PACKED sl_wisun_msg_header_t;
SL_PACK_END()

SL_PACK_START(1)
typedef struct {
  sl_wisun_msg_header_t header;
  struct {
    uint8_t listen_slots;
    uint8_t sleep_slots;
    uint8_t reserved[2];
  } body;
} SL_ATTRIBUTE_PACKED sl_wisun_msg_set_ffn_power_saving_schedule_req_t;
SL_PACK_END()

SL_PACK_START(1)
typedef struct {
  sl_wisun_msg_header_t header;
  struct {
    uint32_t status;
  } body;
} SL_ATTRIBUTE_PACKED sl_wisun_msg_set_ffn_power_saving_schedule_cnf_t;
SL_PACK_END()

void sli_wisun_task_req_set_ffn_power_saving_schedule(const sl_wisun_msg_set_ffn_power_saving_schedule_req_t *req,
                                                    sl_wisun_msg_set_ffn_power_saving_schedule_cnf_t *cnf)
{
  (void) req;
  cnf->header.length = sizeof(*cnf);
  cnf->header.id = SL_WISUN_MSG_SET_FFN_POWER_SAVING_SCHEDULE_CNF_ID;
  cnf->header.info = 0;
  cnf->body.status = SL_STATUS_NOT_AVAILABLE;
}

void sli_wisun_ffn_power_saving_state_indicate(uint8_t activation_state)
{
  (void) activation_state;
}
