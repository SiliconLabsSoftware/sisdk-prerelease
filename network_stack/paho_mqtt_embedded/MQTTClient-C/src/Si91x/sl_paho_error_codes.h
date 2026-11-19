/***************************************************************************/ /**
 * @file sl_paho_error_codes.h
 * @brief Centralized error codes for sl_paho_network_connect and related APIs.
 *
 * Error values are allocated by domain so the numeric code identifies the
 * subsystem. These codes apply to sl_paho_network_connect and to the mapped
 * WebSocket I/O callbacks installed after a successful WebSocket
 * sl_paho_network_connect (n->mqttwrite / n->mqttread).
 * NetworkConnect retains its legacy return values (NETWORK_ERROR_* and
 * transport-specific codes) for backward compatibility.
 *
 * Ranges:
 *   Network   :  -1  ..  -99
 *   WebSocket : -101 .. -199
 *   TCP       : -201 .. -299
 *   TLS       : -301 .. -399
 ******************************************************************************/
#ifndef SL_PAHO_ERROR_CODES_H
#define SL_PAHO_ERROR_CODES_H

#ifdef __cplusplus
extern "C" {
#endif

/*******************************************************************************
 * Range bounds (inclusive)
 ******************************************************************************/
#define SL_PAHO_ERR_NETWORK_BASE   (-1)
#define SL_PAHO_ERR_NETWORK_END    (-99)
#define SL_PAHO_ERR_WEBSOCKET_BASE (-101)
#define SL_PAHO_ERR_WEBSOCKET_END  (-199)
#define SL_PAHO_ERR_TCP_BASE       (-201)
#define SL_PAHO_ERR_TCP_END        (-299)
#define SL_PAHO_ERR_TLS_BASE       (-301)
#define SL_PAHO_ERR_TLS_END        (-399)

/*******************************************************************************
 * Success
 ******************************************************************************/
#define SL_NETWORK_SUCCESS (0)

/*******************************************************************************
 * Network domain errors (-1 .. -99)
 *
 * Used for NetworkConnect / sl_paho_network_connect argument validation.
 * Value -4 is reserved for backend-specific NetworkConnect aliases
 * (e.g. NETWORK_ERROR_TLS_HOSTNAME_REQUIRED on LwIP, NETWORK_ERROR_CONNECT_FAILED
 * on dual-stack). Prefer the TLS / TCP range codes for new call sites.
 ******************************************************************************/
#define SL_NETWORK_ERR_NULL_STRUCTURE (-1) /**< Network structure pointer is NULL */
#define SL_NETWORK_ERR_NULL_ADDRESS   (-2) /**< Address pointer is NULL */
#define SL_NETWORK_ERR_INVALID_TYPE   (-3) /**< Unsupported or invalid transport_type */
#define SL_NETWORK_ERR_INVALID_FLAGS  (-5) /**< Invalid sl_paho_network_connect flags */

/*******************************************************************************
 * WebSocket domain errors (-101 .. -199)
 *
 * Mapped from WiseConnect sl_websocket_error_t values so WS failures never
 * collide with Network / TCP codes. Mapping: WiseConnect (-N) -> (-100 - N).
 *   SL_WEBSOCKET_ERR_SOCKET_CREATION (-1) -> SL_NETWORK_WS_ERR_SOCKET_CREATION (-101)
 ******************************************************************************/
#define SL_NETWORK_WS_ERR_SOCKET_CREATION     (-101) /**< Socket creation failed */
#define SL_NETWORK_WS_ERR_SOCKET_BIND         (-102) /**< Socket bind failed */
#define SL_NETWORK_WS_ERR_SOCKET_CONNECT      (-103) /**< Socket connect failed */
#define SL_NETWORK_WS_ERR_SEND_FRAME          (-104) /**< Send frame failed */
#define SL_NETWORK_WS_ERR_RECEIVE_FRAME       (-105) /**< Receive frame failed */
#define SL_NETWORK_WS_ERR_CLOSE_FRAME         (-106) /**< Close frame failed */
#define SL_NETWORK_WS_ERR_SSL_SETSOCKOPT      (-107) /**< SSL setsockopt failed */
#define SL_NETWORK_WS_ERR_INVALID_PARAMETER   (-108) /**< Invalid WebSocket parameter */
#define SL_NETWORK_WS_ERR_MAX_LENGTH_EXCEEDED (-109) /**< Max length exceeded */
#define SL_NETWORK_WS_ERR_UNKNOWN             (-199) /**< Unrecognized WebSocket error */

/*******************************************************************************
 * TCP domain errors (-201 .. -299)
 ******************************************************************************/
#define SL_NETWORK_TCP_ERR_SOCKET_CREATE  (-201) /**< BSD / Si91x socket() failed */
#define SL_NETWORK_TCP_ERR_SOCKET_BIND    (-202) /**< bind() failed */
#define SL_NETWORK_TCP_ERR_SOCKET_CONNECT (-203) /**< connect() failed */
#define SL_NETWORK_TCP_ERR_SETSOCKOPT     (-204) /**< setsockopt() failed */
#define SL_NETWORK_TCP_ERR_SEND           (-205) /**< send() failed */
#define SL_NETWORK_TCP_ERR_RECV           (-206) /**< recv() failed */

/*******************************************************************************
 * TLS domain errors (-301 .. -399)
 ******************************************************************************/
#define SL_NETWORK_TLS_ERR_HOSTNAME_REQUIRED (-301) /**< TLS hostname not set (SNI / verify) */
#define SL_NETWORK_TLS_ERR_CONTEXT_ALLOC     (-302) /**< Failed to allocate TLS context */
#define SL_NETWORK_TLS_ERR_INIT              (-303) /**< TLS initialization failed */
#define SL_NETWORK_TLS_ERR_HANDSHAKE         (-304) /**< TLS handshake failed */
#define SL_NETWORK_TLS_ERR_NOT_AVAILABLE     (-305) /**< TLS requested but not compiled in */
#define SL_NETWORK_TLS_ERR_ALPN              (-306) /**< TLS ALPN configuration failed */

/*******************************************************************************
 * Legacy NetworkConnect aliases (parameter validation)
 *
 * Keep numeric values stable for existing NetworkConnect callers.
 ******************************************************************************/
#ifndef NETWORK_ERROR_NULL_STRUCTURE
#define NETWORK_ERROR_NULL_STRUCTURE SL_NETWORK_ERR_NULL_STRUCTURE
#endif
#ifndef NETWORK_ERROR_NULL_ADDRESS
#define NETWORK_ERROR_NULL_ADDRESS SL_NETWORK_ERR_NULL_ADDRESS
#endif
#ifndef NETWORK_ERROR_INVALID_TYPE
#define NETWORK_ERROR_INVALID_TYPE SL_NETWORK_ERR_INVALID_TYPE
#endif
#ifndef NETWORK_ERROR_INVALID_FLAGS
#define NETWORK_ERROR_INVALID_FLAGS SL_NETWORK_ERR_INVALID_FLAGS
#endif

/**
 * @brief Map a WiseConnect sl_websocket_error_t value into the WebSocket range.
 *
 * @param[in] ws_error Value returned by a WiseConnect WebSocket API
 *                     (0 success, -1 .. -9 known errors).
 *
 * @return 0 on success; otherwise a SL_NETWORK_WS_ERR_* code in [-101, -199].
 */
static inline int sl_paho_map_websocket_error(int ws_error)
{
  if (ws_error == 0) {
    return SL_NETWORK_SUCCESS;
  }

  /* Known WiseConnect codes are -1 .. -9; map (-N) -> (-100 - N). */
  if (ws_error <= -1 && ws_error >= -9) {
    return (SL_PAHO_ERR_WEBSOCKET_BASE + (ws_error + 1));
  }

  return SL_NETWORK_WS_ERR_UNKNOWN;
}

#ifdef __cplusplus
}
#endif

#endif /* SL_PAHO_ERROR_CODES_H */
