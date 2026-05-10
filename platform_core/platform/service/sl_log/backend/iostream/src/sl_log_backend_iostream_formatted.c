/***************************************************************************//**
 * @file
 * @brief Platform backend glue for the logging subsystem
 *
 * ## Output format
 *
 * Each log line is sent to iostream backend and ends with CRLF (\\r\\n).
 * Log level is not printed.
 *
 * **Payload** (always):
 *
 * **String log**: the format string with specifiers expanded: %d = signed decimal (32-bit),
 * %x = 8-digit hex (32-bit), %p = pointer (0x + 8-digit hex), %s = string.
 * %% produces a literal '%'. Other characters after % are emitted as-is.
 * Example: "count=%d addr=%p" with args -1, 0x1000 gives
 * count=-1 addr=0x00001000
 *
 * **Event**: event_id (8 hex), then for each argument in arg_count: '|' and 8 hex digits.
 * Example: 00000001|AABBCCDD
 *
 * **Optional leading prefix** (see @ref sl_log_formatted_iostream_config.h):
 * - Both @c SL_LOG_FORMATTED_IOSTREAM_PREFIX_TIMESTAMP and
 *   @c SL_LOG_FORMATTED_IOSTREAM_PREFIX_LOG_TYPE: [TIMESTAMP|TYPE] and a space
 *   (TYPE is S or E; TIMESTAMP is 8 hex digits).
 * - Timestamp only: [TIMESTAMP] and a space.
 * - Log type only: [TYPE] and a space.
 *
 * **Optional trailing core ID** (when @c SL_LOG_FORMATTED_IOSTREAM_APPEND_CORE_ID is set):
 * a space and [CC] (2 hex digits) is appended after the payload. Applies to both
 * string and event lines.
 *
 * Example (string, all options): [00005678|S] count=-1 addr=0x00001000 [00]
 * Example (event, all options):  [00001234|E] 00000001|AABBCCDD [00]
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

#include "sl_log_platform_specific.h"
#include "sl_log_helper.h"
#include "sl_log_formatted_iostream_config.h"
#include "sl_log.h"
#include "sl_iostream.h"
#include "sl_iostream_handles.h"
#include "em_device.h"
#include <stdint.h>

#ifndef SL_LOG_FORMATTED_IOSTREAM_PREFIX_TIMESTAMP
#define SL_LOG_FORMATTED_IOSTREAM_PREFIX_TIMESTAMP  0
#endif
#ifndef SL_LOG_FORMATTED_IOSTREAM_PREFIX_LOG_TYPE
#define SL_LOG_FORMATTED_IOSTREAM_PREFIX_LOG_TYPE  0
#endif
#ifndef SL_LOG_FORMATTED_IOSTREAM_APPEND_CORE_ID
#define SL_LOG_FORMATTED_IOSTREAM_APPEND_CORE_ID  0
#endif

/*******************************************************************************
 ***************************  DEFINE MACROS ********************************
 ******************************************************************************/
/** Line buffer size for output. String logs are truncated so an optional
 * leading prefix, payload, optional " [CC]" core-id suffix, and CRLF fit in LINE_MAX. */
#define LINE_MAX  200
/** Reserve trailing bytes during string expansion for the optional " [CC]" core-id
 * suffix (5 chars) plus CRLF. */
#if SL_LOG_FORMATTED_IOSTREAM_APPEND_CORE_ID
#define SL_LOG_FORMATTED_IOSTREAM_RESERVED_TAIL  (2U + 5U)
#else
#define SL_LOG_FORMATTED_IOSTREAM_RESERVED_TAIL  2U
#endif

/*******************************************************************************
 ***************************  LOCAL VARIABLES   ********************************
 ******************************************************************************/
static const char hex_chars[] = "0123456789ABCDEF";

/*******************************************************************************
**************************   LOCAL FUNCTIONS   ********************************
*******************************************************************************/

