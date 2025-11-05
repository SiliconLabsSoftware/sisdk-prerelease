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

#include "driver/btl_driver_delay.h"
#include "em_chip.h"
#include "em_rtcc.h"
#include "em_cmu.h"

#if defined(_SILICON_LABS_GECKO_INTERNAL_SDID_210)  \
  || defined(_SILICON_LABS_GECKO_INTERNAL_SDID_215) \
  || defined(_SILICON_LABS_GECKO_INTERNAL_SDID_220) \
  || defined(_SILICON_LABS_GECKO_INTERNAL_SDID_225) \
  || defined(_SILICON_LABS_GECKO_INTERNAL_SDID_235) \
  || defined(_SILICON_LABS_GECKO_INTERNAL_SDID_260)
#include "sl_hal_sysrtc.h"
#endif

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

void init(void)
{
  CMU_OscillatorEnable(cmuOsc_LFRCO, true, true);
#if defined(_SILICON_LABS_32B_SERIES_1)
  CMU_ClockSelectSet(cmuClock_LFE, cmuSelect_LFRCO);
  CMU_ClockEnable(cmuClock_HFLE, true);
#endif

#if defined(_SILICON_LABS_GECKO_INTERNAL_SDID_210)  \
  || defined(_SILICON_LABS_GECKO_INTERNAL_SDID_215) \
  || defined(_SILICON_LABS_GECKO_INTERNAL_SDID_220) \
  || defined(_SILICON_LABS_GECKO_INTERNAL_SDID_225) \
  || defined(_SILICON_LABS_GECKO_INTERNAL_SDID_235) \
  || defined(_SILICON_LABS_GECKO_INTERNAL_SDID_260)
  sl_sysrtc_config_t sysrtc_config = SYSRTC_CONFIG_DEFAULT;
  sl_sysrtc_group_config_t group_config = SYSRTC_GROUP_CONFIG_DEFAULT;
  CMU_CLOCK_SELECT_SET(SYSRTC, ULFRCO);
  CMU_ClockEnable(cmuClock_SYSRTC, true);
  sl_sysrtc_init(&sysrtc_config);
  group_config.compare_channel0_enable = false;
  sl_sysrtc_init_group(0u, &group_config);
  sl_sysrtc_disable_group_interrupts(0u, _SYSRTC_GRP0_IEN_MASK);
  sl_sysrtc_clear_group_interrupts(0u, _SYSRTC_GRP0_IF_MASK);
  sl_sysrtc_enable();
  sl_sysrtc_set_counter(0u);
#else
  CMU_ClockEnable(cmuClock_RTCC, true);
  RTCC_Init_TypeDef init = RTCC_INIT_DEFAULT;
  RTCC_Init(&init);
#endif
}

void testMsDelayBlocking(void)
{
  delay_init();

  // 100ms blocking delay
  for (int i = 0; i < 10; i++) {
#if defined(_SILICON_LABS_GECKO_INTERNAL_SDID_210)    \
    || defined(_SILICON_LABS_GECKO_INTERNAL_SDID_215) \
    || defined(_SILICON_LABS_GECKO_INTERNAL_SDID_220) \
    || defined(_SILICON_LABS_GECKO_INTERNAL_SDID_225) \
    || defined(_SILICON_LABS_GECKO_INTERNAL_SDID_235) \
    || defined(_SILICON_LABS_GECKO_INTERNAL_SDID_260)
    uint32_t start = sl_sysrtc_get_counter();
#else
    uint32_t start = RTCC_CounterGet();
#endif
    delay_milliseconds(100, true);
#if defined(_SILICON_LABS_GECKO_INTERNAL_SDID_210)    \
    || defined(_SILICON_LABS_GECKO_INTERNAL_SDID_215) \
    || defined(_SILICON_LABS_GECKO_INTERNAL_SDID_220) \
    || defined(_SILICON_LABS_GECKO_INTERNAL_SDID_225) \
    || defined(_SILICON_LABS_GECKO_INTERNAL_SDID_235) \
    || defined(_SILICON_LABS_GECKO_INTERNAL_SDID_260)
    uint32_t diff = sl_sysrtc_get_counter() - start;
#else
    uint32_t diff = RTCC_CounterGet() - start;
#endif
    TEST_ASSERT_UINT_WITHIN(4, 100, diff);
  }
}

void testMsDelay(void)
{
  delay_init();

  // 100ms nonblocking delay
  for (int i = 0; i < 10; i++) {
#if defined(_SILICON_LABS_GECKO_INTERNAL_SDID_210)    \
    || defined(_SILICON_LABS_GECKO_INTERNAL_SDID_215) \
    || defined(_SILICON_LABS_GECKO_INTERNAL_SDID_220) \
    || defined(_SILICON_LABS_GECKO_INTERNAL_SDID_225) \
    || defined(_SILICON_LABS_GECKO_INTERNAL_SDID_235) \
    || defined(_SILICON_LABS_GECKO_INTERNAL_SDID_260)
    uint32_t start = sl_sysrtc_get_counter();
#else
    uint32_t start = RTCC_CounterGet();
#endif
    delay_milliseconds(100, false);

    TEST_ASSERT_FALSE(delay_expired());

    while (!delay_expired()) {
      // Do nothing
    }
#if defined(_SILICON_LABS_GECKO_INTERNAL_SDID_210)    \
    || defined(_SILICON_LABS_GECKO_INTERNAL_SDID_215) \
    || defined(_SILICON_LABS_GECKO_INTERNAL_SDID_220) \
    || defined(_SILICON_LABS_GECKO_INTERNAL_SDID_225) \
    || defined(_SILICON_LABS_GECKO_INTERNAL_SDID_235) \
    || defined(_SILICON_LABS_GECKO_INTERNAL_SDID_260)
    uint32_t diff = sl_sysrtc_get_counter() - start;
#else
    uint32_t diff = RTCC_CounterGet() - start;
#endif
    TEST_ASSERT_UINT_WITHIN(4, 100, diff);
  }
}

