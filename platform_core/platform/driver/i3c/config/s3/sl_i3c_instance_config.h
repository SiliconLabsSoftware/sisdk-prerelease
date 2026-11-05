/***************************************************************************//**
 * @file
 * @brief I3C Config
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

#ifndef SL_I3C_INSTANCE_CONFIG_H
#define SL_I3C_INSTANCE_CONFIG_H

// <<< Use Configuration Wizard in Context Menu

// <h>I3C settings

// <o SL_I3C_INSTANCE_OPERATING_MODE> Operating mode
// <0=> Controller mode
// <1=> Target mode
// <i> Default: 0
#define SL_I3C_INSTANCE_OPERATING_MODE      0

// <h> I3C Timing (Controller mode)

// <o SL_I3C_INSTANCE_OD_HIGH_COUNT> Open-drain SCL high count <0-255>
// <i> Default: 40
#define SL_I3C_INSTANCE_OD_HIGH_COUNT       40

// <o SL_I3C_INSTANCE_OD_LOW_COUNT> Open-drain SCL low count <0-255>
// <i> Default: 47
#define SL_I3C_INSTANCE_OD_LOW_COUNT        47

// <o SL_I3C_INSTANCE_PP_HIGH_COUNT> Push-pull SCL high count <0-255>
// <i> Default: 5
#define SL_I3C_INSTANCE_PP_HIGH_COUNT       5

// <o SL_I3C_INSTANCE_PP_LOW_COUNT> Push-pull SCL low count <0-255>
// <i> Default: 5
#define SL_I3C_INSTANCE_PP_LOW_COUNT        5

// </h>

// <h> I2C Timing (Controller mode, mixed bus)

// <o SL_I3C_INSTANCE_FM_HIGH_COUNT> I2C FM SCL high count <0-65535>
// <i> Default: 120
#define SL_I3C_INSTANCE_FM_HIGH_COUNT       120

// <o SL_I3C_INSTANCE_FM_LOW_COUNT> I2C FM SCL low count <0-65535>
// <i> Default: 132
#define SL_I3C_INSTANCE_FM_LOW_COUNT        132

// <o SL_I3C_INSTANCE_FMP_HIGH_COUNT> I2C FMP SCL high count <0-255>
// <i> Default: 30
#define SL_I3C_INSTANCE_FMP_HIGH_COUNT      30

// <o SL_I3C_INSTANCE_FMP_LOW_COUNT> I2C FMP SCL low count <0-65535>
// <i> Default: 40
#define SL_I3C_INSTANCE_FMP_LOW_COUNT       40

// <q SL_I3C_INSTANCE_I2C_SLAVE_PRESENT> I2C slaves present on bus
// <i> Default: 0
#define SL_I3C_INSTANCE_I2C_SLAVE_PRESENT   0

// </h>

// <h> Queue and FIFO Thresholds

// <o SL_I3C_INSTANCE_CMD_EMPTY_BUF_THLD> Command empty buffer threshold <0-255>
// <i> Default: 0
#define SL_I3C_INSTANCE_CMD_EMPTY_BUF_THLD  0

// <o SL_I3C_INSTANCE_RESP_BUF_THLD> Response buffer threshold <0-255>
// <i> Default: 0
#define SL_I3C_INSTANCE_RESP_BUF_THLD       0

// <o SL_I3C_INSTANCE_IBI_DATA_THLD> IBI data threshold <1-255>
// <i> Default: 1
#define SL_I3C_INSTANCE_IBI_DATA_THLD       1

// <o SL_I3C_INSTANCE_IBI_STATUS_THLD> IBI status threshold <0-255>
// <i> Default: 0
#define SL_I3C_INSTANCE_IBI_STATUS_THLD     0

// </h>

// <h> Controller Address

// <o SL_I3C_INSTANCE_DYNAMIC_ADDRESS> Controller dynamic address <0-127>
// <i> Default: 8
#define SL_I3C_INSTANCE_DYNAMIC_ADDRESS     8

// <o SL_I3C_INSTANCE_BUS_FREE_TIME> Bus free time <0-65535>
// <i> Default: 10
#define SL_I3C_INSTANCE_BUS_FREE_TIME       10

// </h>

// <h> Target mode settings

// <o SL_I3C_INSTANCE_STATIC_ADDRESS> Target static address <0-127>
// <i> Default: 0
#define SL_I3C_INSTANCE_STATIC_ADDRESS      0

// <o SL_I3C_INSTANCE_TARGET_DCR> Target Device Characteristic Register <0-255>
// <i> Default: 0
#define SL_I3C_INSTANCE_TARGET_DCR          0

// <q SL_I3C_INSTANCE_HDR_CAPABLE> HDR capable
// <i> Default: 0
#define SL_I3C_INSTANCE_HDR_CAPABLE         0

// <q SL_I3C_INSTANCE_MAX_DATA_SPEED_LIMIT> Max data speed limitation
// <i> Default: 0
#define SL_I3C_INSTANCE_MAX_DATA_SPEED_LIMIT 0

// <o SL_I3C_INSTANCE_MFG_ID> MIPI Manufacturer ID <0-32767>
// <i> Default: 0
#define SL_I3C_INSTANCE_MFG_ID             0

// <q SL_I3C_INSTANCE_PROV_ID_RANDOM> Provisional ID random
// <i> Default: 0
#define SL_I3C_INSTANCE_PROV_ID_RANDOM     0

// <o SL_I3C_INSTANCE_PART_ID> Part ID value
// <i> Default: 0
#define SL_I3C_INSTANCE_PART_ID            0

// <o SL_I3C_INSTANCE_BUS_AVAILABLE_TIME> Bus available time <0-65535>
// <i> Default: 100
#define SL_I3C_INSTANCE_BUS_AVAILABLE_TIME 100

// </h>

// </h> end I3C config

// <<< end of configuration section >>>

// <<< sl:start pin_tool >>>
// <i3c signal=SCL,SDA> SL_I3C_INSTANCE
// $[I3C_SL_I3C_INSTANCE]
// #warning "I3C peripheral not configured"
#define SL_I3C_INSTANCE_PERIPHERAL      I3C0
#define SL_I3C_INSTANCE_PERIPHERAL_NO   0
// [I3C_SL_I3C_INSTANCE]$
// <<< sl:end pin_tool >>>

#endif // SL_I3C_INSTANCE_CONFIG_H
