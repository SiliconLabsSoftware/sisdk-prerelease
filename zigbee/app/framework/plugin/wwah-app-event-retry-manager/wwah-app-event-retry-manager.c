/***************************************************************************//**
 * @file
 * @brief Routines for the WWAH App Event Retry Manager plugin.
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
#include "wwah-app-event-retry-manager.h"
#include "wwah-server-silabs.h"
#include "sl_zigbee_token.h"
#include "stack/config/sl_zigbee_token_defines.h"
#include "wwah-app-event-retry-manager-tokens.h"

#ifdef SL_ZIGBEE_SCRIPTED_TEST
#include "app/framework/plugin/wwah-app-event-retry-manager/wwah-app-event-retry-manager-test.h"
#endif
// static uint8_t queueSize;
static uint8_t appEventRetryEnabled;
static uint8_t firstBackoffTimeSeconds;
static uint8_t backoffSeqCommonRatio;
static uint32_t maxBackoffTimeSeconds;
static uint8_t maxRedeliveryAttempts;
#ifdef SL_COMPONENT_CATALOG_PRESENT
#include "sl_component_catalog.h"
#endif // SL_COMPONENT_CATALOG_PRESENT
#ifdef SL_CATALOG_ZIGBEE_WWAH_APP_EVENT_RETRY_MANAGER_PRESENT
#include "wwah-app-event-retry-manager-config.h"
#endif
#ifdef SL_CATALOG_ZIGBEE_IAS_ZONE_SERVER_PRESENT
#include "ias-zone-server.h"
#include "ias-zone-server-config.h"
#endif // SL_CATALOG_ZIGBEE_IAS_ZONE_SERVER_PRESENT

#if !(defined(SL_CATALOG_ZIGBEE_CRITICAL_MESSAGE_QUEUE_PRESENT) \
  || (defined(SL_CATALOG_ZIGBEE_IAS_ZONE_SERVER_PRESENT)        \
  && (SL_ZIGBEE_AF_PLUGIN_IAS_ZONE_SERVER_ENABLE_QUEUE == 1))   \
  || defined(SL_ZIGBEE_SCRIPTED_TEST))
#error WWAH App Event Retry Manager error: Either the Critical Message Qeueue plugin or the \
  IAS Zone Server plugin with the Status Queue needs to be enabled.
#endif

static void sli_zigbee_af_wwah_app_event_retry_manager_set_enabled(uint8_t endpoint, bool enabled)
{
  appEventRetryEnabled = (uint8_t)enabled;
  sl_status_t status = slx_zigbee_token_manager_set_data(COMMON_TOKEN_PLUGIN_WWAH_APP_EVENT_RETRY_MANAGER_ENABLED,
                                  (void *)&appEventRetryEnabled,
                                  sizeof(uint8_t));
  if (status != SL_STATUS_OK) {
    sl_zigbee_af_core_println("Failed to set WWAH App Event Retry Manager Enabled, status: 0x%08X", status);
    return;
  }
  sl_zigbee_write_wwah_server_silabs_attribute(endpoint,
                                               ZCL_SL_WWAH_APP_EVENT_RETRY_ENABLED_ATTRIBUTE_ID,
                                               "WWAH App Event Retry",
                                               (uint8_t *)&appEventRetryEnabled,
                                               ZCL_BOOLEAN_ATTRIBUTE_TYPE);
}

static bool sli_zigbee_af_wwah_app_event_retry_manager_get_enabled(void)
{
  sl_status_t status = slx_zigbee_token_manager_get_data(COMMON_TOKEN_PLUGIN_WWAH_APP_EVENT_RETRY_MANAGER_ENABLED,
                                  (void *)&appEventRetryEnabled,
                                  sizeof(uint8_t));
  if (status != SL_STATUS_OK) {
    sl_zigbee_af_core_println("Failed to get WWAH App Event Retry Manager Enabled, status: 0x%08X", status);
    return false;
  }
  return appEventRetryEnabled;  
}

static void sli_zigbee_af_wwah_app_event_retry_manager_get_param_tokens(void)
{
  sl_status_t status = slx_zigbee_token_manager_get_data(COMMON_TOKEN_PLUGIN_WWAH_APP_EVENT_RETRY_MANAGER_FIRST_BACKOFF_TIME_S,
                                  (void *)&firstBackoffTimeSeconds,
                                  sizeof(uint8_t));
  if (status != SL_STATUS_OK) {
    sl_zigbee_af_core_println("Failed to get WWAH App Event Retry Manager First Backoff Time, status: 0x%08X", status);
    return;
  } 

  status = slx_zigbee_token_manager_get_data(COMMON_TOKEN_PLUGIN_WWAH_APP_EVENT_RETRY_MANAGER_BACKOFF_COMMON_RATIO,
                                  (void *)&backoffSeqCommonRatio,
                                  sizeof(uint8_t));
  if (status != SL_STATUS_OK) {
    sl_zigbee_af_core_println("Failed to get WWAH App Event Retry Manager Backoff Common Ratio, status: 0x%08X", status);
    return;
  }

  status = slx_zigbee_token_manager_get_data(COMMON_TOKEN_PLUGIN_WWAH_APP_EVENT_RETRY_MANAGER_MAX_BACKOFF_TIME_S,
                                  (void *)&maxBackoffTimeSeconds,
                                  sizeof(uint32_t));
  if (status != SL_STATUS_OK) {
    sl_zigbee_af_core_println("Failed to get WWAH App Event Retry Manager Max Backoff Time, status: 0x%08X", status);
    return;
  }

  status = slx_zigbee_token_manager_get_data(COMMON_TOKEN_PLUGIN_WWAH_APP_EVENT_RETRY_MANAGER_MAX_REDELIVERY_ATTEMPTS,
                                  (void *)&maxRedeliveryAttempts,
                                  sizeof(uint8_t));
  if (status != SL_STATUS_OK) {
    sl_zigbee_af_core_println("Failed to get WWAH App Event Retry Manager Max Redelivery Attempts, status: 0x%08X", status);
    return;
  }
}

static void sli_zigbee_af_wwah_app_event_retry_manager_set_param_tokens(void)
{
  sl_status_t status = slx_zigbee_token_manager_set_data(COMMON_TOKEN_PLUGIN_WWAH_APP_EVENT_RETRY_MANAGER_FIRST_BACKOFF_TIME_S, (void *)&firstBackoffTimeSeconds, sizeof(uint8_t));
  if (status != SL_STATUS_OK) {
    sl_zigbee_af_core_println("Failed to set WWAH App Event Retry Manager First Backoff Time, status: 0x%08X", status);
    return;
  }

  status = slx_zigbee_token_manager_set_data(COMMON_TOKEN_PLUGIN_WWAH_APP_EVENT_RETRY_MANAGER_BACKOFF_COMMON_RATIO, (void *)&backoffSeqCommonRatio, sizeof(uint8_t));
  if (status != SL_STATUS_OK) {
    sl_zigbee_af_core_println("Failed to set WWAH App Event Retry Manager Backoff Common Ratio, status: 0x%08X", status);
    return;
  }
  status = slx_zigbee_token_manager_set_data(COMMON_TOKEN_PLUGIN_WWAH_APP_EVENT_RETRY_MANAGER_MAX_BACKOFF_TIME_S, (void *)&maxBackoffTimeSeconds, sizeof(uint32_t));
  if (status != SL_STATUS_OK) {
    sl_zigbee_af_core_println("Failed to set WWAH App Event Retry Manager Max Backoff Time, status: 0x%08X", status);
    return;
  }

  status = slx_zigbee_token_manager_set_data(COMMON_TOKEN_PLUGIN_WWAH_APP_EVENT_RETRY_MANAGER_MAX_REDELIVERY_ATTEMPTS, (void *)&maxRedeliveryAttempts, sizeof(uint8_t));
  if (status != SL_STATUS_OK) {
    sl_zigbee_af_core_println("Failed to set WWAH App Event Retry Manager Max Redelivery Attempts, status: 0x%08X", status);
    return;
  }
}

sl_status_t sl_zigbee_af_wwah_app_event_retry_manager_enable_with_params(uint8_t endpoint,
                                                                         uint8_t inFirstBackoffTimeSeconds,
                                                                         uint8_t inBackoffSeqCommonRatio,
                                                                         uint32_t inMaxBackoffTimeSeconds,
                                                                         uint8_t inMaxRedeliveryAttempts)
{
  sl_status_t status;

  // Basic sanity check on input parameter values.
  if (!(inFirstBackoffTimeSeconds)
      || (!inBackoffSeqCommonRatio)
      || (inMaxBackoffTimeSeconds < inFirstBackoffTimeSeconds)
      || (inMaxBackoffTimeSeconds > SL_ZIGBEE_AF_PLUGIN_WWAH_APP_EVENT_RETRY_ABS_MAX_BACKOFF_TIME_SEC)
      || (!inMaxRedeliveryAttempts) ) {
    return SL_STATUS_INVALID_PARAMETER;
  }

  // Test if underlying plugin can receive the backoff parameters.
  status = sl_zigbee_af_wwah_app_event_retry_manager_config_backoff_params_cb(inFirstBackoffTimeSeconds,
                                                                              inBackoffSeqCommonRatio,
                                                                              inMaxBackoffTimeSeconds,
                                                                              inMaxRedeliveryAttempts);
  if (status != SL_STATUS_OK) {
    return status;
  }

  // Set the parameters and persist them.
  firstBackoffTimeSeconds = inFirstBackoffTimeSeconds;
  backoffSeqCommonRatio = inBackoffSeqCommonRatio;
  maxBackoffTimeSeconds = inMaxBackoffTimeSeconds;
  maxRedeliveryAttempts = inMaxRedeliveryAttempts;
  sli_zigbee_af_wwah_app_event_retry_manager_set_enabled(endpoint, true);
  sli_zigbee_af_wwah_app_event_retry_manager_set_param_tokens();

  return SL_STATUS_OK;
}

void sl_zigbee_af_wwah_app_event_retry_manager_disable(uint8_t endpoint)
{
  // Set the backoff values of the corresponding queue provider back to default
  // and disable the functionality.
  sl_zigbee_af_wwah_app_event_retry_manager_set_backoff_params_to_default();
  sli_zigbee_af_wwah_app_event_retry_manager_set_enabled(endpoint, false);
}

sl_status_t sl_zigbee_af_wwah_app_event_retry_manager_token_init(void)
{
  sl_status_t status = SL_STATUS_OK;
  uint8_t tokPluginWWAHAppEventRetryManagerEnabledDefault = TOKEN_PLUGIN_WWAH_APP_EVENT_RETRY_MANAGER_ENABLED_DEFAULT;
  status = sl_zigbee_initialize_basic_token(COMMON_TOKEN_PLUGIN_WWAH_APP_EVENT_RETRY_MANAGER_ENABLED, &tokPluginWWAHAppEventRetryManagerEnabledDefault, sizeof(uint8_t));
  uint8_t tokPluginWWAHAppEventRetryManagerFirstBackoffTimeSDefault = TOKEN_PLUGIN_WWAH_APP_EVENT_RETRY_MANAGER_FIRST_BACKOFF_TIME_S_DEFAULT;
  status = sl_zigbee_initialize_basic_token(COMMON_TOKEN_PLUGIN_WWAH_APP_EVENT_RETRY_MANAGER_FIRST_BACKOFF_TIME_S, &tokPluginWWAHAppEventRetryManagerFirstBackoffTimeSDefault, sizeof(uint8_t));
  uint8_t tokPluginWWAHAppEventRetryManagerBackoffCommonRatioDefault = TOKEN_PLUGIN_WWAH_APP_EVENT_RETRY_MANAGER_BACKOFF_COMMON_RATIO_DEFAULT;
  status = sl_zigbee_initialize_basic_token(COMMON_TOKEN_PLUGIN_WWAH_APP_EVENT_RETRY_MANAGER_BACKOFF_COMMON_RATIO, &tokPluginWWAHAppEventRetryManagerBackoffCommonRatioDefault, sizeof(uint8_t));
  uint32_t tokPluginWWAHAppEventRetryManagerMaxBackoffTimeSDefault = TOKEN_PLUGIN_WWAH_APP_EVENT_RETRY_MANAGER_MAX_BACKOFF_TIME_S_DEFAULT;
  status = sl_zigbee_initialize_basic_token(COMMON_TOKEN_PLUGIN_WWAH_APP_EVENT_RETRY_MANAGER_MAX_BACKOFF_TIME_S, &tokPluginWWAHAppEventRetryManagerMaxBackoffTimeSDefault, sizeof(uint32_t));
  uint8_t tokPluginWwAHAppEventRetryManagerMaxRedeliveryAttemptsDefault = TOKEN_PLUGIN_WWAH_APP_EVENT_RETRY_MANAGER_MAX_REDELIVERY_ATTEMPTS_DEFAULT;
  status = sl_zigbee_initialize_basic_token(COMMON_TOKEN_PLUGIN_WWAH_APP_EVENT_RETRY_MANAGER_MAX_REDELIVERY_ATTEMPTS, &tokPluginWwAHAppEventRetryManagerMaxRedeliveryAttemptsDefault, sizeof(uint8_t));
  return status;
}

void sl_zigbee_af_wwah_app_event_retry_manager_init_cb(uint8_t endpoint)
{
  // Get persisted parmeter values.
  sli_zigbee_af_wwah_app_event_retry_manager_get_enabled();

  // Set WWAH app event retry enabled and queue size attributes.
  sl_zigbee_write_wwah_server_silabs_attribute(endpoint,
                                               ZCL_SL_WWAH_APP_EVENT_RETRY_ENABLED_ATTRIBUTE_ID,
                                               "app event retry enabled",
                                               &appEventRetryEnabled,
                                               ZCL_INT8U_ATTRIBUTE_TYPE);
  uint8_t queueSize = (uint8_t)SL_ZIGBEE_AF_PLUGIN_WWAH_APP_EVENT_RETRY_MANAGER_QUEUE_SIZE;
  sl_zigbee_write_wwah_server_silabs_attribute(endpoint,
                                               ZCL_SL_WWAH_APP_EVENT_RETRY_QUEUE_SIZE_ATTRIBUTE_ID,
                                               "app event retry queue size",
                                               &queueSize,
                                               ZCL_INT8U_ATTRIBUTE_TYPE);

  // Also, set up the app event retry paramteres if the functionality is enabled.
  if (appEventRetryEnabled) {
    sli_zigbee_af_wwah_app_event_retry_manager_get_param_tokens();
    sl_zigbee_af_wwah_app_event_retry_manager_config_backoff_params_cb(firstBackoffTimeSeconds,
                                                                       backoffSeqCommonRatio,
                                                                       maxBackoffTimeSeconds,
                                                                       maxRedeliveryAttempts);
  }
  // Initialize tokens
  assert(SL_STATUS_OK == sl_zigbee_af_wwah_app_event_retry_manager_token_init());
}