void testUsDelay(void)
{
  uint32_t sysClock;
  uint32_t clockSource;
#if defined(_SILICON_LABS_32B_SERIES_2)
  sysClock = SystemHCLKGet();
  clockSource = CMU->SYSCLKCTRL & _CMU_SYSCLKCTRL_CLKSEL_MASK;
#else
  sysClock = SystemHFClockGet();
  clockSource = CMU->HFCLKSTATUS & _CMU_HFCLKSTATUS_SELECTED_MASK;
#endif

  // Set HFRCO frequency.
#if defined(_SILICON_LABS_32B_SERIES_2)
  // Set HFRCO for HF clock.
  CMU_CLOCK_SELECT_SET(SYSCLK, HFRCODPLL);
  CMU_HFRCODPLLBandSet(cmuHFRCODPLLFreq_19M0Hz);
#else
  CMU_ClockSelectSet(cmuClock_HF, cmuSelect_HFRCO);
  CMU_HFRCOBandSet(cmuHFRCOFreq_19M0Hz);
#endif

  // 10 ms delay
#if defined(_SILICON_LABS_GECKO_INTERNAL_SDID_210)  \
  || defined(_SILICON_LABS_GECKO_INTERNAL_SDID_215) \
  || defined(_SILICON_LABS_GECKO_INTERNAL_SDID_220) \
  || defined(_SILICON_LABS_GECKO_INTERNAL_SDID_225) \
  || defined(_SILICON_LABS_GECKO_INTERNAL_SDID_235) \
  || defined(_SILICON_LABS_GECKO_INTERNAL_SDID_260)
  uint32_t start = sl_sysrtc_get_counter();
#else
  uint32_t start = RTCC_CounterGet();
#endif

  delay_microseconds(10000);
#if defined(_SILICON_LABS_GECKO_INTERNAL_SDID_210)  \
  || defined(_SILICON_LABS_GECKO_INTERNAL_SDID_215) \
  || defined(_SILICON_LABS_GECKO_INTERNAL_SDID_220) \
  || defined(_SILICON_LABS_GECKO_INTERNAL_SDID_225) \
  || defined(_SILICON_LABS_GECKO_INTERNAL_SDID_235) \
  || defined(_SILICON_LABS_GECKO_INTERNAL_SDID_260)
  uint32_t diff = sl_sysrtc_get_counter() - start;
#else
  uint32_t diff = RTCC_CounterGet() - start;
#endif
  // delay_microseconds is based on NOP iteration and is not
  // expected to be accurate, however, delay should never be shorter than expected.
  TEST_ASSERT_FALSE(diff < 10);
  TEST_ASSERT_UINT_WITHIN(1, 10, diff);

  // Change HFRCO clock frequency.
#if defined(_SILICON_LABS_32B_SERIES_2)
  CMU_HFRCODPLLBandSet(cmuHFRCODPLLFreq_38M0Hz);
#else
  CMU_HFRCOBandSet(cmuHFRCOFreq_38M0Hz);
#endif

  // 10 ms delay
#if defined(_SILICON_LABS_GECKO_INTERNAL_SDID_210)  \
  || defined(_SILICON_LABS_GECKO_INTERNAL_SDID_215) \
  || defined(_SILICON_LABS_GECKO_INTERNAL_SDID_220) \
  || defined(_SILICON_LABS_GECKO_INTERNAL_SDID_225) \
  || defined(_SILICON_LABS_GECKO_INTERNAL_SDID_235) \
  || defined(_SILICON_LABS_GECKO_INTERNAL_SDID_260)
  start = sl_sysrtc_get_counter();
#else
  start = RTCC_CounterGet();
#endif

  delay_microseconds(10000);
#if defined(_SILICON_LABS_GECKO_INTERNAL_SDID_210)  \
  || defined(_SILICON_LABS_GECKO_INTERNAL_SDID_215) \
  || defined(_SILICON_LABS_GECKO_INTERNAL_SDID_220) \
  || defined(_SILICON_LABS_GECKO_INTERNAL_SDID_225) \
  || defined(_SILICON_LABS_GECKO_INTERNAL_SDID_235) \
  || defined(_SILICON_LABS_GECKO_INTERNAL_SDID_260)
  diff = sl_sysrtc_get_counter() - start;
#else
  diff = RTCC_CounterGet() - start;
#endif
  // delay_microseconds is based on NOP iteration and is not
  // expected to be accurate, however, delay should never be shorter than expected.
  TEST_ASSERT_FALSE(diff < 10);
  TEST_ASSERT_UINT_WITHIN(1, 10, diff);

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

  init();

  UnityBeginGroup("DELAY");

  RUN_TEST(testMsDelayBlocking, __LINE__);
  RUN_TEST(testMsDelay, __LINE__);
  RUN_TEST(testUsDelay, __LINE__);

  UnityEnd();

#if ENABLE_TEST_COVERAGE
  __gcov_flush();
#endif

  UnityPrint("ENDSWO");
  while (1) ;
}
