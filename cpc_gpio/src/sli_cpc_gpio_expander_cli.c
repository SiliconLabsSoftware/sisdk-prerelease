/***************************************************************************/ /**
 * @file
 * @brief CPC GPIO Expander CLI
 *******************************************************************************
 * # License
 * <b>Copyright 2023 Silicon Laboratories Inc. www.silabs.com</b>
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

#if defined(SL_COMPONENT_CATALOG_PRESENT)
#include "sl_component_catalog.h"
#endif

#include "sl_cpc_gpio_expander.h"
#include "sl_cpc_gpio_expander_config.h"
#include "sl_iostream.h"
#include "sl_cli.h"
#include "sl_string.h"
#include "sl_status.h"
#include "em_gpio.h"

/*******************************************************************************
 ***************************  GLOBAL VARIABLES   *******************************
 ******************************************************************************/

// GPIO Expander table
extern sl_cpc_gpio_expander_handle_t sl_cpc_gpio_expander_handle_table[];
extern uint32_t sl_cpc_gpio_expander_handle_count;

#if defined(SLI_CPC_GPIO_EXPANDER_UNDER_TEST)
// Sequence number offset, injected during testing
uint8_t sli_cpc_gpio_expander_under_test_seq_offset;
#endif

/*******************************************************************************
 **************************   GLOBAL FUNCTIONS   *******************************
 ******************************************************************************/

/***************************************************************************//**
 * Get value of GPIO.
 * CLI arguments:
 *   pin     Host pin number
 * Return:
 *   success True on success, otherwise false
 *   value   On success, value is 0 for Low or 1 for High
 ******************************************************************************/
void sli_cpc_gpio_expander_get_value(sl_cli_command_arg_t *arguments)
{
  uint32_t pin = sl_cli_get_argument_uint32(arguments, 0);

  if (pin < sl_cpc_gpio_expander_handle_count) {
    uint32_t state = GPIO_PinInGet(sl_cpc_gpio_expander_handle_table[pin].port,
                                   sl_cpc_gpio_expander_handle_table[pin].pin);
    sl_iostream_printf(SL_IOSTREAM_STDOUT, "success = true\nvalue = %d\n", state ? 1 : 0);
  } else {
    sl_iostream_printf(SL_IOSTREAM_STDOUT, "success = false\n");
  }
}

/***************************************************************************//**
 * Set value of GPIO.
 * CLI arguments:
 *   pin     Host pin number
 *   value   0 for Low, 1 for High
 * Return:
 *   success True on success, otherwise false
 ******************************************************************************/
void sli_cpc_gpio_expander_set_value(sl_cli_command_arg_t *arguments)
{
  uint32_t pin = sl_cli_get_argument_uint32(arguments, 0);
  uint32_t value = sl_cli_get_argument_uint32(arguments, 1);

  if (pin < sl_cpc_gpio_expander_handle_count) {
    if (value) {
      GPIO_PinOutSet(sl_cpc_gpio_expander_handle_table[pin].port,
                     sl_cpc_gpio_expander_handle_table[pin].pin);
    } else {
      GPIO_PinOutClear(sl_cpc_gpio_expander_handle_table[pin].port,
                       sl_cpc_gpio_expander_handle_table[pin].pin);
    }
    sl_iostream_printf(SL_IOSTREAM_STDOUT, "success = true\n");
  } else {
    sl_iostream_printf(SL_IOSTREAM_STDOUT, "success = false\n");
  }
}

/***************************************************************************//**
 * Get direction of GPIO.
 * CLI arguments:
 *   pin        Host pin number
 * Return:
 *   success    True on success, otherwise false
 *   direction  On success,
 *              0 = Disabled
 *              1 = Input
 *              2 = InputPullUp
 *              3 = InputPullDown
 *              4 = OpenDrain
 *              5 = OpenDrainPullUp
 *              6 = OpenSource
 *              7 = OpenSourcePullDown
 *              8 = PushPull
 ******************************************************************************/
