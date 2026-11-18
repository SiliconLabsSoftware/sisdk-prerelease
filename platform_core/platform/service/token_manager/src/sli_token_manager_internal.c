/***************************************************************************//**
 * @file sli_token_manager_internal.c
 * @brief Internal API implementation.
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
/***************************************************************************//**
 * Decode the token type from a 32-bit token value.
 ******************************************************************************/
#include <string.h>
#include "sli_token_manager_internal.h"
#include "sl_token_manager_defines.h"
#include "sl_token_manager_config.h"
#include "sl_token_manager_manufacturing.h"

#if defined(SL_COMMON_TOKEN_MANAGER_ENABLE_UPGRADABLE_TOKENS)
#if defined (__ICCARM__)
__root __no_init uint8_t upgradable_tokens_storage[SL_TOKEN_MANAGER_UT_DEFAULT_NVM_SIZE] @ "UPGRADABLE_TOKENS";
#elif defined (__GNUC__)
__attribute__((used)) uint8_t upgradable_tokens_storage[SL_TOKEN_MANAGER_UT_DEFAULT_NVM_SIZE] __attribute__ ((section(".upgradable_tokens")));
#endif
#endif // SL_COMMON_TOKEN_MANAGER_ENABLE_UPGRADABLE_TOKENS

#if defined(SL_COMMON_TOKEN_MANAGER_ENABLE_STATIC_TOKENS)
/**************************************************************************//**
 * Get the static device token data.
 *****************************************************************************/
sl_status_t sli_token_manager_get_token_data_preinit(uint32_t token,
                                                     void *data,
                                                     uint32_t length)
{
  sl_status_t status;
  sl_token_type_t token_type;

  if (sl_token_manager_init_success == true) {
    return SL_STATUS_NOT_AVAILABLE;
  }

  // Decode the 32-bit token identifier
  token_type = sli_token_manager_decode_token_type(token);

  // Allow static device token read
  if (token_type != SL_TOKEN_TYPE_STATIC_DEVICE_TOKEN) {
    return SL_STATUS_INVALID_PARAMETER;
  }

  // Get the token data
  status = sli_token_manager_get_static_data(token, data, 0, length);

  return status;
}
#endif

/***************************************************************************//**
 * This function identifies the token and returns token type as enum.
 *
 * @param[in] token The token identifier in 32-bit token format.
 *
 * @return sl_token_type_t token type of the token key.
 ******************************************************************************/
sl_token_type_t sli_token_manager_decode_token_type(uint32_t token)
{
  sl_token_type_t token_type = SL_TOKEN_TYPE_MAX;

  if ((token & SL_TOKEN_DYNAMIC_MASK) == SL_TOKEN_TYPE_DYNAMIC) {
    token_type = SL_TOKEN_TYPE_DYNAMIC_TOKEN;
  } else if ((token & SL_TOKEN_DYNAMIC_MASK) == SL_TOKEN_TYPE_DYNAMIC_SECONDARY) {
    token_type = SL_TOKEN_TYPE_DYNAMIC_TOKEN_SECONDARY;
  } else if ((token & SL_TOKEN_STATIC_MASK) == SL_TOKEN_TYPE_STATIC_DEVICE) {
    token_type = SL_TOKEN_TYPE_STATIC_DEVICE_TOKEN;
  } else if ((token & SL_TOKEN_STATIC_MASK) == SL_TOKEN_TYPE_STATIC_SECURE) {
    token_type = SL_TOKEN_TYPE_STATIC_SECURE_TOKEN;
  } else if ((token & SL_TOKEN_STATIC_MASK) == SL_TOKEN_TYPE_UPGRADABLE) {
    token_type = SL_TOKEN_TYPE_UPGRADABLE_TOKEN;
  } else {
    token_type = SL_TOKEN_TYPE_MAX;
  }
  return token_type;
}

#if defined(SL_COMMON_TOKEN_MANAGER_ENABLE_DYNAMIC_TOKENS)
/***************************************************************************//**
 * When NVM3_OVERRIDE is active, returns NVM3 key region value allocated
 * for manufacturing tokens.
 ******************************************************************************/
uint32_t sli_token_manager_get_nvm3_key_space_region(uint32_t token)
{
  uint32_t key_nibble = token & SL_TOKEN_STATIC_TOKEN_BASE_TYPE_KEY_MASK;

  if (key_nibble == SL_TOKEN_STATIC_DEVICE_TOKENS) {
    return SL_TOKEN_NVM3_REGION_STATIC_DEVICE_TOKEN_RANGE;
  }
  if (key_nibble == SL_TOKEN_STATIC_SECURE_DATA_TOKENS) {
    return SL_TOKEN_NVM3_REGION_STATIC_SECURE_TOKEN_RANGE;
  }
  if ((token & SL_TOKEN_STATIC_MASK) == SL_TOKEN_TYPE_STATIC_DEVICE) {
    return SL_TOKEN_NVM3_REGION_STATIC_DEVICE_TOKEN_16BIT_RANGE;
  }
  if ((token & SL_TOKEN_STATIC_MASK) == SL_TOKEN_TYPE_STATIC_SECURE) {
    return SL_TOKEN_NVM3_REGION_STATIC_SECURE_TOKEN_16BIT_RANGE;
  }
  return 0;
}
#endif // SL_COMMON_TOKEN_MANAGER_ENABLE_DYNAMIC_TOKENS
