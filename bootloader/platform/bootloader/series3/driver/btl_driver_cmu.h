/***************************************************************************//**
 * @file
 * @brief Gecko bootloader driver cmu utility functions.
 *******************************************************************************
 * # License
 * <b>Copyright 2024 Silicon Laboratories Inc. www.silabs.com</b>
 *******************************************************************************
 *
 * The licensor of this software is Silicon Laboratories Inc.  Your use of this
 * software is governed by the terms of Silicon Labs Master Software License
 * Agreement (MSLA) available at
 * www.silabs.com/about-us/legal/master-software-license-agreement.  This
 * software is distributed to you in Source Code format and is governed by the
 * sections of the MSLA applicable to Source Code.
 *
 ******************************************************************************/
#ifndef BTL_DRIVER_CMU_H
#define BTL_DRIVER_CMU_H

#include "em_device.h"
#if defined(CMU_PRESENT)

#include <stdbool.h>
#include "sl_enum.h"
#ifdef __cplusplus
extern "C" {
#endif

/* EUSART clock IDs via CMU CLKEN1 — only on devices that define those shifts. */
#if defined(_CMU_CLKEN1_EUSART0_SHIFT) \
  || defined(_CMU_CLKEN1_EUSART1_SHIFT) \
  || defined(_CMU_CLKEN1_EUSART2_SHIFT)
/* Enable register bit positions, for internal use. */
#define CMU_EN_BIT_POS             0U
/* Enable register ID's for internal use. */
#define CMU_NO_EN_REG               0
#define CMU_CLKEN0_EN_REG           1
#define CMU_CLKEN1_EN_REG           2
#define CMU_EN_REG_POS              5U
/** Clock points in CMU clock-tree. */
SL_ENUM_GENERIC(CMU_Clock_TypeDef, uint32_t) {
#if defined(EUSART_PRESENT) && (EUSART_COUNT > 0) && defined(_CMU_CLKEN1_EUSART0_SHIFT)
  cmuClock_EUSART0 = (CMU_CLKEN1_EN_REG << CMU_EN_REG_POS)
                     | (_CMU_CLKEN1_EUSART0_SHIFT << CMU_EN_BIT_POS),           /**< EUSART0 clock. */
#endif
#if defined(EUSART_PRESENT) && (EUSART_COUNT > 1) && defined(_CMU_CLKEN1_EUSART1_SHIFT)
  cmuClock_EUSART1 = (CMU_CLKEN1_EN_REG << CMU_EN_REG_POS)
                     | (_CMU_CLKEN1_EUSART1_SHIFT << CMU_EN_BIT_POS),           /**< EUSART1 clock. */
#endif
#if defined(EUSART_PRESENT) && (EUSART_COUNT > 2) && defined(_CMU_CLKEN1_EUSART2_SHIFT)
  cmuClock_EUSART2 = (CMU_CLKEN1_EN_REG << CMU_EN_REG_POS)
                     | (_CMU_CLKEN1_EUSART2_SHIFT << CMU_EN_BIT_POS),           /**< EUSART2 clock. */
#endif
};
#endif /* _CMU_CLKEN1_EUSART*_SHIFT */

/*
 * Bus-clock helpers for Series 3 per-peripheral *CLKCTRL and legacy
 * CLKEN0/CLKEN1 models. Prefer *CLKCTRL when both are present (matches
 * clock manager / device_manager); fall back to CLKEN on parts that
 * only expose that interface.
 */

/** Enable the GPIO bus clock for the current device. */
static inline void btl_driver_enable_gpio_clock(void)
{
#if defined(CMU_GPIOCLKCTRL_CLKEN)
  CMU->GPIOCLKCTRL_SET = CMU_GPIOCLKCTRL_CLKEN;
#elif defined(CMU_CLKENHV_GPIO)
  CMU->CLKENHV_SET = CMU_CLKENHV_GPIO;
#elif defined(CMU_CLKEN0_GPIO)
  CMU->CLKEN0_SET = CMU_CLKEN0_GPIO;
#endif
}

/** Disable the GPIO bus clock for the current device. */
static inline void btl_driver_disable_gpio_clock(void)
{
#if defined(CMU_GPIOCLKCTRL_CLKEN)
  CMU->GPIOCLKCTRL_CLR = CMU_GPIOCLKCTRL_CLKEN;
#elif defined(CMU_CLKENHV_GPIO)
  CMU->CLKENHV_CLR = CMU_CLKENHV_GPIO;
#elif defined(CMU_CLKEN0_GPIO)
  CMU->CLKEN0_CLR = CMU_CLKEN0_GPIO;
#endif
}

/** Enable the TIMER0 bus clock. */
static inline void btl_driver_enable_timer0_clock(void)
{
#if defined(CMU_TIMER0CLKCTRL_CLKEN)
  CMU->TIMER0CLKCTRL_SET = CMU_TIMER0CLKCTRL_CLKEN;
#elif defined(CMU_CLKEN0_TIMER0)
  CMU->CLKEN0_SET = CMU_CLKEN0_TIMER0;
#endif
}

/** Enable the LFRCO bus clock. */
static inline void btl_driver_enable_lfrco_clock(void)
{
#if defined(CMU_LFRCOCLKCTRL_CLKEN)
  CMU->LFRCOCLKCTRL_SET = CMU_LFRCOCLKCTRL_CLKEN;
#elif defined(CMU_CLKEN0_LFRCO)
  CMU->CLKEN0_SET = CMU_CLKEN0_LFRCO;
#endif
}

/** Disable the LFRCO bus clock. */
static inline void btl_driver_disable_lfrco_clock(void)
{
#if defined(CMU_LFRCOCLKCTRL_CLKEN)
  CMU->LFRCOCLKCTRL_CLR = CMU_LFRCOCLKCTRL_CLKEN;
#elif defined(CMU_CLKEN0_LFRCO)
  CMU->CLKEN0_CLR = CMU_CLKEN0_LFRCO;
#endif
}

/** Enable the GPCRC0 bus clock. */
static inline void btl_driver_enable_gpcrc0_clock(void)
{
#if defined(CMU_GPCRC0CLKCTRL_CLKEN)
  CMU->GPCRC0CLKCTRL_SET = CMU_GPCRC0CLKCTRL_CLKEN;
#elif defined(CMU_CLKEN0_GPCRC0)
  CMU->CLKEN0_SET = CMU_CLKEN0_GPCRC0;
#elif defined(CMU_CLKEN0_GPCRC)
  CMU->CLKEN0_SET = CMU_CLKEN0_GPCRC;
#endif
}

/** Enable the SE mailbox host bus clock. */
static inline void btl_driver_enable_semailbox_clock(void)
{
#if defined(CMU_SEMAILBOXCLKCTRL_CLKEN)
  CMU->SEMAILBOXCLKCTRL_SET = CMU_SEMAILBOXCLKCTRL_CLKEN;
#elif defined(CMU_CLKEN1_SEMAILBOXHOST)
  CMU->CLKEN1_SET = CMU_CLKEN1_SEMAILBOXHOST;
#endif
}

#ifdef __cplusplus
}
#endif
#endif /* defined(CMU_PRESENT) */
#endif
