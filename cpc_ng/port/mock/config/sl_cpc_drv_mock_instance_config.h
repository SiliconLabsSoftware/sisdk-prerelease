/***************************************************************************/ /**
 * @file sl_cpc_drv_mock_instance_config.h
 * @brief CPC mock driver instance configuration.
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

#ifndef SL_CPC_DRV_MOCK_INSTANCE_CONFIG_H
#define SL_CPC_DRV_MOCK_INSTANCE_CONFIG_H

// <<< Use Configuration Wizard in Context Menu >>>

// <h> CPC Mock Driver Configuration

// <h> Frame pool configuration

// <o SL_CPC_DRV_MOCK_INSTANCE_RX_FRAME_POOL_COUNT> RX frame pool size
// <i> Default: 20
// <i> Number of CPC RX frames that can be allocated for this mock instance
#define SL_CPC_DRV_MOCK_INSTANCE_RX_FRAME_POOL_COUNT 20

// <o SL_CPC_DRV_MOCK_INSTANCE_TX_FRAME_POOL_COUNT> TX frame pool size
// <i> Default: 20
// <i> Number of CPC TX frames that can be allocated for this mock instance
#define SL_CPC_DRV_MOCK_INSTANCE_TX_FRAME_POOL_COUNT 20

// </h>
// </h>
// <<< end of configuration section >>>

#endif /* SL_CPC_DRV_MOCK_INSTANCE_CONFIG_H */
