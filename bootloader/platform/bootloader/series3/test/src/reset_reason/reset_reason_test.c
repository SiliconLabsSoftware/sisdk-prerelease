/***************************************************************************//**
 * @file reset_reason_test.c
 * @brief Test btl_reset functions
 * @author Silicon Labs
 * @version 1.7.0
 *******************************************************************************
 * @section License
 * <b>Copyright 2024 Silicon Laboratories, Inc. http://www.silabs.com</b>
 *******************************************************************************
 *
 * This file is licensed under the Silabs License Agreement. See the file
 * "Silabs_License_Agreement.txt" for details. Before using this software for
 * any purpose, you must agree to the terms of that agreement.
 *
 ******************************************************************************/

#include "core/btl_reset.h"
#include "api/btl_reset_info.h"
#include "sl_main_init.h"

// Unity test framework
#include "unity.h"

#include <string.h>

void testResetReason(void)
{
  reset_setResetReason(BOOTLOADER_RESET_REASON_BADIMAGE);
  TEST_ASSERT_EQUAL(BOOTLOADER_RESET_REASON_BADIMAGE, reset_getResetReason());
  TEST_ASSERT_EQUAL(BOOTLOADER_RESET_REASON_BADIMAGE, reset_classifyReset());
  reset_invalidateResetReason();
  TEST_ASSERT_EQUAL(BOOTLOADER_RESET_REASON_UNKNOWN, reset_classifyReset());
}

void testResetCounter(void)
{
  // Test that the reset counter work sand doesn't interfere with the reset
  // reason. The 'stop after n resets' functionality using the counter is tested
  // in a separate host test.

  // Enable preserves reason
  reset_setResetReason(BOOTLOADER_RESET_REASON_BADIMAGE);
  reset_enableResetCounter();
  TEST_ASSERT_EQUAL(BOOTLOADER_RESET_REASON_BADIMAGE, reset_getResetReason());
  TEST_ASSERT_EQUAL(BOOTLOADER_RESET_REASON_UNKNOWN, reset_classifyReset());

  // Enabled returns correct value
  TEST_ASSERT_TRUE(reset_resetCounterEnabled());

  // Default counter value is 0
  TEST_ASSERT_EQUAL(0, reset_getResetCounter());

  // Increment works
  reset_incrementResetCounter();
  TEST_ASSERT_EQUAL(1, reset_getResetCounter());
  reset_incrementResetCounter();
  reset_incrementResetCounter();
  TEST_ASSERT_EQUAL(3, reset_getResetCounter());

  // Counter wraps around after counting to 15
  for (uint32_t i = 15 - reset_getResetCounter(); i > 0; i--) {
    reset_incrementResetCounter();
  }

  TEST_ASSERT_EQUAL(15, reset_getResetCounter());
  reset_incrementResetCounter();
  TEST_ASSERT_EQUAL(0, reset_getResetCounter());

  // Disabling counter preserves reason
  reset_disableResetCounter();
  TEST_ASSERT_EQUAL(BOOTLOADER_RESET_REASON_BADIMAGE, reset_getResetReason());

  // Enabled returns correct value
  TEST_ASSERT_FALSE(reset_resetCounterEnabled());
}

int main(void)
{
  // CHIP_Init();
  sl_main_init();

  UnityBegin("RESET_REASON");

  RUN_TEST(testResetReason, __LINE__);
  RUN_TEST(testResetCounter, __LINE__);

  UnityEnd();

  UnityPrint("ENDSWO");
  while (1) ;
}
