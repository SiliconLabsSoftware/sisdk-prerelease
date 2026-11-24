#!/usr/bin/env python3
"""
Log Console - hybrid decoder for host (.log_fmt) + NWP (.log_fmt and/or SYSVIEW).

Reads fixed-size binary log records from UART or RTT and prints readable lines.

Wire format: the SoC VCOM stream may carry these record shapes, and
``--layout auto`` (default) accepts all of them.

  host / M4 sl_log_event_t : timestamp + epoch + event_id + args[N] + trailer
                             size = 12 + 4*N + 4, N = --max-args (host
                             SL_LOG_CONFIG_ARG, often 3 or 10), core_id 0, or
                             core_id 1 for NWP events the M4 forwards in Host mode
  timestamp-less fallback  : event_id + args[N] + trailer, size = 4 + 4*N + 4.
                             This is the tail of every sl_log_event_t, so it
                             still decodes a record whose timestamp has another
                             width (32 bits before the epoch field was added),
                             once the framer has skipped that timestamp.
  NWP sl_log_entry_t       : uint64 timestamp + event_id + args[3] + trailer
                             size = 28 bytes (12 + 4*3 + 4; SL_LOG_MAX_ARGS is
                             fixed at 3 in sl_logging.h), core_id 1

  trailer = arg_count/core_id/flags/version, one byte each.

Timestamps in the console TS/DELTA columns:
  * Timestamped records -> firmware time from the record, in microseconds. Host
    records logged before the host/NWP time sync carry 0.
  * Records decoded through the timestamp-less fallback -> synthetic PC
    receive time in microseconds since the decoder started (perf_counter).

Host event ids are link-time addresses of the format strings in .log_fmt. GNU ld
gives the non-allocated compact .log_fmt a non-zero VMA after the image in
flash (the value depends on the device flash base); LLD always links
non-allocated sections at VMA 0, so an LLVM build sends plain section offsets
(0..size). Both are decoded; see _host_fmt_zero_based().

Use ``--layout nwp`` when the capture contains only NWP timestamp-first records
(no interleaved host frames).

Decode by core_id (efficient hybrid — .log_fmt for both cores):
  * core 0 (host M4/M55) -> event_id is .log_fmt VMA from --app-out (alias: --m4-out)
  * core 1 (NWP) -> prefer .log_fmt VMA from --nwp-out; under SL_LOGGING_ENABLE this
                    covers SL_PRINTF, sl_btdm_fast_log and sl_btdm_log alike, so the
                    ELF alone decodes everything. Numeric ids from --descriptor
                    (SYSVIEW_CaptiveCore.txt) are only needed for older NWP images
                    built before the BTDM families became address-based.

Usage (M4 .log_fmt + NWP .log_fmt, heart-rate / SL_LOG_CONFIG_ARG=3):
  python log_compact_decoder.py --app-out app.out --nwp-out nwp_logging_fw.out \\
       --port COM4 --max-args 10 --baud 921600
Usage (M4 .log_fmt + NWP SYSVIEW only):
  python log_compact_decoder.py --app-out app.out \\
      --descriptor SYSVIEW_CaptiveCore.txt --port COM38 --max-args 3

Core 1 (NWP) messages are prefixed with the firmware log_component_t name
taken from the 'ID : COMP : fmt' .log_fmt payload (or --descriptor for legacy
numeric ids), e.g. ``[WLAN_UMAC] ...``. Host/M4 lines are printed as-is.
"""

import argparse
import bisect
import os
import re
import struct
import sys
import time
from datetime import datetime
from pathlib import Path
from typing import Any, Callable, Dict, Iterable, List, Optional, Sequence, Tuple

try:
    from elftools.elf.elffile import ELFFile
except ImportError:
    print("ERROR: pyelftools is required. Install with: pip install pyelftools")
    sys.exit(1)

try:
    import serial
    from serial.tools import list_ports
except ImportError:
    print("ERROR: pyserial is required. Install with: pip install pyserial")
    sys.exit(1)

try:
    import pylink  # type: ignore
    PYLINK_AVAILABLE = True
except ImportError:
    pylink = None  # type: ignore
    PYLINK_AVAILABLE = False

try:
    from colorama import init, Fore, Style
    init(autoreset=True)
    COLORS_AVAILABLE = True
except ImportError:
    COLORS_AVAILABLE = False


SL_LOG_CONFIG_ARG_HARD_MAX = 32
# Default EVENT column width. Long names are clipped with "..." so MESSAGE
# always starts at a fixed column (see --event-width). 56 fits typical
# BT_APP_HOST_* / LE_* debug ids without clipping.
EVENT_COLUMN_WIDTH = 56
# Canonical firmware component enum. Keep this ordered exactly like
# log_component_t so integer values and names remain easy to compare.
LOG_COMPONENTS = (
    "COMMON",
    "CM_PM",
    "WLAN_LMAC",
    "WLAN_UMAC",
    "WLAN_NETSTACK",
    "BT_BLE_CTRL",
    "BT_BLE_STACK",
    "BTDM",
)
LOG_COMPONENT_SET = frozenset(LOG_COMPONENTS)
# Legacy NWP labels found in older .log_fmt payloads. Normalize them at decode
# time so the Core 1 message prefix always uses log_component_t names.
LOG_COMPONENT_ALIASES = {
    "COMPONENT_BLE": "BT_BLE_STACK",
}


def _parse_component_list(value: str) -> List[str]:
    """Comma-separated log_component_t names for --component."""
    names: List[str] = []
    for part in value.split(","):
        token = part.strip().upper().replace("-", "_")
        if not token:
            continue
        token = LOG_COMPONENT_ALIASES.get(token, token)
        if token not in LOG_COMPONENT_SET:
            choices = ", ".join(LOG_COMPONENTS)
            raise argparse.ArgumentTypeError(
                f"unknown component {part.strip()!r}; choose from {choices}")
        names.append(token)
    if not names:
        raise argparse.ArgumentTypeError("expected at least one component name")
    return names


# Wire flags (level in bits 1..3): sl_log_level_t 1=DEBUG .. 5=CRASH (see LEVEL_MAP).
LEVEL_MAP = {1: "DEBUG", 2: "INFO", 3: "WARN", 4: "ERROR", 5: "CRASH"}
# Ranks for filtering decoded LEVEL column and --min-level / --component-level.
LEVEL_NAME_RANK = {
    "DEBUG": 1,
    "INFO": 2,
    "WARN": 3,
    "ERROR": 4,
    "CRASH": 5,
}
LEVEL_RANK_CHOICES = ", ".join(sorted({"DEBUG", "INFO", "WARN", "ERROR", "CRASH"}))


def _parse_log_level_name(value: str) -> str:
    """Normalize a log level token for --min-level (DEBUG, INFO, ...)."""
    token = value.strip().upper().replace("-", "_")
    if token == "TRACE":
        token = "DEBUG"
    if token not in LEVEL_NAME_RANK:
        raise argparse.ArgumentTypeError(
            f"unknown level {value.strip()!r}; choose one of {LEVEL_RANK_CHOICES}"
        )
    return token


def _parse_component_level_pairs(value: str) -> List[Tuple[str, str]]:
    """Parse COMP=LEVEL[,COMP=LEVEL,...] for --component-level."""
    result: List[Tuple[str, str]] = []
    for segment in value.split(","):
        segment = segment.strip()
        if not segment:
            continue
        if "=" not in segment:
            raise argparse.ArgumentTypeError(
                f"expected COMP=LEVEL, got {segment!r} (example: WLAN_LMAC=WARN)"
            )
        comp_part, level_part = segment.split("=", 1)
        comp_token = comp_part.strip().upper().replace("-", "_")
        if not comp_token:
            raise argparse.ArgumentTypeError(f"empty component in {segment!r}")
        comp_token = LOG_COMPONENT_ALIASES.get(comp_token, comp_token)
        if comp_token not in LOG_COMPONENT_SET:
            choices = ", ".join(LOG_COMPONENTS)
            raise argparse.ArgumentTypeError(
                f"unknown component {comp_part.strip()!r}; choose from {choices}"
            )
        level_token = _parse_log_level_name(level_part)
        result.append((comp_token, level_token))
    if not result:
        raise argparse.ArgumentTypeError("expected at least one COMP=LEVEL pair")
    return result


def level_rank_for_filter(level_str: str) -> Optional[int]:
    """Numeric rank for filtering, or None if the line should always print."""
    if level_str == "OVERFLOW":
        return None
    rk = LEVEL_NAME_RANK.get(level_str)
    if rk is not None:
        return rk
    return None  # unknown codes (e.g. LEVEL6): always show


def should_print_by_level(
    level_str: str,
    component: Optional[str],
    args: argparse.Namespace,
    core_id: int,
) -> bool:
    """Apply --min-level (all cores) and per-component --component-level (NWP only)."""
    rk = level_rank_for_filter(level_str)
    if rk is None:
        return True

    comp_map: Dict[str, str] = getattr(args, "component_level_map", {})
    if core_id == 1 and component and comp_map:
        comp_min = comp_map.get(component)
        if comp_min is not None:
            cm = LEVEL_NAME_RANK.get(comp_min)
            if cm is not None and rk < cm:
                return False

    gl = getattr(args, "min_level", None)
    if gl is not None:
        gm = LEVEL_NAME_RANK.get(gl)
        if gm is not None and rk < gm:
            return False
    return True


LEVEL_COLORS = {
    "DEBUG": Fore.CYAN if COLORS_AVAILABLE else "",
    "TRACE": Fore.CYAN if COLORS_AVAILABLE else "",  # alias
    "INFO": Fore.GREEN if COLORS_AVAILABLE else "",
    "WARN": Fore.YELLOW if COLORS_AVAILABLE else "",
    "ERROR": Fore.RED if COLORS_AVAILABLE else "",
    "CRASH": Fore.RED if COLORS_AVAILABLE else "",
    "OVERFLOW": Fore.MAGENTA if COLORS_AVAILABLE else "",
}
RESET_COLOR = Style.RESET_ALL if COLORS_AVAILABLE else ""


# -------------------------------------------------------------------------
# Console UI helpers
# -------------------------------------------------------------------------
def print_banner() -> None:
    print(
        "\n+================================================================+\n"
        "|                         LOG CONSOLE                            |\n"
        "|              Real-time sl_log_entry_t decoder                  |\n"
        "+================================================================+\n"
    )


