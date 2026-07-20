/***************************************************************************//**
 * @file
 * @brief Routines for the WWAH Server Silabs plugin, which implement the server
 *        side of the WWAH Silabs cluster.
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

#include "app/framework/include/af.h"
#include "app/framework/util/af-main.h"
#include "aps-link-key-authorization.h"
#include "wwah-server-silabs.h"
#include "../update-tc-link-key/update-tc-link-key.h"
#include "app/framework/plugin/ota-client/ota-client.h"
// TODO - EMZIGBEE-7188: hard dependency to be reviewed.
#include "wwah-connectivity-manager.h"
#include "zap-cluster-command-parser.h"
#include "sl_zigbee_token.h"
#include "stack/config/sl_zigbee_token_defines.h"
#include "wwah-server-silabs-tokens.h"

#ifdef SL_ZIGBEE_SCRIPTED_TEST
#include "app/zaptest/unit-test/zap-command-structs.h"
#else // SL_ZIGBEE_SCRIPTED_TEST
#include "wwah-server-silabs-config.h"
#endif // SL_ZIGBEE_SCRIPTED_TEST

#if (defined(SL_CATALOG_ZIGBEE_DEBUG_PRINT_PRESENT) && !defined(SL_ZIGBEE_TEST) && !defined(EZSP_HOST) && !defined(PRO_COMPLIANCE))
#define WWAH_DEBUG_FEATURES_ENABLED
#define EXTENDED_RESET_INFO
#include "cortexm3/diagnostic.h"
#include "sl_zigbee_debug_print.h"
#endif // WWAH_DEBUG_FEATURES_ENABLED

#define AMAZON_MANUFACTURING_ID 0x1217

#ifdef SL_CATALOG_ZIGBEE_WWAH_CONNECTIVITY_MANAGER_PRESENT
#define WWAH_CONNECTIVITY_MANAGER_PRESENT
#endif
#ifdef SL_CATALOG_ZIGBEE_WWAH_APP_EVENT_RETRY_MANAGER_PRESENT
#define WWAH_APP_EVENT_RETRY_MANAGER_PRESENT
#endif
#ifdef SL_CATALOG_ZIGBEE_DIDO_PRESENT
#define DIDO_PRESENT
#include "dido.h"
#endif
#ifdef SL_CATALOG_ZIGBEE_SIMPLE_CLOCK_PRESENT
#define SIMPLE_CLOCK_PRESENT
#include "simple-clock.h"
#endif
#ifdef SL_CATALOG_ZIGBEE_COMPACT_LOGGER_PRESENT
#define COMPACT_LOGGER_PRESENT
#include "compact-logger.h"
#endif
#ifdef SL_CATALOG_ZIGBEE_IAS_ZONE_SERVER_PRESENT
#define IAS_ZONE_SERVER_PRESENT
#include "ias-zone-server.h"
#endif
#ifdef SL_CATALOG_ZIGBEE_POLL_CONTROL_SERVER_PRESENT
#define POLL_CONTROL_SERVER_PRESENT
#include "poll-control-server.h"
#endif
#ifdef SL_CATALOG_ZIGBEE_TIME_SERVER_PRESENT
#define TIME_SERVER_PRESENT
#include "time-server.h"
#endif
#ifdef SL_CATALOG_ZIGBEE_TRUST_CENTER_KEEPALIVE_PRESENT
#define TRUST_CENTER_KEEPALIVE_PRESENT
#include "trust-center-keepalive.h"
#endif

#ifdef SL_ZIGBEE_SCRIPTED_TEST
#include "app/framework/plugin/wwah-server-silabs/wwah-server-silabs-test.h"
#endif

#ifndef POLL_CONTROL_SERVER_PRESENT
#define sl_zigbee_af_poll_control_server_set_ignore_non_trust_center(ignoreNonTc) (void)0
#endif

#include "stack/include/zigbee-device-stack.h"

#ifndef EZSP_HOST
#include "stack/include/network-formation.h"
#endif

static void (*poweringOffCb)(sl_zigbee_power_down_notification_result_t) = NULL;
static sl_zigbee_af_wwah_power_notification_reason_t resetReason = SL_ZIGBEE_ZCL_WWAH_POWER_NOTIFICATION_REASON_UNKNOWN;
#define INVALID_WWAH_ENDPOINT 0
static uint8_t wwahServerEndpoint = INVALID_WWAH_ENDPOINT;
static uint8_t wwahClientEndpoint = INVALID_WWAH_ENDPOINT;
static uint8_t wwahConfigurationBitmask;

static void initializeWwahClientEndpoint(uint8_t endpoint);
static bool haveWwahClientEndpoint(void);
static sl_status_t discoverWwahClientEndpoint(void);
static void setWwahClientEndpoint(uint8_t endpoint);
static void wwahClientDiscoveryCallback(const sl_zigbee_af_service_discovery_result_t *result);
static void poweringOffNotificationComplete(sl_zigbee_power_down_notification_result_t result);
static sl_zigbee_af_status_t setSlWwahConfigurationMode(bool enable);
static inline sl_zigbee_af_status_t setParentClassificationEnabled(bool enabled);

#ifdef SL_ZIGBEE_TEST
sl_zigbee_beacon_survey_t surveyBeaconDataCache[SL_ZIGBEE_AF_PLUGIN_WWAH_SERVER_SILABS_SURVEY_BEACON_RESULT_LIMIT];
uint8_t surveyBeaconDataCount;
#endif // SL_ZIGBEE_Test

static sl_zigbee_af_event_t triggerSurveyBeaconsNetworkEvents[SL_ZIGBEE_SUPPORTED_NETWORKS];
static void triggerSurveyBeaconsNetworkEventHandler(sl_zigbee_af_event_t * event);
enum {
  WWAH_CLIENT_TICK_STATE_DONE,
  WWAH_CLIENT_TICK_STATE_WAIT_NETWORK_UP,
  WWAH_CLIENT_TICK_STATE_ENDPOINT_DISCOVERY,
  WWAH_CLIENT_TICK_STATE_SEND_POWERING_ON,
  WWAH_CLIENT_TICK_STATE_POWERING_OFF_TIMEOUT
};

static uint8_t clientTickState = WWAH_CLIENT_TICK_STATE_DONE;
#define MAX_DISCOVERY_DELAY 5000
#define MAX_DISCOVERY_RETRIES 3

// Response payload contain 10 clusters and respective statuses.
#define MAX_CLUSTER_CAPACITY_FOR_USE_TC_FOR_CLUSTER_SERVER_RESPONSE_COMMAND 10
static bool isClusterInUseTcForClusterServerToken(uint16_t clusterId);
static void syncClusterWithUseTcForClusterServerToken(void);

static sl_zigbee_aps_frame_t apsFrameCache;
static sl_802154_short_addr_t responseDestinationCache;
static uint8_t responseSeqNumCache;

static bool ignoreNonTrustCenterForTimeCluster = false;

sl_zigbee_af_status_t sl_zigbee_read_wwah_server_silabs_attribute(uint8_t endpoint,
                                                                  sl_zigbee_af_attribute_id_t attributeId,
                                                                  const char *name,
                                                                  uint8_t *data,
                                                                  uint8_t size)
{
  sl_zigbee_af_status_t afStatus = sl_zigbee_af_read_manufacturer_specific_server_attribute(endpoint,
                                                                                            ZCL_SL_WWAH_CLUSTER_ID,
                                                                                            attributeId,
                                                                                            AMAZON_MANUFACTURING_ID,
                                                                                            data,
                                                                                            size);
  if (afStatus != SL_ZIGBEE_ZCL_STATUS_SUCCESS) {
    sl_zigbee_af_sl_wwah_cluster_println("ERR: %sing %s 0x%02X", "read", name, afStatus);
  }
  return afStatus;
}

sl_zigbee_af_status_t sl_zigbee_write_wwah_server_silabs_attribute(uint8_t endpoint,
                                                                   sl_zigbee_af_attribute_id_t attributeId,
                                                                   const char *name,
                                                                   uint8_t *data,
                                                                   sl_zigbee_af_attribute_type_t type)
{
  sl_zigbee_af_status_t afStatus = sl_zigbee_af_write_manufacturer_specific_server_attribute(endpoint,
                                                                                             ZCL_SL_WWAH_CLUSTER_ID,
                                                                                             attributeId,
                                                                                             AMAZON_MANUFACTURING_ID,
                                                                                             data,
                                                                                             type);
  if (afStatus != SL_ZIGBEE_ZCL_STATUS_SUCCESS) {
    sl_zigbee_af_sl_wwah_cluster_println("ERR: %sing %s 0x%02X", "writ", name, afStatus);
  }
  return afStatus;
}

void sl_zigbee_af_sl_wwah_cluster_server_tick_cb(uint8_t endpoint)
{
  static uint8_t discoRetries;    // Used to throttle discovery retries.
  uint16_t delayMs = 0;

  switch (clientTickState) {
    case WWAH_CLIENT_TICK_STATE_WAIT_NETWORK_UP:
      // Wait for the network to come up before proceeding.
      if (sl_zigbee_af_network_state() == SL_ZIGBEE_JOINED_NETWORK) {
        if (haveWwahClientEndpoint()) {
          // Send the power on notification command.
          clientTickState = WWAH_CLIENT_TICK_STATE_SEND_POWERING_ON;
        } else {
          // Start client endpoint discovery.
          clientTickState = WWAH_CLIENT_TICK_STATE_ENDPOINT_DISCOVERY;
        }
        // Set a random delay before continuing to the next state.
        delayMs = sl_zigbee_get_pseudo_random_number() % MAX_DISCOVERY_DELAY;
        sl_zigbee_zcl_schedule_server_tick(endpoint, ZCL_SL_WWAH_CLUSTER_ID, delayMs);
      }
      break;

    case WWAH_CLIENT_TICK_STATE_ENDPOINT_DISCOVERY:
      if (!haveWwahClientEndpoint() && (discoRetries < MAX_DISCOVERY_RETRIES)) {
        discoverWwahClientEndpoint();
        delayMs = sl_zigbee_get_pseudo_random_number() % MAX_DISCOVERY_DELAY;
        sl_zigbee_zcl_schedule_server_tick(endpoint, ZCL_SL_WWAH_CLUSTER_ID, delayMs);
        discoRetries++;
      } else {
        discoRetries = 0;   // Reset this for next time.
        if (haveWwahClientEndpoint()) {
          // Proceed to next state to send power on notification.
          clientTickState = WWAH_CLIENT_TICK_STATE_SEND_POWERING_ON;
          sl_zigbee_zcl_schedule_server_tick(endpoint, ZCL_SL_WWAH_CLUSTER_ID, 500);
        } else {
          // Give up, wwah client endpoint not found.
          clientTickState = WWAH_CLIENT_TICK_STATE_DONE;
        }
      }
      break;

    case WWAH_CLIENT_TICK_STATE_SEND_POWERING_ON:
      // Send the Powering On notification to the coordinator.
      // Send this in the Tick callback to randomize the transmission
      // after the stack comes up.
      sl_zigbee_af_wwah_server_send_powering_on_notification(SL_ZIGBEE_ZIGBEE_COORDINATOR_ADDRESS,
                                                             endpoint,
                                                             wwahClientEndpoint,
                                                             resetReason,
                                                             AMAZON_MANUFACTURING_ID,
                                                             NULL,
                                                             0);
      clientTickState = WWAH_CLIENT_TICK_STATE_DONE;
      break;

    case WWAH_CLIENT_TICK_STATE_POWERING_OFF_TIMEOUT:
      if (poweringOffCb != NULL) {
        sl_zigbee_af_sl_wwah_cluster_println("Powering off notification timeout");
        poweringOffNotificationComplete(SL_ZIGBEE_POWER_DOWN_NOTIFICATION_TIMEOUT);
      }
      clientTickState = WWAH_CLIENT_TICK_STATE_DONE;
      break;
    default:
      break;
  }
}

static void poweringOffNotificationComplete(sl_zigbee_power_down_notification_result_t result)
{
  if (poweringOffCb != NULL) {
    poweringOffCb(result);
    poweringOffCb = NULL;
  }
}

void sl_zigbee_af_sl_wwah_cluster_server_default_response_cb(uint8_t endpoint,
                                                             uint8_t commandId,
                                                             sl_zigbee_af_status_t status)
{
  UNUSED_VAR(endpoint);

  sl_zigbee_power_down_notification_result_t result;
  if (commandId == ZCL_POWERING_OFF_NOTIFICATION_COMMAND_ID) {
    if (status == SL_ZIGBEE_ZCL_STATUS_SUCCESS) {
      sl_zigbee_af_debug_println("Powering off default response: 0x%02X", status);
      result = SL_ZIGBEE_POWER_DOWN_NOTIFICATION_SUCCESS;
    } else {
      sl_zigbee_af_debug_println("Powering off default response error: 0x%02X", status);
      result = SL_ZIGBEE_POWER_DOWN_NOTIFICATION_FAILURE;
    }
    poweringOffNotificationComplete(result);
  }
}

sl_status_t sl_zigbee_af_sl_wwah_cluster_server_token_init(void)
{
  sl_status_t status = SL_STATUS_OK;
  uint8_t tokPluginWWAHClientEndpointDefault = TOKEN_PLUGIN_WWAH_CLIENT_ENDPOINT_DEFAULT;
  status = sl_zigbee_initialize_basic_token(COMMON_TOKEN_PLUGIN_WWAH_CLIENT_ENDPOINT, &tokPluginWWAHClientEndpointDefault, sizeof(uint8_t));
  uint8_t tokWWAHMACPollFailureWaitTimeDefault = TOKEN_WWAH_MAC_POLL_FAILURE_WAIT_TIME_DEFAULT;
  status = sl_zigbee_initialize_basic_token(COMMON_TOKEN_WWAH_MAC_POLL_FAILURE_WAIT_TIME, &tokWWAHMACPollFailureWaitTimeDefault, sizeof(uint8_t));
  uint8_t tokPluginWWAHConfigurationMaskDefault = TOKEN_PLUGIN_WWAH_CONFIGURATION_MASK_DEFAULT;
  status = sl_zigbee_initialize_basic_token(COMMON_TOKEN_PLUGIN_WWAH_CONFIGURATION_MASK, &tokPluginWWAHConfigurationMaskDefault, sizeof(uint8_t));
  uint8_t tokPluginWWAHPendingNetworkUpdateChannelDefault = TOKEN_PLUGIN_WWAH_PENDING_NETWORK_UPDATE_CHANNEL_DEFAULT;
  status = sl_zigbee_initialize_basic_token(COMMON_TOKEN_PLUGIN_WWAH_PENDING_NETWORK_UPDATE_CHANNEL, &tokPluginWWAHPendingNetworkUpdateChannelDefault, sizeof(uint8_t));
  uint16_t tokPluginWWAHPendingNetworkUpdatePanIDDefault = TOKEN_PLUGIN_WWAH_PENDING_NETWORK_UPDATE_PANID_DEFAULT;
  status = sl_zigbee_initialize_basic_token(COMMON_TOKEN_PLUGIN_WWAH_PENDING_NETWORK_UPDATE_PANID, &tokPluginWWAHPendingNetworkUpdatePanIDDefault, sizeof(uint16_t));
  uint16_t tokPluginWWAHUseTCForClusterServerDefault = TOKEN_PLUGIN_WWAH_USE_TC_FOR_CLUSTER_SERVER_DEFAULT;
  status = sl_zigbee_initialize_index_token(COMMON_TOKEN_PLUGIN_WWAH_USE_TC_FOR_CLUSTER_SERVER, &tokPluginWWAHUseTCForClusterServerDefault, sizeof(uint16_t), MAX_CLUSTER_CAPACITY_FOR_USE_TC_FOR_CLUSTER_SERVER_TOKEN);
  return status;
}

void sl_zigbee_af_sl_wwah_cluster_server_init_cb(uint8_t endpoint)
{
  assert(SL_STATUS_OK == sl_zigbee_af_sl_wwah_cluster_server_token_init());
  sl_zigbee_af_network_event_init(triggerSurveyBeaconsNetworkEvents,
                                  triggerSurveyBeaconsNetworkEventHandler);
  uint8_t macPollFailureWaitTimeSec;
  // Get the reset reason and store it.
  // This will be sent after the stack comes up.
  #ifdef WWAH_DEBUG_FEATURES_ENABLED
  uint16_t resetInfo = halGetExtendedResetInfo();
  switch (RESET_BASE_TYPE(resetInfo)) {
    case RESET_FIB:
      if (resetInfo == RESET_FIB_GO) {
        resetReason = SL_ZIGBEE_ZCL_WWAH_POWER_NOTIFICATION_REASON_OTA_BOOTLOAD_SUCCESS;
      } else {
        resetReason = SL_ZIGBEE_ZCL_WWAH_POWER_NOTIFICATION_REASON_BOOTLOAD_FAILURE;
      }
      break;
    case RESET_BOOTLOADER:
      if (resetInfo == RESET_BOOTLOADER_GO) {
        resetReason = SL_ZIGBEE_ZCL_WWAH_POWER_NOTIFICATION_REASON_OTA_BOOTLOAD_SUCCESS;
      } else {
        resetReason = SL_ZIGBEE_ZCL_WWAH_POWER_NOTIFICATION_REASON_BOOTLOAD_FAILURE;
      }
      break;
    #if defined(CORTEXM3_EFM32_MICRO)
    case RESET_BROWNOUT:
      resetReason = SL_ZIGBEE_ZCL_WWAH_POWER_NOTIFICATION_REASON_BROWNOUT;
      break;
    case RESET_POWERON:
      resetReason = SL_ZIGBEE_ZCL_WWAH_POWER_NOTIFICATION_REASON_POWER_BUTTON;
      break;
    #endif //defined(CORTEXM3_EFM32_MICRO)
    case RESET_EXTERNAL:
      resetReason = SL_ZIGBEE_ZCL_WWAH_POWER_NOTIFICATION_REASON_RESET_PIN;
      break;
    case RESET_SOFTWARE:
      resetReason = SL_ZIGBEE_ZCL_WWAH_POWER_NOTIFICATION_REASON_SOFTWARE_RESET;
      break;
    case RESET_WATCHDOG:
      resetReason = SL_ZIGBEE_ZCL_WWAH_POWER_NOTIFICATION_REASON_WATCHDOG;
      break;
    case RESET_CRASH:
    case RESET_FATAL:
      resetReason = SL_ZIGBEE_ZCL_WWAH_POWER_NOTIFICATION_REASON_SOFWARE_EXCEPTION;
      break;
    case RESET_FAULT:
    case RESET_FLASH:
      resetReason = SL_ZIGBEE_ZCL_WWAH_POWER_NOTIFICATION_REASON_MEMORY_HARDWARE_FAULT;
      break;
    default:
      resetReason = SL_ZIGBEE_ZCL_WWAH_POWER_NOTIFICATION_REASON_UNKNOWN;
      break;
  }
  #endif // WWAH_DEBUG_FEATURES_ENABLED

  // Load the WWAH configuration state bitmask
  sl_status_t status = slx_zigbee_token_manager_get_data(COMMON_TOKEN_PLUGIN_WWAH_CONFIGURATION_MASK, (void *)&wwahConfigurationBitmask, sizeof(uint8_t));
  if (status != SL_STATUS_OK) {
    sl_zigbee_af_sl_wwah_cluster_println("Failed to get WWAH Configuration Mask, status: 0x%08X", status);
    return;
  }

  // Set ZLL Interpan attribute
  uint8_t touchlinkEnabled = !(wwahConfigurationBitmask
                               & PLUGIN_WWAH_CONFIGURATION_MASK_ZLL_POLICY_DISABLED);
  sl_zigbee_write_wwah_server_silabs_attribute(endpoint,
                                               ZCL_SL_TOUCHLINK_INTERPAN_ENABLED_ATTRIBUTE_ID,
                                               "zll policy",
                                               &touchlinkEnabled,
                                               ZCL_BOOLEAN_ATTRIBUTE_TYPE);

  // Load mac poll failure wait time token
  status = slx_zigbee_token_manager_get_data(COMMON_TOKEN_WWAH_MAC_POLL_FAILURE_WAIT_TIME, (void *)&macPollFailureWaitTimeSec, sizeof(uint8_t));
  if (status != SL_STATUS_OK) {
    sl_zigbee_af_sl_wwah_cluster_println("Failed to get WWAH Mac Poll Failure Wait Time, status: 0x%08X", status);
    return;
  }

  sl_zigbee_write_wwah_server_silabs_attribute(endpoint,
                                               ZCL_SL_MAC_POLL_FAILURE_WAIT_TIME_ATTRIBUTE_ID,
                                               "mac poll failure wait time",
                                               &macPollFailureWaitTimeSec,
                                               ZCL_INT8U_ATTRIBUTE_TYPE);

  initializeWwahClientEndpoint(endpoint);
  wwahServerEndpoint = endpoint;

  // Synchronize leave without rejoin attribute with security bitmask
  sl_zigbee_extended_security_bitmask_t mask;
  sl_zigbee_get_extended_security_bitmask(&mask);
  uint8_t data = (mask & SL_ZIGBEE_NWK_LEAVE_WITHOUT_REJOIN_NOT_ALLOWED) ? 0 : 1;
  sl_zigbee_write_wwah_server_silabs_attribute(wwahServerEndpoint,
                                               ZCL_SL_MGMT_LEAVE_WITHOUT_REJOIN_ENABLED_ATTRIBUTE_ID,
                                               "mgmt leave without rejoin enabled",
                                               &data,
                                               ZCL_BOOLEAN_ATTRIBUTE_TYPE);

  // Synchronize TCSecurityOnNtwkKeyRotationEnabled attribute with security bitmask
  data = (mask & SL_ZIGBEE_SECURE_NETWORK_KEY_ROTATION) ? 1 : 0;
  sl_zigbee_write_wwah_server_silabs_attribute(wwahServerEndpoint,
                                               ZCL_SL_TC_SECURITY_ON_NTWK_KEY_ROTATION_ENABLED_ATTRIBUTE_ID,
                                               "tc security on ntwk key rotation enabled",
                                               &data,
                                               ZCL_BOOLEAN_ATTRIBUTE_TYPE);

  // Write MACRetryCount and NWKRetryCount
  data = sl_zigbee_get_max_mac_retries();
  sl_zigbee_write_wwah_server_silabs_attribute(wwahServerEndpoint,
                                               ZCL_SL_MAC_RETRY_COUNT_ATTRIBUTE_ID,
                                               "mac retry count",
                                               &data,
                                               ZCL_INT8U_ATTRIBUTE_TYPE);
  data = sl_zigbee_get_max_nwk_retries();
  sl_zigbee_write_wwah_server_silabs_attribute(wwahServerEndpoint,
                                               ZCL_SL_NWK_RETRY_COUNT_ATTRIBUTE_ID,
                                               "network retry count",
                                               &data,
                                               ZCL_INT8U_ATTRIBUTE_TYPE);

  // Set Configuration Mode attribute, update stack
  uint8_t confModeEnabled = wwahConfigurationBitmask
                            & PLUGIN_WWAH_CONFIGURATION_MASK_CONFIGURATION_MODE_ENABLED;
  confModeEnabled = confModeEnabled ? 1 : 0;
  sl_zigbee_write_wwah_server_silabs_attribute(endpoint,
                                               ZCL_SL_CONFIGURATION_MODE_ENABLED_ATTRIBUTE_ID,
                                               "configuration mode enabled",
                                               &confModeEnabled,
                                               ZCL_BOOLEAN_ATTRIBUTE_TYPE);
#ifndef EZSP_HOST
  sl_zigbee_set_zdo_configuration_mode(confModeEnabled != 0);
#endif

  // Set Parent Classification attribute, update stack
  uint8_t parentClassificationEnabled = wwahConfigurationBitmask
                                        & PLUGIN_WWAH_CONFIGURATION_MASK_PARENT_CLASSIFICATION_ENABLED;
  parentClassificationEnabled = parentClassificationEnabled ? 1 : 0;
  sl_zigbee_write_wwah_server_silabs_attribute(endpoint,
                                               ZCL_SL_WWAH_PARENT_CLASSIFICATION_ENABLED_ATTRIBUTE_ID,
                                               "parent classification enabled",
                                               &parentClassificationEnabled,
                                               ZCL_BOOLEAN_ATTRIBUTE_TYPE);
  sl_zigbee_beacon_classification_params_t param;
  sl_zigbee_get_beacon_classification_params(&param);
  if (parentClassificationEnabled) {
    param.beaconClassificationMask |=
      PRIORITIZE_BEACONS_BASED_ON_PARENT_CLASSIFICATION;
  } else {
    param.beaconClassificationMask &=
      ~PRIORITIZE_BEACONS_BASED_ON_PARENT_CLASSIFICATION;
  }
  sl_zigbee_set_beacon_classification_params(&param);

  // Set pending network update channel and panId attributes
  uint8_t channel;
  status = slx_zigbee_token_manager_get_data(COMMON_TOKEN_PLUGIN_WWAH_PENDING_NETWORK_UPDATE_CHANNEL, (void *)&channel, sizeof(uint8_t));
  if (status != SL_STATUS_OK) {
    sl_zigbee_af_sl_wwah_cluster_println("Failed to get WWAH Pending Network Update Channel, status: 0x%08X", status);
    return;
  }

  sl_zigbee_write_wwah_server_silabs_attribute(endpoint,
                                               ZCL_SL_PENDING_NETWORK_UPDATE_CHANNEL_ATTRIBUTE_ID,
                                               "pending network update channel",
                                               &channel,
                                               ZCL_INT8U_ATTRIBUTE_TYPE);

  uint16_t panId;
  status = slx_zigbee_token_manager_get_data(COMMON_TOKEN_PLUGIN_WWAH_PENDING_NETWORK_UPDATE_PANID, (void *)&panId, sizeof(uint16_t));
  if (status != SL_STATUS_OK) {
    sl_zigbee_af_sl_wwah_cluster_println("Failed to get WWAH Pending Network Update PAN ID, status: 0x%08X", status);
    return;
  }
  sl_zigbee_write_wwah_server_silabs_attribute(endpoint,
                                               ZCL_SL_PENDING_NETWORK_UPDATE_PANID_ATTRIBUTE_ID,
                                               "pending network update panId",
                                               (uint8_t*)&panId,
                                               ZCL_INT16U_ATTRIBUTE_TYPE);

  // Sync the ignoreNonTrustCenter setting of all supported clusters
  // with the UseTcForClusterServerToken
  syncClusterWithUseTcForClusterServerToken();

  // Update disable ota downgrades configuration and attribute from token
  uint8_t disableOtaDowngrades = (wwahConfigurationBitmask
                                  & PLUGIN_WWAH_CONFIGURATION_MASK_DISABLE_OTA_DOWNGRADES) ? 1 : 0;
  if (disableOtaDowngrades) {
    sl_zigbee_write_wwah_server_silabs_attribute(endpoint,
                                                 ZCL_SL_DISABLE_OTA_DOWNGRADES_ATTRIBUTE_ID,
                                                 "disable OTA downgrades",
                                                 &disableOtaDowngrades,
                                                 ZCL_BOOLEAN_ATTRIBUTE_TYPE);
    sl_zigbee_af_set_disable_ota_downgrades(disableOtaDowngrades);
  }

#if defined(WWAH_APP_EVENT_RETRY_MANAGER_PRESENT)
  sl_zigbee_af_wwah_app_event_retry_manager_init_cb(endpoint);
#endif
#if defined(WWAH_CONNECTIVITY_MANAGER_PRESENT)
  sl_zigbee_af_wwah_connectivity_manager_init_cb(endpoint);
#endif
}

#define POWERING_ON_NOTIFICATION_MAX_DELAY_MS  5000
void sl_zigbee_af_wwah_server_silabs_stack_status_cb(sl_status_t status)
{
  if (status == SL_STATUS_NETWORK_DOWN) {
    // Clear out the WWAH endpoint in case the node is leaving.
    setWwahClientEndpoint(INVALID_WWAH_ENDPOINT);
  } else if (status == SL_STATUS_NETWORK_UP && clientTickState != WWAH_CLIENT_TICK_STATE_DONE) {
    sl_zigbee_zcl_schedule_server_tick(wwahServerEndpoint, ZCL_SL_WWAH_CLUSTER_ID, 500);
  }
}

// -----------------------------------------------------------------------------
// WWAH Client Endpoint Discovery

static void initializeWwahClientEndpoint(uint8_t endpoint)
{
  // Read from Token
  sl_status_t status = slx_zigbee_token_manager_get_data(COMMON_TOKEN_PLUGIN_WWAH_CLIENT_ENDPOINT, (void *)&wwahClientEndpoint, sizeof(uint8_t));
  if (status != SL_STATUS_OK) {
    sl_zigbee_af_sl_wwah_cluster_println("Failed to get WWAH Client Endpoint, status: 0x%08X", status);
    return;
  }

  sl_zigbee_af_debug_println("Read client endpoint 0x%02X", wwahClientEndpoint);

  // Wait for the network to come up.
  clientTickState = WWAH_CLIENT_TICK_STATE_WAIT_NETWORK_UP;
  sl_zigbee_zcl_schedule_server_tick(endpoint, ZCL_SL_WWAH_CLUSTER_ID, 500);
}

static bool haveWwahClientEndpoint()
{
  return wwahClientEndpoint != INVALID_WWAH_ENDPOINT;
}

static void setWwahClientEndpoint(uint8_t endpoint)
{
  if (wwahClientEndpoint != endpoint) {
    wwahClientEndpoint = endpoint;
    sl_status_t status = slx_zigbee_token_manager_set_data(COMMON_TOKEN_PLUGIN_WWAH_CLIENT_ENDPOINT, (void *)&wwahClientEndpoint, sizeof(uint8_t));
    if (status != SL_STATUS_OK) {
      sl_zigbee_af_sl_wwah_cluster_println("Failed to set WWAH Client Endpoint, status: 0x%08X", status);
      return;
    }
    sl_zigbee_af_debug_println("Set client endpoint 0x%02X", wwahClientEndpoint);
  }
}

static sl_status_t discoverWwahClientEndpoint()
{
  sl_status_t status;
  status = sl_zigbee_af_find_devices_by_profile_and_cluster(SL_ZIGBEE_ZIGBEE_COORDINATOR_ADDRESS,
                                                            HA_PROFILE_ID,
                                                            ZCL_SL_WWAH_CLUSTER_ID,
                                                            SL_ZIGBEE_AF_CLIENT_CLUSTER_DISCOVERY,
                                                            wwahClientDiscoveryCallback);
  if (status != SL_STATUS_OK) {
    sl_zigbee_af_sl_wwah_cluster_println("Failed to send endpoint discovery: 0x%02X", status);
  }
  return status;
}

static void wwahClientDiscoveryCallback(const sl_zigbee_af_service_discovery_result_t *result)
{
  const sl_zigbee_af_endpoint_list_t *epList;
  if (result->status == SL_ZIGBEE_AF_UNICAST_SERVICE_DISCOVERY_COMPLETE_WITH_RESPONSE) {
    if (result->zdoRequestClusterId == MATCH_DESCRIPTORS_REQUEST) {
      epList = (const sl_zigbee_af_endpoint_list_t*)result->responseData;
      if (epList->count >= 1) {
        setWwahClientEndpoint(epList->list[0]);
      }
    }
  }
}

// Pre-Bootload Callback, from ota-client
void sl_zigbee_af_ota_client_pre_bootload_cb(uint8_t srcEndpoint,
                                             uint8_t serverEndpoint,
                                             sl_802154_short_addr_t serverNodeId)
{
  // Attempt to send the Powering Off notification to the coordinator.
  sl_zigbee_af_wwah_server_send_powering_off_notification(serverNodeId,
                                                          srcEndpoint,
                                                          serverEndpoint,
                                                          SL_ZIGBEE_ZCL_WWAH_POWER_NOTIFICATION_REASON_OTA_BOOTLOAD_SUCCESS,
                                                          AMAZON_MANUFACTURING_ID,
                                                          NULL,
                                                          0,
                                                          NULL);
}

// Power Notification Callbacks

#define POWER_DOWN_NOTIFICATION_RESPONSE_TIMEOUT_MS  (MILLISECOND_TICKS_PER_SECOND * 3)
void sl_zigbee_af_wwah_server_send_powering_off_notification(sl_802154_short_addr_t nodeId,
                                                             uint8_t srcEndpoint,
                                                             uint8_t dstEndpoint,
                                                             sl_zigbee_af_wwah_power_notification_reason_t reason,
                                                             uint16_t manufacturerId,
                                                             uint8_t *manufacturerReason,
                                                             uint8_t manufacturerReasonLen,
                                                             void (*pcallback)(sl_zigbee_power_down_notification_result_t) )
{
  sl_status_t status;
  sl_zigbee_af_fill_command_sl_wwah_cluster_powering_off_notification((uint8_t)reason,
                                                                      manufacturerId,
                                                                      manufacturerReasonLen,
                                                                      manufacturerReason,
                                                                      manufacturerReasonLen);
  sl_zigbee_af_set_command_endpoints(srcEndpoint, dstEndpoint);
  status = sl_zigbee_af_send_command_unicast(SL_ZIGBEE_OUTGOING_DIRECT, nodeId);
  if ((SL_STATUS_OK == status) && (pcallback != NULL)) {
    // Track callback function so it can be called when the default response is received,
    // or if a timeout occurs.
    poweringOffCb = pcallback;
    clientTickState = WWAH_CLIENT_TICK_STATE_POWERING_OFF_TIMEOUT;
    sl_zigbee_zcl_schedule_server_tick(srcEndpoint, ZCL_SL_WWAH_CLUSTER_ID, POWER_DOWN_NOTIFICATION_RESPONSE_TIMEOUT_MS);
  } else if (SL_STATUS_OK != status) {
    sl_zigbee_af_sl_wwah_cluster_println("Failed to send Powering Off Notification: 0x%02X", status);
    if (pcallback != NULL) {
      pcallback(SL_ZIGBEE_POWER_DOWN_NOTIFICATION_FAILURE);
    }
  }
}

void sl_zigbee_af_wwah_server_send_powering_on_notification(sl_802154_short_addr_t nodeId,
                                                            uint8_t srcEndpoint,
                                                            uint8_t dstEndpoint,
                                                            sl_zigbee_af_wwah_power_notification_reason_t reason,
                                                            uint16_t manufacturerId,
                                                            uint8_t *manufacturerReason,
                                                            uint8_t manufacturerReasonLen)
{
  sl_status_t status;
  sl_zigbee_af_fill_command_sl_wwah_cluster_powering_on_notification((uint8_t)reason,
                                                                     manufacturerId,
                                                                     manufacturerReasonLen,
                                                                     manufacturerReason,
                                                                     manufacturerReasonLen);
  sl_zigbee_af_set_command_endpoints(srcEndpoint, dstEndpoint);
  status = sl_zigbee_af_send_command_unicast(SL_ZIGBEE_OUTGOING_DIRECT, nodeId);
  if (SL_STATUS_OK != status) {
    sl_zigbee_af_sl_wwah_cluster_println("Failed to send Powering On Notification: 0x%02X", status);
  }
}

static sl_zigbee_af_status_t setSlWwahConfigurationMode(bool enable)
{
  uint8_t confModeEnabled = (enable ? 0x01 : 0x00);
  sl_zigbee_af_status_t afStatus;
  afStatus = sl_zigbee_write_wwah_server_silabs_attribute(wwahServerEndpoint,
                                                          ZCL_SL_CONFIGURATION_MODE_ENABLED_ATTRIBUTE_ID,
                                                          "configuration mode enabled",
                                                          (uint8_t *)&confModeEnabled,
                                                          ZCL_BOOLEAN_ATTRIBUTE_TYPE);

  if (afStatus == SL_ZIGBEE_ZCL_STATUS_SUCCESS) {
#ifndef EZSP_HOST
    // Configure stack
    sl_zigbee_set_zdo_configuration_mode(enable);

    // Write token only if value has changed
    bool currentlyEnabled = wwahConfigurationBitmask
                            & PLUGIN_WWAH_CONFIGURATION_MASK_CONFIGURATION_MODE_ENABLED;
    if (currentlyEnabled != enable) {
      if (enable) {
        wwahConfigurationBitmask |=
          PLUGIN_WWAH_CONFIGURATION_MASK_CONFIGURATION_MODE_ENABLED;
      } else {
        wwahConfigurationBitmask &=
          ~PLUGIN_WWAH_CONFIGURATION_MASK_CONFIGURATION_MODE_ENABLED;
      }
      sl_status_t status = slx_zigbee_token_manager_set_data(COMMON_TOKEN_PLUGIN_WWAH_CONFIGURATION_MASK, (void *)&wwahConfigurationBitmask, sizeof(uint8_t));
      if (status != SL_STATUS_OK) {
        sl_zigbee_af_sl_wwah_cluster_println("Failed to set WWAH Configuration Mask, status: 0x%08X", status);
        return SL_ZIGBEE_ZCL_STATUS_FAILURE;
      }
    }
#endif
  } else {
    sl_zigbee_af_sl_wwah_cluster_println("ERR: writing configuration attribute %02X", afStatus);
  }

  return afStatus;
}

static inline sl_zigbee_af_status_t setParentClassificationEnabled(bool enabled)
{
  sl_zigbee_af_status_t afStatus;
  bool valueChanged = false;
  uint8_t parentClassificationEnabled = enabled ? 1 : 0;

  if (enabled
      == (bool)(wwahConfigurationBitmask
                & PLUGIN_WWAH_CONFIGURATION_MASK_PARENT_CLASSIFICATION_ENABLED)) {
    afStatus = SL_ZIGBEE_ZCL_STATUS_SUCCESS;
  } else {
    // Update the ZCL attribute
    afStatus = sl_zigbee_write_wwah_server_silabs_attribute(sl_zigbee_af_current_endpoint(),
                                                            ZCL_SL_WWAH_PARENT_CLASSIFICATION_ENABLED_ATTRIBUTE_ID,
                                                            "parent classification enabled",
                                                            &parentClassificationEnabled,
                                                            ZCL_BOOLEAN_ATTRIBUTE_TYPE);
    valueChanged = true;
  }

  if (SL_ZIGBEE_ZCL_STATUS_SUCCESS == afStatus) {
    if (valueChanged) {
      // Configure the stack
      sl_zigbee_beacon_classification_params_t param;
      sl_zigbee_get_beacon_classification_params(&param);
      if (enabled) {
        param.beaconClassificationMask |=
          PRIORITIZE_BEACONS_BASED_ON_PARENT_CLASSIFICATION;
      } else {
        param.beaconClassificationMask &=
          ~PRIORITIZE_BEACONS_BASED_ON_PARENT_CLASSIFICATION;
      }
      sl_zigbee_set_beacon_classification_params(&param);

      // Persist the token
      if (enabled) {
        wwahConfigurationBitmask |=
          PLUGIN_WWAH_CONFIGURATION_MASK_PARENT_CLASSIFICATION_ENABLED;
      } else {
        wwahConfigurationBitmask &=
          ~PLUGIN_WWAH_CONFIGURATION_MASK_PARENT_CLASSIFICATION_ENABLED;
      }
      sl_status_t status = slx_zigbee_token_manager_set_data(COMMON_TOKEN_PLUGIN_WWAH_CONFIGURATION_MASK, (void *)&wwahConfigurationBitmask, sizeof(uint8_t));
      if (status != SL_STATUS_OK) {
        sl_zigbee_af_sl_wwah_cluster_println("Failed to set WWAH Configuration Mask, status: 0x%08X", status);
        return SL_ZIGBEE_ZCL_STATUS_FAILURE;
      }
    }
  } else {
    sl_zigbee_af_sl_wwah_cluster_println("WWahParentClassificationCallback failed to "
                                         "update ZCL attribute value: 0x%02X", afStatus);
  }

  return SL_ZIGBEE_ZCL_STATUS_SUCCESS;
}

void sli_zigbee_af_sl_wwah_read_attributes_response_callback(sl_zigbee_af_cluster_id_t clusterId,
                                                             uint8_t *buffer,
                                                             uint16_t bufLen)
{
  uint16_t i;
  uint16_t attributeId;
  uint32_t utcTime;
  uint8_t status;
  if ( clusterId == ZCL_TIME_CLUSTER_ID ) {
    i = 0;
    while ((i + 3) <= bufLen) { // 3 to ensure we can read at least the attribute ID and the status
      attributeId = buffer[i] + (buffer[i + 1] << 8);
      status = buffer[i + 2];
      i += 3;
      if (status == SL_ZIGBEE_ZCL_STATUS_SUCCESS) {
        if ((i + 1) > bufLen) {
          // Too short, dump the message.
          return;
        }
        i++;  // skip the type of the attribute.  We already know what it should be.
        if ( attributeId == ZCL_TIME_ATTRIBUTE_ID ) {
          if ((i + 4) > bufLen) {
            // Too short, dump the message.
            return;
          }
          utcTime = (buffer[i] + (buffer[i + 1] << 8) + (buffer[i + 2] << 16) + (buffer[i + 3] << 24));
          sl_zigbee_af_sl_wwah_cluster_println("Received Utc Time Update, time=%d", utcTime);
#ifdef SIMPLE_CLOCK_PRESENT
          sl_zigbee_af_set_time_cb(utcTime);
#endif
#ifdef TIME_SERVER_PRESENT
          sli_zigbee_af_time_cluster_server_set_current_time(utcTime);
#endif
#ifdef COMPACT_LOGGER_PRESENT
          sl_zigbee_af_compact_logger_update_all_logs_with_utc_time(utcTime);
#endif
          break;
        }
      }
    }
  }
}

//----------------------------------
// Survey Beacons implementation (SoC only)

#ifndef EZSP_HOST

#ifndef SL_ZIGBEE_TEST
sl_zigbee_beacon_survey_t surveyBeaconDataCache[SL_ZIGBEE_AF_PLUGIN_WWAH_SERVER_SILABS_SURVEY_BEACON_RESULT_LIMIT];
static uint8_t surveyBeaconDataCount;
#endif // SL_ZIGBEE_TEST

static uint8_t hubConnectivityIndex, longUptimeIndex, rssiIndex, sortStartIndex, sortEndIndex, targetIndex;

static void cacheApsFrameData(void)
{
  apsFrameCache.destinationEndpoint = sl_zigbee_af_current_command()->apsFrame->sourceEndpoint;
  apsFrameCache.sourceEndpoint = sl_zigbee_af_current_endpoint();
  apsFrameCache.profileId = sl_zigbee_af_profile_id_from_index(sl_zigbee_af_index_from_endpoint(apsFrameCache.destinationEndpoint));
  responseDestinationCache = sl_zigbee_af_current_command()->source;
  responseSeqNumCache = sl_zigbee_af_current_command()->seqNum;
}

bool setupSurveyBeaconProcedure(void)
{
  surveyBeaconDataCount = 0;

  hubConnectivityIndex = 0;
  longUptimeIndex = 0;
  rssiIndex = 0;
  sortStartIndex = 0;
  sortEndIndex = 0;
  targetIndex = 0;

  return true;
}

static bool survey_beacons_scan_complete_handler(uint8_t channel, sl_status_t status)
{
  UNUSED_VAR(channel);

  sl_zigbee_beacon_survey_t beaconSurveyResults[SL_ZIGBEE_AF_PLUGIN_WWAH_SERVER_SILABS_SURVEY_BEACON_RESULT_LIMIT];

  //suppress initialization warnings with legacy packet buffer
  memset(beaconSurveyResults,
         0,
         sizeof(sl_zigbee_beacon_survey_t) * SL_ZIGBEE_AF_PLUGIN_WWAH_SERVER_SILABS_SURVEY_BEACON_RESULT_LIMIT);
  // Compile the results. If there were more than we can fit in a payload, filter the results so we
  // pass the best survey results.
  if (surveyBeaconDataCount > SL_ZIGBEE_AF_PLUGIN_WWAH_SERVER_SILABS_SURVEY_BEACON_RESULT_LIMIT) {
    surveyBeaconDataCount = SL_ZIGBEE_AF_PLUGIN_WWAH_SERVER_SILABS_SURVEY_BEACON_RESULT_LIMIT;
  }
  memmove(&beaconSurveyResults,
          &surveyBeaconDataCache,
          sizeof(sl_zigbee_beacon_survey_t) * surveyBeaconDataCount);

  sl_zigbee_af_fill_command_sl_wwah_cluster_survey_beacons_response(surveyBeaconDataCount,
                                                                    (uint8_t *) beaconSurveyResults,
                                                                    surveyBeaconDataCount * sizeof(sl_zigbee_beacon_survey_t));

  sl_zigbee_aps_frame_t *apsFrame = NULL;
  apsFrame = sl_zigbee_af_get_command_aps_frame();
  apsFrame->destinationEndpoint = apsFrameCache.destinationEndpoint;
  apsFrame->sourceEndpoint = apsFrameCache.sourceEndpoint;
  apsFrame->profileId = apsFrameCache.profileId;

  // This is a response, must set the ZCL header sequence number to that of the request.
  // Offset of sequence number field depends on whether 2-byte mfg code is present.
  if ((sli_zigbee_af_zcl_buffer[0] & ZCL_MANUFACTURER_SPECIFIC_MASK) != 0U) {
    sli_zigbee_af_zcl_buffer[3] = responseSeqNumCache; // fc, mfg code, seq
  } else {
    sli_zigbee_af_zcl_buffer[1] = responseSeqNumCache; // fc, seq
  }

  status = sl_zigbee_af_send_command_unicast(SL_ZIGBEE_OUTGOING_DIRECT, responseDestinationCache);
  if (status != SL_STATUS_OK) {
    sl_zigbee_af_sl_wwah_cluster_println("Failed to send SurveyBeaconResponse: 0x%02X",
                                         status);
  }

  memset(&surveyBeaconDataCache, 0x00, sizeof(surveyBeaconDataCache));

  return true;
}

void sortBeaconSurveyResult(sl_zigbee_beacon_survey_t surveyResult)
{
  // Determine if we have space in our array for this beacon survey result
  if (surveyResult.classificationMask & 0x02 // HUB_CONNECTIVITY_MASK
      && surveyResult.classificationMask & 0x01) { // LONG_UPTIME_MASK
    hubConnectivityIndex++;
    longUptimeIndex++;
    rssiIndex++;
    sortStartIndex = 0;
    sortEndIndex = SL_ZIGBEE_AF_PLUGIN_WWAH_SERVER_SILABS_SURVEY_BEACON_RESULT_LIMIT <= hubConnectivityIndex ? SL_ZIGBEE_AF_PLUGIN_WWAH_SERVER_SILABS_SURVEY_BEACON_RESULT_LIMIT : hubConnectivityIndex;
  } else if (surveyResult.classificationMask & 0x02) { // HUB_CONNECTIVITY_MASK
    longUptimeIndex++;
    rssiIndex++;
    sortStartIndex = hubConnectivityIndex;
    sortEndIndex = SL_ZIGBEE_AF_PLUGIN_WWAH_SERVER_SILABS_SURVEY_BEACON_RESULT_LIMIT <= longUptimeIndex ? SL_ZIGBEE_AF_PLUGIN_WWAH_SERVER_SILABS_SURVEY_BEACON_RESULT_LIMIT : longUptimeIndex;
  } else if (surveyResult.classificationMask & 0x01) { // LONG_UPTIME_MASK
    rssiIndex++;
    sortStartIndex = longUptimeIndex;
    sortEndIndex = SL_ZIGBEE_AF_PLUGIN_WWAH_SERVER_SILABS_SURVEY_BEACON_RESULT_LIMIT <= rssiIndex ? SL_ZIGBEE_AF_PLUGIN_WWAH_SERVER_SILABS_SURVEY_BEACON_RESULT_LIMIT : rssiIndex;
  } else {
    sortStartIndex = rssiIndex;
    sortEndIndex = SL_ZIGBEE_AF_PLUGIN_WWAH_SERVER_SILABS_SURVEY_BEACON_RESULT_LIMIT <= surveyBeaconDataCount + 1 ? SL_ZIGBEE_AF_PLUGIN_WWAH_SERVER_SILABS_SURVEY_BEACON_RESULT_LIMIT : surveyBeaconDataCount + 1;
  }

  if (sortStartIndex >= sortEndIndex) {
    return;
  }

  // Determine index in array to insert this beacon survey result
  int8_t j;
  sl_zigbee_beacon_survey_t temp;
  for (j = sortStartIndex; j < sortEndIndex; j++) {
    temp = surveyBeaconDataCache[j];
    if (j == sortEndIndex - 1 || surveyResult.rssi >= temp.rssi) {
      targetIndex = j;
      break;
    }
  }

  // Shift inferior results out of the array and insert survey result
  for (j = SL_ZIGBEE_AF_PLUGIN_WWAH_SERVER_SILABS_SURVEY_BEACON_RESULT_LIMIT - 2; j >= targetIndex; j--) {
    temp = surveyBeaconDataCache[j];
    surveyBeaconDataCache[j + 1] = temp;
  }
  surveyBeaconDataCache[targetIndex] = surveyResult;
}

static bool survey_beacons_network_found_handler(sl_zigbee_zigbee_network_t* networkFound, sl_zigbee_beacon_data_t *beaconHeard)
{
  int8_t rssi = beaconHeard->rssi;
  sl_802154_short_addr_t nodeId = beaconHeard->sender;
  uint8_t parentClassification = beaconHeard->parentPriority;
  sl_802154_pan_id_t networkPanId = sl_zigbee_get_pan_id();
  if (networkFound->panId == networkPanId) {
    sl_zigbee_beacon_survey_t surveyResult;
    surveyResult.nodeId = nodeId;
    surveyResult.rssi = rssi;
    surveyResult.classificationMask = parentClassification;

    sortBeaconSurveyResult(surveyResult);

    surveyBeaconDataCount++;
  }

  return true;
}

#endif // EZSP_HOST

static void triggerSurveyBeaconsNetworkEventHandler(sl_zigbee_af_event_t * event)
{
  UNUSED_VAR(event);

  #ifndef EZSP_HOST
  sl_status_t status;;
  status = sl_zigbee_survey_beacons(false,
                                    0, // a bitmask value of 0 indicates we should use the current channel
                                    &survey_beacons_network_found_handler,
                                    &survey_beacons_scan_complete_handler);

  if (status != SL_STATUS_OK) {
    memset(&surveyBeaconDataCache, 0x00, sizeof(surveyBeaconDataCache));
    sl_zigbee_af_sl_wwah_cluster_println("Failed to start survey beacon procedure 0x%02X", status);
  }
  sl_zigbee_af_event_set_inactive(triggerSurveyBeaconsNetworkEvents);
  #endif //EZSP_HOST
}

static bool isClusterInUseTcForClusterServerToken(uint16_t clusterId)
{
  uint16_t storedClusterId;
  sl_status_t status = SL_STATUS_OK;
  for (uint8_t i = 0; i < MAX_CLUSTER_CAPACITY_FOR_USE_TC_FOR_CLUSTER_SERVER_TOKEN; i++) {
    status = slx_zigbee_token_manager_get_data(COMMON_TOKEN_PLUGIN_WWAH_USE_TC_FOR_CLUSTER_SERVER + i, (void *)&storedClusterId, sizeof(uint16_t));
    if (status != SL_STATUS_OK) {
      sl_zigbee_af_sl_wwah_cluster_println("Failed to get WWAH Use TC For Cluster Server, status: 0x%08X", status);
      return SL_ZIGBEE_ZCL_STATUS_FAILURE;
    }

    if (clusterId == storedClusterId) {
      // Found a match
      return true;
    } else if (storedClusterId == PLUGIN_WWAH_USE_TC_FOR_CLUSTER_SERVER_NULL_VALUE) {
      // We always store the supported cluster IDs at the beginning of
      // the UseTcForClusterServerToken. If we see a NULL_VALUE here,
      // it means the rest would be the NULL_VALUE as well,
      // we can return earlier here.
      return false;
    }
  }
  return false;
}

static void syncClusterWithUseTcForClusterServerToken(void)
{
  // UseTcForClusterServer for the OTA cluster
  if (isClusterInUseTcForClusterServerToken(ZCL_OTA_BOOTLOAD_CLUSTER_ID)) {
    sl_zigbee_af_ota_client_set_ignore_non_trust_center(true);
  } else {
    sl_zigbee_af_ota_client_set_ignore_non_trust_center(false);
  }

  // UseTcForClusterServer for the Time cluster
  if (isClusterInUseTcForClusterServerToken(ZCL_TIME_CLUSTER_ID)) {
    ignoreNonTrustCenterForTimeCluster = true;
  } else {
    ignoreNonTrustCenterForTimeCluster = false;
  }

  // UseTcForClusterServer for the Poll Control cluster
  if (isClusterInUseTcForClusterServerToken(ZCL_POLL_CONTROL_CLUSTER_ID)) {
    sl_zigbee_af_poll_control_server_set_ignore_non_trust_center(true);
  } else {
    sl_zigbee_af_poll_control_server_set_ignore_non_trust_center(false);
  }

  // UseTcForClusterServer for the Keep Alive cluster
  // Note: In our SDK, we now only use the trust center
  // as the Keep Alive cluster server (see trust-center-keepalive.c).
  // If we support non trust center as Keep Alive cluster servers in the future,
  // we need to update this code as well.
}

void sl_zigbee_af_sl_wwah_cluster_server_manufacturer_specific_attribute_changed_cb(uint8_t endpoint,
                                                                                    sl_zigbee_af_attribute_id_t attributeId,
                                                                                    uint16_t manufacturerCode)
{
  if (manufacturerCode != AMAZON_MANUFACTURING_ID) {
    return;
  }

  switch (attributeId) {
    case ZCL_SL_TOUCHLINK_INTERPAN_ENABLED_ATTRIBUTE_ID:
    {
      sl_status_t status;

      // Since touchlink can only be disabled, not enabled, we don't need to read the attribute
      #ifdef EZSP_HOST
      sl_zigbee_zll_policy_t policy;
      status = sl_zigbee_ezsp_get_policy(SL_ZIGBEE_EZSP_ZLL_POLICY, &policy);
      if (status == SL_STATUS_OK) {
        status = sl_zigbee_af_set_ezsp_policy(SL_ZIGBEE_EZSP_ZLL_POLICY,
                                              policy & ~SL_ZIGBEE_ZLL_POLICY_ENABLED,
                                              "ZLL policy",
                                              "disable");
      }
      #else
      status = sl_zigbee_zll_set_policy(sl_zigbee_zll_get_policy() & ~SL_ZIGBEE_ZLL_POLICY_ENABLED);
      #endif

      if (status != SL_STATUS_OK && status != SL_STATUS_NOT_AVAILABLE) {
        sl_zigbee_af_sl_wwah_cluster_println("Failed to disable touchlink interpan.");
      }
      break;
    }
    case ZCL_SL_MAC_POLL_FAILURE_WAIT_TIME_ATTRIBUTE_ID:
    {
      sl_zigbee_af_status_t status;
      uint8_t macPollFailureWaitTimeSec;
      status = sl_zigbee_read_wwah_server_silabs_attribute(endpoint,
                                                           ZCL_SL_MAC_POLL_FAILURE_WAIT_TIME_ATTRIBUTE_ID,
                                                           "mac poll failure wait time",
                                                           &macPollFailureWaitTimeSec,
                                                           sizeof(macPollFailureWaitTimeSec));

      if (status == SL_ZIGBEE_ZCL_STATUS_SUCCESS) {
        sl_zigbee_set_mac_poll_failure_wait_time(macPollFailureWaitTimeSec * MILLISECOND_TICKS_PER_SECOND);
      }
      break;
    }
    case ZCL_SL_PENDING_NETWORK_UPDATE_CHANNEL_ATTRIBUTE_ID:
    {
      sl_zigbee_af_status_t status;
      uint8_t channel;
      status = sl_zigbee_read_wwah_server_silabs_attribute(endpoint,
                                                           ZCL_SL_PENDING_NETWORK_UPDATE_CHANNEL_ATTRIBUTE_ID,
                                                           "pending network update channel",
                                                           &channel,
                                                           sizeof(channel));
      if (status == SL_ZIGBEE_ZCL_STATUS_SUCCESS) {
        sl_zigbee_set_pending_network_update_channel(channel);
      }
      break;
    }
    case ZCL_SL_PENDING_NETWORK_UPDATE_PANID_ATTRIBUTE_ID:
    {
      sl_zigbee_af_status_t status;
      uint16_t panId;
      status = sl_zigbee_read_wwah_server_silabs_attribute(endpoint,
                                                           ZCL_SL_PENDING_NETWORK_UPDATE_PANID_ATTRIBUTE_ID,
                                                           "pending network update panId",
                                                           (uint8_t*)&panId,
                                                           sizeof(panId));
      if (status == SL_ZIGBEE_ZCL_STATUS_SUCCESS) {
        sl_zigbee_set_pending_network_update_pan_id(panId);
      }
      break;
    }
    default:
      break;
  }
}

//-----------------------
// ZCL commands callbacks

sl_zigbee_af_zcl_request_status_t sl_zigbee_af_sl_wwah_cluster_request_new_aps_link_key_cb(void)
{
  sl_status_t status = sl_zigbee_af_update_tc_link_key_start();
  return status == SL_STATUS_OK ? SL_ZIGBEE_ZCL_STATUS_SUCCESS : SL_ZIGBEE_ZCL_STATUS_FAILURE;
}

sl_zigbee_af_zcl_request_status_t sl_zigbee_af_sl_wwah_cluster_disable_wwah_app_event_retry_algorithm_cb(void)
{
  sl_zigbee_af_status_t afStatus = SL_ZIGBEE_ZCL_STATUS_SUCCESS;

#if defined(WWAH_APP_EVENT_RETRY_MANAGER_PRESENT)
  // Setting up retry parameters
  sl_zigbee_af_wwah_app_event_retry_manager_disable(sl_zigbee_af_current_endpoint());
#else
  sl_zigbee_af_sl_wwah_cluster_println("ERROR - Disable Wwah App Event Retry Algorithm Callback: Functionality is not supported.");
  afStatus = SL_ZIGBEE_ZCL_STATUS_UNSUP_COMMAND;
#endif

  return afStatus;
}

sl_zigbee_af_zcl_request_status_t sl_zigbee_af_sl_wwah_cluster_request_time_cb(void)
{
  sl_status_t status;
  uint8_t timeAttributeIds[] = {
    LOW_BYTE(ZCL_TIME_ATTRIBUTE_ID),
    HIGH_BYTE(ZCL_TIME_ATTRIBUTE_ID),

    LOW_BYTE(ZCL_TIME_STATUS_ATTRIBUTE_ID),
    HIGH_BYTE(ZCL_TIME_STATUS_ATTRIBUTE_ID),

    LOW_BYTE(ZCL_DST_START_ATTRIBUTE_ID),
    HIGH_BYTE(ZCL_DST_START_ATTRIBUTE_ID),

    LOW_BYTE(ZCL_DST_END_ATTRIBUTE_ID),
    HIGH_BYTE(ZCL_DST_END_ATTRIBUTE_ID),

    LOW_BYTE(ZCL_DST_SHIFT_ATTRIBUTE_ID),
    HIGH_BYTE(ZCL_DST_SHIFT_ATTRIBUTE_ID),

    LOW_BYTE(ZCL_LOCAL_TIME_ATTRIBUTE_ID),
    HIGH_BYTE(ZCL_LOCAL_TIME_ATTRIBUTE_ID),
  };
  sl_zigbee_af_fill_command_global_client_to_server_read_attributes(ZCL_TIME_CLUSTER_ID,
                                                                    timeAttributeIds,
                                                                    sizeof(timeAttributeIds));
  sl_zigbee_af_set_command_endpoints(sl_zigbee_af_get_command_aps_frame()->destinationEndpoint,  // source endpoint
                                     sl_zigbee_af_get_command_aps_frame()->sourceEndpoint); // destination endpoint

  sl_802154_short_addr_t sourceNodeId = sl_zigbee_af_current_command()->source;
  if (ignoreNonTrustCenterForTimeCluster
      && (sourceNodeId != SL_ZIGBEE_TRUST_CENTER_NODE_ID)) {
    sl_zigbee_af_sl_wwah_cluster_println("Ignoring non trust center node 0x%04X and requesting time from TC",
                                         sourceNodeId);
    status = sl_zigbee_af_send_command_unicast(SL_ZIGBEE_OUTGOING_DIRECT, SL_ZIGBEE_TRUST_CENTER_NODE_ID);
  } else {
    status = sl_zigbee_af_send_command_unicast(SL_ZIGBEE_OUTGOING_DIRECT, sourceNodeId);
  }

  return status == SL_STATUS_OK ? SL_ZIGBEE_ZCL_STATUS_SUCCESS : SL_ZIGBEE_ZCL_STATUS_FAILURE;
}

sl_zigbee_af_zcl_request_status_t sl_zigbee_af_sl_wwah_cluster_disable_wwah_rejoin_algorithm_cb(void)
{
  sl_zigbee_af_wwah_connectivity_manager_disable_rejoin_algorithm(sl_zigbee_af_current_endpoint());
  return SL_ZIGBEE_ZCL_STATUS_SUCCESS;
}

sl_zigbee_af_zcl_request_status_t sl_zigbee_af_sl_wwah_cluster_clear_binding_table_cb(void)
{
  sl_status_t status;

  status = sl_zigbee_clear_binding_table();

  return status == SL_STATUS_OK ? SL_ZIGBEE_ZCL_STATUS_SUCCESS : SL_ZIGBEE_ZCL_STATUS_FAILURE;
}

sl_zigbee_af_zcl_request_status_t sl_zigbee_af_sl_wwah_cluster_disable_periodic_router_check_ins_cb(void)
{
  sl_status_t status = SL_STATUS_FAIL;

#if defined(WWAH_CONNECTIVITY_MANAGER_PRESENT)
  status = sl_zigbee_af_wwah_connectivity_manager_disable_periodic_router_check_ins();
#else
  sl_zigbee_af_sl_wwah_cluster_println("ERROR - Enable Periodic Router Check Ins Callback: Functionality is not supported.");
#endif

  sl_zigbee_af_status_t afStatus = (status == SL_STATUS_OK) ? SL_ZIGBEE_ZCL_STATUS_SUCCESS : SL_ZIGBEE_ZCL_STATUS_FAILURE;

  return afStatus;
}

// Note: APS ACK is always supported, simply reporting back success.
sl_zigbee_af_zcl_request_status_t sl_zigbee_af_sl_wwah_cluster_remove_aps_acks_on_unicasts_requirement_cb(void)
{
  return SL_ZIGBEE_ZCL_STATUS_SUCCESS;
}

// Note: APS ACK is always supported, exempt list is not stored, so simply reporting back always enabled (ie. length of exempt list is zero).
sl_zigbee_af_zcl_request_status_t sl_zigbee_af_sl_wwah_cluster_aps_ack_requirement_query_cb(void)
{
  sl_status_t status;

  // Build up the response for the query.
  sl_zigbee_af_fill_command_sl_wwah_cluster_aps_ack_enablement_query_response(0, NULL, 0);

  // Send the response.
  status = sl_zigbee_af_send_response();
  if (SL_STATUS_OK != status) {
    sl_zigbee_af_sl_wwah_cluster_println("Failed to send response for ApsAckEnablementQuery: 0x%02X", status);
  }

  return SL_ZIGBEE_ZCL_STATUS_INTERNAL_COMMAND_HANDLED;
}

sl_zigbee_af_zcl_request_status_t sl_zigbee_af_sl_wwah_cluster_disable_ota_downgrades_cb(void)
{
  uint8_t disableOtaDowngrades = 1;
  sl_zigbee_af_status_t afStatus = sl_zigbee_write_wwah_server_silabs_attribute(sl_zigbee_af_current_endpoint(),
                                                                                ZCL_SL_DISABLE_OTA_DOWNGRADES_ATTRIBUTE_ID,
                                                                                "disable OTA downgrades",
                                                                                &disableOtaDowngrades,
                                                                                ZCL_BOOLEAN_ATTRIBUTE_TYPE);

  if (afStatus != SL_ZIGBEE_ZCL_STATUS_SUCCESS) {
    sl_zigbee_af_sl_wwah_cluster_println("Failed to write DisableOtaDowngrades attribute: 0x%02X", afStatus);
  } else {
    wwahConfigurationBitmask |= PLUGIN_WWAH_CONFIGURATION_MASK_DISABLE_OTA_DOWNGRADES;
    sl_status_t status = slx_zigbee_token_manager_set_data(COMMON_TOKEN_PLUGIN_WWAH_CONFIGURATION_MASK, (void *)&wwahConfigurationBitmask, sizeof(uint8_t));
    if (status != SL_STATUS_OK) {
      sl_zigbee_af_sl_wwah_cluster_println("Failed to set WWAH Configuration Mask, status: 0x%08X", status);
      return SL_ZIGBEE_ZCL_STATUS_FAILURE;
    }
    sl_zigbee_af_set_disable_ota_downgrades(disableOtaDowngrades);
  }

  return afStatus;
}

sl_zigbee_af_zcl_request_status_t sl_zigbee_af_sl_wwah_cluster_disable_mgmt_leave_without_rejoin_cb(void)
{
  sl_zigbee_af_status_t afStatus = SL_ZIGBEE_ZCL_STATUS_SUCCESS;
  uint8_t data = 0;

  // Clear MGMTLeaveWithoutRejoinEnabled attribute
  afStatus = sl_zigbee_write_wwah_server_silabs_attribute(wwahServerEndpoint,
                                                          ZCL_SL_MGMT_LEAVE_WITHOUT_REJOIN_ENABLED_ATTRIBUTE_ID,
                                                          "mgmt leave without rejoin enabled",
                                                          &data,
                                                          ZCL_BOOLEAN_ATTRIBUTE_TYPE);
  if (afStatus != SL_ZIGBEE_ZCL_STATUS_SUCCESS) {
    sl_zigbee_af_sl_wwah_cluster_println("Failed to write MGMTLeaveWithoutRejoinEnabled attribute: 0x%02X", afStatus);
  } else {
    // Modify extended security bitmask to disable leave without rejoin
    sl_zigbee_extended_security_bitmask_t mask;
    sl_zigbee_get_extended_security_bitmask(&mask);
    sl_zigbee_set_extended_security_bitmask(mask | SL_ZIGBEE_NWK_LEAVE_WITHOUT_REJOIN_NOT_ALLOWED);
  }

  return afStatus;
}

sl_zigbee_af_zcl_request_status_t sl_zigbee_af_sl_wwah_cluster_disable_touchlink_interpan_message_support_cb(void)
{
  uint8_t touchlinkEnabled = 0;
  sl_zigbee_af_status_t afStatus;
  bool valueChanged = false;

  // Already disabled?
  if (wwahConfigurationBitmask & PLUGIN_WWAH_CONFIGURATION_MASK_ZLL_POLICY_DISABLED) {
    afStatus = SL_ZIGBEE_ZCL_STATUS_SUCCESS;
  } else {
    afStatus = sl_zigbee_write_wwah_server_silabs_attribute(sl_zigbee_af_current_endpoint(),
                                                            ZCL_SL_TOUCHLINK_INTERPAN_ENABLED_ATTRIBUTE_ID,
                                                            "zll policy",
                                                            &touchlinkEnabled,
                                                            ZCL_BOOLEAN_ATTRIBUTE_TYPE);
    valueChanged = true;
  }

  if (afStatus == SL_ZIGBEE_ZCL_STATUS_SUCCESS) {
    if (valueChanged) {
      wwahConfigurationBitmask |= PLUGIN_WWAH_CONFIGURATION_MASK_ZLL_POLICY_DISABLED;
      sl_status_t status = slx_zigbee_token_manager_set_data(COMMON_TOKEN_PLUGIN_WWAH_CONFIGURATION_MASK, (void *)&wwahConfigurationBitmask, sizeof(uint8_t));  
      if (status != SL_STATUS_OK) {
        sl_zigbee_af_sl_wwah_cluster_println("Failed to set WWAH Configuration Mask, status: 0x%08X", status);
        return SL_ZIGBEE_ZCL_STATUS_FAILURE;
      }
    }
  }

  return afStatus;
}

sl_zigbee_af_zcl_request_status_t sl_zigbee_af_sl_wwah_cluster_enable_wwah_parent_classification_cb(void)
{
  return setParentClassificationEnabled(true);
}

sl_zigbee_af_zcl_request_status_t sl_zigbee_af_sl_wwah_cluster_disable_wwah_parent_classification_cb(void)
{
  return setParentClassificationEnabled(false);
}

sl_zigbee_af_zcl_request_status_t sl_zigbee_af_sl_wwah_cluster_disable_configuration_mode_cb(void)
{
  return setSlWwahConfigurationMode(false);
}

sl_zigbee_af_zcl_request_status_t sl_zigbee_af_sl_wwah_cluster_enable_configuration_mode_cb(void)
{
  return setSlWwahConfigurationMode(true);
}

sl_zigbee_af_zcl_request_status_t sl_zigbee_af_sl_wwah_cluster_enable_tc_security_on_ntwk_key_rotation_cb(void)
{
  // Set the attribute such that all network key updates are only processed through unicast messages.
  bool secureNwkKeyRotationEnabled = true;
  sl_zigbee_af_status_t status = sl_zigbee_write_wwah_server_silabs_attribute(sl_zigbee_af_current_endpoint(),
                                                                              ZCL_SL_TC_SECURITY_ON_NTWK_KEY_ROTATION_ENABLED_ATTRIBUTE_ID,
                                                                              "tc security on ntwk key rotation enabled",
                                                                              (uint8_t *)&secureNwkKeyRotationEnabled,
                                                                              ZCL_BOOLEAN_ATTRIBUTE_TYPE);
  if (status == SL_ZIGBEE_ZCL_STATUS_SUCCESS) {
    sl_zigbee_extended_security_bitmask_t extendedBitmask;
    sl_zigbee_get_extended_security_bitmask(&extendedBitmask);
    extendedBitmask |= SL_ZIGBEE_SECURE_NETWORK_KEY_ROTATION;
    sl_zigbee_set_extended_security_bitmask(extendedBitmask);
    return SL_ZIGBEE_ZCL_STATUS_SUCCESS;
  }
  return SL_ZIGBEE_ZCL_STATUS_FAILURE;
}

sl_zigbee_af_zcl_request_status_t sl_zigbee_af_sl_wwah_cluster_disable_wwah_bad_parent_recovery_cb(void)
{
  sli_zigbee_af_wwah_connectivity_manager_disable_bad_parent_recovery(sl_zigbee_af_current_endpoint());
  return SL_ZIGBEE_ZCL_STATUS_SUCCESS;
}

sl_zigbee_af_zcl_request_status_t sl_zigbee_af_sl_wwah_cluster_enable_wwah_bad_parent_recovery_cb(void)
{
  sli_zigbee_af_wwah_connectivity_manager_enable_bad_parent_recovery(sl_zigbee_af_current_endpoint(), 0);
  return SL_ZIGBEE_ZCL_STATUS_SUCCESS;
}

sl_zigbee_af_zcl_request_status_t sl_zigbee_af_sl_wwah_cluster_trust_center_for_cluster_server_query_cb(void)
{
  // n clusters * 2 bytes for cluster IDs
  uint8_t clusterRsp[MAX_CLUSTER_CAPACITY_FOR_USE_TC_FOR_CLUSTER_SERVER_TOKEN * 2];
  uint8_t arrayLen = 0;
  uint8_t numberOfStoredClusters = 0;
  uint16_t clusterId;
  sl_status_t status;

  // Response payload:
  // CMD ID 0x08 | ClusterCount | ClusterId1 | ClusterId2 | .... | clusterIdn |
  //                (1-byte)       (2-bytes)
  for (uint8_t i = 0; i < MAX_CLUSTER_CAPACITY_FOR_USE_TC_FOR_CLUSTER_SERVER_TOKEN; i++) {
    status = slx_zigbee_token_manager_get_data(COMMON_TOKEN_PLUGIN_WWAH_USE_TC_FOR_CLUSTER_SERVER + i, (void *)&clusterId, sizeof(uint16_t));
    if (status != SL_STATUS_OK) {
      sl_zigbee_af_sl_wwah_cluster_println("Failed to get WWAH Use TC For Cluster Server, status: 0x%08X", status);
      return SL_ZIGBEE_ZCL_STATUS_FAILURE;
    }

    if (clusterId != PLUGIN_WWAH_USE_TC_FOR_CLUSTER_SERVER_NULL_VALUE) {
      sl_util_store_low_high_int16u(clusterRsp + arrayLen, clusterId);
      arrayLen += 2;
      numberOfStoredClusters++;
    }
  }

  sl_zigbee_af_fill_command_sl_wwah_cluster_trust_center_for_cluster_server_query_response(numberOfStoredClusters, clusterRsp, arrayLen);
  status = sl_zigbee_af_send_response();
  if (status != SL_STATUS_OK) {
    sl_zigbee_af_sl_wwah_cluster_println("Failed to send TrustCenterForClusterServerQueryResponse: 0x%02X",
                                         status);
  }

  return SL_ZIGBEE_ZCL_STATUS_SUCCESS;
}

sl_zigbee_af_zcl_request_status_t sl_zigbee_af_sl_wwah_cluster_enable_aps_link_key_authorization_cb(sl_zigbee_af_cluster_command_t *cmd)
{
  sl_zcl_sl_works_with_all_hubs_cluster_enable_aps_link_key_authorization_command_t cmd_data;
  sl_zigbee_af_status_t afStatus = SL_ZIGBEE_ZCL_STATUS_SUCCESS;

  if (zcl_decode_sl_works_with_all_hubs_cluster_enable_aps_link_key_authorization_command(cmd, &cmd_data)
      != SL_ZIGBEE_ZCL_STATUS_SUCCESS) {
    return SL_ZIGBEE_ZCL_STATUS_UNSUP_COMMAND;
  }

  // We can just go ahead and enable authorization, no need to worry about missing TC link key
  // as we should always have a key available.
  if (SL_STATUS_OK != sl_zigbee_af_aps_link_key_authorization_enable_with_exempt_cluster_list(cmd_data.numberExemptClusters, cmd_data.clusterId)) {
    sl_zigbee_af_sl_wwah_cluster_println("Failed to store all clusters to exempt list, list is full.");
    afStatus = SL_ZIGBEE_ZCL_STATUS_INSUFFICIENT_SPACE;
  }

  return afStatus;
}

sl_zigbee_af_zcl_request_status_t sl_zigbee_af_sl_wwah_cluster_disable_aps_link_key_authorization_cb(sl_zigbee_af_cluster_command_t *cmd)
{
  sl_zcl_sl_works_with_all_hubs_cluster_disable_aps_link_key_authorization_command_t cmd_data;
  sl_zigbee_af_status_t afStatus = SL_ZIGBEE_ZCL_STATUS_SUCCESS;

  if (zcl_decode_sl_works_with_all_hubs_cluster_disable_aps_link_key_authorization_command(cmd, &cmd_data)
      != SL_ZIGBEE_ZCL_STATUS_SUCCESS) {
    return SL_ZIGBEE_ZCL_STATUS_UNSUP_COMMAND;
  }

  // We can just go ahead and enable authorization, no need to worry about missing TC link key
  // as we should always have a key available.
  if (SL_STATUS_OK != sl_zigbee_af_aps_link_key_authorization_disable_with_exempt_cluster_list(cmd_data.numberExemptClusters, cmd_data.clusterId)) {
    sl_zigbee_af_sl_wwah_cluster_println("Failed to store all clusters to exempt list, list is full.");
    afStatus = SL_ZIGBEE_ZCL_STATUS_INSUFFICIENT_SPACE;
  }

  return afStatus;
}

sl_zigbee_af_zcl_request_status_t sl_zigbee_af_sl_wwah_cluster_aps_link_key_authorization_query_cb(sl_zigbee_af_cluster_command_t *cmd)
{
  sl_zcl_sl_works_with_all_hubs_cluster_aps_link_key_authorization_query_command_t cmd_data;
  sl_status_t status;

  if (zcl_decode_sl_works_with_all_hubs_cluster_aps_link_key_authorization_query_command(cmd, &cmd_data)
      != SL_ZIGBEE_ZCL_STATUS_SUCCESS) {
    return SL_ZIGBEE_ZCL_STATUS_UNSUP_COMMAND;
  }

  // Build up the response for the query
  sl_zigbee_af_fill_command_sl_wwah_cluster_aps_link_key_authorization_query_response(
    cmd_data.clusterId,
    sl_zigbee_af_aps_link_key_authorization_is_required_on_cluster(cmd_data.clusterId));

  // Send the response
  status = sl_zigbee_af_send_response();
  if (SL_STATUS_OK != status) {
    sl_zigbee_af_sl_wwah_cluster_println("Failed to send response for ApsLinkKeyAuthorizationQuery: 0x%02X", status);
  }

  return SL_ZIGBEE_ZCL_STATUS_INTERNAL_COMMAND_HANDLED;
}

sl_zigbee_af_zcl_request_status_t sl_zigbee_af_sl_wwah_cluster_enable_wwah_app_event_retry_algorithm_cb(sl_zigbee_af_cluster_command_t *cmd)
{
  sl_zigbee_af_status_t afStatus = SL_ZIGBEE_ZCL_STATUS_SUCCESS;

#if defined(WWAH_APP_EVENT_RETRY_MANAGER_PRESENT)
  sl_zcl_sl_works_with_all_hubs_cluster_enable_wwah_app_event_retry_algorithm_command_t cmd_data;

  if (zcl_decode_sl_works_with_all_hubs_cluster_enable_wwah_app_event_retry_algorithm_command(cmd, &cmd_data)
      != SL_ZIGBEE_ZCL_STATUS_SUCCESS) {
    return SL_ZIGBEE_ZCL_STATUS_UNSUP_COMMAND;
  }

  // Setting up retry parameters
  if (SL_STATUS_OK != sl_zigbee_af_wwah_app_event_retry_manager_enable_with_params(sl_zigbee_af_current_endpoint(),
                                                                                   cmd_data.firstBackoffTimeSeconds,
                                                                                   cmd_data.backoffSeqCommonRatio,
                                                                                   cmd_data.maxBackoffTimeSeconds,
                                                                                   cmd_data.maxRedeliveryAttempts)) {
    afStatus = SL_ZIGBEE_ZCL_STATUS_INVALID_VALUE;
    sl_zigbee_af_sl_wwah_cluster_println("ERROR - Enable Wwah App Event Retry Algorithm Callback: Invalid parameter.");
  }

#else
  sl_zigbee_af_sl_wwah_cluster_println("ERROR - Enable Wwah App Event Retry Algorithm Callback: Functionality is not supported.");
  afStatus = SL_ZIGBEE_ZCL_STATUS_UNSUP_COMMAND;
#endif

  return afStatus;
}

sl_zigbee_af_zcl_request_status_t sl_zigbee_af_sl_wwah_cluster_enable_wwah_rejoin_algorithm_cb(sl_zigbee_af_cluster_command_t *cmd)
{
  sl_zcl_sl_works_with_all_hubs_cluster_enable_wwah_rejoin_algorithm_command_t cmd_data;

  if (zcl_decode_sl_works_with_all_hubs_cluster_enable_wwah_rejoin_algorithm_command(cmd, &cmd_data)
      != SL_ZIGBEE_ZCL_STATUS_SUCCESS) {
    return SL_ZIGBEE_ZCL_STATUS_UNSUP_COMMAND;
  }

  // Backoffs implemented in a future release
  sl_zigbee_af_wwah_connectivity_manager_set_rejoin_parameters(cmd_data.fastRejoinTimeoutSeconds,
                                                               cmd_data.durationBetweenRejoinsSeconds,
                                                               cmd_data.fastRejoinFirstBackoffSeconds,
                                                               cmd_data.maxBackoffTimeSeconds,
                                                               cmd_data.maxBackoffIterations);
  sl_zigbee_af_wwah_connectivity_manager_enable_rejoin_algorithm(sl_zigbee_af_current_endpoint());
  return SL_ZIGBEE_ZCL_STATUS_SUCCESS;
}

#ifdef IAS_ZONE_SERVER_PRESENT
sl_zigbee_af_zcl_request_status_t sl_zigbee_af_sl_wwah_cluster_set_ias_zone_enrollment_method_cb(sl_zigbee_af_cluster_command_t *cmd)
{
  sl_zcl_sl_works_with_all_hubs_cluster_set_ias_zone_enrollment_method_command_t cmd_data;
  sl_zigbee_af_status_t status;

  if (zcl_decode_sl_works_with_all_hubs_cluster_set_ias_zone_enrollment_method_command(cmd, &cmd_data)
      != SL_ZIGBEE_ZCL_STATUS_SUCCESS) {
    return SL_ZIGBEE_ZCL_STATUS_UNSUP_COMMAND;
  }

  status = sl_zigbee_af_ias_zone_cluster_set_enrollment_method(sl_zigbee_af_get_command_aps_frame()->destinationEndpoint,
                                                               cmd_data.enrollmentMode);
  return status;
}
#endif // IAS_ZONE_SERVER_PRESENT

sl_zigbee_af_zcl_request_status_t sl_zigbee_af_sl_wwah_cluster_enable_periodic_router_check_ins_cb(sl_zigbee_af_cluster_command_t *cmd)
{
  sl_status_t status = SL_STATUS_FAIL;

#if defined(WWAH_CONNECTIVITY_MANAGER_PRESENT)
  sl_zcl_sl_works_with_all_hubs_cluster_enable_periodic_router_check_ins_command_t cmd_data;

  if (zcl_decode_sl_works_with_all_hubs_cluster_enable_periodic_router_check_ins_command(cmd, &cmd_data)
      != SL_ZIGBEE_ZCL_STATUS_SUCCESS) {
    return SL_ZIGBEE_ZCL_STATUS_UNSUP_COMMAND;
  }

  status = sl_zigbee_af_wwah_connectivity_manager_enable_periodic_router_check_ins(cmd_data.checkInInterval);
#else
  sl_zigbee_af_sl_wwah_cluster_println("ERROR - Enable Periodic Router Check Ins Callback: Functionality is not supported.");
#endif

  sl_zigbee_af_status_t afStatus = (status == SL_STATUS_OK) ? SL_ZIGBEE_ZCL_STATUS_SUCCESS : SL_ZIGBEE_ZCL_STATUS_FAILURE;

  return afStatus;
}

sl_zigbee_af_zcl_request_status_t sl_zigbee_af_sl_wwah_cluster_set_mac_poll_failure_wait_time_cb(sl_zigbee_af_cluster_command_t *cmd)
{
  sl_zcl_sl_works_with_all_hubs_cluster_set_mac_poll_failure_wait_time_command_t cmd_data;
  sl_zigbee_af_status_t afStatus;

  if (zcl_decode_sl_works_with_all_hubs_cluster_set_mac_poll_failure_wait_time_command(cmd, &cmd_data)
      != SL_ZIGBEE_ZCL_STATUS_SUCCESS) {
    return SL_ZIGBEE_ZCL_STATUS_UNSUP_COMMAND;
  }

  afStatus = sl_zigbee_write_wwah_server_silabs_attribute(sl_zigbee_af_current_endpoint(),
                                                          ZCL_SL_MAC_POLL_FAILURE_WAIT_TIME_ATTRIBUTE_ID,
                                                          "mac poll failure wait time",
                                                          &cmd_data.waitTime,
                                                          ZCL_INT8U_ATTRIBUTE_TYPE);

  if (afStatus == SL_ZIGBEE_ZCL_STATUS_SUCCESS) {
    sl_status_t status = slx_zigbee_token_manager_set_data(COMMON_TOKEN_WWAH_MAC_POLL_FAILURE_WAIT_TIME, (void *)&cmd_data.waitTime, sizeof(uint8_t));
    if (status != SL_STATUS_OK) {
      sl_zigbee_af_sl_wwah_cluster_println("Failed to set WWAH Mac Poll Failure Wait Time, status: 0x%08X", status);
      return SL_ZIGBEE_ZCL_STATUS_FAILURE;
    }
  }

  return afStatus;
}

sl_zigbee_af_zcl_request_status_t sl_zigbee_af_sl_wwah_cluster_set_pending_network_update_cb(sl_zigbee_af_cluster_command_t *cmd)
{
  sl_zcl_sl_works_with_all_hubs_cluster_set_pending_network_update_command_t cmd_data;
  sl_zigbee_af_status_t afStatus;

  if (zcl_decode_sl_works_with_all_hubs_cluster_set_pending_network_update_command(cmd, &cmd_data)
      != SL_ZIGBEE_ZCL_STATUS_SUCCESS) {
    return false;
  }

  afStatus = sl_zigbee_write_wwah_server_silabs_attribute(sl_zigbee_af_current_endpoint(),
                                                          ZCL_SL_PENDING_NETWORK_UPDATE_CHANNEL_ATTRIBUTE_ID,
                                                          "pending network update channel",
                                                          &cmd_data.channel,
                                                          ZCL_INT8U_ATTRIBUTE_TYPE);

  if (afStatus == SL_ZIGBEE_ZCL_STATUS_SUCCESS) {
    afStatus = sl_zigbee_write_wwah_server_silabs_attribute(sl_zigbee_af_current_endpoint(),
                                                            ZCL_SL_PENDING_NETWORK_UPDATE_PANID_ATTRIBUTE_ID,
                                                            "pending network update panId",
                                                            (uint8_t*)&cmd_data.panId,
                                                            ZCL_INT16U_ATTRIBUTE_TYPE);
  }

  if (afStatus == SL_ZIGBEE_ZCL_STATUS_SUCCESS) {
    sl_status_t status = slx_zigbee_token_manager_set_data(COMMON_TOKEN_PLUGIN_WWAH_PENDING_NETWORK_UPDATE_CHANNEL, (void *)&cmd_data.channel, sizeof(uint8_t));
    if (status != SL_STATUS_OK) {
      sl_zigbee_af_sl_wwah_cluster_println("Failed to set WWAH Pending Network Update Channel, status: 0x%08X", status);
      return SL_ZIGBEE_ZCL_STATUS_FAILURE;
    }
    status = slx_zigbee_token_manager_set_data(COMMON_TOKEN_PLUGIN_WWAH_PENDING_NETWORK_UPDATE_PANID, (void *)&cmd_data.panId, sizeof(uint16_t));
    if (status != SL_STATUS_OK) {
      sl_zigbee_af_sl_wwah_cluster_println("Failed to set WWAH Pending Network Update PAN ID, status: 0x%08X", status);
      return SL_ZIGBEE_ZCL_STATUS_FAILURE;
    }
  }

  return afStatus;
}

// Note: APS ACK is always supported, simply reporting back success.
//       Exempt list is not stored (nor supported) as it'd have a huge token and flash footprint.
sl_zigbee_af_zcl_request_status_t sl_zigbee_af_sl_wwah_cluster_require_aps_acks_on_unicasts_cb(sl_zigbee_af_cluster_command_t *cmd)
{
  (void)cmd;

  return SL_ZIGBEE_ZCL_STATUS_SUCCESS;
}

#ifdef DIDO_PRESENT
sl_zigbee_af_zcl_request_status_t sl_zigbee_af_sl_wwah_cluster_debug_report_query_cb(sl_zigbee_af_cluster_command_t *cmd)
{
  sl_zcl_sl_works_with_all_hubs_cluster_debug_report_query_command_t cmd_data;
  sl_status_t status;

  if (zcl_decode_sl_works_with_all_hubs_cluster_debug_report_query_command(cmd, &cmd_data)
      != SL_ZIGBEE_ZCL_STATUS_SUCCESS) {
    return SL_ZIGBEE_ZCL_STATUS_UNSUP_COMMAND;
  }

  status = sl_zigbee_af_dido_cluster_send_debug_report(cmd_data.debugReportId,
                                                       cmd->source,
                                                       sl_zigbee_af_current_endpoint());
  if (status != SL_STATUS_OK) {
    sl_zigbee_af_sl_wwah_cluster_println("Failed to send debug report, status=0x%02X", status);
  }
  return SL_ZIGBEE_ZCL_STATUS_INTERNAL_COMMAND_HANDLED;
}
#endif // DIDO_PRESENT

sl_zigbee_af_zcl_request_status_t sl_zigbee_af_sl_wwah_cluster_survey_beacons_cb(sl_zigbee_af_cluster_command_t *cmd)
{
  sl_zcl_sl_works_with_all_hubs_cluster_survey_beacons_command_t cmd_data;
  sl_zigbee_af_status_t status;

  if (zcl_decode_sl_works_with_all_hubs_cluster_survey_beacons_command(cmd, &cmd_data)
      != SL_ZIGBEE_ZCL_STATUS_SUCCESS) {
    return SL_ZIGBEE_ZCL_STATUS_UNSUP_COMMAND;
  }

  // Survey beacons is currently only available on SoC devices
  #ifndef EZSP_HOST
  // Enhanced beacon support for survey beacons is currently unavailable on 2.4ghz applications
  if (cmd_data.standardBeacons == 0) {
    // i.e. use enhanced beacon - unsupported - return INVALID_FIELD per WWAH cluster spec rev 023
    status = SL_ZIGBEE_ZCL_STATUS_INVALID_FIELD;
  } else if (setupSurveyBeaconProcedure()) {
    cacheApsFrameData();
    sl_zigbee_af_event_set_delay_ms(triggerSurveyBeaconsNetworkEvents, 1000);
    status = SL_ZIGBEE_ZCL_STATUS_INTERNAL_COMMAND_HANDLED;
  } else {
    status = SL_ZIGBEE_ZCL_STATUS_NOT_FOUND;
  }
  #else
  status = SL_ZIGBEE_ZCL_STATUS_NOT_FOUND;
  #endif // EZSP_HOST

  return status;
}

static sl_zigbee_af_status_t setUseTcForClusterServerToken(uint8_t* tokenIndex, uint16_t clusterId)
{
  uint16_t storedClusterId;
  // Check the token value first to avoid unnecessary token writing.
  sl_status_t status = slx_zigbee_token_manager_get_data(COMMON_TOKEN_PLUGIN_WWAH_USE_TC_FOR_CLUSTER_SERVER + *tokenIndex, (void *)&storedClusterId, sizeof(uint16_t));
  if (status != SL_STATUS_OK) {
    sl_zigbee_af_sl_wwah_cluster_println("Failed to get WWAH Use TC For Cluster Server, status: 0x%08X", status);
    return SL_ZIGBEE_ZCL_STATUS_FAILURE;
  }

  if ((storedClusterId != clusterId)
      && (*tokenIndex < MAX_CLUSTER_CAPACITY_FOR_USE_TC_FOR_CLUSTER_SERVER_TOKEN)) {
    status = slx_zigbee_token_manager_set_data(COMMON_TOKEN_PLUGIN_WWAH_USE_TC_FOR_CLUSTER_SERVER + *tokenIndex, (void *)&clusterId, sizeof(uint16_t));
    if (status != SL_STATUS_OK) {
      sl_zigbee_af_sl_wwah_cluster_println("Failed to set WWAH Use TC For Cluster Server, status: 0x%08X", status);
      return SL_ZIGBEE_ZCL_STATUS_FAILURE;
    }
  }
  (*tokenIndex)++;
  return SL_ZIGBEE_ZCL_STATUS_SUCCESS;
}

static void cleanUseTcForClusterServerToken(uint8_t tokenIndex)
{
  uint16_t clusterId;
  for (uint8_t i = tokenIndex; i < MAX_CLUSTER_CAPACITY_FOR_USE_TC_FOR_CLUSTER_SERVER_TOKEN; i++) {
    sl_status_t status = slx_zigbee_token_manager_get_data(COMMON_TOKEN_PLUGIN_WWAH_USE_TC_FOR_CLUSTER_SERVER + i, (void *)&clusterId, sizeof(uint16_t));
    if (status != SL_STATUS_OK) {
      sl_zigbee_af_sl_wwah_cluster_println("Failed to get WWAH Use TC For Cluster Server, status: 0x%08X", status);
      return;
    }

    if (clusterId != PLUGIN_WWAH_USE_TC_FOR_CLUSTER_SERVER_NULL_VALUE) {
      clusterId = PLUGIN_WWAH_USE_TC_FOR_CLUSTER_SERVER_NULL_VALUE;
      status = slx_zigbee_token_manager_set_data(COMMON_TOKEN_PLUGIN_WWAH_USE_TC_FOR_CLUSTER_SERVER + tokenIndex, (void *)&clusterId, sizeof(uint16_t));
      if (status != SL_STATUS_OK) {
        sl_zigbee_af_sl_wwah_cluster_println("Failed to set WWAH Use TC For Cluster Server, status: 0x%08X", status);
        return;
      }
    }
  }
}

sl_zigbee_af_zcl_request_status_t sl_zigbee_af_sl_wwah_cluster_use_trust_center_for_cluster_server_cb(sl_zigbee_af_cluster_command_t *cmd)
{
  sl_zcl_sl_works_with_all_hubs_cluster_use_trust_center_for_cluster_server_command_t cmd_data;
  sl_status_t status;
  // n clusters * ( 2 bytes for cluster and 1 byte for status)
  uint8_t clusterStatusRsp[MAX_CLUSTER_CAPACITY_FOR_USE_TC_FOR_CLUSTER_SERVER_RESPONSE_COMMAND * (2 + 1)];
  uint8_t clusterStatusLength = 0;
  uint8_t arrayLen = 0;
  uint8_t tokenIndex = 0;
  uint16_t rcvdClusterId;
  sl_zigbee_af_status_t afStatus;

  if (zcl_decode_sl_works_with_all_hubs_cluster_use_trust_center_for_cluster_server_command(cmd, &cmd_data)
      != SL_ZIGBEE_ZCL_STATUS_SUCCESS) {
    return SL_ZIGBEE_ZCL_STATUS_UNSUP_COMMAND;
  }

  // Response payload:
  // CMD ID 0x9E | CmdStatus | ClusterCount | ClusterId1 | Status1 | ClusterId2 | Status2 |.... | clusterIdn | Statusn |
  //               (1-byte)     (1-byte)        (2-bytes)  (1-bytes)
  for (clusterStatusLength = 0; clusterStatusLength < cmd_data.numberOfClusters
       && clusterStatusLength < MAX_CLUSTER_CAPACITY_FOR_USE_TC_FOR_CLUSTER_SERVER_RESPONSE_COMMAND;
       clusterStatusLength++) {
    // fetch a clusterId.
    rcvdClusterId = sl_util_fetch_low_high_int16u(cmd_data.clusterId + (clusterStatusLength * 2));

    // Currently we support to force a device to use trust center only for
    // OTA, TIME, POLL_CONTROL and KEEP_ALIVE clusters.
    // All other cluster are unsupported.
    if (rcvdClusterId == ZCL_OTA_BOOTLOAD_CLUSTER_ID
#if defined(ZCL_USING_TIME_CLUSTER_CLIENT)
        || rcvdClusterId == ZCL_TIME_CLUSTER_ID
#endif
#if defined(ZCL_USING_POLL_CONTROL_CLUSTER_SERVER) && defined(POLL_CONTROL_SERVER_PRESENT)
        || rcvdClusterId == ZCL_POLL_CONTROL_CLUSTER_ID
#endif
#if defined(ZCL_USING_KEEPALIVE_CLUSTER_CLIENT) && defined(TRUST_CENTER_KEEPALIVE_PRESENT)
        || rcvdClusterId == ZCL_KEEPALIVE_CLUSTER_ID
#endif
        ) {
      // Based on WWAH Test Cases, OTA cluster is mandatory for UseTcForClusterServer;
      // Time cluster requires the time client is implemented;
      // Poll Control cluster requires the poll control server is implemented;
      // Keep Alive cluster requires the keep alive client is implemented.
      afStatus = setUseTcForClusterServerToken(&tokenIndex, rcvdClusterId);
    } else {
      afStatus = SL_ZIGBEE_ZCL_STATUS_UNSUP_COMMAND;
    }

    // Append clusterId in response payload
    sl_util_store_low_high_int16u(clusterStatusRsp + arrayLen, rcvdClusterId);
    arrayLen += 2;
    // Append status to indicate whether a cluster support to use TC or not.
    clusterStatusRsp[arrayLen++] = afStatus;
  }

  // Clean the TOKEN_PLUGIN_WWAH_USE_TC_FOR_CLUSTER_SERVER
  // that could be set values by the previous UseTrustCenterForClusterServer command
  // and the values don't match with the current UseTrustCenterForClusterServer command
  cleanUseTcForClusterServerToken(tokenIndex);

  // Sync the ignoreNonTrustCenter setting of all supported clusters
  // with the UseTcForClusterServerToken
  syncClusterWithUseTcForClusterServerToken();

  sl_zigbee_af_fill_command_sl_wwah_cluster_use_trust_center_for_cluster_server_response(SL_ZIGBEE_ZCL_STATUS_SUCCESS,
                                                                                         clusterStatusLength,
                                                                                         clusterStatusRsp,
                                                                                         arrayLen);

  status = sl_zigbee_af_send_response();
  if (status != SL_STATUS_OK) {
    sl_zigbee_af_sl_wwah_cluster_println("Failed to send UseTrustCenterForClusterServerResponse: 0x%02X",
                                         status);
  }

  status = sl_zigbee_af_send_immediate_default_response(SL_ZIGBEE_ZCL_STATUS_SUCCESS);

  if (status != SL_STATUS_OK) {
    sl_zigbee_af_sl_wwah_cluster_println("Failed to send default success response: 0x%02X", status);
  }

  return SL_ZIGBEE_ZCL_STATUS_INTERNAL_COMMAND_HANDLED;
}

uint32_t sl_zigbee_af_sl_wwah_cluster_server_command_parse(sl_service_opcode_t opcode,
                                                           sl_service_function_context_t *context)
{
  (void)opcode;

  sl_zigbee_af_cluster_command_t *cmd = (sl_zigbee_af_cluster_command_t *)context->data;
  sl_zigbee_af_zcl_request_status_t status = SL_ZIGBEE_ZCL_STATUS_UNSUP_COMMAND;

  if (cmd->mfgSpecific) {
    switch (cmd->commandId) {
      case ZCL_ENABLE_APS_LINK_KEY_AUTHORIZATION_COMMAND_ID:
      {
        status = sl_zigbee_af_sl_wwah_cluster_enable_aps_link_key_authorization_cb(cmd);
        break;
      }
      case ZCL_DISABLE_APS_LINK_KEY_AUTHORIZATION_COMMAND_ID:
      {
        status = sl_zigbee_af_sl_wwah_cluster_disable_aps_link_key_authorization_cb(cmd);
        break;
      }
      case ZCL_APS_LINK_KEY_AUTHORIZATION_QUERY_COMMAND_ID:
      {
        status = sl_zigbee_af_sl_wwah_cluster_aps_link_key_authorization_query_cb(cmd);
        break;
      }
      case ZCL_REQUEST_NEW_APS_LINK_KEY_COMMAND_ID:
      {
        status = sl_zigbee_af_sl_wwah_cluster_request_new_aps_link_key_cb();
        break;
      }
      case ZCL_ENABLE_WWAH_APP_EVENT_RETRY_ALGORITHM_COMMAND_ID:
      {
        status = sl_zigbee_af_sl_wwah_cluster_enable_wwah_app_event_retry_algorithm_cb(cmd);
        break;
      }
      case ZCL_DISABLE_WWAH_APP_EVENT_RETRY_ALGORITHM_COMMAND_ID:
      {
        status = sl_zigbee_af_sl_wwah_cluster_disable_wwah_app_event_retry_algorithm_cb();
        break;
      }
      case ZCL_REQUEST_TIME_COMMAND_ID:
      {
        status = sl_zigbee_af_sl_wwah_cluster_request_time_cb();
        break;
      }
      case ZCL_ENABLE_WWAH_REJOIN_ALGORITHM_COMMAND_ID:
      {
        status = sl_zigbee_af_sl_wwah_cluster_enable_wwah_rejoin_algorithm_cb(cmd);
        break;
      }
      case ZCL_DISABLE_WWAH_REJOIN_ALGORITHM_COMMAND_ID:
      {
        status = sl_zigbee_af_sl_wwah_cluster_disable_wwah_rejoin_algorithm_cb();
        break;
      }
  #ifdef IAS_ZONE_SERVER_PRESENT
      case ZCL_SET_IAS_ZONE_ENROLLMENT_METHOD_COMMAND_ID:
      {
        status = sl_zigbee_af_sl_wwah_cluster_set_ias_zone_enrollment_method_cb(cmd);
        break;
      }
  #endif // IAS_ZONE_SERVER_PRESENT
      case ZCL_CLEAR_BINDING_TABLE_COMMAND_ID:
      {
        status = sl_zigbee_af_sl_wwah_cluster_clear_binding_table_cb();
        break;
      }
      case ZCL_ENABLE_PERIODIC_ROUTER_CHECK_INS_COMMAND_ID:
      {
        status = sl_zigbee_af_sl_wwah_cluster_enable_periodic_router_check_ins_cb(cmd);
        break;
      }
      case ZCL_DISABLE_PERIODIC_ROUTER_CHECK_INS_COMMAND_ID:
      {
        status = sl_zigbee_af_sl_wwah_cluster_disable_periodic_router_check_ins_cb();
        break;
      }
      case ZCL_SET_MAC_POLL_FAILURE_WAIT_TIME_COMMAND_ID:
      {
        status = sl_zigbee_af_sl_wwah_cluster_set_mac_poll_failure_wait_time_cb(cmd);
        break;
      }
      case ZCL_SET_PENDING_NETWORK_UPDATE_COMMAND_ID:
      {
        status = sl_zigbee_af_sl_wwah_cluster_set_pending_network_update_cb(cmd);
        break;
      }
      case ZCL_REQUIRE_APS_ACKS_ON_UNICASTS_COMMAND_ID:
      {
        status = sl_zigbee_af_sl_wwah_cluster_require_aps_acks_on_unicasts_cb(cmd);
        break;
      }
      case ZCL_REMOVE_APS_ACKS_ON_UNICASTS_REQUIREMENT_COMMAND_ID:
      {
        status = sl_zigbee_af_sl_wwah_cluster_remove_aps_acks_on_unicasts_requirement_cb();
        break;
      }
      case ZCL_APS_ACK_REQUIREMENT_QUERY_COMMAND_ID:
      {
        status = sl_zigbee_af_sl_wwah_cluster_aps_ack_requirement_query_cb();
        break;
      }
  #ifdef DIDO_PRESENT
      case ZCL_DEBUG_REPORT_QUERY_COMMAND_ID:
      {
        status = sl_zigbee_af_sl_wwah_cluster_debug_report_query_cb(cmd);
        break;
      }
  #endif // DIDO_PRESENT
      case ZCL_SURVEY_BEACONS_COMMAND_ID:
      {
        status = sl_zigbee_af_sl_wwah_cluster_survey_beacons_cb(cmd);
        break;
      }
      case ZCL_DISABLE_OTA_DOWNGRADES_COMMAND_ID:
      {
        status = sl_zigbee_af_sl_wwah_cluster_disable_ota_downgrades_cb();
        break;
      }
      case ZCL_DISABLE_MGMT_LEAVE_WITHOUT_REJOIN_COMMAND_ID:
      {
        status = sl_zigbee_af_sl_wwah_cluster_disable_mgmt_leave_without_rejoin_cb();
        break;
      }
      case ZCL_DISABLE_TOUCHLINK_INTERPAN_MESSAGE_SUPPORT_COMMAND_ID:
      {
        status = sl_zigbee_af_sl_wwah_cluster_disable_touchlink_interpan_message_support_cb();
        break;
      }
      case ZCL_ENABLE_WWAH_PARENT_CLASSIFICATION_COMMAND_ID:
      {
        status = sl_zigbee_af_sl_wwah_cluster_enable_wwah_parent_classification_cb();
        break;
      }
      case ZCL_DISABLE_WWAH_PARENT_CLASSIFICATION_COMMAND_ID:
      {
        status = sl_zigbee_af_sl_wwah_cluster_disable_wwah_parent_classification_cb();
        break;
      }
      case ZCL_ENABLE_TC_SECURITY_ON_NTWK_KEY_ROTATION_COMMAND_ID:
      {
        status = sl_zigbee_af_sl_wwah_cluster_enable_tc_security_on_ntwk_key_rotation_cb();
        break;
      }
      case ZCL_ENABLE_WWAH_BAD_PARENT_RECOVERY_COMMAND_ID:
      {
        status = sl_zigbee_af_sl_wwah_cluster_enable_wwah_bad_parent_recovery_cb();
        break;
      }
      case ZCL_DISABLE_WWAH_BAD_PARENT_RECOVERY_COMMAND_ID:
      {
        status = sl_zigbee_af_sl_wwah_cluster_disable_wwah_bad_parent_recovery_cb();
        break;
      }
      case ZCL_ENABLE_CONFIGURATION_MODE_COMMAND_ID:
      {
        status = sl_zigbee_af_sl_wwah_cluster_enable_configuration_mode_cb();
        break;
      }
      case ZCL_DISABLE_CONFIGURATION_MODE_COMMAND_ID:
      {
        status = sl_zigbee_af_sl_wwah_cluster_disable_configuration_mode_cb();
        break;
      }
      case ZCL_USE_TRUST_CENTER_FOR_CLUSTER_SERVER_COMMAND_ID:
      {
        status = sl_zigbee_af_sl_wwah_cluster_use_trust_center_for_cluster_server_cb(cmd);
        break;
      }
      case ZCL_TRUST_CENTER_FOR_CLUSTER_SERVER_QUERY_COMMAND_ID:
      {
        status = sl_zigbee_af_sl_wwah_cluster_trust_center_for_cluster_server_query_cb();
        break;
      }
    }
  }

  return status;
}
