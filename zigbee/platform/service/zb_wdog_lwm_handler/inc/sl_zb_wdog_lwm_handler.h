/*******************************************************************************
 * @file
 * @brief Zigbee watchdog LWM crash handler definitions
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

#ifndef SL_ZB_WDOG_LWM_HANDLER_H
#define SL_ZB_WDOG_LWM_HANDLER_H

/**
 * Register platform Watchdog Manager starve callback for LWM crash capture.
 */
void sl_zb_wdog_lwm_handler_init(void);

#endif /* SL_ZB_WDOG_LWM_HANDLER_H */
