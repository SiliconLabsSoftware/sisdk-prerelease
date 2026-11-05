/***************************************************************************//**
 * @file
 * @brief Declarations for generated per-object context tables
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

#ifndef SLI_LL_CONTEXT_DECLARATIONS_H
#define SLI_LL_CONTEXT_DECLARATIONS_H

#include "sli_bluetooth_common_init.h"

#ifdef __cplusplus
extern "C" {
#endif


// -----------------------------------------------------------------------------
// Object type `sli_ll_conn`

extern sli_btcommon_context_t sli_ll_conn_context;
extern sli_btcommon_context_t sli_ll_conn_task_schedule_context;
extern sli_btcommon_context_t sli_ll_conn_stats_context;
sli_btcommon_object_deinit_func_t sli_ll_conn_stats_object_deinit;
extern const sli_btcommon_object_type_t sli_ll_conn_object_type;

#ifdef __cplusplus
}
#endif

#endif // SLI_LL_CONTEXT_DECLARATIONS_H
