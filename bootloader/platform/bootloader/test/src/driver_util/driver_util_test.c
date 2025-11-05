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

#include "driver/btl_driver_util.h"
#include "em_chip.h"
#include "em_cmu.h"

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

void testHFRCOClock(void)
{
  uint32_t retVal;
  uint32_t sysClock;
  uint32_t clockSource;
#if defined(_SILICON_LABS_32B_SERIES_2)
  sysClock = SystemHCLKGet();
  clockSource = CMU->SYSCLKCTRL & _CMU_SYSCLKCTRL_CLKSEL_MASK;
#else
  sysClock = SystemHFClockGet();
  clockSource = CMU->HFCLKSTATUS & _CMU_HFCLKSTATUS_SELECTED_MASK;
#endif

  // Select reference clock for High Freq. clock
#if defined(_SILICON_LABS_32B_SERIES_2)
  CMU_CLOCK_SELECT_SET(SYSCLK, HFRCODPLL);
  CMU_HFRCODPLLBandSet(cmuHFRCODPLLFreq_1M0Hz);
#else
  CMU_ClockSelectSet(cmuClock_HF, cmuSelect_HFRCO);
  CMU_HFRCOBandSet(cmuHFRCOFreq_1M0Hz);
#endif
  retVal = util_getClockFreq();
  TEST_ASSERT_EQUAL_UINT32(1000000, retVal);

#if defined(_SILICON_LABS_32B_SERIES_2)
  CMU_HFRCODPLLBandSet(cmuHFRCODPLLFreq_2M0Hz);
#else
  CMU_HFRCOBandSet(cmuHFRCOFreq_2M0Hz);
#endif
  retVal = util_getClockFreq();
  TEST_ASSERT_EQUAL_UINT32(2000000, retVal);

#if defined(_SILICON_LABS_32B_SERIES_2)
  CMU_HFRCODPLLBandSet(cmuHFRCODPLLFreq_4M0Hz);
#else
  CMU_HFRCOBandSet(cmuHFRCOFreq_4M0Hz);
#endif
  retVal = util_getClockFreq();
  TEST_ASSERT_EQUAL_UINT32(4000000, retVal);

#if defined(_SILICON_LABS_32B_SERIES_2)
  CMU_HFRCODPLLBandSet(cmuHFRCODPLLFreq_7M0Hz);
#else
  CMU_HFRCOBandSet(cmuHFRCOFreq_7M0Hz);
#endif
  retVal = util_getClockFreq();
  TEST_ASSERT_EQUAL_UINT32(7000000, retVal);

#if defined(_SILICON_LABS_32B_SERIES_2)
  CMU_HFRCODPLLBandSet(cmuHFRCODPLLFreq_13M0Hz);
#else
  CMU_HFRCOBandSet(cmuHFRCOFreq_13M0Hz);
#endif
  retVal = util_getClockFreq();
  TEST_ASSERT_EQUAL_UINT32(13000000, retVal);

#if defined(_SILICON_LABS_32B_SERIES_2)
  CMU_HFRCODPLLBandSet(cmuHFRCODPLLFreq_16M0Hz);
#else
  CMU_HFRCOBandSet(cmuHFRCOFreq_16M0Hz);
#endif
  retVal = util_getClockFreq();
  TEST_ASSERT_EQUAL_UINT32(16000000, retVal);

#if defined(_SILICON_LABS_32B_SERIES_2)
  CMU_HFRCODPLLBandSet(cmuHFRCODPLLFreq_19M0Hz);
#else
  CMU_HFRCOBandSet(cmuHFRCOFreq_19M0Hz);
#endif
  retVal = util_getClockFreq();
  TEST_ASSERT_EQUAL_UINT32(19000000, retVal);

#if defined(_SILICON_LABS_32B_SERIES_2)
  CMU_HFRCODPLLBandSet(cmuHFRCODPLLFreq_26M0Hz);
#else
  CMU_HFRCOBandSet(cmuHFRCOFreq_26M0Hz);
#endif
  retVal = util_getClockFreq();
  TEST_ASSERT_EQUAL_UINT32(26000000, retVal);

#if defined(_SILICON_LABS_32B_SERIES_2)
  CMU_HFRCODPLLBandSet(cmuHFRCODPLLFreq_32M0Hz);
#else
  CMU_HFRCOBandSet(cmuHFRCOFreq_32M0Hz);
#endif
  retVal = util_getClockFreq();
  TEST_ASSERT_EQUAL_UINT32(32000000, retVal);

#if defined(_SILICON_LABS_32B_SERIES_2)
  CMU_HFRCODPLLBandSet(cmuHFRCODPLLFreq_38M0Hz);
#else
  CMU_HFRCOBandSet(cmuHFRCOFreq_38M0Hz);
#endif
  retVal = util_getClockFreq();
  TEST_ASSERT_EQUAL_UINT32(38000000, retVal);

#if defined(_SILICON_LABS_32B_SERIES_2)
  CMU_HFRCODPLLBandSet(cmuHFRCODPLLFreq_48M0Hz);
  retVal = util_getClockFreq();
  TEST_ASSERT_EQUAL_UINT32(48000000, retVal);
#elif defined(_DEVINFO_HFRCOCAL13_MASK)
  CMU_HFRCOBandSet(cmuHFRCOFreq_48M0Hz);
  retVal = util_getClockFreq();
  TEST_ASSERT_EQUAL_UINT32(48000000, retVal);
#endif

#if defined(_SILICON_LABS_32B_SERIES_2)
  CMU_HFRCODPLLBandSet(cmuHFRCODPLLFreq_56M0Hz);
  retVal = util_getClockFreq();
  TEST_ASSERT_EQUAL_UINT32(56000000, retVal);
#elif defined(_DEVINFO_HFRCOCAL14_MASK)
  CMU_HFRCOBandSet(cmuHFRCOFreq_56M0Hz);
  retVal = util_getClockFreq();
  TEST_ASSERT_EQUAL_UINT32(56000000, retVal);
#endif

#if defined(_SILICON_LABS_32B_SERIES_2)
  CMU_HFRCODPLLBandSet(cmuHFRCODPLLFreq_64M0Hz);
  retVal = util_getClockFreq();
  TEST_ASSERT_EQUAL_UINT32(64000000, retVal);
#elif defined(_DEVINFO_HFRCOCAL15_MASK)
  CMU_HFRCOBandSet(cmuHFRCOFreq_64M0Hz);
  retVal = util_getClockFreq();
  TEST_ASSERT_EQUAL_UINT32(64000000, retVal);
#endif

#if defined(_SILICON_LABS_32B_SERIES_2)
  CMU_HFRCODPLLBandSet(cmuHFRCODPLLFreq_80M0Hz);
  retVal = util_getClockFreq();
  TEST_ASSERT_EQUAL_UINT32(80000000, retVal);
#elif defined(_DEVINFO_HFRCOCAL16_MASK)
  CMU_HFRCOBandSet(cmuHFRCOFreq_72M0Hz);
  retVal = util_getClockFreq();
  TEST_ASSERT_EQUAL_UINT32(72000000, retVal);
#endif

  // Recover the original clock frequency.
#if defined(_SILICON_LABS_32B_SERIES_2)
  if (clockSource == CMU_SYSCLKCTRL_CLKSEL_HFXO) {
    CMU_CLOCK_SELECT_SET(SYSCLK, HFXO);
  } else {
    CMU_HFRCODPLLBandSet((CMU_HFRCODPLLFreq_TypeDef)sysClock);
  }
#else
  if (clockSource == CMU_HFCLKSTATUS_SELECTED_HFXO) {
    CMU_ClockSelectSet(cmuClock_HF, cmuSelect_HFXO);
  } else {
    CMU_HFRCOBandSet((CMU_HFRCOFreq_TypeDef)sysClock);
  }
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

  UnityBeginGroup("DRIVER UTIL");

  RUN_TEST(testHFRCOClock, __LINE__);

  UnityEnd();

#if ENABLE_TEST_COVERAGE
  __gcov_flush();
#endif

  UnityPrint("ENDSWO");
  while (1) ;
}
