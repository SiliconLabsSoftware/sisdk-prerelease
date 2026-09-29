/***************************************************************************/ /**
 * @file
 * @brief CPC panic.
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

#ifndef SLI_CPC_PANIC_H
#define SLI_CPC_PANIC_H

#include "sl_compiler.h"

#include "sli_cpc_log.h"

#ifdef __cplusplus
extern "C" {
#endif

__NO_RETURN void sli_cpc_panic(void);

/**
 * @brief Record a panic location then halt.
 *
 * @note The caller should not expect to return from this macro.
 */
#define SLI_CPC_PANIC(fmt, ...)                                               \
  do {                                                                        \
    __SLI_CPC_LOG(CRASH, CRASH, fmt, __FILE_NAME__, __LINE__, ##__VA_ARGS__); \
    sli_cpc_panic();                                                          \
  } while (0)

#ifdef __cplusplus
}
#endif

#endif // SLI_CPC_PANIC_H
