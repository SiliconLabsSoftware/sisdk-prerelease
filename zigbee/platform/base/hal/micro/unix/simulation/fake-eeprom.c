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
// This is a 'fake' EEPROM used only in simulation.

// It emulates the hal/micro/bootloader-eeprom.h API.  The characteristics can
// be set to whatever we want (e.g. read-modify-write vs. page-erase required)
// and we can even fake the timing of erase operations.
// The actual storage is created by a call to malloc().  EEPROM data is saved
// across reboot via the Java simulator.

#include PLATFORM_HEADER

#include STACK_HEADER
#include "hal/hal.h"

#include "hal/micro/unix/simulation/fake-eeprom.h"

#include <stdio.h>
#include <stdlib.h>

//------------------------------------------------------------------------------
// Globals

static uint8_t* dataSpace = NULL;
static uint32_t dataSpaceAddressBase = 0;

#define DEFAULT_PAGE_SIZE 256

static HalEepromInformationType fakeEeprom = {
  EEPROM_INFO_VERSION,
  0,  // capabilities
  FAKE_EEPROM_DEFAULT_PAGE_ERASE_MS,
  0,  // partEraseTime (not used)
  FAKE_EEPROM_DEFAULT_PAGE_SIZE,
  0, // part size (set later)
  "Totally fake EEPROM",
  FAKE_EEPROM_DEFAULT_WORD_SIZE
};

static uint32_t operationStartTimeMs;
static bool operationInProgress = false;

#if defined(SL_ZIGBEE_SCRIPTED_TEST)
  #define debugPrint(...) simPrint(__VA_ARGS__)
#else  // SL_ZIGBEE_TEST assumed
  #define debugPrint(...) fprintf(stderr, __VA_ARGS__)
#endif

static bool eepromInitialized = false;

//------------------------------------------------------------------------------

#ifndef NDEBUG
static bool powerOf2(uint32_t number)
{
  return (number > 0
          && ((number & (number - 1)) == 0));
}
#endif

static bool doesEepromRequirePageErase(void)
{
  return (EEPROM_CAPABILITIES_PAGE_ERASE_REQD
          == (fakeEeprom.capabilitiesMask & EEPROM_CAPABILITIES_PAGE_ERASE_REQD));
}

uint32_t getFakeEepromSize(void)
{
  return fakeEeprom.partSize;
}

uint8_t* getFakeEepromPointer(void)
{
  return dataSpace;
}

static void setupDataSpace(void)
{
  if (dataSpace == NULL) {
    // This data is never freed.  It is assumed to live throughout the life
    // of the node.  Across reboots it is possible to store and reload
    // the contained data via the Java simulator.
    dataSpace = malloc(fakeEeprom.partSize);
    assert(dataSpace != NULL);
    memset(dataSpace, 0xFF, fakeEeprom.partSize);
  }
}

void setupFakeEeprom(uint32_t partSize,
                     uint32_t addressBase,
                     uint16_t pageSize,
                     uint32_t pageEraseMs,
                     bool pageEraseRequired,
                     uint16_t wordSizeBytes)
{
  assert(powerOf2(pageSize));
  assert((partSize % pageSize) == 0);

  fakeEeprom.pageSize = pageSize;
  fakeEeprom.pageEraseMs = pageEraseMs;
  fakeEeprom.wordSizeBytes = wordSizeBytes;

  if (dataSpace != NULL) {
    // If dataSpace != NULL this means the simulator re-initialized
    // us after reboot.  Make sure the size that the simulator thinks
    // we have is the same as the application thinks we have.
    assert(fakeEeprom.partSize == partSize);
  }
  fakeEeprom.partSize = partSize;

  dataSpaceAddressBase = addressBase;
  if (pageEraseRequired) {
    fakeEeprom.capabilitiesMask = (EEPROM_CAPABILITIES_PAGE_ERASE_REQD
                                   | EEPROM_CAPABILITIES_ERASE_SUPPORTED);
  }

  setupDataSpace();
}

// Assumed to be called after simulated reboot to reload EEPROM data.
// We should occur prior to the application setting up the EEPROM
// with setupFakeEeprom().
void reinitializeFakeEeprom(uint32_t length)
{
  assert(dataSpace == NULL);
  fakeEeprom.partSize = length;
  setupDataSpace();
}

