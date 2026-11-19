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
| `--layout` | `auto` (default): timestamped host `sl_log_event_t` (`timestamp` + `epoch` + `event_id` + args). When `--max-args` is not 3, also accept 28-byte NWP records. Force `no-ts` for older event_id-first captures; `ts` for timestamped-only. |
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

The VCOM stream may interleave:

- **Core 0 (host M4/M55):** `sl_log_event_t` — `uint32 timestamp` + `uint32 epoch` + `event_id` + `args[N]` + trailer (`12+4*N+4`). TS/DELTA use firmware time.
- **Core 1 (NWP):** `sl_log_entry_t` — 64-bit firmware timestamp; 28 bytes when NWP `SL_LOG_MAX_ARGS` is 3.

When `--max-args` is 3, host and NWP share the same record size; `core_id` in the trailer selects the `.log_fmt` map.

Use `--layout no-ts` only for captures that predate the timestamp+epoch fields (event_id first, `4+4*N+4`). Those lines use PC receive time in TS/DELTA.

## Troubleshooting

- **`ERROR: --app-out is required`** — Pass the application ELF that matches the running firmware (`--m4-out` still works as an alias).
- **`ERROR: --port is required`** — Specify the VCOM port; auto-detection is disabled so the wrong adapter is not opened on multi-board benches.
- **Only DEBUG lines, or garbled `(null)` messages** — Wrong ELF, wrong `--max-args`, or `--layout no-ts` on a timestamped stream. Current firmware needs default `--layout auto`.
- **NWP lines show “no descriptor/nwp-out match”** — Add `--nwp-out` or, for legacy builds, `--descriptor`.
- **Missing Python modules** — Install `pyelftools` and `pyserial` (and `pylink-square` for RTT).

## Related documentation

- Platform component: **Log Backend I/O Stream – Compact Binary Output** (`log_backend_iostream_compact`).
- WiSeConnect examples may still bundle a copy of this script; the canonical platform copy is under `platform/service/sl_log/scripts/`.
