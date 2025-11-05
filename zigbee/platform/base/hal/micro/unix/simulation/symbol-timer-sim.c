/***************************************************************************//**
 * @file
 * @brief simulation files for symbol timer part of the HAL
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
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>

#include PLATFORM_HEADER
#include STACK_HEADER
#include "hal/hal.h"
#include "tool/simulator/child/timer.h"
#include "phy/symbol-timer.h"

static uint16_t timerTick = 0;

// New API:

#define SYMBOL_TIMER_MAX 0x0000FFFFul // UINT16_MAX

static volatile
EmHalSymbolDelayCallback_t symbolDelayCallbacks[EM_HAL_SYMBOL_DELAY_CHANNELS];
static uint8_t timerIndices[EM_HAL_SYMBOL_DELAY_CHANNELS];

static void symbolDelayCallback(EmHalSymbolDelayChannel_t delayChan)
{
  EmHalSymbolDelayCallback_t cb = symbolDelayCallbacks[delayChan];
  if (cb != NULL) {
    symbolDelayCallbacks[delayChan] = NULL;
#ifdef  PHY_SIMULATION_LEGACY
    // EMIPSTACK-336 - Check Mac Timer interrupt is really enabled
    extern bool halSimulatorCheckMacTimerIntEnabled(void);

    if (halSimulatorCheckMacTimerIntEnabled())
#endif//PHY_SIMULATION_LEGACY
    {
      (*cb)(delayChan);
    }
  }
}

static void symbolDelayACallback(void)
{
  symbolDelayCallback(EM_HAL_SYMBOL_DELAY_CHANNEL_A);
}

static void symbolDelayBCallback(void)
{
  symbolDelayCallback(EM_HAL_SYMBOL_DELAY_CHANNEL_B);
}

void halInternalStartSymbolTimer(void)
{
  assert(EM_HAL_SYMBOL_DELAY_CHANNELS <= 2); // Currently support up to 2 delayChan
  timerIndices[EM_HAL_SYMBOL_DELAY_CHANNEL_A] = defineTimer(symbolDelayACallback);
  timerIndices[EM_HAL_SYMBOL_DELAY_CHANNEL_B] = defineTimer(symbolDelayBCallback);
}

uint32_t halStackGetInt32uSymbolTick(void)
{
  return timerTick;
}

bool halStackInt32uSymbolTickGTorEqual(uint32_t st1, uint32_t st2)
{
  return (st1 - st2) < 0x80000000u;
}

uint32_t halStackGetSymbolTicksPerSecond(void)
{
  return 1000000ul; // Pretend our timer ticks every microsecond
}

uint32_t halStackOrderSymbolDelay(EmHalSymbolDelayChannel_t delayChan,
                                  EmHalSymbolDelayCallback_t callback,
                                  uint32_t microseconds)
{
  if (callback == NULL) {
    // Caller just wants to know the absolute tick at expiry for polling
  } else {
    assert(delayChan < EM_HAL_SYMBOL_DELAY_CHANNELS);
    assert(getTimer(timerIndices[delayChan]) == 0);
    assert(symbolDelayCallbacks[delayChan] == NULL);
    if (microseconds == 0) {
      // Timer will expire too quickly so simply call the completion routine now.
      (*callback)(delayChan);
    } else {
      symbolDelayCallbacks[delayChan] = callback;
      setTimer(timerIndices[delayChan], microseconds);
    }
  }
  return (timerTick + microseconds) & SYMBOL_TIMER_MAX;
}

void halStackCancelSymbolDelay(EmHalSymbolDelayChannel_t delayChan,
                               EmHalSymbolDelayCallback_t callback)
{
  assert(delayChan < EM_HAL_SYMBOL_DELAY_CHANNELS);
  if (symbolDelayCallbacks[delayChan] == callback) {
    symbolDelayCallbacks[delayChan] = NULL;
    cancelTimer(timerIndices[delayChan]);
  }
}

#ifdef  MAC_TEST_STACK

// Old API -- deprecated:

// WEAK(void halStackSymbolDelayAIsr(void)) {}

static void internalDelayAIsr(EmHalSymbolDelayChannel_t delayChan)
{
  UNUSED_VAR(delayChan);
  halStackSymbolDelayAIsr();
}

#ifndef SL_ZIGBEE_SYMBOL_TIMER_USEC
#define SL_ZIGBEE_SYMBOL_TIMER_USEC 16
#endif//SL_ZIGBEE_SYMBOL_TIMER_USEC

void halStackOrderInt16uSymbolDelayA(uint16_t symbols)
{
  // This API enforces a minimum one-symbol delay
  if (symbols == 0) {
    symbols = 1;
  }
  (void) halStackOrderSymbolDelay(EM_HAL_SYMBOL_DELAY_CHANNEL_A,
                                  &internalDelayAIsr,
                                  symbols * SL_ZIGBEE_SYMBOL_TIMER_USEC);
}

void halStackCancelSymbolDelayA(void)
{
  halStackCancelSymbolDelay(EM_HAL_SYMBOL_DELAY_CHANNEL_A,
                            &internalDelayAIsr);
}

#endif//MAC_TEST_STACK

uint16_t halGetSymbolTimerTick(void)
{
  return timerTick;
}

void halSetSymbolTimerTick(uint16_t time)
{
  timerTick = time;
}
