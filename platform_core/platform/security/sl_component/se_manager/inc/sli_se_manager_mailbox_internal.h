/***************************************************************************//**
 * @file
 * @brief SE Mailbox API - Internal Extensions
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
#ifndef SLI_SE_MANAGER_MAILBOX_INTERNAL_H
#define SLI_SE_MANAGER_MAILBOX_INTERNAL_H

// -----------------------------------------------------------------------------
// Internal-only SE command definitions
// -----------------------------------------------------------------------------
// These commands are not available in public releases. When a command is
// ready to be made public, move its definition to sli_se_manager_mailbox.h.

#if defined(SLI_SE_MAILBOX_HOST_SYSTEM) || defined(SEMAILBOX_PRESENT) || defined(CRYPTOACC_PRESENT)

#ifdef __cplusplus
extern "C" {
#endif

/*******************************************************************************
 ******************************   DEFINES    ***********************************
 ******************************************************************************/

// -----------------------------------------------------------------------------
// SE command words
// Commands are grouped based on availability

#if defined(_SILICON_LABS_32B_SERIES_3)
  #define SLI_SE_COMMAND_CONFIGURE_TCM                    0x43500000UL
  #define SLI_SE_COMMAND_APPLY_PSRAM_REGION_CONFIG        0xFF0A0000UL
  #define SLI_SE_COMMAND_CONFIGURE_DEEP_POWER_DOWN_MODE   0xFF430000UL
#endif

#ifdef __cplusplus
}
#endif

#endif // #if defined(SLI_SE_MAILBOX_HOST_SYSTEM) || defined(SEMAILBOX_PRESENT) || defined(CRYPTOACC_PRESENT)

#endif /* SLI_SE_MANAGER_MAILBOX_INTERNAL_H */
