/**
 * @file
 * @brief CPC log.
 *******************************************************************************
 * # License
 * <b>Copyright 2026 Silicon Laboratories Inc. www.silabs.com</b>
 *******************************************************************************
 *
 * SPDX-License-Identifier: Zlib
 *
 * The licensor of this software is Silicon Laboratories Inc.
 *
 * This software is provided 'as-is', without any express or implied
 * warranty. In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 *
 */

#ifndef SLI_CPC_LOG_H
#define SLI_CPC_LOG_H

#include <stdio.h>

#include "sl_common.h"

#if defined(SL_COMPONENT_CATALOG_PRESENT)
#include "sl_component_catalog.h"
#endif

#include "sl_cpc_config.h"

#if defined(SL_CATALOG_LOG_COMPONENT_PRESENT)
#include "sl_log_helper.h"
#if (SL_CPC_LOG_LEVEL < SL_LOG_CONFIG_LEVEL_COMPILE_TIME)
#warning \
  "SL_CPC_LOG_LEVEL is finer than SL_LOG_CONFIG_LEVEL_COMPILE_TIME: " \
  "log messages below the compile-time threshold will be silently dropped."
#endif
#else
// Mirror the logger component's level nomenclature so SL_CPC_LOG_LEVEL can use
// the same constants regardless of whether the logger component is present.
#define SL_LOG_CONFIG_LEVEL_DEBUG 1
#define SL_LOG_CONFIG_LEVEL_INFO 2
#define SL_LOG_CONFIG_LEVEL_WARN 3
#define SL_LOG_CONFIG_LEVEL_ERROR 4
#define SL_LOG_CONFIG_LEVEL_CRASH 5
#define SL_LOG_CONFIG_LEVEL_NONE 6
#endif

#ifdef __cplusplus
extern "C" {
#endif

/******************************************************************************/
/*                               Logging Macro                                */
/******************************************************************************/

#if (SL_CPC_LOG_LEVEL != SL_LOG_CONFIG_LEVEL_NONE)

#if defined(SL_CATALOG_LOG_COMPONENT_PRESENT)

#define __SLI_CPC_LOG(level, level_config, fmt, filename, line, ...)                     \
  do {                                                                                   \
    if (SL_LOG_CONFIG_LEVEL_##level_config >= SL_CPC_LOG_LEVEL) {                        \
      sl_printf_common(level, "[" filename ":" STRINGIZE(line) "] " fmt, ##__VA_ARGS__); \
      if (SL_LOG_CONFIG_LEVEL_##level_config == SL_LOG_CONFIG_LEVEL_CRASH) {             \
        sl_log_flush();                                                                  \
      }                                                                                  \
    }                                                                                    \
  } while (0)

#else // No logger component

#define __SLI_CPC_LOG(level, level_config, fmt, filename, line, ...)                 \
  do {                                                                               \
    if (SL_LOG_CONFIG_LEVEL_##level_config >= SL_CPC_LOG_LEVEL) {                    \
      printf(#level " [" filename ":" STRINGIZE(line) "] " fmt "\n", ##__VA_ARGS__); \
    }                                                                                \
  } while (0)

#endif

#else // Logs disabled

#define __SLI_CPC_LOG(level, level_config, fmt, filename, line, ...) \
  do {                                                               \
    (void)(fmt);                                                     \
    (void)(filename);                                                \
    (void)(line);                                                    \
  } while (0)

#endif // SL_CPC_LOG_LEVEL

#define SLI_CPC_LOG_ERROR(fmt, ...) __SLI_CPC_LOG(ERR, ERROR, fmt, __FILE_NAME__, __LINE__, ##__VA_ARGS__)
#define SLI_CPC_LOG_WARN(fmt, ...) __SLI_CPC_LOG(WRN, WARN, fmt, __FILE_NAME__, __LINE__, ##__VA_ARGS__)
#define SLI_CPC_LOG_INFO(fmt, ...) __SLI_CPC_LOG(INFO, INFO, fmt, __FILE_NAME__, __LINE__, ##__VA_ARGS__)
#define SLI_CPC_LOG_DEBUG(fmt, ...) __SLI_CPC_LOG(DBG, DEBUG, fmt, __FILE_NAME__, __LINE__, ##__VA_ARGS__)

#ifdef __cplusplus
}
#endif

#endif // SLI_CPC_LOG_H
