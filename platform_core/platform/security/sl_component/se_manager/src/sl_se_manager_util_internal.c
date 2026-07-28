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

#include "sl_se_manager_util_internal.h"

#if defined(SLI_MAILBOX_COMMAND_SUPPORTED)

#include "sli_se_manager_internal.h"
#include "sli_se_manager_mailbox.h"
#include "sli_se_manager_mailbox_internal.h"

// -----------------------------------------------------------------------------
// Internal-only API implementations
// -----------------------------------------------------------------------------
// These functions are not available in public releases. When a function is
// ready to be made public, move its implementation to sl_se_manager_util.c.

#if defined(_SILICON_LABS_32B_SERIES_3) && defined(_SYSCFG_ITCMNUMSRAMBLK_MASK)

/***************************************************************************//**
 * Configure the size of TCM regions.
 ******************************************************************************/
sl_status_t sli_se_configure_tcm(uint32_t tcm_multiplier)
{
  // Write the command using direct register access and not the abstraction
  // layer in sl_se_manager_mailbox.c to avoid code ending up in TCM.


  // Mailbox command format:
  // * header (4 bytes)
  // * command handle (4 bytes)
  // * command (4 bytes)
  // * DMA descriptors (4 bytes in and 4 bytes out)
  // * parameters (4 bytes * num parameters)

  CMU->SEMAILBOXCLKCTRL_SET = CMU_SEMAILBOXCLKCTRL_CLKEN;
  // Make sure the write to CMU is finished.
  __DSB();
  __ISB();

  // Write header (including message size) to start transaction
  SEMAILBOX_HOST->TX_HEADER = sizeof(uint32_t) * 6;

  // Command handle
  SEMAILBOX_HOST->FIFO = 0;

  // Write command into FIFO
  SEMAILBOX_HOST->FIFO = SLI_SE_COMMAND_CONFIGURE_TCM;

  // Write DMA descriptors into FIFO (no I/O except the parameter for this
  // command)
  SEMAILBOX_HOST->FIFO = (uint32_t)NULL;
  SEMAILBOX_HOST->FIFO = (uint32_t)NULL;

  // Write parameter into FIFO
  SEMAILBOX_HOST->FIFO = tcm_multiplier;

  // Command is sent, now wait for the response

  // Poll status register to wait for command completion
  while (!(SEMAILBOX_HOST->RX_STATUS & SEMAILBOX_RX_STATUS_RXINT)) { }

  // Read the response from the mailbox
  sli_se_mailbox_response_t response =
    (sli_se_mailbox_response_t)(SEMAILBOX_HOST->RX_HEADER
                                & SLI_SE_RESPONSE_MASK);

  // Read and ignore the command handle
  uint32_t command_handle = SEMAILBOX_HOST->FIFO;
  (void)command_handle;

  // Translate the (useful) response code to an sl_status_t code without using
  // sli_se_to_sl_status() to avoid code ending up in TCM.
  sl_status_t status = SL_STATUS_FAIL;
  switch (response) {
    case SLI_SE_RESPONSE_OK:
      status = SL_STATUS_OK;
      break;
    case SLI_SE_RESPONSE_INVALID_COMMAND:
      status = SL_STATUS_COMMAND_IS_INVALID;
      break;
    case SLI_SE_RESPONSE_INVALID_PARAMETER:
      status = SL_STATUS_INVALID_PARAMETER;
      break;
    default:
      // keep default value
      break;
  }

  return status;
}

#endif // defined(_SILICON_LABS_32B_SERIES_3) && defined(_SYSCFG_ITCMNUMSRAMBLK_MASK)

#endif // defined(SLI_MAILBOX_COMMAND_SUPPORTED)
