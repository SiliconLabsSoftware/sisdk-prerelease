/***************************************************************************//**
 * @file
 * @brief See @ref led and micro specific modules for documentation.
 *******************************************************************************
 * # License
 * <b>Copyright 2018 Silicon Laboratories Inc. www.silabs.com</b>
 *******************************************************************************
 *
 * The licensor of this software is Silicon Laboratories Inc. Your use of this
 * software is governed by the terms of Silicon Labs Master Software License
 * Agreement (MSLA) available at
 * www.silabs.com/about-us/legal/master-software-license-agreement. This
 * software is distributed to you in Source Code format and is governed by the
 * sections of the MSLA applicable to Source Code.
 *
 ******************************************************************************/
/** @addtogroup led
 * @brief Sample generic API funtions for controlling LEDs.
 *
 * When specifying an LED to use, always use the BOARDLEDx definitions that
 * are defined in the HalBoardLedPins enum in the micro specific led header.
 *
 * @note The micro specific definitions, @ref stm32f103ret_led, is
 * chosen by the build include path pointing at the appropriate directoy.
 *
 * See led-common.h for source code.
 *@{
 */

#ifndef __LED_COMMON_H__
#define __LED_COMMON_H__

/** @brief Configures GPIOs pertaining to the control of LEDs.
 */
void halInternalInitLed(void);

/** @brief Ensures that the definitions for the LEDs
 *  are always used as parameters to the LED functions.
 *
 * @note Even though many compilers will use 16 bits for an enum instead of 8,
 * we choose to use an enum here.  The possible compiler inefficiency does not
 * affect stack-based parameters and local variables, which is the
 * general case for led paramters.
 */
typedef enum HalBoardLedPins HalBoardLed;

#endif //__LED_COMMON_H__

/**@}END addtogroup
 */
