/***************************************************************************/ /**
 * @file  sli_aws_mqtt_transport_lwip.c
 * @brief LwIP + mbedTLS transport I/O for coreMQTT (app-owned TLS session).
 *******************************************************************************
 * # License
 * <b>Copyright 2026 Silicon Laboratories Inc. www.silabs.com</b>
 *******************************************************************************
 *
 * SPDX-License-Identifier: Zlib
 *
 ******************************************************************************/

#include <string.h>
#include "sl_aws_mqtt_transport.h"
#include "cmsis_os2.h"
#include "mbedtls/error.h"
#include "sl_constants.h"

#define AWS_MQTT_TRANSPORT_IO_TIMEOUT_MS 1000U

/* mbedTLS returns these while the session is still healthy: the caller must
 * retry instead of tearing the MQTT connection down. */
static bool sli_transport_retryable(int ret)
{
  bool retryable =
    ((ret == MBEDTLS_ERR_SSL_WANT_READ) || (ret == MBEDTLS_ERR_SSL_WANT_WRITE) || (ret == MBEDTLS_ERR_SSL_TIMEOUT));

#if defined(MBEDTLS_ERR_SSL_RECEIVED_NEW_SESSION_TICKET)
  /* TLS 1.3 servers (including AWS IoT) send NewSessionTicket right after the
   * handshake; it surfaces here as a non-fatal read result. */
  retryable = retryable || (ret == MBEDTLS_ERR_SSL_RECEIVED_NEW_SESSION_TICKET);
#endif

  return retryable;
}

static int32_t sli_transport_send(NetworkContext_t *ctx, const void *buffer, size_t bytes_to_send)
{
  size_t bytes_sent = 0;
  const uint8_t *p  = buffer;
  uint32_t start_ms = osKernelGetTickCount();

  if ((ctx == NULL) || (ctx->ssl == NULL) || (buffer == NULL)) {
    return -1;
  }

  while ((osKernelGetTickCount() - start_ms) < AWS_MQTT_TRANSPORT_IO_TIMEOUT_MS && bytes_sent < bytes_to_send) {
    int ret = mbedtls_ssl_write(ctx->ssl, p + bytes_sent, bytes_to_send - bytes_sent);

    if (sli_transport_retryable(ret)) {
      osDelay(1);
      continue;
    }

    if (ret < 0) {
      SL_DEBUG_LOG("[AWS MQTT] mbedtls_ssl_write failed: -0x%04x", (unsigned int)(-ret));
      ctx->is_connected = false;
      return -1;
    }

    bytes_sent += (size_t)ret;
  }

  return (int32_t)bytes_sent;
}

/* coreMQTT asks for the whole free space of its network buffer on every poll and
 * accepts short reads, so this must return whatever is available right now. It
 * must never wait for `bytes_to_recv` bytes: the missing bytes usually do not
 * exist, and waiting for them parks the caller in a blocking lwIP recv(). */
static int32_t sli_transport_recv(NetworkContext_t *ctx, void *buffer, size_t bytes_to_recv)
{
  int read_return_value = 0;

  if ((ctx == NULL) || (ctx->ssl == NULL) || (buffer == NULL) || (bytes_to_recv == 0U)) {
    return -1;
  }

  read_return_value = mbedtls_ssl_read(ctx->ssl, (unsigned char *)buffer, bytes_to_recv);

  if (sli_transport_retryable(read_return_value)) {
    /* mbedTLS keeps any partially received record buffered internally, so the
     * next poll resumes where this one stopped. */
    return 0;
  }

  if ((read_return_value == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY) || (read_return_value == 0)) {
    SL_DEBUG_LOG("[AWS MQTT] broker closed the TLS session");
    ctx->is_connected = false;
    return -1;
  }

  if (read_return_value < 0) {
    SL_DEBUG_LOG("[AWS MQTT] mbedtls_ssl_read failed: -0x%04x", (unsigned int)(-read_return_value));
    ctx->is_connected = false;
    return -1;
  }

  return (int32_t)read_return_value;
}

static int32_t transportSend(NetworkContext_t *pNetworkContext, const void *pBuffer, size_t bytesToSend)
{
  return sli_transport_send(pNetworkContext, pBuffer, bytesToSend);
}

static int32_t transportRecv(NetworkContext_t *pNetworkContext, void *pBuffer, size_t bytesToRecv)
{
  return sli_transport_recv(pNetworkContext, pBuffer, bytesToRecv);
}

TransportInterface_t sl_aws_mqtt_transportGetInterface(NetworkContext_t *ctx)
{
  TransportInterface_t transport = { 0 };

  transport.pNetworkContext = ctx;
  transport.send            = transportSend;
  transport.recv            = transportRecv;

  return transport;
}
