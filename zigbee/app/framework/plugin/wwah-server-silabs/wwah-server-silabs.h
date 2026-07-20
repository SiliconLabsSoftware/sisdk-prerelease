/***************************************************************************//**
 * @file
 * @brief Definitions for the WWAH Server Silabs plugin.
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

#ifndef SILABS_WWAH_SERVER_SILABS_H
#define SILABS_WWAH_SERVER_SILABS_H

/**
 * @defgroup wwah-server-silabs WWAH Server Silabs
 * @ingroup component cluster
 * @brief API and Callbacks for the Silabs WWAH Cluster Server Component
 *
 * Silicon Labs proprietary Works With All Hubs (WWAH) server cluster.
 *
 */

/**
 * @addtogroup wwah-server-silabs
 * @{
 */

#ifndef DOXYGEN_SHOULD_SKIP_THIS

enum {
  PLUGIN_WWAH_CONFIGURATION_MASK_ZLL_POLICY_DISABLED            = 0x01,
  PLUGIN_WWAH_CONFIGURATION_MASK_CONFIGURATION_MODE_ENABLED     = 0x02,
  PLUGIN_WWAH_CONFIGURATION_MASK_PARENT_CLASSIFICATION_ENABLED  = 0x04,
  PLUGIN_WWAH_CONFIGURATION_MASK_DISABLE_OTA_DOWNGRADES         = 0x08
                                                                  // 4 more free bits
};

// The latest WWAH ZCL spec (17-01067-012) defines Parent Classification's
// default to 0x00 but we're working to change it to 0x01 so that first time
// joiners can take advantage of the Parent Classification algorithm
#define PLUGIN_WWAH_CONFIGURATION_MASK_DEFAULT               \
  (PLUGIN_WWAH_CONFIGURATION_MASK_CONFIGURATION_MODE_ENABLED \
   | PLUGIN_WWAH_CONFIGURATION_MASK_DISABLE_OTA_DOWNGRADES)

// Based on "WWAH ZCL Cluster Definition", 4 clusters (OTA, Time, Poll Control, Keep Alive)
// are specified to use ONLY the trust center as the cluster server.
#define MAX_CLUSTER_CAPACITY_FOR_USE_TC_FOR_CLUSTER_SERVER_TOKEN 4

#define PLUGIN_WWAH_USE_TC_FOR_CLUSTER_SERVER_NULL_VALUE 0xFFFF

// Used to handle read attribute response received from the time server.
void sli_zigbee_af_sl_wwah_read_attributes_response_callback(sl_zigbee_af_cluster_id_t clusterId,
                                                             uint8_t *buffer,
                                                             uint16_t bufLen);
#endif

#ifndef SL_ZIGBEE_AF_PLUGIN_WWAH_SERVER_SILABS_SURVEY_BEACON_RESULT_LIMIT
#define SL_ZIGBEE_AF_PLUGIN_WWAH_SERVER_SILABS_SURVEY_BEACON_RESULT_LIMIT 4 // default
#endif

// Status of the Powering Down Notification command that is
// passed into the callback function if not NULL.
typedef enum {
  SL_ZIGBEE_POWER_DOWN_NOTIFICATION_SUCCESS,  // A default response was received with success status.
  SL_ZIGBEE_POWER_DOWN_NOTIFICATION_FAILURE,  // A default response was received with an error status, or other error.
  SL_ZIGBEE_POWER_DOWN_NOTIFICATION_TIMEOUT,  // A default response was not received.
  SL_ZIGBEE_POWER_DOWN_NOTIFICATION_IN_PROGRESS
} sl_zigbee_power_down_notification_result_t;

/**
 * @name API
 * @{
 */

/** @brief Send a Powering Off Notification command to the specified destination.
 * If the callback function pointer is not NULL, the callback function will
 * be called once the Powering Off Notification command is delivered or times out.
 *
 * @param nodeId Ver.: always
 * @param srcEndpoint Ver.: always
 * @param dstEndpoint Ver.: always
 * @param reason Ver.: always
 * @param manufacturerId Ver.: always
 * @param manufacturerReason Ver.: always
 * @param manufacturerReasonLen Ver.: always
 * @param sl_zigbee_power_down_notification_result_t Ver.: always
 *
 */
void sl_zigbee_af_wwah_server_send_powering_off_notification(sl_802154_short_addr_t nodeId,
                                                             uint8_t srcEndpoint,
                                                             uint8_t dstEndpoint,
                                                             sl_zigbee_af_wwah_power_notification_reason_t reason,
                                                             uint16_t manufacturerId,
                                                             uint8_t *manufacturerReason,
                                                             uint8_t manufacturerReasonLen,
                                                             void (*pcallback)(sl_zigbee_power_down_notification_result_t) );

/** @brief Send a Powering On Notification command to the specified destination.
 *
 * @param nodeId Ver.: always
 * @param srcEndpoint Ver.: always
 * @param dstEndpoint Ver.: always
 * @param reason Ver.: always
 * @param manufacturerId Ver.: always
 * @param manufacturerReason Ver.: always
 * @param manufacturerReasonLen Ver.: always
 *
 */
void sl_zigbee_af_wwah_server_send_powering_on_notification(sl_802154_short_addr_t nodeId,
                                                            uint8_t srcEndpoint,
                                                            uint8_t dstEndpoint,
                                                            sl_zigbee_af_wwah_power_notification_reason_t reason,
                                                            uint16_t manufacturerId,
                                                            uint8_t *manufacturerReason,
                                                            uint8_t manufacturerReasonLen);

/** @brief Read wwah server silabs attribute.
 *
 * @param endpoint Ver.: always
 * @param attributeId Ver.: always
 * @param name Ver.: always
 * @param data Ver.: always
 * @param size Ver.: always
 *
 * @return sl_zigbee_af_status_t app framework status code
 *
 */
sl_zigbee_af_status_t sl_zigbee_read_wwah_server_silabs_attribute(uint8_t endpoint,
                                                                  sl_zigbee_af_attribute_id_t attributeId,
                                                                  const char *name,
                                                                  uint8_t *data,
                                                                  uint8_t size);

/** @brief write wwah server silabs attribute.
 *
 * @param endpoint Ver.: always
 * @param attributeId Ver.: always
 * @param name Ver.: always
 * @param data Ver.: always
 * @param type Ver.: always
 *
 * @return sl_zigbee_af_status_t app framework status code
 *
 */
sl_zigbee_af_status_t sl_zigbee_write_wwah_server_silabs_attribute(uint8_t endpoint,
                                                                   sl_zigbee_af_attribute_id_t attributeId,
                                                                   const char *name,
                                                                   uint8_t *data,
                                                                   sl_zigbee_af_attribute_type_t type);

/** @} */ // end of name API
/** @} */ // end of wwah-server-silabs

bool setupSurveyBeaconProcedure(void);

void sortBeaconSurveyResult(sl_zigbee_beacon_survey_t surveyResult);

#ifdef SL_ZIGBEE_TEST
extern sl_zigbee_beacon_survey_t surveyBeaconDataCache[SL_ZIGBEE_AF_PLUGIN_WWAH_SERVER_SILABS_SURVEY_BEACON_RESULT_LIMIT];
extern uint8_t surveyBeaconDataCount;
#endif // SL_ZIGBEE_Test

#endif // SILABS_WWAH_SERVER_SILABS_H
