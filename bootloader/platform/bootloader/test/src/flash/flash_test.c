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

#include "core/flash/btl_internal_flash.h"

#include "em_chip.h"
#include "em_cmu.h"
#include "em_common.h"

// Unity test framework
#include "unity.h"
#if defined(UNITY_OUTPUT_SERIAL)
#include "unity_usart.h"
#else
#include "unity_swo.h"
#endif

#if defined(_MSC_STATUS_REGLOCK_MASK)
#define MSC_IS_LOCKED()    ((MSC->STATUS & _MSC_STATUS_REGLOCK_MASK) != 0U)
#else
#define MSC_IS_LOCKED()    ((MSC->LOCK & _MSC_LOCK_MASK) != 0U)
#endif

SL_ALIGN(4)
static const uint8_t testWordsWriteBuffer[256] SL_ATTRIBUTE_ALIGN(4) = {
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF
};

SL_ALIGN(4)
static const uint8_t testWordsLDMAWriteBuffer[256] SL_ATTRIBUTE_ALIGN(4) = {
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF
};

#if ENABLE_TEST_COVERAGE
extern void __gcov_flush(void);
#endif

static void lockFlash(void)
{
#if !defined(_SILICON_LABS_32B_SERIES_2)
  // Disable writing to the flash
  MSC->WRITECTRL &= ~MSC_WRITECTRL_WREN;

  // Lock the MSC
  MSC->LOCK = 0UL;
#else
  // Unlock MSC
  MSC->LOCK = MSC_LOCK_LOCKKEY_UNLOCK;
  // Disable flash write
  MSC->WRITECTRL_CLR = MSC_WRITECTRL_WREN;
  // Lock MSC
  MSC->LOCK = MSC_LOCK_LOCKKEY_LOCK;
#endif
}

static void unlockFlash(void)
{
#if !defined(_SILICON_LABS_32B_SERIES_2)
  // Unlock the MSC
  MSC->LOCK = (uint32_t)MSC_UNLOCK_CODE;

  // Disable writing to the flash
  MSC->WRITECTRL &= ~MSC_WRITECTRL_WREN;
#else
  // Unlock MSC
  MSC->LOCK = MSC_LOCK_LOCKKEY_UNLOCK;
  // Disable flash write
  MSC->WRITECTRL_CLR = MSC_WRITECTRL_WREN;
#endif
}

static void testFlashWriteBuffer(void)
{
  bool wasLocked;
  uint32_t testWordsAddress = (uint32_t)testWordsWriteBuffer;
  UnityPrintf("Ptr %x\n", testWordsAddress);

  uint8_t dataArray[12] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12 };

#if defined(_SILICON_LABS_32B_SERIES_2)
  uint8_t dummyArray[12] = {
    0xFFU, 0xFFU, 0xFFU, 0xFFU,
    0xFFU, 0xFFU, 0xFFU, 0xFFU,
    0xFFU, 0xFFU, 0xFFU, 0xFFU,
  };
  // Length 0
  wasLocked = MSC_IS_LOCKED();
  TEST_ASSERT_TRUE(flash_writeBuffer(testWordsAddress, dataArray, 0));
  TEST_ASSERT_EQUAL(wasLocked, MSC_IS_LOCKED());

  // End unaligned
  wasLocked = MSC_IS_LOCKED();
  TEST_ASSERT_FALSE(flash_writeBuffer(testWordsAddress, dataArray, 2));
  TEST_ASSERT_EQUAL_UINT8_ARRAY(&dummyArray[0], &testWordsWriteBuffer[0], 2);
  TEST_ASSERT_EQUAL(wasLocked, MSC_IS_LOCKED());

  // Start unaligned
  lockFlash();
  wasLocked = MSC_IS_LOCKED();
  TEST_ASSERT_EQUAL(true, wasLocked);
  TEST_ASSERT_TRUE(flash_writeBuffer(testWordsAddress + 4, &dataArray[4], 8));
  TEST_ASSERT_EQUAL_UINT8_ARRAY(&dataArray[4], &testWordsWriteBuffer[4], 6);
  TEST_ASSERT_EQUAL(wasLocked, MSC_IS_LOCKED());

  // Both unaligned
  wasLocked = MSC_IS_LOCKED();
  TEST_ASSERT_EQUAL(true, wasLocked);
  TEST_ASSERT_FALSE(flash_writeBuffer(testWordsAddress + 14, &dataArray[2], 8));
  TEST_ASSERT_EQUAL_UINT8_ARRAY(&dummyArray[2], &testWordsWriteBuffer[14], 8);
  TEST_ASSERT_EQUAL(wasLocked, MSC_IS_LOCKED());
