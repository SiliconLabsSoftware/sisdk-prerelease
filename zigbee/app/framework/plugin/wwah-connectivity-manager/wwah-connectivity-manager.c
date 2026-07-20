/***************************************************************************//**
 * @file
 * @brief Definitions for the WWAH Connectivity Manager plugin, which implements
 *        the WWAH rejoining algorithm as defined in the WWAH specifications.
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
#include "wwah-connectivity-manager.h"
#include "stack/include/zigbee-security-manager.h"
#include "sl_zigbee_token.h"
#include "stack/config/sl_zigbee_token_defines.h"

#include "wwah-connectivity-manager-config.h"
#ifdef SL_COMPONENT_CATALOG_PRESENT
#include "sl_component_catalog.h"
#endif
#ifdef SL_CATALOG_ZIGBEE_END_DEVICE_SUPPORT_PRESENT
#include "end-device-support.h"
#endif
#ifdef SL_CATALOG_ZIGBEE_TRUST_CENTER_KEEPALIVE_PRESENT
#include "trust-center-keepalive.h"
#define TRUST_CENTER_KEEPALIVE_PRESENT
#endif
#ifdef SL_CATALOG_ZIGBEE_POLL_CONTROL_SERVER_PRESENT
#include "poll-control-server.h"
#endif
#ifdef SL_CATALOG_ZIGBEE_ZLL_COMMISSIONING_COMMON_PRESENT
#include "zll-commissioning-common.h"
#define ZLL_COMMISSIONING_COMMON_PRESENT
#endif
#if (SL_ZIGBEE_AF_PLUGIN_WWAH_CONNECTIVITY_MANAGER_USE_PREFERRED_CHANNEL_MASK == 1)
#define USE_PREFERRED_CHANNEL_MASK
#endif
#if (SL_ZIGBEE_AF_PLUGIN_WWAH_CONNECTIVITY_MANAGER_ALLOW_TRIGGER_STATE_SEND_REJOIN == 1)
#define ALLOW_TRIGGER_STATE_SEND_REJOIN
#endif

//------------------------------------------------------------------------------
// Notes
/*
   This file implements the rejoin algorithm per the Zigbee WWAH Requirements
   and WWAH ZCL documents. The algorithm is shown below.
   1.  If either of the following conditions is met, skip to step 5:
   a) device is a Router
   b) device is a Non-Sleepy End Device
   c) device is an Sleepy End Device and current rejoin was caused by Poll
     Control check-in failure described above
   2.  Secure Rejoin on the Current channel first
   3.  Trust Center rejoin on current channel
   4.  Optional: Trust Center rejoin on preferred channel list
   5.  Trust Center Rejoin on all channels
   6.  If Trust Center rejoin succeeds, connectivity is restored. No more steps are
    executed.
   7.  Otherwise, device continues to use its previous network parameters and waits
    for X seconds or until additional triggers (example: user button push)
   a.  For Routers and Non-Sleepy End Devices, X seconds will be equal to the
      Keepalive cluster value or 24 hours if Keepalive cluster is not
      implemented.
   b.  For Sleepy End devices, if the WWAH rejoin algorithm is enabled then the
      wait will be determined by the WWAH rejoin algorithm. If the WWAH rejoin
      algorithm is NOT enabled then the wait will be 15 minutes.
   8.  Routers and Non-Sleepy End Devices shall go back to step 3
   9.  Optional:  Secure Rejoins on preferred channel list
   10. Secure Rejoin on all channels
   11. If Secure Rejoin succeeds, parent connectivity is restored.
   a.  Hub connectivity may be tested at this point.
   b.  No more steps are executed.
   12. Otherwise, device sleeps for X seconds or until additional triggers
    (example:  user button push)
   a.  Routers and Non-Sleepy End Devices should not reach this point (return to
      step 3 after step 8).
   b.  For Sleepy End devices, if the WWAH rejoin algorithm is enabled then the
      wait will be determined by the WWAH rejoin algorithm. If the WWAH rejoin
      algorithm is NOT enabled then the wait will be 15 minutes.
   13. Goto step 1
 */

//------------------------------------------------------------------------------
// Globals

enum {
  CONNECTION_LOST_TO_PARENT                             = 0x01,
  CONNECTION_LOST_TO_HUB_DUE_TO_TC_KEEPALIVE            = 0x02,
  CONNECTION_LOST_TO_HUB_DUE_TO_POLL_CONTROL_CHECKIN    = 0x04
};

static uint8_t wwahConnectionErrorsBitmask = 0;

enum {
  STATE_NONE,
  STATE_SECURE_REJOIN_ON_CURRENT_CHANNEL,
  STATE_TC_REJOIN_ON_CURRENT_CHANNEL,
  STATE_TC_REJOIN_ON_PREFERRED_CHANNELS,
  STATE_TC_REJOIN_ON_ALL_CHANNELS,
  STATE_WAIT_FOR_TRIGGER,
  STATE_SECURE_REJOIN_ON_PREFERRED_CHANNELS,
  STATE_SECURE_REJOIN_ON_ALL_CHANNELS,
  STATE_WAIT_FOR_SECOND_TRIGGER
};

static char* wwahStateStrings[] = {
  "None",
  "Secure Rejoin on Current Channel",
  "TC Rejoin on Current Channel",
  "TC Rejoin on Preferred Channels",
  "TC Rejoin on All Channels",
  "Wait for Trigger",
  "Secure Rejoin on Preferred Channels",
  "Secure Rejoin on All Channels",
  "Wait for Second Trigger",
};

#ifdef USE_PREFERRED_CHANNEL_MASK
 #define STATE_AFTER_TC_REJOIN_ON_CURRENT_CHANNEL \
  STATE_TC_REJOIN_ON_PREFERRED_CHANNELS
 #define STATE_AFTER_WAIT_FOR_TRIGGER \
  STATE_SECURE_REJOIN_ON_PREFERRED_CHANNELS
#else
 #define STATE_AFTER_TC_REJOIN_ON_CURRENT_CHANNEL \
  STATE_TC_REJOIN_ON_ALL_CHANNELS
 #define STATE_AFTER_WAIT_FOR_TRIGGER \
  STATE_SECURE_REJOIN_ON_ALL_CHANNELS
#endif

static uint8_t wwahConnectionRecoveryState = STATE_NONE;

static sl_zigbee_af_event_t stateTransitionEvent;
void stateTransitionEventHandler(sl_zigbee_af_event_t * event);

static sl_zigbee_af_event_t longUptimeEvent;
void longUptimeEventHandler(sl_zigbee_af_event_t * event);

static sl_zigbee_af_event_t fastRejoinEvent;
void fastRejoinEventHandler(sl_zigbee_af_event_t * event);

static sl_zigbee_af_event_t stateMachineEvent;
void stateMachineEventHandler(sl_zigbee_af_event_t * event);

static sl_zigbee_af_event_t badParentRecoveryEvent;
static void badParentRecoveryEventHandler(sl_zigbee_af_event_t * event);

#ifdef TRUST_CENTER_KEEPALIVE_PRESENT
 #define WAIT_TIME_BASE_SEC   sli_zigbee_af_trust_center_keepalive_get_base_time_seconds()
 #define WAIT_TIME_JITTER_SEC sli_zigbee_af_trust_center_keepalive_get_jitter_time_seconds()
#else // !TRUST_CENTER_KEEPALIVE_PRESENT
// From Zigbee WWAH Requirements doc:
// For Routers and Non-Sleepy End Devices, X seconds will be equal to the
// Keepalive cluster value or 24 hours if Keepalive cluster is not implemented
 #define WAIT_TIME_BASE_SEC   (24 * 60 * 60)
 #define WAIT_TIME_JITTER_SEC (60)
#endif // TRUST_CENTER_KEEPALIVE_PRESENT

#ifdef ZLL_COMMISSIONING_COMMON_PRESENT
  #define ZLL_RECOVERY_RETRY_DEFAULT      (1000)     // 1000 ms
#endif

#define BAD_PARENT_RECOVERY_RETRY_DEFAULT (24 * 60)  // 24 hours in min

#ifdef SL_ZIGBEE_TEST
  #ifdef SL_ZIGBEE_AF_PLUGIN_WWAH_CONNECTIVITY_MANAGER_LONG_UPTIME_THRESHOLD
    #undef SL_ZIGBEE_AF_PLUGIN_WWAH_CONNECTIVITY_MANAGER_LONG_UPTIME_THRESHOLD
  #endif // SL_ZIGBEE_AF_PLUGIN_WWAH_CONNECTIVITY_MANAGER_LONG_UPTIME_THRESHOLD
  #define SL_ZIGBEE_AF_PLUGIN_WWAH_CONNECTIVITY_MANAGER_LONG_UPTIME_THRESHOLD 10  // min
