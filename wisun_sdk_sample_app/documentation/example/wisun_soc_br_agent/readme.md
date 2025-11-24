# Wi-SUN - SoC Border Router Agent

- [Introduction](#introduction)
- [Getting Started](#getting-started)
- [Features](#features)
- [Architecture Overview](#architecture-overview)
  - [Data Flow](#data-flow)
- [Message Protocol](#message-protocol)
  - [Message Frame Format](#message-frame-format)
  - [Request / Response Codes](#request--response-codes)
  - [Configuration Payload Structure](#configuration-payload-structure)
- [Troubleshooting](#troubleshooting)
- [Resources](#resources)
- [Report Bugs & Get Support](#report-bugs--get-support)

## Introduction

The Wi-SUN SoC Border Router Agent sample application extends the Wi-SUN SoC Border Router by adding an Agent Service capable of:

- Exposing the current network topology and the active Border Router configuration parameters to a remote host agent via Wi-Fi backhaul.
- Receiving new configuration parameters from a remote host and applying them at runtime.
- Stopping and restarting the Border Router operation remotely.

A limited CLI (Command-Line Interface) is exposed to facilitate the Wi-SUN / WiFi configuration.

The SoC term stands for "System on Chip", meaning that this is a standalone application running on the EFR32 without any external MCU required.
It provides an easy and quick way to evaluate the Silicon Labs Wi-SUN stack solution without deploying an expensive and cumbersome production-grade Wi-SUN Border Router.

## Getting Started

To get started with Wi-SUN and Simplicity Studio, see [Getting Started with Wi-SUN Application Development](https://docs.silabs.com/wisun/latest/wisun-getting-started-development).

This sample application is the counterpart to the external Linux "Wi-SUN SoC Border Router Agent" service (remote host agent) whose responsibilities include:

- Maintaining a D-Bus interface for GUI, see [Wi-SUN Border Router GUI](https://docs.silabs.com/wisun/latest/wisun-border-router-gui/).
- Translating device topology & configuration into D-Bus properties.
- Relaying control operations (Restart BR / Stop BR / Set config) originating from UI or scripts to the SoC via the TCP protocol described above.

For prerequisites and complete setup see, [Wi-SUN SoC Border Router Agent Linux Host](https://github.com/foobar/wisun-soc-br-agent).

Wi-Fi Backhaul Connectivity is based on the SiWx91x™ chipset and the WiSeConnect™ SDK v3.x.
Follow the [Getting Started with WiSeConnect™ SDK v3.x and EFR32™ Host in NCP Mode](https://docs.silabs.com/wiseconnect/3.5.1/wiseconnect-getting-started/getting-started-with-ncp-mode-with-efr32) to configure the SiWN917 as a Network Co-Processor (NCP). This document is limited to additional commands and settings, a more detailed documentation can be found here [SoC Border Router with Wi-Fi Backhaul](https://docs.silabs.com/wisun/latest/wisun-network-configuration/06-wisun-soc-border-router-backhaul).

## Features

- TCP server listening on a configurable port for inbound Agent requests.
- Client-initiated TCP connections to a remote Agent host to push:
  - Updated network topology when routing changes occur.
  - Configuration parameters.
- DHCPv6 server integration.
- Wi-Fi support for backhaul connectivity.
- Thread-safe remote address runtime reconfiguration.

## Architecture Overview

The sample application is composed of two main logical parts:

| Component | Responsibility |
|-----------|----------------|
| Core application task | Initializes Wi-SUN Border Router stack, Wi-Fi for backhaul connectivity, DHCPv6 server. |
| Agent Service | Sending Border Router specific metrics to the remote host via Wi-Fi backhaul, implement data model for sending them to the remote Agent running on Linux, serialize data and send over lwIP/TCP, gathers network topology / config, applies settings. |

### Data Flow

The application follows an event-driven architecture with the following data flow patterns:

- Wi-SUN stack events (e.g., routing table changes) trigger:
  - Network topology retrieval
  - TCP client connection to remote host
  - Automatic transmission of updated routing information via Wi-Fi

- Remote host connections are handled through:
  - Accepting connections and reading frames
  - Interpreting message headers and payloads
  - Executing requested actions, runtime configuration updates or returning data

- Border Router startup automatically sends current configuration parameters to the remote host agent as part of the initialization sequence.

## Message Protocol

### Message Frame Format

Each message (request or response) has the following binary layout (network byte order / big-endian for multi-byte fields):

| Offset | Size (bytes) | Field | Description |
|--------|--------------|-------|-------------|
| 0 | 4 | msg_code | 32-bit unsigned request/response code. |
| 4 | 4 | payload_len | 32-bit unsigned payload length in bytes. |
| 8 | payload_len | payload | Omitted if length = 0. |

Alignment: The structure is tightly packed when transmitted (1 byte alignment).

### Request / Response Codes

| Code Macro | Value | Description |
|------------|-------|-------------|
| `SL_WISUN_BR_AGENT_SERVICE_CODE_GET_TOPOLOGY` | 0x01 | Retrieve current topology. |
| `SL_WISUN_BR_AGENT_SERVICE_CODE_GET_CONFIG_PARAMS` | 0x02 | Retrieve current BR configuration parameters. |
| `SL_WISUN_BR_AGENT_SERVICE_CODE_SET_CONFIG_PARAMS` | 0x03 | Apply new BR configuration parameters. No response payload on success. |
| `SL_WISUN_BR_AGENT_SERVICE_CODE_RESTART_BR` | 0x04 | Restart the Border Router. |
| `SL_WISUN_BR_AGENT_SERVICE_CODE_STOP_BR` | 0x05 | Stop the Border Router. |

### Configuration Payload Structure

For GET / SET config operations the payload is a packed structure containing:

```c
char     network_name[APP_SETTING_NETWORK_NAME_MAX_SIZE];
uint8_t  network_size;
int16_t  tx_power_ddbm;
uint8_t  uc_dwell_interval_ms;
uint32_t bc_interval_ms;
uint8_t  bc_dwell_interval_ms;
uint8_t  state;
char     allowed_channels[APP_UTIL_PRINTABLE_DATA_MAX_LENGTH + 1];
char     ipv6_prefix[APP_IPV6_PREFIX_SIZE + 1];
uint8_t  regulation;
uint8_t  fec;
uint8_t  rx_phy_mode_ids[SL_WISUN_MAX_PHY_MODE_ID_COUNT];
uint8_t  rx_phy_mode_ids_count;
uint8_t  lfn_profile;
uint8_t  max_neighbor_count;
uint8_t  max_child_count;
uint16_t max_security_neighbor_count;
uint8_t  keychain;
uint8_t  keychain_index;
uint16_t socket_rx_buffer_size;
sl_wisun_phy_config_t phy;
bool     is_default_phy;
uint16_t pan_id;
```

## Troubleshooting

Before programming the radio board mounted on the WSTK, ensure the power supply switch is in the AEM position (right side), as shown.

![Radio Board Power Supply Switch](readme_img0.png)

## Resources

- [Wi-SUN Getting Started Guide](https://docs.silabs.com/wisun/latest/wisun-getting-started-development)
- [Wi-SUN Stack API documentation](https://docs.silabs.com/wisun/latest)
- [Wi-SUN Border Router GUI](https://docs.silabs.com/wisun/latest/wisun-border-router-gui/)
- [Getting Started with WiSeConnect™ SDK v3.x and EFR32™ Host in NCP Mode](https://docs.silabs.com/wiseconnect/3.5.1/wiseconnect-getting-started/getting-started-with-ncp-mode-with-efr32)
- [Wi-SUN SoC Border Router Agent Linux Host](https://github.com/foobar/wisun-soc-br-agent)
- [SoC Border Router with Wi-Fi Backhaul](https://docs.silabs.com/wisun/latest/wisun-network-configuration/06-wisun-soc-border-router-backhaul)

## Report Bugs & Get Support

You are always encouraged and welcome to ask any questions or report any issues you found to us via [Silicon Labs Community](https://community.silabs.com/s/topic/0TO1M000000qHc6WAE/wisun).
