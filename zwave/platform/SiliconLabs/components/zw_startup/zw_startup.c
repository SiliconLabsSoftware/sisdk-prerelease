/***************************************************************************//**
 * # License
 * <b> Copyright 2023 Silicon Laboratories Inc. www.silabs.com </b>
 *******************************************************************************
 *
 * The licensor of this software is Silicon Laboratories Inc. Your use of this
 * software is governed by the terms of the Silicon Labs Master Software License
 * Agreement (MSLA) available at
 * https://www.silabs.com/about-us/legal/master-software-license-agreement
 * By installing, copying or otherwise using this software, you agree to the
 * terms of the MSLA.
 *
 ******************************************************************************/

#include <assert.h>
#include <stdint.h>
#include <string.h>

#include "FreeRTOS.h"
#include "task.h"

#include "btl_reset_info.h"
#include "em_burtc.h"
#include "em_device.h"
#include "em_emu.h"
#include "em_gpio.h"
#include "em_rmu.h"
#include "nvm3_default.h"
#include "sl_clock_manager.h"
#include "sl_dcdc.h"
#include "sl_device_peripheral.h"

#include "zpal_init.h"
#include "zpal_bootloader.h"
#include "zpal_retention_register.h"
#include "zpal_retention_register_private.h"
#include "zpal_watchdog.h"

#include "SizeOf.h"
#include "ZW_dynamic_tx_power_algorithm_api.h"
#include "zw_dtpa_config.h"
#include "zw_hardware_config.h"
#include "zw_startup.h"

/***************************************************************************//**
 * @brief Gets the BURTC counter frequency.
 *
 * This function querries peripheral and clock manager modules
 * to retreive actual BURTC clock source frequencies, divided by the configured
 * clock divider (CNTPRESC) from the CFG register.
 *
 * @return The current BURTC main counter frequency in Hz.
 ******************************************************************************/
uint32_t zw_startup_get_burtc_counter_frequency_hz(void)
{
  uint32_t clock_freq = 0;
  uint32_t cntpresc;
  sl_clock_branch_t burtc_clock_branch;

  burtc_clock_branch = sl_device_peripheral_get_clock_branch(SL_PERIPHERAL_BURTC);
  sl_clock_manager_get_clock_branch_frequency(burtc_clock_branch, &clock_freq);
  // Read CNTPRESC field from CFG register (bits [7:4])
  cntpresc = (BURTC->CFG & _BURTC_CFG_CNTPRESC_MASK) >> _BURTC_CFG_CNTPRESC_SHIFT;
  return clock_freq >> cntpresc;
}

static int system_startup_core(uint32_t ResetCause);

typedef struct {
  BootloaderResetCause_t reset_info;
  uint32_t completed_sleep_duration_ms;
  uint32_t wake_up_ticks;
  uint32_t go_to_sleep_ticks;
  uint32_t em4_wakeup_gpio_flags;
} zw_startup_ctxt;

static zw_startup_ctxt startup_context = {
  .completed_sleep_duration_ms = UINT32_MAX,
  .wake_up_ticks = UINT32_MAX,
  .go_to_sleep_ticks = UINT32_MAX,
};

bool IsWakeupCausedByRtccTimeout(void)
{
  return !startup_context.em4_wakeup_gpio_flags;
}

uint32_t GetCompletedSleepDurationMs(void)
{
  /* completed_sleep_duration_ms is only written to before the scheduler
   * is started. I.e. it is safe to call this function from any thread
   * after the scheduler is started. */
  return startup_context.completed_sleep_duration_ms;
}

uint32_t GetDeepSleepWakeupTick(void)
{
  /* wake_up_ticks is only written to before the scheduler
   * is started. I.e. it is safe to call this function from any thread
   * after the scheduler is started. */
  return startup_context.wake_up_ticks;
}

uint32_t GetLastTickBeforeDeepSleep(void)
{
  /* go_to_sleep_ticks is only written to before the scheduler
   * is started. I.e. it is safe to call this function from any thread
   * after the scheduler is started. */
  return startup_context.go_to_sleep_ticks;
}

bool zw_startup_is_first_boot(bool *updated_successfully)
{
  *updated_successfully = false;

  // First boot is always triggered by Software Reset.
  if (zpal_get_reset_reason() != ZPAL_RESET_REASON_SOFTWARE) {
    return false;
  }

  if (startup_context.reset_info.reason == BOOTLOADER_RESET_REASON_GO) {
    *updated_successfully = true;
  }

  return ((BOOTLOADER_RESET_REASON_GO <= startup_context.reset_info.reason)
          && (BOOTLOADER_RESET_REASON_TIMEOUT >= startup_context.reset_info.reason));
}

