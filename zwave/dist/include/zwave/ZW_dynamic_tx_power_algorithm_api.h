/*******************************************************************************
 * @file
 * Public API of the Dynamic Tx Power Algorithm (DTPA) for Z-Wave Long Range
 *******************************************************************************
 * # License
 * <b>Copyright 2026 Silicon Laboratories Inc. www.silabs.com</b>
 *******************************************************************************
 *
 * SPDX-License-Identifier: LicenseRef-MSLA
 *
 * The licensor of this software is Silicon Laboratories Inc. Your use of this
 * software is governed by the terms of the Silicon Labs Master Software License
 * Agreement (MSLA) available at
 * www.silabs.com/about-us/legal/master-software-license-agreement
 * By installing, copying or otherwise using this software, you agree to the
 * terms of the MSLA.
 *
 ******************************************************************************/

#ifndef ZW_DYNAMIC_TX_POWER_ALGORITHM_API_H_
#define ZW_DYNAMIC_TX_POWER_ALGORITHM_API_H_
#include <stdint.h>

/**
 * @brief Set the noise margin for the dynamic tx power algorithm
 *
 * @details
 * The noise margin is the target margin in deci-dBm that the algorithm tries to maintain between
 * the received signal strength (RSSI) and the noise floor (on the receiver).
 *
 * @note The noise margin is not saved in flash memory, so it will be reset to the default
 * value after a power cycle.
 *
 * @param[in] margin The noise margin in deci-dBm (e.g., 200 for 20.0 dBm)
 */
void ZW_dtpa_set_noise_margin(uint8_t margin);

/**
 * @brief Get the noise margin for the dynamic tx power algorithm
 *
 * @return The current noise margin in deci-dBm (e.g., 200 for 20.0 dBm)
 */
uint8_t ZW_dtpa_get_noise_margin(void);

#endif /* ZW_DYNAMIC_TX_POWER_ALGORITHM_API_H_ */
