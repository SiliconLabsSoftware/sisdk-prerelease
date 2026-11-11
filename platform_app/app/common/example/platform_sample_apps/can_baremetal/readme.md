# CAN Bare-metal

This example shows how to use the on-chip CAN controller with `sl_hal_can` in a bare-metal application, without an RTOS. It sends and receives frames inside the chip (internal loopback), so you do not need a CAN transceiver or a second node to see it work. Results are printed on the kit's virtual COM port (VCOM).

## Table of Contents

- [Purpose / Scope](#purpose--scope)
- [What the Example Does](#what-the-example-does)
- [Console Output](#console-output)
- [Talking to a Real CAN Bus](#talking-to-a-real-can-bus)
- [Prerequisites / Setup Requirements](#prerequisites--setup-requirements)
- [Steps to Run Demo](#steps-to-run-demo)
- [Troubleshooting](#troubleshooting)
- [Resources](#resources)
- [Report Bugs & Get Support](#report-bugs--get-support)

## Purpose / Scope

A sleeptimer (default 500 ms) starts one pass of the demo. That pass runs three CAN modes, one after another, on CAN0:

| Pass | Mode | Payload | Data rate |
|------|------|---------|-----------|
| 1 | Classic CAN | 8 bytes | 500 kbit/s |
| 2 | CAN FD, same bit rate for data | 64 bytes | 500 kbit/s |
| 3 | CAN FD with bit-rate switch | 64 bytes | 500 kbit/s arbitration, 4 Mbit/s data |

In each pass the application:

- Accepts standard IDs into Rx FIFO 0, extended IDs into Rx FIFO 1, and one exact standard ID into a dedicated Rx buffer.
- Sends a standard-ID frame through the Tx FIFO and checks it on Rx FIFO 0.
- Sends an extended-ID frame through the Tx FIFO and checks it on Rx FIFO 1.
- Sends one frame through a dedicated Tx buffer and checks it in the dedicated Rx buffer.
- Reads the Tx event FIFO, requests a transmit abort, and reads protocol status.

The default operating mode is internal loopback. The controller transmits a frame and receives that same frame. Nothing is driven onto the board pins.

## What the Example Does

Message IDs used in `can_app.c`:

| Frame | ID | Path |
|-------|----|------|
| Standard | `0x123` | Tx FIFO to Rx FIFO 0 |
| Extended | `0x12345678` | Tx FIFO to Rx FIFO 1 |
| Dedicated buffer | `0x111` | Tx buffer 0 to Rx buffer 0 |

CAN0 pins are PA9 (TX) and PA10 (RX). They are configured even in loopback so the same project can be switched to a real bus.

## Console Output

Open VCOM and you should see one block per mode. A successful pass looks like this:

```text
----> CAN mode running: Classic CAN (no FD, no BRS) <----

  >>>>> Std-ID frame transffer and receive completed via tx fifo and rx fifo0
  >>>>> Ext-ID frame transffer and receive completed via tx and rx fifo1
  >>>>> Transffer and receive completed via tx and rx buffer

----> CAN mode running: Classic CAN (no FD, no BRS) completed <----
```

The same block is printed again for `CAN-FD without BRS` and `CAN-FD with BRS`.

A problem looks like one of these lines, and the application then stops on an assert:

```text
CAN timeout waiting for Tx complete
CAN payload mismatch (Std-ID FIFO0): index 0 expected 0x01 got 0x00
Invalid CAN bit timing: bitrate=500000 prescaler=1 ntq=0
```

## Talking to a Real CAN Bus

Loopback is the default because it needs no extra hardware. To talk to another CAN node:

1. In `can_app.c`, change `cfg.mode` from `SL_HAL_CAN_OPERATION_MODE_INTERNAL_LOOPBACK` to `SL_HAL_CAN_OPERATION_MODE_NORMAL`.
2. Connect PA9 and PA10 to a CAN transceiver (CANH / CANL), and give both nodes a common ground.
3. Use the same bit rate on the other node: 500 kbit/s. The third pass also uses 4 Mbit/s for the CAN FD data phase.
4. The other node must accept the IDs above, or change the IDs and filters in `can_app.c` to match that node.

Frames sent in normal mode leave the chip. This example still expects to receive its own frames, so the other node must send those frames back, or you must change the receive checks.

## Prerequisites / Setup Requirements

**Hardware**

- A Silicon Labs kit whose device has CAN0. The default loopback demo needs no transceiver and no second board.
- A CAN transceiver and a second node only if you switch to normal mode.

**Software**

- Simplicity Studio 5 (or later).
- A terminal application to connect to VCOM.

## Steps to Run Demo

1. Open the project in Simplicity Studio, select a part that has CAN, and build.
2. Connect the kit via USB and flash the application.
3. Open the Studio Console or a serial terminal and select the kit's VCOM port.
4. Run the application. After about 500 ms the terminal prints the three mode blocks shown in [Console Output](#console-output).
5. To change the wait before the demo starts, edit `SAMPLE_TIMEOUT_MS` in `can_app.c`, then rebuild and flash.

## Troubleshooting

- **No text on the terminal:** Confirm the VCOM port, 115200 8N1, and RTS/CTS. The first line is printed only after the sleeptimer period (default 500 ms).
- **`CAN timeout waiting for ...`:** In loopback this usually means the CAN clock or the interrupt did not start. Rebuild for the connected part and reset the kit. In normal mode it also means the expected frame was not received.
- **`CAN payload mismatch`:** The received bytes do not match the bytes that were sent. In normal mode, check that the other node is echoing the same payload.
- **`Invalid CAN bit timing`:** The CAN clock is not fast enough for the selected bit rate. The nominal rate is 500 kbit/s. The CAN FD data rate in the third pass is 4 Mbit/s.
- **Nothing on a real bus:** The project stays in internal loopback until `cfg.mode` is set to `SL_HAL_CAN_OPERATION_MODE_NORMAL`.

## Resources

- [Simplicity Studio 5 User's Guide](https://docs.silabs.com/simplicity-studio-5-users-guide/latest/)
- [Gecko Platform Documentation](https://docs.silabs.com/gecko-platform/latest/)
- [Silicon Labs Community](https://www.silabs.com/community)

## Report Bugs & Get Support

You are encouraged to report issues and get help from the community:

- [Silicon Labs Community](https://www.silabs.com/community)
