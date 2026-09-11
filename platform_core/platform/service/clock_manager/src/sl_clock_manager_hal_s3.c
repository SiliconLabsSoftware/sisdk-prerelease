/***************************************************************************//**
 * @file
 * @brief Clock Manager HAL API implementations.
 *******************************************************************************
 * # License
 * <b>Copyright 2023 Silicon Laboratories Inc. www.silabs.com</b>
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

#include "em_device.h"

#include <stddef.h>
#include "sl_clock_manager.h"
#include "sli_clock_manager.h"
#include "sli_clock_manager_hal.h"
#include "sli_clock_manager_perpll.h"
#include "sli_clock_manager_log.h"
#include "sl_assert.h"
#include "sl_common.h"
#include "sl_core.h"
#include "sl_gpio.h"
#include "sl_hal_bus.h"
#include "sl_se_manager_extmem.h"
#include "sl_se_manager_util.h"
#include "sl_se_manager.h"
#if defined(SL_COMPONENT_CATALOG_PRESENT)
#include "sl_component_catalog.h"
#endif
#if defined(SL_CATALOG_CLOCK_MANAGER_PRESENT)
#include "sl_clock_manager_oscillator_config.h"
#if defined(FREQPLAN_PRESENT)
#include "sli_clock_manager_init_hal_freqplan.h"
#endif
#endif
#if defined(SL_CATALOG_POWER_MANAGER_PRESENT)
#include "sli_sleeptimer.h"
#include "sl_power_manager.h"
#endif

/*******************************************************************************
 *******************************   DEFINES   ***********************************
 ******************************************************************************/

#define CLOCK_MANAGER_SOCPLL_FREQ_PRECISION_PPM   41U
#define CLOCK_MANAGER_PERPLL_FREQ_PRECISION_PPM   3U

#define HFXO_IRQ_NUMBER  HFXO0_IRQn
#define HFXO_IRQ_HANDLER_FUNCTION  HFXO0_IRQHandler

#if defined(_HFXO_BUFOUTCTRL_MASK)
#define HFXO_CTUNE_DELTA 40
#else
#define HFXO_CTUNE_DELTA 0
#endif

#if defined(SOCPLL_COUNT) && (SOCPLL_COUNT == 1)
#define SOCPLL_OUTPUT_COUNT 1
#else
#define SOCPLL_OUTPUT_COUNT 3
#endif

#if defined(SL_CATALOG_CLOCK_MANAGER_PERPLL1_RUNTIME_CONFIG_PRESENT)
#define PERPLL_RUNTIME_CONFIG_PRESENT
#define PERPLL_RUNTIME_CONFIG_INSTANCE    1
#define PERPLL_RUNTIME_CONFIG_BUS_CLOCK   SL_BUS_CLOCK_PERPLL1

// Resolution of the PERPLL fractional divider.
#define PERPLL_DIVF_RESOLUTION    16384UL

// Maximum value of each PERPLL divider field.
#define PERPLL_DIVN_MAX           (_PERPLL_CTRL_PERPLLDIVN_MASK >> _PERPLL_CTRL_PERPLLDIVN_SHIFT)
#define PERPLL_DIVF_MAX           (_PERPLL_CTRL_PERPLLDIVF_MASK >> _PERPLL_CTRL_PERPLLDIVF_SHIFT)
#define PERPLL_DCO_DIV_MAX        (_PERPLL_DCOCFG_PERPLLDCOOUTDIV_MASK >> _PERPLL_DCOCFG_PERPLLDCOOUTDIV_SHIFT)
#define PERPLL_DIV_2POW_MAX       (_PERPLL_DCOCFG_PERPLLOUTDIV2POW_MASK >> _PERPLL_DCOCFG_PERPLLOUTDIV2POW_SHIFT)
#endif

/*******************************************************************************
 **************************   GLOBAL VARIABLES   *******************************
 ******************************************************************************/

// LFXO precision.
uint16_t clock_manager_lfxo_precision = 0;
// HFXO precision.
uint16_t clock_manager_hfxo_precision = 0;

#if defined (_SILICON_LABS_32B_SERIES_3_CONFIG_301)
// FLPLL configurations.
sli_se_qspi_flpll_config_t clock_manager_flpll_cfg = {
  .ref_clock = SLI_SE_QSPI_REF_CLOCK_HFRCODPLL,
};
sl_se_command_context_t clock_manager_cmd_ctx;
// Device SE FW version
uint32_t clock_manager_se_version = 0;
// Current QSPI reference clock.
sl_oscillator_t current_qspi_reference_clock = SL_OSCILLATOR_INVALID;
#endif

// HFXO startup time.
#if defined(SL_CLOCK_MANAGER_HFXO_STARTUP_TIME_MEASUREMENT_EN) && SL_CLOCK_MANAGER_HFXO_STARTUP_TIME_MEASUREMENT_EN
uint16_t clock_manager_hfxo_startup_time = 0;
#endif

#if defined(PERPLL_PRESENT) && defined(SL_CLOCK_MANAGER_HFXO_FREQ)
const uint32_t sli_clock_manager_perpll_freq_table[SLI_CLOCK_MANAGER_PERPLL_FREQ_COUNT]
[SLI_CLOCK_MANAGER_PERPLL_FIELD_COUNT] = {
  // Target frequency, DIVN, DIVF, DIVDCO, DIV2POW.
#if (SL_CLOCK_MANAGER_HFXO_FREQ == 38000000UL)
  { 14112000, 14, 5574, 20, 0 }, { 15360000, 14, 2759, 18, 0 }, { 16384000, 13, 8547, 16, 0 }, { 16934400, 14, 706, 16, 0 },
  { 18432000, 13, 8547, 14, 0 }, { 20000000, 13, 12935, 13, 0 }, { 20480000, 14, 2759, 13, 0 }, { 22579200, 14, 10442, 12, 0 },
  { 24576000, 13, 8547, 10, 0 }, { 40000000, 14, 13798, 2, 1 }, { 48000000, 15, 11210, 5, 0 }, { 50000000, 13, 12935, 4, 0 }
#elif (SL_CLOCK_MANAGER_HFXO_FREQ == 38400000UL)
  { 14112000, 14, 2785, 20, 0 }, { 15360000, 14, 0, 18, 0 }, { 16384000, 13, 5898, 16, 0 }, { 16934400, 13, 14352, 16, 0 },
  { 18432000, 13, 5898, 14, 0 }, { 20000000, 13, 10240, 13, 0 }, { 20480000, 14, 0, 13, 0 }, { 22579200, 14, 7602, 12, 0 },
  { 24576000, 13, 5898, 10, 0 }, { 40000000, 14, 10924, 2, 1 }, { 48000000, 15, 8192, 5, 0 }, { 50000000, 13, 10240, 4, 0 }
#elif (SL_CLOCK_MANAGER_HFXO_FREQ == 39000000UL)
  { 14112000, 14, 10569, 21, 0 }, { 15360000, 14, 9000, 19, 0 }, { 16384000, 13, 2029, 16, 0 }, { 16934400, 13, 10351, 16, 0 },
  { 18432000, 14, 1130, 15, 0 }, { 20000000, 13, 6310, 13, 0 }, { 20480000, 13, 12400, 13, 0 }, { 22579200, 14, 3455, 12, 0 },
  { 24576000, 13, 2030, 10, 0 }, { 40000000, 14, 6725, 2, 1 }, { 48000000, 12, 12900, 4, 0 }, { 50000000, 13, 6302, 4, 0 }
#elif (SL_CLOCK_MANAGER_HFXO_FREQ == 40000000UL)
  { 14112000, 14, 3800, 21, 0 }, { 15360000, 14, 2200, 19, 0 }, { 16384000, 12, 12220, 16, 0 }, { 16934400, 13, 3950, 16, 0 },
  { 18432000, 13, 10935, 15, 0 }, { 20000000, 13, 0, 13, 0 }, { 20480000, 13, 6000, 13, 0 }, { 22579200, 14, 3453, 12, 0 },
  { 24576000, 12, 12100, 10, 0 }, { 40000000, 14, 0, 2, 1 }, { 48000000, 12, 6600, 4, 0 }, { 50000000, 13, 0, 4, 0 }
#else
#error "PERPLL predefined frequencies require SL_CLOCK_MANAGER_HFXO_FREQ of 38, 38.4, 39, or 40 MHz."
#endif
};
#endif

/*******************************************************************************
 ***************************   LOCAL FUNCTIONS   *******************************
 ******************************************************************************/

SL_CODE_CLASSIFY(SL_CODE_COMPONENT_CLOCK_MANAGER, SL_CODE_CLASS_TIME_CRITICAL)
static sl_status_t bus_clock_get_register_info(sl_bus_clock_t module,
                                               volatile uint32_t **reg,
                                               uint32_t *bit);
static sl_status_t get_high_frequency_branch_precision(sl_clock_branch_t clock_branch,
                                                       uint16_t *precision);
static sl_status_t get_low_frequency_branch_precision(sl_clock_branch_t clock_branch,
                                                      uint16_t *precision);
static sl_status_t get_mixed_frequency_branch_precision(sl_clock_branch_t clock_branch,
                                                   uint16_t *precision);
static sl_status_t get_reference_clock_branch_precision(sl_clock_branch_t clock_branch,
                                                        uint16_t *precision);

#if defined(_SILICON_LABS_32B_SERIES_3_CONFIG_301) \
  || defined(_SILICON_LABS_32B_SERIES_3_CONFIG_353)
static bool ext_flash_clk_source_depends_on_hfxo(sl_oscillator_t source)
{
  switch (source) {
    case SL_OSCILLATOR_HFXO:
      return true;

#if defined(_SILICON_LABS_32B_SERIES_3_CONFIG_301)
    case SL_OSCILLATOR_FLPLL:
      return true;
#endif

#if defined(_SILICON_LABS_32B_SERIES_3_CONFIG_353)
    case SL_OSCILLATOR_HFRCODPLL:
      return ((DPLL0->STATUS & DPLL_STATUS_ENS) != 0)
             && ((CMU->DPLLREFCLKCTRL & _CMU_DPLLREFCLKCTRL_CLKSEL_MASK)
                 == CMU_DPLLREFCLKCTRL_CLKSEL_HFXO);

    case SL_OSCILLATOR_SOCPLL0:
    case SL_OSCILLATOR_SOCPLL0_OUT0:
      {
        uint32_t refclksel = SOCPLL0->CTRL & _SOCPLL_CTRL_REFCLKSEL_MASK;
        return (refclksel == SOCPLL_CTRL_REFCLKSEL_REF_HFXO)
               || (refclksel == SOCPLL_CTRL_REFCLKSEL_DEFAULT_HFXO);
      }
#endif

    default:
      return false;
  }
}

/***************************************************************************//**
 * Switches the external flash clock for HFXO tuning.
 ******************************************************************************/
static sl_status_t hfxo_tuning_set_flash_clk(sl_oscillator_t oscillator)
{
#if defined(_SILICON_LABS_32B_SERIES_3_CONFIG_301)
  return sli_clock_manager_hal_set_ext_flash_clk(oscillator);
#else
  uint32_t clksel;
  CORE_DECLARE_IRQ_STATE;

  switch (oscillator) {
    case SL_OSCILLATOR_FSRCO:
      clksel = CMU_OSPI0CLKCTRL_CLKSEL_FSRCO;
      break;

    case SL_OSCILLATOR_HFRCODPLL:
      clksel = CMU_OSPI0CLKCTRL_CLKSEL_HFRCODPLL;
      break;

    case SL_OSCILLATOR_HFXO:
      clksel = CMU_OSPI0CLKCTRL_CLKSEL_HFXO;
      break;

    case SL_OSCILLATOR_SOCPLL0:
    case SL_OSCILLATOR_SOCPLL0_OUT0:
      clksel = CMU_OSPI0CLKCTRL_CLKSEL_SOCPLL0;
      break;

    default:
      return SL_STATUS_INVALID_PARAMETER;
  }

  CORE_ENTER_ATOMIC();
  CMU->OSPI0CLKCTRL = (CMU->OSPI0CLKCTRL & ~_CMU_OSPI0CLKCTRL_CLKSEL_MASK)
                      | clksel;
  CORE_EXIT_ATOMIC();

  return SL_STATUS_OK;
#endif
}

/***************************************************************************//**
 * Saves the external flash clock source and switches it to FSRCO when the
 * source depends on HFXO.
 ******************************************************************************/
static sl_status_t hfxo_tuning_prepare_flash(sl_oscillator_t *ext_flash_source)
{
  sl_status_t status;

  status = sli_clock_manager_hal_get_ext_flash_clk(ext_flash_source);
  if ((status == SL_STATUS_OK)
      && ext_flash_clk_source_depends_on_hfxo(*ext_flash_source)) {
    status = hfxo_tuning_set_flash_clk(SL_OSCILLATOR_FSRCO);
  }
  return status;
}

/***************************************************************************//**
 * Restores the HFXO-dependent source saved by hfxo_tuning_prepare_flash().
 ******************************************************************************/
static sl_status_t hfxo_tuning_restore_flash(sl_oscillator_t ext_flash_source,
                                             uint32_t hfxo_ctrl)
{
  sl_status_t status = SL_STATUS_OK;

  if (!ext_flash_clk_source_depends_on_hfxo(ext_flash_source)) {
    return status;
  }

  uint32_t ready_mask = HFXO_STATUS_RDY | HFXO_STATUS_ENS;
#if defined(_SILICON_LABS_32B_SERIES_3_CONFIG_353)
  bool socpll0_temp_forceen = false;
#endif

  if ((hfxo_ctrl & HFXO_CTRL_FORCEEN) == 0) {
    HFXO0->CTRL_SET = HFXO_CTRL_FORCEEN;
  }
  if ((hfxo_ctrl & HFXO_CTRL_DISONDEMAND) == 0) {
    HFXO0->CTRL_CLR = HFXO_CTRL_DISONDEMAND;
  }

  while ((HFXO0->STATUS & ready_mask) != ready_mask) {
    /* Wait for HFXO to become ready before restoring the flash clock source. */
  }

#if defined(_SILICON_LABS_32B_SERIES_3_CONFIG_353)
  if (((ext_flash_source == SL_OSCILLATOR_SOCPLL0)
       || (ext_flash_source == SL_OSCILLATOR_SOCPLL0_OUT0))
      && ((SOCPLL0->STATUS & SOCPLL_STATUS_RDY) == 0)) {
    // OSPI0 can be SOCPLL0's only consumer. Force it on until it is ready so
    // the flash clock can be switched back to SOCPLL0.
    SOCPLL0->CTRL_SET = SOCPLL_CTRL_FORCEEN;
    while ((SOCPLL0->STATUS & SOCPLL_STATUS_RDY) == 0) {
      /* Wait for SOCPLL0 to become ready before selecting it for the flash clock. */
    }
    socpll0_temp_forceen = true;
  }
#endif

  status = hfxo_tuning_set_flash_clk(ext_flash_source);
  if (status == SL_STATUS_OK) {
    while ((HFXO0->STATUS & ready_mask) != ready_mask) {
      /* Keep HFXO running while the restored flash clock source stabilizes. */
    }
  }

#if defined(_SILICON_LABS_32B_SERIES_3_CONFIG_353)
  if (socpll0_temp_forceen) {
    SOCPLL0->CTRL_CLR = SOCPLL_CTRL_FORCEEN;
  }
#endif

  if ((hfxo_ctrl & HFXO_CTRL_FORCEEN) == 0) {
    HFXO0->CTRL_CLR = HFXO_CTRL_FORCEEN;
  }
  return status;
}
#endif

/*******************************************************************************
 **********************   GLOBAL INTERNAL FUNCTIONS   **************************
 ******************************************************************************/

/***************************************************************************//**
 * Performs Clock Manager runtime initialization.
 ******************************************************************************/