/**
 * @brief Initialize Z-Wave platform features before platform services.
 */
void sl_zwave_platform_startup(void)
{
  __attribute__((unused)) sl_status_t clock_status = SL_STATUS_OK;
  /*
   * Early startup code for Z-Wave platform features that need to be
   * initialized before platform services.
   */
  clock_status = sl_clock_manager_enable_bus_clock(SL_BUS_CLOCK_BURTC);
  assert(clock_status == SL_STATUS_OK);

  // Store wake up burtc tick value
  startup_context.wake_up_ticks = BURTC_CounterGet();
}

/**
 * @brief Initialize the Z-Wave protocol after hardware startup.
 */
void sl_zwave_protocol_startup(void)
{
  /* Perform minimal legacy startup actions prior to protocol startup. */
  uint32_t ResetCause = RMU_ResetCauseGet();
  RMU_ResetCauseClear();

  /* Call Z-Wave protocol startup */
  system_startup_core(ResetCause);
}

/**
 * @brief Capture EM4 wake-up GPIO flags before unlatching pin retention.
 *
 * This callback is invoked before the GPIO pin-retention latch is released
 * during wake-up from EM4. It enables the GPIO clock, stores the EM4 wake-up
 * flags in the startup context, and clears the corresponding GPIO interrupt
 * flags.
 */
void on_pre_unlatch_pin_retention(void)
{
  __attribute__((unused)) sl_status_t clock_status = SL_STATUS_OK;
  clock_status = sl_clock_manager_enable_bus_clock(SL_BUS_CLOCK_GPIO);
  assert(clock_status == SL_STATUS_OK);

  startup_context.em4_wakeup_gpio_flags = GPIO_IntGet() & _GPIO_IF_EM4WU_MASK;
  GPIO_IntClear(startup_context.em4_wakeup_gpio_flags);
}

uint32_t getWakeUpFlags(void)
{
  return startup_context.em4_wakeup_gpio_flags;
}

/**
 * @brief Perform the Silicon Labs Z-Wave system startup sequence.
 *
 * @param[in] ResetCause Reset cause value read from the reset management unit.
 *
 * @return Zero after the startup sequence has completed.
 */
