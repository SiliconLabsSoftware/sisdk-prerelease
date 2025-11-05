/***************************************************************************//**
 * @file interface_test_tz_handler.c
 *******************************************************************************
 * @section License
 * <b>Copyright 2021 Silicon Laboratories, Inc. http://www.silabs.com</b>
 *******************************************************************************
 *
 * This file is licensed under the Silabs License Agreement. See the file
 * "Silabs_License_Agreement.txt" for details. Before using this software for
 * any purpose, you must agree to the terms of that agreement.
 *
 ******************************************************************************/

#include "api/btl_interface.h"

// Unity test framework
#include "unity.h"
#if defined(UNITY_OUTPUT_SERIAL)
#include "unity_usart.h"
#else
#include "unity_swo.h"
#endif

#include <string.h>
#include "sl_main_init.h"
#include "fault.h"

extern const ApplicationProperties_t sl_app_properties;

static uint16_t getResetReason(void)
{
  BootloaderResetCause_t* resetCause = (BootloaderResetCause_t*) (SRAM_BASE);
  return resetCause->reason;
}

static uint16_t getFaultedRegister(void)
{
  BootloaderResetCause_t* resetCause = (BootloaderResetCause_t*) (SRAM_BASE + 4u);
  return resetCause->reason;
}

static void setFaultedRegister(uint16_t val)
{
  BootloaderResetCause_t* resetCause = (BootloaderResetCause_t*) (SRAM_BASE + 4u);
  resetCause->reason = val;
}

