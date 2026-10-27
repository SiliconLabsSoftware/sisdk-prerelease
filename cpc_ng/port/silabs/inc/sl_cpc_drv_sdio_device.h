/***************************************************************************/ /**
 * @file sl_cpc_drv_sdio_device.h
 * @brief Silicon Labs SDIO device driver interface for CPC.
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

#ifndef SL_CPC_DRV_SDIO_DEVICE_H
#define SL_CPC_DRV_SDIO_DEVICE_H

#include <stddef.h>
#include <stdint.h>

#include "sl_cpc_bus.h"
#include "sli_cpc_drv.h"

struct sli_cpc_drv_sdio_device_caps {
  uint8_t max_aggregation;
};

/** @brief SDIO device CPC driver configuration. */
typedef struct sl_cpc_drv_sdio_device_config {
} sl_cpc_drv_sdio_device_config_t;

typedef struct sl_cpc_drv_sdio_device {
  sl_cpc_bus_t bus;
  struct sli_cpc_drv_sdio_device_caps local_caps;
  struct sli_cpc_drv_sdio_device_caps remote_caps;
} sl_cpc_drv_sdio_device_t;

/***************************************************************************/ /**
 * Initialize an SDIO device CPC driver and its embedded bus.
 *
 * @param[in] drv  SDIO device driver handle. Must not be NULL.
 * @param[in] cfg  SDIO device driver configuration. Must not be NULL.
 * @param[in] bus_cfg  Bus configuration. Must not be NULL.
 *
 * @retval SL_STATUS_OK Driver and bus initialized successfully.
 * @retval Other        An error occurred.
 ******************************************************************************/
sl_status_t sl_cpc_drv_sdio_device_init(sl_cpc_drv_sdio_device_t *drv, const sl_cpc_drv_sdio_device_config_t *cfg,
                                        const sl_cpc_bus_config_t *bus_cfg);

/***************************************************************************/ /**
 * Get the CPC bus embedded in an SDIO device driver instance.
 *
 * @param[in] drv  SDIO device driver handle. Must not be NULL.
 * @return Pointer to the driver's bus.
 ******************************************************************************/
static inline sl_cpc_bus_t *sl_cpc_drv_sdio_device_get_bus(sl_cpc_drv_sdio_device_t *drv)
{
  return &drv->bus;
}

extern sl_cpc_drv_sdio_device_t sl_cpc_drv_sdio_device_instances[];

#endif /* SL_CPC_DRV_SDIO_DEVICE_H */
