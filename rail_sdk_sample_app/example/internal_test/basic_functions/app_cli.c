/***************************************************************************//**
 * @file
 * @brief Blink examples functions
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

// -----------------------------------------------------------------------------
//                                   Includes
// -----------------------------------------------------------------------------
#include "sl_component_catalog.h"
#if defined(SL_CATALOG_LED0_PRESENT) || defined(SL_CATALOG_LED1_PRESENT)
#include "sl_simple_led_instances.h"
#endif
#if defined(SL_CATALOG_BTN0_PRESENT) || defined(SL_CATALOG_BTN1_PRESENT)
#include "sl_simple_button_instances.h"
#endif
#include "sl_sleeptimer.h"
#include "sl_cli.h"
#include "sl_udelay.h"
#include "printf.h"
#ifdef SL_CATALOG_GLIB_PRESENT
#include "dmd.h"
#include "glib.h"
#endif
#ifdef SL_CATALOG_POWER_MANAGER_PRESENT
#include "sl_power_manager.h"
#endif
#if defined(SL_CATALOG_KERNEL_PRESENT)
#include "app_task_init.h"
#endif
#include "sl_code_classification.h"

// -----------------------------------------------------------------------------
//                              Macros and Typedefs
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
//                          Static Function Declarations
// -----------------------------------------------------------------------------
/// Timer expiration callback for the tester function.
static void sleeptimer_tester_callback(sl_sleeptimer_timer_handle_t *handle, void *data);

#ifdef SL_CATALOG_POWER_MANAGER_PRESENT
/// Timer expiration callback for the tester function.
static void deep_sleep_tester_callback(sl_sleeptimer_timer_handle_t *handle, void *data);
#endif

#ifdef SL_CATALOG_GLIB_PRESENT
/// Display tester api
static void print_display_cli_command(const char *command);
#endif
// -----------------------------------------------------------------------------
//                                Global Variables
// -----------------------------------------------------------------------------
/// Variable to track button press over the app
#if defined(SL_CATALOG_BTN0_PRESENT)
extern volatile bool button_0_pressed;
#endif
#if defined(SL_CATALOG_BTN1_PRESENT)
extern volatile bool button_1_pressed;
#endif
/// Variable to track sleeptimer interrupt over the app
volatile bool sleeptimer_happened = false;
#ifdef SL_CATALOG_GLIB_PRESENT
/// Context used all over the graphics
extern GLIB_Context_t glib_context;
#endif
// -----------------------------------------------------------------------------
//                                Static Variables
// -----------------------------------------------------------------------------
/// Timer for the tester function.
static sl_sleeptimer_timer_handle_t sleeptimer_tester;

#ifdef SL_CATALOG_POWER_MANAGER_PRESENT
/// Timer for the deep sleep tester function.
static sl_sleeptimer_timer_handle_t deep_sleep_timer_tester;
#endif

// -----------------------------------------------------------------------------
//                          Public Function Definitions
// -----------------------------------------------------------------------------
/******************************************************************************
 * Basic CLI functionality to echo back the echo on enter
 *****************************************************************************/
void cli_echo(sl_cli_command_arg_t *arguments)
{
  (void) arguments;  // Unused parameter.
  printf("echo\n");
#ifdef SL_CATALOG_GLIB_PRESENT
  print_display_cli_command("echo");
#endif
}

/******************************************************************************
 * Toggle LED0 state and checks if it update in led0 instance
 *****************************************************************************/
void cli_led_0(sl_cli_command_arg_t *arguments)
{
  (void) arguments;  // Unused parameter.
#if defined(SL_CATALOG_LED0_PRESENT)
#ifdef SL_CATALOG_GLIB_PRESENT
  print_display_cli_command("led 0");
#endif
  sl_led_state_t original_state = sl_led_get_state(&sl_led_led0);
  sl_led_toggle(&sl_led_led0);
  if (original_state != sl_led_get_state(&sl_led_led0)) {
    printf("Led 0 toggle successful\n");
  } else {
    printf("Led 0 toggle failed\n");
  }
#else
  printf("Add led0 instance to the project to use this function\n");
#endif
}

/******************************************************************************
 * Toggle LED1 state and checks if it update in led1 instance
 *****************************************************************************/
void cli_led_1(sl_cli_command_arg_t *arguments)
{
  (void) arguments;  // Unused parameter.
#if defined(SL_CATALOG_LED1_PRESENT)
#ifdef SL_CATALOG_GLIB_PRESENT
  print_display_cli_command("led 1");
#endif
  sl_led_state_t original_state = sl_led_get_state(&sl_led_led1);
  sl_led_toggle(&sl_led_led1);
  if (original_state != sl_led_get_state(&sl_led_led1)) {
    printf("Led 1 toggle successful\n");
  } else {
    printf("Led 1 toggle failed\n");
  }
#else
  printf("Add led1 instance to the project to use this function\n");
#endif
}

/******************************************************************************
 * Waits for a BTN0 press interrupt
 *****************************************************************************/
