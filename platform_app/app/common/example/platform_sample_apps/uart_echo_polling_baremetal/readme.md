# UART Polling Echo Baremetal

Demonstrates a polling UART echo with optional RTS/CTS hardware flow control. The application repeatedly checks the receive FIFO and sends each received byte back to the sender. Use this example for simple, low-throughput transfers where the CPU can continuously poll the UART, or when transfers are very small and may fit in the UART peripheral's FIFO immediately.

## Table of Contents

- [Purpose / Scope](#purpose--scope)
- [Prerequisites / Setup Requirements](#prerequisites--setup-requirements)
- [Steps to Run Demo](#steps-to-run-demo)
- [Troubleshooting](#troubleshooting)
- [Modifying the UART Configuration](#modifying-the-uart-configuration)
- [Resources](#resources)
- [Report Bugs & Get Support](#report-bugs--get-support)

## Purpose / Scope

This example showcases the UART Driver component's polling API for simple, low-throughput transfers. It continuously checks the receive FIFO, reads one byte at a time, and immediately sends each byte back through the virtual COM (VCOM) port. CPU is used to transfer data to and from the UART's peripheral FIFO.

Related examples:

- `uart_echo_baremetal`: UART IRQ Echo Baremetal
- `uart_echo_async_baremetal`: UART Async Echo Baremetal
- `uart_echo_freertos`: UART IRQ Echo FreeRTOS

## Prerequisites / Setup Requirements

**Hardware**
- Silicon Labs kit with UART capabilities and VCOM.
- USB cable or Ethernet connection.

**Software**
- Simplicity Studio 5 (or later).

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
