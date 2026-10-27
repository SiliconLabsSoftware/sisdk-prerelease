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
#include <stddef.h>
#include <string.h>
#include "app_rta.h"
#include "sli_bt_link.h"
#include "sl_bt_link_config.h"
#include "sl_bt_link_runtime.h"
#include "sli_bt_link_runtime.h"
#include "sl_component_catalog.h"
#if defined(SL_CATALOG_KERNEL_PRESENT)
#include "cmsis_os2.h"
#else
#include "sl_main_process_action.h"
#endif // SL_CATALOG_KERNEL_PRESENT

typedef enum {
  link_runtime_state_uninitialized,
  link_runtime_state_constructed,
  link_runtime_state_available,
  link_runtime_state_failed,
} link_runtime_state_t;

struct sli_bt_link_runtime {
  app_rta_context_t                    context;
  sl_bt_link_runtime_error_callback_t  on_error;
  void                                 *error_user;
  size_t                               reserved_events;
  sl_status_t                          status;
  link_runtime_state_t                 state;
};

static struct sli_bt_link_runtime runtime_instance = {
  .status = SL_STATUS_NOT_INITIALIZED,
  .state = link_runtime_state_uninitialized,
};

static sl_bt_link_runtime_error_t sl_bt_link_runtime_map_error(
  app_rta_error_t error)
{
  switch (error) {
    case APP_RTA_ERROR_RUNTIME_INIT_FAILED:
      return sl_bt_link_runtime_error_init;

    case APP_RTA_ERROR_ACQUIRE_FAILED:
      return sl_bt_link_runtime_error_acquire;

    case APP_RTA_ERROR_RELEASE_FAILED:
      return sl_bt_link_runtime_error_release;

    default:
      return sl_bt_link_runtime_error_unknown;
  }
}

static void sl_bt_link_runtime_step(void)
{
  sli_bt_link_event_t event;
  size_t event_size = sizeof(event);
  sl_status_t status;

  do {
    event_size = sizeof(event);
    status = app_rta_queue_read(runtime_instance.context,
                                (uint8_t *)&event,
                                &event_size);
    if ((status == SL_STATUS_OK) && (event_size == sizeof(event))) {
      sli_bt_link_process_event(&event);
    }
  } while (status == SL_STATUS_OK);
}

static void sl_bt_link_runtime_on_error(app_rta_error_t error,
                                        sl_status_t status)
{
  runtime_instance.status = status;
  runtime_instance.state = link_runtime_state_failed;

  if (runtime_instance.on_error != NULL) {
    runtime_instance.on_error(sl_bt_link_runtime_map_error(error),
                              status,
                              runtime_instance.error_user);
  }
}

static sl_status_t sl_bt_link_runtime_construct(void)
{
  sl_status_t status;

  if (runtime_instance.state == link_runtime_state_available) {
    return SL_STATUS_OK;
  }

  if (runtime_instance.state == link_runtime_state_failed) {
    return runtime_instance.status;
  }

  if (runtime_instance.state == link_runtime_state_constructed) {
    status = app_rta_enable_context(runtime_instance.context, true);
    if (status == SL_STATUS_OK) {
      // Enabling a guarded context acquires its guard.
      status = app_rta_release(runtime_instance.context);
    }
  } else {
    app_rta_config_t config = {
      .requirement.runtime          = true,
      .requirement.runtime_separate = true,
      .requirement.guard            = true,
      .requirement.signal           = false,
      .requirement.queue            = true,
      .step                         = sl_bt_link_runtime_step,
      .priority                     = SL_BT_LINK_RUNTIME_TASK_PRIO,
      .stack_size                   = SL_BT_LINK_RUNTIME_TASK_STACK,
      .error                        = sl_bt_link_runtime_on_error,
      .wait_for_guard               = SL_BT_LINK_RUNTIME_GUARD_TIMEOUT,
      .queue_size                   = SL_BT_LINK_RUNTIME_QUEUE_SIZE,
      .queue_element_size           = sizeof(sli_bt_link_event_t),
    };

    status = app_rta_create_context(&config, &runtime_instance.context);
    if (status == SL_STATUS_OK) {
      runtime_instance.state = link_runtime_state_constructed;
      runtime_instance.reserved_events = SL_BT_LINK_RUNTIME_CONTROL_EVENTS;
    }
  }

  if (status == SL_STATUS_OK) {
    runtime_instance.state = link_runtime_state_available;
  } else {
    runtime_instance.status = status;
    runtime_instance.state = link_runtime_state_failed;
  }

  return status;
}

void sli_bt_link_runtime_init(void)
{
  (void)sl_bt_link_runtime_construct();
}

sl_bt_link_runtime_t *sl_bt_link_runtime_get(void)
{
  if ((runtime_instance.state == link_runtime_state_uninitialized)
      || (runtime_instance.state == link_runtime_state_constructed)) {
    (void)sl_bt_link_runtime_construct();
  }

  return ((runtime_instance.state == link_runtime_state_available)
          ? &runtime_instance
          : NULL);
}

