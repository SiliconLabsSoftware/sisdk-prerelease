/***************************************************************************//**
 * @file interface_test_tz_param_validation.c
 *
 * @note This test has to be run as an NonSecure application with the
 *       secure application supporting the bootloader interface service.
 *       The test application expects the SKL library to be placed at the
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

typedef struct {
  uint32_t counter;
} reset_counter_t;

// This depends on the configuration of the SKL secure application
#define NS_RAM_OFFSET 0x2000u

// This define should point to a address at the secure region
#define SECURE_ADDRESS_POINTER (FLASH_BASE + 0x7000u)

static void set_reason_counter(uint32_t counter)
{
  reset_counter_t* reset_counter = (reset_counter_t*) (SRAM_BASE + NS_RAM_OFFSET);
  reset_counter->counter = counter;
}

static reset_counter_t get_reason_counter(void)
{
  reset_counter_t* reset_counter = (reset_counter_t*) (SRAM_BASE + NS_RAM_OFFSET);
  return *reset_counter;
}

static void print_test_function(char* message)
{
  UnityPrintf("%s\n", message);
  volatile uint32_t i = 1000u;
  while (i--) ;
}

int main(void)
{
  // Test method:
  // 0. Record the reset counter
  // 1. Increment the reset counter and trigger a TZ secure reset by sending in an address that points to the secure region
  // 2. After a reset, check for the reset reason, it should be BOOTLOADER_RESET_REASON_TZ_FAULT
  // 3. Repeat the above for each function

  sl_main_init();

#if defined(UNITY_OUTPUT_SERIAL)
  UNITY_UsartSetup();
#else
  SWO_Setup();
#endif
  uint32_t current_counter = 0u;
  reset_counter_t reset_counter = { 0 };
  void *secure_pointer = (void*) SECURE_ADDRESS_POINTER;
  BootloaderResetCause_t reset_reason = bootloader_getResetReason();

  if (reset_reason.reason == BOOTLOADER_RESET_REASON_TZ_FAULT) {
    reset_counter = get_reason_counter();
  }

  if (reset_counter.counter == 0u) {
    UnityBeginGroup("TZ Interface Param Validation");
    volatile uint32_t i = 1000u;
    while (i--) ;
  }

  // bootloader_getInfo
  current_counter = 0u;
  if (reset_counter.counter == current_counter) {
    set_reason_counter(++current_counter);
    print_test_function("bootloader_getInfo");
    bootloader_getInfo(secure_pointer);
    TEST_ASSERT(false);
  }
  TEST_ASSERT_EQUAL_HEX32(BOOTLOADER_RESET_REASON_TZ_FAULT, reset_reason.reason);

  // bootloader_parseImageInfo
  current_counter = 1u;
  if (reset_counter.counter == current_counter) {
    set_reason_counter(++current_counter);
    ApplicationData_t app_info;
    uint32_t bootloader_version;
    print_test_function("bootloader_parseImageInfo first argument");
    bootloader_parseImageInfo(secure_pointer, 32u, &app_info, &bootloader_version);
    TEST_ASSERT(false);
  }
  TEST_ASSERT_EQUAL_HEX32(BOOTLOADER_RESET_REASON_TZ_FAULT, reset_reason.reason);

  // bootloader_parseImageInfo
  current_counter = 2u;
  if (reset_counter.counter == current_counter) {
    set_reason_counter(++current_counter);
    uint8_t random_buffer[32] = { 0 };
    uint32_t bootloader_version;
    print_test_function("bootloader_parseImageInfo second argument");
    bootloader_parseImageInfo(random_buffer, 32u, secure_pointer, &bootloader_version);
    TEST_ASSERT(false);
  }
  TEST_ASSERT_EQUAL_HEX32(BOOTLOADER_RESET_REASON_TZ_FAULT, reset_reason.reason);

  // bootloader_parseImageInfo
  current_counter = 3u;
  if (reset_counter.counter == current_counter) {
    set_reason_counter(++current_counter);
    uint8_t random_buffer[32] = { 0 };
    ApplicationData_t app_info;
    print_test_function("bootloader_parseImageInfo third argument");
    bootloader_parseImageInfo(random_buffer, 32u, &app_info, secure_pointer);
    TEST_ASSERT(false);
  }
  TEST_ASSERT_EQUAL_HEX32(BOOTLOADER_RESET_REASON_TZ_FAULT, reset_reason.reason);

  // bootloader_getCertificateVersion
  current_counter = 4u;
  if (reset_counter.counter == current_counter) {
    set_reason_counter(++current_counter);
    print_test_function("bootloader_getCertificateVersion");
    bootloader_getCertificateVersion(secure_pointer);
    TEST_ASSERT(false);
  }
  TEST_ASSERT_EQUAL_HEX32(BOOTLOADER_RESET_REASON_TZ_FAULT, reset_reason.reason);

  // bootloader_getStorageSlotInfo
  current_counter = 5u;
  if (reset_counter.counter == current_counter) {
    set_reason_counter(++current_counter);
    print_test_function("bootloader_getStorageSlotInfo");
    bootloader_getStorageSlotInfo(0u, secure_pointer);
    TEST_ASSERT(false);
  }
  TEST_ASSERT_EQUAL_HEX32(BOOTLOADER_RESET_REASON_TZ_FAULT, reset_reason.reason);

  // bootloader_readStorage
  current_counter = 6u;
  if (reset_counter.counter == current_counter) {
    set_reason_counter(++current_counter);
    print_test_function("bootloader_readStorage");
    bootloader_readStorage(0u, 0u, secure_pointer, 32u);
    TEST_ASSERT(false);
  }
  TEST_ASSERT_EQUAL_HEX32(BOOTLOADER_RESET_REASON_TZ_FAULT, reset_reason.reason);

  // bootloader_writeStorage
  current_counter = 7u;
  if (reset_counter.counter == current_counter) {
    set_reason_counter(++current_counter);
    print_test_function("bootloader_writeStorage");
    bootloader_writeStorage(0u, 0u, secure_pointer, 32u);
    TEST_ASSERT(false);
  }
  TEST_ASSERT_EQUAL_HEX32(BOOTLOADER_RESET_REASON_TZ_FAULT, reset_reason.reason);

  // bootloader_eraseWriteStorage
  current_counter = 8u;
  if (reset_counter.counter == current_counter) {
    set_reason_counter(++current_counter);
    print_test_function("bootloader_eraseWriteStorage");
    bootloader_eraseWriteStorage(0u, 0u, secure_pointer, 32u);
    TEST_ASSERT(false);
  }
  TEST_ASSERT_EQUAL_HEX32(BOOTLOADER_RESET_REASON_TZ_FAULT, reset_reason.reason);

  // bootloader_initChunkedEraseStorageSlot
  current_counter = 9u;
  if (reset_counter.counter == current_counter) {
    set_reason_counter(++current_counter);
    print_test_function("bootloader_initChunkedEraseStorageSlot");
    bootloader_initChunkedEraseStorageSlot(0u, secure_pointer);
    TEST_ASSERT(false);
  }
  TEST_ASSERT_EQUAL_HEX32(BOOTLOADER_RESET_REASON_TZ_FAULT, reset_reason.reason);

  // bootloader_chunkedEraseStorageSlot
  current_counter = 10u;
  if (reset_counter.counter == current_counter) {
    set_reason_counter(++current_counter);
    print_test_function("bootloader_chunkedEraseStorageSlot");
    bootloader_chunkedEraseStorageSlot(secure_pointer);
    TEST_ASSERT(false);
  }
  TEST_ASSERT_EQUAL_HEX32(BOOTLOADER_RESET_REASON_TZ_FAULT, reset_reason.reason);

  // bootloader_setImagesToBootload
  current_counter = 11u;
  if (reset_counter.counter == current_counter) {
    set_reason_counter(++current_counter);
    print_test_function("bootloader_setImagesToBootload");
    bootloader_setImagesToBootload(secure_pointer, 32u);
    TEST_ASSERT(false);
  }
  TEST_ASSERT_EQUAL_HEX32(BOOTLOADER_RESET_REASON_TZ_FAULT, reset_reason.reason);

  // bootloader_getImagesToBootload
  current_counter = 12u;
  if (reset_counter.counter == current_counter) {
    set_reason_counter(++current_counter);
    print_test_function("bootloader_getImagesToBootload");
    bootloader_getImagesToBootload(secure_pointer, 32u);
    TEST_ASSERT(false);
  }
  TEST_ASSERT_EQUAL_HEX32(BOOTLOADER_RESET_REASON_TZ_FAULT, reset_reason.reason);

  // bootloader_getImageInfo
  current_counter = 13u;
  if (reset_counter.counter == current_counter) {
    set_reason_counter(++current_counter);
    uint32_t bootloader_version;
    print_test_function("bootloader_getImageInfo first argument");
    bootloader_getImageInfo(0u, secure_pointer, &bootloader_version);
    TEST_ASSERT(false);
  }
  TEST_ASSERT_EQUAL_HEX32(BOOTLOADER_RESET_REASON_TZ_FAULT, reset_reason.reason);

  // bootloader_getImageInfo
  current_counter = 14u;
  if (reset_counter.counter == current_counter) {
    set_reason_counter(++current_counter);
    ApplicationData_t app_info;
    print_test_function("bootloader_getImageInfo second argument");
    bootloader_getImageInfo(0u, &app_info, secure_pointer);
    TEST_ASSERT(false);
  }
  TEST_ASSERT_EQUAL_HEX32(BOOTLOADER_RESET_REASON_TZ_FAULT, reset_reason.reason);

  // Verify that we have gone through the correct number of resets
  TEST_ASSERT_EQUAL(15u, reset_counter.counter);

  UnityEnd();
  UnityPrint("ENDSWO");
  while (1) ;
}
