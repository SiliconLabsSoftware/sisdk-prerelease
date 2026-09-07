/***************************************************************************//**
 * @file log_lock.h
 * @brief Debug Logger data access lock API definition.
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

#ifndef LOG_LOCK_H
#define LOG_LOCK_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/***************************************************************************//**
 * @addtogroup sl_log
 * @{
 ******************************************************************************/

/***************************************************************************//**
 * @addtogroup log_lock Debug Logger lock functions
 * @brief This module provides data protection tools for the Debug Logger.
 * @{
 * @details
 * The functions in this module are the default protection tools for the
 * logger's target-side formatting and output path. The application can
 * substitute @ref log_lock_begin and @ref log_lock_end, provided that the
 * protected logger functions remain safe against re-entrant calls.
 *
 * Under an RTOS the lock is a recursive CMSIS-RTOS2 mutex; on bare-metal (or
 * before the kernel is started) it degrades to a nested core critical
 * section (PRIMASK is saved and restored only at the outermost enter/exit).
 *
 * @note These functions are intended for use by the logger and should not
 *       be used directly by any applications.
 ******************************************************************************/

/***************************************************************************//**
 * @brief
 *  Create a mutex for locking and unlocking a section.
 *
 * @details
 *  Called by @ref sl_log_init_stage2 while the start task is still the only
 *  running task. An application that replaces the logger init sequence must
 *  call this itself before any task logs; @ref log_lock_begin only creates
 *  the mutex on demand as a fallback. Repeated calls are ignored, and the
 *  function is a no-op on bare-metal builds.
 ******************************************************************************/
void log_lock_create_mutex(void);

/***************************************************************************//**
 * @brief
 *  Check whether the lock mutex has been created.
 *
 * @return
 *  true when the mutex exists. false before @ref log_lock_create_mutex has
 *  run, and on bare-metal builds, which lock with a critical section and
 *  never create a mutex.
 ******************************************************************************/
bool log_lock_is_mutex_created(void);

/***************************************************************************//**
 * @brief
 *  Begin a lock section.
 ******************************************************************************/
void log_lock_begin(void);

/***************************************************************************//**
 * @brief
 *  End a lock section.
 ******************************************************************************/
void log_lock_end(void);

/** @} (end addtogroup log_lock) */
/** @} (end addtogroup sl_log) */

#ifdef __cplusplus
}
#endif

#endif // LOG_LOCK_H
