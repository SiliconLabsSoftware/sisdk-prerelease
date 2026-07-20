/***************************************************************************//**
 * @file
 * @brief Tokens for the WWAH Server Silabs plugin.
 *******************************************************************************
 * # License
 * <b>Copyright 2018 Silicon Laboratories Inc. www.silabs.com</b>
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

#if !defined(SL_CATALOG_TOKEN_MANAGER_PRESENT)
#include "sl_token_manager_api.h"
#include "sl_token_manager_defines.h"
#endif

#include "config/wwah-server-silabs-config.h"
#include "wwah-server-silabs.h"

/**
 * Custom Application Tokens
 */
#define CREATOR_PLUGIN_WWAH_CLIENT_ENDPOINT         (0x0010)
#define CREATOR_WWAH_MAC_POLL_FAILURE_WAIT_TIME     (0x0011)
// The following is a mask for ZLL state, Configuration Mode, and Parent
// Classification
#define CREATOR_PLUGIN_WWAH_CONFIGURATION_MASK      (0x0012)
#define CREATOR_PLUGIN_WWAH_PENDING_NETWORK_UPDATE_CHANNEL  (0x0013)
#define CREATOR_PLUGIN_WWAH_PENDING_NETWORK_UPDATE_PANID  (0x0014)
// 0x0015 taken by wwah-connection-manager-tokens.h
// 0x0016 taken by wwah-connection-manager-tokens.h
#define CREATOR_PLUGIN_WWAH_USE_TC_FOR_CLUSTER_SERVER  (0x0021)

// 0x001B taken by wwah-app-event-retry-tokens.h
// 0x001C taken by wwah-app-event-retry-tokens.h
// 0x001D taken by wwah-app-event-retry-tokens.h
// 0x001E taken by wwah-app-event-retry-tokens.h
// 0x001F taken by wwah-app-event-retry-tokens.h
// 0x0020 taken by ias-zone-server-tokens.h

#define NVM3KEY_PLUGIN_WWAH_CLIENT_ENDPOINT       (NVM3KEY_DOMAIN_USER | 0x0010)
#define NVM3KEY_WWAH_MAC_POLL_FAILURE_WAIT_TIME   (NVM3KEY_DOMAIN_USER | 0x0011)
#define COMMON_TOKEN_PLUGIN_WWAH_CLIENT_ENDPOINT         SL_TOKEN_GET_DYNAMIC_TOKEN((SL_TOKEN_NVM3_REGION_USER | 0x0010), 0)
#define COMMON_TOKEN_WWAH_MAC_POLL_FAILURE_WAIT_TIME     SL_TOKEN_GET_DYNAMIC_TOKEN((SL_TOKEN_NVM3_REGION_USER | 0x0011), 0)
// The following is a nvm3 mask for ZLL state, Configuration Mode, and Parent
// Classification
#define NVM3KEY_PLUGIN_WWAH_CONFIGURATION_MASK    (NVM3KEY_DOMAIN_USER | 0x0012)
#define NVM3KEY_PLUGIN_WWAH_PENDING_NETWORK_UPDATE_CHANNEL  (NVM3KEY_DOMAIN_USER | 0x0013)
#define NVM3KEY_PLUGIN_WWAH_PENDING_NETWORK_UPDATE_PANID  (NVM3KEY_DOMAIN_USER | 0x0014)
#define COMMON_TOKEN_PLUGIN_WWAH_CONFIGURATION_MASK               SL_TOKEN_GET_DYNAMIC_TOKEN((SL_TOKEN_NVM3_REGION_USER | 0x0012), 0)
#define COMMON_TOKEN_PLUGIN_WWAH_PENDING_NETWORK_UPDATE_CHANNEL   SL_TOKEN_GET_DYNAMIC_TOKEN((SL_TOKEN_NVM3_REGION_USER | 0x0013), 0)
#define COMMON_TOKEN_PLUGIN_WWAH_PENDING_NETWORK_UPDATE_PANID     SL_TOKEN_GET_DYNAMIC_TOKEN((SL_TOKEN_NVM3_REGION_USER | 0x0014), 0)
// (NVM3KEY_DOMAIN_USER | 0x0015) taken by wwah-connection-manager-tokens.h
// (NVM3KEY_DOMAIN_USER | 0x0016) taken by wwah-connection-manager-tokens.h
//This key is used for an indexed token and the subsequent 0x7F keys are also reserved
#define NVM3KEY_PLUGIN_WWAH_USE_TC_FOR_CLUSTER_SERVER  (NVM3KEY_DOMAIN_USER | 0x0021)
#define COMMON_TOKEN_PLUGIN_WWAH_USE_TC_FOR_CLUSTER_SERVER        SL_TOKEN_GET_DYNAMIC_TOKEN((SL_TOKEN_NVM3_REGION_USER | 0x0021), 0)

