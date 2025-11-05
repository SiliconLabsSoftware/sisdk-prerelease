/***************************************************************************//**
 * @file bootloadinfo_test.c
 * @brief Test bootload info functions
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

#include "debug/btl_debug.h"
#include "storage/btl_storage.h"
#include "storage/bootloadinfo/btl_storage_bootloadinfo.h"
#include "storage/ext_mem/btl_storage_ext_mem.h"
#include "sl_main_init.h"
#include "sl_memory_manager.h"

#include BTL_CONFIG_FILE

// Unity test framework
#include "unity.h"

#include <string.h>

void testGetSetBootloadInfo(void)
{
  int32_t ret;
  int32_t expectedSlots[BTL_STORAGE_BOOTLOAD_LIST_LENGTH] = { 0 };
  int32_t actualSlots[BTL_STORAGE_BOOTLOAD_LIST_LENGTH] = { -1 };

  ret = storage_set_bootload_list(actualSlots, BTL_STORAGE_BOOTLOAD_LIST_LENGTH);
  TEST_ASSERT_EQUAL(BOOTLOADER_OK, ret);

  ret = storage_set_bootload_list(actualSlots, BTL_STORAGE_BOOTLOAD_LIST_LENGTH + 1);
  TEST_ASSERT_EQUAL(BOOTLOADER_ERROR_BOOTLOAD_LIST_OVERFLOW, ret);
  int32_t slotID = 930;
#if BTL_STORAGE_BOOTLOAD_LIST_LENGTH > 1
  ret = storage_append_bootload_list(slotID);
  TEST_ASSERT_EQUAL(BOOTLOADER_OK, ret);

  ret = storage_append_bootload_list(slotID);
  TEST_ASSERT_EQUAL(BOOTLOADER_ERROR_BOOTLOAD_LIST_ENTRY_EXISTS, ret);

  ret = storage_get_bootload_list(actualSlots, BTL_STORAGE_BOOTLOAD_LIST_LENGTH);
  TEST_ASSERT_EQUAL(BOOTLOADER_OK, ret);

  for (size_t i = 0; i < BTL_STORAGE_BOOTLOAD_LIST_LENGTH; i++) {
    if (actualSlots[i] == slotID) {
      break;
    }

    if (i == (BTL_STORAGE_BOOTLOAD_LIST_LENGTH - 1)) {
      TEST_ASSERT(false);
    }
  }

  ret = storage_get_bootload_list(actualSlots, BTL_STORAGE_BOOTLOAD_LIST_LENGTH + 1);
  TEST_ASSERT_EQUAL(BOOTLOADER_ERROR_BOOTLOAD_LIST_OVERFLOW, ret);
#else
  ret = storage_append_bootload_list(slotID);
  TEST_ASSERT_EQUAL(BOOTLOADER_ERROR_BOOTLOAD_LIST_FULL, ret);

  expectedSlots[0] = 1;
  ret = storage_set_bootload_list(expectedSlots, BTL_STORAGE_BOOTLOAD_LIST_LENGTH);
  TEST_ASSERT_EQUAL(BOOTLOADER_ERROR_BOOTLOAD_LIST_OVERFLOW, ret);
  expectedSlots[0] = 0;
#endif

  ret = storage_set_bootload_list(expectedSlots, BTL_STORAGE_BOOTLOAD_LIST_LENGTH);
  TEST_ASSERT_EQUAL(BOOTLOADER_OK, ret);

  ret = storage_get_bootload_list(actualSlots, BTL_STORAGE_BOOTLOAD_LIST_LENGTH);
  TEST_ASSERT_EQUAL(BOOTLOADER_OK, ret);

  TEST_ASSERT_EQUAL_INT32_ARRAY(expectedSlots, actualSlots, BTL_STORAGE_BOOTLOAD_LIST_LENGTH);

  for (size_t i = 0; i < BTL_STORAGE_BOOTLOAD_LIST_LENGTH; i++) {
    expectedSlots[i] = i;
  }

  ret = storage_set_bootload_list(expectedSlots, BTL_STORAGE_BOOTLOAD_LIST_LENGTH);
  TEST_ASSERT_EQUAL(BOOTLOADER_OK, ret);

#if BTL_STORAGE_BOOTLOAD_LIST_LENGTH > 1
  ret = storage_append_bootload_list(BTL_STORAGE_BOOTLOAD_LIST_LENGTH);
  TEST_ASSERT_EQUAL(BOOTLOADER_ERROR_BOOTLOAD_LIST_FULL, ret);
#endif

  ret = storage_get_bootload_list(actualSlots, BTL_STORAGE_BOOTLOAD_LIST_LENGTH);
  TEST_ASSERT_EQUAL(BOOTLOADER_OK, ret);

  TEST_ASSERT_EQUAL_INT32_ARRAY(expectedSlots, actualSlots, BTL_STORAGE_BOOTLOAD_LIST_LENGTH);
#if BTL_STORAGE_BOOTLOAD_LIST_LENGTH > 1
  BootloadInfo_t btlInfo = { 0 };
  BootloaderStorageInformation_t storageInfo = { 0 };
  storage_get_info(&storageInfo);
  uint32_t storagePageOne = storage_get_base_address();
  uint32_t storagePageTwo = storagePageOne + FLASH_PAGE_SIZE;
  UnityPrintF(__LINE__, "Slot 0 Page 1 Ptr %x\n", storagePageOne);
  UnityPrintF(__LINE__, "Slot 0 Page 2 Ptr %x\n", storagePageTwo);

  TEST_ASSERT_EQUAL(BOOTLOADER_OK, storage_erase_raw(storagePageOne, FLASH_PAGE_SIZE));
  TEST_ASSERT_EQUAL(BOOTLOADER_OK, storage_read_raw(storagePageOne, (uint8_t *)&btlInfo, 12UL));
  TEST_ASSERT_NOT_EQUAL(btlInfo.magic, BTL_STORAGE_BOOTLOADINFO_MAGIC);
  TEST_ASSERT_EQUAL(BOOTLOADER_OK, storage_read_raw(storagePageTwo, (uint8_t *)&btlInfo, 12UL));
  TEST_ASSERT_EQUAL(btlInfo.magic, BTL_STORAGE_BOOTLOADINFO_MAGIC);

  // Page 0 is corrupt; read from page 1
  ret = storage_get_bootload_list(actualSlots, BTL_STORAGE_BOOTLOAD_LIST_LENGTH);
  TEST_ASSERT_EQUAL(BOOTLOADER_OK, ret);

  // Erase Page 0 and 1.
  TEST_ASSERT_EQUAL(BOOTLOADER_OK, storage_erase_raw(storagePageOne, FLASH_PAGE_SIZE * 2));
  ret = storage_get_bootload_list(actualSlots, BTL_STORAGE_BOOTLOAD_LIST_LENGTH);
  TEST_ASSERT_EQUAL(BOOTLOADER_ERROR_BOOTLOAD_LIST_NO_LIST, ret);
#endif
}

#if defined(BOOTLOADINFO_TEST_SINGLE)
void testGetSetBootloadInfoSingle(void)
{
  int32_t ret;
  int32_t expectedSlots[BTL_STORAGE_BOOTLOAD_LIST_LENGTH];
  int32_t actualSlots[BTL_STORAGE_BOOTLOAD_LIST_LENGTH];

  for (uint32_t i = 0; i < BTL_STORAGE_BOOTLOAD_LIST_LENGTH; i++) {
    expectedSlots[i] = -1;
    actualSlots[i] = -1;
  }

  ret = storage_get_bootload_list(actualSlots, BTL_STORAGE_BOOTLOAD_LIST_LENGTH);
  TEST_ASSERT_EQUAL(BOOTLOADER_OK, ret);

  TEST_ASSERT_EQUAL(0, actualSlots[0]);
  TEST_ASSERT_EQUAL_INT32_ARRAY(&expectedSlots[1], &actualSlots[1], BTL_STORAGE_BOOTLOAD_LIST_LENGTH - 1);

  actualSlots[0] = 2;
  ret = storage_set_bootload_list(actualSlots, BTL_STORAGE_BOOTLOAD_LIST_LENGTH + 1);
  TEST_ASSERT_EQUAL(BOOTLOADER_ERROR_BOOTLOAD_LIST_OVERFLOW, ret);

  actualSlots[0] = 0;
  ret = storage_set_bootload_list(actualSlots, BTL_STORAGE_BOOTLOAD_LIST_LENGTH + 1);
  TEST_ASSERT_EQUAL(BOOTLOADER_OK, ret);

  actualSlots[0] = -1;
  ret = storage_set_bootload_list(actualSlots, BTL_STORAGE_BOOTLOAD_LIST_LENGTH + 1);
  TEST_ASSERT_EQUAL(BOOTLOADER_OK, ret);

  ret = storage_append_bootload_list(654);
  TEST_ASSERT_EQUAL(BOOTLOADER_ERROR_BOOTLOAD_LIST_FULL, ret);
}
#endif

int main(void)
{
  // CHIP_Init();
  sl_main_init();
  BTL_DEBUG_INIT();

  sl_memory_init();

  storage_init();

  UnityBegin("BOOTLOADINFO");
#if !defined(BOOTLOADINFO_TEST_SINGLE)
  RUN_TEST(testGetSetBootloadInfo, __LINE__);
#else
  RUN_TEST(testGetSetBootloadInfoSingle, __LINE__);
#endif
  UnityEnd();

  storage_shutdown();

  UnityPrint("ENDSWO");
  while (1) ;
}
