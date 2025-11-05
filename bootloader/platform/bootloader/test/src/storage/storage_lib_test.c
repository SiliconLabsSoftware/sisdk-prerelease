/***************************************************************************//**
 * @file storage_test.c
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
#include "sl_memory_manager.h"
#include "core/btl_core.h"
#include "core/btl_parse.h"
#include "storage/bootloadinfo/btl_storage_bootloadinfo.h"
#include "storage/btl_storage.h"
#include "test/src/common/fault.h"

#include "em_chip.h"
#include "em_core.h"

#include "ebl/app_test.gbl.c"

#include BTL_CONFIG_FILE

// Unity test framework
#include "unity.h"
#if defined(UNITY_OUTPUT_SERIAL)
#include "unity_usart.h"
#else
#include "unity_swo.h"
#endif

#include <string.h>
#include <stdlib.h>

#if ENABLE_TEST_COVERAGE
extern void __gcov_flush(void);
#endif

static uint32_t bootloaderUpgradeLocation = 0x8000;
static bool gotAppCallback = false;
static bool gotBLCallback = false;
static bool gotCommitSeUpgrade = false;
static bool gotCommitBLUpgrade = false;

#if defined(BOOTLOADER_INTERNAL_FLASH_UPGRADE_LOCATION_OVERLAP_TEST)
static const BootloaderStorageLayout_t storageLayout = {
  INTERNAL_FLASH,
  BTL_STORAGE_NUM_SLOTS,
  BTL_STORAGE_SLOTS
};
#endif

#ifdef __ICCARM__
__root
#endif
MainBootloaderTable_t mainBootloaderTableImpl;
MainBootloaderTable_t *mainBootloaderTable = &mainBootloaderTableImpl;

void bootload_bootloaderCallback(uint32_t offset,
                                 uint8_t  data[],
                                 size_t   length,
                                 void     *context)
{
  (void) context;
  (void) length;
  (void) data;
  (void) offset;
  gotBLCallback = true;
}

void bootload_applicationCallback(uint32_t address,
                                  uint8_t  data[],
                                  size_t   length,
                                  void     *context)
{
  (void) context;
  (void) length;
  (void) data;
  (void) address;
  gotAppCallback = true;
}

bool bootload_checkSeUpgradeVersion(uint32_t upgradeVersion)
{
  (void)upgradeVersion;
  return true;
}

bool bootload_commitSeUpgrade(uint32_t upgradeAddress)
{
  (void)upgradeAddress;
  gotCommitSeUpgrade = true;
  return true;
}

bool bootload_commitBootloaderUpgrade(uint32_t upgradeAddress, uint32_t size)
{
  (void)upgradeAddress;
  (void)size;
  gotCommitBLUpgrade = true;
  return true;
}

uint32_t bootload_getUpgradeLocation(void)
{
  return bootloaderUpgradeLocation;
}

static void testSlotImage(void)
{
  int32_t ret;
  ApplicationData_t appInfo;
  uint32_t bootloaderVersion;

  BootloaderParserContext_t parserContext;
  TEST_ASSERT_EQUAL(BOOTLOADER_OK, storage_initParseSlot(0, &parserContext, sizeof(BootloaderParserContext_t)));
  TEST_ASSERT_EQUAL(BOOTLOADER_OK, parserContext.errorCode);
  TEST_ASSERT(parserContext.slotOffset < parserContext.slotSize);
  do {
    ret = storage_verifySlot(&parserContext, NULL);
  } while (ret == BOOTLOADER_ERROR_PARSE_CONTINUE);
  TEST_ASSERT_EQUAL(BOOTLOADER_ERROR_PARSE_SUCCESS, ret);

  BootloaderParserContext_t parserContextTwo;
  TEST_ASSERT_EQUAL(BOOTLOADER_OK, storage_initParseSlot(0, &parserContextTwo, sizeof(BootloaderParserContext_t)));
  TEST_ASSERT_EQUAL(BOOTLOADER_OK, storage_getSlotMetadata(&parserContextTwo, &appInfo, &bootloaderVersion));
  TEST_ASSERT_EQUAL(BTL_IMAGE_CONTENT_APPLICATION, parserContextTwo.imageProperties.contents);
}

static void testBootloadFromSlot(void)
{
  TEST_ASSERT_FALSE(storage_bootloadBootloaderFromSlot(0, 0));
  TEST_ASSERT_TRUE(storage_bootloadBootloaderFromSlot(0, 0xFFFFFFFF));
  // The GBL stored in the flash does not contain any bootloader image.
  TEST_ASSERT_FALSE(gotAppCallback);
  TEST_ASSERT_FALSE(gotBLCallback);

  TEST_ASSERT_TRUE(storage_upgradeSeFromSlot(0));
  // No SE
  TEST_ASSERT_FALSE(gotAppCallback);
  TEST_ASSERT_FALSE(gotBLCallback);

  TEST_ASSERT_TRUE(storage_bootloadApplicationFromSlot(0, 0xFFFFFFFF, 0));
  TEST_ASSERT_TRUE(gotAppCallback);
  gotAppCallback = false;
  TEST_ASSERT_FALSE(gotBLCallback);
}

static void testStorageMain(void)
{
#if BTL_STORAGE_BOOTLOAD_LIST_LENGTH > 1
  TEST_ASSERT_EQUAL(BOOTLOADER_OK, storage_main());
  TEST_ASSERT_TRUE(gotAppCallback);
#else
  TEST_ASSERT_EQUAL(BOOTLOADER_OK, storage_main());
  TEST_ASSERT_TRUE(gotAppCallback);
  gotAppCallback = false;
#endif
  TEST_ASSERT_FALSE(gotBLCallback);
  TEST_ASSERT_FALSE(gotCommitSeUpgrade);
  TEST_ASSERT_FALSE(gotCommitBLUpgrade);
}

#if defined(BOOTLOADER_INTERNAL_FLASH_UPGRADE_LOCATION_OVERLAP_TEST)
static void testStorageUpgradeLocationOverlap(void)
{
  #if defined(_SILICON_LABS_32B_SERIES_2_CONFIG_1)
    #define UPGRADE_SIZE 0x0000C000UL
  #elif defined(_SILICON_LABS_32B_SERIES_2_CONFIG_2)
    #define UPGRADE_SIZE 0x00006000UL
  #elif (_SILICON_LABS_32B_SERIES_2_CONFIG >= 3)
    #define UPGRADE_SIZE 0x00018000UL
  #endif

  #define FLASH_PAGE_ALIGNED(x) ((((x) / FLASH_PAGE_SIZE) + 1) * FLASH_PAGE_SIZE)
  #define GBL_SIZE sizeof(gblFile)
  #define SLOT_ID 0
  #define DONT_CARE_BOOTLOADER_VERSION 0xFFFFFFFF

  // ------------------------------------ Positive checks ------------------------------------

  // Start with the valid address
  bootloaderUpgradeLocation = 0x8000;
  TEST_ASSERT_TRUE(storage_bootloadBootloaderFromSlot(SLOT_ID, DONT_CARE_BOOTLOADER_VERSION));

  // Staging area is just enough to fit the upgrade
  bootloaderUpgradeLocation = storageLayout.slot[SLOT_ID].address - UPGRADE_SIZE;
  TEST_ASSERT_TRUE(storage_bootloadBootloaderFromSlot(SLOT_ID, DONT_CARE_BOOTLOADER_VERSION));

  // Staging area is placed right after the GBL
  bootloaderUpgradeLocation = FLASH_PAGE_ALIGNED(storageLayout.slot[SLOT_ID].address + GBL_SIZE);
  TEST_ASSERT_TRUE(storage_bootloadBootloaderFromSlot(SLOT_ID, DONT_CARE_BOOTLOADER_VERSION));

  // ------------------------------------ Negative checks ------------------------------------

  // Staging area is equal the start address of the storage slot
  bootloaderUpgradeLocation = storageLayout.slot[SLOT_ID].address;
  TEST_ASSERT_FALSE(storage_bootloadBootloaderFromSlot(SLOT_ID, DONT_CARE_BOOTLOADER_VERSION));

  // Staging area is too small to fit the upgrade
  bootloaderUpgradeLocation = storageLayout.slot[SLOT_ID].address - (FLASH_PAGE_SIZE * 2u);
  TEST_ASSERT_FALSE(storage_bootloadBootloaderFromSlot(SLOT_ID, DONT_CARE_BOOTLOADER_VERSION));

  // Lower boundary check
  bootloaderUpgradeLocation = storageLayout.slot[SLOT_ID].address - UPGRADE_SIZE + 1u;
  TEST_ASSERT_FALSE(storage_bootloadBootloaderFromSlot(SLOT_ID, DONT_CARE_BOOTLOADER_VERSION));

  // Staging area is placed in the middle where the GBL resides
  bootloaderUpgradeLocation = FLASH_PAGE_ALIGNED(storageLayout.slot[SLOT_ID].address + GBL_SIZE / 2u);
  TEST_ASSERT_FALSE(storage_bootloadBootloaderFromSlot(SLOT_ID, DONT_CARE_BOOTLOADER_VERSION));

  // Upper boundary check
  bootloaderUpgradeLocation = FLASH_PAGE_ALIGNED(storageLayout.slot[SLOT_ID].address + GBL_SIZE) - (FLASH_PAGE_SIZE);
  TEST_ASSERT_FALSE(storage_bootloadBootloaderFromSlot(SLOT_ID, DONT_CARE_BOOTLOADER_VERSION));
}
#endif

int main(void)
{
  CHIP_Init();

#if defined(UNITY_OUTPUT_SERIAL)
  UNITY_UsartSetup();
#else
  SWO_Setup();
#endif
  faultInit();

  sl_memory_init();

  UnityBeginGroup("STORAGE");

  // Perform a runtime access into the array to ensure it survives optimizations
  uint32_t off = rand() % sizeof(gblFile) / 4;
  const uint32_t *arr = &gblFile[0];
  UnityPrintf("val = %08x\n", *(arr + off));
  UnityPrintf("Size of GBL: %08x\n", sizeof(gblFile));
  TEST_ASSERT_EQUAL(BOOTLOADER_OK, storage_eraseSlot(0));
  TEST_ASSERT_EQUAL(BOOTLOADER_OK,
                    storage_writeSlot(0, 0, (uint8_t *)gblFile, sizeof(gblFile)));

  RUN_TEST(testSlotImage, __LINE__);
  RUN_TEST(testBootloadFromSlot, __LINE__);
  RUN_TEST(testStorageMain, __LINE__);
#if defined(BOOTLOADER_INTERNAL_FLASH_UPGRADE_LOCATION_OVERLAP_TEST)
  RUN_TEST(testStorageUpgradeLocationOverlap, __LINE__);
#endif

  UnityEnd();

#if ENABLE_TEST_COVERAGE
  __gcov_flush();
#endif

  UnityPrint("ENDSWO");
  while (1) ;
}
