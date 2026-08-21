/**
 * @file ZW_product_infos.h
 * @copyright 2026 Silicon Laboratories Inc.
 *
 * @brief Application product identity and version accessors for stack code (e.g. QR provisioning).
 *
 * Declared in the Z-Wave API layer (not ZAF_version.h) so stack code can call them without
 * depending on ZAF application utilities. Default implementations are weak and may be
 * overridden at link time by the application (see ZW_version.c).
 */

#ifndef ZW_PRODUCT_INFOS_H_
#define ZW_PRODUCT_INFOS_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Product identity fields derived from application metadata.
 */
typedef struct {
  uint16_t app_manufacturer_id;   ///< Manufacturer ID identifiers. (MFG_ID_XXX_XXX)
  uint16_t app_product_id;        ///< Product ID. (PRODUCT_ID_XXX_XXX)
  uint16_t app_product_type;      ///< Product type Id. (PRODUCT_TYPE_ID_ZWAVE_XXX_XXX)
  uint16_t app_icon_type;         ///< Z-Wave+ Icon Type identifiers. (ICON_TYPE_XXX_XXX)
  uint8_t generic_type;           ///< Generic Device Class identifier. (GENERIC_TYPE_XXX_XXX)
  uint8_t specific_type;         ///< Specific Device Class identifier. (SPECIFIC_TYPE_XXX_XXX)
  uint8_t requested_security_key; ///< Bitmask for security keys. (SECURITY_KEY_SX_XXX)
} zw_version_product_id_t;

/**
 * @brief Fill product identity from application metadata.
 *
 * @param[out] product_id Output structure; must not be NULL.
 */
void zw_product_infos_get_product_id(zw_version_product_id_t *product_id);

/**
 * @brief Raw application version word (vendor layout; typically major/minor/patch packed).
 */
uint32_t zw_product_infos_get_app_version(void);

#ifdef __cplusplus
}
#endif

#endif /* ZW_PRODUCT_INFOS_H_ */
