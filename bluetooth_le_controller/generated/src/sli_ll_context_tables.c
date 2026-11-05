/***************************************************************************//**
 * @file
 * @brief Generated per-object context descriptor and init/deinit tables
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
#include "sli_bluetooth_common_init.h"
#include "sli_ll_context_declarations.h"

#ifdef SL_COMPONENT_CATALOG_PRESENT
#include "sl_component_catalog.h"
#endif // SL_COMPONENT_CATALOG_PRESENT
// -----------------------------------------------------------------------------
// Object type `sli_ll_conn`

// NULL-terminated array of `sli_ll_conn` context descriptors.
static sli_btcommon_context_t * const sli_ll_conn_contexts[] = {
#if defined(SL_CATALOG_BLUETOOTH_FEATURE_CONNECTION_PRESENT)
  &sli_ll_conn_context,
#endif
#if defined(SL_CATALOG_BLUETOOTH_FEATURE_CONNECTION_PRESENT)
  &sli_ll_conn_task_schedule_context,
#endif
#if defined(SL_CATALOG_BLUETOOTH_FEATURE_CONNECTION_STATISTICS_PRESENT)
  &sli_ll_conn_stats_context,
#endif
  NULL
};

// NULL-terminated array of `sli_ll_conn` deinit functions, reverse of init order.
static sli_btcommon_object_deinit_func_t * const sli_ll_conn_context_deinit[] = {
#if defined(SL_CATALOG_BLUETOOTH_FEATURE_CONNECTION_STATISTICS_PRESENT)
  sli_ll_conn_stats_object_deinit,
#endif
  NULL
};

// Object type descriptor for `sli_ll_conn`
const sli_btcommon_object_type_t sli_ll_conn_object_type = {
  .contexts = sli_ll_conn_contexts,
  .init = NULL,
  .deinit = sli_ll_conn_context_deinit,
};