sl_status_t sl_bt_link_runtime_get_status(void)
{
  if ((runtime_instance.state == link_runtime_state_uninitialized)
      || (runtime_instance.state == link_runtime_state_constructed)) {
    return sl_bt_link_runtime_construct();
  }

  return ((runtime_instance.state == link_runtime_state_available)
          ? SL_STATUS_OK
          : runtime_instance.status);
}

sl_status_t sl_bt_link_runtime_set_error_callback(
  sl_bt_link_runtime_t *runtime,
  sl_bt_link_runtime_error_callback_t callback,
  void *user)
{
  sl_status_t status;
  sl_status_t release_status;

  status = sli_bt_link_runtime_acquire(runtime);
  if (status != SL_STATUS_OK) {
    return status;
  }

  runtime_instance.on_error = callback;
  runtime_instance.error_user = (callback != NULL) ? user : NULL;

  release_status = sli_bt_link_runtime_release(runtime);
  return release_status;
}

sl_status_t sl_bt_link_runtime_wait(sl_bt_link_runtime_t *runtime,
                                    uint32_t timeout_ticks)
{
  if (runtime != &runtime_instance) {
    return SL_STATUS_INVALID_HANDLE;
  }

  if (runtime_instance.state != link_runtime_state_available) {
    return runtime_instance.status;
  }

#if defined(SL_CATALOG_KERNEL_PRESENT)
  if (osDelay((timeout_ticks == 0u) ? 1u : timeout_ticks) != osOK) {
    return SL_STATUS_FAIL;
  }
#else
  (void)timeout_ticks;
  sl_main_process_action();
#endif // SL_CATALOG_KERNEL_PRESENT

  return SL_STATUS_OK;
}

sl_status_t sli_bt_link_runtime_post(sl_bt_link_runtime_t *runtime,
                                     const sli_bt_link_event_t *event)
{
  if (runtime != &runtime_instance) {
    return SL_STATUS_INVALID_HANDLE;
  }

  if (runtime_instance.state != link_runtime_state_available) {
    return runtime_instance.status;
  }

  if (event == NULL) {
    return SL_STATUS_NULL_POINTER;
  }

  return app_rta_queue_push(runtime_instance.context,
                            (uint8_t *)event,
                            sizeof(*event));
}

sl_status_t sli_bt_link_runtime_acquire(sl_bt_link_runtime_t *runtime)
{
  if (runtime != &runtime_instance) {
    return SL_STATUS_INVALID_HANDLE;
  }

  if (runtime_instance.state != link_runtime_state_available) {
    return runtime_instance.status;
  }

  return app_rta_acquire(runtime_instance.context);
}

sl_status_t sli_bt_link_runtime_release(sl_bt_link_runtime_t *runtime)
{
  if (runtime != &runtime_instance) {
    return SL_STATUS_INVALID_HANDLE;
  }

  if (runtime_instance.state != link_runtime_state_available) {
    return runtime_instance.status;
  }

  return app_rta_release(runtime_instance.context);
}

sl_status_t sli_bt_link_runtime_reserve(sl_bt_link_runtime_t *runtime,
                                        size_t event_count)
{
  sl_status_t status;
  sl_status_t release_status;

  status = sli_bt_link_runtime_acquire(runtime);
  if (status != SL_STATUS_OK) {
    return status;
  }

  if ((runtime_instance.reserved_events > SL_BT_LINK_RUNTIME_QUEUE_SIZE)
      || (event_count > (SL_BT_LINK_RUNTIME_QUEUE_SIZE
                         - runtime_instance.reserved_events))) {
    status = SL_STATUS_NO_MORE_RESOURCE;
  } else {
    runtime_instance.reserved_events += event_count;
  }

  release_status = sli_bt_link_runtime_release(runtime);
  return (status == SL_STATUS_OK) ? release_status : status;
}

sl_status_t sli_bt_link_runtime_unreserve(sl_bt_link_runtime_t *runtime,
                                          size_t event_count)
{
  sl_status_t status;
  sl_status_t release_status;

  status = sli_bt_link_runtime_acquire(runtime);
  if (status != SL_STATUS_OK) {
    return status;
  }

  if ((runtime_instance.reserved_events < SL_BT_LINK_RUNTIME_CONTROL_EVENTS)
      || (event_count > (runtime_instance.reserved_events
                         - SL_BT_LINK_RUNTIME_CONTROL_EVENTS))) {
    status = SL_STATUS_INVALID_PARAMETER;
  } else {
    runtime_instance.reserved_events -= event_count;
  }

  release_status = sli_bt_link_runtime_release(runtime);
  return (status == SL_STATUS_OK) ? release_status : status;
}