def print_config(
    source: str,
    transport_desc: str,
    app_out_file: str,
    nwp_out_file: str,
    fmt_count: int,
    descriptor_path: Optional[str],
    descriptor_id_base: int,
    event_width: int = EVENT_COLUMN_WIDTH,
) -> None:
    print("+- Configuration ------------------------------------------------+")
    print(f"|  Source       : {source.upper():<46} |")
    print(f"|  Transport    : {transport_desc:<46} |")
    print(f"|  APP OUT      : {(Path(app_out_file).name if app_out_file else 'None'):<46} |")
    print(f"|  NWP OUT      : {(Path(nwp_out_file).name if nwp_out_file else 'None'):<46} |")
    print(f"|  Format Strs  : {fmt_count:<46} |")
    print(f"|  Descriptor   : {(Path(descriptor_path).name if descriptor_path else 'not used'):<46} |")
    print(f"|  Descr. ID base: {descriptor_id_base:<45} |")
    print("+----------------------------------------------------------------+\n")
    print("Press Ctrl+C to stop. Until lines appear the stream may look idle; "
          "Ctrl+C prints byte counts and hints if nothing decoded.\n")
    # TIME LEVEL CORE TS DELTA EVENT | MESSAGE
    sep_w = 12 + 1 + 7 + 1 + 6 + 1 + 10 + 1 + 12 + 1 + event_width + 3 + 30
    print("-" * sep_w)
    print(
        f"{'TIME':<12} {'LEVEL':<7} {'CORE':<6} {'TS':>10} {'DELTA':>12} "
        f"{'EVENT':<{event_width}} | MESSAGE"
    )
    print("-" * sep_w)


def get_current_time() -> str:
    now = datetime.now()
    return now.strftime("%H:%M:%S.") + f"{now.microsecond // 1000:03d}"


def colorize(text: str, level: str) -> str:
    if not COLORS_AVAILABLE:
        return text
    return f"{LEVEL_COLORS.get(level, '')}{text}{RESET_COLOR}"


def format_event_column(event: Optional[str], width: int = EVENT_COLUMN_WIDTH) -> str:
    """Fixed-width EVENT field so MESSAGE always starts at the same column.

    Names longer than ``width`` are clipped with a trailing '...'. Use a larger
    ``--event-width`` if you need more of the id visible; full text remains in
    the MESSAGE column.
    """
    if width < 4:
        width = 4
    if not event:
        return f"{'-':<{width}}"
    if len(event) > width:
        return event[: width - 3] + "..."
    return f"{event:<{width}}"


def prefix_core1_message(msg: str, component: Optional[str]) -> str:
    """Prefix a Core 1 / NWP message with ``[log_component_t]``.

    NWP payloads are ``ID : COMP : text`` (or ``ID : COMP``). The COMP field
    is dropped from the body so it only appears in the prefix.
    """
    if not component:
        return msg
    marker = f" : {component}"
    if marker in msg:
        left, right = msg.split(marker, 1)
        right = right.lstrip(" :")
        msg = f"{left.strip()}: {right}" if right else left.strip()
    return f"[{component}] {msg}"


# -------------------------------------------------------------------------
# Args
# -------------------------------------------------------------------------
def _parse_max_args(value: str) -> int:
    try:
        n = int(value, 10)
    except ValueError:
        raise argparse.ArgumentTypeError(f"invalid integer: {value!r}") from None
    if not 1 <= n <= SL_LOG_CONFIG_ARG_HARD_MAX:
        raise argparse.ArgumentTypeError(
            f"--max-args must be between 1 and {SL_LOG_CONFIG_ARG_HARD_MAX}, got {n}"
        )
    return n


def parse_args() -> argparse.Namespace:
    p = argparse.ArgumentParser(
        description="Lightweight UART/RTT log decoder for sl_log_entry_t records.",
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )

    p.add_argument("--source", choices=("uart", "rtt"), default="uart",
                   help="Transport: 'uart' (default) or 'rtt' (SEGGER J-Link RTT).")

    # UART
    p.add_argument("--port", help="[uart] Serial port name (override auto-detection).")
    p.add_argument("--baud", type=int, default=115200, help="[uart] Baud rate (default: 115200).")

    # RTT
    p.add_argument("--device", default="SiWG917M111M",
                   help="[rtt] J-Link target device name (default: SiWG917M111M). "
                        "Must be a device known to your J-Link installation - run "
                        "JLinkExe -CommandFile NUL and search for the part if unsure. "
                        "Common Si91x choices: SiWG917M111M, SiWG917M110L, "
                        "SiWG917M100M. Generic 'Cortex-M4' is rejected by some J-Link "
                        "versions for Si91x and is not recommended.")
    p.add_argument("--jlink-serial", dest="jlink_serial", default=None,
                   help="[rtt] J-Link probe serial number (optional).")
    p.add_argument("--rtt-channel", dest="rtt_channel", type=int, default=0,
                   help="[rtt] Primary RTT up-buffer channel (default: 0).")
    p.add_argument("--rtt-channel2", dest="rtt_channel2", type=int, default=None, metavar="N",
                   help="[rtt] Optional second RTT up-buffer channel (e.g. 8 for NWP).")
    p.add_argument("--rtt-interface", dest="rtt_interface", choices=("swd", "jtag"),
                   default="swd", help="[rtt] J-Link interface (default: swd).")
    p.add_argument("--rtt-speed", dest="rtt_speed", default="auto",
                   help="[rtt] J-Link speed in kHz, or 'auto' (default: auto). "
                        "Si91x debug ports can be unstable at the higher speeds "
                        "that 'auto' negotiates, leading to 'Could not start CPU "
                        "core' on connect; 1000 kHz is a safe baseline (--rtt-speed 1000).")
    p.add_argument("--rtt-block-address", dest="rtt_block_address",
                   type=lambda v: int(v, 0), default=None,
                   help="[rtt] Fixed RTT control block address (e.g. 0x20000000). "
                        "If omitted, taken from --app-out / --m4-out (symbol _SEGGER_RTT or magic scan), "
                        "else J-Link RAM auto-scan.")
    p.add_argument("--rtt-reset", dest="rtt_reset", action="store_true",
                   help="[rtt] Reset the target on connect. Default is to attach to the "
                        "running firmware without resetting (Si91x debug ports can refuse "
                        "the halt that reset performs, producing 'Could not start CPU core').")
    p.add_argument("--rtt-no-reset", dest="rtt_no_reset", action="store_true",
                   help="[rtt] Kept for backward compatibility. Has no effect - no-reset "
                        "is now the default. Pass --rtt-reset to opt in to a reset.")
    p.add_argument("--rtt-connect-timeout", dest="rtt_connect_timeout",
                   type=float, default=18.0, metavar="SECS",
                   help="[rtt] Seconds to wait for the RTT control block (default: 18.0). "
                        "Firmware needs time to boot and call SEGGER_RTT_Init() before "
                        "UP buffers appear; increase if your firmware takes longer.")

    # Decode inputs
    p.add_argument("--app-out", "--m4-out", "--out", "--axf", dest="app_out_file", metavar="PATH",
                   help="Host application ELF (.out/.axf) with .log_fmt (core 0; Cortex-M4 "
                        "or M55). Also used for _SEGGER_RTT / %%s resolution. "
                        "Aliases: --m4-out, --out, --axf.")
    p.add_argument("--nwp-out", "--ta-out", "--noload", dest="nwp_out_file", metavar="PATH",
                   help="NWP ELF with .log_fmt (e.g. nwp_logging_fw.out) for all SL_PRINTF / "
                        "FMT logs on core 1. Alias: --ta-out (legacy), --noload.")
    p.add_argument("--descriptor", "--manifest", dest="descriptor_path", metavar="PATH",
                   help="BTDM SystemView .txt (SYSVIEW_CaptiveCore.txt). Not needed for "
                        "firmware built with SL_LOGGING_ENABLE: SL_PRINTF, "
                        "sl_btdm_fast_log and sl_btdm_log are all address-based, so "
                        "--nwp-out decodes them. Only required for older NWP images that "
                        "still send numeric BTDM event ids (flags bit0=0).")
    p.add_argument("--descriptor-id-base", type=int, default=0, metavar="N",
                   help="Subtract N from each descriptor event_id (default: 0).")

    # Packet shape — must match host SL_LOG_CONFIG_ARG (SoC UART packs one size).
    p.add_argument("--max-args", type=_parse_max_args, default=10, metavar="N",
                   help=f"Arg slots per record; MUST match host SL_LOG_CONFIG_ARG "
                        f"(often 3 or 10). With --layout auto, host record size "
                        f"is 12+4*N+4. "
                        f"Range 1..{SL_LOG_CONFIG_ARG_HARD_MAX} (default 10).")
    p.add_argument("--layout",
                   choices=("auto", "nwp"),
                   default="auto",
                   help="On-wire layout. 'auto' (default): host sl_log_event_t (core 0 "
                        "and M4-repacked core 1) plus raw 28-byte NWP sl_log_entry_t. "
                        "'nwp': only core 1 traffic—28-byte NWP wire and host-packed "
                        "core 1 records (typical Host Mode UART); no core 0 host logs.")
    p.add_argument("--big-endian", action="store_true",
                   help="Use big-endian (default: little-endian).")

    # Display / logging
    p.add_argument("--no-arg-format", action="store_true",
                   help="Skip applying %% args to format strings.")
    p.add_argument("--event-width", type=int, default=EVENT_COLUMN_WIDTH, metavar="N",
                   help=f"Fixed EVENT column width (default {EVENT_COLUMN_WIDTH}). "
                        "Longer names are clipped with '...'; full text stays in MESSAGE. "
                        "Increase this if long debug ids collide with MESSAGE.")
    p.add_argument("--component", action="append", type=_parse_component_list,
                   metavar="NAME",
                   help="Print only these NWP (core 1) components (log_component_t). "
                        "Host / M4 (core 0) lines are never filtered. "
                        "Repeat the flag or pass a comma-separated list. "
                        "Example: --component WLAN_LMAC. "
                        "Names: " + ", ".join(LOG_COMPONENTS) + ".")
    p.add_argument("--min-level", type=_parse_log_level_name, default=None, metavar="LEVEL",
                   help="Hide decoded lines below this severity (" + LEVEL_RANK_CHOICES + "). "
                        "TRACE is treated as DEBUG. OVERFLOW markers are never hidden.")
    p.add_argument("--component-level", action="append", type=_parse_component_level_pairs,
                   metavar="COMP=LEVEL",
                   help="Per-component minimum level (core 1 lines only — ignored if the "
                        "line has no component). Repeat the flag or pass comma-separated "
                        "pairs, e.g. --component-level WLAN_LMAC=WARN,BT_BLE_CTRL=INFO.")
    p.add_argument("--component-style",
                   choices=("enum", "file", "fileline", "dir"),
                   default="enum", metavar="STYLE",
                   help="How --list-formats names host/M4 components from --app-out / --m4-out: "
                        "'enum' (default) emits log_component_t names (CM_PM for "
                        "clock/power-manager sources, COMMON otherwise); 'file' "
                        "uses the source stem, e.g. sli_event_engine; "
                        "'fileline' adds the call-site line, e.g. "
                        "sli_event_engine:137; 'dir' the owning component "
                        "directory read from DWARF (parent of src/). Live Core 1 "
                        "output always uses the 'ID : COMP : fmt' NWP tag.")
    p.add_argument("--log-file", metavar="PATH",
                   help="Also write decoded output to this file (plain text by default).")
    p.add_argument("--log-file-color", action="store_true",
                   help="Keep ANSI colour codes in --log-file output.")

    # Utility
    p.add_argument("--list-formats", action="store_true",
                   help="List all format strings (with addresses) from --app-out / --m4-out / --nwp-out and exit.")
    p.add_argument("--dump-discards", type=int, default=0, metavar="N",
                   help="Diagnostic: hex-dump the first N runs of bytes the framer "
                        "could not decode, with a scan for candidate NWP event_ids "
                        "(0xFExxxxxx). Use this to tell 'NWP logs never arrived' "
                        "apart from 'NWP logs arrived in an unexpected shape'.")

    args = p.parse_args()
    selected = set()
    if args.component:
        for group in args.component:
            selected.update(group)
    args.component_filter = selected

    comp_lv_map: Dict[str, str] = {}
    if args.component_level:
        for group in args.component_level:
            for comp, lvl in group:
                comp_lv_map[comp] = lvl
    args.component_level_map = comp_lv_map
    return args


