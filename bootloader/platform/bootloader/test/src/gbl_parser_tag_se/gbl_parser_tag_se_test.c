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
#include "api/btl_interface_storage.h"

#include "core/btl_parse.h"
#include "core/flash/btl_internal_flash.h"

#include "parser/gbl/btl_gbl_parser.h"
#include "parser/gbl/btl_gbl_format.h"

#include "security/btl_security_types.h"

#include "storage/btl_storage.h"
#include "sl_memory_manager.h"

#include "bin/SE_main.elf.gbl.c"
#if defined(BOOTLOADER_SE_UPGRADE_NO_STAGING) \
  && (BOOTLOADER_SE_UPGRADE_NO_STAGING == 1)
#include "bin/SE_no_staging_dummy_data.gbl.c"
#endif

// --------------------------------
// Test prototypes
void test_gbl_parser_extracts_se_tag(void);
void test_gbl_parser_ignores_se_tag(void);

#if defined(BOOTLOADER_SE_UPGRADE_NO_STAGING) \
  && (BOOTLOADER_SE_UPGRADE_NO_STAGING == 1)
void test_gbl_parser_se_tag_no_staging(void);
void test_gbl_parser_se_upgrade_dry_run(void);
#endif

#if defined(_SILICON_LABS_32B_SERIES_2)
const uint8_t bootloaderKeyAes[16] = {
  0xD0, 0x3F, 0x6D, 0x14, 0xA2, 0x6E, 0xCF, 0x08,
  0x11, 0x86, 0x62, 0xA7, 0x77, 0xB8, 0xEE, 0x9F
};
const uint8_t bootloaderKeyEcdsaX[32] = {
  0x28, 0x3d, 0x7f, 0x16, 0xf3, 0xc8, 0x26, 0x91,
  0x95, 0x86, 0x45, 0x70, 0x8b, 0xbd, 0x7a, 0x98,
  0x81, 0x28, 0x44, 0xc6, 0x3d, 0x74, 0x53, 0xb6,
  0xda, 0x70, 0x3d, 0xcd, 0xd2, 0x0f, 0x84, 0xad
};
const uint8_t bootloaderKeyEcdsaY[32] = {
  0xcb, 0x61, 0xf6, 0x61, 0x2f, 0x1b, 0x66, 0x72,
  0xcb, 0xb1, 0xe6, 0xf3, 0x3c, 0x47, 0xb4, 0x57,
  0x84, 0xbb, 0xd4, 0x48, 0x43, 0xa2, 0xf6, 0xbe,
  0xa9, 0xb6, 0xaf, 0xdc, 0xd8, 0x76, 0xb8, 0x71
};
#endif

// This is to bypass address check for btl app properties pointer
MainBootloaderTable_t mainBootloaderTableImpl = {
  .startOfAppSpace = (BareBootTable_t *) 0x4000UL
};
MainBootloaderTable_t *mainBootloaderTable = &mainBootloaderTableImpl;

// Implement and override SE upgrade functions for testing purposes
bool bootload_checkSeUpgradeVersion(uint32_t upgradeVersion)
{
  (void) upgradeVersion;
  return true;
}

bool bootload_commitSeUpgrade(uint32_t upgradeAddress)
{
  // Check for SE upgrade header magic
  if (*((uint32_t *) upgradeAddress) != GBL_TAG_ID_SE_UPGRADE) {
    return false;
  }

  return true;
}

// Implement parser callbacks
bool parser_applicationUpgradeValidCallback(ApplicationData_t *app)
{
  (void) app;
  // By default, all applications are considered valid
  return true;
}

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

#if defined(BOOTLOADER_SE_UPGRADE_NO_STAGING) \
  && (BOOTLOADER_SE_UPGRADE_NO_STAGING == 1)  \
  && (defined(SEMAILBOX_PRESENT) || defined(CRYPTOACC_PRESENT))
  RUN_TEST(test_gbl_parser_se_tag_no_staging, __LINE__);
  RUN_TEST(test_gbl_parser_se_upgrade_dry_run, __LINE__);
#else
  RUN_TEST(test_gbl_parser_ignores_se_tag, __LINE__);
  RUN_TEST(test_gbl_parser_extracts_se_tag, __LINE__);
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

#define GBL_HEADER_SIZE 16U
#define GBL_END_SIZE    12U
#define GBL_SE_SIZE     (sizeof(gbl_file) - GBL_HEADER_SIZE - GBL_END_SIZE)
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
}

