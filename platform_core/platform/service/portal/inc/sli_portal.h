/***************************************************************************//**
 * @file
 * @brief Portal API definition.
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

#ifndef SLI_PORTAL_H
#define SLI_PORTAL_H

#include "em_device.h"

#if defined(PORTAL_PRESENT) && defined(PORTAL)

#include <stdbool.h>
#include "sl_status.h"
#include "sli_device_portal.h"

#ifdef __cplusplus
extern "C" {
#endif

/***************************************************************************//**
 * @addtogroup portal PORTAL
 *
 * @details
 * ## Overview
 *
 *  Portal is a platform-level software module that provides power domain
 *  management via the Host APB interface. It allows software to request and
 *  release power domain power-up, with optional notification callbacks when
 *  domains are ready.
 *
 *  The service maintains per-domain request counters. The power-up request is
 *  asserted when the first request is made and cleared only when all requests
 *  for that domain have been released.
 *
 *  Supported power domains:
 *  - PD1HOSTBASE (\a SLI_PORTAL_DOMAIN_HOSTBASE)
 *  - PD1HOSTNPU (\a SLI_PORTAL_DOMAIN_HOSTNPU)
 *  - PD1LPW0 (\a SLI_PORTAL_DOMAIN_LPW0)
 *  - PD1WIFI0BASE (\a SLI_PORTAL_DOMAIN_WIFI0BASE)
 *  - PD1WIFI0MODEM11B (\a SLI_PORTAL_DOMAIN_WIFI0MODEM11B)
 *  - PD1WIFI0OFDM (\a SLI_PORTAL_DOMAIN_WIFI0OFDM)
 *
 *  ## Initialization
 *
 *  API function sli_portal_init() initializes the Portal service.
 *  This function must be called before using any other Portal API. It enables
 *  the PORTAL bus clock and the PORTAL interrupt in the NVIC.
 *
 *  ## Functionalities
 *
 *  The Portal service includes functionalities for requesting and releasing
 *  power domain power-up, and for determining when a domain is ready.
 *
 *  ### Request and release power domain power-up
 *
 *  API functions sli_portal_request_domain_powerup() and
 *  sli_portal_request_domain_powerdown() allow requesting and
 *  releasing power-up for a given domain. sli_portal_is_domain_ready()
 *  allows checking if a domain is powered up.
 *
 *  @note An internal request counter is maintained per domain. The power-up
 *        request in the PORTAL peripheral is asserted when the first request
 *        is made and cleared only when all requests for that domain have been
 *        released.
 *
 *  ### Polling for domain ready
 *
 *  The below code example shows how to request power-up, poll until the domain
 *  is ready, access peripherals, and then clear the request.
 *
 *  @code{.c}
 *  #include "sli_portal.h"
 *
 *  sl_status_t status;
 *
 *  status = sli_portal_init();
 *  if (status != SL_STATUS_OK) {
 *    // Handle error
 *  }
 *
 *  status = sli_portal_request_domain_powerup(
 *    SLI_PORTAL_DOMAIN_LPW0);
 *  if (status != SL_STATUS_OK) {
 *    // Handle error
 *  }
 *
 *  while (!sli_portal_is_domain_ready(
 *    SLI_PORTAL_DOMAIN_LPW0)) {
 *  }
 *
 *  // Domain is now powered up, safe to access peripherals (e.g. SEQRAM)
 *  // ... access peripherals ...
 *
 *  status = sli_portal_request_domain_powerdown(
 *    SLI_PORTAL_DOMAIN_LPW0);
 *  @endcode
 *
 *  ### Callback when domain ready
 *
 *  API function sli_portal_request_domain_powerup_with_notif()
 *  allows requesting power-up with a notification callback. The callback is
 *  invoked when the domain is ready via the PORTAL IRQ handler. If the domain
 *  is already powered, the implementation pends the PORTAL interrupt so that
 *  notifications still execute in the same interrupt context.
 *
 *  @code{.c}
 *  #include "sli_portal.h"
 *
 *  static void on_domain_ready(sli_portal_request_handle_t *handle)
 *  {
 *    // Domain is powered up, safe to access peripherals
 *    // ... access peripherals ...
 *
 *    sli_portal_request_domain_powerdown(handle->domain);
 *  }
 *
 *  void example_request_with_callback(void)
 *  {
 *    sl_status_t status;
 *    sli_portal_request_handle_t handle;
 *
 *    status = sli_portal_request_domain_powerup_with_notif(
 *      SLI_PORTAL_DOMAIN_LPW0,
 *      on_domain_ready,
 *      &handle);
 *    if (status != SL_STATUS_OK) {
 *      // Handle error
 *    }
 *  }
 *  @endcode
 *
 * @{
 ******************************************************************************/

