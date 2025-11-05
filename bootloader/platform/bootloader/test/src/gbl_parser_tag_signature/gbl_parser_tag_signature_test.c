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

#include "bin/gbl_test_data.c"

// --------------------------------
// Test prototypes
void test_gbl_parser_signature_positive(void);
void test_gbl_parser_signature_negative_missing_signature(void);
void test_gbl_parser_signature_negative_invalid_signature(void);
void test_gbl_parser_signature_negative_prog_tag_injected_after_sig(void);

// Helper functions
void test_full_gbl_parse_should_fail(uint8_t *gbl_file, size_t gbl_file_size);

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

  UnityBeginGroup("GBL_PARSER_TAG_SIGNATURE");

  /// Setup pointer to application properties struct
  dummyTable.signature = appProperties;
  mainBootloaderTable->startOfAppSpace = &dummyTable;

  RUN_TEST(test_gbl_parser_signature_positive, __LINE__);
  RUN_TEST(test_gbl_parser_signature_negative_invalid_signature, __LINE__);
  RUN_TEST(test_gbl_parser_signature_negative_missing_signature, __LINE__);
  RUN_TEST(test_gbl_parser_signature_negative_prog_tag_injected_after_sig, __LINE__);

  UnityEnd();

#if ENABLE_TEST_COVERAGE
  __gcov_flush();
#endif

  UnityPrint("ENDSWO");

  return 0;
}

// --------------------------------
// Test implementations

// Sanity check that the GBL parser correctly accepts a minimalistic signed GBL
void test_gbl_parser_signature_positive(void)
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
                        (uint8_t *)&(gbl_file_signed_valid[offset]),
                        sizeof(GblHeader_t),
                        &parserCallbacks);
  TEST_ASSERT_EQUAL_HEX32(BOOTLOADER_OK, retval);
  offset += sizeof(GblHeader_t);

  /// Parse Application tag (should pass)
  retval = parser_parse((void*)&parserContext,
                        &imageProperties,
                        (uint8_t *)&(gbl_file_signed_valid[offset]),
                        sizeof(GblApplication_t),
                        &parserCallbacks);
  TEST_ASSERT_EQUAL_HEX32(BOOTLOADER_OK, retval);
  offset += sizeof(GblApplication_t);
  TEST_ASSERT_EQUAL_HEX32(GblParserStateIdle, parserContext.internalState);

  /// Parse Signature tag (should pass)
  retval = parser_parse((void*)&parserContext,
                        &imageProperties,
                        (uint8_t *)&(gbl_file_signed_valid[offset]),
                        sizeof(GblSignatureEcdsaP256_t),
                        &parserCallbacks);
  TEST_ASSERT_EQUAL_HEX32(BOOTLOADER_OK, retval);
  offset += sizeof(GblSignatureEcdsaP256_t);
  TEST_ASSERT_EQUAL_HEX32(GblParserStateIdle, parserContext.internalState);
  TEST_ASSERT_TRUE(parserContext.gotSignature);

  /// Parse GBL End tag and finalize (should pass)
  retval = parser_parse((void*)&parserContext,
                        &imageProperties,
                        (uint8_t *)&(gbl_file_signed_valid[offset]),
                        sizeof(gbl_file_signed_valid) - offset,
                        &parserCallbacks);
  TEST_ASSERT_EQUAL_HEX32(BOOTLOADER_OK, retval);
  TEST_ASSERT_EQUAL_HEX32(GblParserStateDone, parserContext.internalState);
  TEST_ASSERT_TRUE(imageProperties.imageCompleted);
  TEST_ASSERT_TRUE(imageProperties.imageVerified);
}

// A bootloader configured to require authenticity should reject GBLs with
// missing signature
void test_gbl_parser_signature_negative_missing_signature(void)
{
  // Missing signature, GBL_TYPE_NONE
  test_full_gbl_parse_should_fail(gbl_file_no_signature_plain_type,
                                  sizeof(gbl_file_no_signature_plain_type));

  // Missing signature, GBL_TYPE_ENCRYPTION_AESCCM
  test_full_gbl_parse_should_fail(gbl_file_no_signature_enc_type,
                                  sizeof(gbl_file_no_signature_enc_type));

  // Missing signature, GBL_TYPE_SIGNATURE_ECDSA
  test_full_gbl_parse_should_fail(gbl_file_no_signature_signed_type,
                                  sizeof(gbl_file_no_signature_signed_type));

  // Missing signature, GBL_TYPE_SIGNATURE_ECDSA | GBL_TYPE_ENCRYPTION_AESCCM
  test_full_gbl_parse_should_fail(gbl_file_no_signature_signed_enc_type,
                                  sizeof(gbl_file_no_signature_signed_enc_type));
}

// The bootloader should reject GBLs with invalid signatures
void test_gbl_parser_signature_negative_invalid_signature(void)
{
  // Invalid signature
  test_full_gbl_parse_should_fail(gbl_file_signed_invalid,
                                  sizeof(gbl_file_signed_invalid));
}

// The bootloader should reject GBLs with additional tags injected after the
// Signature tag
void test_gbl_parser_signature_negative_prog_tag_injected_after_sig(void)
{
  // Injected Prog tag after signature:
  // [HEADER] --> [APPLICATION] --> [SIGNATURE] --> [PROG] --> [END]
  test_full_gbl_parse_should_fail(gbl_file_prog_tag_after_sig,
                                  sizeof(gbl_file_prog_tag_after_sig));
}

// --------------------------------
// Helper functions

void test_full_gbl_parse_should_fail(uint8_t *gbl_file, size_t gbl_file_size)
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

  /// Parse GBL
  offset = 0;
  retval = parser_parse((void*)&parserContext,
                        &imageProperties,
                        (uint8_t *)&(gbl_file[offset]),
                        gbl_file_size - offset,
                        &parserCallbacks);
  TEST_ASSERT_NOT_EQUAL(BOOTLOADER_OK, retval);
  TEST_ASSERT_EQUAL_HEX32(GblParserStateError, parserContext.internalState);
  TEST_ASSERT_FALSE(imageProperties.imageCompleted);
  TEST_ASSERT_FALSE(imageProperties.imageVerified);
}
