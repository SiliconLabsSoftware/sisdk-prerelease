/***************************************************************************//**
 * @file
 * @brief CS RREQ - logging interface header
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

#ifndef CS_RREQ_LOG_H
#define CS_RREQ_LOG_H

#include "sl_component_catalog.h"
#include "cs_rreq_config.h"

#if defined(SL_CATALOG_APP_LOG_PRESENT) && CS_RREQ_CONFIG_LOG
#include "app_log.h"
#define LOG_PREFIX                  CS_RREQ_CONFIG_LOG_PREFIX " "
#define LOG_NL                      APP_LOG_NL
#define INSTANCE_PREFIX             "[%u] "
#define rreq_log_wrap(code)  code
#else
// Logging disabled: all log macros expand to nothing
#define rreq_log_wrap(...)
#endif // defined(SL_CATALOG_APP_LOG_PRESENT) && CS_RREQ_CONFIG_LOG

// Log API definitionsa
#define rreq_log_debug(...)         rreq_log_wrap(app_log_debug(LOG_PREFIX  __VA_ARGS__))
#define rreq_log_info(...)          rreq_log_wrap(app_log_info(LOG_PREFIX  __VA_ARGS__))
#define rreq_log_warning(...)       rreq_log_wrap(app_log_warning(LOG_PREFIX  __VA_ARGS__))
#define rreq_log_error(...)         rreq_log_wrap(app_log_error(LOG_PREFIX  __VA_ARGS__))
#define rreq_log_critical(...)      rreq_log_wrap(app_log_critical(LOG_PREFIX  __VA_ARGS__))
#define rreq_log_hexdump_debug(...) rreq_log_wrap(app_log_hexdump_debug(__VA_ARGS__))
#define rreq_log_append_debug(...)  rreq_log_wrap(app_log_append_debug(__VA_ARGS__))

#endif // CS_RREQ_LOG_H
