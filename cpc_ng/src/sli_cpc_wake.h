/***************************************************************************/ /**
 * @file
 * @brief CPC Wake Up Definitions
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

#ifndef SLI_CPC_WAKE_H
#define SLI_CPC_WAKE_H

#include "sl_gpio.h"
#include "sl_status.h"

#include "sli_cpc_types.h"

#include "sl_component_catalog.h"

#if defined(SL_CATALOG_CPC_NG_WAKE_HOST_PRESENT)
#include "sli_cpc_hdr.h"
#endif

#if defined(SL_CATALOG_CPC_NG_WAKE_DEVICE_PRESENT)
typedef struct {
  sli_cpc_dispatcher_handle_t dispatcher_handle;
  sl_gpio_t pin;
  int32_t int_no;
  bool active;
} sli_cpc_wake_device_t;
#endif

#if defined(SL_CATALOG_CPC_NG_WAKE_HOST_PRESENT)
/**
 * @brief Host-side wake handshake state.
 *
 * The host drives the wake pin to keep the device awake. It may only submit
 * frames to the driver once the device confirmed that it is awake.
 */
typedef enum sli_cpc_wake_state {
  /// Wake is de-asserted, the device is allowed to sleep.
  SLI_CPC_WAKE_STATE_SLEEP_ALLOWED,

  /// Wake is asserted, waiting for the device to signal that it is awake.
  SLI_CPC_WAKE_STATE_WAITING_ACK,

  /// Device signaled that it is awake, frames can be submitted to the driver.
  SLI_CPC_WAKE_STATE_AWAKE,
} sli_cpc_wake_state_t;

typedef struct {
  sl_gpio_t pin;
  sli_cpc_wake_state_t state;
} sli_cpc_wake_host_t;
#endif

#ifdef __cplusplus
extern "C" {
#endif

#if defined(SL_CATALOG_CPC_NG_WAKE_DEVICE_PRESENT)
/***************************************************************************/ /**
 * Initialize the wake up functionality.
 *
 * @param[in] wake  Handle to initialize.
 *
 * @return SL_STATUS_OK if successful, otherwise an error code.
 ******************************************************************************/
sl_status_t sli_cpc_wake_device_init(sli_cpc_wake_device_t *wake, sl_gpio_t pin);

/***************************************************************************/ /**
 * Deinitialize the wake up functionality.
 *
 * @param[in] Wake  Handle to deinitialize.
 *
 * @return SL_STATUS_OK if successful, otherwise an error code.
 ******************************************************************************/
void sli_cpc_wake_device_deinit(sli_cpc_wake_device_t *wake);
#endif

#if defined(SL_CATALOG_CPC_NG_WAKE_HOST_PRESENT)
/***************************************************************************/ /**
 * Initialize the wake up functionality.
 *
 * @param[in] wake  Wake up structure to initialize
 *
 * @return SL_STATUS_OK if successful, otherwise an error code.
 ******************************************************************************/
sl_status_t sli_cpc_wake_host_init(sli_cpc_wake_host_t *wake, sl_gpio_t pin);

/***************************************************************************/ /**
 * Deinitialize the wake up functionality.
 *
 * @param[in] wake  Wake up structure to deinitialize.
 *
 * @return SL_STATUS_OK if successful, otherwise an error code.
 ******************************************************************************/
void sli_cpc_wake_host_deinit(sli_cpc_wake_host_t *wake);
/***************************************************************************/ /**
 * Request that the device be awake to receive frames.
 *
 * Asserts the wake pin if the device is currently allowed to sleep. The device
 * answers with a standalone ACK on the control endpoint, which is picked up by
 * @ref sli_cpc_wake_handle_rx() and signals the bus to retry transmitting.
 *
 * Must be called before submitting frames to the driver.
 *
 * @param[in] wake  Handle to wake the device up.
 *
 * @return true if the device is awake and frames can be submitted.
 ******************************************************************************/
bool sli_cpc_wake_device(sli_cpc_wake_host_t *wake);

/***************************************************************************/ /**
 * De-assert the wake pin so the device may sleep again.
 *
 * Must only be called once everything queued has been transmitted, as the
 * device can go to sleep as soon as the pin is released.
 *
 * @param[in] wake  Handle to release the wake pin of.
 ******************************************************************************/
void sli_cpc_wake_allow_device_sleep(sli_cpc_wake_host_t *wake);

/***************************************************************************/ /**
 * Update the wake state from a received frame.
 *
 * Any frame received while waiting for the wake acknowledge marks the device as
 * awake and resumes transmission.
 *
 * @param[in] wake Handle.
 * @param[in] hdr  Frame header.
 ******************************************************************************/
void sli_cpc_wake_handle_rx(sli_cpc_wake_host_t *wake, const sli_cpc_hdr_t *hdr);
#endif

#ifdef __cplusplus
}
#endif

#endif // SLI_CPC_WAKE_H
