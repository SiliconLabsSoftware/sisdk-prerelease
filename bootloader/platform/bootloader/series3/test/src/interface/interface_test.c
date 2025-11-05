/***************************************************************************//**
 * @file interface_test.c
 * @brief Test bootloader interface
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

static void CYCLE_Init(void)
{
  DWT->CTRL |= 1;
}

static uint32_t CYCLE_Count(void)
{
  return DWT->CYCCNT;
}

static uint32_t CYCLE_toMs(uint32_t cycles)
{
  uint32_t freq_khz = SystemCoreClockGet() / 1000;
  uint32_t ms = cycles / freq_khz;
  return ms;
}

// void HardFault_Handler(void)
// {
//   UnityPrint("FAULT\n          ");
//   while (1) ;
// }

static void testGetInfo(void)
{
  TEST_ASSERT_EQUAL(BOOTLOADER_OK, bootloader_init());
  BootloaderInformation_t info;
  bootloader_getInfo(&info);

  TEST_ASSERT_EQUAL(BOOTLOADER_OK, bootloader_deinit());

#if defined(CHECK_SECURE_BOOT_FLAG)
  // The interface tests are using bootloaders with secure boot flag set.
  TEST_ASSERT_EQUAL(true, bootloader_secureBootEnforced());
#else
  TEST_ASSERT_EQUAL(false, bootloader_secureBootEnforced());
#endif

  TEST_ASSERT_EQUAL(SL_BOOTLOADER, info.type);

  TEST_PRINTF("Bootloader version: v%d\n", info.version);

  TEST_ASSERT(info.version > 0 && info.version < 0xF0000000);

  // Check capabilities
  // --------------------------------

  // All bootloaders should support GBL
  TEST_ASSERT_TRUE(info.capabilities & BOOTLOADER_CAPABILITY_GBL);

  if (info.capabilities & BOOTLOADER_CAPABILITY_ENFORCE_SECURE_BOOT) {
    // This app should be signed, considering that the bootloader requires it.
    // TODO: Test verification?
  }

  if (info.capabilities & BOOTLOADER_CAPABILITY_ENFORCE_UPGRADE_SIGNATURE) {
    // Bootloader enforces signature -- should support signature
    TEST_ASSERT_TRUE(info.capabilities & BOOTLOADER_CAPABILITY_GBL_SIGNATURE);
  }
  if (info.capabilities & BOOTLOADER_CAPABILITY_ENFORCE_UPGRADE_ENCRYPTION) {
    // Bootloader enforces encryption -- should support encryption
    TEST_ASSERT_TRUE(info.capabilities & BOOTLOADER_CAPABILITY_GBL_ENCRYPTION);
  }

  uint32_t certVersion = 999;
  TEST_ASSERT_FALSE(bootloader_getCertificateVersion(&certVersion));

  TEST_PRINTF("BootloaderParserContext size: %d\n", bootloader_parserContextSize());
  TEST_ASSERT_TRUE(bootloader_parserContextSize() > 0UL);
  TEST_ASSERT_FALSE(BOOTLOADER_STORAGE_VERIFICATION_CONTEXT_SIZE < bootloader_parserContextSize());
}

static void testInit(void)
{
  TEST_ASSERT_EQUAL(BOOTLOADER_OK, bootloader_init());

  TEST_ASSERT_EQUAL(BOOTLOADER_OK, bootloader_deinit());
}

static void testGetStorageInfo(void)
{
  TEST_ASSERT_EQUAL(BOOTLOADER_OK, bootloader_init());

  BootloaderStorageInformation_t info = { 0 };

  bootloader_getStorageInfo(&info);

  TEST_ASSERT_EQUAL(BOOTLOADER_OK, bootloader_deinit());

  TEST_ASSERT(info.numStorageSlots > 0);
}

static void testGetStorageSlotInfo(void)
{
  TEST_ASSERT_EQUAL(BOOTLOADER_OK, bootloader_init());

  BootloaderStorageInformation_t info;
  bootloader_getStorageInfo(&info);

  TEST_ASSERT_EQUAL(BOOTLOADER_OK, bootloader_deinit());

  TEST_ASSERT(info.numStorageSlots > 0);

  BootloaderStorageSlot_t slot;

  // Invalid slot info pointer
  TEST_ASSERT_EQUAL(BOOTLOADER_ERROR_STORAGE_INVALID_SLOT,
                    bootloader_getStorageSlotInfo(0, NULL));
  // Invalid slot index
  TEST_ASSERT_EQUAL(BOOTLOADER_ERROR_STORAGE_INVALID_SLOT,
                    bootloader_getStorageSlotInfo(info.numStorageSlots, &slot));

  for (size_t slotIdx = 0; slotIdx < info.numStorageSlots; slotIdx++) {
    // Valid slot index
    TEST_ASSERT_EQUAL(BOOTLOADER_OK, bootloader_getStorageSlotInfo(slotIdx, &slot));
    // Slot has a non-zero size
    TEST_ASSERT(slot.length > 0);
  }
}

static void testStorageSlotApi(void)
{
  TEST_ASSERT_EQUAL(BOOTLOADER_OK, bootloader_init());

  int32_t retVal;
  BootloaderEraseStatus_t eraseStat;
  BootloaderStorageInformation_t info;
  bootloader_getStorageInfo(&info);

  BootloaderStorageSlot_t slot;
  uint8_t writeData[128];
  uint8_t readData[128] = { 0 };

  for (uint8_t i = 0; i < sizeof(writeData); i++) {
    writeData[i] = i;
  }

  // Get information about slot
  bootloader_getStorageSlotInfo(0, &slot);

  // Erase slot 0
  TEST_ASSERT_EQUAL(BOOTLOADER_OK, bootloader_eraseStorageSlot(0));

  // Read data
  TEST_ASSERT_EQUAL(BOOTLOADER_OK,
                    bootloader_readStorage(0, 0, readData, sizeof(readData)));
  // Should be all FFs
  for (size_t i = 0; i < sizeof(readData); i++) {
    TEST_ASSERT_EQUAL(0xFF, readData[i]);
  }

  // Write data to invalid slot (should error)
  TEST_ASSERT_EQUAL(BOOTLOADER_ERROR_STORAGE_INVALID_SLOT,
                    bootloader_writeStorage(-1, 0, writeData, sizeof(writeData)));

  // Write data outside slot (should error)
  TEST_ASSERT_EQUAL(BOOTLOADER_ERROR_STORAGE_INVALID_ADDRESS,
                    bootloader_writeStorage(0, slot.length, writeData, sizeof(writeData)));

  // Write data (should work)
  TEST_ASSERT_EQUAL(BOOTLOADER_OK,
                    bootloader_writeStorage(0, 0, writeData, sizeof(writeData)));

  // Write data again (should error)
  TEST_ASSERT_EQUAL(BOOTLOADER_ERROR_STORAGE_NEEDS_ERASE,
                    bootloader_writeStorage(0, 0, writeData, sizeof(writeData)));

  // Write data below slot (should error) offset is overflowing
  TEST_ASSERT_EQUAL(BOOTLOADER_ERROR_STORAGE_INVALID_ADDRESS,
                    bootloader_writeStorage(0, 0xFFFFFF7FUL, writeData, sizeof(writeData) + 1));

  // Read data from invalid slot (should error)
  TEST_ASSERT_EQUAL(BOOTLOADER_ERROR_STORAGE_INVALID_SLOT,
                    bootloader_readStorage(-1, 0, readData, sizeof(readData)));

  // Read data outside slot (should error)
  TEST_ASSERT_EQUAL(BOOTLOADER_ERROR_STORAGE_INVALID_ADDRESS,
                    bootloader_readStorage(0, slot.length, readData, sizeof(readData)));

  // Read data below slot (should error) offset is overflowing
  TEST_ASSERT_EQUAL(BOOTLOADER_ERROR_STORAGE_INVALID_ADDRESS,
                    bootloader_readStorage(0, 0xFFFFFF7FUL, readData, sizeof(readData) + 1));

  // Read data (should work)
  TEST_ASSERT_EQUAL(BOOTLOADER_OK,
                    bootloader_readStorage(0, 0, readData, sizeof(readData)));

  // Compare data
  TEST_ASSERT_EQUAL_UINT8_ARRAY(writeData, readData, 128);

  // --------------------------------
  if (info.numStorageSlots > 1) {
    // Erase slot 1
    TEST_ASSERT_EQUAL(BOOTLOADER_OK, bootloader_eraseStorageSlot(1));
  }

  // Read data from slot 0
  memset(readData, 0, sizeof(readData));
  TEST_ASSERT_EQUAL(BOOTLOADER_OK,
                    bootloader_readStorage(0, 0, readData, sizeof(readData)));

  // Compare data; should still be preserved
  TEST_ASSERT_EQUAL_UINT8_ARRAY(writeData, readData, 128);

  // Write data to slot 0 starting at a page boundary
  TEST_ASSERT_EQUAL(BOOTLOADER_OK,
                    bootloader_eraseWriteStorage(0, 0, writeData, sizeof(writeData)));

  // Read and compare data
  memset(readData, 0, sizeof(readData));
  TEST_ASSERT_EQUAL(BOOTLOADER_OK,
                    bootloader_readStorage(0, 0, readData, sizeof(readData)));
  TEST_ASSERT_EQUAL_UINT8_ARRAY(writeData, readData, 128);

  // Write data to slot 0 starting in the middle of a page
  TEST_ASSERT_EQUAL(BOOTLOADER_OK,
                    bootloader_eraseWriteStorage(0, 512, writeData, sizeof(writeData)));

  // Read data, the data stored previously should still be preserved
  memset(readData, 0, sizeof(readData));
  TEST_ASSERT_EQUAL(BOOTLOADER_OK,
                    bootloader_readStorage(0, 0, readData, sizeof(readData)));
  TEST_ASSERT_EQUAL_UINT8_ARRAY(writeData, readData, 128);

  // Read data
  memset(readData, 0, sizeof(readData));
  TEST_ASSERT_EQUAL(BOOTLOADER_OK,
                    bootloader_readStorage(0, 512, readData, sizeof(readData)));
  TEST_ASSERT_EQUAL_UINT8_ARRAY(writeData, readData, 128);

  // Write data to slot 0 starting at the second page boundary
  TEST_ASSERT_EQUAL(BOOTLOADER_OK,
                    bootloader_eraseWriteStorage(0, FLASH_PAGE_SIZE, writeData, sizeof(writeData)));

  // Read data
  memset(readData, 0, sizeof(readData));
  TEST_ASSERT_EQUAL(BOOTLOADER_OK,
                    bootloader_readStorage(0, FLASH_PAGE_SIZE, readData, sizeof(readData)));
  TEST_ASSERT_EQUAL_UINT8_ARRAY(writeData, readData, 128);

  // Write data to slot 0 crossing the first and second page boundary
  TEST_ASSERT_EQUAL(BOOTLOADER_OK,
                    bootloader_eraseWriteStorage(0,
                                                 FLASH_PAGE_SIZE - sizeof(writeData) / 2,
                                                 writeData,
                                                 sizeof(writeData)));

  // Read data, the data stored in the first page previously should still be preserved
  memset(readData, 0, sizeof(readData));
  TEST_ASSERT_EQUAL(BOOTLOADER_OK,
                    bootloader_readStorage(0, 512, readData, sizeof(readData)));
  TEST_ASSERT_EQUAL_UINT8_ARRAY(writeData, readData, 128);

  // Read data, the data stored in the second page previously should erased
  memset(readData, 0, sizeof(readData));
  TEST_ASSERT_EQUAL(BOOTLOADER_OK,
                    bootloader_readStorage(0, FLASH_PAGE_SIZE, readData, sizeof(readData) / 2));
  TEST_ASSERT_EQUAL_UINT8_ARRAY(&writeData[64], readData, 64);

  // Write data to slot 0 to the end of the second page
  TEST_ASSERT_EQUAL(BOOTLOADER_OK,
                    bootloader_eraseWriteStorage(0,
                                                 FLASH_PAGE_SIZE * 2 - sizeof(writeData),
                                                 writeData,
                                                 sizeof(writeData)));
  memset(readData, 0, sizeof(readData));
  TEST_ASSERT_EQUAL(BOOTLOADER_OK,
                    bootloader_readStorage(0,
                                           FLASH_PAGE_SIZE * 2 - sizeof(readData),
                                           readData,
                                           sizeof(readData)));
  TEST_ASSERT_EQUAL_UINT8_ARRAY(writeData, readData, 128);

  // Erase slot 0
  TEST_ASSERT_EQUAL(BOOTLOADER_OK, bootloader_eraseStorageSlot(0));

  // Fill the first five pages in storage slot 0 with dummy data
  for (uint32_t i = 0; i < FLASH_PAGE_SIZE * 5 / 128; i++) {
    TEST_ASSERT_EQUAL(BOOTLOADER_OK,
                      bootloader_writeStorage(0, i * 128, writeData, sizeof(writeData)));
  }

  TEST_ASSERT_EQUAL(BOOTLOADER_OK, bootloader_initChunkedEraseStorageSlot(0, &eraseStat));

  memset(writeData, 0xFF, sizeof(writeData));
  TEST_ASSERT_EQUAL(BOOTLOADER_ERROR_STORAGE_CONTINUE, bootloader_chunkedEraseStorageSlot(&eraseStat));

  // Read data from slot 0 from a random memory space inside page 1
  memset(readData, 0, sizeof(readData));
  TEST_ASSERT_EQUAL(BOOTLOADER_OK,
                    bootloader_readStorage(0, 1024, readData, sizeof(readData)));

  // Compare data; readData should contain only FF's
  TEST_ASSERT_EQUAL_UINT8_ARRAY(writeData, readData, 128);

  // Erase pages 2 - 5 from slot 0
  for (uint8_t i = 0; i < 4; i++) {
    TEST_ASSERT_EQUAL(BOOTLOADER_ERROR_STORAGE_CONTINUE, bootloader_chunkedEraseStorageSlot(&eraseStat));
  }

  // Read data from slot 0 from a random memory space inside page 3
  memset(readData, 0, sizeof(readData));
  TEST_ASSERT_EQUAL(BOOTLOADER_OK,
                    bootloader_readStorage(0,
                                           FLASH_PAGE_SIZE * 2 + 512,
                                           readData,
                                           sizeof(readData)));

  // Compare data; readData should contain only FF's
  TEST_ASSERT_EQUAL_UINT8_ARRAY(writeData, readData, 128);

  // Read data from slot 0 from a random memory space inside page 5
  memset(readData, 0, sizeof(readData));
  TEST_ASSERT_EQUAL(BOOTLOADER_OK,
                    bootloader_readStorage(0,
                                           FLASH_PAGE_SIZE * 4 + 1024,
                                           readData,
                                           sizeof(readData)));

  // Compare data; readData should contain only FF's
  TEST_ASSERT_EQUAL_UINT8_ARRAY(writeData, readData, 128);

  do {
    retVal = bootloader_chunkedEraseStorageSlot(&eraseStat);
  } while (retVal == BOOTLOADER_ERROR_STORAGE_CONTINUE);

  TEST_ASSERT_EQUAL(BOOTLOADER_OK, retVal);
  TEST_ASSERT_EQUAL(BOOTLOADER_OK, bootloader_deinit());
}

static void testBootloadSlotApi(void)
{
  TEST_ASSERT_EQUAL(BOOTLOADER_OK, bootloader_init());

  int32_t setSlots[24]; // Support up to 24 slots for this test
  int32_t getSlots[24];

  BootloaderStorageInformation_t info;
  bootloader_getStorageInfo(&info);

  if (info.numStorageSlots > 1) {
    for (size_t i = 0; i < info.numStorageSlots; i++) {
      setSlots[i] = -1;
    }

    TEST_ASSERT_EQUAL(BOOTLOADER_OK,
                      bootloader_setImageToBootload(-1));

    TEST_ASSERT_EQUAL(BOOTLOADER_OK,
                      bootloader_getImagesToBootload(getSlots, info.numStorageSlots));

    TEST_ASSERT_EQUAL_INT32_ARRAY(setSlots, getSlots, info.numStorageSlots);
  }

  TEST_ASSERT_EQUAL(BOOTLOADER_OK,
                    bootloader_setImageToBootload(0));

  setSlots[0] = 0;

  TEST_ASSERT_EQUAL(BOOTLOADER_OK,
                    bootloader_getImagesToBootload(getSlots, info.numStorageSlots));

  TEST_ASSERT_EQUAL_INT32_ARRAY(setSlots, getSlots, info.numStorageSlots);

  if (info.numStorageSlots > 1) {
    TEST_ASSERT_EQUAL(BOOTLOADER_ERROR_BOOTLOAD_LIST_ENTRY_EXISTS,
                      bootloader_appendImageToBootloadList(0));

    for (size_t i = 0; i < info.numStorageSlots - 1; i++) {
      setSlots[i] = i;
    }
    setSlots[info.numStorageSlots - 1] = -1;

    TEST_ASSERT_EQUAL(BOOTLOADER_OK,
                      bootloader_setImagesToBootload(setSlots, info.numStorageSlots));

    TEST_ASSERT_EQUAL(BOOTLOADER_OK,
                      bootloader_getImagesToBootload(getSlots, info.numStorageSlots));

    TEST_ASSERT_EQUAL_INT32_ARRAY(setSlots, getSlots, info.numStorageSlots);

    TEST_ASSERT_EQUAL(BOOTLOADER_OK,
                      bootloader_appendImageToBootloadList(info.numStorageSlots - 1));
    setSlots[info.numStorageSlots - 1] = info.numStorageSlots - 1;

    TEST_ASSERT_EQUAL(BOOTLOADER_OK,
                      bootloader_getImagesToBootload(getSlots, info.numStorageSlots));

    TEST_ASSERT_EQUAL_INT32_ARRAY(setSlots, getSlots, info.numStorageSlots);

    TEST_ASSERT_EQUAL(BOOTLOADER_ERROR_BOOTLOAD_LIST_FULL,
                      bootloader_appendImageToBootloadList(info.numStorageSlots));

    TEST_ASSERT_EQUAL(BOOTLOADER_ERROR_BOOTLOAD_LIST_OVERFLOW,
                      bootloader_getImagesToBootload(getSlots, 24));

    TEST_ASSERT_EQUAL(BOOTLOADER_ERROR_BOOTLOAD_LIST_OVERFLOW,
                      bootloader_getImagesToBootload(getSlots, info.numStorageSlots + 1));
  }

  TEST_ASSERT_EQUAL(BOOTLOADER_OK, bootloader_deinit());
}

static void testVerifyImage(void)
{
  BootloaderInformation_t btlInfo;
  bootloader_getInfo(&btlInfo);

  TEST_ASSERT_EQUAL(true, bootloader_verifyApplication(BTL_APPLICATION_BASE));
}

int main(void)
{
  // CHIP_Init();
  sl_main_init();
  bootloader_init();

  UnityBegin("INTERFACE");
  TEST_PRINTF("App version %d\n", sl_app_properties.app.version);

  RUN_TEST(testInit, __LINE__);
  RUN_TEST(testGetInfo, __LINE__);

  BootloaderInformation_t btlInfo;
  bootloader_getInfo(&btlInfo);

  if (btlInfo.capabilities & BOOTLOADER_CAPABILITY_STORAGE) {
    RUN_TEST(testGetStorageInfo, __LINE__);
    RUN_TEST(testGetStorageSlotInfo, __LINE__);
    CYCLE_Init();
    uint32_t startTime = CYCLE_toMs(CYCLE_Count());
    RUN_TEST(testStorageSlotApi, __LINE__);
    uint32_t endTime = CYCLE_toMs(CYCLE_Count());
    TEST_PRINTF("The test testStorageSlotApi spent (ms):  %d \n", endTime - startTime);

    RUN_TEST(testBootloadSlotApi, __LINE__);
  }

  if (btlInfo.capabilities & BOOTLOADER_CAPABILITY_ENFORCE_SECURE_BOOT) {
    RUN_TEST(testVerifyImage, __LINE__);
  }

  UnityEnd();

  UnityPrint("ENDSWO");
  while (1) ;
}
