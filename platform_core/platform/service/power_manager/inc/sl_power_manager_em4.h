/***************************************************************************//**
 * @file
 * @brief Power Manager EM4 API definition.
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

#ifndef SL_POWER_MANAGER_EM4_H
#define SL_POWER_MANAGER_EM4_H

#ifdef __cplusplus
extern "C" {
#endif

/***************************************************************************//**
 * @addtogroup power_manager Power Manager
 * @{
 ******************************************************************************/

/***************************************************************************//**
 * @addtogroup power_manager_em4 EM4 Sleep
 * @brief Enter Energy Mode 4 (EM4) and manage EM4 pin retention.
 *
 * @details
 * The Power Manager module provides support for entering Energy Mode 4 (EM4),
 * the lowest energy mode available.
 *
 * To enter EM4, the @ref sl_power_manager_enter_em4() function is used. This function
 * ensures that the system transitions to EM4 safely and performs any necessary
 * pre-sleep operations via the @ref sl_power_manager_em4_presleep_hook() function,
 * which can be overridden by the application if additional actions are required.
 *
 * Additionally, the Power Manager provides support for EM4 pin retention. Use the
 * `SL_POWER_MANAGER_INIT_EMU_EM4_PIN_RETENTION_MODE` configuration to set the pin
 * retention mode in EM4. When enabled, pins retain their state through EM4 entry
 * and wake-up. The retained pin state can be released after wake-up by calling
 * the @ref sl_power_manager_em4_unlatch_pin_retention() function.
 *
 * Keep in mind that EM4 entry is irreversible, and waking up from this energy
 * mode will result in a system reset. Careful consideration should be given to
 * the conditions under which EM4 is entered.
 *
 * If `SL_SLEEPTIMER_PERIPHERAL` is set to `SL_SLEEPTIMER_PERIPHERAL_BURTC` in
 * `sl_sleeptimer_config.h`, it does not configure EM4 wake on BURTC and does
 * not support EM4 timekeeping; see @ref sleeptimer_burtc_em4 in the Sleeptimer
 * documentation.
 *
 * @{
 ******************************************************************************/

/***************************************************************************//**
 * Enter energy mode 4 (EM4).
 *
 * @note  You should not expect to return from this function. Once the device
 *        enters EM4, only a power on reset or external reset pin can wake the
 *        device.
 *
 * @note  On xG22 devices, this function re-configures the IADC if EM4 entry
 *        is possible.
 ******************************************************************************/
void sl_power_manager_enter_em4(void);

/***************************************************************************//**
 *   When EM4 pin retention is set to power_manager_pin_retention_latch,
 *   then pins are retained through EM4 entry and wakeup. The pin state is
 *   released by calling this function. The feature allows peripherals or
 *   GPIO to be re-initialized after EM4 exit (reset), and when
 *   initialization is done, this function can release pins and return
 *   control to the peripherals or GPIO.
 *
 * @note When the EM4 Pin Retention feature is not available on a device,
 *       calling this function will do nothing.
 ******************************************************************************/
void sl_power_manager_em4_unlatch_pin_retention(void);

/** @} (end addtogroup power_manager_em4) */
/** @} (end addtogroup power_manager) */

#ifdef __cplusplus
}
#endif

#endif /* SL_POWER_MANAGER_EM4_H */
