/***************************************************************************//**
 * @file interface_test.c
 * @brief Test bootloader interface
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

#if defined(SL_TRUSTZONE_NONSECURE)
extern uint32_t get_smu_ppusatd0(void);
extern uint32_t get_smu_ppusatd1(void);
#endif

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
#elif defined(_SILICON_LABS_GECKO_INTERNAL_SDID_210) || defined(_SILICON_LABS_GECKO_INTERNAL_SDID_215) || defined(_SILICON_LABS_GECKO_INTERNAL_SDID_220) \
  || defined(_SILICON_LABS_GECKO_INTERNAL_SDID_225) || defined(_SILICON_LABS_GECKO_INTERNAL_SDID_230) || defined(_SILICON_LABS_GECKO_INTERNAL_SDID_235)  \
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

#if defined(BOOTLOADER_INTERFACE_TRUSTZONE_AWARE)
static Bootloader_PPUSATDnCLKENnState_t ppusatdclkennState = { 0 };
#else
static uint32_t ppusatdclkennState;
#endif

static void ppusatdn_save_state(void *ctx)
{
#if defined(BOOTLOADER_INTERFACE_TRUSTZONE_AWARE)
  Bootloader_PPUSATDnCLKENnState_t *ctxTemp = (Bootloader_PPUSATDnCLKENnState_t *)ctx;

#if defined(_CMU_CLKEN1_SMU_MASK)
  CMU->CLKEN1_SET = CMU_CLKEN1_SMU;
#endif
#if defined(_CMU_CLKEN0_MASK)
  // Save the CLKENn states
  ctxTemp->CLKEN0 = CMU->CLKEN0;
  ctxTemp->CLKEN1 = CMU->CLKEN1;
#endif

  uint32_t smu_ppusatd0, smu_ppusatd1;
#if defined(SL_TRUSTZONE_NONSECURE)
  smu_ppusatd0 = get_smu_ppusatd0();
  smu_ppusatd1 = get_smu_ppusatd1();
#else
  smu_ppusatd0 = SMU->PPUSATD0;
  smu_ppusatd1 = SMU->PPUSATD1;
#endif // defined(SL_TRUSTZONE_NONSECURE)
  ctxTemp->PPUSATD0 = smu_ppusatd0;
  ctxTemp->PPUSATD1 = smu_ppusatd1;
#else
  (void)ctx;
#endif
}

static void ppusatdn_verify_state(void *ctx)
{
#if defined(BOOTLOADER_INTERFACE_TRUSTZONE_AWARE)
  Bootloader_PPUSATDnCLKENnState_t *ctxTemp = (Bootloader_PPUSATDnCLKENnState_t *)ctx;
#if defined(_CMU_CLKEN0_MASK)
  TEST_ASSERT_EQUAL_HEX32(ctxTemp->CLKEN0, CMU->CLKEN0);
  TEST_ASSERT_EQUAL_HEX32(ctxTemp->CLKEN1, CMU->CLKEN1);
#endif

  uint32_t smu_ppusatd0, smu_ppusatd1;
#if defined(SL_TRUSTZONE_NONSECURE)
  smu_ppusatd0 = get_smu_ppusatd0();
  smu_ppusatd1 = get_smu_ppusatd1();
#else
  smu_ppusatd0 = SMU->PPUSATD0;
  smu_ppusatd1 = SMU->PPUSATD1;
#endif // defined(SL_TRUSTZONE_NONSECURE)

  TEST_ASSERT_EQUAL_HEX32(ctxTemp->PPUSATD0, smu_ppusatd0);
  TEST_ASSERT_EQUAL_HEX32(ctxTemp->PPUSATD1, smu_ppusatd1);
#else
  (void)ctx;
#endif
}

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

void HardFault_Handler(void)
{
  UnityPrintf("FAULT\n          ");
  while (1) ;
}

static void testGetInfo(void)
{
  ppusatdn_save_state(&ppusatdclkennState);
  TEST_ASSERT_EQUAL(BOOTLOADER_OK, bootloader_init());
  ppusatdn_verify_state(&ppusatdclkennState);

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

  UnityPrintf("Bootloader version: v%08x\r\n", info.version);

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
    TEST_ASSERT_TRUE(info.capabilities & BOOTLOADER_CAPABILITY_GBL_SIGNATURE)
  }
  if (info.capabilities & BOOTLOADER_CAPABILITY_ENFORCE_UPGRADE_ENCRYPTION) {
    // Bootloader enforces encryption -- should support encryption
    TEST_ASSERT_TRUE(info.capabilities & BOOTLOADER_CAPABILITY_GBL_ENCRYPTION)
  }

#if defined(_SILICON_LABS_32B_SERIES_2)
  uint32_t certVersion = 999;
  TEST_ASSERT_FALSE(bootloader_getCertificateVersion(&certVersion));
#endif

  UnityPrintf("BootloaderParserContext size: %ld\n", bootloader_parserContextSize());
  TEST_ASSERT_TRUE(bootloader_parserContextSize() > 0UL);
  TEST_ASSERT_FALSE(BOOTLOADER_STORAGE_VERIFICATION_CONTEXT_SIZE < bootloader_parserContextSize());
}

static void testInit(void)
{
  ppusatdn_save_state(&ppusatdclkennState);
  TEST_ASSERT_EQUAL(BOOTLOADER_OK, bootloader_init());
  ppusatdn_verify_state(&ppusatdclkennState);

  ppusatdn_save_state(&ppusatdclkennState);
  TEST_ASSERT_EQUAL(BOOTLOADER_OK, bootloader_deinit());
  ppusatdn_verify_state(&ppusatdclkennState);
}

static void testGetStorageInfo(void)
{
  TEST_ASSERT_EQUAL(BOOTLOADER_OK, bootloader_init());

  BootloaderStorageInformation_t info = { 0 };

  ppusatdn_save_state(&ppusatdclkennState);
  bootloader_getStorageInfo(&info);
  ppusatdn_verify_state(&ppusatdclkennState);

  TEST_ASSERT_EQUAL(BOOTLOADER_OK, bootloader_deinit());

  TEST_ASSERT(info.numStorageSlots > 0);

  TEST_ASSERT_NOT_NULL(info.info);
}

static void testGetStorageSlotInfo(void)
{
  TEST_ASSERT_EQUAL(BOOTLOADER_OK, bootloader_init());

  BootloaderStorageInformation_t info;
  ppusatdn_save_state(&ppusatdclkennState);
  bootloader_getStorageInfo(&info);
  ppusatdn_verify_state(&ppusatdclkennState);

  TEST_ASSERT_EQUAL(BOOTLOADER_OK, bootloader_deinit());

  TEST_ASSERT(info.numStorageSlots > 0);

  BootloaderStorageSlot_t slot;

#if !defined(SL_TRUSTZONE_NONSECURE)
  // TZ case covered in interface_test_tz_param_validation
  // Invalid slot info pointer
  TEST_ASSERT_EQUAL(BOOTLOADER_ERROR_STORAGE_INVALID_SLOT,
                    bootloader_getStorageSlotInfo(0, NULL));
#endif
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

#if defined(_SILICON_LABS_32B_SERIES_2)
  if (info.storageType == INTERNAL_FLASH) {
    // Channel 2 is the default channel selected.
    TEST_ASSERT_EQUAL(2, bootloader_getAllocatedDMAChannel());
  } else {
    TEST_ASSERT_EQUAL(-1, bootloader_getAllocatedDMAChannel());
  }
#else
#if defined(TEST_DMA_MSC_INTERFACE)
  TEST_ASSERT_EQUAL(2, bootloader_getAllocatedDMAChannel());
#else
  TEST_ASSERT_EQUAL(-1, bootloader_getAllocatedDMAChannel());
#endif
#endif

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
  ppusatdn_save_state(&ppusatdclkennState);
  TEST_ASSERT_EQUAL(BOOTLOADER_OK,
                    bootloader_readStorage(0, 0, readData, sizeof(readData)));
  ppusatdn_verify_state(&ppusatdclkennState);

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
  ppusatdn_save_state(&ppusatdclkennState);
  TEST_ASSERT_EQUAL(BOOTLOADER_OK,
                    bootloader_eraseWriteStorage(0, 0, writeData, sizeof(writeData)));
  ppusatdn_verify_state(&ppusatdclkennState);

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
                    bootloader_eraseWriteStorage(0, info.info->pageSize, writeData, sizeof(writeData)));

  // Read data
  memset(readData, 0, sizeof(readData));
  TEST_ASSERT_EQUAL(BOOTLOADER_OK,
                    bootloader_readStorage(0, info.info->pageSize, readData, sizeof(readData)));
  TEST_ASSERT_EQUAL_UINT8_ARRAY(writeData, readData, 128);

  // Write data to slot 0 crossing the first and second page boundary
  TEST_ASSERT_EQUAL(BOOTLOADER_OK,
                    bootloader_eraseWriteStorage(0,
                                                 info.info->pageSize - sizeof(writeData) / 2,
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
                    bootloader_readStorage(0, info.info->pageSize, readData, sizeof(readData) / 2));
  TEST_ASSERT_EQUAL_UINT8_ARRAY(&writeData[64], readData, 64);

  // Write data to slot 0 to the end of the second page
  TEST_ASSERT_EQUAL(BOOTLOADER_OK,
                    bootloader_eraseWriteStorage(0,
                                                 info.info->pageSize * 2 - sizeof(writeData),
                                                 writeData,
                                                 sizeof(writeData)));
  memset(readData, 0, sizeof(readData));
  TEST_ASSERT_EQUAL(BOOTLOADER_OK,
                    bootloader_readStorage(0,
                                           info.info->pageSize * 2 - sizeof(readData),
                                           readData,
                                           sizeof(readData)));
  TEST_ASSERT_EQUAL_UINT8_ARRAY(writeData, readData, 128);

  // Erase slot 0
  ppusatdn_save_state(&ppusatdclkennState);
  TEST_ASSERT_EQUAL(BOOTLOADER_OK, bootloader_eraseStorageSlot(0));
  ppusatdn_verify_state(&ppusatdclkennState);

  // Fill the first five pages in storage slot 0 with dummy data
  for (uint32_t i = 0; i < info.info->pageSize * 5 / 128; i++) {
    ppusatdn_save_state(&ppusatdclkennState);
    TEST_ASSERT_EQUAL(BOOTLOADER_OK,
                      bootloader_writeStorage(0, i * 128, writeData, sizeof(writeData)));
    ppusatdn_verify_state(&ppusatdclkennState);
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
                                           info.info->pageSize * 2 + 512,
                                           readData,
                                           sizeof(readData)));

  // Compare data; readData should contain only FF's
  TEST_ASSERT_EQUAL_UINT8_ARRAY(writeData, readData, 128);

  // Read data from slot 0 from a random memory space inside page 5
  memset(readData, 0, sizeof(readData));
  TEST_ASSERT_EQUAL(BOOTLOADER_OK,
                    bootloader_readStorage(0,
                                           info.info->pageSize * 4 + 1024,
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

  ppusatdn_save_state(&ppusatdclkennState);
  TEST_ASSERT_EQUAL(BOOTLOADER_OK,
                    bootloader_setImageToBootload(0));
  ppusatdn_verify_state(&ppusatdclkennState);

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

    ppusatdn_save_state(&ppusatdclkennState);
    TEST_ASSERT_EQUAL(BOOTLOADER_OK,
                      bootloader_setImagesToBootload(setSlots, info.numStorageSlots));
    ppusatdn_verify_state(&ppusatdclkennState);

    ppusatdn_save_state(&ppusatdclkennState);
    TEST_ASSERT_EQUAL(BOOTLOADER_OK,
                      bootloader_getImagesToBootload(getSlots, info.numStorageSlots));
    ppusatdn_verify_state(&ppusatdclkennState);

    TEST_ASSERT_EQUAL_INT32_ARRAY(setSlots, getSlots, info.numStorageSlots);

    ppusatdn_save_state(&ppusatdclkennState);
    TEST_ASSERT_EQUAL(BOOTLOADER_OK,
                      bootloader_appendImageToBootloadList(info.numStorageSlots - 1));
    ppusatdn_verify_state(&ppusatdclkennState);
    setSlots[info.numStorageSlots - 1] = info.numStorageSlots - 1;

    ppusatdn_save_state(&ppusatdclkennState);
    TEST_ASSERT_EQUAL(BOOTLOADER_OK,
                      bootloader_getImagesToBootload(getSlots, info.numStorageSlots));
    ppusatdn_verify_state(&ppusatdclkennState);

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

  ppusatdn_save_state(&ppusatdclkennState);
  TEST_ASSERT_EQUAL(true, bootloader_verifyApplication(BTL_APPLICATION_BASE));
  ppusatdn_verify_state(&ppusatdclkennState);
}

#if !defined(_SILICON_LABS_GECKO_INTERNAL_SDID_80)
static void testRollbackPrevention(void)
{
  // 6 slots.
  uint32_t nrSlots = 6UL;

  uint32_t remainingUpgrades = bootloader_remainingApplicationUpgrades();
#if defined(TEST_MAXIMUM_APP_VERSION)
  // Maximum version is seen.
  TEST_ASSERT_EQUAL(0, remainingUpgrades);

  uint32_t endOfBLpage = BTL_FIRST_STAGE_BASE + BTL_FIRST_STAGE_SIZE + BTL_MAIN_STAGE_MAX_SIZE;
  uint32_t *savedAppVersion = (uint32_t*)endOfBLpage - (nrSlots + 1UL);
  // Check application maximum version magic
  TEST_ASSERT_EQUAL(0x1234DCBAUL, *savedAppVersion);
#else
  // Reserved flash for nrSlots upgrades and one of them is already written when booting this
  // test appication.
  TEST_ASSERT_EQUAL(nrSlots - 1, remainingUpgrades);

  uint32_t endOfBLpage = BTL_FIRST_STAGE_BASE + BTL_FIRST_STAGE_SIZE + BTL_MAIN_STAGE_MAX_SIZE;
  uint32_t *savedAppVersion = (uint32_t*)endOfBLpage - 1UL;
  TEST_ASSERT_EQUAL(sl_app_properties.app.version, *savedAppVersion);
#endif
}
#endif

void testUpgradeLocation(void)
{
  uint32_t upgradeLocation;
  bool ret = bootloader_getUpgradeLocation(&upgradeLocation);
  TEST_ASSERT_EQUAL(true, ret);
  TEST_ASSERT_EQUAL((0x8000 + FLASH_BASE), upgradeLocation);
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
  UnityPrintf("App version %08x\n", sl_app_properties.app.version);

  RUN_TEST(testInit, __LINE__);
  RUN_TEST(testGetInfo, __LINE__);
  RUN_TEST(testUpgradeLocation, __LINE__);

  BootloaderInformation_t btlInfo;
  bootloader_getInfo(&btlInfo);

  if (btlInfo.capabilities & BOOTLOADER_CAPABILITY_STORAGE) {
    RUN_TEST(testGetStorageInfo, __LINE__);
    RUN_TEST(testGetStorageSlotInfo, __LINE__);
    CYCLE_Init();
    uint32_t startTime = CYCLE_toMs(CYCLE_Count());
    RUN_TEST(testStorageSlotApi, __LINE__);
    uint32_t endTime = CYCLE_toMs(CYCLE_Count());
    UnityPrintf("The test testStorageSlotApi spent (ms):  %d \n", endTime - startTime);

    RUN_TEST(testBootloadSlotApi, __LINE__);
  }

  if (btlInfo.capabilities & BOOTLOADER_CAPABILITY_ENFORCE_SECURE_BOOT) {
    RUN_TEST(testVerifyImage, __LINE__);
  }

#if !defined(_SILICON_LABS_GECKO_INTERNAL_SDID_80)
  if (btlInfo.capabilities & BOOTLOADER_CAPABILITY_ROLLBACK_PROTECTION) {
    RUN_TEST(testRollbackPrevention, __LINE__);
  }
#endif

  UnityEnd();

  UnityPrint("ENDSWO");
  while (1) ;
}
