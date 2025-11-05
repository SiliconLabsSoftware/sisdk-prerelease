/***************************************************************************//**
 * @file
 * @brief See @ref platform_common for detailed documentation.
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
/** @addtogroup platform_common
 * @brief Compiler and Platform specific definitions and typedefs common to
 * all platforms.
 *
 * platform-common.h provides PLATFORM_HEADER defaults and common definitions.
 * This head should never be included directly, it should only be included
 * by the specific PLATFORM_HEADER used by your platform.
 *
 * See platform-common.h for source code.
 *@{
 */

#ifndef __PLATFORMCOMMON_H__
#define __PLATFORMCOMMON_H__

#ifndef PLATCOMMONOKTOINCLUDE
//  This header should only be included by a PLATFORM_HEADER
  #error  platform-common.h should not be included directly
#endif

////////////////////////////////////////////////////////////////////////////////
// Many of the common definitions must be explicitly enabled by the
//  particular PLATFORM_HEADER being used
////////////////////////////////////////////////////////////////////////////////

#include <string.h>

/**
 * @name Generic Types
 *@{
 */

/**
 * @brief An alias for one, used for clarity.
 */
#define TRUE  1

/**
 * @brief An alias for zero, used for clarity.
 */
#define FALSE 0

#ifndef NULL

/**
 * @brief The null pointer.
 */
#define NULL ((void *)0)
#endif

//@} \\END Generic Types

/**
 * @name  Bit Manipulation Macros
 */
//@{

/**
 * @brief Useful to reference a single bit of a byte.
 */
#define BIT(x) (1U << (x))  // Unsigned avoids compiler warnings re BIT(15)

/**
 * @brief Useful to reference a single bit of an uint32_t type.
 */
#define BIT32(x) (((uint32_t) 1) << (x))

/**
 * @brief Sets \c bit in the \c reg register or byte.
 * @note Assuming \c reg is an IO register, some platforms (such as the
 * AVR) can implement this in a single atomic operation.
 */
#define SETBIT(reg, bit)      (reg) |= BIT(bit)

/**
 * @brief Sets the bits in the \c reg register or the byte
 * as specified in the bitmask \c bits.
 * @note This is never a single atomic operation.
 */
#define SETBITS(reg, bits)    (reg) |= (bits)

/**
 * @brief Clears a bit in the \c reg register or byte.
 * @note Assuming \c reg is an IO register, some platforms (such as the AVR)
 * can implement this in a single atomic operation.
 */
#define CLEARBIT(reg, bit)    (reg) &= ~(BIT(bit))

/**
 * @brief Clears the bits in the \c reg register or byte
 * as specified in the bitmask \c bits.
 * @note This is never a single atomic operation.
 */
#define CLEARBITS(reg, bits)  (reg) &= ~(bits)

/**
 * @brief Returns the value of \c bit within the register or byte \c reg.
 */
#define READBIT(reg, bit)     ((reg) & (BIT(bit)))

/**
 * @brief Returns the value of the bitmask \c bits within
 * the register or byte \c reg.
 */
#define READBITS(reg, bits)   ((reg) & (bits))

//@} \\END Bit Manipulation Macros

////////////////////////////////////////////////////////////////////////////////

/**
 * @name  Byte Manipulation Macros
 */
//@{

/**
 * @brief Returns the low byte of the 16-bit value \c n as an \c uint8_t.
 */
#define LOW_BYTE(n)                     ((uint8_t)(n))

/**
 * @brief Returns the high byte of the 16-bit value \c n as an \c uint8_t.
 */
#define HIGH_BYTE(n)                    LOW_BYTE((n) >> 8)

/**
 * @brief Returns the value built from the two \c uint8_t
 * values \c high and \c low.
 */
#define HIGH_LOW_TO_INT(high, low) (        \
    ((uint16_t) (((uint16_t) (high)) << 8)) \
    + ((uint16_t) ((low) & 0xFFu))          \
    )

/**
 * @brief Returns the value built from the four \c uint8_t as an \c uint32_t.
 */
#define INT8U_TO_INT32U(byte3, byte2, byte1, byte0) ( \
    (((uint32_t) (byte3)) << 24)                      \
    + (((uint32_t) (byte2)) << 16)                    \
    + (((uint32_t) (byte1)) << 8)                     \
    + ((uint32_t) ((byte0) & 0xFFu))                  \
    )

/**
 * @brief Returns the low byte of the 32-bit value \c n as an \c uint8_t.
 */
#define BYTE_0(n)                    ((uint8_t)(n))

/**
 * @brief Returns the second byte of the 32-bit value \c n as an \c uint8_t.
 */
#define BYTE_1(n)                    BYTE_0((n) >> 8)

/**
 * @brief Returns the third byte of the 32-bit value \c n as an \c uint8_t.
 */
#define BYTE_2(n)                    BYTE_0((n) >> 16)

/**
 * @brief Returns the high byte of the 32-bit value \c n as an \c uint8_t.
 */
