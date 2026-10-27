/***************************************************************************/ /**
 * @file sl_cpc_drv_spi.h
 * @brief Silicon Labs SPI driver interface for CPC.
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

#ifndef SLI_CPC_DRV_SPI_H
#define SLI_CPC_DRV_SPI_H

#include "sli_cpc_crc.h"
#include "sli_cpc_types.h"

#define SLI_CPC_DRV_SPI_SOF 0xEB

/***************************************************************************/ /**
 * CPC SPI header structure
 *
 * Each header on the wire is:
 *
 *   [0xEB sync][8-byte header][2-byte header CRC]
 ******************************************************************************/
typedef struct {
  uint8_t sof;
  sli_cpc_hdr_t hdr;
  uint8_t csum_le[SLI_CPC_CRC_LEN];
} sli_cpc_drv_spi_header_t;

#endif /* SLI_CPC_DRV_SPI_H */