static int system_startup_core(uint32_t ResetCause)
{
  __attribute__((unused)) sl_status_t clock_status = SL_STATUS_OK;

  /* Configure EM4 behavior */
  EMU_EM4Init_TypeDef initEM4 = EMU_EM4INIT_DEFAULT;
  EMU_EM23Init_TypeDef initEM23 = EMU_EM23INIT_DEFAULT;
  BootloaderResetCause_t *resetinfo = (BootloaderResetCause_t *)SRAM_BASE;

  initEM4.em4State = ZW_EM4_INITIAL_STATE;
  initEM4.retainLfxo = ZW_EM4_RETAIN_LFXO;
  initEM4.retainLfrco = ZW_EM4_RETAIN_LFRCO;
  initEM4.retainUlfrco = ZW_EM4_RETAIN_ULFRCO;
  /*
   * Even though we select "emuPinRetentionLatch" below it must be selected
   * again *after* configuring EM4 GPIO and *before* going to sleep in EM4.
   * Use:
   *   EMU->EM4CTRL = (EMU->EM4CTRL & ~_EMU_EM4CTRL_EM4IORETMODE_MASK) | EMU_EM4CTRL_EM4IORETMODE_SWUNLATCH;
   *
   * This is caused by a bug fixed in emlib 5.5.
   * Currently we're using emlib 5.4 where GPIO_EM4SetPinRetention(true)
   * (called by GPIO_EM4EnablePinWakeup()) will force pinRetentionMode
   * to "emuPinRetentionEm4Exit".
   */
  initEM4.pinRetentionMode = ZW_EM4_PIN_RETENTION_MODE;
  EMU_EM4Init(&initEM4);

  // Low-power optimized voltage level. Using this voltage level in EM2 and 3
  // adds approximately 30 us to wakeup time
  initEM23.vScaleEM23Voltage = ZW_EM23_VOLTAGE_SCALE;
  EMU_EM23Init(&initEM23);

  clock_status = sl_clock_manager_enable_bus_clock(SL_BUS_CLOCK_GPIO);
  assert(clock_status == SL_STATUS_OK);

  // Save the bootloader reset cause in a global variable
  memcpy(&startup_context.reset_info, resetinfo, sizeof(BootloaderResetCause_t));
  // Make sure to clear this reason so that we do not persist this value at every boot
  resetinfo->reason = 0xFFFF; // Using a value unknown to the bootloader (btl_reset_info.h)

  // Check if the bootloader reset info is valid, if it is then check
  // the status code and perform the appropriate action.
  if (startup_context.reset_info.signature == BOOTLOADER_RESET_SIGNATURE_VALID) {
    if (startup_context.reset_info.reason == BOOTLOADER_RESET_REASON_UPGRADE) {
      // We have been performing a firmware upgrade.
    }
    startup_context.reset_info.signature = BOOTLOADER_RESET_SIGNATURE_INVALID;
  } else {
    // This is most likely a Power on reset
  }

  zpal_reset_reason_t eResetReason = ZPAL_RESET_REASON_PIN;
  /* Bit fields for GPIO IF */
  // Decode Reset cause register into enum
  if (0 != (ResetCause & EMU_RSTCAUSE_POR)) {
    eResetReason = ZPAL_RESET_REASON_POWER_ON;
  } else if (0 != (ResetCause & (EMU_RSTCAUSE_AVDDBOD | EMU_RSTCAUSE_DVDDBOD | EMU_RSTCAUSE_DECBOD))) {
    eResetReason = ZPAL_RESET_REASON_BROWNOUT;
  } else if (0 != (ResetCause & EMU_RSTCAUSE_PIN)) {
    eResetReason = ZPAL_RESET_REASON_PIN;
  } else if (0 != (ResetCause & EMU_RSTCAUSE_SYSREQ)) {
    eResetReason = ZPAL_RESET_REASON_SOFTWARE;
  } else if (0 != (ResetCause & EMU_RSTCAUSE_WDOG0)) {
    eResetReason = ZPAL_RESET_REASON_WATCHDOG;
  } else if (0 != (ResetCause & EMU_RSTCAUSE_EM4)) { /* Reset is caused by EM4 mode exit */
    if (startup_context.em4_wakeup_gpio_flags) {
      eResetReason = ZPAL_RESET_REASON_DEEP_SLEEP_EXT_INT;
    } else {
      eResetReason = ZPAL_RESET_REASON_DEEP_SLEEP_WUT;
    }
  } else if (0 != (ResetCause & EMU_RSTCAUSE_SETAMPER)) {
    eResetReason = ZPAL_RESET_REASON_TAMPER;
  } else {
    eResetReason = ZPAL_RESET_REASON_OTHER;
  }
  zpal_set_reset_reason(eResetReason);

  zpal_retention_register_init();

  // Only clear Retention Registers when the reset reason is a hard reset because
  // the state is undefined when coming out of a hard reset
  if (eResetReason == ZPAL_RESET_REASON_POWER_ON
      || eResetReason == ZPAL_RESET_REASON_PIN
      || eResetReason == ZPAL_RESET_REASON_BROWNOUT
      || eResetReason == ZPAL_RESET_REASON_TAMPER) {
    zpal_retention_register_clear();
  }

  //Important: initialize the voltage monitoring before ApplicationInit()
  sl_dcdc_vmon_init(ZW_DCDC_CONFIG);

  //If woke up from EM4 set sleep duration
  if (0 != (ResetCause & EMU_RSTCAUSE_EM4)) {
    zpal_retention_register_read_private(ZPAL_RETENTION_REGISTER_PRIVATE_DEEP_SLEEP_TICK, &startup_context.go_to_sleep_ticks);
    uint32_t diff;

    if (startup_context.wake_up_ticks > startup_context.go_to_sleep_ticks) {
      diff = startup_context.wake_up_ticks - startup_context.go_to_sleep_ticks;
    } else {
      diff = (UINT32_MAX - startup_context.go_to_sleep_ticks) + startup_context.wake_up_ticks;
    }
    uint32_t burtc_frequency_hz = zw_startup_get_burtc_counter_frequency_hz();
    if (burtc_frequency_hz != 0) {
      startup_context.completed_sleep_duration_ms = (uint32_t)(((uint64_t)diff * 1000) / burtc_frequency_hz);
    }
  }

  /***********************************************************************************
   * HW bring-up completed!
   **********************************************************************************/

  zpal_system_startup(eResetReason);

  ZW_dtpa_set_noise_margin(ZWAVE_DTPA_NOISE_MARGIN_DBM * 10); // Convert from dBm to deci-dBm

  return 0;
}

