// --------------------------------
// Test environment
#include "em_device.h"
#include "em_chip.h"

#include "unity.h"
#if defined(UNITY_OUTPUT_SERIAL)
#include "unity_usart.h"
#else
#include "unity_swo.h"
#endif

#include <stddef.h>
#include <string.h>
#include <stdio.h>

#if ENABLE_TEST_COVERAGE
extern void __gcov_flush(void);
#endif

// --------------------------------
// Test includes

#include "core/btl_parse.h"
#include "core/btl_bootload.h"

#include "parser/gbl/btl_gbl_parser.h"
#include "parser/gbl/btl_gbl_format.h"

#include "security/btl_security_types.h"

#include "bin/gbl_test_data.c"

// --------------------------------
// Test prototypes
void test_gbl_parser_parse_app_info_pass(void);
void test_gbl_parser_parse_app_info_fail(void);

// Application properties
ApplicationProperties_t appPropertiesImpl = {
  .magic = APPLICATION_PROPERTIES_MAGIC,
  .structVersion = 0,
  .app = {
    .version = 0,
  },
};
ApplicationProperties_t *appProperties = &appPropertiesImpl;

// Dummy application vector table
BareBootTable_t dummyTable = { 0 };

// Bootloader table
MainBootloaderTable_t mainBootloaderTableImpl = {
  .header = {
    .type = BOOTLOADER_MAGIC_MAIN,
    .layout = BOOTLOADER_HEADER_VERSION_MAIN,
    .version = 0x00000000UL
  },
  .startOfAppSpace = 0
};
MainBootloaderTable_t *mainBootloaderTable = &mainBootloaderTableImpl;
// --------------------------------
// Test runner

int main(void)
{
  CHIP_Init();

#if defined(UNITY_OUTPUT_SERIAL)
  UNITY_UsartSetup();
#else
  SWO_Setup();
#endif

  UnityPrintf("BootloaderParserContext size: %d\n", sizeof(BootloaderParserContext_t));
  UnityPrintf("ImageProperties size: %d\n", sizeof(ImageProperties_t));
  UnityPrintf("ParserContext size: %d\n", sizeof(ParserContext_t));
  UnityPrintf("DecryptContext size: %d\n", sizeof(DecryptContext_t));
  UnityPrintf("AuthContext size: %d\n", sizeof(AuthContext_t));

  UnityBeginGroup("GBL_PARSER_APP_INFO");

  /// Setup pointer to application properties struct
  dummyTable.signature = appProperties;
  mainBootloaderTable->startOfAppSpace = &dummyTable;

  RUN_TEST(test_gbl_parser_parse_app_info_pass, __LINE__);
  RUN_TEST(test_gbl_parser_parse_app_info_fail, __LINE__);

  UnityEnd();

#if ENABLE_TEST_COVERAGE
  __gcov_flush();
#endif

  UnityPrint("ENDSWO");

  return 0;
}

void test_gbl_parser_parse_app_info_pass(void)
{
  int32_t retval = 0;
  ParserContext_t parserContext = { 0 };
  DecryptContext_t decryptContext = { 0 };
  AuthContext_t authContext = { 0 };

  const BootloaderParserCallbacks_t parserCallbacks = {
    NULL,
    NULL,
    NULL,
    NULL
  };
  ImageProperties_t imageProperties = { 0 };

  /// Init parser
  retval = parser_init(&parserContext,
                       &decryptContext,
                       &authContext,
                       0UL);
  TEST_ASSERT_EQUAL_HEX32(BOOTLOADER_OK, retval);

  /// Parse GBL header
  retval = parser_parse((void*)&parserContext,
                        &imageProperties,
                        (uint8_t *)&(gbl_file_1[0U]),
                        sizeof(gbl_file_1),
                        &parserCallbacks);
  TEST_ASSERT_EQUAL_HEX32(BOOTLOADER_OK, retval);
}

void test_gbl_parser_parse_app_info_fail(void)
{
  int32_t retval = 0;
  ParserContext_t parserContext = { 0 };
  DecryptContext_t decryptContext = { 0 };
  AuthContext_t authContext = { 0 };

  const BootloaderParserCallbacks_t parserCallbacks = {
    NULL,
    NULL,
    NULL,
    NULL
  };
  ImageProperties_t imageProperties = { 0 };

  /// Init parser
  retval = parser_init(&parserContext,
                       &decryptContext,
                       &authContext,
                       0UL);
  TEST_ASSERT_EQUAL_HEX32(BOOTLOADER_OK, retval);
  /// Parse GBL header
  retval = parser_parse((void*)&parserContext,
                        &imageProperties,
                        (uint8_t *)&(gbl_file_2[0U]),
                        sizeof(gbl_file_2),
                        &parserCallbacks);
  TEST_ASSERT_EQUAL_HEX32(BOOTLOADER_ERROR_PARSER_UNEXPECTED, retval);
}
