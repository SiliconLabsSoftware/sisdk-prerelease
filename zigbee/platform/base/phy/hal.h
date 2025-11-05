/***************************************************************************//**
 * @file
 * @brief Header file for EmberPHY Legacy HAL
 *******************************************************************************
 * # License
 * <b>Copyright 2021 Silicon Laboratories, Inc, www.silabs.com</b>
 *******************************************************************************
 *
 * SPDX-License-Identifier: Zlib
 *
 * The licensor of this software is Silicon Laboratories Inc.
 *
 * This software is provided 'as-is', without any express or implied
 * warranty. In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 *
 ******************************************************************************/

#ifndef PHY_HAL_H
#define PHY_HAL_H

#ifdef  LEGACY_PHY_BUILD

#ifndef PLATFORM_HEADER_H // Let service/legacy_hal/inc/platform-header.h lead

#ifdef  PLATFORM_HEADER
// Prevent PLATFORM_HEADER from being sucked in too as this replaces it
#undef  PLATFORM_HEADER
#define PLATFORM_HEADER "phy/hal.h"
#endif//PLATFORM_HEADER

//============================================================================
// PLATFORM_HEADER (hal/micro/cortexm3/compiler/gcc|iar.h)

#include <stddef.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "em_device.h"

// Legacy ember types - some still in use today!
typedef bool boolean;
typedef uint8_t int8u;
typedef int8_t int8s;
typedef uint16_t int16u;
typedef int16_t int16s;
typedef uint32_t int32u;
typedef int32_t int32s;
typedef uint64_t int64u;
typedef int64_t int64s;
typedef unsigned long PointerType;

#define HAL_HAS_INT64
#define BIGENDIAN_CPU  false

#ifndef __SOURCEFILE__
  #define __SOURCEFILE__ __FILE__
#endif

#if defined(__GNUC__) // GCC specific macros
  #define EEPROM
  #define NO_STRIPPING __attribute__((used))
  #define UNUSED __attribute__ ((unused))
  #define VAR_AT_SEGMENT(__variableDeclaration, __segmentName) \
  __variableDeclaration __attribute__ ((section(__segmentName)))
  #define asm(...) __asm__ (__VA_ARGS__)
  #define NO_OPERATION() __asm__ ("nop")
  #define ALIGNMENT(__alignmentBytes) \
  __attribute__ ((aligned(__alignmentBytes)))
  #define WEAK(__symbol) __attribute__ ((weak)) __symbol
  #define NO_INIT(__symbol) VAR_AT_SEGMENT(__symbol, __NO_INIT__)
  #define STATIC_ASSERT(__condition, __errorstr) \
  _Static_assert(__condition, __errorstr)
  #define __no_init
#endif // __GNUC__
#if defined(__ICCARM__) // IAR Specific macros
  #pragma diag_suppress = pe161 // warnings about unknown pragmas
  #define EEPROM  errorerror
  #define NO_STRIPPING  __root
  #define UNUSED
  #define VAR_AT_SEGMENT(__variableDeclaration, __segmentName) \
  __variableDeclaration @ __segmentName
  #define NO_OPERATION() __NOP()
  #define STRINGIZE(X) #X
  #define ALIGNMENT(X) _Pragma(STRINGIZE(data_alignment = X))
  #define WEAK(__symbol) __weak __symbol
  #define NO_INIT(__symbol) __no_init __symbol
  #define STATIC_ASSERT(__condition, __errorstr) \
  static_assert(__condition, __errorstr)
#endif // __ICCARM__

#define SIGNED_ENUM

#define MAIN_FUNCTION_PARAMETERS void
#define MAIN_FUNCTION_ARGUMENTS

#ifndef assert
#include <assert.h>

#undef assert
void halInternalAssertFailed(const char *filename, int linenumber);
#define ASSERT_HANDLER halInternalAssertFailed
#define assert(condition)  \
  do { if (!(condition)) { \
         ASSERT_HANDLER(__SOURCEFILE__, __LINE__); } } while (0)
#endif//assert

