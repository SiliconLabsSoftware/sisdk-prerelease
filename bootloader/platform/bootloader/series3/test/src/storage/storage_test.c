/***************************************************************************//**
 * @file storage_test.c
 * @brief Test storage interface
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
#include "core/btl_core.h"
#include "debug/btl_debug.h"
#include "storage/bootloadinfo/btl_storage_bootloadinfo.h"
#include "storage/btl_storage.h"
#include "sl_memory_manager.h"
#include "sl_main_init.h"

#include BTL_CONFIG_FILE

// Unity test framework
#include "unity.h"

#include <string.h>

static void testInit(void)
{
  TEST_ASSERT_EQUAL(BOOTLOADER_OK, storage_init());
  TEST_ASSERT_EQUAL(0U, storage_is_busy());
  TEST_ASSERT_EQUAL(BOOTLOADER_OK, storage_shutdown());

  TEST_ASSERT_EQUAL(BOOTLOADER_OK, btl_init());
  TEST_ASSERT_EQUAL(BOOTLOADER_OK, btl_deinit());
}

static void testGetInfo(void)
{
  TEST_ASSERT_EQUAL(BOOTLOADER_OK, storage_init());
  BootloaderStorageInformation_t info = { 0 };
  storage_get_info(&info);
  TEST_ASSERT_EQUAL(BOOTLOADER_OK, storage_shutdown());

  TEST_ASSERT(info.numStorageSlots > 0);
  TEST_ASSERT_EQUAL(BTL_STORAGE_NUM_SLOTS, info.numStorageSlots);

#if defined(BTL_PLUGIN_STORAGE_EXT_FLASH)
  TEST_ASSERT_EQUAL(EXTERNAL_FLASH, info.storageType);
#else
  TEST_FAIL_MESSAGE("Unknown storage type");
#endif
}

static void testGetSlotInfo(void)
{
  TEST_ASSERT_EQUAL(BOOTLOADER_OK, storage_init());
  BootloaderStorageInformation_t info;
  storage_get_info(&info);

  TEST_ASSERT(info.numStorageSlots > 0);

  BootloaderStorageSlot_t slot;

  // Invalid slot info pointer
  TEST_ASSERT_EQUAL(BOOTLOADER_ERROR_STORAGE_INVALID_SLOT,
                    storage_get_slot_info(0, NULL));
  // Invalid slot index
  TEST_ASSERT_EQUAL(BOOTLOADER_ERROR_STORAGE_INVALID_SLOT,
                    storage_get_slot_info(info.numStorageSlots, &slot));

  for (size_t slotIdx = 0; slotIdx < info.numStorageSlots; slotIdx++) {
    // Valid slot index
    TEST_ASSERT_EQUAL(BOOTLOADER_OK, storage_get_slot_info(slotIdx, &slot));
    // Slot has a non-zero size
    TEST_ASSERT(slot.length > 0);
    // Slot fits within storage
#if !defined(STORAGE_EXTERNAL_FLASH_TEST)
    // partSize reflects the size of the storage area, not the absolute flash size
    TEST_ASSERT(slot.address + slot.length <= info.info->partSize);
#endif
  }
  TEST_ASSERT_EQUAL(BOOTLOADER_OK, storage_shutdown());
}

static void testSlotApi(void)
{
  TEST_ASSERT_EQUAL(BOOTLOADER_OK, storage_init());
  BootloaderStorageSlot_t slot;
  uint8_t writeData[128];
  uint8_t readData[128] = { 0 };

  for (uint8_t i = 0; i < sizeof(writeData); i++) {
    writeData[i] = i;
  }

  // Get information about slot
  storage_get_slot_info(0, &slot);

  // Erase slot 0
  TEST_ASSERT_EQUAL(BOOTLOADER_OK, storage_erase_slot(0));
  // Read data
  TEST_ASSERT_EQUAL(BOOTLOADER_OK,
                    storage_read_slot(0, 0, readData, sizeof(readData)));
  // Should be all FFs
  for (size_t i = 0; i < sizeof(readData); i++) {
    TEST_ASSERT_EQUAL(0xFF, readData[i]);
  }

  // Write data to invalid slot (should error)
  TEST_ASSERT_EQUAL(BOOTLOADER_ERROR_STORAGE_INVALID_SLOT,
                    storage_write_slot(-1, 0, writeData, sizeof(writeData)));
  // Write data outside slot (should error)
  TEST_ASSERT_EQUAL(BOOTLOADER_ERROR_STORAGE_INVALID_ADDRESS,
                    storage_write_slot(0, slot.length, writeData, sizeof(writeData)));
  // Write data (should work)
  TEST_ASSERT_EQUAL(BOOTLOADER_OK,
                    storage_write_slot(0, 0, writeData, sizeof(writeData)));
  // Write data again (should error)
  TEST_ASSERT_EQUAL(BOOTLOADER_ERROR_STORAGE_NEEDS_ERASE,
                    storage_write_slot(0, 0, writeData, sizeof(writeData)));

  // Read data from invalid slot (should error)
  TEST_ASSERT_EQUAL(BOOTLOADER_ERROR_STORAGE_INVALID_SLOT,
                    storage_read_slot(-1, 0, readData, sizeof(readData)));
  // Read data outside slot (should error)
  TEST_ASSERT_EQUAL(BOOTLOADER_ERROR_STORAGE_INVALID_ADDRESS,
                    storage_read_slot(0, slot.length, readData, sizeof(readData)));
  // Read data (should work)
  TEST_ASSERT_EQUAL(BOOTLOADER_OK,
                    storage_read_slot(0, 0, readData, sizeof(readData)));

  // Compare data
  TEST_ASSERT_EQUAL_UINT8_ARRAY(writeData, readData, 128);

  // --------------------------------

  // Erase slot 1
#if BTL_STORAGE_NUM_SLOTS > 1
  TEST_ASSERT_EQUAL(BOOTLOADER_OK, storage_erase_slot(1));
#else
  TEST_ASSERT_EQUAL(BOOTLOADER_ERROR_STORAGE_INVALID_SLOT, storage_erase_slot(1));
#endif

  // Read data from slot 0
  memset(readData, 0, sizeof(readData));
  TEST_ASSERT_EQUAL(BOOTLOADER_OK,
                    storage_read_slot(0, 0, readData, sizeof(readData)));

  // Compare data; should still be preserved
  TEST_ASSERT_EQUAL_UINT8_ARRAY(writeData, readData, 128);

  TEST_ASSERT_EQUAL(BOOTLOADER_OK, storage_shutdown());
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
                    storage_read_raw(slot.address, readData, 0xFFFFFFFF));
  TEST_ASSERT_EQUAL(BOOTLOADER_ERROR_STORAGE_INVALID_ADDRESS,
                    storage_write_raw(slot.address, readData, 0xFFFFFFFF));
  // Get information about slot
  TEST_ASSERT_EQUAL(BOOTLOADER_OK, storage_get_slot_info(0, &slot));
  // Erase slot 0
  TEST_ASSERT_EQUAL(BOOTLOADER_OK, storage_erase_slot(0));
  // Read data
  TEST_ASSERT_EQUAL(BOOTLOADER_OK,
                    storage_read_raw(slot.address, readData, sizeof(readData)));
  // Should be all FFs
  for (size_t i = 0; i < sizeof(readData); i++) {
    TEST_ASSERT_EQUAL(0xFF, readData[i]);
  }

  TEST_ASSERT_EQUAL(BOOTLOADER_ERROR_STORAGE_NEEDS_ALIGN, storage_erase_raw(slot.address, slot.length - 1));
  TEST_ASSERT_EQUAL(BOOTLOADER_ERROR_STORAGE_NEEDS_ALIGN, storage_erase_raw(slot.address + 1, slot.length));
  TEST_ASSERT_EQUAL(BOOTLOADER_ERROR_STORAGE_INVALID_ADDRESS, storage_erase_raw(slot.address, slot.length * 150));

  // Write to the first storge slot, read data and compare data.
  for (uint32_t addr = slot.address; addr < (slot.address + slot.length); addr = (addr + testArrayLength)) {
    storage_write_raw(addr, writeData, testArrayLength);
    memset(readData, 0, sizeof(readData));
    storage_read_raw(addr, readData, testArrayLength);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(writeData, readData, testArrayLength);
  }
  TEST_ASSERT_EQUAL(BOOTLOADER_OK, storage_erase_raw(slot.address, slot.length));

  TEST_ASSERT_EQUAL(BOOTLOADER_OK, storage_shutdown());
}

static void testGetSetImageToBootload(void)
{
  int32_t btl_slot_list[2] = { -1 };

  TEST_ASSERT_EQUAL(BOOTLOADER_OK, storage_init());
  TEST_ASSERT_EQUAL(BOOTLOADER_OK, storage_set_bootload_list(btl_slot_list, 1));

  btl_slot_list[0] = -2;
  TEST_ASSERT_EQUAL(BOOTLOADER_OK, storage_get_bootload_list(btl_slot_list, 1));

#if BTL_STORAGE_NUM_SLOTS > 1
  // If we have multiple slots, it should be possible to select _no_ slot
  TEST_ASSERT_EQUAL(-1, btl_slot_list[0]);
#else
  // If there is only one slot, it cannot be deselected
  TEST_ASSERT_EQUAL(0, btl_slot_list[0]);
#endif

  btl_slot_list[0] = 0;
  TEST_ASSERT_EQUAL(BOOTLOADER_OK, storage_set_bootload_list(btl_slot_list, 1));

  btl_slot_list[0] = -2;
  TEST_ASSERT_EQUAL(BOOTLOADER_OK, storage_get_bootload_list(btl_slot_list, 1));
  TEST_ASSERT_EQUAL(0, btl_slot_list[0]);

  TEST_ASSERT_EQUAL(BOOTLOADER_OK, storage_shutdown());
}

int main(void)
{
  // CHIP_Init();
  sl_main_init();
  BTL_DEBUG_INIT();

  sl_memory_init();

  UnityBegin("STORAGE");

  RUN_TEST(testInit, __LINE__);
  RUN_TEST(testGetInfo, __LINE__);
  RUN_TEST(testGetSlotInfo, __LINE__);
  RUN_TEST(testSlotApi, __LINE__);
  RUN_TEST(testStorageRaw, __LINE__);
  RUN_TEST(testGetSetImageToBootload, __LINE__);

  UnityEnd();

  UnityPrint("ENDSWO");
  while (1) ;
}
