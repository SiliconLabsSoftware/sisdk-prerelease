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

#include "core/btl_parse.h"
#include "sl_memory_manager.h"

// Get EBL array
#include "ebl/testv3Encrypted.c"
#include "ebl/compressed-gbl-binary.c"
#include "ebl/compressed-raw-binary.c"

#include "parser/gbl/btl_gbl_parser.h"
#include "parser/gbl/btl_gbl_format.h"
#include "parser/gbl/btl_gbl_custom_tags.h"
#include "security/btl_security_types.h"

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
#ifndef NO_COMPRESSION
uint8_t parserBuffer[PARSER_BUFFER_SIZE];
#endif

static uint32_t minimumAppVersion = 0;
static bool gotCallback = false;
static bool gotWithheldCallback = false;

// This is to bypass address check for btl app properties pointer
MainBootloaderTable_t mainBootloaderTableImpl = {
  .startOfAppSpace = (BareBootTable_t *) 0x4000UL
};
MainBootloaderTable_t *mainBootloaderTable = &mainBootloaderTableImpl;

bool parser_applicationUpgradeValidCallback(ApplicationData_t *app)
{
  if (app->version < minimumAppVersion) {
    return false;
  } else {
    return true;
  }
}

void HardFault_Handler(void)
{
  UnityPrintf("FAULT\n");
  while (1) {
    // Do nothing
  }
}

// Implement parser callbacks
void parseCallback(uint32_t offset, uint8_t *buffer, size_t length, void *ctx)
{
  (void) buffer;
  (void) ctx;
  (void) offset;
  (void) length;
  gotCallback = true;
}

void appParseCallback(uint32_t offset, uint8_t *buffer, size_t length, void *ctx)
{
  gotCallback = true;

  uint32_t appPcAddress;
  uint32_t btlPcAddress;
  uint32_t startWithhold;
  uint32_t withholdSrcOffset;
  uint32_t appBaseAddress = (uint32_t) mainBootloaderTable->startOfAppSpace;

  // Initial PC or reset vector should be written to the flash at the end when entire
  // EBL is validated.
  ParserContext_t *parserContext = (ParserContext_t *)ctx;
  if (parserContext->internalState == GblParserStateFinalize) {
    if (offset > BTL_UPGRADE_LOCATION) {
      // reset vector of the upgrade space should be written at this stage.
      TEST_ASSERT_TRUE(offset < (BTL_UPGRADE_LOCATION + 8U));
    } else {
      TEST_ASSERT_TRUE(offset >= (appBaseAddress + 4U));
      TEST_ASSERT_TRUE(offset < (appBaseAddress + 28U));
    }
    gotWithheldCallback = true;
  } else {
    appPcAddress = appBaseAddress + 4UL;
    if ((offset <= appPcAddress) && ((offset + length) > appPcAddress)) {
      startWithhold = (offset > appPcAddress) ? offset : appPcAddress;
      withholdSrcOffset = startWithhold - offset;
      TEST_ASSERT_EQUAL_HEX8(0xFFUL, buffer[withholdSrcOffset]);
      TEST_ASSERT_EQUAL_HEX8(0xFFUL, buffer[withholdSrcOffset + 1]);
      TEST_ASSERT_EQUAL_HEX8(0xFFUL, buffer[withholdSrcOffset + 2]);
      TEST_ASSERT_EQUAL_HEX8(0xFFUL, buffer[withholdSrcOffset + 3]);
    }

    btlPcAddress = BTL_UPGRADE_LOCATION + 4UL;
    if ((offset <= btlPcAddress) && ((offset + length) >= (btlPcAddress + 4UL))) {
      withholdSrcOffset = btlPcAddress - offset;
      // TEST_ASSERT_EQUAL(withholdSrcOffset, length);
      TEST_ASSERT_EQUAL_HEX8(0xFFUL, buffer[withholdSrcOffset]);
      TEST_ASSERT_EQUAL_HEX8(0xFFUL, buffer[withholdSrcOffset + 1]);
      TEST_ASSERT_EQUAL_HEX8(0xFFUL, buffer[withholdSrcOffset + 2]);
      TEST_ASSERT_EQUAL_HEX8(0xFFUL, buffer[withholdSrcOffset + 3]);
    }
  }
  // UnityPrintf("parseCb %08x | %08x\n", offset, length);
}

