/***************************************************************************//**
 * @file
 * @brief simulation files for the system timer part of the HAL
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
#include <sys/time.h>

#include PLATFORM_HEADER
#include "stack/include/sl_zigbee.h"
#include "hal/hal.h"

// If halUseRealtime is true, this reports time as provided by the OS.
// Calling microSetSystemTime() resets the reported time to the given
// value, with the clock continuing to run from there.
//
// If halUseRealtime is false, this simply reports the last value passed
// to microSetSystemTime().

bool halUseRealtime = false;

static uint64_t _systemTick;

static uint64_t startTime;

uint16_t halInternalStartSystemTimer(void)
{
  if (halUseRealtime) {
    startTime = 0;
    startTime = halCommonGetInt64uMillisecondTick();
  } else {
    _systemTick = 0;
  }
  return 0;
}

uint16_t halCommonGetInt16uMillisecondTick(void)
{
  return (uint16_t)(halCommonGetInt64uMillisecondTick() & 0x000000000000FFFF);
}

uint32_t halCommonGetInt32uMillisecondTick(void)
{
  return (uint32_t)(halCommonGetInt64uMillisecondTick() & 0x00000000FFFFFFFF);
}

uint16_t halCommonGetInt16uQuarterSecondTick(void)
{
  return (uint16_t)((halCommonGetInt64uMillisecondTick() >> 8) & 0x000000000000FFFF);
}

uint64_t halCommonGetInt64uMillisecondTick(void)
{
  if (halUseRealtime) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (((tv.tv_sec * 1000) + (tv.tv_usec / 1000))
            - startTime);
  } else {
    return _systemTick;
  }
}

void microSetSystemTime(uint32_t time)
{
  if (halUseRealtime) {
    startTime = 0;
    startTime = halCommonGetInt32uMillisecondTick() - time;
  } else {
    _systemTick = time;
  }
}
