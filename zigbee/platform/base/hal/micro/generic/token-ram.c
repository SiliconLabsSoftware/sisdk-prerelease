/***************************************************************************//**
 * @file
 * @brief  Ram based token system.  See token-ram.h for details.
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

#include "stack/include/sl_zigbee.h"
#include "hal/hal.h"

uint8_t simulatorEeprom[TOKEN_MAXIMUM_SIZE];  //the exact amount of space needed

// The ram tokens use a layer of indirection to access the actual tokens. This
//  allows the stack to be built against a generic enum identifying the tokens
//  and look up the actual address which may be application dependent
#define TOKEN_MFG TOKEN_DEF
#define DEFINETOKENS
#define TOKEN_DEF(name, creator, iscnt, isidx, type, arraysize, ...) \
  TOKEN_##name##_ADDRESS,
static const uint16_t emAddressLookup[] = {
    #include "stack/config/token-stack.h"
};
#undef TOKEN_DEF
#undef DEFINETOKENS

// When not running on a micro we check size limits for tokens.
#ifdef SL_ZIGBEE_TEST
#define TOKEN_MFG TOKEN_DEF
#define DEFINETOKENS
#define TOKEN_DEF(name, creator, iscnt, isidx, type, arraysize, ...) \
  TOKEN_##name##_END,
static const uint16_t emEndAddressLookup[] = {
    #include "stack/config/token-stack.h"
};
#undef TOKEN_DEF
#undef DEFINETOKENS
#endif

void resetSystemTokens(void)
{
/*
 *    Normally this routine is never called since it will rewrite the tokens.
 *    In RAM based tokens, though, the tokens are no longer non-volatile and
 *    therefore must always be reset upon bootup.
 */
  #define DEFINETOKENS
  #undef TOKEN_MFG
  #define TOKEN_MFG(name, creator, iscnt, isidx, type, arraysize, ...)
  #define TOKEN_DEF(name, creator, iscnt, isidx, type, arraysize, ...)    \
  {                                                                       \
    int8_t i;                                                             \
    type data = __VA_ARGS__;                                              \
    if (arraysize == 1) {                                                 \
      halInternalSetTokenData(TOKEN_##name, 0x7F, &data, sizeof(type)); } \
    else {                                                                \
      for (i = 0; i < arraysize; i++) {                                   \
        halInternalSetTokenData(TOKEN_##name, i, &data, sizeof(type));    \
      }                                                                   \
    }                                                                     \
  }
    #include "stack/config/token-stack.h"
  #undef TOKEN_MFG
  #undef TOKEN_DEF
  #undef DEFINETOKENS
}

sl_status_t halStackInitTokens(void)
{
  // By the time we get here in simulation, tokens have already been initialized
  // by initializeSimulatedEeprom() and TOKEN_MFG_EUI_64 has been set.
  #if !defined(SL_ZIGBEE_TEST)
  resetSystemTokens();
  #endif

  return SL_STATUS_OK;
}

void halInternalGetTokenData(void *data, uint16_t token, uint8_t index, uint8_t len)
{
  uint8_t i;
  uint8_t *ram = (uint8_t*)data;
  uint16_t address = emAddressLookup[token];

  //base token address loaded.  offset to the index
  if (index != 0x7F) {
    address += (index * len);
  }

  {
    DECLARE_INTERRUPT_STATE;
    DISABLE_INTERRUPTS();
    for ( i = 0; i < len; i++ ) {
      ram[i] = simulatorEeprom[address + i];
    }
    RESTORE_INTERRUPTS();
  }
}

void halInternalSetTokenData(uint16_t token, uint8_t index, void *data, uint8_t len)
{
  uint8_t i;
  uint8_t *ram = (uint8_t*)data;
  uint16_t address = emAddressLookup[token];

  //base token address loaded.  offset to the index
  if (index != 0x7F) {
    address += (index * len);
  }

  #ifdef SL_ZIGBEE_TEST
  // Catch cases where the token definitions are not as expected.
  assert(address + len - 1 <= emEndAddressLookup[token]);
  #endif

  {
    DECLARE_INTERRUPT_STATE;
    DISABLE_INTERRUPTS();
    for ( i = 0; i < len; i++ ) {
      simulatorEeprom[address + i] = ram[i];
    }
    RESTORE_INTERRUPTS();
  }
}

// The following interfaces are admittedly code space hogs but serve
// as glue interfaces to link creator codes to tokens (primarily for haltest)

uint16_t getTokenAddress(uint16_t creator)
{
  #define DEFINETOKENS
  switch (creator) {
    #define TOKEN_MFG TOKEN_DEF
    #define TOKEN_DEF(name, creator, iscnt, isidx, type, arraysize, ...) \
  case creator: return TOKEN_##name##_ADDRESS;
    #include "stack/config/token-stack.h"
    #undef TOKEN_MFG
    #undef TOKEN_DEF
  }
  ;
  #undef DEFINETOKENS
  return INVALID_EE_ADDRESS;
}

uint8_t getTokenSize(uint16_t creator)
{
  #define DEFINETOKENS
  switch (creator) {
    #define TOKEN_MFG TOKEN_DEF
    #define TOKEN_DEF(name, creator, iscnt, isidx, type, arraysize, ...) \
  case creator: return sizeof(type);
    #include "stack/config/token-stack.h"
    #undef TOKEN_MFG
    #undef TOKEN_DEF
  }
  ;
  #undef DEFINETOKENS
  return 0;
}

uint8_t getTokenArraySize(uint16_t creator)
{
  #define DEFINETOKENS
  switch (creator) {
    #define TOKEN_MFG TOKEN_DEF
    #define TOKEN_DEF(name, creator, iscnt, isidx, type, arraysize, ...) \
  case creator: return arraysize;
    #include "stack/config/token-stack.h"
    #undef TOKEN_MFG
    #undef TOKEN_DEF
  }
  ;
  #undef DEFINETOKENS
  return 0;
}

void initializeSimulatedEeprom(void)
{
  memset(simulatorEeprom, 0xFF, sizeof(simulatorEeprom));
  resetSystemTokens();
}

uint16_t simulatedEepromSize(void)
{
  return TOKEN_MAXIMUM_SIZE;
}

// Don't have simulated mfg token support to handle this
void halInternalSetMfgTokenData(uint16_t token, void *data, uint8_t len)
{
  (void)token;
  (void)data;
  (void)len;
}

// this size should be defined based on the actual size in the hardware
#define RTCC_RAM_SIZE   32
uint32_t simulatedRtccRam[RTCC_RAM_SIZE];
uint16_t simulatedRtccRamSize(void)
{
  return RTCC_RAM_SIZE * 4;
}
//for now we just assume everything that is written to and read from RTCC RAM is an integer
void halInternalReadRtccRam(uint8_t index, void *data, uint8_t len /* for now it is always just size of integer*/)
{
  (void)len;
  uint32_t *ram = (uint32_t*)data;
  *ram = simulatedRtccRam[index];
}
void halInternalWriteRtccRam(uint8_t index, void *data, uint8_t len /* for now it is always just size of integer*/)
{
  (void)len;
  uint32_t *ram = (uint32_t*)data;
  simulatedRtccRam[index] = *ram;
}
