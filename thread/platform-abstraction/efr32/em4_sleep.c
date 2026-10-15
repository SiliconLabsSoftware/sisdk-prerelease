/*
 *  Copyright (c) 2026, The OpenThread Authors.
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are met:
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *  3. Neither the name of the copyright holder nor the
 *     names of its contributors may be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 *  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * @file
 *   This file implements the OpenThread platform abstraction for EM4 sleep
 * management.
 *
 *   Requires Series 2 with BURTC and EM4GRPACLK sourced from ULFRCO.
 *   Only a single OpenThread instance is supported.
 *
 *   EM4 entry only occurs on the next EM2 transition after arming. If Power
 *   Manager never enters EM2, EM4 will never be entered.
 */

#define CURRENT_MODULE_NAME "OPENTHREAD"

#include "em4_sleep.h"

#include <openthread-core-config.h>
#include <openthread/instance.h>
#include <openthread/tasklet.h>
#include <openthread/thread.h>
#include <openthread/platform/radio.h>
#include <openthread/platform/toolchain.h>

#ifdef SL_COMPONENT_CATALOG_PRESENT
#include "sl_component_catalog.h"
#endif

#if defined(SL_CATALOG_POWER_MANAGER_PRESENT)
#include "sl_power_manager.h"
#endif

#include "em_device.h"
#include "micro.h"

#if OPENTHREAD_CONFIG_MULTIPLE_INSTANCE_ENABLE
#error "ot_em4_sleep does not support multi-instance builds."
#endif

#ifndef BURTC_PRESENT

otError sl_ot_em4_init(void)
{
    return OT_ERROR_NOT_CAPABLE;
}

bool sl_ot_em4_is_wake_from_em4(void)
{
    return false;
}

otError sl_ot_em4_request(otInstance *aInstance, uint32_t aWakeMs)
{
    OT_UNUSED_VARIABLE(aInstance);
    OT_UNUSED_VARIABLE(aWakeMs);
    return OT_ERROR_NOT_CAPABLE;
}

void sl_ot_em4_cancel(void)
{
}

bool sl_ot_em4_is_pending(void)
{
    return false;
}

uint32_t sl_ot_em4_get_default_wake_ms(otInstance *aInstance)
{
    OT_UNUSED_VARIABLE(aInstance);
    return 0;
}

#else /* BURTC_PRESENT */

#include "sl_clock_manager.h"
#include "sl_clock_manager_tree_config.h"
#include "sl_device_peripheral.h"
#include "sl_hal_burtc.h"

#ifndef SL_OT_EM4_ULFRCO_FMIN_HZ
#define SL_OT_EM4_ULFRCO_FMIN_HZ 944U
#endif

#ifndef SL_OT_EM4_ULFRCO_FTYP_HZ
#define SL_OT_EM4_ULFRCO_FTYP_HZ 1000U
#endif

#if (SL_OT_EM4_ULFRCO_FMIN_HZ == 0U) || (SL_OT_EM4_ULFRCO_FMIN_HZ >= SL_OT_EM4_ULFRCO_FTYP_HZ)
#error "SL_OT_EM4_ULFRCO_FMIN_HZ must be non-zero and less than SL_OT_EM4_ULFRCO_FTYP_HZ"
#endif

#if defined(SL_CLOCK_MANAGER_EM4GRPACLK_SOURCE) \
    && (SL_CLOCK_MANAGER_EM4GRPACLK_SOURCE != CMU_EM4GRPACLKCTRL_CLKSEL_ULFRCO)
#error "ot_em4_sleep requires SL_CLOCK_MANAGER_EM4GRPACLK_SOURCE = CMU_EM4GRPACLKCTRL_CLKSEL_ULFRCO"
#endif

//------------------------------------------------------------------------------
// Forward declarations

static void     burtcClockEnable(void);
static uint32_t burtcCounterHz(uint32_t aClkDiv);
static uint32_t burtcCompareTicks(uint32_t aWakeMs, uint32_t aClkDiv);
static otError  configureBurtcWake(uint32_t aWakeMs);
static bool     radioIsIdle(otInstance *aInstance);

