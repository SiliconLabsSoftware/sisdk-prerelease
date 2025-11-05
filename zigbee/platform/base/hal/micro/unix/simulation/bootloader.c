/***************************************************************************//**
 * @file
 * @brief simulation files for the HAL bootloader.
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
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>

#include PLATFORM_HEADER
#include STACK_HEADER
#include "hal/hal.h"

#if defined(SL_ZIGBEE_SCRIPTED_TEST)
  #define debugPrint(x) fprintf(stderr, x)
#else // still assume simulation only
  #define debugPrint(x)  simPrint(x)
#endif

void simPrint(char *format, ...);

bool bootloaderEnabled = true;

// Bootloader simulation stuff.
uint8_t simulatedDownloadSpace[NVM_SEGMENT_MAX * BOOTLOADER_SEGMENT_SIZE];
uint8_t simulatedApplicationSpace[NVM_SEGMENT_MAX * BOOTLOADER_SEGMENT_SIZE];

sl_status_t halLaunchStandaloneBootloaderV1(uint8_t mode, uint8_t *eui64)
{
  (void)mode;
  (void)eui64;
  // This does not usually return.
  return SL_STATUS_FAIL;
}

sl_status_t halAppBootloaderInstallNewImage(void)
{
  return SL_STATUS_FAIL;
}

sl_status_t halLaunchStandaloneBootloader(uint8_t mode)
{
  if (bootloaderEnabled && mode != NO_BOOTLOADER_MODE) {
    debugPrint("Simulating standalone bootloader launch with REBOOT!\n");
    halReboot();
    return SL_STATUS_OK;
  }

  return SL_STATUS_FAIL;
}

uint16_t halGetStandaloneBootloaderVersion(void)
{
  if (!bootloaderEnabled) {
    return 0xFFFF;  // BOOTLOADER_INVALID_VERSION
  }

  return 0x1234;
}

BlBaseType halBootloaderGetType(void)
{
  return BL_TYPE_NULL;
}

BlExtendedType halBootloaderGetInstalledType(void)
{
  return BL_TYPE_NULL;
}

void halGetExtendedBootloaderVersion(uint32_t* getEmberVersion, uint32_t* customerVersion)
{
  *getEmberVersion = 0;
  *customerVersion = 0;
}

uint16_t halGetBootloaderVersion(void)
{
  return 0;
}

// This is an artificial way to test EBL verification in simulation.
// We will just "verify" the arbitrary number of pages below to
// be able to simulate the full state machine for our Zigbee OTA Client
// code that calls into this.
// In theory this could be hooked into the halEeprom() routines and
// actually run against all pages.
static uint16_t currentPage = 0;
static const uint16_t totalPages = 100;
static const uint16_t simulationFailurePage = 50;

bool emAppBootloaderEblVerifySimulationFailure = false;

void halAppBootloaderImageIsValidReset(void)
{
  currentPage = 0;
}

uint16_t halAppBootloaderImageIsValid(void)
{
  currentPage++;

  if (emAppBootloaderEblVerifySimulationFailure
      && (currentPage >= simulationFailurePage)) {
    return 0;
  }

  if (currentPage >= totalPages) {
    return totalPages;
  }
  return BL_IMAGE_IS_VALID_CONTINUE;
}
