# LED Boost DCDC Bare-metal

This example demonstrates LEDVDD voltage ramp from 1.8 V to 3.8 V on the DCDC boost regulator. Ramp completion is signalled by the DCDC interrupt. After the 3.8 V ramp completes, the DCDC is powered off so LEDVDD falls back to the board's default voltage (~3.2 V).

## Table of Contents

- [Purpose / Scope](#purpose--scope)
- [Prerequisites / Setup Requirements](#prerequisites--setup-requirements)
- [Steps to Run Demo](#steps-to-run-demo)
- [Troubleshooting](#troubleshooting)
- [Resources](#resources)
- [Report Bugs & Get Support](#report-bugs--get-support)

## Purpose / Scope

This example shows how to bring up the DCDC boost regulator and run a LEDVDD ramp sequence in a bare-metal application. After initialising the regulator, LEDVDD is held at 1.8 V until it reaches regulation, then commanded to 3.8 V. Once the 3.8 V ramp completes, the application powers off the DCDC so LEDVDD collapses to the board's default voltage (~3.2 V).

## Prerequisites / Setup Requirements

**Hardware**
- Silicon Labs kit with DCDC LEDVDD support.
- USB cable for flashing and powering the board.

**Software**
- Simplicity Studio 5 (or later).
- A debugger to observe the `ramp_done` flag, or a multimeter / oscilloscope on the LEDVDD pin to observe the voltage transition from 1.8 V to 3.8 V back to ~3.2 V.

## Steps to Run Demo

1. Open the project in Simplicity Studio and build it.
2. Connect the kit via USB.
3. Flash and run the application on the board.
4. Observe the LEDVDD output transition `1.8 V -> 3.8 V -> ~3.2 V (default)` on a scope or meter, or set a breakpoint inside `ledboost_process_action()` to confirm the DCDC ramp-done interrupt fires.

## Troubleshooting

- **Ramp interrupt never fires:** Confirm the part has DCDC LEDVDD support.
- **LEDVDD stays at 1.8 V:** Verify `sl_hal_emu_dcdc_get_ledvddon()` returns true after init; if it never does, the DCDC bus clock or DCDC enable may not have been applied.
- **Build error on `DCDC_IF_LEDVDDRAMPDONE`:** The selected part does not expose the LEDVDD ramp-done flag; this example targets boost parts that do.

## Resources

- [EMU HAL Documentation](https://docs.silabs.com/gecko-platform/latest/platform-peripheral/emu)
- [Simplicity Studio 5 User's Guide](https://docs.silabs.com/simplicity-studio-5-users-guide/latest/)
- [Silicon Labs Community](https://www.silabs.com/community)

## Report Bugs & Get Support

You are encouraged to report issues and get help from the community:

- [Silicon Labs Community](https://www.silabs.com/community)
