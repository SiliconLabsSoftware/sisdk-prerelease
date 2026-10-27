/***************************************************************************//**
 * @file
 * @brief Pre-configured OOB key store for GP v1.1.3 (GP-113-12).
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

#ifdef SL_COMPONENT_CATALOG_PRESENT
#include "sl_component_catalog.h"
#endif
#ifdef SL_CATALOG_ZIGBEE_ZCL_FRAMEWORK_CORE_PRESENT
#include "app/framework/include/af.h"
#else
#include "green-power-adapter.h"
#endif

#include "green-power-v113.h"
#include "green-power-common.h"

#if (SL_ZIGBEE_AF_PLUGIN_GREEN_POWER_V113_USE_OOB_KEY_TOKENS == 1) && !defined(EZSP_HOST)
#include "green-power-oob-key-tokens.h"
#ifdef SL_CATALOG_ZIGBEE_ZCL_FRAMEWORK_CORE_PRESENT
#include "sl_zigbee_token.h"
#include "sl_token_manager_api.h"
#define sl_zigbee_gp_set_token(token, data, length) (void)slx_zigbee_token_manager_set_data(token, data, length)
#define sl_zigbee_gp_get_token(token, data, length) (void)slx_zigbee_token_manager_get_data(token, data, length)
#else
#include "green-power-token-wrapper.h"
#endif
#endif

static sli_zigbee_gp_oob_key_entry_t sli_gpOobKeyTable[SL_ZIGBEE_GP_OOB_KEY_TABLE_SIZE];

static sli_zigbee_gp_oob_key_entry_t *sli_gp_find_oob_entry(const sl_zigbee_gp_address_t *gpdAddr)
{
  for (uint8_t i = 0; i < SL_ZIGBEE_GP_OOB_KEY_TABLE_SIZE; i++) {
    if (sli_gpOobKeyTable[i].inUse
        && sl_zigbee_af_green_power_common_gp_addr_compare(gpdAddr, &sli_gpOobKeyTable[i].gpdAddr)) {
      return &sli_gpOobKeyTable[i];
    }
  }
  return NULL;
}

static sli_zigbee_gp_oob_key_entry_t *sli_gp_allocate_oob_entry(void)
{
  for (uint8_t i = 0; i < SL_ZIGBEE_GP_OOB_KEY_TABLE_SIZE; i++) {
    if (!sli_gpOobKeyTable[i].inUse) {
      return &sli_gpOobKeyTable[i];
    }
  }
  return NULL;
}

#if (SL_ZIGBEE_AF_PLUGIN_GREEN_POWER_V113_USE_OOB_KEY_TOKENS == 1) && !defined(EZSP_HOST)
static uint8_t sli_gp_oob_entry_index(const sli_zigbee_gp_oob_key_entry_t *entry)
{
  return (uint8_t)(entry - sli_gpOobKeyTable);
}

static void sli_gp_oob_persist_entry(const sli_zigbee_gp_oob_key_entry_t *entry)
{
  uint8_t index = sli_gp_oob_entry_index(entry);
  if (index < SL_ZIGBEE_GP_OOB_KEY_TABLE_SIZE) {
    sl_zigbee_gp_set_token(COMMON_TOKEN_GP_OOB_KEY_TABLE + index,
                           (void *)entry,
                           sizeof(sli_zigbee_gp_oob_key_entry_t));
  }
}
#else
static void sli_gp_oob_persist_entry(const sli_zigbee_gp_oob_key_entry_t *entry)
{
  (void)entry;
}
#endif

void sli_zigbee_af_gp_oob_key_table_init(void)
{
  memset(sli_gpOobKeyTable, 0, sizeof(sli_gpOobKeyTable));

#if (SL_ZIGBEE_AF_PLUGIN_GREEN_POWER_V113_USE_OOB_KEY_TOKENS == 1) && !defined(EZSP_HOST)
  for (uint8_t i = 0; i < SL_ZIGBEE_GP_OOB_KEY_TABLE_SIZE; i++) {
    sl_zigbee_gp_get_token(COMMON_TOKEN_GP_OOB_KEY_TABLE + i,
                           (void *)&sli_gpOobKeyTable[i],
                           sizeof(sli_zigbee_gp_oob_key_entry_t));
  }
#endif
}

sl_status_t sl_zigbee_af_gp_set_oob_key(const sl_zigbee_gp_address_t *gpdAddr,
                                        const uint8_t key[SL_ZIGBEE_ENCRYPTION_KEY_SIZE])
{
  sli_zigbee_gp_oob_key_entry_t *entry;

  if (gpdAddr == NULL || key == NULL) {
    return SL_STATUS_NULL_POINTER;
  }

  entry = sli_gp_find_oob_entry(gpdAddr);
  if (entry == NULL) {
    entry = sli_gp_allocate_oob_entry();
    if (entry == NULL) {
      return SL_STATUS_NO_MORE_RESOURCE;
    }
    memset(entry, 0, sizeof(sli_zigbee_gp_oob_key_entry_t));
    entry->gpdAddr = *gpdAddr;
    entry->inUse = true;
  }

  memcpy(entry->oobKey.contents, key, SL_ZIGBEE_ENCRYPTION_KEY_SIZE);
  sli_gp_oob_persist_entry(entry);
  return SL_STATUS_OK;
}

sl_status_t sl_zigbee_af_gp_lookup_oob_key(const sl_zigbee_gp_address_t *gpdAddr,
                                           sl_zigbee_key_data_t *keyOut)
{
  const sli_zigbee_gp_oob_key_entry_t *entry;

  if (gpdAddr == NULL || keyOut == NULL) {
    return SL_STATUS_NULL_POINTER;
  }

  entry = sli_gp_find_oob_entry(gpdAddr);
  if (entry == NULL) {
    return SL_STATUS_NOT_FOUND;
  }

  memcpy(keyOut->contents, entry->oobKey.contents, SL_ZIGBEE_ENCRYPTION_KEY_SIZE);
  return SL_STATUS_OK;
}

sl_status_t sl_zigbee_af_gp_clear_oob_key(const sl_zigbee_gp_address_t *gpdAddr)
{
  sli_zigbee_gp_oob_key_entry_t *entry;

  if (gpdAddr == NULL) {
    return SL_STATUS_NULL_POINTER;
  }

  entry = sli_gp_find_oob_entry(gpdAddr);
  if (entry == NULL) {
    return SL_STATUS_NOT_FOUND;
  }

  memset(entry, 0, sizeof(sli_zigbee_gp_oob_key_entry_t));
  sli_gp_oob_persist_entry(entry);
  return SL_STATUS_OK;
}
