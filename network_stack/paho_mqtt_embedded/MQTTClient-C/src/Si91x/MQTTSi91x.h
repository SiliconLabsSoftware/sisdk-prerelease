/*******************************************************************************
 * Copyright (c) 2014 IBM Corp.
 *
 * All rights reserved. This program and the accompanying materials
 * are made available under the terms of the Eclipse Public License v1.0
 * and Eclipse Distribution License v1.0 which accompany this distribution.
 *
 * The Eclipse Public License is available at
 *    http://www.eclipse.org/legal/epl-v10.html
 * and the Eclipse Distribution License is available at
 *   http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * Contributors:
 *    Allan Stockdill-Mander - initial API and implementation and/or initial documentation
 *******************************************************************************/

#ifndef MQTTSI91X_H
#define MQTTSI91X_H

#include <stdint.h>
#include "sl_utility.h"
#include "cmsis_os2.h"

#define MQTT_WITH_WEBSOCKET_HEADER_LEN         3
#define WAIT_TIMEOUT                           5000
#define SLI_917_WAIT_FOREVER                   0
#define WIFI_BLOCKED_TIMEOUT_SF                1
#define DEFAULT_TIMEOUT                        SLI_TX_EVENT_WAIT_TIME
#define SLI_TX_EVENT_WAIT_TIME                 (30000 + TX_WAIT_TIME)
#define TX_WAIT_TIME                           0
#define SLI_SOCKET_RECVFROM_RESPONSE_WAIT_TIME ((SLI_917_WAIT_FOREVER * WIFI_BLOCKED_TIMEOUT_SF) + (DEFAULT_TIMEOUT))
#define SINGLE_PKT_TCP_STREAM_TIMEOUT          WAIT_TIMEOUT + SLI_SOCKET_RECVFROM_RESPONSE_WAIT_TIME

#include "sl_paho_error_codes.h"

// Address family for sl_paho_network_connect; set exactly one (distinct bits).
#define SL_PAHO_NETWORK_FLAG_IPV4 (1U << 0)
#define SL_PAHO_NETWORK_FLAG_IPV6 (1U << 1)

// MQTT TLS Configuration
#ifndef MQTT_TLS_AVAILABLE
#define MQTT_TLS_AVAILABLE 1 // Set to 1 to enable TLS support, 0 to disable
#endif

#include "sli_mqtt_tls_alpn.h"

typedef struct Timer Timer;
struct Timer {
  uint32_t systick_period;
  uint32_t end_time;
};

typedef enum { MQTT_TRANSPORT_TCP, MQTT_TRANSPORT_WEBSOCKET } mqtt_transport_t;

/**
 * @brief Represents the network abstraction layer for MQTT communication.
 *
 * @details
 * The `Network` structure provides an abstraction for network operations required by the MQTT client.
 * It supports both TCP and WebSocket transport types and includes function pointers for transport-specific
 * operations such as reading, writing, and disconnecting.
 *
 * Fields:
 * - `socket`: The socket descriptor used for the network connection.
 *   - For TCP, this represents the file descriptor of the TCP socket.
 *   - For WebSocket, this represents the WebSocket connection handle.
 *
 * - `mqttread`: Function pointer for reading data from the network.
 *
 * - `mqttwrite`: Function pointer for writing data to the network.
 *
 * - `disconnect`: Function pointer for disconnecting the network connection.
 *
 * - `transport_type`: Enum indicating the transport type used for the connection.
 *   - Possible values:
 *     - `MQTT_TRANSPORT_TCP`: Indicates that the connection uses TCP transport.
 *     - `MQTT_TRANSPORT_WEBSOCKET`: Indicates that the connection uses WebSocket transport.
 *
 * @note
 * - The `Network` structure must be initialized using the `NetworkInit` function before use.
 * - The transport-specific function pointers (`mqttread`, `mqttwrite`, and `disconnect`) are assigned
 *   during initialization based on the `transport_type`.
 */

typedef struct Network Network;
struct Network {
  int socket;
  int (*mqttread)(Network *, unsigned char *, int, int);
  int (*mqttwrite)(Network *, unsigned char *, int, int);
  void (*disconnect)(Network *);
  mqtt_transport_t transport_type;
};

uint32_t osKernelGetTickCount(void);
void InitTimer(Timer *);
char expired(Timer *);
void countdown_ms_mqtt(Timer *, unsigned int);
void countdown(Timer *, unsigned int);
int left_ms_mqtt(Timer *);