# -------------------------------------------------------------------------
# Record parsing
# -------------------------------------------------------------------------
# These record shapes travel on the SoC VCOM stream:
#
#   host / M4 sl_log_event_t  (timestamp + epoch, core_id 0, or core_id 1 for
#                              NWP events the M4 forwards in Host mode)
#     timestamp(I) + epoch(I) + event_id + args[N] + arg_count/core_id/flags/version
#     size = 12 + 4*N + 4         -> 28 bytes for N=3
#
#   timestamp-less fallback   (the tail of every sl_log_event_t)
#     event_id + args[N] + arg_count/core_id/flags/version
#     size = 4 + 4*N + 4          -> 20 bytes for N=3
#     Matched only after a resync has skipped the timestamp, so that a record
#     whose timestamp has another width still decodes (it was 32 bits before the
#     epoch field was added). Such records show PC receive time.
#
#   NWP sl_log_entry_t        (leading uint64 timestamp, core_id 1, SL_LOG_MAX_ARGS=3)
#     timestamp(Q) + event_id + args[3] + arg_count/core_id/flags/version
#     size = 12 + 4*3 + 4 = 28 bytes  (was 24 when timestamp was uint32)
#
# --layout auto (default) accepts all of them. timestamp + epoch are read as one
# little-endian uint64, so for N=3 the host shape and the NWP shape are the same
# 28 bytes and differ only in core_id. No framing marker is on the wire: a record
# is recognised by its trailer and by event_id being a known .log_fmt address.
#
WIRE_LAYOUT_AUTO = "auto"
WIRE_LAYOUT_NWP = "nwp"
WIRE_LAYOUT_NO_TS = "no-ts"  # internal default for host-shaped helpers only

# NWP sl_log_entry_t is compiled with SL_LOG_MAX_ARGS = 3 (sl_logging.h) and is
# forwarded to the host verbatim, so its size never follows host SL_LOG_CONFIG_ARG.
NWP_ENTRY_ARGS = 3

# Largest epoch accepted in a host timestamp: 65535 wraps is almost 9 years.
HOST_EPOCH_MAX = 0xFFFF

# Seconds without new bytes after which a timestamp-less match on a short window
# is accepted (see _parse_buffer in main). One record takes about 2.5 ms at
# 115200 baud, so this is far longer than any gap inside a record.
SHORT_WINDOW_SETTLE_S = 0.1


def normalize_layout(layout: str) -> str:
    if layout == WIRE_LAYOUT_NWP:
        return WIRE_LAYOUT_NWP
    return WIRE_LAYOUT_AUTO


def layout_has_timestamp(layout: str) -> bool:
    return normalize_layout(layout) == WIRE_LAYOUT_NWP


def record_size_fixed(max_args: int, layout: str = WIRE_LAYOUT_NO_TS) -> int:
    # NWP sl_log_entry_t uses a packed uint64 timestamp (+4 bytes vs older uint32).
    if layout_has_timestamp(layout):
        return 12 + 4 * max_args + 4
    return 4 + 4 * max_args + 4


def mk_struct_fmt(max_args: int, big_endian: bool, layout: str = WIRE_LAYOUT_NO_TS) -> str:
    endian = ">" if big_endian else "<"
    if layout_has_timestamp(layout):
        # Q = uint64 timestamp, I = event_id, then args + trailer bytes
        return endian + ("QI" + "I" * max_args + "BBBB")
    return endian + ("I" + "I" * max_args + "BBBB")


class RecordSpec:
    """One concrete on-wire record shape the decoder is willing to accept."""

    __slots__ = ("name", "max_args", "has_ts", "expect_core", "size", "fmt")

    def __init__(self, name: str, max_args: int, big_endian: bool,
                 has_ts: bool, expect_core: Optional[int]) -> None:
        layout = WIRE_LAYOUT_NWP if has_ts else WIRE_LAYOUT_NO_TS
        self.name = name
        self.max_args = max_args
        self.has_ts = has_ts
        self.expect_core = expect_core
        self.size = record_size_fixed(max_args, layout)
        self.fmt = mk_struct_fmt(max_args, big_endian, layout)

    def __repr__(self) -> str:  # pragma: no cover - debug aid
        core = "any" if self.expect_core is None else str(self.expect_core)
        return f"<{self.name} {self.size}B args={self.max_args} core={core}>"


def build_record_specs(layout: str, max_args: int, big_endian: bool) -> List[RecordSpec]:
    """Ordered candidate shapes. Strictest (NWP wire) first so it wins ambiguity."""
    mode = normalize_layout(layout)
    # nwp: core 1 only. auto: core 0 plus core 1 forwarded by the M4 (Host mode).
    host_core = 1 if mode == WIRE_LAYOUT_NWP else None
    # Raw 28-byte NWP sl_log_entry_t (timestamp + core_id 1).
    nwp_wire = RecordSpec("nwp", NWP_ENTRY_ARGS, big_endian, has_ts=True, expect_core=1)
    # Current sl_log_event_t: timestamp + epoch ahead of event_id.
    host_ts = RecordSpec("host-ts", max_args, big_endian, has_ts=True,
                         expect_core=host_core)
    # Timestamp-less fallback: the event_id..trailer tail of a host record.
    # Tried last: its window is the shortest, so it is the easiest one to
    # satisfy by accident.
    host = RecordSpec("host-nwp" if host_core == 1 else "host", max_args, big_endian,
                      has_ts=False, expect_core=host_core)
    specs: List[RecordSpec] = []
    # For SL_LOG_CONFIG_ARG = 3 host-ts is byte-identical to the NWP wire shape
    # and already covers it. The NWP wire shape is also dropped when it has the
    # size of the timestamp-less shape, as size alone cannot tell those apart.
    if nwp_wire.fmt != host_ts.fmt and nwp_wire.size != host.size:
        specs.append(nwp_wire)
    specs.extend((host_ts, host))
    return specs


def parse_record_spec(chunk: bytes,
                      spec: RecordSpec) -> Tuple[int, int, Tuple[int, ...], int, int, int, int]:
    vals = struct.unpack(spec.fmt, chunk)
    n = spec.max_args
    if spec.has_ts:
        ts, event_id, *rest = vals
    else:
        event_id, *rest = vals
        ts = 0
    args = tuple(rest[:n])
    arg_count, core_id, flags, version = rest[n : n + 4]
    return ts, event_id, args, arg_count, core_id, flags, version


def parse_record(chunk: bytes, fmt: str, max_args: int,
                 layout: str = WIRE_LAYOUT_NO_TS) -> Tuple[int, int, Tuple[int, ...], int, int, int, int]:
    vals = struct.unpack(fmt, chunk)
    if layout_has_timestamp(layout):
        ts, event_id, *rest = vals
        args = tuple(rest[:max_args])
        arg_count, core_id, flags, version = rest[max_args : max_args + 4]
    else:
        event_id, *rest = vals
        ts = 0
        args = tuple(rest[:max_args])
        arg_count, core_id, flags, version = rest[max_args : max_args + 4]
    return ts, event_id, args, arg_count, core_id, flags, version


# Firmware: SL_LOG_FLAG_FORMATTED_STRING=1, SL_LOG_FLAG_EVENT_ID=0 (flags bit0).
# Host M4 historically emits format-string records with bit0=0 and event_id = VMA.
# NWP WSL_PRINTF_FMT sets bit0=1. Treat a VMA that hits .log_fmt as a format log
# regardless of bit0. Do not mask arg_count: host uses full 0..SL_LOG_CONFIG_ARG.
SL_LOG_FLAG_FORMATTED_STRING = 0x01


def is_formatted_string_log(flags: int) -> bool:
    """True when firmware marked flags bit0 (NWP FMT). Host often leaves bit0=0."""
    return (int(flags) & SL_LOG_FLAG_FORMATTED_STRING) != 0


def debug_id_from_fmt(fmt_str: str) -> Optional[str]:
    """Extract leading debug-id token from '.log_fmt' payload: 'ID : COMP : fmt'."""
    if not fmt_str:
        return None
    head = fmt_str.split(":", 1)[0].strip()
    return head or None


def component_from_fmt(fmt_str: str, field: int = 1) -> Optional[str]:
    """Component field of an NWP '.log_fmt' payload: 'ID : COMP : fmt'.

    ``field`` selects the colon-separated field to read: 1 for a full NWP log
    string, 0 for a --descriptor format column, where the id is a separate
    column already. Only values from firmware ``log_component_t`` are accepted;
    this prevents ordinary message text from being mistaken for a component.
    """
    if not fmt_str:
        return None
    fields = fmt_str.split(":")
    if len(fields) <= field:
        return None
    token = fields[field].strip()
    token = LOG_COMPONENT_ALIASES.get(token, token)
    return token if token in LOG_COMPONENT_SET else None


# Populated by main(): per-core .log_fmt VMA ranges + optional SYSVIEW max id.
_HOST_FMT_MIN: int = 0
_HOST_FMT_MAX: int = 0
_TA_FMT_MIN: int = 0
_TA_FMT_MAX: int = 0
_MANIFEST_MAX_ID: int = 0
_WIRE_HAS_TS: bool = True
# Start address of every host format string; see _host_fmt_zero_based().
_HOST_FMT_ADDRS: frozenset = frozenset()


