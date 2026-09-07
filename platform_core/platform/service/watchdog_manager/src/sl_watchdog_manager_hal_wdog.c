/***************************************************************************//**
 * @file
 * @brief Watchdog Manager HAL Implementation for WDOG peripheral
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

#include "sli_watchdog_manager_hal.h"
#include "sli_watchdog_manager_hal_clock_config.h"
#include "sl_hal_wdog.h"
#include "sl_clock_manager.h"
#include "sl_core.h"
#include "sl_assert.h"
#include "sl_watchdog_manager_config.h"
#include "sli_watchdog_manager.h"
#include "sl_interrupt_manager.h"
#include "sl_component_catalog.h"
#if defined(SL_CATALOG_CRASH_MANAGER_COMPONENT_PRESENT)
#include "sli_crash_manager.h"
#endif
#include "em_device.h"
#if SLI_WATCHDOG_MANAGER_USE_EM_TRANSITION_HOOK
#include "sl_power_manager.h"
#endif
/***************************************************************************//**
 * @addtogroup watchdog_manager_hal
 * @{
 ******************************************************************************/

/*******************************************************************************
 *******************************   DEFINES   ***********************************
 ******************************************************************************/

// Determine which WDOG instance to use.
// Priority: WDOG1 > WDOG0.
#if defined(WDOG_PRESENT) && (WDOG_COUNT > 1)
  #define WATCHDOG_PERIPHERAL     WDOG1
  #define WATCHDOG_BUS_CLOCK      SL_BUS_CLOCK_WDOG1
  #define WATCHDOG_IRQN           WDOG1_IRQn
#elif defined(WDOG_PRESENT) && (WDOG_COUNT == 1)
  #define WATCHDOG_PERIPHERAL     WDOG0
  #define WATCHDOG_BUS_CLOCK      SL_BUS_CLOCK_WDOG0
  #define WATCHDOG_IRQN           WDOG0_IRQn
#else
  #warning "No WDOG peripheral available"
#endif

#if defined(SL_CATALOG_CRASH_MANAGER_COMPONENT_PRESENT)
  #define SLI_WATCHDOG_MANAGER_CRASH_MANAGER_INTEGRATION  1
#else
  #define SLI_WATCHDOG_MANAGER_CRASH_MANAGER_INTEGRATION  0
#endif

// CMU clock select for the WDOG instance used by the watchdog manager
#if defined(WDOG_PRESENT) && (WDOG_COUNT > 1) && defined(_CMU_WDOG1CLKCTRL_MASK)
  #define WATCHDOG_HAS_CLKSEL              1
  #define WATCHDOG_CLKCTRL_REG             CMU->WDOG1CLKCTRL
  #define WATCHDOG_CLKCTRL_CLKSEL_MASK     _CMU_WDOG1CLKCTRL_CLKSEL_MASK
  #if defined(CMU_WDOG1CLKCTRL_CLKSEL_HCLKDIV1024)
    #define WATCHDOG_CLKSEL_HCLK  CMU_WDOG1CLKCTRL_CLKSEL_HCLKDIV1024
  #elif defined(_CMU_WDOG1CLKCTRL_CLKSEL_HCLKDIV1024) \
    && defined(_CMU_WDOG1CLKCTRL_CLKSEL_SHIFT)
    #define WATCHDOG_CLKSEL_HCLK \
    (_CMU_WDOG1CLKCTRL_CLKSEL_HCLKDIV1024 << _CMU_WDOG1CLKCTRL_CLKSEL_SHIFT)
  #elif defined(CMU_WDOG1CLKCTRL_CLKSEL_HOSTCPU0CLKDIV1024)
    #define WATCHDOG_CLKSEL_HCLK  CMU_WDOG1CLKCTRL_CLKSEL_HOSTCPU0CLKDIV1024
    #define WATCHDOG_CLKSEL_HCLK_ALT  CMU_WDOG1CLKCTRL_CLKSEL_HOSTCPU1CLKDIV1024
  #elif defined(_CMU_WDOG1CLKCTRL_CLKSEL_HOSTCPU0CLKDIV1024) \
    && defined(_CMU_WDOG1CLKCTRL_CLKSEL_SHIFT)
    #define WATCHDOG_CLKSEL_HCLK \
    (_CMU_WDOG1CLKCTRL_CLKSEL_HOSTCPU0CLKDIV1024 << _CMU_WDOG1CLKCTRL_CLKSEL_SHIFT)
    #define WATCHDOG_CLKSEL_HCLK_ALT \
    (_CMU_WDOG1CLKCTRL_CLKSEL_HOSTCPU1CLKDIV1024 << _CMU_WDOG1CLKCTRL_CLKSEL_SHIFT)
  #endif
  #define WATCHDOG_CLKSEL_LFRCO            CMU_WDOG1CLKCTRL_CLKSEL_LFRCO
  #define WATCHDOG_CLKSEL_LFXO             CMU_WDOG1CLKCTRL_CLKSEL_LFXO
  #define WATCHDOG_CLKSEL_ULFRCO           CMU_WDOG1CLKCTRL_CLKSEL_ULFRCO
