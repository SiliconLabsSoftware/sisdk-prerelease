/***************************************************************************/ /**
 * @file  sl_aws_mqtt_transport.h
 * @brief SiLabs LwIP TLS transport for coreMQTT (TransportInterface_t).
 *
 * The application owns TLS socket setup (TCP connect + mbedTLS handshake).
 * Populate NetworkContext_t with the connected socket and SSL objects, then
 * call sl_aws_mqtt_transportGetInterface() for coreMQTT.
 *******************************************************************************
 * # License
 * <b>Copyright 2026 Silicon Laboratories Inc. www.silabs.com</b>
 *******************************************************************************
 *
 * SPDX-License-Identifier: Zlib
 *
 ******************************************************************************/

#ifndef SL_AWS_MQTT_TRANSPORT_H
#define SL_AWS_MQTT_TRANSPORT_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "mbedtls/ssl.h"
#include "transport_interface.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Network context for LwIP + mbedTLS MQTT transport.
 *
 * Application must establish TLS and set:
 * - socket_id: connected LwIP TCP socket
 * - ssl / conf: mbedTLS objects after a successful handshake
 * - is_connected: true when the TLS session is ready for MQTT I/O
 */
struct NetworkContext {
  int socket_id;
  bool is_connected;
  mbedtls_ssl_context *ssl;
  mbedtls_ssl_config *conf;
};

typedef struct NetworkContext NetworkContext_t;

/**
 * @brief Get a TransportInterface_t for coreMQTT.
 *
 * Uses ctx->ssl for encrypted send/recv. TLS must already be established.
 */
TransportInterface_t sl_aws_mqtt_transportGetInterface(NetworkContext_t *ctx);

#ifdef __cplusplus
}
#endif

#endif /* SL_AWS_MQTT_TRANSPORT_H */
