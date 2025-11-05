/*
 * File: timer.c
 * Description: microsecond timers for use by simulations.
 *
 * Author(s): Richard Kelsey, kelsey@ember.com
 *
 * Copyright 2004 by Ember Corporation. All rights reserved.                *80*
 */

// This provides 256 microsecond timers.  They are allocated as
// needed by calling defineTimer(), which returns an int8u indicating
// which timer you have.

#include PLATFORM_HEADER

#include "timer.h"

static int32u timerDelays[256] = { 0, };
static TimerCallback timerCallbacks[256] = { NULL, };
static int timerCount = 0;

bool advanceTimers(int32u ticks)
{
  int i;
  bool fired = FALSE;

  for (i = 0; i < timerCount; i++) {
    int32u delay = timerDelays[i];
    if (delay != 0) {
      if (delay <= ticks) {
        timerDelays[i] = 0;
        (timerCallbacks[i])();
        fired = TRUE;
      } else {
        timerDelays[i] = delay - ticks;
      }
    }
  }
  return fired;
}

int32u leastTimerDelay(void)
{
  int32u leastDelay = 0;
  int i;

  for (i = 0; i <= timerCount; i++) {
    int32u delay = timerDelays[i];
    if (delay != 0
        && (leastDelay == 0
            || delay < leastDelay)) {
      leastDelay = delay;
    }
  }

  return leastDelay;
}

int8u defineTimer(TimerCallback callback)
{
  int8u index = timerCount;
  timerCount += 1;
  timerCallbacks[index] = callback;
  return index;
}

void setTimer(int8u timerIndex, int32u delay)
{
  assert(timerCallbacks[timerIndex] != NULL);
  timerDelays[timerIndex] = delay;
}

int32u getTimer(int8u timerIndex)
{
  assert(timerCallbacks[timerIndex] != NULL);
  return timerDelays[timerIndex];
}
