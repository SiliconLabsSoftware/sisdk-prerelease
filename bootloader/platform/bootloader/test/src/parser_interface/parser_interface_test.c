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

#include "em_chip.h"

// Unity test framework
#include "unity.h"
#if defined(UNITY_OUTPUT_SERIAL)
#include "unity_usart.h"
#else
#include "unity_swo.h"
#endif

#include <string.h>

extern const ApplicationProperties_t sl_app_properties;

#if defined(_SILICON_LABS_GECKO_INTERNAL_SDID_80) || defined(_SILICON_LABS_GECKO_INTERNAL_SDID_200)
#define APP_START 0x4000UL
#if defined(__GNUC__)
const uint8_t bootloaderBlock[16 * 1024] __attribute__((section(".bootloader")));
#elif defined(__ICCARM__)
__no_init __root const uint8_t bootloaderBlock[16 * 1024] @ 0x00000000;
#else
#error "Bootloader space not reserved on this compiler"
#endif
#elif defined(_SILICON_LABS_GECKO_INTERNAL_SDID_210) || defined(_SILICON_LABS_GECKO_INTERNAL_SDID_215) \
  || defined(_SILICON_LABS_GECKO_INTERNAL_SDID_220) || defined(_SILICON_LABS_GECKO_INTERNAL_SDID_225)  \
  || defined(_SILICON_LABS_GECKO_INTERNAL_SDID_230) || defined(_SILICON_LABS_GECKO_INTERNAL_SDID_235)  \
  || defined(_SILICON_LABS_GECKO_INTERNAL_SDID_240) || defined(_SILICON_LABS_GECKO_INTERNAL_SDID_260)
#define APP_START 0x08006000UL
#if defined(__GNUC__)
const uint8_t bootloaderBlock[24 * 1024] __attribute__((section(".bootloader")));
#elif defined(__ICCARM__)
__no_init __root const uint8_t bootloaderBlock[24 * 1024] @ 0x08000000;
#else
#error "Bootloader space not reserved on this compiler"
#endif
#elif defined(_SILICON_LABS_GECKO_INTERNAL_SDID_89) && defined(MAIN_BOOTLOADER_IN_MAIN_FLASH) \
  || defined(_SILICON_LABS_GECKO_INTERNAL_SDID_95) && defined(MAIN_BOOTLOADER_IN_MAIN_FLASH)
#define APP_START 0x4800UL
#if defined(__GNUC__)
const uint8_t bootloaderBlock[18 * 1024] __attribute__((section(".bootloader")));
#elif defined(__ICCARM__)
__no_init __root const uint8_t bootloaderBlock[18 * 1024] @ 0x00000000;
#else
#error "Bootloader space not reserved on this compiler"
#endif
#else
#define APP_START 0x0UL
#endif

#if !defined(SL_TRUSTZONE_NONSECURE)
#include "../eblparser/ebl/testv3Encrypted.c"
#include "bin/parserTestAppArray.c"
#else
#include "bin/parserTestAppArrayTrustZone.c"
#endif

#if defined(BOOTLOADER_INTERFACE_TRUSTZONE_AWARE)
static Bootloader_PPUSATDnCLKENnState_t ppusatdclkennState = { 0 };
#else
static uint32_t ppusatdclkennState;
#endif

#if defined(SL_TRUSTZONE_NONSECURE)
extern uint32_t get_smu_ppusatd0(void);
extern uint32_t get_smu_ppusatd1(void);
#endif

static void ppusatdn_save_state(void *ctx)
{
#if defined(BOOTLOADER_INTERFACE_TRUSTZONE_AWARE)
  Bootloader_PPUSATDnCLKENnState_t *ctxTemp = (Bootloader_PPUSATDnCLKENnState_t *)ctx;

#if defined(_CMU_CLKEN1_SMU_MASK)
  CMU->CLKEN1_SET = CMU_CLKEN1_SMU;
#endif
#if defined(_CMU_CLKEN0_MASK)
  // Save the CLKENn states
  ctxTemp->CLKEN0 = CMU->CLKEN0;
  ctxTemp->CLKEN1 = CMU->CLKEN1;
#endif

  uint32_t smu_ppusatd0, smu_ppusatd1;
#if defined(SL_TRUSTZONE_NONSECURE)
  smu_ppusatd0 = get_smu_ppusatd0();
  smu_ppusatd1 = get_smu_ppusatd1();
#else
  smu_ppusatd0 = SMU->PPUSATD0;
  smu_ppusatd1 = SMU->PPUSATD1;
#endif // defined(SL_TRUSTZONE_NONSECURE)
  ctxTemp->PPUSATD0 = smu_ppusatd0;
  ctxTemp->PPUSATD1 = smu_ppusatd1;
#else
  (void)ctx;
#endif
}

