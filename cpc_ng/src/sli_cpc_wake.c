/***************************************************************************/ /**
 * @file sli_cpc_wake.c
 * @brief CPC Wake Up implementation.
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

#include "sl_assert.h"
#include "sl_gpio.h"

#include "sli_cpc.h"
#include "sli_cpc_assert.h"
#include "sli_cpc_atomic.h"
#include "sli_cpc_bus.h"
#include "sli_cpc_dispatcher.h"
#include "sli_cpc_wake.h"

/******************************************************************************/
/*                              Local functions                               */
/******************************************************************************/

#if defined(SL_CATALOG_CPC_NG_PM_RUNTIME_PRESENT)
// Must match sli_cpc_dispatcher_fnct_t (opaque mutable context for the callback).
static void wake_dispatcher_fnct(void *context)
{
  sli_cpc_pm_runtime_t *wake = context;
  sl_cpc_bus_t *bus = container_of(wake, sl_cpc_bus_t, pm_runtime);
  sl_status_t status;
  bool wake_val;

  status = sl_gpio_get_pin_input(&wake->pin, &wake_val);
  if (status != SL_STATUS_OK) {
    SLI_CPC_PANIC("Failed to read wake GPIO, status=0x%lx", (unsigned long)status);
  }

  // If wake pin asserted, get a PM reference only if it wasn't already taken.
  // Correspondingly, release PM reference only if wake pin deasserted and a
  // previous reference was taken
  if (wake_val && !wake->active) {
    sli_cpc_bus_pm_runtime_get(bus);
    wake->active = true;
  } else if (!wake_val && wake->active) {
    sli_cpc_bus_pm_runtime_put(bus);
    wake->active = false;
  }

  if (!wake_val) {
    return;
  }

  if (bus->ctrl.ep.bus == NULL) {
    SLI_CPC_PANIC("Failed to send wake ACK, no ep found");
  }

  sli_cpc_ep_send_ack(&bus->ctrl.ep);
}

static void wake_irq_cb(uint8_t int_no, void *context)
{
  sli_cpc_pm_runtime_t *wake = context;

  SLI_CPC_ASSERT(int_no == wake->int_no);

  sli_cpc_dispatcher_push(&wake->dispatcher_handle, wake_dispatcher_fnct, wake);
}
#endif

#if defined(SL_CATALOG_CPC_NG_WAKE_HOST_PRESENT)
/***************************************************************************/ /**
  * Tell whether a received frame is the device's wake acknowledge.
  *
  * The device acknowledges a wake-up with a standalone ACK on the control
  * endpoint, which carries no payload and requests no ACK of its own.
  ******************************************************************************/
static bool is_wake_ack_frame(const sli_cpc_hdr_t *hdr)
{
  return sli_cpc_header_get_address(hdr) == SL_CPC_EP_ID_CONTROL && sli_cpc_header_get_payload_size(hdr) == 0
         && !sli_cpc_header_is_ack_requested(hdr) && !sli_cpc_header_is_syn(hdr) && !sli_cpc_header_is_reset(hdr);
}
#endif

/******************************************************************************/
/*                             External functions                             */
/******************************************************************************/

#if defined(SL_CATALOG_CPC_NG_PM_RUNTIME_PRESENT)
sl_status_t sli_cpc_wake_from_gpio_init(sli_cpc_pm_runtime_t *wake)
{
  sl_cpc_bus_t *bus = container_of(wake, sl_cpc_bus_t, pm_runtime);
  sl_status_t status;

  sli_cpc_dispatcher_init_handle(&wake->dispatcher_handle, bus);
  sli_cpc_dispatcher_set_pre(&wake->dispatcher_handle);

  wake->int_no = SL_GPIO_INTERRUPT_UNAVAILABLE;
  wake->active = false;

  status = sl_gpio_set_pin_direction(&wake->pin, SL_GPIO_PIN_DIRECTION_IN);
  if (status != SL_STATUS_OK) {
    return status;
  }

  status = sl_gpio_set_pin_mode(&wake->pin, SL_GPIO_MODE_INPUT_PULL, 0);
  if (status != SL_STATUS_OK) {
    goto disable_wake;
  }

  status = sl_gpio_configure_external_interrupt(&wake->pin, &wake->int_no, SL_GPIO_INTERRUPT_RISING_FALLING_EDGE,
                                                wake_irq_cb, wake);
  if (status != SL_STATUS_OK) {
    goto disable_wake;
  }

  return SL_STATUS_OK;

disable_wake:
  sl_gpio_set_pin_mode(&wake->pin, SL_GPIO_MODE_DISABLED, 0);

  return status;
}

