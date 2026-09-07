/***************************************************************************//**
 * @file
 * @brief Clock Manager tracing macros.
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

#ifndef SLI_CLOCK_MANAGER_LOG_H_
#define SLI_CLOCK_MANAGER_LOG_H_

#include "sl_log_component.h"
#include "sl_clock_manager_log_config.h"

/// Compile-time trace level for the Clock Manager. Traces above this level are
/// not built at all. Set it from the component configuration, or from the
/// project with `-DSL_CLOCK_MANAGER_LOG_LEVEL_COMPILE_TIME=SL_LOG_CONFIG_LEVEL_DEBUG`.
/// The global compile-time level may restrict this further.
///
/// @note The Clock Manager initializes after sl_log_init_stage1() but before
///       the backend transport is up, so init-path traces are held in the
///       early ring buffer and only appear once stage 2 flushes it. That
///       buffer holds 20 events and overwrites the oldest, so at DEBUG the
///       first oscillator traces can be lost; every pre-stage-2 event also
///       carries timestamp 0. The formatted backend discards them outright.
#ifndef SL_CLOCK_MANAGER_LOG_LEVEL_COMPILE_TIME
#define SL_CLOCK_MANAGER_LOG_LEVEL_COMPILE_TIME  SL_LOG_CONFIG_LEVEL_NONE
#endif

/// Tag prepended to every Clock Manager trace. No backend prints a module name
/// of its own, so this is what attributes an output line to this component.
#define SLI_CLOCK_MANAGER_LOG_TAG  "CM: "

#if ((SL_CLOCK_MANAGER_LOG_LEVEL_COMPILE_TIME <= SL_LOG_CONFIG_LEVEL_DEBUG) \
  && (SL_LOG_CONFIG_LEVEL_COMPILE_TIME <= SL_LOG_CONFIG_LEVEL_DEBUG))
#define SLI_CLOCK_MANAGER_LOG_DEBUG(format, ...) \
  SLI_LOG_COMPONENT_PRINT(DEBUG, SLI_CLOCK_MANAGER_LOG_TAG, format, ## __VA_ARGS__)
#else
#define SLI_CLOCK_MANAGER_LOG_DEBUG(format, ...) \
  SLI_LOG_COMPONENT_PRINT_NONE(format, ## __VA_ARGS__)
#endif

#if ((SL_CLOCK_MANAGER_LOG_LEVEL_COMPILE_TIME <= SL_LOG_CONFIG_LEVEL_INFO) \
  && (SL_LOG_CONFIG_LEVEL_COMPILE_TIME <= SL_LOG_CONFIG_LEVEL_INFO))
#define SLI_CLOCK_MANAGER_LOG_INFO(format, ...) \
  SLI_LOG_COMPONENT_PRINT(INFO, SLI_CLOCK_MANAGER_LOG_TAG, format, ## __VA_ARGS__)
#else
#define SLI_CLOCK_MANAGER_LOG_INFO(format, ...) \
  SLI_LOG_COMPONENT_PRINT_NONE(format, ## __VA_ARGS__)
#endif

#if ((SL_CLOCK_MANAGER_LOG_LEVEL_COMPILE_TIME <= SL_LOG_CONFIG_LEVEL_WARN) \
  && (SL_LOG_CONFIG_LEVEL_COMPILE_TIME <= SL_LOG_CONFIG_LEVEL_WARN))
#define SLI_CLOCK_MANAGER_LOG_WARN(format, ...) \
  SLI_LOG_COMPONENT_PRINT(WARN, SLI_CLOCK_MANAGER_LOG_TAG, format, ## __VA_ARGS__)
#else
#define SLI_CLOCK_MANAGER_LOG_WARN(format, ...) \
  SLI_LOG_COMPONENT_PRINT_NONE(format, ## __VA_ARGS__)
#endif

#if ((SL_CLOCK_MANAGER_LOG_LEVEL_COMPILE_TIME <= SL_LOG_CONFIG_LEVEL_ERROR) \
  && (SL_LOG_CONFIG_LEVEL_COMPILE_TIME <= SL_LOG_CONFIG_LEVEL_ERROR))
#define SLI_CLOCK_MANAGER_LOG_ERROR(format, ...) \
  SLI_LOG_COMPONENT_PRINT(ERROR, SLI_CLOCK_MANAGER_LOG_TAG, format, ## __VA_ARGS__)
#else
#define SLI_CLOCK_MANAGER_LOG_ERROR(format, ...) \
  SLI_LOG_COMPONENT_PRINT_NONE(format, ## __VA_ARGS__)
#endif

#endif /* SLI_CLOCK_MANAGER_LOG_H_ */