static void ppusatdn_verify_state(void *ctx)
{
#if defined(BOOTLOADER_INTERFACE_TRUSTZONE_AWARE)
  Bootloader_PPUSATDnCLKENnState_t *ctxTemp = (Bootloader_PPUSATDnCLKENnState_t *)ctx;
#if defined(_CMU_CLKEN0_MASK)
  TEST_ASSERT_EQUAL_HEX32(ctxTemp->CLKEN0, CMU->CLKEN0);
  TEST_ASSERT_EQUAL_HEX32(ctxTemp->CLKEN1, CMU->CLKEN1);
#endif

  uint32_t smu_ppusatd0, smu_ppusatd1;
#if defined(SL_TRUSTZONE_NONSECURE)
  smu_ppusatd0 = get_smu_ppusatd0();
  smu_ppusatd1 = get_smu_ppusatd1();
#else
  smu_ppusatd0 = SMU->PPUSATD0;
  smu_ppusatd1 = SMU->PPUSATD1;
#endif // defined(SL_TRUSTZONE_NONSECURE)

  TEST_ASSERT_EQUAL_HEX32(ctxTemp->PPUSATD0, smu_ppusatd0);
  TEST_ASSERT_EQUAL_HEX32(ctxTemp->PPUSATD1, smu_ppusatd1);
#else
  (void)ctx;
#endif
}

char stringBuffer[48];

static void printAppData(const ApplicationData_t *app)
{
  switch (app->type) {
    case APPLICATION_TYPE_ZIGBEE:
      UnityPrintf("  Application type: Zigbee\n");
      break;
    case APPLICATION_TYPE_THREAD:
      UnityPrintf("  Application type: Thread\n");
      break;
    case APPLICATION_TYPE_FLEX:
      UnityPrintf("  Application type: Flex\n");
      break;
    case APPLICATION_TYPE_BLUETOOTH:
      UnityPrintf("  Application type: Bluetooth (full)\n");
      break;
    case APPLICATION_TYPE_BLUETOOTH_APP:
      UnityPrintf("  Application type: Bluetooth (app)\n");
      break;
    case APPLICATION_TYPE_MCU:
      UnityPrintf("  Application type: MCU\n");
      break;
    default:
      UnityPrintf("  Application type: UNKNOWN\n");
      break;
  }
  UnityPrintf("  Version:          %ld \n", app->version);
  UnityPrintf("  Capabilities:     0x%x \n", app->capabilities);
  UnityPrintf("  Product ID:       ");
  for (size_t i = 0; i < 16; i++) {
    UnityPrintf("%02x", app->productId[i]);
    if ((i == 3) || (i == 5) || (i == 7) || (i == 9)) {
      UnityPrintf("-");
    }
  }
  UnityPrintf("\n");
}

void HardFault_Handler(void)
{
  UnityPrintf("FAULT\n          ");
  while (1) {
    // Do nothing
  }
}

void callback(uint32_t offset, uint8_t *buffer, size_t length, void *ctx)
{
  (void) buffer;
  (void) ctx;
  UnityPrintf("parseCb %08X | %d\n", offset, length);
}

