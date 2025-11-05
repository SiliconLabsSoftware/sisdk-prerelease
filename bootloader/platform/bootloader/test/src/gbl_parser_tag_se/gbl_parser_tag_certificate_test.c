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

#include "api/btl_interface.h"
#include "api/btl_errorcode.h"
#include "sl_memory_manager.h"

#include "core/btl_parse.h"
#include "parser/gbl/btl_gbl_parser.h"
#include "security/btl_security_types.h"

#include "bin/SE_certificate_test.gbl.c"

// --------------------------------
// Test prototypes
void test_gbl_parser_extracts_se_tag(void);
void test_gbl_parser_ignores_se_tag(void);

#if defined(_SILICON_LABS_32B_SERIES_2)
const uint8_t bootloaderKeyAes[16] = {
  0xD0, 0x3F, 0x6D, 0x14, 0xA2, 0x6E, 0xCF, 0x08,
  0x11, 0x86, 0x62, 0xA7, 0x77, 0xB8, 0xEE, 0x9F
};
// test-ecckey-cert-tokens.txt
const uint8_t certificateKeyEcdsaX[32] = {
  0xDE, 0xA8, 0x3B, 0xB7, 0x56, 0xBF, 0x19, 0x3D,
  0xF5, 0x67, 0x8F, 0xA6, 0x0D, 0x4E, 0xE8, 0x56,
  0xE3, 0x5D, 0xD5, 0x68, 0xE1, 0x0F, 0xE7, 0x92,
  0x9E, 0xFD, 0xF2, 0x73, 0x38, 0x15, 0xEE, 0x8E
};
const uint8_t certificateKeyEcdsaY[32] = {
  0x8D, 0xA0, 0xEC, 0x6B, 0x06, 0xC1, 0xC3, 0x5C,
  0xF2, 0xF4, 0xCC, 0x61, 0xC4, 0x6B, 0x16, 0xDF,
  0x52, 0xFD, 0xAA, 0x04, 0x03, 0xA7, 0x62, 0xE6,
  0x17, 0x4D, 0x22, 0x3F, 0x1E, 0x1D, 0xC9, 0x3D
};
// SE_certificate.bin
// a603 7a0e 503f fcf8 158b baa2 c61e db5f
// 527b 4df0 4fb6 4534 f14c f853 ebe2 bce5
// 9af0 f528 bc0a 38f1 45a7 343a 1433 4c5f
// 5595 340a 468d 93aa 05c1 aa7f 2956 177d
const uint8_t certificateSignatureR[32] = {
  0xa6, 0x03, 0x7a, 0x0e, 0x50, 0x3f, 0xfc, 0xf8,
  0x15, 0x8b, 0xba, 0xa2, 0xc6, 0x1e, 0xdb, 0x5f,
  0x52, 0x7b, 0x4d, 0xf0, 0x4f, 0xb6, 0x45, 0x34,
  0xf1, 0x4c, 0xf8, 0x53, 0xeb, 0xe2, 0xbc, 0xe5
};
const uint8_t certificateSignatureS[32] = {
  0x9a, 0xf0, 0xf5, 0x28, 0xbc, 0x0a, 0x38, 0xf1,
  0x45, 0xa7, 0x34, 0x3a, 0x14, 0x33, 0x4c, 0x5f,
  0x55, 0x95, 0x34, 0x0a, 0x46, 0x8d, 0x93, 0xaa,
  0x05, 0xc1, 0xaa, 0x7f, 0x29, 0x56, 0x17, 0x7d
};
const uint8_t zeros[32] = {
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};
#endif

#if defined(GBL_WITH_HIGHER_CERT_VERSION)
#define CERTIFICATE_VERSION 998
#elif defined(GBL_WITH_LOWER_CERT_VERSION)
#define CERTIFICATE_VERSION 1000
#endif

const ApplicationCertificate_t sl_app_certificate = {
  .structVersion = APPLICATION_CERTIFICATE_VERSION,
  .flags = { 0U },
  .key = { // test-tokens.txt
    0x28, 0x3d, 0x7f, 0x16, 0xf3, 0xc8, 0x26, 0x91, 0x95, 0x86, 0x45,
    0x70, 0x8b, 0xbd, 0x7a, 0x98, 0x81, 0x28, 0x44, 0xc6, 0x3d, 0x74,
    0x53, 0xb6, 0xda, 0x70, 0x3d, 0xcd, 0xd2, 0x0f, 0x84, 0xad, 0xcb,
    0x61, 0xf6, 0x61, 0x2f, 0x1b, 0x66, 0x72, 0xcb, 0xb1, 0xe6, 0xf3,
    0x3c, 0x47, 0xb4, 0x57, 0x84, 0xbb, 0xd4, 0x48, 0x43, 0xa2, 0xf6,
    0xbe, 0xa9, 0xb6, 0xaf, 0xdc, 0xd8, 0x76, 0xb8, 0x71
  },
  .version = CERTIFICATE_VERSION,
  .signature = { 0U },
};

