/***************************************************************************//**
 * @file
 * @brief SL Diagnostic CRC-32
 *******************************************************************************
 * # License
 * <b>Copyright 2026 Silicon Laboratories Inc. www.silabs.com</b>
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

#include "sli_diag_internal.h"

#if SL_DIAG_ENABLE

/*******************************************************************************
 ***************************  LOCAL VARIABLES   ********************************
 ******************************************************************************/

// Nibble table for the reflected polynomial 0xEDB88320. 64 bytes instead of
// the 1 KB of a byte table.
static const uint32_t crc32_nibble_table[16] = {
  0x00000000u, 0x1DB71064u, 0x3B6E20C8u, 0x26D930ACu,
  0x76DC4190u, 0x6B6B51F4u, 0x4DB26158u, 0x5005713Cu,
  0xEDB88320u, 0xF00F9344u, 0xD6D6A3E8u, 0xCB61B38Cu,
  0x9B64C2B0u, 0x86D3D2D4u, 0xA00AE278u, 0xBDBDF21Cu
};

/*******************************************************************************
 ***************************   LOCAL FUNCTIONS   *******************************
 ******************************************************************************/

/*******************************************************************************
 * Updates a CRC-32 state with one byte.
 ******************************************************************************/
static uint32_t crc32_update_byte(uint32_t crc, uint8_t byte)
{
  crc ^= byte;
  // Low nibble, then high nibble.
  crc = (crc >> 4) ^ crc32_nibble_table[crc & 0x0Fu];
  crc = (crc >> 4) ^ crc32_nibble_table[crc & 0x0Fu];
  return crc;
}

/*******************************************************************************
 **************************   GLOBAL FUNCTIONS   *******************************
 ******************************************************************************/

/***************************************************************************//**
 * Updates a CRC-32 state.
 ******************************************************************************/
uint32_t sli_diag_crc32_update(uint32_t crc,
                               const void *data,
                               uint32_t len)
{
  const uint8_t *bytes = (const uint8_t *)data;

  for (uint32_t i = 0u; i < len; i++) {
    crc = crc32_update_byte(crc, bytes[i]);
  }
  return crc;
}

/***************************************************************************//**
 * Updates a CRC-32 state from retained memory.
 ******************************************************************************/
uint32_t sli_diag_crc32_update_retained(uint32_t crc,
                                        const volatile uint8_t *data,
                                        uint32_t len)
{
  for (uint32_t i = 0u; i < len; i++) {
    crc = crc32_update_byte(crc, data[i]);
  }
  return crc;
}

/***************************************************************************//**
 * Computes a CRC-32.
 ******************************************************************************/
uint32_t sli_diag_crc32(const void *data,
                        uint32_t len)
{
  return sli_diag_crc32_update(SLI_DIAG_CRC32_INIT, data, len) ^ SLI_DIAG_CRC32_XOROUT;
}

#endif // SL_DIAG_ENABLE
