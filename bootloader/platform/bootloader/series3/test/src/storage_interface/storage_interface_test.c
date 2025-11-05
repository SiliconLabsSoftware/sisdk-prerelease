/***************************************************************************//**
 * @file storage_interface_test.c
 * @brief Test bootloader storage interface with real EBL
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

#include "api/btl_interface.h"
#include "sl_main_init.h"

// Unity test framework
#include "unity.h"

#include <string.h>

extern const ApplicationProperties_t sl_app_properties;

#if defined(_SILICON_LABS_32B_SERIES_3_CONFIG_301)
#if defined(__GNUC__)
const uint8_t bootloaderBlock[32 * 1024] __attribute__((section(".bootloader")));
#elif defined(__ICCARM__)
__no_init __root const uint8_t bootloaderBlock[32 * 1024] @ 0x01000000;
#else
#error "Bootloader space not reserved on this compiler"
#endif
#endif

static void testGetSlotMetadata(void)
{
  TEST_ASSERT_EQUAL(BOOTLOADER_OK, bootloader_init());

  ApplicationData_t appInfo;
  uint32_t bootloaderVersion;
  unsigned char pid[16] = { 0 };
  TEST_ASSERT_EQUAL(BOOTLOADER_OK,
                    bootloader_getImageInfo(0, &appInfo, &bootloaderVersion));
  TEST_ASSERT_EQUAL(0x00000000, bootloaderVersion);

  TEST_ASSERT_EQUAL(0x00000010, appInfo.type);
  TEST_ASSERT_EQUAL(0x00000003, appInfo.version);
  TEST_ASSERT_EQUAL(0x01020304, appInfo.capabilities);
  TEST_ASSERT_EQUAL_UINT8_ARRAY(pid, appInfo.productId, 16);

  TEST_ASSERT_EQUAL(BOOTLOADER_OK, bootloader_deinit());
}

int main(void)
{
  // CHIP_Init();
  sl_main_init();

  UnityBegin("STORAGE_INTERFACE");
  TEST_PRINTF("App version %d\n", sl_app_properties.app.version);

  RUN_TEST(testGetSlotMetadata, __LINE__);

  UnityEnd();

  UnityPrint("ENDSWO");
  while (1) ;
}