const HalEepromInformationType *halEepromInfo(void)
{
  return &fakeEeprom;
}

bool halEepromBusy(void)
{
  if (!operationInProgress) {
    return false;
  }

  if (timeGTorEqualInt32u(halCommonGetInt32uMillisecondTick(),
                          operationStartTimeMs + fakeEeprom.pageEraseMs)) {
    operationInProgress = false;
  }

  return operationInProgress;
}

static uint32_t basicEepromCheckAndReturnAdjustedBaseAddress(uint32_t address)
{
  assert(eepromInitialized);
  assert(dataSpace != NULL);  // setupFakeEeprom() not called.
  assert(address >= dataSpaceAddressBase);
  assert(!operationInProgress);

  address -= dataSpaceAddressBase;

  return address;
}

uint8_t halEepromInit(void)
{
  // Nothing to do.

  // If EEPROM routines are not used then failing to setup the EEPROM
  // via setupFakeEeprom() has no negative effects.

  // We expect read/write routines to assert if attempts are made to use
  // the fake EEPROM if it isn't setup.

  eepromInitialized = true;

  return EEPROM_SUCCESS;
}

void halEepromShutdown(void)
{
  assert(eepromInitialized);

  // We do not free the memory here, since the function is called
  // each time a simulated end-node goes to sleep

  eepromInitialized = false;
}

uint8_t halEepromRead(uint32_t address, uint8_t *data, uint16_t len)
{
  address = basicEepromCheckAndReturnAdjustedBaseAddress(address);
  assert(address + len < fakeEeprom.partSize);

  memcpy(data, dataSpace + address, len);
  return EEPROM_SUCCESS;
}

uint8_t halEepromWrite(uint32_t address, const uint8_t *data, uint16_t len)
{
  address = basicEepromCheckAndReturnAdjustedBaseAddress(address);

  if (doesEepromRequirePageErase()) {
    assert((address % fakeEeprom.wordSizeBytes) == 0);
    assert((len % fakeEeprom.wordSizeBytes) == 0);
  }
  assert(address + len < fakeEeprom.partSize);

  if (doesEepromRequirePageErase()) {
    uint16_t i;
    for (i = 0; i < len; i++) {
      if (dataSpace[address + i] != 0xFF) {
        debugPrint("Flash Memory at address 0x%08X is not erased!\n",
                   address + i);
        assert(0);
      }
    }
  }

  memcpy(dataSpace + address, data, len);

  return EEPROM_SUCCESS;
}

uint8_t halEepromErase(uint32_t address, uint32_t totalLength)
{
  assert((fakeEeprom.capabilitiesMask & EEPROM_CAPABILITIES_ERASE_SUPPORTED)
         == EEPROM_CAPABILITIES_ERASE_SUPPORTED);

  address = basicEepromCheckAndReturnAdjustedBaseAddress(address);

  if (doesEepromRequirePageErase()) {
    assert((address % fakeEeprom.pageSize) == 0);
    assert((totalLength % fakeEeprom.pageSize) == 0);
  }

  memset(dataSpace + address, 0xFF, totalLength);

  if (fakeEeprom.pageEraseMs > 0) {
    operationStartTimeMs = halCommonGetInt32uMillisecondTick();
    operationInProgress = true;
  }

  return EEPROM_SUCCESS;
}

#if !defined(EZSP_HOST)

uint8_t halAppBootloaderInit(void)
{
  return halEepromInit();
}

const HalEepromInformationType* halAppBootloaderInfo(void)
{
  return halEepromInfo();
}

uint8_t halAppBootloaderWriteRawStorage(uint32_t address,
                                        const uint8_t *data,
                                        uint16_t len)
{
  return halEepromWrite(address, data, len);
}

uint8_t halAppBootloaderReadRawStorage(uint32_t address, uint8_t *data, uint16_t len)
{
  return halEepromRead(address, data, len);
}

uint8_t halAppBootloaderEraseRawStorage(uint32_t address, uint32_t len)
{
  return halEepromErase(address, len);
}

bool halAppBootloaderStorageBusy(void)
{
  return halEepromBusy();
}

void halAppBootloaderShutdown(void)
{
  return halEepromShutdown();
}

#endif
