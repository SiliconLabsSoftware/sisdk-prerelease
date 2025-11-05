/***************************************************************************//**
 * @file crc_test.c
 * @brief Test bootloader CRC implementation
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

#include "em_chip.h"
#include "security/btl_crc32.h"
#include "security/btl_crc16.h"
#include "sl_memory_manager.h"

// Unity test framework
#include "unity.h"
#if defined(UNITY_TEST_REPORT_SERIAL)
#include "unity_usart.h"
#else
#include "unity_swo.h"
#endif

#include "em_common.h"

#include <string.h>

#if ENABLE_TEST_COVERAGE
extern void __gcov_flush(void);
#endif

static void testCrcBuffer(void)
{
  // Verify a test vector
  char *vector = "123456789";
  uint32_t crc = btl_crc32Stream((uint8_t *)vector, strlen(vector), BTL_CRC32_START);
  UnityPrintf("crc = %x\n", crc);
  TEST_ASSERT_EQUAL_HEX(~0xcbf43926, crc);

  // Bit-flip CRC result
  uint32_t crcInv = ~crc;

  // Append (bit-flipped) previous result to CRC, should get constant end value
  crc = btl_crc32Stream((uint8_t *)&crcInv, 4, crc);
  UnityPrintf("crc = %x\n", crc);
  TEST_ASSERT_EQUAL_HEX(BTL_CRC32_END, crc);
}

static void testCrc16Buffer(void)
{
  char *vector = "123456789";
  uint16_t crc = btl_crc16Stream((uint8_t *)vector, strlen(vector), 0xFFFF);
  UnityPrintf("crc = %x\n", crc);
  TEST_ASSERT_EQUAL_HEX(0x29b1, crc);

  crc = btl_crc16Stream((uint8_t *)vector, strlen(vector), 0x1D0F);
  UnityPrintf("crc = %x\n", crc);
  TEST_ASSERT_EQUAL_HEX(0xe5cc, crc);

  char *vectorTwo = "A";
  crc = btl_crc16Stream((uint8_t *)vectorTwo, strlen(vectorTwo), 0xFFFF);
  UnityPrintf("crc = %x\n", crc);
  TEST_ASSERT_EQUAL_HEX(0xb915, crc);
}

int main(void)
{
  CHIP_Init();

#if defined(UNITY_TEST_REPORT_SERIAL)
  UNITY_UsartSetup();
#else
  SWO_Setup();
#endif

  UnityBeginGroup("crc");
  sl_memory_init();

  RUN_TEST(testCrcBuffer, __LINE__);
  RUN_TEST(testCrc16Buffer, __LINE__);

  UnityEnd();

#if ENABLE_TEST_COVERAGE
  __gcov_flush();
#endif

  UnityPrint("ENDSWO");
  while (1) ;
}
