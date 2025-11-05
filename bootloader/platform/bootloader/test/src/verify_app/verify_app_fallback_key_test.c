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

#include "em_chip.h"
#include "em_device.h"

#include "security/btl_crc32.h"
#include "sl_memory_manager.h"

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
#endif

#if defined(_SILICON_LABS_32B_SERIES_2)
const ApplicationCertificate_t sl_app_certificate = {
  .structVersion = APPLICATION_CERTIFICATE_VERSION,
  .flags = { 0U },
  .key = { 0U },
  .version = 0,
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

static void testVerifyAppSecurityEnforced(void)
{
#if (BOOTLOADER_ENFORCE_SECURE_BOOT == 1) && (PARSER_REQUIRE_CERTIFICATE_AUTHENTICITY == 0)
  // ECDSA signed app should pass
  TEST_ASSERT(bootload_verifyApplication(ECDSA_VALID_APP_START));
  // Invalid ECDSA signature should fail
  TEST_ASSERT_FALSE(bootload_verifyApplication(ECDSA_INVALID_APP_START));
  // Unsigned app should fail
  TEST_ASSERT_FALSE(bootload_verifyApplication(AP_UNSIGNED_APP_START));
  // App without linked AP should fail
  TEST_ASSERT_FALSE(bootload_verifyApplication(NO_AP_APP_START));
  // CRCed app should fail
  TEST_ASSERT_FALSE(bootload_verifyApplication(CRC_VALID_APP_START));
  // Invalid CRC should fail
  TEST_ASSERT_FALSE(bootload_verifyApplication(CRC_INVALID_APP_START));
  // App using higher app-prop version should fail.
  TEST_ASSERT_FALSE(bootload_verifyApplication(SIGNED_AP_HIGHER_VERSION_APP_START));
#endif
#if defined(_SILICON_LABS_32B_SERIES_2)
#if (BOOTLOADER_ENFORCE_SECURE_BOOT == 1) && (PARSER_REQUIRE_CERTIFICATE_AUTHENTICITY == 1)
  // ECDSA signed app with certificate
  // Certificate is signed with test-ecckey.pem and it contains test-eccpubkey-cert.
  // This should return false, since certificate can ONLY be verified by "bootloader"
  // certificate key, which contains zeros in this test case.
  TEST_ASSERT_FALSE(bootload_verifyApplication(SIGNED_CERT_APP_START));
#endif
#endif //_SILICON_LABS_32B_SERIES_2
}

static void testVerifyAppNoSecurityEnforced(void)
{
#if (BOOTLOADER_ENFORCE_SECURE_BOOT == 0) && (PARSER_REQUIRE_CERTIFICATE_AUTHENTICITY == 0)
  // ECDSA signed app should pass
  TEST_ASSERT(bootload_verifyApplication(ECDSA_VALID_APP_START));
  // Invalid ECDSA signature should fail
  TEST_ASSERT_FALSE(bootload_verifyApplication(ECDSA_INVALID_APP_START));
  // Unsigned app should pass
  TEST_ASSERT(bootload_verifyApplication(AP_UNSIGNED_APP_START));
  // App without linked AP should pass
  TEST_ASSERT(bootload_verifyApplication(NO_AP_APP_START));
  // CRCed app should pass
  TEST_ASSERT(bootload_verifyApplication(CRC_VALID_APP_START));
  // Invalid CRC should fail
  TEST_ASSERT_FALSE(bootload_verifyApplication(CRC_INVALID_APP_START));
  // App using higher app-prop version should fail.
  TEST_ASSERT_FALSE(bootload_verifyApplication(SIGNED_AP_HIGHER_VERSION_APP_START));
#endif
#if defined(_SILICON_LABS_32B_SERIES_2)
#if (BOOTLOADER_ENFORCE_SECURE_BOOT == 0) && (PARSER_REQUIRE_CERTIFICATE_AUTHENTICITY == 1)
  // ECDSA signed app with certificate
  // Certificate is signed with test-ecckey.pem and it contains test-eccpubkey-cert.
  // This should return false, since certificate can ONLY be verified by "bootloader"
  // certificate key, which contains zeros in this test case.
  TEST_ASSERT_FALSE(bootload_verifyApplication(SIGNED_CERT_APP_START));
#endif
#endif //_SILICON_LABS_32B_SERIES_2
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

  // Perform a runtime access into the array to ensure it survives optimizations.
  uint32_t off = rand() % sizeof(dataArray) / 4;
  const uint32_t *arr = &dataArray[0];
  UnityPrintf("val = %08x\n", *(arr + off));

  UnityBeginGroup("verify_app");
  UnityPrintf("App version %d\n", sl_app_properties.app.version);
#if defined(SEMAILBOX_PRESENT)
#if defined(_CMU_CLKEN1_SEMAILBOXHOST_MASK)
  CMU->CLKEN1_SET = CMU_CLKEN1_SEMAILBOXHOST;
#endif

  sl_status_t ret;
  sl_se_command_context_t cmd_ctx;
  uint8_t key[64];

  TEST_ASSERT_EQUAL(SL_STATUS_OK, sl_se_init());
  TEST_ASSERT_EQUAL(SL_STATUS_OK, sl_se_init_command_context(&cmd_ctx));

  ret = sl_se_read_pubkey(&cmd_ctx, SL_SE_KEY_TYPE_IMMUTABLE_BOOT, key, 64);

  sl_se_deinit_command_context(&cmd_ctx);
  sl_se_deinit();

  // Key is not installed on the device that runs this test.
#if (_SILICON_LABS_32B_SERIES_2_CONFIG > 2)
  TEST_ASSERT_EQUAL(SL_STATUS_NOT_INITIALIZED, ret);
#else
  TEST_ASSERT_EQUAL(SL_STATUS_FAIL, ret);
#endif
#endif // SEMAILBOX_PRESENT

  RUN_TEST(testVerifyAppNoSecurityEnforced, __LINE__);
  RUN_TEST(testVerifyAppSecurityEnforced, __LINE__);

  UnityEnd();

#if ENABLE_TEST_COVERAGE
  __gcov_flush();
#endif

  UnityPrint("ENDSWO");
  while (1) ;
}