#endif // SL_ZIGBEE_TEST

// State information
static bool issuedRejoin = false;
static bool inFastRejoin = false;
static bool fastRejoinCompleted = false;
static bool backoffBetweenFastRejoinsCompleted = false;
static uint16_t numConsecutiveFastRejoinsCompleted = 0;

/*
 * parentRssiCheckStats keeps stats of how many times we have checked
 * the parent connectivity since the last NETWORK_UP or badParentRecovery enabled event
 */
static uint32_t parentRssiCheckStats = 0;

/*
 * attemptedParentRejoinStats keeps general stats about the number of parent rejoin
 * attempts becasue of bad parent connectivity.
 * This number is reset everytime badParentRecovery is disabled/enabled.
 */
static uint16_t attemptedParentRejoinStats = 0;

/*
 * attempt parent rejoin for disconnected parent once every badParentRecoveryRetry period
 * the effect especially kicks in if rejoinAlgoritm itself is not enabled
 */
static bool parentRecoveryRequired = false;

// // Configurable parameters
static sl_zigbee_af_connection_manager_token_struct_t connectionManagerTokenStruct;
static uint16_t badParentRecoveryRetryPeriod = BAD_PARENT_RECOVERY_RETRY_DEFAULT;//24 hours by default

// WWAH server endpoint as conveyed via wwah-server-silabs plugin during init.
static uint8_t wwahServerEndpoint = 0;

//------------------------------------------------------------------------------
// Prototypes

static uint8_t getNextState(void);
static uint32_t getTriggerStateWaitTimeMs(void);
static uint32_t calculateWaitTimeForNextStateMs(void);
static void wwahRunConnectivityRecoveryStateMachine(void);
static void clearState(void);
static void rejoinOnChannelMask(uint32_t channelMask, bool secureRejoin);
static void setTrustCenterConnectivity();

#define rejoinOnCurrentChannel(secure)  rejoinOnChannelMask(0, secure)
#define rejoinOnAllChannels(secure) \
  rejoinOnChannelMask(SL_ZIGBEE_ALL_802_15_4_CHANNELS_MASK, secure)
#ifdef USE_PREFERRED_CHANNEL_MASK
 #define rejoinOnPreferredChannels(secure) \
  rejoinOnChannelMask(SL_ZIGBEE_AF_PLUGIN_WWAH_CONNECTIVITY_MANAGER_PREFERRED_CHANNEL_MASK, secure)
#else // USE_PREFERRED_CHANNEL_MASK
 #define rejoinOnPreferredChannels(secure)
#endif // USE_PREFERRED_CHANNEL_MASK

sl_status_t sl_zigbee_af_wwah_connectivity_manager_token_init(void)
{
  sl_status_t status = SL_STATUS_OK;
  sl_zigbee_af_connection_manager_token_struct_t tokPluginConnectionManagerStateDefault = TOKEN_PLUGIN_CONNECTION_MANAGER_STATE_DEFAULT;
  status = sl_zigbee_initialize_basic_token(COMMON_TOKEN_PLUGIN_CONNECTION_MANAGER_STATE, &tokPluginConnectionManagerStateDefault, sizeof(sl_zigbee_af_connection_manager_token_struct_t));
  uint16_t tokPluginWWAHConnectionManagerPeriodicRouterCheckinIntervalDefault = TOKEN_PLUGIN_WWAH_CONNECTION_MANAGER_PERIODIC_ROUTER_CHECKIN_INTERVAL_DEFAULT;
  status = sl_zigbee_initialize_basic_token(COMMON_TOKEN_PLUGIN_WWAH_CONNECTION_MANAGER_PERIODIC_ROUTER_CHECKIN_INTERVAL, &tokPluginWWAHConnectionManagerPeriodicRouterCheckinIntervalDefault, sizeof(uint16_t));
  return status;
}

//------------------------------------------------------------------------------
// Implemented callbacks

void sl_zigbee_af_wwah_connectivity_manager_init_cb(uint8_t endpoint)
{
  assert(SL_STATUS_OK == sl_zigbee_af_wwah_connectivity_manager_token_init());

  sl_zigbee_af_network_event_init(&stateTransitionEvent,
                                  stateTransitionEventHandler);
  sl_zigbee_af_network_event_init(&longUptimeEvent,
                                  longUptimeEventHandler);
  sl_zigbee_af_network_event_init(&fastRejoinEvent,
                                  fastRejoinEventHandler);
  sl_zigbee_af_network_event_init(&stateMachineEvent,
                                  stateMachineEventHandler);
  sl_zigbee_af_network_event_init(&badParentRecoveryEvent,
                                  badParentRecoveryEventHandler);
  //Restore connectivity manager state
  sl_status_t status = slx_zigbee_token_manager_get_data(COMMON_TOKEN_PLUGIN_CONNECTION_MANAGER_STATE, (void *)&connectionManagerTokenStruct, sizeof(sl_zigbee_af_connection_manager_token_struct_t));
  if (status != SL_STATUS_OK) {
    sl_zigbee_af_sl_wwah_cluster_println("Failed to get WWAH Connection Manager State, status: 0x%08X", status);
    return;
  }
  // Restore WWAH rejoin algorithm enabled attribute
  bool rejoinAlgorithmEnabled = (connectionManagerTokenStruct.connectionManagerFlags & PLUGIN_WWAH_CONNECTION_MANAGER_ENABLED_STATE);
  sl_zigbee_write_wwah_server_silabs_attribute(endpoint,
                                               ZCL_SL_WWAH_REJOIN_ENABLED_ATTRIBUTE_ID,
                                               "WWAH rejoin",
                                               (uint8_t *)&rejoinAlgorithmEnabled,
                                               ZCL_BOOLEAN_ATTRIBUTE_TYPE);

  // Restore WWAH bad parent recovery enabled attribute
  bool badParentRecoveryEnabled = (connectionManagerTokenStruct.connectionManagerFlags & PLUGIN_WWAH_CONNECTION_MANAGER_BAD_PARENT_RECOVERY);
  sl_zigbee_write_wwah_server_silabs_attribute(endpoint,
                                               ZCL_SL_WWAH_BAD_PARENT_RECOVERY_ENABLED_ATTRIBUTE_ID,
                                               "WWAH bad parent",
                                               (uint8_t *)&badParentRecoveryEnabled,
                                               ZCL_BOOLEAN_ATTRIBUTE_TYPE);

  // Arm the long uptime timer
  sl_zigbee_af_event_set_delay_ms(&longUptimeEvent,
                                  SL_ZIGBEE_AF_PLUGIN_WWAH_CONNECTIVITY_MANAGER_LONG_UPTIME_THRESHOLD * 60000);      // convert minutes to ms

  bool periodicRouterCheckinEnabled = (connectionManagerTokenStruct.connectionManagerFlags & PLUGIN_WWAH_CONNECTION_MANAGER_PERIODIC_ROUTER_CHECKIN_ENABLED_STATE);
  sl_zigbee_write_wwah_server_silabs_attribute(endpoint,
                                               ZCL_SL_ROUTER_CHECKIN_ENABLED_ATTRIBUTE_ID,
                                               "router checkin enabled",
                                               (uint8_t *)&periodicRouterCheckinEnabled,
                                               ZCL_BOOLEAN_ATTRIBUTE_TYPE);

  // Save WWAH endpoint for use by Bad Parent Recovery CLI command.
  wwahServerEndpoint = endpoint;
}

void sl_zigbee_af_wwah_connectivity_manager_stack_status_cb(sl_status_t status)
{
  if (connectionManagerTokenStruct.connectionManagerFlags & PLUGIN_WWAH_CONNECTION_MANAGER_ENABLED_STATE) {
    issuedRejoin = false;
    if (status == SL_STATUS_NETWORK_UP) {
      clearState();
    }
  }
  // The BadParentRecovery timer should anyways reset in case of attempted rejoins
  if (connectionManagerTokenStruct.connectionManagerFlags & PLUGIN_WWAH_CONNECTION_MANAGER_BAD_PARENT_RECOVERY) {
    if (status == SL_STATUS_NETWORK_UP
        || status == SL_STATUS_NETWORK_DOWN /* we need to reschule another rejoin attempt*/) {
      sl_zigbee_af_event_set_inactive(&badParentRecoveryEvent);
      sl_zigbee_af_event_set_delay_ms(&badParentRecoveryEvent,
                                      badParentRecoveryRetryPeriod * MILLISECOND_TICKS_PER_MINUTE);
      parentRecoveryRequired = false;
      parentRssiCheckStats = 0;
    }
  }
}

