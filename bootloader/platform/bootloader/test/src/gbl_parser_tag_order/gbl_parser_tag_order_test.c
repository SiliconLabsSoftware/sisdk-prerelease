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

#include "bin/gbl_tag_order.c"

// --------------------------------
// Test prototypes
void test_gbl_parser_single_enc_init_allowed(void);
void test_gbl_parser_multiple_enc_init_forbidden(void);
#if defined(_SILICON_LABS_32B_SERIES_2)
void test_gbl_parser_verdep_enc_tag_order_pass(void);
void test_gbl_parser_verdep_enc_tag_order_fail(void);
#endif

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

  UnityBeginGroup("GBL_PARSER_TAG_ORDER");

  /// Setup pointer to application properties struct
  dummyTable.signature = appProperties;
  mainBootloaderTable->startOfAppSpace = &dummyTable;

  RUN_TEST(test_gbl_parser_single_enc_init_allowed, __LINE__);
  RUN_TEST(test_gbl_parser_multiple_enc_init_forbidden, __LINE__);
#if defined(_SILICON_LABS_32B_SERIES_2)
  RUN_TEST(test_gbl_parser_verdep_enc_tag_order_pass, __LINE__);
  RUN_TEST(test_gbl_parser_verdep_enc_tag_order_fail, __LINE__);
#endif
  UnityEnd();

#if ENABLE_TEST_COVERAGE
  __gcov_flush();
#endif

  UnityPrint("ENDSWO");

  return 0;
}

// --------------------------------
// Test implementations

// Sanity check that the GBL parser correctly accepts a GBL containing
// a single ENC_INIT tag
void test_gbl_parser_single_enc_init_allowed(void)
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
                        (uint8_t *)&(gbl_file_single_enc_init[offset]),
                        sizeof(GblHeader_t),
                        &parserCallbacks);
  TEST_ASSERT_EQUAL_HEX32(BOOTLOADER_OK, retval);
  offset += sizeof(GblHeader_t);

  /// Parse ENC_INIT tag (should pass)
  retval = parser_parse((void*)&parserContext,
                        &imageProperties,
                        (uint8_t *)&(gbl_file_single_enc_init[offset]),
                        sizeof(GblEncryptionInitAesCcm_t),
                        &parserCallbacks);
  TEST_ASSERT_EQUAL_HEX32(BOOTLOADER_OK, retval);
  offset += sizeof(GblEncryptionInitAesCcm_t);
  TEST_ASSERT_EQUAL_HEX32(GblParserStateIdle, parserContext.internalState);

  /// Parse GBL End tag and finalize (should pass)
  retval = parser_parse((void*)&parserContext,
                        &imageProperties,
                        (uint8_t *)&(gbl_file_single_enc_init[offset]),
                        sizeof(gbl_file_single_enc_init) - offset,
                        &parserCallbacks);
  TEST_ASSERT_EQUAL_HEX32(BOOTLOADER_OK, retval);
  TEST_ASSERT_EQUAL_HEX32(GblParserStateDone, parserContext.internalState);
  TEST_ASSERT_TRUE(imageProperties.imageCompleted);
  TEST_ASSERT_TRUE(imageProperties.imageVerified);
}

// Confirm that the GBL parser actually rejects GBL images containing multiple
// ENC_INIT tags, preventing the ENC_INIT replay attack described in PSIRT-142
void test_gbl_parser_multiple_enc_init_forbidden(void)
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
                        (uint8_t *)&(gbl_file_double_enc_init[offset]),
                        sizeof(GblHeader_t),
                        &parserCallbacks);
  TEST_ASSERT_EQUAL_HEX32(BOOTLOADER_OK, retval);
  offset += sizeof(GblHeader_t);

  /// Parse first ENC_INIT tag (should pass)
  retval = parser_parse((void*)&parserContext,
                        &imageProperties,
                        (uint8_t *)&(gbl_file_double_enc_init[offset]),
                        sizeof(GblEncryptionInitAesCcm_t),
                        &parserCallbacks);
  TEST_ASSERT_EQUAL_HEX32(BOOTLOADER_OK, retval);
  offset += sizeof(GblEncryptionInitAesCcm_t);
  TEST_ASSERT_EQUAL_HEX32(GblParserStateIdle, parserContext.internalState);

  /// Attempt to parse second ENC_INIT tag (should fail)
  retval = parser_parse((void*)&parserContext,
                        &imageProperties,
                        (uint8_t *)&(gbl_file_double_enc_init[offset]),
                        sizeof(GblEncryptionInitAesCcm_t),
                        &parserCallbacks);
  TEST_ASSERT_EQUAL_HEX32(BOOTLOADER_ERROR_PARSER_INVALID_TAG_ORDER, retval);
  offset += sizeof(GblEncryptionInitAesCcm_t);
  TEST_ASSERT_EQUAL_HEX32(GblParserStateError, parserContext.internalState);

  /// Attempt to parse GBL End tag and finalize (should fail)
  retval = parser_parse((void*)&parserContext,
                        &imageProperties,
                        (uint8_t *)&(gbl_file_double_enc_init[offset]),
                        sizeof(gbl_file_double_enc_init) - offset,
                        &parserCallbacks);
  TEST_ASSERT_EQUAL_HEX32(BOOTLOADER_ERROR_PARSER_EOF, retval);
  TEST_ASSERT_EQUAL_HEX32(GblParserStateError, parserContext.internalState);
  TEST_ASSERT_FALSE(imageProperties.imageCompleted);
  TEST_ASSERT_FALSE(imageProperties.imageVerified);
}

#if defined(_SILICON_LABS_32B_SERIES_2)
void test_gbl_parser_verdep_enc_tag_order_pass(void)
{
  int32_t retval = 0;
  size_t offset = 0;
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

  retval = parser_parse((void*)&parserContext,
                        &imageProperties,
                        (uint8_t *)&(gbl_file_verdep_enc_init_1[offset]),
                        sizeof(gbl_file_verdep_enc_init_1),
                        &parserCallbacks);
  TEST_ASSERT_EQUAL_HEX32(BOOTLOADER_OK, retval);
}

void test_gbl_parser_verdep_enc_tag_order_fail(void)
{
  int32_t retval = 0;
  size_t offset = 0;
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

  retval = parser_parse((void*)&parserContext,
                        &imageProperties,
                        (uint8_t *)&(gbl_file_verdep_enc_init_2[offset]),
                        sizeof(gbl_file_verdep_enc_init_2),
                        &parserCallbacks);
  TEST_ASSERT_EQUAL_HEX32(BOOTLOADER_ERROR_PARSER_INVALID_TAG_ORDER, retval);
}
#endif
