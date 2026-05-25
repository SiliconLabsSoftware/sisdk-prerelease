/***************************************************************************//**
 * @file
 * @brief CS Manager - Core implementation
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
 ******************************************************************************/

#ifndef CS_MANAGER_LOG_INTERNAL_H
#define CS_MANAGER_LOG_INTERNAL_H

// -----------------------------------------------------------------------------
// Includes

#include "sl_component_catalog.h"
#if defined(SL_CATALOG_APP_LOG_PRESENT) && CS_MANAGER_CONFIG_LOG_ENABLE
#include "app_log.h"
#endif

// -----------------------------------------------------------------------------
// Macros

#if defined(SL_CATALOG_APP_LOG_PRESENT) && CS_MANAGER_CONFIG_LOG_ENABLE
#define LOG_PREFIX                  CS_MANAGER_CONFIG_LOG_PREFIX " "
#define NL                          APP_LOG_NL
#define INSTANCE_PREFIX             "[%u] "
#define cs_manager_log_debug(...)    app_log_debug(LOG_PREFIX  __VA_ARGS__)
#define cs_manager_log_info(...)     app_log_info(LOG_PREFIX  __VA_ARGS__)
#define cs_manager_log_warning(...)  app_log_warning(LOG_PREFIX  __VA_ARGS__)
#define cs_manager_log_error(...)    app_log_error(LOG_PREFIX  __VA_ARGS__)
#define cs_manager_log_critical(...) app_log_critical(LOG_PREFIX  __VA_ARGS__)
#else
#define NL
#define INSTANCE_PREFIX
#define cs_manager_log_debug(...)
#define cs_manager_log_info(...)
#define cs_manager_log_warning(...)
#define cs_manager_log_error(...)
#define cs_manager_log_critical(...)
#endif

#endif // CS_MANAGER_LOG_INTERNAL_H