const ApplicationProperties_t sl_app_properties = {
  .magic = APPLICATION_PROPERTIES_MAGIC,
  .structVersion = APPLICATION_PROPERTIES_VERSION,
  .signatureType = APPLICATION_SIGNATURE_ECDSA_P256,
  .signatureLocation = 0xFFFFFFFF,
  .app = {
    .type = APPLICATION_TYPE_BOOTLOADER,
    .version = 1,
    .capabilities = 0x01020304,
    .productId = { 0 }
  },
  .cert = (ApplicationCertificate_t *)&sl_app_certificate,
};

// This gives us mainBootloaderTable->startOfAppSpace = 0, which is used by
// bootload_verifyApplication().
#ifdef __ICCARM__
__root
#endif

MainBootloaderTable_t mainBootloaderTableImpl;
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

  UnityBeginGroup("GBL_PARSER_TAG_SE");

  RUN_TEST(test_gbl_parser_extracts_se_tag, __LINE__);
  RUN_TEST(test_gbl_parser_ignores_se_tag, __LINE__);

  UnityEnd();

#if ENABLE_TEST_COVERAGE
  __gcov_flush();
#endif

  UnityPrint("ENDSWO");

  return 0;
}

// --------------------------------
// Test implementations

#define GBL_HEADER_SIZE 16U
#define GBL_END_SIZE    12U
#define GBL_SIG_SIZE    72U
#define GBL_CERT_SIZE   144U
#define GBL_SE_SIZE     (sizeof(gbl_file) - GBL_HEADER_SIZE - GBL_END_SIZE - GBL_CERT_SIZE - GBL_SIG_SIZE)
static uint8_t seen_word[1500];
static char str[128];

void callback(uint32_t offset, uint8_t *buffer, size_t length, void *ctx)
{
  (void) buffer;
  (void) ctx;
  size_t actual_length = length;

  if (offset + length > GBL_SE_SIZE) {
    // The last word might be a partial one, clamp the length to the actual byte size for testing
    actual_length = GBL_SE_SIZE - offset;
    TEST_ASSERT(actual_length < 4U);
  }

  // Check data against original binary
  sprintf(str, "offset %lu", offset);
  TEST_ASSERT_EQUAL_HEX8_ARRAY_MESSAGE(&gbl_file[offset + GBL_HEADER_SIZE], buffer, actual_length, str);

  // Store the fact that we've checked this word in a bitmask
  size_t bit_start = (offset / 4) % 8;
  for (size_t i = bit_start; i < bit_start + (length / 4); i++) {
    // Assert that we haven't seen this word before
    TEST_ASSERT_BIT_LOW(i % 8, seen_word[offset / 32 + i / 8]);
    // Flag that the word was seen
    seen_word[offset / 32 + i / 8] |= 1 << (i % 8);
  }
}

void no_callback(uint32_t offset, uint8_t *buffer, size_t length, void *ctx)
{
  (void) offset;
  (void) buffer;
  (void) length;
  (void) ctx;
  TEST_ASSERT(false);
}

// The purpose of this test is to verify that the GBL parser will extract the
// SE image from a GBL file and return it in the bootloader callback. No other
// callbacks should be issued.
void test_gbl_parser_extracts_se_tag(void)
{
  int32_t retval = 0;
  ParserContext_t parserContext = { 0 };
  DecryptContext_t decryptContext = { 0 };
  AuthContext_t authContext = { 0 };
  const BootloaderParserCallbacks_t parserCallbacks = {
    NULL,
    NULL,
    NULL,
    &callback
  };
  ImageProperties_t imageProperties = { 0 };
  imageProperties.instructions = BTL_IMAGE_INSTRUCTION_SE;

  retval = parser_init(&parserContext,
                       &decryptContext,
                       &authContext,
                       0UL);
  TEST_ASSERT_EQUAL_HEX32(0UL, retval);

  retval = parser_parse((void*)&parserContext,
                        &imageProperties,
                        (uint8_t *)gbl_file,
                        sizeof(gbl_file),
                        &parserCallbacks);

#if defined(GBL_WITH_HIGHER_CERT_VERSION)
  // The GBL should be accpeted, since the MBL certificate version is higher to the GBL certificate version.
  TEST_ASSERT_EQUAL_HEX32(0UL, retval);

  if (!imageProperties.imageCompleted) {
    TEST_FAIL_MESSAGE("Image parsing not completed, but verification failed. Are the correct keys flashed?");
  }

  if (!imageProperties.imageVerified) {
    TEST_FAIL_MESSAGE("Image parsing completed, but verification failed. Are the correct keys flashed?");
  }

  // Assert that all words in the SE upgrade image were returned in the callback
  for (size_t i = 0; i < GBL_SE_SIZE / 32; i++) {
    sprintf(str, "(mask #%u)", i);
    TEST_ASSERT_BITS_HIGH_MESSAGE(0xFF, seen_word[i], str);
  }
  size_t last_idx = GBL_SE_SIZE / 32;
  uint8_t pattern = 0;
  for (size_t i = 0; i < ((GBL_SE_SIZE % 32) + 3) / 4; i++) {
    pattern |= 1 << i;
  }
  sprintf(str, "(mask #%u)", last_idx);
  TEST_ASSERT_BITS_MESSAGE(0xFF, pattern, seen_word[last_idx], str);

  // Check that we extracted the SE version number
  const uint32_t *se_version_number = (const uint32_t *)&gbl_file[GBL_HEADER_SIZE + 12];
  TEST_ASSERT(*se_version_number > 0);
  TEST_ASSERT_EQUAL_HEX32(*se_version_number, imageProperties.seUpgradeVersion);

  // Check Certificate
  TEST_ASSERT(parserContext.certificate.structVersion == 1); // Test gbl used for this test stores certificate with the version 999
  TEST_ASSERT(parserContext.certificate.version == 999); // Test gbl used for this test stores certificate with the version 999
  TEST_ASSERT_EQUAL_INT8_ARRAY(certificateKeyEcdsaX, parserContext.certificate.key, 32);
  TEST_ASSERT_EQUAL_INT8_ARRAY(certificateKeyEcdsaY, &parserContext.certificate.key[32], 32);
  TEST_ASSERT_EQUAL_INT8_ARRAY(certificateSignatureR, parserContext.certificate.signature, 32);
  TEST_ASSERT_EQUAL_INT8_ARRAY(certificateSignatureS, &parserContext.certificate.signature[32], 32);
#elif defined(GBL_WITH_LOWER_CERT_VERSION)
  // The GBL should be rejected, since the MBL certificate version is lower than the GBL certificate version.
  TEST_ASSERT_EQUAL_HEX32(BOOTLOADER_ERROR_PARSER_SIGNATURE, retval);

  if (imageProperties.imageCompleted) {
    TEST_FAIL_MESSAGE("Image parsing should not been completed, since the cert version check failed.");
  }

  if (imageProperties.imageVerified) {
    TEST_FAIL_MESSAGE("Image verification should not been passed, since the cert version check failed");
  }
#endif
}

