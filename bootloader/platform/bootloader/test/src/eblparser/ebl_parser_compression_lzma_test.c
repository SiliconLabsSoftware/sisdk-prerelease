/***************************************************************************//**
 * @file btl_second_stage.c
 * @brief Main file for Second Stage Bootloader.
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

#include "em_device.h"
#include "em_chip.h"
#include "em_cmu.h"
#include "em_gpio.h"
#include "em_common.h"
#include "sl_memory_manager.h"

#include "core/btl_parse.h"
#if defined(FLASH_BUFFER_BASE)
#include "core/btl_bootload.h"
#ifdef __ICCARM__
__root
#endif
#endif

MainBootloaderTable_t mainBootloaderTableImpl;
MainBootloaderTable_t *mainBootloaderTable = &mainBootloaderTableImpl;

#if defined(BOOTLOADER_HAS_FIRST_STAGE)
FirstBootloaderTable_t firstBootloaderTableImpl;
FirstBootloaderTable_t *firstBootloaderTable = &firstBootloaderTableImpl;
#endif

// Get EBL array
//#include "ebl/testv2.c"
//#include "ebl/testv3Encrypted.c"
#if defined(TEST_COMPRESSION_LZ4)
#include "ebl/compressed-gbl-binary2.c"
#include "ebl/compressed-raw-binary2.c"
#elif defined(TEST_COMPRESSION_LZ4_AND_ENCRYPTION)
#include "ebl/compressed-gbl-binary3.c"
#include "ebl/compressed-raw-binary2.c"
#elif defined(TEST_COMPRESSION_LZMA)
#include "ebl/compressed-gbl-binary4.c"
#include "ebl/compressed-raw-binary2.c"
#elif defined(TEST_COMPRESSION_LZMA_ODD_LENGTH)
#include "ebl/compressed-gbl-binary6.c"
#include "ebl/compressed-raw-binary6.c"
#elif defined(TEST_COMPRESSION_LZMA_CHUNKED)
#include "ebl/compressed-gbl-binary7.c"
#include "ebl/compressed-raw-binary6.c"
#elif defined(TEST_COMPRESSION_LZMA_8K_LP1_LC1)
#include "ebl/compressed-gbl-binary8.c"
#include "ebl/compressed-raw-binary2.c"
#elif defined(TEST_COMPRESSION_LZMA_LONG)
#include "ebl/test_app_lzma_gbl.c"
#include "ebl/test_app_lzma_raw.c"
#elif defined(TEST_COMPRESSION_LZMA_PSEC_2111)
#include "ebl/test_lzma_psec_2111.gbl.c"
#include "ebl/test_lzma_psec_2111_raw.c"
#endif

#include "parser/gbl/btl_gbl_parser.h"
#include "security/btl_security_types.h"

bool parser_applicationUpgradeValidCallback(ApplicationData_t *app)
{
  (void) app;
  // By default, all applications are considered valid
  return true;
}

// Get size_t
#include <stddef.h>

#include <string.h>
#include <stdio.h>
// Unity test framework
#include "unity.h"
#if defined(UNITY_OUTPUT_SERIAL)
#include "unity_usart.h"
#else
#include "unity_swo.h"
#endif

#if ENABLE_TEST_COVERAGE
extern void __gcov_flush(void);
#endif

char stringBuffer[48];
#if !defined(FLASH_BUFFER_BASE)
uint8_t parserBuffer[PARSER_BUFFER_SIZE];
#endif

void HardFault_Handler(void)
{
  UnityPrintf("FAULT\n");
  while (1) {
    // Do nothing
  }
}

// Implement parser callbacks
void compressedParseCallback(uint32_t offset, uint8_t *buffer, size_t length, void *ctx)
{
  uint32_t appPcAddress;
  uint32_t startWithhold;
  uint32_t withholdSrcOffset;
  uint32_t appBase;

  // Should always write word size chunks
  TEST_ASSERT_EQUAL(0, length % 4);
  appBase = (uint32_t) mainBootloaderTable->startOfAppSpace;
  // Initial PC or reset vector should be written to the flash at the end when entire
  // EBL is validated.
  ParserContext_t *parserContext = (ParserContext_t *)ctx;
  if (parserContext->internalState == GblParserStateFinalize) {
    if (offset == (appBase + 8UL)) {
      // Reset-vector
      TEST_ASSERT_EQUAL_HEX8_ARRAY(&compressedRawData[offset], buffer, 20UL);
    } else if (offset == (appBase + 4UL)) {
      // Initial PC
      TEST_ASSERT_EQUAL_HEX8_ARRAY(&compressedRawData[offset], buffer, 4UL);
    }
  } else {
    appPcAddress = appBase + 4UL;
    if ((offset <= appPcAddress) && ((offset + length) > appPcAddress)) {
      startWithhold = (offset > appPcAddress) ? offset : appPcAddress;
      withholdSrcOffset = startWithhold - offset;
      TEST_ASSERT_EQUAL_HEX8(0xFFUL, buffer[withholdSrcOffset]);
      TEST_ASSERT_EQUAL_HEX8(0xFFUL, buffer[withholdSrcOffset + 1]);
      TEST_ASSERT_EQUAL_HEX8(0xFFUL, buffer[withholdSrcOffset + 2]);
      TEST_ASSERT_EQUAL_HEX8(0xFFUL, buffer[withholdSrcOffset + 3]);
    }
  }

#if defined(FLASH_BUFFER_BASE)
  bootload_applicationCallback(offset + FLASH_BUFFER_BASE, buffer, length, ctx);
#else
  memcpy(&parserBuffer[offset - appBase], buffer, length);
#endif
}

void testGblParserCompressed(void)
{
  // Test the EBL parser with a file that has been compiled in
  size_t offset = 0;
  const size_t chunkSize = 64U;
  size_t remaining = 0;
  int32_t retval = 0;
  ParserContext_t parserContext;
  DecryptContext_t decryptContext = { 0 };
  AuthContext_t authContext;
  const BootloaderParserCallbacks_t parserCallbacks = {
    (void*)&parserContext,
    compressedParseCallback,
    compressedParseCallback,
    compressedParseCallback
  };
  ImageProperties_t imageProperties = { 0 };

  retval = parser_init(&parserContext,
                       &decryptContext,
                       &authContext,
                       PARSER_FLAG_PARSE_CUSTOM_TAGS);
  UnityPrintf("Init = %0x\n", retval);

  while (offset < sizeof(compressedGblFile)) {
    remaining = SL_MIN(chunkSize, sizeof(compressedGblFile) - offset);
    retval = parser_parse((void*)&parserContext,
                          &imageProperties,
                          (uint8_t *)&compressedGblFile[offset],
                          remaining,
                          &parserCallbacks);
    offset += chunkSize;

    if (retval != BOOTLOADER_OK) {
      sprintf(stringBuffer, "parser_parse error 0x%08lx at offset %d", retval, offset);
      TEST_FAIL_MESSAGE(stringBuffer);
      break;
    } else if (imageProperties.imageCompleted) {
      if (!imageProperties.imageVerified) {
        TEST_FAIL_MESSAGE("Image parsing completed, but verification failed. Are the correct keys flashed?");
      }
      break;
    } else {
    }
  }

#if defined(FLASH_BUFFER_BASE)
  TEST_ASSERT_EQUAL_HEX8_ARRAY(compressedRawData, (void*)FLASH_BUFFER_BASE, sizeof(compressedRawData));
#else
  TEST_ASSERT_EQUAL_HEX8_ARRAY(compressedRawData, parserBuffer, sizeof(compressedRawData));
#endif
}

int main(void)
{
  CHIP_Init();

#if defined(UNITY_OUTPUT_SERIAL)
  UNITY_UsartSetup();
#else
  SWO_Setup();
#endif
  sl_memory_init();

#if defined(FLASH_BUFFER_BASE)
  UnityPrintf("mbt = %lx\n", (uint32_t)&(mainBootloaderTable));
  mainBootloaderTable->endOfAppSpace = (void *)(FLASH_BUFFER_BASE + PARSER_BUFFER_SIZE);
#endif

  UnityPrintf("BootloaderParserContext size: %d\n", sizeof(BootloaderParserContext_t));
  UnityPrintf("ImageProperties size: %d\n", sizeof(ImageProperties_t));
  UnityPrintf("ParserContext size: %d\n", sizeof(ParserContext_t));
  UnityPrintf("DecryptContext size: %d\n", sizeof(DecryptContext_t));
  UnityPrintf("AuthContext size: %d\n", sizeof(AuthContext_t));

  UnityBeginGroup("EBL_PARSER_LONG");

  RUN_TEST(testGblParserCompressed, __LINE__);

  UnityEnd();

#if ENABLE_TEST_COVERAGE
  __gcov_flush();
#endif

  UnityPrint("ENDSWO");
  while (1) ;
}