bool sl_zigbee_af_trust_center_keepalive_timeout_cb(void)
{
  sl_zigbee_beacon_classification_params_t param;
  sl_zigbee_get_beacon_classification_params(&param);

  param.beaconClassificationMask &= ~TC_CONNECTIVITY;
  sl_zigbee_set_beacon_classification_params(&param);

  setTrustCenterConnectivity();

  bool keepAliveFailure = wwahConnectionErrorsBitmask
                          & CONNECTION_LOST_TO_HUB_DUE_TO_TC_KEEPALIVE;

  if (!(connectionManagerTokenStruct.connectionManagerFlags & PLUGIN_WWAH_CONNECTION_MANAGER_ENABLED_STATE)) {
    return false;
  } else if (!keepAliveFailure) {
    sl_zigbee_af_sl_wwah_cluster_println("WWAH Connectivity Mgr: lost connection to %s "
                                         "due to %s failures",
                                         "hub",
                                         "trust center keepalive");

    wwahConnectionErrorsBitmask |= CONNECTION_LOST_TO_HUB_DUE_TO_TC_KEEPALIVE;

#ifdef ZLL_COMMISSIONING_COMMON_PRESENT
    if (sl_zigbee_af_zll_touch_link_in_progress()) {
      // Using ZLL for connectivity recovery
      sl_zigbee_af_event_set_delay_ms(&stateMachineEvent,
                                      ZLL_RECOVERY_RETRY_DEFAULT);
      return true;
    }
#endif

    // Kick the state machine only if we're not already trying to restore
    // connectivity
    if (wwahConnectionRecoveryState == STATE_NONE) {
      wwahConnectionRecoveryState = getNextState();
      wwahRunConnectivityRecoveryStateMachine();
    }
  }

  return true;
}

bool sl_zigbee_af_trust_center_keepalive_serverless_is_supported_cb(void)
{
  return true;
}

#if defined(TRUST_CENTER_KEEPALIVE_PRESENT)
bool sl_zigbee_af_trust_center_keepalive_overwrite_default_timing_cb(uint16_t *baseTimeSeconds, uint16_t *jitterTimeSeconds)
{
  if (connectionManagerTokenStruct.connectionManagerFlags & PLUGIN_WWAH_CONNECTION_MANAGER_PERIODIC_ROUTER_CHECKIN_ENABLED_STATE) {
    // Set up TC Keep Alive timing based on Periodic Router Check-In requirements.
    uint16_t checkinIntervalSeconds;

    sl_status_t status = slx_zigbee_token_manager_get_data(COMMON_TOKEN_PLUGIN_WWAH_CONNECTION_MANAGER_PERIODIC_ROUTER_CHECKIN_INTERVAL, (void *)&checkinIntervalSeconds, sizeof(uint16_t));
    if (status != SL_STATUS_OK) {
      sl_zigbee_af_sl_wwah_cluster_println("Failed to get WWAH Connection Manager Periodic Router Checkin Interval, status: 0x%08X", status);
      return false;
    }
    // Note, that periodic router check in supposes no more retries after checking in,
    // meaning that TC Keep Alive has to reach its failure limit (3 consecutive failures)
    // by that time.
    *baseTimeSeconds = (uint16_t)(checkinIntervalSeconds / SL_ZIGBEE_AF_PLUGIN_TRUST_CENTER_KEEPALIVE_FAILURE_LIMIT);
    // Also note, that while Periodic Router Check In does not assume a random jitter
    // added to check-in interval, however, TC Keep Alive code adds that (0-7 seconds).
    *jitterTimeSeconds = 0;

    return true;
  }

  return false;
}
#endif

bool sl_zigbee_af_trust_center_keepalive_serverless_is_enabled_cb(void)
{
  return
    (connectionManagerTokenStruct.connectionManagerFlags
     & PLUGIN_WWAH_CONNECTION_MANAGER_PERIODIC_ROUTER_CHECKIN_ENABLED_STATE)
    ? true : false;
}

sl_status_t sl_zigbee_af_wwah_connectivity_manager_enable_periodic_router_check_ins(uint16_t checkInInterval)
{
#if defined(TRUST_CENTER_KEEPALIVE_PRESENT) && !defined(SL_ZIGBEE_AF_HAS_SLEEPY_NETWORK)
  connectionManagerTokenStruct.connectionManagerFlags |= PLUGIN_WWAH_CONNECTION_MANAGER_PERIODIC_ROUTER_CHECKIN_ENABLED_STATE;
  sl_status_t status = slx_zigbee_token_manager_set_data(COMMON_TOKEN_PLUGIN_CONNECTION_MANAGER_STATE, (void *)&connectionManagerTokenStruct, sizeof(sl_zigbee_af_connection_manager_token_struct_t));
  if (status != SL_STATUS_OK) {
    sl_zigbee_af_sl_wwah_cluster_println("Failed to set WWAH Connection Manager State, status: 0x%08X", status);
    return status;
  }

  status = slx_zigbee_token_manager_set_data(COMMON_TOKEN_PLUGIN_WWAH_CONNECTION_MANAGER_PERIODIC_ROUTER_CHECKIN_INTERVAL, (void *)&checkInInterval, sizeof(uint16_t));
  if (status != SL_STATUS_OK) {
    sl_zigbee_af_sl_wwah_cluster_println("Failed to set WWAH Connection Manager Periodic Router Checkin Interval, status: 0x%08X", status);
    return status;
  }

  bool periodicRouterCheckinEnabled = (connectionManagerTokenStruct.connectionManagerFlags & PLUGIN_WWAH_CONNECTION_MANAGER_PERIODIC_ROUTER_CHECKIN_ENABLED_STATE);
  sl_zigbee_write_wwah_server_silabs_attribute(wwahServerEndpoint,
                                               ZCL_SL_ROUTER_CHECKIN_ENABLED_ATTRIBUTE_ID,
                                               "WWAH rejoin",
                                               (uint8_t *)&periodicRouterCheckinEnabled,
                                               ZCL_BOOLEAN_ATTRIBUTE_TYPE);

  sl_zigbee_af_trust_center_keepalive_enable();
  return SL_STATUS_OK;
#else
  UNUSED_VAR(checkInInterval);
  sl_zigbee_af_sl_wwah_cluster_println("WWAH Periodic Router Checkin is not implemented.");
  return SL_STATUS_FAIL;
#endif
}

sl_status_t sl_zigbee_af_wwah_connectivity_manager_disable_periodic_router_check_ins(void)
{
#if defined(TRUST_CENTER_KEEPALIVE_PRESENT) && !defined(SL_ZIGBEE_AF_HAS_SLEEPY_NETWORK)
  connectionManagerTokenStruct.connectionManagerFlags &= ~PLUGIN_WWAH_CONNECTION_MANAGER_PERIODIC_ROUTER_CHECKIN_ENABLED_STATE;
  sl_status_t status = slx_zigbee_token_manager_set_data(COMMON_TOKEN_PLUGIN_CONNECTION_MANAGER_STATE, (void *)&connectionManagerTokenStruct, sizeof(sl_zigbee_af_connection_manager_token_struct_t)); 
  if (status != SL_STATUS_OK) {
    sl_zigbee_af_sl_wwah_cluster_println("Failed to set WWAH Connection Manager State, status: 0x%08X", status);
    return status;
  }

  // Do not disable Trust Center Keep Alive if the TC has the Trust Center Server
  // Cluster available. Doing this so that we don't unintentionally disable both
  // WWAH Periodic Router Check In *and* Trust Center Keep Alive at the same time.
  if (!sli_zigbee_af_trust_center_keepalive_tc_has_server_cluster()) {
    sl_zigbee_af_trust_center_keepalive_disable();
  }

  bool periodicRouterCheckinEnabled = (connectionManagerTokenStruct.connectionManagerFlags & PLUGIN_WWAH_CONNECTION_MANAGER_PERIODIC_ROUTER_CHECKIN_ENABLED_STATE);
  sl_zigbee_write_wwah_server_silabs_attribute(wwahServerEndpoint,
                                               ZCL_SL_ROUTER_CHECKIN_ENABLED_ATTRIBUTE_ID,
                                               "WWAH rejoin",
                                               (uint8_t *)&periodicRouterCheckinEnabled,
                                               ZCL_BOOLEAN_ATTRIBUTE_TYPE);

  return SL_STATUS_OK;
#else
  sl_zigbee_af_sl_wwah_cluster_println("WWAH Periodic Router Checkin is not implemented.");
  return SL_STATUS_FAIL;
#endif
}

