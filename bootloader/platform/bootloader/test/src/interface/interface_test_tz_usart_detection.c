/***************************************************************************//**
 * @file interface_test_tz_usart_detection.c
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

#if defined(EFR32MG22C224F512IM40)
#define BTL_DEFAULT_USART SMU_PPUSATD0_USART0

#if defined(USART1_BASE) && defined(USART2_BASE)
#define BTL_UNUSED_USART (SMU_PPUSATD0_USART1 | SMU_PPUSATD0_USART2)
#elif defined(USART1_BASE)
#define BTL_UNUSED_USART SMU_PPUSATD0_USART1
#else
#define BTL_UNUSED_USART 0
#endif
#endif // BRD4182A

#if defined(EFM32PG22C200F512IM40)
#define BTL_DEFAULT_USART SMU_PPUSATD0_USART0

#if defined(USART1_BASE) && defined(USART2_BASE)
#define BTL_UNUSED_USART (SMU_PPUSATD0_USART1 | SMU_PPUSATD0_USART2)
#elif defined(USART1_BASE)
#define BTL_UNUSED_USART SMU_PPUSATD0_USART1
#else
#define BTL_UNUSED_USART 0
#endif
#endif // DK2503A

#if defined(EFR32FG23A010F512GM48)
#define BTL_DEFAULT_USART SMU_PPUSATD0_USART0

#if defined(USART1_BASE) && defined(USART2_BASE)
#define BTL_UNUSED_USART (SMU_PPUSATD0_USART1 | SMU_PPUSATD0_USART2)
#elif defined(USART1_BASE)
#define BTL_UNUSED_USART SMU_PPUSATD0_USART1
#else
#define BTL_UNUSED_USART 0
#endif
#endif // BRD4263B

#if defined(EFR32FG23B010F512IM48)
#define BTL_DEFAULT_USART SMU_PPUSATD0_USART0

#if defined(USART1_BASE) && defined(USART2_BASE)
#define BTL_UNUSED_USART (SMU_PPUSATD0_USART1 | SMU_PPUSATD0_USART2)
#elif defined(USART1_BASE)
#define BTL_UNUSED_USART SMU_PPUSATD0_USART1
#else
#define BTL_UNUSED_USART 0
#endif
#endif // BRD4263C

#if defined(EFR32MG24A010F1536GM48)
#define BTL_DEFAULT_USART SMU_PPUSATD0_USART0

#if defined(USART1_BASE) && defined(USART2_BASE)
#define BTL_UNUSED_USART (SMU_PPUSATD0_USART1 | SMU_PPUSATD0_USART2)
#elif defined(USART1_BASE)
#define BTL_UNUSED_USART SMU_PPUSATD0_USART1
#else
#define BTL_UNUSED_USART 0
#endif
#endif // BRD4186A

#if defined(EFR32MG24A020F1536GM48)
#define BTL_DEFAULT_USART SMU_PPUSATD0_USART0

#if defined(USART1_BASE) && defined(USART2_BASE)
#define BTL_UNUSED_USART (SMU_PPUSATD0_USART1 | SMU_PPUSATD0_USART2)
#elif defined(USART1_BASE)
#define BTL_UNUSED_USART SMU_PPUSATD0_USART1
#else
#define BTL_UNUSED_USART 0
#endif
#endif // BRD4187A

#if defined(EFR32MG24B110F1536GM48)
#define BTL_DEFAULT_USART SMU_PPUSATD0_USART0

#if defined(USART1_BASE) && defined(USART2_BASE)
#define BTL_UNUSED_USART (SMU_PPUSATD0_USART1 | SMU_PPUSATD0_USART2)
#elif defined(USART1_BASE)
#define BTL_UNUSED_USART SMU_PPUSATD0_USART1
#else
#define BTL_UNUSED_USART 0
#endif
#endif // BRD2601A

#if defined(EFR32MG26B410F3200IM48)
#define BTL_DEFAULT_USART SMU_PPUSATD1_USART0

#if defined(USART1_BASE) && defined(USART2_BASE)
#define BTL_UNUSED_USART (SMU_PPUSATD1_USART1 | SMU_PPUSATD1_USART2)
#elif defined(USART1_BASE)
#define BTL_UNUSED_USART SMU_PPUSATD1_USART1
#else
#define BTL_UNUSED_USART 0
#endif
#endif // BRD4116A

#if defined(EFR32MG27C140F768IM40)
#define BTL_DEFAULT_USART SMU_PPUSATD0_USART0

#if defined(USART1_BASE) && defined(USART2_BASE)
#define BTL_UNUSED_USART (SMU_PPUSATD0_USART1 | SMU_PPUSATD0_USART2)
#elif defined(USART1_BASE)
#define BTL_UNUSED_USART SMU_PPUSATD0_USART1
#else
#define BTL_UNUSED_USART 0
#endif
#endif // BRD4194A

#if defined(EFR32FG28B312F1024IM68)
#define BTL_DEFAULT_USART SMU_PPUSATD0_USART0

#if defined(USART1_BASE) && defined(USART2_BASE)
#define BTL_UNUSED_USART (SMU_PPUSATD0_USART1 | SMU_PPUSATD0_USART2)
#elif defined(USART1_BASE)
#define BTL_UNUSED_USART SMU_PPUSATD0_USART1
#else
#define BTL_UNUSED_USART 0
#endif
#endif // BRD4400A

#if defined(EFR32XG2DXFULL)
#define BTL_DEFAULT_USART SMU_PPUSATD0_USART0

#if defined(USART1_BASE) && defined(USART2_BASE)
#define BTL_UNUSED_USART (SMU_PPUSATD0_USART1 | SMU_PPUSATD0_USART2)
#elif defined(USART1_BASE)
#define BTL_UNUSED_USART SMU_PPUSATD0_USART1
#else
#define BTL_UNUSED_USART 0
#endif
#endif // BRD4277A
static void testVerifyPPUSATDConfig(void)
{
  Bootloader_PPUSATDnCLKENnState_t blPPUSATDnCLKENnState = { 0 };
  bootloader_ppusatdnSaveReconfigureState(&blPPUSATDnCLKENnState);
  TEST_ASSERT_EQUAL_HEX32(BTL_DEFAULT_USART, SMU->PPUSATD0 & BTL_DEFAULT_USART);
  TEST_ASSERT_EQUAL_HEX32(0u, SMU->PPUSATD0 & BTL_UNUSED_USART);
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
  // 1. Check the default USART configuration
  // 2. Call the ppusatdn save and re-configure function and check if the correct USART bit is set

  UnityBeginGroup("INTERFACE");
  UnityPrintf("App version %08x\n", sl_app_properties.app.version);

  RUN_TEST(testVerifyPPUSATDConfig, __LINE__);

  UnityEnd();
  UnityPrint("ENDSWO");
  while (1) ;
}
