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
#include "sl_component_catalog.h"
#include "sli_bt_core_contexts.h"


#if defined(SL_CATALOG_BLUETOOTH_FEATURE_LOCAL_PRIVACY_PRESENT) \
  && defined(SL_CATALOG_BLUETOOTH_FEATURE_ADVERTISER_PRESENT)
#define SLI_BT_LOCAL_PRIVACY_ADV_TIMERS_PRESENT
#endif

#if defined(SL_CATALOG_BLUETOOTH_FEATURE_LOCAL_PRIVACY_PRESENT) \
  && (defined(SL_CATALOG_BLUETOOTH_FEATURE_SCANNER_PRESENT) \
      || defined(SL_CATALOG_BLUETOOTH_FEATURE_PAWR_ADVERTISER_PRESENT))
#define SLI_BT_LOCAL_PRIVACY_CENTRAL_TIMER_PRESENT
#endif

#ifdef SL_COMPONENT_CATALOG_PRESENT
#include "sl_component_catalog.h"
#endif // SL_COMPONENT_CATALOG_PRESENT
// -----------------------------------------------------------------------------
// Object type `sli_bt_connection`

// NULL-terminated array of `sli_bt_connection` context descriptors.
static sli_btcommon_context_t * const sli_bt_connection_contexts[] = {
#if defined(SL_CATALOG_BLUETOOTH_FEATURE_CONNECTION_PRESENT)
  &sli_bt_connection_context,
#endif
  NULL
};

// Object type descriptor for `sli_bt_connection`
const sli_btcommon_object_type_t sli_bt_connection_object_type = {
  .contexts = sli_bt_connection_contexts,
  .init = NULL,
  .deinit = NULL,
};
