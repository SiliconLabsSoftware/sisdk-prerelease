/***************************************************************************//**
 * @file ebl_parser_metadata_test.c
 * @brief Metadata test.
 *        Ensure that the parser can handle a GBL that only contains metadata.
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
#include "ebl/metadata.bin.c"
#include "ebl/metadata.gbl.c"

#include "parser/gbl/btl_gbl_parser.h"
#include "security/btl_security_types.h"

// Get size_t
#include <stddef.h>

#include <string.h>
#include <stdio.h>
// Unity test framework
#include "unity.h"
#if defined(UNITY_TEST_REPORT_SERIAL)
#include "unity_usart.h"
#else
#include "unity_swo.h"
#endif

#if ENABLE_TEST_COVERAGE
extern void __gcov_flush(void);
#endif

char stringBuffer[48];
uint8_t parserBuffer[PARSER_BUFFER_SIZE];

// This is to bypass address check for btl app properties pointer
MainBootloaderTable_t mainBootloaderTableImpl;
MainBootloaderTable_t *mainBootloaderTable = &mainBootloaderTableImpl;

void HardFault_Handler(void)
{
  UnityPrintf("FAULT\n");
  while (1) {
    // Do nothing
  }
}

// Implement parser callbacks
bool parser_applicationUpgradeValidCallback(ApplicationData_t *app)
{
  (void) app;
  // By default, all applications are considered valid
  return true;
}

void parseCallback(uint32_t offset, uint8_t *buffer, size_t length, void *ctx)
{
  (void) buffer;
  (void) ctx;
  (void) offset;
  (void) length;
  TEST_FAIL_MESSAGE("Non-metadata content received");
//  UnityPrintf("parseCb %08x | %08x\n", offset, length);
}

void metadataCallback(uint32_t offset, uint8_t *buffer, size_t length, void *ctx)
{
  (void) ctx;
//  UnityPrintf("parseCb %08x | %08x | ", offset, length);
//  for (size_t i = 0; i < length; i++) {
//    UnityPrintf("%02x", buffer[i]);
//  }
//  UNITY_PRINT_EOL;

  // Store app data in array for comparison later
  memcpy(&parserBuffer[offset], buffer, length);
}

void testGblParser(void)
{
  memset(parserBuffer, 0, sizeof(parserBuffer));
  // Test the EBL parser with a file that has been compiled in
  size_t offset = 0;
  const size_t chunkSize = 64U;
  size_t remaining = 0;
  int32_t retval = 0;
  ParserContext_t parserContext;
  DecryptContext_t decryptContext = { 0 };
  AuthContext_t authContext;
  const BootloaderParserCallbacks_t parserCallbacks = {
    NULL,
    parseCallback,
    metadataCallback,
    parseCallback
  };
  ImageProperties_t imageProperties = { 0 };

  retval = parser_init(&parserContext,
                       &decryptContext,
                       &authContext,
                       PARSER_FLAG_PARSE_CUSTOM_TAGS);
  UnityPrintf("Init = %0x\n", retval);

  while (offset < sizeof(gblFile)) {
    remaining = SL_MIN(chunkSize, sizeof(gblFile) - offset);
    retval = parser_parse((void*)&parserContext,
                          &imageProperties,
                          (uint8_t *)&gblFile[offset],
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

  TEST_ASSERT_EQUAL_HEX8_ARRAY(rawFile, parserBuffer, sizeof(rawFile));
}

void testCoreParseBuffer(void)
{
  memset(parserBuffer, 0, sizeof(parserBuffer));
  // Test the EBL parser with a file that has been compiled in
  size_t offset = 0;
  uint32_t retval = 0;
  BootloaderParserContext_t context;
  const BootloaderParserCallbacks_t parserCallbacks = { NULL, parseCallback, metadataCallback, parseCallback };

  core_initParser(&context, sizeof(BootloaderParserContext_t));

  while (offset < (sizeof(gblFile))) {
    if (offset < (sizeof(gblFile)) - 16) {
      retval = core_parseBuffer(&context, &parserCallbacks, (uint8_t*)&(gblFile[offset]), 64);
      offset += 64;
    } else {
      retval = core_parseBuffer(&context, &parserCallbacks, (uint8_t*)&(gblFile[offset]), 4);
      offset += 4;
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

  TEST_ASSERT_EQUAL_HEX8_ARRAY(rawFile, parserBuffer, sizeof(rawFile));
}

int main(void)
{
  CHIP_Init();

#if defined(UNITY_TEST_REPORT_SERIAL)
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

  UnityBeginGroup("EBL_PARSER_METADATA");

  RUN_TEST(testGblParser, __LINE__);
  RUN_TEST(testCoreParseBuffer, __LINE__);

  UnityEnd();

#if ENABLE_TEST_COVERAGE
  __gcov_flush();
#endif

  UnityPrint("ENDSWO");
}
