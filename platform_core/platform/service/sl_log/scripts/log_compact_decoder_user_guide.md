# Log compact decoder (`log_compact_decoder.py`)

Host-side tool for **real-time decoding** of SL Log **compact binary** output (`sl_log_event_t` / `sl_log_entry_t`) from UART (VCOM) or SEGGER RTT. Format strings are resolved from the `.log_fmt` ELF section in your build artifacts—not from Flash at runtime.

```text
platform/service/sl_di/scripts/log_compact_decoder.py
platform/service/sl_log/scripts/log_compact_decoder_user_guide.md
```

## Prerequisites

Python 3.8+ on the host (Windows, macOS, or Linux).

```bash
pip install pyelftools pyserial
```

Optional:

| Package | When |
|--------|------|
| `pylink-square` | `--source rtt` (J-Link RTT) |
| `colorama` | Colored console output (default on if installed) |

## Required arguments (live decode)

| Argument | Required | Notes |
|----------|----------|--------|
| `--app-out` | **Yes** | Host application ELF (`.out` / `.axf`) built with compact logging (Cortex-M4, M33 or M55). Contains `.log_fmt` for core 0. Also used for RTT control-block lookup and `%s` string resolution. Aliases: `--m4-out`, `--out`, `--axf`. |
| `--port` | **Yes** (UART only) | Serial port for the board VCOM, e.g. `COM4` (Windows), `/dev/ttyACM0` (Linux), `/dev/tty.usbmodem*` (macOS). Not used with `--source rtt`. |

## Optional arguments (common)

| Argument | Purpose |
|----------|---------|
| `--max-args` | Must match firmware `SL_LOG_CONFIG_ARG` (**3**..**10**). Default: **3**. |
| `--level` | Minimum severity to print (`all`, `debug`, `info`, `warn`, `error`, `crash`). Same as `sl_log_set_loglevel`: the selected level and every more severe level are shown. Default: `all`. |
| `--baud` | UART baud rate. Default **115200**; use **921600** when NWP logging is enabled on UART. |
| `--layout` | `auto` (default): timestamped host `sl_log_event_t` (`timestamp` + `epoch` + `event_id` + args) and 28-byte NWP records. `nwp`: NWP records only (no host frames in the capture). |
| `--source` | `uart` (default) or `rtt`. |
| `--log-file` | Mirror decoded output to a file. |
| `--list-formats` | Dump `.log_fmt` addresses and strings from `--app-out` / `--nwp-out`, then exit (no UART/RTT). |
| `--nwp-out` | NWP / captive-core ELF with `.log_fmt` (e.g. `nwp_logging_fw.out`) for core 1 Wi‑Fi/BLE logs. Omit if you only care about host logs. Aliases: `--ta-out`, `--noload`. |

Run `python log_compact_decoder.py --help` for RTT-specific flags (`--device`, `--rtt-channel`, etc.).

## Quick start (host logs only)

Build and flash your application with **Log Backend I/O Stream – Compact Binary Output** (`log_backend_iostream_compact`). Point the decoder at the **same** application ELF you flashed and at the VCOM port.

```bash
python log_compact_decoder.py --app-out path/to/app.out --port COM4 --max-args 3
```

Adjust `--max-args` to match **CONFIG_MAX_ARGS** / `SL_LOG_CONFIG_ARG` in your project.

## Host + NWP (optional `--nwp-out`)

When captive-core logs are muxed on the same UART, add the NWP logging ELF so core 1 events decode:

```bash
python log_compact_decoder.py --app-out app.out --nwp-out nwp_logging_fw.out --port COM4 --max-args 3 --baud 921600
```

Core 1 lines are prefixed with the firmware component name (e.g. `[WLAN_UMAC] ...`).

## RTT (no `--port`)

```bash
pip install pylink-square

python log_compact_decoder.py --source rtt --app-out app.out --max-args 3
```

`--device` is optional for RTT.

## Inspect format strings

```bash
python log_compact_decoder.py --app-out app.out --list-formats
```

## Wire format (default `--layout auto`)

Each record is an `sl_log_event_t`: `uint32 timestamp` + `uint32 epoch` + `event_id` + `args[N]` + trailer (`arg_count`, `core_id`, `flags`, `version`, one byte each). The record size is `12 + 4*N + 4` bytes, with `N` = `--max-args`. TS/DELTA use the firmware time from the record.

Firmware from SDKs that predate the `epoch` field has a 32-bit timestamp. Those records still decode, but without their firmware time: TS/DELTA show the PC receive time instead.

### GCC and LLVM builds

Event IDs are the link-time addresses of the format strings in `.log_fmt`. For flash builds, compact logging keeps `.log_fmt` out of the image as a non-allocated section. GNU ld still assigns it a non-zero address after the image in flash; the value depends on the device flash base. LLD always links non-allocated sections at address `0`, so an LLVM build sends section offsets as event IDs. RAM builds place `.log_fmt` in RAM, and both linkers give it a RAM address.

The decoder reads the base address from `--app-out` and handles all of these; it prints `Host .log_fmt is linked at address 0 (LLVM/LLD build)` when it applies. For such a build, `--app-out` must be the ELF of the firmware on the board: event IDs are matched on exact format-string addresses, so records from a different build are dropped instead of shown as `<Unknown Event>`.

With either toolchain, a `%s` argument is resolved only when it points to a constant string in the ELF. A pointer to a RAM buffer (for example one filled with `snprintf`) is printed as its address.

## Troubleshooting

- **`ERROR: --app-out is required`** — Pass the application ELF that matches the running firmware (`--m4-out` still works as an alias).
- **`ERROR: --port is required`** — Specify the VCOM port; auto-detection is disabled so the wrong adapter is not opened on multi-board benches.
- **Only DEBUG lines, or garbled `(null)` messages** — Wrong ELF, wrong `--max-args`, or the Formatted Output backend is on the target instead of Compact Binary Output.
- **Last log lines missing in Host mode, but printed in Console mode** — In Host mode logs wait in the ring buffer until `sl_log_flush()` runs, typically from the idle task. If an RTOS task function returns, FreeRTOS halts in `prvTaskExitError()` and the idle task never runs again, so the logs queued just before the return are never sent. End the task with `osThreadExit()` instead of `return`, including on error paths.
- **Missing Python modules** — Install `pyelftools` and `pyserial` (and `pylink-square` for RTT).

## Related documentation

- Platform component: **Log Backend I/O Stream – Compact Binary Output** (`log_backend_iostream_compact`).
- WiSeConnect examples may still bundle a copy of this script; the canonical platform copy is under `platform/service/sl_log/scripts/`.
