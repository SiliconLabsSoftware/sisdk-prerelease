/***************************************************************************//**
 * @file
 * @brief LED Boost DCDC example functions
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

#include "ledboost.h"
#include "em_device.h"
#include "sl_clock_manager.h"
#include "sl_hal_emu.h"

#if defined(SL_HAL_EMU_DCDC_BOOST_PRESENT) && defined(_DCDC_DVDDBBCFG_MASK)

/*******************************************************************************
 ***************************   LOCAL DEFINES   *********************************
 ******************************************************************************/
#define LEDBOOST_WAIT_LIMIT          2000000U   // busy-wait limit for the IRQ-completion
#define LEDBOOST_SETTLE_LONG_CYCLES  20000000U  // LEDVDD output settle.

/*******************************************************************************
 ***************************  LOCAL VARIABLES   ********************************
 ******************************************************************************/

static volatile bool ramp_complete = false;

/***************************************************************************//**
 * Wait for DCDC interrupt to fire.
 ******************************************************************************/
static bool ledboost_wait_if(uint32_t mask, uint32_t want)
{
  bool ok = false;

  /* LEDVDD output settle time. */
  for (volatile uint32_t i = 0U; i < LEDBOOST_SETTLE_LONG_CYCLES; i++) {
  }

  sl_hal_emu_dcdc_enable_interrupts(mask);
  NVIC_EnableIRQ(DCDC_IRQn);

  for (uint32_t n = 0U; n < LEDBOOST_WAIT_LIMIT; n++) {
    if ((sl_hal_emu_dcdc_get_pending_interrupts() & mask) == want) {
      sl_hal_emu_dcdc_clear_pending_interrupts(mask);
      ok = true;
      break;
    }
  }

  sl_hal_emu_dcdc_disable_interrupts((uint32_t)_DCDC_IEN_MASK);
  NVIC_DisableIRQ(DCDC_IRQn);
  NVIC_ClearPendingIRQ(DCDC_IRQn);

  return ok;
}

/***************************************************************************//**
 * DCDC interrupt handler.
 ******************************************************************************/
void DCDC_IRQHandler(void)
{
  sl_hal_emu_dcdc_disable_interrupts((uint32_t)_DCDC_IEN_MASK);
  NVIC_DisableIRQ(DCDC_IRQn);
  NVIC_ClearPendingIRQ(DCDC_IRQn);
}

/***************************************************************************//**
 * Initialize LED Boost example.
 ******************************************************************************/
void ledboost_init(void)
{
  /* Bus clock enable. */
  sl_clock_manager_enable_bus_clock(SL_BUS_CLOCK_DCDC);

  /* Clear all latched IF. */
  sl_hal_emu_dcdc_clear_pending_interrupts((uint32_t)_DCDC_IF_MASK);
  sl_hal_emu_dcdc_disable_interrupts((uint32_t)_DCDC_IEN_MASK);
  NVIC_DisableIRQ(DCDC_IRQn);
  NVIC_ClearPendingIRQ(DCDC_IRQn);

  /* Choose dual-output regulation. */
  sl_hal_emu_dcdc_set_regulation_type(SL_HAL_EMU_DCDC_REGULATION_TYPE_REGDVDDDEC);

  /* Initialize DCDC boost. */
  sl_hal_emu_dcdc_boost_init_t boost_cfg = SL_HAL_EMU_DCDC_BOOST_INIT_DEFAULT;
  sl_hal_emu_init_dcdc_boost(&boost_cfg);

  /* Clear pending interrupts. */
  sl_hal_emu_dcdc_clear_pending_interrupts(DCDC_IF_LEDVDDRAMPDONE
                                           | DCDC_IF_BOOSTPOSEDG);
  sl_hal_emu_dcdc_sync(_DCDC_SYNCBUSY_MASK);
  /* Ramp LEDVDD to 1.8 V. */
  sl_hal_emu_set_dcdc_boost_output_voltage(SL_HAL_EMU_DCDC_BOOST_OUTPUT_VOLTAGE_1V8);

  /* Wait for LEDVDDRAMPDONE. */
  (void)ledboost_wait_if(DCDC_IF_LEDVDDRAMPDONE, DCDC_IF_LEDVDDRAMPDONE);

  /* Clear pending interrupts. */
  sl_hal_emu_dcdc_clear_pending_interrupts(DCDC_IF_LEDVDDRAMPDONE
                                           | DCDC_IF_BOOSTPOSEDG);
  sl_hal_emu_dcdc_sync(_DCDC_SYNCBUSY_MASK);
  /* Set LEDVDD to 3.8 V. */
  sl_hal_emu_set_dcdc_boost_output_voltage(SL_HAL_EMU_DCDC_BOOST_OUTPUT_VOLTAGE_3V8);

  /* Wait for LEDVDDRAMPDONE. */
  (void)ledboost_wait_if(DCDC_IF_LEDVDDRAMPDONE, DCDC_IF_LEDVDDRAMPDONE);

  /* Power off the DCDC regulator. Set to default voltage(~3.3 V). */
  sl_hal_emu_dcdc_power_off();
}

/***************************************************************************//**
 * LED Boost process action.
 ******************************************************************************/
void ledboost_process_action(void)
{
  if (ramp_complete) {
    ramp_complete = false;
  }
}

#else /* !(SL_HAL_EMU_DCDC_BOOST_PRESENT && _DCDC_DVDDBBCFG_MASK) */

/***************************************************************************//**
 * Initialize LED Boost example (unsupported device: empty).
 ******************************************************************************/
void ledboost_init(void)
{
}

/***************************************************************************//**
 * LED Boost process action (unsupported device: empty).
 ******************************************************************************/
void ledboost_process_action(void)
{
}

#endif /* SL_HAL_EMU_DCDC_BOOST_PRESENT && _DCDC_DVDDBBCFG_MASK */
