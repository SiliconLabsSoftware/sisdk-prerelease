/***************************************************************************//**
 * @file interface_test.c
 * @brief Test bootloader interface
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
#include "sl_main_init.h"
#include "core/btl_parse.h"

// Unity test framework
#include "unity.h"

#include <string.h>

extern const ApplicationProperties_t sl_app_properties;

#if defined(_SILICON_LABS_32B_SERIES_3_CONFIG_301)
#define APP_START 0x01010000UL
#if defined(__GNUC__)
const uint8_t bootloaderBlock[32 * 1024] __attribute__((section(".bootloader")));
#elif defined(__ICCARM__)
__no_init __root const uint8_t bootloaderBlock[32 * 1024] @ 0x01000000;
#else
#error "Bootloader space not reserved on this compiler"
#endif
#endif

#include "bin/testv4Encrypted.c"
#include "bin/parserTestAppArray.c"

char stringBuffer[48];

static void printAppData(const ApplicationData_t *app)
{
  switch (app->type) {
    case APPLICATION_TYPE_ZIGBEE:
      UnityPrintF(__LINE__, "  Application type: Zigbee");
      break;
    case APPLICATION_TYPE_THREAD:
      UnityPrintF(__LINE__, "  Application type: Thread");
      break;
    case APPLICATION_TYPE_FLEX:
      UnityPrintF(__LINE__, "  Application type: Flex");
      break;
    case APPLICATION_TYPE_BLUETOOTH:
      UnityPrintF(__LINE__, "  Application type: Bluetooth (full)");
      break;
    case APPLICATION_TYPE_BLUETOOTH_APP:
      UnityPrintF(__LINE__, "  Application type: Bluetooth (app)");
      break;
    case APPLICATION_TYPE_MCU:
      UnityPrintF(__LINE__, "  Application type: MCU");
      break;
    default:
      UnityPrintF(__LINE__, "  Application type: UNKNOWN");
      break;
  }
  TEST_PRINTF("  Version:          %d \n", app->version);
  TEST_PRINTF("  Capabilities:     %d \n", app->capabilities);
  UnityPrintF(__LINE__, "  Product ID:       ");
  for (size_t i = 0; i < 16; i++) {
    TEST_PRINTF("%d", app->productId[i]);
    if ((i == 3) || (i == 5) || (i == 7) || (i == 9)) {
      TEST_MESSAGE("-");
    }
  }
}

void callback(uint32_t offset, uint8_t *buffer, size_t length, unsigned int region_idx, void *ctx)
{
  (void) buffer;
  (void) ctx;
  (void) region_idx;
  TEST_PRINTF("parseCb %d | %d\n", offset, length);
}

static void testParser(void)
{
  ApplicationData_t appInfo = { 0 };
  uint32_t blVersion = 0;

  TEST_ASSERT_EQUAL(BOOTLOADER_OK, bootloader_init());

  // Test the GBL parser with a file that has been compiled in
  size_t offset = 0;
  uint32_t retval = 0;
  uint8_t ctx[BOOTLOADER_STORAGE_VERIFICATION_CONTEXT_SIZE];
  BootloaderParserContext_t* parseCtx = (BootloaderParserContext_t*)ctx;
  BootloaderParserCallbacks_t callbacks = { 0 };
  callbacks.applicationCallback = callback;

  TEST_ASSERT_EQUAL(BOOTLOADER_OK,
                    bootloader_initParser((BootloaderParserContext_t *)ctx, BOOTLOADER_STORAGE_VERIFICATION_CONTEXT_SIZE));

  while (offset < (sizeof(gblv4Encrypted) / sizeof(gblv4Encrypted[0]))) {
    if (offset < ((sizeof(gblv4Encrypted) - 64) / sizeof(gblv4Encrypted[0]))) {
      retval = bootloader_parseBuffer(parseCtx,
                                      &callbacks,
                                      (uint8_t*)&(gblv4Encrypted[offset]),
                                      64);
      offset += 64 / sizeof(test_app[0]);
    } else {
      retval = bootloader_parseBuffer(parseCtx,
                                      &callbacks,
                                      (uint8_t*)&(gblv4Encrypted[offset]),
                                      4);
      offset += 4 / sizeof(gblv4Encrypted[0]);
    }

    if (retval == BOOTLOADER_ERROR_PARSE_SUCCESS) {
      break;
    }

    if (retval != BOOTLOADER_ERROR_PARSE_CONTINUE) {
      // sprintf(stringBuffer, "parseBuffer error 0x%08lx at offset %d", retval, offset);
      TEST_FAIL_MESSAGE(stringBuffer);
      break;
    }
  }

  //validate the GBLv4 parsing was complete
  bool ret = parseCtx->imageProperties.imageCompleted && parseCtx->imageProperties.imageVerified;
  TEST_ASSERT_TRUE(ret);

  TEST_ASSERT_EQUAL(BOOTLOADER_OK,
                    bootloader_initParser(parseCtx, BOOTLOADER_STORAGE_VERIFICATION_CONTEXT_SIZE));

  TEST_ASSERT_EQUAL(BOOTLOADER_OK,
                    bootloader_parseImageInfo(
                      parseCtx,
                      (uint8_t*)&test_app,
                      sizeof(test_app),
                      &appInfo,
                      &blVersion));

  printAppData(&appInfo);
  TEST_ASSERT_EQUAL_HEX32(0x0000000, blVersion);
  TEST_ASSERT_EQUAL(APPLICATION_TYPE_MCU, appInfo.type);
  TEST_ASSERT_EQUAL(0x00000003, appInfo.version);
  TEST_ASSERT_EQUAL(0x01020304, appInfo.capabilities);
  uint8_t AppExpectedProductId[16] = { 0 };
  TEST_ASSERT_EQUAL_UINT8_ARRAY(AppExpectedProductId, appInfo.productId, 16UL);

  TEST_ASSERT_EQUAL(BOOTLOADER_OK, bootloader_deinit());

  TEST_PRINTF("BootloaderParserContext size: %d\n", bootloader_parserContextSize());
  TEST_ASSERT_TRUE(bootloader_parserContextSize() > 0UL);
  TEST_ASSERT_FALSE(BOOTLOADER_STORAGE_VERIFICATION_CONTEXT_SIZE < bootloader_parserContextSize());
}

static void testVerifyApp(void)
{
  TEST_ASSERT_EQUAL(BOOTLOADER_OK, bootloader_init());

  TEST_ASSERT_TRUE(bootloader_verifyApplication(APP_START));

  TEST_ASSERT_FALSE(bootloader_verifyApplication(APP_START + 0x4));

  TEST_ASSERT_EQUAL(BOOTLOADER_OK, bootloader_deinit());
}

int main(void)
{
  // CHIP_Init();
  sl_main_init();
  bootloader_init();

  UnityBegin("PARSER_INTERFACE");
  TEST_PRINTF("App version %d\n", sl_app_properties.app.version);

  RUN_TEST(testParser, __LINE__);
  RUN_TEST(testVerifyApp, __LINE__);

  UnityEnd();

  UnityPrint("ENDSWO");
  while (1) ;
}
