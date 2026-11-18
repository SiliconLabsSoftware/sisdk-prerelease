/*****************************************************************************//**
 * @file sli_token_manager_backend_nvm.c
 * @brief NVM backend operation. Invokes respective NVM read/write request
 * to the configured HAL driver.
 * @version 1.0.0
 *******************************************************************************
 * # License
 * <b>Copyright 2025 Silicon Laboratories Inc. www.silabs.com</b>
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
#include "sli_token_manager_hal_flash.h"
#include "sli_token_manager_backend_nvm.h"

sl_status_t sli_token_manager_nvm_open(uint32_t nvm_adr, uint32_t nvm_size)
{
  // Init static secure token region
  return sli_token_manager_hal_nvm_handle.open(nvm_adr, nvm_size);
}

void sli_token_manager_nvm_close(void)
{
  sli_token_manager_hal_nvm_handle.close();
}

// Odd size is not supported by this function
sl_status_t sli_token_manager_nvm_write(uint32_t nvm_adr, void const *data, uint32_t size)
{
  return sli_token_manager_hal_nvm_handle.write(nvm_adr, data, size);
}

// Odd size is not supported by this function
sl_status_t sli_token_manager_nvm_read(uint32_t nvm_adr, void *data, uint32_t size)
{
  return sli_token_manager_hal_nvm_handle.read(nvm_adr, data, size);
}

sl_status_t sli_token_manager_nvm_page_erase(uint32_t nvm_adr)
{
  return sli_token_manager_hal_nvm_handle.erase(nvm_adr);
}
