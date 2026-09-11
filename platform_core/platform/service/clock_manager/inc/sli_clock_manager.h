/***************************************************************************//**
 * @file
 * @brief Clock Manager Private API definition.
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

#ifndef SLI_CLOCK_MANAGER_H
#define SLI_CLOCK_MANAGER_H

#include "sl_clock_manager.h"
#include "sl_status.h"
#include "sl_compiler.h"
#include "sl_code_classification.h"

#if defined(SL_COMPONENT_CATALOG_PRESENT)
#include "sl_component_catalog.h"
#endif

#if defined(SL_CATALOG_CLOCK_MANAGER_PRESENT)
#include "em_device.h"
#include "sl_clock_manager_oscillator_config.h"
#if defined(SL_CLOCK_MANAGER_HFXO_MODE) && (SL_CLOCK_MANAGER_HFXO_MODE != HFXO_CFG_MODE_XTAL) \
  && (!defined(SL_CLOCK_MANAGER_HFXO_CRYSTAL_SHARING_EN) || (SL_CLOCK_MANAGER_HFXO_CRYSTAL_SHARING_EN == 0)) \
  && !defined(SLI_CLOCK_MANAGER_RUNTIME_CONFIGURATION)
#define SLI_HFXO_BYPASS_MODE
#endif
#endif

#ifdef __cplusplus
extern "C" {
#endif

/// Predefined PERPLL target frequencies.
/// Same value space as SL_CLOCK_MANAGER_PERPLL<n>_FREQ in
/// sl_clock_manager_oscillator_config.h.
/// Used with sli_clock_manager_set_perpll_predefined_frequency().
SL_ENUM(sli_clock_manager_perpll_predefined_frequency_t) {
  SLI_CLOCK_MANAGER_PERPLL_PREDEFINED_FREQ_14112000 = 0,  ///< 14.112 MHz
  SLI_CLOCK_MANAGER_PERPLL_PREDEFINED_FREQ_15360000,      ///< 15.36 MHz
  SLI_CLOCK_MANAGER_PERPLL_PREDEFINED_FREQ_16384000,      ///< 16.384 MHz
  SLI_CLOCK_MANAGER_PERPLL_PREDEFINED_FREQ_16934400,      ///< 16.9344 MHz
  SLI_CLOCK_MANAGER_PERPLL_PREDEFINED_FREQ_18432000,      ///< 18.432 MHz
  SLI_CLOCK_MANAGER_PERPLL_PREDEFINED_FREQ_20000000,      ///< 20 MHz
  SLI_CLOCK_MANAGER_PERPLL_PREDEFINED_FREQ_20480000,      ///< 20.48 MHz
  SLI_CLOCK_MANAGER_PERPLL_PREDEFINED_FREQ_22579200,      ///< 22.5792 MHz
  SLI_CLOCK_MANAGER_PERPLL_PREDEFINED_FREQ_24576000,      ///< 24.576 MHz
  SLI_CLOCK_MANAGER_PERPLL_PREDEFINED_FREQ_40000000,      ///< 40 MHz
  SLI_CLOCK_MANAGER_PERPLL_PREDEFINED_FREQ_48000000,      ///< 48 MHz
  SLI_CLOCK_MANAGER_PERPLL_PREDEFINED_FREQ_50000000,      ///< 50 MHz
};

/***************************************************************************//**
 * When this callback function is called, it means that HFXO is ready.
 ******************************************************************************/
__WEAK void sli_clock_manager_notify_hfxo_ready(void);

/***************************************************************************//**
 * Gets the HFXO's average startup time.
 *
 * @param[out] val  The HFXO's AVGSTUP value.
 *
 * @return  Status code.
 *          SL_STATUS_OK if successful.
 *          SL_STATUS_NOT_READY if measure is not done yet.
 *          SL_STATUS_NOT_SUPPORTED if not supported.
 *
 ******************************************************************************/
SL_CODE_CLASSIFY(SL_CODE_COMPONENT_CLOCK_MANAGER, SL_CODE_CLASS_TIME_CRITICAL)
sl_status_t sli_clock_manager_get_hfxo_average_startup_time(uint32_t *val);

/***************************************************************************//**
 * Processes a completed HFXO startup time measurement.
 ******************************************************************************/
SL_CODE_CLASSIFY(SL_CODE_COMPONENT_CLOCK_MANAGER, SL_CODE_CLASS_TIME_CRITICAL)
void sli_clock_manager_process_hfxo_startup_time_measurement(void);

/***************************************************************************//**
 * Retrieves the FREQPLAN NWP SOCPLL config.
 *
 * @param[out]  socpll_freqplan_config Table with SOCPLL FREQPLAN configurations
 *   for FREQPLAN_FREQSEL_WIDTH frequencies.
 *
 * @param[out]  target_frequency_index Index of the target frequency in the
 *   table with SOCPLL FREQPLAN configurations.
 *
 * @return  Status code.
 *          SL_STATUS_OK if successful. Error code otherwise.
 ******************************************************************************/