def set_event_id_bounds(host_fmt_min: int, host_fmt_max: int,
                        ta_fmt_min: int, ta_fmt_max: int,
                        manifest_max_id: int,
                        has_timestamp: bool = True,
                        host_fmt_addrs: Optional[Iterable[int]] = None) -> None:
    """Configure per-core event_id sanity ranges."""
    global _HOST_FMT_MIN, _HOST_FMT_MAX, _TA_FMT_MIN, _TA_FMT_MAX
    global _MANIFEST_MAX_ID, _WIRE_HAS_TS, _HOST_FMT_ADDRS
    _HOST_FMT_MIN = int(host_fmt_min)
    _HOST_FMT_MAX = int(host_fmt_max)
    _TA_FMT_MIN = int(ta_fmt_min)
    _TA_FMT_MAX = int(ta_fmt_max)
    _MANIFEST_MAX_ID = int(manifest_max_id)
    _WIRE_HAS_TS = bool(has_timestamp)
    _HOST_FMT_ADDRS = frozenset(host_fmt_addrs or ())


def _event_id_in_host_fmt(event_id: int) -> bool:
    return bool(_HOST_FMT_MAX) and _HOST_FMT_MIN <= event_id <= _HOST_FMT_MAX


def _event_id_in_ta_fmt(event_id: int) -> bool:
    return bool(_TA_FMT_MAX) and _TA_FMT_MIN <= event_id <= _TA_FMT_MAX


def _event_id_in_fmt_range(event_id: int) -> bool:
    return _event_id_in_host_fmt(event_id) or _event_id_in_ta_fmt(event_id)


def _host_fmt_zero_based() -> bool:
    """True when the host .log_fmt is linked at VMA 0, as LLD does.

    Compact .log_fmt is a non-allocated (INFO) section. GNU ld still gives it
    the current location counter, a non-zero flash address; LLD always places non-allocated
    sections at address 0. Host event ids are then plain offsets 0..size, so
    "inside the .log_fmt range" holds for nearly any small word (0 included)
    and says nothing about whether a record really starts there.
    """
    return bool(_HOST_FMT_MAX) and _HOST_FMT_MIN == 0


def _event_id_in_distinctive_fmt_range(event_id: int) -> bool:
    """In a .log_fmt range that ordinary data is unlikely to fall into."""
    if _event_id_in_ta_fmt(event_id):
        return True
    return _event_id_in_host_fmt(event_id) and not _host_fmt_zero_based()


def validate_log_fields(ts: int, event_id: int, arg_count: int,
                        core_id: int, flags: int, version: int, max_args: int,
                        has_ts: bool = True,
                        expect_core: Optional[int] = None) -> bool:
    """Heuristics that reject junk so resync can find a real record.

    Checks core/level/version plausibility, then a per-core event_id range:
    core 0 -> host .log_fmt VMA, core 1 -> NWP .log_fmt VMA or descriptor id.
    ``expect_core`` pins the core a shape is allowed to carry, which is what
    keeps the 20-byte host and 28-byte NWP shapes from matching each other.
    """
    if arg_count > max_args:
        return False

    if core_id > 1:
        return False
    if expect_core is not None and core_id != expect_core:
        return False

    # SL_LOG_SIGNAL_LOST / overflow marker. Requires a plausible version so a
    # run of 0xFF filler bytes cannot lock framing.
    if event_id == 0xFFFFFFFF:
        return 1 <= version <= 2

    # The NWP never reports timestamp 0. The host does, until its timer has been
    # synchronised with the NWP, so 0 is only junk on core 1.
    if has_ts and ts == 0 and core_id != 0:
        return False
    # The upper half of a host timestamp is the epoch: wraps of a 32-bit
    # microsecond counter, about 71 minutes each. A large epoch means the
    # window is misaligned and holds other data where the timestamp should be.
    if has_ts and core_id == 0 and (ts >> 32) > HOST_EPOCH_MAX:
        return False

    lvl_code = (flags >> 1) & 0x07
    # Host: DEBUG..ERROR = 1..4; NWP also CRASH=5. Reject 0 and >5.
    if lvl_code == 0 or lvl_code > 5:
        # Host sometimes packs level differently; accept known .log_fmt VMA.
        if not _event_id_in_distinctive_fmt_range(event_id):
            return False
    if version == 0 or version > 2:
        if not _event_id_in_distinctive_fmt_range(event_id):
            return False
    if flags & 0xF0:
        if not _event_id_in_distinctive_fmt_range(event_id):
            return False

    # Per-core event_id range (kills most false-positive resyncs).
    if core_id == 0:
        if _host_fmt_zero_based() and _HOST_FMT_ADDRS:
            # The range starts at 0 and proves nothing; require the exact
            # address of a format string.
            if event_id not in _HOST_FMT_ADDRS:
                return False
        elif _HOST_FMT_MAX and not _event_id_in_host_fmt(event_id):
            return False
    else:
        # core 1: NWP .log_fmt VMA OR numeric SYSVIEW descriptor id
        if _event_id_in_ta_fmt(event_id):
            return True
        if _MANIFEST_MAX_ID and event_id <= _MANIFEST_MAX_ID + 16:
            return True
        if _TA_FMT_MAX or _MANIFEST_MAX_ID:
            return False
    return True


def level_from_flags(flags: int) -> str:
    lvl_code = (flags >> 1) & 0x07
    return LEVEL_MAP.get(lvl_code, f"LEVEL{lvl_code}")


def spec_matches(buffer: bytes, offset: int, spec: RecordSpec) -> bool:
    """True when a record of shape ``spec`` starts at ``buffer[offset]``."""
    if offset + spec.size > len(buffer):
        return False
    try:
        ts, event_id, _args, arg_count, core_id, flags, version = parse_record_spec(
            bytes(buffer[offset : offset + spec.size]), spec
        )
    except Exception:
        return False
    return validate_log_fields(ts, event_id, arg_count, core_id, flags, version,
                               spec.max_args, has_ts=spec.has_ts,
                               expect_core=spec.expect_core)


def match_spec_at(buffer: bytes, offset: int,
                  specs: Sequence[RecordSpec]) -> Optional[RecordSpec]:
    for spec in specs:
        if spec_matches(buffer, offset, spec):
            return spec
    return None


def find_spec_sync(buffer: bytes, specs: Sequence[RecordSpec],
                   max_search: int = 50,
                   no_ts_window: int = 0) -> Tuple[int, Optional[RecordSpec]]:
    """Scan forward for the first offset where any shape matches.

    Returns (offset, spec). ``spec is None`` means nothing matched within the
    window, and the caller should drop one byte and retry.

    ``no_ts_window`` is the number of bytes that must follow an offset before a
    timestamp-less shape is tried there. Set it to the widest shape so that a
    timestamped record that has only partly arrived is not taken for a shorter
    timestamp-less one starting inside it. 0 applies no restriction.
    """
    if not specs:
        return 1, None
    smallest = min(spec.size for spec in specs)
    if len(buffer) < smallest:
        return 0, None
    search_limit = min(max_search, len(buffer) - smallest + 1)
    for offset in range(max(search_limit, 1)):
        if offset + no_ts_window > len(buffer):
            candidates: Sequence[RecordSpec] = [s for s in specs if s.has_ts]
        else:
            candidates = specs
        spec = match_spec_at(buffer, offset, candidates)
        if spec is not None:
            return offset, spec
    return 1, None


def is_valid_record(chunk: bytes, fmt: str, max_args: int, record_size: int,
                    layout: str = WIRE_LAYOUT_NO_TS) -> bool:
    if len(chunk) != record_size:
        return False
    try:
        ts, event_id, _args, arg_count, core_id, flags, version = parse_record(
            chunk, fmt, max_args, layout
        )
        return validate_log_fields(ts, event_id, arg_count, core_id, flags, version,
                                   max_args, has_ts=layout_has_timestamp(layout))
    except Exception:
        return False


def find_record_sync(buffer: bytearray, fmt: str, max_args: int,
                     record_size: int, max_search: int = 50,
                     layout: str = WIRE_LAYOUT_NO_TS) -> int:
    if len(buffer) < record_size:
        return 0
    search_limit = min(max_search, len(buffer) - record_size + 1)
    for offset in range(search_limit):
        if is_valid_record(buffer[offset : offset + record_size], fmt, max_args,
                           record_size, layout):
            return offset
    return 1


# -------------------------------------------------------------------------
# ELF helpers: .log_fmt, _SEGGER_RTT, %s string resolver
# -------------------------------------------------------------------------
def load_log_fmt_map(out_path: str) -> Tuple[Dict[int, str], int, int]:
    """Returns (addr -> format string map, section_base, section_end_exclusive)."""
    try:
        with open(out_path, "rb") as f:
            elf = ELFFile(f)
            sec = elf.get_section_by_name(".log_fmt")
            if sec is None:
                print("Warning: .log_fmt section not found in OUT file.")
                return {}, 0, 0
            base_addr = int(sec["sh_addr"])
            data = sec.data()
            n = len(data)

        fmt_map: Dict[int, str] = {}
        i = 0
        while i < n:
            if data[i] == 0:
                i += 1
                continue
            start = i
            while i < n and data[i] != 0:
                i += 1
            s = data[start:i].decode("utf-8", errors="replace")
            fmt_map[base_addr + start] = s
            i += 1
        return fmt_map, base_addr, base_addr + n
    except Exception as e:
        print(f"ERROR: Failed to load format strings from {out_path}: {e}")
        return {}, 0, 0


def list_format_strings(
    fmt_map: Dict[int, str],
    component_of: Optional[Callable[[int, str], Optional[str]]] = None,
) -> None:
    """Pretty-print the .log_fmt strings loaded from the OUT file."""
    if not fmt_map:
        print("No format strings found in the .out file.")
        return

    print("\n" + "=" * 100)
    print("FORMAT STRINGS FROM .out FILE")
    print("=" * 100)
    print(f"Total format strings: {len(fmt_map)}\n")
    comp_header = f"{'Component':<24} " if component_of else ""
    print(f"{'Address':<12} {'Hex Address':<14} {comp_header}Format String")
    print("-" * 100)

    for addr in sorted(fmt_map.keys()):
        fmt_str = fmt_map[addr]
        display = fmt_str.replace("\n", "\\n").replace("\r", "\\r").replace("\t", "\\t")
        if len(display) > 70:
            display = display[:67] + "..."
        comp_col = ""
        if component_of:
            comp_col = f"{(component_of(addr, fmt_str) or '-'):<24} "
        print(f"{addr:<12} 0x{addr:08X}    {comp_col}{display}")

    print("-" * 100)
    print(f"\nTotal: {len(fmt_map)} format strings\n")