int main(void)
{
  sl_main_init();
  faultInit();

#if defined(UNITY_OUTPUT_SERIAL)
  UNITY_UsartSetup();
#else
  SWO_Setup();
#endif

  // Approach
  // 1. Trigger a fault by accessing a peripheral at S address - We know at this point that the
  //    peripherals are moved to the NS address by the startup routine.
  // 2. Verify that the device comes back from a reset properly by checking the reset reason
  // 3. Check if the PPUSATD bits are correctly set for that peripheral

  // List of the peripherals used to test:
  // - TIMER0
  // - TIMER1
  // - I2C1
  // - WDOG
  // - IADC0

  if (getResetReason() != BOOTLOADER_RESET_REASON_FAULT) {
    UnityBeginGroup("INTERFACE");
    UnityPrintf("App version %08x\n", sl_app_properties.app.version);
  } else {
    UnityPrintf("Got back from a reset\n");
  }

  #define TZ_TEST_TIMER0_FAULT  0xF000
  #define TZ_TEST_TIMER1_FAULT  0xF001
  #define TZ_TEST_I2C1_FAULT    0xF002
  #define TZ_TEST_WDOG_FAULT    0xF003
  #define TZ_TEST_IADC0_FAULT   0xF004

#if defined(CMU_CLKEN0_TIMER0)
  CMU->CLKEN0_SET = CMU_CLKEN0_TIMER0;
#endif
#if defined(CMU_CLKEN0_TIMER1)
  CMU->CLKEN0_SET = CMU_CLKEN0_TIMER1;
#endif
#if defined(CMU_CLKEN0_I2C1)
  CMU->CLKEN0_SET = CMU_CLKEN0_I2C1;
#endif
#if defined(CMU_CLKEN0_WDOG0)
  CMU->CLKEN0_SET = CMU_CLKEN0_WDOG0;
#endif
#if defined(CMU_CLKEN0_IADC0)
  CMU->CLKEN0_SET = CMU_CLKEN0_IADC0;
#endif

  Bootloader_PPUSATDnCLKENnState_t blPPUSATDnCLKENnState = { 0 };
  bootloader_ppusatdnSaveReconfigureState(&blPPUSATDnCLKENnState);

  // Store the SMU state before exiting atomic section by calling the restore PPUSATD function
  uint32_t smu_ppusatd0_state = SMU->PPUSATD0;
  uint32_t smu_ppusatd1_state = SMU->PPUSATD1;

  bootloader_ppusatdnRestoreState(&blPPUSATDnCLKENnState);

  if (getResetReason() != BOOTLOADER_RESET_REASON_FAULT) {
    UnityPrintf("Start triggering secure access faults\n");
    #if defined(SMU_PPUSATD1_I2C1)
    TEST_ASSERT_EQUAL(0, smu_ppusatd0_state
                      & (SMU_PPUSATD0_TIMER0 | SMU_PPUSATD0_TIMER1));
    TEST_ASSERT_EQUAL(0, smu_ppusatd1_state
                      & (SMU_PPUSATD1_WDOG0 | SMU_PPUSATD1_IADC0 | SMU_PPUSATD1_I2C1));
    #else
    TEST_ASSERT_EQUAL(0, smu_ppusatd0_state
                      & (SMU_PPUSATD0_TIMER0 | SMU_PPUSATD0_TIMER1 | SMU_PPUSATD0_I2C1));
    TEST_ASSERT_EQUAL(0, smu_ppusatd1_state
                      & (SMU_PPUSATD1_WDOG0 | SMU_PPUSATD1_IADC0));
    #endif
    setFaultedRegister(TZ_TEST_TIMER0_FAULT);
    TIMER0_S->EN_SET = TIMER_EN_EN;
  }

  if (getFaultedRegister() == TZ_TEST_TIMER0_FAULT) {
    UnityPrintf("Verifying TIMER0 PPUSATD state\n");
    TEST_ASSERT_EQUAL(SMU_PPUSATD0_TIMER0, smu_ppusatd0_state & SMU_PPUSATD0_TIMER0);
    setFaultedRegister(TZ_TEST_TIMER1_FAULT);
    TIMER1_S->EN_SET = TIMER_EN_EN;
  }

  if (getFaultedRegister() == TZ_TEST_TIMER1_FAULT) {
    UnityPrintf("Verifying TIMER1 PPUSATD state\n");
    TEST_ASSERT_EQUAL(SMU_PPUSATD0_TIMER0, smu_ppusatd0_state & SMU_PPUSATD0_TIMER0);
    TEST_ASSERT_EQUAL(SMU_PPUSATD0_TIMER1, smu_ppusatd0_state & SMU_PPUSATD0_TIMER1);
    setFaultedRegister(TZ_TEST_I2C1_FAULT);
    I2C1_S->EN_SET = I2C_EN_EN;
  }

  if (getFaultedRegister() == TZ_TEST_I2C1_FAULT) {
    UnityPrintf("Verifying I2C1 PPUSATD state\n");
    TEST_ASSERT_EQUAL(SMU_PPUSATD0_TIMER0, smu_ppusatd0_state & SMU_PPUSATD0_TIMER0);
    TEST_ASSERT_EQUAL(SMU_PPUSATD0_TIMER1, smu_ppusatd0_state & SMU_PPUSATD0_TIMER1);
    #if defined(SMU_PPUSATD1_I2C1)
    TEST_ASSERT_EQUAL(SMU_PPUSATD1_I2C1, smu_ppusatd1_state & SMU_PPUSATD1_I2C1);
    #else
    TEST_ASSERT_EQUAL(SMU_PPUSATD0_I2C1, smu_ppusatd0_state & SMU_PPUSATD0_I2C1);
    #endif
    setFaultedRegister(TZ_TEST_WDOG_FAULT);
    WDOG0_S->EN = WDOG_EN_EN;
  }

  if (getFaultedRegister() == TZ_TEST_WDOG_FAULT) {
    UnityPrintf("Verifying WDOG PPUSATD state\n");
    TEST_ASSERT_EQUAL(SMU_PPUSATD0_TIMER0, smu_ppusatd0_state & SMU_PPUSATD0_TIMER0);
    TEST_ASSERT_EQUAL(SMU_PPUSATD0_TIMER1, smu_ppusatd0_state & SMU_PPUSATD0_TIMER1);
    #if defined(SMU_PPUSATD1_I2C1)
    TEST_ASSERT_EQUAL(SMU_PPUSATD1_I2C1, smu_ppusatd1_state & SMU_PPUSATD1_I2C1);
    #else
    TEST_ASSERT_EQUAL(SMU_PPUSATD0_I2C1, smu_ppusatd0_state & SMU_PPUSATD0_I2C1);
    #endif
    TEST_ASSERT_EQUAL(SMU_PPUSATD1_WDOG0, smu_ppusatd1_state & SMU_PPUSATD1_WDOG0);
    setFaultedRegister(TZ_TEST_IADC0_FAULT);
    IADC0_S->EN_SET = IADC_EN_EN;
  }

  if (getFaultedRegister() == TZ_TEST_IADC0_FAULT) {
    UnityPrintf("Verifying IADC0 PPUSATD state\n");
    TEST_ASSERT_EQUAL(SMU_PPUSATD0_TIMER0, smu_ppusatd0_state & SMU_PPUSATD0_TIMER0);
    TEST_ASSERT_EQUAL(SMU_PPUSATD0_TIMER1, smu_ppusatd0_state & SMU_PPUSATD0_TIMER1);
    #if defined(SMU_PPUSATD1_I2C1)
    TEST_ASSERT_EQUAL(SMU_PPUSATD1_I2C1, smu_ppusatd1_state & SMU_PPUSATD1_I2C1);
    #else
    TEST_ASSERT_EQUAL(SMU_PPUSATD0_I2C1, smu_ppusatd0_state & SMU_PPUSATD0_I2C1);
    #endif
    TEST_ASSERT_EQUAL(SMU_PPUSATD1_WDOG0, smu_ppusatd1_state & SMU_PPUSATD1_WDOG0);
    TEST_ASSERT_EQUAL(SMU_PPUSATD1_IADC0, smu_ppusatd1_state & SMU_PPUSATD1_IADC0);
  }

  if ((getFaultedRegister() != TZ_TEST_TIMER0_FAULT)
      && (getFaultedRegister() != TZ_TEST_TIMER1_FAULT)
      && (getFaultedRegister() != TZ_TEST_I2C1_FAULT)
      && (getFaultedRegister() != TZ_TEST_WDOG_FAULT)
      && (getFaultedRegister() != TZ_TEST_IADC0_FAULT)) {
    // Did not finish the test properly
    TEST_ASSERT(false);
  }

  UnityEnd();

  UnityPrint("ENDSWO");
  while (1) ;
}
