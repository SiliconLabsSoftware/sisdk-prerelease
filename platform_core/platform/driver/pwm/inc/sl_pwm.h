/***************************************************************************//**
 * @file
 * @brief PWM Driver
 *******************************************************************************
 * # License
 * <b>Copyright 2020 Silicon Laboratories Inc. www.silabs.com</b>
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
#ifndef SL_PWM_H
#define SL_PWM_H

#include "em_device.h"

#if defined(SL_COMPONENT_CATALOG_PRESENT)
#include "sl_component_catalog.h"
#endif

#if defined(SL_CATALOG_GPIO_PERIPHERAL_PIN_LATCHING_PRESENT)
#define PWM_USES_GPIO_LATCHING
#endif

#include "sl_status.h"
#include "sl_enum.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/***************************************************************************//**
 * @addtogroup pwm PWM Driver
 * @brief PWM Driver
 * @{
 * The PWM driver uses one or more TIMER peripherals to generate one or more PWM
 * waveform, with configurable frequency, duty cycle, and polarity. Multiple
 * instances of the driver can be created and allocated to their own TIMER
 * channel.
 *
 * The duty cycle of the PWM waveform can be updated, while the PWM driver is
 * running by calling @ref sl_pwm_set_duty_cycle(), without causing glitches
 * in the output waveform.
 *
 * @note If several PWM driver instances are set up to use the same TIMER
 * instance, the PWM frequency of these instances must be the same.
 * @note On devices with GPIO pin-latching support, @ref sl_pwm_timer_stop()
 * latches the GPIO pins for enabled TIMER CC routes before stopping the TIMER.
 * @ref sl_pwm_timer_start() resumes the TIMER and releases those pins.
 * Devices without GPIO pin-latching support retain their existing behavior.
 * The per-channel @ref sl_pwm_start() and @ref sl_pwm_stop() APIs enable and
 * disable their GPIO routes; stopping a channel also releases its pin if it
 * was latched by @ref sl_pwm_timer_stop().
 * ### PWM Example Code
 *
 * Basic example for generating PWM waveform:
 *
 * ```c
 *
 * #include "sl_pwm.h"
 * #include "sl_gpio.h"
 *
 * int main( void )
 * {
 *
 *   ...
 *
 *   sl_pwm_instance_t sl_pwm_led0 = {
 *     .timer    = TIMER0,
 *     .channel  = 0,
 *     .port     = SL_GPIO_PORT_A,
 *     .pin      = 0,
 *     .location = 0,
 *   };
 *
 *   sl_pwm_config_t pwm_led0_config = {
 *     .frequency = 10000,
 *     .polarity  = PWM_ACTIVE_HIGH,
 *   };
 *
 *   // Initialize PWM
 *   sl_pwm_init(&sl_pwm_led0, &pwm_led0_config);
 *
 *   // Set duty cycle to 40%
 *   sl_pwm_set_duty_cycle(&sl_pwm_led0, 40);
 *
 *   // Enable PWM output
 *   sl_pwm_start(&sl_pwm_led0);
 *
 *   ...
 *
 * }
 * ```
 *
 ******************************************************************************/

/** PWM polarity selection */
SL_ENUM(sl_pwm_polarity_t) {
  /** PWM polarity active high */
  PWM_ACTIVE_HIGH = 0,
  /** PWM polarity active low */
  PWM_ACTIVE_LOW = 1,
};

/**
 * PWM driver instance.
 * Allocated by the application. The application must
 * not modify the contents of this handle and should not depend on its values.
 */
typedef struct sl_pwm_instance {
  /** @cond DO_NOT_INCLUDE_WITH_DOXYGEN */
  TIMER_TypeDef *timer; /**< TIMER instance */
  uint8_t channel;      /**< TIMER channel */
  uint8_t port;         /**< GPIO port */
  uint8_t pin;          /**< GPIO pin */
  uint8_t location;     /**< GPIO location */
  /** @endcond */
} sl_pwm_instance_t;

/**
 * PWM driver configuration
 */
typedef struct sl_pwm_config {
  int frequency;              /**< PWM frequency */
  sl_pwm_polarity_t polarity; /**< PWM polarity */
} sl_pwm_config_t;

/**************************************************************************//**
 * @brief
 *   Initialize PWM driver.
 *
 * @details
 *   On devices with GPIO output latching, adding a channel to a running TIMER
 *   that already has an active PWM channel returns @ref SL_STATUS_INVALID_STATE.
 *   Stop the TIMER with @ref sl_pwm_timer_stop() before adding the channel;
 *   enabled TIMER CC output pins are latched during the stop and released when
 *   the TIMER is resumed with @ref sl_pwm_timer_start().
 *
 * @param[in] pwm
 *   PWM driver instance
 *
 * @param[in] config
 *   Driver configuration
 *
 * @return
 *   SL_STATUS_OK on success, or an error for an invalid configuration, a
 *   channel that is already initialized, or an active TIMER that was not
 *   stopped before adding a channel on devices with GPIO output latching.
 *****************************************************************************/
