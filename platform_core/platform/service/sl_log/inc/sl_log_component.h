/***************************************************************************//**
 * @file
 * @brief Component-scoped tracing helpers for the Debug Logger (sl_log).
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

#ifndef SL_LOG_COMPONENT_H
#define SL_LOG_COMPONENT_H

#include "sl_log.h"
#include "sl_log_helper.h"

/***************************************************************************//**
 * @addtogroup sl_log_component Component-Scoped Logging
 * @brief Per-component compile-time and optional runtime log filtering.
 *
 * The Debug Logger offers a single global level. A component that wants its
 * own level so it can be traced without drowning the output in traces from
 * every other component builds its macro set on top of the two macros below.
 *
 * Each component owns a private header that pairs a compile-time level macro,
 * exposed as a configuration knob in the component's config header, with one
 * macro per log level. The level must be tested with `#if` rather than a
 * folded `if`, because @ref SL_PRINT_STRING_DEBUG and friends emit their
 * format string into the dedicated `.log_fmt` section; an unreachable call
 * still costs flash.
 *
 * The header also owns a short tag that identifies the component in the
 * output. No backend prints a module or file name of its own - the formatted
 * iostream backend prefixes only an optional timestamp and core ID - so
 * without a tag a line cannot be attributed to its component.
 *
 * A severity is built only when both the component level and
 * @ref SL_LOG_CONFIG_LEVEL_COMPILE_TIME admit it. The component setting
 * cannot enable a severity excluded globally. For code that remains, the
 * global runtime level (@ref sl_log_set_loglevel) controls emission.
 * Components that need a debugger-adjustable runtime level can use
 * @ref SLI_LOG_COMPONENT_PRINT_RUNTIME. That level can only narrow the
 * severities retained at compile time.
 *
 * A component header looks like this:
 *
 * @code
 * #include "sl_log_component.h"
 * #include "sl_foo_config.h"
 *
 * #ifndef SL_FOO_LOG_LEVEL_COMPILE_TIME
 * #define SL_FOO_LOG_LEVEL_COMPILE_TIME  SL_LOG_CONFIG_LEVEL_NONE
 * #endif
 *
 * #define SLI_FOO_LOG_TAG  "FOO: "
 *
 * #if ((SL_FOO_LOG_LEVEL_COMPILE_TIME <= SL_LOG_CONFIG_LEVEL_DEBUG) \
 *   && (SL_LOG_CONFIG_LEVEL_COMPILE_TIME <= SL_LOG_CONFIG_LEVEL_DEBUG))
 * #define SLI_FOO_LOG_DEBUG(format, ...) \
 *   SLI_LOG_COMPONENT_PRINT(DEBUG, SLI_FOO_LOG_TAG, format, ## __VA_ARGS__)
 * #else
 * #define SLI_FOO_LOG_DEBUG(format, ...) \
 *   SLI_LOG_COMPONENT_PRINT_NONE(format, ## __VA_ARGS__)
 * #endif
 * @endcode
 *
 * @{
 ******************************************************************************/

/***************************************************************************//**
 * Emit a component trace.
 *
 * @param level   Log level name: DEBUG, INFO, WARN or ERROR.
 * @param tag     Short component tag literal, concatenated ahead of @p format.
 * @param format  Format string literal.
 * @param ...     Up to SL_LOG_CONFIG_ARG arguments.
 *
 * @note @p tag and @p format are concatenated by the preprocessor, so both
 *       must be string literals. Carrying the tag here rather than in every
 *       trace keeps it consistent and off the call sites.
 ******************************************************************************/
#define SLI_LOG_COMPONENT_PRINT(level, tag, format, ...) \
  SL_PRINT_STRING_ ## level(tag format, ## __VA_ARGS__)

/***************************************************************************//**
 * Emit a component trace if its runtime level admits the severity.
 *
 * @param level          Log level name: DEBUG, INFO, WARN or ERROR.
 * @param runtime_level  Component runtime @ref sl_log_level_t variable.
 * @param tag            Short component tag literal.
 * @param format         Format string literal.
 * @param ...            Up to SL_LOG_CONFIG_ARG arguments.
 ******************************************************************************/
#define SLI_LOG_COMPONENT_PRINT_RUNTIME(level, runtime_level, tag, format, ...) \
  do {                                                                          \
    if ((runtime_level) <= (sl_log_level_t)SL_LOG_CONFIG_LEVEL_ ## level) {      \
      SLI_LOG_COMPONENT_PRINT(level, tag, format, ## __VA_ARGS__);              \
    }                                                                           \
  } while (0)

/***************************************************************************//**
 * Discard a component trace that is disabled at compile time.
 *
 * @note The arguments are not evaluated and not even referenced, so a trace
 *       must never be the sole user of a local variable (unused-variable
 *       warning) and must never carry a side-effecting argument.
 ******************************************************************************/
#define SLI_LOG_COMPONENT_PRINT_NONE(format, ...) \
  do {                                            \
  } while (0)

/** @} (end addtogroup sl_log_component) */

#endif /* SL_LOG_COMPONENT_H */
