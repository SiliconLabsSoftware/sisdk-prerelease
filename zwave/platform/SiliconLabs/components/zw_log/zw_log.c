/**
 * @file
 * @brief Silicon Labs implementation of the Z-Wave PAL logging module.
 *
 * @copyright 2025 Silicon Laboratories Inc.
 */

#include "zpal_log.h"
#include <stdarg.h>
#include <stdbool.h>

#if !defined(SL_CATALOG_ZW_LOG_PRESENT)

/*
 * No sl_log backend is required in the project when zw_log is not selected, so this
 * translation unit must not reference sl_log, FreeRTOS or iostream symbols.
 * The ZPAL log API symbols are kept as no-ops for link compatibility.
 */
void zpal_log_init(void)
{
}

void zpal_log(
  zpal_log_component_t component,
  zpal_log_level_t level,
  const char * pFormat,
  ...)
{
  (void)component;
  (void)level;
  (void)pFormat;
}

#else // defined(SL_CATALOG_ZW_LOG_PRESENT)

#include <stdio.h>
#include <FreeRTOS.h>
#include <assert.h>
#include <task.h>
#include <queue.h>
#include "sl_log.h"

// ZPAL_LOG_BACKEND_COMPACT is defined by zpal_log.h and selected by the
// sl_log backend component installed in the application project.
#if !ZPAL_LOG_BACKEND_COMPACT

#define ZW_LOG_QUEUE_LENGTH 8
#define ZW_LOG_TASK_STACK_SIZE (configMINIMAL_STACK_SIZE + 128)
#define ZW_LOG_QUEUE_WAIT_TICKS pdMS_TO_TICKS(100)

struct zpal_log_event {
  zpal_log_component_t component;
  zpal_log_level_t level;
  uint8_t message[ZW_LOG_BUFFER_SIZE];
};

/*
 * log_buffer is private to zpal_log_task(). Log producers copy their message
 * into a queue event. Keep this ownership model intact.
 */
static uint8_t log_buffer[ZW_LOG_BUFFER_SIZE];
static QueueHandle_t log_queue;
static StaticQueue_t log_queue_buffer;
static uint8_t log_queue_storage[ZW_LOG_QUEUE_LENGTH * sizeof(struct zpal_log_event)];
static StaticTask_t log_task_buffer;
static StackType_t log_task_stack[ZW_LOG_TASK_STACK_SIZE];
static volatile uint32_t log_dropped_count;

static const bool log_channels[ZPAL_LOG_LEVEL_END] = {
  [ZPAL_LOG_LEVEL_DEBUG] =   ZW_LOG_ENABLE_DEBUG,
  [ZPAL_LOG_LEVEL_INFO] =    ZW_LOG_ENABLE_INFO,
  [ZPAL_LOG_LEVEL_WARNING] = ZW_LOG_ENABLE_WARNING,
  [ZPAL_LOG_LEVEL_ERROR] =   ZW_LOG_ENABLE_ERROR,
};

static const char level_chars[] = {
  [ZPAL_LOG_LEVEL_DEBUG] =   'D',
  [ZPAL_LOG_LEVEL_INFO] =    'I',
  [ZPAL_LOG_LEVEL_WARNING] = 'W',
  [ZPAL_LOG_LEVEL_ERROR] =   'E'
};

static void zpal_log_emit_via_sl_log(const char *fmt, ...)
{
  va_list ap;

  va_start(ap, fmt);
  sl_log_vprint_target_ex(0, fmt, ap);
  va_end(ap);
}

static void zpal_log_record_drop(void)
{
  // Compiler builtins provide target-appropriate atomic operations without
  // hand-written assembly; relaxed ordering is sufficient for this counter.
  // Atomically increments the count and discards its previous value.
  (void)__atomic_fetch_add(&log_dropped_count, 1u, __ATOMIC_RELAXED);
}

static void zpal_log_report_drops(void)
{
  // Atomically reads the count and resets it; concurrent later drops remain
  // available for the next warning.
  const uint32_t dropped_count =
    __atomic_exchange_n(&log_dropped_count, 0u, __ATOMIC_RELAXED);

  if (dropped_count > 0u) {
    zpal_log_emit_via_sl_log("[W] dropped %u Z-Wave log message(s)\n",
                             (unsigned int)dropped_count);
  }
}

static void zpal_log_append(
  char * buffer,
  size_t buffer_size,
  size_t * used,
  const char * format,
  ...)
{
  if (buffer_size == 0u) {
    return;
  }
  if (*used >= (buffer_size - 1u)) {
    *used = buffer_size - 1u;
    buffer[*used] = '\0';
    return;
  }

  va_list args;
  va_start(args, format);
  const int32_t written = vsnprintf(buffer + *used,
                                    buffer_size - *used,
                                    format,
                                    args);
  va_end(args);

  if (written < 0) {
    buffer[*used] = '\0';
  } else if ((size_t)written >= (buffer_size - *used)) {
    *used = buffer_size - 1u;
  } else {
    *used += (size_t)written;
  }
}

