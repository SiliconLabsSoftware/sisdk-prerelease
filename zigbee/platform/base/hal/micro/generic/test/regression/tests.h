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
#if defined(INCLUDE_RANDOM_TEST)
void Test_random_Main(void);

#endif

#if defined(INCLUDE_STARTUP_TEST)
void Test_Startup_Main(void);
#endif

#if defined (INCLUDE_INTERRUPT_TEST)
void Test_Interrupt_Main(void);
#endif

#if defined(INCLUDE_MPU_TEST)
void Test_MPU_Main(void);
#endif

// Test-specific Unity setup and teardown functions
typedef void (* voidFuncPtr_t) (void);

extern voidFuncPtr_t testSpecificUnitySetUp;
extern voidFuncPtr_t testSpecificUnityTearDown;

// Helper functions available in all tests //

// Fill data with a value
void fill(uint8_t *data, uint32_t length, uint8_t value);

// Fill data with the values 0, 1, 2, ..., length - 1.
void fillWithRange(uint8_t *data, uint32_t length);

// Print array along with the test report
void debugPrintArray(uint8_t *data, uint32_t length);

// Skip remaining tests
void debugEndEarly(void);

// Calculate a timer delta, taking into account that it might have overflown
uint32_t calculateDelta(uint32_t timeBefore, uint32_t timeAfter, uint32_t top);

// Check if an interrupt is enabled in the NVIC
bool nvicEnabled(size_t interruptNumber);
