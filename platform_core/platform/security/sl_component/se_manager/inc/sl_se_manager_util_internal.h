/***************************************************************************//**
 * @file
 * @brief Silicon Labs Secure Engine Manager API - Internal Extensions
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
#ifndef SL_SE_MANAGER_UTIL_INTERNAL_H
#define SL_SE_MANAGER_UTIL_INTERNAL_H

#include "sl_se_manager_types.h"
#include "sli_se_manager_features.h"
#include "sl_status.h"

#ifdef __cplusplus
extern "C" {
#endif

// -----------------------------------------------------------------------------
// Internal-only API
// -----------------------------------------------------------------------------
// These functions are not available in public releases. When a function is
// ready to be made public, move its declaration to sl_se_manager_util.h.

#if defined(SLI_MAILBOX_COMMAND_SUPPORTED)

#if defined(_SILICON_LABS_32B_SERIES_3) && defined(_SYSCFG_ITCMNUMSRAMBLK_MASK)

/***************************************************************************//**
 * @brief
 *   Configure the size of TCM regions.
 *
 * @details
 *   This function configures the TCM (Tightly Coupled Memory) region sizes.
 *   This is an internal-only feature not available in public releases.
 *   To avoid a situation where this function lands in ITCM memory, the function
 *   uses direct register access and not the established abstraction layers for
 *   the SE mailbox. This also means we do not need a commmand context for this
 *   function.
 *
 * @param[in] tcm_multiplier
 *   TCM size multiplier value. Valid range is 2-8 inclusive.
 *
 *   TCM is incremented in chuncks of 64kb. The multiplier
 *   value represents the size of the TCM area e.g. 2 = 128kb
 *   and 8 = 512kb
 *
 * @return
 *   SL_STATUS_OK when the function executed successfully, else, a status code
 *   of type @ref sl_status_t that indicates why the function was not successful
 ******************************************************************************/
sl_status_t sli_se_configure_tcm(uint32_t tcm_multiplier);

#endif // defined(_SILICON_LABS_32B_SERIES_3) && defined(_SYSCFG_ITCMNUMSRAMBLK_MASK)

#endif // defined(SLI_MAILBOX_COMMAND_SUPPORTED)

#ifdef __cplusplus
}
#endif

#endif // SL_SE_MANAGER_UTIL_INTERNAL_H
