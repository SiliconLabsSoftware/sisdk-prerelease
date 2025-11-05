/***************************************************************************//**
 * @file interface_test_tz_config.c
 *******************************************************************************
 * @section License
 * <b>Copyright 2021 Silicon Laboratories, Inc. http://www.silabs.com</b>
 *******************************************************************************
 *
 * This file is licensed under the Silabs License Agreement. See the file
 * "Silabs_License_Agreement.txt" for details. Before using this software for
 * any purpose, you must agree to the terms of that agreement.
 *
 ******************************************************************************/

#include "api/btl_interface.h"
#include "btl_interface_cfg.h"

// Unity test framework
#include "unity.h"
#if defined(UNITY_OUTPUT_SERIAL)
#include "unity_usart.h"
#else
#include "unity_swo.h"
#endif

#include <string.h>
#include "sl_main_init.h"
#include "fault.h"

extern const ApplicationProperties_t sl_app_properties;

#if defined(_SILICON_LABS_32B_SERIES_2_CONFIG_6)
// PPUSATD0, PPUSATD1 is different for XG26 Boards
#define TZ_CONFIG_TEST_PPUSATD0_MASK (SMU_PPUSATD0_HFRCO0 \
                                      | SMU_PPUSATD0_MSC)
#define TZ_CONFIG_TEST_PPUSATD1_MASK (SMU_PPUSATD1_GPCRC \
                                      | SMU_PPUSATD1_SMU)
#else
#define TZ_CONFIG_TEST_PPUSATD0_MASK (SMU_PPUSATD0_LFXO           \
                                      | SMU_PPUSATD0_HFRCO0       \
                                      | SMU_PPUSATD0_PRS          \
                                      | SMU_PPUSATD0_CHIPTESTCTRL \
                                      | SMU_PPUSATD0_TIMER0       \
                                      | SMU_PPUSATD0_TIMER1       \
                                      | SMU_PPUSATD0_TIMER2)
#define TZ_CONFIG_TEST_PPUSATD1_MASK (SMU_PPUSATD1_WDOG0      \
                                      | SMU_PPUSATD1_I2C0     \
                                      | SMU_PPUSATD1_AHBRADIO \
                                      | SMU_PPUSATD1_RADIOAES \
                                      | SMU_PPUSATD1_IADC0    \
                                      | SMU_PPUSATD1_LETIMER0)
#endif

static void testVerifyPPUSATDConfig(void)
{
  UnityPrintf("PPUSATD0 ---------------> %x\n", SMU->PPUSATD0);
  UnityPrintf("PPUSATD0_MASK ---------------> %x\n", TZ_CONFIG_TEST_PPUSATD0_MASK);
  Bootloader_PPUSATDnCLKENnState_t blPPUSATDnCLKENnState = { 0 };
  bootloader_ppusatdnSaveReconfigureState(&blPPUSATDnCLKENnState);

  TEST_ASSERT_EQUAL_HEX32(TZ_CONFIG_TEST_PPUSATD0_MASK, SMU->PPUSATD0 & TZ_CONFIG_TEST_PPUSATD0_MASK);
  TEST_ASSERT_EQUAL_HEX32(TZ_CONFIG_TEST_PPUSATD1_MASK, SMU->PPUSATD1 & TZ_CONFIG_TEST_PPUSATD1_MASK);

  bootloader_ppusatdnRestoreState(&blPPUSATDnCLKENnState);
}

int main(void)
{
  sl_main_init();
  faultInit();

#if defined(UNITY_OUTPUT_SERIAL)
  UNITY_UsartSetup();
#else
  SWO_Setup();
#endif

  // Approach
  // 1. Enable manual override config with a set of peripherals to get updated.
  // 2. Call the ppusatdn save and re-configure function to check if those bits get set

  UnityBeginGroup("INTERFACE");
  UnityPrintf("App version %08x\n", sl_app_properties.app.version);

  BootloaderInformation_t btlInfo;
  bootloader_getInfo(&btlInfo);
  TEST_ASSERT_EQUAL_HEX32(BOOTLOADER_CAPABILITY_PERIPHERAL_LIST,
                          btlInfo.capabilities & BOOTLOADER_CAPABILITY_PERIPHERAL_LIST);

  RUN_TEST(testVerifyPPUSATDConfig, __LINE__);

  UnityEnd();
  UnityPrint("ENDSWO");
  while (1) ;
}
