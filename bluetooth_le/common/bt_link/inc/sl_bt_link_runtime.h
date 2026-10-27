/***************************************************************************//**
 * @file
 * @brief Shared deferred runtime for cpc-ng message links.
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
#ifndef SL_BT_LINK_RUNTIME_H
#define SL_BT_LINK_RUNTIME_H

#include <stdint.h>
#include "sl_status.h"

/***********************************************************************************************//**
 * @addtogroup sl_bt_link
 * @{
 **************************************************************************************************/

/// Opaque runtime that serializes link events onto one deferred context.
typedef struct sli_bt_link_runtime sl_bt_link_runtime_t;

/// Location of an asynchronous message-link runtime failure.
typedef enum {
  sl_bt_link_runtime_error_init,    ///< Runtime initialization failed.
  sl_bt_link_runtime_error_acquire, ///< Runtime guard acquisition failed.
  sl_bt_link_runtime_error_release, ///< Runtime guard release failed.
  sl_bt_link_runtime_error_unknown, ///< Unrecognized runtime failure.
} sl_bt_link_runtime_error_t;

/**************************************************************************//**
 * Notification emitted when the message-link runtime fails asynchronously.
 *
 * The callback runs in the runtime error-reporting context after the failure
 * status has been stored. It must not block or perform link operations;
 * application-specific handling should be deferred when necessary. Registering
 * no callback leaves the failure available through
 * @ref sl_bt_link_runtime_get_status and subsequent runtime or link operations.
 *
 * @param[in] error Runtime operation that failed.
 * @param[in] status Failure status.
 * @param[in] user Application-provided context.
 *****************************************************************************/
typedef void (*sl_bt_link_runtime_error_callback_t)(
  sl_bt_link_runtime_error_t error,
  sl_status_t status,
  void *user);

/**************************************************************************//**
 * Return the shared message-link runtime.
 *
 * Protocol components do not create or destroy this object. The `bt_link`
 * component constructs it during system initialization.
 *
 * @return Runtime object, or NULL if the runtime is unavailable.
 *****************************************************************************/
sl_bt_link_runtime_t *sl_bt_link_runtime_get(void);

/**************************************************************************//**
 * Return the current status of the shared message-link runtime.
 *
 * @return SL_STATUS_OK when the runtime is available, otherwise the stored
 *         initialization or asynchronous failure status.
 *****************************************************************************/
sl_status_t sl_bt_link_runtime_get_status(void);

/**************************************************************************//**
 * Set the application callback for asynchronous runtime failures.
 *
 * Only one callback is associated with the shared runtime. Passing NULL
 * disables notification without changing status propagation.
 *
 * @param[in] runtime Runtime object.
 * @param[in] callback Error callback, or NULL to disable notification.
 * @param[in] user Application context passed to @p callback.
 * @return SL_STATUS_OK on success, otherwise an error code.
 *****************************************************************************/
sl_status_t sl_bt_link_runtime_set_error_callback(
  sl_bt_link_runtime_t *runtime,
  sl_bt_link_runtime_error_callback_t callback,
  void *user);

/**************************************************************************//**
 * Yield execution while waiting for link activity.
 *
 * Used by a blocking host input callback without exposing the scheduler.
 *
 * @param[in] runtime Runtime object.
 * @param[in] timeout_ticks Maximum number of scheduler ticks to wait.
 * @return SL_STATUS_OK on success, otherwise an error code.
 *****************************************************************************/
sl_status_t sl_bt_link_runtime_wait(sl_bt_link_runtime_t *runtime,
                                    uint32_t timeout_ticks);

/** @} (end addtogroup sl_bt_link) */

#endif // SL_BT_LINK_RUNTIME_H
