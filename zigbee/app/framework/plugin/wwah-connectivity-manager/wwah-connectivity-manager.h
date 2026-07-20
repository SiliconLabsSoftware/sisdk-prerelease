/***************************************************************************//**
 * @file
 * @brief Definitions for the WWAH Connectivity Manager plugin.
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

#ifndef SILABS_WWAH_CONNECTION_MANAGER_H
#define SILABS_WWAH_CONNECTION_MANAGER_H

/**
 * @defgroup wwah-connectivity-manager WWAH Connectivity Manager
 * @ingroup component
 * @brief API and Callbacks for the WWAH Connectivity Manager Component
 *
 * Silicon Labs implementation of the Works With All Hubs connectivity functions
 * per the Zigbee WWAH Requirements document. This component should be included by
 * routers and end devices only. This component manages connectivity states to both
 * the hub and, for end devices, to the parent. In the event of connection loss,
 * restoration is performed per the WWAH Connectivity Recovery Procedure as defined
 * in the Zigbee WWAH Requirements specification. The component uses the Trust
 * Center Keepalive component on routers and non-sleepy end devices to determine
 * hub connectivity. On sleepy end devices, it uses the Poll Control Server
 * component to determine hub connectivity. On end devices, this component also
 * uses the End Device Support component to determine parent connectivity.
 *
 */

/**
 * @addtogroup wwah-connectivity-manager
 * @{
 */

/**
 * @name API
 * @{
 */

/** @brief Get rejoin algorithm enabled state.
 *
 * @return bool true if enabled
 *
 */
bool sl_zigbee_af_wwah_connectivity_manager_is_rejoin_algorithm_enabled(void);

/** @brief Enable rejoin algorithm.
 *
 * @param endpoint Ver.: always
 *
 */
void sl_zigbee_af_wwah_connectivity_manager_enable_rejoin_algorithm(uint8_t endpoint);

/** @brief Disable rejoin algorithm.
 *
 * @param endpoint Ver.: always
 *
 */
void sl_zigbee_af_wwah_connectivity_manager_disable_rejoin_algorithm(uint8_t endpoint);

/** @brief Get bad parent recovery state.
 *
 * @return bool true if enabled
 *
 */
bool sl_zigbee_af_wwah_connectivity_manager_is_bad_parent_recovery_enabled(void);

/** @brief Enable bad parent recovery.
 *
 * @param badParentRejoinPeriod Ver.: always
 *
 */
void sl_zigbee_af_wwah_connectivity_manager_enable_bad_parent_recovery(uint16_t badParentRejoinPeriod);

/** @brief Disable bad parent recovery.
 *
 */
void sl_zigbee_af_wwah_connectivity_manager_disable_bad_parent_recovery();

/** @brief Enable periodic router checkins.
 *
 * @param checkInInterval Ver.: always
 *
 * @return sl_status_t status code
 *
 */
sl_status_t sl_zigbee_af_wwah_connectivity_manager_enable_periodic_router_check_ins(uint16_t checkInInterval);

/** @brief Disable periodic router checkins.
 *
 * @return sl_status_t status code
 *
 */
sl_status_t sl_zigbee_af_wwah_connectivity_manager_disable_periodic_router_check_ins(void);

/** @brief Set rejoin parameters.
 *
 * @param fastRejoinTimeoutSeconds Ver.: always
 * @param durstionBetweenRejoinSeconds Ver.: always
 * @param fastRejoinFirstbackoffSeconds Ver.: always
 * @param maxBackoffTimeSeconds Ver.: always
 * @param maxBackoffIterations Ver.: always
 *
 */
void sl_zigbee_af_wwah_connectivity_manager_set_rejoin_parameters(uint16_t fastRejoinTimeoutSeconds,
                                                                  uint16_t durationBetweenRejoinsSeconds,
                                                                  uint16_t fastRejoinFirstBackoffSeconds,
                                                                  uint16_t maxBackoffTimeSeconds,
                                                                  uint16_t maxBackoffIterations);

/** @} */ // end of name API

/**
 * @name Callbacks
 * @{
 */

/**
 * @defgroup wwah_connectivity_mgr_cb WWAH Connectivity Manager
 * @ingroup af_callback
 * @brief Callbacks for WWAH Connectivity Manager Component
 *
 */

/**
 * @addtogroup wwah_connectivity_mgr_cb
 * @{
 */

/** @brief Initialization callback for connectivity manager.
 *
 * @param endpoint Ver.: always
 *
 */
void sl_zigbee_af_wwah_connectivity_manager_init_cb(uint8_t endpoint);

/** @} */ // end of wwah_connectivity_mgr_cb
/** @} */ // end of name Callbacks
/** @} */ // end of wwah-client-silabs

// Internal APIs
void wwahConnectivityManagerPrintInfo(void);
void wwahConnectivityManagerShow(void);
void sli_zigbee_af_wwah_connectivity_manager_enable_bad_parent_recovery(uint8_t endpoint, uint16_t badParentRejoinPeriod);
void sli_zigbee_af_wwah_connectivity_manager_disable_bad_parent_recovery(uint8_t endpoint);

#endif // SILABS_WWAH_CONNECTION_MANAGER_H
