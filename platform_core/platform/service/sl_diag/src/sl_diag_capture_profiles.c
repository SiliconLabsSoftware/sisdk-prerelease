/***************************************************************************//**
 * @file
 * @brief SL Diagnostic capture profiles
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

#include "sl_common.h"
#include "sli_diag.h"
#include "sli_diag_internal.h"

#if SL_DIAG_ENABLE

/*******************************************************************************
 ***************************  LOCAL VARIABLES   ********************************
 ******************************************************************************/

// Provider IDs in execution order. The recent-event tail is a core TLV, not a
// provider. Only providers in SLI_DIAG_LINKED_PROVIDER_LIST can register; a
// listed provider that is not registered is skipped with no callback and no
// TLV. Crash profiles without extended providers keep the CPU Crash-only
// recipe. Each must be added to SLI_DIAG_LINKED_PROVIDER_LIST,
// only in the build profiles that link it. This grows the slot and changes
// the retained layout signature. The System provider max_latency_us must leave
// enough of SLI_DIAG_CAPTURE_TIME_BUDGET_WDT_US for the CPU Crash max_latency_us.

#if SL_DIAG_EXTENDED_PROVIDERS_ENABLE
static const uint16_t hard_fault_providers[] = {
  SL_DIAG_PROVIDER_SYSTEM,
  SL_DIAG_PROVIDER_CPU_CRASH,
  SL_DIAG_PROVIDER_HEAP
};

static const uint16_t assert_providers[] = {
  SL_DIAG_PROVIDER_SYSTEM,
  SL_DIAG_PROVIDER_CPU_CRASH
};

static const uint16_t watchdog_warning_providers[] = {
  SL_DIAG_PROVIDER_SYSTEM,
  SL_DIAG_PROVIDER_CPU_CRASH
};
#else
// Crash profiles link only the CPU Crash provider.
static const uint16_t hard_fault_providers[] = {
  SL_DIAG_PROVIDER_CPU_CRASH
};

static const uint16_t assert_providers[] = {
  SL_DIAG_PROVIDER_CPU_CRASH
};

static const uint16_t watchdog_warning_providers[] = {
  SL_DIAG_PROVIDER_CPU_CRASH
};
#endif // SL_DIAG_EXTENDED_PROVIDERS_ENABLE

#if SL_DIAG_NON_CRASH_TRIGGERS_ENABLE
static const uint16_t ipc_timeout_providers[] = {
  SL_DIAG_PROVIDER_SYSTEM,
  SL_DIAG_PROVIDER_HOST_IF,
  SL_DIAG_PROVIDER_NWP_HEALTH,
  SL_DIAG_PROVIDER_HEAP
};

static const uint16_t nwp_heartbeat_loss_providers[] = {
  SL_DIAG_PROVIDER_SYSTEM,
  SL_DIAG_PROVIDER_HOST_IF,
  SL_DIAG_PROVIDER_NWP_HEALTH
};

static const uint16_t tls_timeout_providers[] = {
  SL_DIAG_PROVIDER_SYSTEM,
  SL_DIAG_PROVIDER_NWP_HEALTH,
  SL_DIAG_PROVIDER_HEAP
};

// Manual lists every defined non-crash provider. Those above the privacy
// policy are not linked, and the privacy gate reports any that register as
// REDACTED. The application configures the depth through the request, up to
// the profile maximum.
static const uint16_t manual_providers[] = {
  SL_DIAG_PROVIDER_SYSTEM,
  SL_DIAG_PROVIDER_HEAP,
  SL_DIAG_PROVIDER_HOST_IF,
  SL_DIAG_PROVIDER_NWP_HEALTH
};
#endif // SL_DIAG_NON_CRASH_TRIGGERS_ENABLE

// One profile per trigger listed in the HLD capture profile table. Other
// triggers have no profile, so sli_diag_lookup_profile() returns NULL.
static const sli_diag_capture_profile_t capture_profiles[] = {
  {
    .trigger = SL_DIAG_TRIGGER_HARD_FAULT,
    .max_depth = SLI_DIAG_CAPTURE_DEPTH_NORMAL,
    .time_budget_us = SL_DIAG_CAPTURE_TIME_BUDGET_US,
    .provider_count = (uint16_t)SL_ARRAY_SIZE(hard_fault_providers),
    .providers = hard_fault_providers
  },
  {
    .trigger = SL_DIAG_TRIGGER_ASSERT,
    .max_depth = SLI_DIAG_CAPTURE_DEPTH_NORMAL,
    .time_budget_us = SL_DIAG_CAPTURE_TIME_BUDGET_US,
    .provider_count = (uint16_t)SL_ARRAY_SIZE(assert_providers),
    .providers = assert_providers
  },
  {
    .trigger = SL_DIAG_TRIGGER_WATCHDOG_WARNING,
    .max_depth = SLI_DIAG_CAPTURE_DEPTH_MINIMAL,
    .time_budget_us = SLI_DIAG_CAPTURE_TIME_BUDGET_WDT_US,
    .provider_count = (uint16_t)SL_ARRAY_SIZE(watchdog_warning_providers),
    .providers = watchdog_warning_providers
  },
#if SL_DIAG_NON_CRASH_TRIGGERS_ENABLE
  {
    .trigger = SL_DIAG_TRIGGER_IPC_TIMEOUT,
    .max_depth = SLI_DIAG_CAPTURE_DEPTH_NORMAL,
    .time_budget_us = SL_DIAG_CAPTURE_TIME_BUDGET_US,
    .provider_count = (uint16_t)SL_ARRAY_SIZE(ipc_timeout_providers),
    .providers = ipc_timeout_providers
  },
  {
    .trigger = SL_DIAG_TRIGGER_NWP_HEARTBEAT_LOSS,
    .max_depth = SLI_DIAG_CAPTURE_DEPTH_NORMAL,
    .time_budget_us = SL_DIAG_CAPTURE_TIME_BUDGET_US,
    .provider_count = (uint16_t)SL_ARRAY_SIZE(nwp_heartbeat_loss_providers),
    .providers = nwp_heartbeat_loss_providers
  },
  {
    .trigger = SL_DIAG_TRIGGER_TLS_TIMEOUT,
    .max_depth = SLI_DIAG_CAPTURE_DEPTH_NORMAL,
    .time_budget_us = SL_DIAG_CAPTURE_TIME_BUDGET_US,
    .provider_count = (uint16_t)SL_ARRAY_SIZE(tls_timeout_providers),
    .providers = tls_timeout_providers
  },
  {
    .trigger = SL_DIAG_TRIGGER_MANUAL,
    .max_depth = SLI_DIAG_CAPTURE_DEPTH_NORMAL,
    .time_budget_us = SL_DIAG_CAPTURE_TIME_BUDGET_US,
    .provider_count = (uint16_t)SL_ARRAY_SIZE(manual_providers),
    .providers = manual_providers
  },
#endif // SL_DIAG_NON_CRASH_TRIGGERS_ENABLE
};

/*******************************************************************************
 **************************   GLOBAL FUNCTIONS   *******************************
 ******************************************************************************/

/***************************************************************************//**
 * Gets the capture profile of a trigger.
 ******************************************************************************/
const sli_diag_capture_profile_t *sli_diag_lookup_profile(sl_diag_trigger_t trigger)
{
  for (uint32_t i = 0u; i < SL_ARRAY_SIZE(capture_profiles); i++) {
    if (capture_profiles[i].trigger == trigger) {
      return &capture_profiles[i];
    }
  }
  return NULL;
}

#endif // SL_DIAG_ENABLE
