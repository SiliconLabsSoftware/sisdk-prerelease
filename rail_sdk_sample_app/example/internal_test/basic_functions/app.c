/***************************************************************************//**
 * @file
 * @brief Top level application functions
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
#include "sl_code_classification.h"
#include "sl_common.h"
#if defined(SL_CATALOG_LED0_PRESENT) || defined(SL_CATALOG_LED1_PRESENT)
#include "sl_simple_led_instances.h"
#endif
#if defined(SL_CATALOG_BTN0_PRESENT) || defined(SL_CATALOG_BTN1_PRESENT)
#include "sl_simple_button_instances.h"
#endif
#include "printf.h"
#ifdef SL_CATALOG_GLIB_PRESENT
#include "dmd.h"
#include "glib.h"
#endif
#if defined(SL_CATALOG_KERNEL_PRESENT)
#include "app_task_init.h"
#endif
#if defined(SL_CATALOG_APP_LOG_PRESENT)
#include "app_log.h"
#endif

// -----------------------------------------------------------------------------
//                              Macros and Typedefs
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
//                          Static Function Declarations
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
//                                Global Variables
// -----------------------------------------------------------------------------
/// Variable to track button press over the app
#if defined(SL_CATALOG_BTN0_PRESENT)
volatile bool button_0_pressed = false;
#endif
#if defined(SL_CATALOG_BTN1_PRESENT)
volatile bool button_1_pressed = false;
#endif
#ifdef SL_CATALOG_GLIB_PRESENT
/// Context used all over the graphics
GLIB_Context_t glib_context;
#endif

// -----------------------------------------------------------------------------
//                          Public Function Definitions
// -----------------------------------------------------------------------------
/******************************************************************************
 * Print sample app name
 *****************************************************************************/
SL_WEAK void print_sample_app_name(const char* app_name)
{
#ifdef SL_CATALOG_APP_LOG_PRESENT
  app_log_info("%s\n", app_name);
#endif
  // Avoid unused parameter warning
  (void)app_name;
}

/***************************************************************************//**
 * Initialize application.
 ******************************************************************************/
void rail_app_init(void)
{
#if defined(SL_CATALOG_LED0_PRESENT)
  sl_led_turn_off(&sl_led_led0);
#endif
#if defined(SL_CATALOG_LED1_PRESENT)
  sl_led_turn_off(&sl_led_led1);
#endif
  print_sample_app_name("Basic Function Tester");

#ifdef SL_CATALOG_GLIB_PRESENT
  EMSTATUS status;

  /* Initialize the DMD module for the DISPLAY device driver. */
  status = DMD_init(0);
  if (DMD_OK != status) {
    while (1) ;
  }

  status = GLIB_contextInit(&glib_context);
  if (GLIB_OK != status) {
    while (1) ;
  }

  glib_context.backgroundColor = White;
  glib_context.foregroundColor = Black;

  /* Use Normal font */
  GLIB_setFont(&glib_context, (GLIB_Font_t *)&GLIB_FontNormal8x8);
#endif
}

void app_init(void)
{
#if !defined(SL_CATALOG_KERNEL_PRESENT)
  rail_app_init();
#else
  app_task_init();
#endif
}

/***************************************************************************//**
 * App ticking function.
 ******************************************************************************/
void app_process_action(void)
{
// Empty, all parts are in the CLI file
#if defined(SL_CATALOG_KERNEL_PRESENT)
  printf("Task is activated\n");
#endif
}

#if defined(SL_CATALOG_BTN0_PRESENT) || defined(SL_CATALOG_BTN1_PRESENT)
/******************************************************************************
 * Button callback, called if any button is pressed or released.
 *****************************************************************************/
SL_CODE_RAM void sl_button_on_change(const sl_button_t *handle)
{
  if (sl_button_get_state(handle) == SL_SIMPLE_BUTTON_PRESSED) {
#if defined(SL_CATALOG_BTN0_PRESENT)
    if (handle == &sl_button_btn0) {
      printf("Button 0 is pressed\n");
      button_0_pressed = true;
#if defined(SL_CATALOG_KERNEL_PRESENT)
      app_task_notify();
#endif
    }
#endif
#if defined(SL_CATALOG_BTN1_PRESENT)
    if (handle == &sl_button_btn1) {
      printf("Button 1 is pressed\n");
      button_1_pressed = true;
#if defined(SL_CATALOG_KERNEL_PRESENT)
      app_task_notify();
#endif
    }
#endif
  }
}
#endif
