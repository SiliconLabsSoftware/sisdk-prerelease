/***************************************************************************//**
 * @file
 * @brief Silicon Labs Secure Engine Manager API.
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

#include "sl_se_manager_extmem_internal.h"

#if defined(_SILICON_LABS_32B_SERIES_3) && defined(SLI_MAILBOX_COMMAND_SUPPORTED)

#include "sli_se_manager_internal.h"
#include "sli_se_manager_mailbox.h"
#include "sli_se_manager_mailbox_internal.h"

// -----------------------------------------------------------------------------
// Global functions

#if !defined(_SILICON_LABS_32B_SERIES_3_CONFIG_301)
/***************************************************************************//**
 * Configure the Deep Power Down mode of a SPI memory.
 * SE Manager wrapper for mailbox command 0xFF43.
 ******************************************************************************/
 sl_status_t sli_se_configure_dpd_mode(sl_se_command_context_t *cmd_ctx,
  sli_se_spi_memory_instance_t spi_instance,
  sli_se_deep_power_down_mode_t mode)
{
  (void)mode;

  if ((cmd_ctx == NULL) || (spi_instance > 1) || (mode > 3)) {
    return SL_STATUS_INVALID_PARAMETER;
  }

  uint32_t command_word = SLI_SE_COMMAND_CONFIGURE_DEEP_POWER_DOWN_MODE;
  command_word |= (mode << 8);
  command_word |= spi_instance;
  sli_se_command_init(cmd_ctx, command_word);

  return sli_se_execute_and_wait(cmd_ctx);
}
#endif // !defined(_SILICON_LABS_32B_SERIES_3_CONFIG_301)

#if !defined(_SILICON_LABS_32B_SERIES_3_CONFIG_301)
/***************************************************************************//**
 *   Apply PSRAM region configuration (0xFF0A).
 ******************************************************************************/
 sl_status_t sli_se_psram_region_apply_config(sl_se_command_context_t *cmd_ctx,
                                              sli_se_psram_configs_t *regions_array)
{
  if ((cmd_ctx == NULL) || (regions_array == NULL)) {
    return SL_STATUS_INVALID_PARAMETER;
  }

  sli_se_mailbox_command_t *se_cmd = &cmd_ctx->command;
  sli_se_command_init(cmd_ctx, SLI_SE_COMMAND_APPLY_PSRAM_REGION_CONFIG);

  sli_se_datatransfer_t metadata_in =
    SLI_SE_DATATRANSFER_DEFAULT(regions_array, sizeof(sli_se_psram_configs_t));

  sli_se_mailbox_command_add_input(se_cmd, &metadata_in);

  return sli_se_execute_and_wait(cmd_ctx);
}
#endif // !defined(_SILICON_LABS_32B_SERIES_3_CONFIG_301)

#endif // defined(_SILICON_LABS_32B_SERIES_3) && defined(SLI_MAILBOX_COMMAND_SUPPORTED)
