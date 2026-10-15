/*
 *  Copyright (c) 2026, The OpenThread Authors.
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
 *   This file includes the initializers for supporting OpenThread with EM4 sleep.
 *
 *   Requires Series 2 with BURTC and EM4GRPACLK sourced from ULFRCO.
 *   Only a single OpenThread instance is supported.
 */

#ifndef EM4_SLEEP_H_
#define EM4_SLEEP_H_

#include <stdbool.h>
#include <stdint.h>

#include <openthread/error.h>
#include <openthread/instance.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Initialize EM4 support (PM EM2 subscription; detect EM4 wake).
 *
 * @retval OT_ERROR_NONE         Success.
 * @retval OT_ERROR_NOT_CAPABLE  BURTC or power manager unavailable.
 */
otError sl_ot_em4_init(void);

/**
 * @retval true  Boot was a wake from EM4.
 */
bool sl_ot_em4_is_wake_from_em4(void);

/**
 * Default BURTC wake duration in milliseconds.
 */
uint32_t sl_ot_em4_get_default_wake_ms(otInstance *aInstance);

/**
 * Request EM4 when the stack is next idle. Does not enter EM4 synchronously.
 *
 * EM4 entry only occurs on the next EM2 transition after arming. If Power
 * Manager never enters EM2, EM4 will never be entered.
 *
 * Call sl_ot_em4_cancel() (or allow the pending request to complete) before
 * calling again with a different wake time; otherwise OT_ERROR_INVALID_STATE.
 *
 * @param[in] aInstance  OpenThread instance.
 * @param[in] aWakeMs    Wake duration in ms; 0 uses default from child timeout.
 */
otError sl_ot_em4_request(otInstance *aInstance, uint32_t aWakeMs);

/**
 * Cancel a pending EM4 request.
 */
void sl_ot_em4_cancel(void);

/**
 * @retval true  An EM4 request is armed.
 */
bool sl_ot_em4_is_pending(void);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // EM4_SLEEP_H_