void sl_zigbee_af_poll_control_server_check_in_timeout_cb(void)
{
  bool pollControlFailure = wwahConnectionErrorsBitmask
                            & CONNECTION_LOST_TO_HUB_DUE_TO_POLL_CONTROL_CHECKIN;

  if ((connectionManagerTokenStruct.connectionManagerFlags & PLUGIN_WWAH_CONNECTION_MANAGER_ENABLED_STATE)
      && (!pollControlFailure)) {
    sl_zigbee_af_sl_wwah_cluster_println("WWAH Connectivity Mgr: lost connection to %s "
                                         "due to %s failures",
                                         "hub",
                                         "poll control checkin");

    wwahConnectionErrorsBitmask |=
      CONNECTION_LOST_TO_HUB_DUE_TO_POLL_CONTROL_CHECKIN;

#ifdef ZLL_COMMISSIONING_COMMON_PRESENT
    if (sl_zigbee_af_zll_touch_link_in_progress()) {
      // Using ZLL for connectivity recovery
      sl_zigbee_af_event_set_delay_ms(&stateMachineEvent,
                                      ZLL_RECOVERY_RETRY_DEFAULT);
      return;
    }
#endif

    // Kick the state machine only if we're not already trying to restore
    // connectivity
    if (wwahConnectionRecoveryState == STATE_NONE) {
      wwahConnectionRecoveryState = getNextState();
      wwahRunConnectivityRecoveryStateMachine();
    }
  } else if ((connectionManagerTokenStruct.connectionManagerFlags & PLUGIN_WWAH_CONNECTION_MANAGER_BAD_PARENT_RECOVERY)
             && !(connectionManagerTokenStruct.connectionManagerFlags & PLUGIN_WWAH_CONNECTION_MANAGER_ENABLED_STATE)) {
    // require a parent recovery rejoin and trigger the event to rejoin immediately
    parentRecoveryRequired = true;
    sl_zigbee_af_event_set_active(&badParentRecoveryEvent);
  } else {
    // The connectivity manager isn't enabled, and we aren't performing bad parent recovery,
    // do nothing
  }
  return;
}

static void setTrustCenterConnectivity()
{
  sl_zigbee_beacon_classification_params_t param;
  sl_zigbee_get_beacon_classification_params(&param);
  bool is_hub_connected = (param.beaconClassificationMask
                           & TC_CONNECTIVITY) ? true : false;
  if (is_hub_connected) {
    connectionManagerTokenStruct.connectionManagerFlags |= PLUGIN_WWAH_CONNECTION_MANAGER_TRUST_CENTER_CONNECTIVITY;
  } else {
    connectionManagerTokenStruct.connectionManagerFlags &= ~PLUGIN_WWAH_CONNECTION_MANAGER_TRUST_CENTER_CONNECTIVITY;
  }
  sl_status_t status = slx_zigbee_token_manager_set_data(COMMON_TOKEN_PLUGIN_CONNECTION_MANAGER_STATE, (void *)&connectionManagerTokenStruct, sizeof(sl_zigbee_af_connection_manager_token_struct_t));
  if (status != SL_STATUS_OK) {
    sl_zigbee_af_sl_wwah_cluster_println("Failed to set WWAH Connection Manager State, status: 0x%08X", status);
    return;
  }
}

void sl_zigbee_af_trust_center_keepalive_connectivity_established_cb(void)
{
  sl_zigbee_beacon_classification_params_t param;
  sl_zigbee_get_beacon_classification_params(&param);

  param.beaconClassificationMask |= TC_CONNECTIVITY;
  sl_zigbee_set_beacon_classification_params(&param);
  setTrustCenterConnectivity();
}

bool sl_zigbee_af_end_device_support_lost_parent_connectivity_cb(void)
{
  bool pollControlFailure = wwahConnectionErrorsBitmask
                            & CONNECTION_LOST_TO_PARENT;

  if ((connectionManagerTokenStruct.connectionManagerFlags & PLUGIN_WWAH_CONNECTION_MANAGER_ENABLED_STATE)
      && (!pollControlFailure)) {
    sl_zigbee_af_sl_wwah_cluster_println("WWAH Connectivity Mgr: lost connection to %s",
                                         "parent");

    inFastRejoin = true;
    sl_zigbee_af_event_set_delay_ms(&fastRejoinEvent,
                                    connectionManagerTokenStruct.fastRejoinTimeoutSec * MILLISECOND_TICKS_PER_SECOND);

    wwahConnectionErrorsBitmask |= CONNECTION_LOST_TO_PARENT;

#ifdef ZLL_COMMISSIONING_COMMON_PRESENT
    if (sl_zigbee_af_zll_touch_link_in_progress()) {
      // Using ZLL for connectivity recovery
      sl_zigbee_af_event_set_delay_ms(&stateMachineEvent,
                                      ZLL_RECOVERY_RETRY_DEFAULT);
      return true;
    }
#endif

    // Kick the state machine only if we're not already trying to restore
    // connectivity
    if (wwahConnectionRecoveryState == STATE_NONE) {
      wwahConnectionRecoveryState = getNextState();
      wwahRunConnectivityRecoveryStateMachine();
    }
  } else if ((connectionManagerTokenStruct.connectionManagerFlags & PLUGIN_WWAH_CONNECTION_MANAGER_BAD_PARENT_RECOVERY) && !(connectionManagerTokenStruct.connectionManagerFlags & PLUGIN_WWAH_CONNECTION_MANAGER_ENABLED_STATE)) {
    parentRecoveryRequired = true;
  } else {
    // The connectivity manager isn't enabled, so let fall back
    // to the non WWAH move code.
    return false;
  }
  return true;
}

bool sl_zigbee_af_end_device_support_pre_network_move_cb(void)
{
  // Various plugins try to issue Network Moves when certain events happen. They
  // call a callback which is defined by end-device-move.c, who begins a process
  // of network rejoins. To stop it from issuing rejoins, we simply return true
  // here if either the rejoin algorithm or bad parent recovery are eanbled.
  // There is nothing else we need to catch or do here since our other
  // connectivity callbacks take care of it.
  return ((connectionManagerTokenStruct.connectionManagerFlags & PLUGIN_WWAH_CONNECTION_MANAGER_ENABLED_STATE) || (connectionManagerTokenStruct.connectionManagerFlags & PLUGIN_WWAH_CONNECTION_MANAGER_BAD_PARENT_RECOVERY));
}

//------------------------------------------------------------------------------
// Event Handlers
void stateTransitionEventHandler(sl_zigbee_af_event_t * event)
{
  UNUSED_VAR(event);

  sl_zigbee_af_event_set_inactive(&stateTransitionEvent);
  wwahConnectionRecoveryState = getNextState();
  wwahRunConnectivityRecoveryStateMachine();
}

void longUptimeEventHandler(sl_zigbee_af_event_t * event)
{
  UNUSED_VAR(event);

  sl_zigbee_af_event_set_inactive(&longUptimeEvent);

  sl_zigbee_beacon_classification_params_t param;
  sl_zigbee_get_beacon_classification_params(&param);

  param.beaconClassificationMask |= LONG_UPTIME;
  sl_zigbee_set_beacon_classification_params(&param);
}

void fastRejoinEventHandler(sl_zigbee_af_event_t * event)
{
  UNUSED_VAR(event);

  sl_zigbee_af_event_set_inactive(&fastRejoinEvent);
  numConsecutiveFastRejoinsCompleted++;
  inFastRejoin = false;
  fastRejoinCompleted = true;
}
void stateMachineEventHandler(sl_zigbee_af_event_t * event)
{
  UNUSED_VAR(event);

  sl_zigbee_af_event_set_inactive(&stateMachineEvent);
#ifdef ZLL_COMMISSIONING_COMMON_PRESENT
  if (!sl_zigbee_af_zll_touch_link_in_progress()) {
    // If the touch link fails, we need to recover the connectivity
    if ((wwahConnectionRecoveryState == STATE_NONE)
        && (sl_zigbee_af_network_state() != SL_ZIGBEE_JOINED_NETWORK)) {
      wwahConnectionRecoveryState = getNextState();
      wwahRunConnectivityRecoveryStateMachine();
    }
  } else {
    // Touch link is still in progress, check it next time
    sl_zigbee_af_event_set_delay_ms(&stateMachineEvent,
                                    ZLL_RECOVERY_RETRY_DEFAULT);
  }
#endif
}

//this is handled independent of the state machine
static void badParentRecoveryEventHandler(sl_zigbee_af_event_t * event)
{
  UNUSED_VAR(event);
  sl_zigbee_af_event_set_inactive(&badParentRecoveryEvent);
  if (connectionManagerTokenStruct.connectionManagerFlags & PLUGIN_WWAH_CONNECTION_MANAGER_BAD_PARENT_RECOVERY) {
    sl_status_t status;
    sl_zigbee_node_type_t nodeType;
    status = sl_zigbee_af_get_node_type(&nodeType);

    if ((status == SL_STATUS_OK)
        && (nodeType != SL_ZIGBEE_ROUTER)) {
      if (wwahConnectionRecoveryState == STATE_NONE) {
        ++parentRssiCheckStats;
        if (parentRecoveryRequired) {
          rejoinOnAllChannels(true);
          ++attemptedParentRejoinStats;
          return;
        } else {
          sl_zigbee_beacon_classification_params_t param;
          sl_zigbee_get_beacon_classification_params(&param);
          if (param.beaconClassificationMask
              & BAD_PARENT_CONNECTIVITY ) {
            rejoinOnCurrentChannel(true);
            ++attemptedParentRejoinStats;
            return;
          }
        }
      }
    }
    sl_zigbee_af_event_set_delay_ms(&badParentRecoveryEvent,
                                    badParentRecoveryRetryPeriod * MILLISECOND_TICKS_PER_MINUTE);
  }
}