void sli_cpc_gpio_expander_get_direction(sl_cli_command_arg_t *arguments)
{
  uint32_t pin = sl_cli_get_argument_uint32(arguments, 0);

  if (pin < sl_cpc_gpio_expander_handle_count) {
    GPIO_Mode_TypeDef mode = GPIO_PinModeGet(sl_cpc_gpio_expander_handle_table[pin].port,
                                             sl_cpc_gpio_expander_handle_table[pin].pin);

    switch (mode) {
      case gpioModeDisabled:
        sl_iostream_printf(SL_IOSTREAM_STDOUT, "success = true\ndirection = 0\n");
        break;
      case gpioModeInput:
        sl_iostream_printf(SL_IOSTREAM_STDOUT, "success = true\ndirection = 1\n");
        break;
      case gpioModeInputPull: {
        unsigned int dout = GPIO_PinOutGet(sl_cpc_gpio_expander_handle_table[pin].port,
                                           sl_cpc_gpio_expander_handle_table[pin].pin);
        if (dout) {
          sl_iostream_printf(SL_IOSTREAM_STDOUT, "success = true\ndirection = 2\n");
        } else {
          sl_iostream_printf(SL_IOSTREAM_STDOUT, "success = true\ndirection = 3\n");
        }
      }
      break;
      case gpioModeWiredAnd:
        sl_iostream_printf(SL_IOSTREAM_STDOUT, "success = true\ndirection = 4\n");
        break;
      case gpioModeWiredAndPullUp:
        sl_iostream_printf(SL_IOSTREAM_STDOUT, "success = true\ndirection = 5\n");
        break;
      case gpioModeWiredOr:
        sl_iostream_printf(SL_IOSTREAM_STDOUT, "success = true\ndirection = 6\n");
        break;
      case gpioModeWiredOrPullDown:
        sl_iostream_printf(SL_IOSTREAM_STDOUT, "success = true\ndirection = 7\n");
        break;
      case gpioModePushPull:
        sl_iostream_printf(SL_IOSTREAM_STDOUT, "success = true\ndirection = 8\n");
        break;
      default:
        sl_iostream_printf(SL_IOSTREAM_STDOUT, "success = false\n");
        break;
    }
  } else {
    sl_iostream_printf(SL_IOSTREAM_STDOUT, "success = false\n");
  }
}

/***************************************************************************//**
 * Set direction of GPIO.
 * CLI arguments:
 *   pin        Host pin number
 *   direction  0 = Disabled
 *              1 = Input
 *              2 = InputPullUp
 *              3 = InputPullDown
 *              4 = OpenDrain
 *              5 = OpenDrainPullUp
 *              6 = OpenSource
 *              7 = OpenSourcePullDown
 *              8 = PushPull
 * Return:
 *   success    True on success, otherwise false
 ******************************************************************************/
