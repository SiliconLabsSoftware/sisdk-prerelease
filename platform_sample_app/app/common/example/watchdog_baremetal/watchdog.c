/***************************************************************************//**
 * @file
 * @brief Watchdog examples functions
 *******************************************************************************
 * # License
 * <b>Copyright 2026 Silicon Laboratories Inc. www.silabs.com</b>
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

#include <stdio.h>
#include "sl_code_classification.h"
#include "sl_simple_button_instances.h"
#include "sl_iostream_init_instances.h"
#include "sl_udelay.h"
#include "sl_sleeptimer.h"

#include "sl_watchdog_manager.h"
#if defined(SL_CATALOG_POWER_MANAGER_PRESENT)
#include "sl_power_manager.h"
#endif

/*******************************************************************************
 *******************************   DEFINES   ***********************************
 ******************************************************************************/

#ifndef TOOGLE_DELAY_MS
#define TOOGLE_DELAY_MS         1000
#endif

#ifndef BUTTON_INSTANCE_0
#define BUTTON_INSTANCE_0   sl_button_btn0
#endif

#ifndef BUTTON_INSTANCE_1
#define BUTTON_INSTANCE_1   sl_button_btn1
#endif

/*******************************************************************************
 ***************************  LOCAL VARIABLES   ********************************
 ******************************************************************************/

sl_sleeptimer_timer_handle_t timer;
volatile bool toggle_timeout = false;

sl_watchdog_handle_t my_watchdog_0;
sl_watchdog_handle_t my_watchdog_1;
sl_status_t status;

static volatile bool btn_pressed[2] = {true, true};

static  bool watchdog_0_faulty = false;
static  bool watchdog_1_faulty = false;
/*******************************************************************************
 *********************   LOCAL FUNCTION PROTOTYPES   ***************************
 ******************************************************************************/

 SL_CODE_RAM static void on_timeout(sl_sleeptimer_timer_handle_t *handle,
                                    void *data);

/*******************************************************************************
 **************************   GLOBAL FUNCTIONS   *******************************
 ******************************************************************************/

#if defined(SL_CATALOG_POWER_MANAGER_PRESENT)  && !defined(_WDOG_CFG_EM1RUN_MASK)
/***************************************************************************//**
 * Force the devices without EM1RUN always be awake.
 *
 * This function is used for the devices that don't have EM1RUN while
 * the application includes power manager component.
 *
 ******************************************************************************/
bool app_is_ok_to_sleep(void)
{
  return false; // prevent the device from sleeping
}
#endif

/***************************************************************************//**
 * Callback on button change.
 *
 * This function overrides a weak implementation defined in the simple_button
 * module. It is triggered when the user activates one of the buttons.
 *
 ******************************************************************************/

void sl_button_on_change(const sl_button_t *handle)
{
  if (sl_button_get_state(handle) == SL_SIMPLE_BUTTON_PRESSED) {
    if (&BUTTON_INSTANCE_0 == handle) {
      btn_pressed[0] = !btn_pressed[0];
      printf("[BTN] Button 0 pressed\r\n");
    } else if (&BUTTON_INSTANCE_1 == handle) {
      btn_pressed[1] = !btn_pressed[1];
      printf("[BTN] Button 1 pressed\r\n");
    }
  }
}

/***************************************************************************//**
 * Initialize blink example.
 ******************************************************************************/
void watchdog_init(void)
{
  printf("\r\n***************************************************\r\n");
  printf("STARTING WATCHDOG EXAMPLE\r\n");
  printf("--------------------------------------------------------\r\n");
  if (watchdog_0_faulty == true) {
    printf("[WDOG] Watchdog 0 was fault last time\r\n");
  }
  if (watchdog_1_faulty == true) {
    printf("[WDOG] Watchdog 1 was fault last time\r\n");
  }
  sl_udelay_wait(50000);
  printf("--------------------------------------------------------\r\n");

  // Create a watchdog with unique ID
  status = sl_watchdog_manager_create(&my_watchdog_0, 0x12345678);
  if (status == SL_STATUS_OK) {
    // Watchdog created successfully
    sl_watchdog_manager_enable(&my_watchdog_0);
    printf("[WDOG] Watchdog 0 created\r\n");
  }

  status = sl_watchdog_manager_create(&my_watchdog_1, 0x12345679);
  if (status == SL_STATUS_OK) {
    // Watchdog created successfully
    sl_watchdog_manager_enable(&my_watchdog_1);
    printf("[WDOG] Watchdog 1 created\r\n");
  }

  // Force feed the watchdog to prevent the system from resetting in initialization phase.
  sl_watchdog_manager_force_feed();

  // Create timer for waking up the system periodically.
  sl_sleeptimer_start_periodic_timer_ms(&timer,
                                        TOOGLE_DELAY_MS,
                                        on_timeout, NULL,
                                        0,
                                        SL_SLEEPTIMER_NO_HIGH_PRECISION_HF_CLOCKS_REQUIRED_FLAG);
  printf("[TIMER] Timer created\r\n");

}

/***************************************************************************//**
 * Watchdog feed function.
 ******************************************************************************/
void watchdog_process_action(void)
{
  if (toggle_timeout == true) {
    if (btn_pressed[0] == true) {
      sl_watchdog_manager_feed(&my_watchdog_0);
      printf("[APP] Watchdog 0 fed");
    } else {
      // Do nothing
      printf("[APP] Watchdog 0 not fed");
    }
    printf(" - ");
    if (btn_pressed[1] == true) {
      sl_watchdog_manager_feed(&my_watchdog_1);
      printf("Watchdog 1 fed");
    } else {
      // Do nothing
      printf("Watchdog 1 not fed");
    }
    printf("\r\n");
    toggle_timeout = false;
  }
}

/***************************************************************************//**
 * Sleeptimer timeout callback.
 ******************************************************************************/
 static void on_timeout(sl_sleeptimer_timer_handle_t *handle,
                        void *data)
{
  (void)&handle;
  (void)&data;
  toggle_timeout = true;
}

void watchdog_retrieve_faulty(void)
{
  sl_watchdog_handle_t faulty_handle;
  sl_watchdog_manager_retrieve_faulty(&faulty_handle);
  if (faulty_handle == 1) {
    watchdog_0_faulty = true;
  } else if (faulty_handle == 2) {
    watchdog_1_faulty = true;
  }
}