// The purpose of this test is to verify that the GBL parser will not extract
// the SE image from a GBL file when the SE instruction is not given to the
// parser.
void test_gbl_parser_ignores_se_tag(void)
{
  int32_t retval = 0;
  ParserContext_t parserContext = { 0 };
  DecryptContext_t decryptContext = { 0 };
  AuthContext_t authContext = { 0 };
  const BootloaderParserCallbacks_t parserCallbacks = {
    NULL,
    NULL,
    NULL,
    &no_callback
  };
  ImageProperties_t imageProperties = { 0 };

  retval = parser_init(&parserContext,
                       &decryptContext,
                       &authContext,
                       0UL);
  TEST_ASSERT_EQUAL_HEX32(0UL, retval);

  retval = parser_parse((void*)&parserContext,
                        &imageProperties,
                        (uint8_t *)gbl_file,
                        sizeof(gbl_file),
                        &parserCallbacks);
#if defined(GBL_WITH_HIGHER_CERT_VERSION)
  TEST_ASSERT_EQUAL_HEX32(0UL, retval);

  if (!imageProperties.imageCompleted) {
    TEST_FAIL_MESSAGE("Image parsing not completed, but verification failed. Are the correct keys flashed?");
  }

  if (!imageProperties.imageVerified) {
    TEST_FAIL_MESSAGE("Image parsing completed, but verification failed. Are the correct keys flashed?");
  }

  // Check that we extracted the SE version number even though we didn't receive callbacks
  const uint32_t *se_version_number = (const uint32_t *)&gbl_file[GBL_HEADER_SIZE + 12];
  TEST_ASSERT(*se_version_number > 0);
  TEST_ASSERT_EQUAL_HEX32(*se_version_number, imageProperties.seUpgradeVersion);

  // Check Certificate
  TEST_ASSERT(parserContext.certificate.structVersion == 1); // Test gbl used for this test stores certificate with the version 999
  TEST_ASSERT(parserContext.certificate.version == 999); // Test gbl used for this test stores certificate with the version 999
  TEST_ASSERT_EQUAL_INT8_ARRAY(certificateKeyEcdsaX, parserContext.certificate.key, 32);
  TEST_ASSERT_EQUAL_INT8_ARRAY(certificateKeyEcdsaY, &parserContext.certificate.key[32], 32);
  TEST_ASSERT_EQUAL_INT8_ARRAY(certificateSignatureR, parserContext.certificate.signature, 32);
  TEST_ASSERT_EQUAL_INT8_ARRAY(certificateSignatureS, &parserContext.certificate.signature[32], 32);
#elif defined (GBL_WITH_LOWER_CERT_VERSION)
  TEST_ASSERT_EQUAL_HEX32(BOOTLOADER_ERROR_PARSER_SIGNATURE, retval);

  if (imageProperties.imageCompleted) {
    TEST_FAIL_MESSAGE("Image parsing should not been completed, since the cert version check failed.");
  }

  if (imageProperties.imageVerified) {
    TEST_FAIL_MESSAGE("Image verification should not been passed, since the cert version check failed");
  }
#endif
}