#elif defined(WDOG_PRESENT) && (WDOG_COUNT == 1) && defined(_CMU_WDOG0CLKCTRL_MASK)
  #define WATCHDOG_HAS_CLKSEL              1
  #define WATCHDOG_CLKCTRL_REG             CMU->WDOG0CLKCTRL
  #define WATCHDOG_CLKCTRL_CLKSEL_MASK     _CMU_WDOG0CLKCTRL_CLKSEL_MASK
  #if defined(CMU_WDOG0CLKCTRL_CLKSEL_HCLKDIV1024)
    #define WATCHDOG_CLKSEL_HCLK  CMU_WDOG0CLKCTRL_CLKSEL_HCLKDIV1024
  #elif defined(_CMU_WDOG0CLKCTRL_CLKSEL_HCLKDIV1024) \
    && defined(_CMU_WDOG0CLKCTRL_CLKSEL_SHIFT)
    #define WATCHDOG_CLKSEL_HCLK \
    (_CMU_WDOG0CLKCTRL_CLKSEL_HCLKDIV1024 << _CMU_WDOG0CLKCTRL_CLKSEL_SHIFT)
  #elif defined(CMU_WDOG0CLKCTRL_CLKSEL_HOSTCPU0CLKDIV1024)
    #define WATCHDOG_CLKSEL_HCLK  CMU_WDOG0CLKCTRL_CLKSEL_HOSTCPU0CLKDIV1024
    #define WATCHDOG_CLKSEL_HCLK_ALT  CMU_WDOG0CLKCTRL_CLKSEL_HOSTCPU1CLKDIV1024
  #elif defined(_CMU_WDOG0CLKCTRL_CLKSEL_HOSTCPU0CLKDIV1024) \
    && defined(_CMU_WDOG0CLKCTRL_CLKSEL_SHIFT)
    #define WATCHDOG_CLKSEL_HCLK \
    (_CMU_WDOG0CLKCTRL_CLKSEL_HOSTCPU0CLKDIV1024 << _CMU_WDOG0CLKCTRL_CLKSEL_SHIFT)
    #define WATCHDOG_CLKSEL_HCLK_ALT \
    (_CMU_WDOG0CLKCTRL_CLKSEL_HOSTCPU1CLKDIV1024 << _CMU_WDOG0CLKCTRL_CLKSEL_SHIFT)
  #endif
  #define WATCHDOG_CLKSEL_LFRCO            CMU_WDOG0CLKCTRL_CLKSEL_LFRCO
  #define WATCHDOG_CLKSEL_LFXO             CMU_WDOG0CLKCTRL_CLKSEL_LFXO
  #define WATCHDOG_CLKSEL_ULFRCO           CMU_WDOG0CLKCTRL_CLKSEL_ULFRCO