#if defined(BOOTLOADER_SE_UPGRADE_NO_STAGING) \
  && (BOOTLOADER_SE_UPGRADE_NO_STAGING == 1)
// --------------------------------
// Static functions

static void flashData(uint32_t address,
                      uint8_t  data[],
                      size_t   length)
{
  const uint32_t pageSize = (uint32_t)FLASH_PAGE_SIZE;

  // Erase the page if write starts at a page boundary
  if (address % pageSize == 0UL) {
    flash_erasePage(address);
  }

  // Erase all pages that start inside the write range
  for (uint32_t pageAddress = (address + pageSize) & ~(pageSize - 1UL);
       pageAddress < (address + length);
       pageAddress += pageSize) {
    flash_erasePage(pageAddress);
  }

  flash_writeBuffer(address, data, length);
}

// Test correct operation when BOOTLOADER_SE_UPGRADE_NO_STAGING is enabled.
// 1) The bootloaderCallback should not be called (no data should be written to the staging area)
// 2) Check that the offset of the SE tag inside the GBL is calculated correctly
void test_gbl_parser_se_tag_no_staging(void)
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
  imageProperties.instructions = BTL_IMAGE_INSTRUCTION_SE;

  retval = parser_init(&parserContext,
                       &decryptContext,
                       &authContext,
                       0UL);
  TEST_ASSERT_EQUAL_HEX32(0UL, retval);

  retval = parser_parse((void*)&parserContext,
                        &imageProperties,
                        (uint8_t *)gbl_file_se_dummy,
                        sizeof(gbl_file_se_dummy),
                        &parserCallbacks);
  TEST_ASSERT_EQUAL_HEX32(0UL, retval);

  if (!imageProperties.imageCompleted) {
    TEST_FAIL_MESSAGE("Image parsing not completed, but verification failed. Are the correct keys flashed?");
  }

  if (!imageProperties.imageVerified) {
    TEST_FAIL_MESSAGE("Image parsing completed, but verification failed. Are the correct keys flashed?");
  }

  // Check that we extracted the SE version number
  const uint32_t *se_version_number = (const uint32_t *)&gbl_file_se_dummy[GBL_HEADER_SIZE + 12];
  TEST_ASSERT(*se_version_number == 0x00001337);
  TEST_ASSERT_EQUAL_HEX32(*se_version_number, imageProperties.seUpgradeVersion);

  // Check that the offsets inside the GBL are calculated correctly
  const uint32_t correctOffsetSeTag = 0x00000010UL;
  TEST_ASSERT_EQUAL_HEX32(correctOffsetSeTag, parserContext.offsetOfSeUpgradeTag);
  TEST_ASSERT_EQUAL_HEX32(GBL_TAG_ID_SE_UPGRADE, *((uint32_t*) &gbl_file_se_dummy[parserContext.offsetOfSeUpgradeTag]));
  TEST_ASSERT_EQUAL_HEX32(sizeof(gbl_file_se_dummy) - 3, parserContext.offsetInGbl); // 3 bytes of padding at the end
}

// Test correct operation and calculation of the upgrade address in storage
// by performing a dry run of the SE upgrade.
void test_gbl_parser_se_upgrade_dry_run(void)
{
  // Write GBL file to storage slot 1
  int32_t ret;
  const uint32_t slotId = 1;
  const BootloaderStorageSlot_t storageSlots[] = BTL_STORAGE_SLOTS;
  int32_t slotIds[] = { slotId };

  ret = storage_setBootloadList(slotIds, 1);
  TEST_ASSERT_EQUAL_HEX32(BOOTLOADER_OK, ret);

//  ret = storage_eraseSlot(slotId);
//  TEST_ASSERT_EQUAL_HEX32(BOOTLOADER_OK, ret);
//  ret = storage_writeSlot(slotId, 0, (uint8_t *) gbl_file_se_dummy, sizeof(gbl_file_se_dummy));
//  TEST_ASSERT_EQUAL_HEX32(BOOTLOADER_OK, ret);
  flashData(storageSlots[slotId].address, (uint8_t *) gbl_file_se_dummy, sizeof(gbl_file_se_dummy));
  TEST_ASSERT_EQUAL_HEX32(*((uint32_t *) gbl_file_se_dummy), *((uint32_t *) storageSlots[slotId].address));

  // Run main storage function.
  // The test version of the function bootloader_commitSeUpgrade is configured for dry run
  // and not actual installation during the test.
  ret = storage_main();
  TEST_ASSERT_EQUAL_HEX32(BOOTLOADER_OK, ret);
}
#endif
