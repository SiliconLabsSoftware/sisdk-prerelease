/***************************************************************************//**
 * @file
 * @brief Portal Service Implementation
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
 * 2. Altered source versions must be plainly marked as such, and must not
 *    be misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 *
 ******************************************************************************/

#include "em_device.h"

#if defined(PORTAL_PRESENT) && defined(PORTAL)

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "sl_status.h"
#include "sl_common.h"
#include "sl_core.h"
#include "sl_clock_manager.h"
#include "sl_device_clock.h"
#include "sli_portal.h"
#include "sli_hal_portal.h"

// -----------------------------------------------------------------------------
// STATIC VARIABLES

// Initialization flag
static bool portal_initialized = false;

// Request counters for each power domain
static uint32_t domain_request_count[SLI_PORTAL_DOMAIN_MAX] = { 0 };

// Head of linked list for pending notification handles per domain
static sli_portal_request_handle_t *pending_handle_head[SLI_PORTAL_DOMAIN_MAX] = { NULL };

// Tail of linked list for pending notification handles per domain
static sli_portal_request_handle_t *pending_handle_tail[SLI_PORTAL_DOMAIN_MAX] = { NULL };

// Bit mask of domains that currently have one or more queued callback handles.
static volatile uint32_t pending_callback_mask = 0U;

// Bit mask of domains queued for software-pended callback delivery through PORTAL IRQ context.
static volatile uint32_t sw_pending_callback_mask = 0U;

// -----------------------------------------------------------------------------
// STATIC FUNCTION DEFINITIONS

/***************************************************************************//**
 * Check if a handle is already queued.
 *
 * @param[in] handle  Pointer to the request handle to search for.
 *
 * @return true if the handle is found in any pending queue,
 *         false otherwise.
 *
 * @note Caller must disable interrupts (e.g., CORE_ENTER_ATOMIC()) while calling
 * this function, since it accesses shared lists.
 ******************************************************************************/
static bool is_handle_queued(sli_portal_request_handle_t *handle)
{
  for (uint8_t scan_domain = SLI_PORTAL_DOMAIN_HOSTBASE; scan_domain < SLI_PORTAL_DOMAIN_MAX; scan_domain++) {
    if (!SLI_PORTAL_DOMAIN_SUPPORTED((sli_portal_domain_t)scan_domain)) {
      continue;
    }

    sli_portal_request_handle_t *curr = pending_handle_head[scan_domain];
    while (curr != NULL) {
      if (curr == handle) {
        return true;
      }
      curr = curr->next;
    }
  }

  return false;
}

/***************************************************************************//**
 * Deliver callbacks for each handle in a detached list.
 *
 * @param[in] list  Head of the detached callback list to process. Each handle
 *                  is unlinked before its notification function is called.
 *
 * @note Intended to run only from PORTAL IRQ context so that notification
 *       callbacks execute in a single, consistent context.
 ******************************************************************************/
static void deliver_callback_list(sli_portal_request_handle_t *list)
{
  while (list != NULL) {
    sli_portal_request_handle_t *handle = list;
    list = handle->next;
    handle->next = NULL;

    if (handle->notification_function != NULL) {
      handle->notification_function(handle);
    }
  }
}

/***************************************************************************//**
 * Detach and deliver all queued callback handles for a given domain.
 *
 * @param[in] domain  Power domain whose pending callback list shall be serviced.
 *
 * @note The list is detached from pending_handle_head[] / pending_handle_tail[]
 *       before callbacks are invoked so that callbacks may safely enqueue new
 *       requests for the same domain. Such newly queued requests remain on the
 *       global pending list and are handled by a later IRQ pass.
 ******************************************************************************/
static void deliver_domain_callbacks(sli_portal_domain_t domain)
{
  if (!SLI_PORTAL_DOMAIN_SUPPORTED(domain)) {
    return;
  }

  sli_portal_request_handle_t *list = pending_handle_head[domain];

  if (list == NULL) {
    return;
  }

  pending_handle_head[domain] = NULL;
  pending_handle_tail[domain] = NULL;
  pending_callback_mask &= ~(1UL << (uint32_t)domain);

  deliver_callback_list(list);
}

// -----------------------------------------------------------------------------
// PUBLIC FUNCTION DEFINITIONS

/***************************************************************************//**
 * Initializes portal service.
 ******************************************************************************/
