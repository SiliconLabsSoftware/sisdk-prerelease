/***************************************************************************//**
 * @file ebl_parser_components_test.c
 * @brief Defines unit tests for a selection of individual parser components.
 * Sanity checks for correct parsing.
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
#include "api/application_properties.h"
#include "sl_memory_manager.h"
#include "em_device.h"
#include "em_chip.h"
#include "em_cmu.h"
#include "em_gpio.h"
#include "em_common.h"

#include "core/btl_parse.h"

// Get EBL array
#include "ebl/testv2.c"
#include "ebl/testv3Encrypted.c"
#include "ebl/bootloader-and-signed-app.gbl.c"

#include "parser/gbl/btl_gbl_parser.h"
#include "parser/gbl/btl_gbl_format.h"
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

MainBootloaderTable_t mainBootloaderTableImpl;
MainBootloaderTable_t *mainBootloaderTable = &mainBootloaderTableImpl;

#if defined(BOOTLOADER_HAS_FIRST_STAGE)
FirstBootloaderTable_t firstBootloaderTableImpl;
FirstBootloaderTable_t *firstBootloaderTable = &firstBootloaderTableImpl;
#endif

static uint32_t bootloaderCallbackOffset = 0;
static uint32_t bootloaderCallbackWord = 0;
static uint32_t bootloaderCallbackLength = 0;
static bool gotBootloaderCallback = false;
static bool gblfileIsModified = false;

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
}

void bootloaderCallback(uint32_t offset, uint8_t *buffer, size_t length, void *ctx)
{
  (void) ctx;
  gotBootloaderCallback = true;
  bootloaderCallbackOffset = offset;
  bootloaderCallbackLength = length;
  bootloaderCallbackWord = *((uint32_t*)buffer);
}

// This test ensures that the parser can correctly determine the EBL file version and that appropriate flags were set.
void testDetermineEblVersion(void)
{
  uint32_t retval = 0;
  ParserContext_t parserContext;
  DecryptContext_t decryptContext;
  AuthContext_t authContext;
  const BootloaderParserCallbacks_t parserCallbacks = { (void*)&parserContext, parseCallback, parseCallback, parseCallback };
  ImageProperties_t imageProperties = { 0 };

  // EBLv2, no longer supported
  retval = parser_init(&parserContext,
                       &decryptContext,
                       &authContext,
                       PARSER_FLAG_PARSE_CUSTOM_TAGS);
  TEST_ASSERT_EQUAL_HEX32(BOOTLOADER_OK, retval);

  retval = parser_parse((void*)&parserContext, &imageProperties, (uint8_t*)&(eblv2[0]), 8, &parserCallbacks);
  TEST_ASSERT_EQUAL_HEX32(BOOTLOADER_ERROR_PARSER_UNEXPECTED, retval);

  // EBLv3
  retval = parser_init(&parserContext,
                       &decryptContext,
                       &authContext,
                       0);
  TEST_ASSERT_EQUAL_HEX32(BOOTLOADER_OK, retval);

  retval = parser_parse((void*)&parserContext, &imageProperties, (uint8_t*)&(eblv3Encrypted[0]), 8, &parserCallbacks);

  TEST_ASSERT_EQUAL_HEX32(BOOTLOADER_OK, retval);
  TEST_ASSERT_EQUAL(GblParserStateHeader, parserContext.internalState);
  TEST_ASSERT_EQUAL(8, parserContext.lengthOfTag);
}

// Test correct parsing of EBLv3 header
void testParseEblHeaderV3(void)
{
  size_t offset = 0;
  uint32_t retval = 0;
  ParserContext_t parserContext;
  DecryptContext_t decryptContext;
  AuthContext_t authContext;
  const BootloaderParserCallbacks_t parserCallbacks = { (void*)&parserContext, parseCallback, parseCallback, parseCallback };
  ImageProperties_t imageProperties = { 0 };

  retval = parser_init(&parserContext,
                       &decryptContext,
                       &authContext,
                       0);
  TEST_ASSERT_EQUAL_HEX32(BOOTLOADER_OK, retval);

  retval = parser_parse((void*)&parserContext, &imageProperties, (uint8_t*)&(eblv3Encrypted[offset]), 8, &parserCallbacks);
  TEST_ASSERT_EQUAL_HEX32(BOOTLOADER_OK, retval);
  TEST_ASSERT_EQUAL(GblParserStateHeader, parserContext.internalState);
  TEST_ASSERT_EQUAL(8, parserContext.lengthOfTag);
  offset += 8 / sizeof(eblv3Encrypted[0]);

  retval = parser_parse((void*)&parserContext, &imageProperties, (uint8_t*)&(eblv3Encrypted[offset]), 8, &parserCallbacks);

#if defined(BTL_PARSER_NO_SUPPORT_ENCRYPTION)
  TEST_ASSERT_EQUAL_HEX32(BOOTLOADER_ERROR_PARSER_FILETYPE, retval);
#else
  TEST_ASSERT_EQUAL_HEX32(BOOTLOADER_OK, retval);
  TEST_ASSERT_EQUAL(GblParserStateIdle, parserContext.internalState);
#endif
}

// Test symmetric encryption functionality:
// - Initialize encryption context
// - Check that the correct flags are set
// - Check that decryption results in expected known plaintext
void testSymmetricEncryption(void)
{
  size_t offset = 0;
  size_t nbytes;
  uint32_t retval = 0;
  ParserContext_t parserContext;
  DecryptContext_t decryptContext;
  AuthContext_t authContext;
  const BootloaderParserCallbacks_t parserCallbacks = { (void*)&parserContext, parseCallback, parseCallback, parseCallback };
  ImageProperties_t imageProperties = { 0 };

  retval = parser_init(&parserContext,
                       &decryptContext,
                       &authContext,
                       0);
  TEST_ASSERT_EQUAL_HEX32(BOOTLOADER_OK, retval);

  // Parse EBLv3 header and Encryption init tag
  nbytes = sizeof(GblHeader_t) + sizeof(GblEncryptionInitAesCcm_t);
  while (offset < nbytes / sizeof(eblv3Encrypted[0])) {
    retval = parser_parse((void*)&parserContext, &imageProperties, (uint8_t*)&(eblv3Encrypted[offset]), 4, &parserCallbacks);
    offset += 4 / sizeof(eblv3Encrypted[0]);
  }

  TEST_ASSERT_EQUAL_HEX32(BOOTLOADER_OK, retval);
  TEST_ASSERT_TRUE(parserContext.flags & PARSER_FLAG_ENCRYPTED);
  TEST_ASSERT_EQUAL(GblParserStateIdle, parserContext.internalState);

  UnityPrintf("AES counter: ");
  for (int i = 0; i < 16; i++) {
    UnityPrintf("%02x", decryptContext.aesCtr.counter[i]);
  }
  UnityPrintf("\n");

  // AES CCM Flags
  TEST_ASSERT_EQUAL_HEX8(0x02U, decryptContext.aesCtr.counter[0]);
  // AES CCM l(m), 3 bytes stored as big-endian
  TEST_ASSERT_EQUAL_HEX8(0x00U, decryptContext.aesCtr.counter[13]);
  TEST_ASSERT_EQUAL_HEX8(0x00U, decryptContext.aesCtr.counter[14]);
  TEST_ASSERT_EQUAL_HEX8(0x01U, decryptContext.aesCtr.counter[15]);

  // Parse Encryption data tag and decrypt next tag header (Application tag in plaintext)
  nbytes += 2 * sizeof(GblTagHeader_t);
  while (offset < nbytes / sizeof(eblv3Encrypted[0])) {
    retval = parser_parse((void*)&parserContext, &imageProperties, (uint8_t*)&(eblv3Encrypted[offset]), 4, &parserCallbacks);
    offset += 4 / sizeof(eblv3Encrypted[0]);
  }

  TEST_ASSERT_EQUAL_HEX32(BOOTLOADER_OK, retval);
  TEST_ASSERT_TRUE(parserContext.inEncryptedContainer);
  TEST_ASSERT_EQUAL(GblParserStateApplication, parserContext.internalState);
}

// Test correct parsing of Application tag
void testParseApplicationTag(void)
{
  size_t offset = 0;
  uint32_t retval = 0;
  ParserContext_t parserContext;
  DecryptContext_t decryptContext;
  AuthContext_t authContext;
  const BootloaderParserCallbacks_t parserCallbacks = { (void*)&parserContext, parseCallback, parseCallback, parseCallback };
  ImageProperties_t imageProperties = { 0 };

  retval = parser_init(&parserContext,
                       &decryptContext,
                       &authContext,
                       0);
  TEST_ASSERT_EQUAL_HEX32(BOOTLOADER_OK, retval);

  // Skip EBLv3 header, parse Application tag header
  offset = sizeof(GblHeader_t);
  parserContext.internalState = GblParserStateIdle;
  retval = parser_parse((void*)&parserContext,
                        &imageProperties,
                        (uint8_t*)&(bl_and_signed_app_gblfile[offset]),
                        sizeof(GblTagHeader_t),
                        &parserCallbacks);
  offset += sizeof(GblTagHeader_t);

  TEST_ASSERT_EQUAL_HEX32(BOOTLOADER_OK, retval);
  TEST_ASSERT_EQUAL(GblParserStateApplication, parserContext.internalState);

  // Parse remaining body of Application tag
  retval = parser_parse((void*)&parserContext,
                        &imageProperties,
                        (uint8_t*)&(bl_and_signed_app_gblfile[offset]),
                        sizeof(GblApplication_t) - sizeof(GblTagHeader_t),
                        &parserCallbacks);
  offset += sizeof(GblApplication_t) - sizeof(GblTagHeader_t);

  TEST_ASSERT_EQUAL_HEX32(BOOTLOADER_OK, retval);
  TEST_ASSERT_EQUAL(GblParserStateIdle, parserContext.internalState);
  TEST_ASSERT_TRUE(imageProperties.contents & BTL_IMAGE_CONTENT_APPLICATION);

  // ApplicationData_t
  TEST_ASSERT_EQUAL_HEX32(APPLICATION_TYPE_BLUETOOTH_APP, imageProperties.application.type);
  TEST_ASSERT_EQUAL_HEX32(0x12345678, imageProperties.application.version);
  TEST_ASSERT_TRUE(!memcmp(imageProperties.application.productId,
                           "\x00\x11\x22\x33\x44\x55\x66\x77\x88\x99\xAA\xBB\xCC\xDD\xEE\xFF",
                           16));
}

// Test correct parsing of Bootloader
void testParseBootloader(void)
{
  size_t offset = 0;
  size_t nbytes;
  uint32_t retval = 0;
  ParserContext_t parserContext;
  DecryptContext_t decryptContext;
  AuthContext_t authContext;
  const BootloaderParserCallbacks_t parserCallbacks = { (void*)&parserContext, parseCallback, parseCallback, bootloaderCallback };
  ImageProperties_t imageProperties = { 0 };
  imageProperties.instructions |= BTL_IMAGE_INSTRUCTION_BOOTLOADER;

  retval = parser_init(&parserContext,
                       &decryptContext,
                       &authContext,
                       0);
  TEST_ASSERT_EQUAL_HEX32(BOOTLOADER_OK, retval);

  // Skip to Bootloader tag and parse tag header
  offset = GBLFILE_BOOTLOADER_OFFSET_START;
  parserContext.internalState = GblParserStateIdle;
  retval = parser_parse((void*)&parserContext,
                        &imageProperties,
                        (uint8_t*)&(bl_and_signed_app_gblfile[offset]),
                        sizeof(GblTagHeader_t),
                        &parserCallbacks);
  offset += sizeof(GblTagHeader_t);

  TEST_ASSERT_EQUAL_HEX32(BOOTLOADER_OK, retval);
  TEST_ASSERT_EQUAL(GblParserStateBootloader, parserContext.internalState);

  // Parse remaining body of Bootloader tag
  nbytes = sizeof(GblBootloader_t) - sizeof(GblTagHeader_t) - sizeof(uint8_t*);
  retval = parser_parse((void*)&parserContext,
                        &imageProperties,
                        (uint8_t*)&(bl_and_signed_app_gblfile[offset]),
                        nbytes,
                        &parserCallbacks);
  offset += nbytes;

  TEST_ASSERT_EQUAL_HEX32(BOOTLOADER_OK, retval);
  TEST_ASSERT_EQUAL(GblParserStateBootloaderData, parserContext.internalState);
  TEST_ASSERT_TRUE(imageProperties.contents & BTL_IMAGE_CONTENT_BOOTLOADER);
  TEST_ASSERT_TRUE(parserContext.receivedFlags & BTL_PARSER_RECEIVED_BOOTLOADER);

  TEST_ASSERT_EQUAL_HEX32(0xFFFFFFFF, *((uint32_t*)parserContext.withheldBootloaderVectors));

  // Parse Bootloader data
  while (offset < GBLFILE_BOOTLOADER_OFFSET_END) {
    retval = parser_parse((void*)&parserContext, &imageProperties, (uint8_t*)&(bl_and_signed_app_gblfile[offset]), 4, &parserCallbacks);
    offset += 4;
    TEST_ASSERT_EQUAL_HEX32(BOOTLOADER_OK, retval);
  }

  TEST_ASSERT_TRUE(gotBootloaderCallback);
  TEST_ASSERT_EQUAL_HEX32(GBLFILE_BOOTLOADER_PC, *((uint32_t*)parserContext.withheldBootloaderVectors));
}

// Test correct parsing of Signature and End tag
void testParseSignatureAndFinalize(void)
{
  size_t offset = 0;
  uint32_t retval = 0;
  ParserContext_t parserContext;
  DecryptContext_t decryptContext;
  AuthContext_t authContext;
  const BootloaderParserCallbacks_t parserCallbacks = { (void*)&parserContext, parseCallback, parseCallback, bootloaderCallback };
  ImageProperties_t imageProperties = { 0 };
  imageProperties.instructions |= BTL_IMAGE_INSTRUCTION_BOOTLOADER;

  retval = parser_init(&parserContext,
                       &decryptContext,
                       &authContext,
                       PARSER_FLAG_PARSE_CUSTOM_TAGS);
  TEST_ASSERT_EQUAL_HEX32(BOOTLOADER_OK, retval);

  // Parse file contents before Signature tag
  while (offset < GBLFILE_SIGNATURE_OFFSET_START) {
    retval = parser_parse((void*)&parserContext, &imageProperties, (uint8_t*)&(bl_and_signed_app_gblfile[offset]), 4, &parserCallbacks);
    offset += 4;
    TEST_ASSERT_EQUAL_HEX32(BOOTLOADER_OK, retval);
  }

  TEST_ASSERT_EQUAL(GblParserStateIdle, parserContext.internalState);

  // Parse Signature tag
  TEST_ASSERT_FALSE(imageProperties.imageVerified);
  TEST_ASSERT_FALSE(parserContext.gotSignature);

  offset = GBLFILE_SIGNATURE_OFFSET_START;
  retval = parser_parse((void*)&parserContext,
                        &imageProperties,
                        (uint8_t*)&(bl_and_signed_app_gblfile[offset]),
                        sizeof(GblTagHeader_t),
                        &parserCallbacks);
  offset += sizeof(GblTagHeader_t);

  TEST_ASSERT_EQUAL_HEX32(BOOTLOADER_OK, retval);
  TEST_ASSERT_EQUAL(GblParserStateSignature, parserContext.internalState);

  retval = parser_parse((void*)&parserContext,
                        &imageProperties,
                        (uint8_t*)&(bl_and_signed_app_gblfile[offset]),
                        sizeof(GblSignatureEcdsaP256_t) - sizeof(GblTagHeader_t),
                        &parserCallbacks);
  offset += sizeof(GblSignatureEcdsaP256_t) - sizeof(GblTagHeader_t);

  if (gblfileIsModified == false) {
    TEST_ASSERT_EQUAL_HEX32(BOOTLOADER_OK, retval);
    TEST_ASSERT_EQUAL(GblParserStateIdle, parserContext.internalState);
    TEST_ASSERT_TRUE(imageProperties.imageVerified);
    TEST_ASSERT_TRUE(parserContext.gotSignature);

    // Parse End tag and finalize
    retval = parser_parse((void*)&parserContext,
                          &imageProperties,
                          (uint8_t*)&(bl_and_signed_app_gblfile[offset]),
                          sizeof(GblTagHeader_t),
                          &parserCallbacks);
    offset += sizeof(GblTagHeader_t);

    TEST_ASSERT_EQUAL_HEX32(BOOTLOADER_OK, retval);
    TEST_ASSERT_EQUAL(GblParserStateFinalize, parserContext.internalState);

    retval = parser_parse((void*)&parserContext,
                          &imageProperties,
                          (uint8_t*)&(bl_and_signed_app_gblfile[offset]),
                          sizeof(GblEnd_t) - sizeof(GblTagHeader_t),
                          &parserCallbacks);
    offset += sizeof(GblEnd_t) - sizeof(GblTagHeader_t);

    TEST_ASSERT_EQUAL_HEX32(BOOTLOADER_OK, retval);
    TEST_ASSERT_EQUAL(GblParserStateDone, parserContext.internalState);
    TEST_ASSERT_TRUE(imageProperties.imageCompleted);

    // Check that withheld bootloader PC is written (passed to callback) during final stage of parsing
    TEST_ASSERT_EQUAL(4U, bootloaderCallbackOffset);
    TEST_ASSERT_EQUAL_HEX32(GBLFILE_BOOTLOADER_PC, bootloaderCallbackWord);
    TEST_ASSERT_EQUAL(4U, bootloaderCallbackLength);
  } else {
    TEST_ASSERT_EQUAL_HEX32(BOOTLOADER_ERROR_PARSER_SIGNATURE, retval);
    TEST_ASSERT_EQUAL(GblParserStateError, parserContext.internalState);
    TEST_ASSERT_FALSE(imageProperties.imageVerified);
    TEST_ASSERT_FALSE(parserContext.gotSignature);

    // Try to parse End tag and finalize. Should fail.
    retval = parser_parse((void*)&parserContext,
                          &imageProperties,
                          (uint8_t*)&(bl_and_signed_app_gblfile[offset]),
                          sizeof(GblEnd_t),
                          &parserCallbacks);
    offset += sizeof(GblEnd_t);

    TEST_ASSERT_EQUAL_HEX32(BOOTLOADER_ERROR_PARSER_EOF, retval);
    TEST_ASSERT_EQUAL(GblParserStateError, parserContext.internalState);
    TEST_ASSERT_FALSE(imageProperties.imageCompleted);

    // Check that withheld bootloader PC is NOT written (passed to callback) during final stage of parsing
    TEST_ASSERT_NOT_EQUAL(4U, bootloaderCallbackOffset);
    TEST_ASSERT_NOT_EQUAL(GBLFILE_BOOTLOADER_PC, bootloaderCallbackWord);
  }
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

  UnityBeginGroup("EBL_PARSER_COMPONENTS");

  RUN_TEST(testDetermineEblVersion, __LINE__);
  RUN_TEST(testParseEblHeaderV3, __LINE__);
#if !defined(BTL_PARSER_NO_SUPPORT_ENCRYPTION)
  RUN_TEST(testSymmetricEncryption, __LINE__);
#endif
  RUN_TEST(testParseApplicationTag, __LINE__);

  uint32_t *gblBootloaderImageBaseAddress = (uint32_t*)&bl_and_signed_app_gblfile[GBLFILE_BOOTLOADER_BASE_ADDRESS_OFFSET];
  uint32_t newBootloaderImageBaseAddress = FLASH_BASE;
  uint32_t oldBootloaderImageBaseAddress = *gblBootloaderImageBaseAddress;

  if (newBootloaderImageBaseAddress != oldBootloaderImageBaseAddress) {
    // Patch base address of bootloader image in GBL file to FLASH_BASE
    *gblBootloaderImageBaseAddress = newBootloaderImageBaseAddress;
    gblfileIsModified = true;
  }
#if defined(BOOTLOADER_HAS_FIRST_STAGE)
  firstBootloaderTable->header.type = BOOTLOADER_MAGIC_FIRST_STAGE;
  firstBootloaderTable->mainBootloader = newBootloaderImageBaseAddress; // Must match value in the GBL test file
#endif
  RUN_TEST(testParseBootloader, __LINE__);
  RUN_TEST(testParseSignatureAndFinalize, __LINE__);

  UnityEnd();

#if ENABLE_TEST_COVERAGE
  __gcov_flush();
#endif

  UnityPrint("ENDSWO");
}