// -----------------------------------------------------------------------------
// TYPE DEFINITIONS

/// Declaration of request handle structure
typedef struct sli_portal_request_handle sli_portal_request_handle_t;

/// Notification callback function type
typedef void (*sli_portal_notification_t) (sli_portal_request_handle_t *handle);

/// Portal request handle structure
struct sli_portal_request_handle {
  sli_portal_domain_t domain;                        ///< Power domain associated with this request
  sli_portal_notification_t notification_function;   ///< Notification function to be called when domain is ready
  sli_portal_request_handle_t *next;                 ///< Next handle in linked list
};

// -----------------------------------------------------------------------------
// PROTOTYPES

/***************************************************************************//**
 * Initializes portal service.
 *
 * This function must be called before using any other Portal API. It enables
 * the PORTAL bus clock and the PORTAL interrupt in the NVIC.
 *
 * @return SL_STATUS_OK if successful.
 ******************************************************************************/
sl_status_t sli_portal_init(void);

/***************************************************************************//**
 * Request a power-up of a given domain.
 *
 * @param[in] domain Domain targeted for the powerup request. Possible values are:
 *                    - SLI_PORTAL_DOMAIN_HOSTBASE
 *                    - SLI_PORTAL_DOMAIN_HOSTNPU
 *                    - SLI_PORTAL_DOMAIN_LPW0
 *                    - SLI_PORTAL_DOMAIN_WIFI0BASE
 *                    - SLI_PORTAL_DOMAIN_WIFI0MODEM11B
 *                    - SLI_PORTAL_DOMAIN_WIFI0OFDM
 *
 * @return SL_STATUS_OK if successful.
 *         SL_STATUS_INVALID_PARAMETER if domain is out of range.
 *
 * @note
 *  - Powers up the requested domain.
 *  - The caller must use sli_portal_is_domain_ready() to
 *    determine when the requested domain is powered up.
 *  - An internal request counter is maintained per domain. The power up
 *    request is cleared only when there are no active requests.
 *  - Each call must be balanced by a corresponding call to
 *    sli_portal_request_domain_powerdown().
 ******************************************************************************/
sl_status_t sli_portal_request_domain_powerup(sli_portal_domain_t domain);

