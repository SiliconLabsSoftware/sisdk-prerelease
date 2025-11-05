/***************************************************************************//**
 * @file storage_interface_test.c
 * @brief Test bootloader storage interface with real EBL
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
#include "em_cmu.h"
#include "em_emu.h"
#include "em_cryotimer.h"
#include "core/btl_bootload.h"
#include "core/btl_reset.h"

// Unity test framework
#include "unity.h"
#if defined(UNITY_OUTPUT_SERIAL)
#include "unity_usart.h"
#else
#include "unity_swo.h"
#endif

#include <string.h>

extern const ApplicationProperties_t sl_app_properties;

#if defined(_SILICON_LABS_GECKO_INTERNAL_SDID_80)
#if defined(__GNUC__)
const uint8_t bootloaderBlock[16 * 1024] __attribute__((section(".bootloader")));
#elif defined(__ICCARM__)
__no_init __root const uint8_t bootloaderBlock[16 * 1024] @ 0x00000000;
#else
#error "Bootloader space not reserved on this compiler"
#endif
#endif

void HardFault_Handler(void)
{
  UnityPrintf("FAULT\n          ");
  while (1) {
    // Do nothing
  }
}

static void cryotimerInit(void)
{
  CMU_ClockEnable(cmuClock_CRYOTIMER, true); // Enable cryotimer clock
  CMU_ClockSelectSet(cmuClock_LFE, cmuSelect_ULFRCO); // Select ULFRCO oscillator

  CRYOTIMER_Init_TypeDef init = CRYOTIMER_INIT_DEFAULT;
  init.enable = false;
  CRYOTIMER_Init(&init); // Reset Cryotimer
  TEST_ASSERT_UINT_WITHIN(10UL, 10UL, CRYOTIMER_CounterGet());
  init.osc = cryotimerOscULFRCO;
  init.presc = cryotimerPresc_1;
  init.period = cryotimerPeriod_1; // Wake up immediately.
  init.em4Wakeup = true;
  init.enable = true;
  CRYOTIMER_Init(&init); // Initialize Cryotimer
  CRYOTIMER_IntClear(CRYOTIMER_IFC_PERIOD);
  CRYOTIMER_IntEnable(CRYOTIMER_IEN_PERIOD); // Enable cryotimer interrupt
}

#if !defined(_SILICON_LABS_GECKO_INTERNAL_SDID_80)
static void enterEM4(void)
{
  EMU_EM4Init_TypeDef em4Init = EMU_EM4INIT_DEFAULT;
  EMU_EM4Init(&em4Init);
  UnityPrintf("Entering EM4\n");
  volatile uint32_t i = 1000000;
  while (i--) ;
  cryotimerInit();
  EMU_EnterEM4H();
}
#endif

int main(void)
{
  CHIP_Init();
  EMU_UnlatchPinRetention();
#if defined(UNITY_OUTPUT_SERIAL)
  UNITY_UsartSetup();
#else
  SWO_Setup();
#endif
  UnityPrintf("--------------------------------------------------\n");
  UnityPrintf("Entered boot_em4 test main ");
  UnityPrintf("(RMU RSTCAUSE: %08x)\n", RMU->RSTCAUSE);
  UnityPrintf("--------------------------------------------------\n");
  if (RMU->RSTCAUSE == RMU_RSTCAUSE_EM4RST) {
    CMU_ClockEnable(cmuClock_CRYOTIMER, true);
    // Application verification should be skipped.
    // Accepted boot time 0 ms - 20 ms.
    UnityPrintf("Returned to the app after EM4 reset\n");
    TEST_ASSERT_UINT_WITHIN(10UL, 10UL, CRYOTIMER_CounterGet());
    uint32_t clockFreq =  SystemULFRCOClockGet();
    UnityPrintf("ULFRCO freq: %d Hz\n", clockFreq);
    UnityPrintf("Time passed (ms):  %d \n\n\n", CRYOTIMER_CounterGet());
    // Clear the RSTCAUSE register.
    RMU->CMD = RMU_CMD_RCCLR;
    TEST_ASSERT_EQUAL_HEX32(0x0, RMU->RSTCAUSE);
    UnityEnd();
    UnityPrint("ENDSWO");
    while (1) ;
  } else {
    if (reset_getResetReason() == BOOTLOADER_RESET_REASON_GO) {
      CMU_ClockEnable(cmuClock_CRYOTIMER, true);
      // Accepted boot time 100 ms - 200 ms
      TEST_ASSERT_UINT_WITHIN(50UL, 150UL, CRYOTIMER_CounterGet());
      UnityPrintf("Returned to the app \n");
      uint32_t clockFreq =  SystemULFRCOClockGet();
      UnityPrintf("ULFRCO freq: %d Hz\n", clockFreq);
      UnityPrintf("Time passed (ms):  %d \n\n\n", CRYOTIMER_CounterGet());
#if defined(_SILICON_LABS_GECKO_INTERNAL_SDID_80)
      // Finish the test here for dumbo: ERRATA_FIX_EMU_E208_EN
      UnityPrint("ENDSWO");
      while (1) ;
#endif
    } else {
      UnityBeginGroup("BOOT EM4");
      RMU->CTRL = (RMU->CTRL & ~_RMU_CTRL_SYSRMODE_MASK) | RMU_CTRL_SYSRMODE_LIMITED;
      volatile uint32_t i = 1000;
      while (i--) ;
      cryotimerInit();
      reset_setResetReason(BOOTLOADER_RESET_REASON_GO);
      NVIC_SystemReset();
    }
#if !defined(_SILICON_LABS_GECKO_INTERNAL_SDID_80)
    RMU->CTRL = (RMU->CTRL & ~_RMU_CTRL_SYSRMODE_MASK) | RMU_CTRL_SYSRMODE_LIMITED;
    RMU->CMD = RMU_CMD_RCCLR;
    TEST_ASSERT_EQUAL_HEX32(0x0, RMU->RSTCAUSE);
    enterEM4();
#endif
  }
}