static void zpal_log_task(void * p_parameters)
{
  (void)p_parameters; // unused parameter, everything is passed via message queue
  struct zpal_log_event event;

  for (;;) {
    if (xQueueReceive(log_queue, &event, ZW_LOG_QUEUE_WAIT_TICKS) != pdTRUE) {
      zpal_log_report_drops();
      continue;
    }

    size_t msg_len = 0;
    if (ZW_LOG_DISPLAY_TIMESTAMP) {
      zpal_log_append((char *)log_buffer,
                      ZW_LOG_BUFFER_SIZE,
                      &msg_len,
                      "%09u ",
                      (unsigned int)xTaskGetTickCount());
    }
    if (ZW_LOG_DISPLAY_LEVEL) {
      zpal_log_append((char *)log_buffer,
                      ZW_LOG_BUFFER_SIZE,
                      &msg_len,
                      "[%c] ",
                      level_chars[event.level]);
    }
    if (ZW_LOG_DISPLAY_COMPONENT) {
      zpal_log_append((char *)log_buffer,
                      ZW_LOG_BUFFER_SIZE,
                      &msg_len,
                      "(%d) ",
                      event.component);
    }
    zpal_log_append((char *)log_buffer,
                    ZW_LOG_BUFFER_SIZE,
                    &msg_len,
                    "%s",
                    (char *)event.message);

    if ((msg_len > 0) && log_channels[event.level]) {
      zpal_log_emit_via_sl_log("%s", (char *)log_buffer);
    }
    zpal_log_report_drops();
  }
}

void zpal_log_init(void)
{
  if (xPortIsInsideInterrupt()) {
    return;
  }

  /*
   * Initialization is normally performed once during system startup, but
   * zpal_log() also supports lazy initialization for callers outside the
   * normal startup path. The static queue/task handles cannot be initialized
   * safely by two tasks concurrently, and a mutex cannot protect resources
   * that do not exist yet. This short critical section serializes creation.
   *
   * The trade-off is a short interrupt-latency increase while FreeRTOS
   * creates the static objects. Initialization must happen from task context.
   */
  taskENTER_CRITICAL();
  if (log_queue == NULL) {
    log_queue = xQueueCreateStatic(ZW_LOG_QUEUE_LENGTH,
                                   sizeof(struct zpal_log_event),
                                   log_queue_storage,
                                   &log_queue_buffer);
    if (log_queue != NULL) {
      TaskHandle_t task_handle = xTaskCreateStatic(zpal_log_task,
                                                   "zw_log",
                                                   ZW_LOG_TASK_STACK_SIZE,
                                                   NULL,
                                                   tskIDLE_PRIORITY + 1, // log has lowest prio of the system (+1 of IDLE task)
                                                   log_task_stack,
                                                   &log_task_buffer);
      if (task_handle == NULL) {
        log_queue = NULL;
      }
    }
  }
  taskEXIT_CRITICAL();
}

void zpal_log(
  zpal_log_component_t component,
  zpal_log_level_t level,
  const char * pFormat,
  ...)
{
  if (xPortIsInsideInterrupt()) {
    return;
  }

  // Parse variable argument list
  va_list args;
  va_start(args, pFormat);

  // Validate log level and determine the character prefix to display
  if (level >= ZPAL_LOG_LEVEL_END) {
    assert(false); // Invalid log level
    va_end(args);
    return;
  }

  /*
   * The queue and logging task are initialized explicitly from
   * ZW_system_startup(), before ApplicationInit(). The lazy initialization
   * below remains as a fallback for callers that use this implementation
   * outside the normal startup path.
   */
  zpal_log_init();
  if (log_queue == NULL) {
    va_end(args);
    return;
  }

  struct zpal_log_event event = {
    .component = component,
    .level = level,
  };
  (void)vsnprintf((char *)event.message, sizeof(event.message), pFormat, args);
  va_end(args);

  if (xQueueSend(log_queue, &event, 0) != pdTRUE) {
    zpal_log_record_drop();
  }
}

#else // ZPAL_LOG_BACKEND_COMPACT

#define ZW_LOG_COMPACT_TASK_STACK_SIZE (configMINIMAL_STACK_SIZE + 64)

static StaticTask_t compact_log_task_buffer;
static StackType_t compact_log_task_stack[ZW_LOG_COMPACT_TASK_STACK_SIZE];

static void zpal_log_compact_task(void * p_parameters)
{
  (void)p_parameters;

  for (;;) {
    (void)sl_log_flush();
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

void zpal_log_init(void)
{
  static bool is_initialized;

  if (xPortIsInsideInterrupt() || is_initialized) {
    return;
  }

  /*
   * Protect the one-time task creation against concurrent task-context
   * callers. A mutex cannot be used before the logging task exists. The
   * section is short and only runs in task context.
   */
  taskENTER_CRITICAL();
  if (!is_initialized) {
    TaskHandle_t task_handle = xTaskCreateStatic(zpal_log_compact_task,
                                                 "zw_log",
                                                 ZW_LOG_COMPACT_TASK_STACK_SIZE,
                                                 NULL,
                                                 tskIDLE_PRIORITY + 1,
                                                 compact_log_task_stack,
                                                 &compact_log_task_buffer);
    is_initialized = (task_handle != NULL);
  }
  taskEXIT_CRITICAL();
}

void zpal_log(
  zpal_log_component_t component,
  zpal_log_level_t level,
  const char * pFormat,
  ...)
{
  (void)component;
  (void)level;
  (void)pFormat;
  /*
   * Compact producers bypass this variadic API at the call site. Keep the
   * symbol for binary compatibility with code that references zpal_log
   * directly, but do not format or enqueue a message here.
   */
}

#endif // !ZPAL_LOG_BACKEND_COMPACT

#endif // defined(SL_CATALOG_ZW_LOG_PRESENT)
