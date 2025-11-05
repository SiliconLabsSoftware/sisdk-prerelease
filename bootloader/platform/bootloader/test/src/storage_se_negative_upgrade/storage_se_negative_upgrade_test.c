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
#include "sli_se_manager_mailbox.h"

#define UPGRADE_ADDRESS 0x20000UL

extern const ApplicationProperties_t sl_app_properties;

#if defined(_SILICON_LABS_GECKO_INTERNAL_SDID_200)
#if defined(__GNUC__)
const uint8_t bootloaderBlock[16 * 1024] __attribute__((section(".bootloader")));
#elif defined(__ICCARM__)
__no_init __root const uint8_t bootloaderBlock[16 * 1024] @ 0x00000000;
#else
#error "Bootloader space not reserved on this compiler"
#endif
#elif defined(_SILICON_LABS_GECKO_INTERNAL_SDID_205)
#if defined(__GNUC__)
const uint8_t bootloaderBlock[24 * 1024] __attribute__((section(".bootloader")));
#elif defined(__ICCARM__)
__no_init __root const uint8_t bootloaderBlock[24 * 1024] @ 0x00000000;
#else
#error "Bootloader space not reserved on this compiler"
#endif
#elif defined(_SILICON_LABS_GECKO_INTERNAL_SDID_210) || defined(_SILICON_LABS_GECKO_INTERNAL_SDID_215) \
  || defined(_SILICON_LABS_GECKO_INTERNAL_SDID_220) || defined(_SILICON_LABS_GECKO_INTERNAL_SDID_225)  \
  || defined(_SILICON_LABS_GECKO_INTERNAL_SDID_230) || defined(_SILICON_LABS_GECKO_INTERNAL_SDID_235)  \
  || defined(_SILICON_LABS_GECKO_INTERNAL_SDID_240) || defined(_SILICON_LABS_GECKO_INTERNAL_SDID_260)
#if defined(__GNUC__)
const uint8_t bootloaderBlock[24 * 1024] __attribute__((section(".bootloader")));
#elif defined(__ICCARM__)
__no_init __root const uint8_t bootloaderBlock[24 * 1024] @ 0x08000000;
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

#if defined(SEMAILBOX_PRESENT)
static int32_t seSigVerifyStatus(void)
{
  sli_se_mailbox_response_t response;
  sli_se_mailbox_command_t checkImage = SLI_SE_MAILBOX_COMMAND_DEFAULT(SLI_SE_COMMAND_CHECK_SE_IMAGE);
  sli_se_mailbox_command_add_parameter(&checkImage, UPGRADE_ADDRESS);

  sli_se_mailbox_execute_command(&checkImage);
  response = sli_se_mailbox_read_response();

  if (response != SLI_SE_RESPONSE_OK) {
    return response;
  }
  return SLI_SE_RESPONSE_OK;
}
#endif

static void testSEWrongSignature(void)
{
  TEST_ASSERT_EQUAL(BOOTLOADER_OK, bootloader_init());

  // The storage space of SE upgrade image does not overlap with the application,
  // the bootloader should allow re-entering the application.
  if (!bootload_commitSeUpgrade(UPGRADE_ADDRESS)) {
#if defined(_SILICON_LABS_GECKO_INTERNAL_SDID_205)
    UnityPrintf("This should not be printed\n");
    TEST_ASSERT_TRUE(false);
#else
    UnityPrintf("SE rejected the upgrade image\n");
    int i = 1000;
    while (i--) ;
#endif
    // Check if after this reset, it is possible to reboot the application.
    reset_resetWithReason(BOOTLOADER_RESET_REASON_BADIMAGE);
  }

  // Should never get here.
  UnityPrintf("This should not be printed\n");
  TEST_ASSERT_TRUE(false);
}

int main(void)
{
  CHIP_Init();

#if defined(UNITY_OUTPUT_SERIAL)
  UNITY_UsartSetup();
#else
  SWO_Setup();
#endif

  if (reset_getResetReason() == BOOTLOADER_RESET_REASON_BADIMAGE) {
#if defined(_SILICON_LABS_GECKO_INTERNAL_SDID_205)
    // The mailbox status has already been invalidated, by SE_ackCommand()
    // sli_se_mailbox_read_response will never return...

    // int32_t response = sli_se_mailbox_read_response();
    // TEST_ASSERT_EQUAL(SLI_SE_RESPONSE_INVALID_SIGNATURE, response);
#elif defined(SEMAILBOX_PRESENT)
    int32_t response = seSigVerifyStatus();
    TEST_ASSERT_EQUAL(SLI_SE_RESPONSE_INVALID_SIGNATURE, response);
#endif
    // The image with wrong signature has been successfully rejected and returned to the application.
    UnityPrintf("The image with wrong signature successfully rejected\n");
    UnityEnd();

    UnityPrint("ENDSWO");
    while (1) ;
  } else {
    UnityBeginGroup("STORAGE_INTERFACE_SE_UPGRADE");
    UnityPrintf("App version %d\n", sl_app_properties.app.version);

    int i = 1000;
    while (i--) ;

    RUN_TEST(testSEWrongSignature, __LINE__);
  }
}