#else
  #define WATCHDOG_HAS_CLKSEL              0
#endif
#ifndef WATCHDOG_CLKSEL_HCLK
  #undef WATCHDOG_HAS_CLKSEL
  #define WATCHDOG_HAS_CLKSEL              0
#endif
#if WATCHDOG_HAS_CLKSEL
#if defined(WATCHDOG_CLKSEL_HCLK_ALT)
  #define WATCHDOG_CLKSEL_IS_HCLK(sel) \
    (((sel) == WATCHDOG_CLKSEL_HCLK) || ((sel) == WATCHDOG_CLKSEL_HCLK_ALT))
#else
  #define WATCHDOG_CLKSEL_IS_HCLK(sel)  ((sel) == WATCHDOG_CLKSEL_HCLK)
#endif
#endif
/*******************************************************************************
 *****************************   LOCAL DATA   **********************************
 ******************************************************************************/

/// Flag indicating if watchdog has been initialized.
static bool hal_initialized = false;

static uint8_t hal_timeout_period;

#if SLI_WATCHDOG_MANAGER_USE_EM_TRANSITION_HOOK
/// True if we disabled WDOG when leaving EM0; re-enable only when entering EM0 if set.
static bool wdog_disabled_for_sleep = false;
#endif

#if defined(_WDOG_CFG_WARNSEL_MASK)
/// True after starve IRQ handler and NVIC are installed.
static bool starve_interrupt_enabled = false;

/***************************************************************************//**
 * @brief WDOG warning interrupt handler (starve).
 ******************************************************************************/
static void watchdog_starve_irq_handler(void)
{
  uint32_t flags = sl_hal_wdog_get_enabled_pending_interrupts(WATCHDOG_PERIPHERAL);

  if ((flags & WDOG_IF_WARN) != 0u) {
    sl_hal_wdog_clear_interrupts(WATCHDOG_PERIPHERAL, WDOG_IF_WARN);
#if SLI_WATCHDOG_MANAGER_CRASH_MANAGER_INTEGRATION
    sli_crash_manager_on_watchdog_warning(flags);
#endif
    sli_watchdog_manager_on_starve();
  }

#if defined(WDOG_IF_TOUT)
  if ((flags & WDOG_IF_TOUT) != 0u) {
    sl_hal_wdog_clear_interrupts(WATCHDOG_PERIPHERAL, WDOG_IF_TOUT);
  }
#endif
}

#if defined(WDOG_COUNT) && (WDOG_COUNT > 1)
/***************************************************************************//**
 * @brief WDOG1 IRQ vector (overrides weak Default_Handler alias in startup).
 *
 * @details On Series 2, sl_interrupt_manager_set_irq_handler() requires the
 *          interrupt_manager_vector_table_in_ram component. Providing this
 *          symbol ensures the starve handler is reachable without that component.
 ******************************************************************************/
void WDOG1_IRQHandler(void)
{
  watchdog_starve_irq_handler();
}
#elif defined(WDOG_COUNT) && (WDOG_COUNT == 1)
void WDOG0_IRQHandler(void)
{
  watchdog_starve_irq_handler();
}
#endif

/***************************************************************************//**
 * @brief Map config warning period to HAL warning select.
 ******************************************************************************/
static sl_hal_wdog_warning_timeout_select_t hal_map_warning_time(void)
{
  switch (SL_WATCHDOG_MANAGER_WARNING_TIME) {
    case SL_WATCHDOG_MANAGER_WARNING_TIME25:
      return SL_WDOG_WARNING_TIME25;

    case SL_WATCHDOG_MANAGER_WARNING_TIME50:
      return SL_WDOG_WARNING_TIME50;

    case SL_WATCHDOG_MANAGER_WARNING_TIME75:
      return SL_WDOG_WARNING_TIME75;

    default:
#if SLI_WATCHDOG_MANAGER_CRASH_MANAGER_INTEGRATION
      return SL_WDOG_WARNING_TIME75;
#else
      return SL_WDOG_WARNING_DISABLE;
#endif
  }
}
#endif

