/*******************************************************************************
 * @file cpc_atomic.h
 * @brief Atomic Intrinsics for EFR32 Series.
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

#ifndef CPC_ATOMIC_H
#define CPC_ATOMIC_H

#include "sl_atomic.h"
#include "sl_core.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef MCU_DECLARE_IRQ_STATE
#define MCU_DECLARE_IRQ_STATE CORE_DECLARE_IRQ_STATE
#endif

#ifndef MCU_ENTER_CRITICAL
#define MCU_ENTER_CRITICAL CORE_ENTER_CRITICAL
#endif

#ifndef MCU_EXIT_CRITICAL
#define MCU_EXIT_CRITICAL CORE_EXIT_CRITICAL
#endif

#ifndef MCU_ENTER_ATOMIC
#define MCU_ENTER_ATOMIC CORE_ENTER_ATOMIC
#endif

#ifndef MCU_EXIT_ATOMIC
#define MCU_EXIT_ATOMIC CORE_EXIT_ATOMIC
#endif

#ifndef MCU_ATOMIC_LOAD
#define MCU_ATOMIC_LOAD sl_atomic_load
#endif

#ifndef MCU_ATOMIC_STORE
#define MCU_ATOMIC_STORE sl_atomic_store
#endif

#ifdef __cplusplus
}
#endif

#endif // CPC_ATOMIC_H
