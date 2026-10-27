/***************************************************************************/ /**
 * @file
 * @brief CPC UART driver — EUSART instance.
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

#include "sl_cpc_drv_uart_eusart.h"
#include "sl_cpc_drv_instances.h"
#include "sli_cpc_drv_uart.h"

/***************************************************************************/ /**
 * Initialize an EUSART UART CPC driver and its embedded bus.
 ******************************************************************************/
sl_status_t sl_cpc_drv_uart_eusart_init(sl_cpc_drv_uart_eusart_t *drv, const sl_cpc_drv_uart_eusart_config_t *cfg,
                                        const sl_cpc_bus_config_t *bus_cfg)
{
  return sli_cpc_drv_uart_init(drv, cfg, bus_cfg);
}

sl_cpc_drv_uart_eusart_t sl_cpc_drv_uart_eusart_instances[SL_CPC_DRV_UART_EUSART_INSTANCES_COUNT];
