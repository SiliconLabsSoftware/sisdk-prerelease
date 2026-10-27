/***************************************************************************/ /**
 * @file unity_assert.h
 * @brief CPC unit test assertion declarations.
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

#ifndef CPC_UNITY_ASSERT_H
#define CPC_UNITY_ASSERT_H

#include "../sli_cpc_bus.h"

#ifdef __cplusplus
extern "C" {
#endif

void cpc_frame_pools_start_test(unsigned int lineno, const sl_cpc_bus_t *bus);
void cpc_frame_pools_end_test(unsigned int lineno, const sl_cpc_bus_t *bus);

#define CPC_FRAME_POOLS_START_TEST(bus) cpc_frame_pools_start_test(__LINE__, (bus))

#define CPC_FRAME_POOLS_END_TEST(bus) cpc_frame_pools_end_test(__LINE__, (bus))

#ifdef __cplusplus
}
#endif

#endif
