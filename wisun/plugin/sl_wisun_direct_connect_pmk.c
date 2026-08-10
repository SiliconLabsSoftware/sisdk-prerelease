/***************************************************************************//**
 * @file sl_wisun_direct_connect_pmk.c
 * @brief Direct Connect PMK provisioning
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

#include "sl_wisun_direct_connect_pmk.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include "em_system.h"
#include "psa/crypto.h"
#include "sl_status.h"
#include "sl_wisun_direct_connect_pmk_config.h"
#include "sl_wisun_trace_api.h"
#include "sl_wisun_types.h"
#include "sl_psa_crypto.h"

static const uint8_t direct_connect_pmk_zero[SL_WISUN_PMK_LEN] = { 0 };

static sl_status_t direct_connect_pmk_set_hex(const char *hex_str, uint8_t *pmk)
{
  unsigned int val;

  if (hex_str == NULL || pmk == NULL) {
    return SL_STATUS_FAIL;
  }

  if (strlen(hex_str) != ((size_t) SL_WISUN_PMK_LEN * 3U) - 1U) {
    return SL_STATUS_FAIL;
  }

  for (int i = 0; i < SL_WISUN_PMK_LEN; i++) {
    if (hex_str[2] != '\0' && hex_str[2] != ':') {
      return SL_STATUS_FAIL;
    }
    if (sscanf(hex_str, "%x", &val) != 1) {
      return SL_STATUS_FAIL;
    }
    pmk[i] = (uint8_t) val;
    hex_str += 3;
  }

  if (hex_str[-1] != '\0') {
    return SL_STATUS_FAIL;
  }

  return SL_STATUS_OK;
}

sl_status_t sl_wisun_direct_connect_pmk_import(const char *hex_str)
{
  uint8_t direct_connect_pmk[SL_WISUN_PMK_LEN];
  psa_key_attributes_t pmk_key_attributes = psa_key_attributes_init();
  psa_key_location_t pmk_location = PSA_KEY_LOCATION_LOCAL_STORAGE;
  psa_key_id_t pmk_key_id = SL_WISUN_DIRECT_CONNECT_PMK_KEY_ID;
  psa_status_t ret;

  if (direct_connect_pmk_set_hex(hex_str, direct_connect_pmk) != SL_STATUS_OK) {
    sl_wisun_trace_error("Failed to parse Direct Connect PMK");
    return SL_STATUS_FAIL;
  }

  if (memcmp(direct_connect_pmk, direct_connect_pmk_zero, SL_WISUN_PMK_LEN) == 0) {
    sl_wisun_trace_error("Direct Connect PMK is not configured");
    return SL_STATUS_FAIL;
  }

#if defined(SEMAILBOX_PRESENT)
  if (SYSTEM_GetSecurityCapability() == securityCapabilityVault) {
    pmk_location = SL_PSA_KEY_LOCATION_WRAPPED;
  }
#endif

  psa_set_key_lifetime(&pmk_key_attributes,
                       PSA_KEY_LIFETIME_FROM_PERSISTENCE_AND_LOCATION(
                         PSA_KEY_LIFETIME_VOLATILE,
                         pmk_location));

  ret = psa_destroy_key(pmk_key_id);
  if (ret == PSA_SUCCESS) {
    sl_wisun_trace_info("Replacing existing Direct Connect PMK key 0x%08" PRIx32,
                        (uint32_t) pmk_key_id);
  }

  psa_set_key_id(&pmk_key_attributes, SL_WISUN_DIRECT_CONNECT_PMK_KEY_ID);
  psa_set_key_usage_flags(&pmk_key_attributes, PSA_KEY_USAGE_SIGN_HASH);
  psa_set_key_type(&pmk_key_attributes, PSA_KEY_TYPE_HMAC);
  psa_set_key_algorithm(&pmk_key_attributes, PSA_ALG_HMAC(PSA_ALG_SHA_1));
  ret = psa_import_key(&pmk_key_attributes,
                       direct_connect_pmk,
                       SL_WISUN_PMK_LEN,
                       &pmk_key_id);
  if (ret != PSA_SUCCESS) {
    sl_wisun_trace_error("Failed to import Direct Connect PMK: %" PRIu32, ret);
    psa_reset_key_attributes(&pmk_key_attributes);
    return SL_STATUS_FAIL;
  }

  psa_reset_key_attributes(&pmk_key_attributes);

  return SL_STATUS_OK;
}

void sl_wisun_direct_connect_pmk_init(void)
{
  if (SL_WISUN_DIRECT_CONNECT_PMK[0] != '\0') {
    if (sl_wisun_direct_connect_pmk_import(SL_WISUN_DIRECT_CONNECT_PMK) != SL_STATUS_OK) {
      sl_wisun_trace_error("Failed to import configured Direct Connect PMK at init");
    } else {
      sl_wisun_trace_info("Direct Connect PMK imported");
    }
  }
}
