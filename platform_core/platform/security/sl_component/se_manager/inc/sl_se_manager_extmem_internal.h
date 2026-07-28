/***************************************************************************//**
 * @file
 * @brief Silicon Labs Secure Engine Manager API.
 *******************************************************************************
 * # License
 * <b>Copyright 2023 Silicon Laboratories Inc. www.silabs.com</b>
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
#ifndef SL_SE_MANAGER_EXTMEM_INTERNAL_H
#define SL_SE_MANAGER_EXTMEM_INTERNAL_H

#include "sl_se_manager_types.h"
#include "sl_se_manager_extmem.h"
#include "sl_status.h"
#include "sl_enum.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#if defined(_SILICON_LABS_32B_SERIES_3) && defined(SLI_MAILBOX_COMMAND_SUPPORTED)

/// @addtogroup sl_se_manager
/// @{

/***************************************************************************//**
 * @addtogroup sl_se_manager_extmem External memory support
 *
 * @brief
 *   API for managing and accessing the external memory.
 *
 * @details
 *   The API for the external memory includes functions for managing
 *   external flash memory on Series-3 devices. The exerrnal flash
 *   memory is partitioned in a number of Code Regions and one Data region.
 *   The number of Code Regions is defined by @ref SL_SE_MAX_CODE_REGIONS and
 *   varies on different devices.
 *   The APIs include functions for
 *     - Code region configuration, erase and write
 *     - Data region get location, erase and write
 *
 * @{
 ******************************************************************************/

/// @addtogroup sl_se_memory_region_utils Memory Region commands
/// @{

/// PSRAM region configuration

#define SLI_SE_PSRAM_CONFIG_SIZE_MASK     0x01FFU   // Size in increments of 32KB.
#define SLI_SE_PSRAM_CONFIG_ENCRYPT_MASK  0x0300U   // 0: plaintext, 1: encrypted (EXiP).
#define SLI_SE_PSRAM_CONFIG_LOCK_MASK     0x8000U   // 0: unlocked, 1: locked (write protected).
#define SLI_SE_PSRAM_CONFIG_SIZE_SHIFT    0
#define SLI_SE_PSRAM_CONFIG_ENCRYPT_SHIFT 12
#define SLI_SE_PSRAM_CONFIG_LOCK_SHIFT    15

typedef uint16_t sli_se_psram_configs_t[9]; // 9 regions, each region is a 16-bit configuration word.

/// Deep Power Down mode.
SL_ENUM( sli_se_deep_power_down_mode_t) {
  SLI_SE_DEEP_POWER_DOWN_MODE_ENABLE = 0,  ///< Enable auto Deep Power Down mode.
  SLI_SE_DEEP_POWER_DOWN_MODE_DISABLE = 1, ///< Disable auto Deep Power Down mode.
  SLI_SE_DEEP_POWER_DOWN_MODE_ENTER = 2,   ///< Enter Deep Power Down mode.
  SLI_SE_DEEP_POWER_DOWN_MODE_EXIT = 3,    ///< Exit Deep Power Down mode.
};

/// @} (end addtogroup sl_se_memory_region_utils)

// -----------------------------------------------------------------------------
// Prototypes

/// @addtogroup sl_se_memory_region_utils
/// @{

#if !defined(_SILICON_LABS_32B_SERIES_3_CONFIG_301)
/***************************************************************************//**
 * @brief
 *   Configure the Deep Power Down mode of a SPI memory.
 *
 * @param[in] cmd_ctx
 *   Pointer to an SE command context object.
 * @param[in] spi_instance
 *   SPI instance number.
 * @param[in] mode
 *   Deep Power Down mode, one of \ref sli_se_deep_power_down_mode_t
 *
 * @return SL_STATUS_OK if the operation is successful, or error code
 *         defined in sl_status.h.
 ******************************************************************************/
 sl_status_t sli_se_configure_dpd_mode(sl_se_command_context_t *cmd_ctx,
                                       sli_se_spi_memory_instance_t spi_instance,
                                       sli_se_deep_power_down_mode_t mode);
#endif

#if !defined(_SILICON_LABS_32B_SERIES_3_CONFIG_301)
/***************************************************************************//**
 * @brief
 *   Apply PSRAM region configuration
 *
 * @details
 *   This function applies region settings for external PSRAM memory
 *   attached to OSPI1.
 *
 * @param[in] cmd_ctx
 *   Pointer to an SE command context object.
 * @param[in] region_config
 *   Pointer to array of region configuration elements.
 *
 * @return
 *   SL_STATUS_OK when the function was successfully, or else, a status code
 *   of type sl_status_t that indicates why the command was not successful,
 *   @file sl_status.h.
 ******************************************************************************/
 sl_status_t sli_se_psram_region_apply_config(sl_se_command_context_t *cmd_ctx,
                                              sli_se_psram_configs_t *regions_array);
#endif

/// @} (end addtogroup sl_se_memory_region_utils)
/// @} (end addtogroup sl_se_manager_extmem)
/// @} (end addtogroup sl_se_manager)

#endif // defined(_SILICON_LABS_32B_SERIES_3) && defined(SLI_MAILBOX_COMMAND_SUPPORTED)

#ifdef __cplusplus
}
#endif

#endif // SL_SE_MANAGER_EXTMEM_INTERNAL_H