sl_status_t sli_portal_init(void)
{
  CORE_DECLARE_IRQ_STATE;

  CORE_ENTER_ATOMIC();

  if (portal_initialized) {
    CORE_EXIT_ATOMIC();
    return SL_STATUS_OK;
  }

  // Enable portal bus clock.
  sl_clock_manager_enable_bus_clock(SL_BUS_CLOCK_PORTAL);

  // Clear any pending PORTAL interrupt before enabling.
  NVIC_ClearPendingIRQ(PORTAL_IRQn);
  // Enable portal interrupt.
  NVIC_EnableIRQ(PORTAL_IRQn);

  portal_initialized = true;

  CORE_EXIT_ATOMIC();

  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Request a power up of a given domain.
 ******************************************************************************/
sl_status_t sli_portal_request_domain_powerup(sli_portal_domain_t domain)
{
  CORE_DECLARE_IRQ_STATE;

  // Validate power domain
  if (!SLI_PORTAL_DOMAIN_SUPPORTED(domain)) {
    return SL_STATUS_INVALID_PARAMETER;
  }

  CORE_ENTER_ATOMIC();

  // Increment request counter
  domain_request_count[domain]++;

  // If this is the first request for this domain, set the power up request
  if (domain_request_count[domain] == 1U) {
    sli_hal_portal_request_domain_powerup(domain);
  }

  CORE_EXIT_ATOMIC();

  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Request a power up of a given domain with notification callback.
 ******************************************************************************/
sl_status_t sli_portal_request_domain_powerup_with_notif(sli_portal_domain_t domain,
                                                         sli_portal_notification_t notification_function,
                                                         sli_portal_request_handle_t *handle)
{
  CORE_DECLARE_IRQ_STATE;
  bool pend_irq = false;

  // Validate parameters
  if (handle == NULL) {
    return SL_STATUS_NULL_POINTER;
  }

  if (notification_function == NULL) {
    return SL_STATUS_NULL_POINTER;
  }

  // Validate power domain
  if (!SLI_PORTAL_DOMAIN_SUPPORTED(domain)) {
    return SL_STATUS_INVALID_PARAMETER;
  }

  CORE_ENTER_ATOMIC();

  // Reject a handle that is already queued to prevent linked list corruption.
  if (is_handle_queued(handle)) {
    CORE_EXIT_ATOMIC();
    return SL_STATUS_ALREADY_EXISTS;
  }

  // Initialize and queue handle in FIFO order.
  handle->domain = domain;
  handle->notification_function = notification_function;
  handle->next = NULL;

  if (pending_handle_tail[domain] == NULL) {
    pending_handle_head[domain] = handle;
    pending_handle_tail[domain] = handle;
  } else {
    pending_handle_tail[domain]->next = handle;
    pending_handle_tail[domain] = handle;
  }

  pending_callback_mask |= (1UL << (uint32_t)domain);

  // Increment request counter
  domain_request_count[domain]++;

  // If this is the first request for this domain, set the power up request
  if (domain_request_count[domain] == 1U) {
    sli_hal_portal_request_domain_powerup(domain);
  }

  // If already ready, service queued handles from PORTAL IRQ context by
  // pended software IRQ. The ISR will detach and drain the list.
  if (sli_hal_portal_is_domain_powered_up(domain)) {
    sli_hal_portal_disable_domain_interrupts(1UL << (uint32_t)domain);
    sli_hal_portal_clear_domain_interrupt_flags(1UL << (uint32_t)domain);
    sw_pending_callback_mask |= (1UL << (uint32_t)domain);
    pend_irq = true;
  } else {
    // The interrupt flag is set by hardware on the rising edge of the ack
    // signal regardless of the interrupt enable state. Clear any residual flag
    // so that enabling the interrupt doesn't invoke callbacks prematurely.
    sli_hal_portal_clear_domain_interrupt_flags(1UL << (uint32_t)domain);
    sli_hal_portal_enable_domain_interrupt(domain);

    // Re-check after enabling interrupt. If the domain became ready in the
    // race window, disable the hardware interrupt and pend software IRQ so the
    // ISR can detach and drain the list in consistent context.
    if (sli_hal_portal_is_domain_powered_up(domain)) {
      sli_hal_portal_disable_domain_interrupts(1UL << (uint32_t)domain);
      sw_pending_callback_mask |= (1UL << (uint32_t)domain);
      pend_irq = true;
    }
  }

  CORE_EXIT_ATOMIC();

  if (pend_irq) {
    NVIC_SetPendingIRQ(PORTAL_IRQn);
  }

  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Clears a power up request of a given domain.
 ******************************************************************************/
sl_status_t sli_portal_request_domain_powerdown(sli_portal_domain_t domain)
{
  CORE_DECLARE_IRQ_STATE;

  // Validate power domain
  if (!SLI_PORTAL_DOMAIN_SUPPORTED(domain)) {
    return SL_STATUS_INVALID_PARAMETER;
  }

  CORE_ENTER_ATOMIC();

  // Check if there are any active requests
  if (domain_request_count[domain] == 0U) {
    CORE_EXIT_ATOMIC();
    return SL_STATUS_INVALID_STATE;
  }

  // Decrement request counter
  domain_request_count[domain]--;

  // If no more requests remain, clear the hardware request and discard any
  // pending queued handles for this domain.
  if (domain_request_count[domain] == 0U) {
    sli_hal_portal_disable_domain_interrupts(1UL << (uint32_t)domain);
    sli_hal_portal_clear_domain_interrupt_flags(1UL << (uint32_t)domain);
    sw_pending_callback_mask &= ~(1UL << (uint32_t)domain);

    while (pending_handle_head[domain] != NULL) {
      sli_portal_request_handle_t *handle = pending_handle_head[domain];
      pending_handle_head[domain] = handle->next;
      handle->next = NULL;
    }

    pending_handle_tail[domain] = NULL;
    pending_callback_mask &= ~(1UL << (uint32_t)domain);

    sli_hal_portal_request_domain_powerdown(domain);
  }

  CORE_EXIT_ATOMIC();

  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Retrieves a domain status, ready or not.
 ******************************************************************************/
bool sli_portal_is_domain_ready(sli_portal_domain_t domain)
{
  // Validate power domain
  if (!SLI_PORTAL_DOMAIN_SUPPORTED(domain)) {
    return false;
  }

  return sli_hal_portal_is_domain_powered_up(domain);
}

/***************************************************************************//**
 * Portal IRQ handler implementation.
 *
 * Services only domains that are explicitly pending due to:
 * - hardware completion (interrupt flag set and enabled), or
 * - software-pended callback delivery for already-ready/race-window cases.
 *
 * Callback delivery is additionally filtered to domains that still have queued
 * handles. This prevents stale hardware/software pending state from causing
 * callback delivery after final powerdown has already discarded the queue.
 *
 * Thread-safety: On Cortex-M, normal-context list mutations are protected
 * by CORE_ENTER_ATOMIC() / CORE_EXIT_ATOMIC(), preventing PORTAL interrupt
 * preemption during those updates. PORTAL_IRQn itself is not re-entered
 * while active.
 ******************************************************************************/
static void portal_irq_handler(void)
{
  uint32_t hw_irq_mask = sli_hal_portal_get_active_domain_interrupt_mask() & SLI_PORTAL_DOMAIN_MASK;
  uint32_t sw_pending_mask = sw_pending_callback_mask & SLI_PORTAL_DOMAIN_MASK;
  uint32_t callback_service_mask = (hw_irq_mask | sw_pending_mask)
                                   & pending_callback_mask
                                   & SLI_PORTAL_DOMAIN_MASK;

  // Clear only the software-pended domains that are actually being serviced.
  // Any callback that queues new work will set a new bit and pend the IRQ again.
  sw_pending_callback_mask &= ~callback_service_mask;

  // Acknowledge hardware completion only for domains that still have pending
  // callback handles. This avoids acting on stale hardware flags for domains
  // whose callback queues were already discarded.
  uint32_t hw_ack_mask = hw_irq_mask & pending_callback_mask;
  if (hw_ack_mask != 0U) {
    sli_hal_portal_clear_domain_interrupt_flags(hw_ack_mask);
    sli_hal_portal_disable_domain_interrupts(hw_ack_mask);
  }

  while (callback_service_mask != 0U) {
    // Count trailing zeros to find the lowest set domain bit.
    uint32_t index = SL_CTZ(callback_service_mask);
    sli_portal_domain_t domain = (sli_portal_domain_t)index;

    callback_service_mask &= ~(1UL << index);
    deliver_domain_callbacks(domain);
  }
}

/***************************************************************************//**
 * Portal interrupt handler - called from interrupt context.
 ******************************************************************************/
void PORTAL_IRQHandler(void)
{
  portal_irq_handler();
}

#endif /* defined(PORTAL_PRESENT) && defined(PORTAL) */