/**
 * @brief Initializes the network structure and sets up transport-specific callbacks.
 *
 * @param[in] n Pointer to the Network structure to be initialized.
 *
 * @details
 * - This function initializes the `Network` structure by setting the `socket` to `-1`.
 * - It assigns transport-specific function pointers (`mqttread`, `mqttwrite`, and `disconnect`) 
 *   based on the `transport_type` field in the `Network` structure.
 * - Supported transport types are:
 *   - `MQTT_TRANSPORT_TCP`: Initializes for TCP transport.
 *   - `MQTT_TRANSPORT_WEBSOCKET`: Initializes for WebSocket transport.
 *
 * @note
 * - Ensure that the `Network` structure is allocated and valid before calling this function.
 * - If the `transport_type` is invalid, the function does nothing.
 */
void NetworkInit(Network *n);

/**
 * @brief Establishes a connection to the MQTT broker using the specified transport type.
 *
 * @param[in] n         Pointer to the initialized Network structure.
 * @param[in] flags     Connection flags (reserved for future use).
 * @param[in] addr      Pointer to the broker's IP address (IPv4 or IPv6).
 * @param[in] dst_port  Destination port of the MQTT broker.
 * @param[in] src_port  Source port for the connection.
 * @param[in] ssl       Boolean flag to enable/disable SSL for the connection.
 *
 * @return
 * - `0` on success.
 * - `NETWORK_ERROR_NULL_STRUCTURE (-1)` if the `Network` structure is `NULL`.
 * - `NETWORK_ERROR_NULL_ADDRESS (-2)` if the `addr` is `NULL`.
 * - `NETWORK_ERROR_INVALID_TYPE (-3)` if the `transport_type` is invalid.
 * - Transport-specific error codes for connection failures (legacy values unchanged).
 *
 * @details
 * - This function establishes a connection to the MQTT broker using either TCP or WebSocket transport.
 * - It delegates the connection logic to transport-specific handlers:
 *   - `mqtt_tcpconnection_handler` for TCP.
 *   - `mqtt_websocketconnection_handler` for WebSocket.
 * - SSL can be enabled for secure connections by setting the `ssl` parameter to `true`.
 * - When `MQTT_TLS_ALPN_ENABLED` is set to `1`, ALPN is configured automatically during
 *   TCP connection setup (after socket creation and TLS enable, before connect).
 *   Set `MQTT_TLS_ALPN_PROTOCOL` to the broker's ALPN name (e.g. `"mqtt"` for Mosquitto on port 443).
 * - ALPN applies to @ref MQTT_TRANSPORT_TCP with TLS only. The WebSocket transport
 *   (`MQTT_TRANSPORT_WEBSOCKET`) does not configure TLS ALPN through this SDK path.
 *   Secure WebSocket (WSS) on port 443 typically negotiates `http/1.1` at the TLS layer
 *   rather than `mqtt`; that scenario is not covered by this ALPN integration.
 *
 * @note
 * - Ensure that the `Network` structure is initialized using `NetworkInit` before calling this function.
 * - The `addr` parameter must point to a valid IP address string.
 */
int NetworkConnect(Network *n, uint8_t flags, char *addr, int dst_port, int src_port, bool ssl);

