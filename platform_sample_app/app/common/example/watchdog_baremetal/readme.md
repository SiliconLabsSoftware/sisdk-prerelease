# Watchdog Bare Metal

This example application demonstrates the use of the Software Watchdog Manager in a bare metal environment. The application registers two watchdog handles and feeds them periodically using a sleeptimer. On-board buttons allow the user to stop feeding individual watchdog handles, simulating a fault condition that triggers a system reset.

## Application Overview

The application creates **two watchdog handles** (`my_watchdog_0` and `my_watchdog_1`) and a **periodic sleeptimer** (1 second interval) to feed them.

### Startup Sequence

1. **Fault detection** (`app_init_early`): Before any service initialization, the application calls `sl_watchdog_manager_retrieve_faulty()` to check whether the previous reset was caused by an unfed watchdog. If so, the faulty handle is identified and reported on the serial console.
2. **EM1RUN configuration** (`watchdog_init`): On devices that support EM1RUN (`_WDOG_CFG_EM1RUN_MASK` defined), the watchdog hardware is re-initialized with `em1_run = true` so it keeps counting in EM1 energy mode.
3. **Watchdog creation** (`watchdog_init`): Two watchdog handles are created, enabled, and the watchdog manager is started. A force-feed is issued to prevent a premature reset during initialization.
4. **Timer start**: A periodic sleeptimer is started to wake the system every 1 second for watchdog feeding.

### Energy Mode Handling

The application integrates with the **Power Manager** to ensure correct watchdog behavior across energy modes:

- **Devices with EM1RUN support**: The watchdog peripheral is configured with `em1_run = true`, allowing it to keep counting while the device is in EM1. The device is allowed to sleep normally.
- **Devices without EM1RUN support**: The `app_is_ok_to_sleep()` callback returns `false` to prevent the device from entering EM1, ensuring the watchdog timer keeps running.

### Runtime Behavior

On each timer timeout, the application feeds the watchdog handles based on the current button state:

- **Both buttons unpressed (default)**: Both `my_watchdog_0` and `my_watchdog_1` are fed normally. The system runs indefinitely.
- **Button 0 pressed**: `my_watchdog_0` stops being fed. After the watchdog timeout period elapses, the watchdog manager detects the unfed handle and resets the system.
- **Button 1 pressed**: `my_watchdog_1` stops being fed, triggering a reset in the same manner.
- **After reset**: On the next boot, the application detects which watchdog handle caused the reset and prints a message (e.g., `[WDOG] Watchdog 0 was fault last time`).

Pressing the same button again re-enables feeding for that handle (toggle behavior).

### Console Output

Status messages are printed to the serial console (VCOM) at each feeding cycle.

On devices with EM1RUN support:

```
***************************************************
STARTING WATCHDOG EXAMPLE
--------------------------------------------------------
[WDOG] Disabling EM1RUN
[WDOG] Watchdog 0 created
[WDOG] Watchdog 1 created
[WDOG] Watchdog 0 and 1 started
[TIMER] Timer created
[APP] Watchdog 0 fed - Watchdog 1 fed
[APP] Watchdog 0 fed - Watchdog 1 fed
[BTN] Button 0 pressed
[APP] Watchdog 0 not fed - Watchdog 1 fed
...
```

On devices without EM1RUN support:

```
***************************************************
STARTING WATCHDOG EXAMPLE
--------------------------------------------------------
[WDOG] Watchdog 0 created
[WDOG] Watchdog 1 created
[WDOG] Watchdog 0 and 1 started
[TIMER] Timer created
[APP] Watchdog 0 fed - Watchdog 1 fed
...
```

After the system resets due to an unfed watchdog, the next boot will show:

```
***************************************************
STARTING WATCHDOG EXAMPLE
--------------------------------------------------------
[WDOG] Watchdog 0 was fault last time
--------------------------------------------------------
```

## Requirements

- Silicon Labs board with **2 buttons** (btn0, btn1) and Watchdog support.
- Serial console connection (VCOM) for observing output.