#define BYTE_3(n)                    BYTE_0((n) >> 24)

/**
 * @brief Returns the fifth byte of the 64-bit value \c n as an \c uint8_t.
 */
#define BYTE_4(n)                    BYTE_0((n) >> 32)

/**
 * @brief Returns the sixth byte of the 64-bit value \c n as an \c uint8_t.
 */
#define BYTE_5(n)                    BYTE_0((n) >> 40)

/**
 * @brief Returns the seventh byte of the 64-bit value \c n as an \c uint8_t.
 */
#define BYTE_6(n)                    BYTE_0((n) >> 48)

/**
 * @brief Returns the high byte of the 64-bit value \c n as an \c uint8_t.
 */
#define BYTE_7(n)                    BYTE_0((n) >> 56)

/**
 * @brief Returns the number of entries in an array.
 */
#define COUNTOF(a) (sizeof(a) / sizeof(a[0]))

//@} \\END Byte manipulation macros

////////////////////////////////////////////////////////////////////////////////

/**
 * @name  Time Manipulation Macros
 */
//@{

/**
 * @brief Returns the elapsed time between two 8 bit values.
 *        Result may not be valid if the time samples differ by more than 127
 */
#define elapsedTimeInt8u(oldTime, newTime) \
  ((uint8_t) ((uint8_t)(newTime) - (uint8_t)(oldTime)))

/**
 * @brief Returns the elapsed time between two 16 bit values.
 *        Result may not be valid if the time samples differ by more than 32767
 */
#define elapsedTimeInt16u(oldTime, newTime) \
  ((uint16_t) ((uint16_t)(newTime) - (uint16_t)(oldTime)))

/**
 * @brief Returns the elapsed time between two 32 bit values.
 *   Result may not be valid if the time samples differ by more than 2147483647
 */
#define elapsedTimeInt32u(oldTime, newTime) \
  ((uint32_t) ((uint32_t)(newTime) - (uint32_t)(oldTime)))

/**
 * @brief Returns true if t1 is greater than t2.  Can only account for 1 wrap
 * around of the variable before it is wrong.
 */
#define MAX_INT8U_VALUE       (0xFF)
#define HALF_MAX_INT8U_VALUE  (0x80)
#define timeGTorEqualInt8u(t1, t2) \
  (elapsedTimeInt8u(t2, t1) <= (HALF_MAX_INT8U_VALUE))

/**
 * @brief Returns true if t1 is greater than t2.  Can only account for 1 wrap
 * around of the variable before it is wrong.
 */
#define MAX_INT16U_VALUE      (0xFFFF)
#define HALF_MAX_INT16U_VALUE (0x8000)
#define timeGTorEqualInt16u(t1, t2) \
  (elapsedTimeInt16u(t2, t1) <= (HALF_MAX_INT16U_VALUE))

/**
 * @brief Returns true if t1 is greater than t2.  Can only account for 1 wrap
 * around of the variable before it is wrong.
 */
#define MAX_INT32U_VALUE      (0xFFFFFFFFUL)
#define HALF_MAX_INT32U_VALUE (0x80000000UL)
#define timeGTorEqualInt32u(t1, t2) \
  (elapsedTimeInt32u(t2, t1) <= (HALF_MAX_INT32U_VALUE))

//@} \\END Time manipulation macros

////////////////////////////////////////////////////////////////////////////////

/**
 * @name Miscellaneous Macros
 */
//@{

#ifndef UNUSED_VAR

/**
 * @description Useful macro for avoiding compiler warnings related to unused
 * function arguments or unused variables.
 */
#define UNUSED_VAR(x) (void)(x)
#endif

#ifdef DEBUG_OFF
// On March 17, 2014, DEBUG_OFF was deprecated and replaced by DEBUG_STRIPPED.
// Some builds still attempt to use DEBUG_OFF and as a result there are
// failures.  On 2016-10-04 implemented a little bandaid here to define
// DEBUG_STRIPPED when debugging is meant to be "off".
  #define DEBUG_STRIPPED
#endif

/**
 * @brief Set debug level based on whether DEBUG or DEBUG_STRIPPED are defined.
 */
#ifndef DEBUG_LEVEL
  #if defined(DEBUG) && defined(DEBUG_STRIPPED)
    #error "DEBUG and DEBUG_OFF cannot be both be defined!"
  #elif defined(DEBUG)
    #define DEBUG_LEVEL FULL_DEBUG
  #elif defined(DEBUG_STRIPPED)
    #define DEBUG_LEVEL NO_DEBUG
  #else
    #define DEBUG_LEVEL BASIC_DEBUG
  #endif
#endif

/**
 * @brief Disable static assertions on compilers that don't support them.
 */
#ifndef STATIC_ASSERT
  #define STATIC_ASSERT(__condition, __errorstr)
#endif

//@} \\END Miscellaneous Macros

#endif //__PLATFORMCOMMON_H__

/** @}  END addtogroup */
