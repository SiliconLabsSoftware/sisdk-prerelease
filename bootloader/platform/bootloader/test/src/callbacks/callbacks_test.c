/***************************************************************************//**
 * @file flash_test.c
 * @brief Test bootloader flash driver
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

#include "config/btl_config.h"
#include "api/btl_interface.h"
#include "core/btl_bootload.h"
#include "core/flash/btl_internal_flash.h"
#include "em_chip.h"
#include "sl_memory_manager.h"

// Unity test framework
#include "unity.h"
#if defined(UNITY_OUTPUT_SERIAL)
#include "unity_usart.h"
#else
#include "unity_swo.h"
#endif

#include "em_common.h"

#include <stdlib.h>

#if ENABLE_TEST_COVERAGE
extern void __gcov_flush(void);
#endif

#ifdef __ICCARM__
__root
#endif
MainBootloaderTable_t mainBootloaderTableImpl;
MainBootloaderTable_t *mainBootloaderTable = &mainBootloaderTableImpl;

#define TEST_DATA_LENGTH 256
#define TEST_PAGE        BTL_UPGRADE_LOCATION
#define LAST_FLASH_PAGE  (FLASH_BASE + FLASH_SIZE - FLASH_PAGE_SIZE)

// PSIRT-140 / PSEC-3251
#if defined(LOCKBITS_BASE) && (LOCKBITS_BASE != LAST_FLASH_PAGE)
#define ALLOW_LAST_FLASH_PAGE 1
#else
#define ALLOW_LAST_FLASH_PAGE 0
#endif

uint32_t parser_getBootloaderUpgradeAddress(void)
{
  return TEST_PAGE;
}

void testApplicationCallbacks(void)
{
  uint8_t dataArray[TEST_DATA_LENGTH];

  // Test if app callback does not write anything if the length variable is not valid.
  for (uint32_t i = 0; i < TEST_DATA_LENGTH; i++) {
    dataArray[i] = i;
  }
  bootload_applicationCallback(TEST_PAGE, dataArray, 0xFFFFFFFF, NULL);
  for (uint32_t i = 0; i < TEST_DATA_LENGTH; i++) {
    dataArray[i] = 0xFF;
  }
  TEST_ASSERT_EQUAL_UINT8_ARRAY(dataArray, (char *)TEST_PAGE, TEST_DATA_LENGTH);

  // Test if app callback does writes as expected.
  for (uint32_t i = 0; i < TEST_DATA_LENGTH; i++) {
    dataArray[i] = i;
  }
  bootload_applicationCallback(TEST_PAGE, dataArray, TEST_DATA_LENGTH, NULL);
  TEST_ASSERT_EQUAL_UINT8_ARRAY(dataArray, (char *)TEST_PAGE, TEST_DATA_LENGTH);

  TEST_ASSERT_TRUE(flash_erasePage(TEST_PAGE));
}

void testBootloaderCallbacks(void)
{
  uint8_t dataArray[TEST_DATA_LENGTH];

  for (uint32_t i = 0; i < TEST_DATA_LENGTH; i++) {
    dataArray[i] = i;
  }
  bootload_bootloaderCallback(0, dataArray, 0xFFFFFFFF, NULL);
  for (uint32_t i = 0; i < TEST_DATA_LENGTH; i++) {
    dataArray[i] = 0xFF;
  }
  TEST_ASSERT_EQUAL_UINT8_ARRAY(dataArray, (char *)TEST_PAGE, TEST_DATA_LENGTH);

  for (uint32_t i = 0; i < TEST_DATA_LENGTH; i++) {
    dataArray[i] = 0xDC;
  }
  // Write dummy data to startOfAppSpace
  TEST_ASSERT_TRUE(flash_writeBuffer((TEST_PAGE - FLASH_PAGE_SIZE), dataArray, TEST_DATA_LENGTH));
  TEST_ASSERT_EQUAL_UINT8_ARRAY(dataArray, TEST_PAGE - FLASH_PAGE_SIZE, TEST_DATA_LENGTH);

  for (uint32_t i = 0; i < TEST_DATA_LENGTH; i++) {
    dataArray[i] = i;
  }
  // Check if bl callback erases the page at startOfAppSpace if offset is set to 0.
  bootload_bootloaderCallback(0, dataArray, TEST_DATA_LENGTH, NULL);
  TEST_ASSERT_EQUAL_UINT8_ARRAY(dataArray, (char *)TEST_PAGE, TEST_DATA_LENGTH);
  for (uint32_t i = 0; i < TEST_DATA_LENGTH; i++) {
    dataArray[i] = 0xFF;
  }
  TEST_ASSERT_EQUAL_UINT8_ARRAY(dataArray, (char *)(TEST_PAGE - FLASH_PAGE_SIZE), TEST_DATA_LENGTH);

  //// PSIRT-140 / PSEC-3251

  uint8_t arrayAA[8], arrayFF[8];
  for (uint32_t i = 0; i < 8; i++) {
    arrayAA[i] = 0xAA;
    arrayFF[i] = 0xFF;
  }

  // 1) (address < BTL_UPGRADE_LOCATION)
  // Erase flash page at (BTL_UPGRADE_LOCATION - FLASH_PAGE_SIZE) to start in known state
  TEST_ASSERT_TRUE(flash_erasePage(BTL_UPGRADE_LOCATION - FLASH_PAGE_SIZE))

  uint32_t offset = (uint32_t) -4;
  bootload_bootloaderCallback(offset, arrayAA, 4, NULL);
  TEST_ASSERT_EQUAL_UINT8_ARRAY(arrayFF, BTL_UPGRADE_LOCATION - 4, 4);

  // 2) (address == LAST_FLASH_PAGE)
  // Erase last flash page to start in known state
  TEST_ASSERT_TRUE(flash_erasePage(LAST_FLASH_PAGE));

  offset = (uint32_t) (LAST_FLASH_PAGE - BTL_UPGRADE_LOCATION);
  bootload_bootloaderCallback(offset, arrayAA, 4, NULL);

  if (ALLOW_LAST_FLASH_PAGE) {
    TEST_ASSERT_EQUAL_UINT8_ARRAY(arrayAA, LAST_FLASH_PAGE, 4);
  } else {
    TEST_ASSERT_EQUAL_UINT8_ARRAY(arrayFF, LAST_FLASH_PAGE, 4);
  }

  // 3) (address < LAST_FLASH_PAGE) && (address + length > LAST_FLASH_PAGE)
  // Erase last flash page to start in known state
  TEST_ASSERT_TRUE(flash_erasePage(LAST_FLASH_PAGE));

  offset = (uint32_t) ((LAST_FLASH_PAGE - 4) - BTL_UPGRADE_LOCATION);
  bootload_bootloaderCallback(offset, arrayAA, 8, NULL);

  if (ALLOW_LAST_FLASH_PAGE) {
    TEST_ASSERT_EQUAL_UINT8_ARRAY(arrayAA, LAST_FLASH_PAGE, 4);
  } else {
    TEST_ASSERT_EQUAL_UINT8_ARRAY(arrayFF, LAST_FLASH_PAGE, 4);
  }
}

int main(void)
{
  CHIP_Init();

#if defined(UNITY_OUTPUT_SERIAL)
  UNITY_UsartSetup();
#else
  SWO_Setup();
#endif
  sl_memory_init();
  mainBootloaderTable->startOfAppSpace = (void *)(TEST_PAGE - FLASH_PAGE_SIZE);
  mainBootloaderTable->endOfAppSpace = (void *)(FLASH_BASE + FLASH_SIZE);
  UnityPrintf("mbt = %lx\n", (uint32_t)&(mainBootloaderTable));

  UnityBeginGroup("callbacks");
  RUN_TEST(testApplicationCallbacks, __LINE__);
  RUN_TEST(testBootloaderCallbacks, __LINE__);

  UnityEnd();

#if ENABLE_TEST_COVERAGE
  __gcov_flush();
#endif

  UnityPrint("ENDSWO");
  while (1) ;
}