/**
 * @brief Write a 32-bit value as 8 hexadecimal digits into a buffer.
 *
 * @param[in,out] p  Pointer to the next character position in the buffer
 * @param[in]     v  Value to format (32-bit unsigned)
 *
 * @return Pointer to the character past the written digits (p + 8)
 */
static inline char* u32_to_hex8(char *p, uint32_t v)
{
  for (int i = 7; i >= 0; i--) {
    *p++ = hex_chars[(v >> (i * 4)) & 0xF];
  }
  return p;
}

#if SL_LOG_FORMATTED_IOSTREAM_APPEND_CORE_ID
/**
 * @brief Write an 8-bit value as 2 hexadecimal digits into a buffer.
 *
 * @param[in,out] p  Pointer to the next character position in the buffer
 * @param[in]     v  Value to format (8-bit unsigned)
 *
 * @return Pointer to the character past the written digits (p + 2)
 */
static inline char* u8_to_hex2(char *p, uint8_t v)
{
  *p++ = hex_chars[(v >> 4) & 0xF];
  *p++ = hex_chars[v & 0xF];
  return p;
}
#endif

/**
 * @brief Write a 32-bit value as decimal digits into a buffer.
 *
 * @param[in,out] p  Pointer to the next character position in the buffer
 * @param[in]     v  Value to format (32-bit unsigned)
 *
 * @return Pointer to the character past the written digits
 */
static inline char* u32_to_dec(char *p, uint32_t v)
{
  char tmp[10];
  int i = 0;

  if (v == 0) {
    *p++ = '0';
    return p;
  }

  while (v > 0) {
    tmp[i++] = '0' + (v % 10);
    v /= 10;
  }

  while (i--) {
    *p++ = tmp[i];
  }

  return p;
}

/*******************************************************************************
**************************   GLOBAL FUNCTIONS   ********************************
*******************************************************************************/

/**
 * @brief Initialize the logging backend.
 *
 * This prototype is exported to the generic logging code via the
 * sl_log_api_backend structure below. The implementation brings up the
 * platform transport (for example, UART) and prepares it for log output.
 *
 * Synchronization: called from the logger initialization path (single-threaded)
 *
 * @return SL_STATUS_OK on success, an sl_status_t error code otherwise.
 */
sl_status_t sl_log_hal_backend_init(void)
{
  return sl_iostream_set_default(sl_iostream_recommended_console_stream);
}

/**
 * @brief Write a formatted log event to the backend transport.
 *
 * Formatted iostream output is a direct-write path: caller provides a pointer
 * to one event and the backend formats that event into a text line and pushes
 * it immediately to the configured iostream.
 *
 * @param[in] buffer      Pointer to the event to format and send
 * @param[in] read_index  Unused for formatted direct-write path
 * @param[in] event_count Unused for formatted direct-write path (expected 1)
 *
 * @return SL_STATUS_OK on success or an sl_status_t error code from
 *         sl_iostream_write().
 */