sl_status_t sli_clock_manager_get_nwp_socpll_freqplan_config(const uint16_t **socpll_freqplan_config,
                                                             uint8_t *target_frequency_index);

/***************************************************************************//**
 * Retrieves the FREQPLAN NWP CLKMULT config.
 *
 * @param[in]   clkmult_index Index of the CLKMULT instance (0 or 1).
 *
 * @param[out]  nwp_clkmult_freqplan_config Table with CLKMULT FREQPLAN
 *   configurations for FREQPLAN_FREQSEL_WIDTH frequencies.
 *
 * @param[out]  target_frequency_index Index of the target frequency in the
 *   table with CLKMULT FREQPLAN configurations.
 *
 * @return  Status code.
 *          SL_STATUS_OK if successful. Error code otherwise.
 ******************************************************************************/
sl_status_t sli_clock_manager_get_nwp_clkmult_freqplan_config(uint8_t clkmult_index,
                                                              const uint8_t **nwp_clkmult_freqplan_config,
                                                              uint8_t *target_frequency_index);

/***************************************************************************//**
 * Sets a runtime-configurable Clock Branch's clock-select mux and prescaler.
 *
 * @param[in] clock_branch  Clock Branch to reconfigure.
 *
 * @param[in] clksel        Pre-shifted CLKSEL field value for that branch's CMU
 *   <BRANCH>CTRL register (e.g. CMU_I2S0CLKCTRL_CLKSEL_*).
 *
 * @param[in] presc         Pre-shifted PRESC field value for that branch's CMU
 *   <BRANCH>CTRL register (e.g. CMU_I2S0CLKCTRL_PRESC_*).
 *
 * @return  Status code.
 *          SL_STATUS_OK if successful.
 *          SL_STATUS_NOT_AVAILABLE if clock_branch is not supported by this
 *            function on the current device: only SL_CLOCK_BRANCH_I2S0CLK is
 *            supported today.
 *          SL_STATUS_INVALID_PARAMETER if clksel or presc has bits outside of
 *            its CMU register field.
 *
 * @note This is an advanced function. This bypasses Clock Manager's normal
 *       compile-time-only clock tree configuration. The caller is responsible
 *       for making sure the branch's consumer(s) can tolerate the clock change
 *       (e.g. stopped or otherwise quiesced), since Clock Manager has no
 *       visibility into whether a peripheral fed by the branch is currently
 *       active.
 ******************************************************************************/
sl_status_t sli_clock_manager_set_clock_branch_source(sl_clock_branch_t clock_branch,
                                                      uint32_t clksel,
                                                      uint32_t presc);

/***************************************************************************//**
 * Enables or disables a Clock Branch's functional clock (CMU <BRANCH>CTRL EN).
 *
 * Some peripherals (I2S0, CAN, EMAC, SDHC, RADIO) request their functional
 * clock with the CMU <BRANCH>CTRL EN bit instead of an on-demand request from
 * a peripheral EN register. This is distinct from the bus clock (CLKEN), which
 * is still controlled with sl_clock_manager_enable_bus_clock() /
 * sl_clock_manager_disable_bus_clock().
 *
 * @param[in] clock_branch  Clock Branch whose functional clock to enable or
 *   disable.
 *
 * @param[in] enable        true to set EN, false to clear EN.
 *
 * @return  Status code.
 *          SL_STATUS_OK if successful.
 *          SL_STATUS_NOT_AVAILABLE if clock_branch is not supported by this
 *            function on the current device. Supported today:
 *            SL_CLOCK_BRANCH_I2S0CLK, SL_CLOCK_BRANCH_CAN0CLK,
 *            SL_CLOCK_BRANCH_CAN1CLK, SL_CLOCK_BRANCH_EMAC0CLK,
 *            SL_CLOCK_BRANCH_SDHC0CLK, SL_CLOCK_BRANCH_HCLKRADIO.
 *
 * @note Enabling I2S0CLK while it is sourced from PERPLL1 keeps that PERPLL
 *       requested after Clock Manager returns it to on-demand mode.
 ******************************************************************************/
sl_status_t sli_clock_manager_enable_clock_branch(sl_clock_branch_t clock_branch,
                                                  bool enable);

