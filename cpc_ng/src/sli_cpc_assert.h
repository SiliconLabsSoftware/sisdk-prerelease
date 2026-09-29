/***************************************************************************/ /**
 * @file
 * @brief CPC assert.
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

#ifndef SLI_CPC_ASSERT_H
#define SLI_CPC_ASSERT_H

#include "sl_assert.h"

#include "sli_cpc_panic.h"

#ifdef __cplusplus
extern "C" {
#endif

/******************************************************************************/
/*                              Assertion Macros                              */
/******************************************************************************/

/**
 * @brief Assert that an expression is true, halting the current execution otherwise.
 *
 * @param condition Boolean expression that must be true under normal circumstances.
 *
 * @note The caller should not expect to return from this macro if the assertion fails.
 * @note Users can override this macro by defining SLI_CPC_ASSERT.
 */

#if !defined(DEBUG_EFM) && !defined(DEBUG_EFM_USER)
#define SLI_CPC_ASSERT(condition) (void)(condition)
#else
#define SLI_CPC_ASSERT(condition) sli_cpc_assert((condition) != 0, __FILE__, __LINE__)

__STATIC_FORCEINLINE void sli_cpc_assert(int condition, const char *file, int line)
{
  if (!condition) {
    assertEFM(file, line);
    sli_cpc_panic();
  }
}
#endif

#ifdef __cplusplus
}
#endif

#endif // SLI_CPC_ASSERT_H
