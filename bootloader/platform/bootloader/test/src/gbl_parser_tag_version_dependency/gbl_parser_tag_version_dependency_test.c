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
#include "sl_memory_manager.h"

#include "bin/gbl_version_dependency.c"

// --------------------------------
// Test prototypes
void test_gbl_parser_version_dependency_tag_positive(void);
void test_gbl_parser_version_dependency_tag_negative(void);
void test_gbl_parser_version_dependency_tag_interval(void);

// Helper functions
void test_full_gbl_parse_should_pass(void);
void test_full_gbl_parse_should_fail(void);

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
  sl_memory_init();

  UnityPrintf("BootloaderParserContext size: %d\n", sizeof(BootloaderParserContext_t));
  UnityPrintf("ImageProperties size: %d\n", sizeof(ImageProperties_t));
  UnityPrintf("ParserContext size: %d\n", sizeof(ParserContext_t));
  UnityPrintf("DecryptContext size: %d\n", sizeof(DecryptContext_t));
  UnityPrintf("AuthContext size: %d\n", sizeof(AuthContext_t));

  UnityBeginGroup("GBL_PARSER_TAG_VERSION_DEPENDENCY");

  /// Setup pointer to application properties struct
  dummyTable.signature = appProperties;
  mainBootloaderTable->startOfAppSpace = &dummyTable;

  RUN_TEST(test_gbl_parser_version_dependency_tag_positive, __LINE__);
  RUN_TEST(test_gbl_parser_version_dependency_tag_negative, __LINE__);
  RUN_TEST(test_gbl_parser_version_dependency_tag_interval, __LINE__);

  UnityEnd();

#if ENABLE_TEST_COVERAGE
  __gcov_flush();
#endif

  UnityPrint("ENDSWO");

  return 0;
}

// --------------------------------
// Test implementations

/// Positive tests
void test_gbl_parser_version_dependency_tag_positive(void)
{
  int32_t retval = 0;
  size_t offset;
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
  offset = 0;
  retval = parser_parse((void*)&parserContext,
                        &imageProperties,
                        (uint8_t *)&(gbl_file_1[offset]),
                        sizeof(GblHeader_t),
                        &parserCallbacks);
  TEST_ASSERT_EQUAL_HEX32(BOOTLOADER_OK, retval);
  offset += sizeof(GblHeader_t);

  /// Test case 1, bootloaderVersion < 0x00010203
  mainBootloaderTable->header.version = 0x00010202UL;
  retval = parser_parse((void*)&parserContext,
                        &imageProperties,
                        (uint8_t *)&(gbl_file_1[offset]),
                        sizeof(GblTagHeader_t) + sizeof(VersionDependency_t),
                        &parserCallbacks);
  TEST_ASSERT_EQUAL_HEX32(BOOTLOADER_OK, retval);
  offset += sizeof(GblTagHeader_t) + sizeof(VersionDependency_t);

  /// Test case 2, bootloaderVersion != 0x00010203
  mainBootloaderTable->header.version = 0x00010202UL;
  retval = parser_parse((void*)&parserContext,
                        &imageProperties,
                        (uint8_t *)&(gbl_file_1[offset]),
                        sizeof(GblTagHeader_t) + sizeof(VersionDependency_t),
                        &parserCallbacks);
  TEST_ASSERT_EQUAL_HEX32(BOOTLOADER_OK, retval);
  offset += sizeof(GblTagHeader_t) + sizeof(VersionDependency_t);

  /// Test case 3, (bootloaderVersion > 0x00010203) && (appVersion == 0x00030201)
  mainBootloaderTable->header.version = 0x00010204UL;
  appProperties->app.version          = 0x00030201UL;
  retval = parser_parse((void*)&parserContext,
                        &imageProperties,
                        (uint8_t *)&(gbl_file_1[offset]),
                        sizeof(GblTagHeader_t) + 2 * sizeof(VersionDependency_t),
                        &parserCallbacks);
  TEST_ASSERT_EQUAL_HEX32(BOOTLOADER_OK, retval);
  offset += sizeof(GblTagHeader_t) + 2 * sizeof(VersionDependency_t);

  /// Test case 4a, (bootloaderVersion == 0x12345678) &&
  ///               ((appVersion == 0x12345678) || (appVersion == 0x87654321))
  mainBootloaderTable->header.version = 0x12345678UL;
  appProperties->app.version          = 0x12345678UL;
  retval = parser_parse((void*)&parserContext,
                        &imageProperties,
                        (uint8_t *)&(gbl_file_1[offset]),
                        sizeof(GblTagHeader_t) + 3 * sizeof(VersionDependency_t),
                        &parserCallbacks);
  TEST_ASSERT_EQUAL_HEX32(BOOTLOADER_OK, retval);
  offset += sizeof(GblTagHeader_t) + 3 * sizeof(VersionDependency_t);

  /// Test case 4b, (bootloaderVersion == 0x12345678) &&
  ///               ((appVersion == 0x12345678) || (appVersion == 0x87654321))
  mainBootloaderTable->header.version = 0x12345678UL;
  appProperties->app.version          = 0x87654321UL;
  retval = parser_parse((void*)&parserContext,
                        &imageProperties,
                        (uint8_t *)&(gbl_file_1[offset]),
                        sizeof(GblTagHeader_t) + 3 * sizeof(VersionDependency_t),
                        &parserCallbacks);
  TEST_ASSERT_EQUAL_HEX32(BOOTLOADER_OK, retval);
  offset += sizeof(GblTagHeader_t) + 3 * sizeof(VersionDependency_t);

  /// Parse GBL End tag and finalize
  TEST_ASSERT_EQUAL_HEX32(GBL_TAG_ID_END, *((uint32_t *) &gbl_file_1[offset]));
  retval = parser_parse((void*)&parserContext,
                        &imageProperties,
                        (uint8_t *)&(gbl_file_1[offset]),
                        sizeof(gbl_file_1) - offset,
                        &parserCallbacks);
  TEST_ASSERT_EQUAL_HEX32(BOOTLOADER_OK, retval);
  TEST_ASSERT_TRUE(imageProperties.imageCompleted);
  TEST_ASSERT_TRUE(imageProperties.imageVerified);
}

