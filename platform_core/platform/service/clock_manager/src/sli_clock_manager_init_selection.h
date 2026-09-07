/***************************************************************************//**
 * @file
 * @brief Clock Manager INIT Selection.
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

#ifndef SLI_CLOCK_MANAGER_INIT_SELECTION_H
#define SLI_CLOCK_MANAGER_INIT_SELECTION_H

#include "sl_clock_manager_oscillator_config.h"
#include "sl_clock_manager_tree_config.h"
#include "em_device.h"

#ifdef __cplusplus
extern "C" {
#endif

/*******************************************************************************
 *************************   INIT OSCILLATORS   ********************************
 ******************************************************************************/

#if defined(SL_CLOCK_MANAGER_HFXO_EN) && (SL_CLOCK_MANAGER_HFXO_EN == 1)
#define SLI_CLOCK_MANAGER_INIT_HFXO
#endif

#if defined(SL_CLOCK_MANAGER_LFXO_EN) && (SL_CLOCK_MANAGER_LFXO_EN == 1)
#define SLI_CLOCK_MANAGER_INIT_LFXO
#endif

#define SLI_CLOCK_MANAGER_INIT_HFRCODPLL0
#if defined(SLI_CLOCK_MANAGER_RUNTIME_CONFIGURATION)
// Runtime configuration can enable or disable DPLL after build time.
#define SLI_CLOCK_MANAGER_INIT_DPLL0
#define SLI_CLOCK_MANAGER_INIT_HFRCO0
#elif defined(SL_CLOCK_MANAGER_HFRCO_DPLL_EN) && (SL_CLOCK_MANAGER_HFRCO_DPLL_EN == 1)
#define SLI_CLOCK_MANAGER_INIT_DPLL0
#else
#define SLI_CLOCK_MANAGER_INIT_HFRCO0
#endif

#define SLI_CLOCK_MANAGER_INIT_HFRCOEM23

#define SLI_CLOCK_MANAGER_INIT_LFRCO

#if (defined(SL_CLOCK_MANAGER_SYSCLK_SOURCE) && (SL_CLOCK_MANAGER_SYSCLK_SOURCE == CMU_SYSCLKCTRL_CLKSEL_CLKIN0)) \
  || (defined(SL_CLOCK_MANAGER_DPLL_REFCLK) && (SL_CLOCK_MANAGER_DPLL_REFCLK == CMU_DPLLREFCLKCTRL_CLKSEL_CLKIN0))
#define SLI_CLOCK_MANAGER_INIT_CLKIN0
#endif

#if defined(SLI_CLOCK_MANAGER_RUNTIME_CONFIGURATION)                           \
  || (defined(SL_CLOCK_MANAGER_SOCPLL_EN) && (SL_CLOCK_MANAGER_SOCPLL_EN == 1)) \
  || (defined(SL_CLOCK_MANAGER_SOCPLL0_EN) && (SL_CLOCK_MANAGER_SOCPLL0_EN == 1))
#define SLI_CLOCK_MANAGER_INIT_SOCPLL0
#endif

#if (defined(SLI_CLOCK_MANAGER_RUNTIME_CONFIGURATION) && defined(SOCPLL1)) \
  || (defined(SL_CLOCK_MANAGER_SOCPLL1_EN) && (SL_CLOCK_MANAGER_SOCPLL1_EN == 1))
#define SLI_CLOCK_MANAGER_INIT_SOCPLL1
#endif

#if (defined(SLI_CLOCK_MANAGER_RUNTIME_CONFIGURATION) && defined(SOCPLL2)) \
  || (defined(SL_CLOCK_MANAGER_SOCPLL2_EN) && (SL_CLOCK_MANAGER_SOCPLL2_EN == 1))
#define SLI_CLOCK_MANAGER_INIT_SOCPLL2
#endif

/*******************************************************************************
 *************************   INIT CLOCK BRANCHES   *****************************
 ******************************************************************************/

#if defined(SL_CLOCK_MANAGER_SYSCLK_SOURCE)
#define SLI_CLOCK_MANAGER_INIT_SYSCLK
#endif
#if defined(SL_CLOCK_MANAGER_TRACECLK_SOURCE) || defined(SL_CLOCK_MANAGER_TRACECLK_DIVIDER)
#define SLI_CLOCK_MANAGER_INIT_TRACECLK
#endif
#if defined(SL_CLOCK_MANAGER_OSPI0CLK_SOURCE)
#define SLI_CLOCK_MANAGER_INIT_OSPI0CLK
#endif
#if defined(SL_CLOCK_MANAGER_EM01GRPACLK_SOURCE)
#define SLI_CLOCK_MANAGER_INIT_EM01GRPACLK
#endif
#if defined(SL_CLOCK_MANAGER_EM01GRPCCLK_SOURCE)
#define SLI_CLOCK_MANAGER_INIT_EM01GRPCCLK
#endif
#if defined(SL_CLOCK_MANAGER_EM01GRPDCLK_SOURCE)
#define SLI_CLOCK_MANAGER_INIT_EM01GRPDCLK
#endif
#if defined(SL_CLOCK_MANAGER_EM23GRPACLK_SOURCE)
#define SLI_CLOCK_MANAGER_INIT_EM23GRPACLK
#endif
#if defined(SL_CLOCK_MANAGER_EM4GRPACLK_SOURCE)
#define SLI_CLOCK_MANAGER_INIT_EM4GRPACLK
#endif
#if defined(SL_CLOCK_MANAGER_ADCCLK_SOURCE)
#define SLI_CLOCK_MANAGER_INIT_ADCCLK
#endif
#if defined(SL_CLOCK_MANAGER_PIXELRZCLK_SOURCE)
#define SLI_CLOCK_MANAGER_INIT_PIXELRZCLK
#endif
#if defined(SL_CLOCK_MANAGER_SYSRTCCLK_SOURCE)
#define SLI_CLOCK_MANAGER_INIT_SYSRTC0CLK
#endif
#if defined(SL_CLOCK_MANAGER_WDOG0CLK_SOURCE)
#define SLI_CLOCK_MANAGER_INIT_WDOG0CLK
#endif
#if defined(SL_CLOCK_MANAGER_WDOG1CLK_SOURCE)
#define SLI_CLOCK_MANAGER_INIT_WDOG1CLK
#endif
#if defined(SL_CLOCK_MANAGER_PCNT0CLK_SOURCE)
#define SLI_CLOCK_MANAGER_INIT_PCNT0CLK
#endif
#if defined(SL_CLOCK_MANAGER_EUSART0CLK_SOURCE)
#define SLI_CLOCK_MANAGER_INIT_EUSART0CLK
#endif
#if defined(SL_CLOCK_MANAGER_EUSART1CLK_SOURCE)
#define SLI_CLOCK_MANAGER_INIT_EUSART1CLK
#endif
#if defined(SL_CLOCK_MANAGER_I2C0CLK_SOURCE)
#define SLI_CLOCK_MANAGER_INIT_I2C0CLK
#endif
#if defined(SL_CLOCK_MANAGER_SYSTICKCLK_SOURCE)
#define SLI_CLOCK_MANAGER_INIT_SYSTICKCLK
#endif
#if defined(SL_CLOCK_MANAGER_VDAC0CLK_SOURCE)
#define SLI_CLOCK_MANAGER_INIT_VDAC0CLK
#endif

#ifdef __cplusplus
}
#endif

#endif /* SLI_CLOCK_MANAGER_INIT_SELECTION_H */
