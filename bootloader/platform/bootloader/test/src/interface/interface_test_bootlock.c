/***************************************************************************//**
 * @file interface_test_bootlock.c
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

#include "em_msc.h"
#include "em_chip.h"

//Globals
uint32_t numOfBLPages =
#if defined(_SILICON_LABS_GECKO_INTERNAL_SDID_84)
// Dedicated bootloader area of 38k
  19;
#elif defined(_SILICON_LABS_GECKO_INTERNAL_SDID_89)
// Dedicated bootloader area of 16k
  8;
#elif defined(_SILICON_LABS_GECKO_INTERNAL_SDID_95)
// Dedicated bootloader area of 18k
  9;
#elif defined(_SILICON_LABS_GECKO_INTERNAL_SDID_200)
// No bootloader area: Place the bootloader in main flash
  2;
#elif defined(_SILICON_LABS_GECKO_INTERNAL_SDID_205)
// No bootloader area: Place the bootloader in main flash
  3;
#elif defined(_SILICON_LABS_GECKO_INTERNAL_SDID_210)
// No bootloader area: Place the bootloader in main flash
  3;
#elif defined(_SILICON_LABS_GECKO_INTERNAL_SDID_215)
// No bootloader area: Place the bootloader in main flash
  3;
#elif defined(_SILICON_LABS_GECKO_INTERNAL_SDID_220)
// No bootloader area: Place the bootloader in main flash
  3;
#elif defined(_SILICON_LABS_GECKO_INTERNAL_SDID_225)
  3;
#elif defined(_SILICON_LABS_GECKO_INTERNAL_SDID_230)
  3;
#elif defined(_SILICON_LABS_GECKO_INTERNAL_SDID_235)
  3;
#elif defined(_SILICON_LABS_GECKO_INTERNAL_SDID_240)
  3;
#elif defined(_SILICON_LABS_GECKO_INTERNAL_SDID_260)
  3;
#else
// Test not supported for this family
  0;
#endif

static void testBooloaderLockPages(void)
{
  //Test if bootloader area was locked before booting to application

  uint32_t numOfPagesLocked = (BTL_MAIN_STAGE_MAX_SIZE + BTL_FIRST_STAGE_SIZE) / FLASH_PAGE_SIZE;
  UnityPrintf("Number of pages locked = %d\n", numOfPagesLocked);
  TEST_ASSERT_EQUAL(numOfBLPages, numOfPagesLocked);

  //Write FFs to random locations to check if bootloader pages are locked
  while (numOfPagesLocked) {
    uint32_t flashPageBase = (uint32_t)(BTL_FIRST_STAGE_BASE + ((numOfPagesLocked - 1) * FLASH_PAGE_SIZE));

    //Set the address in the flash page
    uint8_t data[4] = { 0 };
    uint32_t buffAddress = flashPageBase + (FLASH_PAGE_SIZE / 2);

    //Read random locations from a page
    memcpy(data, (void*)buffAddress, 4);

    UnityPrintf("baseAddress = %d\n", buffAddress);
    UnityPrintf("data[0] = %d\n", data[0]);
    UnityPrintf("data[1] = %d\n", data[1]);
    UnityPrintf("data[2] = %d\n", data[2]);
    UnityPrintf("data[3] = %d\n", data[3]);

    //Write FFs to the random locations
    uint8_t writeBuff[4] = { 0xFF };

    #if defined(_CMU_CLKEN1_MASK)
    CMU->CLKEN1_SET = CMU_CLKEN1_MSC;
    #endif
    MSC_Status_TypeDef retval = MSC_ErasePage((uint32_t *)flashPageBase);

    #if defined(_SILICON_LABS_32B_SERIES_1)
    TEST_ASSERT_EQUAL(mscReturnInvalidAddr, retval);
    #else
    TEST_ASSERT_EQUAL(mscReturnLocked, retval);
    #endif

    retval = MSC_WriteWord((uint32_t *)buffAddress, writeBuff, 4);
    #if defined(_SILICON_LABS_32B_SERIES_1)
    TEST_ASSERT_EQUAL(mscReturnInvalidAddr, retval);
    #else
    TEST_ASSERT_EQUAL(mscReturnLocked, retval);
    #endif

    //Read back from the random locations
    uint8_t readData[4] = { 0 };
    memcpy(readData, (void*)buffAddress, 4);

    UnityPrintf("readData[0] = %d\n", readData[0]);
    UnityPrintf("readData[1] = %d\n", readData[1]);
    UnityPrintf("readData[2] = %d\n", readData[2]);
    UnityPrintf("readData[3] = %d\n", readData[3]);

    //Compare if data and readData is same
    TEST_ASSERT_EQUAL_UINT8_ARRAY(data, readData, 4);

    UnityPrintf("Page %d verified\n", numOfPagesLocked);

    numOfPagesLocked--;
  }
}

int main(void)
{
  CHIP_Init();
  bootloader_init();

#if defined(UNITY_OUTPUT_SERIAL)
  UNITY_UsartSetup();
#else
  SWO_Setup();
#endif

  UnityBeginGroup("INTERFACE");

  RUN_TEST(testBooloaderLockPages, __LINE__);

  UnityEnd();

  UnityPrint("ENDSWO");
  while (1) ;
}