sl_status_t sl_log_hal_backend_write(sl_log_event_t *buffer, uint32_t read_index, uint32_t event_count)
{
  /* iostream formatted output path is direct-write only: caller passes a single event (&event). */
  (void)read_index;
  sl_status_t status = SL_STATUS_OK;

  /* event_count is always expected to be 1 for formatted output. */
  if (event_count != 1) {
    return SL_STATUS_INVALID_PARAMETER;
  }

  char line[LINE_MAX];
  char *p = line;

  /* Type: bit 0 — 0 = string log, 1 = event (level is not emitted). */
  uint8_t type = buffer->flags & 0x01;

#if (SL_LOG_FORMATTED_IOSTREAM_PREFIX_TIMESTAMP || SL_LOG_FORMATTED_IOSTREAM_PREFIX_LOG_TYPE)
  *p++ = '[';
#if SL_LOG_FORMATTED_IOSTREAM_PREFIX_TIMESTAMP
  p = u32_to_hex8(p, buffer->timestamp);
#endif
#if (SL_LOG_FORMATTED_IOSTREAM_PREFIX_TIMESTAMP && SL_LOG_FORMATTED_IOSTREAM_PREFIX_LOG_TYPE)
  *p++ = '|';
#endif
#if SL_LOG_FORMATTED_IOSTREAM_PREFIX_LOG_TYPE
  *p++ = (type ? 'E' : 'S');
#endif
  *p++ = ']';
  *p++ = ' ';
#endif

  if (type)  /* Event type: event_id and args as 8-hex digits joined by '|'. */
  {
    uint32_t i;

    p = u32_to_hex8(p, buffer->event_id);

    for (i = 0; i < buffer->arg_count; i++) {
      *p++ = '|';
      p = u32_to_hex8(p, buffer->args[i]);
    }
  } else { /* String log: event_id is format string, expand specifiers with args. */
    const char *fmt = (const char *)buffer->event_id;
    uint32_t arg_index = 0;

    while (*fmt && (p < (line + sizeof(line) - SL_LOG_FORMATTED_IOSTREAM_RESERVED_TAIL))) {

      if (*fmt == '%') {
        fmt++;

        if (*fmt == '%') {
          *p++ = '%';
          fmt++;
        }
        else if (*fmt == 'd' && arg_index < buffer->arg_count) {
          int32_t sval = (int32_t)buffer->args[arg_index++];
          if (sval < 0) {
            *p++ = '-';
            p = u32_to_dec(p, 0u - (uint32_t)sval);
          } else {
            p = u32_to_dec(p, (uint32_t)sval);
          }
          fmt++;
        }
        else if (*fmt == 'x' && arg_index < buffer->arg_count) {
          p = u32_to_hex8(p, buffer->args[arg_index++]);
          fmt++;
        }
        else if (*fmt == 'p' && arg_index < buffer->arg_count) {
          *p++ = '0';
          *p++ = 'x';
          p = u32_to_hex8(p, buffer->args[arg_index++]);
          fmt++;
        }
        else if (*fmt == 's' && arg_index < buffer->arg_count) {
          const char *s = (const char *)(uintptr_t)buffer->args[arg_index++];
          if (s != NULL) {
            while (*s && (p < (line + sizeof(line) - SL_LOG_FORMATTED_IOSTREAM_RESERVED_TAIL))) {
              *p++ = *s++;
            }
          }
          fmt++;
        }
        else {
          *p++ = '%';
          if (*fmt != '\0') {
            *p++ = *fmt++;
          }
        }
      }
      else {
        *p++ = *fmt++;
      }
    }
  }

#if SL_LOG_FORMATTED_IOSTREAM_APPEND_CORE_ID
  *p++ = ' ';
  *p++ = '[';
  p = u8_to_hex2(p, buffer->core_id);
  *p++ = ']';
#endif

  /* Terminate line with CRLF. */
  *p++ = '\r';
  *p++ = '\n';

  status = sl_iostream_write(sl_iostream_recommended_console_stream, line, p - line);
  return status;
}

/**
 * @brief Deinitialize the logging backend.
 *
 * Tear down any resources allocated by sl_log_hal_backend_init(). After this
 * call the backend is considered inactive until reinitialized.
 *
 * @return SL_STATUS_OK on success or an sl_status_t error code.
 */
sl_status_t sl_log_hal_backend_deinit(void)
{
  return SL_STATUS_OK;
}

/**
 * @brief   Logging backend API structure.
 */
sl_log_api_backend_t sl_log_api_backend = { .backend_init   = sl_log_hal_backend_init,
                                            .backend_write  = sl_log_hal_backend_write,
                                            .backend_deinit = sl_log_hal_backend_deinit };

/**
 * @brief Return pointer to the backend API structure.
 *
 * Provides the generic logging core with the platform-specific backend
 * implementation (init/write/deinit). This function returns a pointer to the
 * statically allocated `sl_log_api_backend` structure.
 *
 * @return Pointer to the populated sl_log_api_backend_t structure.
 */
sl_log_api_backend_t *sl_log_get_api_backend(void)
{
  return &sl_log_api_backend;
}
