/***************************************************************************//**
 * @file
 * @brief RAIL symbol timer functions
 *******************************************************************************
 * # License
 * <b>Copyright 2018 Silicon Laboratories Inc. www.silabs.com</b>
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
#if     (PHY_DUALRAIL && !PHY_THIS && !PHY_INCLUDE_SOURCE)

// If SLC builds this source file without being included, make it empty

#else//!(PHY_DUALRAIL && !PHY_THIS && !PHY_INCLUDE_SOURCE)

#include "phy/symbol-timer.h"
#include "rail.h"
#include "sl_core.h"

extern RAIL_Handle_t emPhyRailHandle;

#ifndef assert //@TODO: What to do about asserts?
#define assert(x) if (!(x)) while (true)
#endif

#define SYMBOL_TIMER_MAX (0xFFFFFFFFUL) // Full 32-bit range

// halInternalStartSymbolTimer() is implemented in PHY layer

uint32_t halStackGetInt32uSymbolTick(void)
{
  return RAIL_GetTime();
}

bool halStackInt32uSymbolTickGTorEqual(uint32_t st1, uint32_t st2)
{
  uint32_t symbolTimerMax = SYMBOL_TIMER_MAX; // Local copy of potential volatile
  // Returns true if t1 >= t2 (using half the range of the timer).
  // Can only account for 1 wrap around between t1 and t2 before
  // it is wrong.
  st1 &= symbolTimerMax; // Normalize st1 within timer range
  st2 &= symbolTimerMax; // Normalize st1 within timer range
  // To ensure subtract below doesn't underflow:
  if (st1 < st2) {
    st1 += symbolTimerMax + 1;
  }
  return (st1 - st2) <= (symbolTimerMax / 2);
}

uint32_t halStackGetSymbolTicksPerSecond(void)
{
  // RAIL timer presents a 1us tick (even tho it actually ticks at 2us inside)
  return (1000000UL);
}

static volatile
EmHalSymbolDelayCallback_t symbolDelayCallbacks[EM_HAL_SYMBOL_DELAY_CHANNELS];

static void halStackSymbolDelayIsr(EmHalSymbolDelayChannel_t delayChan)
{
  EmHalSymbolDelayCallback_t cb = symbolDelayCallbacks[delayChan];
  if (cb != NULL) {
    symbolDelayCallbacks[delayChan] = NULL;
    (*cb)(delayChan);
  }
}

static void RAILCb_TimerExpired(RAIL_Handle_t railHandle)
{
  // This callback is only for Channel A
  (void)railHandle;
  halStackSymbolDelayIsr(EM_HAL_SYMBOL_DELAY_CHANNEL_A);
}

#if     PHY_DUAL // MultiTimer-capable
static RAIL_MultiTimer_t timerChannelB;

static void RAILCb_MultiTimerExpired(RAIL_MultiTimer_t *tmr,
                                     RAIL_Time_t expectedTimeOfEvent,
                                     void *cbArg)
{
  (void)tmr;
  (void)expectedTimeOfEvent;
  halStackSymbolDelayIsr((EmHalSymbolDelayChannel_t)(uint32_t)cbArg);
}
#endif//PHY_DUAL // MultiTimer-capable

uint32_t halStackOrderSymbolDelay(EmHalSymbolDelayChannel_t delayChan,
                                  EmHalSymbolDelayCallback_t callback,
                                  uint32_t microseconds)
{
  uint32_t timerTicks = microseconds;

  if (callback == NULL) {
    // Caller just wants to know the absolute tick at expiry for polling
    return (halStackGetInt32uSymbolTick() + timerTicks) & SYMBOL_TIMER_MAX;
  }

  assert(delayChan < EM_HAL_SYMBOL_DELAY_CHANNELS);
  assert(symbolDelayCallbacks[delayChan] == NULL);
  assert(timerTicks <= (SYMBOL_TIMER_MAX / 2));
  symbolDelayCallbacks[delayChan] = callback;
  if (delayChan == EM_HAL_SYMBOL_DELAY_CHANNEL_A) {
    if (RAIL_SetTimer(emPhyRailHandle, timerTicks, RAIL_TIME_DELAY, &RAILCb_TimerExpired) == RAIL_STATUS_NO_ERROR) {
      return RAIL_GetTimer(emPhyRailHandle);
    }
  } else {
   #if     PHY_DUAL // MultiTimer-capable
    if (RAIL_SetMultiTimer(&timerChannelB,
                           timerTicks, RAIL_TIME_DELAY,
                           &RAILCb_MultiTimerExpired,
                           (void *)(uint32_t)delayChan) == RAIL_STATUS_NO_ERROR) {
      return RAIL_GetMultiTimer(&timerChannelB, RAIL_TIME_ABSOLUTE);
    }
   #else//!PHY_DUAL // Not MultiTimer-capable
    assert(false);
   #endif//PHY_DUAL // End MultiTimer-capable
  }
  // Oops -- the event was in the past -- fire callback now, manually
  timerTicks = (halStackGetInt32uSymbolTick() - 1) & SYMBOL_TIMER_MAX;
  halStackSymbolDelayIsr(delayChan);
  return timerTicks;
}

void halStackCancelSymbolDelay(EmHalSymbolDelayChannel_t delayChan,
                               EmHalSymbolDelayCallback_t callback)
{
  //assert(delayChan < EM_HAL_SYMBOL_DELAY_CHANNELS);
  CORE_DECLARE_IRQ_STATE;
  CORE_ENTER_ATOMIC();
  if (symbolDelayCallbacks[delayChan] == callback) {
    // Delay doesn't appear to have finished yet
    symbolDelayCallbacks[delayChan] = NULL;
    if (delayChan == EM_HAL_SYMBOL_DELAY_CHANNEL_A) {
      RAIL_CancelTimer(emPhyRailHandle);
    } else {
     #if     PHY_DUAL // MultiTimer-capable
      RAIL_CancelMultiTimer(&timerChannelB);
     #else//!PHY_DUAL // Not MultiTimer-capable
      assert(false);
     #endif//PHY_DUAL // End MultiTimer-capable
    }
  }
  CORE_EXIT_ATOMIC();
}

#endif//(PHY_DUALRAIL && !PHY_THIS && !PHY_INCLUDE_SOURCE)
