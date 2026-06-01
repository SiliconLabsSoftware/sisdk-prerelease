/***************************************************************************//**
 * @file
 * @brief Target-side printf implementation for the I/O Stream log backend.
 *
 * Provides the proprietary-backend implementation of sl_log_vprint_target_ex(),
 * the va_list primitive that the single variadic wrapper
 * SL_LOG_PRINT_TARGET_EX forwards to. SL_PRINT_FMT_* macros declared in
 * sl_log_helper.h reach this code path. The text is formatted on target with
 * vsnprintf() and
 * emitted to the recommended console iostream as a single line of the form:
 *
 *   [L|F|TIMESTAMP] <formatted text>\r\n
 *
 * where L is one of I (info / log), W (warning) or E (error), F denotes
 * the "formatted printf" payload type (distinct from the S/E payload types
 * used by sl_log_backend_iostream_formatted.c), and TIMESTAMP is the current
 * host-core timestamp counter.
 *
 * This file is shared by both iostream output formats (formatted and compact)
 * because the SL_PRINT_FMT_* path is independent of the regular event /
 * string-log encoding. When the compact (binary) format is selected, the text
 * line emitted here is interleaved into the otherwise-binary stream: this is
 * intentional and is the documented trade-off for using SL_PRINT_FMT_* on the
 * compact backend.
 *
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

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stddef.h>

#include "sl_log.h"
#include "sl_iostream.h"
#include "sl_iostream_handles.h"

/** Maximum size (bytes) of one printf line emitted to iostream, including the
 *  bracketed header and the trailing CRLF. Lines longer than this are
 *  truncated. */
#ifndef SL_LOG_PRINT_LINE_MAX
#define SL_LOG_PRINT_LINE_MAX 200
#endif

/**
 * @brief Map a SL_LOG_PRINT_OPT_* flag to a single-character level prefix.
 */
static inline char level_prefix_from_options(uint32_t options)
{
  switch (options & 0x3Fu) {
    case SL_LOG_PRINT_OPT_WARN:  return 'W';
    case SL_LOG_PRINT_OPT_ERROR: return 'E';
    case SL_LOG_PRINT_OPT_LOG:
    default:                     return 'I';
  }
}

void sl_log_vprint_target_ex(uint32_t options, const char *fmt, va_list ap)
{
  char line[SL_LOG_PRINT_LINE_MAX];
  int header_len;
  int body_len;
  size_t total;

  if (fmt == NULL) {
    return;
  }

  // Capture the current host-core timestamp field used by the formatted event/string log lines emitted from sl_log_backend_iostream_formatted.c.
  uint32_t timestamp = sl_log_get_timestamp_count(SL_LOG_HOST_CORE_ID);

  header_len = snprintf(line,
                        sizeof(line),
                        "[%c|F|%08lu] ",
                        level_prefix_from_options(options),
                        (unsigned long)timestamp);
  if (header_len < 0 || (size_t)header_len >= sizeof(line) - 2u) {
    return;
  }

  body_len = vsnprintf(line + header_len,
                       sizeof(line) - (size_t)header_len - 2u,
                       fmt,
                       ap);

  if (body_len < 0) {
    return;
  }

  total = (size_t)header_len + (size_t)body_len;
  if (total > sizeof(line) - 2u) {
    /* vsnprintf returns the would-be length; clamp on truncation. */
    total = sizeof(line) - 2u;
  }

  line[total++] = '\r';
  line[total++] = '\n';

  (void)sl_iostream_write(sl_iostream_recommended_console_stream, line, total);
}
