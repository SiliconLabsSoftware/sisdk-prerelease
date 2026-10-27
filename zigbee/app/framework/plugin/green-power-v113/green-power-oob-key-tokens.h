/***************************************************************************//**
 * @file
 * @brief Token defines for the Green Power v1.1.3 OOB key table.
 *******************************************************************************
 * # License
 * <b>Copyright 2026 Silicon Laboratories Inc. www.silabs.com</b>
 *******************************************************************************
 *
 * The licensor of this software is Silicon Laboratories Inc. Your use of this
 * software is governed by the terms of Silicon Labs Master Software License
 * Agreement (MSLA) available at
 * www.silabs.com/about-us/legal/master-software-license-agreement. This
 * software is distributed to you in Source Code format and is governed by the
 * sections of the MSLA applicable to Source Code.
 *
 ******************************************************************************/

#if !defined(SL_CATALOG_TOKEN_MANAGER_PRESENT)
#include "sl_token_manager_api.h"
#include "sl_token_manager_defines.h"
#endif
#include "green-power-v113-config.h"
#include "green-power-v113.h"

#if (SL_ZIGBEE_AF_PLUGIN_GREEN_POWER_V113_USE_OOB_KEY_TOKENS == 1)

#define CREATOR_GP_OOB_KEY_TABLE  (0x8730)

// Indexed token: subsequent 0x7F keys are reserved (AN1135 §3.1.2).
#define NVM3KEY_GP_OOB_KEY_TABLE  (NVM3KEY_DOMAIN_ZIGBEE | 0x8930)
#define COMMON_TOKEN_GP_OOB_KEY_TABLE \
  SL_TOKEN_GET_DYNAMIC_TOKEN((SL_TOKEN_NVM3_REGION_ZIGBEE | 0x8930), 0)

// Fully-braced default for -Werror=missing-braces (union id + key.contents).
#define TOKEN_GP_OOB_KEY_TABLE_DEFAULT { false, { { { 0 } }, 0, 0 }, { { 0 } } }

#ifdef DEFINETOKENS
DEFINE_INDEXED_TOKEN(GP_OOB_KEY_TABLE,
                     sli_zigbee_gp_oob_key_entry_t,
                     SL_ZIGBEE_GP_OOB_KEY_TABLE_SIZE,
                     TOKEN_GP_OOB_KEY_TABLE_DEFAULT)
#endif // DEFINETOKENS

#endif // SL_ZIGBEE_AF_PLUGIN_GREEN_POWER_V113_USE_OOB_KEY_TOKENS
