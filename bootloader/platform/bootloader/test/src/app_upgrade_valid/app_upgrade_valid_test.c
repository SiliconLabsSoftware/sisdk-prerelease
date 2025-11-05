/***************************************************************************//**
 * @file delay_test.c
 * @brief Test delay driver
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

#include "parser/gbl/btl_gbl_parser.h"
#include "em_chip.h"
#include "api/btl_interface.h"

// Unity test framework
#include "unity.h"
#if defined(UNITY_OUTPUT_SERIAL)
#include "unity_usart.h"
#else
#include "unity_swo.h"
#endif

#include <string.h>

#if ENABLE_TEST_COVERAGE
extern void __gcov_flush(void);
#endif

extern const MainBootloaderTable_t mbt_valid;
extern const MainBootloaderTable_t mbt_invalid;
extern const MainBootloaderTable_t mbt_invalid_magic;

MainBootloaderTable_t *mainBootloaderTable;

void init(void)
{
//  ApplicationProperties_t *appProps = mainBootloaderTable->startOfAppSpace->signature;
//  UnityPrintf("App version 0x%lx\n", appProps->app.version);
}

void testValidUpgrade(void)
{
  mainBootloaderTable = (void *)&mbt_valid;
  ApplicationData_t appUpgrade = {
    .version = 0x20,
    .productId = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 1, 2, 3, 4, 5, 6, 7 }
  };

  // Version is higher, upgrade should be accepted
  TEST_ASSERT_TRUE(parser_applicationUpgradeValidCallback(&appUpgrade));
}

void testUpgradeAppPropertiesMagic(void)
{
  mainBootloaderTable = (void *)&mbt_invalid_magic;

  ApplicationData_t appUpgrade = {
    .version = 0x10,
    .productId = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 1, 2, 3, 4, 5, 6, 7 }
  };

  // No app properties
  TEST_ASSERT(parser_applicationUpgradeValidCallback(&appUpgrade));
}

void testInvalidUpgradeVersion(void)
{
  mainBootloaderTable = (void *)&mbt_valid;
  ApplicationData_t appUpgrade = {
    .version = 0x10,
    .productId = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 1, 2, 3, 4, 5, 6, 7 }
  };

  // Version is lower, upgrade should be rejected
  TEST_ASSERT_FALSE(parser_applicationUpgradeValidCallback(&appUpgrade));
}

void testInvalidUpgradeProduct(void)
{
  mainBootloaderTable = (void *)&mbt_valid;
  ApplicationData_t appUpgrade = {
    .version = 0x20,
    .productId = { 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4 }
  };

  // Version is higher, but productId is wrong.
  // Upgrade should be rejected.
  TEST_ASSERT_FALSE(parser_applicationUpgradeValidCallback(&appUpgrade));
}

void testUpgradeWithoutExistingVersion(void)
{
  mainBootloaderTable = (void *)&mbt_invalid;

  ApplicationData_t appUpgrade = {
    .version = 0x10,
    .productId = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 1, 2, 3, 4, 5, 6, 7 }
  };

  // Version is lower, but pointer to sl_app_properties is broken.
  // Upgrade should be accepted.
  TEST_ASSERT_TRUE(parser_applicationUpgradeValidCallback(&appUpgrade));
}

int main(void)
{
  CHIP_Init();

#if defined(UNITY_OUTPUT_SERIAL)
  UNITY_UsartSetup();
#else
  SWO_Setup();
#endif

  init();

  UnityBeginGroup("APP_UPGRADE");

  RUN_TEST(testValidUpgrade, __LINE__);
  RUN_TEST(testUpgradeAppPropertiesMagic, __LINE__);
  RUN_TEST(testInvalidUpgradeVersion, __LINE__);
  RUN_TEST(testInvalidUpgradeProduct, __LINE__);
  RUN_TEST(testUpgradeWithoutExistingVersion, __LINE__);

  UnityEnd();

#if ENABLE_TEST_COVERAGE
  __gcov_flush();
#endif

  UnityPrint("ENDSWO");
  while (1) ;
}
