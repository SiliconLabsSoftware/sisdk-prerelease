/*
 * File: timer.h
 * Description: microsecond timers for use by simulations.
 *
 * Author(s): Richard Kelsey, kelsey@ember.com
 *
 * Copyright 2004 by Ember Corporation. All rights reserved.               *80*
 */

// This provides 256 microsecond timers.  They are allocated as
// needed by calling defineTimer(), which returns an int8u indicating
// which timer you have.

// Callback functions passed to defineTimer().
typedef void (*TimerCallback)(void);

// Get an unused timer and set its callback function.
int8u defineTimer(TimerCallback callback);

// Arrange for a timer's callback function to be called 'delay' microseconds
// from now.
void setTimer(int8u timerIndex, int32u delay);

// Cancel a previously arranged timeout.
#define cancelTimer(timerIndex) (setTimer((timerIndex), 0))

// Returns the number of microseconds before the timer expires.
int32u getTimer(int8u timerIndex);

// Advances all timers by 'ticks' microseconds and invokes the callbacks
// of any timers that reach zero.  Returns TRUE if any timer fired.
bool advanceTimers(int32u ticks);

// Returns the smallest non-zero timer delay, or zero if there are no
// active timers.
int32u leastTimerDelay(void);