sl_status_t sl_pwm_init(sl_pwm_instance_t *pwm, sl_pwm_config_t *config);

/**************************************************************************//**
 * @brief
 *   Deinitialize PWM driver.
 *
 * @details
 *   Disables the PWM route, returns the pin to disabled GPIO mode, and releases
 *   the GPIO latch if it was latched by @ref sl_pwm_timer_stop().
 *
 * @param[in] pwm
 *   PWM driver instance
 *
 * @return
 *   SL_STATUS_OK if there are no errors.
 *****************************************************************************/
sl_status_t sl_pwm_deinit(sl_pwm_instance_t *pwm);

/**************************************************************************//**
 * @brief
 *   Start generating PWM waveform
 *
 * @details
 *   Enables the PWM output route. The TIMER and GPIO latch state are not
 *   changed. Use @ref sl_pwm_timer_start() to resume a TIMER stopped with
 *   @ref sl_pwm_timer_stop().
 *
 * @param[in] pwm
 *   PWM driver instance
 *****************************************************************************/
void sl_pwm_start(sl_pwm_instance_t *pwm);

/**************************************************************************//**
 * @brief
 *   Stop generating PWM waveform
 *
 * @details
 *   Disables the PWM output route. The TIMER remains running. If the pin was
 *   latched by @ref sl_pwm_timer_stop(), this releases that pin after
 *   disabling its route.
 *
 * @param[in] pwm
 *   PWM driver instance
 *****************************************************************************/
void sl_pwm_stop(sl_pwm_instance_t *pwm);

/**************************************************************************//**
 * @brief
 *   Stop a TIMER used by PWM without changing its output routes.
 *
 * @details
 *   On Series 3, waits for the TIMER to stop and retains PWM channel settings
 *   so they can be restored by @ref sl_pwm_timer_start(). On devices with GPIO
 *   pin-latching support, GPIO pins for enabled TIMER CC routes are latched
 *   before the TIMER stops. Devices without GPIO pin-latching support use the
 *   native TIMER stop behavior without changing GPIO latch state.
 *
 * @param[in] timer
 *   TIMER instance initialized for PWM
 *
 * @return
 *   SL_STATUS_OK on success, or an error for an invalid TIMER.
 *****************************************************************************/
sl_status_t sl_pwm_timer_stop(TIMER_TypeDef *timer);

/**************************************************************************//**
 * @brief
 *   Resume a TIMER stopped with @ref sl_pwm_timer_stop().
 *
 * @details
 *   Restores the PWM channel configuration and starts the TIMER at a cycle
 *   boundary on Series 3. On Series 2, this enables the TIMER using the native
 *   TIMER control. After the TIMER resumes, GPIO pins for enabled TIMER CC
 *   pins latched by @ref sl_pwm_timer_stop() are released on devices with GPIO
 *   pin-latching support. PWM output routes are not changed.
 *
 * @param[in] timer
 *   TIMER instance initialized for PWM
 *
 * @return
 *   SL_STATUS_OK on success, SL_STATUS_INVALID_PARAMETER for an invalid TIMER,
 *   or SL_STATUS_INVALID_STATE if no PWM channel is configured on devices that
 *   retain PWM TIMER state.
 *****************************************************************************/
sl_status_t sl_pwm_timer_start(TIMER_TypeDef *timer);

/**************************************************************************//**
 * @brief
 *   Set duty cycle for PWM waveform.
 *
 * @param[in] pwm
 *   PWM driver instance
 *
 * @param[in] percent
 *   Percent of the PWM period waveform is in the state defined
 *   as the active polarity in the driver configuration
 *****************************************************************************/
void sl_pwm_set_duty_cycle(sl_pwm_instance_t *pwm, uint8_t percent);

/**************************************************************************//**
 * @brief
 *   Set duty cycle for PWM waveform.
 *
 * @param[in] pwm
 *   PWM driver instance
 *
 * @return
 *   Percent of the PWM period waveform is in the state defined
 *   as the active polarity in the driver configuration
 *****************************************************************************/
uint8_t sl_pwm_get_duty_cycle(sl_pwm_instance_t *pwm);

/** @} (end addtogroup pwm) */

#ifdef __cplusplus
}
#endif

#endif // SL_PWM_H
