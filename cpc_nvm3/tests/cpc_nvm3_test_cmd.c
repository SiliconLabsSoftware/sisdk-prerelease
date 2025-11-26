/***************************************************************************//**
 * @file
 * @brief Test CLI commands for CPC NVM3
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

#include "ecode.h"
#include "nvm3_default.h"
#include "nvm3_generic.h"
#include "sl_sleeptimer.h"
#include "sl_status.h"
#include "sl_iostream.h"
#include "sl_cli.h"
#include "nvm3.h"
#include "sli_cpc.h"

#include <stdbool.h>
#include <string.h>

/***************************************************************************//**
 * Handle erase NVM3 CLI command.
 ******************************************************************************/
void sli_cpc_nvm3_restart_endpoint(void);

void cpc_nvm3_erase_cmd_handler(sl_cli_command_arg_t *arguments)
{
  (void)arguments;

  Ecode_t ecode;

  ecode = nvm3_eraseAll(nvm3_defaultHandle);
  if (ecode == ECODE_OK) {
    sl_iostream_printf(SL_IOSTREAM_STDOUT, "Success\n");
  } else {
    sl_iostream_printf(SL_IOSTREAM_STDOUT, "Failure\n");
  }
}

/***************************************************************************//**
 * Reset the MCU
 ******************************************************************************/
void cpc_nvm3_reset_cmd_handler(sl_cli_command_arg_t *arguments)
{
  (void)arguments;
  NVIC_SystemReset();
}

/***************************************************************************//**
 * Restart the endpoint
 ******************************************************************************/
void cpc_nvm3_restart_endpoint_cmd_handler(sl_cli_command_arg_t *arguments)
{
  (void)arguments;
  sli_cpc_nvm3_restart_endpoint();
}

/***************************************************************************//**
 * Halt the MCU in a busy loop for testing purposes
 * CLI arguments:
 *   delay_ms Size of the payload data to use during test
 ******************************************************************************/
void cpc_nvm3_halt_mcu_cmd_handler(sl_cli_command_arg_t *arguments)
{
  uint32_t delay_ms = sl_cli_get_argument_uint32(arguments, 0);
  sl_sleeptimer_delay_millisecond(delay_ms);
}
