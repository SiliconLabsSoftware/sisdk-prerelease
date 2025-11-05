/***************************************************************************//**
 * @file
 * @brief segment lcd timer examples functions
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
#include "app.h"
#include "sl_simple_button_instances.h"
#include "sl_clock_manager.h"
#include "em_device.h"
#include "em_emu.h"
#include "em_letimer.h"
#include "sl_segmentlcd.h"
#include "sl_gpio.h"

#define BTN0_INSTANCE (*(sl_simple_button_context_t *)sl_button_btn0.context)
#define BTN1_INSTANCE (*(sl_simple_button_context_t *)sl_button_btn1.context)

volatile uint32_t counter = 0;          // general time counter
uint32_t compare_mode = 0;     // enter compare mode set up
uint32_t compare_value = 0;    // compare value for compare mode
uint32_t compare_start = 0;    // start compare mode
uint32_t compare_match = 0;    // Flag triggered if counter = compare value
uint32_t letimer_enable = 0;   // LETIMER enable flag
volatile uint32_t hold_timer;  // Timer to counter hold time elapsed on PB1

/***************************************************************************//**
 * LETIEMR Interrupt Handler
 ******************************************************************************/
void LETIMER0_IRQHandler(void)
{
  // Clear all interrupt flags
  uint32_t flags = LETIMER_IntGet(LETIMER0);
  LETIMER_IntClear(LETIMER0, flags);

  // If compare match, re-configure counter as blinking flag
  if (compare_match) {
    counter = (counter + 1) % 2;
  }
  // If no compare match, continue counting up counter
  else {
    counter += 1;
    // hold_timer used to determine if compare mode should
    // be entered. This is triggered by holding down PB1 for
    // 2 or more seconds
    hold_timer += 1;
  }
}

/***************************************************************************//**
 * An internal callback called in interrupt context whenever a button changes
 * its state.
 *
 * @note The internal callback will be triggered when the Push button 0 is pressed.
 *
 * @param[in] int_no      Interrupt number (pin number)
 * @param[in] ctx         Pointer to context
 ******************************************************************************/
static void on_button_0_change(uint8_t int_no, void *ctx)
{
  (void)ctx;
  // Interrupt triggered by PB0 (PB1) falling edge
  if (int_no == BTN0_INSTANCE.pin) {
    // If in compare mode, then each press of PB0 will increment
    // compare_value by 1;
    if (compare_mode) {
      compare_value++;
    }
    // If a compare_match happened, that is counter == compare_value
    // then reset compare_match variable and counter variable
    else if (compare_match) {
      compare_match = 0;
      counter = 0;
    }
    // For all other cases, PB0 is used to start/stop the LETIMER
    else {
      letimer_enable = (letimer_enable + 1) % 2;
      LETIMER_Enable(LETIMER0, letimer_enable);
    }
  }
}

/***************************************************************************//**
 * An internal callback called in interrupt context whenever a button changes
 * its state.
 *
 * @note The internal callback will be triggered when the Push button 1 is pressed.
 *
 * @param[in] int_no      Interrupt number (pin number)
 * @param[in] ctx         Pointer to context
 ******************************************************************************/
static void on_button_1_change(uint8_t int_no, void *ctx)
{
  (void)ctx;
  if (int_no == BTN1_INSTANCE.pin) {
    LETIMER_Enable(LETIMER0, true);  // Need LETIMER running to detect compare
                                     // mode
    // If in compare mode set up
    if (compare_mode) {
      compare_start = 1;  // start compare mode
      compare_mode = 0;   // reset compare_mode variable
      letimer_enable = 0; // reset LETIMER flag
      LETIMER_Enable(LETIMER0, false);  // If in compare mode, disable LETIMER
    }
    // For all other cases, PB1 will act as a timer reset
    else {
      compare_match = 0;
      letimer_enable = 0;
    }
    counter = 0;
  }
}

/***************************************************************************//**
 * Initialize CMU
 ******************************************************************************/
void init_cmu(void)
{
  sl_clock_manager_enable_bus_clock(SL_BUS_CLOCK_LETIMER0);
  sl_clock_manager_enable_bus_clock(SL_BUS_CLOCK_GPIO);
}

/***************************************************************************//**
 * Initialize GPIO
 ******************************************************************************/
