# Throughput test

This host example demonstrates how to measure Bluetooth LE throughput between the NCP target *EFR* device and a remote *EFR* device.
It is designed to help evaluate data transfer rates and optimize Bluetooth LE communication.

## Features

- Supports bidirectional throughput measurement
- Adjustable packet size, connection interval, and PHY type
- Selectable test modes: notification or indication
- Configurable MTU and data length
- Real-time throughput and statistics display
- Flexible connection options: TCP/IP, UART, or AF socket
- Customizable logging and configuration file support


## Limitations, known issues

The host application supports only the central role.


## Requirements

- An EFR MCU flashed with Bluetooth NCP example with bluetooth_feature_power_control feature enabled
- Host platform running Linux, Windows, or macOS
- Compatible Bluetooth LE device running Bluetooth - SoC Throughput example
- libcjson host library with headers installed by the user


## Configuration


### Command Line Arguments

The program supports the following command line options for flexible configuration:

- `-t <tcp_address>`: Use TCP/IP connection. Specify the dev board's TCP/IP address.
- `-u <serial_port>`: Use UART serial connection. Specify the serial port (e.g., COM# on Windows, /dev/tty# on POSIX).
- `-n <server_socket>`: Use AF socket connection. Specify the path to the AF socket file descriptor.
- `-b <baud_rate>`: Set UART baud rate (default: 115200).
- `-f`: Disable flow control (RTS/CTS). By default, flow control is enabled.
- `-l <level>`: Set application log level filter (default: 4).
    - 0: Critical
    - 1: Critical, error
    - 2: Critical, error, warning
    - 3: Critical, error, warning, info
    - 4: Critical, error, warning, info, debug
- `-T <time>`: Fixed time mode. Specify measurement duration in seconds.
- `-D <data_length>`: Fixed data length mode. Specify measurement data length in bytes.
- `-p <phy>`: Select PHY type:
    - 1: 1M (default)
    - 2: 2M
    - 4: Coded, 125k
    - 8: Coded, 500k
- `-i <interval>`: Set connection interval (in 1.25 ms steps).
- `-M <mtu>`: Set Maximum Transmission Unit (MTU) size in bytes.
- `-N <type>`: Select test type:
    - 1: Notification
    - 2: Indication
- `-c <config>`: Specify path to configuration file.
- `-o <log>`: Specify path to log file.
- `-h`: Print help message.


## Usage

1. Build and flash a Bluetooth - SoC Throughput example or demo onto your Bluetooth LE device.
2. Generate a Bluetooth - NCP application.
    - Add the bluetooth_feature_power_control.
    - Build and flash to the target.
3. Connect the NCP target to your host.
4. Start the host application.
5. If no time or data length was specified, press Button 0
    on the board with the Bluetooth - SoC Throughput application.


## Output

The script displays:
- Current throughput (bps)
- Total bytes sent/received
- Test duration
- Status

## References

- [Bluetooth LE SDK Documentation](https://docs.silabs.com/bluetooth/latest/)
- [Using the Silicon Labs Bluetooth® Stack v3.x and Higher in Network
Co-Processor Mode](https://www.silabs.com/documents/public/application-notes/an1259-bt-ncp-mode-sdk-v3x.pdf)
- [Bluetooth Host Development](https://docs.silabs.com/bluetooth/latest/bluetooth-network-coprocessor-mode/03-ncp-host-development)