class LogFmtComponentMap:
    """Maps a host .log_fmt address to the source file that emitted the string.

    ``sl_printf_common()`` stores every format string as a named static in
    .log_fmt - ``static const char logstr_<__LINE__>0[]`` - so the ELF symbol
    table holds one symbol per string, and the STT_FILE symbol preceding a run
    of them names the translation unit. The trailing 0 of the symbol name comes
    from SLI_LOG_UNIQUE_NAME(), and GCC may append its own '.<n>' suffix to
    disambiguate statics, so ``logstr_1370.12`` is line 137.

    The default ``style='enum'`` returns a canonical ``log_component_t`` name:
    CM_PM for clock/power-manager translation units and COMMON for all other
    host sources. ``style='dir'`` additionally reads DWARF compilation-unit
    paths to name the component directory (the parent of ``src/``).
    """

    _SYM_LINE_RE = re.compile(r"^logstr_(\d+?)0(?:\.\d+)?$")

    def __init__(self, elf_path: str, style: str = "enum") -> None:
        self._style = style
        self._starts: List[int] = []
        # Parallel to _starts: (end_addr, source file name, call-site line).
        self._entries: List[Tuple[int, str, Optional[int]]] = []
        self._dirs: Dict[str, str] = {}
        self._cache: Dict[int, Optional[str]] = {}
        self._load(elf_path)

    def __len__(self) -> int:
        return len(self._starts)

    def _load(self, elf_path: str) -> None:
        try:
            with open(elf_path, "rb") as fp:
                elf = ELFFile(fp)
                sec_index = elf.get_section_index(".log_fmt")
                symtab = elf.get_section_by_name(".symtab")
                if sec_index is None or symtab is None:
                    return
                symbols: List[Tuple[int, int, str, Optional[int]]] = []
                src_file: Optional[str] = None
                for sym in symtab.iter_symbols():
                    if sym["st_info"]["type"] == "STT_FILE":
                        src_file = sym.name
                        continue
                    if not sym.name or src_file is None:
                        continue
                    if sym["st_shndx"] != sec_index:
                        continue
                    match = self._SYM_LINE_RE.match(sym.name)
                    line = int(match.group(1)) if match else None
                    symbols.append((int(sym["st_value"]), int(sym["st_size"]),
                                    src_file, line))
                symbols.sort()
                self._starts = [addr for addr, _, _, _ in symbols]
                self._entries = [(addr + size, src, line)
                                 for addr, size, src, line in symbols]
                if self._style == "dir":
                    self._load_component_dirs(elf)
        except Exception as e:
            print(f"Warning: no host component info from "
                  f"{Path(elf_path).name}: {e}")

    def _load_component_dirs(self, elf: ELFFile) -> None:
        """Map source file name -> owning component directory, from DWARF."""
        if not elf.has_dwarf_info():
            print("Warning: --component-style dir needs DWARF in --app-out / --m4-out; "
                  "falling back to file names.")
            return
        for cu in elf.get_dwarf_info().iter_CUs():
            name = cu.get_top_DIE().attributes.get("DW_AT_name")
            if name is None:
                continue
            path = name.value
            if isinstance(path, bytes):
                path = path.decode("utf-8", errors="replace")
            segments = [s for s in path.replace("\\", "/").split("/") if s]
            if len(segments) < 2:
                continue
            if "src" in segments[:-1]:
                owner = segments[segments.index("src") - 1]
            else:
                owner = segments[-2]
            if owner:
                self._dirs.setdefault(segments[-1], owner)

    def describe(self, addr: int) -> Optional[str]:
        """Component name for a .log_fmt address, or None if unattributable."""
        if addr in self._cache:
            return self._cache[addr]
        name = self._describe_uncached(addr)
        self._cache[addr] = name
        return name

    def _describe_uncached(self, addr: int) -> Optional[str]:
        if not self._starts:
            return None
        i = bisect.bisect_right(self._starts, addr) - 1
        if i < 0:
            return None
        _end, src_file, line = self._entries[i]
        stem = Path(src_file).stem or src_file
        if self._style == "enum":
            lowered = stem.lower()
            if ("power_manager" in lowered
                    or "clock_manager" in lowered
                    or lowered.startswith(("sl_si91x_power", "sli_si91x_power",
                                           "sl_si91x_clock", "sli_si91x_clock"))):
                return "CM_PM"
            return "COMMON"
        name = self._dirs.get(src_file, stem) if self._style == "dir" else stem
        # Each string is its own symbol, so a line number is only trustworthy
        # for an exact hit. A string without a symbol of its own still belongs
        # to this file, since .log_fmt input sections are per translation unit.
        if self._style == "fileline" and line is not None and self._starts[i] == addr:
            return f"{name}:{line}"
        return name


def find_rtt_block_address(out_path: str) -> Optional[int]:
    """Find SEGGER RTT control block in an ELF: _SEGGER_RTT symbol, then magic-string scan."""
    if not out_path or not os.path.exists(out_path):
        return None
    try:
        with open(out_path, "rb") as f:
            elf = ELFFile(f)
            for sec in elf.iter_sections():
                iter_symbols = getattr(sec, "iter_symbols", None)
                if not callable(iter_symbols):
                    continue
                try:
                    for sym in iter_symbols():
                        if sym.name == "_SEGGER_RTT":
                            value = int(sym["st_value"])
                            if value:
                                return value & 0xFFFFFFFF
                except Exception:
                    continue

            magic = b"SEGGER RTT\x00\x00\x00\x00\x00\x00"
            for sec in elf.iter_sections():
                try:
                    sh_addr = int(sec["sh_addr"])
                    sh_size = int(sec["sh_size"])
                    sh_flags = int(sec["sh_flags"])
                    sh_type = sec["sh_type"]
                except Exception:
                    continue
                if not sh_addr or not sh_size:
                    continue
                if not (sh_flags & 0x2) or sh_type != "SHT_PROGBITS":
                    continue
                try:
                    data = sec.data()
                except Exception:
                    continue
                idx = data.find(magic)
                if idx != -1:
                    return (sh_addr + idx) & 0xFFFFFFFF
    except Exception as e:
        print(f"Warning: Could not scan {out_path} for RTT control block: {e}")
    return None


class ELFStringResolver:
    """Read null-terminated strings from PROGBITS sections for %s pointer args.

    Accepts one ELF path or a list (M4 .out then NWP image).
    """

    def __init__(self, out_path: Optional[str] = None,
                 out_paths: Optional[Sequence[str]] = None):
        self._fps: list = []
        self._sections: list = []
        self._cache: Dict[int, str] = {}
        paths: List[str] = []
        if out_paths:
            paths.extend([p for p in out_paths if p])
        elif out_path:
            paths.append(out_path)
        for path in paths:
            try:
                fp = open(path, "rb")
                elf = ELFFile(fp)
                self._fps.append(fp)
                for sec in elf.iter_sections():
                    try:
                        # A .log_fmt linked at VMA 0 (LLD) is left out: it would
                        # shadow low RAM addresses, and a %s argument pointing
                        # at a RAM buffer would print an unrelated format string.
                        if ((sec.name == ".log_fmt" and sec["sh_addr"])
                                or (sec["sh_addr"] and sec["sh_size"]
                                    and (sec["sh_flags"] & 0x2)
                                    and sec["sh_type"] == "SHT_PROGBITS")):
                            self._sections.append(sec)
                    except Exception:
                        continue
            except Exception:
                continue

    def close(self) -> None:
        for fp in self._fps:
            try:
                fp.close()
            except Exception:
                pass
        self._fps = []

    def read_cstring(self, addr: int, max_len: int = 256) -> Optional[str]:
        if addr in self._cache:
            return self._cache[addr]
        for sec in self._sections:
            sh_addr = sec["sh_addr"]
            sh_size = sec["sh_size"]
            if sh_addr <= addr < (sh_addr + sh_size):
                try:
                    data = sec.data()
                    offset = addr - sh_addr
                    raw = data[offset : min(len(data), offset + max_len)]
                    z = raw.find(b"\x00")
                    if z != -1:
                        raw = raw[:z]
                    s = raw.decode("utf-8", errors="replace")
                    if s and any(ch.isprintable() for ch in s):
                        self._cache[addr] = s
                        return s
                except Exception:
                    continue
        return None


# -------------------------------------------------------------------------
# SystemView descriptor (core 1)
# -------------------------------------------------------------------------
def _parse_systemview_descriptor_line(line: str) -> Optional[Tuple[int, str, str]]:
    stripped = line.strip()
    if not stripped or stripped.startswith("#"):
        return None
    if stripped.startswith("M=") or stripped.startswith("V="):
        return None

    if "\t" in line:
        left, right = line.split("\t", 1)
        try:
            eid = int(left.strip())
        except ValueError:
            return None
        right = right.strip()
        if "  " in right:
            debug_id, fmt = right.split("  ", 1)
            return eid, debug_id.strip(), fmt.strip()
        return eid, right, ""

    if not stripped[0].isdigit():
        return None
    i = 0
    while i < len(stripped) and stripped[i].isdigit():
        i += 1
    try:
        eid = int(stripped[:i])
    except ValueError:
        return None
    rest = stripped[i:].lstrip()
    if "  " not in rest:
        return eid, rest.strip(), ""
    debug_id, fmt = rest.split("  ", 1)
    return eid, debug_id.strip(), fmt.strip()


def load_systemview_descriptor(descriptor_path: str, id_base: int = 0) -> Dict[int, Dict[str, Any]]:
    out: Dict[int, Dict[str, Any]] = {}
    skipped_negative = 0
    try:
        with open(descriptor_path, "r", encoding="utf-8") as f:
            for line in f:
                line = line.rstrip("\n\r")
                parsed = _parse_systemview_descriptor_line(line)
                if not parsed:
                    continue
                eid, debug_id, fmt = parsed
                key = eid - id_base
                if key < 0:
                    skipped_negative += 1
                    continue
                out[key] = {"debug_id": debug_id, "format": fmt, "raw_line": line}
        print(
            f"Loaded {len(out)} entries from SystemView descriptor "
            f"({Path(descriptor_path).name})"
            f"{f' (id_base={id_base})' if id_base else ''}."
        )
        if skipped_negative:
            print(f"Note: skipped {skipped_negative} rows with id < id_base ({id_base}).")
        return out
    except Exception as e:
        print(f"ERROR: Failed to load SystemView descriptor {descriptor_path}: {e}")
        return {}


# -------------------------------------------------------------------------
# printf-style arg substitution
# -------------------------------------------------------------------------
_PRINTF_SPEC_RE = re.compile(
    r"%(%)|%("
    r"(?:\d+\$)?"
    r"[#0\- +]*"
    r"\d*"
    r"(?:\.\d+)?"
    r"(?:hh|h|ll|l|z|t|j)?"
    r"([diuoxXpsc]))"
)


def _to_signed32(x: int) -> int:
    x &= 0xFFFFFFFF
    return x - 0x100000000 if x & 0x80000000 else x


