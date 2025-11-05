/***************************************************************************//**
 * @file
 * @brief
 *******************************************************************************
 * # License
 * <b>Copyright 2018 Silicon Laboratories Inc. www.silabs.com</b>
 *******************************************************************************
 *
 * The licensor of this software is Silicon Laboratories Inc. Your use of this
 * software is governed by the terms of Silicon Labs Master Software License
 * Agreement (MSLA) available at
 * www.silabs.com/about-us/legal/master-software-license-agreement. This
 * software is distributed to you in Source Code format and is governed by the
 * sections of the MSLA applicable to Source Code.
 *
 ******************************************************************************/
#include PLATFORM_HEADER
#include "hal/micro/micro.h"
#include "mac-child.h"

#include "unity.h"
#if defined(UNITY_OUTPUT_SERIAL)
#include "unity_usart.h"
#else
#include "unity_swo.h"
#endif

#include "tests.h"

voidFuncPtr_t testSpecificUnitySetUp = NULL;
voidFuncPtr_t testSpecificUnityTearDown = NULL;

void setUp(void)
{
  if (testSpecificUnitySetUp != NULL) {
    testSpecificUnitySetUp();
  }

  UNITY_Flush();
}

void tearDown(void)
{
  if (testSpecificUnityTearDown != NULL) {
    testSpecificUnityTearDown();
  }

  UNITY_Flush();
}

int main()
{
  halInit();

  // The new buffer system must be initialized before use. This is required both
  // for VUART operation on EM3xx and certain serial tests on EFR32.
  sli_legacy_buffer_manager_initialize_buffers();

  /* Enable unity test report output via SWO (Cortex-M3/M4) or
   *    USART (Cortex-M0+) */
  UNITY_Init();

#if defined(INCLUDE_RANDOM_TEST)
  Test_random_Main();
#endif

#if defined(INCLUDE_STARTUP_TEST)
  Test_Startup_Main();
#endif

#if defined(INCLUDE_INTERRUPT_TEST)
  Test_Interrupt_Main();
#endif

#if defined(INCLUDE_MPU_TEST)
  Test_MPU_Main();
#endif

  UnityPrint("ENDSWO");
  // Print a couple newlines so it's easier for a human to see a new test run.
  UnityPrintf("\n\n");
  while (1) {
    halInternalResetWatchDog();
  }
}

/////////////// Helper functions available in all tests ////////////////////////

// Fill data with a value
void fill(uint8_t *data, uint32_t length, uint8_t value)
{
  for (uint32_t i = 0; i < length; ++i) {
    data[i] = value;
  }
}

// Fill data with the values 0, 1, 2, ..., length - 1.
void fillWithRange(uint8_t *data, uint32_t length)
{
  for (uint32_t i = 0; i < length; ++i) {
    data[i] = i;
  }
}

// Print array along with the test report
void debugPrintArray(uint8_t *data, uint32_t length)
{
  for (uint32_t i = 0; i < length - 1; ++i) {
    UnityPrintf("%d, ", data[i]);
  }
  UnityPrintf("%d\n", data[length - 1]);
}

// Calculate a timer delta, taking into account that it might have overflown
uint32_t calculateDelta(uint32_t timeBefore, uint32_t timeAfter, uint32_t top)
{
  if (timeAfter > timeBefore) {
    return timeAfter - timeBefore;
  } else {
    // Overflow
    return timeAfter + (top - timeBefore);
  }
}

// Check if an interrupt is enabled in the NVIC
bool nvicEnabled(size_t interruptNumber)
{
  size_t nvic_reg = interruptNumber >> 5UL;
  uint32_t nvic_bit = 1UL << ((uint32_t)interruptNumber & 0x1FUL);
  return (bool)(NVIC->ISER[nvic_reg] & nvic_bit);
}

//////////// Stubs that aren't needed in regression tests //////////////////////

__weak void halButtonIsr(uint8_t button, uint8_t state)
{
}

#include "stack/core/sl_zigbee_stack.h"
#include "stack/include/sl_zigbee.h"
#include "phy/phy.h"

WEAK(void emberRadioTxAckIsrCallback(void)
{
})

void emberRadioTransmitCompleteIsrCallback(EmberStatus status,
                                           uint32_t sfdSentTime,
                                           bool framePending)
{
}

//void emberRadioSfdSentIsrCallback(uint32_t sfdSentTime) {}

RadioTransmitConfig radioTransmitConfig = RADIO_TRANSMIT_CONFIG_NOCCA_DEFAULTS;

void emberRadioReceiveIsrCallback(uint8_t *packet,
                                  bool ackFramePendingSet,
                                  uint32_t time,
                                  uint16_t errors,
                                  int8_t rssi)
{
}

bool sli_mac_long_id_data_pending(sl_802154_long_addr_t address)
{
  return false;
}

void emberRadioMacTimerCompareIsrCallback(void)
{
}

sl_zigbee_network_status_t sli_zigbee_stack_network_state(void)
{
  return SL_ZIGBEE_NO_NETWORK;
}

void emberCalibrateVref(void)
{
}

sl_802154_pan_id_t sli_802154phy_radio_get_pan_id(void)
{
  return 0;
}

int8_t sli_802154phy_get_phy_radio_power(void)
{
  return 0;
}

uint8_t sli_802154phy_get_phy_radio_channel(void)
{
  return 0;
}

sl_802154_short_addr_t sli_802154mac_radio_get_node_id(void)
{
  return 0;
}

void sli_802154phy_radio_sleep(void)
{
}

uint8_t sli_802154mac_local_eui64[8];
