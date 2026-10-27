/***************************************************************************//**
 * @file sl_ftp.h
 * @brief File Transfer Protocol
 *******************************************************************************
 * # License
 * <b>Copyright 2023 Silicon Laboratories Inc. www.silabs.com</b>
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

#ifndef SL_FTP_H
#define SL_FTP_H

#ifdef __cplusplus
extern "C" {
#endif

// -----------------------------------------------------------------------------
//                                   Includes
// -----------------------------------------------------------------------------
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>

#include "sl_common.h"
#include "sl_ftp_config.h"

// -----------------------------------------------------------------------------
//                              Macros and Typedefs
// -----------------------------------------------------------------------------

#ifndef __STATIC_INLINE
#define __STATIC_INLINE static inline
#endif

/// FTP debug macro function
#if SL_FTP_DEBUG
#define sl_ftp_debug(format, ...)  \
  do {                             \
    printf("[FTP_DEBUG] ");        \
    printf(format, ##__VA_ARGS__); \
  } while (0)
#else
#define sl_ftp_debug(format, ...) \
  (void) 0UL;
#endif

/// FTP Localhost string
#define SL_FTP_LOCAL_HOST_STR               "::"

/// FTP invalid socket ID
#define SL_FTP_INVALID_SOCKID               (-1)

/// FTP error value
#define SL_FTP_ERROR                        SL_FTP_INVALID_SOCKID

#if SL_FTP_ENABLE_TFTP_PROTOCOL

/// TFTP Localhost string (alias)
#define SL_TFTP_LOCAL_HOST_STR              SL_FTP_LOCAL_HOST_STR

/// TFTP invalid socket ID (alias)
#define SL_TFTP_INVALID_SOCKID              SL_FTP_INVALID_SOCKID

/// TFTP error value (alias)
#define SL_TFTP_ERROR                       SL_FTP_ERROR

#endif
// -----------------------------------------------------------------------------
//                          Public Function Definitions
// -----------------------------------------------------------------------------

/***************************************************************************//**
 * @brief FTP Init
 * @details Initialize FTP service
 ******************************************************************************/
void sl_ftp_init(void);

/***************************************************************************//**
 * @brief Dump buffer
 * @details Util function to dump buffer in hex format
 * @param[in] data_ptr Data ptr
 * @param[in] data_size Data size
 ******************************************************************************/
void sl_ftp_dump_buff(const uint8_t * const data_ptr, const uint16_t data_size);

/***************************************************************************//**
 * @brief FTP get global IP address
 * @details Portable function
 * @return Global address string representation
 ******************************************************************************/
const char * sl_ftp_get_global_addr(void);

#if SL_FTP_ENABLE_TFTP_PROTOCOL

/// Define SL_TFTP_DEBUG
#if SL_FTP_DEBUG
#ifndef SL_TFTP_DEBUG
#define SL_TFTP_DEBUG (SL_FTP_DEBUG)
#endif
#endif

/***************************************************************************//**
 * @brief TFTP Dump buffer
 * @details Alias function
 *
 * @param[in] data_ptr Data ptr
 * @param[in] data_size Data size
 ******************************************************************************/
__STATIC_INLINE void sl_tftp_dump_buff(const uint8_t * const data_ptr, const uint16_t data_size)
{
  sl_ftp_dump_buff(data_ptr, data_size);
}

/***************************************************************************//**
 * @brief TFTP get global IP address
 * @details Portable function
 * @return Global address string representation
 ******************************************************************************/
__STATIC_INLINE const char * sl_tftp_get_global_addr(void)
{
  return sl_ftp_get_global_addr();
}

/**************************************************************************//**
 * @brief Get UDP address structure
 * @details Allocate address structure and set host address and port
 * @param[in] host Host string
 * @param[in] port Port
 * @return void * Address structure
 *****************************************************************************/
void * sl_tftp_udp_get_addr(const char *host,
                            uint16_t port);

#endif // SL_FTP_ENABLE_TFTP_PROTOCOL

#ifdef __cplusplus
}
#endif

#endif // SL_FTP_H
