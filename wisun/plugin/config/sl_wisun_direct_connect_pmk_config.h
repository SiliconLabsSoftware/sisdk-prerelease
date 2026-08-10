/***************************************************************************//**
 * @file sl_wisun_direct_connect_pmk_config.h
 * @brief Wi-SUN Direct Connect PMK configuration
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

#ifndef SL_WISUN_DIRECT_CONNECT_PMK_CONFIG_H
#define SL_WISUN_DIRECT_CONNECT_PMK_CONFIG_H

// <<< Use Configuration Wizard in Context Menu >>>

// <h> Wi-SUN Direct Connect PMK

// <s SL_WISUN_DIRECT_CONNECT_PMK> Direct Connect PMK
// <i> Colon-separated hex bytes (32 bytes), e.g. 34:ba:32:26:a0:b2:ad:66:...
// <i> Must match the PMK on the peer device. Leave empty to skip boot-time import.
#define SL_WISUN_DIRECT_CONNECT_PMK "34:ba:32:26:a0:b2:ad:66:7c:9f:66:02:e5:db:75:77:dd:bd:5d:2b:34:3a:93:06:2b:90:c0:7b:e2:8e:4e:54"

// <o SL_WISUN_DIRECT_CONNECT_PMK_KEY_ID> PSA key ID <f.h>
// <i> PSA slot for the imported PMK. Used by Direct Connect client and server APIs.
// <d> 0x00080001
#define SL_WISUN_DIRECT_CONNECT_PMK_KEY_ID 0x00080001

// </h>

// <<< end of configuration section >>>

#endif // SL_WISUN_DIRECT_CONNECT_PMK_CONFIG_H
