/***************************************************************************//**
 * @file sl_wisun_direct_connect_pmk.h
 * @brief Direct Connect PMK provisioning
 *******************************************************************************
 * # License
 * <b>Copyright 2026 Silicon Laboratories Inc. www.silabs.com</b>
 *******************************************************************************
 *
 * The licensor of this software is Silicon Laboratories Inc. Your use of this
 * software is governed by the terms of Silicon Labs Master Software License
 * Agreement (MSLA) available at
 * www.silabs.com/about-us/legal/master-software-license-agreement. This
 * software is distributed to you in Source Code format and is governed by the
 * sections of the MSLA applicable to Source Code.
 *
 ******************************************************************************/

#ifndef SL_WISUN_DIRECT_CONNECT_PMK_H
#define SL_WISUN_DIRECT_CONNECT_PMK_H

#include "sl_status.h"

/**************************************************************************//**
 * The wisun_direct_connect_pmk component stores the Direct Connect Pairwise
 * Master Key (PMK) and imports it into a fixed PSA key slot. Both the Direct
 * Connect server and client must be provisioned with the same PMK.
 *
 * Configuration
 * Configure the PMK value and PSA key ID in sl_wisun_direct_connect_pmk_config.h
 * or via the Simplicity Studio component UI:
 * - #SL_WISUN_DIRECT_CONNECT_PMK — colon-separated hex string (32 bytes)
 * - #SL_WISUN_DIRECT_CONNECT_PMK_KEY_ID — PSA slot for the imported key
 *
 * Boot-time provisioning
 * sl_wisun_direct_connect_pmk_init() is registered automatically when the
 * component is added. If #SL_WISUN_DIRECT_CONNECT_PMK is non-empty, the PMK is
 * parsed and imported into #SL_WISUN_DIRECT_CONNECT_PMK_KEY_ID at boot.
 *
 * Application usage
 * This component only imports the PMK into PSA. The application is responsible
 * for passing the key ID to the Wi-SUN stack:
 * - For DC client: pass #SL_WISUN_DIRECT_CONNECT_PMK_KEY_ID to
 *   sl_wisun_connect_to_direct_connect_server().
 * - For DC server: call sl_wisun_set_direct_connect_pmk() with
 *   SL_WISUN_DIRECT_CONNECT_PMK_KEY_ID before enabling Direct Connect.
 *
 * Runtime update
 * To change the PMK after boot, call sl_wisun_direct_connect_pmk_import().
 * When wisun_cli is included, the set_dc_pmk command is available via the
 * wisun_direct_connect_pmk_cli component.
 *
 *****************************************************************************/

/***************************************************************************//**
 * @brief Import the configured Direct Connect PMK at boot.
 *
 * Registered as an internal_app_init handler by the wisun_direct_connect_pmk
 * component. Does nothing if #SL_WISUN_DIRECT_CONNECT_PMK is empty.
 ******************************************************************************/
void sl_wisun_direct_connect_pmk_init(void);

/***************************************************************************//**
 * @brief Parse and import a Direct Connect PMK into #SL_WISUN_DIRECT_CONNECT_PMK_KEY_ID.
 *
 * Replaces any existing key in the configured PSA slot.
 *
 * @param[in] hex_str PMK as colon-separated hex bytes (SL_WISUN_PMK_LEN bytes),
 *                    e.g. "34:ba:32:26:...:4e:54".
 *
 * @return SL_STATUS_OK on success.
 ******************************************************************************/
sl_status_t sl_wisun_direct_connect_pmk_import(const char *hex_str);

/** @} (end sl_wisun_direct_connect_pmk) */

#endif // SL_WISUN_DIRECT_CONNECT_PMK_H
