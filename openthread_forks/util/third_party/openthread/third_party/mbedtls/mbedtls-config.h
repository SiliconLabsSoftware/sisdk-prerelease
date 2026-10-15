/*
 *  Copyright (c) 2018, The OpenThread Authors.
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are met:
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *  3. Neither the name of the copyright holder nor the
 *     names of its contributors may be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 *  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 */

// Spans multiple lines to avoid being processed by unifdef
#ifndef \
    MBEDTLS_CONFIG_H
#define MBEDTLS_CONFIG_H

#include "openthread-core-config.h"

#include <openthread/config.h>

#define MBEDTLS_CONFIG_VERSION 0x04010000

// ==============================================================================
// SSL / TLS configuration (Mbed TLS 4.x upper layer)
// Crypto algorithms live in psa-crypto-config.h (TF_PSA_CRYPTO_CONFIG_FILE).
// ==============================================================================

#define MBEDTLS_SSL_CLI_C
#define MBEDTLS_SSL_DTLS_ANTI_REPLAY
#define MBEDTLS_SSL_DTLS_HELLO_VERIFY
#define MBEDTLS_SSL_EXPORT_KEYS
#define MBEDTLS_SSL_MAX_FRAGMENT_LENGTH
#define MBEDTLS_SSL_PROTO_TLS1_2
#define MBEDTLS_SSL_PROTO_DTLS
#define MBEDTLS_SSL_TLS_C

#if OPENTHREAD_CONFIG_BORDER_AGENT_ENABLE || OPENTHREAD_CONFIG_COMMISSIONER_ENABLE || OPENTHREAD_CONFIG_COAP_SECURE_API_ENABLE
#define MBEDTLS_SSL_COOKIE_C
#define MBEDTLS_SSL_SRV_C
#endif

#if OPENTHREAD_CONFIG_BLE_TCAT_ENABLE
#define MBEDTLS_SSL_KEEP_PEER_CERTIFICATE
#endif

// EC-JPAKE via PSA PAKE (requires PSA_WANT_ALG_JPAKE in psa-crypto-config.h).
#define MBEDTLS_KEY_EXCHANGE_ECJPAKE_ENABLED

#if OPENTHREAD_CONFIG_COAP_SECURE_API_ENABLE
#define MBEDTLS_KEY_EXCHANGE_PSK_ENABLED
#endif

#if OPENTHREAD_CONFIG_COAP_SECURE_API_ENABLE || OPENTHREAD_CONFIG_TLS_ENABLE
#define MBEDTLS_KEY_EXCHANGE_ECDHE_ECDSA_ENABLED
#endif

#if OPENTHREAD_CONFIG_BLE_TCAT_ENABLE
#define MBEDTLS_SSL_MAX_CONTENT_LEN      2000 /**< Maximum fragment length in bytes */
#elif OPENTHREAD_CONFIG_COAP_SECURE_API_ENABLE
#define MBEDTLS_SSL_MAX_CONTENT_LEN      900 /**< Maximum fragment length in bytes */
#else
#define MBEDTLS_SSL_MAX_CONTENT_LEN      768 /**< Maximum fragment length in bytes */
#endif

#define MBEDTLS_SSL_IN_CONTENT_LEN       MBEDTLS_SSL_MAX_CONTENT_LEN
#define MBEDTLS_SSL_OUT_CONTENT_LEN      MBEDTLS_SSL_MAX_CONTENT_LEN
#define MBEDTLS_SSL_CIPHERSUITES         MBEDTLS_TLS_ECJPAKE_WITH_AES_128_CCM_8

// ==============================================================================
// X.509 (TLS layer). PK/PEM/BASE64 live in psa-crypto-config.h for Mbed TLS 4.x.
// ==============================================================================

#if OPENTHREAD_CONFIG_COAP_SECURE_API_ENABLE || OPENTHREAD_CONFIG_TLS_ENABLE
#define MBEDTLS_X509_USE_C
#define MBEDTLS_X509_CRT_PARSE_C
#endif

// Spans multiple lines to avoid being processed by unifdef
#if defined(\
    MBEDTLS_USER_CONFIG_FILE)
#include MBEDTLS_USER_CONFIG_FILE
#endif

#endif /* MBEDTLS_CONFIG_H */
