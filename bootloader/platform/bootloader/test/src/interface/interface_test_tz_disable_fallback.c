/***************************************************************************//**
 * @file interface_test_tz_updated_bl_config.c
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
#if defined(_SILICON_LABS_32B_SERIES_2_CONFIG_1)  \
  || defined(_SILICON_LABS_32B_SERIES_2_CONFIG_2) \
  || defined(_SILICON_LABS_32B_SERIES_2_CONFIG_3) \
  || defined(_SILICON_LABS_32B_SERIES_2_CONFIG_4) \
  || defined(_SILICON_LABS_32B_SERIES_2_CONFIG_7) \
  || defined(_SILICON_LABS_32B_SERIES_2_CONFIG_13)
#define TZ_CONFIG_TEST_PPUSATD0_MASK (SMU_PPUSATD0_MSC      \
                                      | SMU_PPUSATD0_CMU    \
                                      | SMU_PPUSATD0_HFRCO0 \
                                      | SMU_PPUSATD0_GPIO   \
                                      | SMU_PPUSATD0_GPCRC  \
                                      | SMU_PPUPATD0_LDMA   \
                                      | SMU_PPUSATD0_LDMAXBAR)
#elif defined(_SILICON_LABS_32B_SERIES_2_CONFIG_6)
#define TZ_CONFIG_TEST_PPUSATD0_MASK (SMU_PPUSATD0_MSC      \
                                      | SMU_PPUSATD0_CMU    \
                                      | SMU_PPUSATD0_HFRCO0 \
                                      | SMU_PPUPATD0_LDMA   \
                                      | SMU_PPUSATD0_LDMAXBAR)
#else
#define TZ_CONFIG_TEST_PPUSATD0_MASK (SMU_PPUSATD0_MSC      \
                                      | SMU_PPUSATD0_CMU    \
                                      | SMU_PPUSATD0_HFRCO0 \
                                      | SMU_PPUSATD0_GPCRC  \
                                      | SMU_PPUPATD0_LDMA   \
                                      | SMU_PPUSATD0_LDMAXBAR)
#endif
#if defined(SMU_PPUSATD1_CRYPTOACC)
  #define TZ_CONFIG_TEST_PPUSATD1_MASK SMU_PPUSATD1_CRYPTOACC
#endif
#if defined(SMU_PPUPATD1_SEMAILBOX)
  #define TZ_CONFIG_TEST_PPUSATD1_MASK SMU_PPUPATD1_SEMAILBOX
#elif defined(SMU_PPUPATD2_SEMAILBOX)
  #define TZ_CONFIG_TEST_PPUSATD2_MASK SMU_PPUPATD2_SEMAILBOX
#endif

static void testVerifyPPUSATDConfig(void)
{
  Bootloader_PPUSATDnCLKENnState_t blPPUSATDnCLKENnState = { 0 };
  bootloader_ppusatdnSaveReconfigureState(&blPPUSATDnCLKENnState);
  UnityPrintf("PPUSATD0 ---------------> %x\n", SMU->PPUSATD0);
  UnityPrintf("PPUSATD0_MASK ---------------> %x\n", TZ_CONFIG_TEST_PPUSATD0_MASK);
  TEST_ASSERT_EQUAL_HEX32(TZ_CONFIG_TEST_PPUSATD0_MASK, SMU->PPUSATD0 & TZ_CONFIG_TEST_PPUSATD0_MASK);
  #if defined(SMU_PPUPATD1_SEMAILBOX)
  TEST_ASSERT_EQUAL_HEX32(TZ_CONFIG_TEST_PPUSATD1_MASK, SMU->PPUSATD1 & TZ_CONFIG_TEST_PPUSATD1_MASK);
  #elif defined(SMU_PPUPATD2_SEMAILBOX)
  TEST_ASSERT_EQUAL_HEX32(TZ_CONFIG_TEST_PPUSATD2_MASK, SMU->PPUSATD2 & TZ_CONFIG_TEST_PPUSATD2_MASK);
  #endif

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
  // 1. Disable multi tiered fallback logic
  // 2. Call the ppusatdn save and re-configure function to check if the expected bits are set

  UnityBeginGroup("INTERFACE");
  UnityPrintf("App version %08x\n", sl_app_properties.app.version);

  RUN_TEST(testVerifyPPUSATDConfig, __LINE__);

  UnityEnd();
  UnityPrint("ENDSWO");
  while (1) ;
}