void init_gpio(void)
{
  int32_t int_no;
  sl_gpio_t gpio_btn0 = {
    .port = BTN0_INSTANCE.port,
    .pin = BTN0_INSTANCE.pin,
  };
  sl_gpio_t gpio_btn1 = {
    .port = BTN1_INSTANCE.port,
    .pin = BTN1_INSTANCE.pin,
  };

  // Configure PB1 (Push Button 0) as input with filter enabled
  // Enable PB1 interrupt on falling edge
  int_no = BTN0_INSTANCE.pin;
  sl_gpio_set_pin_mode(&gpio_btn0, SL_GPIO_MODE_INPUT_PULL_FILTER, 1);
  sl_gpio_configure_external_interrupt(&gpio_btn0, &int_no, SL_GPIO_INTERRUPT_FALLING_EDGE, (sl_gpio_irq_callback_t)on_button_0_change, NULL);

  // Configure PB6 (Push Button 1) as input with filter enabled
  // Enable PB6 interrupt on falling edge
  int_no  = BTN1_INSTANCE.pin;
  sl_gpio_set_pin_mode(&gpio_btn1, SL_GPIO_MODE_INPUT_PULL_FILTER, 1);
  sl_gpio_configure_external_interrupt(&gpio_btn1, &int_no, SL_GPIO_INTERRUPT_FALLING_EDGE, (sl_gpio_irq_callback_t)on_button_1_change, NULL);

  // Enable GPIO odd IRQHandler
  NVIC_ClearPendingIRQ(GPIO_ODD_IRQn);
  NVIC_EnableIRQ(GPIO_ODD_IRQn);

  // Enable GPIO even IRQHandler
  NVIC_ClearPendingIRQ(GPIO_EVEN_IRQn);
  NVIC_EnableIRQ(GPIO_EVEN_IRQn);
}

/***************************************************************************//**
 * Initialize LETIMER.
 ******************************************************************************/
void init_letimer(void)
{
  // LETIMER initialization
  // Top value = LF Clock frequency, compare match frequency = 1 HZ
  LETIMER_Init_TypeDef initLetimer = LETIMER_INIT_DEFAULT;
  initLetimer.enable = false;
  initLetimer.topValue = 32768;
  LETIMER_Init(LETIMER0, &initLetimer);

  LETIMER_IntDisable(LETIMER0, _LETIMER_IEN_MASK);
  LETIMER_IntEnable(LETIMER0, LETIMER_IEN_COMP0);

  NVIC_ClearPendingIRQ(LETIMER0_IRQn);
  NVIC_EnableIRQ(LETIMER0_IRQn);
}

/***************************************************************************//**
 * Initialize application.
 ******************************************************************************/
void segment_lcd_app_init(void)
{
  // Initialize peripheral clocks
  init_cmu();

  // GPIO configuration
  init_gpio();

  // LETIMER configuration
  init_letimer();

  // Configure LCD to use step down mode and disable unused segments
  // Default display value 0
  sl_segment_lcd_init(false);
  LCD->BIASCTRL_SET = LCD_BIASCTRL_VDDXSEL_AVDD;
  // Example only used upper numeric segments; disable unused segments
  SL_LCD_SEGMENTS_ALPHA_DIS();
  sl_segment_lcd_number(0);
}

/***************************************************************************//**
 * compare mode setup
 ******************************************************************************/
void counter_compare(void)
{
  compare_mode = 1;
  compare_value = 0;
  counter = 0;
  sl_segment_lcd_number(0);
  // display compare symbol
  sl_segment_lcd_symbol(SL_LCD_SYMBOL_DEGC, true);
}

/***************************************************************************//**
 * App ticking function.
 ******************************************************************************/
void segment_lcd_app_process_action(void)
{
  bool pin_value;
  sl_gpio_t gpio_btn1 = {
    .port = BTN1_INSTANCE.port,
    .pin = BTN1_INSTANCE.pin,
  };

  sl_gpio_get_pin_input(&gpio_btn1, &pin_value);

  // compare mode
  if (compare_mode) {
    // Display compare value and compare symbol
    sl_segment_lcd_number(compare_value);
    sl_segment_lcd_symbol(SL_LCD_SYMBOL_DEGC, true);
  }
  // compare mode started
  else if (compare_start) {
    // display timer counter
    sl_segment_lcd_number(counter);
    if (counter == compare_value) {
      compare_start = 0;
      compare_match = 1;
    }
  }
  // check if compare mode set up should be entered
  else if (pin_value == 0) {
    sl_segment_lcd_number(0);
    hold_timer = 0;
    do {
      sl_gpio_get_pin_input(&gpio_btn1, &pin_value);
    } while (pin_value == 0);

    if (hold_timer > 1) {
      counter_compare();
    }
    // Once completed, disable LETIMER to allow reset
    letimer_enable = 0;
    counter = 0;
    LETIMER_Enable(LETIMER0, letimer_enable);
  }
  // compare match
  else if (compare_match) {
    // reset compare mode related variables
    compare_start = 0;
    compare_mode = 0;
    // counter now behaves as a blinking flag, the LCD display will blink every
    // second to indicate that a compare match happened.
    if (counter) {
      sl_segment_lcd_all_off();
    } else {
      sl_segment_lcd_number(compare_value);
    }
  }
  // regular timer mode
  else {
    sl_segment_lcd_number(counter);
  }
}
