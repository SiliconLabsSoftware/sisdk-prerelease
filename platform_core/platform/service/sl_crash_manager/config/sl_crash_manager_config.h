/***************************************************************************/ /**
* @file sl_crash_manager_config.h
* @brief Build-time configuration for the Silicon Labs Crash Manager
* @version 1.0.0
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

// <<< Use Configuration Wizard in Context Menu >>>

#ifndef SL_CRASH_MANAGER_CONFIG_H
#define SL_CRASH_MANAGER_CONFIG_H

#include <stdint.h>

#if defined(SL_COMPONENT_CATALOG_PRESENT)
#include "sl_component_catalog.h"
#endif

/* Require exactly one crash persistence backend when Crash Manager is in the project. */
#if defined(SL_COMPONENT_CATALOG_PRESENT) && defined(SL_CATALOG_CRASH_MANAGER_COMPONENT_PRESENT)
#if defined(SL_CATALOG_CRASH_MANAGER_BACKEND_RAM_PRESENT) \
  && defined(SL_CATALOG_CRASH_MANAGER_BACKEND_CTM_PRESENT)
#error "Crash Manager: select only one backend (RAM or CTM), not both."
#endif
#if !defined(SL_CATALOG_CRASH_MANAGER_BACKEND_RAM_PRESENT) \
  && !defined(SL_CATALOG_CRASH_MANAGER_BACKEND_CTM_PRESENT)
#error "Crash Manager: select exactly one backend (crash_manager_backend_ram or crash_manager_backend_ctm)."
#endif
#endif /* SL_COMPONENT_CATALOG_PRESENT && SL_CATALOG_CRASH_MANAGER_COMPONENT_PRESENT */

// <e SL_CRASH_MANAGER_DEBUG_DUMP_ENABLED> Enable debug dump (registers + stack at crash)
// <i> Logs R0-R3, R12, LR, PC, xPSR and optional stack.
// <i> Default: 0 (disabled)
#ifndef SL_CRASH_MANAGER_DEBUG_DUMP_ENABLED
#define SL_CRASH_MANAGER_DEBUG_DUMP_ENABLED  0
#endif

// <o SL_CRASH_MANAGER_DEBUG_DUMP_STACK_WORDS> Stack dump word count
// <0=> No stack dump (registers only)
// <8=> 8 words (32 bytes)
// <12=> 12 words (48 bytes)
// <16=> 16 words (64 bytes)
// <24=> 24 words (96 bytes)
// <32=> 32 words (128 bytes)
// <i> Words beyond exception frame to log. 0 = registers only. Default: 12
#ifndef SL_CRASH_MANAGER_DEBUG_DUMP_STACK_WORDS
#define SL_CRASH_MANAGER_DEBUG_DUMP_STACK_WORDS  12
#endif

#if SL_CRASH_MANAGER_DEBUG_DUMP_ENABLED
// <o SL_CRASH_MANAGER_STACK_SNAPSHOT_MAX_WORDS> Max stack words in persisted crash log
// <i> Caps NVM3/RAM log size; must be >= SL_CRASH_MANAGER_DEBUG_DUMP_STACK_WORDS. Default: 32
#ifndef SL_CRASH_MANAGER_STACK_SNAPSHOT_MAX_WORDS
#define SL_CRASH_MANAGER_STACK_SNAPSHOT_MAX_WORDS  32u
#endif

#if (SL_CRASH_MANAGER_DEBUG_DUMP_STACK_WORDS) > (SL_CRASH_MANAGER_STACK_SNAPSHOT_MAX_WORDS)
#error "SL_CRASH_MANAGER_DEBUG_DUMP_STACK_WORDS must be <= SL_CRASH_MANAGER_STACK_SNAPSHOT_MAX_WORDS"
#endif

#else /* !SL_CRASH_MANAGER_DEBUG_DUMP_ENABLED */

#define SL_CRASH_MANAGER_STACK_SNAPSHOT_MAX_WORDS  0u

#endif /* SL_CRASH_MANAGER_DEBUG_DUMP_ENABLED */

// </e>

/*
 * Derived sizes (not Configuration Wizard options): byte length of persisted stack_snapshot[]
 * from SL_CRASH_MANAGER_STACK_SNAPSHOT_MAX_WORDS above.
 */
#define SL_CRASH_MANAGER_STACK_SNAPSHOT_BYTE_MAX \
  ((unsigned)(SL_CRASH_MANAGER_STACK_SNAPSHOT_MAX_WORDS) * (unsigned)sizeof(uint32_t))

/*
 * Fixed proprietary/SystemView event fourCC tags for crash debug-dump logging (not UC options).
 * Used in sl_crash_manager_hooks.c when SL_CRASH_MANAGER_DEBUG_DUMP_ENABLED.
 */
#if SL_CRASH_MANAGER_DEBUG_DUMP_ENABLED
/** @brief Event ID: R0,R1,R2 (args: r0, r1, r2). "CRR0" = Crash Regs R0-R2. */
#define SL_LOG_EVENT_ID_CRASH_DEBUG_R012  0x43525230U
/** @brief Event ID: R3,R12,LR (args: r3, r12, lr). "CRR3" = Crash Regs R3,R12,LR. */
#define SL_LOG_EVENT_ID_CRASH_DEBUG_R3LR  0x43525233U
/** @brief Event ID: PC,xPSR (args: pc, xpsr, 0). "CRPC" = Crash PC. */
#define SL_LOG_EVENT_ID_CRASH_DEBUG_PC    0x43525043U
/** @brief Event ID: Stack pair (args: offset, word_a, word_b). "CRST" = Crash STack. */
#define SL_LOG_EVENT_ID_CRASH_DEBUG_STACK 0x43525354U
#endif

// <e SL_CRASH_MANAGER_ASSERT_HANDLER_ENABLED> Enable assert handler
// <i> Supplies assertEFM() for sl_assert (EFM_ASSERT in sl_assert.h). Defines DEBUG_EFM_USER
// <i> so sl_assert.c omits its default assertEFM; Crash Manager implements it in hooks.
// <i> Records SL_CRASH_TYPE_ASSERT, optional log, sl_crash_manager_handle_crash, then
// <i> sli_log_assert_implementation() (sl_log). Default: 0 (disabled)
#ifndef SL_CRASH_MANAGER_ASSERT_HANDLER_ENABLED
#define SL_CRASH_MANAGER_ASSERT_HANDLER_ENABLED  0
#endif

// </e>

/*
 * Assert-handler integration (not UC-editable beyond the enable flag above).
 */
#if SL_CRASH_MANAGER_ASSERT_HANDLER_ENABLED
#define DEBUG_EFM_USER
/** @brief Event ID: Assert (args: file ptr, line). "CRAT" = Crash AsserT */
#define SL_LOG_EVENT_ID_CRASH_ASSERT      0x43524154U
#endif

#endif /* SL_CRASH_MANAGER_CONFIG_H */

// <<< end of configuration section >>>
