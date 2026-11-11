/***************************************************************************//**
 * @file app_config.h
 * @brief Throughput measurement app configuration file
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
#ifndef APP_CONFIG_H
#define APP_CONFIG_H

// <<< Use Configuration Wizard in Context Menu >>>

// <h> App settings

// <q THROUGHPUT_DEBUG_LOGS_ENABLED> Enable application debug logs
// <i> Default: 0
#define THROUGHPUT_DEBUG_LOGS_ENABLED      (0)

// </h> End App settings

// <h> Measurement parameters

// <o THROUGHPUT_MEASUREMENT_PERIOD_SEC> Measurement period in seconds
// <i> Default: 5
// <i> Define the measurement window duration in seconds
#define THROUGHPUT_MEASUREMENT_PERIOD_SEC      (5)

// </h> End Measurement parameters

// <h> IEEE 802.15.4 settings

// <o THROUGHPUT_IEEE802154_DEFAULT_FRAME_LENGTH> Fixed frame length in bytes
// <i> Note: This value is used if the user does not set a frame length through the CLI.
// <i> Default: 16
// <i> Define the IEEE 802.15.4 default frame length in bytes.
#define THROUGHPUT_IEEE802154_DEFAULT_FRAME_LENGTH      (16)

// <o THROUGHPUT_IEEE802154_RX_TO_TX_DELAY_US> IEEE 802.15.4 RX to TX delay in microseconds
// <i> Default: 192
// <i> Define the IEEE 802.15.4 RX to TX delay in microseconds.
#define THROUGHPUT_IEEE802154_RX_TO_TX_DELAY_US      (192)

// </h> End IEEE 802.15.4 settings

// <<< end of configuration section >>>

#endif // APP_CONFIG_H
 