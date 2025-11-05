/***************************************************************************//**
 * @file interface_test_tz_ram_clean_up.c
 *******************************************************************************
 * @section License
 * <b>Copyright 2022 Silicon Laboratories, Inc. http://www.silabs.com</b>
 *******************************************************************************
 *
 * This file is licensed under the Silabs License Agreement. See the file
 * "Silabs_License_Agreement.txt" for details. Before using this software for
 * any purpose, you must agree to the terms of that agreement.
 *
 ******************************************************************************/

#define TEST_RAM_SIZE   (0x10u)
#define TEST_RAM_BASE (SRAM_BASE + SRAM_SIZE - TEST_RAM_SIZE)
#define TEST_RAM_KNOWN_PATTERN (0xca)
#define TEST_RAM_KNOWN_WORD    (0xadcacada)
#define TEST_MAGIC_PATTERN     (0xEBFE0000)

// Stack Memory boundaries
#define TEST_STACK_MEM_START  (0x20000008)

// rw section memory boundary
#define TEST_RW_MEM_START   (0x20001010)

#if defined(TEST_BOOTLOADER_RAM_CLEAN_UP)

#include <string.h>
#include "core/btl_reset.h"

void ram_clean_up_test(void)
{
  // Set reset reason to something != 0
  reset_setResetReason(0x0200u);

  // Fill the last 16 bytes (TEST_RAM_SIZE) with some known data
  memset((void *)TEST_RAM_BASE, TEST_RAM_KNOWN_PATTERN, TEST_RAM_SIZE);
}

#else

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

void testRAMSection(uint8_t* start_addr)
{
  // Iterate over the memory section until we hit the magic test pattern
  uint8_t* rw_ptr = start_addr;
  while (1) {
    // Test for 0u until test magic pattern is reached
    if (*rw_ptr != 0) {
      // Read next four bytes and check if it's equal to the magic pattern
      uint32_t ram_word = (*rw_ptr << 24) | (*(rw_ptr + 1u) << 16) | (*(rw_ptr + 2u) << 8) | *(rw_ptr + 3u);
      TEST_ASSERT_EQUAL_HEX32(TEST_MAGIC_PATTERN, ram_word);
      break;
    } else {
      TEST_ASSERT_EQUAL_HEX32(0u, *rw_ptr);
      rw_ptr++;
    }
    // Sanity check to ensure we are within RAM boundaries
    TEST_ASSERT_NOT_EQUAL((uint32_t)rw_ptr, SRAM_BASE + SRAM_SIZE);
  }
}

void testRAMContent(void)
{
  // The reset reason should be preserved.
  BootloaderResetCause_t reset_reason = bootloader_getResetReason();
  // Only verify that the reset reason is not equal 0 since test glitches might set the reset reason to
  // something not expected, like BOOTLOADER_RESET_REASON_BADAPP, if flashing of the test application fail.
  TEST_ASSERT_NOT_EQUAL(0u, reset_reason.reason);

  // Verify that the rw region is erased
  testRAMSection((uint8_t*)TEST_RW_MEM_START);

  // Verify that the stack region is erased
  testRAMSection((uint8_t*)TEST_STACK_MEM_START);

  // Check if TEST_RAM data is persistent in SRAM
  uint8_t *ram_ptr = (uint8_t *)TEST_RAM_BASE;
  for (uint32_t i = 0u; i < TEST_RAM_SIZE; i++) {
    TEST_ASSERT_EQUAL_HEX32(TEST_RAM_KNOWN_PATTERN, *(ram_ptr + i));
  }
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

  UnityBeginGroup("INTERFACE");
  UnityPrintf("App version %08x\n", sl_app_properties.app.version);

  RUN_TEST(testRAMContent, __LINE__);

  UnityEnd();
  UnityPrint("ENDSWO");
  while (1) ;
}

#endif