#else
  // Length 0
  wasLocked = MSC_IS_LOCKED();
  TEST_ASSERT_TRUE(flash_writeBuffer(testWordsAddress, dataArray, 0));
  TEST_ASSERT_EQUAL(wasLocked, MSC_IS_LOCKED());
  // Length 1
  wasLocked = MSC_IS_LOCKED();
  TEST_ASSERT_FALSE(flash_writeBuffer(testWordsAddress, dataArray, 1));
  TEST_ASSERT_EQUAL(wasLocked, MSC_IS_LOCKED());

  // End unaligned
  wasLocked = MSC_IS_LOCKED();
  TEST_ASSERT_TRUE(flash_writeBuffer(testWordsAddress, dataArray, 2));
  TEST_ASSERT_EQUAL_UINT8_ARRAY(&dataArray[0], &testWordsWriteBuffer[0], 2);
  TEST_ASSERT_EQUAL(wasLocked, MSC_IS_LOCKED());

  // Start unaligned
  lockFlash();
  wasLocked = MSC_IS_LOCKED();
  TEST_ASSERT_EQUAL(true, wasLocked);
  TEST_ASSERT_TRUE(flash_writeBuffer(testWordsAddress + 6, &dataArray[2], 6));
  TEST_ASSERT_EQUAL_UINT8_ARRAY(&dataArray[2], &testWordsWriteBuffer[6], 6);
  TEST_ASSERT_EQUAL(wasLocked, MSC_IS_LOCKED());

  // Both unaligned
  wasLocked = MSC_IS_LOCKED();
  TEST_ASSERT_EQUAL(true, wasLocked);
  TEST_ASSERT_TRUE(flash_writeBuffer(testWordsAddress + 14, &dataArray[2], 8));
  TEST_ASSERT_EQUAL_UINT8_ARRAY(&dataArray[2], &testWordsWriteBuffer[14], 8);
  TEST_ASSERT_EQUAL(wasLocked, MSC_IS_LOCKED());
#endif
  unlockFlash();
  uint8_t dataArray2[256];
  for (size_t i = 0; i < sizeof(dataArray2); i++) {
    dataArray2[i] = i;
  }

  // Large write (aligned)
  wasLocked = MSC_IS_LOCKED();
  TEST_ASSERT_EQUAL(false, wasLocked);
  TEST_ASSERT_TRUE(flash_writeBuffer(testWordsAddress + 24, &dataArray2[24], 232));
  TEST_ASSERT_EQUAL_UINT8_ARRAY(&dataArray2[24], &testWordsWriteBuffer[24], 232);
  TEST_ASSERT_EQUAL(wasLocked, MSC_IS_LOCKED());
}

static void testFlashLDMAWriteBuffer(void)
{
  const int dmaCh = 2;

  bool wasLocked;
  uint32_t testWordsAddress = (uint32_t)testWordsLDMAWriteBuffer;
  UnityPrintf("Ptr %x\n", testWordsAddress);

  uint8_t dataArray[12] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12 };

#if defined(_SILICON_LABS_32B_SERIES_2)
  uint8_t dummyArray[12] = {
    0xFFU, 0xFFU, 0xFFU, 0xFFU,
    0xFFU, 0xFFU, 0xFFU, 0xFFU,
    0xFFU, 0xFFU, 0xFFU, 0xFFU,
  };

  // Invalid channel test
  TEST_ASSERT_FALSE(flash_writeBuffer_dma(testWordsAddress, dataArray, 0, -1));

  // Length 0
  wasLocked = MSC_IS_LOCKED();
  TEST_ASSERT_TRUE(flash_writeBuffer_dma(testWordsAddress, dataArray, 0, dmaCh));
  TEST_ASSERT_EQUAL(wasLocked, MSC_IS_LOCKED());

  // End unaligned
  wasLocked = MSC_IS_LOCKED();
  TEST_ASSERT_FALSE(flash_writeBuffer_dma(testWordsAddress, dataArray, 2, dmaCh));
  TEST_ASSERT_EQUAL_UINT8_ARRAY(&dummyArray[0], &testWordsLDMAWriteBuffer[0], 2);
  TEST_ASSERT_EQUAL(wasLocked, MSC_IS_LOCKED());

  // Start unaligned
  lockFlash();
  wasLocked = MSC_IS_LOCKED();
  TEST_ASSERT_EQUAL(true, wasLocked);
  TEST_ASSERT_TRUE(flash_writeBuffer_dma(testWordsAddress + 4, &dataArray[4], 8, dmaCh));
  TEST_ASSERT_EQUAL_UINT8_ARRAY(&dataArray[4], &testWordsLDMAWriteBuffer[4], 6);
  TEST_ASSERT_EQUAL(wasLocked, MSC_IS_LOCKED());

  // Both unaligned
  wasLocked = MSC_IS_LOCKED();
  TEST_ASSERT_EQUAL(true, wasLocked);
  TEST_ASSERT_FALSE(flash_writeBuffer_dma(testWordsAddress + 14, &dataArray[2], 8, dmaCh));
  TEST_ASSERT_EQUAL_UINT8_ARRAY(&dummyArray[2], &testWordsLDMAWriteBuffer[14], 8);
  TEST_ASSERT_EQUAL(wasLocked, MSC_IS_LOCKED());
