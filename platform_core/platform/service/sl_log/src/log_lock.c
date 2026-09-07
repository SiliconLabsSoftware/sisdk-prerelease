/***************************************************************************//**
 * @file log_lock.c
 * @brief Debug Logger data access lock API implementation.
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

#include "sl_common.h"
#include "sl_core.h"
#include "log_lock.h"

#if defined(SL_COMPONENT_CATALOG_PRESENT)
#include "sl_component_catalog.h"
#endif

#if defined(SL_CATALOG_KERNEL_PRESENT)
#include "cmsis_os2.h"
#if defined(__ARM_ARCH) || defined(__CORTEX_M)
#include "em_device.h"
#endif
#endif

//****************************************************************************

/// @cond DO_NOT_INCLUDE_WITH_DOXYGEN
#if defined(SL_CATALOG_KERNEL_PRESENT)
static osMutexId_t log_mutex;   // Debug Logger lock mutex

/***************************************************************************//**
 * Halt without going through the logger.
 *
 * SL_LOG_CRASH_ASSERT logs via sli_log_assert_implementation(), and the
 * formatted backend takes this lock again. If mutex create/acquire already
 * failed that recurses until the stack overflows. BKPT / spin instead.
 ******************************************************************************/
SL_NORETURN static void log_lock_halt(void)
{
#if defined(__ARM_ARCH) || defined(__CORTEX_M)
  if ((CoreDebug->DHCSR & CoreDebug_DHCSR_C_DEBUGEN_Msk) != 0U) {
    __BKPT(1);
  }
#endif
  while (true) {
  }
}

#define LOG_LOCK_HALT_UNLESS(cond) \
  do {                             \
    if (!(cond)) {                 \
      log_lock_halt();             \
    }                              \
  } while (0)
#else
static CORE_DECLARE_IRQ_STATE;
static uint32_t log_lock_nest;
#endif
/// @endcond

/***************************************************************************//**
 * @addtogroup sl_log
 * @{
 ******************************************************************************/
/***************************************************************************//**
 * @addtogroup log_lock
 * @{
 ******************************************************************************/

/***************************************************************************//**
 * @details
 * Create a mutex for enabling and disabling locks.
 *
 * This implementation provides options for using "mutexes" for RTOS users
 * and "core_critical" APIs for bare-metal users.
 *
 * @note RTOS users should avoid invoking the logger lock APIs from within
 *       critical sections, as this may result in unexpected behavior.
 *       Ensure that the kernel is initialized before calling this API.
 ******************************************************************************/
SL_WEAK void log_lock_create_mutex(void)
{
#if defined(SL_CATALOG_KERNEL_PRESENT)
  if (log_mutex == NULL) {
    const osMutexAttr_t mutex_attr = {
      "Debug Logger Mutex",
      osMutexRecursive | osMutexPrioInherit,
      NULL,
      0
    };
    // Create and initialize a mutex
    log_mutex = osMutexNew(&mutex_attr);
    LOG_LOCK_HALT_UNLESS(log_mutex != NULL);
  }
#endif
}

/***************************************************************************//**
 * @details
 * Report whether the mutex is already in place, so that an integration can
 * confirm its init path built the lock before any task started logging.
 ******************************************************************************/
SL_WEAK bool log_lock_is_mutex_created(void)
{
#if defined(SL_CATALOG_KERNEL_PRESENT)
  return log_mutex != NULL;
#else
  return false;
#endif
}

/***************************************************************************//**
 * @details
 * The default lock-begin implementation.
 *
 * @note RTOS users should avoid invoking the logger lock APIs from within
 *       critical sections, as this may result in unexpected behavior.
 *       Ensure that the kernel is initialized before calling this API.
 ******************************************************************************/
SL_WEAK void log_lock_begin(void)
{
#if defined(SL_CATALOG_KERNEL_PRESENT)
  osStatus_t os_status = osError;
  // Bypass the lock if the kernel is not running
  if (osKernelGetState() == osKernelRunning) {
    if (log_mutex == NULL) {
      // Fallback for integrations that do not call log_lock_create_mutex()
      // from their init path. Suspending the scheduler makes the check and
      // the creation atomic against other tasks, so two tasks logging for the
      // first time cannot each build a mutex and overwrite one another.
      // osKernelLock() reports a negative status in interrupt context, where
      // osMutexNew() would be illegal; the section then stays unprotected,
      // as it already does for any other ISR log.
      int32_t kernel_lock_state = osKernelLock();
      if (kernel_lock_state >= 0) {
        log_lock_create_mutex();
        if (kernel_lock_state == 0) {
          // Resume only when this call is the one that suspended the
          // scheduler. osKernelRestoreLock(1) suspends again instead of
          // leaving a caller's existing suspension untouched.
          (void)osKernelRestoreLock(kernel_lock_state);
        }
      }
    }
    // Acquire the mutex. Logging from an ISR is allowed: osMutexAcquire()
    // reports osErrorISR without blocking and the section stays unprotected.
    os_status = osMutexAcquire(log_mutex, osWaitForever);
    LOG_LOCK_HALT_UNLESS((os_status == osOK) || (os_status == osErrorISR));
  }
#else
  // Nested begin must not overwrite irqState. PRIMASK is saved only on the
  // outermost enter; inner enters just bump the nest count.
  //
  // The critical section covers timestamp, vsnprintf, and iostream write so
  // an ISR cannot interleave a line. That masks IRQs for a UART-length
  // window (latency / watchdog). Shortening it to the write only is a
  // follow-up; it would also allow timestamp order to diverge from wire order.
  if (log_lock_nest == 0U) {
    CORE_ENTER_CRITICAL();
  }
  log_lock_nest++;
#endif
}

/***************************************************************************//**
 * @details
 * The default lock-end implementation.
 *
 * @note RTOS users should avoid invoking the logger lock APIs from within
 *       critical sections, as this may result in unexpected behavior.
 *       Ensure that the kernel is initialized before calling this API.
 ******************************************************************************/
SL_WEAK void log_lock_end(void)
{
#if defined(SL_CATALOG_KERNEL_PRESENT)
  osStatus_t os_status = osError;
  // Bypass the lock if the kernel is not running
  if (osKernelGetState() == osKernelRunning) {
    // Release the mutex
    os_status = osMutexRelease(log_mutex);
    LOG_LOCK_HALT_UNLESS((os_status == osOK) || (os_status == osErrorISR));
  }
#else
  if (log_lock_nest == 0U) {
    return;
  }
  log_lock_nest--;
  if (log_lock_nest == 0U) {
    CORE_EXIT_CRITICAL();
  }
#endif
}

/** @} (end addtogroup log_lock) */
/** @} (end addtogroup sl_log) */