//------------------------------------------------------------------------------
// Internal functions

#if !defined(ALLOW_REJOINS_WITH_WELL_KNOWN_LINK_KEY)
static bool checkForWellKnownTrustCenterLinkKey(void)
{
  sl_zigbee_sec_man_context_t context;
  sl_zigbee_sec_man_init_context(&context);

  context.core_key_type = SL_ZB_SEC_MAN_KEY_TYPE_TC_LINK;
  sl_status_t status = sl_zigbee_sec_man_check_key_context(&context);

  const sl_zigbee_key_data_t smartEnergyWellKnownTestKey = SE_SECURITY_TEST_LINK_KEY;
  const sl_zigbee_key_data_t zigbeeAlliance09Key = ZIGBEE_PROFILE_INTEROPERABILITY_LINK_KEY;

  if (status != SL_STATUS_OK) {
    // Assume by default we have a well-known key if we failed to retrieve it.
    // This will prevent soliciting a TC rejoin that might expose the network
    // key such that a passive attacker can obtain the key.  Better to be
    // conservative in this circumstance.
    return true;
  }

  if (sl_zigbee_sec_man_compare_key_to_value(&context, (sl_zigbee_sec_man_key_t*)&smartEnergyWellKnownTestKey)
      || sl_zigbee_sec_man_compare_key_to_value(&context, (sl_zigbee_sec_man_key_t*)&zigbeeAlliance09Key)) {
    return true;
  }

  return false;
}
#endif

static uint8_t getNextState(void)
{
  uint8_t nextState;
  sl_status_t status;
  sl_zigbee_node_type_t nodeType;
  bool skipToTcRejoinOnAllChannels = false;

  status = sl_zigbee_af_get_node_type(&nodeType);
  if (SL_STATUS_OK != status) {
    return STATE_NONE;
  }

  // If we're a router, non sleepy, or sleepy with a failed poll control
  // checkin, we skip directly to TC Rejoin on All Channels since we know TC
  // connectivity is lost
  if ((nodeType == SL_ZIGBEE_ROUTER)
      || (nodeType == SL_ZIGBEE_END_DEVICE)
      || (wwahConnectionErrorsBitmask
          & CONNECTION_LOST_TO_HUB_DUE_TO_POLL_CONTROL_CHECKIN)) {
    skipToTcRejoinOnAllChannels = true;
  }

  switch (wwahConnectionRecoveryState) {
    case STATE_NONE:
    case STATE_SECURE_REJOIN_ON_CURRENT_CHANNEL:
      nextState = skipToTcRejoinOnAllChannels ? STATE_TC_REJOIN_ON_ALL_CHANNELS
                  : (wwahConnectionRecoveryState + 1);
      break;

    case STATE_TC_REJOIN_ON_CURRENT_CHANNEL:
      nextState = skipToTcRejoinOnAllChannels ? STATE_TC_REJOIN_ON_ALL_CHANNELS
                  : STATE_AFTER_TC_REJOIN_ON_CURRENT_CHANNEL;
      break;

    case STATE_TC_REJOIN_ON_PREFERRED_CHANNELS:
      nextState = STATE_TC_REJOIN_ON_ALL_CHANNELS;
      break;

    case STATE_TC_REJOIN_ON_ALL_CHANNELS:
      nextState = STATE_WAIT_FOR_TRIGGER;
      break;

    case STATE_WAIT_FOR_TRIGGER:
      // Routers and Non-Sleepy End Devices shall go back to step 3
      if ((nodeType == SL_ZIGBEE_ROUTER) || (nodeType == SL_ZIGBEE_END_DEVICE)) {
        nextState = STATE_TC_REJOIN_ON_CURRENT_CHANNEL;
      } else {
        nextState = STATE_AFTER_WAIT_FOR_TRIGGER;
      }
      break;

    case STATE_SECURE_REJOIN_ON_PREFERRED_CHANNELS:
      nextState = STATE_SECURE_REJOIN_ON_ALL_CHANNELS;
      break;

    case STATE_SECURE_REJOIN_ON_ALL_CHANNELS:
      nextState = STATE_WAIT_FOR_SECOND_TRIGGER;
      break;

    case STATE_WAIT_FOR_SECOND_TRIGGER:
      nextState = skipToTcRejoinOnAllChannels ? STATE_TC_REJOIN_ON_ALL_CHANNELS
                  : STATE_SECURE_REJOIN_ON_CURRENT_CHANNEL;
      break;

    default:
      nextState = STATE_NONE;
      break;
  }

  return nextState;
}

// In two steps of the rejoin algorithm, we delay a while before continuing
// rejoins. This function gets that conditional delay amount
static uint32_t getTriggerStateWaitTimeMs(void)
{
  uint32_t waitTimeMs;
  uint32_t waitTimeBaseSec;
  uint16_t waitTimeJitterSec;
  sl_status_t status;
  sl_zigbee_node_type_t nodeType;

  status = sl_zigbee_get_network_parameters(&nodeType, NULL);
  if (SL_STATUS_OK != status) {
    return STATE_NONE;
  }

  // For Routers and Non-Sleepy End Devices, X seconds will be equal to the
  // Keepalive cluster value or 24 hours if Keepalive cluster is not implemented
  // For Sleepy End devices, if the WWAH rejoin algorithm is enabled then the
  // wait will be determined by the WWAH rejoin algorithm. If the WWAH rejoin
  // algorithm is NOT enabled then the wait will be 15 minutes.
  if ((wwahConnectionRecoveryState == STATE_WAIT_FOR_TRIGGER)
      || (wwahConnectionRecoveryState == STATE_WAIT_FOR_SECOND_TRIGGER)) {
    waitTimeBaseSec = WAIT_TIME_BASE_SEC;
    waitTimeJitterSec = WAIT_TIME_JITTER_SEC;
  } else {
    waitTimeBaseSec = SL_ZIGBEE_AF_PLUGIN_WWAH_CONNECTIVITY_MANAGER_NON_FAST_REJOIN_BACKOFF_MIN * 60;
    waitTimeJitterSec = SL_ZIGBEE_AF_PLUGIN_WWAH_CONNECTIVITY_MANAGER_NON_FAST_REJOIN_BACKOFF_JITTER_SEC;
  }

  // Setup wait time, pay special attention to jitter as that can be zero.
  waitTimeMs = (waitTimeBaseSec * MILLISECOND_TICKS_PER_SECOND);
  waitTimeMs += (waitTimeJitterSec)
                ? ((sl_zigbee_get_pseudo_random_number() % waitTimeJitterSec)
                   * MILLISECOND_TICKS_PER_SECOND)
                : 0;

  return waitTimeMs;
}

static void rejoinOnChannelMask(uint32_t channelMask, bool secureRejoin)
{
  sl_status_t status;
#if !defined(ALLOW_REJOINS_WITH_WELL_KNOWN_LINK_KEY)
  if (checkForWellKnownTrustCenterLinkKey() && !secureRejoin) {
    sl_zigbee_af_sl_wwah_cluster_println("WWAH Connectivity Mgr: Cancelling TC rejoin attempt with well known key");
    return;
  }
#endif

  status = sl_zigbee_find_and_rejoin_network(secureRejoin,
                                             channelMask,
                                             SL_ZIGBEE_AF_REJOIN_DUE_TO_WWAH_CONNECTIVITY_MANAGER,
                                             SL_ZIGBEE_DEVICE_TYPE_UNCHANGED);

  issuedRejoin = true;

  sl_zigbee_af_sl_wwah_cluster_print("WWAH Connectivity Mgr: issued %s rejoin on ",
                                     secureRejoin ? "secure" : "insecure");
  if (channelMask == 0) {
    sl_zigbee_af_sl_wwah_cluster_print("current channel: ");
  } else {
    sl_zigbee_af_sl_wwah_cluster_print("%s channels (mask 0x%08X): ",
                                       (SL_ZIGBEE_ALL_802_15_4_CHANNELS_MASK == channelMask)
                                       ? "all" : "preferred",
                                       channelMask);
  }
  sl_zigbee_af_sl_wwah_cluster_println("0x%02X", status);
}