/**
 * @brief Establishes a connection to the MQTT broker with unique domain error codes.
 *
 * Supports TCP (`MQTT_TRANSPORT_TCP`) and WebSocket (`MQTT_TRANSPORT_WEBSOCKET`).
 * Parameter meaning for `flags` and `addr` depends on `n->transport_type` (see contract below).
 *
 * @param[in] n         Pointer to the initialized Network structure (`transport_type` must be set).
 * @param[in] flags     Transport-specific (see **Parameter contract**).
 * @param[in] addr      Transport-specific (see **Parameter contract**). Must not be `NULL`.
 * @param[in] dst_port  Destination port of the MQTT broker.
 * @param[in] src_port  Source / client port for the connection.
 * @param[in] ssl       Boolean flag to enable/disable SSL/TLS (or WSS for WebSocket).
 *
 * @return
 * - `SL_NETWORK_SUCCESS (0)` on success.
 * - `SL_NETWORK_ERR_NULL_STRUCTURE (-1)` if the `Network` structure is `NULL`.
 * - `SL_NETWORK_ERR_NULL_ADDRESS (-2)` if the `addr` is `NULL`.
 * - `SL_NETWORK_ERR_INVALID_TYPE (-3)` if the `transport_type` is invalid.
 * - `SL_NETWORK_ERR_INVALID_FLAGS (-5)` if TCP flags are invalid (neither/both IPV4/IPV6,
 *   or IPV6 on an IPv4-only build). Not applicable to WebSocket (`flags` ignored).
 * - `SL_NETWORK_WS_ERR_*` (-101 .. -199) for WebSocket failures (mapped from WiseConnect).
 * - `SL_NETWORK_TCP_ERR_*` (-201 .. -299) for TCP socket failures.
 * - `SL_NETWORK_TLS_ERR_*` (-301 .. -399) for TLS configuration failures (TCP path).
 *
 * @par Parameter contract (TCP vs WebSocket)
 * | | TCP (`MQTT_TRANSPORT_TCP`) | WebSocket (`MQTT_TRANSPORT_WEBSOCKET`) |
 * |---|---------------------------|----------------------------------------|
 * | `flags` | Exactly one of `SL_PAHO_NETWORK_FLAG_IPV4` or `SL_PAHO_NETWORK_FLAG_IPV6` | Ignored (pass `0`) |
 * | `addr` | Raw IP octets: 4 bytes (IPv4) or 16 bytes (IPv6) | NUL-terminated host or IP **string** for the WebSocket client |
 *
 * Do not pass a hostname string as `addr` for TCP, and do not pass raw IP octets as `addr` for
 * WebSocket—the same parameter has different layouts per transport.
 *
 * @par Examples
 * @code
 * // TCP IPv4: addr is 4 raw octets
 * uint8_t broker_ip[4] = { 192, 168, 1, 10 };
 * n.transport_type = MQTT_TRANSPORT_TCP;
 * NetworkInit(&n);
 * sl_paho_network_connect(&n, SL_PAHO_NETWORK_FLAG_IPV4, (char *)broker_ip, 8883, 0, true);
 *
 * // WebSocket: addr is a host string; flags unused
 * n.transport_type = MQTT_TRANSPORT_WEBSOCKET;
 * NetworkInit(&n);
 * sl_paho_network_connect(&n, 0, "mqtt.example.com", 443, 0, true);
 * @endcode
 *
 * @details
 * - Prefer this API over NetworkConnect for unique, domain-based error codes.
 * - For TCP on dual-IP builds, use `flags` to select the socket family.
 * - All return codes are unique by domain; see `sl_paho_error_codes.h`.
 * - On successful WebSocket connect, `n->mqttwrite` / `n->mqttread` are switched to
 *   mapped callbacks so subsequent Paho MQTT I/O returns `SL_NETWORK_WS_ERR_*`
 *   (-101 .. -199). `NetworkConnect` leaves the legacy raw WiseConnect write/read
 *   callbacks in place for backward compatibility.
 * - SSL can be enabled for secure connections by setting the `ssl` parameter to `true`.
 * - When `MQTT_TLS_ALPN_ENABLED` is set to `1`, ALPN is configured automatically during
 *   TCP connection setup (after socket creation and TLS enable, before connect).
 *   Set `MQTT_TLS_ALPN_PROTOCOL` to the broker's ALPN name (e.g. `"mqtt"` for Mosquitto on port 443).
 * - ALPN applies to TCP/TLS only; WebSocket/WSS does not configure MQTT ALPN through this path.
 *
 * @note
 * - Call `NetworkInit` after setting `n->transport_type` and before calling this function.
 * - Choosing the wrong `addr` layout for the transport is a caller error and may fail with
 *   TCP/WS domain codes rather than `SL_NETWORK_ERR_INVALID_FLAGS`.
 */
int sl_paho_network_connect(Network *n, uint8_t flags, char *addr, int dst_port, int src_port, bool ssl);

/**
 * @brief Cleans up and closes the network connection.
 *
 * @param[in] n Pointer to the initialized Network structure.
 *
 * @details
 * - This function invokes the transport-specific `disconnect` callback to clean up the connection.
 * - For TCP, it closes the socket using `mqtt_tcp_disconnect`.
 * - For WebSocket, it deinitializes the WebSocket connection using `mqtt_ws_disconnect`.
 *
 * @note
 * - Ensure that the `Network` structure is valid and initialized before calling this function.
 * - If the `disconnect` callback is not set, the function does nothing.
 */
void NetworkDisconnect(Network *n);
#endif
