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

#include "debug/btl_debug.h"
#include "sl_main_init.h"

// Unity test framework
#include "unity.h"

#include <string.h>
#include <stdlib.h>
#include <inttypes.h>

static void testHostSync(void)
{
  uint8_t str[] = "{{sync}}";
  btl_debugWriteLine((const char *)str);
}

static void testHostEnd(void)
{
  uint8_t str[] = "{{end}}";

  btl_debugWriteLine((const char *)str);
}

static void testEndSwo(void)
{
  uint8_t str[] = "ENDSWO";

  btl_debugWriteLine((const char *)str);
}

static void testDebugWriteLine(void)
{
  const char str[] = "debugWriteLine";
  btl_debugWriteLine(str);
}

static void testDebugWriteWordHex(void)
{
  uint32_t wordHex = 0x10000000;
  btl_debugWriteWordHex(wordHex);
  btl_debugWriteString(BTL_DEBUG_NEWLINE);
  wordHex = 0x11000000;
  btl_debugWriteWordHex(wordHex);
  btl_debugWriteString(BTL_DEBUG_NEWLINE);
  wordHex = 0x11100000;
  btl_debugWriteWordHex(wordHex);
  btl_debugWriteString(BTL_DEBUG_NEWLINE);
  wordHex = 0x11110000;
  btl_debugWriteWordHex(wordHex);
  btl_debugWriteString(BTL_DEBUG_NEWLINE);
  wordHex = 0x11111000;
  btl_debugWriteWordHex(wordHex);
  btl_debugWriteString(BTL_DEBUG_NEWLINE);
  wordHex = 0x11111100;
  btl_debugWriteWordHex(wordHex);
  btl_debugWriteString(BTL_DEBUG_NEWLINE);
  wordHex = 0x11111110;
  btl_debugWriteWordHex(wordHex);
  btl_debugWriteString(BTL_DEBUG_NEWLINE);
  wordHex = 0x11111111;
  btl_debugWriteWordHex(wordHex);
  btl_debugWriteString(BTL_DEBUG_NEWLINE);
}

static void testDebugWriteInt(void)
{
  int intVal = 10;
  btl_debugWriteInt(intVal);
  btl_debugWriteString(BTL_DEBUG_NEWLINE);
  intVal = 100;
  btl_debugWriteInt(intVal);
  btl_debugWriteString(BTL_DEBUG_NEWLINE);
  intVal = -1000;
  btl_debugWriteInt(intVal);
  btl_debugWriteString(BTL_DEBUG_NEWLINE);
  intVal = 10000;
  btl_debugWriteInt(intVal);
  btl_debugWriteString(BTL_DEBUG_NEWLINE);
  intVal = -100000;
  btl_debugWriteInt(intVal);
  btl_debugWriteString(BTL_DEBUG_NEWLINE);
}

int main(void)
{
  // CHIP_Init();
  sl_main_init();

#if defined(_CMU_CLKEN0_HFRCOEM23_MASK)
  CMU->CLKEN0_SET = CMU_CLKEN0_HFRCOEM23;
#endif

  // btl_debugInit
  BTL_DEBUG_INIT();

  UnityBegin("DEBUG");

  RUN_TEST(testDebugWriteLine, __LINE__);
  RUN_TEST(testDebugWriteWordHex, __LINE__);
  RUN_TEST(testDebugWriteInt, __LINE__);

  UnityEnd();

  testHostEnd();
  testHostSync();

  // Without these lines, this test does not get finished (will get a timeout message).
  // Meaning that the function call to btl_debugWriteLine did not do anything.
  testHostSync();
  testEndSwo();
  while (1) ;
}
