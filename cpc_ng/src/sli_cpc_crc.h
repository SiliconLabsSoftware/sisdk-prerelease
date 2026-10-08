/***************************************************************************/ /**
 * @file
 * @brief CPC CRC Definitions
 *******************************************************************************
 * # License
 * <b>Copyright 2019 Silicon Laboratories Inc. www.silabs.com</b>
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

#ifndef SLI_CPC_CRC_H
#define SLI_CPC_CRC_H

#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>

#include "sl_cpc_buf.h"
#include "sl_status.h"

#ifdef __cplusplus
extern "C" {
#endif

/***************************************************************************/ /**
 * Computes CRC-16 XMODEM on given data. Software implementation.
 *
 * @param ptr Pointer to the data to compute the CRC on.
 * @param len Length of the data, in bytes.
 *
 * @return CRC value.
 ******************************************************************************/
uint16_t sli_cpc_crc(const void *ptr, size_t len);

/**
 * @brief Compute the CRC of the bytes in a CPC buffer.
 *
 * @param[in] buf Buffer.
 * @return The computed CRC.
 */
static inline uint16_t sli_cpc_crc_buf(const sl_cpc_buf_t *buf)
{
  return sli_cpc_crc(buf->ptr, buf->len);
}

#ifdef __cplusplus
}
#endif

#endif // SLI_CPC_CRC_H
