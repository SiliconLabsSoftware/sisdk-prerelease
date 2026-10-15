/*
 *  Copyright (c) 2016, The OpenThread Authors.
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

/**
 * @file
 *   This file implements the use of mbedTLS.
 */

#include "mbedtls.hpp"

#include <mbedtls/debug.h>
#include <mbedtls/platform.h>
#include <mbedtls/ssl.h>
#include <mbedtls/threading.h>
#if (MBEDTLS_VERSION_NUMBER < 0x04000000)
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/entropy.h>
#else
#include <psa/crypto.h>
#endif

#ifdef MBEDTLS_KEY_EXCHANGE_ECDHE_ECDSA_ENABLED
#include <mbedtls/pem.h>
#endif

#include "common/code_utils.hpp"
#include "common/error.hpp"
#include "common/heap.hpp"
#include "common/random.hpp"

namespace ot {
namespace Crypto {

MbedTls::MbedTls(void)
{
#ifdef MBEDTLS_DEBUG_C
    // mbedTLS's debug level is almost the same as OpenThread's
    mbedtls_debug_set_threshold(OPENTHREAD_CONFIG_LOG_LEVEL);
#endif
#if OPENTHREAD_CONFIG_ENABLE_BUILTIN_MBEDTLS_MANAGEMENT && !OPENTHREAD_CONFIG_HEAP_EXTERNAL_ENABLE
    mbedtls_platform_set_calloc_free(Heap::CAlloc, Heap::Free);
#endif
}

Error MbedTls::MapError(int aMbedTlsError)
{
    Error error = kErrorNone;

    switch (aMbedTlsError)
    {
#if OPENTHREAD_CONFIG_ECDSA_ENABLE
#if (MBEDTLS_VERSION_NUMBER < 0x04000000)
    case MBEDTLS_ERR_ECP_BAD_INPUT_DATA:
    case MBEDTLS_ERR_MPI_BAD_INPUT_DATA:
#endif
    case MBEDTLS_ERR_MPI_INVALID_CHARACTER:
#endif
#ifdef MBEDTLS_KEY_EXCHANGE_ECDHE_ECDSA_ENABLED
    case MBEDTLS_ERR_PK_TYPE_MISMATCH:
    case MBEDTLS_ERR_PK_FILE_IO_ERROR:
    case MBEDTLS_ERR_PK_KEY_INVALID_VERSION:
    case MBEDTLS_ERR_PK_KEY_INVALID_FORMAT:
    case MBEDTLS_ERR_PK_UNKNOWN_PK_ALG:
    case MBEDTLS_ERR_PK_PASSWORD_REQUIRED:
    case MBEDTLS_ERR_PK_PASSWORD_MISMATCH:
    case MBEDTLS_ERR_PK_INVALID_PUBKEY:
    case MBEDTLS_ERR_PK_INVALID_ALG:
    case MBEDTLS_ERR_PK_UNKNOWN_NAMED_CURVE:
#if (MBEDTLS_VERSION_NUMBER < 0x04000000)
    case MBEDTLS_ERR_PK_BAD_INPUT_DATA:
#endif
    case MBEDTLS_ERR_X509_SIG_MISMATCH:
    case MBEDTLS_ERR_X509_BAD_INPUT_DATA:
    case MBEDTLS_ERR_X509_FILE_IO_ERROR:
    case MBEDTLS_ERR_X509_CERT_UNKNOWN_FORMAT:
    case MBEDTLS_ERR_X509_INVALID_VERSION:
    case MBEDTLS_ERR_X509_UNKNOWN_SIG_ALG:
    case MBEDTLS_ERR_X509_INVALID_SERIAL:
    case MBEDTLS_ERR_X509_UNKNOWN_OID:
    case MBEDTLS_ERR_X509_INVALID_FORMAT:
    case MBEDTLS_ERR_X509_INVALID_ALG:
    case MBEDTLS_ERR_X509_INVALID_NAME:
    case MBEDTLS_ERR_X509_INVALID_DATE:
    case MBEDTLS_ERR_X509_INVALID_SIGNATURE:
    case MBEDTLS_ERR_X509_INVALID_EXTENSIONS:
    case MBEDTLS_ERR_X509_UNKNOWN_VERSION:
#endif // MBEDTLS_KEY_EXCHANGE_ECDHE_ECDSA_ENABLED
#if (MBEDTLS_VERSION_NUMBER < 0x04000000)
    case MBEDTLS_ERR_SSL_BAD_INPUT_DATA:
    case MBEDTLS_ERR_CTR_DRBG_REQUEST_TOO_BIG:
    case MBEDTLS_ERR_CTR_DRBG_INPUT_TOO_BIG:
#else
    case PSA_ERROR_INVALID_ARGUMENT:
#endif
        error = kErrorInvalidArgs;
        break;

#if (MBEDTLS_VERSION_NUMBER < 0x04000000)
#if OPENTHREAD_CONFIG_ECDSA_ENABLE
    case MBEDTLS_ERR_ECP_BUFFER_TOO_SMALL:
    case MBEDTLS_ERR_MPI_BUFFER_TOO_SMALL:
    case MBEDTLS_ERR_MPI_ALLOC_FAILED:
#endif
#ifdef MBEDTLS_KEY_EXCHANGE_ECDHE_ECDSA_ENABLED
    case MBEDTLS_ERR_PEM_ALLOC_FAILED:
    case MBEDTLS_ERR_PK_ALLOC_FAILED:
    case MBEDTLS_ERR_X509_BUFFER_TOO_SMALL:
    case MBEDTLS_ERR_X509_ALLOC_FAILED:
#endif
    case MBEDTLS_ERR_SSL_ALLOC_FAILED:
#else
    case PSA_ERROR_INSUFFICIENT_MEMORY:
    case PSA_ERROR_BUFFER_TOO_SMALL:
#endif
    case MBEDTLS_ERR_SSL_WANT_WRITE:
#if (MBEDTLS_VERSION_NUMBER < 0x04000000)
    case MBEDTLS_ERR_ENTROPY_MAX_SOURCES:
#endif
        error = kErrorNoBufs;
        break;

#ifdef MBEDTLS_KEY_EXCHANGE_ECDHE_ECDSA_ENABLED
    case MBEDTLS_ERR_PK_FEATURE_UNAVAILABLE:
#if (MBEDTLS_VERSION_NUMBER < 0x04000000)
    case MBEDTLS_ERR_PK_SIG_LEN_MISMATCH:
#endif
    case MBEDTLS_ERR_X509_FEATURE_UNAVAILABLE:
    case MBEDTLS_ERR_X509_CERT_VERIFY_FAILED:
#endif // MBEDTLS_KEY_EXCHANGE_ECDHE_ECDSA_ENABLED
#if (MBEDTLS_VERSION_NUMBER < 0x04000000)
    case MBEDTLS_ERR_CTR_DRBG_ENTROPY_SOURCE_FAILED:
    case MBEDTLS_ERR_ENTROPY_SOURCE_FAILED:
    case MBEDTLS_ERR_ENTROPY_NO_SOURCES_DEFINED:
    case MBEDTLS_ERR_ENTROPY_NO_STRONG_SOURCE:
#endif
#if (MBEDTLS_VERSION_NUMBER < 0x03000000)
    case MBEDTLS_ERR_SSL_PEER_VERIFY_FAILED:
#endif
#if (MBEDTLS_VERSION_NUMBER < 0x04000000)
    case MBEDTLS_ERR_THREADING_BAD_INPUT_DATA:
#endif
    case MBEDTLS_ERR_THREADING_MUTEX_ERROR:
        error = kErrorSecurity;
        break;

#ifdef MBEDTLS_KEY_EXCHANGE_ECDHE_ECDSA_ENABLED
    case MBEDTLS_ERR_X509_FATAL_ERROR:
        error = kErrorFailed;
        break;
#endif
    case MBEDTLS_ERR_SSL_TIMEOUT:
    case MBEDTLS_ERR_SSL_WANT_READ:
        error = kErrorBusy;
        break;

#if OPENTHREAD_CONFIG_ECDSA_ENABLE
    case MBEDTLS_ERR_ECP_FEATURE_UNAVAILABLE:
        error = kErrorNotCapable;
        break;
#endif

    default:
        if (aMbedTlsError < 0)
        {
            error = kErrorFailed;
        }

        break;
    }

    return error;
}

#if OPENTHREAD_FTD || OPENTHREAD_MTD

int MbedTls::CryptoSecurePrng(void *, unsigned char *aBuffer, size_t aSize)
{
    IgnoreError(ot::Random::Crypto::FillBuffer(aBuffer, static_cast<uint16_t>(aSize)));

    return 0;
}

#endif // OPENTHREAD_FTD || OPENTHREAD_MTD

EcJpakePassword::EcJpakePassword(void)
#if (MBEDTLS_VERSION_NUMBER >= 0x04000000) && defined(MBEDTLS_KEY_EXCHANGE_ECJPAKE_ENABLED)
    : mKeyId(MBEDTLS_SVC_KEY_ID_INIT)
#endif
{
}

void EcJpakePassword::Clear(void)
{
#if (MBEDTLS_VERSION_NUMBER >= 0x04000000) && defined(MBEDTLS_KEY_EXCHANGE_ECJPAKE_ENABLED)
    if (!mbedtls_svc_key_id_is_null(mKeyId))
    {
        psa_destroy_key(mKeyId);
        mKeyId = MBEDTLS_SVC_KEY_ID_INIT;
    }
#endif
}

int EcJpakePassword::Set(mbedtls_ssl_context &aSsl, const uint8_t *aPassword, size_t aLength)
{
#if defined(MBEDTLS_KEY_EXCHANGE_ECJPAKE_ENABLED)
#if (MBEDTLS_VERSION_NUMBER >= 0x04000000)
    psa_key_attributes_t attributes = PSA_KEY_ATTRIBUTES_INIT;
    psa_status_t         status;
    int                  rval;

    Clear();

    psa_set_key_usage_flags(&attributes, PSA_KEY_USAGE_DERIVE);
    psa_set_key_algorithm(&attributes, PSA_ALG_JPAKE(PSA_ALG_SHA_256));
    psa_set_key_type(&attributes, PSA_KEY_TYPE_PASSWORD);

    status = psa_import_key(&attributes, aPassword, aLength, &mKeyId);
    if (status != PSA_SUCCESS)
    {
        mKeyId = MBEDTLS_SVC_KEY_ID_INIT;
        return MBEDTLS_ERR_SSL_HW_ACCEL_FAILED;
    }

    rval = mbedtls_ssl_set_hs_ecjpake_password_opaque(&aSsl, mKeyId);
    if (rval != 0)
    {
        Clear();
    }

    return rval;
#else
    return mbedtls_ssl_set_hs_ecjpake_password(&aSsl, aPassword, aLength);
#endif
#else
    OT_UNUSED_VARIABLE(aSsl);
    OT_UNUSED_VARIABLE(aPassword);
    OT_UNUSED_VARIABLE(aLength);

    return MBEDTLS_ERR_SSL_FEATURE_UNAVAILABLE;
#endif
}

} // namespace Crypto
} // namespace ot