def apply_args_to_format(fmt_str: str, args_list: List[int], arg_count: int,
                         resolver: Optional[ELFStringResolver] = None) -> str:
    if not fmt_str:
        return fmt_str
    max_args = min(arg_count, len(args_list))
    arg_idx = 0

    def repl(m: re.Match) -> str:
        nonlocal arg_idx
        if m.group(1) == "%":
            return "%"
        spec = m.group(3)
        if arg_idx >= max_args:
            return m.group(0)
        val = args_list[arg_idx]
        arg_idx += 1
        if spec in ("d", "i"):
            return str(_to_signed32(val))
        if spec == "u":
            return str(val & 0xFFFFFFFF)
        if spec == "x":
            return f"{val & 0xFFFFFFFF:x}"
        if spec == "X":
            return f"{val & 0xFFFFFFFF:X}"
        if spec == "p":
            return f"0x{val & 0xFFFFFFFF:08x}"
        if spec == "c":
            return chr(val & 0xFF)
        if spec == "s":
            if resolver:
                s = resolver.read_cstring(val)
                if s is not None:
                    return s
            return f"0x{val & 0xFFFFFFFF:08X}"
        return str(val)

    return _PRINTF_SPEC_RE.sub(repl, fmt_str)


# -------------------------------------------------------------------------
# Transports: serial + RTT
# -------------------------------------------------------------------------
def find_jlink_port() -> Optional[str]:
    desired = ("JLink CDC Uart Port", "JLink CDC UART", "J-Link")
    for p in list_ports.comports():
        desc = p.description or ""
        for d in desired:
            if d.lower() in desc.lower():
                return p.device
    return None


def list_available_ports() -> None:
    ports = list_ports.comports()
    if not ports:
        print("No serial ports found.")
        return
    print("\nAvailable serial ports:")
    for p in ports:
        print(f"  {p.device}: {p.description}")


def open_serial(port: str, baud: int) -> serial.Serial:
    return serial.Serial(
        port=port, baudrate=baud,
        bytesize=serial.EIGHTBITS, parity=serial.PARITY_NONE,
        stopbits=serial.STOPBITS_ONE, timeout=0.0, write_timeout=0.0,
    )


class RttReader:
    """Minimal non-blocking RTT reader with read(n)/close() like serial.Serial."""

    def __init__(self, device: str, channel: int = 0,
                 jlink_serial: Optional[str] = None, interface: str = "swd",
                 speed: Any = "auto", block_address: Optional[int] = None,
                 reset: bool = True, connect_timeout: float = 15.0):
        if not PYLINK_AVAILABLE:
            raise RuntimeError("pylink-square is required for --source rtt. "
                               "Install with: pip install pylink-square")
        if not device:
            raise RuntimeError("--device is required for --source rtt.")

        self._channel = int(channel)
        self._extra_channels: List[int] = []
        self._jlink = pylink.JLink()

        try:
            if jlink_serial:
                try:
                    self._jlink.open(serial_no=int(jlink_serial))
                except ValueError:
                    self._jlink.open(serial_no=jlink_serial)
            else:
                self._jlink.open()
        except Exception as e:
            raise RuntimeError(f"J-Link open failed: {e}") from e

        iface = (pylink.enums.JLinkInterfaces.JTAG
                 if str(interface).lower() == "jtag"
                 else pylink.enums.JLinkInterfaces.SWD)
        self._jlink.set_tif(iface)

        try:
            self._jlink.connect(device, speed=speed, verbose=False)
        except Exception as e:
            self._jlink.close()
            hint = ""
            if "unsupported device" in str(e).lower():
                hint = (" Hint: pass an exact device name known to your J-Link "
                        "installation via --device. For Si91x try SiWG917M111M, "
                        "SiWG917M110L or SiWG917M100M; generic 'Cortex-M4' is "
                        "rejected by recent J-Link versions for Si91x.")
            raise RuntimeError(f"J-Link connect failed: {e}.{hint}") from e

        if reset:
            try:
                self._jlink.reset(halt=False)
            except Exception:
                pass

        try:
            self._jlink.rtt_stop()
        except Exception:
            pass
        try:
            self._jlink.rtt_start(block_address)
        except Exception as e:
            self._jlink.close()
            raise RuntimeError(f"RTT start failed: {e}") from e

        deadline = time.time() + connect_timeout
        num_up = 0
        while time.time() < deadline:
            try:
                num_up = self._jlink.rtt_get_num_up_buffers()
                self._jlink.rtt_get_num_down_buffers()
                if num_up > 0:
                    break
            except Exception:
                pass
            time.sleep(0.1)

        if num_up == 0:
            self._jlink.close()
            raise RuntimeError(
                "Timed out waiting for RTT control block. Increase --rtt-connect-timeout, "
                "verify SEGGER_RTT is initialized in firmware, or pass --rtt-block-address."
            )

        if self._channel < 0 or self._channel >= num_up:
            self._jlink.close()
            raise RuntimeError(
                f"RTT up-channel {self._channel} not available "
                f"(target exposes {num_up} up-buffer(s))."
            )

        print(f"RTT connected: device={device}, iface={interface.upper()}, "
              f"speed={speed}, up_buffers={num_up}, channel={self._channel}")

        # Drain stale ring-buffer contents so parsing starts on a record
        # boundary. Two-phase drain:
        #   1. Hard ceiling so we never block forever if firmware is logging
        #      continuously at line rate.
        #   2. Quiescence detector: stop as soon as the buffer stays empty
        #      for ~80 ms in a row - that's the cleanest possible start.
        drained = self._drain_channel(self._channel, max_secs=1.5, quiet_ms=80)
        if drained:
            print(f"  Discarded {drained} stale RTT bytes from ch{self._channel} "
                  f"to align on a record boundary.")

    def add_channel(self, channel: int, num_up: int) -> None:
        ch = int(channel)
        if ch < 0 or ch >= num_up:
            raise RuntimeError(
                f"RTT up-channel {ch} not available (target exposes {num_up} up-buffer(s))."
            )
        if ch == self._channel or ch in self._extra_channels:
            return
        self._extra_channels.append(ch)
        drained = self._drain_channel(ch, max_secs=1.5, quiet_ms=80)
        if drained:
            print(f"  Discarded {drained} stale RTT bytes from ch{ch} "
                  f"to align on a record boundary.")

    def _drain_channel(self, channel: int, max_secs: float = 1.5,
                       quiet_ms: int = 80) -> int:
        """Drain a channel until it stays empty for ``quiet_ms`` ms (or hits
        the ``max_secs`` ceiling).  Returns the total byte count discarded.
        """
        drained = 0
        deadline = time.time() + max_secs
        quiet_until = time.time() + (quiet_ms / 1000.0)
        while time.time() < deadline:
            try:
                chunk = self._jlink.rtt_read(channel, 4096)
            except Exception:
                break
            if chunk:
                drained += len(chunk)
                quiet_until = time.time() + (quiet_ms / 1000.0)
            else:
                if time.time() >= quiet_until:
                    break
                time.sleep(0.01)
        return drained

    @property
    def num_up_buffers(self) -> int:
        try:
            return self._jlink.rtt_get_num_up_buffers()
        except Exception:
            return 0

    @property
    def all_channels(self) -> List[int]:
        return [self._channel] + self._extra_channels

    def read(self, size: int) -> bytes:
        return self.read_channel(self._channel, size)

    def read_channel(self, channel: int, size: int) -> bytes:
        try:
            chunk = self._jlink.rtt_read(channel, size)
        except Exception as e:
            raise IOError(f"RTT read (ch{channel}) failed: {e}") from e
        return bytes(chunk) if chunk else b""

    def close(self) -> None:
        try:
            self._jlink.rtt_stop()
        except Exception:
            pass
        try:
            self._jlink.close()
        except Exception:
            pass


# -------------------------------------------------------------------------
# Decode
# -------------------------------------------------------------------------
def _format_from_fmt_map(event_id: int,
                         fmt_map: Dict[int, str],
                         args_val: Sequence[int],
                         arg_count: int,
                         no_arg_format: bool,
                         resolver: Optional[ELFStringResolver],
                         level_str: str,
                         is_ta_pool: bool = False,
                         component_map: Optional[LogFmtComponentMap] = None,
                         ) -> Optional[Tuple[str, str, Optional[str], Optional[str]]]:
    """Return (level, msg, event_display, component) if event_id hits a pool."""
    base_msg = fmt_map.get(event_id) if fmt_map else None
    if not base_msg and resolver is not None and event_id > 0x10000:
        base_msg = resolver.read_cstring(event_id)
    if not base_msg:
        return None
    event_display = debug_id_from_fmt(base_msg)
    # NWP strings carry their component inline; host strings do not, so their
    # component comes from the --app-out / --m4-out symbol table instead.
    if is_ta_pool:
        component = component_from_fmt(base_msg)
    else:
        component = component_map.describe(event_id) if component_map else None
    if not no_arg_format and arg_count > 0:
        msg = apply_args_to_format(base_msg, list(args_val), arg_count, resolver=resolver)
    else:
        msg = base_msg
    return level_str, msg.rstrip("\r\n"), event_display, component


def decode_message(event_id: int,
                   args_val: Sequence[int],
                   arg_count: int,
                   flags: int,
                   host_fmt_map: Dict[int, str],
                   ta_fmt_map: Dict[int, str],
                   manifest_map: Dict[int, Dict[str, Any]],
                   no_arg_format: bool,
                   core_id: int,
                   version: int,
                   resolver: Optional[ELFStringResolver],
                   max_args_supported: int = 10,
                   component_map: Optional[LogFmtComponentMap] = None,
                   ) -> Tuple[str, str, Optional[str], Optional[str]]:
    if event_id == 0xFFFFFFFF:
        overflow_count = args_val[0] if arg_count > 0 else 0
        return ("OVERFLOW",
                f"Buffer overflow detected! Count: {overflow_count}", None, None)

    level_str = level_from_flags(flags)
    arg_count = min(int(arg_count), max_args_supported)

    # Address lookup ordered by core, then the other core's pool as fallback.
    # Host (0x08xxxxxx) and NWP (0xFExxxxxx) .log_fmt ranges are disjoint, so the
    # fallback is unambiguous and survives a mislabelled core_id. A host pool
    # linked at VMA 0 is not a fallback for core 1: its addresses are small
    # integers and would claim numeric NWP event ids.
    if core_id == 0:
        pools = ((host_fmt_map, False), (ta_fmt_map, True))
    elif _host_fmt_zero_based():
        pools = ((ta_fmt_map, True),)
    else:
        pools = ((ta_fmt_map, True), (host_fmt_map, False))
    for pool, is_ta_pool in pools:
        hit = _format_from_fmt_map(event_id, pool, args_val, arg_count,
                                   no_arg_format, resolver, level_str,
                                   is_ta_pool=is_ta_pool,
                                   component_map=component_map)
        if hit:
            return hit

    if core_id == 0:
        return (level_str,
                f"<Unknown Event 0x{event_id:08X}> Args={list(args_val)[:arg_count]}",
                None, None)

    # core 1 fallthrough: legacy numeric sl_btdm_log via SYSVIEW descriptor.
    if is_formatted_string_log(flags) or _event_id_in_fmt_range(event_id):
        return (level_str,
                f"<Unknown fmt addr 0x{event_id:08X}> Args={list(args_val)[:arg_count]}",
                None, None)

    if not manifest_map:
        return (level_str,
                f"<Core1: no descriptor/nwp-out match> Args={list(args_val)[:arg_count]}",
                None, None)
    entry = manifest_map.get(event_id)
    if entry:
        fmt_str = entry.get("format", "") or ""
        debug_id = (entry.get("debug_id", "") or "").strip()
        event_display = debug_id if debug_id else "unknown"
        # Descriptor rows split the id off into its own column, so the component
        # is the leading field of the format instead of the second.
        component = component_from_fmt(fmt_str, field=0)
        full_row = (entry.get("raw_line") or "").strip()
        if not full_row:
            full_row = (f"{event_id}\t{debug_id}  {fmt_str.strip()}"
                        if debug_id else str(event_id))
        tab_prefix = f"{event_id}\t"
        if full_row.startswith(tab_prefix):
            body = full_row[len(tab_prefix):]
        else:
            body = (f"{debug_id}  {fmt_str.strip()}".strip()
                    if debug_id else fmt_str.strip())
        if not body:
            body = "(empty)"
        if (not no_arg_format) and arg_count > 0:
            msg = apply_args_to_format(body, list(args_val), arg_count, resolver=resolver)
        else:
            msg = body
        return level_str, msg, event_display, component
    return (level_str,
            f"Not in descriptor (id={event_id}). Args={list(args_val)[:arg_count]}",
            "unknown", None)


