/***************************************************************************//**
 * @file
 * @brief Wake and sleep functionality
 *******************************************************************************
 * # License
 * <b>Copyright 2024 Silicon Laboratories Inc. www.silabs.com</b>
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
// Define module name for Power Manager debugging feature.
#define CURRENT_MODULE_NAME    "APP_BLUETOOTH_EXAMPLE_WAKE_LOCK"

#include "sl_device_gpio.h"
#include "sl_common.h"
#include "sl_gpio.h"
#include "sl_wake_lock.h"
#include "sl_power_manager.h"
#include "app_assert.h"

#if SL_WAKE_LOCK_ENABLE_LOCAL == 1
// Global mutexes for wake-up handling
static bool wl_em_req_added = false;

// Interrupt number assigned to the input pin
static int32_t wl_input_intno = SL_GPIO_INTERRUPT_UNAVAILABLE;

// Wake Lock GPIO in
static const sl_gpio_t wl_gpio_in = {
  .port = SL_WAKE_LOCK_INPUT_PORT,
  .pin  = SL_WAKE_LOCK_INPUT_PIN
};

// Callback function for wake-up functionality
static void wake_lock_cb(uint8_t int_no, void *context);
#endif //SL_WAKE_LOCK_ENABLE_LOCAL

#if SL_WAKE_LOCK_ENABLE_REMOTE == 1
// Wake Lock GPIO out
static const sl_gpio_t wl_gpio_out = {
  .port = SL_WAKE_LOCK_OUTPUT_PORT,
  .pin  = SL_WAKE_LOCK_OUTPUT_PIN
};
#endif // SL_WAKE_LOCK_ENABLE_REMOTE

/**************************************************************************//**
 * Wake and sleep (lock) initialization function
 *****************************************************************************/
void sl_wake_lock_init(void)
{
#if SL_WAKE_LOCK_ENABLE_LOCAL == 1
  sl_status_t ret = SL_STATUS_FAIL;
#endif //SL_WAKE_LOCK_ENABLE_LOCAL

#if SL_WAKE_LOCK_ENABLE_REMOTE == 1
  // Set output pin to wake-up remote controller
  (void)sl_gpio_set_pin_mode(&wl_gpio_out,
                             SL_GPIO_MODE_PUSH_PULL,
                             SL_WAKE_LOCK_OUTPUT_POLARITY_ACTIVE_HIGH);

  // Set pin to disable wake-up
  if (SL_WAKE_LOCK_OUTPUT_POLARITY_ACTIVE_HIGH) {
    (void)sl_gpio_set_pin(&wl_gpio_out);
  } else {
    (void)sl_gpio_clear_pin(&wl_gpio_out);
  }
#endif // SL_WAKE_LOCK_ENABLE_REMOTE

#if SL_WAKE_LOCK_ENABLE_LOCAL == 1
  wl_em_req_added = false;

  // Set input pin to wake up from sleep
  (void)sl_gpio_init();
  (void)sl_gpio_set_pin_mode(&wl_gpio_in,
                             SL_GPIO_MODE_INPUT,
                             SL_WAKE_LOCK_INPUT_POLARITY_ACTIVE_HIGH);

  // Set callback, interrupt number and interrupt trigger direction
  ret = sl_gpio_configure_external_interrupt(&wl_gpio_in,
                                             &wl_input_intno,
                                             SL_GPIO_INTERRUPT_RISING_FALLING_EDGE,
                                             wake_lock_cb,
                                             NULL);

  if (ret != SL_STATUS_OK) {
    app_assert(false, "Failed to register callback function for wake-lock.\n");
  }
#endif //SL_WAKE_LOCK_ENABLE_LOCAL
}

#if SL_WAKE_LOCK_ENABLE_REMOTE == 1
/**************************************************************************//**
 * Signal wake-up to remote controller.
 *
 * Set output pin to signal remote controller to wake-up.
 *
 * @note Wait 1ms before sending out command to ensure that remote end has fully
 * woken up.
 *****************************************************************************/
