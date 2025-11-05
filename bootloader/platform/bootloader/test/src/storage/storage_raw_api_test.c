/***************************************************************************//**
 * @file storage_raw_api_test.c
 * @brief Test storage interface
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
#include "core/btl_core.h"
#include "debug/btl_debug.h"
#include "storage/bootloadinfo/btl_storage_bootloadinfo.h"
#include "storage/btl_storage.h"
#include "sl_memory_manager.h"

#include "em_chip.h"

#include BTL_CONFIG_FILE

// Unity test framework
#include "unity.h"
#if defined(UNITY_OUTPUT_SERIAL)
#include "unity_usart.h"
#else
#include "unity_swo.h"
#endif

#include <string.h>

#if ENABLE_TEST_COVERAGE
extern void __gcov_flush(void);
#endif

static void testInit(void)
{
  TEST_ASSERT_EQUAL(BOOTLOADER_OK, storage_init());
  TEST_ASSERT_EQUAL(0U, storage_isBusy());
  TEST_ASSERT_EQUAL(BOOTLOADER_OK, storage_shutdown());

  TEST_ASSERT_EQUAL(BOOTLOADER_OK, btl_init());
  TEST_ASSERT_EQUAL(BOOTLOADER_OK, btl_deinit());
}

static void testStorageRaw(void)
{
  TEST_ASSERT_EQUAL(BOOTLOADER_OK, storage_init());
  BootloaderStorageSlot_t slot;
  uint8_t testArrayLength = 128;
  uint8_t writeData[128];
  uint8_t readData[128] = { 0 };

  for (uint8_t i = 0; i < sizeof(writeData); i++) {
    writeData[i] = i;
  }
  TEST_ASSERT_EQUAL(BOOTLOADER_ERROR_STORAGE_INVALID_ADDRESS,
                    storage_readRaw(slot.address, readData, 0xFFFFFFFF));
  TEST_ASSERT_EQUAL(BOOTLOADER_ERROR_STORAGE_INVALID_ADDRESS,
                    storage_writeRaw(slot.address, readData, 0xFFFFFFFF));
  // Get information about slot
  TEST_ASSERT_EQUAL(BOOTLOADER_OK, storage_getSlotInfo(0, &slot));
  // Erase slot 0
  TEST_ASSERT_EQUAL(BOOTLOADER_OK, storage_eraseSlot(0));
  // Read data
  TEST_ASSERT_EQUAL(BOOTLOADER_OK,
                    storage_readRaw(slot.address, readData, sizeof(readData)));
  // Should be all FFs
  for (size_t i = 0; i < sizeof(readData); i++) {
    TEST_ASSERT_EQUAL(0xFF, readData[i]);
  }

  TEST_ASSERT_EQUAL(BOOTLOADER_ERROR_STORAGE_NEEDS_ALIGN, storage_eraseRaw(slot.address, slot.length - 1));
  TEST_ASSERT_EQUAL(BOOTLOADER_ERROR_STORAGE_NEEDS_ALIGN, storage_eraseRaw(slot.address + 1, slot.length));
  TEST_ASSERT_EQUAL(BOOTLOADER_ERROR_STORAGE_INVALID_ADDRESS, storage_eraseRaw(slot.address, slot.length * 150));

  // Write to the first storge slot, read data and compare data.
  for (uint32_t addr = slot.address; addr < (slot.address + slot.length); addr = (addr + testArrayLength)) {
    storage_writeRaw(addr, writeData, testArrayLength);
    memset(readData, 0, sizeof(readData));
    storage_readRaw(addr, readData, testArrayLength);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(writeData, readData, testArrayLength);
  }
  TEST_ASSERT_EQUAL(BOOTLOADER_OK, storage_eraseRaw(slot.address, slot.length));

  TEST_ASSERT_EQUAL(BOOTLOADER_OK, storage_shutdown());
}

int main(void)
{
  CHIP_Init();
  BTL_DEBUG_INIT();

#if defined(UNITY_OUTPUT_SERIAL)
  UNITY_UsartSetup();
#else
  SWO_Setup();
#endif
  sl_memory_init();

  UnityBeginGroup("STORAGE");

  RUN_TEST(testInit, __LINE__);
  RUN_TEST(testStorageRaw, __LINE__);

  UnityEnd();

#if ENABLE_TEST_COVERAGE
  __gcov_flush();
#endif

  UnityPrint("ENDSWO");
  while (1) ;
}
