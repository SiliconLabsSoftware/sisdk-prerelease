# UART IRQ Echo FreeRTOS

Demonstrates interrupt-driven UART echo using FreeRTOS with optional RTS/CTS hardware flow control. UART callbacks notify an echo task when data is ready and when its echo transmission finishes; the task echoes the data while reception continues. Use this example when a FreeRTOS task should process UART events instead of performing all work in interrupt context.

## Table of Contents

- [Purpose / Scope](#purpose--scope)
- [Prerequisites / Setup Requirements](#prerequisites--setup-requirements)
- [Optional Hardware Flow Control](#optional-hardware-flow-control)
- [Steps to Run Demo](#steps-to-run-demo)
- [Troubleshooting](#troubleshooting)
- [Modifying the UART Configuration](#modifying-the-uart-configuration)
- [Resources](#resources)
- [Report Bugs & Get Support](#report-bugs--get-support)

## Purpose / Scope

This example showcases the UART Driver component's interrupt callbacks with FreeRTOS and separates IRQ notification from application processing. The UART callbacks run in interrupt context and signal the echo task with semaphores. The task drains and copies the FIFO data, re-enables RX notification, transmits the echo, and waits for TX completion before processing the next received data. This keeps the callbacks short while the task sequences the UART operations. RTS/CTS hardware flow control provides back-pressure if the FIFO fills.

Because RX-ready notification is level-triggered, the task keeps it disabled until it has copied the FIFO data and is ready to receive another notification.

Related examples:

- `uart_echo_polling_baremetal`: UART Polling Echo Baremetal
- `uart_echo_baremetal`: UART IRQ Echo Baremetal
- `uart_echo_async_baremetal`: UART Async Echo Baremetal

## Prerequisites / Setup Requirements

**Hardware**
- Silicon Labs kit with UART capabilities and VCOM.
- USB cable or Ethernet connection.

**Software**
- Simplicity Studio 5 (or later).

## Optional Hardware Flow Control

While this example is echoing received data, it temporarily stops servicing the RX FIFO. Without flow control, incoming data can overflow the RX FIFO and be lost.

With RTS/CTS enabled, the UART peripheral requests that the sender pause when the RX FIFO cannot accept more data. The sender resumes when space is available, preventing overflow.

## Steps to Run Demo

1. Open the project in Simplicity Studio and build it.
2. Connect the kit via USB or Ethernet.
3. In Simplicity Studio, open **Tools** and start **Commander**.
4. Select the connected kit and open the **VCOM** section.
5. Select **Apply VCOM configuration on connect**, then set the VCOM baud rate and flow-control option to match the UART Driver instance configuration.
6. Press **Connect**.
7. Flash the project to your board.
8. The banner appears in the VCOM console. Messages entered in the console are echoed back.

## Troubleshooting

- **No VCOM port:** Ensure the kit is connected and the VCOM driver is installed.
- **No banner or echo or garbled communication:** Confirm the VCOM configuration matches the UART Driver instance configuration, then reconnect and flash the kit again.

## Modifying the UART Configuration

The project uses an auto-generated UART configuration that routes the UART instance to VCOM. No changes are required to run this example. To modify the UART configuration, open the **Software Components** view in Simplicity Studio and find **UART Driver** under **Platform**. Select the VCOM instance and click **Configure**. From the configuration page, you can change the UART pin assignments, baud rate, parity, stop bits, data bits, oversampling, and RTS/CTS flow-control settings.

## Resources

- [Simplicity Studio 5 User's Guide](https://docs.silabs.com/simplicity-studio-5-users-guide/latest/)
- [UART Driver Documentation](https://docs.silabs.com/gecko-platform/latest/platform-driver/uart)
- [Silicon Labs Community](https://www.silabs.com/community)

## Report Bugs & Get Support

You are encouraged to report issues and get help from the community:

- [Silicon Labs Community](https://www.silabs.com/community)
