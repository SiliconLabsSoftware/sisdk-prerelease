/***************************************************************************//**
 * @file fault_catching_test_s.c
 * @brief The secure side of fault catching test.
 *******************************************************************************
 * # License
 * <b>Copyright 2023 Silicon Laboratories Inc. www.silabs.com</b>
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

#include <setjmp.h>
#include "unity.h"
#include "fault_catching_test.h"
#include "tz_secure_memory_autogen.h"
#include "sl_assert.h"
#include "sl_main_init.h"

typedef void (*nsfunc)(void) __attribute__((cmse_nonsecure_call));

fault_catching_test_id_t test_func1();
fault_catching_test_id_t test_func2();
fault_catching_test_id_t test_func3();
fault_catching_test_id_t(*run_test[3]) (void);

// Variable to store secure program state before calling NS
jmp_buf securePreFaultSetjmpBuff;
// Counter used to keep track of actual test
static uint32_t counter = (sizeof(run_test) / sizeof(run_test[0]));

// ID of actual test
static fault_catching_test_id_t actual_test;

int32_t sli_tz_s_interface_dispatch_fault_catching(uint32_t sid,
                                                   uint32_t arg)
{
  EFM_ASSERT(sizeof(fault_catching_function_table) / sizeof
             (fault_catching_function_table[0])
             == SLI_TZ_FAULT_CATCHING_MAX_SID);
  EFM_ASSERT(sid < SLI_TZ_FAULT_CATCHING_MAX_SID);
  return fault_catching_function_table[sid](arg);
}

void start_ns_app(void)
{
  sl_main_init();
  setjmp(securePreFaultSetjmpBuff);

  // Set stack pointer.
  __TZ_set_MSP_NS(*((uint32_t *)(TZ_S_FLASH_END)));
  __TZ_set_PSP_NS(*((uint32_t *)(TZ_S_FLASH_END)));

  // Set vector table (may be done by SystemInit() on NS side, but do it here to
  // be sure).
  SCB_NS->VTOR = TZ_S_FLASH_END;

  // Call non-secure reset handler.
  nsfunc fp = (nsfunc) * ((uint32_t *)((TZ_S_FLASH_END) +4UL));
  fp();
}

uint32_t sli_tz_fault_catching_init(uint32_t arg)
{
  (void) arg;
  UNITY_Init();
  // Set up test sequence.
  run_test[2] = test_func1;
  run_test[1] = test_func2;
  run_test[0] = test_func3;
  counter--;
  return run_test[counter]();
}

fault_catching_test_id_t test_func1()
{
  sl_unity_start_test("Fault catching test func 1");
  //A code that is meant to be run before the test 1

  //run usage fault catching test.
  actual_test = FAULT_CATCHING_TEST_USAGE;
  return actual_test;
}

fault_catching_test_id_t test_func2()
{
  sl_unity_start_test("Fault catching test func 2");
  //A code that is meant to be run before the test 2

  //run secure fault catching test.
  actual_test = FAULT_CATCHING_TEST_SECURE;
  return actual_test;
}

fault_catching_test_id_t test_func3()
{
  sl_unity_start_test("Fault catching test func 3");
  //A code that is meant to be run before the test 3

  //run usage fault catching test again, but after different code.
  actual_test = FAULT_CATCHING_TEST_USAGE;
  return actual_test;
}

uint32_t sli_tz_fault_handler(uint32_t error_code)
{
  // Clear fault flags
  SCB->SHCSR &= ~(SCB_SHCSR_SECUREFAULTACT_Msk)
                & ~(SCB_SHCSR_BUSFAULTACT_Msk)
                & ~(SCB_SHCSR_USGFAULTACT_Msk)
                & ~(SCB_SHCSR_MEMFAULTACT_Msk);

  switch (error_code) {
    case FAULT_CATCHING_NS_USAGEFAULT:
      TEST_ASSERT_EQUAL(actual_test, FAULT_CATCHING_TEST_USAGE);
      break;
    case FAULT_CATCHING_S_SECUREFAULT:
      TEST_ASSERT_EQUAL(actual_test, FAULT_CATCHING_TEST_SECURE);
      break;
    default:
      UnityPrintf("Not implemented.");
      sl_unity_stop_test();
      return FAULT_CATCHING_TEST_EXIT;
      break;
  }
  if (counter == 0) {
    UnityPrintf("\n\n");
    UnityConcludeTest();
    sl_unity_stop_test();
    return FAULT_CATCHING_TEST_EXIT;
  }
  UnityPrintf("\n\n");
  UnityConcludeTest();
  UnityEnd();
  longjmp(securePreFaultSetjmpBuff, 1);
}

void SMU_SECURE_IRQHandler(void)
{
  sli_tz_fault_handler(FAULT_CATCHING_S_SMU_SECURE_IRQ);
}

void HardFault_Handler(void)
{
  sli_tz_fault_handler(FAULT_CATCHING_S_HARDFAULT);
}

void SecureFault_Handler(void)
{
  sli_tz_fault_handler(FAULT_CATCHING_S_SECUREFAULT);
}

void UsageFault_Handler(void)
{
  sli_tz_fault_handler(FAULT_CATCHING_S_USAGEFAULT);
}

void BusFault_Handler(void)
{
  sli_tz_fault_handler(FAULT_CATCHING_S_BUSFAULT);
}

void mpu_fault_handler(void)
{
  sli_tz_fault_handler(FAULT_CATCHING_S_MPU_FAULT);
}
