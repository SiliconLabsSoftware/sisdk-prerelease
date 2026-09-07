/***************************************************************************//**
 * @file
 * @brief GPIO Peripheral Pin Latching API
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

#ifndef SL_GPIO_PERIPHERAL_PIN_LATCHING_H
#define SL_GPIO_PERIPHERAL_PIN_LATCHING_H

#ifdef __cplusplus
extern "C" {
#endif

#include "sl_status.h"
#include "sl_device_peripheral.h"

// *****************************************************************************
/// @addtogroup gpio_peripheral_pin_latching GPIO Peripheral Pin Latching
/// @brief Latch and unlatch peripheral GPIO pins for low-power mode transitions.
///
/// Before entering a sleep mode where a peripheral loses retention, all pins
/// driven by that peripheral via DBUS should be latched so they maintain their
/// current output state. After the peripheral is restored and re-enabled,
/// the pins should be unlatched to return pad control to the peripheral.
///
/// @{
// *****************************************************************************

/***************************************************************************//**
 * Latch all GPIO pins routed to a peripheral.
 *
 * Freezes the output state of every pin currently routed to the given
 * peripheral through the GPIO DBUS. This prevents pin glitches when the
 * peripheral is disabled or loses retention during sleep.
 *
 * @param[in] peripheral  The peripheral whose routed pins should be latched.
 *
 * @return SL_STATUS_OK on success.
 *         SL_STATUS_INVALID_PARAMETER if the peripheral has no ROUTEEN
 *         information (gpio_routeen_offset == 0xFFFF).
 ******************************************************************************/
sl_status_t sl_gpio_latch_peripheral_pins(sl_peripheral_t peripheral);

/***************************************************************************//**
 * Unlatch all GPIO pins routed to a peripheral.
 *
 * Releases the output latch on every pin currently routed to the given
 * peripheral, returning pad control to the live GPIO/peripheral path.
 * Call this after the peripheral has been fully restored and re-enabled.
 *
 * @param[in] peripheral  The peripheral whose routed pins should be unlatched.
 *
 * @return SL_STATUS_OK on success.
 *         SL_STATUS_INVALID_PARAMETER if the peripheral has no ROUTEEN
 *         information (gpio_routeen_offset == 0xFFFF).
 ******************************************************************************/
sl_status_t sl_gpio_unlatch_peripheral_pins(sl_peripheral_t peripheral);

/// @} (end addtogroup gpio_peripheral_pin_latching)

#ifdef __cplusplus
}
#endif

#endif /* SL_GPIO_PERIPHERAL_PIN_LATCHING_H */
