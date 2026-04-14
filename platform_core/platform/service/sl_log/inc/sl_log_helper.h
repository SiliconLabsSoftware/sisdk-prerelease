
/***************************************************************************/ /**
* @file sl_log_helper.h
* @brief SL Log Helper Macros and Utilities
* @version 1.0.0
*******************************************************************************
* # License
* <b>Copyright 2025 Silicon Laboratories Inc. www.silabs.com</b>
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
******************************************************************************/

#ifndef SL_LOG_HELPER_H
#define SL_LOG_HELPER_H

#ifdef __cplusplus
extern "C" {
#endif
#if defined(SL_COMPONENT_CATALOG_PRESENT)
#include "sl_component_catalog.h"
#endif
#include "sl_log.h"
#include "sl_log_common_config.h"
#ifdef SL_CATALOG_LOG_BACKEND_SYSTEMVIEW_PRESENT
#include "SEGGER_SYSVIEW.h"
#endif
#include "sl_common.h"
#define SL_STRINGIFY(x) STRINGIZE(x)

/** @addtogroup sl_log_helper SL Log Helper Functions
 * @brief Helper macros and utilities for the Silicon Labs logging system
 *
 * This module provides the core helper macros that user can use to log messages and
 * events with various log levels and argument counts. It includes compile-time validation
 * to ensure correct usage and efficient logging implementations.
 *
 * @{
 */

/**
 * @defgroup sl_log_disabled_macros Disabled Logging Macros
 * @brief Macros that disable logging when compile-time level is set to NONE
 *
 * When SL_LOG_CONFIG_LEVEL_COMPILE_TIME is set to SL_LOG_CONFIG_LEVEL_NONE,
 * all logging macros are replaced with void operations to completely eliminate
 * logging overhead at compile time.
 *
 * @{
 */


/**
 * @defgroup sl_log_memory_sections Memory Section Definitions
 * @brief Linker section attributes for log string placement
 * @{
 */

/**
 * @defgroup sl_log_compiler_support Compiler-Specific Support
 * @brief Compiler-specific definitions for string section placement
 * @{
 */
#if defined(__GNUC__)
/**
 * @brief Linker section attribute for log format strings
 *
 * This attribute places log format strings in a dedicated memory section
 * for optimized memory layout and potential compression or removal.
 */
#define SL_COMPACT_STRINGS_SECTION __attribute__((section(".log_fmt")))

#elif defined(__ICCARM__)
/**
 * @brief Compiler-specific string section placement for IAR
 *
 * IAR-specific pragma for placing log format strings in a dedicated section.
 */
#define SL_COMPACT_STRINGS_SECTION @"log_fmt"

#else
/**
 * @brief Default definition (no special section)
 *
 * For compilers that don't support custom section placement.
 */
#define SL_COMPACT_STRINGS_SECTION
#endif

/** @} (end addtogroup sl_log_memory_sections) */

/** @} (end addtogroup sl_log_compiler_support) */




#define SL_PRINT_VOID_ARG0_DBG(EVENT, EVENT_TYPE)                           do { (void)sizeof(EVENT); (void)sizeof(EVENT_TYPE); } while (0);
#define SL_PRINT_VOID_ARG0_ERR(EVENT, EVENT_TYPE)                           do { (void)sizeof(EVENT); (void)sizeof(EVENT_TYPE); } while (0);
#define SL_PRINT_VOID_ARG0_WRN(EVENT, EVENT_TYPE)                           do { (void)sizeof(EVENT); (void)sizeof(EVENT_TYPE); } while (0);
#define SL_PRINT_VOID_ARG0_INFO(EVENT, EVENT_TYPE)                          do { (void)sizeof(EVENT); (void)sizeof(EVENT_TYPE); } while (0);

#define SL_PRINT_VOID_ARG1_DBG(EVENT, EVENT_TYPE, ARG1)                     do { (void)sizeof(EVENT); (void)sizeof(EVENT_TYPE); (void)sizeof(ARG1); } while (0);
#define SL_PRINT_VOID_ARG1_ERR(EVENT, EVENT_TYPE, ARG1)                     do { (void)sizeof(EVENT); (void)sizeof(EVENT_TYPE); (void)sizeof(ARG1); } while (0);
#define SL_PRINT_VOID_ARG1_WRN(EVENT, EVENT_TYPE, ARG1)                     do { (void)sizeof(EVENT); (void)sizeof(EVENT_TYPE); (void)sizeof(ARG1); } while (0);
#define SL_PRINT_VOID_ARG1_INFO(EVENT, EVENT_TYPE, ARG1)                    do { (void)sizeof(EVENT); (void)sizeof(EVENT_TYPE); (void)sizeof(ARG1); } while (0);

#define SL_PRINT_VOID_ARG2_DBG(EVENT, EVENT_TYPE, ARG1, ARG2)               do { (void)sizeof(EVENT); (void)sizeof(EVENT_TYPE); (void)sizeof(ARG1); (void)sizeof(ARG2); } while (0);
#define SL_PRINT_VOID_ARG2_ERR(EVENT, EVENT_TYPE, ARG1, ARG2)               do { (void)sizeof(EVENT); (void)sizeof(EVENT_TYPE); (void)sizeof(ARG1); (void)sizeof(ARG2); } while (0);
#define SL_PRINT_VOID_ARG2_WRN(EVENT, EVENT_TYPE, ARG1, ARG2)               do { (void)sizeof(EVENT); (void)sizeof(EVENT_TYPE); (void)sizeof(ARG1); (void)sizeof(ARG2); } while (0);
#define SL_PRINT_VOID_ARG2_INFO(EVENT, EVENT_TYPE, ARG1, ARG2)              do { (void)sizeof(EVENT); (void)sizeof(EVENT_TYPE); (void)sizeof(ARG1); (void)sizeof(ARG2); } while (0);

#define SL_PRINT_VOID_ARG3_DBG(EVENT, EVENT_TYPE, ARG1, ARG2, ARG3)         do { (void)sizeof(EVENT); (void)sizeof(EVENT_TYPE); (void)sizeof(ARG1); (void)sizeof(ARG2); (void)sizeof(ARG3); } while (0);
#define SL_PRINT_VOID_ARG3_ERR(EVENT, EVENT_TYPE, ARG1, ARG2, ARG3)         do { (void)sizeof(EVENT); (void)sizeof(EVENT_TYPE); (void)sizeof(ARG1); (void)sizeof(ARG2); (void)sizeof(ARG3); } while (0);
#define SL_PRINT_VOID_ARG3_WRN(EVENT, EVENT_TYPE, ARG1, ARG2, ARG3)         do { (void)sizeof(EVENT); (void)sizeof(EVENT_TYPE); (void)sizeof(ARG1); (void)sizeof(ARG2); (void)sizeof(ARG3); } while (0);
#define SL_PRINT_VOID_ARG3_INFO(EVENT, EVENT_TYPE, ARG1, ARG2, ARG3)        do { (void)sizeof(EVENT); (void)sizeof(EVENT_TYPE); (void)sizeof(ARG1); (void)sizeof(ARG2); (void)sizeof(ARG3); } while (0);

#define SL_PRINT_VOID_ARG4_DBG(EVENT, EVENT_TYPE, ARG1, ARG2, ARG3, ARG4)   do { (void)sizeof(EVENT); (void)sizeof(EVENT_TYPE); (void)sizeof(ARG1); (void)sizeof(ARG2); (void)sizeof(ARG3); (void)sizeof(ARG4); } while (0);
#define SL_PRINT_VOID_ARG4_ERR(EVENT, EVENT_TYPE, ARG1, ARG2, ARG3, ARG4)   do { (void)sizeof(EVENT); (void)sizeof(EVENT_TYPE); (void)sizeof(ARG1); (void)sizeof(ARG2); (void)sizeof(ARG3); (void)sizeof(ARG4); } while (0);
#define SL_PRINT_VOID_ARG4_WRN(EVENT, EVENT_TYPE, ARG1, ARG2, ARG3, ARG4)   do { (void)sizeof(EVENT); (void)sizeof(EVENT_TYPE); (void)sizeof(ARG1); (void)sizeof(ARG2); (void)sizeof(ARG3); (void)sizeof(ARG4); } while (0);
#define SL_PRINT_VOID_ARG4_INFO(EVENT, EVENT_TYPE, ARG1, ARG2, ARG3, ARG4)  do { (void)sizeof(EVENT); (void)sizeof(EVENT_TYPE); (void)sizeof(ARG1); (void)sizeof(ARG2); (void)sizeof(ARG3); (void)sizeof(ARG4); } while (0);

#define SL_PRINT_VOID_ARG5_DBG(EVENT, EVENT_TYPE, ARG1, ARG2, ARG3, ARG4, ARG5)   do { (void)sizeof(EVENT); (void)sizeof(EVENT_TYPE); (void)sizeof(ARG1); (void)sizeof(ARG2); (void)sizeof(ARG3); (void)sizeof(ARG4); (void)sizeof(ARG5); } while (0);
#define SL_PRINT_VOID_ARG5_ERR(EVENT, EVENT_TYPE, ARG1, ARG2, ARG3, ARG4, ARG5)   do { (void)sizeof(EVENT); (void)sizeof(EVENT_TYPE); (void)sizeof(ARG1); (void)sizeof(ARG2); (void)sizeof(ARG3); (void)sizeof(ARG4); (void)sizeof(ARG5); } while (0);
#define SL_PRINT_VOID_ARG5_WRN(EVENT, EVENT_TYPE, ARG1, ARG2, ARG3, ARG4, ARG5)   do { (void)sizeof(EVENT); (void)sizeof(EVENT_TYPE); (void)sizeof(ARG1); (void)sizeof(ARG2); (void)sizeof(ARG3); (void)sizeof(ARG4); (void)sizeof(ARG5); } while (0);
#define SL_PRINT_VOID_ARG5_INFO(EVENT, EVENT_TYPE, ARG1, ARG2, ARG3, ARG4, ARG5)  do { (void)sizeof(EVENT); (void)sizeof(EVENT_TYPE); (void)sizeof(ARG1); (void)sizeof(ARG2); (void)sizeof(ARG3); (void)sizeof(ARG4); (void)sizeof(ARG5); } while (0);

#define SL_PRINT_VOID_ARG6_DBG(EVENT, EVENT_TYPE, ARG1, ARG2, ARG3, ARG4, ARG5, ARG6)   do { (void)sizeof(EVENT); (void)sizeof(EVENT_TYPE); (void)sizeof(ARG1); (void)sizeof(ARG2); (void)sizeof(ARG3); (void)sizeof(ARG4); (void)sizeof(ARG5); (void)sizeof(ARG6); } while (0);
#define SL_PRINT_VOID_ARG6_ERR(EVENT, EVENT_TYPE, ARG1, ARG2, ARG3, ARG4, ARG5, ARG6)   do { (void)sizeof(EVENT); (void)sizeof(EVENT_TYPE); (void)sizeof(ARG1); (void)sizeof(ARG2); (void)sizeof(ARG3); (void)sizeof(ARG4); (void)sizeof(ARG5); (void)sizeof(ARG6); } while (0);
#define SL_PRINT_VOID_ARG6_WRN(EVENT, EVENT_TYPE, ARG1, ARG2, ARG3, ARG4, ARG5, ARG6)   do { (void)sizeof(EVENT); (void)sizeof(EVENT_TYPE); (void)sizeof(ARG1); (void)sizeof(ARG2); (void)sizeof(ARG3); (void)sizeof(ARG4); (void)sizeof(ARG5); (void)sizeof(ARG6); } while (0);
#define SL_PRINT_VOID_ARG6_INFO(EVENT, EVENT_TYPE, ARG1, ARG2, ARG3, ARG4, ARG5, ARG6)  do { (void)sizeof(EVENT); (void)sizeof(EVENT_TYPE); (void)sizeof(ARG1); (void)sizeof(ARG2); (void)sizeof(ARG3); (void)sizeof(ARG4); (void)sizeof(ARG5); (void)sizeof(ARG6); } while (0);

#define SL_PRINT_VOID_ARG7_DBG(EVENT, EVENT_TYPE, ARG1, ARG2, ARG3, ARG4, ARG5, ARG6, ARG7)   do { (void)sizeof(EVENT); (void)sizeof(EVENT_TYPE); (void)sizeof(ARG1); (void)sizeof(ARG2); (void)sizeof(ARG3); (void)sizeof(ARG4); (void)sizeof(ARG5); (void)sizeof(ARG6); (void)sizeof(ARG7); } while (0);
#define SL_PRINT_VOID_ARG7_ERR(EVENT, EVENT_TYPE, ARG1, ARG2, ARG3, ARG4, ARG5, ARG6, ARG7)   do { (void)sizeof(EVENT); (void)sizeof(EVENT_TYPE); (void)sizeof(ARG1); (void)sizeof(ARG2); (void)sizeof(ARG3); (void)sizeof(ARG4); (void)sizeof(ARG5); (void)sizeof(ARG6); (void)sizeof(ARG7); } while (0);
#define SL_PRINT_VOID_ARG7_WRN(EVENT, EVENT_TYPE, ARG1, ARG2, ARG3, ARG4, ARG5, ARG6, ARG7)   do { (void)sizeof(EVENT); (void)sizeof(EVENT_TYPE); (void)sizeof(ARG1); (void)sizeof(ARG2); (void)sizeof(ARG3); (void)sizeof(ARG4); (void)sizeof(ARG5); (void)sizeof(ARG6); (void)sizeof(ARG7); } while (0);
#define SL_PRINT_VOID_ARG7_INFO(EVENT, EVENT_TYPE, ARG1, ARG2, ARG3, ARG4, ARG5, ARG6, ARG7)  do { (void)sizeof(EVENT); (void)sizeof(EVENT_TYPE); (void)sizeof(ARG1); (void)sizeof(ARG2); (void)sizeof(ARG3); (void)sizeof(ARG4); (void)sizeof(ARG5); (void)sizeof(ARG6); (void)sizeof(ARG7); } while (0);

#define SL_PRINT_VOID_ARG8_DBG(EVENT, EVENT_TYPE, ARG1, ARG2, ARG3, ARG4, ARG5, ARG6, ARG7, ARG8)   do { (void)sizeof(EVENT); (void)sizeof(EVENT_TYPE); (void)sizeof(ARG1); (void)sizeof(ARG2); (void)sizeof(ARG3); (void)sizeof(ARG4); (void)sizeof(ARG5); (void)sizeof(ARG6); (void)sizeof(ARG7); (void)sizeof(ARG8); } while (0);
#define SL_PRINT_VOID_ARG8_ERR(EVENT, EVENT_TYPE, ARG1, ARG2, ARG3, ARG4, ARG5, ARG6, ARG7, ARG8)   do { (void)sizeof(EVENT); (void)sizeof(EVENT_TYPE); (void)sizeof(ARG1); (void)sizeof(ARG2); (void)sizeof(ARG3); (void)sizeof(ARG4); (void)sizeof(ARG5); (void)sizeof(ARG6); (void)sizeof(ARG7); (void)sizeof(ARG8); } while (0);
#define SL_PRINT_VOID_ARG8_WRN(EVENT, EVENT_TYPE, ARG1, ARG2, ARG3, ARG4, ARG5, ARG6, ARG7, ARG8)   do { (void)sizeof(EVENT); (void)sizeof(EVENT_TYPE); (void)sizeof(ARG1); (void)sizeof(ARG2); (void)sizeof(ARG3); (void)sizeof(ARG4); (void)sizeof(ARG5); (void)sizeof(ARG6); (void)sizeof(ARG7); (void)sizeof(ARG8); } while (0);
#define SL_PRINT_VOID_ARG8_INFO(EVENT, EVENT_TYPE, ARG1, ARG2, ARG3, ARG4, ARG5, ARG6, ARG7, ARG8)  do { (void)sizeof(EVENT); (void)sizeof(EVENT_TYPE); (void)sizeof(ARG1); (void)sizeof(ARG2); (void)sizeof(ARG3); (void)sizeof(ARG4); (void)sizeof(ARG5); (void)sizeof(ARG6); (void)sizeof(ARG7); (void)sizeof(ARG8); } while (0);

#define SL_PRINT_VOID_ARG9_DBG(EVENT, EVENT_TYPE, ARG1, ARG2, ARG3, ARG4, ARG5, ARG6, ARG7, ARG8, ARG9)   do { (void)sizeof(EVENT); (void)sizeof(EVENT_TYPE); (void)sizeof(ARG1); (void)sizeof(ARG2); (void)sizeof(ARG3); (void)sizeof(ARG4); (void)sizeof(ARG5); (void)sizeof(ARG6); (void)sizeof(ARG7); (void)sizeof(ARG8); (void)sizeof(ARG9); } while (0);
#define SL_PRINT_VOID_ARG9_ERR(EVENT, EVENT_TYPE, ARG1, ARG2, ARG3, ARG4, ARG5, ARG6, ARG7, ARG8, ARG9)   do { (void)sizeof(EVENT); (void)sizeof(EVENT_TYPE); (void)sizeof(ARG1); (void)sizeof(ARG2); (void)sizeof(ARG3); (void)sizeof(ARG4); (void)sizeof(ARG5); (void)sizeof(ARG6); (void)sizeof(ARG7); (void)sizeof(ARG8); (void)sizeof(ARG9); } while (0);
#define SL_PRINT_VOID_ARG9_WRN(EVENT, EVENT_TYPE, ARG1, ARG2, ARG3, ARG4, ARG5, ARG6, ARG7, ARG8, ARG9)   do { (void)sizeof(EVENT); (void)sizeof(EVENT_TYPE); (void)sizeof(ARG1); (void)sizeof(ARG2); (void)sizeof(ARG3); (void)sizeof(ARG4); (void)sizeof(ARG5); (void)sizeof(ARG6); (void)sizeof(ARG7); (void)sizeof(ARG8); (void)sizeof(ARG9); } while (0);
#define SL_PRINT_VOID_ARG9_INFO(EVENT, EVENT_TYPE, ARG1, ARG2, ARG3, ARG4, ARG5, ARG6, ARG7, ARG8, ARG9)  do { (void)sizeof(EVENT); (void)sizeof(EVENT_TYPE); (void)sizeof(ARG1); (void)sizeof(ARG2); (void)sizeof(ARG3); (void)sizeof(ARG4); (void)sizeof(ARG5); (void)sizeof(ARG6); (void)sizeof(ARG7); (void)sizeof(ARG8); (void)sizeof(ARG9); } while (0);

#define SL_PRINT_VOID_ARG10_DBG(EVENT, EVENT_TYPE, ARG1, ARG2, ARG3, ARG4, ARG5, ARG6, ARG7, ARG8, ARG9, ARG10)   do { (void)sizeof(EVENT); (void)sizeof(EVENT_TYPE); (void)sizeof(ARG1); (void)sizeof(ARG2); (void)sizeof(ARG3); (void)sizeof(ARG4); (void)sizeof(ARG5); (void)sizeof(ARG6); (void)sizeof(ARG7); (void)sizeof(ARG8); (void)sizeof(ARG9); (void)sizeof(ARG10); } while (0);
#define SL_PRINT_VOID_ARG10_ERR(EVENT, EVENT_TYPE, ARG1, ARG2, ARG3, ARG4, ARG5, ARG6, ARG7, ARG8, ARG9, ARG10)   do { (void)sizeof(EVENT); (void)sizeof(EVENT_TYPE); (void)sizeof(ARG1); (void)sizeof(ARG2); (void)sizeof(ARG3); (void)sizeof(ARG4); (void)sizeof(ARG5); (void)sizeof(ARG6); (void)sizeof(ARG7); (void)sizeof(ARG8); (void)sizeof(ARG9); (void)sizeof(ARG10); } while (0);
#define SL_PRINT_VOID_ARG10_WRN(EVENT, EVENT_TYPE, ARG1, ARG2, ARG3, ARG4, ARG5, ARG6, ARG7, ARG8, ARG9, ARG10)   do { (void)sizeof(EVENT); (void)sizeof(EVENT_TYPE); (void)sizeof(ARG1); (void)sizeof(ARG2); (void)sizeof(ARG3); (void)sizeof(ARG4); (void)sizeof(ARG5); (void)sizeof(ARG6); (void)sizeof(ARG7); (void)sizeof(ARG8); (void)sizeof(ARG9); (void)sizeof(ARG10); } while (0);
#define SL_PRINT_VOID_ARG10_INFO(EVENT, EVENT_TYPE, ARG1, ARG2, ARG3, ARG4, ARG5, ARG6, ARG7, ARG8, ARG9, ARG10)  do { (void)sizeof(EVENT); (void)sizeof(EVENT_TYPE); (void)sizeof(ARG1); (void)sizeof(ARG2); (void)sizeof(ARG3); (void)sizeof(ARG4); (void)sizeof(ARG5); (void)sizeof(ARG6); (void)sizeof(ARG7); (void)sizeof(ARG8); (void)sizeof(ARG9); (void)sizeof(ARG10); } while (0);

/** @} (end addtogroup sl_log_disabled_macros) */
#if defined(SL_CATALOG_LOG_BACKEND_SYSTEMVIEW_PRESENT)
/** @brief Debug log with no arguments - disabled */
#define SL_PRINT_ARG0_DBG(EVENT, EVENT_TYPE) \
  (void)EVENT_TYPE; \
  SEGGER_SYSVIEW_RecordVoid(EVENT);
/** @brief Error log with no arguments - disabled */
#define SL_PRINT_ARG0_ERR(EVENT, EVENT_TYPE) \
  (void)EVENT_TYPE; \
  SEGGER_SYSVIEW_RecordVoid(EVENT);
/** @brief Warning log with no arguments - disabled */
#define SL_PRINT_ARG0_WRN(EVENT, EVENT_TYPE) \
  (void)EVENT_TYPE; \
  SEGGER_SYSVIEW_RecordVoid(EVENT);
/** @brief Info log with no arguments - disabled */
#define SL_PRINT_ARG0_INFO(EVENT, EVENT_TYPE) \
  (void)EVENT_TYPE; \
  SEGGER_SYSVIEW_RecordVoid(EVENT);
/** @brief Debug log with 1 argument - disabled */
#define SL_PRINT_ARG1_DBG(EVENT, EVENT_TYPE, ARG1) \
  (void)EVENT_TYPE; \
  SEGGER_SYSVIEW_RecordU32(EVENT, ARG1);
/** @brief Error log with 1 argument - disabled */
#define SL_PRINT_ARG1_ERR(EVENT, EVENT_TYPE, ARG1) \
  (void)EVENT_TYPE; \
  SEGGER_SYSVIEW_RecordU32(EVENT, ARG1);
/** @brief Warning log with 1 argument - disabled */
#define SL_PRINT_ARG1_WRN(EVENT, EVENT_TYPE, ARG1) \
  (void)EVENT_TYPE; \
  SEGGER_SYSVIEW_RecordU32(EVENT, ARG1);
/** @brief Info log with 1 argument - disabled */
#define SL_PRINT_ARG1_INFO(EVENT, EVENT_TYPE, ARG1) \
  (void)EVENT_TYPE; \
  SEGGER_SYSVIEW_RecordU32(EVENT, ARG1);
/** @brief Debug log with 2 arguments - disabled */
#define SL_PRINT_ARG2_DBG(EVENT, EVENT_TYPE, ARG1, ARG2) \
  (void)EVENT_TYPE; \
  SEGGER_SYSVIEW_RecordU32x2(EVENT, ARG1, ARG2);
/** @brief Error log with 2 arguments - disabled */
#define SL_PRINT_ARG2_ERR(EVENT, EVENT_TYPE, ARG1, ARG2) \
  (void)EVENT_TYPE; \
  SEGGER_SYSVIEW_RecordU32x2(EVENT, ARG1, ARG2);
/** @brief Warning log with 2 arguments - disabled */
#define SL_PRINT_ARG2_WRN(EVENT, EVENT_TYPE, ARG1, ARG2) \
  (void)EVENT_TYPE; \
  SEGGER_SYSVIEW_RecordU32x2(EVENT, ARG1, ARG2);
/** @brief Info log with 2 arguments - disabled */
#define SL_PRINT_ARG2_INFO(EVENT, EVENT_TYPE, ARG1, ARG2) \
  (void)EVENT_TYPE; \
  SEGGER_SYSVIEW_RecordU32x2(EVENT, ARG1, ARG2);
/** @brief Debug log with 3 arguments - disabled */
#define SL_PRINT_ARG3_DBG(EVENT, EVENT_TYPE, ARG1, ARG2, ARG3) \
  (void)EVENT_TYPE; \
  SEGGER_SYSVIEW_RecordU32x3(EVENT, ARG1, ARG2, ARG3);
/** @brief Error log with 3 arguments - disabled */
#define SL_PRINT_ARG3_ERR(EVENT, EVENT_TYPE, ARG1, ARG2, ARG3) \
  (void)EVENT_TYPE; \
  SEGGER_SYSVIEW_RecordU32x3(EVENT, ARG1, ARG2, ARG3);
/** @brief Warning log with 3 arguments - disabled */
#define SL_PRINT_ARG3_WRN(EVENT, EVENT_TYPE, ARG1, ARG2, ARG3) \
  (void)EVENT_TYPE; \
  SEGGER_SYSVIEW_RecordU32x3(EVENT, ARG1, ARG2, ARG3);
/** @brief Info log with 3 arguments - disabled */
#define SL_PRINT_ARG3_INFO(EVENT, EVENT_TYPE, ARG1, ARG2, ARG3) \
  (void)EVENT_TYPE; \
  SEGGER_SYSVIEW_RecordU32x3(EVENT, ARG1, ARG2, ARG3);
#if (SL_LOG_CONFIG_ARG >= 4)
/** @brief Debug log with 4 arguments */
#define SL_PRINT_ARG4_DBG(EVENT, EVENT_TYPE, ARG1, ARG2, ARG3, ARG4) \
  (void)EVENT_TYPE; \
  SEGGER_SYSVIEW_RecordU32x4(EVENT, ARG1, ARG2, ARG3, ARG4);
/** @brief Error log with 4 arguments */
#define SL_PRINT_ARG4_ERR(EVENT, EVENT_TYPE, ARG1, ARG2, ARG3, ARG4) \
  (void)EVENT_TYPE; \
  SEGGER_SYSVIEW_RecordU32x4(EVENT, ARG1, ARG2, ARG3, ARG4);
/** @brief Warning log with 4 arguments */
#define SL_PRINT_ARG4_WRN(EVENT, EVENT_TYPE, ARG1, ARG2, ARG3, ARG4) \
  (void)EVENT_TYPE; \
  SEGGER_SYSVIEW_RecordU32x4(EVENT, ARG1, ARG2, ARG3, ARG4);
/** @brief Info log with 4 arguments */
#define SL_PRINT_ARG4_INFO(EVENT, EVENT_TYPE, ARG1, ARG2, ARG3, ARG4) \
  (void)EVENT_TYPE; \
  SEGGER_SYSVIEW_RecordU32x4(EVENT, ARG1, ARG2, ARG3, ARG4);
#endif
#if (SL_LOG_CONFIG_ARG >= 5)
/** @brief Debug log with 5 arguments */
#define SL_PRINT_ARG5_DBG(EVENT, EVENT_TYPE, ARG1, ARG2, ARG3, ARG4, ARG5) \
  (void)EVENT_TYPE; \
  SEGGER_SYSVIEW_RecordU32x5(EVENT, ARG1, ARG2, ARG3, ARG4, ARG5);
#define SL_PRINT_ARG5_ERR(EVENT, EVENT_TYPE, ARG1, ARG2, ARG3, ARG4, ARG5) \
  (void)EVENT_TYPE; \
  SEGGER_SYSVIEW_RecordU32x5(EVENT, ARG1, ARG2, ARG3, ARG4, ARG5);
#define SL_PRINT_ARG5_WRN(EVENT, EVENT_TYPE, ARG1, ARG2, ARG3, ARG4, ARG5) \
  (void)EVENT_TYPE; \
  SEGGER_SYSVIEW_RecordU32x5(EVENT, ARG1, ARG2, ARG3, ARG4, ARG5);
#define SL_PRINT_ARG5_INFO(EVENT, EVENT_TYPE, ARG1, ARG2, ARG3, ARG4, ARG5) \
  (void)EVENT_TYPE; \
  SEGGER_SYSVIEW_RecordU32x5(EVENT, ARG1, ARG2, ARG3, ARG4, ARG5);
#endif
#if (SL_LOG_CONFIG_ARG >= 6)
#define SL_PRINT_ARG6_DBG(EVENT, EVENT_TYPE, ARG1, ARG2, ARG3, ARG4, ARG5, ARG6) \
  (void)EVENT_TYPE; \
  SEGGER_SYSVIEW_RecordU32x6(EVENT, ARG1, ARG2, ARG3, ARG4, ARG5, ARG6);
#define SL_PRINT_ARG6_ERR(EVENT, EVENT_TYPE, ARG1, ARG2, ARG3, ARG4, ARG5, ARG6) \
  (void)EVENT_TYPE; \
  SEGGER_SYSVIEW_RecordU32x6(EVENT, ARG1, ARG2, ARG3, ARG4, ARG5, ARG6);
#define SL_PRINT_ARG6_WRN(EVENT, EVENT_TYPE, ARG1, ARG2, ARG3, ARG4, ARG5, ARG6) \
  (void)EVENT_TYPE; \
  SEGGER_SYSVIEW_RecordU32x6(EVENT, ARG1, ARG2, ARG3, ARG4, ARG5, ARG6);
#define SL_PRINT_ARG6_INFO(EVENT, EVENT_TYPE, ARG1, ARG2, ARG3, ARG4, ARG5, ARG6) \
  (void)EVENT_TYPE; \
  SEGGER_SYSVIEW_RecordU32x6(EVENT, ARG1, ARG2, ARG3, ARG4, ARG5, ARG6);
#endif
#if (SL_LOG_CONFIG_ARG >= 7)
#define SL_PRINT_ARG7_DBG(EVENT, EVENT_TYPE, ARG1, ARG2, ARG3, ARG4, ARG5, ARG6, ARG7) \
  (void)EVENT_TYPE; \
  SEGGER_SYSVIEW_RecordU32x7(EVENT, ARG1, ARG2, ARG3, ARG4, ARG5, ARG6, ARG7);
#define SL_PRINT_ARG7_ERR(EVENT, EVENT_TYPE, ARG1, ARG2, ARG3, ARG4, ARG5, ARG6, ARG7) \
  (void)EVENT_TYPE; \
  SEGGER_SYSVIEW_RecordU32x7(EVENT, ARG1, ARG2, ARG3, ARG4, ARG5, ARG6, ARG7);
#define SL_PRINT_ARG7_WRN(EVENT, EVENT_TYPE, ARG1, ARG2, ARG3, ARG4, ARG5, ARG6, ARG7) \
  (void)EVENT_TYPE; \
  SEGGER_SYSVIEW_RecordU32x7(EVENT, ARG1, ARG2, ARG3, ARG4, ARG5, ARG6, ARG7);
#define SL_PRINT_ARG7_INFO(EVENT, EVENT_TYPE, ARG1, ARG2, ARG3, ARG4, ARG5, ARG6, ARG7) \
  (void)EVENT_TYPE; \
  SEGGER_SYSVIEW_RecordU32x7(EVENT, ARG1, ARG2, ARG3, ARG4, ARG5, ARG6, ARG7);
#endif
#if (SL_LOG_CONFIG_ARG >= 8)
#define SL_PRINT_ARG8_DBG(EVENT, EVENT_TYPE, ARG1, ARG2, ARG3, ARG4, ARG5, ARG6, ARG7, ARG8) \
  (void)EVENT_TYPE; \
  SEGGER_SYSVIEW_RecordU32x8(EVENT, ARG1, ARG2, ARG3, ARG4, ARG5, ARG6, ARG7, ARG8);
#define SL_PRINT_ARG8_ERR(EVENT, EVENT_TYPE, ARG1, ARG2, ARG3, ARG4, ARG5, ARG6, ARG7, ARG8) \
  (void)EVENT_TYPE; \
  SEGGER_SYSVIEW_RecordU32x8(EVENT, ARG1, ARG2, ARG3, ARG4, ARG5, ARG6, ARG7, ARG8);
#define SL_PRINT_ARG8_WRN(EVENT, EVENT_TYPE, ARG1, ARG2, ARG3, ARG4, ARG5, ARG6, ARG7, ARG8) \
  (void)EVENT_TYPE; \
  SEGGER_SYSVIEW_RecordU32x8(EVENT, ARG1, ARG2, ARG3, ARG4, ARG5, ARG6, ARG7, ARG8);
#define SL_PRINT_ARG8_INFO(EVENT, EVENT_TYPE, ARG1, ARG2, ARG3, ARG4, ARG5, ARG6, ARG7, ARG8) \
  (void)EVENT_TYPE; \
  SEGGER_SYSVIEW_RecordU32x8(EVENT, ARG1, ARG2, ARG3, ARG4, ARG5, ARG6, ARG7, ARG8);
#endif
#if (SL_LOG_CONFIG_ARG >= 9)
#define SL_PRINT_ARG9_DBG(EVENT, EVENT_TYPE, ARG1, ARG2, ARG3, ARG4, ARG5, ARG6, ARG7, ARG8, ARG9) \
  (void)EVENT_TYPE; \
  SEGGER_SYSVIEW_RecordU32x9(EVENT, ARG1, ARG2, ARG3, ARG4, ARG5, ARG6, ARG7, ARG8, ARG9);
#define SL_PRINT_ARG9_ERR(EVENT, EVENT_TYPE, ARG1, ARG2, ARG3, ARG4, ARG5, ARG6, ARG7, ARG8, ARG9) \
  (void)EVENT_TYPE; \
  SEGGER_SYSVIEW_RecordU32x9(EVENT, ARG1, ARG2, ARG3, ARG4, ARG5, ARG6, ARG7, ARG8, ARG9);
#define SL_PRINT_ARG9_WRN(EVENT, EVENT_TYPE, ARG1, ARG2, ARG3, ARG4, ARG5, ARG6, ARG7, ARG8, ARG9) \
  (void)EVENT_TYPE; \
  SEGGER_SYSVIEW_RecordU32x9(EVENT, ARG1, ARG2, ARG3, ARG4, ARG5, ARG6, ARG7, ARG8, ARG9);
#define SL_PRINT_ARG9_INFO(EVENT, EVENT_TYPE, ARG1, ARG2, ARG3, ARG4, ARG5, ARG6, ARG7, ARG8, ARG9) \
  (void)EVENT_TYPE; \
  SEGGER_SYSVIEW_RecordU32x9(EVENT, ARG1, ARG2, ARG3, ARG4, ARG5, ARG6, ARG7, ARG8, ARG9);
#endif
#if (SL_LOG_CONFIG_ARG >= 10)
#define SL_PRINT_ARG10_DBG(EVENT, EVENT_TYPE, ARG1, ARG2, ARG3, ARG4, ARG5, ARG6, ARG7, ARG8, ARG9, ARG10) \
  (void)EVENT_TYPE; \
  SEGGER_SYSVIEW_RecordU32x10(EVENT, ARG1, ARG2, ARG3, ARG4, ARG5, ARG6, ARG7, ARG8, ARG9, ARG10);
#define SL_PRINT_ARG10_ERR(EVENT, EVENT_TYPE, ARG1, ARG2, ARG3, ARG4, ARG5, ARG6, ARG7, ARG8, ARG9, ARG10) \
  (void)EVENT_TYPE; \
  SEGGER_SYSVIEW_RecordU32x10(EVENT, ARG1, ARG2, ARG3, ARG4, ARG5, ARG6, ARG7, ARG8, ARG9, ARG10);
#define SL_PRINT_ARG10_WRN(EVENT, EVENT_TYPE, ARG1, ARG2, ARG3, ARG4, ARG5, ARG6, ARG7, ARG8, ARG9, ARG10) \
  (void)EVENT_TYPE; \
  SEGGER_SYSVIEW_RecordU32x10(EVENT, ARG1, ARG2, ARG3, ARG4, ARG5, ARG6, ARG7, ARG8, ARG9, ARG10);
#define SL_PRINT_ARG10_INFO(EVENT, EVENT_TYPE, ARG1, ARG2, ARG3, ARG4, ARG5, ARG6, ARG7, ARG8, ARG9, ARG10) \
  (void)EVENT_TYPE; \
  SEGGER_SYSVIEW_RecordU32x10(EVENT, ARG1, ARG2, ARG3, ARG4, ARG5, ARG6, ARG7, ARG8, ARG9, ARG10);
#endif
#else

/**
 * @defgroup sl_log_backend_functions Backend Function Declarations
 * @brief External function declarations for log backend implementations
 *
 * These functions are provided by the selected backend implementation
 * (proprietary or SystemView) and handle the actual transmission of
 * log messages with various argument counts.
 *
 * @{
 */

/**
 * @defgroup sl_log_active_macros Active Logging Macros
 * @brief Macros that perform actual logging when enabled
 *
 * These macros are used when logging is enabled (compile-time level is not
 * NONE). They call the appropriate backend functions with formatted log level
 * information.
 *
 * @{
 */

/** @brief Send debug log message with no arguments */
#define SL_PRINT_ARG0_DBG(EVENT, EVENT_TYPE)                                   \
  sl_log_send_no_args(EVENT, SL_LOG_CONFIG_LEVEL_DEBUG << 1 | EVENT_TYPE);

/** @brief Send error log message with no arguments */
#define SL_PRINT_ARG0_ERR(EVENT, EVENT_TYPE)                                   \
  sl_log_send_no_args(EVENT, SL_LOG_CONFIG_LEVEL_ERROR << 1 | EVENT_TYPE);

/** @brief Send warning log message with no arguments */
#define SL_PRINT_ARG0_WRN(EVENT, EVENT_TYPE)                                   \
  sl_log_send_no_args(EVENT, SL_LOG_CONFIG_LEVEL_WARN << 1 | EVENT_TYPE);

/** @brief Send info log message with no arguments */
#define SL_PRINT_ARG0_INFO(EVENT, EVENT_TYPE)                                  \
  sl_log_send_no_args(EVENT, SL_LOG_CONFIG_LEVEL_INFO << 1 | EVENT_TYPE);

/** @brief Send debug log message with 1 argument */
#define SL_PRINT_ARG1_DBG(EVENT, EVENT_TYPE, ARG1)                             \
  sl_log_send_arg1(EVENT, SL_LOG_CONFIG_LEVEL_DEBUG << 1 | EVENT_TYPE, ARG1);

/** @brief Send error log message with 1 argument */
#define SL_PRINT_ARG1_ERR(EVENT, EVENT_TYPE, ARG1)                             \
  sl_log_send_arg1(EVENT, SL_LOG_CONFIG_LEVEL_ERROR << 1 | EVENT_TYPE, ARG1);

/** @brief Send warning log message with 1 argument */
#define SL_PRINT_ARG1_WRN(EVENT, EVENT_TYPE, ARG1)                             \
  sl_log_send_arg1(EVENT, SL_LOG_CONFIG_LEVEL_WARN << 1 | EVENT_TYPE, ARG1);

/** @brief Send info log message with 1 argument */
#define SL_PRINT_ARG1_INFO(EVENT, EVENT_TYPE, ARG1)                            \
  sl_log_send_arg1(EVENT, SL_LOG_CONFIG_LEVEL_INFO << 1 | EVENT_TYPE, ARG1);

/** @brief Send debug log message with 2 arguments */
#define SL_PRINT_ARG2_DBG(EVENT, EVENT_TYPE, ARG1, ARG2)                       \
  sl_log_send_arg2(EVENT, SL_LOG_CONFIG_LEVEL_DEBUG << 1 | EVENT_TYPE, ARG1,   \
                   ARG2);

/** @brief Send error log message with 2 arguments */
#define SL_PRINT_ARG2_ERR(EVENT, EVENT_TYPE, ARG1, ARG2)                       \
  sl_log_send_arg2(EVENT, SL_LOG_CONFIG_LEVEL_ERROR << 1 | EVENT_TYPE, ARG1,   \
                   ARG2);

/** @brief Send warning log message with 2 arguments */
#define SL_PRINT_ARG2_WRN(EVENT, EVENT_TYPE, ARG1, ARG2)                       \
  sl_log_send_arg2(EVENT, SL_LOG_CONFIG_LEVEL_WARN << 1 | EVENT_TYPE, ARG1,    \
                   ARG2);

/** @brief Send info log message with 2 arguments */
#define SL_PRINT_ARG2_INFO(EVENT, EVENT_TYPE, ARG1, ARG2)                      \
  sl_log_send_arg2(EVENT, SL_LOG_CONFIG_LEVEL_INFO << 1 | EVENT_TYPE, ARG1,    \
                   ARG2);

/** @brief Send debug log message with 3 arguments */
#define SL_PRINT_ARG3_DBG(EVENT, EVENT_TYPE, ARG1, ARG2, ARG3)                 \
  sl_log_send_arg3(EVENT, SL_LOG_CONFIG_LEVEL_DEBUG << 1 | EVENT_TYPE, ARG1,   \
                   ARG2, ARG3);

/** @brief Send error log message with 3 arguments */
#define SL_PRINT_ARG3_ERR(EVENT, EVENT_TYPE, ARG1, ARG2, ARG3)                 \
  sl_log_send_arg3(EVENT, SL_LOG_CONFIG_LEVEL_ERROR << 1 | EVENT_TYPE, ARG1,   \
                   ARG2, ARG3);

/** @brief Send warning log message with 3 arguments */
#define SL_PRINT_ARG3_WRN(EVENT, EVENT_TYPE, ARG1, ARG2, ARG3)                 \
  sl_log_send_arg3(EVENT, SL_LOG_CONFIG_LEVEL_WARN << 1 | EVENT_TYPE, ARG1,    \
                   ARG2, ARG3);

/** @brief Send info log message with 3 arguments */
#define SL_PRINT_ARG3_INFO(EVENT, EVENT_TYPE, ARG1, ARG2, ARG3)                \
  sl_log_send_arg3(EVENT, SL_LOG_CONFIG_LEVEL_INFO << 1 | EVENT_TYPE, ARG1,    \
                   ARG2, ARG3);

#if (SL_LOG_CONFIG_ARG >= 4)
/** @brief Send debug log message with 4 arguments */
#define SL_PRINT_ARG4_DBG(EVENT, EVENT_TYPE, ARG1, ARG2, ARG3, ARG4)          \
  sl_log_send_arg4(EVENT, SL_LOG_CONFIG_LEVEL_DEBUG << 1 | EVENT_TYPE, ARG1,  \
                   ARG2, ARG3, ARG4);
/** @brief Send error log message with 4 arguments */
#define SL_PRINT_ARG4_ERR(EVENT, EVENT_TYPE, ARG1, ARG2, ARG3, ARG4)          \
  sl_log_send_arg4(EVENT, SL_LOG_CONFIG_LEVEL_ERROR << 1 | EVENT_TYPE, ARG1,   \
                   ARG2, ARG3, ARG4);
/** @brief Send warning log message with 4 arguments */
#define SL_PRINT_ARG4_WRN(EVENT, EVENT_TYPE, ARG1, ARG2, ARG3, ARG4)          \
  sl_log_send_arg4(EVENT, SL_LOG_CONFIG_LEVEL_WARN << 1 | EVENT_TYPE, ARG1,    \
                   ARG2, ARG3, ARG4);
/** @brief Send info log message with 4 arguments */
#define SL_PRINT_ARG4_INFO(EVENT, EVENT_TYPE, ARG1, ARG2, ARG3, ARG4)          \
  sl_log_send_arg4(EVENT, SL_LOG_CONFIG_LEVEL_INFO << 1 | EVENT_TYPE, ARG1,   \
                   ARG2, ARG3, ARG4);
#endif

#if (SL_LOG_CONFIG_ARG >= 5)
#define SL_PRINT_ARG5_DBG(EVENT, EVENT_TYPE, ARG1, ARG2, ARG3, ARG4, ARG5)     \
  sl_log_send_arg5(EVENT, SL_LOG_CONFIG_LEVEL_DEBUG << 1 | EVENT_TYPE, ARG1,   \
                   ARG2, ARG3, ARG4, ARG5);
#define SL_PRINT_ARG5_ERR(EVENT, EVENT_TYPE, ARG1, ARG2, ARG3, ARG4, ARG5)     \
  sl_log_send_arg5(EVENT, SL_LOG_CONFIG_LEVEL_ERROR << 1 | EVENT_TYPE, ARG1,   \
                   ARG2, ARG3, ARG4, ARG5);
#define SL_PRINT_ARG5_WRN(EVENT, EVENT_TYPE, ARG1, ARG2, ARG3, ARG4, ARG5)     \
  sl_log_send_arg5(EVENT, SL_LOG_CONFIG_LEVEL_WARN << 1 | EVENT_TYPE, ARG1,    \
                   ARG2, ARG3, ARG4, ARG5);
#define SL_PRINT_ARG5_INFO(EVENT, EVENT_TYPE, ARG1, ARG2, ARG3, ARG4, ARG5)    \
  sl_log_send_arg5(EVENT, SL_LOG_CONFIG_LEVEL_INFO << 1 | EVENT_TYPE, ARG1,    \
                   ARG2, ARG3, ARG4, ARG5);
#endif

#if (SL_LOG_CONFIG_ARG >= 6)
#define SL_PRINT_ARG6_DBG(EVENT, EVENT_TYPE, ARG1, ARG2, ARG3, ARG4, ARG5, ARG6) \
  sl_log_send_arg6(EVENT, SL_LOG_CONFIG_LEVEL_DEBUG << 1 | EVENT_TYPE, ARG1,   \
                   ARG2, ARG3, ARG4, ARG5, ARG6);
#define SL_PRINT_ARG6_ERR(EVENT, EVENT_TYPE, ARG1, ARG2, ARG3, ARG4, ARG5, ARG6) \
  sl_log_send_arg6(EVENT, SL_LOG_CONFIG_LEVEL_ERROR << 1 | EVENT_TYPE, ARG1,  \
                   ARG2, ARG3, ARG4, ARG5, ARG6);
#define SL_PRINT_ARG6_WRN(EVENT, EVENT_TYPE, ARG1, ARG2, ARG3, ARG4, ARG5, ARG6) \
  sl_log_send_arg6(EVENT, SL_LOG_CONFIG_LEVEL_WARN << 1 | EVENT_TYPE, ARG1,    \
                   ARG2, ARG3, ARG4, ARG5, ARG6);
#define SL_PRINT_ARG6_INFO(EVENT, EVENT_TYPE, ARG1, ARG2, ARG3, ARG4, ARG5, ARG6) \
  sl_log_send_arg6(EVENT, SL_LOG_CONFIG_LEVEL_INFO << 1 | EVENT_TYPE, ARG1,    \
                   ARG2, ARG3, ARG4, ARG5, ARG6);
#endif

#if (SL_LOG_CONFIG_ARG >= 7)
#define SL_PRINT_ARG7_DBG(EVENT, EVENT_TYPE, ARG1, ARG2, ARG3, ARG4, ARG5, ARG6, ARG7) \
  sl_log_send_arg7(EVENT, SL_LOG_CONFIG_LEVEL_DEBUG << 1 | EVENT_TYPE, ARG1,  \
                   ARG2, ARG3, ARG4, ARG5, ARG6, ARG7);
#define SL_PRINT_ARG7_ERR(EVENT, EVENT_TYPE, ARG1, ARG2, ARG3, ARG4, ARG5, ARG6, ARG7) \
  sl_log_send_arg7(EVENT, SL_LOG_CONFIG_LEVEL_ERROR << 1 | EVENT_TYPE, ARG1,  \
                   ARG2, ARG3, ARG4, ARG5, ARG6, ARG7);
#define SL_PRINT_ARG7_WRN(EVENT, EVENT_TYPE, ARG1, ARG2, ARG3, ARG4, ARG5, ARG6, ARG7) \
  sl_log_send_arg7(EVENT, SL_LOG_CONFIG_LEVEL_WARN << 1 | EVENT_TYPE, ARG1,    \
                   ARG2, ARG3, ARG4, ARG5, ARG6, ARG7);
#define SL_PRINT_ARG7_INFO(EVENT, EVENT_TYPE, ARG1, ARG2, ARG3, ARG4, ARG5, ARG6, ARG7) \
  sl_log_send_arg7(EVENT, SL_LOG_CONFIG_LEVEL_INFO << 1 | EVENT_TYPE, ARG1,   \
                   ARG2, ARG3, ARG4, ARG5, ARG6, ARG7);
#endif

#if (SL_LOG_CONFIG_ARG >= 8)
#define SL_PRINT_ARG8_DBG(EVENT, EVENT_TYPE, ARG1, ARG2, ARG3, ARG4, ARG5, ARG6, ARG7, ARG8) \
  sl_log_send_arg8(EVENT, SL_LOG_CONFIG_LEVEL_DEBUG << 1 | EVENT_TYPE, ARG1,  \
                   ARG2, ARG3, ARG4, ARG5, ARG6, ARG7, ARG8);
#define SL_PRINT_ARG8_ERR(EVENT, EVENT_TYPE, ARG1, ARG2, ARG3, ARG4, ARG5, ARG6, ARG7, ARG8) \
  sl_log_send_arg8(EVENT, SL_LOG_CONFIG_LEVEL_ERROR << 1 | EVENT_TYPE, ARG1,  \
                   ARG2, ARG3, ARG4, ARG5, ARG6, ARG7, ARG8);
#define SL_PRINT_ARG8_WRN(EVENT, EVENT_TYPE, ARG1, ARG2, ARG3, ARG4, ARG5, ARG6, ARG7, ARG8) \
  sl_log_send_arg8(EVENT, SL_LOG_CONFIG_LEVEL_WARN << 1 | EVENT_TYPE, ARG1,    \
                   ARG2, ARG3, ARG4, ARG5, ARG6, ARG7, ARG8);
#define SL_PRINT_ARG8_INFO(EVENT, EVENT_TYPE, ARG1, ARG2, ARG3, ARG4, ARG5, ARG6, ARG7, ARG8) \
  sl_log_send_arg8(EVENT, SL_LOG_CONFIG_LEVEL_INFO << 1 | EVENT_TYPE, ARG1,   \
                   ARG2, ARG3, ARG4, ARG5, ARG6, ARG7, ARG8);
#endif

#if (SL_LOG_CONFIG_ARG >= 9)
#define SL_PRINT_ARG9_DBG(EVENT, EVENT_TYPE, ARG1, ARG2, ARG3, ARG4, ARG5, ARG6, ARG7, ARG8, ARG9) \
  sl_log_send_arg9(EVENT, SL_LOG_CONFIG_LEVEL_DEBUG << 1 | EVENT_TYPE, ARG1,  \
                   ARG2, ARG3, ARG4, ARG5, ARG6, ARG7, ARG8, ARG9);
#define SL_PRINT_ARG9_ERR(EVENT, EVENT_TYPE, ARG1, ARG2, ARG3, ARG4, ARG5, ARG6, ARG7, ARG8, ARG9) \
  sl_log_send_arg9(EVENT, SL_LOG_CONFIG_LEVEL_ERROR << 1 | EVENT_TYPE, ARG1,  \
                   ARG2, ARG3, ARG4, ARG5, ARG6, ARG7, ARG8, ARG9);
#define SL_PRINT_ARG9_WRN(EVENT, EVENT_TYPE, ARG1, ARG2, ARG3, ARG4, ARG5, ARG6, ARG7, ARG8, ARG9) \
  sl_log_send_arg9(EVENT, SL_LOG_CONFIG_LEVEL_WARN << 1 | EVENT_TYPE, ARG1,    \
                   ARG2, ARG3, ARG4, ARG5, ARG6, ARG7, ARG8, ARG9);
#define SL_PRINT_ARG9_INFO(EVENT, EVENT_TYPE, ARG1, ARG2, ARG3, ARG4, ARG5, ARG6, ARG7, ARG8, ARG9) \
  sl_log_send_arg9(EVENT, SL_LOG_CONFIG_LEVEL_INFO << 1 | EVENT_TYPE, ARG1,   \
                   ARG2, ARG3, ARG4, ARG5, ARG6, ARG7, ARG8, ARG9);
#endif

#if (SL_LOG_CONFIG_ARG >= 10)
#define SL_PRINT_ARG10_DBG(EVENT, EVENT_TYPE, ARG1, ARG2, ARG3, ARG4, ARG5, ARG6, ARG7, ARG8, ARG9, ARG10) \
  sl_log_send_arg10(EVENT, SL_LOG_CONFIG_LEVEL_DEBUG << 1 | EVENT_TYPE, ARG1,  \
                    ARG2, ARG3, ARG4, ARG5, ARG6, ARG7, ARG8, ARG9, ARG10);
#define SL_PRINT_ARG10_ERR(EVENT, EVENT_TYPE, ARG1, ARG2, ARG3, ARG4, ARG5, ARG6, ARG7, ARG8, ARG9, ARG10) \
  sl_log_send_arg10(EVENT, SL_LOG_CONFIG_LEVEL_ERROR << 1 | EVENT_TYPE, ARG1,  \
                    ARG2, ARG3, ARG4, ARG5, ARG6, ARG7, ARG8, ARG9, ARG10);
#define SL_PRINT_ARG10_WRN(EVENT, EVENT_TYPE, ARG1, ARG2, ARG3, ARG4, ARG5, ARG6, ARG7, ARG8, ARG9, ARG10) \
  sl_log_send_arg10(EVENT, SL_LOG_CONFIG_LEVEL_WARN << 1 | EVENT_TYPE, ARG1,    \
                    ARG2, ARG3, ARG4, ARG5, ARG6, ARG7, ARG8, ARG9, ARG10);
#define SL_PRINT_ARG10_INFO(EVENT, EVENT_TYPE, ARG1, ARG2, ARG3, ARG4, ARG5, ARG6, ARG7, ARG8, ARG9, ARG10) \
  sl_log_send_arg10(EVENT, SL_LOG_CONFIG_LEVEL_INFO << 1 | EVENT_TYPE, ARG1,   \
                    ARG2, ARG3, ARG4, ARG5, ARG6, ARG7, ARG8, ARG9, ARG10);
#endif

/** @} (end addtogroup sl_log_active_macros) */

#endif



/**
 * @defgroup sl_log_level_checks Log Level Check Macros
 * @brief Macros for checking if a log level should be processed
 *
 * These macros provide efficient level checking to determine if a log
 * message should be processed based on the current log level configuration.
 *
 * @{
 */

/** @brief Check if debug level logging is enabled for the given level */
#define SL_LOG_LVL_DEBUG_CHECK(level) (level <= SL_LOG_CONFIG_LEVEL_DEBUG)

/** @brief Check if info level logging is enabled for the given level */
#define SL_LOG_LVL_INFO_CHECK(level) (level <= SL_LOG_CONFIG_LEVEL_INFO)

/** @brief Check if error level logging is enabled for the given level */
#define SL_LOG_LVL_ERROR_CHECK(level) (level <= SL_LOG_CONFIG_LEVEL_ERROR)

/** @brief Check if warning level logging is enabled for the given level */
#define SL_LOG_LVL_WARN_CHECK(level) (level <= SL_LOG_CONFIG_LEVEL_WARN)

/** @} (end addtogroup sl_log_level_checks) */

/**
 * @defgroup sl_log_macro_helpers Macro Processing Helpers
 * @brief Helper macros for token concatenation and argument processing
 *
 * These macros provide the foundation for the variadic logging system,
 * enabling automatic selection of appropriate logging functions based
 * on argument count and generating unique identifiers.
 *
 * @{
 */

/** @brief Token concatenation helper */
#define CONCAT(a, b) a##b

/** @brief Expanded token concatenation helper */
#define EXPAND_CONCAT(a, b) CONCAT(a, b)

/** @brief Generate unique name based on line number */
#define UNIQUE_NAME(base) EXPAND_CONCAT(base, EXPAND_CONCAT(__LINE__, 0))

/** @brief Implementation macro for counting variadic arguments (up to 10) */
#define COUNT_ARGS_IMPL(_0, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, N, ...) N

/** @brief Count the number of variadic arguments (up to 10) */
#define COUNT_ARGS(fmt, ...) COUNT_ARGS_IMPL(_, ##__VA_ARGS__, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0)

/** @brief Helper for choosing logging macro based on argument count */
#define LOG_MACRO_CHOOSER2(count) EXPAND_CONCAT(SL_PRINT_ARG, count##_)

/** @brief Macro dispatcher for selecting appropriate logging function */
#define LOG_VOID_MACRO_CHOOSER2(count) EXPAND_CONCAT(SL_PRINT_VOID_ARG, count##_)

/** @brief Macro dispatcher for selecting appropriate logging function */
#define LOG_VOID_MACRO_CHOOSER1(count) LOG_VOID_MACRO_CHOOSER2(count)

/** @brief Macro dispatcher for selecting appropriate logging function */
#define LOG_MACRO_CHOOSER1(count) LOG_MACRO_CHOOSER2(count)

/** @} (end addtogroup sl_log_macro_helpers) */

/**
 * @defgroup sl_log_common_macros Common Logging Macros
 * @brief Core macros for printf-style and event-based logging
 *
 * These macros provide the foundation for both printf-style string logging
 * and numeric event-based logging with compile-time argument validation.
 *
 * @{
 */

/**
 * @brief Common printf-style logging macro
 *
 * Creates a static format string in the log section and calls the appropriate
 * logging function based on argument count. Includes compile-time validation
 * to ensure no more than SL_LOG_CONFIG_ARG arguments are provided.
 *
 * @param level Log level suffix (INFO, DBG, ERR, WRN)
 * @param fmt Format string
 * @param ... Variable arguments (up to 10)
 */
#define sl_printf_common(level, fmt, ...)                                      \
  static const char UNIQUE_NAME(logstr_)[] SL_COMPACT_STRINGS_SECTION = fmt;               \
  _Static_assert(COUNT_ARGS(fmt, ##__VA_ARGS__) <= SL_LOG_CONFIG_ARG, "Too many arguments!");  \
  EXPAND_CONCAT(LOG_MACRO_CHOOSER1(COUNT_ARGS(fmt, ##__VA_ARGS__)), level)     \
  ((uintptr_t)UNIQUE_NAME(logstr_), 0, ##__VA_ARGS__)

/**
 * @brief Common event-based logging macro
 *
 * Logs a numeric event ID with optional arguments. Includes compile-time
 * validation to ensure no more than SL_LOG_CONFIG_ARG arguments are provided.
 *
 * @param level Log level suffix (INFO, DBG, ERR, WRN)
 * @param event_id Numeric event identifier
 * @param ... Variable arguments (up to 10)
 */
#define sl_event_common(level, event_id, ...)                                  \
  _Static_assert(COUNT_ARGS(event_id, ##__VA_ARGS__) <= SL_LOG_CONFIG_ARG, "Too many arguments!");  \
  EXPAND_CONCAT(LOG_MACRO_CHOOSER1(COUNT_ARGS(event_id, ##__VA_ARGS__)), level)     \
  (event_id, 1, ##__VA_ARGS__)

/** @} (end addtogroup sl_log_common_macros) */

/***************************************************************************//**
 * @addtogroup assert ASSERT - Assert
 * @brief Enhanced assert/error checking module with detailed logging
 * @details
 * This implementation provides comprehensive assert handling with:
 * - File name, line number, and expression logging
 * - Debug session detection and breakpoint triggering
 * - Conditional debug asserts via SL_LOG_DEBUG_ASSERT_ENABLE flag
 *
 * Assert Macros:
 * - SL_LOG_CRASH_ASSERT: Always active, triggers assert handler
 * - SL_LOG_DEBUG_ASSERT: Active only when SL_LOG_DEBUG_ASSERT_ENABLE is defined
 * - assert: Core assert implementation with detailed logging
 * @{
 ******************************************************************************/

/***************************************************************************//**
 * @brief
 *    Core assert implementation with file, line, and expression logging
 * @details
 *    If the condition is false, constructs a detailed error string and
 *    passes it to sli_assert_implementation for handling.
 ******************************************************************************/
#define debug_assert(__e) \
    ((__e) \
        ? (void)0 \
        : sli_log_assert_implementation( \
            __FILE__ ":" SL_STRINGIFY(__LINE__) " - Assertion failed: " #__e) \
)


/***************************************************************************//**
* @brief
*    Crash assert - always enabled regardless of SL_LOG_DEBUG_ASSERT_ENABLE configuration
******************************************************************************/

#define SL_LOG_CRASH_ASSERT(condition) debug_assert(condition)

/***************************************************************************//**
* @brief
*    Debug assert - enabled only when SL_LOG_DEBUG_ASSERT_ENABLE is defined
* @details
*    In debug builds, maps to SL_LOG_CRASH_ASSERT for full error handling.
*    In release builds, this macro is disabled (no-op) to save code space.
******************************************************************************/

#if SL_LOG_DEBUG_ASSERT_ENABLE
#define SL_LOG_DEBUG_ASSERT(condition) SL_LOG_CRASH_ASSERT(condition) // If SL_LOG_DEBUG is non-zero, enable debug asserts
#else
#define SL_LOG_DEBUG_ASSERT(condition) ((void)0) // If SL_LOG_DEBUG is 0, compile out the assert (no-op)
#endif


/** @} (end addtogroup SL_DEBUG_ASSERT) */

#if defined(LIBRARY_BUILD) || defined(SL_CATALOG_LOG_COMPONENT_PRESENT)

/**
 * @defgroup sl_log_printf_api Printf-Style Logging API
 * @brief High-level printf-style logging macros with level filtering
 *
 * These macros provide the main user-facing API for printf-style logging.
 * They include both compile-time and runtime level filtering for optimal
 * performance and flexibility.
 *
 * @{
 */

/**
 * @brief Print info-level message with printf-style formatting
 *
 * Logs a message at INFO level if either compile-time or runtime
 * configuration allows info-level logging.
 *
 * @param fmt Printf-style format string
 * @param ... Variable arguments for format string (up to 10)
 */
#if (SL_LOG_CONFIG_LEVEL_COMPILE_TIME == SL_LOG_CONFIG_LEVEL_NONE)
#define SL_PRINT_STRING_INFO(fmt, ...) do{ }while(0)
#else
#define SL_PRINT_STRING_INFO(fmt, ...) do{sl_printf_common(INFO, fmt, ##__VA_ARGS__) }while(0)
#endif

/**
 * @brief Print debug-level message with printf-style formatting
 *
 * Logs a message at DEBUG level if either compile-time or runtime
 * configuration allows debug-level logging.
 *
 * @param fmt Printf-style format string
 * @param ... Variable arguments for format string (up to 10)
 */
#if (SL_LOG_CONFIG_LEVEL_COMPILE_TIME == SL_LOG_CONFIG_LEVEL_NONE)
#define SL_PRINT_STRING_DEBUG(fmt, ...) do{ }while(0)
#else
#define SL_PRINT_STRING_DEBUG(fmt, ...) do{sl_printf_common(DBG, fmt, ##__VA_ARGS__) }while(0)
#endif

/**
 * @brief Print warning-level message with printf-style formatting
 *
 * Logs a message at WARNING level if either compile-time or runtime
 * configuration allows warning-level logging.
 *
 * @param fmt Printf-style format string
 * @param ... Variable arguments for format string (up to 10)
 */
#if (SL_LOG_CONFIG_LEVEL_COMPILE_TIME == SL_LOG_CONFIG_LEVEL_NONE)
#define SL_PRINT_STRING_WARN(fmt, ...) do{ }while(0)
#else
#define SL_PRINT_STRING_WARN(fmt, ...) do{sl_printf_common(WRN, fmt, ##__VA_ARGS__) }while(0)
#endif

/**
 * @brief Print error-level message with printf-style formatting
 *
 * Logs a message at ERROR level if either compile-time or runtime
 * configuration allows error-level logging.
 *
 * @param fmt Printf-style format string
 * @param ... Variable arguments for format string (up to 10)
 */
#if (SL_LOG_CONFIG_LEVEL_COMPILE_TIME == SL_LOG_CONFIG_LEVEL_NONE)
#define SL_PRINT_STRING_ERROR(fmt, ...) do{ }while(0)
#else
#define SL_PRINT_STRING_ERROR(fmt, ...) do{sl_printf_common(ERR, fmt, ##__VA_ARGS__) }while(0)
#endif

/** @} (end addtogroup sl_log_printf_api) */

/**
 * @defgroup sl_log_event_api Event-Based Logging API
 * @brief High-level event-based logging macros with level filtering
 *
 * These macros provide the main user-facing API for event-based logging
 * using numeric event IDs. They include both compile-time and runtime
 * level filtering for optimal performance and flexibility.
 *
 * @{
 */

/**
 * @brief Log info-level event with optional arguments
 *
 * Logs an event at INFO level if either compile-time or runtime
 * configuration allows info-level logging.
 *
 * @param event_id Numeric event identifier
 * @param ... Variable arguments for the event (up to 10)
 */
#if (SL_LOG_CONFIG_LEVEL_COMPILE_TIME == SL_LOG_CONFIG_LEVEL_NONE)
#define SL_PRINT_EVENT_INFO(event_id, ...) do{ }while(0)
#else
#define SL_PRINT_EVENT_INFO(event_id, ...) do{sl_event_common(INFO, event_id, ##__VA_ARGS__) }while(0)
#endif

/**
 * @brief Log debug-level event with optional arguments
 *
 * Logs an event at DEBUG level if either compile-time or runtime
 * configuration allows debug-level logging.
 *
 * @param event_id Numeric event identifier
 * @param ... Variable arguments for the event (up to 10)
 */
#if (SL_LOG_CONFIG_LEVEL_COMPILE_TIME == SL_LOG_CONFIG_LEVEL_NONE)
#define SL_PRINT_EVENT_DEBUG(event_id, ...) do{ }while(0)
#else
#define SL_PRINT_EVENT_DEBUG(event_id, ...) do{sl_event_common(DBG, event_id, ##__VA_ARGS__) }while(0)
#endif
/**
 * @brief Log warning-level event with optional arguments
 *
 * Logs an event at WARNING level if either compile-time or runtime
 * configuration allows warning-level logging.
 *
 * @param event_id Numeric event identifier
 * @param ... Variable arguments for the event (up to 10)
 */
#if (SL_LOG_CONFIG_LEVEL_COMPILE_TIME == SL_LOG_CONFIG_LEVEL_NONE)
#define SL_PRINT_EVENT_WARN(event_id, ...) do{ }while(0)
#else
#define SL_PRINT_EVENT_WARN(event_id, ...) do{sl_event_common(WRN, event_id, ##__VA_ARGS__) }while(0)
#endif
/**
 * @brief Log error-level event with optional arguments
 *
 * Logs an event at ERROR level if either compile-time or runtime
 * configuration allows error-level logging.
 *
 * @param event_id Numeric event identifier
 * @param ... Variable arguments for the event (up to 10)
 */
#if (SL_LOG_CONFIG_LEVEL_COMPILE_TIME == SL_LOG_CONFIG_LEVEL_NONE)
#define SL_PRINT_EVENT_ERROR(event_id, ...) do{ }while(0)
#else
#define SL_PRINT_EVENT_ERROR(event_id, ...) do{sl_event_common(ERR, event_id, ##__VA_ARGS__) }while(0)
#endif

/** @} (end addtogroup sl_log_event_api) */


#else // LIBRARY_BUILD
/**
 * @defgroup sl_log_common_macros Common Logging Macros
 * @brief Core macros for printf-style and event-based logging
 *
 * These macros provide the foundation for both printf-style string logging
 * and numeric event-based logging with compile-time argument validation.
 *
 * @{
 */

/**
 * @brief Common printf-style logging macro
 *
 * This apis discards the string provided by the user and and send empty string address magic word
 *
 * @param level Log level suffix (INFO, DBG, ERR, WRN)
 * @param fmt Format string
 * @param ... Variable arguments (up to 10)
 */

#define sl_log_common_void(level, event_id, ...)                                  \
  _Static_assert(COUNT_ARGS(event_id, ##__VA_ARGS__) <= SL_LOG_CONFIG_ARG, "Too many arguments!");  \
  EXPAND_CONCAT(LOG_VOID_MACRO_CHOOSER1(COUNT_ARGS(event_id, ##__VA_ARGS__)), level)     \
  (event_id, 1, ##__VA_ARGS__)

/* Compile-time–controlled PRINTF-style macros */
#if (SL_LOG_CONFIG_LEVEL_COMPILE_TIME == SL_LOG_CONFIG_LEVEL_NONE)
#define SL_PRINT_STRING_INFO(fmt, ...) do{ }while(0)
#define SL_PRINT_STRING_DEBUG(fmt, ...) do{ }while(0)
#define SL_PRINT_STRING_WARN(fmt, ...) do{ }while(0)
#define SL_PRINT_STRING_ERROR(fmt, ...) do{ }while(0)
#else
#define SL_PRINT_STRING_INFO(fmt, ...)  sl_log_common_void(INFO, fmt, ##__VA_ARGS__)
#define SL_PRINT_STRING_DEBUG(fmt, ...) sl_log_common_void(DBG, fmt, ##__VA_ARGS__)
#define SL_PRINT_STRING_WARN(fmt, ...)  sl_log_common_void(WRN, fmt, ##__VA_ARGS__)
#define SL_PRINT_STRING_ERROR(fmt, ...) sl_log_common_void(ERR, fmt, ##__VA_ARGS__)
#endif

/* Compile-time–controlled EVENT-style macros */
#if (SL_LOG_CONFIG_LEVEL_COMPILE_TIME == SL_LOG_CONFIG_LEVEL_NONE)
#define SL_PRINT_EVENT_INFO(event_id, ...) do{ }while(0)
#define SL_PRINT_EVENT_DEBUG(event_id, ...) do{ }while(0)
#define SL_PRINT_EVENT_WARN(event_id, ...) do{ }while(0)
#define SL_PRINT_EVENT_ERROR(event_id, ...) do{ }while(0)
#else
#define SL_PRINT_EVENT_INFO(event_id, ...)  sl_log_common_void(INFO, event_id, ##__VA_ARGS__)
#define SL_PRINT_EVENT_DEBUG(event_id, ...) sl_log_common_void(DBG, event_id, ##__VA_ARGS__)
#define SL_PRINT_EVENT_WARN(event_id, ...)  sl_log_common_void(WRN, event_id, ##__VA_ARGS__)
#define SL_PRINT_EVENT_ERROR(event_id, ...) sl_log_common_void(ERR, event_id, ##__VA_ARGS__)
#endif

#endif // LIBRARY_BUILD

#ifdef __cplusplus
}
#endif

#endif // SL_LOG_HELPER_H