/**
 * @brief Run periodic maintenance when the FreeRTOS scheduler enters idle.
 *
 * FreeRTOS invokes this hook from the idle task when no application task is
 * ready to run. It feeds the watchdog and repacks the NVM storage when garbage
 * collection is required.
 *
 * @note This hook must not call a function that can block.
 */
void vApplicationIdleHook(void)
{
  //Clear the watchdog timer here, reload with the new desired value
  zpal_feed_watchdog();

  //Idle is called, so application as nothing to do. So it's time check for a nvm repack.
  if (nvm3_repackNeeded(nvm3_defaultHandle)) {
    nvm3_repack(nvm3_defaultHandle);
  }
}

/**
 * @brief Handle a FreeRTOS task stack overflow.
 *
 * @param[in] pxTask Task handle associated with the overflowing task.
 * @param[in] pcTaskName Name of the overflowing task.
 */
void vApplicationStackOverflowHook(__attribute__((unused)) TaskHandle_t pxTask, __attribute__((unused)) char *pcTaskName) /* NOSONAR */
{
  /* This function will get called if a task overflows its stack.   If the
     parameters are corrupt then inspect pxCurrentTCB to find which was the
     offending task. */

  assert(false); // Stack overflow for pcTaskName
}

/**
 * @brief Provide statically allocated memory for the FreeRTOS idle task.
 *
 * @param[out] ppxIdleTaskTCBBuffer Address of the idle task TCB buffer.
 * @param[out] ppxIdleTaskStackBuffer Address of the idle task stack buffer.
 * @param[out] pulIdleTaskStackSize Size of the idle task stack in words.
 */
void vApplicationGetIdleTaskMemory(StaticTask_t **ppxIdleTaskTCBBuffer,
                                   StackType_t **ppxIdleTaskStackBuffer,
                                   uint32_t *pulIdleTaskStackSize)
{
  static StaticTask_t idle_task_tcb = { 0 };
  static StackType_t idle_task_stack[configMINIMAL_STACK_SIZE];

  /* Pass out a pointer to the StaticTask_t structure in which the Idle task's
     state will be stored. */
  *ppxIdleTaskTCBBuffer = &idle_task_tcb;

  /* Pass out the array that will be used as the Idle task's stack. */
  *ppxIdleTaskStackBuffer = idle_task_stack;

  /* Pass out the size of the array pointed to by *ppxIdleTaskStackBuffer.
     Note that, as the array is necessarily of type StackType_t,
     configMINIMAL_STACK_SIZE is specified in words, not bytes. */
  *pulIdleTaskStackSize = sizeof_array(idle_task_stack);
}

/**
 * @brief Provide statically allocated memory for the FreeRTOS timer task.
 *
 * @param[out] ppxTimerTaskTCBBuffer Address of the timer task TCB buffer.
 * @param[out] ppxTimerTaskStackBuffer Address of the timer task stack buffer.
 * @param[out] pulTimerTaskStackSize Size of the timer task stack in words.
 */
void vApplicationGetTimerTaskMemory(StaticTask_t **ppxTimerTaskTCBBuffer,
                                    StackType_t **ppxTimerTaskStackBuffer,
                                    uint32_t *pulTimerTaskStackSize)
{
  static StaticTask_t timer_task_tcb = { 0 };
  static StackType_t timer_task_stack[configTIMER_TASK_STACK_DEPTH];

  /* Pass out a pointer to the StaticTask_t structure in which the Timer
     task's state will be stored. */
  *ppxTimerTaskTCBBuffer = &timer_task_tcb;

  /* Pass out the array that will be used as the Timer task's stack. */
  *ppxTimerTaskStackBuffer = timer_task_stack;

  /* Pass out the size of the array pointed to by *ppxTimerTaskStackBuffer.
     Note that, as the array is necessarily of type StackType_t,
     configTIMER_TASK_STACK_DEPTH is specified in words, not bytes. */
  *pulTimerTaskStackSize = sizeof_array(timer_task_stack);
}

/******************************************************************************
 * @brief User-defined function for pre-clock app initialization.
 *
 * @details This function is called from sli_main_init():
 * - Before essential infrastructure components are initialized
 * (memory management, clock, oscillators, interrupt management, etc.).
 *****************************************************************************/
void app_init_pre_clock(void)
{
  sl_zwave_platform_startup();
}
