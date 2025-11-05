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

// Defined addresses for tests
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

#include "em_common.h"

#include <stdlib.h>

#if ENABLE_TEST_COVERAGE
extern void __gcov_flush(void);
#endif

#if defined(SEMAILBOX_PRESENT)
#include "sl_se_manager.h"
#include "sl_se_manager_util.h"
uint8_t test_buffer[64] __attribute__((aligned));
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
  .version = 1,
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

static void testVerifyAppSecurityEnforced(void)
{
#if defined(TEST_CERT_SIGNED_APP_VERIFICATION)
  // No cert, should be rejected
  TEST_ASSERT_FALSE(bootload_verifyApplication(ECDSA_VALID_APP_START));
  // No cert, should be rejected
  TEST_ASSERT_FALSE(bootload_verifyApplication(ECDSA_INVALID_APP_START));
  // No cert, should be rejected
  TEST_ASSERT_FALSE(bootload_verifyApplication(AP_UNSIGNED_APP_START));
  // No cert, should be rejected
  TEST_ASSERT_FALSE(bootload_verifyApplication(NO_AP_APP_START));
  // No cert, should be rejected
  TEST_ASSERT_FALSE(bootload_verifyApplication(CRC_VALID_APP_START));
  // No cert, should be rejected
  TEST_ASSERT_FALSE(bootload_verifyApplication(CRC_INVALID_APP_START));
  // App using higher app-prop version should fail.
  TEST_ASSERT_FALSE(bootload_verifyApplication(SIGNED_AP_HIGHER_VERSION_APP_START));

  // ECDSA signed app with certificate
  // Certificate is signed with test-ecckey.pem and it contains test-eccpubkey-cert.
  TEST_ASSERT_TRUE(bootload_verifyApplication(SIGNED_CERT_APP_START));
  // ECDSA signed app with certificate version 0
  // This certificate needs to be rejected because of the version number of the certificate.
  // The certificate version of the app needs to be higher than the bootloader.
  TEST_ASSERT_FALSE(bootload_verifyApplication(SIGNED_CERT_VERSION_ZERO_APP_START));
  // Certificate with invalid ECDSA signature.
  TEST_ASSERT_FALSE(bootload_verifyApplication(SIGNED_CERT_INVALID_APP_START));
  // Certificate with invalid pubkey
  TEST_ASSERT_FALSE(bootload_verifyApplication(SIGNED_CERT_INVALID_PUBKEY_APP_START));
#else

  // Use bootloader certificate key to verify.
  TEST_ASSERT(bootload_verifyApplication(ECDSA_VALID_APP_START));
  TEST_ASSERT_FALSE(bootload_verifyApplication(ECDSA_INVALID_APP_START));
  TEST_ASSERT_FALSE(bootload_verifyApplication(AP_UNSIGNED_APP_START));
  TEST_ASSERT_FALSE(bootload_verifyApplication(NO_AP_APP_START));
  TEST_ASSERT_FALSE(bootload_verifyApplication(CRC_VALID_APP_START));
  TEST_ASSERT_FALSE(bootload_verifyApplication(CRC_INVALID_APP_START));
  TEST_ASSERT_FALSE(bootload_verifyApplication(SIGNED_AP_HIGHER_VERSION_APP_START));

  TEST_ASSERT_TRUE(bootload_verifyApplication(SIGNED_CERT_APP_START));
  TEST_ASSERT_FALSE(bootload_verifyApplication(SIGNED_CERT_VERSION_ZERO_APP_START));
  TEST_ASSERT_FALSE(bootload_verifyApplication(SIGNED_CERT_INVALID_APP_START));
  TEST_ASSERT_FALSE(bootload_verifyApplication(SIGNED_CERT_INVALID_PUBKEY_APP_START));
#endif
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

  RUN_TEST(testVerifyAppSecurityEnforced, __LINE__);

  UnityEnd();

#if ENABLE_TEST_COVERAGE
  __gcov_flush();
#endif

  UnityPrint("ENDSWO");
  while (1) ;
}