#define TOKEN_PLUGIN_WWAH_CLIENT_ENDPOINT_DEFAULT 0x00
#define TOKEN_WWAH_MAC_POLL_FAILURE_WAIT_TIME_DEFAULT MAC_POLL_FAILURE_WAIT_TIME_DEFAULT
#define TOKEN_PLUGIN_WWAH_CONFIGURATION_MASK_DEFAULT PLUGIN_WWAH_CONFIGURATION_MASK_DEFAULT
#define TOKEN_PLUGIN_WWAH_PENDING_NETWORK_UPDATE_CHANNEL_DEFAULT 0xFF
#define TOKEN_PLUGIN_WWAH_PENDING_NETWORK_UPDATE_PANID_DEFAULT 0xFFFF
#define TOKEN_PLUGIN_WWAH_USE_TC_FOR_CLUSTER_SERVER_DEFAULT { PLUGIN_WWAH_USE_TC_FOR_CLUSTER_SERVER_NULL_VALUE }

// (NVM3KEY_DOMAIN_USER | 0x001B) taken by wwah-app-event-retry-tokens.h
// (NVM3KEY_DOMAIN_USER | 0x001C) taken by wwah-app-event-retry-tokens.h
// (NVM3KEY_DOMAIN_USER | 0x001D) taken by wwah-app-event-retry-tokens.h
// (NVM3KEY_DOMAIN_USER | 0x001E) taken by wwah-app-event-retry-tokens.h
// (NVM3KEY_DOMAIN_USER | 0x001F) taken by wwah-app-event-retry-tokens.h
// (NVM3KEY_DOMAIN_USER | 0x0020) taken by ias-zone-server-tokens.h

#define MAC_POLL_FAILURE_WAIT_TIME_DEFAULT  0x03

#ifdef DEFINETOKENS
// Define the actual token storage information here
DEFINE_BASIC_TOKEN(PLUGIN_WWAH_CLIENT_ENDPOINT, uint8_t, TOKEN_PLUGIN_WWAH_CLIENT_ENDPOINT_DEFAULT)
DEFINE_BASIC_TOKEN(WWAH_MAC_POLL_FAILURE_WAIT_TIME, uint8_t, TOKEN_WWAH_MAC_POLL_FAILURE_WAIT_TIME_DEFAULT)
DEFINE_BASIC_TOKEN(PLUGIN_WWAH_CONFIGURATION_MASK,
                   uint8_t,
                   TOKEN_PLUGIN_WWAH_CONFIGURATION_MASK_DEFAULT)
DEFINE_BASIC_TOKEN(PLUGIN_WWAH_PENDING_NETWORK_UPDATE_CHANNEL,
                   uint8_t,
                   TOKEN_PLUGIN_WWAH_PENDING_NETWORK_UPDATE_CHANNEL_DEFAULT)
DEFINE_BASIC_TOKEN(PLUGIN_WWAH_PENDING_NETWORK_UPDATE_PANID,
                   uint16_t,
                   TOKEN_PLUGIN_WWAH_PENDING_NETWORK_UPDATE_PANID_DEFAULT)
DEFINE_INDEXED_TOKEN(PLUGIN_WWAH_USE_TC_FOR_CLUSTER_SERVER,
                     uint16_t,
                     MAX_CLUSTER_CAPACITY_FOR_USE_TC_FOR_CLUSTER_SERVER_TOKEN,
                     TOKEN_PLUGIN_WWAH_USE_TC_FOR_CLUSTER_SERVER_DEFAULT)
#endif  // DEFINETOKENS