#define SET_REG_FIELD(reg, field, value)          \
  do {                                            \
    reg = ((reg & (~field##_MASK))                \
           | ((((uint32_t) value) << field##_BIT) \
              & (field##_MASK)));                 \
  } while (0)

#define SET_CMSIS_REG(reg, mask, value)  \
  do {                                   \
    reg = (((reg) & (~mask)) | (value)); \
  } while (0)

#define SET_CMSIS_REG_FIELD(reg, field, value) \
  do {                                         \
    reg = ((reg & (~_##field##_MASK))          \
           | ((value << _##field##_SHIFT)      \
              & (_##field##_MASK)));           \
  } while (0)

#define simulatedTimePasses()
#define simulatedTimePassesMs(x)
#define simulatedSerialTimePasses()

#define _HAL_USE_COMMON_PGM_
#define _HAL_USE_COMMON_DIVMOD_

//============================================================================
// PLATFORM_HEADER (hal/micro/generic/compiler/platform-common.h

#include <stdlib.h>

#define TRUE  1u
#define FALSE 0u
#ifndef NULL
  #define NULL ((void *)0)
#endif

#define BIT(x) (1U << (x))  // Unsigned avoids compiler warnings re BIT(15)
#define BIT32(x) (((uint32_t) 1) << (x))
#define SETBIT(reg, bit)      (reg) |= BIT(bit)
#define SETBITS(reg, bits)    (reg) |= (bits)
#define CLEARBIT(reg, bit)    (reg) &= ~(BIT(bit))
#define CLEARBITS(reg, bits)  (reg) &= ~(bits)
#define READBIT(reg, bit)     ((reg) & (BIT(bit)))
#define READBITS(reg, bits)   ((reg) & (bits))

#define LOW_BYTE(n)                     ((uint8_t)((n) & 0xFF))
#define HIGH_BYTE(n)                    ((uint8_t)(LOW_BYTE((n) >> 8)))
#define HIGH_LOW_TO_INT(high, low) ( \
    (((uint16_t) (high)) << 8)       \
    + ((uint16_t) ((low) & 0xFF))    \
    )
#define INT8U_TO_INT32U(byte3, byte2, byte1, byte0) ( \
    (((uint32_t) (byte3)) << 24)                      \
    + (((uint32_t) (byte2)) << 16)                    \
    + (((uint32_t) (byte1)) << 8)                     \
    + ((uint32_t) ((byte0) & 0xFFu))                  \
    )
#define BYTE_0(n)                    ((uint8_t)((n) & 0xFF))
#define BYTE_1(n)                    BYTE_0((n) >> 8)
#define BYTE_2(n)                    BYTE_0((n) >> 16)
#define BYTE_3(n)                    BYTE_0((n) >> 24)
#define BYTE_4(n)                    BYTE_0((n) >> 32)
#define BYTE_5(n)                    BYTE_0((n) >> 40)
#define BYTE_6(n)                    BYTE_0((n) >> 48)
#define BYTE_7(n)                    BYTE_0((n) >> 56)

#define COUNTOF(a) (sizeof(a) / sizeof(a[0]))

#define elapsedTimeInt8u(oldTime, newTime) \
  ((uint8_t) ((uint8_t)(newTime) - (uint8_t)(oldTime)))
#define elapsedTimeInt16u(oldTime, newTime) \
  ((uint16_t) ((uint16_t)(newTime) - (uint16_t)(oldTime)))
#define elapsedTimeInt32u(oldTime, newTime) \
  ((uint32_t) ((uint32_t)(newTime) - (uint32_t)(oldTime)))
#define MAX_INT8U_VALUE       (0xFF)
#define HALF_MAX_INT8U_VALUE  (0x80)
#define timeGTorEqualInt8u(t1, t2) \
  (elapsedTimeInt8u(t2, t1) <= (HALF_MAX_INT8U_VALUE))
#define MAX_INT16U_VALUE      (0xFFFF)
#define HALF_MAX_INT16U_VALUE (0x8000)
#define timeGTorEqualInt16u(t1, t2) \
  (elapsedTimeInt16u(t2, t1) <= (HALF_MAX_INT16U_VALUE))
#define MAX_INT32U_VALUE      (0xFFFFFFFFUL)
#define HALF_MAX_INT32U_VALUE (0x80000000UL)
#define timeGTorEqualInt32u(t1, t2) \
  (elapsedTimeInt32u(t2, t1) <= (HALF_MAX_INT32U_VALUE))

#ifndef UNUSED_VAR
  #define UNUSED_VAR(x) (void)(x)
#endif

#ifndef STATIC_ASSERT
  #define STATIC_ASSERT(__condition, __errorstr)
#endif

//=============================================================================
// hal/hal.h and sub-include stuff

#include "em_core.h"

#define DECLARE_INTERRUPT_STATE CORE_DECLARE_IRQ_STATE
#define DISABLE_INTERRUPTS() CORE_ENTER_ATOMIC()
#define RESTORE_INTERRUPTS() CORE_EXIT_ATOMIC()
#define INTERRUPTS_ON() CORE_ATOMIC_IRQ_ENABLE()
#define INTERRUPTS_OFF() CORE_ATOMIC_IRQ_DISABLE()
#define INTERRUPTS_ARE_OFF() CORE_IrqIsDisabled()
#define ATOMIC(blah) CORE_ATOMIC_SECTION(blah)
#define HANDLE_PENDING_INTERRUPTS() CORE_YIELD_ATOMIC()

#define MILLISECOND_TICKS_PER_SECOND 1000UL
#ifndef MILLISECOND_TICKS_PER_DECISECOND
  #define MILLISECOND_TICKS_PER_DECISECOND (MILLISECOND_TICKS_PER_SECOND / 10)
#endif
#ifndef MILLISECOND_TICKS_PER_QUARTERSECOND
  #define MILLISECOND_TICKS_PER_QUARTERSECOND (MILLISECOND_TICKS_PER_SECOND >> 2)
#endif
#ifndef MILLISECOND_TICKS_PER_MINUTE
  #define MILLISECOND_TICKS_PER_MINUTE (60UL * MILLISECOND_TICKS_PER_SECOND)
#endif
#ifndef MILLISECOND_TICKS_PER_HOUR
  #define MILLISECOND_TICKS_PER_HOUR (60UL * MILLISECOND_TICKS_PER_MINUTE)
#endif
#ifndef MILLISECOND_TICKS_PER_DAY
  #define MILLISECOND_TICKS_PER_DAY (24UL * MILLISECOND_TICKS_PER_HOUR)
#endif

uint16_t halCommonGetInt16uMillisecondTick(void);
uint32_t halCommonGetInt32uMillisecondTick(void);
uint64_t halCommonGetInt64uMillisecondTick(void);
uint16_t halCommonGetInt16uQuarterSecondTick(void);
void     halCommonDelayMicroseconds(uint16_t us);
void     halCommonDelayMilliseconds(uint16_t ms);
uint16_t halCommonGetRandom(void);

void     halStackIndicateActivity(bool turnOn);
void     halStackProcessBootCount(void);
void     halStackSeedRandom(uint32_t seed);
void     sli_802154phy_radio_seed_random(void);

// From serial/serial.h
typedef uint32_t SerialBaudRate;

#endif//PLATFORM_HEADER_H

#ifdef RTOS
void rtosResetWatchdog(void);
#define halResetWatchdog()  rtosResetWatchdog()
#else
void halResetWatchdog(void);
#endif //RTOS
bool halInternalWatchDogEnabled(void);

#if defined(__GNUC__) // GCC specific macros
  #define NTOHS(val16) ((uint16_t)(((uint16_t)(val16) << 8) \
                                   | ((uint16_t)(val16) >> 8)))
  #define NTOHL(val32) (((uint32_t)(val32) << 24)                  \
                        | (((uint32_t)(val32) & 0x0000FF00U) << 8) \
                        | (((uint32_t)(val32) & 0x00FF0000U) >> 8) \
                        | ((uint32_t)(val32) >> 24))
#endif
#if defined(__ICCARM__) // IAR Specific macros
  #define NTOHS(val) (__REV16(val))
  #define NTOHL(val) (__REV(val))
#endif // __ICCARM__

// From endian.h
#ifndef NTOHS  // some platforms already define this
uint16_t NTOHS(uint16_t val);
#endif
#ifndef NTOHL  // some platforms already define this
uint32_t NTOHL(uint32_t val);
#endif
#ifndef HTONL
  #define HTONL NTOHL
#endif
#ifndef HTONS
  #define HTONS NTOHS
#endif

#include "sl_token_api.h" // also sucks in sl_zigbee_types.h needed by below
#define __TOKEN_H__ // Prevent inclusion of hal/micro/token.h, too.

sl_status_t sli_util_debug_init(void);
sl_status_t halCommonIdleForMilliseconds(uint32_t *duration);

void emDebugPowerDown(void);
void emDebugPowerUp(void);

// From micro-types.h
#ifndef PLAT

#ifdef  SL_ZIGBEE_TEST
  #define PLAT  1 // SL_ZIGBEE_PLATFORM_AVR_ATMEGA
  #define MICRO 2 // ??
#else//!SL_ZIGBEE_TEST
  #define PLAT   4 // SL_ZIGBEE_PLATFORM_CORTEXM3
  #define MICRO 24 // SL_ZIGBEE_MICRO_CORTEXM3_EFR32
#endif//SL_ZIGBEE_TEST
#if defined(PHY_PRO2PLUS)
  #define PHY    6 // SL_ZIGBEE_PHY_PRO2PLUS
#elif defined(PHY_TRANSCEIVER_SIM)
  #define PHY   13 // SL_ZIGBEE_PHY_TRANSCEIVER_SIM
#elif defined(PHY_RAIL)
  #define PHY   15 // SL_ZIGBEE_PHY_RAIL
#elif defined(PHY_RAIL_MP)
  #define PHY   16 // SL_ZIGBEE_PHY_RAIL_MP
#elif defined(PHY_SIMULATION_DUAL)
  #define PHY   17 // SL_ZIGBEE_PHY_SIMULATION_DUAL
#elif defined(PHY_RAILGB)
  #define PHY   18 // SL_ZIGBEE_PHY_RAILGB
#elif defined(PHY_RAILGB_MP)
  #define PHY   19 // SL_ZIGBEE_PHY_RAILGB_MP
#elif defined(PHY_DUALRAIL)
  #define PHY   20 // SL_ZIGBEE_PHY_DUALRAIL
#elif defined(PHY_RAIL_LEGACY_PHY)
  #define PHY   21 // SL_ZIGBEE_PHY_RAIL_LEGACY_PHY
#elif defined(PHY_SIMULATION)
  #define PHY   22 // SL_ZIGBEE_PHY_SIMULATION
#elif defined(PHY_SIMULATION_LEGACY)
  #define PHY   23 // SL_ZIGBEE_PHY_SIMULATION_LEGACY
#else
  #define PHY    0 // SL_ZIGBEE_PHY_NULL
#endif

#endif//PLAT

#else//!LEGACY_PHY_BUILD

#include "../hal/hal.h" // Include base/hal/hal.h instead

#endif//LEGACY_PHY_BUILD

#endif//PHY_HAL_H
