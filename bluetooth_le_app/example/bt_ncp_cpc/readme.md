# Bluetooth NCP over cpc-ng

This FreeRTOS example runs a Bluetooth NCP as a CPC Secondary node on an EFR32.
It communicates over cpc-ng on the EXP UART with a second EFR32 running the
`bt_ncp_host_cpc` CPC Primary example. The `bluetooth_bgapi_link` component
provides the NCP transport and carries one complete BGAPI command, response or
event per endpoint frame on EXP UART.

## Getting Started

The Bluetooth stack runs on this NCP target and is controlled by the NCP host
through BGAPI. The `bluetooth_bgapi_link` component provides the NCP transport
and carries one complete BGAPI message per cpc-ng endpoint frame on the EXP
UART.

For an introduction to Bluetooth NCP applications, see
[AN1259: Using the v3.x Silicon Labs Bluetooth Stack in Network Co-Processor Mode](https://www.silabs.com/documents/public/application-notes/an1259-bt-ncp-mode-sdk-v3x.pdf).

## Requirements

- Two compatible EFR32 development boards with EXP UART support
- The `bt_ncp_host_cpc` example running on the NCP host
- Three jumper wires for UART TX, UART RX, and ground

## Configuring the Host and Target

1. Generate this project for the target board and the `bt_ncp_host_cpc` project
   for the host board.
2. Build and flash both projects.
3. Connect the boards as follows:

   | Target board | Host board |
   |--------------|------------|
   | EXP UART TX | EXP UART RX |
   | EXP UART RX | EXP UART TX |
   | GND | GND |

4. Power both boards and open the SWO output of the host board.

## Usage

The NCP target exchanges BGAPI messages with the host over the cpc-ng endpoint.
Application activity is reported through the timestamped, level-tagged SWO log
on the host board.

### Reset recovery
Reset recovery depends on which peer restarts:

- If the CPC Primary host restarts, the Secondary reopens the endpoint while
  the NCP target remains running. After reconnecting, the host stops and starts
  the Bluetooth stack as part of its synchronization sequence, then starts
  advertising and scanning again. Any active Bluetooth connections are closed
  and are not re-established automatically.
- If the CPC Secondary NCP target restarts, its Bluetooth stack restarts with
  it. The host reconnects to the endpoint and runs the same synchronization
  sequence. Advertising and scanning start again, but any previous Bluetooth
  connections remain lost.

### Optional runtime error notification

This example leaves the message-link runtime error callback unset. An
application can register its own callback during initialization:

```c
static void on_link_runtime_error(sl_bt_link_runtime_error_t error,
                                  sl_status_t status,
                                  void *user)
{
  // Record the failure or defer application-specific handling.
}

void app_init(void)
{
  sl_status_t status = sl_bt_link_runtime_set_error_callback(
    sl_bt_bgapi_link_get_runtime(),
    on_link_runtime_error,
    NULL);

  // Handle callback registration failure as required by the application.
}
```

The application must check the registration result and decide how to handle a
reported failure. The callback runs in the runtime error-reporting context and
must not block or call link APIs. Without a callback, the failure remains
available through `sl_bt_link_runtime_get_status()` and subsequent runtime or
link operations.

## Troubleshooting

- Verify that TX and RX are connected crosswise and that the board grounds are
  connected.
- Verify that the corresponding CPC Primary and Secondary examples are running
  on the host and target boards.
- Use the host SWO log to check the CPC endpoint state and BGAPI activity.

## Resources

- [Bluetooth Documentation](https://docs.silabs.com/bluetooth/latest/)
- [AN1259: Using the v3.x Silicon Labs Bluetooth Stack in Network Co-Processor Mode](https://www.silabs.com/documents/public/application-notes/an1259-bt-ncp-mode-sdk-v3x.pdf)
- [QSG169: Bluetooth SDK v3.x Quick Start Guide](https://www.silabs.com/documents/public/quick-start-guides/qsg169-bluetooth-sdk-v3x-quick-start-guide.pdf)

## Report Bugs & Get Support

You are always encouraged and welcome to report any issues you found to us via
[Silicon Labs Community](https://www.silabs.com/community).
