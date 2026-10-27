/***************************************************************************//**
 * @file sl_iperf_network_interface.h
 * @brief iPerf network interface
 *******************************************************************************
 * # License
 * <b>Copyright 2022 Silicon Laboratories Inc. www.silabs.com</b>
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

#ifndef SL_IPERF_NETWORK_INTERFACE_H
#define SL_IPERF_NETWORK_INTERFACE_H

#ifdef __cplusplus
extern "C" {
#endif

// -----------------------------------------------------------------------------
//                                   Includes
// -----------------------------------------------------------------------------
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include "sl_iperf_config.h"

#if !defined(__STATIC_INLINE)
  #define __STATIC_INLINE             static inline
#endif
#if !defined(__GNUC__)
// IAR
  #if !defined(SL_WEAK)
    #define SL_WEAK                   __weak
  #endif
  #if !defined(SL_PACK_START)
    #define STRINGIZE(X)              #X
    #define SL_PACK_START(X)          _Pragma(STRINGIZE(pack(X)))
    #define SL_PACK_END()             _Pragma("pack()")
  #endif
#else
// GCC
  #if !defined(SL_WEAK) && defined(__GNUC__)
    #define SL_WEAK                   __attribute__ ((weak))
  #endif
  #if !defined(SL_PACK_START)
// GCC always uses 1 byte maximum alignment
    #define SL_PACK_START(x)
    #define SL_PACK_END()
  #endif
#endif

/**************************************************************************//**
 * @addtogroup SL_IPERF_NETWORK_INTERFACE_API Portable interface
 * @ingroup SL_IPERF
 * @{
 *****************************************************************************/

// -----------------------------------------------------------------------------
//                              Macros and Typedefs
// -----------------------------------------------------------------------------

/// iPerf timestamp millisecond data type definition
typedef uint64_t sl_iperf_ts_ms_t;

/// iPerf time data type definition
typedef struct sl_iperf_time {
  /// Seconds
  uint32_t sec;
  /// Microseconds
  uint32_t usec;
} sl_iperf_time_t;

/// iPerf socket IP protocol type definition enum
typedef enum sl_iperf_protocol {
  /// UDP IPv6 protocol
  SL_IPERF_IPROTOV6_UDP = 0,
  /// TCP IPv6 protocol
  SL_IPERF_IPROTOV6_TCP,
  /// UDP IPv4 protocol
  SL_IPERF_IPROTOV4_UDP,
  /// TCP IPv4 protocol
  SL_IPERF_IPROTOV4_TCP
} sl_iperf_protocol_t;

/** @}*/

#ifdef __cplusplus
}
#endif

#endif // SL_IPERF_NETWORK_INTERFACE_H
