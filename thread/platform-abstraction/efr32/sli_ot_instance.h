/*******************************************************************************
 * @file
 * @brief Internal helper for OpenThread instance lifecycle checks.
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

#ifndef SLI_OT_INSTANCE_H
#define SLI_OT_INSTANCE_H

#include <stdbool.h>
#include <openthread/instance.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Indicates whether @p aInstance has completed initialization and has not been
 * finalized.
 *
 * @param[in] aInstance  A pointer to an OpenThread instance, or NULL.
 *
 * @retval true   @p aInstance is non-NULL and initialized.
 * @retval false  @p aInstance is NULL, not yet initialized, or finalized.
 */
bool sli_ot_instance_is_initialized(const otInstance *aInstance);

#ifdef __cplusplus
}
#endif

#endif // SLI_OT_INSTANCE_H
