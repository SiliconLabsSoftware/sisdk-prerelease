# RAIL - SoC Throughput

## Introduction

The Throughput Measurement example shows how PHY configuration and MAC-level
features affect radio throughput. It is a two-node measurement: one board is
the transmitter and the other is the receiver for the measurement interval.
The same image runs on both boards. The application assigns the roles, keeps
the nodes synchronized, and prints the results on the transmitter.

Use it to compare radio configurations, to see how IEEE 802.15.4 framing, ACK,
and CSMA/CA change the result, and as a reference for a repeatable throughput
measurement. PHY settings come from the Radio Configurator. Measurement
parameters and MAC options are set in `config/app_config.h` or from the
transmitter CLI.

## Getting Started

Create a radio configuration with the Radio Configurator. The application uses
the first channel configuration entry. Generate the project, then flash the
same image to both boards.

Optional compile-time settings are in `config/app_config.h`:

- `THROUGHPUT_MEASUREMENT_PERIOD_SEC` is the measurement window in seconds
  (default `5`, maximum `30`). Increase it for a low-data-rate PHY with a long
  frame so the result is based on more packets so is more precise.
- `THROUGHPUT_IEEE802154_DEFAULT_FRAME_LENGTH` is the IEEE 802.15.4 frame
  length in bytes (default `16`) used when `ieee802154_configure` does not set
  one.
- `THROUGHPUT_IEEE802154_RX_TO_TX_DELAY_US` is the RX-to-TX turnaround for
  IEEE 802.15.4 auto-ACK (default `192`). TX-to-RX is this value minus 10 µs.
- `THROUGHPUT_DEBUG_LOGS_ENABLED` prints state-machine transitions when set
  to `1`.

After reset, each board prints the application name and the valid channel
ranges for the selected PHY, then waits for CLI commands and listens for a
peer:

```
Throughput Measurement App
Valid channel range: 0-20
```

Open a serial console on the board that you want to use as the transmitter. 
Issue `help` to list commands. All CLI interaction is done on that board. 
The peer stays in receive mode and follows the transmitter.

1. (Optional) Enable the IEEE 802.15.4 MAC frame format with
   `ieee802154_configure`. This is rejected on a fixed-length proprietary PHY.
   See [CLI Commands](#cli-commands).
2. (Optional) Enable CSMA/CA with `csma_configure`.
3. (Optional) Set PHY-specific options (rate mode, FCS length, whitening, and
   similar) with the Packet Assistant commands. Issue `packet_assistant` to
   list them. A setting takes effect only when the matching PHY is selected.
4. Start the measurement with `start_measurement` and the radio channel to
   use.
5. When the measurement finishes, the transmitter prints the results. Both
   boards then reset, so any CLI configuration has to be entered again before
   the next run.

Example:

```
start_measurement 0
> Measurement is in progress...
> Measurement done
> ********************************************
> Packets sent: 1000
> Packets received: 900
> Frame errors: 0
> Packet delivery rate: 90.00%
>
> TX goodput: 25.6 kbits/s
> RX goodput: 23.0 kbits/s
> Over-the-air loss: 2.6 kbits/s
> Protocol overhead: 0.00%
> ********************************************
```

The figures above match a 16-byte frame, a 5 s window, and no PHY or MAC
header. `ACK retries` is printed when IEEE 802.15.4 ACK is enabled. `CSMA retries` is
printed when CSMA/CA is enabled. Goodput counts payload bytes delivered during
`THROUGHPUT_MEASUREMENT_PERIOD_SEC`, excluding the IEEE 802.15.4 MAC header
when that framing is enabled. Protocol overhead is the PHY header plus that
MAC header, as a percentage of the frame.

### Setting the frame length

Fixed-length proprietary PHYs take the frame length from the Radio
Configurator. The application reads that length from RAIL, so no other length
setting is required.

IEEE 802.15.4-compatible variable-length PHYs have two places to set the
length:

- `THROUGHPUT_IEEE802154_DEFAULT_FRAME_LENGTH` in `config/app_config.h` is
  used when `ieee802154_configure` has not been issued. Change this macro to
  set the frame length without enabling the IEEE 802.15.4 MAC frame, address
  filtering, or ACK.
- The `frame_length` argument of `ieee802154_configure` overrides that default
  for the next measurement. That command also enables the IEEE 802.15.4 frame,
  address filtering, and optional ACK.

## CLI Commands

Runtime measurement and MAC-level options are controlled through the CLI.
Connect to the device over VCOM and use the commands below. Issue `help` to
list all available commands and their arguments.

Unsigned integer arguments accept decimal values or hexadecimal values with a
`0x` prefix (for example `0xBEEF`).

### `ieee802154_configure`

Configures IEEE 802.15.4 addressing, frame format, and ACK behavior. The
command is available for IEEE 802.15.4-compatible PHYs. On a fixed-length
proprietary PHY it prints an error and leaves the previous settings unchanged.

```
ieee802154_configure <pan_id> <rx_address> <tx_address> <frame_length> <source_address> <pan_id_compression> <ack_timeout_us>
```

| Argument | Type | Description |
|----------|------|-------------|
| `pan_id` | uint16 | PAN ID shared by both nodes (e.g. `0xBEEF`) |
| `rx_address` | uint16 | Short address of the receiver node (e.g. `0x0001`) |
| `tx_address` | uint16 | Short address of the transmitter node (e.g. `0x0002`) |
| `frame_length` | uint16 | Frame length |
| `source_address` | uint8 | Source address present: `0` = false, `1` = true |
| `pan_id_compression` | uint8 | PAN ID compression: `0` = false, `1` = true |
| `ack_timeout_us` | uint16 | ACK timeout in microseconds. Set to `0` to disable ACK |

Both nodes must use the same `pan_id`, `rx_address`, and `tx_address`. The
local short address is taken from the node's role (`rx_address` on the
receiver, `tx_address` on the transmitter). The destination address is always
present. Source addressing and PAN ID compression are optional: PAN ID
compression omits the source PAN ID when source addressing is enabled.