/***************************************************************************//**
 * Request a power up of a given domain. Get a callback when requested domain is
 * powered up.
 *
 * @param[in] domain Domain targeted for the powerup request. Possible values are:
 *                    - SLI_PORTAL_DOMAIN_HOSTBASE
 *                    - SLI_PORTAL_DOMAIN_HOSTNPU
 *                    - SLI_PORTAL_DOMAIN_LPW0
 *                    - SLI_PORTAL_DOMAIN_WIFI0BASE
 *                    - SLI_PORTAL_DOMAIN_WIFI0MODEM11B
 *                    - SLI_PORTAL_DOMAIN_WIFI0OFDM
 *
 * @param[in] notification_function Function to be called when given
 *                                  domain/subsystem is ready.
 *
 * @param[in] handle Pointer to request handle. Must be allocated by caller.
 *
 * @return SL_STATUS_OK if successful.
 *         SL_STATUS_NULL_POINTER if handle or notification_function is NULL.
 *         SL_STATUS_INVALID_PARAMETER if domain is out of range.
 *         SL_STATUS_ALREADY_EXISTS if handle is already queued.
 *
 * @note
 *  - Powers up the requested domain. If the domain is not yet ready, the
 *    notification callback is invoked once the domain becomes ready.
 *  - The notification function receives the request handle as an argument. The
 *    handle can be freed or re-used for another request after the call.
 *  - An internal request counter is maintained per domain. The power up request
 *    is cleared only when there are no active requests.
 *  - Each call must be balanced by a corresponding call to
 *    sli_portal_request_domain_powerdown(). The notification
 *    callback does not release the request; the caller is still responsible
 *    for clearing it.
 *  - If all requests for the domain are released before the notification is
 *    delivered, any queued handle for that domain is discarded and the callback
 *    will not be invoked.
 *  - The handle must not be reused or passed to this function again while it
 *    is still pending (i.e. before the callback fires or the handle is
 *    discarded by sli_portal_request_domain_powerdown()). Doing so corrupts
 *    the internal linked list.
 *
 * @warning The notification function is always called from PORTAL interrupt
 *          context. Implementations must be ISR-safe and must not perform
 *          blocking operations or acquire locks that are not ISR-safe.
 ******************************************************************************/
sl_status_t sli_portal_request_domain_powerup_with_notif(sli_portal_domain_t domain,
                                                         sli_portal_notification_t notification_function,
                                                         sli_portal_request_handle_t *handle);

/***************************************************************************//**
 * Clears a power up request of a given domain.
 *
 * @param[in] domain Domain to clear the powerup request. Possible values are:
 *                    - SLI_PORTAL_DOMAIN_HOSTBASE
 *                    - SLI_PORTAL_DOMAIN_HOSTNPU
 *                    - SLI_PORTAL_DOMAIN_LPW0
 *                    - SLI_PORTAL_DOMAIN_WIFI0BASE
 *                    - SLI_PORTAL_DOMAIN_WIFI0MODEM11B
 *                    - SLI_PORTAL_DOMAIN_WIFI0OFDM
 *
 * @return SL_STATUS_OK if successful.
 *         SL_STATUS_INVALID_PARAMETER if domain is out of range.
 *         SL_STATUS_INVALID_STATE if there are no active requests for the domain.
 *
 * @note
 *  - Call this function when a subsystem is done with a domain so the domain
 *    can be powered down.
 *  - An internal request counter is maintained per domain. The power up request
 *    is cleared only when there are no active requests.
 *  - When the request count reaches zero, all pending handles and callback-queued
 *    handles for the domain are discarded and the domain interrupt is disabled.
 *    Callers must not expect the notification callback to fire after this function
 *    returns with no remaining active requests.
 *  - This function is ISR-safe and may be called from notification callbacks
 *    or other interrupt context.
 ******************************************************************************/
sl_status_t sli_portal_request_domain_powerdown(sli_portal_domain_t domain);

/***************************************************************************//**
 * Retrieves a domain status, ready or not.
 *
 * @param[in] domain Domain. Options are:
 *                    - SLI_PORTAL_DOMAIN_HOSTBASE
 *                    - SLI_PORTAL_DOMAIN_HOSTNPU
 *                    - SLI_PORTAL_DOMAIN_LPW0
 *                    - SLI_PORTAL_DOMAIN_WIFI0BASE
 *                    - SLI_PORTAL_DOMAIN_WIFI0MODEM11B
 *                    - SLI_PORTAL_DOMAIN_WIFI0OFDM
 *
 * @return True if domain is ready, false otherwise.
 *
 * @note
 *  - Returns the current power status of the domain.
 ******************************************************************************/
bool sli_portal_is_domain_ready(sli_portal_domain_t domain);

/** @} (end addtogroup portal) */

#ifdef __cplusplus
}
#endif

#endif /* defined(PORTAL_PRESENT) && defined(PORTAL) */
#endif /* SLI_PORTAL_H */
