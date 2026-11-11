/**
 * @file
 *
 * ZAF PrintAppInfo module source file
 * @copyright 2023 Silicon Laboratories Inc.
 */

#include "ZAF_PrintAppInfo.h"
#include "ZAF_AppName.h"
#include "ZAF_version.h"
#include "zw_version_config.h"
#include "zpal_log.h"
#if ZPAL_LOG_BACKEND_COMPACT
#include <stdint.h>

static uint32_t app_name_hash(const char *app_name)
{
  uint32_t hash = 2166136261u;

  while (*app_name != '\0') {
    hash ^= (uint8_t)*app_name++;
    hash *= 16777619u;
  }
  return hash;
}
#endif

void ZAF_PrintAppInfo(void)
{
  char app_name[ZAF_FILE_SIZE_APP_NAME];
  ZAF_AppName_Read(app_name);
  ZPAL_LOG_INFO(ZPAL_LOG_APP,
                "\n\n----------------------------------\n");
#if !ZPAL_LOG_BACKEND_COMPACT
  ZPAL_LOG_INFO(ZPAL_LOG_APP,
                "Z-Wave App: %s \n", app_name);
#else
  /*
   * Compact records cannot reconstruct a runtime string from the ELF. Keep a
   * stable identifier for the app while retaining the full name in formatted
   * builds (and in builds without an sl_log backend, such as x86).
   */
  ZPAL_LOG_INFO(ZPAL_LOG_APP,
                "Z-Wave App hash: 0x%08x\n", app_name_hash(app_name));
#endif
  ZPAL_LOG_INFO(ZPAL_LOG_APP,
                "SDK: %d.%d.%d ZAF: %d.%d.%d.%d\n",
                SDK_GetVersionMajor(), SDK_GetVersionMinor(), SDK_GetVersionPatch(),
                ZAF_GetVersionMajor(), ZAF_GetVersionMinor(), ZAF_GetVersionPatch(),
                APP_BUILD_NO);
}