#if WATCHDOG_HAS_CLKSEL

static sli_watchdog_manager_hal_clock_source_t hal_clock_source =
  SLI_WATCHDOG_MANAGER_HAL_CLK_HCLKDIV1024;

/***************************************************************************//**
 * @brief Map HAL clock enum to CMU WDOGxCLKCTRL.CLKSEL value.
 *
 * @param[in]  clock_source HAL clock source.
 * @param[out] cmu_clksel   CMU CLKSEL bitfield value.
 *
 * @return SL_STATUS_OK on success.
 * @return SL_STATUS_NOT_SUPPORTED if LFXO is requested but not enabled in project.
 * @return SL_STATUS_INVALID_PARAMETER if @p clock_source is invalid.
 ******************************************************************************/
static sl_status_t hal_cmu_encode_clock_source(
  sli_watchdog_manager_hal_clock_source_t clock_source,
  uint32_t *cmu_clksel)
{
  switch (clock_source) {
    case SLI_WATCHDOG_MANAGER_HAL_CLK_HCLKDIV1024:
      *cmu_clksel = WATCHDOG_CLKSEL_HCLK;
      return SL_STATUS_OK;

    case SLI_WATCHDOG_MANAGER_HAL_CLK_LFRCO:
      *cmu_clksel = WATCHDOG_CLKSEL_LFRCO;
      return SL_STATUS_OK;

    case SLI_WATCHDOG_MANAGER_HAL_CLK_ULFRCO:
      *cmu_clksel = WATCHDOG_CLKSEL_ULFRCO;
      return SL_STATUS_OK;

    case SLI_WATCHDOG_MANAGER_HAL_CLK_LFXO:
#if SLI_WATCHDOG_MANAGER_HAL_LFXO_CLOCK_AVAILABLE
      *cmu_clksel = WATCHDOG_CLKSEL_LFXO;
      return SL_STATUS_OK;
#else
      (void)cmu_clksel;
      return SL_STATUS_NOT_SUPPORTED;
#endif

    case SLI_WATCHDOG_MANAGER_HAL_CLK_INVALID:
    default:
      return SL_STATUS_INVALID_PARAMETER;
  }
}

/***************************************************************************//**
 * @brief Map CMU WDOGxCLKCTRL.CLKSEL value to HAL clock enum.
 *
 * @param[in]  cmu_clksel   CMU CLKSEL bitfield value.
 * @param[out] clock_source HAL clock source.
 *
 * @return SL_STATUS_OK on success.
 * @return SL_STATUS_INVALID_STATE if @p cmu_clksel is not a recognized selection.
 ******************************************************************************/
static sl_status_t hal_cmu_decode_clock_source(
  uint32_t cmu_clksel,
  sli_watchdog_manager_hal_clock_source_t *clock_source)
{
  if (WATCHDOG_CLKSEL_IS_HCLK(cmu_clksel)) {
    *clock_source = SLI_WATCHDOG_MANAGER_HAL_CLK_HCLKDIV1024;
  } else if (cmu_clksel == WATCHDOG_CLKSEL_LFRCO) {
    *clock_source = SLI_WATCHDOG_MANAGER_HAL_CLK_LFRCO;
  } else if (cmu_clksel == WATCHDOG_CLKSEL_LFXO) {
    *clock_source = SLI_WATCHDOG_MANAGER_HAL_CLK_LFXO;
  } else if (cmu_clksel == WATCHDOG_CLKSEL_ULFRCO) {
    *clock_source = SLI_WATCHDOG_MANAGER_HAL_CLK_ULFRCO;
  } else {
    return SL_STATUS_INVALID_STATE;
  }

  return SL_STATUS_OK;
}