/// Negative tests
void test_gbl_parser_version_dependency_tag_negative(void)
{
  int32_t retval = 0;
  size_t offset;
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
  offset = 0;
  retval = parser_parse((void*)&parserContext,
                        &imageProperties,
                        (uint8_t *)&(gbl_file_1[offset]),
                        sizeof(GblHeader_t),
                        &parserCallbacks);
  TEST_ASSERT_EQUAL_HEX32(BOOTLOADER_OK, retval);
  offset += sizeof(GblHeader_t);

  /// Test case 1, bootloaderVersion < 0x00010203
  mainBootloaderTable->header.version = 0x00010203UL;
  retval = parser_parse((void*)&parserContext,
                        &imageProperties,
                        (uint8_t *)&(gbl_file_1[offset]),
                        sizeof(GblTagHeader_t) + sizeof(VersionDependency_t),
                        &parserCallbacks);
  TEST_ASSERT_EQUAL_HEX32(BOOTLOADER_ERROR_PARSER_VERSION, retval);
  offset += sizeof(GblTagHeader_t) + sizeof(VersionDependency_t);
  parserContext.internalState = GblParserStateIdle;

  /// Test case 2, bootloaderVersion != 0x00010203
  mainBootloaderTable->header.version = 0x00010203UL;
  retval = parser_parse((void*)&parserContext,
                        &imageProperties,
                        (uint8_t *)&(gbl_file_1[offset]),
                        sizeof(GblTagHeader_t) + sizeof(VersionDependency_t),
                        &parserCallbacks);
  TEST_ASSERT_EQUAL_HEX32(BOOTLOADER_ERROR_PARSER_VERSION, retval);
  offset += sizeof(GblTagHeader_t) + sizeof(VersionDependency_t);
  parserContext.internalState = GblParserStateIdle;

  /// Test case 3, (bootloaderVersion > 0x00010203) && (appVersion == 0x00030201)
  mainBootloaderTable->header.version = 0x00010204UL;
  appProperties->app.version          = 0x00030202UL;
  retval = parser_parse((void*)&parserContext,
                        &imageProperties,
                        (uint8_t *)&(gbl_file_1[offset]),
                        sizeof(GblTagHeader_t) + 2 * sizeof(VersionDependency_t),
                        &parserCallbacks);
  TEST_ASSERT_EQUAL_HEX32(BOOTLOADER_ERROR_PARSER_VERSION, retval);
  offset += sizeof(GblTagHeader_t) + 2 * sizeof(VersionDependency_t);
  parserContext.internalState = GblParserStateIdle;

  /// Test case 4a, (bootloaderVersion == 0x12345678) &&
  ///               ((appVersion == 0x12345678) || (appVersion == 0x87654321))
  mainBootloaderTable->header.version = 0x12345678UL;
  appProperties->app.version          = 0x00000000UL;
  retval = parser_parse((void*)&parserContext,
                        &imageProperties,
                        (uint8_t *)&(gbl_file_1[offset]),
                        sizeof(GblTagHeader_t) + 3 * sizeof(VersionDependency_t),
                        &parserCallbacks);
  TEST_ASSERT_EQUAL_HEX32(BOOTLOADER_ERROR_PARSER_VERSION, retval);
  offset += sizeof(GblTagHeader_t) + 3 * sizeof(VersionDependency_t);
  parserContext.internalState = GblParserStateIdle;

  /// Test case 4b, (bootloaderVersion == 0x12345678) &&
  ///               ((appVersion == 0x12345678) || (appVersion == 0x87654321))
  mainBootloaderTable->header.version = 0x00000000UL;
  appProperties->app.version          = 0x87654321UL;
  retval = parser_parse((void*)&parserContext,
                        &imageProperties,
                        (uint8_t *)&(gbl_file_1[offset]),
                        sizeof(GblTagHeader_t) + 3 * sizeof(VersionDependency_t),
                        &parserCallbacks);
  TEST_ASSERT_EQUAL_HEX32(BOOTLOADER_ERROR_PARSER_VERSION, retval);
  offset += sizeof(GblTagHeader_t) + 3 * sizeof(VersionDependency_t);

  /// Attempt to parse GBL End tag and finalize. Should not succeed.
  TEST_ASSERT_EQUAL_HEX32(GBL_TAG_ID_END, *((uint32_t *) &gbl_file_1[offset]));
  retval = parser_parse((void*)&parserContext,
                        &imageProperties,
                        (uint8_t *)&(gbl_file_1[offset]),
                        sizeof(gbl_file_1) - offset,
                        &parserCallbacks);
  TEST_ASSERT_EQUAL_HEX32(BOOTLOADER_ERROR_PARSER_EOF, retval);
  TEST_ASSERT_EQUAL_HEX32(GblParserStateError, parserContext.internalState);
  TEST_ASSERT_FALSE(imageProperties.imageCompleted);
  TEST_ASSERT_FALSE(imageProperties.imageVerified);
}