static void transitionStateAfterDelay(uint32_t delayMs)
{
  sl_zigbee_af_sl_wwah_cluster_println("WWAH Connectivity Mgr: moving state after %d ms",
                                       delayMs);

  sl_zigbee_af_event_set_delay_ms(&stateTransitionEvent,
                                  delayMs);
}

static uint32_t calculateWaitTimeForNextStateMs(void)
{
  sl_status_t status;
  sl_zigbee_node_type_t nodeType;

  sl_zigbee_af_sl_wwah_cluster_println("WWAH Connectivity Mgr: calculating delay until "
                                       "next state (inFastRejoin %s, "
                                       "fastRejoinCompleted %s, "
                                       "numConsecutiveFastRejoinsCompleted %d, "
                                       "backoffBetweenFastRejoinsCompleted %s)",
                                       inFastRejoin ? "Y" : "N",
                                       fastRejoinCompleted ? "Y" : "N",
                                       numConsecutiveFastRejoinsCompleted,
                                       backoffBetweenFastRejoinsCompleted ? "Y" : "N");

  // If we just completed a fast rejoin backoff, we're in fast rejoin again
  if (backoffBetweenFastRejoinsCompleted) {
    inFastRejoin = true;
    sl_zigbee_af_event_set_delay_ms(&fastRejoinEvent,
                                    connectionManagerTokenStruct.fastRejoinTimeoutSec * MILLISECOND_TICKS_PER_SECOND);
    backoffBetweenFastRejoinsCompleted = false;
  }

  status = sl_zigbee_get_network_parameters(&nodeType, NULL);

  // The backoff retry algorithm is designed to help end devices recover and
  // reconnect to the network without issuing too many rejoins which would drain
  // battery. If we're a router or always-on end device, we rejoin at a faster
  // pace
  if ((status != SL_STATUS_OK)
      || (nodeType == SL_ZIGBEE_ROUTER)
      || (nodeType == SL_ZIGBEE_END_DEVICE)) {
    return getTriggerStateWaitTimeMs();
  }

  // At this point on, we're a sleepy
#ifdef SL_ZIGBEE_AF_HAS_SLEEPY_NETWORK
  uint32_t waitTimeMs;
  uint32_t maxWaitTimeMs;

  if (inFastRejoin) {
    waitTimeMs = connectionManagerTokenStruct.durationBetweenEachRejoinSec * MILLISECOND_TICKS_PER_SECOND;
  } else if (fastRejoinCompleted) {
    if (connectionManagerTokenStruct.fastRejoinFirstBackoffTimeSec == 0) {
      return 0; // no backoff/retry
    } else {
      waitTimeMs = connectionManagerTokenStruct.fastRejoinFirstBackoffTimeSec * MILLISECOND_TICKS_PER_SECOND;

      // In between each fast rejoin iteration, we delay by a constantly
      // doubling connectionManagerTokenStruct.fastRejoinFirstBackoffTimeSec seconds
      // When we've done connectionManagerTokenStruct.maxBackoffIter number of fast rejoins, the
      // backoff amount is reset back to the first backoff time
      if (connectionManagerTokenStruct.maxBackoffIter + 1 == numConsecutiveFastRejoinsCompleted) {
        numConsecutiveFastRejoinsCompleted = 1;
      }

      // each backoff between fast rejoins is double the last
      // as numConsecutiveFastRejoinsCompleted will increase 1 after each fast rejoin completed
      if (numConsecutiveFastRejoinsCompleted > 0) {
        waitTimeMs = waitTimeMs << (numConsecutiveFastRejoinsCompleted - 1);
      }

      // Ensure that the backoff does not exceed connectionManagerTokenStruct.maxBackoffTimeSec
      maxWaitTimeMs = connectionManagerTokenStruct.maxBackoffTimeSec * MILLISECOND_TICKS_PER_SECOND;
      if (waitTimeMs > maxWaitTimeMs) {
        waitTimeMs = maxWaitTimeMs;
      }

      // Update state for the next fast rejoin attempt
      fastRejoinCompleted = false;
      // The following is acted upon when the state machine next runs
      backoffBetweenFastRejoinsCompleted = true;
    }
  } else {
    waitTimeMs = (SL_ZIGBEE_AF_PLUGIN_WWAH_CONNECTIVITY_MANAGER_NON_FAST_REJOIN_BACKOFF_MIN
                  * MILLISECOND_TICKS_PER_MINUTE);
    waitTimeMs += ((sl_zigbee_get_pseudo_random_number() % SL_ZIGBEE_AF_PLUGIN_WWAH_CONNECTIVITY_MANAGER_NON_FAST_REJOIN_BACKOFF_JITTER_SEC)
                   * MILLISECOND_TICKS_PER_SECOND);
  }

  return waitTimeMs;
#else
  return 0;
#endif // SL_ZIGBEE_AF_HAS_SLEEPY_NETWORK
}

static void wwahRunConnectivityRecoveryStateMachine(void)
{
  uint32_t waitTimeMs;

  // If the algorithm is disabled or if connectivity is totally restored to both
  // hub and parent, reset state and exit
  if (!(connectionManagerTokenStruct.connectionManagerFlags & PLUGIN_WWAH_CONNECTION_MANAGER_ENABLED_STATE) || (0 == wwahConnectionErrorsBitmask)) {
    wwahConnectionRecoveryState = STATE_NONE;
    return;
  }

  sl_zigbee_af_sl_wwah_cluster_println("WWAH Connectivity Mgr: running state %s "
                                       "(connection error mask 0x%02X, fast rejoin: %s)",
                                       wwahStateStrings[wwahConnectionRecoveryState],
                                       wwahConnectionErrorsBitmask,
                                       inFastRejoin ? "Y" : "N");

  switch (wwahConnectionRecoveryState) {
    case STATE_NONE:
      break;

    case STATE_SECURE_REJOIN_ON_CURRENT_CHANNEL:
      rejoinOnCurrentChannel(true); // secure rejoin
      break;

    case STATE_TC_REJOIN_ON_CURRENT_CHANNEL:
      rejoinOnCurrentChannel(false); // insecure rejoin
      break;

    case STATE_TC_REJOIN_ON_PREFERRED_CHANNELS:
      rejoinOnPreferredChannels(false); // insecure rejoin
      break;

    case STATE_TC_REJOIN_ON_ALL_CHANNELS:
      rejoinOnAllChannels(false); // insecure rejoin
      break;

    case STATE_WAIT_FOR_TRIGGER:
#ifdef ALLOW_TRIGGER_STATE_SEND_REJOIN
      // EMZIGBEE-4387: This rejoin attempt is to accommodate the discrepancy between
      // "17-01066-029-Zigbee_WWAH_Requirements" and "WWAH Test Cases - 191002"
      // for the trigger waiting states. The "16-02828-005-PRO-BDB-Specification-v1.0-Errata"
      // removes the trigger waiting states for the rejoin algorithm. If the future
      // "Zigbee_WWAH_Requirements" adopts the new rejoin algorithm we should remove this.
      rejoinOnAllChannels(false); // insecure rejoin
#endif
      break;

    case STATE_SECURE_REJOIN_ON_PREFERRED_CHANNELS:
      rejoinOnPreferredChannels(true); // secure rejoin
      break;

    case STATE_SECURE_REJOIN_ON_ALL_CHANNELS:
      rejoinOnAllChannels(true);  // secure rejoin
      break;

    case STATE_WAIT_FOR_SECOND_TRIGGER:
#ifdef ALLOW_TRIGGER_STATE_SEND_REJOIN
      // EMZIGBEE-4387: This rejoin attempt is to accommodate the discrepancy between
      // "17-01066-029-Zigbee_WWAH_Requirements" and "WWAH Test Cases - 191002"
      // for the trigger waiting states. The "16-02828-005-PRO-BDB-Specification-v1.0-Errata"
      // removes the trigger waiting states for the rejoin algorithm. If the future
      // "Zigbee_WWAH_Requirements" adopts the new rejoin algorithm we should remove this.
      rejoinOnAllChannels(true);  // secure rejoin
#endif
      break;

    default:
      break;
  }

  waitTimeMs = calculateWaitTimeForNextStateMs();
  if (waitTimeMs != 0) {
    transitionStateAfterDelay(waitTimeMs);
  }
}

