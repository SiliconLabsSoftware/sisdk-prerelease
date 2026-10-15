/**
 * @file
 * @brief Manage ZAF version
 * @copyright 2024 Silicon Laboratories Inc.
 */

#ifndef ZAF_APPLICATIONUTILITIES_ZAF_VERSION_H_
#define ZAF_APPLICATIONUTILITIES_ZAF_VERSION_H_

#include <stdint.h>

/**
 * @brief Get the application build number.
 *
 * @deprecated Use the `APP_BUILD_NO` compile-time macro.
 */
uint16_t ZAF_GetBuildNumber(void) __attribute__((deprecated("Deprecated since SDK 8.2.0; last available in SDK 8.4.0. Use APP_BUILD_NO instead")));

uint8_t ZAF_GetVersionMajor(void);

uint8_t ZAF_GetVersionMinor(void);

uint8_t ZAF_GetVersionPatch(void);

uint8_t SDK_GetVersionMajor(void);

uint8_t SDK_GetVersionMinor(void);

uint8_t SDK_GetVersionPatch(void);

#endif /* ZAF_APPLICATIONUTILITIES_ZAF_VERSION_H_ */
