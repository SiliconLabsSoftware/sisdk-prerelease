/***************************************************************************//**
 * @file
 * @brief Green Power Install Code utilities for GP v1.1.3.
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

#include "stack/include/gp-install-code.h"
#include "core/sl_zigbee_stack.h"
#include "stack/include/sl_zigbee_address_info.h"
#include "byte-utilities.h"

typedef struct {
  bool inUse;
  sl_zigbee_gp_address_t gpdAddr;
  sl_zigbee_key_data_t derivedKey;
} sli_zigbee_gp_ic_key_entry_t;

static sli_zigbee_gp_ic_key_entry_t sli_gpIcKeyTable[SL_ZIGBEE_GP_IC_KEY_TABLE_SIZE];

static uint16_t sli_zigbee_gp_install_code_crc16(const uint8_t *data, uint16_t length)
{
  uint16_t crc = 0xFFFFU;

  for (uint16_t i = 0; i < length; i++) {
    uint8_t byte = data[i];
    for (uint8_t bit = 0; bit < 8; bit++) {
      if (((crc ^ byte) & 0x0001U) != 0U) {
        crc = (uint16_t)((crc >> 1) ^ 0x8408U);
      } else {
        crc >>= 1;
      }
      byte >>= 1;
    }
  }
  return (uint16_t)(~crc);
}

static bool sli_zigbee_gp_gpd_addresses_match(const sl_zigbee_gp_address_t *a,
                                               const sl_zigbee_gp_address_t *b)
{
  if (a == NULL || b == NULL) {
    return false;
  }
  if (a->applicationId != b->applicationId) {
    return false;
  }
  if (a->applicationId == SL_ZIGBEE_GP_APPLICATION_IEEE_ADDRESS) {
    // 0x00 / 0xFF are wildcard endpoints (Pairing/Decomm notify uses 0xFF).
    return (memcmp(a->id.gpdIeeeAddress, b->id.gpdIeeeAddress, EUI64_SIZE) == 0
            && (a->endpoint == b->endpoint
                || a->endpoint == 0xFF
                || b->endpoint == 0xFF
                || a->endpoint == 0x00
                || b->endpoint == 0x00));
  }
  return (a->id.sourceId == b->id.sourceId);
}

static sli_zigbee_gp_ic_key_entry_t *sli_zigbee_gp_find_ic_entry(const sl_zigbee_gp_address_t *gpdAddr)
{
  for (uint8_t i = 0; i < SL_ZIGBEE_GP_IC_KEY_TABLE_SIZE; i++) {
    if (sli_gpIcKeyTable[i].inUse
        && sli_zigbee_gp_gpd_addresses_match(gpdAddr, &sli_gpIcKeyTable[i].gpdAddr)) {
      return &sli_gpIcKeyTable[i];
    }
  }
  return NULL;
}

static sli_zigbee_gp_ic_key_entry_t *sli_zigbee_gp_allocate_ic_entry(void)
{
  for (uint8_t i = 0; i < SL_ZIGBEE_GP_IC_KEY_TABLE_SIZE; i++) {
    if (!sli_gpIcKeyTable[i].inUse) {
      return &sli_gpIcKeyTable[i];
    }
  }
  return NULL;
}

sl_status_t sl_zigbee_gp_validate_install_code_crc(const uint8_t ic[SL_ZIGBEE_GP_INSTALL_CODE_SIZE])
{
  uint16_t calculatedCrc;
  uint16_t storedCrc;

  if (ic == NULL) {
    return SL_STATUS_NULL_POINTER;
  }

  calculatedCrc = sli_zigbee_gp_install_code_crc16(ic, SL_ZIGBEE_GP_INSTALL_CODE_SIZE - 2U);
  storedCrc = sl_util_fetch_low_high_int16u(ic + SL_ZIGBEE_GP_INSTALL_CODE_SIZE - 2U);

  return (calculatedCrc == storedCrc) ? SL_STATUS_OK : SL_STATUS_INVALID_PARAMETER;
}

sl_status_t sl_zigbee_gp_derive_key_from_install_code(const uint8_t ic[SL_ZIGBEE_GP_INSTALL_CODE_SIZE],
                                                       sl_zigbee_key_data_t *derivedKey)
{
  sl_status_t status;

  if (ic == NULL || derivedKey == NULL) {
    return SL_STATUS_NULL_POINTER;
  }

  status = sl_zigbee_gp_validate_install_code_crc(ic);
  if (status != SL_STATUS_OK) {
    return status;
  }

  return sli_zigbee_stack_aes_hash_simple(SL_ZIGBEE_GP_INSTALL_CODE_SIZE,
                                          ic,
                                          sl_zigbee_key_contents(derivedKey));
}

sl_status_t sl_zigbee_gp_encode_gpd_device_address(sl_zigbee_gp_application_id_t appId,
                                                    uint32_t srcId,
                                                    sl_802154_long_addr_t ieee,
                                                    sl_802154_long_addr_t deviceAddressOut)
{
  if (deviceAddressOut == NULL) {
    return SL_STATUS_NULL_POINTER;
  }

  memset(deviceAddressOut, 0, EUI64_SIZE);

  if (appId == SL_ZIGBEE_GP_APPLICATION_SOURCE_ID) {
    deviceAddressOut[0] = SL_ZIGBEE_GP_IC_SRCID_ADDR_PREFIX;
    deviceAddressOut[4] = (uint8_t)(srcId);
    deviceAddressOut[5] = (uint8_t)(srcId >> 8);
    deviceAddressOut[6] = (uint8_t)(srcId >> 16);
    deviceAddressOut[7] = (uint8_t)(srcId >> 24);
    return SL_STATUS_OK;
  }

  if (appId == SL_ZIGBEE_GP_APPLICATION_IEEE_ADDRESS) {
    memmove(deviceAddressOut, ieee, EUI64_SIZE);
    return SL_STATUS_OK;
  }

  return SL_STATUS_INVALID_PARAMETER;
}

sl_status_t sl_zigbee_gp_lookup_ic_derived_key(const sl_zigbee_gp_address_t *gpdAddr,
                                                sl_zigbee_key_data_t *keyOut)
{
  const sli_zigbee_gp_ic_key_entry_t *entry;

  if (gpdAddr == NULL || keyOut == NULL) {
    return SL_STATUS_NULL_POINTER;
  }

  entry = sli_zigbee_gp_find_ic_entry(gpdAddr);
  if (entry == NULL) {
    return SL_STATUS_NOT_FOUND;
  }

  memcpy(keyOut->contents, entry->derivedKey.contents, SL_ZIGBEE_ENCRYPTION_KEY_SIZE);
  return SL_STATUS_OK;
}

sl_status_t sl_zigbee_gp_tc_register_install_code(const sl_zigbee_gp_address_t *gpdAddr,
                                                   const uint8_t ic[SL_ZIGBEE_GP_INSTALL_CODE_SIZE])
{
  sli_zigbee_gp_ic_key_entry_t *entry;
  sl_status_t status;
  sl_zigbee_key_data_t derivedKey = { { 0 } };

  if (gpdAddr == NULL || ic == NULL) {
    return SL_STATUS_NULL_POINTER;
  }

  status = sl_zigbee_gp_derive_key_from_install_code(ic, &derivedKey);
  if (status != SL_STATUS_OK) {
    return status;
  }

  entry = sli_zigbee_gp_find_ic_entry(gpdAddr);
  if (entry == NULL) {
    entry = sli_zigbee_gp_allocate_ic_entry();
    if (entry == NULL) {
      return SL_STATUS_NO_MORE_RESOURCE;
    }
    memset(entry, 0, sizeof(sli_zigbee_gp_ic_key_entry_t));
    entry->gpdAddr = *gpdAddr;
    entry->inUse = true;
  }

  memcpy(entry->derivedKey.contents, derivedKey.contents, SL_ZIGBEE_ENCRYPTION_KEY_SIZE);
  return SL_STATUS_OK;
}

sl_status_t sl_zigbee_gp_tc_remove_install_code(const sl_zigbee_gp_address_t *gpdAddr)
{
  sli_zigbee_gp_ic_key_entry_t *entry;

  if (gpdAddr == NULL) {
    return SL_STATUS_NULL_POINTER;
  }

  entry = sli_zigbee_gp_find_ic_entry(gpdAddr);
  if (entry == NULL) {
    return SL_STATUS_NOT_FOUND;
  }

  memset(entry, 0, sizeof(sli_zigbee_gp_ic_key_entry_t));
  return SL_STATUS_OK;
}

sl_status_t sli_zigbee_gp_tc_register_gpd_install_code(sl_zigbee_gp_application_id_t appId,
                                                        uint32_t srcId,
                                                        sl_802154_long_addr_t ieee,
                                                        const uint8_t ic[SL_ZIGBEE_GP_INSTALL_CODE_SIZE])
{
  sl_zigbee_gp_address_t gpdAddr = { 0 };

  gpdAddr.applicationId = appId;
  if (appId == SL_ZIGBEE_GP_APPLICATION_SOURCE_ID) {
    gpdAddr.id.sourceId = srcId;
  } else if (appId == SL_ZIGBEE_GP_APPLICATION_IEEE_ADDRESS) {
    memmove(gpdAddr.id.gpdIeeeAddress, ieee, EUI64_SIZE);
  } else {
    return SL_STATUS_INVALID_PARAMETER;
  }

  return sl_zigbee_gp_tc_register_install_code(&gpdAddr, ic);
}