void wwahConnectivityManagerPrintInfo(void)
{
  bool parentConnectivityFailure = wwahConnectionErrorsBitmask
                                   & CONNECTION_LOST_TO_PARENT;
  bool tcKeepAliveFailure = wwahConnectionErrorsBitmask
                            & CONNECTION_LOST_TO_HUB_DUE_TO_TC_KEEPALIVE;
  bool pollControlFailure = wwahConnectionErrorsBitmask
                            & CONNECTION_LOST_TO_HUB_DUE_TO_POLL_CONTROL_CHECKIN;
  bool hubConnectivityFailure = tcKeepAliveFailure || pollControlFailure;
  sl_zigbee_beacon_classification_params_t param;
  sl_zigbee_get_beacon_classification_params(&param);

  sl_zigbee_af_sl_wwah_cluster_println("WWAH Connectivity Manager Info");
  sl_zigbee_af_sl_wwah_cluster_println("------------------------------------------------"
                                       "--------------------");
  sl_zigbee_af_sl_wwah_cluster_println("Enabled:                         %s",
                                       (connectionManagerTokenStruct.connectionManagerFlags & PLUGIN_WWAH_CONNECTION_MANAGER_ENABLED_STATE) ? "Yes" : "No");
  sl_zigbee_af_sl_wwah_cluster_println("Connection Error Mask:           0x%02X",
                                       wwahConnectionErrorsBitmask);
  sl_zigbee_af_sl_wwah_cluster_println("  Parent Connection Lost:        %s",
                                       parentConnectivityFailure ? "Yes" : "No");
  sl_zigbee_af_sl_wwah_cluster_println("  Trust Center Connectivity:     %s",
                                       connectionManagerTokenStruct.connectionManagerFlags & PLUGIN_WWAH_CONNECTION_MANAGER_TRUST_CENTER_CONNECTIVITY ? "Yes" : "No");
  sl_zigbee_af_sl_wwah_cluster_println("  Hub Connection Lost:           %s",
                                       hubConnectivityFailure ? "Yes" : "No");
  sl_zigbee_af_sl_wwah_cluster_println("    TC Keepalive Failure:        %s",
                                       tcKeepAliveFailure ? "Yes" : "No");
  sl_zigbee_af_sl_wwah_cluster_println("    Poll Control Server Failure: %s",
                                       pollControlFailure ? "Yes" : "No");
  sl_zigbee_af_sl_wwah_cluster_println("State:                           %s",
                                       wwahStateStrings[wwahConnectionRecoveryState]);
  sl_zigbee_af_sl_wwah_cluster_println("  Issued Rejoin:                 %s",
                                       issuedRejoin ? "Yes" : "No");
  sl_zigbee_af_sl_wwah_cluster_println("  In Fast Rejoin:                %s",
                                       inFastRejoin ? "Yes" : "No");
  sl_zigbee_af_sl_wwah_cluster_println("  Consecutive Fast Rejoins Done: %d",
                                       numConsecutiveFastRejoinsCompleted);
  sl_zigbee_af_sl_wwah_cluster_println("  Long Uptime:                   %s",
                                       (param.beaconClassificationMask & LONG_UPTIME)  ? "Yes" : "No");
  sl_zigbee_af_sl_wwah_cluster_println("Bad Parent Recovery Enabled:     %s",
                                       (connectionManagerTokenStruct.connectionManagerFlags & PLUGIN_WWAH_CONNECTION_MANAGER_BAD_PARENT_RECOVERY) ? "Yes" : "No");
  sl_zigbee_af_sl_wwah_cluster_println("  Quality checks:                %d",
                                       parentRssiCheckStats);
  sl_zigbee_af_sl_wwah_cluster_println("  Measurement period(Min):       %d",
                                       badParentRecoveryRetryPeriod);
  sl_zigbee_af_sl_wwah_cluster_println("  Attempted rejoins:             %d",
                                       attemptedParentRejoinStats);
  sl_zigbee_af_sl_wwah_cluster_println("minRssiForReceivingPackets:      %d",
                                       param.minRssiForReceivingPkts);
}

void wwahConnectivityManagerShow(void)
{
  sl_zigbee_af_sl_wwah_cluster_println("Fast Rejoin Timeout:             %d s",
                                       connectionManagerTokenStruct.fastRejoinTimeoutSec);
  sl_zigbee_af_sl_wwah_cluster_println("Duration Between Each Rejoin:    %d s",
                                       connectionManagerTokenStruct.durationBetweenEachRejoinSec);
  sl_zigbee_af_sl_wwah_cluster_println("Fast Rejoin First Backoff Time:  %d s",
                                       connectionManagerTokenStruct.fastRejoinFirstBackoffTimeSec);
  sl_zigbee_af_sl_wwah_cluster_println("Max Backoff Timeout:             %d s",
                                       connectionManagerTokenStruct.maxBackoffTimeSec);
  sl_zigbee_af_sl_wwah_cluster_println("Max Backoff Iterations:          %d",
                                       connectionManagerTokenStruct.maxBackoffIter);
}

static void clearState(void)
{
  wwahConnectionRecoveryState = STATE_NONE;
  wwahConnectionErrorsBitmask = 0;
  issuedRejoin = false;
  inFastRejoin = false;
  fastRejoinCompleted = false;
  backoffBetweenFastRejoinsCompleted = false;
  numConsecutiveFastRejoinsCompleted = 0;
  sl_zigbee_af_event_set_inactive(&stateTransitionEvent);
  sl_zigbee_af_event_set_inactive(&fastRejoinEvent);
}
//------------------------------------------------------------------------------
// Public APIs

bool sl_zigbee_af_wwah_connectivity_manager_is_rejoin_algorithm_enabled(void)
{
  return connectionManagerTokenStruct.connectionManagerFlags & PLUGIN_WWAH_CONNECTION_MANAGER_ENABLED_STATE;
}

void sl_zigbee_af_wwah_connectivity_manager_enable_rejoin_algorithm(uint8_t endpoint)
{
  if (!(connectionManagerTokenStruct.connectionManagerFlags & PLUGIN_WWAH_CONNECTION_MANAGER_ENABLED_STATE)) {
    parentRecoveryRequired = false;
    connectionManagerTokenStruct.connectionManagerFlags |= PLUGIN_WWAH_CONNECTION_MANAGER_ENABLED_STATE;
    sl_status_t status = slx_zigbee_token_manager_set_data(COMMON_TOKEN_PLUGIN_CONNECTION_MANAGER_STATE, (void *)&connectionManagerTokenStruct, sizeof(sl_zigbee_af_connection_manager_token_struct_t));
    if (status != SL_STATUS_OK) {
      sl_zigbee_af_sl_wwah_cluster_println("Failed to set WWAH Connection Manager State, status: 0x%08X", status);
      return;
    }
    bool rejoinAlgorithmEnabled = connectionManagerTokenStruct.connectionManagerFlags & PLUGIN_WWAH_CONNECTION_MANAGER_ENABLED_STATE;
    sl_zigbee_write_wwah_server_silabs_attribute(endpoint,
                                                 ZCL_SL_WWAH_REJOIN_ENABLED_ATTRIBUTE_ID,
                                                 "WWAH rejoin",
                                                 (uint8_t *)&rejoinAlgorithmEnabled,
                                                 ZCL_BOOLEAN_ATTRIBUTE_TYPE);
  }
}

void sl_zigbee_af_wwah_connectivity_manager_disable_rejoin_algorithm(uint8_t endpoint)
{
  if (connectionManagerTokenStruct.connectionManagerFlags & PLUGIN_WWAH_CONNECTION_MANAGER_ENABLED_STATE) {
    connectionManagerTokenStruct.connectionManagerFlags &= ~PLUGIN_WWAH_CONNECTION_MANAGER_ENABLED_STATE;
    sl_status_t status = slx_zigbee_token_manager_set_data(COMMON_TOKEN_PLUGIN_CONNECTION_MANAGER_STATE, (void *)&connectionManagerTokenStruct, sizeof(sl_zigbee_af_connection_manager_token_struct_t));
    if (status != SL_STATUS_OK) {
      sl_zigbee_af_sl_wwah_cluster_println("Failed to set WWAH Connection Manager State, status: 0x%08X", status);
      return;
    }
    clearState();
    bool rejoinAlgorithmEnabled = connectionManagerTokenStruct.connectionManagerFlags & PLUGIN_WWAH_CONNECTION_MANAGER_ENABLED_STATE;
    sl_zigbee_write_wwah_server_silabs_attribute(endpoint,
                                                 ZCL_SL_WWAH_REJOIN_ENABLED_ATTRIBUTE_ID,
                                                 "WWAH rejoin",
                                                 (uint8_t *)&rejoinAlgorithmEnabled,
                                                 ZCL_BOOLEAN_ATTRIBUTE_TYPE);
  }
}

// Invoked by CLI command handler for Bad Parent Recovery.
void sl_zigbee_af_wwah_connectivity_manager_enable_bad_parent_recovery(uint16_t badParentRejoinPeriod)
{
  sli_zigbee_af_wwah_connectivity_manager_enable_bad_parent_recovery(wwahServerEndpoint, badParentRejoinPeriod);
}

