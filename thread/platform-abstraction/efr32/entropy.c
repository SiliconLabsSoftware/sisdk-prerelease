/*
 *  Copyright (c) 2023, The OpenThread Authors.
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
 *   This file implements the OpenThread platform abstraction for random number generator.
 *
 */

#include <openthread-core-config.h>
#include <stddef.h>
#include <openthread/platform/entropy.h>
#include "utils/code_utils.h"

#include "security_manager.h"

#if OPENTHREAD_CONFIG_CRYPTO_LIB == OPENTHREAD_CONFIG_CRYPTO_LIB_PSA

void otPlatCryptoRandomInit(void)
{
    // Intentionally left blank, nothing to init
}

void otPlatCryptoRandomDeinit(void)
{
    // Intentionally left blank, nothing to deinit
}

otError otPlatCryptoRandomGet(uint8_t *aBuffer, uint16_t aSize)
{
    otError      error = OT_ERROR_NONE;
    psa_status_t status;

    status = sl_sec_man_get_random(aBuffer, aSize);

    otEXPECT_ACTION((status == PSA_SUCCESS), error = OT_ERROR_FAILED);

exit:
    return error;
}

#endif // OPENTHREAD_CONFIG_CRYPTO_LIB_PSA

/* Always provide otPlatEntropyGet — radio/RCP and some OT core paths
 * call it even when CRYPTO_LIB_PSA is selected (mbedtls 4.x). */
otError otPlatEntropyGet(uint8_t *aOutput, uint16_t aOutputLength)
{
    otError      error = OT_ERROR_NONE;
    psa_status_t status;

    otEXPECT_ACTION(aOutput, error = OT_ERROR_INVALID_ARGS);

    status = sl_sec_man_get_random(aOutput, aOutputLength);
    otEXPECT_ACTION((status == PSA_SUCCESS), error = OT_ERROR_FAILED);

exit:
    return error;
}
