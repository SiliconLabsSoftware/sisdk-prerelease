/***************************************************************************//**
 * # License
 * <b> Copyright 2022 Silicon Laboratories Inc. www.silabs.com </b>
 *******************************************************************************
 *
 * The licensor of this software is Silicon Laboratories Inc. Your use of this
 * software is governed by the terms of the Silicon Labs Master Software License
 * Agreement (MSLA) available at
 * https://www.silabs.com/about-us/legal/master-software-license-agreement
 * By installing, copying or otherwise using this software, you agree to the
 * terms of the MSLA.
 *
 ******************************************************************************/

/**
 * @file
 * Export of data collected during system startup
 */

#ifndef __SYSTEM_STARTUP_H__
#define __SYSTEM_STARTUP_H__

#include <stdint.h>

/**
 * @brief Get the GPIO flags captured during the last EM4 wake-up.
 *
 * @return GPIO bitmask containing the pins that caused the wake-up.
 */
uint32_t getWakeUpFlags(void);

/**
 * @brief Get the BURTC main counter frequency.
 *
 * The returned frequency accounts for the configured BURTC clock divider.
 *
 * @return The BURTC main counter frequency in Hz.
 */
uint32_t zw_startup_get_burtc_counter_frequency_hz(void);

#endif /* __SYSTEM_STARTUP_H__ */
