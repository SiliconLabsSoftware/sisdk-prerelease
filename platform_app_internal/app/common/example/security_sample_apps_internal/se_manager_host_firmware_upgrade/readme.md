# Platform Security - SoC SE Manager Host Firmware Upgrade and Debug Lock

Demonstrates how to validate and apply an embedded host firmware image and enable debug lock with the SE Manager host-upgrade and debug APIs on Secure Vault devices.

## Table of Contents

- [Purpose / Scope](#purpose--scope)
- [Prerequisites / Setup Requirements](#prerequisites--setup-requirements)
- [Steps to Run Demo](#steps-to-run-demo)
- [Troubleshooting](#troubleshooting)
- [Resources](#resources)
- [Report Bugs & Get Support](#report-bugs--get-support)

## Purpose / Scope

This example demonstrates **host firmware upgrade** and **debug lock** using SE Manager utility APIs. The host firmware is stored as a C array (`host_firmware_image[]` in `app_host_firmware_image.c`) linked into the example project.
**Important:** These operations are **destructive or security-sensitive**:
- A **successful host firmware upgrade** replaces the running host application with the embedded image (the stock image is a small **VCOM echo** demo).
- **Debug lock** permanently restricts debug access when applied.
Output is on the kit **VCOM** port, with **clock-cycle counts** when `SE_MANAGER_PRINT=1` (default).

### Interactive menu

- **SPACE** — toggle between **HOST FIRMWARE UPGRADE** and **DEBUG LOCK**
- **ENTER (CR)** — run the selected operation

### Host firmware upgrade path

1. Report image start address and size
2. `sl_se_get_upgrade_status_host_image` — last upgrade status and version
3. `sl_se_check_host_image` — validate the embedded image
4. **ENTER** to `sl_se_apply_host_image` (or **SPACE** to cancel on HSE)
On **VSE** devices (`CRYPTOACC_PRESENT`), several steps issue a **device reset**; results are retrieved after reset via `sl_se_read_executed_command` and `sl_se_ack_command`.

### Debug lock path

1. On **VSE:** read SE firmware version (debug lock via application requires **v1.2.5+**)
2. `sl_se_get_debug_lock_status`
3. If disabled, **ENTER** to `sl_se_apply_debug_lock` (or **SPACE** to cancel)

### Replacing the host firmware image

Convert a `.bin` to a C array with [SEGGER Bin2C](https://www.segger.com/free-utilities/bin2c/). Copy the data into `host_firmware_image[]` in `app_host_firmware_image.c` (omit a trailing `NULL` / `0x00` byte if Bin2C adds one). Rebuild the example.
**Components used:** `se_manager`, `sl_main`, `device_init`, `clock_manager`, VCOM stdio retargeting.

### SE Manager APIs exercised

`sl_se_init`, `sl_se_deinit`, `sl_se_init_command_context`, `sl_se_deinit_command_context`, `sl_se_get_upgrade_status_host_image`, `sl_se_check_host_image`, `sl_se_apply_host_image`, `sl_se_get_se_version`, `sl_se_get_debug_lock_status`, `sl_se_apply_debug_lock`, and on VSE: `sl_se_read_executed_command`, `sl_se_ack_command`.

## Prerequisites / Setup Requirements

### Hardware Requirements

- A **Secure Vault** development kit supported by this example (device-specific firmware image data in `app_host_firmware_image.c`).
- **AEM** power selected when programming (see image below).
- USB for programming and VCOM.

### Software Requirements

- **Simplicity Studio 5** (current SDK).
- Serial terminal on **VCOM**: **115200** 8-N-1, line terminator **None** (Device Console).
- Latest **adapter** and **SE firmware** ([General Device Information](https://docs.silabs.com/simplicity-studio-5-users-guide/latest/ss-5-users-guide-about-the-launcher/welcome-and-device-tabs#general-device-information)).

### Critical precautions

- **Disconnect the debugger** before host firmware upgrade or debug lock (as noted in the original example documentation).
- Use a **development** or recoverable board; do not run upgrade/debug lock on production units without a recovery plan.
- After a successful **host firmware upgrade**, this example no longer runs — you will be executing the upgraded host image instead.

## Steps to Run Demo

1. Flash this example once with the debugger connected for initial programming.
2. Open VCOM (115200 8-N-1, line terminator **None**).
3. **Disconnect the debugger** before proceeding with upgrade or debug lock.
4. Reset the board and watch the serial menu.
5. Press **SPACE** to choose **HOST FIRMWARE UPGRADE** or **DEBUG LOCK**; press **ENTER** to start.

### Host firmware upgrade

1. Note the printed image **start address** and **size**.
2. Follow status → validate prompts (on VSE, allow **reset** between steps and read responses from the log).
3. When prompted, press **ENTER** to apply the image, or **SPACE** to exit without upgrading.
4. If upgrade succeeds, the device runs the new host firmware (VCOM echo demo in the stock image).

### Debug lock

1. On VSE, confirm SE version supports application-initiated debug lock.
2. Read lock status; if disabled, press **ENTER** to apply (irreversible) or **SPACE** to return to the menu.

### Optional: disable timing prints

Define **`SE_MANAGER_PRINT=0`** in preprocessor symbols.

## Troubleshooting

| Symptom | What to check |
|--------|----------------|
| No serial output | VCOM settings; `SL_BOARD_ENABLE_VCOM=1` |
| SE manager init failed | Latest SE firmware |
| Unexpected reset during flow | **Normal on VSE** for upgrade/lock APIs — read post-reset response lines |
| Validate image failed | `host_firmware_image[]` matches target device/part; Bin2C data correct |
| Debug lock not available on VSE | SE firmware **v1.2.5+** required |
| Debugger attach fails after lock | Expected if debug lock was applied |
| After upgrade, example “gone” | Expected — host now runs upgraded firmware; reflash example project to repeat |
| Programming fails | AEM switch position |

## Resources

- [SE Manager API documentation](https://docs.silabs.com/gecko-platform/latest/service/api/group-sl-se-manager)
- [SEGGER Bin2C utility](https://www.segger.com/free-utilities/bin2c/)
- [Simplicity Studio 5 User's Guide — Examples](https://docs.silabs.com/simplicity-studio-5-users-guide/latest/ss-5-users-guide-getting-started/start-a-project#examples)

## Report Bugs & Get Support

You are always encouraged and welcome to report any issues you found to us via [Silicon Labs Community](https://www.silabs.com/community).
