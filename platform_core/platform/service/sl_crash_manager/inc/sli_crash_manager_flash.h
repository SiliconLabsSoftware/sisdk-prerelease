/***************************************************************************/ /**
* @file sli_crash_manager_flash.h
* @brief Internal Silicon Labs Crash Manager persistence API
* @version 1.0.0
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

#ifndef SLI_CRASH_MANAGER_FLASH_H
#define SLI_CRASH_MANAGER_FLASH_H

#ifdef __cplusplus
extern "C" {
#endif

/*******************************************************************************
 ********************************   INCLUDES   ********************************
 ******************************************************************************/

#include "sl_crash_manager.h"
#include "sl_crash_manager_config.h"

#include <stdbool.h>

/**
 * @defgroup sli_crash_manager_flash Crash Manager flash persistence (internal)
 * @brief Internal API for crash log read/write/delete on flash
 *
 * @{
 */

/**
 * @brief Initialize flash-backed crash storage.
 *
 * @return sl_status_t SL_STATUS_OK on success; error code on failure.
 */
sl_status_t sli_crash_manager_flash_init(void);

/**
 * @brief Persist crash log to flash.
 *
 * Intended from @ref sl_crash_manager_persist_pending_record (task context), not fault/ISR context.
 *
 * @param[in] data  Crash log to persist. Must not be NULL.
 * @return sl_status_t SL_STATUS_OK on success; error code on failure.
 */
sl_status_t sli_crash_manager_flash_write(const volatile sl_crash_manager_data_t *data);

/**
 * @brief Remove stored crash log from flash.
 *
 * @return sl_status_t SL_STATUS_OK on success; error code on failure.
 */
sl_status_t sli_crash_manager_flash_delete(void);

/**
 * @brief Retrieve crash log with status.
 *
 * Reads the stored crash log into data and returns a status code. Validates
 * object existence, type, size, and crash type before returning.
 *
 * @param[out] data  Buffer to receive the crash log. Must not be NULL.
 * @return sl_status_t SL_STATUS_OK if valid crash; SL_STATUS_EMPTY if type is
 *         NONE; SL_STATUS_NOT_FOUND/SL_STATUS_INVALID_STATE/SL_STATUS_FAIL on error.
 */
sl_status_t sli_crash_manager_flash_get_log(sl_crash_manager_data_t *data);

/** @} (end addtogroup sli_crash_manager_flash) */

#ifdef __cplusplus
}
#endif

#endif /* SLI_CRASH_MANAGER_FLASH_H */
