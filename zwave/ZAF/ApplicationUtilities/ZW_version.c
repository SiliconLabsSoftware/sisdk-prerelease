#include <stdint.h>
#include "zw_version_config.h"
#include "zaf_config.h"
#include "zaf_config_security.h"
#include "ZW_classcmd.h"
#include "ZW_product_infos.h"

#include <assert.h>

static const uint8_t app_product_id[16] = {
  (ZAF_CONFIG_MANUFACTURER_ID >> 8) & 0xFF, (ZAF_CONFIG_MANUFACTURER_ID >> 0) & 0xFF,
  (ZAF_CONFIG_PRODUCT_ID >> 8) & 0xFF, (ZAF_CONFIG_PRODUCT_ID >> 0) & 0xFF,
  0x00, 0x00,
  0x00, 0x00,
  (ZAF_CONFIG_PRODUCT_TYPE_ID >> 8) & 0xFF, (ZAF_CONFIG_PRODUCT_TYPE_ID >> 0) & 0xFF,
  (ZAF_CONFIG_INSTALLER_ICON_TYPE >> 8) & 0xFF, (ZAF_CONFIG_INSTALLER_ICON_TYPE >> 0) & 0xFF,
  (ZAF_CONFIG_GENERIC_TYPE >> 0) & 0xFF, (ZAF_CONFIG_SPECIFIC_TYPE >> 0) & 0xFF,
  (ZAF_CONFIG_REQUESTED_SECURITY_KEYS >> 0) & 0xFF, 0x00 // Placeholder for 16-byte array
};

uint32_t zw_product_infos_get_app_version(void)
{
  return (uint8_t)APP_VERSION << 16 | (uint8_t)APP_REVISION << 8 | (uint8_t)APP_PATCH;
}

void zw_product_infos_get_product_id(zw_version_product_id_t *product_id)
{
  assert(product_id);
  if (product_id) {
    product_id->app_manufacturer_id = ((app_product_id[0] << 8) & 0xFF00) + (app_product_id[1] & 0xFF);
    product_id->app_product_id = ((app_product_id[2] << 8) & 0xFF00) + (app_product_id[3] & 0xFF);
    product_id->app_product_type = ((app_product_id[8] << 8) & 0xFF00) + (app_product_id[9] & 0xFF);
    product_id->app_icon_type = ((app_product_id[10] << 8) & 0xFF00) + (app_product_id[11] & 0xFF);

    /* Apply Serial API Controller override: zero out generic and specific types for compatibility */
#if (ZAF_CONFIG_GENERIC_TYPE == GENERIC_TYPE_STATIC_CONTROLLER \
     && ZAF_CONFIG_SPECIFIC_TYPE == SPECIFIC_TYPE_PC_CONTROLLER)
    product_id->generic_type = 0;
    product_id->specific_type = 0;
#else
    product_id->generic_type = app_product_id[12] & 0xFF;
    product_id->specific_type = app_product_id[13] & 0xFF;
#endif
    product_id->requested_security_key = app_product_id[14] & 0xFF;
  }
}