void sl_wake_lock_set_remote_req(void)
{
  // set pin to wake up remote controller
  if (SL_WAKE_LOCK_OUTPUT_POLARITY_ACTIVE_HIGH) {
    (void)sl_gpio_set_pin(&wl_gpio_out);
  } else {
    (void)sl_gpio_clear_pin(&wl_gpio_out);
  }
  // wait until remote side wakes up (typically it takes 600us)
  sl_sleeptimer_delay_millisecond(1);
}

/**************************************************************************//**
 * Signal go to sleep (lock) to remote controller.
 *
 * Signal to remote controller that the transmission ended (or close to end)
 * and it can go to sleep after.
 *****************************************************************************/
void sl_wake_lock_clear_remote_req(void)
{
  // clear pin to sleep remote controller
  if (SL_WAKE_LOCK_OUTPUT_POLARITY_ACTIVE_HIGH) {
    (void)sl_gpio_clear_pin(&wl_gpio_out);
  } else {
    (void)sl_gpio_set_pin(&wl_gpio_out);
  }
}

#else // SL_WAKE_LOCK_ENABLE_REMOTE
// Unused placeholder functions without wake-up functionality enabled
void sl_wake_lock_set_remote_req(void)
{
}

void sl_wake_lock_clear_remote_req(void)
{
}
#endif // SL_WAKE_LOCK_ENABLE_REMOTE

/**************************************************************************//**
 * Wake-up signal arrived from remote controller.
 *
 * @note Placeholder function.
 *****************************************************************************/
SL_WEAK void sl_wake_lock_set_req_rx_cb(void)
{
}

/**************************************************************************//**
 * Go to sleep (lock) signal arrived from remote controller.
 *
 * @note Placeholder function.
 *****************************************************************************/
SL_WEAK void sl_wake_lock_clear_req_rx_cb(void)
{
}

#if SL_WAKE_LOCK_ENABLE_LOCAL == 1
/**************************************************************************//**
 * Enable sleep mode locally.
 *
 * Enable sleep mode for this controller. Only enables it if not yet enabled.
 *
 * @note It is called automatically by wake-up event handling function.
 *****************************************************************************/
void sl_wake_lock_set_local(void)
{
  // Only add requirement if not added before.
  if (wl_em_req_added) {
    sl_power_manager_remove_em_requirement(SL_POWER_MANAGER_EM1);
    wl_em_req_added = false;
  }
}

/**************************************************************************//**
 * Disable sleep mode locally.
 *
 * Disable sleep mode for this controller. Only disables it if not yet disabled.
 *
 * @note Have to be called from either wake-up or go sleep callback functions
 * before more time consuming functions.
 *****************************************************************************/
void sl_wake_lock_clear_local(void)
{
  // Only remove requirement if not removed before
  if (!wl_em_req_added) {
    sl_power_manager_add_em_requirement(SL_POWER_MANAGER_EM1);
    wl_em_req_added = true;
  }
}

/**************************************************************************//**
 * Wake-up event handling function.
 *
 * Checks the arrived event and calls the appropriate wake-up or go to sleep
 * function callbacks. Wakes up controller if wake-up signal arrived.
 *
 * @note: It is the responsibility of the application to call
 * sl_wake_lock_set_local() function.
 *****************************************************************************/
static void wake_lock_cb(uint8_t int_no, void *context)
{
  (void)context; // Context is not used here
  bool pin_value = false;

  if (int_no == wl_input_intno) {
    (void)sl_gpio_get_pin_input(&wl_gpio_in, &pin_value);

    if ( pin_value != SL_WAKE_LOCK_INPUT_POLARITY_ACTIVE_HIGH ) {
      // Go to sleep signal arrived from remote controller.
      sl_wake_lock_clear_req_rx_cb();
    } else {
      // Wake-up signal arrived from remote controller.
      sl_wake_lock_clear_local();
      sl_wake_lock_set_req_rx_cb();
    }
  }
}

#else //SL_WAKE_LOCK_ENABLE_LOCAL
// Unused placeholder without deep sleep function enabled
void sl_wake_lock_set_local(void)
{
}

void sl_wake_lock_clear_local(void)
{
}
#endif //SL_WAKE_LOCK_ENABLE_LOCAL