void compressedParseCallback(uint32_t offset, uint8_t *buffer, size_t length, void *ctx)
{
  (void) ctx;
#ifdef NO_COMPRESSION
  (void) offset;
  (void) buffer;
  (void) length;

  TEST_FAIL_MESSAGE("No compression supported, but compression callback still called");
#else
  // Should always write word size chunks
  TEST_ASSERT_EQUAL(0, length % 4);

//  UnityPrintf("parseCb %08x | %08x | ", offset, length);
//  for (size_t i = 0; i < length; i++) {
//    UnityPrintf("%02x", buffer[i]);
//  }
//  UNITY_PRINT_EOL;

  // Store app data in array for comparison later
  size_t appBase = (uint32_t) mainBootloaderTable->startOfAppSpace;
  memcpy(&parserBuffer[offset - appBase], buffer, length);
#endif
}

void testGblParser(void)
{
  // Test the EBL parser with a file that has been compiled in
  size_t offset = 0;
  uint32_t retval = 0;
  ParserContext_t parserContext;
  DecryptContext_t decryptContext;
  AuthContext_t authContext;
  const BootloaderParserCallbacks_t parserCallbacks = { (void*)&parserContext, appParseCallback, parseCallback, parseCallback };
  ImageProperties_t imageProperties = { 0 };

  retval = parser_init(&parserContext,
                       &decryptContext,
                       &authContext,
                       PARSER_FLAG_PARSE_CUSTOM_TAGS);
  UnityPrintf("Init = %0x\n", retval);
  gotCallback = false;
  gotWithheldCallback = false;

  while (offset < sizeof(eblv3Encrypted) / sizeof(eblv3Encrypted[0])) {
    if (offset < sizeof(eblv3Encrypted) - 64) {
      retval = parser_parse((void*)&parserContext, &imageProperties, (uint8_t*)&(eblv3Encrypted[offset]), 64, &parserCallbacks);
      offset += 64 / sizeof(eblv3Encrypted[0]);
    } else {
      retval = parser_parse((void*)&parserContext, &imageProperties, (uint8_t*)&(eblv3Encrypted[offset]), 4, &parserCallbacks);
      offset += 4 / sizeof(eblv3Encrypted[0]);
    }

    if (retval != 0) {
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

  TEST_ASSERT_TRUE(gotCallback);
  TEST_ASSERT_TRUE(gotWithheldCallback);
}

void testGblParserAppTooOld(void)
{
  // Test the EBL parser with a file that has been compiled in
  size_t offset = 0;
  uint32_t retval = 0;
  ParserContext_t parserContext;
  DecryptContext_t decryptContext = { 0 };
  AuthContext_t authContext;
  const BootloaderParserCallbacks_t parserCallbacks = { (void*)&parserContext, appParseCallback, parseCallback, parseCallback };
  ImageProperties_t imageProperties = { 0 };

  retval = parser_init(&parserContext,
                       &decryptContext,
                       &authContext,
                       PARSER_FLAG_PARSE_CUSTOM_TAGS);
  UnityPrintf("Init = %0x\n", retval);

  minimumAppVersion = 1;
  gotCallback = false;
  gotWithheldCallback = false;

  while (offset < sizeof(eblv3Encrypted) / sizeof(eblv3Encrypted[0])) {
    if (offset < sizeof(eblv3Encrypted) - 64) {
      retval = parser_parse((void*)&parserContext, &imageProperties, (uint8_t*)&(eblv3Encrypted[offset]), 64, &parserCallbacks);
      offset += 64 / sizeof(eblv3Encrypted[0]);
    } else {
      retval = parser_parse((void*)&parserContext, &imageProperties, (uint8_t*)&(eblv3Encrypted[offset]), 4, &parserCallbacks);
      offset += 4 / sizeof(eblv3Encrypted[0]);
    }

    if (retval != 0) {
      break;
    }
  }

  TEST_ASSERT_FALSE(imageProperties.imageCompleted);
  TEST_ASSERT_FALSE(imageProperties.imageVerified);
  TEST_ASSERT_FALSE(gotCallback);
  TEST_ASSERT_FALSE(gotWithheldCallback);

  TEST_ASSERT_EQUAL_HEX32(BOOTLOADER_ERROR_PARSER_REJECTED, retval);
}

void testGblParserCompressed(void)
{
  // Test the EBL parser with a file that has been compiled in
  size_t offset = 0;
  const size_t chunkSize = 64U;
  size_t remaining = 0;
  int32_t retval = 0;
  ParserContext_t parserContext;
  DecryptContext_t decryptContext;
  AuthContext_t authContext;
  const BootloaderParserCallbacks_t parserCallbacks = {
    NULL,
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
#ifdef NO_COMPRESSION
      // Only fail if not unknown tag
      if (retval != BOOTLOADER_ERROR_PARSER_UNKNOWN_TAG) {
#endif
      sprintf(stringBuffer, "parser_parse error 0x%08lx at offset %d", retval, offset);
      TEST_FAIL_MESSAGE(stringBuffer);
#ifdef NO_COMPRESSION
    }
#endif
      break;
    } else if (imageProperties.imageCompleted) {
#ifdef NO_COMPRESSION
      TEST_FAIL_MESSAGE("Compressed image shouldn't be completed");
#else
      if (!imageProperties.imageVerified) {
        TEST_FAIL_MESSAGE("Image parsing completed, but verification failed. Are the correct keys flashed?");
      }
#endif
      break;
    } else {
    }
  }

#ifndef NO_COMPRESSION
  TEST_ASSERT_EQUAL_HEX8_ARRAY(compressedRawData, parserBuffer, sizeof(compressedRawData));
#endif
}

void testCoreParseBuffer(void)
{
  // Test the EBL parser with a file that has been compiled in
  size_t offset = 0;
  uint32_t retval = 0;
  BootloaderParserContext_t context;
  const BootloaderParserCallbacks_t parserCallbacks = { (void*)&context.parserContext, appParseCallback, parseCallback, parseCallback };

  TEST_ASSERT_EQUAL(BOOTLOADER_ERROR_PARSE_CONTEXT, core_initParser(&context, sizeof(BootloaderParserContext_t) - 1));
  core_initParser(&context, sizeof(BootloaderParserContext_t));

  gotCallback = false;
  gotWithheldCallback = false;

  while (offset < (sizeof(eblv3Encrypted) / sizeof(eblv3Encrypted[0]))) {
    if (offset < (sizeof(eblv3Encrypted) / sizeof(eblv3Encrypted[0])) - 16) {
      retval = core_parseBuffer(&context, &parserCallbacks, (uint8_t*)&(eblv3Encrypted[offset]), 64);
      offset += 64 / sizeof(eblv3Encrypted[0]);
    } else {
      retval = core_parseBuffer(&context, &parserCallbacks, (uint8_t*)&(eblv3Encrypted[offset]), 4);
      offset += 4 / sizeof(eblv3Encrypted[0]);
    }

    if (retval != BOOTLOADER_ERROR_PARSE_CONTINUE
        && retval != BOOTLOADER_ERROR_PARSE_SUCCESS) {
      sprintf(stringBuffer, "parseBuffer error 0x%08lx at offset %d", retval, offset);
      TEST_FAIL_MESSAGE(stringBuffer);
      break;
    } else if (context.imageProperties.imageCompleted) {
      if (!context.imageProperties.imageVerified) {
        TEST_FAIL_MESSAGE("Image parsing completed, but verification failed. Are the correct keys flashed?");
      }
      break;
    } else {
    }
  }

  TEST_ASSERT_TRUE(gotCallback);
  TEST_ASSERT_TRUE(gotWithheldCallback);
}

void testCustomTag(void)
{
#ifdef BTL_PARSER_SUPPORT_LZ4
  GblTagHeader_t tagHeader;
  tagHeader.tagId = 0xFD0707FDUL; // LZMA
  TEST_ASSERT_FALSE(gbl_isCustomTag(&tagHeader));
  tagHeader.tagId = 0xFD0505FDUL; // LZ4
  TEST_ASSERT(gbl_isCustomTag(&tagHeader));

  TEST_ASSERT_EQUAL(0xFD0505FDUL, gbl_getCustomTagProperties(0xFD0505FDUL)->tagId);
#else
  TEST_ASSERT_EQUAL(0, gbl_getCustomTagProperties(0xFD0505FDUL));
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

  UnityPrintf("BootloaderParserContext size: %d\n", sizeof(BootloaderParserContext_t));
  UnityPrintf("ImageProperties size: %d\n", sizeof(ImageProperties_t));
  UnityPrintf("ParserContext size: %d\n", sizeof(ParserContext_t));
  UnityPrintf("DecryptContext size: %d\n", sizeof(DecryptContext_t));
  UnityPrintf("AuthContext size: %d\n", sizeof(AuthContext_t));

  UnityBeginGroup("EBL_PARSER");

  RUN_TEST(testGblParser, __LINE__);
  RUN_TEST(testGblParserCompressed, __LINE__);
  RUN_TEST(testCoreParseBuffer, __LINE__);
  RUN_TEST(testGblParserAppTooOld, __LINE__);
  RUN_TEST(testCustomTag, __LINE__);

  UnityEnd();

#if ENABLE_TEST_COVERAGE
  __gcov_flush();
#endif

  UnityPrint("ENDSWO");
}