/***************************************************************************//**
 * Reprograms a PERPLL instance's dividers at runtime and waits for it to
 * relock.
 *
 * @param[in] perpll_num      PERPLL instance index (e.g. 1 for PERPLL1).
 *
 * @param[in] divn            PERPLLDIVN field value.
 *
 * @param[in] divf            PERPLLDIVF field value (used only if
 *   fractional_en).
 *
 * @param[in] dco_div         PERPLLDCOOUTDIV field value.
 *
 * @param[in] div_2pow        PERPLLOUTDIV2POW field value.
 *
 * @param[in] fractional_en   True for fractional-N mode, false for integer-N
 *   mode.
 *
 * @return  Status code.
 *          SL_STATUS_OK if successful.
 *          SL_STATUS_NOT_AVAILABLE if perpll_num is not marked
 *            runtime-configurable, or if the device has no PERPLL. Only PERPLL1
 *            is supported today, and only when the PERPLL1 runtime
 *            configuration component is part of the project.
 *          SL_STATUS_INVALID_PARAMETER if a divider is out of range, if divn is
 *            0 in integer-N mode or if divf is 0 in fractional-N mode.
 *
 * @note This is an advanced function. Changing a PERPLL's frequency at runtime
 *       will affect every Clock Branch fed by that PERPLL and could affect
 *       their operation and/or performance. The caller is responsible for
 *       making sure this is safe to do when calling this function.
 ******************************************************************************/
sl_status_t sli_clock_manager_set_perpll_frequency(uint8_t perpll_num,
                                                   uint32_t divn,
                                                   uint32_t divf,
                                                   uint32_t dco_div,
                                                   uint32_t div_2pow,
                                                   bool fractional_en);

/***************************************************************************//**
 * Reprograms a PERPLL instance to one of the predefined target frequencies
 * from the built-in PERPLL frequency table (same table used at compile-time by
 * the Clock Manager init). The function determines the correct divider
 * settings automatically based on the current HFXO reference clock frequency.
 *
 * @param[in] perpll_num  PERPLL instance index (e.g. 1 for PERPLL1).
 *
 * @param[in] frequency   Predefined target frequency. See
 *   sli_clock_manager_perpll_predefined_frequency_t.
 *
 * @return  Status code.
 *          SL_STATUS_OK if successful.
 *          SL_STATUS_NOT_AVAILABLE if perpll_num is not marked
 *            runtime-configurable, or if the device has no PERPLL. Only PERPLL1
 *            is supported today, and only when the PERPLL1 runtime
 *            configuration component is part of the project.
 *          SL_STATUS_INVALID_PARAMETER if frequency is out of range or the HFXO
 *            reference clock is not one of the supported frequencies
 *            (38 / 38.4 / 40 MHz).
 *
 * @note This is an advanced function. Changing a PERPLL's frequency at runtime
 *       will affect every Clock Branch fed by that PERPLL and could affect
 *       their operation and/or performance.
 ******************************************************************************/
sl_status_t sli_clock_manager_set_perpll_predefined_frequency(uint8_t perpll_num,
                                                              sli_clock_manager_perpll_predefined_frequency_t frequency);

#if defined (SL_CLOCK_MANAGER_SYSCLK_OSPI_DYNAMIC_CFG_EN) && (SL_CLOCK_MANAGER_SYSCLK_OSPI_DYNAMIC_CFG_EN == 1)
/***************************************************************************//**
 * Sets the Radio Friendly mode state.
 *
 * @param[in] enable  true to enable RF-friendly mode, false to disable.
 *
 * @return  Status code.
 *          SL_STATUS_OK if successful. Error code otherwise.
 *
 * @note SYSCLK and OSPICLK branches can create radio interference depending on
 *       their frequency and the radio channels being used. To reduce
 *       interference, the FREQPLAN module is used to dynamically frequency
 *       plan the System Clock and OSPI clocks when SOCPLL is used as source.
 *       Since SOCPLL consumes more power than other oscillators, it can be wise
 *       to switch the SYSCLK and OSPICLKs oscillators for period of No-RF
 *       activities known by the application.
 *
 * @note This feature is only available with the
 *       SL_CLOCK_MANAGER_SYSCLK_OSPI_DYNAMIC_CFG_EN configurations enabled.
 ******************************************************************************/
sl_status_t sli_clock_manager_set_rf_friendly_clock_mode(bool enable);

/***************************************************************************//**
 * Gets the current RF-friendly clock mode state.
 *
 * @return  bool true if RF-friendly mode is enabled, false otherwise.
 *
 * @note This feature is only available with the
 *       SL_CLOCK_MANAGER_SYSCLK_OSPI_DYNAMIC_CFG_EN configuration enabled.
 ******************************************************************************/
bool sli_clock_manager_clock_mode_is_rf_friendly(void);

#endif // defined (SL_CLOCK_MANAGER_SYSCLK_OSPI_DYNAMIC_CFG_EN) && (SL_CLOCK_MANAGER_SYSCLK_OSPI_DYNAMIC_CFG_EN == 1)

#ifdef __cplusplus
}
#endif

#endif // SLI_CLOCK_MANAGER_H