static void testParser(void)
{
  ApplicationData_t appInfo = { 0 };
  uint32_t blVersion = 0;

  TEST_ASSERT_EQUAL(BOOTLOADER_OK, bootloader_init());

#if !defined(SL_TRUSTZONE_NONSECURE)
  // Test the EBL parser with a file that has been compiled in
  size_t offset = 0;
  uint32_t retval = 0;
  uint8_t ctx[BOOTLOADER_STORAGE_VERIFICATION_CONTEXT_SIZE];
  BootloaderParserCallbacks_t callbacks = { 0 };
  callbacks.applicationCallback = callback;

  ppusatdn_save_state(&ppusatdclkennState);
  TEST_ASSERT_EQUAL(BOOTLOADER_OK,
                    bootloader_initParser((BootloaderParserContext_t *)ctx, BOOTLOADER_STORAGE_VERIFICATION_CONTEXT_SIZE));
  ppusatdn_verify_state(&ppusatdclkennState);

  while (offset < (sizeof(eblv3Encrypted) / sizeof(eblv3Encrypted[0]))) {
    if (offset < ((sizeof(eblv3Encrypted) - 64) / sizeof(eblv3Encrypted[0]))) {
      ppusatdn_save_state(&ppusatdclkennState);
      retval = bootloader_parseBuffer((BootloaderParserContext_t *)ctx,
                                      &callbacks,
                                      (uint8_t*)&(eblv3Encrypted[offset]),
                                      64);
      ppusatdn_verify_state(&ppusatdclkennState);
      offset += 64 / sizeof(eblv3Encrypted[0]);
    } else {
      retval = bootloader_parseBuffer((BootloaderParserContext_t *)ctx,
                                      &callbacks,
                                      (uint8_t*)&(eblv3Encrypted[offset]),
                                      4);
      offset += 4 / sizeof(eblv3Encrypted[0]);
    }

    if (retval == BOOTLOADER_ERROR_PARSE_SUCCESS) {
      break;
    }

    if (retval != BOOTLOADER_ERROR_PARSE_CONTINUE) {
      sprintf(stringBuffer, "parseBuffer error 0x%08lx at offset %d", retval, offset);
      TEST_FAIL_MESSAGE(stringBuffer);
      break;
    }
  }

  TEST_ASSERT_EQUAL(BOOTLOADER_OK,
                    bootloader_initParser((BootloaderParserContext_t *)ctx, BOOTLOADER_STORAGE_VERIFICATION_CONTEXT_SIZE));
#endif // !defined(SL_TRUSTZONE_NONSECURE)

  ppusatdn_save_state(&ppusatdclkennState);
  TEST_ASSERT_EQUAL(BOOTLOADER_OK,
                    bootloader_parseImageInfo(
#if !defined(SL_TRUSTZONE_NONSECURE)
                      (BootloaderParserContext_t *)ctx,
#endif
                      (uint8_t*)&test_app,
                      sizeof(test_app),
                      &appInfo,
                      &blVersion));
  ppusatdn_verify_state(&ppusatdclkennState);

  printAppData(&appInfo);
  TEST_ASSERT_EQUAL_HEX32(0x10b0000, blVersion);
  TEST_ASSERT_EQUAL(APPLICATION_TYPE_MCU, appInfo.type);
  TEST_ASSERT_EQUAL(25UL, appInfo.version);
  TEST_ASSERT_EQUAL_HEX32(0x1020304, appInfo.capabilities);
  uint8_t AppExpectedProductId[16] = { 0xb0, 0xaa, 0x8d, 0x02, 0x9f, 0x7d, 0x11, 0xe7,
                                       0xab, 0xc4, 0xce, 0xc2, 0x78, 0xb6, 0xb5, 0x0a };
  TEST_ASSERT_EQUAL_UINT8_ARRAY(AppExpectedProductId, appInfo.productId, 16UL);

  TEST_ASSERT_EQUAL(BOOTLOADER_OK, bootloader_deinit());

  UnityPrintf("BootloaderParserContext size: %ld\n", bootloader_parserContextSize());
  TEST_ASSERT_TRUE(bootloader_parserContextSize() > 0UL);
  TEST_ASSERT_FALSE(BOOTLOADER_STORAGE_VERIFICATION_CONTEXT_SIZE < bootloader_parserContextSize());
}

static void testVerifyApp(void)
{
  TEST_ASSERT_EQUAL(BOOTLOADER_OK, bootloader_init());

  ppusatdn_save_state(&ppusatdclkennState);
  TEST_ASSERT_TRUE(bootloader_verifyApplication(APP_START));
  ppusatdn_verify_state(&ppusatdclkennState);

  TEST_ASSERT_FALSE(bootloader_verifyApplication(APP_START + 0x4));

  TEST_ASSERT_EQUAL(BOOTLOADER_OK, bootloader_deinit());
}

int main(void)
{
  CHIP_Init();
  bootloader_init();

#if defined(UNITY_OUTPUT_SERIAL)
  UNITY_UsartSetup();
#else
  SWO_Setup();
#endif

  UnityBeginGroup("PARSER_INTERFACE");
  UnityPrintf("App version %d\n", sl_app_properties.app.version);

  RUN_TEST(testParser, __LINE__);
  RUN_TEST(testVerifyApp, __LINE__);

  UnityEnd();

  UnityPrint("ENDSWO");
  while (1) ;
}