/// Test full GBL parsing for different bootloader versions subject to
/// the version dependency (0x00010201 <= bootloaderVersion < 0x00010203)
void test_gbl_parser_version_dependency_tag_interval(void)
{
  mainBootloaderTable->header.version = 0x00010200UL;
  test_full_gbl_parse_should_fail();

  mainBootloaderTable->header.version = 0x00010201UL;
  test_full_gbl_parse_should_pass();

  mainBootloaderTable->header.version = 0x00010202UL;
  test_full_gbl_parse_should_pass();

  mainBootloaderTable->header.version = 0x00010203UL;
  test_full_gbl_parse_should_fail();
}

void test_full_gbl_parse_should_pass(void)
{
  int32_t retval = 0;
  size_t offset;
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
  offset = 0;
  retval = parser_parse((void*)&parserContext,
                        &imageProperties,
                        (uint8_t *)&(gbl_file_2[offset]),
                        sizeof(GblHeader_t),
                        &parserCallbacks);
  TEST_ASSERT_EQUAL_HEX32(BOOTLOADER_OK, retval);
  offset += sizeof(GblHeader_t);

  /// Parse version dependency tag
  retval = parser_parse((void*)&parserContext,
                        &imageProperties,
                        (uint8_t *)&(gbl_file_2[offset]),
                        sizeof(GblTagHeader_t) + 2 * sizeof(VersionDependency_t),
                        &parserCallbacks);
  TEST_ASSERT_EQUAL_HEX32(BOOTLOADER_OK, retval);
  offset += sizeof(GblTagHeader_t) + 2 * sizeof(VersionDependency_t);

  /// Parse the rest of the GBL
  TEST_ASSERT_EQUAL_HEX32(GBL_TAG_ID_END, *((uint32_t *) &gbl_file_2[offset]));
  retval = parser_parse((void*)&parserContext,
                        &imageProperties,
                        (uint8_t *)&(gbl_file_2[offset]),
                        sizeof(gbl_file_2) - offset,
                        &parserCallbacks);
  TEST_ASSERT_EQUAL_HEX32(BOOTLOADER_OK, retval);
  TEST_ASSERT_EQUAL_HEX32(GblParserStateDone, parserContext.internalState);
  TEST_ASSERT_TRUE(imageProperties.imageCompleted);
  TEST_ASSERT_TRUE(imageProperties.imageVerified);
}

void test_full_gbl_parse_should_fail(void)
{
  int32_t retval = 0;
  size_t offset;
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
  offset = 0;
  retval = parser_parse((void*)&parserContext,
                        &imageProperties,
                        (uint8_t *)&(gbl_file_2[offset]),
                        sizeof(GblHeader_t),
                        &parserCallbacks);
  TEST_ASSERT_EQUAL_HEX32(BOOTLOADER_OK, retval);
  offset += sizeof(GblHeader_t);

  /// Parse version dependency tag
  retval = parser_parse((void*)&parserContext,
                        &imageProperties,
                        (uint8_t *)&(gbl_file_2[offset]),
                        sizeof(GblTagHeader_t) + 2 * sizeof(VersionDependency_t),
                        &parserCallbacks);
  TEST_ASSERT_EQUAL_HEX32(BOOTLOADER_ERROR_PARSER_VERSION, retval);
  offset += sizeof(GblTagHeader_t) + 2 * sizeof(VersionDependency_t);

  /// Parse the rest of the GBL
  TEST_ASSERT_EQUAL_HEX32(GBL_TAG_ID_END, *((uint32_t *) &gbl_file_2[offset]));
  retval = parser_parse((void*)&parserContext,
                        &imageProperties,
                        (uint8_t *)&(gbl_file_2[offset]),
                        sizeof(gbl_file_2) - offset,
                        &parserCallbacks);
  TEST_ASSERT_EQUAL_HEX32(BOOTLOADER_ERROR_PARSER_EOF, retval);
  TEST_ASSERT_EQUAL_HEX32(GblParserStateError, parserContext.internalState);
  TEST_ASSERT_FALSE(imageProperties.imageCompleted);
  TEST_ASSERT_FALSE(imageProperties.imageVerified);
}