void sli_cpc_wake_from_gpio_deinit(sli_cpc_pm_runtime_t *wake)
{
  sl_cpc_bus_t *bus = container_of(wake, sl_cpc_bus_t, pm_runtime);
  MCU_DECLARE_IRQ_STATE;

  sli_cpc_dispatcher_cancel(&wake->dispatcher_handle);

  MCU_ENTER_ATOMIC();
  if (wake->int_no != SL_GPIO_INTERRUPT_UNAVAILABLE) {
    sl_gpio_deconfigure_external_interrupt(wake->int_no);
  }

  if (wake->active) {
    sli_cpc_bus_pm_runtime_put(bus);
    wake->active = false;
  }
  MCU_EXIT_ATOMIC();

  sl_gpio_set_pin_mode(&wake->pin, SL_GPIO_MODE_DISABLED, 0);
}
#endif

#if defined(SL_CATALOG_CPC_NG_WAKE_HOST_PRESENT)
sl_status_t sli_cpc_wake_host_init(sli_cpc_wake_host_t *wake)
{
  sl_status_t status;

  wake->state = SLI_CPC_WAKE_STATE_SLEEP_ALLOWED;

  // Configure de-asserted. The pin is only driven once there is something to
  // transmit. This must be done first to prevent a glitch on the line.
  status = sl_gpio_set_pin_mode(&wake->pin, SL_GPIO_MODE_PUSH_PULL, 0);
  if (status != SL_STATUS_OK) {
    sl_gpio_set_pin_mode(&wake->pin, SL_GPIO_MODE_DISABLED, 0);
    return status;
  }

  return SL_STATUS_OK;
}

void sli_cpc_wake_host_deinit(sli_cpc_wake_host_t *wake)
{
  wake->state = SLI_CPC_WAKE_STATE_SLEEP_ALLOWED;
  sl_gpio_set_pin_mode(&wake->pin, SL_GPIO_MODE_DISABLED, 0);
}
#endif

#if defined(SL_CATALOG_CPC_NG_WAKE_HOST_PRESENT)
bool sli_cpc_wake_device(sli_cpc_wake_host_t *wake)
{
  sl_status_t status;

  if (wake->state == SLI_CPC_WAKE_STATE_SLEEP_ALLOWED) {
    status = sl_gpio_set_pin(&wake->pin);
    if (status != SL_STATUS_OK) {
      // Nothing can be transmitted as long as the device is not woken up.
      SLI_CPC_PANIC("Failed to assert wake GPIO, status=0x%lx", (unsigned long)status);
    }

    wake->state = SLI_CPC_WAKE_STATE_WAITING_ACK;
  }

  return wake->state == SLI_CPC_WAKE_STATE_AWAKE;
}

void sli_cpc_wake_allow_device_sleep(sli_cpc_wake_host_t *wake)
{
  sl_status_t status;

  // Only release a wake-up that the device acknowledged. De-asserting while it
  // is still waking up would drop the request it is answering to.
  if (wake->state != SLI_CPC_WAKE_STATE_AWAKE) {
    return;
  }

  status = sl_gpio_clear_pin(&wake->pin);
  if (status != SL_STATUS_OK) {
    // The device would remain awake forever, defeating the whole mechanism.
    SLI_CPC_PANIC("Failed to de-assert wake GPIO, status=0x%lx", (unsigned long)status);
  }

  wake->state = SLI_CPC_WAKE_STATE_SLEEP_ALLOWED;
}

void sli_cpc_wake_handle_rx(sli_cpc_wake_host_t *wake, const sli_cpc_hdr_t *hdr)
{
  sl_cpc_bus_t *bus = container_of(wake, sl_cpc_bus_t, wake.host);

  if (wake->state != SLI_CPC_WAKE_STATE_WAITING_ACK) {
    // A frame received while sleep is allowed was sent by a device that woke up
    // on its own. It may go back to sleep right after, so it cannot be
    // considered awake.
    return;
  }

  // Any inbound frame proves that the device is awake.
  if (is_wake_ack_frame(hdr)) {
    wake->state = SLI_CPC_WAKE_STATE_AWAKE;
    sli_cpc_bus_signal_event(bus);
  }
}
#endif
