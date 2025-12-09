/***************************************************************************//**
 * @file
 * @brief APIs and defines for the Device Query Service component.
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

/**
 * @defgroup device-query-service Device Query Service
 * @ingroup component
 * @brief API and Callbacks for the Device Query Service Component
 *
 * This component queries new devices for their endpoints and clusters
 * to record information in the Device Database component. New devices are discovered
 * on receipt of a Device Announce frame.
 */

/**
 * @addtogroup device-query-service
 * @{
 */

/**
 * @name API
 * @{
 */

/** @brief Enable or disable the device query service.
 *
 * @param enable[in] True to enable the service, false to disable.
 *
 * @return None
 */
void sl_zigbee_af_device_query_service_enable_disable(bool enable);

/** @brief Get the global enable state of the Device Query service.
 *
 * @return enable True if enabled, false if disabled.
 */
bool sl_zigbee_af_device_query_service_get_enabled_state(void);

/** @brief Get the EUI64 of current discovery target. The service queries one device at a time.
 *
 * @param returnEui64[out] The EUI of the device in the network currently being queried.
 *
 * @return None
 *
 * @note When a device is fully discovered by this component, it marks the device as discovery-complete
 * in the Device Database component, which issues the ::sl_zigbee_af_device_database_discovery_complete_cb
 * callback to let the user application know.
 */
void sl_zigbee_af_device_query_service_get_current_discovery_target_eui64(sl_802154_long_addr_t returnEui64);

/** @} */ // end of name API
/** @} */ // end of device-query-service
