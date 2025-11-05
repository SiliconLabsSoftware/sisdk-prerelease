/***************************************************************************//**
 * @file flash_test.c
 * @brief Test bootloader flash driver
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
#include "core/btl_bootload.h"
#include "sl_memory_manager.h"

#include "em_chip.h"
#include "em_device.h"
#include "security/btl_crc32.h"

#if (_SILICON_LABS_32B_SERIES_2_CONFIG > 2)
  #include "bin_offset_8M/combined-test-vectors.c"
#else
  #include "bin/combined-test-vectors.c"
#endif

// Unity test framework
#include "unity.h"
#if defined(UNITY_OUTPUT_SERIAL)
#include "unity_usart.h"
#else
#include "unity_swo.h"
#endif

#include "em_common.h"

#include <stdlib.h>

// Defined addresses for tests
// Note : The app start location for all binaries have been shifted to 0x10000
// we are taking an offset of 0x4400 (17KB) for each binaries created.
#define ECDSA_VALID_APP_START FLASH_BASE + 0x10000
#define ECDSA_INVALID_APP_START FLASH_BASE + 0x14400
#define AP_UNSIGNED_APP_START FLASH_BASE + 0x18800
#define NO_AP_APP_START FLASH_BASE + 0x1CC00
#define CRC_VALID_APP_START FLASH_BASE + 0x21000
#define CRC_INVALID_APP_START FLASH_BASE + 0x25400
#define SIGNED_AP_HIGHER_VERSION_APP_START FLASH_BASE + 0x29800
#define SIGNED_CERT_APP_START FLASH_BASE + 0x2DC00
#define SIGNED_CERT_VERSION_ZERO_APP_START FLASH_BASE + 0x32000
#define SIGNED_CERT_INVALID_APP_START FLASH_BASE + 0x36400
#define SIGNED_CERT_INVALID_PUBKEY_APP_START FLASH_BASE + 0x3A800
#define SIGNED_VALID_APP_START FLASH_BASE + 0x3EC00

#if ENABLE_TEST_COVERAGE
extern void __gcov_flush(void);
#endif

#if defined(SEMAILBOX_PRESENT)
#include "sl_se_manager.h"
#include "sl_se_manager_util.h"
uint8_t test_buffer[64] __attribute__((aligned));
#endif

#if defined(_SILICON_LABS_32B_SERIES_2)
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
  .version = 1,
  .signature = { 0U },
};
#endif

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
#if defined(_SILICON_LABS_32B_SERIES_2)
  .cert = (ApplicationCertificate_t *)&sl_app_certificate,
#else
  .cert = NULL,
#endif
};

// This gives us mainBootloaderTable->startOfAppSpace = 0, which is used by
// bootload_verifyApplication().
#ifdef __ICCARM__
__root
#endif
MainBootloaderTable_t mainBootloaderTableImpl;
MainBootloaderTable_t *mainBootloaderTable = &mainBootloaderTableImpl;

#if (BOOTLOADER_ENFORCE_SECURE_BOOT == 1) && (PARSER_REQUIRE_CERTIFICATE_AUTHENTICITY == 0)
static bool checkLockedPage(uint32_t pageLockRegisterNr,
                            uint8_t* lockedPages,
                            uint8_t  lockedPagesLen,
                            bool shouldBeLocked)
{
#if defined(_MSC_PAGELOCK0_MASK)
#if defined(CRYPTOACC_PRESENT)
  CMU->CLKEN1_SET = CMU_CLKEN1_MSC;
#endif
  uint32_t * pageLockAddr = (uint32_t *)(&(MSC->PAGELOCK0));
  pageLockAddr = &pageLockAddr[pageLockRegisterNr];
  for (uint32_t i = 0U; i < lockedPagesLen; i++) {
    if (shouldBeLocked) {
      if ((*pageLockAddr & (0x1 << lockedPages[i])) == 0) {
        return false;
      }
    } else {
      if ((*pageLockAddr & (0x1 << lockedPages[i])) != 0) {
        return false;
      }
    }
  }
#if defined(CRYPTOACC_PRESENT)
  CMU->CLKEN1_CLR = CMU_CLKEN1_MSC;
#endif
#else
  (void) pageLockRegisterNr;
  (void) lockedPages;
  (void) lockedPagesLen;
  (void) shouldBeLocked;
#endif
  return true;
}

#if defined(_MSC_PAGELOCK0_MASK)
static void testLockApplicationArea(void)
{
  UnityPrintf("testLock\n");
  uint8_t lockedPages[5];
  uint8_t lockedPagesLen;

  TEST_ASSERT(bootload_lockApplicationArea(ECDSA_VALID_APP_START, ECDSA_VALID_APP_START + 0x4000));
  lockedPages[0] = 8;
  lockedPages[1] = 9;
  lockedPages[2] = 10;
  lockedPagesLen = 3;
  TEST_ASSERT(checkLockedPage(0, lockedPages, lockedPagesLen, true));

  lockedPages[0] = 11;
  lockedPagesLen = 1;
  TEST_ASSERT(checkLockedPage(0, lockedPages, lockedPagesLen, false));

  // S: Start E: End
  // 11   12   13   14
  // |----|----|----|
  //       ^ ^
  //       E S
  TEST_ASSERT_FALSE(bootload_lockApplicationArea(AP_UNSIGNED_APP_START + 0xCF0, AP_UNSIGNED_APP_START));
  lockedPages[0] = 12;
  lockedPagesLen = 1;
  TEST_ASSERT(checkLockedPage(0, lockedPages, lockedPagesLen, false));

  // 11   12   13   14
  // |----|----|----|
  //       ^ ^
  //       S E
  TEST_ASSERT(bootload_lockApplicationArea(AP_UNSIGNED_APP_START, AP_UNSIGNED_APP_START + 0xCF0));
  lockedPages[0] = 12;
  lockedPagesLen = 1;
  TEST_ASSERT(checkLockedPage(0, lockedPages, lockedPagesLen, true));

  lockedPages[0] = 13;
  lockedPagesLen = 1;
  TEST_ASSERT(checkLockedPage(0, lockedPages, lockedPagesLen, false));

  // 11   12   13   14   15   16
  // |----|----|----|----|----|
  //                 ^      ^
  //                 S      E
  TEST_ASSERT(bootload_lockApplicationArea(NO_AP_APP_START, NO_AP_APP_START + 0x2000));
  lockedPages[0] = 14;
  lockedPages[1] = 15;
  lockedPagesLen = 2;
  TEST_ASSERT(checkLockedPage(0, lockedPages, lockedPagesLen, true));

  TEST_ASSERT(bootload_lockApplicationArea(CRC_VALID_APP_START, CRC_VALID_APP_START + 0x2000));
  lockedPages[0] = 16;
  lockedPages[1] = 17;
  lockedPagesLen = 2;
  TEST_ASSERT(checkLockedPage(0, lockedPages, lockedPagesLen, true));

  lockedPages[0] = 19;
  lockedPagesLen = 1;
  TEST_ASSERT(checkLockedPage(0, lockedPages, lockedPagesLen, false));

  TEST_ASSERT(bootload_lockApplicationArea(SIGNED_AP_HIGHER_VERSION_APP_START, SIGNED_AP_HIGHER_VERSION_APP_START + 0x8000));
  lockedPages[0] = 20;
  lockedPages[1] = 21;
  lockedPages[2] = 22;
  lockedPages[3] = 23;
  lockedPagesLen = 4;
  TEST_ASSERT(checkLockedPage(0, lockedPages, lockedPagesLen, true));

  // 23   24   25
  // |----|----|
  //  ^    ^
  //  S    E
  TEST_ASSERT(bootload_lockApplicationArea(SIGNED_CERT_APP_START, SIGNED_CERT_APP_START + 0x2000));
  lockedPages[0] = 23;
  lockedPages[1] = 24;
  lockedPagesLen = 2;
  TEST_ASSERT(checkLockedPage(0, lockedPages, lockedPagesLen, true));

  lockedPages[0] = 25;
  lockedPagesLen = 1;
  TEST_ASSERT(checkLockedPage(0, lockedPages, lockedPagesLen, false));

  // 26   27   28
  // |----|----|
  //   ^    ^
  //   S    E
  TEST_ASSERT(bootload_lockApplicationArea(SIGNED_CERT_VERSION_ZERO_APP_START, FLASH_BASE + 0x32900 + 0x4000));
  lockedPages[0] = 26;
  lockedPages[1] = 27;
  lockedPagesLen = 2;
  TEST_ASSERT(checkLockedPage(0, lockedPages, lockedPagesLen, true));

  lockedPages[0] = 28;
  lockedPagesLen = 1;
  TEST_ASSERT(checkLockedPage(0, lockedPages, lockedPagesLen, false));

  // 28   29   30
  // |----|----|
  //   ^      ^
  //   S      E
  TEST_ASSERT(bootload_lockApplicationArea(SIGNED_CERT_INVALID_APP_START + 0x2000, FLASH_BASE + 0x3B400 + 0x2000));
  lockedPages[0] = 28;
  lockedPages[1] = 29;
  lockedPages[2] = 30;
  lockedPagesLen = 3;
  TEST_ASSERT(checkLockedPage(0, lockedPages, lockedPagesLen, true));

  lockedPages[0] = 31;
  lockedPagesLen = 1;
  TEST_ASSERT(checkLockedPage(0, lockedPages, lockedPagesLen, false));
}
#endif
#endif // (BOOTLOADER_ENFORCE_SECURE_BOOT == 1) && (PARSER_REQUIRE_CERTIFICATE_AUTHENTICITY == 0)

static void testVerifyAppSecurityEnforced(void)
{
#if (BOOTLOADER_ENFORCE_SECURE_BOOT == 1) && (PARSER_REQUIRE_CERTIFICATE_AUTHENTICITY == 0)
  // ECDSA signed app should pass
  mainBootloaderTable->startOfAppSpace = (BareBootTable_t *)(ECDSA_VALID_APP_START);
  TEST_ASSERT(bootload_verifyApplication(ECDSA_VALID_APP_START));
  uint8_t lockedPages[1];
  uint8_t lockedPagesLen;
#if defined(_MSC_PAGELOCK0_MASK)
  TEST_ASSERT(bootload_lockApplicationArea(ECDSA_VALID_APP_START, 0));
#endif
  lockedPages[0] = 8;
  lockedPagesLen = 1;
  TEST_ASSERT(checkLockedPage(0, lockedPages, lockedPagesLen, true));
  // Invalid ECDSA signature should fail
  mainBootloaderTable->startOfAppSpace = (BareBootTable_t *)(ECDSA_INVALID_APP_START);
  TEST_ASSERT_FALSE(bootload_verifyApplication(ECDSA_INVALID_APP_START));
  // Unsigned app should fail
  mainBootloaderTable->startOfAppSpace = (BareBootTable_t *)(AP_UNSIGNED_APP_START);
  TEST_ASSERT_FALSE(bootload_verifyApplication(AP_UNSIGNED_APP_START));
  // App without linked AP should fail
  mainBootloaderTable->startOfAppSpace = (BareBootTable_t *)(NO_AP_APP_START);
  TEST_ASSERT_FALSE(bootload_verifyApplication(NO_AP_APP_START));
  // CRCed app should fail
  mainBootloaderTable->startOfAppSpace = (BareBootTable_t *)(CRC_VALID_APP_START);
  TEST_ASSERT_FALSE(bootload_verifyApplication(CRC_VALID_APP_START));
  // Invalid CRC should fail
  mainBootloaderTable->startOfAppSpace = (BareBootTable_t *)(CRC_INVALID_APP_START);
  TEST_ASSERT_FALSE(bootload_verifyApplication(CRC_INVALID_APP_START));
  // App using higher app-prop struct version should fail.
  mainBootloaderTable->startOfAppSpace = (BareBootTable_t *)(SIGNED_AP_HIGHER_VERSION_APP_START);
  TEST_ASSERT_FALSE(bootload_verifyApplication(SIGNED_AP_HIGHER_VERSION_APP_START));
#if defined(_MSC_PAGELOCK0_MASK)
  TEST_ASSERT_FALSE(bootload_lockApplicationArea(SIGNED_AP_HIGHER_VERSION_APP_START, 0));
#endif
  lockedPages[0] = 7;
  lockedPagesLen = 1;
  TEST_ASSERT(checkLockedPage(0, lockedPages, lockedPagesLen, false));
  // Reprodcer of EFM32ESS-7701
  mainBootloaderTable->startOfAppSpace = (BareBootTable_t *)(SIGNED_VALID_APP_START);
  TEST_ASSERT(bootload_verifyApplication(SIGNED_VALID_APP_START));
#endif
#if defined(_SILICON_LABS_32B_SERIES_2)
#if (BOOTLOADER_ENFORCE_SECURE_BOOT == 1) && (PARSER_REQUIRE_CERTIFICATE_AUTHENTICITY == 0)
  // Use platform key / lock bits key to verify images,
  // which will fail since those images are not signed with test-ecckey.pem
  TEST_ASSERT_FALSE(bootload_verifyApplication(SIGNED_CERT_APP_START));
  TEST_ASSERT_FALSE(bootload_verifyApplication(SIGNED_CERT_VERSION_ZERO_APP_START));
  TEST_ASSERT_FALSE(bootload_verifyApplication(SIGNED_CERT_INVALID_APP_START));
  TEST_ASSERT_FALSE(bootload_verifyApplication(SIGNED_CERT_INVALID_PUBKEY_APP_START));
#endif
#if (BOOTLOADER_ENFORCE_SECURE_BOOT == 1) && (PARSER_REQUIRE_CERTIFICATE_AUTHENTICITY == 1)
  // ECDSA signed app with certificate
  // Certificate is signed with test-ecckey.pem and it contains test-eccpubkey-cert.
  mainBootloaderTable->startOfAppSpace = (BareBootTable_t *)(SIGNED_CERT_APP_START);
  TEST_ASSERT_TRUE(bootload_verifyApplication(SIGNED_CERT_APP_START));
  // ECDSA signed app with certificate version 0
  // This certificate needs to be rejected because of the version number of the certificate.
  // The certificate version of the app needs to be higher than the bootloader.
  mainBootloaderTable->startOfAppSpace = (BareBootTable_t *)(SIGNED_CERT_VERSION_ZERO_APP_START);
  TEST_ASSERT_FALSE(bootload_verifyApplication(SIGNED_CERT_VERSION_ZERO_APP_START));
  // Certificate with invalid ECDSA signature.
  mainBootloaderTable->startOfAppSpace = (BareBootTable_t *)(SIGNED_CERT_INVALID_APP_START);
  TEST_ASSERT_FALSE(bootload_verifyApplication(SIGNED_CERT_INVALID_APP_START));
  // Certificate with invalid pubkey
  mainBootloaderTable->startOfAppSpace = (BareBootTable_t *)(SIGNED_CERT_INVALID_PUBKEY_APP_START);
  TEST_ASSERT_FALSE(bootload_verifyApplication(SIGNED_CERT_INVALID_PUBKEY_APP_START));
#endif
#endif // _SILICON_LABS_32B_SERIES_2
}

static void testVerifyAppNoSecurityEnforced(void)
{
#if (BOOTLOADER_ENFORCE_SECURE_BOOT == 0) && (PARSER_REQUIRE_CERTIFICATE_AUTHENTICITY == 0)
  // ECDSA signed app should pass
  mainBootloaderTable->startOfAppSpace = (BareBootTable_t *)(ECDSA_VALID_APP_START);
  TEST_ASSERT(bootload_verifyApplication(ECDSA_VALID_APP_START));
//   // Invalid ECDSA signature should fail
  mainBootloaderTable->startOfAppSpace = (BareBootTable_t *)(ECDSA_INVALID_APP_START);
  TEST_ASSERT_FALSE(bootload_verifyApplication(ECDSA_INVALID_APP_START));
  // Unsigned app should pass
  mainBootloaderTable->startOfAppSpace = (BareBootTable_t *)(AP_UNSIGNED_APP_START);
  // TEST_ASSERT(bootload_verifyApplication(AP_UNSIGNED_APP_START));
  // App without linked AP should pass
  mainBootloaderTable->startOfAppSpace = (BareBootTable_t *)(NO_AP_APP_START);
  // TEST_ASSERT(bootload_verifyApplication(NO_AP_APP_START));
  // CRCed app should pass
  mainBootloaderTable->startOfAppSpace = (BareBootTable_t *)(CRC_VALID_APP_START);
  TEST_ASSERT(bootload_verifyApplication(CRC_VALID_APP_START));
  // Invalid CRC should fail
  mainBootloaderTable->startOfAppSpace = (BareBootTable_t *)(CRC_INVALID_APP_START);
  TEST_ASSERT_FALSE(bootload_verifyApplication(CRC_INVALID_APP_START));
  // App using higher app-prop version should fail.
  mainBootloaderTable->startOfAppSpace = (BareBootTable_t *)(SIGNED_AP_HIGHER_VERSION_APP_START);
  TEST_ASSERT_FALSE(bootload_verifyApplication(SIGNED_AP_HIGHER_VERSION_APP_START));
  // Reprodcer of EFM32ESS-7701
  mainBootloaderTable->startOfAppSpace = (BareBootTable_t *)(SIGNED_VALID_APP_START);
  TEST_ASSERT(bootload_verifyApplication(SIGNED_VALID_APP_START));
#endif
#if defined(_SILICON_LABS_32B_SERIES_2)
#if (BOOTLOADER_ENFORCE_SECURE_BOOT == 0) && (PARSER_REQUIRE_CERTIFICATE_AUTHENTICITY == 1)
  // ECDSA signed app with certificate
  // Certificate is signed with test-ecckey.pem and it contains test-eccpubkey-cert.
  mainBootloaderTable->startOfAppSpace = (BareBootTable_t *)(SIGNED_CERT_APP_START); // was 0xf000
  TEST_ASSERT_TRUE(bootload_verifyApplication(SIGNED_CERT_APP_START));
  // ECDSA signed app with certificate version 0
  // This certificate needs to be rejected because of the version number of the certificate.
  // The certificate version of the app needs to be higher than the bootloader.
  mainBootloaderTable->startOfAppSpace = (BareBootTable_t *)(SIGNED_CERT_VERSION_ZERO_APP_START);
  TEST_ASSERT_FALSE(bootload_verifyApplication(SIGNED_CERT_VERSION_ZERO_APP_START));
  // Certificate with invalid ECDSA signature.
  mainBootloaderTable->startOfAppSpace = (BareBootTable_t *)(SIGNED_CERT_INVALID_APP_START);
  TEST_ASSERT_FALSE(bootload_verifyApplication(SIGNED_CERT_INVALID_APP_START));
  // Certificate with invalid pubkey
  mainBootloaderTable->startOfAppSpace = (BareBootTable_t *)(SIGNED_CERT_INVALID_PUBKEY_APP_START);
  TEST_ASSERT_FALSE(bootload_verifyApplication(SIGNED_CERT_INVALID_PUBKEY_APP_START));
#endif
#endif // _SILICON_LABS_32B_SERIES_2
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

  UnityPrintf("mbt = %lx\n", (uint32_t)&(mainBootloaderTable));

  // Perform a runtime access into the array to ensure it survives optimizations
  uint32_t off = rand() % sizeof(dataArray) / 4;
  const uint32_t *arr = &dataArray[0];
  UnityPrintf("val = %08x\n", *(arr + off));

  UnityBeginGroup("verify_app");

  UnityPrintf("App version %d\n", sl_app_properties.app.version);
#if defined(BOOTLOADER_SUPPORT_CERTIFICATES)
  UnityPrintf("Certificate version %d\n", sl_app_properties.cert->version);
#endif

#if defined(SEMAILBOX_PRESENT)
#if defined(_CMU_CLKEN1_SEMAILBOXHOST_MASK)
  CMU->CLKEN1_SET = CMU_CLKEN1_SEMAILBOXHOST;
#endif
  sl_se_command_context_t cmd_ctx = { 0u };
  sl_status_t ret = sl_se_read_pubkey(&cmd_ctx,
                                      SL_SE_KEY_TYPE_IMMUTABLE_BOOT,
                                      &test_buffer,
                                      64);
  TEST_ASSERT_EQUAL(SL_STATUS_OK, ret);
#endif

  RUN_TEST(testVerifyAppNoSecurityEnforced, __LINE__);
  RUN_TEST(testVerifyAppSecurityEnforced, __LINE__);

#if (BOOTLOADER_ENFORCE_SECURE_BOOT == 1) && (PARSER_REQUIRE_CERTIFICATE_AUTHENTICITY == 0)
#if defined(_MSC_PAGELOCK0_MASK)
  RUN_TEST(testLockApplicationArea, __LINE__);
#endif
#endif
  UnityEnd();

#if ENABLE_TEST_COVERAGE
  __gcov_flush();
#endif

  UnityPrint("ENDSWO");
  while (1) ;
}
