/***************************************************************************//**
 * @file
 * @brief Definitions for the WWAH App Event Retry Manager plugin.
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

#ifndef SILABS_WWAH_APP_EVENT_RETRY_MANAGER_H
#define SILABS_WWAH_APP_EVENT_RETRY_MANAGER_H

/**
 * @defgroup wwah-app-event-retry-manager WWAH App Event Retry Manager
 * @ingroup component
 * @brief API and Callbacks for the WWAH App Event Retry Manager Component
 *
 * Silicon Labs proprietary Works With All Hubs (WWAH) app event retry manager.
 *
 */

/**
 * @addtogroup wwah-app-event-retry-manager
 * @{
 */

#ifndef DOXYGEN_SHOULD_SKIP_THIS

// Absolute max backoff time, at least one retry a day
// (ie. 24 hours * 60 minutes * 60 seconds).
#define SL_ZIGBEE_AF_PLUGIN_WWAH_APP_EVENT_RETRY_ABS_MAX_BACKOFF_TIME_SEC (24 * 60 * 60)

void sl_zigbee_af_wwah_app_event_retry_manager_init_cb(uint8_t endpoint);

sl_status_t sl_zigbee_af_wwah_app_event_retry_manager_enable_with_params(uint8_t endpoint,
                                                                         uint8_t firstBackoffTimeSeconds,
                                                                         uint8_t backoffSeqCommonRatio,
                                                                         uint32_t maxBackoffTimeSeconds,
                                                                         uint8_t maxRedeliveryAttempts);

void sl_zigbee_af_wwah_app_event_retry_manager_disable(uint8_t endpoint);
#endif

/**
 * @name API
 * @{
 */

/** @brief Write attribute-enabled callback.
 *
 * This callback is called when the enabled backoff parameter is changed.
 *
 * @param enabled   Ver.: always
 */
void sl_zigbee_af_wwah_app_event_retry_manager_write_attribute_enabled(uint8_t enabled);

/** @brief Write attribute queue size callback.
 *
 * This callback is called when the app event retry queue size is being reported.
 *
 * @param queueSize   Ver.: always
 */
void sl_zigbee_af_wwah_app_event_retry_manager_write_attribute_queue_size(uint8_t queueSize);

/** @brief WWAH app event retry manager set backoff parameters to default.
 *
 * This callback is called when the app event retry manager is about to set
 * the backoff parameters of the critical message queue provider plugin back to their default.
 *
 */
void sl_zigbee_af_wwah_app_event_retry_manager_set_backoff_params_to_default(void);

/** @} */ // end of name API

/**
 * @name Callbacks
 * @{
 */

/**
 * @defgroup wwah_event_retry_mgr_cb WWAH App Event Retry manager
 * @ingroup af_callback
 * @brief Callbacks for WWAH App Event Retry manager Component
 *
 */

/**
 * @addtogroup wwah_event_retry_mgr_cb
 * @{
 */

/** @brief WWAH app event retry manager config backoff parameters callback.
 *
 * This callback is called when the app event retry manager (re)configures
 * the backoff parameters of the critical message queue provider plugin.
 *
 * @param firstBackoffTimeSeconds Ver.: always
 * @param backoffSeqCommonRatio   Ver.: always
 * @param maxBackoffTimeSeconds   Ver.: always
 * @param maxRedeliveryAttempts   Ver.: always
 */
//TODO: parameters
sl_status_t sl_zigbee_af_wwah_app_event_retry_manager_config_backoff_params_cb(uint8_t firstBackoffTimeSeconds,
                                                                               uint8_t backoffSeqCommonRatio,
                                                                               uint32_t maxBackoffTimeSeconds,
                                                                               uint8_t maxRedeliveryAttempts);

/** @} */ // end of wwah_event_retry_mgr_cb
/** @} */ // end of name Callbacks
/** @} */ // end of wwah-app-event-retry-manager

#endif // SILABS_WWAH_APP_EVENT_RETRY_MANAGER_H
