/*******************************************************************************
 * @file
 * @brief OpenThread Sub-GHz configuration file.
 *******************************************************************************
 * # License
 * <b>Copyright 2024 Silicon Laboratories Inc. www.silabs.com</b>
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

#ifndef _SL_OPENTHREAD_SUBGHZ_CONFIG_H
#define _SL_OPENTHREAD_SUBGHZ_CONFIG_H

//-------- <<< Use Configuration Wizard in Context Menu >>> -----------------
//
// Installed with the ot_duty_cycle_manager component. The window length is
// fixed. The bucket length and the maximum duty, as a percent of that hour,
// are the Studio-configurable values.
//
// <h> TX Duty-Cycle Accounting
// <o.1 SL_OT_DUTY_CYCLE_MAX_PERCENT_DEFAULT> Maximum TX duty [% of the hour] <0.1-100.0:0.1>
// <i> Default: 2.8. The accountant stores this as permille (percent times 10), so 2.8% is 28.
// <o SL_OT_RADIO_TX_TIME_BUCKET_SEC_DEFAULT> Accounting bucket [s] <1..3600:1>
// <i> Granularity at which the rolling window advances (default: 60, 1 minute).
// <i> The window length is fixed at 3600 s. This value must divide 3600 evenly.
// <i> Smaller values track the window more closely at the cost of RAM and
// <i> more frequent timer wakeups.
#ifndef SL_OT_DUTY_CYCLE_MAX_PERCENT_DEFAULT
#define SL_OT_DUTY_CYCLE_MAX_PERCENT_DEFAULT 2.8
#endif
#ifndef SL_OT_RADIO_TX_TIME_BUCKET_SEC_DEFAULT
#define SL_OT_RADIO_TX_TIME_BUCKET_SEC_DEFAULT (60u)
#endif
// </h>

static_assert(SL_OT_RADIO_TX_TIME_BUCKET_SEC_DEFAULT > 0
    && (SL_OT_RADIO_TX_TIME_WINDOW_SEC_DEFAULT % SL_OT_RADIO_TX_TIME_BUCKET_SEC_DEFAULT) == 0,
"SL_OT_RADIO_TX_TIME_WINDOW_SEC_DEFAULT must be divisible by SL_OT_RADIO_TX_TIME_BUCKET_SEC_DEFAULT");

// Number of buckets included in the rolling window.
#ifndef SL_OT_RADIO_TX_TIME_BUCKETS_IN_WINDOW_DEFAULT
#define SL_OT_RADIO_TX_TIME_BUCKETS_IN_WINDOW_DEFAULT \
    (SL_OT_RADIO_TX_TIME_WINDOW_SEC_DEFAULT / SL_OT_RADIO_TX_TIME_BUCKET_SEC_DEFAULT)
#endif

// Ring size: rolling-window buckets plus one spare slot for rotation.
#ifndef SL_OT_RADIO_TX_TIME_BUCKET_COUNT_DEFAULT
#define SL_OT_RADIO_TX_TIME_BUCKET_COUNT_DEFAULT \
    (SL_OT_RADIO_TX_TIME_BUCKETS_IN_WINDOW_DEFAULT + 1u)
#endif

// Rolling-window duration in microseconds.
#ifndef SL_OT_RADIO_TX_TIME_WINDOW_DURATION_US_DEFAULT
#define SL_OT_RADIO_TX_TIME_WINDOW_DURATION_US_DEFAULT \
    (SL_OT_RADIO_TX_TIME_WINDOW_SEC_DEFAULT * 1000000u)
#endif

// <<< end of configuration section >>>
#endif // _SL_OPENTHREAD_SUBGHZ_CONFIG_H
