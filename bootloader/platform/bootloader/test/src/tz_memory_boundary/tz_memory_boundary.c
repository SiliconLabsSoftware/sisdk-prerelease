/***************************************************************************//**
 * @file tz_memory_boundary.c
 * @brief Test TZ memory boundary function
 *******************************************************************************
 * @section License
 * <b>Copyright 2022 Silicon Laboratories, Inc. http://www.silabs.com</b>
 *******************************************************************************
 *
 * This file is licensed under the Silabs License Agreement. See the file
 * "Silabs_License_Agreement.txt" for details. Before using this software for
 * any purpose, you must agree to the terms of that agreement.
 *
 ******************************************************************************/

#include "em_device.h"
#include "api/btl_interface.h"

// Unity test framework
#include "unity.h"
#if defined(UNITY_OUTPUT_SERIAL)
#include "unity_usart.h"
#else
#include "unity_swo.h"
#endif
#include <arm_cmse.h>
#include <stdbool.h>

#if ENABLE_TEST_COVERAGE
extern void __gcov_flush(void);
#endif

// -----------------------------------------------------------------------------
//                              Defines
// -----------------------------------------------------------------------------

#define NS_FLASH_OFFSET_WITH_FLASH_BASE (FLASH_BASE + NS_FLASH_OFFSET)
#define NS_RAM_OFFSET_WITH_RAM_BASE     (SRAM_BASE + NS_RAM_OFFSET)
#define NS_FLASH_SIZE                   (BTL_APPLICATION_BASE - NS_FLASH_OFFSET_WITH_FLASH_BASE)
#define S_FLASH_SIZE                    (NS_FLASH_OFFSET)
#define NS_RAM_SIZE                     (SRAM_SIZE - NS_RAM_OFFSET)
#define S_RAM_SIZE                      (NS_RAM_OFFSET)

#define PERIPHERALS_BASE_NS_START (0x50000000)
#define PERIPHERALS_BASE_NS_END   (0x5FFFFFFF)
#define PERIPHERALS_BASE_S_START  (0x40000000)
#define PERIPHERALS_BASE_S_END    (0x4FFFFFFF)

#define SECURE_REGION                   (NULL)

// -----------------------------------------------------------------------------
//                          Static Functions
// -----------------------------------------------------------------------------

static void sau_region_validation(cmse_address_info_t cmse_flag,
                                  bool sau_region_valid,
                                  bool secure)
{
  TEST_ASSERT_EQUAL(sau_region_valid, cmse_flag.flags.sau_region_valid);
  TEST_ASSERT_EQUAL(secure, cmse_flag.flags.secure);
  if (secure == true) {
    TEST_ASSERT_EQUAL(false, cmse_flag.flags.nonsecure_read_ok);
    TEST_ASSERT_EQUAL(false, cmse_flag.flags.nonsecure_readwrite_ok);
  }
}