void sli_cpc_gpio_expander_set_direction(sl_cli_command_arg_t *arguments)
{
  enum GpioDirection {
    Disabled = 0,
    Input = 1,
    InputPullUp = 2,
    InputPullDown = 3,
    OutputOpenDrain = 4,
    OutputOpenDrainPullUp = 5,
    OutputOpenSource = 6,
    OutputOpenSourcePullDown = 7,
    OutputPushPull = 8,
  };

  uint32_t pin = sl_cli_get_argument_uint32(arguments, 0);
  uint32_t direction = sl_cli_get_argument_uint32(arguments, 1);

  if (pin < sl_cpc_gpio_expander_handle_count) {
    switch (direction) {
      case Disabled:
        GPIO_PinModeSet(sl_cpc_gpio_expander_handle_table[pin].port,
                        sl_cpc_gpio_expander_handle_table[pin].pin, gpioModeDisabled, 0);
        sl_iostream_printf(SL_IOSTREAM_STDOUT, "success = true\n");
        break;
      case Input:
        GPIO_PinModeSet(sl_cpc_gpio_expander_handle_table[pin].port,
                        sl_cpc_gpio_expander_handle_table[pin].pin, gpioModeInput, 0);
        sl_iostream_printf(SL_IOSTREAM_STDOUT, "success = true\n");
        break;
      case InputPullUp:
        GPIO_PinModeSet(sl_cpc_gpio_expander_handle_table[pin].port,
                        sl_cpc_gpio_expander_handle_table[pin].pin, gpioModeInputPull, 1);
        sl_iostream_printf(SL_IOSTREAM_STDOUT, "success = true\n");
        break;
      case InputPullDown:
        GPIO_PinModeSet(sl_cpc_gpio_expander_handle_table[pin].port,
                        sl_cpc_gpio_expander_handle_table[pin].pin, gpioModeInputPull, 0);
        sl_iostream_printf(SL_IOSTREAM_STDOUT, "success = true\n");
        break;
      case OutputOpenDrain:
        GPIO_PinModeSet(sl_cpc_gpio_expander_handle_table[pin].port,
                        sl_cpc_gpio_expander_handle_table[pin].pin, gpioModeWiredAnd, 0);
        sl_iostream_printf(SL_IOSTREAM_STDOUT, "success = true\n");
        break;
      case OutputOpenDrainPullUp:
        GPIO_PinModeSet(sl_cpc_gpio_expander_handle_table[pin].port,
                        sl_cpc_gpio_expander_handle_table[pin].pin, gpioModeWiredAndPullUp, 0);
        sl_iostream_printf(SL_IOSTREAM_STDOUT, "success = true\n");
        break;
      case OutputOpenSource:
        GPIO_PinModeSet(sl_cpc_gpio_expander_handle_table[pin].port,
                        sl_cpc_gpio_expander_handle_table[pin].pin, gpioModeWiredOr, 0);
        sl_iostream_printf(SL_IOSTREAM_STDOUT, "success = true\n");
        break;
      case OutputOpenSourcePullDown:
        GPIO_PinModeSet(sl_cpc_gpio_expander_handle_table[pin].port,
                        sl_cpc_gpio_expander_handle_table[pin].pin, gpioModeWiredOrPullDown, 0);
        sl_iostream_printf(SL_IOSTREAM_STDOUT, "success = true\n");
        break;
      case OutputPushPull:
        GPIO_PinModeSet(sl_cpc_gpio_expander_handle_table[pin].port,
                        sl_cpc_gpio_expander_handle_table[pin].pin, gpioModePushPull, 0);
        sl_iostream_printf(SL_IOSTREAM_STDOUT, "success = true\n");
        break;
      default:
        sl_iostream_printf(SL_IOSTREAM_STDOUT, "success = false\n");
        break;
    }
  } else {
    sl_iostream_printf(SL_IOSTREAM_STDOUT, "success = false\n");
  }
}

/***************************************************************************//**
 * Get name of GPIO.
 * CLI arguments:
 *   pin      Host pin number
 * Return:
 *   success  True on success, otherwise false
 *   name     On success, name of the pin
 ******************************************************************************/
void sli_cpc_gpio_expander_get_name(sl_cli_command_arg_t *arguments)
{
  uint32_t pin = sl_cli_get_argument_uint32(arguments, 0);

  if (pin < sl_cpc_gpio_expander_handle_count) {
    const char *name = sl_cpc_gpio_expander_handle_table[pin].name;
    sl_iostream_printf(SL_IOSTREAM_STDOUT, "success = true\nname = \"%s\"\n", name);
  } else {
    sl_iostream_printf(SL_IOSTREAM_STDOUT, "success = false\n");
  }
}

/***************************************************************************//**
 * Get the GPIO Expander Chip Label string
 * CLI arguments:
 *   None
 * Return:
 *   success  True on success, otherwise false
 *   label    The null terminated chip label string
 ******************************************************************************/
void sli_cpc_gpio_expander_get_chip_label(sl_cli_command_arg_t *arguments)
{
  (void) arguments;
  const char *label = SL_CPC_GPIO_EXPANDER_CHIP_LABEL;
  sl_iostream_printf(SL_IOSTREAM_STDOUT, "success = true\nlabel = \"%s\"\n", label);
}

/***************************************************************************//**
 * Set sequence number offset [SLI_CPC_GPIO_EXPANDER_UNDER_TEST]
 * CLI arguments:
 *   offset   Sequence number offset
 * Return:
 *   success  True on success, otherwise false
 ******************************************************************************/
void sli_cpc_gpio_expander_set_seq_offset(sl_cli_command_arg_t *arguments)
{
#if defined(SLI_CPC_GPIO_EXPANDER_UNDER_TEST)
  uint8_t offset = sl_cli_get_argument_uint8(arguments, 0);
  sli_cpc_gpio_expander_under_test_seq_offset += offset;
  sl_iostream_printf(SL_IOSTREAM_STDOUT, "success = true\n");
#else
  (void) arguments;
  sl_iostream_printf(SL_IOSTREAM_STDOUT, "success = false\n");
#endif
}