sl_status_t sli_clock_manager_hal_runtime_init(void)
{
  // Enable HFXO bus clock.
  sl_clock_manager_enable_bus_clock(SL_BUS_CLOCK_HFXO0);

  // Set the external FLASH reference clock to FLPLL with HFXO.
#if defined (_SILICON_LABS_32B_SERIES_3_CONFIG_301)
  sli_clock_manager_hal_set_ext_flash_clk(SL_OSCILLATOR_FLPLL);
#endif

  // Enable HFXO RDY Interrupt.
  HFXO0->IEN_CLR = HFXO_IEN_RDY;
  HFXO0->IF_CLR = HFXO_IF_RDY;

  NVIC_ClearPendingIRQ(HFXO_IRQ_NUMBER);
  NVIC_EnableIRQ(HFXO_IRQ_NUMBER);

#if !defined(SLI_SLEEPTIMER_SYSRTC_WITH_PRETRIGGERS)
  HFXO0->IEN_SET = HFXO_IEN_RDY;
#endif

  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Gets frequency of given oscillator.
 ******************************************************************************/
sl_status_t sli_clock_manager_hal_get_oscillator_frequency(sl_oscillator_t oscillator,
                                                           uint32_t *frequency)
{
  switch (oscillator) {
    case SL_OSCILLATOR_FSRCO:
      *frequency = SystemFSRCOClockGet();
      break;

    case SL_OSCILLATOR_HFXO:
      *frequency = SystemHFXOClockGet();
      break;

    case SL_OSCILLATOR_HFRCODPLL:
      *frequency = SystemHFRCODPLLClockGet();
      break;

    case SL_OSCILLATOR_HFRCOEM23:
      *frequency = SystemHFRCOEM23ClockGet();
      break;

    case SL_OSCILLATOR_LFXO:
      *frequency = SystemLFXOClockGet();
      break;

    case SL_OSCILLATOR_LFRCO:
      *frequency = SystemLFRCOClockGet();
      break;

    case SL_OSCILLATOR_ULFRCO:
      *frequency = SystemULFRCOClockGet();
      break;

#if (SOCPLL_OUTPUT_COUNT == 3)
    case SL_OSCILLATOR_SOCPLL0_OUT0:
      *frequency = SystemSOCPLLClockGet(0, 0);
      break;

    case SL_OSCILLATOR_SOCPLL0_OUT1:
      *frequency = SystemSOCPLLClockGet(0, 1);
      break;

    case SL_OSCILLATOR_SOCPLL0_OUT2:
      *frequency = SystemSOCPLLClockGet(0, 2);
      break;
#else
    case SL_OSCILLATOR_SOCPLL0:
      *frequency = SystemSOCPLLClockGet(0, 0);
      break;
#endif

#if defined(SOCPLL1)
    case SL_OSCILLATOR_SOCPLL1_OUT0:
      *frequency = SystemSOCPLLClockGet(1, 0);
      break;

    case SL_OSCILLATOR_SOCPLL1_OUT1:
      *frequency = SystemSOCPLLClockGet(1, 1);
      break;

    case SL_OSCILLATOR_SOCPLL1_OUT2:
      *frequency = SystemSOCPLLClockGet(1, 2);
      break;
#endif

#if defined(SOCPLL2)
    case SL_OSCILLATOR_SOCPLL2_OUT0:
      *frequency = SystemSOCPLLClockGet(2, 0);
      break;

    case SL_OSCILLATOR_SOCPLL2_OUT1:
      *frequency = SystemSOCPLLClockGet(2, 1);
      break;

    case SL_OSCILLATOR_SOCPLL2_OUT2:
      *frequency = SystemSOCPLLClockGet(2, 2);
      break;
#endif

#if defined(PERPLL_PRESENT)
    case SL_OSCILLATOR_PERPLL0:
      *frequency = SystemPERPLLClockGet(0);
      break;
#if (PERPLL_COUNT > 1)
    case SL_OSCILLATOR_PERPLL1:
      *frequency = SystemPERPLLClockGet(1);
      break;
#endif
#endif

    default:
      *frequency = 0U;
      return SL_STATUS_INVALID_PARAMETER;
  }

  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Gets precision of given oscillator.
 ******************************************************************************/
sl_status_t sli_clock_manager_hal_get_oscillator_precision(sl_oscillator_t oscillator,
                                                           uint16_t *precision)
{
  switch (oscillator) {
    case SL_OSCILLATOR_LFXO:
      // If precision is not set, LFXO has not been initialized
      if (clock_manager_lfxo_precision == 0) {
        return SL_STATUS_FAIL;
      }
      *precision = clock_manager_lfxo_precision;
      break;

    case SL_OSCILLATOR_HFXO:
      // If precision is not set, HFXO has not been initialized
      if (clock_manager_hfxo_precision == 0) {
        return SL_STATUS_FAIL;
      }
      *precision = clock_manager_hfxo_precision;
      break;

    case SL_OSCILLATOR_LFRCO:
      if ((LFRCO->CFG & _LFRCO_CFG_HIGHPRECEN_MASK) != 0) {
        *precision = 500;
      } else {
        *precision = 0xFFFF;
        return SL_STATUS_NOT_AVAILABLE;
      }
      break;

#if (SOCPLL_OUTPUT_COUNT == 3)
    case SL_OSCILLATOR_SOCPLL0_OUT0:
    case SL_OSCILLATOR_SOCPLL0_OUT1:
    case SL_OSCILLATOR_SOCPLL0_OUT2:
#else
    case SL_OSCILLATOR_SOCPLL0:
#endif
#if defined(SOCPLL1)
    case SL_OSCILLATOR_SOCPLL1_OUT0:
    case SL_OSCILLATOR_SOCPLL1_OUT1:
    case SL_OSCILLATOR_SOCPLL1_OUT2:
#endif
#if defined(SOCPLL2)
    case SL_OSCILLATOR_SOCPLL2_OUT0:
    case SL_OSCILLATOR_SOCPLL2_OUT1:
    case SL_OSCILLATOR_SOCPLL2_OUT2:
#endif
      *precision = CLOCK_MANAGER_SOCPLL_FREQ_PRECISION_PPM;
      break;

#if defined(PERPLL_PRESENT)
    case SL_OSCILLATOR_PERPLL0:
#if (PERPLL_COUNT > 1)
    case SL_OSCILLATOR_PERPLL1:
#endif
      *precision = CLOCK_MANAGER_PERPLL_FREQ_PRECISION_PPM;
      break;
#endif

    case SL_OSCILLATOR_FSRCO:
    case SL_OSCILLATOR_HFRCODPLL:
    case SL_OSCILLATOR_ULFRCO:
    case SL_OSCILLATOR_HFRCOEM23:
      *precision = 0xFFFF;
      return SL_STATUS_NOT_AVAILABLE;

    default:
      *precision = 0U;
      return SL_STATUS_INVALID_PARAMETER;
  }
  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Enables/Disables the given module's bus clock..
 ******************************************************************************/
sl_status_t sli_clock_manager_hal_enable_bus_clock(sl_bus_clock_t module, bool enable)
{
  sl_status_t status;
  volatile uint32_t *reg = NULL;
  uint32_t bit;

  // Get the register and bit for the bus clock.
  status = bus_clock_get_register_info(module, &reg, &bit);
  if (status != SL_STATUS_OK) {
    return status;
  }

  // Enable/disable bus clock.
  sl_hal_bus_reg_write_bit(reg, bit, (uint32_t)enable);

#if defined(_SILICON_LABS_32B_SERIES_3_CONFIG_353)
  // Read back value to ensure the operation is successful.
  uint32_t read_value = sl_hal_bus_reg_read_bit(reg, bit);
  if (read_value != enable) {
    return SL_STATUS_FAIL;
  }
#endif

  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Gets the enable status of the given module's bus clock.
 ******************************************************************************/
sl_status_t sli_clock_manager_hal_is_bus_clock_enabled(sl_bus_clock_t module,
                                                       bool *enabled)
{
  sl_status_t status;
  volatile uint32_t *reg = NULL;
  uint32_t bit;

  // Get the register and bit for the bus clock.
  status = bus_clock_get_register_info(module, &reg, &bit);
  if (status != SL_STATUS_OK) {
    return status;
  }

  // Get the enable status of the bus clock.
  *enabled = (bool)sl_hal_bus_reg_read_bit(reg, bit);

  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Configures the exported clock feature on CMU to output user selected
 * clock source specified GPIO pin.
 ******************************************************************************/
sl_status_t sli_clock_manager_hal_set_gpio_clock_output(sl_clock_manager_export_clock_source_t export_clock_source,
                                                        sl_clock_manager_export_clock_output_select_t output_select,
                                                        uint16_t divider,
                                                        uint32_t port,
                                                        uint32_t pin)
{
  uint32_t tmp = 0;
  uint32_t mask;
  CORE_DECLARE_IRQ_STATE;

  EFM_ASSERT(pin <= (_GPIO_CMU_CLKOUT0ROUTE_PIN_MASK >> _GPIO_CMU_CLKOUT0ROUTE_PIN_SHIFT));
  EFM_ASSERT(divider <= 32U);

  sl_gpio_t gpio;
  gpio.port = (sl_gpio_port_t)port;
  gpio.pin = (uint8_t)pin;

  switch (output_select) {
    case SL_CLOCK_MANAGER_EXPORT_CLOCK_OUTPUT_SELECT_0:
    case SL_CLOCK_MANAGER_EXPORT_CLOCK_OUTPUT_SELECT_1:
      EFM_ASSERT((port == 2U) || (port == 3U));
      break;
    case SL_CLOCK_MANAGER_EXPORT_CLOCK_OUTPUT_SELECT_2:
      EFM_ASSERT((port == 0U) || (port == 1U));
      break;
    default:
      EFM_ASSERT(false);
      break;
  }

  switch (export_clock_source) {
    case SL_CLOCK_MANAGER_EXPORT_CLOCK_SOURCE_DISABLED:
      tmp = CMU_EXPORTCLKCTRL_CLKOUTSEL0_DISABLED;
      break;

    case SL_CLOCK_MANAGER_EXPORT_CLOCK_SOURCE_FSRCO:
      tmp = CMU_EXPORTCLKCTRL_CLKOUTSEL0_FSRCO;
      break;

    case SL_CLOCK_MANAGER_EXPORT_CLOCK_SOURCE_HFXO:
      tmp = CMU_EXPORTCLKCTRL_CLKOUTSEL0_HFXO;
      break;

    case SL_CLOCK_MANAGER_EXPORT_CLOCK_SOURCE_HFRCODPLL:
      tmp = CMU_EXPORTCLKCTRL_CLKOUTSEL0_HFRCODPLL;
      break;

    case SL_CLOCK_MANAGER_EXPORT_CLOCK_SOURCE_HFRCOEM23:
      tmp = CMU_EXPORTCLKCTRL_CLKOUTSEL0_HFRCOEM23;
      break;

    case SL_CLOCK_MANAGER_EXPORT_CLOCK_SOURCE_HFEXPCLK:
      EFM_ASSERT(divider > 0U);
      tmp  = CMU_EXPORTCLKCTRL_CLKOUTSEL0_HFEXPCLK;
      break;

    case SL_CLOCK_MANAGER_EXPORT_CLOCK_SOURCE_LFXO:
      tmp = CMU_EXPORTCLKCTRL_CLKOUTSEL0_LFXO;
      break;

    case SL_CLOCK_MANAGER_EXPORT_CLOCK_SOURCE_LFRCO:
      tmp = CMU_EXPORTCLKCTRL_CLKOUTSEL0_LFRCO;
      break;

    case SL_CLOCK_MANAGER_EXPORT_CLOCK_SOURCE_ULFRCO:
      tmp = CMU_EXPORTCLKCTRL_CLKOUTSEL0_ULFRCO;
      break;

    case SL_CLOCK_MANAGER_EXPORT_CLOCK_SOURCE_HCLK:
      tmp = CMU_EXPORTCLKCTRL_CLKOUTSEL0_HCLK;
      break;

#if defined(CMU_EXPORTCLKCTRL_CLKOUTSEL0_SOCPLL0)
    case SL_CLOCK_MANAGER_EXPORT_CLOCK_SOURCE_SOCPLL0:
      tmp = CMU_EXPORTCLKCTRL_CLKOUTSEL0_SOCPLL0;
      break;
#elif defined(CMU_EXPORTCLKCTRL_CLKOUTSEL0_SOCPLL)
    case SL_CLOCK_MANAGER_EXPORT_CLOCK_SOURCE_SOCPLL0:
      tmp = CMU_EXPORTCLKCTRL_CLKOUTSEL0_SOCPLL;
      break;
#endif

#if defined(CMU_EXPORTCLKCTRL_CLKOUTSEL0_SOCPLL1)
    case SL_CLOCK_MANAGER_EXPORT_CLOCK_SOURCE_SOCPLL1:
      tmp = CMU_EXPORTCLKCTRL_CLKOUTSEL0_SOCPLL1;
      break;
#endif

#if defined(CMU_EXPORTCLKCTRL_CLKOUTSEL0_SOCPLL2)
    case SL_CLOCK_MANAGER_EXPORT_CLOCK_SOURCE_SOCPLL2:
      tmp = CMU_EXPORTCLKCTRL_CLKOUTSEL0_SOCPLL2;
      break;
#endif

#if defined(CMU_EXPORTCLKCTRL_CLKOUTSEL0_PERPLL0)
    case SL_CLOCK_MANAGER_EXPORT_CLOCK_SOURCE_PERPLL0:
      tmp = CMU_EXPORTCLKCTRL_CLKOUTSEL0_PERPLL0;
      break;
#endif

#if defined(CMU_EXPORTCLKCTRL_CLKOUTSEL0_PERPLL1)
    case SL_CLOCK_MANAGER_EXPORT_CLOCK_SOURCE_PERPLL1:
      tmp = CMU_EXPORTCLKCTRL_CLKOUTSEL0_PERPLL1;
      break;
#endif

    default:
      return SL_STATUS_NOT_SUPPORTED;
  }

  mask  = _CMU_EXPORTCLKCTRL_CLKOUTSEL0_MASK << (output_select * _CMU_EXPORTCLKCTRL_CLKOUTSEL1_SHIFT);
  tmp <<= output_select * _CMU_EXPORTCLKCTRL_CLKOUTSEL1_SHIFT;

  if ((export_clock_source != SL_CLOCK_MANAGER_EXPORT_CLOCK_SOURCE_DISABLED) && (divider > 0U)) {
    if (export_clock_source == SL_CLOCK_MANAGER_EXPORT_CLOCK_SOURCE_HFEXPCLK) {
      tmp  |= (divider - 1U) << _CMU_EXPORTCLKCTRL_PRESC_SHIFT;
      mask |= _CMU_EXPORTCLKCTRL_PRESC_MASK;
    } else {
  #if defined(_CMU_EXPORTCLKCTRL_CLKOUTPRESC_MASK)
      tmp  |= (divider - 1U) << _CMU_EXPORTCLKCTRL_CLKOUTPRESC_SHIFT;
      mask |= _CMU_EXPORTCLKCTRL_CLKOUTPRESC_MASK;
  #endif
    }
  }

  CORE_ENTER_ATOMIC();

  CMU->EXPORTCLKCTRL = (CMU->EXPORTCLKCTRL & ~mask) | tmp;

  if (export_clock_source == SL_CLOCK_MANAGER_EXPORT_CLOCK_SOURCE_DISABLED) {
    GPIO->CMUROUTE_CLR.ROUTEEN = GPIO_CMU_ROUTEEN_CLKOUT0PEN << output_select;
    sl_gpio_set_pin_mode(&gpio, SL_GPIO_MODE_DISABLED, false);
  } else {
    GPIO->CMUROUTE_SET.ROUTEEN = GPIO_CMU_ROUTEEN_CLKOUT0PEN << output_select;
    if (output_select == SL_CLOCK_MANAGER_EXPORT_CLOCK_OUTPUT_SELECT_0) {
      GPIO->CMUROUTE.CLKOUT0ROUTE = (port << _GPIO_CMU_CLKOUT0ROUTE_PORT_SHIFT)
                                    | (pin << _GPIO_CMU_CLKOUT0ROUTE_PIN_SHIFT);
    } else if (output_select == SL_CLOCK_MANAGER_EXPORT_CLOCK_OUTPUT_SELECT_1) {
      GPIO->CMUROUTE.CLKOUT1ROUTE = (port << _GPIO_CMU_CLKOUT1ROUTE_PORT_SHIFT)
                                    | (pin << _GPIO_CMU_CLKOUT1ROUTE_PIN_SHIFT);
    } else {
      GPIO->CMUROUTE.CLKOUT2ROUTE = (port << _GPIO_CMU_CLKOUT2ROUTE_PORT_SHIFT)
                                    | (pin << _GPIO_CMU_CLKOUT2ROUTE_PIN_SHIFT);
    }
    sl_gpio_set_pin_mode(&gpio, SL_GPIO_MODE_PUSH_PULL, false);
  }

  CORE_EXIT_ATOMIC();

  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Gets frequency of given clock branch.
 ******************************************************************************/
sl_status_t sli_clock_manager_hal_get_clock_branch_frequency(sl_clock_branch_t clock_branch,
                                                             uint32_t *frequency)
{
  uint8_t pclk_divider;
  uint8_t per_divider;
  sl_status_t return_status = SL_STATUS_OK;
  CORE_DECLARE_IRQ_STATE;

  CORE_ENTER_ATOMIC();

  switch (clock_branch) {
    case SL_CLOCK_BRANCH_SYSCLK:
      *frequency = SystemSYSCLKGet();
      break;

#if defined(_CMU_SYSCLKCTRL_CPUCLKPRESC_MASK)
    case SL_CLOCK_BRANCH_CPUCLK:
      *frequency = SystemCPUCLKGet();
      break;
#endif

    case SL_CLOCK_BRANCH_HCLK:
      *frequency = SystemHCLKGet();
      break;

#if defined(_CMU_SYSCLKCTRL_HCLKDIVNPRESC_MASK)
    case SL_CLOCK_BRANCH_HCLKDIVN:
      *frequency = SystemHCLKDIVNGet();
      break;
#endif

    case SL_CLOCK_BRANCH_PCLK:
      pclk_divider = ((CMU->SYSCLKCTRL & _CMU_SYSCLKCTRL_PCLKPRESC_MASK) >> _CMU_SYSCLKCTRL_PCLKPRESC_SHIFT) + 1;
      *frequency = SystemHCLKGet() / pclk_divider;
      break;

    case SL_CLOCK_BRANCH_LSPCLK:
      pclk_divider = ((CMU->SYSCLKCTRL & _CMU_SYSCLKCTRL_PCLKPRESC_MASK) >> _CMU_SYSCLKCTRL_PCLKPRESC_SHIFT) + 1;
      *frequency = SystemHCLKGet() / pclk_divider / 2U;
      break;

#if defined(_CMU_TRACECLKCTRL_MASK)
    case SL_CLOCK_BRANCH_TRACECLK:
      per_divider = ((CMU->TRACECLKCTRL & _CMU_TRACECLKCTRL_PRESC_MASK) >> _CMU_TRACECLKCTRL_PRESC_SHIFT) + 1;
      switch (CMU->TRACECLKCTRL & _CMU_TRACECLKCTRL_CLKSEL_MASK) {
        case CMU_TRACECLKCTRL_CLKSEL_SYSCLK:
          *frequency = SystemSYSCLKGet() / per_divider;
          break;

        case CMU_TRACECLKCTRL_CLKSEL_HFRCOEM23:
          *frequency = SystemHFRCOEM23ClockGet() / per_divider;
          break;

#if defined(CMU_TRACECLKCTRL_CLKSEL_HFRCODPLL)
        case CMU_TRACECLKCTRL_CLKSEL_HFRCODPLL:
#endif
#if defined(CMU_TRACECLKCTRL_CLKSEL_HFRCODPLLRT)
        case CMU_TRACECLKCTRL_CLKSEL_HFRCODPLLRT:
#endif
          *frequency = SystemHFRCODPLLClockGet() / per_divider;
          break;

        default:
          *frequency = 0U;
          return_status = SL_STATUS_INVALID_STATE;
          break;
      }
      break;
#else
  (void)per_divider;
#endif

    case SL_CLOCK_BRANCH_EXPORTCLK:
      *frequency = SystemSYSCLKGet() / (((CMU->EXPORTCLKCTRL & _CMU_EXPORTCLKCTRL_PRESC_MASK) >> _CMU_EXPORTCLKCTRL_PRESC_SHIFT) + 1);
      break;

#if defined(_CMU_EM01GRPACLKCTRL_CLKSEL_MASK)
    case SL_CLOCK_BRANCH_EM01GRPACLK:
      switch (CMU->EM01GRPACLKCTRL & _CMU_EM01GRPACLKCTRL_CLKSEL_MASK) {
        case CMU_EM01GRPACLKCTRL_CLKSEL_HFRCODPLL:
#if defined(CMU_EM01GRPACLKCTRL_CLKSEL_HFRCODPLLRT)
        case CMU_EM01GRPACLKCTRL_CLKSEL_HFRCODPLLRT:
#endif
          *frequency = SystemHFRCODPLLClockGet();
          break;

        case CMU_EM01GRPACLKCTRL_CLKSEL_HFXO:
#if defined(CMU_EM01GRPACLKCTRL_CLKSEL_HFXORT)
        case CMU_EM01GRPACLKCTRL_CLKSEL_HFXORT:
#endif
          *frequency = SystemHFXOClockGet();
          break;

        case CMU_EM01GRPACLKCTRL_CLKSEL_HFRCOEM23:
          *frequency = SystemHFRCOEM23ClockGet();
          break;

        case CMU_EM01GRPACLKCTRL_CLKSEL_FSRCO:
          *frequency = SystemFSRCOClockGet();
          break;

        default:
          *frequency = 0U;
          return_status = SL_STATUS_INVALID_STATE;
      }
      break;
#endif

#if defined(_CMU_EM01GRPCCLKCTRL_CLKSEL_MASK)
    case SL_CLOCK_BRANCH_EM01GRPCCLK:
      switch (CMU->EM01GRPCCLKCTRL & _CMU_EM01GRPCCLKCTRL_CLKSEL_MASK) {
        case CMU_EM01GRPCCLKCTRL_CLKSEL_HFRCODPLL:
#if defined(CMU_EM01GRPCCLKCTRL_CLKSEL_HFRCODPLLRT)
        case CMU_EM01GRPCCLKCTRL_CLKSEL_HFRCODPLLRT:
#endif
          *frequency = SystemHFRCODPLLClockGet();
          break;

        case CMU_EM01GRPCCLKCTRL_CLKSEL_HFRCOEM23:
          *frequency = SystemHFRCOEM23ClockGet();
          break;

        case CMU_EM01GRPCCLKCTRL_CLKSEL_HFXO:
#if defined(CMU_EM01GRPCCLKCTRL_CLKSEL_HFXORT)
        case CMU_EM01GRPCCLKCTRL_CLKSEL_HFXORT:
#endif
          *frequency = SystemHFXOClockGet();
          break;

        case CMU_EM01GRPCCLKCTRL_CLKSEL_FSRCO:
          *frequency = SystemFSRCOClockGet();
          break;

        default:
          *frequency = 0U;
          return_status = SL_STATUS_INVALID_STATE;
          break;
      }
      break;
#endif

#if defined(_CMU_EM23GRPACLKCTRL_CLKSEL_MASK)
    case SL_CLOCK_BRANCH_EM23GRPACLK:
      switch (CMU->EM23GRPACLKCTRL & _CMU_EM23GRPACLKCTRL_CLKSEL_MASK) {
        case CMU_EM23GRPACLKCTRL_CLKSEL_LFRCO:
          *frequency = SystemLFRCOClockGet();
          break;

        case CMU_EM23GRPACLKCTRL_CLKSEL_LFXO:
          *frequency = SystemLFXOClockGet();
          break;

        case CMU_EM23GRPACLKCTRL_CLKSEL_ULFRCO:
          *frequency = SystemULFRCOClockGet();
          break;

        default:
          *frequency = 0U;
          return_status = SL_STATUS_INVALID_STATE;
          break;
      }
      break;
#endif

#if defined(_CMU_EM4GRPACLKCTRL_CLKSEL_MASK)
    case SL_CLOCK_BRANCH_EM4GRPACLK:
      switch (CMU->EM4GRPACLKCTRL & _CMU_EM4GRPACLKCTRL_CLKSEL_MASK) {
        case CMU_EM4GRPACLKCTRL_CLKSEL_LFRCO:
          *frequency = SystemLFRCOClockGet();
          break;

        case CMU_EM4GRPACLKCTRL_CLKSEL_LFXO:
          *frequency = SystemLFXOClockGet();
          break;

        case CMU_EM4GRPACLKCTRL_CLKSEL_ULFRCO:
          *frequency = SystemULFRCOClockGet();
          break;

        default:
          *frequency = 0U;
          return_status = SL_STATUS_INVALID_STATE;
          break;
      }
      break;
#endif

    case SL_CLOCK_BRANCH_WDOG0CLK:
      switch (CMU->WDOG0CLKCTRL & _CMU_WDOG0CLKCTRL_CLKSEL_MASK) {
        case CMU_WDOG0CLKCTRL_CLKSEL_LFRCO:
          *frequency = SystemLFRCOClockGet();
          break;

        case CMU_WDOG0CLKCTRL_CLKSEL_LFXO:
          *frequency = SystemLFXOClockGet();
          break;

        case CMU_WDOG0CLKCTRL_CLKSEL_ULFRCO:
          *frequency = SystemULFRCOClockGet();
          break;

#if defined(CMU_WDOG0CLKCTRL_CLKSEL_HCLKDIV1024)
        case CMU_WDOG0CLKCTRL_CLKSEL_HCLKDIV1024:
#if defined(_CMU_SYSCLKCTRL_HCLKDIVNPRESC_MASK)
          *frequency = SystemHCLKDIVNGet();
#else
          *frequency = SystemHCLKGet() / 1024U;
#endif
          break;
#endif

        default:
          *frequency = 0U;
          return_status = SL_STATUS_INVALID_STATE;
          break;
      }
      break;

    case SL_CLOCK_BRANCH_WDOG1CLK:
#if defined(_CMU_WDOG1CLKCTRL_CLKSEL_MASK)
      switch (CMU->WDOG1CLKCTRL & _CMU_WDOG1CLKCTRL_CLKSEL_MASK) {
        case CMU_WDOG1CLKCTRL_CLKSEL_LFRCO:
          *frequency = SystemLFRCOClockGet();
          break;

        case CMU_WDOG1CLKCTRL_CLKSEL_LFXO:
          *frequency = SystemLFXOClockGet();
          break;

        case CMU_WDOG1CLKCTRL_CLKSEL_ULFRCO:
          *frequency = SystemULFRCOClockGet();
          break;

#if defined(CMU_WDOG1CLKCTRL_CLKSEL_HCLKDIV1024)
        case CMU_WDOG1CLKCTRL_CLKSEL_HCLKDIV1024:
#if defined(_CMU_SYSCLKCTRL_HCLKDIVNPRESC_MASK)
          *frequency = SystemHCLKDIVNGet();
#else
          *frequency = SystemHCLKGet() / 1024U;
#endif
          break;
#endif

        default:
          *frequency = 0U;
          return_status = SL_STATUS_INVALID_STATE;
          break;
      }
      break;
#else
      *frequency = SystemHCLKGet() / 1024U;
      break;
#endif

    case SL_CLOCK_BRANCH_SYSRTCCLK:
      switch (CMU->SYSRTC0CLKCTRL & _CMU_SYSRTC0CLKCTRL_CLKSEL_MASK) {
        case CMU_SYSRTC0CLKCTRL_CLKSEL_LFRCO:
          *frequency = SystemLFRCOClockGet();
          break;

        case CMU_SYSRTC0CLKCTRL_CLKSEL_LFXO:
          *frequency = SystemLFXOClockGet();
          break;

        case CMU_SYSRTC0CLKCTRL_CLKSEL_ULFRCO:
          *frequency = SystemULFRCOClockGet();
          break;

        default:
          *frequency = 0U;
          return_status = SL_STATUS_INVALID_STATE;
          break;
      }
      break;

    case SL_CLOCK_BRANCH_EUSART0CLK:
      switch (CMU->EUSART0CLKCTRL & _CMU_EUSART0CLKCTRL_CLKSEL_MASK) {
#if defined(CMU_EUSART0CLKCTRL_CLKSEL_EM01GRPCCLK)
        case CMU_EUSART0CLKCTRL_CLKSEL_EM01GRPCCLK:
          return_status =  sli_clock_manager_hal_get_clock_branch_frequency(SL_CLOCK_BRANCH_EM01GRPCCLK, frequency);
          break;
#endif
#if defined(CMU_EUSART0CLKCTRL_CLKSEL_FSRCO)
        case CMU_EUSART0CLKCTRL_CLKSEL_FSRCO:
          *frequency = SystemFSRCOClockGet();
          break;
#endif
        case CMU_EUSART0CLKCTRL_CLKSEL_HFRCOEM23:
          *frequency = SystemHFRCOEM23ClockGet();
          break;

        case CMU_EUSART0CLKCTRL_CLKSEL_LFRCO:
          *frequency = SystemLFRCOClockGet();
          break;

        case CMU_EUSART0CLKCTRL_CLKSEL_LFXO:
          *frequency = SystemLFXOClockGet();
          break;

        default:
          *frequency = 0U;
          return_status = SL_STATUS_INVALID_STATE;
          break;
      }
      break;

#if defined(_CMU_EUSART1CLKCTRL_MASK)
    case SL_CLOCK_BRANCH_EUSART1CLK:
      switch (CMU->EUSART1CLKCTRL & _CMU_EUSART1CLKCTRL_CLKSEL_MASK) {
#if defined(_CMU_EUSART1CLKCTRL_CLKSEL_EM01GRPCCLK)
        case CMU_EUSART1CLKCTRL_CLKSEL_EM01GRPCCLK:
          return_status =  sli_clock_manager_hal_get_clock_branch_frequency(SL_CLOCK_BRANCH_EM01GRPCCLK, frequency);
          break;
#endif
#if defined(CMU_EUSART1CLKCTRL_CLKSEL_FSRCO)
        case CMU_EUSART1CLKCTRL_CLKSEL_FSRCO:
          *frequency = SystemFSRCOClockGet();
          break;
#endif
        case CMU_EUSART1CLKCTRL_CLKSEL_HFRCOEM23:
          *frequency = SystemHFRCOEM23ClockGet();
          break;

        case CMU_EUSART1CLKCTRL_CLKSEL_LFRCO:
          *frequency = SystemLFRCOClockGet();
          break;

        case CMU_EUSART1CLKCTRL_CLKSEL_LFXO:
          *frequency = SystemLFXOClockGet();
          break;

        default:
          *frequency = 0U;
          return_status = SL_STATUS_INVALID_STATE;
          break;
      }
      break;
  #endif

    case SL_CLOCK_BRANCH_PCNT0CLK:
      switch (CMU->PCNT0CLKCTRL & _CMU_PCNT0CLKCTRL_CLKSEL_MASK) {
#if defined(_CMU_PCNT0CLKCTRL_CLKSEL_EM23GRPACLK)
        case CMU_PCNT0CLKCTRL_CLKSEL_EM23GRPACLK:
          return_status = sli_clock_manager_hal_get_clock_branch_frequency(SL_CLOCK_BRANCH_EM23GRPACLK, frequency);
          break;
#endif
        case CMU_PCNT0CLKCTRL_CLKSEL_PCNTS0:
          *frequency = 0U;
          return_status = SL_STATUS_NOT_SUPPORTED;
          break;

        default:
          *frequency = 0U;
          return_status = SL_STATUS_INVALID_STATE;
          break;
      }
      break;

#if defined(_CMU_EM01GRPDCLKCTRL_CLKSEL_MASK)
    case SL_CLOCK_BRANCH_EM01GRPDCLK:
      switch (CMU->EM01GRPDCLKCTRL & _CMU_EM01GRPDCLKCTRL_CLKSEL_MASK) {
        case CMU_EM01GRPDCLKCTRL_CLKSEL_HFRCODPLL:
          *frequency = SystemHFRCODPLLClockGet();
          break;

        case CMU_EM01GRPDCLKCTRL_CLKSEL_HFXO:
          *frequency = SystemHFXOClockGet();
          break;

        case CMU_EM01GRPDCLKCTRL_CLKSEL_FSRCO:
          *frequency = SystemFSRCOClockGet();
          break;

        case CMU_EM01GRPDCLKCTRL_CLKSEL_HFRCOEM23:
          *frequency = SystemHFRCOEM23ClockGet();
          break;

        default:
          *frequency = 0U;
          return_status = SL_STATUS_INVALID_STATE;
          break;
      }
      break;
#endif

    case SL_CLOCK_BRANCH_I2C0CLK:
      switch (CMU->I2C0CLKCTRL & _CMU_I2C0CLKCTRL_CLKSEL_MASK) {
#if defined(_CMU_I2C0CLKCTRL_CLKSEL_EM01GRPDCLK)
        case CMU_I2C0CLKCTRL_CLKSEL_EM01GRPDCLK:
          return_status = sli_clock_manager_hal_get_clock_branch_frequency(SL_CLOCK_BRANCH_EM01GRPDCLK, frequency);
          break;
#endif
        case CMU_I2C0CLKCTRL_CLKSEL_HFRCOEM23:
          *frequency = SystemHFRCOEM23ClockGet();
          break;

        case CMU_I2C0CLKCTRL_CLKSEL_LFRCO:
          *frequency = SystemLFRCOClockGet();
          break;

        case CMU_I2C0CLKCTRL_CLKSEL_LFXO:
          *frequency = SystemLFXOClockGet();
          break;

        default:
          *frequency = 0U;
          return_status = SL_STATUS_INVALID_STATE;
          break;
      }
      break;

#if defined(_CMU_PIXELRZCLKCTRL_MASK)
    case SL_CLOCK_BRANCH_PIXELRZCLK:
      per_divider = ((CMU->PIXELRZCLKCTRL & _CMU_PIXELRZCLKCTRL_PRESC_MASK) >> _CMU_PIXELRZCLKCTRL_PRESC_SHIFT) + 1;
      switch (CMU->PIXELRZCLKCTRL & _CMU_PIXELRZCLKCTRL_CLKSEL_MASK) {
        case CMU_PIXELRZCLKCTRL_CLKSEL_HFRCODPLL:
          *frequency = SystemHFRCODPLLClockGet() / per_divider;
          break;

        case CMU_PIXELRZCLKCTRL_CLKSEL_HFXO:
          *frequency = SystemHFXOClockGet() / per_divider;
          break;

        case CMU_PIXELRZCLKCTRL_CLKSEL_FSRCO:
          *frequency = SystemFSRCOClockGet() / per_divider;
          break;

        case CMU_PIXELRZCLKCTRL_CLKSEL_HFRCOEM23:
          *frequency = SystemHFRCOEM23ClockGet() / per_divider;
          break;

        default:
          *frequency = 0U;
          return_status = SL_STATUS_INVALID_STATE;
          break;
      }
      break;
#endif

#if defined(_CMU_ADCCLKCTRL_CLKSEL_MASK)
    case SL_CLOCK_BRANCH_ADCCLK:
      switch (CMU->ADCCLKCTRL & _CMU_ADCCLKCTRL_CLKSEL_MASK) {
        case CMU_ADCCLKCTRL_CLKSEL_EM01GRPACLK:
          return_status =  sli_clock_manager_hal_get_clock_branch_frequency(SL_CLOCK_BRANCH_EM01GRPACLK, frequency);
          break;

        case CMU_ADCCLKCTRL_CLKSEL_FSRCO:
          *frequency = SystemFSRCOClockGet();
          break;

        case CMU_ADCCLKCTRL_CLKSEL_HFRCOEM23:
          *frequency = SystemHFRCOEM23ClockGet();
          break;

        default:
          *frequency = 0U;
          return_status =  SL_STATUS_INVALID_STATE;
          break;
      }
      break;
#endif

    case SL_CLOCK_BRANCH_SYSTICKCLK:
      if (SysTick->CTRL & SysTick_CTRL_CLKSOURCE_Msk) {
        // Systick connected to processor clock when CLKSOURCE bit is set: HCLK
        return_status = sli_clock_manager_hal_get_clock_branch_frequency(SL_CLOCK_BRANCH_HCLK, frequency);
      } else {
        // Systick connected to external clock when CLKSOURCE bit is clear: EM23GRPACLK
        return_status = sli_clock_manager_hal_get_clock_branch_frequency(SL_CLOCK_BRANCH_EM23GRPACLK, frequency);
      }
      break;

#if defined(_CMU_VDAC0CLKCTRL_MASK)
    case SL_CLOCK_BRANCH_VDAC0CLK:
      switch (CMU->VDAC0CLKCTRL & _CMU_VDAC0CLKCTRL_CLKSEL_MASK) {
#if defined(_CMU_VDAC0CLKCTRL_CLKSEL_EM01GRPACLK)
        case CMU_VDAC0CLKCTRL_CLKSEL_EM01GRPACLK:
          return_status = sli_clock_manager_hal_get_clock_branch_frequency(SL_CLOCK_BRANCH_EM01GRPACLK, frequency);
          break;
#endif
#if defined(_CMU_VDAC0CLKCTRL_CLKSEL_EM23GRPACLK)
        case CMU_VDAC0CLKCTRL_CLKSEL_EM23GRPACLK:
          return_status = sli_clock_manager_hal_get_clock_branch_frequency(SL_CLOCK_BRANCH_EM23GRPACLK, frequency);
          break;
#endif
        case CMU_VDAC0CLKCTRL_CLKSEL_FSRCO:
          *frequency = SystemFSRCOClockGet();
          break;

        case CMU_VDAC0CLKCTRL_CLKSEL_HFRCOEM23:
          *frequency = SystemHFRCOEM23ClockGet();
          break;

        default:
          *frequency = 0U;
          return_status =  SL_STATUS_INVALID_STATE;
          break;
      }
      break;
#endif

    case SL_CLOCK_BRANCH_DPLLREFCLK:
      switch (CMU->DPLLREFCLKCTRL & _CMU_DPLLREFCLKCTRL_CLKSEL_MASK) {
        case CMU_DPLLREFCLKCTRL_CLKSEL_HFXO:
          *frequency = SystemHFXOClockGet();
          break;

        case CMU_DPLLREFCLKCTRL_CLKSEL_LFXO:
          *frequency = SystemLFXOClockGet();
          break;

        case CMU_DPLLREFCLKCTRL_CLKSEL_CLKIN0:
          *frequency = SystemCLKIN0Get();
          break;

        default:
          *frequency = 0U;
          return_status =  SL_STATUS_INVALID_STATE;
          break;
      }
      break;

#if defined(_CMU_QSPISYSCLKCTRL_MASK)
    case SL_CLOCK_BRANCH_QSPISYSCLK:
      per_divider = ((CMU->QSPISYSCLKCTRL & _CMU_QSPISYSCLKCTRL_PRESC_MASK) >> _CMU_QSPISYSCLKCTRL_PRESC_SHIFT) + 1;
      switch (CMU->QSPISYSCLKCTRL & _CMU_QSPISYSCLKCTRL_CLKSEL_MASK) {
        case CMU_QSPISYSCLKCTRL_CLKSEL_HFRCODPLL:
          *frequency = SystemHFRCODPLLClockGet() / per_divider;
          break;

        case CMU_QSPISYSCLKCTRL_CLKSEL_HFXO:
          *frequency = SystemHFXOClockGet() / per_divider;
          break;

        case CMU_QSPISYSCLKCTRL_CLKSEL_SOCPLL:
          *frequency = SystemSOCPLLClockGet(0, 0) / per_divider;
          break;

        default:
          *frequency = 0U;
          (void)per_divider;
          return_status =  SL_STATUS_INVALID_STATE;
          break;
      }
      break;
#endif

#if defined(_CMU_FLPLLREFCLKCTRL_MASK)
    case SL_CLOCK_BRANCH_FLPLLREFCLK:
      per_divider = ((CMU->FLPLLREFCLKCTRL & _CMU_FLPLLREFCLKCTRL_PRESC_MASK) >> _CMU_FLPLLREFCLKCTRL_PRESC_SHIFT) + 1;
      switch (CMU->FLPLLREFCLKCTRL & _CMU_FLPLLREFCLKCTRL_CLKSEL_MASK) {
        case CMU_FLPLLREFCLKCTRL_CLKSEL_HFRCODPLLRT:
          *frequency = SystemHFRCODPLLClockGet() / per_divider;
          break;

        case CMU_FLPLLREFCLKCTRL_CLKSEL_CLKIN0:
          *frequency = SystemCLKIN0Get() / per_divider;
          break;

        default:
          *frequency = 0U;
          (void)per_divider;
          return_status =  SL_STATUS_INVALID_STATE;
          break;
      }
      break;
#endif

    default:
#if defined(SL_CATALOG_CLOCK_MANAGER_RUNTIME_HAL_INTERNAL_PRESENT)
      return_status = sli_clock_manager_hal_get_clock_branch_frequency_internal(clock_branch, frequency);
#else
      *frequency = 0U;
      return_status = SL_STATUS_INVALID_PARAMETER;
#endif
      break;
  }

  CORE_EXIT_ATOMIC();

  return return_status;
}

static sl_status_t get_high_frequency_branch_precision(sl_clock_branch_t clock_branch,
                                                       uint16_t *precision)
{
  sl_status_t return_status = SL_STATUS_FAIL;

  switch (clock_branch) {
    case SL_CLOCK_BRANCH_SYSCLK:
#if defined(_CMU_SYSCLKCTRL_CPUCLKPRESC_MASK)
    case SL_CLOCK_BRANCH_CPUCLK:
#endif
    case SL_CLOCK_BRANCH_HCLK:
#if defined(_CMU_SYSCLKCTRL_HCLKDIVNPRESC_MASK)
    case SL_CLOCK_BRANCH_HCLKDIVN:
#endif
    case SL_CLOCK_BRANCH_PCLK:
    case SL_CLOCK_BRANCH_LSPCLK:
    case SL_CLOCK_BRANCH_EXPORTCLK:
      switch (CMU->SYSCLKCTRL & _CMU_SYSCLKCTRL_CLKSEL_MASK) {
        case CMU_SYSCLKCTRL_CLKSEL_HFXO:
          return_status = sli_clock_manager_hal_get_oscillator_precision(SL_OSCILLATOR_HFXO, precision);
          break;

        case CMU_SYSCLKCTRL_CLKSEL_HFRCODPLL:
        case CMU_SYSCLKCTRL_CLKSEL_FSRCO:
          *precision = 0xFFFF;
          return_status = SL_STATUS_NOT_AVAILABLE;
          break;

#if (SOCPLL_OUTPUT_COUNT == 3)
        case CMU_SYSCLKCTRL_CLKSEL_SOCPLL:
          return_status = sli_clock_manager_hal_get_oscillator_precision(SL_OSCILLATOR_SOCPLL0_OUT1, precision);
          break;
#else
        case CMU_SYSCLKCTRL_CLKSEL_SOCPLL:
          return_status = sli_clock_manager_hal_get_oscillator_precision(SL_OSCILLATOR_SOCPLL0, precision);
          break;
#endif

#if defined(CMU_SYSCLKCTRL_CLKSEL_SOCPLL1)
        case CMU_SYSCLKCTRL_CLKSEL_SOCPLL1:
          return_status = sli_clock_manager_hal_get_oscillator_precision(SL_OSCILLATOR_SOCPLL1_OUT1, precision);
          break;
#endif

        default:
          *precision = 0U;
          return_status = SL_STATUS_INVALID_STATE;
          break;
      }
      break;

#if defined(_CMU_TRACECLKCTRL_MASK)
    case SL_CLOCK_BRANCH_TRACECLK:
      switch (CMU->TRACECLKCTRL & _CMU_TRACECLKCTRL_CLKSEL_MASK) {
        case CMU_TRACECLKCTRL_CLKSEL_SYSCLK:
          return_status = sli_clock_manager_hal_get_clock_branch_precision(SL_CLOCK_BRANCH_SYSCLK, precision);
          break;

        case CMU_TRACECLKCTRL_CLKSEL_HFRCOEM23:
#if defined(CMU_TRACECLKCTRL_CLKSEL_HFRCODPLLRT)
        case CMU_TRACECLKCTRL_CLKSEL_HFRCODPLLRT:
#endif
        case CMU_TRACECLKCTRL_CLKSEL_DISABLE:
          *precision = 0xFFFF;
          return_status = SL_STATUS_NOT_AVAILABLE;
          break;

        default:
          *precision = 0U;
          return_status = SL_STATUS_INVALID_STATE;
          break;
      }
      break;
#endif

#if defined(_CMU_EM01GRPACLKCTRL_CLKSEL_MASK)
    case SL_CLOCK_BRANCH_EM01GRPACLK:
      switch (CMU->EM01GRPACLKCTRL & _CMU_EM01GRPACLKCTRL_CLKSEL_MASK) {
        case CMU_EM01GRPACLKCTRL_CLKSEL_HFXO:
#if defined(CMU_EM01GRPACLKCTRL_CLKSEL_HFXORT)
        case CMU_EM01GRPACLKCTRL_CLKSEL_HFXORT:
#endif
          return_status = sli_clock_manager_hal_get_oscillator_precision(SL_OSCILLATOR_HFXO, precision);
          break;

        case CMU_EM01GRPACLKCTRL_CLKSEL_HFRCODPLL:
#if defined(CMU_EM01GRPACLKCTRL_CLKSEL_HFRCODPLLRT)
        case CMU_EM01GRPACLKCTRL_CLKSEL_HFRCODPLLRT:
#endif
        case CMU_EM01GRPACLKCTRL_CLKSEL_HFRCOEM23:
        case CMU_EM01GRPACLKCTRL_CLKSEL_FSRCO:
          *precision = 0xFFFF;
          return_status = SL_STATUS_NOT_AVAILABLE;
          break;

        default:
          *precision = 0U;
          return_status = SL_STATUS_INVALID_STATE;
          break;
      }
      break;
#endif

#if defined(_CMU_EM01GRPCCLKCTRL_CLKSEL_MASK)
    case SL_CLOCK_BRANCH_EM01GRPCCLK:
      switch (CMU->EM01GRPCCLKCTRL & _CMU_EM01GRPCCLKCTRL_CLKSEL_MASK) {
        case CMU_EM01GRPCCLKCTRL_CLKSEL_HFXO:
#if defined(CMU_EM01GRPCCLKCTRL_CLKSEL_HFXORT)
        case CMU_EM01GRPCCLKCTRL_CLKSEL_HFXORT:
#endif
          return_status = sli_clock_manager_hal_get_oscillator_precision(SL_OSCILLATOR_HFXO, precision);
          break;

        case CMU_EM01GRPCCLKCTRL_CLKSEL_HFRCODPLL:
#if defined(CMU_EM01GRPCCLKCTRL_CLKSEL_HFRCODPLLRT)
        case CMU_EM01GRPCCLKCTRL_CLKSEL_HFRCODPLLRT:
#endif
        case CMU_EM01GRPCCLKCTRL_CLKSEL_HFRCOEM23:
        case CMU_EM01GRPCCLKCTRL_CLKSEL_FSRCO:
          *precision = 0xFFFF;
          return_status = SL_STATUS_NOT_AVAILABLE;
          break;

        default:
          *precision = 0U;
          return_status = SL_STATUS_INVALID_STATE;
          break;
      }
      break;
#endif

#if defined(_CMU_EM01GRPDCLKCTRL_CLKSEL_MASK)
    case SL_CLOCK_BRANCH_EM01GRPDCLK:
      switch (CMU->EM01GRPDCLKCTRL & _CMU_EM01GRPDCLKCTRL_CLKSEL_MASK) {
        case CMU_EM01GRPDCLKCTRL_CLKSEL_HFXO:
          return_status = sli_clock_manager_hal_get_oscillator_precision(SL_OSCILLATOR_HFXO, precision);
          break;

        case CMU_EM01GRPDCLKCTRL_CLKSEL_HFRCODPLL:
        case CMU_EM01GRPDCLKCTRL_CLKSEL_HFRCOEM23:
        case CMU_EM01GRPDCLKCTRL_CLKSEL_FSRCO:
          *precision = 0xFFFF;
          return_status = SL_STATUS_NOT_AVAILABLE;
          break;

        default:
          *precision = 0U;
          return_status = SL_STATUS_INVALID_STATE;
          break;
      }
      break;
#endif

#if defined(_CMU_QSPISYSCLKCTRL_MASK)
    case SL_CLOCK_BRANCH_QSPISYSCLK:
      switch (CMU->QSPISYSCLKCTRL & _CMU_QSPISYSCLKCTRL_CLKSEL_MASK) {
        case CMU_QSPISYSCLKCTRL_CLKSEL_HFXO:
          return_status = sli_clock_manager_hal_get_oscillator_precision(SL_OSCILLATOR_HFXO, precision);
          break;

        case CMU_QSPISYSCLKCTRL_CLKSEL_HFRCODPLL:
          *precision = 0xFFFF;
          return_status = SL_STATUS_NOT_AVAILABLE;
          break;

        case CMU_QSPISYSCLKCTRL_CLKSEL_SOCPLL:
          return_status = sli_clock_manager_hal_get_oscillator_precision(SL_OSCILLATOR_SOCPLL0, precision);
          break;

        default:
          *precision = 0U;
          return_status = SL_STATUS_INVALID_STATE;
          break;
      }
      break;
#endif

#if defined(_CMU_PIXELRZCLKCTRL_MASK)
    case SL_CLOCK_BRANCH_PIXELRZCLK:
      switch (CMU->PIXELRZCLKCTRL & _CMU_PIXELRZCLKCTRL_CLKSEL_MASK) {
        case CMU_PIXELRZCLKCTRL_CLKSEL_HFXO:
          return_status = sli_clock_manager_hal_get_oscillator_precision(SL_OSCILLATOR_HFXO, precision);
          break;

        case CMU_PIXELRZCLKCTRL_CLKSEL_HFRCODPLL:
        case CMU_PIXELRZCLKCTRL_CLKSEL_FSRCO:
        case CMU_PIXELRZCLKCTRL_CLKSEL_HFRCOEM23:
          *precision = 0xFFFF;
          return_status = SL_STATUS_NOT_AVAILABLE;
          break;

        default:
          *precision = 0U;
          return_status = SL_STATUS_INVALID_STATE;
          break;
      }
      break;
#endif

    default:
      *precision = 0U;
      return_status = SL_STATUS_INVALID_PARAMETER;
      break;
  }

  return return_status;
}

static sl_status_t get_low_frequency_branch_precision(sl_clock_branch_t clock_branch,
                                                      uint16_t *precision)
{
  sl_status_t return_status = SL_STATUS_FAIL;

  switch (clock_branch) {
#if defined(_CMU_EM23GRPACLKCTRL_CLKSEL_MASK)
    case SL_CLOCK_BRANCH_EM23GRPACLK:
      switch (CMU->EM23GRPACLKCTRL & _CMU_EM23GRPACLKCTRL_CLKSEL_MASK) {
        case CMU_EM23GRPACLKCTRL_CLKSEL_LFXO:
          return_status = sli_clock_manager_hal_get_oscillator_precision(SL_OSCILLATOR_LFXO, precision);
          break;

        case CMU_EM23GRPACLKCTRL_CLKSEL_LFRCO:
          return_status = sli_clock_manager_hal_get_oscillator_precision(SL_OSCILLATOR_LFRCO, precision);
          break;

        case CMU_EM23GRPACLKCTRL_CLKSEL_ULFRCO:
          *precision = 0xFFFF;
          return_status = SL_STATUS_NOT_AVAILABLE;
          break;

        default:
          *precision = 0U;
          return_status = SL_STATUS_INVALID_STATE;
          break;
      }
      break;
#endif

#if defined(_CMU_EM4GRPACLKCTRL_CLKSEL_MASK)
    case SL_CLOCK_BRANCH_EM4GRPACLK:
      switch (CMU->EM4GRPACLKCTRL & _CMU_EM4GRPACLKCTRL_CLKSEL_MASK) {
        case CMU_EM4GRPACLKCTRL_CLKSEL_LFXO:
          return_status = sli_clock_manager_hal_get_oscillator_precision(SL_OSCILLATOR_LFXO, precision);
          break;

        case CMU_EM4GRPACLKCTRL_CLKSEL_LFRCO:
          return_status = sli_clock_manager_hal_get_oscillator_precision(SL_OSCILLATOR_LFRCO, precision);
          break;

        case CMU_EM4GRPACLKCTRL_CLKSEL_ULFRCO:
          *precision = 0xFFFF;
          return_status = SL_STATUS_NOT_AVAILABLE;
          break;

        default:
          *precision = 0U;
          return_status = SL_STATUS_INVALID_STATE;
          break;
      }
      break;
#endif

    case SL_CLOCK_BRANCH_WDOG0CLK:
      switch (CMU->WDOG0CLKCTRL & _CMU_WDOG0CLKCTRL_CLKSEL_MASK) {
        case CMU_WDOG0CLKCTRL_CLKSEL_LFXO:
          return_status = sli_clock_manager_hal_get_oscillator_precision(SL_OSCILLATOR_LFXO, precision);
          break;

        case CMU_WDOG0CLKCTRL_CLKSEL_LFRCO:
          return_status = sli_clock_manager_hal_get_oscillator_precision(SL_OSCILLATOR_LFRCO, precision);
          break;

#if defined(CMU_WDOG0CLKCTRL_CLKSEL_HCLKDIV1024)
        case CMU_WDOG0CLKCTRL_CLKSEL_HCLKDIV1024:
          return_status = sli_clock_manager_hal_get_clock_branch_precision(SL_CLOCK_BRANCH_HCLK, precision);
          break;
#endif

        case CMU_WDOG0CLKCTRL_CLKSEL_ULFRCO:
          *precision = 0xFFFF;
          return_status = SL_STATUS_NOT_AVAILABLE;
          break;

        default:
          *precision = 0U;
          return_status = SL_STATUS_INVALID_STATE;
          break;
      }
      break;

    case SL_CLOCK_BRANCH_WDOG1CLK:
#if defined(_CMU_WDOG1CLKCTRL_CLKSEL_MASK)
      switch (CMU->WDOG1CLKCTRL & _CMU_WDOG1CLKCTRL_CLKSEL_MASK) {
        case CMU_WDOG1CLKCTRL_CLKSEL_LFXO:
          return_status = sli_clock_manager_hal_get_oscillator_precision(SL_OSCILLATOR_LFXO, precision);
          break;

        case CMU_WDOG1CLKCTRL_CLKSEL_LFRCO:
          return_status = sli_clock_manager_hal_get_oscillator_precision(SL_OSCILLATOR_LFRCO, precision);
          break;

#if defined(CMU_WDOG1CLKCTRL_CLKSEL_HCLKDIV1024)
        case CMU_WDOG1CLKCTRL_CLKSEL_HCLKDIV1024:
          return_status = sli_clock_manager_hal_get_clock_branch_precision(SL_CLOCK_BRANCH_HCLK, precision);
          break;
#endif

        case CMU_WDOG1CLKCTRL_CLKSEL_ULFRCO:
          *precision = 0xFFFF;
          return_status = SL_STATUS_NOT_AVAILABLE;
          break;

        default:
          *precision = 0U;
          return_status = SL_STATUS_INVALID_STATE;
          break;
      }
      break;
#else
      return_status = sli_clock_manager_hal_get_clock_branch_precision(SL_CLOCK_BRANCH_HCLK, precision);
      break;
#endif

    case SL_CLOCK_BRANCH_SYSRTCCLK:
      switch (CMU->SYSRTC0CLKCTRL & _CMU_SYSRTC0CLKCTRL_CLKSEL_MASK) {
        case CMU_SYSRTC0CLKCTRL_CLKSEL_LFXO:
          return_status = sli_clock_manager_hal_get_oscillator_precision(SL_OSCILLATOR_LFXO, precision);
          break;

        case CMU_SYSRTC0CLKCTRL_CLKSEL_LFRCO:
          return_status = sli_clock_manager_hal_get_oscillator_precision(SL_OSCILLATOR_LFRCO, precision);
          break;

        case CMU_SYSRTC0CLKCTRL_CLKSEL_ULFRCO:
          *precision = 0xFFFF;
          return_status = SL_STATUS_NOT_AVAILABLE;
          break;

        default:
          *precision = 0U;
          return_status = SL_STATUS_INVALID_STATE;
          break;
      }
      break;

    default:
      *precision = 0U;
      return_status = SL_STATUS_INVALID_PARAMETER;
      break;
  }

  return return_status;
}

static sl_status_t get_mixed_frequency_branch_precision(sl_clock_branch_t clock_branch,
                                                   uint16_t *precision)
{
  sl_status_t return_status = SL_STATUS_FAIL;

  switch (clock_branch) {
    case SL_CLOCK_BRANCH_EUSART0CLK:
      switch (CMU->EUSART0CLKCTRL & _CMU_EUSART0CLKCTRL_CLKSEL_MASK) {
#if defined(_CMU_EUSART0CLKCTRL_CLKSEL_EM01GRPCCLK)
        case CMU_EUSART0CLKCTRL_CLKSEL_EM01GRPCCLK:
          return_status = sli_clock_manager_hal_get_clock_branch_precision(SL_CLOCK_BRANCH_EM01GRPCCLK, precision);
          break;
#endif
#if defined(CMU_EUSART0CLKCTRL_CLKSEL_FSRCO)
        case CMU_EUSART0CLKCTRL_CLKSEL_FSRCO:
          *precision = 0xFFFF;
          return_status = SL_STATUS_NOT_AVAILABLE;
          break;
#endif
        case CMU_EUSART0CLKCTRL_CLKSEL_HFRCOEM23:
          *precision = 0xFFFF;
          return_status = SL_STATUS_NOT_AVAILABLE;
          break;

        case CMU_EUSART0CLKCTRL_CLKSEL_LFRCO:
          return_status = sli_clock_manager_hal_get_oscillator_precision(SL_OSCILLATOR_LFRCO, precision);
          break;

        case CMU_EUSART0CLKCTRL_CLKSEL_LFXO:
          return_status = sli_clock_manager_hal_get_oscillator_precision(SL_OSCILLATOR_LFXO, precision);
          break;

        default:
          *precision = 0U;
          return_status = SL_STATUS_INVALID_STATE;
          break;
      }
      break;

#if defined(_CMU_EUSART1CLKCTRL_CLKSEL_MASK)
    case SL_CLOCK_BRANCH_EUSART1CLK:
      switch (CMU->EUSART1CLKCTRL & _CMU_EUSART1CLKCTRL_CLKSEL_MASK) {
#if defined(_CMU_EUSART1CLKCTRL_CLKSEL_EM01GRPCCLK)
        case CMU_EUSART1CLKCTRL_CLKSEL_EM01GRPCCLK:
          return_status = sli_clock_manager_hal_get_clock_branch_precision(SL_CLOCK_BRANCH_EM01GRPCCLK, precision);
          break;
#endif
#if defined(CMU_EUSART1CLKCTRL_CLKSEL_FSRCO)
        case CMU_EUSART1CLKCTRL_CLKSEL_FSRCO:
          *precision = 0xFFFF;
          return_status = SL_STATUS_NOT_AVAILABLE;
          break;
#endif
        case CMU_EUSART1CLKCTRL_CLKSEL_HFRCOEM23:
          *precision = 0xFFFF;
          return_status = SL_STATUS_NOT_AVAILABLE;
          break;

        case CMU_EUSART1CLKCTRL_CLKSEL_LFRCO:
          return_status = sli_clock_manager_hal_get_oscillator_precision(SL_OSCILLATOR_LFRCO, precision);
          break;

        case CMU_EUSART1CLKCTRL_CLKSEL_LFXO:
          return_status = sli_clock_manager_hal_get_oscillator_precision(SL_OSCILLATOR_LFXO, precision);
          break;

        default:
          *precision = 0U;
          return_status = SL_STATUS_INVALID_STATE;
          break;
      }
      break;
#endif

    case SL_CLOCK_BRANCH_PCNT0CLK:
      switch (CMU->PCNT0CLKCTRL & _CMU_PCNT0CLKCTRL_CLKSEL_MASK) {
#if defined(_CMU_PCNT0CLKCTRL_CLKSEL_EM23GRPACLK)
        case CMU_PCNT0CLKCTRL_CLKSEL_EM23GRPACLK:
          return_status = sli_clock_manager_hal_get_clock_branch_precision(SL_CLOCK_BRANCH_EM23GRPACLK, precision);
          break;
#endif

        case CMU_PCNT0CLKCTRL_CLKSEL_PCNTS0:
          *precision = 0;
          return_status = SL_STATUS_NOT_SUPPORTED;
          break;

        default:
          *precision = 0U;
          return_status = SL_STATUS_INVALID_STATE;
          break;
      }
      break;

    case SL_CLOCK_BRANCH_I2C0CLK:
      switch (CMU->I2C0CLKCTRL & _CMU_I2C0CLKCTRL_CLKSEL_MASK) {
#if defined(_CMU_I2C0CLKCTRL_CLKSEL_EM01GRPDCLK)
        case CMU_I2C0CLKCTRL_CLKSEL_EM01GRPDCLK:
          return_status = sli_clock_manager_hal_get_clock_branch_precision(SL_CLOCK_BRANCH_EM01GRPDCLK, precision);
          break;
#endif
        case CMU_I2C0CLKCTRL_CLKSEL_HFRCOEM23:
          *precision = 0xFFFF;
          return_status = SL_STATUS_NOT_AVAILABLE;
          break;

        case CMU_I2C0CLKCTRL_CLKSEL_LFRCO:
          return_status = sli_clock_manager_hal_get_oscillator_precision(SL_OSCILLATOR_LFRCO, precision);
          break;

        case CMU_I2C0CLKCTRL_CLKSEL_LFXO:
          return_status = sli_clock_manager_hal_get_oscillator_precision(SL_OSCILLATOR_LFXO, precision);
          break;

        default:
          *precision = 0U;
          return_status = SL_STATUS_INVALID_STATE;
          break;
      }
      break;

    case SL_CLOCK_BRANCH_SYSTICKCLK:
      if (SysTick->CTRL & SysTick_CTRL_CLKSOURCE_Msk) {
        // Systick connected to processor clock when CLKSOURCE bit is set: HCLK
        return_status = sli_clock_manager_hal_get_clock_branch_precision(SL_CLOCK_BRANCH_HCLK, precision);
      } else {
        // Systick connected to external clock when CLKSOURCE bit is clear: EM23GRPACLK
        return_status = sli_clock_manager_hal_get_clock_branch_precision(SL_CLOCK_BRANCH_EM23GRPACLK, precision);
      }
      break;

#if defined(_CMU_VDAC0CLKCTRL_MASK)
    case SL_CLOCK_BRANCH_VDAC0CLK:
      switch (CMU->VDAC0CLKCTRL & _CMU_VDAC0CLKCTRL_CLKSEL_MASK) {
#if defined(_CMU_VDAC0CLKCTRL_CLKSEL_EM01GRPACLK)
        case CMU_VDAC0CLKCTRL_CLKSEL_EM01GRPACLK:
          return_status = sli_clock_manager_hal_get_clock_branch_precision(SL_CLOCK_BRANCH_EM01GRPACLK, precision);
          break;
#endif
#if defined(_CMU_VDAC0CLKCTRL_CLKSEL_EM23GRPACLK)
        case CMU_VDAC0CLKCTRL_CLKSEL_EM23GRPACLK:
          return_status = sli_clock_manager_hal_get_clock_branch_precision(SL_CLOCK_BRANCH_EM23GRPACLK, precision);
          break;
#endif
        case CMU_VDAC0CLKCTRL_CLKSEL_FSRCO:
        case CMU_VDAC0CLKCTRL_CLKSEL_HFRCOEM23:
          *precision = 0xFFFF;
          return_status = SL_STATUS_NOT_AVAILABLE;
          break;

        default:
          *precision = 0U;
          return_status = SL_STATUS_INVALID_STATE;
          break;
      }
      break;
#endif

    default:
      *precision = 0U;
      return_status = SL_STATUS_INVALID_PARAMETER;
      break;
  }

  return return_status;
}

static sl_status_t get_reference_clock_branch_precision(sl_clock_branch_t clock_branch,
                                                        uint16_t *precision)
{
  sl_status_t return_status = SL_STATUS_FAIL;

  if (clock_branch == SL_CLOCK_BRANCH_DPLLREFCLK) {
    switch (CMU->DPLLREFCLKCTRL & _CMU_DPLLREFCLKCTRL_CLKSEL_MASK) {
      case CMU_DPLLREFCLKCTRL_CLKSEL_HFXO:
        return_status = sli_clock_manager_hal_get_oscillator_precision(SL_OSCILLATOR_HFXO, precision);
        break;

      case CMU_DPLLREFCLKCTRL_CLKSEL_LFXO:
        return_status = sli_clock_manager_hal_get_oscillator_precision(SL_OSCILLATOR_LFXO, precision);
        break;

      case CMU_DPLLREFCLKCTRL_CLKSEL_CLKIN0:
        *precision = 0xFFFF;
        return_status = SL_STATUS_NOT_AVAILABLE;
        break;

      default:
        *precision = 0U;
        return_status = SL_STATUS_INVALID_STATE;
        break;
    }
  }

#if defined(_CMU_FLPLLREFCLKCTRL_MASK)
  else if (clock_branch == SL_CLOCK_BRANCH_FLPLLREFCLK) {
    switch (CMU->FLPLLREFCLKCTRL & _CMU_FLPLLREFCLKCTRL_CLKSEL_MASK) {
      case CMU_FLPLLREFCLKCTRL_CLKSEL_HFRCODPLLRT:
      case CMU_FLPLLREFCLKCTRL_CLKSEL_CLKIN0:
        *precision = 0xFFFF;
        return_status = SL_STATUS_NOT_AVAILABLE;
        break;

      default:
        *precision = 0U;
        return_status = SL_STATUS_INVALID_STATE;
        break;
    }
  }
#endif

  else {
    *precision = 0U;
    return_status = SL_STATUS_INVALID_PARAMETER;
  }

  return return_status;
}

/***************************************************************************//**
 * Gets precision of given clock branch.
 ******************************************************************************/
sl_status_t sli_clock_manager_hal_get_clock_branch_precision(sl_clock_branch_t clock_branch,
                                                             uint16_t *precision)
{
  sl_status_t return_status = SL_STATUS_FAIL;
  CORE_DECLARE_IRQ_STATE;

  CORE_ENTER_ATOMIC();

  switch (clock_branch) {
    case SL_CLOCK_BRANCH_SYSCLK:
#if defined(_CMU_SYSCLKCTRL_CPUCLKPRESC_MASK)
    case SL_CLOCK_BRANCH_CPUCLK:
#endif
    case SL_CLOCK_BRANCH_HCLK:
#if defined(_CMU_SYSCLKCTRL_HCLKDIVNPRESC_MASK)
    case SL_CLOCK_BRANCH_HCLKDIVN:
#endif
    case SL_CLOCK_BRANCH_PCLK:
    case SL_CLOCK_BRANCH_LSPCLK:
    case SL_CLOCK_BRANCH_EXPORTCLK:
#if defined(_CMU_TRACECLKCTRL_MASK)
    case SL_CLOCK_BRANCH_TRACECLK:
#endif
#if defined(_CMU_EM01GRPACLKCTRL_CLKSEL_MASK)
    case SL_CLOCK_BRANCH_EM01GRPACLK:
#endif
#if defined(_CMU_EM01GRPCCLKCTRL_CLKSEL_MASK)
    case SL_CLOCK_BRANCH_EM01GRPCCLK:
#endif
#if defined(_CMU_EM01GRPDCLKCTRL_CLKSEL_MASK)
    case SL_CLOCK_BRANCH_EM01GRPDCLK:
#endif
#if defined(_CMU_QSPISYSCLKCTRL_MASK)
    case SL_CLOCK_BRANCH_QSPISYSCLK:
#endif
#if defined(_CMU_PIXELRZCLKCTRL_MASK)
    case SL_CLOCK_BRANCH_PIXELRZCLK:
#endif
      return_status = get_high_frequency_branch_precision(clock_branch, precision);
      break;

#if defined(_CMU_EM23GRPACLKCTRL_CLKSEL_MASK)
    case SL_CLOCK_BRANCH_EM23GRPACLK:
#endif
#if defined(_CMU_EM4GRPACLKCTRL_CLKSEL_MASK)
    case SL_CLOCK_BRANCH_EM4GRPACLK:
#endif
    case SL_CLOCK_BRANCH_WDOG0CLK:
    case SL_CLOCK_BRANCH_WDOG1CLK:
    case SL_CLOCK_BRANCH_SYSRTCCLK:
      return_status = get_low_frequency_branch_precision(clock_branch, precision);
      break;

    case SL_CLOCK_BRANCH_EUSART0CLK:
#if defined(_CMU_EUSART1CLKCTRL_CLKSEL_MASK)
    case SL_CLOCK_BRANCH_EUSART1CLK:
#endif
    case SL_CLOCK_BRANCH_PCNT0CLK:
    case SL_CLOCK_BRANCH_I2C0CLK:
    case SL_CLOCK_BRANCH_SYSTICKCLK:
#if defined(_CMU_VDAC0CLKCTRL_MASK)
    case SL_CLOCK_BRANCH_VDAC0CLK:
#endif
      return_status = get_mixed_frequency_branch_precision(clock_branch, precision);
      break;

    case SL_CLOCK_BRANCH_DPLLREFCLK:
#if defined(_CMU_FLPLLREFCLKCTRL_MASK)
    case SL_CLOCK_BRANCH_FLPLLREFCLK:
#endif
      return_status = get_reference_clock_branch_precision(clock_branch, precision);
      break;

    default:
#if defined(SL_CATALOG_CLOCK_MANAGER_RUNTIME_HAL_INTERNAL_PRESENT)
      return_status = sli_clock_manager_hal_get_clock_branch_precision_internal(clock_branch, precision);
#else
      *precision = 0U;
      return_status = SL_STATUS_INVALID_PARAMETER;
#endif
      break;
  }

  CORE_EXIT_ATOMIC();

  return return_status;
}

/***************************************************************************//**
 * Gets the RC oscillator frequency tuning setting.
 ******************************************************************************/
sl_status_t sli_clock_manager_hal_get_rc_oscillator_calibration(sl_oscillator_t oscillator,
                                                                uint32_t *val)
{
  switch (oscillator) {
    case SL_OSCILLATOR_HFRCODPLL:
      sl_clock_manager_enable_bus_clock(SL_BUS_CLOCK_HFRCO0);
      *val = (HFRCO0->CAL & _HFRCO_CAL_TUNING_MASK) >> _HFRCO_CAL_TUNING_SHIFT;
      break;

    case SL_OSCILLATOR_HFRCOEM23:
      *val = (HFRCOEM23->CAL & _HFRCO_CAL_TUNING_MASK) >> _HFRCO_CAL_TUNING_SHIFT;
      break;

#if defined(_LFRCO_CAL_FREQTRIM_MASK)
    case SL_OSCILLATOR_LFRCO:
      CMU->CLKEN0_SET = CMU_CLKEN0_LFRCO;
      *val = (LFRCO->CAL & _LFRCO_CAL_FREQTRIM_MASK) >> _LFRCO_CAL_FREQTRIM_SHIFT;
      break;
#endif

    default:
      *val = 0;
      return SL_STATUS_INVALID_PARAMETER;
  }

  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Sets the RC oscillator frequency tuning control.
 ******************************************************************************/
sl_status_t sli_clock_manager_hal_set_rc_oscillator_calibration(sl_oscillator_t oscillator,
                                                                uint32_t val)
{
  switch (oscillator) {
    case SL_OSCILLATOR_HFRCODPLL:
      sl_clock_manager_enable_bus_clock(SL_BUS_CLOCK_HFRCO0);
      EFM_ASSERT(val <= (_HFRCO_CAL_TUNING_MASK >> _HFRCO_CAL_TUNING_SHIFT));
      val &= _HFRCO_CAL_TUNING_MASK >> _HFRCO_CAL_TUNING_SHIFT;
      while ((HFRCO0->STATUS & HFRCO_STATUS_SYNCBUSY) != 0UL) {
        // Wait for any ongoing synchronization to finish.
      }
      HFRCO0->CAL = (HFRCO0->CAL & ~_HFRCO_CAL_TUNING_MASK)
                    | (val << _HFRCO_CAL_TUNING_SHIFT);
      break;

    case SL_OSCILLATOR_HFRCOEM23:
      EFM_ASSERT(val <= (_HFRCO_CAL_TUNING_MASK >> _HFRCO_CAL_TUNING_SHIFT));
      val &= _HFRCO_CAL_TUNING_MASK >> _HFRCO_CAL_TUNING_SHIFT;
      while ((HFRCOEM23->STATUS & HFRCO_STATUS_SYNCBUSY) != 0UL) {
        // Wait for any ongoing synchronization to finish.
      }
      HFRCOEM23->CAL = (HFRCOEM23->CAL & ~_HFRCO_CAL_TUNING_MASK)
                       | (val << _HFRCO_CAL_TUNING_SHIFT);
      break;

#if defined(_LFRCO_CAL_FREQTRIM_MASK)
    case SL_OSCILLATOR_LFRCO:
      CMU->CLKEN0_SET = CMU_CLKEN0_LFRCO;
      EFM_ASSERT(val <= (_LFRCO_CAL_FREQTRIM_MASK
                         >> _LFRCO_CAL_FREQTRIM_SHIFT));
      val &= _LFRCO_CAL_FREQTRIM_MASK >> _LFRCO_CAL_FREQTRIM_SHIFT;
      while (LFRCO->SYNCBUSY != 0U) {
        // Wait for any ongoing synchronization to finish.
      }
      LFRCO->CAL = (LFRCO->CAL & ~_LFRCO_CAL_FREQTRIM_MASK)
                   | (val << _LFRCO_CAL_FREQTRIM_SHIFT);
      break;
#endif

    default:
      SLI_CLOCK_MANAGER_LOG_WARN("unsupported oscillator for RC calibration, osc=%u",
                                 (uint32_t)oscillator);
      return SL_STATUS_INVALID_PARAMETER;
  }

  SLI_CLOCK_MANAGER_LOG_INFO("RC oscillator calibration updated, osc=%u val=%u",
                             (uint32_t)oscillator, val);

  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Sets HFXO calibration value.
 ******************************************************************************/
sl_status_t sli_clock_manager_hal_set_hfxo_calibration(uint32_t val)
{
  CORE_DECLARE_IRQ_STATE;
  bool disondemand = false;
  sl_status_t status = SL_STATUS_OK;

#if defined(_SILICON_LABS_32B_SERIES_3_CONFIG_301) \
  || defined(_SILICON_LABS_32B_SERIES_3_CONFIG_353)
  sl_oscillator_t ext_flash_source;
  uint32_t hfxo_ctrl_backup = HFXO0->CTRL;

  status = hfxo_tuning_prepare_flash(&ext_flash_source);
  if (status != SL_STATUS_OK) {
    return status;
  }
#endif

  sl_clock_manager_enable_bus_clock(SL_BUS_CLOCK_HFXO0);
  EFM_ASSERT(val <= (_HFXO_XTALCTRL_COREBIASANA_MASK >> _HFXO_XTALCTRL_COREBIASANA_SHIFT));

  // Make sure HFXO is disabled.
  EFM_ASSERT((HFXO0->STATUS & HFXO_STATUS_ENS) == 0);
  if ((HFXO0->STATUS & HFXO_STATUS_ENS) != 0) {
#if defined(_SILICON_LABS_32B_SERIES_3_CONFIG_301) \
  || defined(_SILICON_LABS_32B_SERIES_3_CONFIG_353)
    status = hfxo_tuning_restore_flash(ext_flash_source, hfxo_ctrl_backup);
    if (status != SL_STATUS_OK) {
      return status;
    }
#endif
    return SL_STATUS_INVALID_STATE;
  }

  CORE_ENTER_ATOMIC();

  // Set DISONDEMAND if not already set and wait for FSMLOCK to be clear so that
  // software can write to register
  disondemand = (HFXO0->CTRL & _HFXO_CTRL_DISONDEMAND_MASK) >> _HFXO_CTRL_DISONDEMAND_SHIFT;
  if (disondemand == false) {
    HFXO0->CTRL_SET = HFXO_CTRL_DISONDEMAND;
  }

  // Update Core Bias Ana setting and enable Optimization skip
  HFXO0->XTALCTRL = (HFXO0->XTALCTRL & ~_HFXO_XTALCTRL_COREBIASANA_MASK)
                    | (val << _HFXO_XTALCTRL_COREBIASANA_SHIFT)
                    | HFXO_XTALCTRL_SKIPCOREBIASOPT;
  // Clear back DISONDEMAND if needed
  if (disondemand == false) {
    HFXO0->CTRL_CLR = HFXO_CTRL_DISONDEMAND;
  }

#if defined(_SILICON_LABS_32B_SERIES_3_CONFIG_301) \
  || defined(_SILICON_LABS_32B_SERIES_3_CONFIG_353)
  status = hfxo_tuning_restore_flash(ext_flash_source, hfxo_ctrl_backup);
#endif

  CORE_EXIT_ATOMIC();

  if (status == SL_STATUS_OK) {
    SLI_CLOCK_MANAGER_LOG_INFO("HFXO calibration updated, val=%u", val);
  } else {
    SLI_CLOCK_MANAGER_LOG_WARN("HFXO calibration update failed, val=%u status=0x%x",
                               val, (uint32_t)status);
  }

  return status;
}

/***************************************************************************//**
 * Gets the HFXO calibration value.
 ******************************************************************************/
sl_status_t sli_clock_manager_hal_get_hfxo_calibration(uint32_t *val)
{
  sl_clock_manager_enable_bus_clock(SL_BUS_CLOCK_HFXO0);
  *val = (HFXO0->XTALCTRL & _HFXO_XTALCTRL_COREBIASANA_MASK) >> _HFXO_XTALCTRL_COREBIASANA_SHIFT;

  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Sets the HFXO CTUNE setting.
 ******************************************************************************/
static sl_status_t hfxo_set_ctune(uint32_t ctune, bool log_result)
{
  sl_status_t status = SL_STATUS_OK;

  CORE_DECLARE_IRQ_STATE;

  // Make sure the given CTUNE value is within the allowable range.
  if (ctune > (_HFXO_XTALCTRL_CTUNEXIANA_MASK >> _HFXO_XTALCTRL_CTUNEXIANA_SHIFT)) {
    if (log_result) {
      SLI_CLOCK_MANAGER_LOG_WARN("HFXO CTUNE update rejected, val=%u", ctune);
    }
    return SL_STATUS_INVALID_PARAMETER;
  }

#if defined(_SILICON_LABS_32B_SERIES_3_CONFIG_301) \
  || defined(_SILICON_LABS_32B_SERIES_3_CONFIG_353)
  sl_oscillator_t ext_flash_source;
  uint32_t hfxo_ctrl_backup = HFXO0->CTRL;

  status = hfxo_tuning_prepare_flash(&ext_flash_source);
  if (status != SL_STATUS_OK) {
    return status;
  }
#endif

  uint32_t ctuneXoana = ctune + HFXO_CTUNE_DELTA;
  if (ctuneXoana > (_HFXO_XTALCTRL_CTUNEXOANA_MASK >> _HFXO_XTALCTRL_CTUNEXOANA_SHIFT)) {
    ctuneXoana = _HFXO_XTALCTRL_CTUNEXOANA_MASK >> _HFXO_XTALCTRL_CTUNEXOANA_SHIFT; // Max value
  }

  CORE_ENTER_ATOMIC();

  bool was_locked = ((HFXO0->STATUS & HFXO_STATUS_LOCK_LOCKED) != 0) ? true : false;

  // Unlock register interface.
  HFXO0->LOCK = HFXO_LOCK_LOCKKEY_UNLOCK;

  // Write new CTune value.
  HFXO0->XTALCTRL = ((HFXO0->XTALCTRL & ~(_HFXO_XTALCTRL_CTUNEXOANA_MASK | _HFXO_XTALCTRL_CTUNEXIANA_MASK))
                     | ((ctuneXoana << _HFXO_XTALCTRL_CTUNEXOANA_SHIFT) & _HFXO_XTALCTRL_CTUNEXOANA_MASK)
                     | ((ctune << _HFXO_XTALCTRL_CTUNEXIANA_SHIFT) & _HFXO_XTALCTRL_CTUNEXIANA_MASK));

  // Relock register if it was locked in the first place.
  if (was_locked) {
    HFXO0->LOCK = ~HFXO_LOCK_LOCKKEY_UNLOCK;
  }

#if defined(_SILICON_LABS_32B_SERIES_3_CONFIG_301) \
  || defined(_SILICON_LABS_32B_SERIES_3_CONFIG_353)
  status = hfxo_tuning_restore_flash(ext_flash_source, hfxo_ctrl_backup);
#endif

  CORE_EXIT_ATOMIC();

  if (log_result) {
    if (status == SL_STATUS_OK) {
      SLI_CLOCK_MANAGER_LOG_INFO("HFXO CTUNE updated, val=%u", ctune);
    } else {
      SLI_CLOCK_MANAGER_LOG_WARN("HFXO CTUNE update failed, val=%u status=0x%x",
                                 ctune, (uint32_t)status);
    }
  }

  return status;
}

sl_status_t sli_clock_manager_hal_hfxo_set_ctune(uint32_t ctune)
{
  return hfxo_set_ctune(ctune, true);
}

/***************************************************************************//**
 * Gets the HFXO CTUNE setting.
 ******************************************************************************/
sl_status_t sli_clock_manager_hal_hfxo_get_ctune(uint32_t *ctune)
{
  if (ctune == NULL) {
    EFM_ASSERT(ctune != NULL);
    return SL_STATUS_NULL_POINTER;
  }

  *ctune = ((HFXO0->XTALCTRL & _HFXO_XTALCTRL_CTUNEXIANA_MASK)
            >> _HFXO_XTALCTRL_CTUNEXIANA_SHIFT);

  return SL_STATUS_OK;
}

#if defined(SL_CLOCK_MANAGER_HFXO_STARTUP_TIME_MEASUREMENT_EN) \
  && (SL_CLOCK_MANAGER_HFXO_STARTUP_TIME_MEASUREMENT_EN == 1) \
  && defined(SL_CLOCK_MANAGER_HFXO_EN) && (SL_CLOCK_MANAGER_HFXO_EN == 1) \
  && (SL_CLOCK_MANAGER_HFXO_MODE == HFXO_CFG_MODE_XTAL)
static bool clock_manager_hfxo_startup_measurement_active = false;
#endif

/***************************************************************************//**
 * Updates the tuning capacitances and calibrate the Core Bias Current.
 ******************************************************************************/
sl_status_t sli_clock_manager_hal_hfxo_calibrate_ctune(uint32_t ctune)
{
  uint32_t hfxo_ctrl_backup = HFXO0->CTRL;
  sl_status_t status = SL_STATUS_OK;
  bool startup_measurement_stopped = false;

  if (ctune > (_HFXO_XTALCTRL_CTUNEXIANA_MASK >> _HFXO_XTALCTRL_CTUNEXIANA_SHIFT)) {
    SLI_CLOCK_MANAGER_LOG_WARN("HFXO CTUNE calibration rejected, val=%u", ctune);
    return SL_STATUS_INVALID_PARAMETER;
  }

#if defined(_SILICON_LABS_32B_SERIES_3_CONFIG_301) \
  || defined(_SILICON_LABS_32B_SERIES_3_CONFIG_353)
  sl_oscillator_t ext_flash_source;

  status = hfxo_tuning_prepare_flash(&ext_flash_source);
  if (status != SL_STATUS_OK) {
    return status;
  }
#endif

  if (sli_clock_manager_hal_stop_hfxo_startup_time_measurement() == SL_STATUS_OK) {
    startup_measurement_stopped = true;
  }

  // The FORCEEN and DISONDEMAND bits need to be set to launch the calibration.
  HFXO0->CTRL_SET = HFXO_CTRL_FORCEEN;
  while ((HFXO0->STATUS & (HFXO_STATUS_COREBIASOPTRDY | HFXO_STATUS_RDY)) != (HFXO_STATUS_COREBIASOPTRDY | HFXO_STATUS_RDY)) {
    // Wait for crystal to startup.
  }
  HFXO0->CTRL_SET = HFXO_CTRL_DISONDEMAND;

  while ((HFXO0->STATUS & (HFXO_STATUS_COREBIASOPTRDY | HFXO_STATUS_RDY | HFXO_STATUS_ENS))
         != (HFXO_STATUS_COREBIASOPTRDY | HFXO_STATUS_RDY | HFXO_STATUS_ENS)) {
    // Making sure HFXO is in steady state.
  }

  status = hfxo_set_ctune(ctune, false);
  if (status == SL_STATUS_OK) {
    // Start core bias optimization.
    HFXO0->CMD_SET = HFXO_CMD_COREBIASOPT;

    while ((HFXO0->STATUS & HFXO_STATUS_COREBIASOPTRDY) == HFXO_STATUS_COREBIASOPTRDY) {
      // Wait for core bias optimization to start.
    }
    while ((HFXO0->STATUS & HFXO_STATUS_COREBIASOPTRDY) == 0) {
      // Wait for core bias optimization to finish.
    }
  }

#if defined(_SILICON_LABS_32B_SERIES_3_CONFIG_301) \
  || defined(_SILICON_LABS_32B_SERIES_3_CONFIG_353)
  sl_status_t restore_status = hfxo_tuning_restore_flash(ext_flash_source, hfxo_ctrl_backup);
  if (status == SL_STATUS_OK) {
    status = restore_status;
  }
#endif

  // Retrieve initial state of the CTRL register.
  sl_hal_bus_reg_write_mask(&HFXO0->CTRL, (_HFXO_CTRL_DISONDEMAND_MASK | _HFXO_CTRL_FORCEEN_MASK), hfxo_ctrl_backup);

  if (status == SL_STATUS_OK) {
    SLI_CLOCK_MANAGER_LOG_INFO("HFXO CTUNE calibration completed, val=%u", ctune);
  } else {
    SLI_CLOCK_MANAGER_LOG_WARN("HFXO CTUNE calibration failed, val=%u status=0x%x",
                               ctune, (uint32_t)status);
  }
  if (startup_measurement_stopped) {
    (void)sli_clock_manager_hal_start_hfxo_startup_time_measurement();
  }

  return status;
}

/***************************************************************************//**
 * Starts an HFXO startup time measurement. This function assumes execution in a critical section.
 ******************************************************************************/
sl_status_t sli_clock_manager_hal_start_hfxo_startup_time_measurement(void)
{
#if defined(SL_CLOCK_MANAGER_HFXO_STARTUP_TIME_MEASUREMENT_EN) \
  && (SL_CLOCK_MANAGER_HFXO_STARTUP_TIME_MEASUREMENT_EN == 1) \
  && defined(SL_CLOCK_MANAGER_HFXO_EN) && (SL_CLOCK_MANAGER_HFXO_EN == 1) \
  && (SL_CLOCK_MANAGER_HFXO_MODE == HFXO_CFG_MODE_XTAL)
  sl_status_t status = SL_STATUS_OK;

#if defined(SLI_CLOCK_MANAGER_RUNTIME_CONFIGURATION)
  if (SLI_CLOCK_MANAGER_HFXO_MODE != HFXO_CFG_MODE_XTAL) {
    return SL_STATUS_NOT_AVAILABLE;
  }
#endif

  if (clock_manager_hfxo_startup_measurement_active) {
    status = SL_STATUS_INVALID_STATE;
  } else {
    clock_manager_hfxo_startup_measurement_active = true;
    CMU->HFXO0LFCLKCTRL = (CMU->HFXO0LFCLKCTRL
                           & ~_CMU_HFXO0LFCLKCTRL_CLKSEL_MASK)
                          | SL_CLOCK_MANAGER_HFXO0LFCLK_SOURCE;
    HFXO0->CMD_SET = HFXO_CMD_STOPMEAS;
    while ((HFXO0->STATUS & _HFXO_STATUS_SYNCBUSY_MASK) != 0U) {
      // Wait for the stop command
    }
    HFXO0->IF_CLR = HFXO_IF_STUPMEASDONE;
    HFXO0->CMD_SET = HFXO_CMD_STARTMEAS;
  }

  return status;
#else
  return SL_STATUS_NOT_AVAILABLE;
#endif
}

/***************************************************************************//**
 * Stops the active HFXO startup time measurement.
 ******************************************************************************/
sl_status_t sli_clock_manager_hal_stop_hfxo_startup_time_measurement(void)
{
#if defined(SL_CLOCK_MANAGER_HFXO_STARTUP_TIME_MEASUREMENT_EN) \
  && (SL_CLOCK_MANAGER_HFXO_STARTUP_TIME_MEASUREMENT_EN == 1) \
  && defined(SL_CLOCK_MANAGER_HFXO_EN) && (SL_CLOCK_MANAGER_HFXO_EN == 1) \
  && (SL_CLOCK_MANAGER_HFXO_MODE == HFXO_CFG_MODE_XTAL)
  sl_status_t status = SL_STATUS_OK;

  if (!clock_manager_hfxo_startup_measurement_active) {
    status = SL_STATUS_INVALID_STATE;
  } else {
    HFXO0->CMD_SET = HFXO_CMD_STOPMEAS;
    while ((HFXO0->STATUS & _HFXO_STATUS_SYNCBUSY_MASK) != 0U) {
      // Wait for the stop command
    }

    HFXO0->IF_CLR = HFXO_IF_STUPMEASDONE;
    CMU->HFXO0LFCLKCTRL = (CMU->HFXO0LFCLKCTRL
                           & ~_CMU_HFXO0LFCLKCTRL_CLKSEL_MASK)
                          | _CMU_HFXO0LFCLKCTRL_CLKSEL_DISABLED;
    clock_manager_hfxo_startup_measurement_active = false;
  }

  return status;
#else
  return SL_STATUS_NOT_AVAILABLE;
#endif
}

/***************************************************************************//**
 * Gets the HFXO's average startup time.
 ******************************************************************************/
sl_status_t sli_clock_manager_hal_get_hfxo_average_startup_time(uint32_t *val)
{
#if defined(SL_CLOCK_MANAGER_HFXO_STARTUP_TIME_MEASUREMENT_EN) && SL_CLOCK_MANAGER_HFXO_STARTUP_TIME_MEASUREMENT_EN
  if (clock_manager_hfxo_startup_time != 0) {
    // We got a value, return ok.
    *val = clock_manager_hfxo_startup_time;
    return SL_STATUS_OK;
  } else {
    // Still waiting for a measure, return not ready.
    *val = SL_CLOCK_MANAGER_HFXO_STARTUP_TIME_DEFAULT_VALUE;
    return SL_STATUS_NOT_READY;
  }

#else
  (void) val;
  return SL_STATUS_NOT_SUPPORTED;
#endif
}

/***************************************************************************//**
 * Processes a completed HFXO startup time measurement.
 ******************************************************************************/
void sli_clock_manager_hal_process_hfxo_startup_time_measurement(void)
{
#if defined(SL_CLOCK_MANAGER_HFXO_STARTUP_TIME_MEASUREMENT_EN) \
  && (SL_CLOCK_MANAGER_HFXO_STARTUP_TIME_MEASUREMENT_EN == 1) \
  && defined(SL_CLOCK_MANAGER_HFXO_EN) && (SL_CLOCK_MANAGER_HFXO_EN == 1) \
  && (SL_CLOCK_MANAGER_HFXO_MODE == HFXO_CFG_MODE_XTAL)

  if (((HFXO0->IF & HFXO_IF_STUPMEASDONE) != 0U)
      && ((HFXO0->STATUS & HFXO_STATUS_STUPMEASBSY) == 0U)) {
    clock_manager_hfxo_startup_time = (HFXO0->AVGSTARTUPTIME
                                       & _HFXO_AVGSTARTUPTIME_AVGSTUP_MASK)
                                      >> _HFXO_AVGSTARTUPTIME_AVGSTUP_SHIFT;
    HFXO0->CMD_SET = HFXO_CMD_STOPMEAS;
    while ((HFXO0->STATUS & _HFXO_STATUS_SYNCBUSY_MASK) != 0U) {
      // Wait for the stop command
    }
    HFXO0->IF_CLR = HFXO_IF_STUPMEASDONE;
    CMU->HFXO0LFCLKCTRL = (CMU->HFXO0LFCLKCTRL
                           & ~_CMU_HFXO0LFCLKCTRL_CLKSEL_MASK)
                          | _CMU_HFXO0LFCLKCTRL_CLKSEL_DISABLED;
    clock_manager_hfxo_startup_measurement_active = false;
  }
#endif
}

/***************************************************************************//**
 * Sets LFXO frequency tuning control.
 ******************************************************************************/
sl_status_t sli_clock_manager_hal_set_lfxo_calibration(uint32_t val)
{
  CORE_DECLARE_IRQ_STATE;
  bool lfxo_lock_status = false;
  uint8_t ctune = 0;

  CORE_ENTER_ATOMIC();

  sl_clock_manager_enable_bus_clock(SL_BUS_CLOCK_LFXO);
  lfxo_lock_status = (LFXO->STATUS & _LFXO_STATUS_LOCK_MASK) >> _LFXO_STATUS_LOCK_SHIFT;
  // Unlock register interface if register is locked before
  if (lfxo_lock_status == _LFXO_STATUS_LOCK_LOCKED) {
    LFXO->LOCK = LFXO_LOCK_LOCKKEY_UNLOCK;
  }

  EFM_ASSERT(val <= (_LFXO_CAL_CAPTUNE_MASK >> _LFXO_CAL_CAPTUNE_SHIFT));
  // Max internal capacitance tuning value is 0x59 (29.1 pF)
  ctune = (uint8_t) SL_MIN(0x59U, (uint8_t)val);

  // Wait for CALBSY bit to clear before writing the tuning value to CAL register
  while (((LFXO->SYNCBUSY & _LFXO_SYNCBUSY_CAL_MASK) >> _LFXO_SYNCBUSY_CAL_SHIFT) != 0U) ;
  LFXO->CAL = (LFXO->CAL & ~_LFXO_CAL_CAPTUNE_MASK)
              | ((uint32_t)ctune << _LFXO_CAL_CAPTUNE_SHIFT);

  // Lock register interface again
  if (lfxo_lock_status == _LFXO_STATUS_LOCK_LOCKED) {
    LFXO->LOCK = ~LFXO_LOCK_LOCKKEY_UNLOCK;
  }

  CORE_EXIT_ATOMIC();

  SLI_CLOCK_MANAGER_LOG_INFO("LFXO calibration updated, val=%u", val);

  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Gets the LFXO frequency tuning setting.
 ******************************************************************************/
sl_status_t sli_clock_manager_hal_get_lfxo_calibration(uint32_t *val)
{
  sl_clock_manager_enable_bus_clock(SL_BUS_CLOCK_LFXO);
  *val = (LFXO->CAL & _LFXO_CAL_CAPTUNE_MASK) >> _LFXO_CAL_CAPTUNE_SHIFT;

  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Configures the RCO calibration.
 ******************************************************************************/
sl_status_t sli_clock_manager_hal_configure_rco_calibration(uint32_t cycles,
                                                            sl_clock_manager_clock_calibration_t down_counter_selection,
                                                            sl_clock_manager_clock_calibration_t up_counter_selection,
                                                            bool continuous_calibration)
{
  CORE_DECLARE_IRQ_STATE;

  CORE_ENTER_ATOMIC();

  // Keep untouched configuration settings
  uint32_t calCtrl = CMU->CALCTRL
                     & ~(_CMU_CALCTRL_UPSEL_MASK
                         | _CMU_CALCTRL_DOWNSEL_MASK);
  // Check for cycle count overflow
  EFM_ASSERT(cycles <= (_CMU_CALTOP_CALTOP_MASK >> _CMU_CALTOP_CALTOP_SHIFT));
  CMU->CALTOP = cycles << _CMU_CALTOP_CALTOP_SHIFT;

  // Set down counting clock source selector
  switch (down_counter_selection) {
    case SL_CLOCK_MANAGER_CLOCK_CALIBRATION_HCLK:
      calCtrl |= CMU_CALCTRL_DOWNSEL_HCLK;
      break;

    case SL_CLOCK_MANAGER_CLOCK_CALIBRATION_PRS:
      calCtrl |= CMU_CALCTRL_DOWNSEL_PRS;
      break;

    case SL_CLOCK_MANAGER_CLOCK_CALIBRATION_HFXO:
      calCtrl |= CMU_CALCTRL_DOWNSEL_HFXO;
      break;

    case SL_CLOCK_MANAGER_CLOCK_CALIBRATION_LFXO:
      calCtrl |= CMU_CALCTRL_DOWNSEL_LFXO;
      break;

    case SL_CLOCK_MANAGER_CLOCK_CALIBRATION_HFRCODPLL:
      calCtrl |= CMU_CALCTRL_DOWNSEL_HFRCODPLL;
      break;

    case SL_CLOCK_MANAGER_CLOCK_CALIBRATION_HFRCOEM23:
      calCtrl |= CMU_CALCTRL_DOWNSEL_HFRCOEM23;
      break;

    case SL_CLOCK_MANAGER_CLOCK_CALIBRATION_FSRCO:
      calCtrl |= CMU_CALCTRL_DOWNSEL_FSRCO;
      break;

    case SL_CLOCK_MANAGER_CLOCK_CALIBRATION_LFRCO:
      calCtrl |= CMU_CALCTRL_DOWNSEL_LFRCO;
      break;

    case SL_CLOCK_MANAGER_CLOCK_CALIBRATION_ULFRCO:
      calCtrl |= CMU_CALCTRL_DOWNSEL_ULFRCO;
      break;

    default:
      CORE_EXIT_ATOMIC();
      return SL_STATUS_NOT_AVAILABLE;
  }

  // Set up counting clock source selector
  switch (up_counter_selection) {
    case SL_CLOCK_MANAGER_CLOCK_CALIBRATION_PRS:
      calCtrl |= CMU_CALCTRL_UPSEL_PRS;
      break;

    case SL_CLOCK_MANAGER_CLOCK_CALIBRATION_HFXO:
      calCtrl |= CMU_CALCTRL_UPSEL_HFXO;
      break;

    case SL_CLOCK_MANAGER_CLOCK_CALIBRATION_LFXO:
      calCtrl |= CMU_CALCTRL_UPSEL_LFXO;
      break;

    case SL_CLOCK_MANAGER_CLOCK_CALIBRATION_HFRCODPLL:
      calCtrl |= CMU_CALCTRL_UPSEL_HFRCODPLL;
      break;

    case SL_CLOCK_MANAGER_CLOCK_CALIBRATION_HFRCOEM23:
      calCtrl |= CMU_CALCTRL_UPSEL_HFRCOEM23;
      break;

    case SL_CLOCK_MANAGER_CLOCK_CALIBRATION_FSRCO:
      calCtrl |= CMU_CALCTRL_UPSEL_FSRCO;
      break;

    case SL_CLOCK_MANAGER_CLOCK_CALIBRATION_LFRCO:
      calCtrl |= CMU_CALCTRL_UPSEL_LFRCO;
      break;

    case SL_CLOCK_MANAGER_CLOCK_CALIBRATION_ULFRCO:
      calCtrl |= CMU_CALCTRL_UPSEL_ULFRCO;
      break;

    case SL_CLOCK_MANAGER_CLOCK_CALIBRATION_HCLK:
      CORE_EXIT_ATOMIC();
      return SL_STATUS_NOT_SUPPORTED;

    default:
      CORE_EXIT_ATOMIC();
      return SL_STATUS_NOT_AVAILABLE;
  }

  CMU->CALCTRL = calCtrl;
  sl_hal_bus_reg_write_bit(&CMU->CALCTRL, _CMU_CALCTRL_CONT_SHIFT, (uint32_t)continuous_calibration);

  CORE_EXIT_ATOMIC();

  SLI_CLOCK_MANAGER_LOG_DEBUG("RCO calibration configured, cycles=%u down=%u up=%u",
                              cycles, (uint32_t)down_counter_selection, (uint32_t)up_counter_selection);

  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Starts the RCO calibration.
 ******************************************************************************/
void sli_clock_manager_hal_start_rco_calibration(void)
{
  CMU->CALCMD = CMU_CALCMD_CALSTART;

  SLI_CLOCK_MANAGER_LOG_DEBUG("RCO calibration started");
}

/***************************************************************************//**
 * Stops the RCO calibration.
 ******************************************************************************/
void sli_clock_manager_hal_stop_rco_calibration(void)
{
  CMU->CALCMD = CMU_CALCMD_CALSTOP;

  SLI_CLOCK_MANAGER_LOG_DEBUG("RCO calibration stopped");
}

/***************************************************************************//**
 * Waits for the RCO calibration to finish.
 ******************************************************************************/
void sli_clock_manager_hal_wait_rco_calibration(void)
{
  // Wait until calibration completes, UNLESS continuous calibration mode is on
  if ((CMU->CALCTRL & CMU_CALCTRL_CONT) == 0UL) {
    // Wait until calibration completes
    while ((CMU->STATUS & CMU_STATUS_CALRDY) == 0UL) ;
  }
}

/***************************************************************************//**
 * Gets calibration count value.
 ******************************************************************************/
sl_status_t sli_clock_manager_hal_get_rco_calibration_count(uint32_t *count)
{
  *count = CMU->CALCNT;

  SLI_CLOCK_MANAGER_LOG_DEBUG("RCO calibration result, count=%u", *count);

  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Sets SYSCLK clock source clock.
 *
 * @note On series 3, we actively wait for the oscillator to be ready before
 *       switching the SYSCLK branch. This is to not gate the SYSCLK branch
 *       during switching for other HW modules connected to SYSCLK branch.
 ******************************************************************************/
sl_status_t sli_clock_manager_hal_set_sysclk_source(sl_oscillator_t source)
{
  sl_status_t return_status = SL_STATUS_OK;
  CORE_DECLARE_IRQ_STATE;

  CORE_ENTER_ATOMIC();

  switch (source) {
    case SL_OSCILLATOR_FSRCO:
      CMU->SYSCLKCTRL = (CMU->SYSCLKCTRL & ~_CMU_SYSCLKCTRL_CLKSEL_MASK) | CMU_SYSCLKCTRL_CLKSEL_FSRCO;
      break;

    case SL_OSCILLATOR_HFXO:
      if ((HFXO0->STATUS & HFXO_STATUS_RDY) == 0) {
        HFXO0->CTRL_SET = HFXO_CTRL_FORCEEN;
        while ((HFXO0->STATUS & HFXO_STATUS_RDY) == 0) ;
      }
      CMU->SYSCLKCTRL = (CMU->SYSCLKCTRL & ~_CMU_SYSCLKCTRL_CLKSEL_MASK) | CMU_SYSCLKCTRL_CLKSEL_HFXO;
      HFXO0->CTRL_CLR = HFXO_CTRL_FORCEEN;
      break;

    case SL_OSCILLATOR_HFRCODPLL:
      if ((HFRCO0->STATUS & HFRCO_STATUS_RDY) == 0) {
        HFRCO0->CTRL_SET = HFRCO_CTRL_FORCEEN;
        while ((HFRCO0->STATUS & HFRCO_STATUS_RDY) == 0) ;
      }
      CMU->SYSCLKCTRL = (CMU->SYSCLKCTRL & ~_CMU_SYSCLKCTRL_CLKSEL_MASK) | CMU_SYSCLKCTRL_CLKSEL_HFRCODPLL;
      HFRCO0->CTRL_CLR = HFRCO_CTRL_FORCEEN;
      break;

    case SL_OSCILLATOR_CLKIN0:
      CMU->SYSCLKCTRL = (CMU->SYSCLKCTRL & ~_CMU_SYSCLKCTRL_CLKSEL_MASK) | CMU_SYSCLKCTRL_CLKSEL_CLKIN0;
      break;

#if (SOCPLL_OUTPUT_COUNT == 3)
    case SL_OSCILLATOR_SOCPLL0_OUT1:
#else
    case SL_OSCILLATOR_SOCPLL0:
#endif
      if ((SOCPLL0->STATUS & SOCPLL_STATUS_RDY) == 0) {
        SOCPLL0->CTRL_SET = SOCPLL_CTRL_FORCEEN;
#if defined(_SOCPLL_CTRL1_ENOPENLOOP_MASK)
        if ((SOCPLL0->CTRL1 & SOCPLL_CTRL1_ENOPENLOOP) == 0) {
          while ((SOCPLL0->STATUS & SOCPLL_STATUS_RDY) == 0) ;
        }
#else
        while ((SOCPLL0->STATUS & SOCPLL_STATUS_RDY) == 0) ;
#endif
      }
      CMU->SYSCLKCTRL = (CMU->SYSCLKCTRL & ~_CMU_SYSCLKCTRL_CLKSEL_MASK) | CMU_SYSCLKCTRL_CLKSEL_SOCPLL;
      SOCPLL0->CTRL_CLR = SOCPLL_CTRL_FORCEEN;
      break;

#if defined(CMU_SYSCLKCTRL_CLKSEL_SOCPLL1)
    case SL_OSCILLATOR_SOCPLL1_OUT1:
      if ((SOCPLL1->STATUS & SOCPLL_STATUS_RDY) == 0) {
        SOCPLL1->CTRL_SET = SOCPLL_CTRL_FORCEEN;
#if defined(_SOCPLL_CTRL1_ENOPENLOOP_MASK)
        if ((SOCPLL1->CTRL1 & SOCPLL_CTRL1_ENOPENLOOP) == 0) {
          while ((SOCPLL1->STATUS & SOCPLL_STATUS_RDY) == 0) ;
        }
#else
        while ((SOCPLL1->STATUS & SOCPLL_STATUS_RDY) == 0) ;
#endif
      }
      CMU->SYSCLKCTRL = (CMU->SYSCLKCTRL & ~_CMU_SYSCLKCTRL_CLKSEL_MASK) | CMU_SYSCLKCTRL_CLKSEL_SOCPLL1;
      SOCPLL1->CTRL_CLR = SOCPLL_CTRL_FORCEEN;
      break;
#endif

    default:
      return_status = SL_STATUS_INVALID_PARAMETER;
      break;
  }

  SystemCoreClockUpdate();
  CORE_EXIT_ATOMIC();

#if defined(SL_CATALOG_POWER_MANAGER_PRESENT)
  slx_power_manager_update_clock_info();
#endif

  return return_status;
}

/***************************************************************************//**
 * Gets SYSCLK clock source clock.
 ******************************************************************************/
sl_status_t sli_clock_manager_hal_get_sysclk_source(sl_oscillator_t *source)
{
  CORE_DECLARE_IRQ_STATE;

  CORE_ENTER_ATOMIC();

  switch (CMU->SYSCLKCTRL & _CMU_SYSCLKCTRL_CLKSEL_MASK) {
    case  CMU_SYSCLKCTRL_CLKSEL_FSRCO:
      *source = SL_OSCILLATOR_FSRCO;
      break;
    case  CMU_SYSCLKCTRL_CLKSEL_HFXO:
      *source = SL_OSCILLATOR_HFXO;
      break;
    case  CMU_SYSCLKCTRL_CLKSEL_HFRCODPLL:
      *source = SL_OSCILLATOR_HFRCODPLL;
      break;
    case  CMU_SYSCLKCTRL_CLKSEL_CLKIN0:
      *source = SL_OSCILLATOR_CLKIN0;
      break;
#if (SOCPLL_OUTPUT_COUNT == 3)
    case  CMU_SYSCLKCTRL_CLKSEL_SOCPLL:
      *source = SL_OSCILLATOR_SOCPLL0_OUT1;
      break;
#else
    case  CMU_SYSCLKCTRL_CLKSEL_SOCPLL:
      *source = SL_OSCILLATOR_SOCPLL0;
      break;
#endif
#if defined(CMU_SYSCLKCTRL_CLKSEL_SOCPLL1)
    case  CMU_SYSCLKCTRL_CLKSEL_SOCPLL1:
      *source = SL_OSCILLATOR_SOCPLL1_OUT1;
      break;
#endif
    default:
      EFM_ASSERT(false);
      break;
  }

  CORE_EXIT_ATOMIC();

  return SL_STATUS_OK;
}

/*******************************************************************************
 * HFXO interrupt handler.
 ******************************************************************************/
void HFXO_IRQ_HANDLER_FUNCTION(void)
{
  uint32_t irq_flag = HFXO0->IF;
  (void)irq_flag;

#if defined(SL_CLOCK_MANAGER_HFXO_SLEEPY_CRYSTAL_SUPPORT) && (SL_CLOCK_MANAGER_HFXO_SLEEPY_CRYSTAL_SUPPORT == 1)
  // SLEEPYXTAL Interrupt Flag Handling.
  if (irq_flag & HFXO_IF_SLEEPYXTAL) {
    // Clear error flag
    HFXO0->IF_CLR = irq_flag & HFXO_IF_SLEEPYXTAL;

    // Notification callback function
    sl_clock_manager_hfxo_notify_consecutive_failed_startups();
  }
#endif

  // Ready Interrupt Flag Handling.
#if !defined(SLI_SLEEPTIMER_SYSRTC_WITH_PRETRIGGERS)
  if (irq_flag & HFXO_IF_RDY) {
    // Clear Ready flag.
    HFXO0->IF_CLR = irq_flag & HFXO_IF_RDY;

    // Callback fuction to notify HFXO is ready.
    sli_clock_manager_notify_hfxo_ready();
  }
#endif
}

/***************************************************************************//**
 * Waits for USBPLL clock to be ready.
 ******************************************************************************/
sl_status_t sli_clock_manager_hal_wait_usbpll(void)
{
  return SL_STATUS_NOT_AVAILABLE;
}

/***************************************************************************//**
 * Sets the external FLASH reference clock.
 ******************************************************************************/
sl_status_t sli_clock_manager_hal_set_ext_flash_clk(sl_oscillator_t oscillator)
{
#if defined (_SILICON_LABS_32B_SERIES_3_CONFIG_301)
  sl_status_t status = SL_STATUS_OK;
  CORE_DECLARE_IRQ_STATE;

  // Check if the QSPI is already clocked by target oscillator.
  if (current_qspi_reference_clock == oscillator) {
    return status;
  }

  if (clock_manager_se_version >= 0x00202L) {
    switch (oscillator) {
      case SL_OSCILLATOR_FLPLL:
        if (clock_manager_flpll_cfg.ref_clock == SLI_SE_QSPI_REF_CLOCK_HFXO) {
          // Send the SE command to switch the QSPI clock to FLPLL with HFXO reference clock.
          CORE_ENTER_ATOMIC();
          status = sli_se_qspi_configure_clock_flpll(&clock_manager_cmd_ctx, &clock_manager_flpll_cfg);
          CORE_EXIT_ATOMIC();
        } else {
          status = SL_STATUS_NOT_AVAILABLE;
        }
        break;

      case SL_OSCILLATOR_FSRCO:
        // Send the SE command to switch QSPI clock to FSRCO
        CORE_ENTER_ATOMIC();
        status = sli_se_qspi_configure_clock_fsrco(&clock_manager_cmd_ctx);
        CORE_EXIT_ATOMIC();
        break;

      default:
        status = SL_STATUS_INVALID_PARAMETER;
        break;
    }

    if (status != SL_STATUS_OK) {
      return status;
    }
    // Update global QSPI reference clock variable.
    current_qspi_reference_clock = oscillator;
  } else {
    (void)oscillator;
  }

  return status;
#else
  (void)oscillator;
  return SL_STATUS_NOT_SUPPORTED;
#endif
}
/***************************************************************************//**
 * Gets the external FLASH clock source.
 ******************************************************************************/
sl_status_t sli_clock_manager_hal_get_ext_flash_clk(sl_oscillator_t *oscillator)
{
  if (oscillator == NULL) {
    return SL_STATUS_NULL_POINTER;
  }

#if defined (_SILICON_LABS_32B_SERIES_3_CONFIG_301)
  if (current_qspi_reference_clock == SL_OSCILLATOR_INVALID) {
    return SL_STATUS_NOT_AVAILABLE;
  }

  *oscillator = current_qspi_reference_clock;
  return SL_STATUS_OK;
#elif defined (_SILICON_LABS_32B_SERIES_3_CONFIG_353)
  switch (CMU->OSPI0CLKCTRL & _CMU_OSPI0CLKCTRL_CLKSEL_MASK) {
    case CMU_OSPI0CLKCTRL_CLKSEL_DISABLED:
      *oscillator = SL_OSCILLATOR_INVALID;
      break;

    case CMU_OSPI0CLKCTRL_CLKSEL_FSRCO:
      *oscillator = SL_OSCILLATOR_FSRCO;
      break;

    case CMU_OSPI0CLKCTRL_CLKSEL_SOCPLL0:
      *oscillator = SL_OSCILLATOR_SOCPLL0_OUT0;
      break;

    case CMU_OSPI0CLKCTRL_CLKSEL_HFRCODPLL:
      *oscillator = SL_OSCILLATOR_HFRCODPLL;
      break;

    case CMU_OSPI0CLKCTRL_CLKSEL_HFXO:
      *oscillator = SL_OSCILLATOR_HFXO;
      break;

    default:
      return SL_STATUS_INVALID_STATE;
  }

  return SL_STATUS_OK;
#else
  (void)oscillator;
  return SL_STATUS_NOT_SUPPORTED;
#endif
}

/***************************************************************************//**
 * Retrieves the FREQPLAN NWP SOCPLL config.
 ******************************************************************************/
sl_status_t sli_clock_manager_hal_get_nwp_socpll_freqplan_config(const uint16_t **socpll_freqplan_config,
                                                                 uint8_t *target_frequency_index)
{
#if defined(SLI_CLOCK_MANAGER_NWPSOCPLL_FREQPLAN_DATA)
  *socpll_freqplan_config = SLI_CLOCK_MANAGER_NWPSOCPLL_FREQPLAN_DATA->socpll_config;
  *target_frequency_index = SLI_CLOCK_MANAGER_NWPSOCPLL_FREQPLAN_DATA->freqplan_data.default_freqsel;
  return SL_STATUS_OK;
#else
  (void)socpll_freqplan_config;
  (void)target_frequency_index;
  return SL_STATUS_NOT_SUPPORTED;
#endif
}

/***************************************************************************//**
 * Retrieves the FREQPLAN NWP CLKMULT config.
 ******************************************************************************/
sl_status_t sli_clock_manager_hal_get_nwp_clkmult_freqplan_config(uint8_t clkmult_index,
                                                                  const uint8_t **nwp_clkmult_freqplan_config,
                                                                  uint8_t *target_frequency_index)
{
#if defined(SLI_CLOCK_MANAGER_NWPCLKMULT_FREQPLAN_DATA)
  if (clkmult_index == 0) {
    *nwp_clkmult_freqplan_config = SLI_CLOCK_MANAGER_NWPCLKMULT_FREQPLAN_DATA->clkmult0_config;
  } else if (clkmult_index == 1) {
    *nwp_clkmult_freqplan_config = SLI_CLOCK_MANAGER_NWPCLKMULT_FREQPLAN_DATA->clkmult1_config;
  } else {
    return SL_STATUS_INVALID_PARAMETER;
  }
  *target_frequency_index = SLI_CLOCK_MANAGER_NWPCLKMULT_FREQPLAN_DATA->freqplan_data.default_freqsel;

  return SL_STATUS_OK;
#else
  (void)clkmult_index;
  (void)nwp_clkmult_freqplan_config;
  (void)target_frequency_index;
  return SL_STATUS_NOT_SUPPORTED;
#endif
}

/***************************************************************************//**
 * Sets a runtime-configurable Clock Branch's clock-select mux and prescaler.
 *
 * @note Default implementation. Devices supporting runtime Clock Branch
 *       reconfiguration provide a strong implementation in their device
 *       specific HAL.
 ******************************************************************************/
SL_WEAK sl_status_t sli_clock_manager_hal_set_clock_branch_source(sl_clock_branch_t clock_branch,
                                                                  uint32_t clksel,
                                                                  uint32_t presc)
{
  (void)clock_branch;
  (void)clksel;
  (void)presc;
  return SL_STATUS_NOT_AVAILABLE;
}

/***************************************************************************//**
 * Enables or disables a Clock Branch's functional clock (CMU <BRANCH>CTRL EN).
 *
 * @note Default implementation. Devices whose Clock Branches expose a
 *       functional-clock EN bit (e.g. I2S0, CAN, EMAC, SDHC, RADIO) provide a
 *       strong implementation in their device specific HAL.
 ******************************************************************************/
SL_WEAK sl_status_t sli_clock_manager_hal_enable_clock_branch(sl_clock_branch_t clock_branch,
                                                              bool enable)
{
  (void)clock_branch;
  (void)enable;
  return SL_STATUS_NOT_AVAILABLE;
}

#if defined(PERPLL_RUNTIME_CONFIG_PRESENT)
/***************************************************************************//**
 * Computes the PERPLL output frequency for a given set of dividers.
 *
 * Fout = (Fref / 2) * ((DIVN + 2) + (DIVF / 16384)) / (DCODIV + 2) / 2^DIV2POW
 ******************************************************************************/
static uint32_t perpll_get_frequency(uint32_t reference_frequency,
                                     uint32_t divn,
                                     uint32_t divf,
                                     uint32_t dco_div,
                                     uint32_t div_2pow,
                                     bool fractional_en)
{
  uint64_t numerator = (uint64_t)(reference_frequency / 2UL)
                       * (((uint64_t)(divn + 2UL) * PERPLL_DIVF_RESOLUTION) + (fractional_en ? divf : 0UL));
  uint64_t denominator = ((uint64_t)PERPLL_DIVF_RESOLUTION * (dco_div + 2UL)) << div_2pow;

  return (uint32_t)(numerator / denominator);
}

/***************************************************************************//**
 * Applies a new set of dividers to a PERPLL instance and waits for it to
 * relock. The PLL is taken out of on-demand mode for the duration of the
 * reprogramming so that its output is stable again before returning.
 ******************************************************************************/
static void perpll_reprogram(uint8_t perpll_num,
                             uint32_t divn,
                             uint32_t divf,
                             uint32_t dco_div,
                             uint32_t div_2pow,
                             bool fractional_en,
                             uint32_t frequency)
{
  PERPLL_TypeDef *perpll = PERPLL(perpll_num);
  sl_status_t status;
  bool forceen_was_set;

  EFM_ASSERT(perpll != NULL);

  // The PERPLL bus clock is normally already enabled by the Clock Manager init,
  // but the caller may reach this point before or independently from it.
  status = sl_clock_manager_enable_bus_clock(PERPLL_RUNTIME_CONFIG_BUS_CLOCK);
  EFM_ASSERT(status == SL_STATUS_OK);

  // Unlock register interface.
  perpll->LOCKKEY = PERPLL_LOCKKEY_LOCKKEY_UNLOCK;

  // FORCEEN is normally clear. Save it so the original request state can be
  // restored after the temporary software clock request used to relock.
  forceen_was_set = ((perpll->CTRL & PERPLL_CTRL_FORCEEN) != 0U);

  // Disable clock on-demand while reconfiguring the PERPLL.
  perpll->CTRL_SET = PERPLL_CTRL_DISONDEMAND;
  perpll->CTRL_CLR = PERPLL_CTRL_FORCEEN;
  while ((perpll->STATUS & _PERPLL_STATUS_ENS_MASK) != 0U) {
    // Wait until PERPLL is disabled.
  }

  perpll->CTRL = (perpll->CTRL & ~(_PERPLL_CTRL_ENFRACN_MASK | _PERPLL_CTRL_PERPLLDIVN_MASK | _PERPLL_CTRL_PERPLLDIVF_MASK))
                 | (divn << _PERPLL_CTRL_PERPLLDIVN_SHIFT)
                 | ((fractional_en ? 1UL : 0UL) << _PERPLL_CTRL_ENFRACN_SHIFT)
                 | ((fractional_en ? divf : 0UL) << _PERPLL_CTRL_PERPLLDIVF_SHIFT);

  perpll->DCOCFG = (perpll->DCOCFG & ~(_PERPLL_DCOCFG_PERPLLDCOOUTDIV_MASK | _PERPLL_DCOCFG_PERPLLOUTDIV2POW_MASK))
                   | (dco_div << _PERPLL_DCOCFG_PERPLLDCOOUTDIV_SHIFT)
                   | (div_2pow << _PERPLL_DCOCFG_PERPLLOUTDIV2POW_SHIFT);

  // Enable signal for pllclk0 from GP_PLL48.
  perpll->CTRL_SET = PERPLL_CTRL_ENPLLCLKOUT0;

  // Force a clock request by software to update analog part of PERPLL.
  perpll->CTRL_SET = PERPLL_CTRL_FORCEEN;

  while ((perpll->STATUS & (PERPLL_STATUS_RDY | PERPLL_STATUS_PLLLOCK | PERPLL_STATUS_ENS))
         != (PERPLL_STATUS_RDY | PERPLL_STATUS_PLLLOCK | PERPLL_STATUS_ENS)) {
    // Wait for PERPLL lock and ready.
  }

  // Update CMSIS PERPLL frequency.
  SystemPERPLLClockSet(perpll_num, frequency);

  // Re-enable clock on-demand by hardware and restore FORCEEN.
  perpll->CTRL_CLR = PERPLL_CTRL_DISONDEMAND;
  if (forceen_was_set) {
    perpll->CTRL_SET = PERPLL_CTRL_FORCEEN;
  } else {
    perpll->CTRL_CLR = PERPLL_CTRL_FORCEEN;
  }
}
#endif

/***************************************************************************//**
 * Reprograms a PERPLL instance's dividers and waits for it to relock.
 ******************************************************************************/
sl_status_t sli_clock_manager_hal_set_perpll_frequency(uint8_t perpll_num,
                                                       uint32_t divn,
                                                       uint32_t divf,
                                                       uint32_t dco_div,
                                                       uint32_t div_2pow,
                                                       bool fractional_en)
{
#if defined(PERPLL_RUNTIME_CONFIG_PRESENT)
  if (perpll_num != PERPLL_RUNTIME_CONFIG_INSTANCE) {
    return SL_STATUS_NOT_AVAILABLE;
  }

  if ((divn > PERPLL_DIVN_MAX)
      || (divf > PERPLL_DIVF_MAX)
      || (dco_div > PERPLL_DCO_DIV_MAX)
      || (div_2pow > PERPLL_DIV_2POW_MAX)) {
    return SL_STATUS_INVALID_PARAMETER;
  }

  // DIVN cannot be 0 in Integer-N mode and DIVF cannot be 0 in Fractional-N mode.
  if (fractional_en ? (divf == 0U) : (divn == 0U)) {
    return SL_STATUS_INVALID_PARAMETER;
  }

  uint32_t frequency = perpll_get_frequency(SystemHFXOClockGet(), divn, divf, dco_div, div_2pow, fractional_en);

  perpll_reprogram(perpll_num, divn, divf, dco_div, div_2pow, fractional_en, frequency);

  return SL_STATUS_OK;
#else
  (void)perpll_num;
  (void)divn;
  (void)divf;
  (void)dco_div;
  (void)div_2pow;
  (void)fractional_en;
  return SL_STATUS_NOT_AVAILABLE;
#endif
}

/***************************************************************************//**
 * Reprograms a PERPLL instance to a predefined target frequency.
 ******************************************************************************/
sl_status_t sli_clock_manager_hal_set_perpll_predefined_frequency(uint8_t perpll_num,
                                                                  sli_clock_manager_perpll_predefined_frequency_t frequency)
{
#if defined(PERPLL_RUNTIME_CONFIG_PRESENT)
  if (perpll_num != PERPLL_RUNTIME_CONFIG_INSTANCE) {
    return SL_STATUS_NOT_AVAILABLE;
  }

  if ((uint32_t)frequency >= SLI_CLOCK_MANAGER_PERPLL_FREQ_COUNT) {
    return SL_STATUS_INVALID_PARAMETER;
  }

  const uint32_t *perpll_freq_config = sli_clock_manager_perpll_freq_table[frequency];

  // All predefined frequencies are expressed in Fractional-N mode.
  perpll_reprogram(perpll_num,
                   perpll_freq_config[SLI_CLOCK_MANAGER_PERPLL_FIELD_DIVN],
                   perpll_freq_config[SLI_CLOCK_MANAGER_PERPLL_FIELD_DIVF],
                   perpll_freq_config[SLI_CLOCK_MANAGER_PERPLL_FIELD_DCO_DIV],
                   perpll_freq_config[SLI_CLOCK_MANAGER_PERPLL_FIELD_DIV_2POW],
                   true,
                   perpll_freq_config[SLI_CLOCK_MANAGER_PERPLL_FIELD_FREQ]);

  return SL_STATUS_OK;
#else
  (void)perpll_num;
  (void)frequency;
  return SL_STATUS_NOT_AVAILABLE;
#endif
}

/***************************************************************************//**
 * Gets the register and bit for the bus clock.
 ******************************************************************************/
static sl_status_t bus_clock_get_register_info(sl_bus_clock_t module,
                                               volatile uint32_t **reg,
                                               uint32_t *bit)
{
  if (module == SL_BUS_CLOCK_INVALID) {
    return SL_STATUS_NOT_AVAILABLE;
  }

#if defined (_SILICON_LABS_32B_SERIES_3_CONFIG_381)
  if (*module == SL_BUS_CLOCK_NOT_OWNED_VALUE) {
    EFM_ASSERT(false);
    return SL_STATUS_NOT_AVAILABLE;
  }
#endif

#if defined (_SILICON_LABS_32B_SERIES_3_CONFIG_301)
  uint32_t clken_index;

  *bit = (*module & _BUS_CLOCK_CLKEN_BIT_MASK) >> _BUS_CLOCK_CLKEN_BIT_SHIFT;
  clken_index = (*module & _BUS_CLOCK_CLKENX_MASK) >> _BUS_CLOCK_CLKENX_SHIFT;

  if (clken_index == BUS_CLOCK_CLKEN0) {
    *reg = &CMU->CLKEN0;
  } else if (clken_index == BUS_CLOCK_CLKEN1) {
    *reg = &CMU->CLKEN1;
#if defined(_CMU_CLKENHV_MASK)
  } else if (clken_index == BUS_CLOCK_CLKENHV) {
    *reg = &CMU->CLKENHV;
#endif
  } else {
    return SL_STATUS_NOT_AVAILABLE;
  }
#else
  *reg = (uint32_t *)(*module);
  if (*reg == SL_BUS_CLOCK_INVALID || *reg == (uint32_t *)0xFFFFFFFF) {
    return SL_STATUS_NOT_AVAILABLE;
  }

  // The CLKEN field is always the first bit of the register
  *bit = 0;
#endif

  return SL_STATUS_OK;
}
