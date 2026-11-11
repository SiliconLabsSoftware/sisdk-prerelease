/***************************************************************************//**
 * @file
 * @brief SL Diagnostic configuration
 *******************************************************************************
 * # License
 * <b>Copyright 2026 Silicon Laboratories Inc. www.silabs.com</b>
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

#ifndef SL_DIAG_CONFIG_H
#define SL_DIAG_CONFIG_H

// Internal Defines: DO NOT MODIFY

// Diagnostic profile values
#define SL_DIAG_PROFILE_OFF                 0
#define SL_DIAG_PROFILE_CRASH_BASIC         1
#define SL_DIAG_PROFILE_CRASH_STANDARD      2
#define SL_DIAG_PROFILE_CRASH_WITH_SUMMARY  3
#define SL_DIAG_PROFILE_FULL_TEMPLATE       4

// Privacy class values
#define SL_DIAG_PRIVACY_PUBLIC       0u
#define SL_DIAG_PRIVACY_OPERATIONAL  1u
#define SL_DIAG_PRIVACY_SENSITIVE    2u
#define SL_DIAG_PRIVACY_SECRET       3u

// <<< Use Configuration Wizard in Context Menu >>>

// <h> SL Diagnostic Configuration

// <o SL_DIAG_PROFILE> Diagnostic profile
// <i> Selects the diagnostic features built into the application.
// <i> Off: no retained region and no API. Only two empty init functions are
// <i> built, so the RAM cost is the same as without the component.
// <i> Crash Basic: CPU crash record.
// <i> Crash Standard: Crash Basic plus Trigger, Reset/Boot and Build metadata.
// <i> Crash with Summary: Crash Standard plus the Summary Ring.
// <i> Full Template: all features, including non-crash capture.
// <i> Default: SL_DIAG_PROFILE_OFF
// <SL_DIAG_PROFILE_OFF=> Off
// <SL_DIAG_PROFILE_CRASH_BASIC=> Crash Basic
// <SL_DIAG_PROFILE_CRASH_STANDARD=> Crash Standard
// <SL_DIAG_PROFILE_CRASH_WITH_SUMMARY=> Crash with Summary
// <SL_DIAG_PROFILE_FULL_TEMPLATE=> Full Template
#define SL_DIAG_PROFILE  SL_DIAG_PROFILE_OFF

// <o SL_DIAG_SLOT_COUNT> Number of record slots
// <i> Two slots enable copy-on-commit.
// <i> Default: 1
// <1=> 1
// <2=> 2
#define SL_DIAG_SLOT_COUNT  1

// <q SL_DIAG_EMERGENCY_ENABLE> Enable Mini Emergency Tail
// <i> Keeps minimal crash evidence if the record commit does not complete.
// <i> Default: 0
#define SL_DIAG_EMERGENCY_ENABLE  0

// <o SL_DIAG_STACK_SNAPSHOT_BYTES> CPU crash stack snapshot size (bytes) <0-256:4>
// <i> 0 disables the stack snapshot. Must be a multiple of 4.
// <i> Each byte is added to the crash scratch and to every record slot.
// <i> Default: 0
#define SL_DIAG_STACK_SNAPSHOT_BYTES  0

// <o SL_DIAG_CAPTURE_TIME_BUDGET_US> Capture time budget (us) <1-5000>
// <i> Provider time budget for fault, assert and non-crash captures.
// <i> Reference default is 5000 us. Crash captures run with interrupts masked and
// <i> the hardware watchdog still counting; values above 5000 us are not supported.
// <i> Default: 5000
#define SL_DIAG_CAPTURE_TIME_BUDGET_US  5000

// <o SL_DIAG_MAX_PRIVACY_CLASS> Maximum provider privacy class
// <i> Providers above this class are skipped and reported as REDACTED.
// <i> Default: SL_DIAG_PRIVACY_OPERATIONAL
// <SL_DIAG_PRIVACY_PUBLIC=> Public
// <SL_DIAG_PRIVACY_OPERATIONAL=> Operational
// <SL_DIAG_PRIVACY_SENSITIVE=> Sensitive
// <SL_DIAG_PRIVACY_SECRET=> Secret
#define SL_DIAG_MAX_PRIVACY_CLASS  SL_DIAG_PRIVACY_OPERATIONAL

// </h>

#if ((SL_DIAG_PROFILE < SL_DIAG_PROFILE_OFF) || (SL_DIAG_PROFILE > SL_DIAG_PROFILE_FULL_TEMPLATE))
#error "Invalid SL_DIAG_PROFILE"
#endif

#if ((SL_DIAG_SLOT_COUNT != 1u) && (SL_DIAG_SLOT_COUNT != 2u))
#error "SL_DIAG_SLOT_COUNT must be 1 or 2"
#endif

#if ((SL_DIAG_MAX_PRIVACY_CLASS < SL_DIAG_PRIVACY_PUBLIC) \
  || (SL_DIAG_MAX_PRIVACY_CLASS > SL_DIAG_PRIVACY_SECRET))
#error "Invalid SL_DIAG_MAX_PRIVACY_CLASS"
#endif

#endif // SL_DIAG_CONFIG_H

// <<< end of configuration section >>>
