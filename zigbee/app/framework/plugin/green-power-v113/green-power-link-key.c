/***************************************************************************//**
 * @file
 * @brief GP v1.1.3 link key resolution for sink-side security.
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

#include "af.h"
#include "green-power-v113.h"
#include "green-power-server.h"
#include "stack/include/zigbee-security-manager.h"

#define GP_DEFAULT_LINK_KEY { 0x5A, 0x69, 0x67, 0x42, 0x65, 0x65, 0x41, 0x6C, \
                              0x6C, 0x69, 0x61, 0x6E, 0x63, 0x65, 0x30, 0x39 }

static bool sli_gp_keys_equal(const sl_zigbee_key_data_t *a, const sl_zigbee_key_data_t *b)
{
  if (a == NULL || b == NULL) {
    return false;
  }
  return (memcmp(a->contents, b->contents, SL_ZIGBEE_ENCRYPTION_KEY_SIZE) == 0);
}

sl_status_t sl_zigbee_af_gp_resolve_link_key(const sl_zigbee_gp_address_t *gpdAddr,
                                              sl_zigbee_gp_link_key_source_t preferredSource,
                                              sl_zigbee_key_data_t *linkKeyOut,
                                              sl_zigbee_gp_link_key_source_t *actualSourceOut)
{
  sl_zigbee_key_data_t gpLinkKey = { { 0 } };
  sl_zigbee_key_data_t icKey = { { 0 } };
  sl_zigbee_key_data_t tcLinkKey = { { 0 } };
  const uint8_t za09[] = GP_DEFAULT_LINK_KEY;
  sl_zigbee_af_status_t status;
  sl_zigbee_af_attribute_type_t type;

  if (linkKeyOut == NULL) {
    return SL_STATUS_NULL_POINTER;
  }

  if (actualSourceOut != NULL) {
    *actualSourceOut = SL_ZIGBEE_GP_LINK_KEY_SOURCE_DEFAULT_TCLK;
  }

  if (gpdAddr != NULL
      && preferredSource != SL_ZIGBEE_GP_LINK_KEY_SOURCE_TC_LINK
      && sl_zigbee_gp_lookup_ic_derived_key(gpdAddr, &icKey) == SL_STATUS_OK) {
    memcpy(linkKeyOut->contents, icKey.contents, SL_ZIGBEE_ENCRYPTION_KEY_SIZE);
    if (actualSourceOut != NULL) {
      *actualSourceOut = SL_ZIGBEE_GP_LINK_KEY_SOURCE_IC_DERIVED;
    }
    return SL_STATUS_OK;
  }

  status = sl_zigbee_af_read_attribute(SL_ZIGBEE_GP_ENDPOINT,
                                       ZCL_GREEN_POWER_CLUSTER_ID,
                                       ZCL_GP_SERVER_GP_LINK_KEY_ATTRIBUTE_ID,
                                       CLUSTER_MASK_SERVER,
                                       gpLinkKey.contents,
                                       SL_ZIGBEE_ENCRYPTION_KEY_SIZE,
                                       &type);
  if (status == SL_ZIGBEE_ZCL_STATUS_SUCCESS) {
    sl_zigbee_key_data_t za09Key = { { 0 } };
    memcpy(za09Key.contents, za09, SL_ZIGBEE_ENCRYPTION_KEY_SIZE);
    if (!sli_gp_keys_equal(&gpLinkKey, &za09Key)) {
      memcpy(linkKeyOut->contents, gpLinkKey.contents, SL_ZIGBEE_ENCRYPTION_KEY_SIZE);
      return SL_STATUS_OK;
    }
  }

  {
    sl_zigbee_sec_man_context_t context;
    sl_zigbee_sec_man_key_t exportedKey;
    sl_zigbee_sec_man_init_context(&context);
    context.core_key_type = SL_ZB_SEC_MAN_KEY_TYPE_TC_LINK;
    if (sl_zigbee_sec_man_export_key(&context, &exportedKey) == SL_STATUS_OK) {
      sl_zigbee_key_data_t za09Key = { { 0 } };
      memcpy(za09Key.contents, za09, SL_ZIGBEE_ENCRYPTION_KEY_SIZE);
      memcpy(tcLinkKey.contents, exportedKey.key, SL_ZIGBEE_ENCRYPTION_KEY_SIZE);
      if (!sli_gp_keys_equal(&tcLinkKey, &za09Key)) {
        memcpy(linkKeyOut->contents, tcLinkKey.contents, SL_ZIGBEE_ENCRYPTION_KEY_SIZE);
        if (actualSourceOut != NULL) {
          *actualSourceOut = SL_ZIGBEE_GP_LINK_KEY_SOURCE_TC_LINK;
        }
        return SL_STATUS_OK;
      }
    }
  }

  memcpy(linkKeyOut->contents, za09, SL_ZIGBEE_ENCRYPTION_KEY_SIZE);
  return SL_STATUS_OK;
}
