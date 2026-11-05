/***************************************************************************//**
 * @file
 * @brief RAIL utility to drain HWSCHED deferred work from process_action / RTOS.
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

#include "sl_rail_util_scheduler.h"
#include "sl_rail_util_scheduler_config.h"

#if defined(SL_CATALOG_KERNEL_PRESENT)
#include "cmsis_os2.h"

static osSemaphoreId_t s_event_signal;
static osThreadId_t s_thread_id;
static uint32_t s_semaphore_cb[osSemaphoreCbSize / sizeof(uint32_t)];
static uint32_t s_thread_cb[osThreadCbSize / sizeof(uint32_t)];
static uint8_t s_thread_stack[SL_RAIL_UTIL_SCHEDULER_TASK_STACK_SIZE
                              * sizeof(void *)];

static void pending_work_notifier(void)
{
  if (s_event_signal != NULL) {
    (void)osSemaphoreRelease(s_event_signal);
  }
}

static void scheduler_task(void *arg)
{
  (void)arg;
  while (true) {
    (void)osSemaphoreAcquire(s_event_signal, osWaitForever);
    sl_rail_util_scheduler_process_action();
  }
}
#endif // SL_CATALOG_KERNEL_PRESENT

void sl_rail_util_scheduler_init(void)
{
#if defined(SL_CATALOG_KERNEL_PRESENT)
  osSemaphoreAttr_t sem_attr = {
    .name = "rail_sched",
    .cb_mem = s_semaphore_cb,
    .cb_size = sizeof(s_semaphore_cb),
  };
  osThreadAttr_t task_attr = {
    .name = "rail_sched",
    .attr_bits = 0U,
    .cb_mem = s_thread_cb,
    .cb_size = sizeof(s_thread_cb),
    .stack_mem = s_thread_stack,
    .stack_size = sizeof(s_thread_stack),
    .priority = (osPriority_t)SL_RAIL_UTIL_SCHEDULER_TASK_PRIORITY,
  };

  s_event_signal = osSemaphoreNew(1U, 0U, &sem_attr);
  if (s_event_signal == NULL) {
    return;
  }

  sl_rail_util_scheduler_set_pending_work_notifier(pending_work_notifier);

  s_thread_id = osThreadNew(scheduler_task, NULL, &task_attr);
  if (s_thread_id == NULL) {
    sl_rail_util_scheduler_set_pending_work_notifier(NULL);
    (void)osSemaphoreDelete(s_event_signal);
    s_event_signal = NULL;
  }
#endif // SL_CATALOG_KERNEL_PRESENT
}

#if defined(SL_CATALOG_POWER_MANAGER_PRESENT) \
  && !defined(SL_CATALOG_KERNEL_PRESENT)
bool sl_rail_util_scheduler_is_ok_to_sleep(void)
{
  return !sl_rail_util_scheduler_has_pending_work();
}

sl_power_manager_on_isr_exit_t sl_rail_util_scheduler_sleep_on_isr_exit(void)
{
  return (sl_rail_util_scheduler_has_pending_work()
          ? SL_POWER_MANAGER_WAKEUP
          : SL_POWER_MANAGER_IGNORE);
}
#endif