#if defined(SL_CATALOG_POWER_MANAGER_PRESENT)
static void tryEnterEm4(void);
static void em4OnEmTransition(sl_power_manager_em_t aFrom, sl_power_manager_em_t aTo);
#endif

//------------------------------------------------------------------------------
// Static variables

static bool          sInitialized     = false;
static bool          sWokeFromEm4     = false;
static volatile bool sPending         = false;
static volatile bool sEnterStarted    = false;
static bool          sBurtcConfigured = false;
static uint32_t      sWakeMs          = 0;

#if defined(SL_CATALOG_POWER_MANAGER_PRESENT)
static sl_power_manager_em_transition_event_handle_t sEmTransitionHandle;

static const sl_power_manager_em_transition_event_info_t sEmTransitionInfo = {
    .event_mask = SL_POWER_MANAGER_EVENT_TRANSITION_ENTERING_EM2,
    .on_event   = em4OnEmTransition,
};
#endif

//------------------------------------------------------------------------------
// Helpers

static void burtcClockEnable(void)
{
    sl_clock_manager_enable_bus_clock(SL_BUS_CLOCK_BURTC);
}

static uint32_t burtcCounterHz(uint32_t aClkDiv)
{
    uint32_t          burtcFrequency   = 0U;
    sl_clock_branch_t burtcClockBranch = sl_device_peripheral_get_clock_branch(SL_PERIPHERAL_BURTC);
    sl_status_t       status           = sl_clock_manager_get_clock_branch_frequency(burtcClockBranch, &burtcFrequency);

    if ((status != SL_STATUS_OK) || (burtcFrequency == 0U) || (aClkDiv == 0U))
    {
        return 0;
    }

    return burtcFrequency / aClkDiv;
}

static uint32_t burtcCompareTicks(uint32_t aWakeMs, uint32_t aClkDiv)
{
    const uint32_t freqHz = burtcCounterHz(aClkDiv);
    uint64_t       ticks;

    if (freqHz == 0U)
    {
        return 0;
    }

    ticks = ((uint64_t)freqHz * (uint64_t)aWakeMs) / 1000ULL;

    if (ticks == 0ULL)
    {
        ticks = 1ULL;
    }
    else if (ticks > 0xFFFFFFFFULL)
    {
        ticks = 0xFFFFFFFFULL;
    }

    return (uint32_t)ticks;
}

static otError configureBurtcWake(uint32_t aWakeMs)
{
    sl_hal_burtc_init_t burtcInit = SL_HAL_BURTC_INIT_DEFAULT;
    const uint32_t      clkDiv    = 1U;
    uint32_t            compare;

    sWakeMs = aWakeMs;
    compare = burtcCompareTicks(sWakeMs, clkDiv);

    if (compare == 0U)
    {
        return OT_ERROR_FAILED;
    }

    burtcClockEnable();

    burtcInit.clock_divider  = clkDiv;
    burtcInit.compare0_top   = true;
    burtcInit.em4_comparator = true;
    burtcInit.em4_overflow   = false;
    burtcInit.debug_run      = false;

    sl_hal_burtc_init(&burtcInit);
    sl_hal_burtc_enable();
    sl_hal_burtc_stop();
    sl_hal_burtc_wait_sync();
    sl_hal_burtc_clear_interrupts(BURTC_IF_COMP | BURTC_IF_OF);
    sl_hal_burtc_set_compare(compare);

    sBurtcConfigured = true;
    return OT_ERROR_NONE;
}

static bool radioIsIdle(otInstance *aInstance)
{
    otRadioState state = otPlatRadioGetState(aInstance);

    return (state == OT_RADIO_STATE_SLEEP) || (state == OT_RADIO_STATE_DISABLED);
}

