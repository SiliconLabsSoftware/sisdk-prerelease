/***************************************************************************//**
 * @file
 * @brief SL Diagnostic reset-cause latch and boot classification
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

#include <stdbool.h>
#include <stdint.h>

#include "em_device.h"
#include "sl_common.h"
#include "sl_core.h"
#include "sli_diag.h"
#include "sli_diag_internal.h"

#if SL_DIAG_ENABLE

#if defined(SL_CATALOG_HAL_EMU_PRESENT)
#include "sl_hal_emu.h"
#endif

/*******************************************************************************
 *******************************   DEFINES   ***********************************
 ******************************************************************************/

// Reset-cause registers read by the default latch. Wide-mask devices have a
// hard and a soft reset-cause register; other devices have RSTCAUSE. Each
// layout needs the masks under which the HAL both declares and defines its
// getters.
#if defined(SL_CATALOG_HAL_EMU_PRESENT)                                 \
  && defined(_EMU_HARDRST_CAUSE_MASK) && defined(_EMU_HARDRSTCTRL_MASK) \
  && defined(_EMU_SOFTRST_CAUSE_MASK) && defined(_EMU_SOFTRSTCTRL_MASK)
#define RESET_CAUSE_HARD_SOFT
#elif defined(SL_CATALOG_HAL_EMU_PRESENT) \
  && defined(_EMU_RSTCAUSE_MASK) && defined(_EMU_RSTCTRL_MASK)
#define RESET_CAUSE_SINGLE
#endif

// Watchdog reset bits, in the register that holds them: SOFTRST on wide-mask
// devices (HARDRST has no watchdog cause), RSTCAUSE otherwise. A bit the
// device does not define is 0.
#if defined(RESET_CAUSE_HARD_SOFT)
#if defined(EMU_SOFTRST_CAUSE_WDOG0)
#define RESET_WDOG0  EMU_SOFTRST_CAUSE_WDOG0
#endif
#if defined(EMU_SOFTRST_CAUSE_WDOG1)
#define RESET_WDOG1  EMU_SOFTRST_CAUSE_WDOG1
#endif
#if defined(EMU_SOFTRST_CAUSE_WDOG2)
#define RESET_WDOG2  EMU_SOFTRST_CAUSE_WDOG2
#endif
#if defined(EMU_SOFTRST_CAUSE_WDOG3)
#define RESET_WDOG3  EMU_SOFTRST_CAUSE_WDOG3
#endif
#elif defined(RESET_CAUSE_SINGLE)
#if defined(EMU_RSTCAUSE_WDOG0)
#define RESET_WDOG0  EMU_RSTCAUSE_WDOG0
#endif
#if defined(EMU_RSTCAUSE_WDOG1)
#define RESET_WDOG1  EMU_RSTCAUSE_WDOG1
#endif
#endif

#if !defined(RESET_WDOG0)
#define RESET_WDOG0  0u
#endif
#if !defined(RESET_WDOG1)
#define RESET_WDOG1  0u
#endif
#if !defined(RESET_WDOG2)
#define RESET_WDOG2  0u
#endif
#if !defined(RESET_WDOG3)
#define RESET_WDOG3  0u
#endif

// Mask of every watchdog reset bit on this device. It is zero when the device
// defines none.
#define RESET_WDOG_MASK  ((uint32_t)(RESET_WDOG0 | RESET_WDOG1 | RESET_WDOG2 | RESET_WDOG3))

/*******************************************************************************
 ***************************  LOCAL VARIABLES   ********************************
 ******************************************************************************/

// Reset causes for this boot. This RAM is not retained across reset.
static sli_diag_reset_boot_t reset_boot = { 0 };
// True after the reset cause was latched for this boot.
static bool reset_boot_latched = false;

/*******************************************************************************
 ***************************   LOCAL FUNCTIONS   *******************************
 ******************************************************************************/

/*******************************************************************************
 * Checks whether a latched reset cause reports a watchdog reset. Matches the
 * register layout of the default latch.
 ******************************************************************************/
static bool reset_is_watchdog(const sli_diag_reset_boot_t *reset)
{
#if defined(RESET_CAUSE_HARD_SOFT)
  return (reset->reset_reason_ext & RESET_WDOG_MASK) != 0u;
#else
  return (reset->reset_reason & RESET_WDOG_MASK) != 0u;
#endif
}

/*******************************************************************************
 **************************   GLOBAL FUNCTIONS   *******************************
 ******************************************************************************/

/***************************************************************************//**
 * Latches the reset cause.
 ******************************************************************************/
SL_WEAK void sli_platform_latch_reset_boot(sli_diag_reset_boot_t *out)
{
  if (out == NULL) {
    return;
  }
#if defined(RESET_CAUSE_HARD_SOFT)
  // The HAL caches the first register read for the whole boot.
  out->reset_reason = sl_hal_emu_get_hard_reset_cause();
  out->reset_reason_ext = sl_hal_emu_get_soft_reset_cause();
#elif defined(RESET_CAUSE_SINGLE)
  out->reset_reason = sl_hal_emu_get_reset_cause();
  out->reset_reason_ext = 0u;
#else
  // No supported reset-cause registers on this device.
  out->reset_reason = 0u;
  out->reset_reason_ext = 0u;
#endif
}

/***************************************************************************//**
 * Latches the reset cause of this boot.
 ******************************************************************************/
void sli_diag_latch_reset_boot(void)
{
  CORE_DECLARE_IRQ_STATE;

  // Publish the latched value and the flag together.
  CORE_ENTER_CRITICAL();
  if (!reset_boot_latched) {
    // Latch the reset cause.
    sli_platform_latch_reset_boot(&reset_boot);

    // Set the latched flag.
    reset_boot_latched = true;
  }
  CORE_EXIT_CRITICAL();
}

/***************************************************************************//**
 * Gets the reset cause of this boot.
 ******************************************************************************/
void sli_diag_get_reset_boot(sli_diag_reset_boot_t *out)
{
  CORE_DECLARE_IRQ_STATE;

  if (out == NULL) {
    return;
  }
  // Copy the latched value only. Captures never read reset-cause hardware.
  CORE_ENTER_CRITICAL();
  *out = reset_boot;
  CORE_EXIT_CRITICAL();
}

/***************************************************************************//**
 * Gets the trigger assigned to this boot from its reset cause.
 ******************************************************************************/
sl_diag_trigger_t sli_diag_get_boot_trigger(void)
{
  sli_diag_reset_boot_t reset;

  // Do not classify the boot until sl_diag_init() latched the reset cause.
  if (!sli_diag_is_initialized()) {
    return SL_DIAG_TRIGGER_NONE;
  }

  // Get the reset cause and classify the boot.
  sli_diag_get_reset_boot(&reset);

  // Return the trigger.
  return reset_is_watchdog(&reset) ? SL_DIAG_TRIGGER_WATCHDOG_RESET : SL_DIAG_TRIGGER_NONE;
}

#endif // SL_DIAG_ENABLE

/***************************************************************************//**
 * Latches the reset cause of this boot at platform init.
 ******************************************************************************/
void sli_diag_platform_init(void)
{
#if SL_DIAG_ENABLE
  sli_diag_latch_reset_boot();
#endif
}