# -------------------------------------------------------------------------
# Main
# -------------------------------------------------------------------------
def main() -> None:
    args = parse_args()
    print_banner()

    # --app-out / --m4-out (M4) and --nwp-out (nwp_logging_fw.out) both contribute .log_fmt.
    # Descriptor is optional: only legacy EVENT_ID (numeric sl_btdm_log) needs it.
    def _require_elf(path: Optional[str], label: str) -> Optional[str]:
        if not path:
            return None
        if not os.path.exists(path):
            print(f"ERROR: {label} not found: {path}")
            sys.exit(1)
        return path

    app_out_path = _require_elf(args.app_out_file, "APP OUT file")
    nwp_out_path = _require_elf(args.nwp_out_file, "NWP OUT file")

    host_fmt_map: Dict[int, str] = {}
    ta_fmt_map: Dict[int, str] = {}
    host_fmt_min = host_fmt_max = 0
    ta_fmt_min = ta_fmt_max = 0

    if app_out_path:
        host_fmt_map, base, end = load_log_fmt_map(app_out_path)
        print(f"Loaded {len(host_fmt_map)} host format strings from .log_fmt "
              f"[0x{base:08X}..0x{end:08X}) ({Path(app_out_path).name})")
        if end > base:
            host_fmt_min, host_fmt_max = base, end - 1
        if host_fmt_map and base == 0:
            print("Host .log_fmt is linked at address 0 (LLVM/LLD build): "
                  "host event ids are section offsets.")

    if nwp_out_path:
        ta_fmt_map, base, end = load_log_fmt_map(nwp_out_path)
        print(f"Loaded {len(ta_fmt_map)} NWP format strings from .log_fmt "
              f"[0x{base:08X}..0x{end:08X}) ({Path(nwp_out_path).name})")
        if end > base:
            ta_fmt_min, ta_fmt_max = base, end - 1

    component_map: Optional[LogFmtComponentMap] = None
    if args.list_formats and app_out_path:
        component_map = LogFmtComponentMap(app_out_path, style=args.component_style)
        print(f"Mapped {len(component_map)} host format strings to source "
              f"files from the .log_fmt symbols of {Path(app_out_path).name}")

    if args.list_formats:
        merged = dict(host_fmt_map)
        merged.update(ta_fmt_map)
        if not merged:
            print("ERROR: --list-formats requires --app-out/--m4-out and/or --nwp-out with .log_fmt.")
            sys.exit(1)

        def _component_of(addr: int, fmt_str: str) -> Optional[str]:
            if addr in ta_fmt_map:
                return component_from_fmt(fmt_str)
            return component_map.describe(addr) if component_map else None

        list_format_strings(merged, component_of=_component_of)
        sys.exit(0)

    manifest_map: Dict[int, Dict[str, Any]] = {}
    if args.descriptor_path:
        if not os.path.exists(args.descriptor_path):
            print(f"ERROR: descriptor file not found: {args.descriptor_path}")
            sys.exit(1)
        manifest_map = load_systemview_descriptor(
            args.descriptor_path, id_base=args.descriptor_id_base
        )
    else:
        print("Decode: core0=host .log_fmt; core1=NWP .log_fmt"
              + (" (no --descriptor; legacy numeric sl_btdm_log won't decode)."
                 if ta_fmt_map else
                 " — pass --nwp-out and/or --descriptor for NWP."))

    manifest_max_id = max(manifest_map.keys()) if manifest_map else 0
    layout = normalize_layout(args.layout)
    set_event_id_bounds(host_fmt_min, host_fmt_max, ta_fmt_min, ta_fmt_max,
                        manifest_max_id, has_timestamp=layout_has_timestamp(layout),
                        host_fmt_addrs=host_fmt_map.keys())

    elf_paths = [p for p in (app_out_path, nwp_out_path) if p]
    resolver = ELFStringResolver(out_paths=elf_paths) if elf_paths else None

    max_args = args.max_args
    specs = build_record_specs(layout, max_args, args.big_endian)
    # Legacy single-shape values kept for the RTT pre-align helper.
    record_size = specs[0].size
    rec_fmt = specs[0].fmt

    shape_desc = ", ".join(
        f"{s.name}={s.size}B"
        + ("/ts" if s.has_ts else "/no-ts")
        + ("" if s.expect_core is None else f"/core{s.expect_core}")
        for s in specs
    )
    print(f"Framing [{args.source}]: layout={layout} [{shape_desc}], "
          f"host max_args={max_args}. "
          f"Decode: core0=--app-out/--m4-out .log_fmt, core1=--nwp-out .log_fmt then --descriptor.")
    print("TS/DELTA: firmware time from the record (us); host records logged "
          "before the host/NWP time sync carry 0. Records decoded without "
          "their timestamp show PC receive time since decoder start (us).")

    # ---- Open transport ----
    ser: Any
    transport_desc: str
    if args.source == "rtt":
        if not PYLINK_AVAILABLE:
            print("ERROR: --source rtt requires 'pylink-square'. "
                  "Install with: pip install pylink-square")
            sys.exit(1)

        rtt_block_addr = args.rtt_block_address
        block_source = "user"
        if rtt_block_addr is None and app_out_path:
            found = find_rtt_block_address(app_out_path)
            if found is not None:
                rtt_block_addr = found
                block_source = f"ELF ({Path(app_out_path).name})"
                print(f"RTT control block from {block_source}: 0x{rtt_block_addr:08X}")
            else:
                block_source = "J-Link auto-scan"
        elif rtt_block_addr is None:
            block_source = "J-Link auto-scan"

        try:
            ser = RttReader(
                device=args.device,
                channel=args.rtt_channel,
                jlink_serial=args.jlink_serial,
                interface=args.rtt_interface,
                speed=args.rtt_speed,
                block_address=rtt_block_addr,
                reset=args.rtt_reset,
                connect_timeout=args.rtt_connect_timeout,
            )
        except Exception as e:
            print(f"ERROR: Failed to open RTT: {e}")
            sys.exit(1)

        if args.rtt_channel2 is not None:
            try:
                ser.add_channel(args.rtt_channel2, ser.num_up_buffers)
                print(f"RTT channel {args.rtt_channel2} also enabled.")
            except Exception as e:
                print(f"WARNING: Could not add RTT channel {args.rtt_channel2}: {e}")

        block_desc = (f"0x{rtt_block_addr:08X} [{block_source}]"
                      if rtt_block_addr is not None else f"auto [{block_source}]")
        ch_desc = (f"ch{args.rtt_channel}+ch{args.rtt_channel2}"
                   if args.rtt_channel2 is not None else f"ch{args.rtt_channel}")
        transport_desc = (f"RTT {ch_desc} via J-Link "
                          f"({args.rtt_interface.upper()}, {args.device}, CB {block_desc})")
    else:
        port_name = args.port or find_jlink_port()
        if not port_name:
            print("ERROR: JLink CDC UART port not found.")
            list_available_ports()
            print("\nUse --port <port> to specify the serial port manually.")
            sys.exit(1)
        if not args.port:
            print(f"Auto-detected serial port: {port_name}")
        try:
            ser = open_serial(port_name, args.baud)
        except Exception as e:
            print(f"ERROR: Failed to open serial port {port_name}: {e}")
            list_available_ports()
            sys.exit(1)
        transport_desc = f"UART {port_name} @ {args.baud}"

    print()
    event_width = max(8, int(args.event_width))
    print_config(args.source, transport_desc, app_out_path or "", nwp_out_path or "",
                 len(host_fmt_map) + len(ta_fmt_map), args.descriptor_path,
                 descriptor_id_base=args.descriptor_id_base,
                 event_width=event_width)

    # ---- Decode loop ----
    read_size = 4096
    record_count = 0
    # "Initial" = bytes/episodes discarded BEFORE the first valid record was
    # decoded - these come from attaching mid-stream / stale ring-buffer
    # contents and are not lost log messages.
    # "Runtime" = bytes/episodes discarded AFTER decoding started - these
    # usually indicate firmware overran the RTT up-buffer (the host wasn't
    # draining fast enough), so log messages WERE lost.
    bytes_rx = 0  # total bytes read from transport (silence vs junk diagnostic)
    bytes_initial = 0
    bytes_runtime = 0
    resync_initial = 0
    resync_runtime = 0
    last_ts_by_core: Dict[int, Optional[int]] = {}

    log_file = None
    if args.log_file:
        try:
            log_file = open(args.log_file, "w", encoding="utf-8", buffering=1)
            print(f"Saving decoded log to: {args.log_file}  "
                  f"[{'with ANSI colours' if args.log_file_color else 'plain text'}]")
        except Exception as e:
            print(f"WARNING: Cannot open log file '{args.log_file}': {e}.")

    _ANSI_RE = re.compile(r"\x1b\[[0-9;]*m")

    _is_rtt_multi = (args.source == "rtt" and args.rtt_channel2 is not None)
    if _is_rtt_multi:
        _channel_buffers: Dict[int, bytearray] = {
            ch: bytearray() for ch in ser.all_channels  # type: ignore[union-attr]
        }
    else:
        _channel_buffers = {0: bytearray()}

    # ---- Pre-alignment: silently consume bytes until the first valid record
    # boundary so the main parser starts cleanly. Whatever we discard here is
    # the inevitable mid-record tail left over after the drain ended on a
    # time-based quiet window rather than a frame boundary, and would
    # otherwise show up as the "Bytes discarded (alignment)" overhead in the
    # final summary. Not counted toward bytes_initial.
    def _read_one(ch: int) -> bytes:
        if args.source == "rtt":
            try:
                return ser.read_channel(ch, read_size)  # type: ignore[union-attr]
            except Exception:
                return b""
        try:
            return ser.read(read_size)
        except Exception:
            return b""

    def _pre_align_channel(ch: int, buf: bytearray) -> int:
        """Walk forward until a valid record sits at offset 0 of buf. Returns
        the number of bytes silently consumed during alignment."""
        consumed = 0
        deadline = time.time() + 2.0
        min_window = record_size * 2
        while time.time() < deadline:
            data = _read_one(ch)
            if data:
                buf.extend(data)
            if len(buf) < min_window:
                if not data:
                    time.sleep(0.005)
                continue
            off = find_record_sync(buf, rec_fmt, max_args, record_size,
                                   max_search=min(256, len(buf) - record_size),
                                   layout=layout)
            if off == 0:
                return consumed
            if off > 0:
                if off >= len(buf):
                    consumed += off
                    del buf[:off]
                    continue
                consumed += off
                del buf[:off]
                if is_valid_record(bytes(buf[:record_size]), rec_fmt,
                                   max_args, record_size, layout):
                    return consumed
            if not data:
                time.sleep(0.005)
        return consumed

    # Only run pre-alignment for RTT - UART starts at byte 0 of the stream
    # so it never has a mid-record tail to skip.  Runs silently so the
    # decoded-log stream isn't preceded by noise lines while debugging.
    if args.source == "rtt":
        for _ch, _buf in _channel_buffers.items():
            actual_ch = _ch if _is_rtt_multi else args.rtt_channel
            _pre_align_channel(actual_ch, _buf)

    # Records decoded through the timestamp-less fallback have no firmware time.
    # Synthesize a receive-time counter (us since decoder start) so TS/DELTA are
    # still useful for them.
    host_ts_t0 = time.perf_counter()

    def _host_receive_ts_us() -> int:
        return int((time.perf_counter() - host_ts_t0) * 1_000_000.0)

    def handle_one_record(ts: int, event_id: int, args_val: Tuple[int, ...],
                          arg_count: int, core_id: int, flags: int,
                          version: int, has_ts: bool = True) -> None:
        nonlocal record_count
        record_count += 1
        if has_ts:
            display_ts = int(ts)
        else:
            # Synthetic receive timestamp (microseconds).
            display_ts = _host_receive_ts_us()

        last_ts = last_ts_by_core.get(core_id)
        delta = (display_ts - last_ts) if (last_ts is not None) else None
        last_ts_by_core[core_id] = display_ts

        level_str, msg, event_disp, component = decode_message(
            event_id=event_id, args_val=args_val, arg_count=arg_count, flags=flags,
            host_fmt_map=host_fmt_map, ta_fmt_map=ta_fmt_map,
            manifest_map=manifest_map,
            no_arg_format=args.no_arg_format,
            core_id=core_id, version=version, resolver=resolver,
            max_args_supported=max_args,
            component_map=component_map,
        )
        if not should_print_by_level(level_str, component, args, core_id):
            return
        # --component is NWP-only; host (core 0) records have no log_component_t.
        if (args.component_filter and core_id == 1 and component is not None
                and component not in args.component_filter):
            return
        if core_id != 0:
            msg = prefix_core1_message(msg, component)
        event_col = format_event_column(event_disp, width=event_width)
        delta_str = f"{delta}" if delta is not None else "-"
        ts_str = f"{display_ts}"
        level_field = f"[{colorize(f'{level_str:>5}', level_str)}]"
        output = (f"{get_current_time()} {level_field} "
                  f"{core_id:<6} {ts_str:>10} {delta_str:>12} {event_col} | {msg}\n")
        sys.stdout.write(output)
        sys.stdout.flush()
        if log_file:
            log_file.write(output if args.log_file_color else _ANSI_RE.sub("", output))

    dumps_left = max(0, int(args.dump_discards))

    def _dump_discard(raw: bytes) -> None:
        """Hex-dump undecodable bytes and flag any 0xFExxxxxx word inside them.

        An NWP .log_fmt VMA appearing here means NWP logs did reach the host but
        in a shape the framer does not accept; nothing here means they never
        arrived at all.
        """
        nonlocal dumps_left
        if dumps_left <= 0 or not raw:
            return
        dumps_left -= 1
        hexs = raw.hex(" ")
        ascii_view = "".join(chr(b) if 32 <= b < 127 else "." for b in raw)
        hits = []
        for off in range(0, max(0, len(raw) - 3)):
            word = int.from_bytes(raw[off : off + 4], "little")
            if _event_id_in_ta_fmt(word) or _event_id_in_host_fmt(word):
                pool = "NWP" if _event_id_in_ta_fmt(word) else "host"
                hits.append(f"+{off}:0x{word:08X}({pool})")
        line = f"[discard {len(raw)}B] {hexs}\n             ascii: {ascii_view}\n"
        if hits:
            line += f"             fmt-VMA candidates: {', '.join(hits[:8])}\n"
        sys.stdout.write(line)
        sys.stdout.flush()

    def _account_discard(n: int, is_resync_episode: bool = False,
                         raw: Optional[bytes] = None) -> None:
        """Charge ``n`` discarded bytes to either the initial or runtime bucket.

        Silent unless --dump-discards asked for hex dumps: counters are reported
        in the final summary so the live decoded-log stream stays clean.
        """
        nonlocal bytes_initial, bytes_runtime, resync_initial, resync_runtime
        if record_count == 0:
            bytes_initial += n
            if is_resync_episode:
                resync_initial += 1
        else:
            bytes_runtime += n
            if is_resync_episode:
                resync_runtime += 1
        if raw is not None:
            _dump_discard(raw)

    smallest_spec = min(s.size for s in specs)
    largest_spec = max(s.size for s in specs)
    # Updated by the read loop whenever bytes arrive.
    last_rx_time = time.monotonic()

    def _parse_buffer(buf: bytearray) -> None:
        while len(buf) >= smallest_spec:
            # The timestamp-less shape is the shortest one, and part of a
            # timestamped record that is still arriving can pass for it
            # (timestamp or epoch read as event_id), most easily when .log_fmt
            # is linked at address 0. While bytes keep coming, such a match
            # needs a window as long as the widest shape. Once the line is
            # quiet the rest of a record is not coming, and what is buffered
            # is judged as it is.
            line_quiet = (time.monotonic() - last_rx_time) >= SHORT_WINDOW_SETTLE_S
            no_ts_window = 0 if line_quiet else largest_spec
            spec = match_spec_at(buf, 0, specs)
            if spec is not None and not spec.has_ts and len(buf) < no_ts_window:
                break
            if spec is None:
                # Not aligned here. Keep enough bytes for the widest shape so a
                # 28-byte NWP record is never judged on a truncated window.
                if len(buf) < largest_spec and not line_quiet:
                    break
                skip, spec = find_spec_sync(buf, specs,
                                            max_search=min(256, len(buf)),
                                            no_ts_window=no_ts_window)
                if spec is None:
                    n = max(skip, 1)
                    _account_discard(n, raw=bytes(buf[:n]))
                    del buf[:n]
                    continue
                if skip > 0:
                    _account_discard(skip, is_resync_episode=True,
                                     raw=bytes(buf[:skip]))
                    del buf[:skip]
                    if len(buf) < spec.size:
                        break
            chunk = bytes(buf[:spec.size])
            try:
                (ts, event_id, args_val, arg_count,
                 core_id, flags, version) = parse_record_spec(chunk, spec)
            except Exception:
                del buf[:spec.size]
                _account_discard(spec.size)
                continue
            del buf[:spec.size]
            handle_one_record(ts, event_id, args_val, arg_count,
                              core_id, flags, version, has_ts=spec.has_ts)

    _dead_channels: set = set()

    try:
        while True:
            got_any = False
            if _is_rtt_multi:
                primary_ch = args.rtt_channel
                for ch, buf in list(_channel_buffers.items()):
                    if ch in _dead_channels:
                        continue
                    try:
                        data = ser.read_channel(ch, read_size)  # type: ignore[union-attr]
                    except Exception as e:
                        if ch == primary_ch:
                            sys.stdout.write(f"\n[ERROR] RTT read (ch{ch}) failed: {e}\n")
                            raise KeyboardInterrupt
                        sys.stdout.write(f"\n[WARNING] RTT ch{ch} read failed: {e}. "
                                         f"Skipping ch{ch}.\n")
                        _dead_channels.add(ch)
                        continue
                    if data:
                        bytes_rx += len(data)
                        buf.extend(data)
                        got_any = True
                        last_rx_time = time.monotonic()
                    _parse_buffer(buf)
            else:
                try:
                    data = ser.read(read_size)
                except Exception as e:
                    sys.stdout.write(f"\n[ERROR] Read failed: {e}\n")
                    break
                buf = _channel_buffers[0]
                if data:
                    bytes_rx += len(data)
                    buf.extend(data)
                    got_any = True
                    last_rx_time = time.monotonic()
                _parse_buffer(buf)

            if not got_any:
                time.sleep(0.001)

    except KeyboardInterrupt:
        print("\n" + "-" * 64)
        print()
        print(f"Console stopped. Records decoded: {record_count}")
        print(f"Bytes received: {bytes_rx}")
        print(f"Bytes discarded (pre-sync): {bytes_initial}  "
              f"(resync episodes: {resync_initial})")
        print(f"Bytes discarded (runtime):  {bytes_runtime}  "
              f"(resync episodes: {resync_runtime})")
        if record_count == 0 and bytes_rx == 0:
            print("HINT: transport was silent — no bytes on UART/RTT. Check that "
                  "the app is running, logging is enabled "
                  "(SL_SI91X_ENABLE_NWP_LOGGING + log configure), and for SoC "
                  "try --source rtt if binary logs go to RTT not VCOM.")
        elif record_count == 0 and bytes_rx > 0:
            print("HINT: bytes arrived but no valid record matched. "
                  "Check --max-args vs host SL_LOG_CONFIG_ARG; --layout auto "
                  "accepts the 12+4*N+4 host and 28-byte NWP shapes. "
                  "Check that --app-out is the ELF of the firmware "
                  "on the board: an LLVM build is matched on exact format-string "
                  "addresses. "
                  "ASCII printf on the same COM port is skipped until sync.")
        if log_file:
            print(f"Log saved to: {args.log_file}")
    finally:
        try:
            ser.close()
        except Exception:
            pass
        if resolver:
            try:
                resolver.close()
            except Exception:
                pass
        if log_file:
            try:
                log_file.close()
            except Exception:
                pass


if __name__ == "__main__":
    main()