// Common handler for ZCL and CLI that conveys endpoint.
void sli_zigbee_af_wwah_connectivity_manager_enable_bad_parent_recovery(uint8_t endpoint, uint16_t badParentRejoinPeriod)
{
  if (!(connectionManagerTokenStruct.connectionManagerFlags & PLUGIN_WWAH_CONNECTION_MANAGER_BAD_PARENT_RECOVERY)) {
    parentRecoveryRequired = false;
    parentRssiCheckStats = 0;
    sl_status_t status;
    sl_zigbee_node_type_t nodeType;
    status = sl_zigbee_get_network_parameters(&nodeType, NULL);

    if ((status == SL_STATUS_OK)
        && (nodeType != SL_ZIGBEE_ROUTER)) {
      connectionManagerTokenStruct.connectionManagerFlags |= PLUGIN_WWAH_CONNECTION_MANAGER_BAD_PARENT_RECOVERY;
      sl_status_t status = slx_zigbee_token_manager_set_data(COMMON_TOKEN_PLUGIN_CONNECTION_MANAGER_STATE, (void *)&connectionManagerTokenStruct, sizeof(sl_zigbee_af_connection_manager_token_struct_t));
      if (status != SL_STATUS_OK) {
        sl_zigbee_af_sl_wwah_cluster_println("Failed to set WWAH Connection Manager State, status: 0x%08X", status);
        return;
      }
      uint8_t badParentRecoveryEnabled = 1; // ZCL bool true
      sl_zigbee_write_wwah_server_silabs_attribute(endpoint,
                                                   ZCL_SL_WWAH_BAD_PARENT_RECOVERY_ENABLED_ATTRIBUTE_ID,
                                                   "WWAH bad parent",
                                                   (uint8_t *)&badParentRecoveryEnabled,
                                                   ZCL_BOOLEAN_ATTRIBUTE_TYPE);
      if (badParentRejoinPeriod) {
        badParentRecoveryRetryPeriod = badParentRejoinPeriod;  //in min
      } else {
        badParentRecoveryRetryPeriod = BAD_PARENT_RECOVERY_RETRY_DEFAULT;
      }

      uint32_t delayMs = badParentRecoveryRetryPeriod * MILLISECOND_TICKS_PER_MINUTE;
      status = SL_STATUS_OK;
      if (delayMs <= SL_ZIGBEE_MAX_EVENT_DELAY_MS) {
        sl_zigbee_af_event_set_delay_ms(&badParentRecoveryEvent, delayMs);
      } else {
        status = SL_STATUS_INVALID_PARAMETER;
      }
      if (status != SL_STATUS_OK) {
        sl_zigbee_af_sl_wwah_cluster_println("WWAH Connectivity Mgr: retry period is too large, falling back on default");
        badParentRecoveryRetryPeriod = BAD_PARENT_RECOVERY_RETRY_DEFAULT;
      } else {
        // MISRA
      }
      sl_zigbee_af_sl_wwah_cluster_println("WWAH Connectivity Mgr: Enabled bad parent recovery checking parent quality every %d minutes", badParentRecoveryRetryPeriod);
    }
  } else {
    sl_zigbee_af_sl_wwah_cluster_println("WWAH Connectivity Mgr: bad parent recovery already enabled. Checking parent quality every %d minutes", badParentRecoveryRetryPeriod);
  }
}

// Invoked by CLI command handler for Bad Parent Recovery.
void sl_zigbee_af_wwah_connectivity_manager_disable_bad_parent_recovery(void)
{
  sli_zigbee_af_wwah_connectivity_manager_disable_bad_parent_recovery(wwahServerEndpoint);
}

// Common handler for ZCL and CLI that conveys endpoint.
void sli_zigbee_af_wwah_connectivity_manager_disable_bad_parent_recovery(uint8_t endpoint)
{
  if (connectionManagerTokenStruct.connectionManagerFlags & PLUGIN_WWAH_CONNECTION_MANAGER_BAD_PARENT_RECOVERY) {
    parentRecoveryRequired = false;
    connectionManagerTokenStruct.connectionManagerFlags &= ~PLUGIN_WWAH_CONNECTION_MANAGER_BAD_PARENT_RECOVERY;
    sl_status_t status = slx_zigbee_token_manager_set_data(COMMON_TOKEN_PLUGIN_CONNECTION_MANAGER_STATE, (void *)&connectionManagerTokenStruct, sizeof(sl_zigbee_af_connection_manager_token_struct_t));
    if (status != SL_STATUS_OK) {
      sl_zigbee_af_sl_wwah_cluster_println("Failed to set WWAH Connection Manager State, status: 0x%08X", status);
      return;
    }
    uint8_t badParentRecoveryEnabled = 0; // ZCL bool false
    sl_zigbee_write_wwah_server_silabs_attribute(endpoint,
                                                 ZCL_SL_WWAH_BAD_PARENT_RECOVERY_ENABLED_ATTRIBUTE_ID,
                                                 "WWAH bad parent",
                                                 (uint8_t *)&badParentRecoveryEnabled,
                                                 ZCL_BOOLEAN_ATTRIBUTE_TYPE);
    sl_zigbee_af_event_set_inactive(&badParentRecoveryEvent);
    sl_zigbee_af_sl_wwah_cluster_println("WWAH Connectivity Mgr: Disabled bad parent recovery");
    attemptedParentRejoinStats = 0;
    parentRssiCheckStats = 0;
  } else {
    sl_zigbee_af_sl_wwah_cluster_println("WWAH Connectivity Mgr: bad parent recovery already disabled.");
  }
}

void sl_zigbee_af_wwah_connectivity_manager_set_rejoin_parameters(uint16_t fastRejoinTimeoutSeconds,
                                                                  uint16_t durationBetweenRejoinsSeconds,
                                                                  uint16_t fastRejoinFirstBackoffSeconds,
                                                                  uint16_t maxBackoffTimeSeconds,
                                                                  uint16_t maxBackoffIterations)
{
  connectionManagerTokenStruct.fastRejoinTimeoutSec = fastRejoinTimeoutSeconds;
  sl_status_t status = slx_zigbee_token_manager_set_data(COMMON_TOKEN_PLUGIN_CONNECTION_MANAGER_STATE, (void *)&connectionManagerTokenStruct, sizeof(sl_zigbee_af_connection_manager_token_struct_t));
  if (status != SL_STATUS_OK) {
    sl_zigbee_af_sl_wwah_cluster_println("Failed to set WWAH Connection Manager State, status: 0x%08X", status);
    return;
  }

  connectionManagerTokenStruct.durationBetweenEachRejoinSec = durationBetweenRejoinsSeconds;
  status = slx_zigbee_token_manager_set_data(COMMON_TOKEN_PLUGIN_CONNECTION_MANAGER_STATE, (void *)&connectionManagerTokenStruct, sizeof(sl_zigbee_af_connection_manager_token_struct_t));
  if (status != SL_STATUS_OK) {
    sl_zigbee_af_sl_wwah_cluster_println("Failed to set WWAH Connection Manager State, status: 0x%08X", status);
    return;
  }

  connectionManagerTokenStruct.fastRejoinFirstBackoffTimeSec = fastRejoinFirstBackoffSeconds;
  status = slx_zigbee_token_manager_set_data(COMMON_TOKEN_PLUGIN_CONNECTION_MANAGER_STATE, (void *)&connectionManagerTokenStruct, sizeof(sl_zigbee_af_connection_manager_token_struct_t));
  if (status != SL_STATUS_OK) {
    sl_zigbee_af_sl_wwah_cluster_println("Failed to set WWAH Connection Manager State, status: 0x%08X", status);
    return;
  }

  connectionManagerTokenStruct.maxBackoffTimeSec = maxBackoffTimeSeconds;
  status = slx_zigbee_token_manager_set_data(COMMON_TOKEN_PLUGIN_CONNECTION_MANAGER_STATE, (void *)&connectionManagerTokenStruct, sizeof(sl_zigbee_af_connection_manager_token_struct_t));
  if (status != SL_STATUS_OK) {
    sl_zigbee_af_sl_wwah_cluster_println("Failed to set WWAH Connection Manager State, status: 0x%08X", status);
    return;
  }

  connectionManagerTokenStruct.maxBackoffIter = maxBackoffIterations;
  status = slx_zigbee_token_manager_set_data(COMMON_TOKEN_PLUGIN_CONNECTION_MANAGER_STATE, (void *)&connectionManagerTokenStruct, sizeof(sl_zigbee_af_connection_manager_token_struct_t));
  if (status != SL_STATUS_OK) {
    sl_zigbee_af_sl_wwah_cluster_println("Failed to set WWAH Connection Manager State, status: 0x%08X", status);
    return;
  }
}
