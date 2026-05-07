/***************************************************************************//**
 * @file
 * @brief CS algo - logging interface header
 *******************************************************************************
 * # License
 * <b>Copyright 2024 Silicon Laboratories Inc. www.silabs.com</b>
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

#ifndef CS_ALGO_LOG_H
#define CS_ALGO_LOG_H

#include "sl_component_catalog.h"
#include "cs_algo_config.h"

#if defined(SL_CATALOG_APP_LOG_PRESENT) && CS_ALGO_LOG
#include "app_log.h"
#define LOG_PREFIX                  CS_ALGO_LOG_PREFIX " "
#define LOG_NL                      APP_LOG_NL
#define INSTANCE_PREFIX             "[%u] "
#define algo_log_wrap(code)         code
#else
#define algo_log_wrap(...)
#endif // defined(SL_CATALOG_APP_LOG_PRESENT) && CS_ALGO_LOG

#define algo_log_debug(...)         algo_log_wrap(app_log_debug(LOG_PREFIX  __VA_ARGS__))
#define algo_log_info(...)          algo_log_wrap(app_log_info(LOG_PREFIX  __VA_ARGS__))
#define algo_log_error(...)         algo_log_wrap(app_log_error(LOG_PREFIX  __VA_ARGS__))

#endif // CS_ALGO_LOG_H