The frame length must leave room for the MAC header of the selected addressing
mode and for a CRC of up to 4 bytes. The command prints the accepted range
when the value is outside it.

Example with ACK enabled (30 ms timeout):

```
ieee802154_configure 0xBEEF 0x0001 0x0002 64 1 0 30000
```

Example with ACK disabled:

```
ieee802154_configure 0xBEEF 0x0001 0x0002 64 1 0 0
```

### `csma_configure`

Configures CSMA/CA parameters used during transmission.

```
csma_configure <csma_min_bo_exp> <csma_max_bo_exp> <csma_tries> <cca_threshold_dbm> <cca_backoff_us> <cca_duration_us> <csma_timeout_us>
```

| Argument | Type | Description |
|----------|------|-------------|
| `csma_min_bo_exp` | uint8 | Minimum backoff exponent (`0` … `SL_RAIL_MAX_CSMA_EXPONENT`). If `0`, `csma_max_bo_exp` must also be `0` (fixed backoff) |
| `csma_max_bo_exp` | uint8 | Maximum backoff exponent (`0` … `SL_RAIL_MAX_CSMA_EXPONENT`); must be ≥ `csma_min_bo_exp` |
| `csma_tries` | uint8 | Number of backoff-then-CCA iterations (`0` … `SL_RAIL_MAX_LBT_TRIES`). `0` means TX with no CSMA |
| `cca_threshold_dbm` | int8 | CCA RSSI threshold in dBm |
| `cca_backoff_us` | uint16 | Backoff unit period in microseconds |
| `cca_duration_us` | uint16 | Minimum CCA check duration in microseconds; must be non-zero |
| `csma_timeout_us` | uint32 | Overall CSMA timeout in microseconds. Set to `0` for no timeout |

Platform-specific limits for random backoff (for example truncation of large
`cca_backoff_us` values on Series 2 / Series 3) are defined by RAIL and may
change. Check
[`sl_rail_csma_config_t`](https://docs.silabs.com/rail/latest/rail-api/sl-rail-csma-config-t)
and
[`sl_rail_start_cca_csma_tx()`](https://docs.silabs.com/rail/latest/rail-api/packet-tx#sl-rail-start-cca-csma-tx)
before choosing parameters.

Example:

```
csma_configure 3 5 5 -75 320 128 0
> MinBo:3,MaxBo:5,Tries:5,Threshold:-75,Backoff:320,Duration:128,Timeout:0
```

### `packet_assistant`

PHY-specific packet options are provided by the Packet Assistant component.
Issue `packet_assistant` to list the commands available on the
current device. Typical options are FCS length, whitening, SUN OFDM rate and
scrambler, and SUN OQPSK rate and spreading mode. A setting is applied only
when the corresponding PHY is selected. Run a packet_assistant command with 
`packet_assistant <packet_assistant command>`. 

### `start_measurement`

Starts a throughput measurement on the given radio channel. The board that
issues the command becomes the transmitter. The peer becomes the receiver.
The channel must be inside a range printed at startup.

```
start_measurement <channel>
```

| Argument | Type | Description |
|----------|------|-------------|
| `channel` | uint16 | Radio channel used for the measurement |

## Notes

> The application can run on an RTOS. Add RTOS support in the Software
> Components window. Micrium OS and FreeRTOS are supported.

> Both boards stay in receive mode between measurements. Because of the higher
> current consumption of continuous radio use, power the boards from USB or a
> USB power bank when they are not on a mainboard supply.

> Each measurement ends with a reset. Enter `ieee802154_configure`,
> `csma_configure`, and any Packet Assistant settings again before the next
> `start_measurement`.

## Report Bugs & Get Support

You are always encouraged and welcome to report any issues you found to us via
[Silicon Labs
Community](https://community.silabs.com/s/topic/0TO1M000000qHaKWAU/proprietary?language=en_US).
