/***************************************************************************//**
 * @file storage_interface_test.c
 * @brief Test bootloader storage interface with real EBL
 * @author Silicon Labs
 * @version 1.7.0
 *******************************************************************************
 * @section License
 * <b>Copyright 2016 Silicon Laboratories, Inc. http://www.silabs.com</b>
 *******************************************************************************
 *
 * This file is licensed under the Silabs License Agreement. See the file
 * "Silabs_License_Agreement.txt" for details. Before using this software for
 * any purpose, you must agree to the terms of that agreement.
 *
 ******************************************************************************/

#include "api/btl_interface.h"

#include "em_chip.h"

// Unity test framework
#include "unity.h"
#if defined(UNITY_OUTPUT_SERIAL)
#include "unity_usart.h"
#else
#include "unity_swo.h"
#endif

#include <string.h>

extern const ApplicationProperties_t sl_app_properties;

#if defined(_SILICON_LABS_GECKO_INTERNAL_SDID_80) || defined(_SILICON_LABS_GECKO_INTERNAL_SDID_200)
#if defined(__GNUC__)
const uint8_t bootloaderBlock[16 * 1024] __attribute__((section(".bootloader")));
#elif defined(__ICCARM__)
__no_init __root const uint8_t bootloaderBlock[16 * 1024] @ 0x00000000;
#else
#error "Bootloader space not reserved on this compiler"
#endif
#elif defined(_SILICON_LABS_GECKO_INTERNAL_SDID_205)
#if defined(__GNUC__)
const uint8_t bootloaderBlock[24 * 1024] __attribute__((section(".bootloader")));
#elif defined(__ICCARM__)
__no_init __root const uint8_t bootloaderBlock[24 * 1024] @ 0x00000000;
#else
#error "Bootloader space not reserved on this compiler"
#endif
#elif defined(_SILICON_LABS_GECKO_INTERNAL_SDID_210) || defined(_SILICON_LABS_GECKO_INTERNAL_SDID_215) \
  || defined(_SILICON_LABS_GECKO_INTERNAL_SDID_220) || defined(_SILICON_LABS_GECKO_INTERNAL_SDID_225)  \
  || defined(_SILICON_LABS_GECKO_INTERNAL_SDID_230) || defined(_SILICON_LABS_GECKO_INTERNAL_SDID_235)  \
  || defined(_SILICON_LABS_GECKO_INTERNAL_SDID_240) || defined(_SILICON_LABS_GECKO_INTERNAL_SDID_260)
#if defined(__GNUC__)
const uint8_t bootloaderBlock[24 * 1024] __attribute__((section(".bootloader")));
#elif defined(__ICCARM__)
__no_init __root const uint8_t bootloaderBlock[24 * 1024] @ 0x08000000;
#else
#error "Bootloader space not reserved on this compiler"
#endif
#elif defined(_SILICON_LABS_GECKO_INTERNAL_SDID_89) && defined(MAIN_BOOTLOADER_IN_MAIN_FLASH) \
  || defined(_SILICON_LABS_GECKO_INTERNAL_SDID_95) && defined(MAIN_BOOTLOADER_IN_MAIN_FLASH)
#if defined(__GNUC__)
const uint8_t bootloaderBlock[18 * 1024] __attribute__((section(".bootloader")));
#elif defined(__ICCARM__)
__no_init __root const uint8_t bootloaderBlock[18 * 1024] @ 0x00000000;
#else
#error "Bootloader space not reserved on this compiler"
#endif
#endif

void HardFault_Handler(void)
{
  UnityPrintf("FAULT\n          ");
  while (1) {
    // Do nothing
  }
}

static void testGetSlotMetadata(void)
{
  TEST_ASSERT_EQUAL(BOOTLOADER_OK, bootloader_init());

  ApplicationData_t appInfo;
  uint32_t bootloaderVersion;
  TEST_ASSERT_EQUAL(BOOTLOADER_OK,
                    bootloader_getImageInfo(0, &appInfo, &bootloaderVersion));

  TEST_ASSERT_EQUAL(0x00010000, bootloaderVersion);

  TEST_ASSERT_EQUAL(0x10101010, appInfo.type);
  TEST_ASSERT_EQUAL(0x20202020, appInfo.version);
  TEST_ASSERT_EQUAL(0x00001000, appInfo.capabilities);
  TEST_ASSERT_EQUAL_UINT8_ARRAY("0000000000000000", appInfo.productId, 16);

  TEST_ASSERT_EQUAL(BOOTLOADER_OK, bootloader_deinit());
}

int main(void)
{
  CHIP_Init();

#if defined(UNITY_OUTPUT_SERIAL)
  UNITY_UsartSetup();
#else
  SWO_Setup();
#endif

  UnityBeginGroup("STORAGE_INTERFACE");
  UnityPrintf("App version %d\n", sl_app_properties.app.version);

  RUN_TEST(testGetSlotMetadata, __LINE__);

  UnityEnd();

  UnityPrint("ENDSWO");
  while (1) ;
}
