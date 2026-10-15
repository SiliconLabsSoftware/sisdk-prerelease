/***************************************************************************//**
 * @file
 * @brief Compatibility implementations for APIs deprecated in SDK 8.2.0.
 * @copyright 2026 Silicon Laboratories Inc.
 */

#include <stdint.h>

#include "ZAF_version.h"
#include "ZW_product_infos.h"
#include "zpal_misc.h"
#include "zw_version_config.h"

uint16_t ZAF_GetBuildNumber(void)
{
  return (uint16_t)APP_BUILD_NO;
}

uint32_t zpal_get_app_version(void)
{
  return zw_product_infos_get_app_version();
}

uint8_t zpal_get_app_version_major(void)
{
  return (uint8_t)(zw_product_infos_get_app_version() >> 16);
}

uint8_t zpal_get_app_version_minor(void)
{
  return (uint8_t)(zw_product_infos_get_app_version() >> 8);
}

uint8_t zpal_get_app_version_patch(void)
{
  return (uint8_t)zw_product_infos_get_app_version();
}

void zpal_get_product_id(zpal_product_id_t *product_id)
{
  zw_version_product_id_t current_product_id;

  zw_product_infos_get_product_id(&current_product_id);
  product_id->app_manufacturer_id = current_product_id.app_manufacturer_id;
  product_id->app_product_id = current_product_id.app_product_id;
  product_id->app_product_type = current_product_id.app_product_type;
  product_id->app_icon_type = current_product_id.app_icon_type;
  product_id->generic_type = current_product_id.generic_type;
  product_id->specyfic_type = current_product_id.specific_type;
  product_id->requested_security_key = current_product_id.requested_security_key;
}
