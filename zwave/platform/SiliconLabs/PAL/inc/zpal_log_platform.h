/**
 * @file
 * @brief Silicon Labs platform selection for Z-Wave PAL logging.
 *
 * @copyright 2025 Silicon Laboratories Inc.
 */

#ifndef ZPAL_LOG_PLATFORM_H_
#define ZPAL_LOG_PLATFORM_H_

#if defined(SL_COMPONENT_CATALOG_PRESENT)
#include "sl_component_catalog.h"
#endif

#if defined(SL_CATALOG_ZW_LOG_PRESENT)
#define ZPAL_LOG_PLATFORM_LOG_AVAILABLE 1

#if defined(SL_CATALOG_LOG_BACKEND_PROPRIETARY_PRESENT) \
  && !defined(SL_CATALOG_LOG_FORMATTED_OUTPUT_PRESENT)
#define ZPAL_LOG_BACKEND_COMPACT 1
#else
#define ZPAL_LOG_BACKEND_COMPACT 0
#endif

#else
#define ZPAL_LOG_PLATFORM_LOG_AVAILABLE 0
#define ZPAL_LOG_BACKEND_COMPACT 0
#endif

#endif // ZPAL_LOG_PLATFORM_H_
