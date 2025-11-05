/***************************************************************************//**
 * @file
 * @brief simulation files for the HAL
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
#include <unistd.h> // alarm()
#include <signal.h>
#include <sys/time.h>
#include <errno.h>

#if !defined(MAC_TEST_STACK) && !defined(UNIFIED_MAC_SCRIPTED_TEST)
#include "stack/config/sl_zigbee_token_defines.h"
#include "sl_token_manager_api.h"
#endif

#include PLATFORM_HEADER
#include STACK_HEADER
#include STACK_CORE_HEADER
#include "hal/hal.h"

#include "phy/symbol-timer.h"

#define WATCHDOG_TIMEOUT_SECONDS 2

uint16_t rebootCount;
bool em4Reset = false;
#if defined(SL_ZIGBEE_SCRIPTED_TEST)
// Normally simulator ID will be defined by the embedded code to support the simulator.
// However for scripted (unit) tests, that doesn't happen.
// I picked 0xFFFF since it will indicate this is a scripted test.  Normally real
// simulator IDs start from 0.
uint16_t simulatorId = 0xFFFF;
#endif

void setEm4ResetFlag(bool flag)
{
  em4Reset = flag;
}

bool getEm4ResetFlag(void)
{
  return em4Reset;
}

void halInit(void)
{
  halInternalStartSystemTimer();
#if !defined(EZSP_HOST) && !defined(UNIX_HOST_SIM)
  halInternalStartSymbolTimer();
#endif
  halInternalInitBoard();
}

static void (*microRebootHandler)(void) = NULL;

void setMicroRebootHandler(void (*handler)(void))
{
  microRebootHandler = handler;
}

void halReboot(void)
{
  if (microRebootHandler == NULL) {
    assert(false);
  } else {
    microRebootHandler();
  }
}

void halPowerDown(void)
{
  halInternalPowerDownBoard();
}

void halPowerUp(void)
{
  halInternalPowerUpBoard();
}

void halInternalAssertFailed(char const* filename, int linenumber)
{
  (void)filename;
  (void)linenumber;
  // print something?
  halReboot();
}

const char *halGetResetString(void)
{
  static const char *resetString = "UNKWN";
  return (resetString);
}

uint8_t halGetResetInfo(void)
{
  if (em4Reset) {
    return RESET_2xx_SOFTWARE_EM4;
  } else if (rebootCount == 0) {
    return RESET_2xx_POWERON;
  } else {
    return RESET_2xx_SOFTWARE;
  }
}

void halStackProcessBootCount(void)
{
  // Note:  We need to add the increment call in order to test the lighting
  // sample applications.
#if !defined(MAC_TEST_STACK) && !defined(UNIFIED_MAC_SCRIPTED_TEST)
#if defined(COMMON_TOKEN_STACK_BOOT_COUNTER)
#ifndef SL_ZIGBEE_SCRIPTED_TEST
  (void)sl_token_manager_increment_counter(COMMON_TOKEN_STACK_BOOT_COUNTER);
#endif
#endif
#else
#if defined(CREATOR_STACK_BOOT_COUNTER)
#ifndef SL_ZIGBEE_SCRIPTED_TEST
  halCommonIncrementCounterToken(TOKEN_STACK_BOOT_COUNTER);
#endif
#endif
#endif
}

static void watchdogFunction(int signalNumber)
{
  (void)signalNumber;
  // SIGALRM doesn't cause a core dump and I couldn't find a way to change that.
  // So we just catch it and assert so we can get a nice core dump with a backtrace.
  fprintf(stderr, "[simid: %d] WATCHDOG timer fired after %d seconds\n", simulatorId, WATCHDOG_TIMEOUT_SECONDS);
  assert(0);
}

static void installSignalHandler(void)
{
  static bool installed = false;
  if (!installed) {
    signal(SIGVTALRM, watchdogFunction);
    installed = true;
  }
}

#if defined(__CYGWIN__)
// Googling around I found that Cygwin doesn't appear to implement ITIMER_VIRTUAL,
// therefore we use ITIMER_REAL.  ITIMER_VIRTUAL only measures time while the process
// is running and that is more important in Unix where the user could Ctrl-Z or
// run with a debugger connected.
  #define TIMER_TYPE ITIMER_REAL
#else
// Only decrement when process is executing, not when for example we suspend
// the process with Ctrl-Z.
  #define TIMER_TYPE ITIMER_VIRTUAL
#endif

static void setWatchdogTimer(uint32_t timeValueSeconds)
{
  int result;
  struct itimerval timerStruct = {
    // it_interval (what happens when timer is reset)
    // We don't care about this since the behavior is to assert()
    // when the watchdog fires.
    {
      0,  // seconds
      0,  // micro seconds
    },

    // it_value (amount of time remaining on the timer)
    {
      timeValueSeconds,
      0,  // micro seconds
    },
  };
  result = setitimer(TIMER_TYPE,
                     &timerStruct,
                     NULL);  // previous timer value struct
  if (result) {
    fprintf(stderr, "[simid: %d] setitimer() failed: %d (%s)\n", simulatorId, result, strerror(errno));
  }
  assert(result == 0);
}

void halInternalResetWatchDog(void)
{
  installSignalHandler();
  setWatchdogTimer(WATCHDOG_TIMEOUT_SECONDS);
}

void halInternalEnableWatchDog(void)
{
  halInternalResetWatchDog();
}

bool halInternalWatchDogEnabled(void)
{
  struct itimerval timerStruct;
  assert(0 == getitimer(ITIMER_VIRTUAL,
                        &timerStruct));
  return ((timerStruct.it_value.tv_sec > 0)
          || (timerStruct.it_value.tv_usec > 0));
}

void halInternalDisableWatchDog(uint8_t magicKey)
{
  (void)magicKey;
  setWatchdogTimer(0);
}

void halCommonDelayMicroseconds(uint16_t us)
{
#ifndef SL_ZIGBEE_SCRIPTED_TEST
  simulatedTimePassesUs(us);
#else
  (void)us;
#endif
}

void halCommonDelayMilliseconds(uint16_t msec)
{
  uint16_t cnt = msec;

  if (msec == 0) {
    return;
  }

  while (cnt-- > 0)
    halCommonDelayMicroseconds(1000);
}

EmberStatus emReadFIFOSpecifyByteOrder(uint8_t *dst, uint8_t length,
                                       bool partial, uint8_t endianness)
{
  (void)dst;
  (void)length;
  (void)partial;
  (void)endianness;
  // Unimplemented.  Prevent unintended consequences by calling assert.
  assert(0);
  return 0xFF;
}

bool halInternalUartTxIsIdle(uint8_t port)
{
  (void)port;
  return true;
}

#if defined(__APPLE__)

size_t strnlen(const char* string, size_t maxlen)
{
  size_t length = 0;
  while ((length < maxlen) && (string[length] != '\0')) {
    length++;
  }
  return length;
}

#endif