static void memory_boundary_test_sau(void)
{
  int flags = CMSE_NONSECURE;
  cmse_address_info_t cmse_flags;
  void *cmse_ret = NULL;

  // Secure flash
  cmse_flags = cmse_TTAT((void *)FLASH_BASE);
  sau_region_validation(cmse_flags, false, true);
  cmse_flags = cmse_TTAT((void *)(NS_FLASH_OFFSET_WITH_FLASH_BASE - 1u));
  sau_region_validation(cmse_flags, true, true);
  cmse_flags = cmse_TTAT((void *)(FLASH_BASE + FLASH_SIZE));
  sau_region_validation(cmse_flags, false, true);
  cmse_ret = cmse_check_address_range((void *)FLASH_BASE,
                                      S_FLASH_SIZE, flags);
  TEST_ASSERT_EQUAL(SECURE_REGION, cmse_ret);

  // Non-secure flash
  cmse_flags = cmse_TTAT((void *)NS_FLASH_OFFSET_WITH_FLASH_BASE);
  sau_region_validation(cmse_flags, true, false);
  cmse_flags = cmse_TTAT((void *)(BTL_APPLICATION_BASE - 1u));
  sau_region_validation(cmse_flags, true, false);
  cmse_ret = cmse_check_address_range((void *)NS_FLASH_OFFSET_WITH_FLASH_BASE,
                                      NS_FLASH_SIZE,
                                      flags);
  TEST_ASSERT_NOT_EQUAL(SECURE_REGION, cmse_ret);

  // Secure RAM
  cmse_flags = cmse_TTAT((void *)SRAM_BASE);
  sau_region_validation(cmse_flags, false, true);
  cmse_flags = cmse_TTAT((void *)(NS_RAM_OFFSET_WITH_RAM_BASE - 1u));
  sau_region_validation(cmse_flags, false, true);
  cmse_flags = cmse_TTAT((void *)(SRAM_BASE + SRAM_SIZE));
  sau_region_validation(cmse_flags, false, true);
  cmse_ret = cmse_check_address_range((void *)SRAM_BASE,
                                      S_RAM_SIZE, flags);
  TEST_ASSERT_EQUAL(SECURE_REGION, cmse_ret);

  // Non-secure RAM
  cmse_flags = cmse_TTAT((void *)NS_RAM_OFFSET_WITH_RAM_BASE);
  sau_region_validation(cmse_flags, true, false);
  cmse_flags = cmse_TTAT((void *)(SRAM_BASE + SRAM_SIZE - 1u));
  sau_region_validation(cmse_flags, true, false);
  cmse_ret = cmse_check_address_range((void *)NS_RAM_OFFSET_WITH_RAM_BASE,
                                      NS_RAM_SIZE, flags);
  TEST_ASSERT_NOT_EQUAL(SECURE_REGION, cmse_ret);

  // Secure Peripherals: 0x40000000 - 0x4FFFFFFF
  cmse_flags = cmse_TTAT((void *)(PERIPHERALS_BASE_S_START));
  sau_region_validation(cmse_flags, false, true);
  cmse_flags = cmse_TTAT((void *)(PERIPHERALS_BASE_S_END));
  sau_region_validation(cmse_flags, false, true);
  cmse_ret = cmse_check_address_range((void *)PERIPHERALS_BASE_S_START,
                                      PERIPHERALS_BASE_S_END - PERIPHERALS_BASE_S_START,
                                      flags);
  TEST_ASSERT_EQUAL(SECURE_REGION, cmse_ret);

  // Non-Secure Peripherals: 0x50000000 - 0xBFFFFFFF
  // Secure (the boundary -1/+1)
  cmse_flags = cmse_TTAT((void *)(PERIPHERALS_BASE_NS_START - 1u));
  sau_region_validation(cmse_flags, false, true);
  cmse_flags = cmse_TTAT((void *)(PERIPHERALS_BASE_NS_END + 1u));
  sau_region_validation(cmse_flags, true, true);

  // Non-secure
  cmse_flags = cmse_TTAT((void *)PERIPHERALS_BASE_NS_START);
  sau_region_validation(cmse_flags, true, false);
  cmse_flags = cmse_TTAT((void *)(PERIPHERALS_BASE_NS_END));
  sau_region_validation(cmse_flags, true, false);
  cmse_ret = cmse_check_address_range((void *)PERIPHERALS_BASE_NS_START,
                                      PERIPHERALS_BASE_NS_END - PERIPHERALS_BASE_NS_START,
                                      flags);
  TEST_ASSERT_NOT_EQUAL(SECURE_REGION, cmse_ret);
}

static void memory_boundary_test_mpu(void)
{
  cmse_address_info_t cmse_flags;

  cmse_flags = cmse_TTAT((void *)(BTL_APPLICATION_BASE));
  TEST_ASSERT_EQUAL(true, cmse_flags.flags.mpu_region_valid);
  cmse_flags = cmse_TTAT((void *)(FLASH_BASE + FLASH_SIZE - 1));
  TEST_ASSERT_EQUAL(true, cmse_flags.flags.mpu_region_valid);

  bool mpu_enabled = (bool)(MPU->CTRL & MPU_CTRL_ENABLE_Msk);
  TEST_ASSERT_EQUAL(true, mpu_enabled);
  mpu_enabled = (bool)(MPU_NS->CTRL & MPU_CTRL_ENABLE_Msk);
  TEST_ASSERT_EQUAL(true, mpu_enabled);
}

static void memory_boundary_test_page_lock(void)
{
  #define PAGES_TO_BE_LOCKED 3 // This occupies the bootloader region

#if defined(_CMU_CLKEN1_MASK)
  CMU->CLKEN1_SET = CMU_CLKEN1_MSC;
#endif

  for (uint32_t i = 0u; i < PAGES_TO_BE_LOCKED; i++) {
    TEST_ASSERT_EQUAL(true, (MSC->PAGELOCK0 & (1 << i)) >> i);
  }

  TEST_ASSERT_EQUAL(false, (MSC->PAGELOCK0 & (1 << PAGES_TO_BE_LOCKED)) >> PAGES_TO_BE_LOCKED);
}

int memory_boundary_test(void)
{
#if defined(UNITY_OUTPUT_SERIAL)
  UNITY_UsartSetup();
#else
  SWO_Setup();
#endif
  UnityBeginGroup("TZ Memory Boundary");

  memory_boundary_test_sau();
  memory_boundary_test_mpu();
  memory_boundary_test_page_lock();

  UnityEnd();

#if ENABLE_TEST_COVERAGE
  __gcov_flush();
#endif

  UnityPrint("ENDSWO");
  while (1) ;
}
