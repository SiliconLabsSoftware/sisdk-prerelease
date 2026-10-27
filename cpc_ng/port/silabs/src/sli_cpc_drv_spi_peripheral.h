/***************************************************************************/ /**
 * @file sli_cpc_drv_spi_peripheral.h
 * @brief Internal SPI peripheral driver definitions for CPC.
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

#ifndef SLI_CPC_DRV_SPI_PERIPHERAL_H
#define SLI_CPC_DRV_SPI_PERIPHERAL_H

#include "sl_cpc_bus.h"
#include "sl_cpc_drv_spi_peripheral.h"
#include "sl_status.h"

/***************************************************************************/ /**
 * Initialize the SPI peripheral CPC bus and bind the shared driver ops.
 *
 * Peripheral-specific init (USART/EUSART) must configure clocks, GPIOs, and
 * the SPI peripheral before calling this function. The driver handle must
 * already be zeroed and initialized by the caller.
 *
 * @param[in] drv      SPI peripheral driver handle. Must not be NULL.
 * @param[in] bus_cfg  Bus configuration. Must not be NULL.
 *
 * @retval SL_STATUS_OK Bus initialized successfully.
 * @retval Other        An error occurred.
 ******************************************************************************/
sl_status_t sli_cpc_drv_spi_peripheral_init(sl_cpc_drv_spi_peripheral_t *drv, const sl_cpc_bus_config_t *bus_cfg);

#endif /* SLI_CPC_DRV_SPI_PERIPHERAL_H */
