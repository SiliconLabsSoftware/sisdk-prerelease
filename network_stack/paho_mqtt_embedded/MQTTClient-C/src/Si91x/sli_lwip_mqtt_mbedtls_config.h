/***************************************************************************/ /**
 * @file sli_lwip_mqtt_mbedtls_config.h
 * @brief Mbed TLS 4.x / TF-PSA-Crypto configuration for MQTT with LwIP on Silicon Labs devices.
 *******************************************************************************
 * # License
 * <b>Copyright 2025 Silicon Laboratories Inc. www.silabs.com</b>
 *******************************************************************************
 *
 * SPDX-License-Identifier: Zlib
 *
 * The licensor of this software is Silicon Laboratories Inc.
 *
 * This software is provided 'as-is', without any express or implied
 * warranty. In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 *
 ******************************************************************************/
/* ---- Mbed TLS 4.x (TLS / X.509) ---- */
#define MBEDTLS_SSL_TLS_C
#define MBEDTLS_SSL_CLI_C
#define MBEDTLS_SSL_PROTO_TLS1_2
#define MBEDTLS_SSL_KEEP_PEER_CERTIFICATE
#define MBEDTLS_SSL_SERVER_NAME_INDICATION
#define MBEDTLS_KEY_EXCHANGE_ECDHE_RSA_ENABLED
#define MBEDTLS_KEY_EXCHANGE_ECDHE_ECDSA_ENABLED
#define MBEDTLS_X509_USE_C
#define MBEDTLS_X509_CRT_PARSE_C

#ifndef MQTT_TLS_ALPN_ENABLED
#define MQTT_TLS_ALPN_ENABLED 0
#endif
#if MQTT_TLS_ALPN_ENABLED
#define MBEDTLS_SSL_ALPN
#endif

#ifdef MBEDTLS_MPI_MAX_SIZE
#undef MBEDTLS_MPI_MAX_SIZE
#endif
#define MBEDTLS_MPI_MAX_SIZE 256

#ifdef MBEDTLS_THREADING_C
#undef MBEDTLS_THREADING_C
#endif
#ifdef MBEDTLS_LMS_C
#undef MBEDTLS_LMS_C
#endif
#ifdef MBEDTLS_PSA_CRYPTO_STORAGE_C
#undef MBEDTLS_PSA_CRYPTO_STORAGE_C
#endif
