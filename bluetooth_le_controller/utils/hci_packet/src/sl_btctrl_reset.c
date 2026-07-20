/******************************************************************************
*
* @file  sl_btctrl_reset.c
* @brief Internal helper functions for reset and getting the reset reason,
* these are called internally by the HCI layer.
*
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

#include "sl_btctrl_reset.h"
#include <sl_common.h>
#include <sl_core.h>
#include <stdbool.h>
#include <stdint.h>

SL_ALIGN(4) static uint32_t reset_custom_reason SL_ATTRIBUTE_ALIGN(4) SL_ATTRIBUTE_SECTION(".noinit");

void sl_btctrl_hci_reset(void)
{
  CORE_ResetSystem();
}

void sl_btctrl_reset_set_custom_reason(uint32_t reason)
{
  reason &= SL_BTCTRL_RESET_VALID_MASK;
  if (reason) {
    reset_custom_reason = reason | SL_BTCTRL_RESET_CUSTOM_REASON_SIGNATURE;
  }
}

uint32_t sl_btctrl_reset_get_custom_reason(void)
{
  uint32_t reason = reset_custom_reason;
  reset_custom_reason = SL_BTCTRL_RESET_CUSTOM_REASON_NA;
  if ((reason & ~SL_BTCTRL_RESET_VALID_MASK) != SL_BTCTRL_RESET_CUSTOM_REASON_SIGNATURE) {
    return SL_BTCTRL_RESET_CUSTOM_REASON_NA;
  }
  reason &= SL_BTCTRL_RESET_VALID_MASK;
  return reason;
}

void sl_btctrl_reset_clear_custom_reason(void)
{
  reset_custom_reason = SL_BTCTRL_RESET_CUSTOM_REASON_UNKNOWN;
}
