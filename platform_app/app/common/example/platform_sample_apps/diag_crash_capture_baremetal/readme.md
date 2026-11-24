# SL Diagnostic Crash Capture Bare-metal

This example project demonstrates SL Diagnostic (`sl_diag`) with Crash Capture in a bare-metal configuration. Press the board's supported button input to trigger a fault. Crash Capture seals the CPU state into the retained SL Diagnostic record and resets the device. After the reset, the decoded record and a raw hex dump of the retained region are printed over VCOM. The app clears the record only after that export succeeds and `sl_diag_has_record()` is false, so the next button press can seal a new generation on any enabled profile. A failed export or a failed clear leaves the record in place.

## Table of Contents

- [Purpose / Scope](#purpose--scope)
- [Prerequisites / Setup Requirements](#prerequisites--setup-requirements)
- [Steps to Run Demo](#steps-to-run-demo)
- [How It Works](#how-it-works)
- [Configuration](#configuration)
- [Troubleshooting](#troubleshooting)
- [Resources](#resources)
- [Report Bugs & Get Support](#report-bugs--get-support)

## Purpose / Scope

`sl_diag` preserves diagnostic evidence in retained RAM so that it is still available after a reset. Crash Capture is the producer for CPU crashes: its HardFault handler captures the exception frame and the fault status registers, then hands them to `sl_diag`, which writes and commits a CRC-protected record of TLV blocks.

This example shows the complete crash path for a HardFault:

1. Boot: `sl_diag` validates its retained region and Crash Capture registers the CPU Crash provider. Both run from the `service_init` event, before `app_init()`.
2. If a sealed record from the previous boot exists, the app prints it, then clears it with `sl_diag_clear(SL_DIAG_CLEAR_RECORD)`.
3. Crash: press the configured board button. The app executes an undefined instruction. Crash Capture records the CPU state and fault status, seals the record and resets the device (`NVIC_SystemReset` in Crash Capture).
4. Post-reset: print again, clear, press the button for another cycle. `generation` advances on each new seal.

## Prerequisites / Setup Requirements

**Hardware**
- A supported board with button input and VCOM support. The button input is used to trigger the fault.

**Software**
- Simplicity Studio 5 (or later).
- A serial terminal connected to the kit VCOM port.

## Steps to Run Demo

1. Open the project in Simplicity Studio and build it.
2. Flash with `commander` (or Studio **without** leaving a debug session attached). Open only a serial terminal on VCOM.
3. On the first boot no record exists:

   ```text
   SL Diagnostic crash capture example
   No crash record.
   Press the board button to trigger a fault (Crash Capture seals + resets).
   After reset, compare generation with the previous print.
   ```

4. Press the board button. The device faults, Crash Capture seals the record, then soft-resets.
5. After the reset, the record from the previous boot is printed, then cleared:

   ```text
   A crash record from a previous boot was found:
   SL Diagnostic record
     state          : VALID
     generation     : 1
     ...
   CPU crash evidence
     PC   : 0x0000XXXX  <- faulting instruction
     ...
   Retained record raw dump (... bytes: ... byte header + ... TLV bytes)
     0000: ...
   Record cleared; the next button press will seal a new generation.
   Press the board button to trigger a fault ...
   ```

6. Press the board button again. After the next reset, `generation` should be higher than in step 5.

Look up the printed PC in the `.map` file or with `arm-none-eabi-addr2line -e <image>.out <PC>`.

## How It Works

The supported button input sets a pending flag from `sl_button_on_change()` (interrupt context). The super-loop flushes the log, then executes `__asm volatile ("udf #0" ::: "memory")` to trigger an undefined-instruction UsageFault. Since the example disables configurable fault handlers, it escalates to a HardFault. Crash Capture handles the HardFault, seals the CPU state into `.sl_diag_retained.noinit`, and resets the device. The app triggers the fault; it does not implement the crash handler or capture logic.

The record survives a software reset. A power cycle clears retained RAM.

The record is cleared only after the export returns success, `sl_diag_clear()` returns success, and `sl_diag_has_record()` is false. A failed read, a record that does not fit the export buffer, a logger level above INFO, or a busy clear leaves the retained record in place. Clearing after a successful export makes each button press create a new generation on every profile. Without that clear, Crash Basic keeps the first VALID record, and Crash Standard may only bump `repeat_count` for the same HardFault.

## Configuration

Default profile is **Crash Basic**. Change `SL_DIAG_PROFILE` (and other options) in the **SL Diagnostic** UC screen in Studio if you want Standard, Summary, or Full Template.

| Option | Default in this example | Effect |
| --- | --- | --- |
| `SL_DIAG_PROFILE` | `SL_DIAG_PROFILE_CRASH_BASIC` | Change in UC for Standard+ (adds `boot_id` / Trigger / Reset/Boot / Build metadata, and optionally Summary) |
| `SL_LOG_CONFIG_LEVEL_COMPILE_TIME` | Logger UC default (`ERROR`) | Set to INFO or DEBUG in the Logger UC screen to print the export. Above INFO, the record is kept |
| `SL_DIAG_SLOT_COUNT` | UC default | `2` enables dual-slot copy-on-commit |
| `SL_DIAG_STACK_SNAPSHOT_BYTES` | UC default | Size of the stack snapshot in the CPU Crash TLV (`0` disables it) |

`SL_DIAG_PROFILE_OFF` is not supported: Crash Capture requires an enabled profile.

## Troubleshooting

- **No VCOM output:** Confirm `SL_BOARD_ENABLE_VCOM` is 1, set logger level to at least INFO in the Logger UC screen, and that the terminal uses the correct port and baud rate. Keep the terminal open before pressing the board button.
- **The device resets but no record is printed:** Detach the debugger (or disable halt-on-HardFault). If the debugger catches the fault, Crash Capture never seals the retained record. Flash with `commander`, open only a serial terminal, then reset the kit.
- **The device does not reset after the HardFault message:** A debugger may halt the core on the fault. Run without a debug session, or resume the core.
- **No record after a power cycle:** The record is kept in retained RAM, which a power cycle clears. Soft reset (Crash Capture) keeps it; unplug clears it.
- **`generation` does not advance:** The example clears the record only after a successful export. If the export fails, or if you remove that clear, Crash Basic preserves the first record and Standard may only increment `repeat_count`.
- **`boot_id` missing from the print:** Present only when the profile includes incident metadata (Crash Standard or higher). Change `SL_DIAG_PROFILE` in the SL Diagnostic UC screen.

## Resources

- [Simplicity Studio 5 User's Guide](https://docs.silabs.com/simplicity-studio-5-users-guide/latest/)
- [Gecko Platform Documentation](https://docs.silabs.com/gecko-platform/latest/)
- [Silicon Labs Community](https://www.silabs.com/community)

## Report Bugs & Get Support

You are encouraged to report issues and get help from the community:

- [Silicon Labs Community](https://www.silabs.com/community)
