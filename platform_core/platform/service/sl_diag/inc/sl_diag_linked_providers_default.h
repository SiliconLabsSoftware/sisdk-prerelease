/***************************************************************************//**
 * @file
 * @brief Default SL Diagnostic linked-provider list when SLC autogen is absent
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

#ifndef SL_DIAG_LINKED_PROVIDERS_DEFAULT_H
#define SL_DIAG_LINKED_PROVIDERS_DEFAULT_H

/// Fallback linked providers (CPU Crash only). SLC-generated projects use
/// sl_diag_linked_providers_autogen.h instead, which merges stack .slcc entries.
#define SLI_DIAG_LINKED_PROVIDER_LIST(X) \
  X(SL_DIAG_PROVIDER_CPU_CRASH, SLI_DIAG_CPU_CRASH_MAX_ENCODED_BYTES)

#endif // SL_DIAG_LINKED_PROVIDERS_DEFAULT_H
