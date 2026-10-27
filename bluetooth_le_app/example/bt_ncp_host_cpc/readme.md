# Bluetooth NCP host over cpc-ng

This FreeRTOS example runs a Bluetooth NCP host as a CPC Primary node on an
EFR32. It communicates over cpc-ng on the EXP UART with a second EFR32 running
the `bt_ncp_cpc` CPC Secondary example.

## Getting Started

The NCP host controls the Bluetooth stack running on the NCP target through
BGAPI. The two examples exchange complete BGAPI messages over a cpc-ng endpoint
on the EXP UART.

For an introduction to Bluetooth NCP applications, see
[AN1259: Using the v3.x Silicon Labs Bluetooth Stack in Network Co-Processor Mode](https://www.silabs.com/documents/public/application-notes/an1259-bt-ncp-mode-sdk-v3x.pdf).

## Requirements

- Two compatible EFR32 development boards with EXP UART support
- The `bt_ncp_cpc` example running on the NCP target
- Three jumper wires for UART TX, UART RX, and ground
- An SWO-capable debugger for observing the host application log

## Configuring the Host and Target

1. Generate the `bt_ncp_cpc` project for the target board and this project for
   the host board.
2. Build and flash both projects.
3. Connect the boards as follows:

   | Host board | Target board |
   |------------|--------------|
   | EXP UART TX | EXP UART RX |
   | EXP UART RX | EXP UART TX |
   | GND | GND |

4. Power both boards and open the SWO output of the host board.

## Usage

The SWO output provides a timestamped, level-tagged application log. When the
CPC endpoint connects, the application sends BGAPI commands and starts finite
advertising together with low-duty-cycle passive scanning on the NCP target.

Scanning stops while a Bluetooth connection is open. Advertising and scanning
restart after the connection closes. The CPC link reconnects when either peer
restarts.

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
- If no application log appears, verify that the debugger is connected to the
  host board and that its SWO output is selected and the clocks are configured
  properly.

## Resources

- [Bluetooth Documentation](https://docs.silabs.com/bluetooth/latest/)
- [AN1259: Using the v3.x Silicon Labs Bluetooth Stack in Network Co-Processor Mode](https://www.silabs.com/documents/public/application-notes/an1259-bt-ncp-mode-sdk-v3x.pdf)
- [QSG169: Bluetooth SDK v3.x Quick Start Guide](https://www.silabs.com/documents/public/quick-start-guides/qsg169-bluetooth-sdk-v3x-quick-start-guide.pdf)

## Report Bugs & Get Support

You are always encouraged and welcome to report any issues you found to us via
[Silicon Labs Community](https://www.silabs.com/community).
