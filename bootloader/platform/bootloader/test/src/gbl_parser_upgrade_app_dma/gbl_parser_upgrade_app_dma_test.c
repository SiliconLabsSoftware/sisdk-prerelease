// --------------------------------
// Test environment
#include "em_device.h"
#include "em_chip.h"
#include "em_cmu.h"

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

#include "core/btl_parse.h"
#include "parser/gbl/btl_gbl_parser.h"
#include "security/btl_security_types.h"

#include "bin/upgrade_test_app.c"
#include "core/btl_bootload.h"

// --------------------------------
// Test prototypes
static void test_gbl_upgrade_parse(void);

// This is to bypass address check for btl app properties pointer
MainBootloaderTable_t mainBootloaderTableImpl;
MainBootloaderTable_t *mainBootloaderTable = &mainBootloaderTableImpl;

// Implement parser callbacks
bool parser_applicationUpgradeValidCallback(ApplicationData_t *app)
{
  (void) app;
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

  UnityBeginGroup("GBL_PARSER_UPGRADE_APP");
  mainBootloaderTable->endOfAppSpace = (uint32_t *)FLASH_SIZE;

  RUN_TEST(test_gbl_upgrade_parse, __LINE__);

  UnityEnd();

#if ENABLE_TEST_COVERAGE
  __gcov_flush();
#endif

  UnityPrint("ENDSWO");

  return 0;
}

// --------------------------------
// Test implementations

static void CYCLE_Init(void)
{
  DWT->CTRL |= 1;
}

static uint32_t CYCLE_Count(void)
{
  return DWT->CYCCNT;
}

static uint32_t CYCLE_toMs(uint32_t cycles)
{
  uint32_t freq_khz = SystemCoreClockGet() / 1000;
  uint32_t ms = cycles / freq_khz;
  return ms;
}

static void no_callback(uint32_t offset, uint8_t *buffer, size_t length, void *ctx)
{
  (void) offset;
  (void) buffer;
  (void) length;
  (void) ctx;
  TEST_ASSERT(false);
}

// The purpose of this test is to measure the time required to finish an upgrade of an image with size ~100k
// gbl_file stores an app that will be parsed to the address 0x30000.
static void test_gbl_upgrade_parse(void)
{
  int32_t retval = 0;
  uint32_t startTime, endTime;
  ParserContext_t parserContext = { 0 };
  DecryptContext_t decryptContext = { 0 };
  AuthContext_t authContext = { 0 };
  const BootloaderParserCallbacks_t parserCallbacks = {
    NULL,
    bootload_applicationCallback,
    no_callback,
    no_callback,
  };
  ImageProperties_t imageProperties = { 0 };
  imageProperties.instructions = BTL_IMAGE_INSTRUCTION_APPLICATION;

  // Enable DWT in order to measure the execution time of some operations.
#if defined(CoreDebug_DEMCR_TRCENA_Msk)
  CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
#endif
  CYCLE_Init();

  retval = parser_init(&parserContext,
                       &decryptContext,
                       &authContext,
                       0UL);
  TEST_ASSERT_EQUAL_HEX32(0UL, retval);

  startTime = CYCLE_toMs(CYCLE_Count());
  retval = parser_parse((void*)&parserContext,
                        &imageProperties,
                        (uint8_t *)gbl_file,
                        sizeof(gbl_file),
                        &parserCallbacks);
  endTime = CYCLE_toMs(CYCLE_Count());
  TEST_ASSERT_EQUAL_HEX32(0UL, retval);

  UnityPrintf("Time passed (ms):  %d \n", endTime - startTime);

  if (!imageProperties.imageCompleted) {
    TEST_FAIL_MESSAGE("Image parsing not completed, but verification failed.");
  }

  if (!imageProperties.imageVerified) {
    TEST_FAIL_MESSAGE("Image parsing completed, but verification failed.");
  }
}
