# UART Async Echo Baremetal

Demonstrates DMA-accelerated asynchronous UART echo using two queued receive buffers with optional RTS/CTS hardware flow control. The application echoes data when a receive buffer fills or when no UART data arrives for 100 ms. After a full buffer has been echoed, it is queued again for reception so the application can continue receiving data.

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

This example showcases the UART Driver component's asynchronous DMA API for receiving data into two queued buffers while previously received data is echoed. When DMA fills one buffer, reception advances to the other queued buffer while the completed buffer is transmitted. A buffer is requeued only after all TX transfers that reference it have completed, so DMA does not overwrite data still being transmitted.

If no data arrives for 100 ms, the driver reports an RX timeout and the bytes received so far are echoed. The active receive buffer remains available for additional data after a timeout; later timeout events can echo more bytes from the same buffer until it fills. RTS/CTS hardware flow control provides back-pressure when both receive buffers are occupied.

Related examples:

- `uart_echo_polling_baremetal`: UART Polling Echo Baremetal
- `uart_echo_baremetal`: UART IRQ Echo Baremetal
- `uart_echo_freertos`: UART IRQ Echo FreeRTOS

## Prerequisites / Setup Requirements

**Hardware**
- Silicon Labs kit with UART capabilities and VCOM.
- USB cable or Ethernet connection.

**Software**
- Simplicity Studio 5 (or later).

## Optional Hardware Flow Control

This example queues two DMA receive buffers. After DMA fills one buffer, it uses the other while the completed buffer is being echoed. The completed buffer cannot be reused until its final TX transfer finishes. When both buffers are occupied, no receive buffer is immediately available for additional data.

With RTS/CTS enabled, the UART peripheral requests that the sender pause until a receive buffer is available again, preventing data loss.

Without hardware flow control, increasing `ECHO_BUFFER_SIZE` increases the amount of data each receive buffer can hold. This gives the application more time to process and echo the previous buffer before both receive buffers fill, but does not guarantee that data will not be dropped.

## Steps to Run Demo

1. Open the project in Simplicity Studio and build it.
2. Connect the kit via USB or Ethernet.
3. In Simplicity Studio, open **Tools** and start **Commander**.
4. Select the connected kit and open the **VCOM** section.
5. Select **Apply VCOM configuration on connect**, then set the VCOM baud rate and flow-control option to match the UART Driver instance configuration.
6. Press **Connect**.
7. Flash the project to your board.
8. The banner appears in the VCOM console. Messages entered in the console are echoed after the 100 ms RX idle timeout, or immediately when a receive buffer fills.

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
