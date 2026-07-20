/***************************************************************************//**
 * @file
 * @brief APIs for the APS Link Key Authorization feature, which sets APS
 *        encryption requirement when interfacing defined clusters.
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

#ifdef SL_ZIGBEE_SCRIPTED_TEST
  #include "aps-link-key-authorization-test.h"
#else // SL_ZIGBEE_SCRIPTED_TEST
  #ifdef SL_CATALOG_ZIGBEE_SIMULATION_PRESENT
    #include "config/aps-link-key-authorization-zigbee-simulation-config.h"
  #else // SL_CATALOG_ZIGBEE_SIMULATION_PRESENT
    #include "config/aps-link-key-authorization-config.h"
  #endif // SL_CATALOG_ZIGBEE_SIMULATION_PRESENT
#endif // SL_ZIGBEE_SCRIPTED_TEST

#define CREATOR_APS_LINK_KEY_AUTH_ENABLED  (0x8726)
#define NVM3KEY_APS_LINK_KEY_AUTH_ENABLED (NVM3KEY_DOMAIN_ZIGBEE | 0x8726)
#define COMMON_TOKEN_APS_LINK_KEY_AUTH_ENABLED SL_TOKEN_GET_DYNAMIC_TOKEN((SL_TOKEN_NVM3_REGION_ZIGBEE | 0x8726), 0)

#define CREATOR_APS_LINK_KEY_AUTH_EXEMPT_LIST_CNT  (0x8727)
#define NVM3KEY_APS_LINK_KEY_AUTH_EXEMPT_LIST_CNT (NVM3KEY_DOMAIN_ZIGBEE | 0x8727)
#define COMMON_TOKEN_APS_LINK_KEY_AUTH_EXEMPT_LIST_CNT SL_TOKEN_GET_DYNAMIC_TOKEN((SL_TOKEN_NVM3_REGION_ZIGBEE | 0x8727), 0)

#define CREATOR_APS_LINK_KEY_AUTH_EXEMPT_LIST (0x8728)
// This key is used for an indexed token and the subsequent 0x7F keys are also reserved
#define NVM3KEY_APS_LINK_KEY_AUTH_EXEMPT_LIST (NVM3KEY_DOMAIN_ZIGBEE | 0x4100)
#define COMMON_TOKEN_APS_LINK_KEY_AUTH_EXEMPT_LIST SL_TOKEN_GET_DYNAMIC_TOKEN((SL_TOKEN_NVM3_REGION_ZIGBEE | 0x4100), 0)

#define TOKEN_APS_LINK_KEY_AUTH_ENABLED_DEFAULT 0x00
#define TOKEN_APS_LINK_KEY_AUTH_EXEMPT_LIST_CNT_DEFAULT 0x00
#define TOKEN_APS_LINK_KEY_AUTH_EXEMPT_LIST_DEFAULT { 0x00 }

#ifdef DEFINETOKENS
// Define the actual token storage information here
DEFINE_BASIC_TOKEN(APS_LINK_KEY_AUTH_ENABLED, uint8_t, TOKEN_APS_LINK_KEY_AUTH_ENABLED_DEFAULT)
DEFINE_BASIC_TOKEN(APS_LINK_KEY_AUTH_EXEMPT_LIST_CNT, uint8_t, TOKEN_APS_LINK_KEY_AUTH_EXEMPT_LIST_CNT_DEFAULT)
DEFINE_INDEXED_TOKEN(APS_LINK_KEY_AUTH_EXEMPT_LIST,
                     uint16_t,
                     SL_ZIGBEE_AF_PLUGIN_APS_LINK_KEY_AUTHORIZATION_MAX_EXEMPT_CLUSTERS,
                     TOKEN_APS_LINK_KEY_AUTH_EXEMPT_LIST_DEFAULT)
#endif //DEFINETOKENS