void cli_button_0(sl_cli_command_arg_t *arguments)
{
  (void) arguments;  // Unused parameter.
#if defined(SL_CATALOG_BTN0_PRESENT)
#ifdef SL_CATALOG_GLIB_PRESENT
  print_display_cli_command("button 0");
#endif
  button_0_pressed = false;
  printf("Waiting for button 0 press\n");

  for (uint8_t cntr = 0; cntr < 100; cntr++) {
    if (button_0_pressed) {
      button_0_pressed = false;
      printf("Button 0 was pressed\n");
      break;
    }

#if !defined(SL_CATALOG_KERNEL_PRESENT)
    sl_udelay_wait(100000);
#else
    osDelay(100);
#endif
  }

  #if defined(SL_CATALOG_BTN1_PRESENT)
  if (button_1_pressed) {
    button_1_pressed = false;
  }
  #endif

#else
  printf("Add btn0 instance to the project to use this function\n");
#endif
}

/******************************************************************************
 * Waits for a BTN0 press interrupt
 *****************************************************************************/
void cli_button_1(sl_cli_command_arg_t *arguments)
{
  (void) arguments;  // Unused parameter.
#if defined(SL_CATALOG_BTN1_PRESENT)
#ifdef SL_CATALOG_GLIB_PRESENT
  print_display_cli_command("button 1");
#endif
  button_1_pressed = false;
  printf("Waiting for button 1 press\n");

  for (uint8_t cntr = 0; cntr < 100; cntr++) {
    if (button_1_pressed) {
      button_1_pressed = false;
      printf("Button 1 was pressed\n");
      break;
    }

#if !defined(SL_CATALOG_KERNEL_PRESENT)
    sl_udelay_wait(100000);
#else
    osDelay(100);
#endif
  }

#if defined(SL_CATALOG_BTN0_PRESENT)
  if (button_0_pressed) {
    button_0_pressed = false;
  }
#endif

#else
  printf("Add btn1 instance to the project to use this function\n");
#endif
}

/******************************************************************************
 * Start and waits for a sleep timer for 2 seconds
 *****************************************************************************/
void cli_sleeptimer(sl_cli_command_arg_t *arguments)
{
  (void) arguments;  // Unused parameter.
  sl_status_t sleep_timer_status = 0;
#ifdef SL_CATALOG_GLIB_PRESENT
  print_display_cli_command("sleeptimer");
#endif
  sleep_timer_status = sl_sleeptimer_start_timer_ms(&sleeptimer_tester, 2000, sleeptimer_tester_callback, NULL, 0, 0);
  if (sleep_timer_status != 0) {
    printf("Sleeptimer couldn't start!\n");
  }

  printf("Waiting for sleeptimer to return\n");
  while (true) {
    if (sleeptimer_happened) {
      sleeptimer_happened = false;
      printf("Waiting for sleeptimer returned\n");
      break;
    }
  }
}

#ifdef SL_CATALOG_POWER_MANAGER_PRESENT
void cli_deep_sleep(sl_cli_command_arg_t *arguments)
{
  (void) arguments;  // Unused parameter.
  sl_status_t sleep_timer_status = 0;
#ifdef SL_CATALOG_GLIB_PRESENT
  print_display_cli_command("deep_sleep");
#endif
  printf("After deep sleep returned reset device to be able to use CLI again!\n");
  sl_power_manager_remove_em_requirement(SL_POWER_MANAGER_EM1);
  sleep_timer_status = sl_sleeptimer_start_timer_ms(&deep_sleep_timer_tester, 2000, deep_sleep_tester_callback, NULL, 0, 0);
  if (sleep_timer_status != 0) {
    printf("Sleeptimer couldn't start!\n");
  }
  printf("Waiting for wake up\n");
}
#endif

// -----------------------------------------------------------------------------
//                          Static Function Definitions
// -----------------------------------------------------------------------------
SL_CODE_RAM static void sleeptimer_tester_callback(sl_sleeptimer_timer_handle_t *handle, void *data)
{
  (void)data;  // Unused parameter.
  (void)handle;  // Unused parameter.
  sleeptimer_happened = true;
  printf("Sleeptimer have ended\n");
#if defined(SL_CATALOG_KERNEL_PRESENT)
  app_task_notify();
#endif
}

#ifdef SL_CATALOG_GLIB_PRESENT
/******************************************************************************
 * API tu update display first raw with cli command name
 *****************************************************************************/
static void print_display_cli_command(const char *command)
{
  // Clear what's currently on screen
  GLIB_clear(&glib_context);
  // Add the demo output strings
  GLIB_drawStringOnLine(&glib_context, command, 1, GLIB_ALIGN_CENTER, 0, 0, 0);
  // Force a redraw
  DMD_updateDisplay();
}
#endif

#ifdef SL_CATALOG_POWER_MANAGER_PRESENT
SL_CODE_RAM static void deep_sleep_tester_callback(sl_sleeptimer_timer_handle_t *handle, void *data)
{
  (void)data;  // Unused parameter.
  (void)handle;  // Unused parameter.
  printf("Deep sleep ended\n");
#if defined(SL_CATALOG_KERNEL_PRESENT)
  app_task_notify();
#endif
}
#endif
