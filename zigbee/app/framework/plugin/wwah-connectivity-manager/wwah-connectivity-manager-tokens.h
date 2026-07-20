/***************************************************************************//**
 * @file
 * @brief Tokens for the WWAH Connectivity Manager plugin.
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

#include "wwah-connectivity-manager.h"
#include "wwah-connectivity-manager-config.h"
/**
 * Custom Application Tokens
 */

#define CREATOR_PLUGIN_CONNECTION_MANAGER_STATE                       (0x0015)
#define NVM3KEY_PLUGIN_CONNECTION_MANAGER_STATE                       (NVM3KEY_DOMAIN_USER | 0x0015)
#define COMMON_TOKEN_PLUGIN_CONNECTION_MANAGER_STATE                  SL_TOKEN_GET_DYNAMIC_TOKEN((SL_TOKEN_NVM3_REGION_USER | 0x0015), 0)

#define CREATOR_PLUGIN_WWAH_CONNECTION_MANAGER_PERIODIC_ROUTER_CHECKIN_INTERVAL  (0x0016)
#define NVM3KEY_PLUGIN_WWAH_CONNECTION_MANAGER_PERIODIC_ROUTER_CHECKIN_INTERVAL  (NVM3KEY_DOMAIN_USER | 0x0016)
#define COMMON_TOKEN_PLUGIN_WWAH_CONNECTION_MANAGER_PERIODIC_ROUTER_CHECKIN_INTERVAL    SL_TOKEN_GET_DYNAMIC_TOKEN((SL_TOKEN_NVM3_REGION_USER | 0x0016), 0)

#define TOKEN_PLUGIN_CONNECTION_MANAGER_STATE_DEFAULT {                               \
    0x00000000,                                                                       \
    SL_ZIGBEE_AF_PLUGIN_WWAH_CONNECTIVITY_MANAGER_FAST_REJOIN_TIMEOUT_SEC,            \
    SL_ZIGBEE_AF_PLUGIN_WWAH_CONNECTIVITY_MANAGER_DURATION_BETWEEN_EACH_REJOIN_SEC,   \
    SL_ZIGBEE_AF_PLUGIN_WWAH_CONNECTIVITY_MANAGER_FAST_REJOIN_FIRST_BACKOFF_TIME_SEC, \
    SL_ZIGBEE_AF_PLUGIN_WWAH_CONNECTIVITY_MANAGER_MAX_BACKOFF_TIME_SEC,               \
    SL_ZIGBEE_AF_PLUGIN_WWAH_CONNECTIVITY_MANAGER_MAX_BACKOFF_ITERATIONS              \
}
#define TOKEN_PLUGIN_WWAH_CONNECTION_MANAGER_PERIODIC_ROUTER_CHECKIN_INTERVAL_DEFAULT 0xFFFF

#ifdef DEFINETYPES
// Include or define any typedef for tokens here
typedef struct {
  // bit 0 = connection manager enabled state
  // bit 1 = connection manager bad parent recovery
  // bit 2 = connection manager trust center connectivity
  // bit 3 = wwah periodic router checkins enabled state
  // Remaining bits are reserved
  uint32_t connectionManagerFlags;

  uint16_t fastRejoinTimeoutSec;
  uint16_t durationBetweenEachRejoinSec;
  uint16_t fastRejoinFirstBackoffTimeSec;
  uint16_t maxBackoffTimeSec;
  uint16_t maxBackoffIter;
} sl_zigbee_af_connection_manager_token_struct_t;

#endif  // DEFINETYPES

#define PLUGIN_WWAH_CONNECTION_MANAGER_ENABLED_STATE                          (0x00000001)
#define PLUGIN_WWAH_CONNECTION_MANAGER_BAD_PARENT_RECOVERY                    (0x00000002)
#define PLUGIN_WWAH_CONNECTION_MANAGER_TRUST_CENTER_CONNECTIVITY              (0x00000004)
#define PLUGIN_WWAH_CONNECTION_MANAGER_PERIODIC_ROUTER_CHECKIN_ENABLED_STATE  (0x00000008)

#ifdef DEFINETOKENS
// Define the actual token storage information here
DEFINE_BASIC_TOKEN(PLUGIN_CONNECTION_MANAGER_STATE,
                   sl_zigbee_af_connection_manager_token_struct_t,
                   TOKEN_PLUGIN_CONNECTION_MANAGER_STATE_DEFAULT)
DEFINE_BASIC_TOKEN(PLUGIN_WWAH_CONNECTION_MANAGER_PERIODIC_ROUTER_CHECKIN_INTERVAL,
                   uint16_t,
                   TOKEN_PLUGIN_WWAH_CONNECTION_MANAGER_PERIODIC_ROUTER_CHECKIN_INTERVAL_DEFAULT)
#endif  // DEFINETOKENS
