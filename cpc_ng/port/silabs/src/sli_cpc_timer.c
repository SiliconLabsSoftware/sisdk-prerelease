/***************************************************************************/ /**
 * @file
 * @brief Implementation of CPC Timer API for EFR32 Series.
 *******************************************************************************
 * # License
 * <b>Copyright 2024 Silicon Laboratories Inc. www.silabs.com</b>
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
#include <string.h>

#include "sl_sleeptimer.h"
#include "sl_status.h"

#include "../../../src/sli_cpc_timer.h"

/***************************************************************************/ /**
 * Initialize the CPC timer.
 ******************************************************************************/
sl_status_t sli_cpc_timer_init(sli_cpc_timer_t *timer)
{
  memset(timer, 0, sizeof(*timer));

  return SL_STATUS_OK;
}

/***************************************************************************/ /**
 * Get the CPC timer frequency.
 ******************************************************************************/
uint32_t sli_cpc_timer_get_timer_frequency(void)
{
  return sl_sleeptimer_get_timer_frequency();
}

/***************************************************************************/ /**
 * Converts milliseconds into ticks.
 ******************************************************************************/
uint32_t sli_cpc_timer_ms_to_tick(uint16_t time_ms)
{
  return sl_sleeptimer_ms_to_tick(time_ms);
}

/***************************************************************************/ /**
 * Converts ticks in milliseconds.
 ******************************************************************************/
uint32_t sli_cpc_timer_tick_to_ms(uint32_t tick)
{
  return sl_sleeptimer_tick_to_ms(tick);
}

/***************************************************************************/ /**
 * Gets current 32 bits global tick count.
 ******************************************************************************/
uint32_t sli_cpc_timer_get_tick_count(void)
{
  return sl_sleeptimer_get_tick_count();
}

/***************************************************************************/ /**
 * Gets current 64 bits global tick count.
 ******************************************************************************/
uint64_t sli_cpc_timer_get_tick_count64(void)
{
  return sl_sleeptimer_get_tick_count64();
}

/***************************************************************************/ /**
 * Starts a 32 bits timer.
 ******************************************************************************/
sl_status_t sli_cpc_timer_start(sli_cpc_timer_t *timer, uint32_t timeout, sli_cpc_timer_callback_t callback,
                                void *callback_data)
{
  return sl_sleeptimer_start_timer(timer, timeout, callback, callback_data, 0, 0);
}

/***************************************************************************/ /**
 * Starts a 32 bits timer with a millisecond timeout.
 ******************************************************************************/
sl_status_t sli_cpc_timer_start_timer_ms(sli_cpc_timer_t *timer, uint32_t timeout_ms, sli_cpc_timer_callback_t callback,
                                         void *callback_data)
{
  return sl_sleeptimer_start_timer_ms(timer, timeout_ms, callback, callback_data, 0, 0);
}

/***************************************************************************/ /**
 * Restarts a 32 bits timer.
 ******************************************************************************/
sl_status_t sli_cpc_timer_restart(sli_cpc_timer_t *timer, uint32_t timeout, sli_cpc_timer_callback_t callback,
                                  void *callback_data)
{
  return sl_sleeptimer_restart_timer(timer, timeout, callback, callback_data, 0, 0);
}

/***************************************************************************/ /**
 * Stops a timer.
 ******************************************************************************/
sl_status_t sli_cpc_timer_stop(sli_cpc_timer_t *timer)
{
  return sl_sleeptimer_stop_timer(timer);
}

/***************************************************************************/ /**
 * Checks whether a timer is running.
 ******************************************************************************/
bool sli_cpc_timer_is_running(const sli_cpc_timer_t *timer)
{
  sl_status_t status;
  bool is_running;

  status = sl_sleeptimer_is_timer_running(timer, &is_running);

  return status == SL_STATUS_OK && is_running;
}

/***************************************************************************/ /**
 * Delays for the specified number of milliseconds.
 ******************************************************************************/
void sli_cpc_timer_delay_millisecond(uint16_t delay_ms)
{
  sl_sleeptimer_delay_millisecond(delay_ms);
}
