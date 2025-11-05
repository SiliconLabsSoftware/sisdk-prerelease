/***************************************************************************//**
 * @file crc_test.c
 * @brief Test bootloader CRC implementation
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

#include "security/btl_crc32.h"
#include "security/btl_crc16.h"
#include "sl_memory_manager.h"
#include "sl_main_init.h"

// Unity test framework
#include "unity.h"

#include <string.h>

static void testCrcBuffer(void)
{
  // Verify a test vector
  char *vector = "123456789";
  uint32_t crc = btl_crc32Stream((uint8_t *)vector, strlen(vector), BTL_CRC32_START);
  TEST_PRINTF("crc = %x\n", crc);
  TEST_ASSERT_EQUAL_HEX(~0xcbf43926, crc);

  // Bit-flip CRC result
  uint32_t crcInv = ~crc;

  // Append (bit-flipped) previous result to CRC, should get constant end value
  crc = btl_crc32Stream((uint8_t *)&crcInv, 4, crc);
  TEST_PRINTF("crc = %x\n", crc);
  TEST_ASSERT_EQUAL_HEX(BTL_CRC32_END, crc);
}

static void testCrc16Buffer(void)
{
  char *vector = "123456789";
  uint16_t crc = btl_crc16Stream((uint8_t *)vector, strlen(vector), 0xFFFF);
  TEST_PRINTF("crc = %x\n", crc);
  TEST_ASSERT_EQUAL_HEX(0x29b1, crc);

  crc = btl_crc16Stream((uint8_t *)vector, strlen(vector), 0x1D0F);
  TEST_PRINTF("crc = %x\n", crc);
  TEST_ASSERT_EQUAL_HEX(0xe5cc, crc);

  char *vectorTwo = "A";
  crc = btl_crc16Stream((uint8_t *)vectorTwo, strlen(vectorTwo), 0xFFFF);
  TEST_PRINTF("crc = %x\n", crc);
  TEST_ASSERT_EQUAL_HEX(0xb915, crc);
}

int main(void)
{
  // CHIP_Init();
  sl_main_init();

  UnityBegin("crc");
  sl_memory_init();

  RUN_TEST(testCrcBuffer, __LINE__);
  RUN_TEST(testCrc16Buffer, __LINE__);

  UnityEnd();

  UnityPrint("ENDSWO");
  while (1) ;
}