#else
  // Length 0
  wasLocked = MSC_IS_LOCKED();
  TEST_ASSERT_TRUE(flash_writeBuffer_dma(testWordsAddress, dataArray, 0, dmaCh));
  TEST_ASSERT_EQUAL(wasLocked, MSC_IS_LOCKED());
  // Length 1
  wasLocked = MSC_IS_LOCKED();
  TEST_ASSERT_FALSE(flash_writeBuffer_dma(testWordsAddress, dataArray, 1, dmaCh));
  TEST_ASSERT_EQUAL(wasLocked, MSC_IS_LOCKED());

  // End unaligned
  wasLocked = MSC_IS_LOCKED();
  TEST_ASSERT_TRUE(flash_writeBuffer_dma(testWordsAddress, dataArray, 2, dmaCh));
  TEST_ASSERT_EQUAL_UINT8_ARRAY(&dataArray[0], &testWordsLDMAWriteBuffer[0], 2);
  TEST_ASSERT_EQUAL(wasLocked, MSC_IS_LOCKED());

  // Start unaligned
  lockFlash();
  wasLocked = MSC_IS_LOCKED();
  TEST_ASSERT_EQUAL(true, wasLocked);
  TEST_ASSERT_TRUE(flash_writeBuffer_dma(testWordsAddress + 6, &dataArray[2], 6, dmaCh));
  TEST_ASSERT_EQUAL_UINT8_ARRAY(&dataArray[2], &testWordsLDMAWriteBuffer[6], 6);
  TEST_ASSERT_EQUAL(wasLocked, MSC_IS_LOCKED());

  // Both unaligned
  wasLocked = MSC_IS_LOCKED();
  TEST_ASSERT_EQUAL(true, wasLocked);
  TEST_ASSERT_TRUE(flash_writeBuffer_dma(testWordsAddress + 14, &dataArray[2], 8, dmaCh));
  TEST_ASSERT_EQUAL_UINT8_ARRAY(&dataArray[2], &testWordsLDMAWriteBuffer[14], 8);
  TEST_ASSERT_EQUAL(wasLocked, MSC_IS_LOCKED());
#endif
  unlockFlash();
  uint8_t dataArray2[256];
  for (size_t i = 0; i < sizeof(dataArray2); i++) {
    dataArray2[i] = i;
  }

  // Large write (aligned)
  wasLocked = MSC_IS_LOCKED();
  TEST_ASSERT_EQUAL(false, wasLocked);
  TEST_ASSERT_TRUE(flash_writeBuffer_dma(testWordsAddress + 24, &dataArray2[24], 232, dmaCh));
  TEST_ASSERT_EQUAL_UINT8_ARRAY(&dataArray2[24], &testWordsLDMAWriteBuffer[24], 232);
  TEST_ASSERT_EQUAL(wasLocked, MSC_IS_LOCKED());
}

void testFlashErasePage(void)
{
  bool wasLocked;
  // Addr of the last flash page.
  uint32_t eraseOffset = FLASH_BASE + FLASH_SIZE - FLASH_PAGE_SIZE;
  UnityPrintf("Ptr %x\n", eraseOffset);

  uint8_t dataArray[256];
  for (size_t i = 0; i < sizeof(dataArray); i++) {
    dataArray[i] = i;
  }
  wasLocked = MSC_IS_LOCKED();
  TEST_ASSERT_TRUE(flash_writeBuffer(eraseOffset, dataArray, 256));
  TEST_ASSERT_EQUAL_UINT8_ARRAY(dataArray, (char *)eraseOffset, 256);
  TEST_ASSERT_EQUAL(wasLocked, MSC_IS_LOCKED());

  for (size_t i = 0; i < sizeof(dataArray); i++) {
    dataArray[i] = 0xFF;
  }
  wasLocked = MSC_IS_LOCKED();
  TEST_ASSERT_TRUE(flash_erasePage(eraseOffset));
  TEST_ASSERT_EQUAL_UINT8_ARRAY(dataArray, (char *)eraseOffset, 256);
  TEST_ASSERT_EQUAL(wasLocked, MSC_IS_LOCKED());
}

int main(void)
{
  CHIP_Init();

#if defined(UNITY_OUTPUT_SERIAL)
  UNITY_UsartSetup();
#else
  SWO_Setup();
#endif

  UnityBeginGroup("FLASH");

#if defined(_CMU_CLKEN1_MASK)
  CMU->CLKEN1_SET = CMU_CLKEN1_MSC;
#endif

  RUN_TEST(testFlashWriteBuffer, __LINE__);
  RUN_TEST(testFlashLDMAWriteBuffer, __LINE__);
  RUN_TEST(testFlashErasePage, __LINE__);

  UnityEnd();

#if ENABLE_TEST_COVERAGE
  __gcov_flush();
#endif

  UnityPrint("ENDSWO");
  while (1) ;
}
