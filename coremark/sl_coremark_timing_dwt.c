/***************************************************************************//**
 * @file
 * @brief DWT timing backend for CoreMark.
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

#include "sl_coremark_timing.h"

#include "sl_cycle_counter.h"

static sl_cycle_counter_handle_t coremark_cycle_counter;

void coremark_timing_init(void)
{
  (void)sl_cycle_counter_init(&coremark_cycle_counter);
  sl_cycle_counter_enable();
}

void coremark_timing_start(void)
{
  sl_cycle_counter_start(&coremark_cycle_counter);
}

void coremark_timing_stop(void)
{
  sl_cycle_counter_stop(&coremark_cycle_counter);
}

uint32_t coremark_timing_get_elapsed_ticks(void)
{
  return sl_cycle_counter_get_cycles(&coremark_cycle_counter);
}