#if defined(SL_CATALOG_POWER_MANAGER_PRESENT)
static void tryEnterEm4(void)
{
    otInstance *instance = otInstanceGetSingle();

    if (otTaskletsArePending(instance) || !radioIsIdle(instance))
    {
        sEnterStarted = false;
        return;
    }

    if (!sBurtcConfigured
        && (configureBurtcWake(sWakeMs != 0U ? sWakeMs : sl_ot_em4_get_default_wake_ms(instance)) != OT_ERROR_NONE))
    {
        sEnterStarted = false;
        sPending      = false;
        return;
    }

    sl_hal_burtc_reset_counter();
    sl_hal_burtc_wait_sync();
    sl_power_manager_enter_em4();
}

// EM4 entry only occurs on the next EM2 transition after arming. If Power
// Manager never enters EM2, EM4 will never be entered and sPending stays true.
static void em4OnEmTransition(sl_power_manager_em_t aFrom, sl_power_manager_em_t aTo)
{
    OT_UNUSED_VARIABLE(aFrom);

    if (aTo != SL_POWER_MANAGER_EM2)
    {
        return;
    }

    if (!sPending || sEnterStarted)
    {
        return;
    }

    sEnterStarted = true;
    tryEnterEm4();
}
#endif

//------------------------------------------------------------------------------
// Public API

uint32_t sl_ot_em4_get_default_wake_ms(otInstance *aInstance)
{
    const uint32_t childTimeoutSec = otThreadGetChildTimeout(aInstance);
    uint32_t       wakeSec;

    wakeSec = (uint32_t)(((uint64_t)childTimeoutSec * (uint64_t)SL_OT_EM4_ULFRCO_FMIN_HZ)
                         / (uint64_t)SL_OT_EM4_ULFRCO_FTYP_HZ);

    if (wakeSec == 0U)
    {
        wakeSec = 1U;
    }

    return wakeSec * 1000U;
}

otError sl_ot_em4_init(void)
{
#if !defined(SL_CATALOG_POWER_MANAGER_PRESENT)
    return OT_ERROR_NOT_CAPABLE;
#else
    if (sInitialized)
    {
        return OT_ERROR_NONE;
    }

    if (halGetExtendedResetInfo() == RESET_SOFTWARE_EM4)
    {
        sWokeFromEm4 = true;
        burtcClockEnable();

        if ((sl_hal_burtc_get_pending_interrupts() & BURTC_IF_COMP) != 0U)
        {
            sl_hal_burtc_clear_interrupts(BURTC_IF_COMP);
        }
    }

    sl_power_manager_subscribe_em_transition_event(&sEmTransitionHandle, &sEmTransitionInfo);
    sInitialized = true;
    return OT_ERROR_NONE;
#endif
}

bool sl_ot_em4_is_wake_from_em4(void)
{
    return sWokeFromEm4;
}

otError sl_ot_em4_request(otInstance *aInstance, uint32_t aWakeMs)
{
    uint32_t wakeMs;

    if (aInstance == NULL)
    {
        return OT_ERROR_INVALID_ARGS;
    }

    if (!sInitialized)
    {
        otError initError = sl_ot_em4_init();

        if (initError != OT_ERROR_NONE)
        {
            return initError;
        }
    }

    if (sPending || sEnterStarted)
    {
        return OT_ERROR_INVALID_STATE;
    }

    wakeMs = (aWakeMs != 0U) ? aWakeMs : sl_ot_em4_get_default_wake_ms(aInstance);

    if (configureBurtcWake(wakeMs) != OT_ERROR_NONE)
    {
        return OT_ERROR_FAILED;
    }

    sPending      = true;
    sEnterStarted = false;
    return OT_ERROR_NONE;
}

void sl_ot_em4_cancel(void)
{
    if (sBurtcConfigured)
    {
        sl_hal_burtc_stop();
        sl_hal_burtc_disable();
    }

    sPending         = false;
    sEnterStarted    = false;
    sBurtcConfigured = false;
}

bool sl_ot_em4_is_pending(void)
{
    return sPending;
}

#endif /* BURTC_PRESENT */
