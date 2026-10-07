/*
 *    Copyright (c) 2019, The OpenThread Authors.
 *    All rights reserved.
 *
 *    Redistribution and use in source and binary forms, with or without
 *    modification, are permitted provided that the following conditions are met:
 *    1. Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *    2. Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *    3. Neither the name of the copyright holder nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 *    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *    AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *    IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *    ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 *    LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *    CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *    SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *    INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *    CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *    ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *    POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef OTBR_MBEDTLS_CONFIG_H_
#define OTBR_MBEDTLS_CONFIG_H_

#define MBEDTLS_CONFIG_VERSION 0x04010000

// ==============================================================================
// SSL / TLS (Mbed TLS 4.x upper layer).
// Crypto algorithms live in OpenThread's psa-crypto-config.h
// (TF_PSA_CRYPTO_CONFIG_FILE), not in this file.
//
// Version notes (gate on MBEDTLS_CONFIG_VERSION; version.h cannot be included
// from inside this config without a circular include via build_info.h):
// - MBEDTLS_SSL_EXPORT_KEYS removed in 4.x (always enabled).
// - MBEDTLS_SSL_MAX_CONTENT_LEN removed in 4.x; use IN/OUT_CONTENT_LEN.
// ==============================================================================

#define MBEDTLS_DEBUG_C

#define MBEDTLS_SSL_CLI_C
#define MBEDTLS_SSL_SRV_C
#define MBEDTLS_SSL_TLS_C
#define MBEDTLS_SSL_COOKIE_C
#define MBEDTLS_SSL_DTLS_ANTI_REPLAY
#define MBEDTLS_SSL_DTLS_HELLO_VERIFY
#if !defined(MBEDTLS_CONFIG_VERSION) || (MBEDTLS_CONFIG_VERSION < 0x04000000)
#define MBEDTLS_SSL_EXPORT_KEYS
#endif
#define MBEDTLS_SSL_KEEP_PEER_CERTIFICATE
#define MBEDTLS_SSL_MAX_FRAGMENT_LENGTH
#define MBEDTLS_SSL_PROTO_TLS1_2
#define MBEDTLS_SSL_PROTO_DTLS

// EC-JPAKE via PSA PAKE (requires PSA_WANT_ALG_JPAKE in psa-crypto-config.h).
#define MBEDTLS_KEY_EXCHANGE_ECJPAKE_ENABLED

// CoAPS PSK / ECDHE-ECDSA
#define MBEDTLS_KEY_EXCHANGE_PSK_ENABLED
#define MBEDTLS_KEY_EXCHANGE_ECDHE_ECDSA_ENABLED

#if !defined(MBEDTLS_CONFIG_VERSION) || (MBEDTLS_CONFIG_VERSION < 0x04000000)
#define MBEDTLS_SSL_MAX_CONTENT_LEN 900
#else
#define MBEDTLS_SSL_IN_CONTENT_LEN  900
#define MBEDTLS_SSL_OUT_CONTENT_LEN 900
#endif
#define MBEDTLS_SSL_CIPHERSUITES MBEDTLS_TLS_ECJPAKE_WITH_AES_128_CCM_8

// ==============================================================================
// X.509 (TLS layer). PK/PEM/BASE64 live in psa-crypto-config.h.
// ==============================================================================

#define MBEDTLS_X509_USE_C
#define MBEDTLS_X509_CRT_PARSE_C
#define MBEDTLS_X509_CRL_PARSE_C
#define MBEDTLS_X509_CSR_PARSE_C

#if defined(MBEDTLS_USER_CONFIG_FILE)
#include MBEDTLS_USER_CONFIG_FILE
#endif

#endif // OTBR_MBEDTLS_CONFIG_H_
