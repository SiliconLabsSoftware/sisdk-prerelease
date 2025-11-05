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
//#include "ebl/testv2.c"
//#include "ebl/testv3Encrypted.c"
#include "ebl/efm32ess-3875-gbl-binary.c"
#include "ebl/efm32ess-3875-raw-binary.c"

#include "parser/gbl/btl_gbl_parser.h"
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
uint8_t parserBuffer[PARSER_BUFFER_SIZE];

// This is to bypass address check for btl app properties pointer
MainBootloaderTable_t mainBootloaderTableImpl;
MainBootloaderTable_t *mainBootloaderTable = &mainBootloaderTableImpl;

bool parser_applicationUpgradeValidCallback(ApplicationData_t *app)
{
  (void) app;
  // By default, all applications are considered valid
  return true;
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
  (void) ctx;
  // Should always write word size chunks
  TEST_ASSERT_EQUAL(0, length % 4);

//  UnityPrintf("parseCb %08x | %08x | ", offset, length);
//  for (size_t i = 0; i < length; i++) {
//    UnityPrintf("%02x", buffer[i]);
//  }
//  UNITY_PRINT_EOL;

  // Store app data in array for comparison later
  size_t appBase = (size_t) mainBootloaderTable->startOfAppSpace;
  memcpy(&parserBuffer[offset - appBase], buffer, length);
}

void testGblParserEfm32ess3875(void)
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
    NULL,
    parseCallback,
    parseCallback,
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

  TEST_ASSERT_EQUAL_HEX8_ARRAY(rawData, parserBuffer, sizeof(rawData));
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

  RUN_TEST(testGblParserEfm32ess3875, __LINE__);

  UnityEnd();

#if ENABLE_TEST_COVERAGE
  __gcov_flush();
#endif

  UnityPrint("ENDSWO");
  while (1) ;
}