/***************************************************************************//**
 * @brief Write CMU WDOGxCLKCTRL.CLKSEL for the selected HAL clock source.
 *
 * @param[in] clock_source HAL clock source to program.
 *
 * @return SL_STATUS_OK on success.
 * @return Error code from hal_cmu_encode_clock_source() on failure.
 ******************************************************************************/
static sl_status_t hal_cmu_program_clock_source(
  sli_watchdog_manager_hal_clock_source_t clock_source)
{
  uint32_t cmu_clksel;
  sl_status_t status;

  status = hal_cmu_encode_clock_source(clock_source, &cmu_clksel);
  if (status != SL_STATUS_OK) {
    return status;
  }

  WATCHDOG_CLKCTRL_REG = (WATCHDOG_CLKCTRL_REG & ~WATCHDOG_CLKCTRL_CLKSEL_MASK)
                         | cmu_clksel;
  return SL_STATUS_OK;
}

/***************************************************************************//**
 * @brief Read CMU WDOGxCLKCTRL.CLKSEL and update hal_clock_source.
 *
 * @details Called on first HAL init only. Does not modify the CMU register.
 *
 * @return SL_STATUS_OK on success.
 * @return SL_STATUS_INVALID_STATE if the CMU CLKSEL value is not recognized.
 ******************************************************************************/
static sl_status_t hal_cmu_load_clock_source(void)
{
  uint32_t cmu_clksel = WATCHDOG_CLKCTRL_REG & WATCHDOG_CLKCTRL_CLKSEL_MASK;

  return hal_cmu_decode_clock_source(cmu_clksel, &hal_clock_source);
}

#endif // WATCHDOG_HAS_CLKSEL

/*******************************************************************************
 **************************   GLOBAL FUNCTIONS   *******************************
 ******************************************************************************/

/***************************************************************************//**
 * @brief Initialize the hardware watchdog.
 *
 * @details On first init, loads the CMU clock source from Clock Manager without
 *          rewriting CLKSEL. On subsequent inits, programs CLKSEL from
 *          hal_clock_source before calling sl_hal_wdog_init().
 ******************************************************************************/
sl_status_t sli_watchdog_manager_hal_init(uint8_t timeout_period)
{
  if (timeout_period > 15) {
    return SL_STATUS_INVALID_PARAMETER;
  }

  // Enable clock to watchdog.
  sl_clock_manager_enable_bus_clock(WATCHDOG_BUS_CLOCK);

#if WATCHDOG_HAS_CLKSEL
  {
    sl_status_t status = SL_STATUS_OK;

    if (!hal_initialized) {
      // Sync HAL state from clock manager; do not rewrite CMU on first init.
      status = hal_cmu_load_clock_source();
      if (status != SL_STATUS_OK) {
        return status;
      }
    } else {
      status = hal_cmu_program_clock_source(hal_clock_source);
      if (status != SL_STATUS_OK) {
        return status;
      }
    }
  }
#endif

  // Initialize WDOG structure.
  sl_hal_wdog_init_t init = SL_HAL_WDOG_INIT_DEFAULT;

  // Set timeout period.
  init.period_select = timeout_period;

  // Debug run: counter runs during debug halt when set (from config).
#if defined(_WDOG_CFG_DEBUGRUN_MASK)
  init.debug_run = (SL_WATCHDOG_MANAGER_DEBUG_RUN != 0);
#endif

  // Block EM4 when WDOG is enabled (from config).
  init.em4_block = (SL_WATCHDOG_MANAGER_EM4_BLOCK != 0);

  // Lock configuration after init (from config).
  init.lock = (SL_WATCHDOG_MANAGER_LOCK != 0);

  // Disable reset output when set (from config).
  init.reset_disable = (SL_WATCHDOG_MANAGER_RESET_DISABLE != 0);

  // EM1/EM2/EM3 run: counter runs in sleep when set (from config).
#if defined(_WDOG_CFG_EM1RUN_MASK)
  init.em1_run = (SL_WATCHDOG_MANAGER_EM1_RUN != 0);
#endif
#if defined(_WDOG_CFG_EM2RUN_MASK)
  init.em2_run = (SL_WATCHDOG_MANAGER_EM2_RUN != 0);
#endif
#if defined(_WDOG_CFG_EM3RUN_MASK)
  init.em3_run = (SL_WATCHDOG_MANAGER_EM3_RUN != 0);
#endif

  // Warning interrupt period (starve callback fires at this point before reset).
#if defined(_WDOG_CFG_WARNSEL_MASK)
  init.warning_time_select = hal_map_warning_time();
#else
  init.warning_time_select = SL_WDOG_WARNING_DISABLE;
#endif

  // Disable window interrupt.
  init.window_time_select = SL_WDOG_ILLEGAL_WINDOW_DISABLE;

  // Initialize watchdog.
  sl_hal_wdog_init(WATCHDOG_PERIPHERAL, &init);

  hal_initialized = true;

  hal_timeout_period = timeout_period;

  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Start the hardware watchdog.
 ******************************************************************************/
sl_status_t sli_watchdog_manager_hal_start(void)
{
  if (!hal_initialized) {
    return SL_STATUS_NOT_INITIALIZED;
  }

  // Enable watchdog.
  sl_hal_wdog_enable(WATCHDOG_PERIPHERAL);

  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Feed the hardware watchdog.
 ******************************************************************************/
sl_status_t sli_watchdog_manager_hal_feed(void)
{
  if (!hal_initialized) {
    return SL_STATUS_NOT_INITIALIZED;
  }

  // Feed the watchdog.
  sl_hal_wdog_feed(WATCHDOG_PERIPHERAL);
  // Wait for feed operation to complete.
  sl_hal_wdog_wait_sync(WATCHDOG_PERIPHERAL);

  return SL_STATUS_OK;
}

#if defined(_WDOG_CFG_WARNSEL_MASK)
/***************************************************************************//**
 * Enable WDOG warning interrupt for starve callback.
 ******************************************************************************/
sl_status_t sli_watchdog_manager_hal_enable_starve_interrupt(void)
{
  if (!hal_initialized) {
    return SL_STATUS_NOT_INITIALIZED;
  }

  if (hal_map_warning_time() == SL_WDOG_WARNING_DISABLE) {
    return SL_STATUS_NOT_SUPPORTED;
  }

  if (!starve_interrupt_enabled) {
    sl_hal_wdog_clear_interrupts(WATCHDOG_PERIPHERAL, WDOG_IF_WARN);
    sl_hal_wdog_enable_interrupts(WATCHDOG_PERIPHERAL, WDOG_IF_WARN);
    sl_interrupt_manager_clear_irq_pending(WATCHDOG_IRQN);
    sl_interrupt_manager_enable_irq(WATCHDOG_IRQN);
#if defined(SL_CATALOG_INTERRUPT_MANAGER_VECTOR_TABLE_IN_RAM_PRESENT)
    {
      sl_status_t irq_status = sl_interrupt_manager_set_irq_handler(
        WATCHDOG_IRQN,
        watchdog_starve_irq_handler);
      if (irq_status != SL_STATUS_OK) {
        return irq_status;
      }
    }
#endif
    starve_interrupt_enabled = true;
  }

  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Disable WDOG warning interrupt for starve callback.
 ******************************************************************************/
sl_status_t sli_watchdog_manager_hal_disable_starve_interrupt(void)
{
  if (!hal_initialized) {
    return SL_STATUS_NOT_INITIALIZED;
  }

  if (!starve_interrupt_enabled) {
    return SL_STATUS_OK;
  }

  sl_hal_wdog_clear_interrupts(WATCHDOG_PERIPHERAL, WDOG_IF_WARN);
  sl_hal_wdog_disable_interrupts(WATCHDOG_PERIPHERAL, WDOG_IF_WARN);
  sl_interrupt_manager_disable_irq(WATCHDOG_IRQN);
  starve_interrupt_enabled = false;

  return SL_STATUS_OK;
}
#endif

/***************************************************************************//**
 * Disable the hardware watchdog.
 ******************************************************************************/
sl_status_t sli_watchdog_manager_hal_disable(void)
{
  if (!hal_initialized) {
    return SL_STATUS_NOT_INITIALIZED;
  }

  // Disable Hardware watchdog.
  sl_hal_wdog_disable(WATCHDOG_PERIPHERAL);
  // Wait for disable operation to complete.
  sl_hal_wdog_wait_ready(WATCHDOG_PERIPHERAL);

  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Enable the hardware watchdog.
 ******************************************************************************/
sl_status_t sli_watchdog_manager_hal_enable(void)
{
  if (!hal_initialized) {
    return SL_STATUS_NOT_INITIALIZED;
  }

  // Enable Hardware watchdog.
  sl_hal_wdog_enable(WATCHDOG_PERIPHERAL);

  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Check if hardware watchdog has EM1RUN capability.
 ******************************************************************************/
bool sli_watchdog_manager_hal_has_em1run(void)
{
  return (SLI_WATCHDOG_MANAGER_HAS_EM1RUN != 0);
}

/***************************************************************************//**
 * @brief Get the hardware watchdog timeout period index.
 ******************************************************************************/
sl_status_t sli_watchdog_manager_hal_get_timeout_period(uint8_t *timeout_period)
{
  if (timeout_period == NULL) {
    return SL_STATUS_NULL_POINTER;
  }

  if (!hal_initialized) {
    return SL_STATUS_NOT_INITIALIZED;
  }

  *timeout_period = hal_timeout_period;

  return SL_STATUS_OK;
}

/***************************************************************************//**
 * @brief Set the hardware watchdog timeout period index.
 *
 * @note Caller must disable the hardware watchdog before calling this function.
 ******************************************************************************/
sl_status_t sli_watchdog_manager_hal_set_timeout_period(uint8_t timeout_period)
{
  if (timeout_period > 15) {
    return SL_STATUS_INVALID_PARAMETER;
  }

  if (!hal_initialized) {
    return SL_STATUS_NOT_INITIALIZED;
  }

  if (timeout_period == hal_timeout_period) {
    return SL_STATUS_OK;
  }

  return sli_watchdog_manager_hal_init(timeout_period);
}

/***************************************************************************//**
 * @brief Get the hardware watchdog CMU clock source.
 ******************************************************************************/
sl_status_t sli_watchdog_manager_hal_get_clock_source(
  sli_watchdog_manager_hal_clock_source_t *clock_source)
{
  if (clock_source == NULL) {
    return SL_STATUS_NULL_POINTER;
  }

#if !WATCHDOG_HAS_CLKSEL
  *clock_source = SLI_WATCHDOG_MANAGER_HAL_CLK_INVALID;
  return SL_STATUS_NOT_SUPPORTED;
#else
  if (!hal_initialized) {
    return SL_STATUS_NOT_INITIALIZED;
  }

  *clock_source = hal_clock_source;
  return SL_STATUS_OK;
#endif
}

/***************************************************************************//**
 * @brief Set the hardware watchdog CMU clock source.
 *
 * @note Caller must disable the hardware watchdog before calling this function.
 ******************************************************************************/
sl_status_t sli_watchdog_manager_hal_set_clock_source(
  sli_watchdog_manager_hal_clock_source_t clock_source)
{
#if !WATCHDOG_HAS_CLKSEL
  (void)clock_source;
  return SL_STATUS_NOT_SUPPORTED;
#else
  sl_status_t status;
  uint32_t cmu_clksel;

  if (!hal_initialized) {
    return SL_STATUS_NOT_INITIALIZED;
  }
  if (clock_source == hal_clock_source) {
    return SL_STATUS_OK;
  }

  status = hal_cmu_encode_clock_source(clock_source, &cmu_clksel);
  if (status != SL_STATUS_OK) {
    return status;
  }

  hal_clock_source = clock_source;
  return sli_watchdog_manager_hal_init(hal_timeout_period);
#endif
}

/***************************************************************************//**
 * Notify HAL of an energy mode transition (disable when leaving EM0, re-enable when entering EM0).
 * When leaving EM0: software-disable WDOG unless config EMx_RUN is set or the part has
 * EMxRUN HW (then EM behavior is handled by WDOG init, not by disable here).
 ******************************************************************************/
#if SLI_WATCHDOG_MANAGER_USE_EM_TRANSITION_HOOK
void sli_watchdog_manager_hal_on_em_transition(uint8_t from, uint8_t to)
{
  if (!hal_initialized) {
    return;
  }

  // SL_POWER_MANAGER_EM0 = 0, EM1 = 1, EM2 = 2, EM3 = 3
  if (from == SL_POWER_MANAGER_EM0) {
    // Leaving EM0: skip disable if EMx_RUN requested or HW has EMxRUN (init sets EM behavior).
    wdog_disabled_for_sleep = true;
    if ((to == SL_POWER_MANAGER_EM1)
        && ((SL_WATCHDOG_MANAGER_EM1_RUN != 0) || (SLI_WATCHDOG_MANAGER_HAS_EM1RUN != 0))) {
      wdog_disabled_for_sleep = false;
    } else if ((to == SL_POWER_MANAGER_EM2)
               && ((SL_WATCHDOG_MANAGER_EM2_RUN != 0) || (SLI_WATCHDOG_MANAGER_HAS_EM2RUN != 0))) {
      wdog_disabled_for_sleep = false;
    } else if ((to == SL_POWER_MANAGER_EM3)
               && ((SL_WATCHDOG_MANAGER_EM3_RUN != 0) || (SLI_WATCHDOG_MANAGER_HAS_EM3RUN != 0))) {
      wdog_disabled_for_sleep = false;
    }
    if (wdog_disabled_for_sleep) {
      sl_hal_wdog_disable(WATCHDOG_PERIPHERAL);
      sl_hal_wdog_wait_ready(WATCHDOG_PERIPHERAL);
    }
  } else if ((to == SL_POWER_MANAGER_EM0) && wdog_disabled_for_sleep) {
    // Entering EM0: re-enable watchdog only if we had disabled it when leaving EM0.
    sl_hal_wdog_enable(WATCHDOG_PERIPHERAL);
    wdog_disabled_for_sleep = false;
  }
}
#endif

/***************************************************************************//**
 * Enable reset on timeout.
 ******************************************************************************/
sl_status_t sli_watchdog_manager_hal_enable_reset(void)
{
  if (sl_hal_wdog_is_enabled(WATCHDOG_PERIPHERAL)) {
    // Cannot perform operation on enabled watchdog.
    return SL_STATUS_PERMISSION;
  }

  // Enable reset on timeout.
  sl_hal_wdog_enable_reset(WATCHDOG_PERIPHERAL);

  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Disable reset on timeout.
 ******************************************************************************/
sl_status_t sli_watchdog_manager_hal_disable_reset(void)
{
  if (sl_hal_wdog_is_enabled(WATCHDOG_PERIPHERAL)) {
    // Cannot perform operation on enabled watchdog.
    return SL_STATUS_PERMISSION;
  }

  // Disable reset on timeout.
  sl_hal_wdog_disable_reset(WATCHDOG_PERIPHERAL);

  return SL_STATUS_OK;
}
/** @} (end addtogroup watchdog_manager_hal) */
